/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Coporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "EmbedPrompter.h"
#include "nsReadableUtils.h"

enum {
    INCLUDE_USERNAME = 1 << 0,
    INCLUDE_PASSWORD = 1 << 1,
    INCLUDE_CHECKBOX = 1 << 2,
    INCLUDE_CANCEL   = 1 << 3
};

struct DialogDescription {
    int    flags;
    gchar* icon;
};

// This table contains the optional widgets and icons associated with
// each type of dialog.

static const DialogDescription DialogTable[] = {
    { 0,                      GTK_STOCK_DIALOG_WARNING  },  // ALERT
    { INCLUDE_CHECKBOX,       GTK_STOCK_DIALOG_WARNING  },  // ALERT_CHECK
    { INCLUDE_CANCEL,         GTK_STOCK_DIALOG_QUESTION },  // CONFIRM
    { INCLUDE_CHECKBOX |
      INCLUDE_CANCEL,         GTK_STOCK_DIALOG_QUESTION },  // CONFIRM_CHECK
    { INCLUDE_CANCEL |
      INCLUDE_CHECKBOX,       GTK_STOCK_DIALOG_QUESTION },  // PROMPT
    { INCLUDE_CANCEL |
      INCLUDE_USERNAME |
      INCLUDE_PASSWORD |
      INCLUDE_CHECKBOX,       GTK_STOCK_DIALOG_QUESTION },  // PROMPT_USER_PASS
    { INCLUDE_CANCEL |
      INCLUDE_PASSWORD |
      INCLUDE_CHECKBOX,       GTK_STOCK_DIALOG_QUESTION },  // PROMPT_PASS
    { INCLUDE_CANCEL,         GTK_STOCK_DIALOG_QUESTION },  // SELECT
    { INCLUDE_CANCEL |
      INCLUDE_CHECKBOX,       GTK_STOCK_DIALOG_QUESTION }   // UNIVERSAL
};

EmbedPrompter::EmbedPrompter(void)
    : mCheckValue(PR_FALSE),
      mItemList(nsnull),
      mItemCount(0),
      mButtonPressed(0),
      mConfirmResult(PR_FALSE),
      mSelectedItem(0),
      mWindow(NULL),
      mUserField(NULL),
      mPassField(NULL),
      mTextField(NULL),
      mOptionMenu(NULL),
      mCheckBox(NULL)
{
}

EmbedPrompter::~EmbedPrompter(void)
{
    if (mItemList)
        delete[] mItemList;
}

