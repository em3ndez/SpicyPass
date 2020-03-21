/*  gui.cpp
 *
 *
 *  Copyright (C) 2020 Jfreegman <Jfreegman@gmail.com>
 *
 *  This file is part of SpicyPass.
 *
 *  SpicyPass is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  SpicyPass is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with SpicyPass.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifdef GUI_SUPPORT

#include "spicy.hpp"
#include "gui.hpp"
#include "load.hpp"
#include "util.hpp"
#include "password.hpp"

#ifndef SpicyPass_LOGO_FILE_PATH
#define SpicyPass_LOGO_FILE_PATH "../icon/spicypass.svg"
#endif

#ifndef GLADE_FILE_PATH
#define GLADE_FILE_PATH "../gui/gui.glade"
#endif

enum {
   KEY_COLUMN,
   PASS_COLUMN,
   N_COLUMNS
};

struct Callback_Data {
    GtkWidget         *window;
    GtkWidget         *widget1;
    GtkWidget         *widget2;
    GtkWidget         *widget3;
    GtkToggleButton   *buttonShowPass;
    Pass_Store        *p;
    struct List_Store *ls;
};

static void on_quit(GtkButton *button, gpointer data);
static void on_pwButtonEnter_clicked(GtkEntry *button, gpointer data);
static void on_key_enter(GtkEntry *entry, gpointer data);


static int load_pass_store_entries(Pass_Store &p, struct List_Store &ls)
{
    vector<tuple<string, string>> result;
    int matches = p.get_matches("", result, false);

    if (matches == PASS_STORE_LOCKED) {
        return PASS_STORE_LOCKED;
    }

    GtkTreeIter iter;

    for (const auto &item: result) {
        gtk_list_store_append(ls.store, &iter);
        gtk_list_store_set(ls.store, &iter, KEY_COLUMN, get<0>(item).c_str(), PASS_COLUMN, "******", -1);
    }

    return 0;
}

static int password_prompt(Pass_Store &p, struct List_Store &ls)
{
    gtk_list_store_clear(ls.store);

    GtkBuilder *builder = gtk_builder_new_from_file(GLADE_FILE_PATH);
    GtkWidget *pwWindow = GTK_WIDGET(gtk_builder_get_object(builder, "pwWindow"));
    GtkEntry *pwEntry = GTK_ENTRY(gtk_builder_get_object(builder, "pwEntry"));

    gtk_entry_set_visibility(pwEntry, false);
    gtk_entry_set_max_length(pwEntry, MAX_STORE_PASSWORD_SIZE);

    GtkButton *pwButtonEnter = GTK_BUTTON(gtk_builder_get_object(builder, "pwButtonEnter"));
    GtkButton *pwButtonQuit = GTK_BUTTON(gtk_builder_get_object(builder, "pwButtonQuit"));

    g_object_unref(builder);

    struct Callback_Data *cb_data = (struct Callback_Data *) calloc(1, sizeof(struct Callback_Data));

    if (cb_data == NULL) {
        return -1;
    }

    cb_data->window = pwWindow;
    cb_data->widget1 = GTK_WIDGET(pwEntry);
    cb_data->ls = &ls;
    cb_data->p = &p;

    g_signal_connect(pwButtonEnter, "clicked", G_CALLBACK(on_pwButtonEnter_clicked), cb_data);
    g_signal_connect(pwButtonQuit, "clicked", G_CALLBACK(on_quit), cb_data);
    g_signal_connect(pwEntry, "activate", G_CALLBACK(on_key_enter), pwButtonEnter);

    gtk_window_set_keep_above(GTK_WINDOW(pwWindow), true);

    gtk_widget_show(pwWindow);

    return 0;
}

static void dialog_box(const gchar *message, GtkMessageType type, GtkWidget *parent)
{
    const gchar *name = NULL;

    switch (type) {
        case GTK_MESSAGE_INFO: {
            name = "dialogInfo";
            break;
        }
        case GTK_MESSAGE_WARNING: {
            name = "dialogWarning";
            break;
        }
        case GTK_MESSAGE_ERROR: {
            name = "dialogError";
            break;
        }
        default: {
            name = "dialogInfo";
            break;
        }
    }

    GtkBuilder *builder = gtk_builder_new_from_file(GLADE_FILE_PATH);
    GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object(builder, name));
    g_object_unref(builder);

    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
    g_signal_connect_swapped(dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog);
    gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(dialog), message);
    gtk_widget_show(dialog);
}

/***
 *** Signal handlers
 ***/
static bool on_key_escape(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    UNUSED_VAR(data);

    if (event->keyval != GDK_KEY_Escape) {
        return false;
    }

    gtk_widget_destroy(widget);

    return true;
}

static void on_key_enter(GtkEntry *entry, gpointer data)
{
    UNUSED_VAR(entry);

    if (data) {
        GtkButton *button = GTK_BUTTON(data);
        gtk_button_clicked(button);
    }
}

