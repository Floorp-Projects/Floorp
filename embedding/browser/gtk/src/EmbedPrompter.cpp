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

#include "EmbedPrompter.h"
#include "nsReadableUtils.h"

// call backs from gtk widgets

gboolean
toplevel_delete_cb(GtkWidget *aWidget, GdkEventAny *aEvent,
		   EmbedPrompter *aPrompter);

gboolean
ok_clicked_cb(GtkButton *button, EmbedPrompter *aPrompter);

gboolean
cancel_clicked_cb(GtkButton *button, EmbedPrompter *aPrompter);

EmbedPrompter::EmbedPrompter(void)
{
  mCheckValue    = PR_FALSE;
  mConfirmResult = PR_FALSE;

  mWindow        = nsnull;
  mUserField     = nsnull;
  mPassField     = nsnull;
  mTextField     = nsnull;
  mCheckBox      = nsnull;
}

EmbedPrompter::~EmbedPrompter(void)
{
}

nsresult
EmbedPrompter::Create(PromptType aType)
{
  nsresult rv = NS_OK;

  int includeCheckFlag = 0;

  switch (aType) {
  case TYPE_PROMPT_USER_PASS:
    if (mCheckMessage.Length())
      includeCheckFlag = EmbedPrompter::INCLUDE_CHECKBOX;
    CreatePasswordPrompter(EmbedPrompter::INCLUDE_USERNAME | 
			   includeCheckFlag);
    break;
  case TYPE_PROMPT_PASS:
    if (mCheckMessage.Length())
      includeCheckFlag = EmbedPrompter::INCLUDE_CHECKBOX;
    CreatePasswordPrompter(includeCheckFlag);
    break;
  case TYPE_ALERT:
    CreateAlertPrompter(0);
    break;
  case TYPE_CONFIRM:
    CreateAlertPrompter(EmbedPrompter::INCLUDE_CANCEL);
    break;
  case TYPE_CONFIRM_CHECK:
    CreateAlertPrompter(EmbedPrompter::INCLUDE_CANCEL |
			EmbedPrompter::INCLUDE_CHECKBOX);
    break;
  case TYPE_ALERT_CHECK:
    CreateAlertPrompter(EmbedPrompter::INCLUDE_CHECKBOX);
    break;
  case TYPE_PROMPT:
    if (mCheckMessage.Length())
      includeCheckFlag = EmbedPrompter::INCLUDE_CHECKBOX;
    CreateAlertPrompter(EmbedPrompter::INCLUDE_CANCEL |
			EmbedPrompter::INCLUDE_TEXTFIELD |
			includeCheckFlag);
    break;
  default:
    rv = NS_ERROR_NOT_IMPLEMENTED;
    break;
  }
  return rv;
}

void
EmbedPrompter::SetTitle(const PRUnichar *aTitle)
{
  mTitle.AssignWithConversion(aTitle);
}

void
EmbedPrompter::SetTextValue(const PRUnichar *aTextValue)
{
  mTextValue.AppendWithConversion(aTextValue);
}

void
EmbedPrompter::SetCheckMessage(const PRUnichar *aMessage)
{
  mCheckMessage.AppendWithConversion(aMessage);
}

void
EmbedPrompter::SetMessageText(const PRUnichar *aMessageText)
{
  mMessageText.AppendWithConversion(aMessageText);
}

void
EmbedPrompter::SetUser(const PRUnichar *aUser)
{
  mUser.AppendWithConversion(aUser);
}

void
EmbedPrompter::SetPassword(const PRUnichar *aPass)
{
  mPass.AppendWithConversion(aPass);
}

void
EmbedPrompter::SetCheckValue(const PRBool aValue)
{
  mCheckValue = aValue;
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
EmbedPrompter::Run(void)
{
  gtk_widget_show_all(mWindow);
  gtk_grab_add(mWindow);
  gtk_main();
}

void
EmbedPrompter::UserCancel(void)
{
  // someone pressed cancel or else they closed the window which is
  // the same as a cancel.
  mConfirmResult = PR_FALSE;

  gtk_grab_remove(mWindow);
  gtk_main_quit();

  // destroy all of our widgets
  gtk_widget_destroy(mWindow);

  mWindow = nsnull;
  mUserField = nsnull;
  mPassField = nsnull;
  mTextField = nsnull;
  mCheckBox = nsnull;

}

void
EmbedPrompter::UserOK(void)
{
  // someone pressed OK
  mConfirmResult = PR_TRUE;

  // save all of the data
  if (mUserField) {
    gchar *user;
    user = gtk_editable_get_chars(GTK_EDITABLE(mUserField), 0, -1);
    mUser.Assign(user);
    g_free(user);
  }

  if (mPassField) {
    gchar *pass;
    pass = gtk_editable_get_chars(GTK_EDITABLE(mPassField), 0, -1);
    mPass.Assign(pass);
    g_free(pass);
  }

  if (mCheckBox)
    mCheckValue = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mCheckBox));

  if (mTextField) {
    gchar *text;
    text = gtk_editable_get_chars(GTK_EDITABLE(mTextField), 0, -1);
    mTextValue.Assign(text);
    g_free(text);
  }
  
  gtk_grab_remove(mWindow);
  gtk_main_quit();

  // destroy all of our widgets
  gtk_widget_destroy(mWindow);

  mWindow = nsnull;
  mUserField = nsnull;
  mPassField = nsnull;
  mTextField = nsnull;
  mCheckBox = nsnull;

}