nsresult
EmbedPrompter::Create(PromptType aType, GtkWindow* aParentWindow)
{
    mWindow = gtk_dialog_new_with_buttons(mTitle.get(), aParentWindow,
                                          GTK_DIALOG_DESTROY_WITH_PARENT,
                                          NULL);

    // gtk will resize this for us as necessary
    gtk_window_set_default_size(GTK_WINDOW(mWindow), 100, 50);

    // this HBox will contain the icon, and a vbox which contains the
    // dialog text and other widgets.
    GtkWidget* dialogHBox = gtk_hbox_new(FALSE, 12);


    // Set up dialog properties according to the GNOME HIG
    // (http://developer.gnome.org/projects/gup/hig/1.0/windows.html#alert-windows)

    gtk_container_set_border_width(GTK_CONTAINER(mWindow), 6);
    gtk_dialog_set_has_separator(GTK_DIALOG(mWindow), FALSE);
    gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(mWindow)->vbox), 12);
    gtk_container_set_border_width(GTK_CONTAINER(dialogHBox), 6);


    // This is the VBox which will contain the label and other controls.
    GtkWidget* contentsVBox = gtk_vbox_new(FALSE, 12);

    // get the stock icon for this dialog and put it in the box
    const gchar* iconDesc = DialogTable[aType].icon;
    GtkWidget* icon = gtk_image_new_from_stock(iconDesc, GTK_ICON_SIZE_DIALOG);
    gtk_misc_set_alignment(GTK_MISC(icon), 0.5, 0.0);
    gtk_box_pack_start(GTK_BOX(dialogHBox), icon, FALSE, FALSE, 0);

    // now pack the label into the vbox
    GtkWidget* label = gtk_label_new(mMessageText.get());
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_selectable(GTK_LABEL(label), TRUE);
    gtk_box_pack_start(GTK_BOX(contentsVBox), label, FALSE, FALSE, 0);

    int widgetFlags = DialogTable[aType].flags;

    if (widgetFlags & (INCLUDE_USERNAME | INCLUDE_PASSWORD)) {

        // If we're creating a username and/or password field, make an hbox
        // which will contain two vboxes, one for the labels and one for the
        // text fields.  This will let us line up the textfields.

        GtkWidget* userPassHBox = gtk_hbox_new(FALSE, 12);
        GtkWidget* userPassLabels = gtk_vbox_new(TRUE, 6);
        GtkWidget* userPassFields = gtk_vbox_new(TRUE, 6);

        if (widgetFlags & INCLUDE_USERNAME) {
            GtkWidget* userLabel = gtk_label_new("User Name:");
            gtk_box_pack_start(GTK_BOX(userPassLabels), userLabel, FALSE,
                               FALSE, 0);

            mUserField = gtk_entry_new();

            if (!mUser.IsEmpty())
                gtk_entry_set_text(GTK_ENTRY(mUserField), mUser.get());

            gtk_entry_set_activates_default(GTK_ENTRY(mUserField), TRUE);

            gtk_box_pack_start(GTK_BOX(userPassFields), mUserField, FALSE,
                               FALSE, 0);
        }
        if (widgetFlags & INCLUDE_PASSWORD) {
            GtkWidget* passLabel = gtk_label_new("Password:");
            gtk_box_pack_start(GTK_BOX(userPassLabels), passLabel, FALSE,
                               FALSE, 0);

            mPassField = gtk_entry_new();

            if (!mPass.IsEmpty())
                gtk_entry_set_text(GTK_ENTRY(mPassField), mPass.get());

            gtk_entry_set_visibility(GTK_ENTRY(mPassField), FALSE);
            gtk_entry_set_activates_default(GTK_ENTRY(mPassField), TRUE);

            gtk_box_pack_start(GTK_BOX(userPassFields), mPassField, FALSE,
                               FALSE, 0);
        }

        gtk_box_pack_start(GTK_BOX(userPassHBox), userPassLabels, FALSE,
                           FALSE, 0);
        gtk_box_pack_start(GTK_BOX(userPassHBox), userPassFields, FALSE,
                           FALSE, 0);
        gtk_box_pack_start(GTK_BOX(contentsVBox), userPassHBox, FALSE, FALSE, 0);
    }

    if (aType == TYPE_PROMPT) {
        mTextField = gtk_entry_new();

        if (!mTextValue.IsEmpty())
            gtk_entry_set_text(GTK_ENTRY(mTextField), mTextValue.get());

        gtk_entry_set_activates_default(GTK_ENTRY(mTextField), TRUE);

        gtk_box_pack_start(GTK_BOX(contentsVBox), mTextField, FALSE, FALSE, 0);
    }

    // Add a checkbox
    if ((widgetFlags & INCLUDE_CHECKBOX) && !mCheckMessage.IsEmpty()) {
        mCheckBox = gtk_check_button_new_with_label(mCheckMessage.get());

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mCheckBox),
                                     mCheckValue);

        gtk_box_pack_start(GTK_BOX(contentsVBox), mCheckBox, FALSE, FALSE, 0);
    }

    // Add a dropdown menu
    if (aType == TYPE_SELECT) {
        // Build up a GtkMenu containing the items
        GtkWidget* menu = gtk_menu_new();
        for (PRUint32 i = 0; i < mItemCount; ++i) {
            GtkWidget* item = gtk_menu_item_new_with_label(mItemList[i].get());
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        }

        // Now create an OptionMenu and set this as the menu
        mOptionMenu = gtk_option_menu_new();

        gtk_option_menu_set_menu(GTK_OPTION_MENU(mOptionMenu), menu);
        gtk_box_pack_start(GTK_BOX(contentsVBox), mOptionMenu, FALSE, FALSE, 0);
    }

    if (aType == TYPE_UNIVERSAL) {
        // Create buttons based on the flags passed in.
        for (int i = EMBED_MAX_BUTTONS; i >= 0; --i) {
            if (!mButtonLabels[i].IsEmpty())
                gtk_dialog_add_button(GTK_DIALOG(mWindow),
                                      mButtonLabels[i].get(), i);
        }
        gtk_dialog_set_default_response(GTK_DIALOG(mWindow), 0);
    } else {
        // Create standard ok and cancel buttons
        if (widgetFlags & INCLUDE_CANCEL)
            gtk_dialog_add_button(GTK_DIALOG(mWindow), GTK_STOCK_CANCEL,
                                  GTK_RESPONSE_CANCEL);

        GtkWidget* okButton = gtk_dialog_add_button(GTK_DIALOG(mWindow),
                                                    GTK_STOCK_OK,
                                                    GTK_RESPONSE_ACCEPT);
        gtk_widget_grab_default(okButton);
    }

    // Pack the contentsVBox into the dialogHBox and the dialog.
    gtk_box_pack_start(GTK_BOX(dialogHBox), contentsVBox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(mWindow)->vbox), dialogHBox, FALSE,
                       FALSE, 0);

    return NS_OK;
}