static void on_addEntryButtonOk(GtkButton *button, gpointer data)
{
    if (!data) {
        return;
    }

    UNUSED_VAR(button);

    struct Callback_Data *cb_data = (struct Callback_Data *) data;
    Pass_Store *p = cb_data->p;
    struct List_Store *ls = cb_data->ls;
    GtkWidget *window = cb_data->window;

    GtkEntry *loginEntry = GTK_ENTRY(cb_data->widget1);
    GtkEntry *passEntry = GTK_ENTRY(cb_data->widget2);

    const gchar *loginText = gtk_entry_get_text(loginEntry);
    const gchar *passText = gtk_entry_get_text(passEntry);
    gint keylen = gtk_entry_get_text_length(loginEntry);
    gint passlen = gtk_entry_get_text_length(passEntry);

    gchar keyBuf[MAX_ENTRY_KEY_SIZE + 1];
    gchar passBuf[MAX_STORE_PASSWORD_SIZE + 1];

#ifdef DEBUG
    assert((size_t) passlen < sizeof(keyBuf));
    assert((size_t) keylen < sizeof(passBuf));
#endif

    memcpy(keyBuf, loginText, keylen);
    memcpy(passBuf, passText, passlen);
    keyBuf[keylen] = 0;
    passBuf[passlen] = 0;

    char msg[128];
    bool has_err = true;
    int exists;
    int ret;

    if (keylen == 0) {
        snprintf(msg, sizeof(msg), "Entry cannot be empty");
        goto on_exit;
    }

    if (string_contains(loginText, DELIMITER)) {
        snprintf(msg, sizeof(msg), "Error: Login may not contain the `%s` character", DELIMITER);
        goto on_exit;
    }

    if (passlen <= 1) {
        string password = random_password(16U);

        if (password.empty()) {
            snprintf(msg, sizeof(msg), "Error: Failed to generate random password");
            goto on_exit;
        }

        passlen = password.length();
        memcpy(passBuf, password.c_str(), passlen);
        passBuf[passlen] = 0;
    }

    exists = p->key_exists(string(keyBuf));

    if (exists == PASS_STORE_LOCKED) {
        if (password_prompt(*p, *ls) != 0) {
            dialog_box("Failed to unlock pass store", GTK_MESSAGE_ERROR, window);
        }

        gtk_widget_destroy(window);
        return;
    }

    if (exists > 0) {
        snprintf(msg, sizeof(msg), "Entry already exists");
        goto on_exit;
    }

    if (p->insert(string(keyBuf), string(passBuf)) != 0) {
        snprintf(msg, sizeof(msg), "Failed to add entry");
        goto on_exit;
    }

    GtkTreeIter iter;
    gtk_list_store_append(ls->store, &iter);
    gtk_list_store_set(ls->store, &iter, KEY_COLUMN, keyBuf, PASS_COLUMN,
                      gtk_toggle_button_get_active(cb_data->buttonShowPass) ? passBuf : "******",
                      -1);

    ret = save_password_store(*p);

    if (ret != 0) {
        snprintf(msg, sizeof(msg), "Failed to save pass store (error %d)", ret);
        goto on_exit;
    }

    has_err = false;

on_exit:
    if (has_err) {
        dialog_box(msg, GTK_MESSAGE_ERROR, window);
    } else {
        gtk_widget_destroy(window);
    }
}

static void on_addEntryButtonCancel(GtkButton *button, gpointer data)
{
    UNUSED_VAR(button);

    if (data) {
        struct Callback_Data *cb_data = (struct Callback_Data *) data;
        gtk_widget_destroy(cb_data->window);
    }
}

static void on_buttonAdd_clicked(GtkButton *button, gpointer data)
{
    UNUSED_VAR(button);

    if (!data) {
        return;
    }

    struct Callback_Data *cb_data = (struct Callback_Data *) data;

    Pass_Store *p = cb_data->p;
    struct List_Store *ls = cb_data->ls;

    GtkBuilder *builder = gtk_builder_new_from_file(GLADE_FILE_PATH);
    GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(builder, "addEntryWindow"));

    GtkButton *okButton = GTK_BUTTON(gtk_builder_get_object(builder, "addEntryButtonOk"));
    GtkButton *cancelButton = GTK_BUTTON(gtk_builder_get_object(builder, "addEntryButtonCancel"));
    GtkEntry *loginEntry = GTK_ENTRY(gtk_builder_get_object(builder, "loginEntry"));
    GtkEntry *passEntry = GTK_ENTRY(gtk_builder_get_object(builder, "passEntry"));

    g_object_unref(builder);

    gtk_entry_set_max_length(loginEntry, MAX_ENTRY_KEY_SIZE);
    gtk_entry_set_max_length(passEntry, MAX_STORE_PASSWORD_SIZE);

    cb_data->widget1 = GTK_WIDGET(loginEntry);
    cb_data->widget2 = GTK_WIDGET(passEntry);
    cb_data->window = window;

    g_signal_connect(okButton, "clicked", G_CALLBACK(on_addEntryButtonOk), cb_data);
    g_signal_connect(cancelButton, "clicked", G_CALLBACK(on_addEntryButtonCancel), cb_data);
    g_signal_connect(loginEntry, "activate", G_CALLBACK(on_key_enter), okButton);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_escape), NULL);

    gtk_window_set_keep_above(GTK_WINDOW(window), true);
    gtk_widget_show(window);

    if (p->check_lock()) {
        if (password_prompt(*p, *ls) != 0) {
            dialog_box("Failed to unlock pass store", GTK_MESSAGE_ERROR, window);
        }
    }
}