void
EmbedPrompter::CreatePasswordPrompter(int aFlags)
{
  // toplevel window
  mWindow = gtk_window_new(GTK_WINDOW_DIALOG);
  
  // toplevel vbox
  GtkBox *topLevelVBox = GTK_BOX(gtk_vbox_new(FALSE, /* homogeneous */
					      3));    /* spacing */

  // add it to the window
  gtk_container_add(GTK_CONTAINER(mWindow),
		    GTK_WIDGET(topLevelVBox));

  // set our border width
  gtk_container_set_border_width(GTK_CONTAINER(mWindow),
				 4);

  GtkWidget *msgLabel = gtk_label_new(mMessageText.get());
  // add it
  gtk_box_pack_start(topLevelVBox,
		     msgLabel,
		     FALSE, /* expand */
		     FALSE, /* fill */
		     0);    /* padding */

  PRInt32 startPos;

  if (aFlags & EmbedPrompter::INCLUDE_USERNAME) {
    // the username label
    GtkWidget *userLabel = gtk_label_new("User Name");
    gtk_box_pack_start(topLevelVBox,
		       userLabel,
		       FALSE, /* expand */
		       FALSE, /* fill */
		       0);    /* padding */
    // the username text area
    mUserField = gtk_entry_new();
    if (mUser.Length()) {
      startPos = 0;
      gtk_editable_insert_text(GTK_EDITABLE(mUserField),
			       mUser.get(), mUser.Length(),
			       &startPos);
    }
    gtk_box_pack_start(topLevelVBox,
		       mUserField,
		       FALSE, /* expand */
		       FALSE, /* fill */
		       0);    /* padding */
  }

  // password label
  GtkWidget *passLabel = gtk_label_new("Password");
  gtk_box_pack_start(topLevelVBox,
		   passLabel,
		   FALSE, /* expand */
		   FALSE, /* fill */
		   0);    /* padding */

  // password text field
  mPassField = gtk_entry_new();
  // it's a password field
  gtk_entry_set_visibility(GTK_ENTRY(mPassField), FALSE);
  if (mPass.Length()) {
    startPos = 0;
    gtk_editable_insert_text(GTK_EDITABLE(mPassField),
			     mPass.get(), mPass.Length(),
			     &startPos);
  }
  // add it
  gtk_box_pack_start(topLevelVBox,
		   mPassField,
		   FALSE, /* expand */
		   FALSE, /* fill */
		   0);    /* padding */

  // password manager field
  if (aFlags & EmbedPrompter::INCLUDE_CHECKBOX) {
    // make it
    mCheckBox =
      gtk_check_button_new_with_label(mCheckMessage.get());
    // set its state
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mCheckBox),
				 mCheckValue);
    // add it
    gtk_box_pack_start(topLevelVBox,
		     mCheckBox,
		     FALSE, /* expand */
		     FALSE, /* fill */
		     0);    /* padding */
  }

  // gtk button box for the OK and Cancel buttons
  GtkButtonBox *buttonBox = GTK_BUTTON_BOX(gtk_hbutton_box_new());
  gtk_button_box_set_layout(buttonBox, GTK_BUTTONBOX_SPREAD);

  gtk_box_pack_start(topLevelVBox,
		     GTK_WIDGET(buttonBox),
		     FALSE, /* expand */
		     TRUE,  /* fill */
		     0);    /* padding */

  // OK
  GtkWidget *okButton = gtk_button_new_with_label("OK");
  gtk_box_pack_start(GTK_BOX(buttonBox),
		     okButton,
		     FALSE, /* expand */
		     TRUE,  /* fill */
		     0);    /* padding */

  // cancel
  GtkWidget *cancelButton = gtk_button_new_with_label("Cancel");
  gtk_box_pack_start(GTK_BOX(buttonBox),
		     cancelButton,
		     FALSE, /* expand */
		     TRUE,  /* fill */
		     0);    /* padding */
  
  // hook up signals
  gtk_signal_connect(GTK_OBJECT(mWindow), "delete_event",
		     GTK_SIGNAL_FUNC(toplevel_delete_cb),
		     this);
  
  gtk_signal_connect(GTK_OBJECT(okButton), "clicked",
		     GTK_SIGNAL_FUNC(ok_clicked_cb), this);
  gtk_signal_connect(GTK_OBJECT(cancelButton), "clicked",
		     GTK_SIGNAL_FUNC(cancel_clicked_cb), this);
}

