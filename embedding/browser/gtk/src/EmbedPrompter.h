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
 */

#include <nsCOMPtr.h>
#include <nsString.h>
#include <gtk/gtk.h>

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
    TYPE_UNIVERSAL };

  nsresult Create(PromptType aType);
  void     SetTitle(const PRUnichar *aTitle);
  void     SetTextValue (const PRUnichar *aTextValue);
  void     SetCheckMessage(const PRUnichar *aCheckMessage);
  void     SetCheckValue(const PRBool aValue);
  void     SetMessageText(const PRUnichar *aMessageText);
  void     SetUser(const PRUnichar *aUser);
  void     SetPassword(const PRUnichar *aPass);

  void     GetCheckValue(PRBool *aValue);
  void     GetConfirmValue(PRBool *aConfirmValue);
  void     GetTextValue(PRUnichar **aTextValue);
  void     GetUser(PRUnichar **aUser);
  void     GetPassword(PRUnichar **aPass);

  void     Run(void);

  void     UserCancel(void);
  void     UserOK(void);

 private:

  enum {
    INCLUDE_USERNAME    = 1U,
    INCLUDE_CHECKBOX    = 2U,
    INCLUDE_TEXTFIELD   = 4U,
    INCLUDE_CANCEL      = 8U
  };
  
  void     CreatePasswordPrompter(int aFlags);
  void     CreateAlertPrompter(int aFlags);

  nsCString  mTitle;
  nsCString  mMessageText;
  nsCString  mTextValue;
  nsCString  mCheckMessage;
  PRBool     mCheckValue;

  PRBool     mConfirmResult;
  nsCString  mUser;
  nsCString  mPass;

  GtkWidget *mWindow;
  GtkWidget *mUserField;
  GtkWidget *mPassField;
  GtkWidget *mTextField;
  GtkWidget *mCheckBox;

};