static void on_deleteEntryButtonYes(GtkButton *button, gpointer data)
{
    UNUSED_VAR(button);

    struct Callback_Data *cb_data = (struct Callback_Data *) data;
    struct List_Store *ls = cb_data->ls;
    Pass_Store *p = cb_data->p;
    GtkWidget *window = cb_data->window;

    GtkTreeModel *model = GTK_TREE_MODEL(ls->store);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(ls->view);

    char msg[128];
    bool has_err = true;
    GtkTreeIter iter;
    int removed;
    int ret;
    gchar *key = NULL;

    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        snprintf(msg, sizeof(msg), "Error: Selection not found");
        goto on_exit;
    }

    gtk_tree_model_get(model, &iter, KEY_COLUMN, &key, -1);

    removed = p->remove(string(key));

    if (removed == PASS_STORE_LOCKED) {
        gtk_widget_destroy(window);

        if (password_prompt(*p, *ls) != 0) {
            dialog_box("Failed to unlock pass store", GTK_MESSAGE_ERROR, window);
        }

        g_free(key);

        return;
    }

    if (removed != 0) {
        snprintf(msg, sizeof(msg), "Failed to find key \"%s\" in pass store", key);
        goto on_exit;
    }

    ret = save_password_store(*p);

    if (ret != 0) {
        snprintf(msg, sizeof(msg), "Failed to save pass store (error %d)", ret);
        goto on_exit;
    }

    gtk_list_store_remove(ls->store, &iter);

    has_err = false;
on_exit:
    if (has_err) {
        dialog_box(msg, GTK_MESSAGE_ERROR, window);
    } else {
        gtk_widget_destroy(window);
    }

    g_free(key);
}

static void on_buttonExit_clicked(GtkButton *button, gpointer data)
{
    UNUSED_VAR(button);

    if (data) {
        struct Callback_Data *cb_data = (struct Callback_Data *) data;
        gtk_widget_destroy(cb_data->window);
    }
}

static void on_buttonDelete_clicked(GtkButton *button, gpointer data)
{
    UNUSED_VAR(button);

    if (!data) {
        return;
    }

    struct Callback_Data *cb_data = (struct Callback_Data *) data;

    struct List_Store *ls = cb_data->ls;
    Pass_Store *p = cb_data->p;
    GtkWidget *window = cb_data->window;

    if (p->check_lock()) {
        if (password_prompt(*p, *ls) != 0) {
            dialog_box("Failed to unlock pass store", GTK_MESSAGE_ERROR, window);
        }

        return;
    }

    GtkTreeModel *model = GTK_TREE_MODEL(ls->store);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(ls->view);

    GtkBuilder *builder = gtk_builder_new_from_file(GLADE_FILE_PATH);

    GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object(builder, "dialogWindow"));
    GtkButton *yesButton = GTK_BUTTON(gtk_builder_get_object(builder, "confirmButtonYes"));
    GtkButton *noButton = GTK_BUTTON(gtk_builder_get_object(builder, "confirmButtonNo"));
    GtkLabel *label = GTK_LABEL(gtk_builder_get_object(builder, "confirmLabel"));

    g_object_unref(builder);

    cb_data->window = dialog;

    g_signal_connect(yesButton, "clicked", G_CALLBACK(on_deleteEntryButtonYes), cb_data);
    g_signal_connect(noButton, "clicked", G_CALLBACK(on_buttonExit_clicked), cb_data);

    GtkTreeIter iter;

    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gtk_widget_destroy(dialog);
        return;
    }

    gchar *key;
    gtk_tree_model_get(model, &iter, KEY_COLUMN, &key, -1);

    gchar msg[128];
    snprintf(msg, sizeof(msg), "Are you sure you want to delete \"%s\"?", key);
    gtk_label_set_text(label, msg);

    g_free(key);

    gtk_widget_show(dialog);
}