void
EmbedPrompter::CreateAlertPrompter(int aFlags)
{
    // toplevel window
  mWindow = gtk_window_new(GTK_WINDOW_DIALOG);
  
  // toplevel vbox
  GtkBox *topLevelVBox = GTK_BOX(gtk_vbox_new(FALSE, /* homogeneous */
					      3));    /* spacing */

  // add it to the window
  gtk_container_add(GTK_CONTAINER(mWindow),
		    GTK_WIDGET(topLevelVBox));

  // set our border width
  gtk_container_set_border_width(GTK_CONTAINER(mWindow),
				 4);

  // create our label
  GtkWidget *label = gtk_label_new(mMessageText.get());
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);

  // add it
  gtk_box_pack_start(topLevelVBox,
		     label,
		     TRUE, /* expand */
		     TRUE, /* fill */
		     0);    /* padding */

  // text field
  if (aFlags & EmbedPrompter::INCLUDE_TEXTFIELD) {
    mTextField = gtk_entry_new();
    if (mTextValue.Length()) {
      int startPos = 0;
      gtk_editable_insert_text(GTK_EDITABLE(mTextField),
			       mTextValue.get(), mTextValue.Length(),
			       &startPos);
    }
    gtk_box_pack_start(topLevelVBox,
		       mTextField,
		       FALSE, /* expand */
		       FALSE, /* fill */
		       0);    /* padding */
  }

  if (aFlags & EmbedPrompter::INCLUDE_CHECKBOX) {
    // make it
    mCheckBox = gtk_check_button_new_with_label(mCheckMessage);
    // set its state
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mCheckBox),
				 mCheckValue);
    // add it
    gtk_box_pack_start(topLevelVBox,
		       mCheckBox,
		       FALSE, /* expand */
		       FALSE, /* fill */
		       0);    /* padding */
  }

  // gtk button box for the OK button
  GtkButtonBox *buttonBox = GTK_BUTTON_BOX(gtk_hbutton_box_new());
  gtk_button_box_set_layout(buttonBox, GTK_BUTTONBOX_SPREAD);

  gtk_box_pack_start(topLevelVBox,
		     GTK_WIDGET(buttonBox),
		     FALSE, /* expand */
		     FALSE,  /* fill */
		     0);    /* padding */

  // OK
  GtkWidget *okButton = gtk_button_new_with_label("OK");
  gtk_box_pack_start(GTK_BOX(buttonBox),
		     okButton,
		     FALSE, /* expand */
		     TRUE,  /* fill */
		     0);    /* padding */

  if (aFlags & EmbedPrompter::INCLUDE_CANCEL) {
    // cancel
    GtkWidget *cancelButton = gtk_button_new_with_label("Cancel");
    gtk_box_pack_start(GTK_BOX(buttonBox),
		       cancelButton,
		       FALSE, /* expand */
		       TRUE,  /* fill */
		       0);    /* padding */

    // hook up the signal here instead of later since the object will
    // go outta scope.
    gtk_signal_connect(GTK_OBJECT(cancelButton), "clicked",
		       GTK_SIGNAL_FUNC(cancel_clicked_cb), this);
  }
  

  // hook up signals
  gtk_signal_connect(GTK_OBJECT(mWindow), "delete_event",
		     GTK_SIGNAL_FUNC(toplevel_delete_cb),
		     this);
  
  gtk_signal_connect(GTK_OBJECT(okButton), "clicked",
		     GTK_SIGNAL_FUNC(ok_clicked_cb), this);
}

gboolean
toplevel_delete_cb(GtkWidget *aWidget, GdkEventAny *aEvent,
		   EmbedPrompter *aPrompter)
{
  aPrompter->UserCancel();
  return TRUE;
}

gboolean
ok_clicked_cb(GtkButton *button, EmbedPrompter *aPrompter)
{
  aPrompter->UserOK();
  return TRUE;
}

gboolean
cancel_clicked_cb(GtkButton *button, EmbedPrompter *aPrompter)
{
  aPrompter->UserCancel();
  return TRUE;
}