void
EmbedPrompter::SetTitle(const PRUnichar *aTitle)
{
    CopyUTF16toUTF8(aTitle, mTitle);
}

void
EmbedPrompter::SetTextValue(const PRUnichar *aTextValue)
{
    CopyUTF16toUTF8(aTextValue, mTextValue);
}

void
EmbedPrompter::SetCheckMessage(const PRUnichar *aMessage)
{
    CopyUTF16toUTF8(aMessage, mCheckMessage);
}

void
EmbedPrompter::SetMessageText(const PRUnichar *aMessageText)
{
    CopyUTF16toUTF8(aMessageText, mMessageText);
}

void
EmbedPrompter::SetUser(const PRUnichar *aUser)
{
    CopyUTF16toUTF8(aUser, mUser);
}

void
EmbedPrompter::SetPassword(const PRUnichar *aPass)
{
    CopyUTF16toUTF8(aPass, mPass);
}

void
EmbedPrompter::SetCheckValue(const PRBool aValue)
{
    mCheckValue = aValue;
}

void
EmbedPrompter::SetItems(const PRUnichar** aItemArray, PRUint32 aCount)
{
    if (mItemList)
        delete[] mItemList;

    mItemCount = aCount;
    mItemList = new nsCString[aCount];
    for (PRUint32 i = 0; i < aCount; ++i)
        CopyUTF16toUTF8(aItemArray[i], mItemList[i]);
}

void
EmbedPrompter::SetButtons(const PRUnichar* aButton0Label,
                          const PRUnichar* aButton1Label,
                          const PRUnichar* aButton2Label)
{
    CopyUTF16toUTF8(aButton0Label, mButtonLabels[0]);
    CopyUTF16toUTF8(aButton1Label, mButtonLabels[1]);
    CopyUTF16toUTF8(aButton2Label, mButtonLabels[2]);
}

void
EmbedPrompter::GetCheckValue(PRBool *aValue)
{
    *aValue = mCheckValue;
}

void
EmbedPrompter::GetConfirmValue(PRBool *aConfirmValue)
{
    *aConfirmValue = mConfirmResult;
}
 
void
EmbedPrompter::GetTextValue(PRUnichar **aTextValue)
{
    *aTextValue = ToNewUnicode(mTextValue);
}

void
EmbedPrompter::GetUser(PRUnichar **aUser)
{
    *aUser = ToNewUnicode(mUser);
}

void
EmbedPrompter::GetPassword(PRUnichar **aPass)
{
    *aPass = ToNewUnicode(mPass);
}

void
EmbedPrompter::GetSelectedItem(PRInt32 *aIndex)
{
    *aIndex = mSelectedItem;
}

void
EmbedPrompter::GetButtonPressed(PRInt32 *aButton)
{
    *aButton = mButtonPressed;
}

void
EmbedPrompter::Run(void)
{
    gtk_widget_show_all(mWindow);
    gint response = gtk_dialog_run(GTK_DIALOG(mWindow));
    switch (response) {
    case GTK_RESPONSE_NONE:
    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_DELETE_EVENT:
        mConfirmResult = PR_FALSE;
        break;
    case GTK_RESPONSE_ACCEPT:
        mConfirmResult = PR_TRUE;
        SaveDialogValues();
        break;
    default:
        mButtonPressed = response;
        SaveDialogValues();
    }

    gtk_widget_destroy(mWindow);
}

void
EmbedPrompter::SaveDialogValues()
{
    if (mUserField)
        mUser.Assign(gtk_entry_get_text(GTK_ENTRY(mUserField)));

    if (mPassField)
        mPass.Assign(gtk_entry_get_text(GTK_ENTRY(mPassField)));

    if (mCheckBox)
        mCheckValue = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mCheckBox));

    if (mTextField)
        mTextValue.Assign(gtk_entry_get_text(GTK_ENTRY(mTextField)));

    if (mOptionMenu)
        mSelectedItem = gtk_option_menu_get_history(GTK_OPTION_MENU(mOptionMenu));
}