static void on_buttonCopy_clicked(GtkButton *button, gpointer data)
{
    UNUSED_VAR(button);

    if (!data) {
        return;
    }

    struct Callback_Data *cb_data = (struct Callback_Data *) data;

    struct List_Store *ls = cb_data->ls;
    Pass_Store *p = cb_data->p;
    GtkWidget *window = cb_data->window;

    GtkTreeModel *model = GTK_TREE_MODEL(ls->store);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(ls->view);
    GtkTreeIter iter;

    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return;
    }

    gchar *key = NULL;
    gtk_tree_model_get(model, &iter, KEY_COLUMN, &key, -1);

    vector<tuple<string, string>> result;
    int matches = p->get_matches(key, result, true);

    g_free(key);

    if (matches == PASS_STORE_LOCKED) {
        if (password_prompt(*p, *ls) != 0) {
            dialog_box("Failed to unlock pass store", GTK_MESSAGE_ERROR, window);
        }

        return;
    }

    if (result.size() != 1) {
        cerr << "Warning: copy invoked invalid key request" << endl;
        return;
    }

    tuple<string, string> v_item = result.at(0);
    const gchar *login = get<0>(v_item).c_str();
    const gchar *password = get<1>(v_item).c_str();

    GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(clipboard, password, -1);

    char msg[128];
    snprintf(msg, sizeof(msg), "Copied password for \"%s\" to clipboard", login);
    dialog_box(msg, GTK_MESSAGE_INFO, window);
}

static void on_buttonShowPass_toggled(GtkToggleButton *button, gpointer data)
{
    if (!data) {
        return;
    }

    struct Callback_Data *cb_data = (struct Callback_Data *) data;
    struct List_Store *ls = cb_data->ls;
    Pass_Store *p = cb_data->p;
    GtkWidget *window = cb_data->window;

    vector<tuple<string, string>> result;
    int matches = p->get_matches("", result, false);

    if (matches == PASS_STORE_LOCKED) {
        if (password_prompt(*p, *ls) != 0) {
            dialog_box("Failed to unlock pass store", GTK_MESSAGE_ERROR, window);
        }

        return;
    }

    gtk_list_store_clear(ls->store);

    bool toggle_on = gtk_toggle_button_get_active(button);

    GtkTreeIter iter;

    for (const auto &item: result) {
        const char *val = toggle_on ? get<1>(item).c_str() : "******";
        gtk_list_store_append(ls->store, &iter);
        gtk_list_store_set(ls->store, &iter, KEY_COLUMN, get<0>(item).c_str(), PASS_COLUMN, val, -1);
    }
}

static void on_quit(GtkButton *button, gpointer data)
{
    UNUSED_VAR(button);

    if (data) {
        struct Callback_Data *cb_data = (struct Callback_Data *) data;
        gtk_widget_destroy(cb_data->window);
        free(cb_data);
    }

    gtk_main_quit();
}

static void on_changePassButtonOk_clicked(GtkButton *button, gpointer data)
{
    UNUSED_VAR(button);

    struct Callback_Data *cb_data = (struct Callback_Data *) data;
    Pass_Store *p = cb_data->p;
    struct List_Store *ls = cb_data->ls;
    GtkWidget *window = cb_data->window;

    if (p->check_lock()) {
        if (password_prompt(*p, *ls) != 0) {
            dialog_box("Failed to unlock pass store", GTK_MESSAGE_ERROR, window);
        }

        return;
    }

    GtkEntry *entry1 = GTK_ENTRY(cb_data->widget1);
    GtkEntry *entry2 = GTK_ENTRY(cb_data->widget2);
    GtkEntry *entry3 = GTK_ENTRY(cb_data->widget3);

    const gchar *old_pass = gtk_entry_get_text(entry1);
    const gchar *new_pass1 = gtk_entry_get_text(entry2);
    const gchar *new_pass2 = gtk_entry_get_text(entry3);
    gint old_pass_len = gtk_entry_get_text_length(entry1);
    gint new_pass1_len = gtk_entry_get_text_length(entry2);

#ifdef debug
    assert(old_pass_len <= MAX_STORE_PASSWORD_SIZE && new_pass1_len <= MAX_STORE_PASSWORD_SIZE);
#endif

    bool has_err = true;
    int ret;
    char msg[128];
    snprintf(msg, sizeof(msg), "Successfully updated password");

    unsigned char old_pass_buf[MAX_STORE_PASSWORD_SIZE + 2];
    unsigned char new_pass_buf[MAX_STORE_PASSWORD_SIZE + 2];

    if (new_pass1_len < MIN_STORE_PASSWORD_SIZE) {
        snprintf(msg, sizeof(msg), "Password must be at least %d characters long", MIN_STORE_PASSWORD_SIZE);
        goto on_exit;
    }

    if (strcmp(new_pass1, new_pass2) != 0) {
        snprintf(msg, sizeof(msg), "New passwords don't match");
        goto on_exit;
    }

    memcpy(old_pass_buf, old_pass, old_pass_len);
    old_pass_buf[old_pass_len++] = '\n';
    old_pass_buf[old_pass_len] = 0;

    unsigned char hash[CRYPTO_HASH_SIZE];
    p->get_password_hash(hash);

    if (!crypto_verify_pass_hash(hash, old_pass_buf, old_pass_len)) {
        snprintf(msg, sizeof(msg), "Invalid password");
        goto on_exit;
    }

    memcpy(new_pass_buf, new_pass1, new_pass1_len);

    new_pass_buf[new_pass1_len++] = '\n';
    new_pass_buf[new_pass1_len] = 0;

    ret = update_crypto(*p, new_pass_buf, new_pass1_len);

    crypto_memwipe(new_pass_buf, sizeof(new_pass_buf));

    if (ret == PASS_STORE_LOCKED) {
        if (password_prompt(*p, *ls) != 0) {
            dialog_box("Failed to unlock pass store", GTK_MESSAGE_ERROR, window);
        }

        gtk_widget_destroy(window);
        return;
    }

    if (ret < 0) {
        snprintf(msg, sizeof(msg), "Failed to update password (error: %d)", ret);
        goto on_exit;
    }

    has_err = false;

on_exit:
    if (!has_err) {
        gtk_widget_destroy(window);
        dialog_box(msg, GTK_MESSAGE_INFO, window);
    } else {
        dialog_box(msg, GTK_MESSAGE_ERROR, window);
    }
}

