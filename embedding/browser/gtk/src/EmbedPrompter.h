/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4: */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 *   Brian Ryner <bryner@brianryner.com>
 */

#include <nsString.h>
#include <gtk/gtk.h>

#define EMBED_MAX_BUTTONS 3

class EmbedPrompter {

public:

    EmbedPrompter();
    ~EmbedPrompter();

    enum PromptType {
        TYPE_ALERT,
        TYPE_ALERT_CHECK,
        TYPE_CONFIRM,
        TYPE_CONFIRM_CHECK,
        TYPE_PROMPT,
        TYPE_PROMPT_USER_PASS,
        TYPE_PROMPT_PASS,
        TYPE_SELECT,
        TYPE_UNIVERSAL
    };

    nsresult Create(PromptType aType, GtkWindow* aParentWindow);
    void     SetTitle(const PRUnichar *aTitle);
    void     SetTextValue (const PRUnichar *aTextValue);
    void     SetCheckMessage(const PRUnichar *aCheckMessage);
    void     SetCheckValue(const PRBool aValue);
    void     SetMessageText(const PRUnichar *aMessageText);
    void     SetUser(const PRUnichar *aUser);
    void     SetPassword(const PRUnichar *aPass);
    void     SetButtons(const PRUnichar* aButton0Label,
                        const PRUnichar* aButton1Label,
                        const PRUnichar* aButton2Label);
    void     SetItems(const PRUnichar **aItemArray, PRUint32 aCount);

    void     GetCheckValue(PRBool *aValue);
    void     GetConfirmValue(PRBool *aConfirmValue);
    void     GetTextValue(PRUnichar **aTextValue);
    void     GetUser(PRUnichar **aUser);
    void     GetPassword(PRUnichar **aPass);
    void     GetButtonPressed(PRInt32 *aButton);
    void     GetSelectedItem(PRInt32 *aIndex);

    void     Run(void);

private:

    void     SaveDialogValues();

    nsCString    mTitle;
    nsCString    mMessageText;
    nsCString    mTextValue;
    nsCString    mCheckMessage;
    PRBool       mCheckValue;
    nsCString    mUser;
    nsCString    mPass;
    nsCString    mButtonLabels[EMBED_MAX_BUTTONS];
    nsCString   *mItemList;
    PRUint32     mItemCount;

    PRInt32      mButtonPressed;
    PRBool       mConfirmResult;
    PRInt32      mSelectedItem;

    GtkWidget   *mWindow;
    GtkWidget   *mUserField;
    GtkWidget   *mPassField;
    GtkWidget   *mTextField;
    GtkWidget   *mOptionMenu;
    GtkWidget   *mCheckBox;
};
