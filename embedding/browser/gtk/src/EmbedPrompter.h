/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * Christopher Blizzard. Portions created by Christopher Blizzard are Copyright (C) Christopher Blizzard.  All Rights Reserved.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 *   Brian Ryner <bryner@brianryner.com>
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

#ifdef MOZILLA_INTERNAL_API
#include "nsString.h"
#include "nsReadableUtils.h"
#else
#include "nsStringAPI.h"
#endif
#include <gtk/gtk.h>

#include <stdlib.h>
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
    void     SetTextValue(const PRUnichar *aTextValue);
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