static void on_menuChangePassword_activate(GtkMenuItem *menuitem, gpointer data)
{
    UNUSED_VAR(menuitem);

    if (!data) {
        return;
    }

    struct Callback_Data *cb_data = (struct Callback_Data *) data;

    struct List_Store *ls = cb_data->ls;
    Pass_Store *p = cb_data->p;

    GtkBuilder *builder = gtk_builder_new_from_file(GLADE_FILE_PATH);

    GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(builder, "changePwWindow"));
    GtkButton *buttonOk = GTK_BUTTON(gtk_builder_get_object(builder, "changePwButtonOk"));
    GtkButton *buttonCancel = GTK_BUTTON(gtk_builder_get_object(builder, "changePwButtonCancel"));
    GtkEntry *entry1 = GTK_ENTRY(gtk_builder_get_object(builder, "changePwEntry1"));
    GtkEntry *entry2 = GTK_ENTRY(gtk_builder_get_object(builder, "changePwEntry2"));
    GtkEntry *entry3 = GTK_ENTRY(gtk_builder_get_object(builder, "changePwEntry3"));

    g_object_unref(builder);

    gtk_entry_set_max_length(entry1, MAX_STORE_PASSWORD_SIZE);
    gtk_entry_set_max_length(entry2, MAX_STORE_PASSWORD_SIZE);
    gtk_entry_set_max_length(entry3, MAX_STORE_PASSWORD_SIZE);
    gtk_entry_set_visibility(entry1, false);
    gtk_entry_set_visibility(entry2, false);
    gtk_entry_set_visibility(entry3, false);

    cb_data->window = window;
    cb_data->widget1 = GTK_WIDGET(entry1);
    cb_data->widget2 = GTK_WIDGET(entry2);
    cb_data->widget3 = GTK_WIDGET(entry3);

    g_signal_connect(buttonOk, "clicked", G_CALLBACK(on_changePassButtonOk_clicked), cb_data);
    g_signal_connect(buttonCancel, "clicked", G_CALLBACK(on_buttonExit_clicked), cb_data);
    g_signal_connect(entry3, "activate", G_CALLBACK(on_key_enter), buttonOk);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_escape), NULL);

    gtk_window_set_keep_above(GTK_WINDOW(window), true);
    gtk_widget_show(window);

    if (p->check_lock()) {
        if (password_prompt(*p, *ls) != 0) {
            dialog_box("Failed to unlock pass store", GTK_MESSAGE_ERROR, window);
        }
    }
}

static void on_menuPassGenGenerate_clicked(GtkButton *button, gpointer data)
{
    UNUSED_VAR(button);

    struct Callback_Data *cb_data = (struct Callback_Data *) data;
    GtkWidget *window = cb_data->window;

    GtkEntry *entry1 = GTK_ENTRY(cb_data->widget1);
    GtkEntry *entry2 = GTK_ENTRY(cb_data->widget2);

    const gchar *length_text = gtk_entry_get_text(entry1);
    gint text_length = gtk_entry_get_text_length(entry1);

    int length = 0;
    bool has_err = true;
    char msg[128];
    snprintf(msg, sizeof(msg), "Length must be an integer between 4 and 64");
    string password;

    if (text_length > 2 || text_length < 1) {
        goto on_exit;
    }

    try {
        length = stoi(length_text);
    } catch (const exception &e) {
        goto on_exit;
    }

    if (length < NUM_PASS_GUARANTEED_CHARS || length > MAX_STORE_PASSWORD_SIZE) {
        goto on_exit;
    }

    password = random_password(length);

    if (password.empty()) {
        goto on_exit;
    }

    gtk_entry_set_text(entry2, password.c_str());

    has_err = false;

on_exit:
    if (has_err) {
        dialog_box(msg, GTK_MESSAGE_WARNING, window);
    }
}

static void on_menuPassGen_activate(GtkMenuItem *menuitem, gpointer data)
{
    UNUSED_VAR(menuitem);

    if (!data) {
        return;
    }

    struct Callback_Data *cb_data = (struct Callback_Data *) data;

    GtkBuilder *builder = gtk_builder_new_from_file(GLADE_FILE_PATH);

    GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(builder, "passGenWindow"));
    GtkButton *buttonGen = GTK_BUTTON(gtk_builder_get_object(builder, "passGenButtonGen"));
    GtkButton *buttonExit = GTK_BUTTON(gtk_builder_get_object(builder, "passGenButtonExit"));
    GtkEntry *entry1 = GTK_ENTRY(gtk_builder_get_object(builder, "passGenEntry1"));
    GtkEntry *entry2 = GTK_ENTRY(gtk_builder_get_object(builder, "passGenEntry2"));

    g_object_unref(builder);

    gtk_entry_set_max_length(entry1, 2);
    gtk_entry_set_max_length(entry2, MAX_STORE_PASSWORD_SIZE);

    cb_data->window = window;
    cb_data->widget1 = GTK_WIDGET(entry1);
    cb_data->widget2 = GTK_WIDGET(entry2);

    g_signal_connect(buttonGen, "clicked", G_CALLBACK(on_menuPassGenGenerate_clicked), cb_data);
    g_signal_connect(buttonExit, "clicked", G_CALLBACK(on_buttonExit_clicked), cb_data);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_escape), NULL);

    gtk_window_set_keep_above(GTK_WINDOW(window), true);
    gtk_widget_show(window);
}

static void on_menuAbout_activate(GtkMenuItem *menuitem, gpointer data)
{
    UNUSED_VAR(menuitem);
    UNUSED_VAR(data);

    GtkBuilder *builder = gtk_builder_new_from_file(GLADE_FILE_PATH);
    GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(builder, "aboutDialog"));
    g_object_unref(builder);

    char version[256];
    snprintf(version, sizeof(version), "Version %s.%s.%s", SpicyPass_VERSION_MAJOR, SpicyPass_VERSION_MINOR, SpicyPass_VERSION_PATCH);
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(window), version);

    GError *err = NULL;
    GdkPixbuf *logo = gdk_pixbuf_new_from_file(SpicyPass_LOGO_FILE_PATH, &err);

    if (err) {
        fprintf(stderr, "%s\n", err->message);
    }

    gtk_about_dialog_set_logo(GTK_ABOUT_DIALOG(window), logo);

    g_signal_connect_swapped(window, "response", G_CALLBACK(gtk_widget_destroy), window);

    gtk_window_set_keep_above(GTK_WINDOW(window), true);
    gtk_widget_show(window);
}

static void on_pwButtonEnter_clicked(GtkEntry *button, gpointer data)
{
    UNUSED_VAR(button);

    if (!data) {
        return;
    }

    struct Callback_Data *cb_data = (struct Callback_Data *) data;
    struct List_Store *ls = cb_data->ls;
    Pass_Store *p = cb_data->p;
    GtkWidget *window = cb_data->window;

    GtkEntry *entry = GTK_ENTRY(cb_data->widget1);

    const gchar *text = gtk_entry_get_text(entry);
    gint length = gtk_entry_get_text_length(entry);

#ifdef DEBUG
    assert(length <= MAX_STORE_PASSWORD_SIZE);
#endif

    char msg[128];
    bool has_err = true;
    int ret;
    unsigned char password[MAX_STORE_PASSWORD_SIZE + 2];
    memcpy(password, text, length);

    password[length++] = '\n';
    password[length] = 0;

    ret = load_password_store(*p, password, length);

    crypto_memwipe(password, sizeof(password));

    if (ret < 0) {
        switch (ret) {
            case -1: {
                snprintf(msg, sizeof(msg), "Failed to open pass store file");
                goto on_exit;
            }
            case -2: {
                snprintf(msg, sizeof(msg), "Invalid password");
                goto on_exit;
            }
            case -3: {
                snprintf(msg, sizeof(msg), "Failed to decrypt pass store file");
                goto on_exit;
            }
            case -4: {
                snprintf(msg, sizeof(msg), "Failed to load pass store: Invalid file format");
                goto on_exit;
            }
            default: {
                snprintf(msg, sizeof(msg), "Unknown error in load_password_store()");
                goto on_exit;
            }
        }
    }

    if (load_pass_store_entries(*cb_data->p, *cb_data->ls) != 0) {
        if (password_prompt(*p, *ls) != 0) {
            dialog_box("Failed to unlock pass store", GTK_MESSAGE_ERROR, window);
        }

        return;
    }

    has_err = false;

on_exit:
    if (has_err) {
        dialog_box(msg, GTK_MESSAGE_ERROR, window);
    } else {
        gtk_widget_destroy(window);
        free(cb_data);
    }
}

static void on_newPwButtonEnter_clicked(GtkEntry *button, gpointer data)
{
    UNUSED_VAR(button);

    if (!data) {
        return;
    }

    struct Callback_Data *cb_data = (struct Callback_Data *) data;
    Pass_Store *p = cb_data->p;
    GtkWidget *window = cb_data->window;

    GtkEntry *entry1 = GTK_ENTRY(cb_data->widget1);
    GtkEntry *entry2 = GTK_ENTRY(cb_data->widget2);

    const gchar *text1 = gtk_entry_get_text(entry1);
    const gchar *text2 = gtk_entry_get_text(entry2);
    gint text1_len = gtk_entry_get_text_length(entry1);
    gint text2_len = gtk_entry_get_text_length(entry2);

#ifdef DEBUG
    assert(text1_len <= MAX_STORE_PASSWORD_SIZE && text2_len <= MAX_STORE_PASSWORD_SIZE);
#endif

    unsigned char passBuff[MAX_STORE_PASSWORD_SIZE + 2];
    size_t passLen = text1_len;
    bool has_err = true;
    int ret;

    const string path = get_store_path(false);

    char msg[1024];
    snprintf(msg, sizeof(msg),  "Profile successfully created.\n\n"\
                                "Please remember that the store file is encrypted on your disk, "\
                                "and that it is impossible to recover your data if you lose "\
                                "or forget your master password.\n\n"\
                                "It is strongly recommended that you regularly create backups of the store file, "\
                                "located at: %s", path.c_str());

    if (text1_len < MIN_STORE_PASSWORD_SIZE) {
        snprintf(msg, sizeof(msg), "Password must be at least %d characters long", MIN_STORE_PASSWORD_SIZE);
        goto on_exit;
    }

    if (strcmp(text1, text2) != 0) {
        snprintf(msg, sizeof(msg), "Passwords don't match");
        goto on_exit;
    }

    memcpy(passBuff, text1, text1_len);
    passBuff[passLen++] = '\n';
    passBuff[passLen] = 0;

    if (init_pass_hash(passBuff, passLen) != 0) {
        snprintf(msg, sizeof(msg), "init_pass_hash() failed");
        goto on_exit;
    }

    ret = load_password_store(*p, passBuff, passLen);

    if (ret < 0) {
        snprintf(msg, sizeof(msg), "Failed to load pass store (error: %d)", ret);
        goto on_exit;
    }

    has_err = false;

on_exit:
    if (!has_err) {
        dialog_box(msg, GTK_MESSAGE_INFO, window);
        gtk_widget_destroy(window);
        crypto_memwipe(passBuff, sizeof(passBuff));
        free(cb_data);
    } else {
        dialog_box(msg, GTK_MESSAGE_ERROR, window);
    }
}

/***
 *** GUI class methods
 ***/
void GUI::init_window(GtkBuilder *builder)
{
    GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
    gtk_builder_connect_signals(builder, NULL);
    g_signal_connect(window , "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkMenuItem *menuExit = GTK_MENU_ITEM(gtk_builder_get_object(builder, "menuExit"));
    g_signal_connect(menuExit, "activate", G_CALLBACK(gtk_main_quit), NULL);

    ls.store = GTK_LIST_STORE(gtk_builder_get_object(builder, "liststore1"));
    ls.view = GTK_TREE_VIEW(gtk_builder_get_object(builder, "treeview1"));
    ls.col1 = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "treeviewcolumn1"));
    ls.col2 = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "treeviewcolumn2"));
    ls.crt1 = GTK_CELL_RENDERER(gtk_builder_get_object(builder, "cr1"));
    ls.crt2 = GTK_CELL_RENDERER(gtk_builder_get_object(builder, "cr2"));

    gtk_tree_view_column_add_attribute(ls.col1, ls.crt1, "text", KEY_COLUMN);
    gtk_tree_view_column_add_attribute(ls.col2, ls.crt2, "text", PASS_COLUMN);
    gtk_tree_view_set_grid_lines(ls.view, GTK_TREE_VIEW_GRID_LINES_NONE);

    GError *err = NULL;
    gtk_window_set_default_icon_from_file(SpicyPass_LOGO_FILE_PATH, &err);

    if (err) {
        fprintf(stderr, "%s\n", err->message);
    }

    gtk_widget_show(window);
}

int GUI::load(Pass_Store &p)
{
    return password_prompt(p, ls);
}

int GUI::load_new(Pass_Store &p, GtkBuilder *builder)
{
    GtkWidget *newPwWindow = GTK_WIDGET(gtk_builder_get_object(builder, "newPwWindow"));
    GtkEntry *newPwEntry1 = GTK_ENTRY(gtk_builder_get_object(builder, "newPwEntry1"));
    GtkEntry *newPwEntry2 = GTK_ENTRY(gtk_builder_get_object(builder, "newPwEntry2"));
    gtk_entry_set_visibility(newPwEntry1, false);
    gtk_entry_set_visibility(newPwEntry2, false);
    gtk_entry_set_max_length(newPwEntry1, MAX_STORE_PASSWORD_SIZE);
    gtk_entry_set_max_length(newPwEntry2, MAX_STORE_PASSWORD_SIZE);

    GtkButton *newPwButtonEnter = GTK_BUTTON(gtk_builder_get_object(builder, "newPwButtonEnter"));
    GtkButton *newPwButtonQuit = GTK_BUTTON(gtk_builder_get_object(builder, "newPwButtonQuit"));

    struct Callback_Data *cb_data = (struct Callback_Data *) calloc(1, sizeof(struct Callback_Data));

    if (cb_data == NULL) {
        cerr << "calloc() failed in GUI::load_new()" << endl;
        return -1;
    }

    cb_data->window = newPwWindow;
    cb_data->widget1 = GTK_WIDGET(newPwEntry1);
    cb_data->widget2 = GTK_WIDGET(newPwEntry2);
    cb_data->ls = &ls;
    cb_data->p = &p;

    g_signal_connect(newPwButtonEnter, "clicked", G_CALLBACK(on_newPwButtonEnter_clicked), cb_data);
    g_signal_connect(newPwButtonQuit, "clicked", G_CALLBACK(on_quit), cb_data);
    g_signal_connect(newPwEntry1, "activate", G_CALLBACK(on_key_enter), newPwButtonEnter);

    gtk_widget_show(newPwWindow);

    return 0;
}

void GUI::run(Pass_Store &p)
{
    GtkBuilder *builder = gtk_builder_new_from_file(GLADE_FILE_PATH);

    struct Callback_Data *cb_data = (struct Callback_Data *) calloc(1, sizeof(struct Callback_Data));

    if (cb_data == NULL) {
        cerr << "calloc() failed in GUI::run()" << endl;
        return;
    }

    init_window(builder);

    if (first_time_run()) {
        if (load_new(p, builder) != 0) {
            cerr << "load_new() failed in GUI::run()" << endl;
            free(cb_data);
            return;
        }
    } else if (load(p) != 0) {
        cerr << "load failed in GUI::run()" << endl;
        free(cb_data);
        return;
    }

    GtkButton *buttonAdd = GTK_BUTTON(gtk_builder_get_object(builder, "buttonAdd"));
    GtkButton *buttonDelete = GTK_BUTTON(gtk_builder_get_object(builder, "buttonDelete"));
    GtkButton *buttonCopy = GTK_BUTTON(gtk_builder_get_object(builder, "buttonCopy"));
    GtkToggleButton *buttonShowPass = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "buttonShowPass"));
    GtkMenuItem *menuChangePass = GTK_MENU_ITEM(gtk_builder_get_object(builder, "menuChangePass"));
    GtkMenuItem *menuPassGen = GTK_MENU_ITEM(gtk_builder_get_object(builder, "menuPassGen"));
    GtkMenuItem *menuAbout = GTK_MENU_ITEM(gtk_builder_get_object(builder, "menuAbout"));

    cb_data->ls = &ls;
    cb_data->p = &p;
    cb_data->buttonShowPass = buttonShowPass;

    g_signal_connect(buttonAdd, "clicked", G_CALLBACK(on_buttonAdd_clicked), cb_data);
    g_signal_connect(buttonDelete, "clicked", G_CALLBACK(on_buttonDelete_clicked), cb_data);
    g_signal_connect(buttonCopy, "clicked", G_CALLBACK(on_buttonCopy_clicked), cb_data);
    g_signal_connect(buttonShowPass, "toggled", G_CALLBACK(on_buttonShowPass_toggled), cb_data);
    g_signal_connect(menuChangePass, "activate", G_CALLBACK(on_menuChangePassword_activate), cb_data);
    g_signal_connect(menuPassGen, "activate", G_CALLBACK(on_menuPassGen_activate), cb_data);
    g_signal_connect(menuAbout, "activate", G_CALLBACK(on_menuAbout_activate), NULL);

    gtk_main();

    g_object_unref(builder);

    free(cb_data);
}

#endif // GUI_SUPPORT