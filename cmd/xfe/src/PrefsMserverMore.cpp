/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* 
   PrefsMserverMore.cpp -- The more dialog for Mail server preference
   Created: Linda Wei <lwei@netscape.com>, 23-Oct-96.
 */



#include "felocale.h"
#include "structs.h"
#include "fonts.h"
#include "xpassert.h"
#include "xfe.h"
#include "e_kit.h"
#include "prefapi.h"
#include "PrefsMserverMore.h"

#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/ToggleB.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/TextF.h> 
#include <Xm/ToggleBG.h> 
#include <Xfe/Xfe.h>

extern int XFE_ERROR_SAVING_OPTIONS;
extern int XFE_MAIL_DIR;
extern int XFE_WARNING;
extern int XFE_DIR_DOES_NOT_EXIST;
extern int XFE_EMPTY_DIR;

extern "C"
{
	char *XP_GetString(int i);
	void fe_installMserverMore();
	void fe_browse_file_of_text(MWContext *context, Widget text_field, Boolean dirp);
}

// ==================== Public Member Functions ====================

// Member:       XFE_PrefsMserverMoreDialog
// Description:  Constructor
// Inputs:
// Side effects: Creates the More dialog for Mail Server Preferences

XFE_PrefsMserverMoreDialog::XFE_PrefsMserverMoreDialog(XFE_PrefsDialog *prefsDialog,
													   Widget     parent,      // dialog parent
													   char      *name,        // dialog name
													   Boolean    modal)       // modal dialog?
	: XFE_Dialog(parent, 
				 name,
				 TRUE,     // ok
				 TRUE,     // cancel
				 FALSE,    // help
				 FALSE,    // apply
				 FALSE,    // separator
				 modal     // modal
				 ),
	  m_prefsDialog(prefsDialog),
	  m_prefsDataMserverMore(0)
{
	PrefsDataMserverMore *fep = NULL;

	fep = new PrefsDataMserverMore;
	memset(fep, 0, sizeof(PrefsDataMserverMore));
	m_prefsDataMserverMore = fep;
	
	Widget     form;
	Widget     frame1;
	Widget     frame2;
	Widget     form1;
	Widget     form2;
	Widget     mail_dir_label;
	Widget     mail_dir_text;
	Widget     mail_dir_browse_button;
	Widget     imap_mail_dir_label;
	Widget     imap_mail_dir_text;
	Widget     imap_mail_local_dir_label;
	Widget     imap_mail_local_dir_text;
	Widget     imap_mail_local_dir_browse_button;
	Widget     check_for_mail_toggle;
	Widget     check_mail_interval_text;
	Widget     minutes_label;
#if 0
	Widget     encrypted_passwd_toggle;
#endif
	Widget     remember_email_passwd_toggle;
#if 0
	Widget     enable_biff_toggle;
#endif
	Widget     kids[100];
	Arg        av[50];
	int        ac;
	int        i;

	form = XtVaCreateWidget("form", xmFormWidgetClass, m_chrome,
							XmNmarginWidth, 8,
							XmNtopAttachment, XmATTACH_FORM,
							XmNleftAttachment, XmATTACH_FORM,
							XmNrightAttachment, XmATTACH_FORM,
							XmNbottomAttachment, XmATTACH_FORM,
							NULL);
	XtManageChild (form);

	ac = 0;
	XtSetArg (av [ac], XmNmarginWidth, 8); ac++;
	XtSetArg (av [ac], XmNmarginHeight, 4); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame1 = XmCreateFrame (form, "dirFrame", av, ac);

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	form1 = XmCreateForm (frame1, "dirBox", av, ac);
	
	// Mail directory

	ac = 0;
	i = 0;

	kids[i++] = mail_dir_label =
		XmCreateLabelGadget(form1, "mailDirLabel", av, ac);

	kids[i++] = mail_dir_text =
		fe_CreateTextField(form1, "mailDirText", av, ac);

	kids[i++] = mail_dir_browse_button =
		XmCreatePushButtonGadget(form1, "mailDirBrowse", av, ac);

	kids[i++] = imap_mail_dir_label =
		XmCreateLabelGadget(form1, "imapMailDirLabel", av, ac);

	kids[i++] = imap_mail_dir_text =	
		fe_CreateTextField(form1, "imapMailDirText", av, ac);

	kids[i++] = imap_mail_local_dir_label =
		XmCreateLabelGadget(form1, "imapMailLocalDirLabel", av, ac);

	kids[i++] = imap_mail_local_dir_text =
		fe_CreateTextField(form1, "imapMailLocalDirText", av, ac);

	kids[i++] = imap_mail_local_dir_browse_button =
		XmCreatePushButtonGadget(form1, "imapMailLocalDirBrowse", av, ac);

	XtManageChildren(kids, i);
	XtManageChild(form1);
	XtManageChild(frame1);

	// bottom frame

	ac = 0;
	XtSetArg (av [ac], XmNmarginWidth, 8); ac++;
	XtSetArg (av [ac], XmNmarginHeight, 4); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg (av [ac], XmNtopWidget, frame1); ac++;
	XtSetArg (av [ac], XmNtopOffset, 8); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame2 = XmCreateFrame (form, "bottomFrame", av, ac);

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	form2 = XmCreateForm (frame2, "bottomBox", av, ac);
	
	ac = 0;
	i = 0;

	kids[i++] = check_for_mail_toggle =
		XmCreateToggleButtonGadget (form2, "checkMailToggle", av, ac);

	kids[i++] = remember_email_passwd_toggle = 
		XmCreateToggleButtonGadget (form2, "rememberEmailPasswdToggle", av, ac);

#if 0
	kids[i++] = encrypted_passwd_toggle = 
	XmCreateToggleButtonGadget (form2, "encryptPasswdToggle", av, ac);
#endif

	kids[i++] = minutes_label = 
		XmCreateLabelGadget(form2, "minutesLabel", av, ac);

	kids[i++] = check_mail_interval_text =
		fe_CreateTextField(form2, "checkMailIntText", av, ac);

#if 0
	kids[i++] = enable_biff_toggle = 
		XmCreateToggleButtonGadget(form2, "enableBiff", av, ac);
#endif

	XtManageChildren(kids, i);
	XtManageChild(form2);
	XtManageChild(frame2);

	int labels_width;
	labels_width = XfeVaGetWidestWidget(mail_dir_label,
										imap_mail_dir_label,
										imap_mail_local_dir_label,
										NULL);
	int labels_height2;
	labels_height2 = XfeVaGetTallestWidget(mail_dir_label,
										   mail_dir_text,
										   mail_dir_browse_button,
										   NULL);

	XtVaSetValues(mail_dir_label,
				  XmNheight, labels_height2,
				  RIGHT_JUSTIFY_VA_ARGS(mail_dir_label, labels_width),
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(mail_dir_text,
				  XmNcolumns, 30,
				  XmNheight, labels_height2,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, mail_dir_label,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, mail_dir_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(mail_dir_browse_button,
				  XmNuserData, mail_dir_text,
				  XmNheight, labels_height2,
				  XmNmarginWidth, 8,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, mail_dir_text,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, mail_dir_text,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(imap_mail_dir_label,
				  XmNheight, labels_height2,
				  RIGHT_JUSTIFY_VA_ARGS(imap_mail_dir_label, labels_width),
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, mail_dir_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(imap_mail_dir_text,
				  XmNcolumns, 30,
				  XmNheight, labels_height2,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, imap_mail_dir_label,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, mail_dir_text,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(imap_mail_local_dir_label,
				  XmNheight, labels_height2,
				  RIGHT_JUSTIFY_VA_ARGS(imap_mail_local_dir_label, labels_width),
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, imap_mail_dir_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(imap_mail_local_dir_text,
				  XmNcolumns, 30,
				  XmNheight, labels_height2,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, imap_mail_local_dir_label,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, mail_dir_text,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(imap_mail_local_dir_browse_button,
				  XmNuserData, imap_mail_local_dir_text,
				  XmNheight, labels_height2,
				  XmNmarginWidth, 8,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, imap_mail_local_dir_text,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, mail_dir_browse_button,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	int labels_height;
	labels_height = XfeVaGetTallestWidget(check_for_mail_toggle,
										  check_mail_interval_text,
										  minutes_label,
										  NULL);

	XtVaSetValues(check_for_mail_toggle,
				  XmNindicatorType, XmN_OF_MANY,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	XtVaSetValues(check_mail_interval_text,
				  XmNcolumns, 3,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, check_for_mail_toggle,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, check_for_mail_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	XtVaSetValues(minutes_label,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, check_for_mail_toggle,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, check_mail_interval_text,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
#if 0
	XtVaSetValues(enable_biff_toggle,
				  XmNindicatorType, XmN_OF_MANY,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, check_for_mail_toggle,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
#endif

	XtVaSetValues(remember_email_passwd_toggle,
				  XmNindicatorType, XmN_OF_MANY,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, check_for_mail_toggle,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

#if 0
	XtVaSetValues(encrypted_passwd_toggle,
				  XmNindicatorType, XmN_OF_MANY,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, remember_email_passwd_toggle,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	// mapi frame

	Widget frame3;
	Widget form3;

	ac = 0;
	XtSetArg (av [ac], XmNmarginWidth, 8); ac++;
	XtSetArg (av [ac], XmNmarginHeight, 4); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg (av [ac], XmNtopWidget, frame2); ac++;
	XtSetArg (av [ac], XmNtopOffset, 8); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame3 = XmCreateFrame (form, "mapiFrame", av, ac);

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	form3 = XmCreateForm (frame3, "mapiBox", av, ac);
	
	Widget mapi_label;
	Widget always_toggle;
	Widget never_toggle;

	ac = 0;
	i = 0;

	kids[i++] = mapi_label = 
		XmCreateLabelGadget(form3, "mapiLabel", av, ac);

	kids[i++] = always_toggle =
		XmCreateToggleButtonGadget(form3, "alwaysToggle", av, ac);

	kids[i++] = never_toggle = 
		XmCreateToggleButtonGadget(form3, "neverToggle", av, ac);

	XtVaSetValues(mapi_label,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(always_toggle,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, mapi_label,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, 16,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(never_toggle,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, always_toggle,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, always_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtManageChildren(kids, i);
	XtManageChild(form3);
	XtManageChild(frame3);
#endif

	fep->mail_dir_text = mail_dir_text;
	fep->browse_button = mail_dir_browse_button;
	fep->imap_mail_dir_text = imap_mail_dir_text;
	fep->imap_mail_local_dir_text = imap_mail_local_dir_text;
	fep->imap_mail_local_dir_browse_button = imap_mail_local_dir_browse_button;
	fep->check_for_mail_toggle = check_for_mail_toggle;
	fep->check_mail_interval_text = check_mail_interval_text;
	//	fep->encrypted_passwd_toggle = encrypted_passwd_toggle;
	fep->remember_email_passwd_toggle = remember_email_passwd_toggle;
	//	fep->always_toggle = always_toggle;
	//	fep->never_toggle = never_toggle;
#if 0
	fep->enable_biff_toggle = enable_biff_toggle;
#endif

	// Add callbacks

	XtAddCallback(m_chrome, XmNokCallback, cb_ok, this);
	XtAddCallback(m_chrome, XmNcancelCallback, cb_cancel, this);
	XtAddCallback(mail_dir_browse_button, XmNactivateCallback, cb_browseMailDir, this);
	XtAddCallback(imap_mail_local_dir_browse_button, XmNactivateCallback, cb_browseMailDir, this);
	//	XtAddCallback(always_toggle, XmNvalueChangedCallback, cb_toggleMapiServer, fep);
	//	XtAddCallback(never_toggle, XmNvalueChangedCallback, cb_toggleMapiServer, fep);
}

// Member:       ~XFE_PrefsMserverMoreDialog
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsMserverMoreDialog::~XFE_PrefsMserverMoreDialog()
{
	// Clean up

	delete m_prefsDataMserverMore;
}

// Member:       show
// Description:  Pop up dialog
// Inputs:
// Side effects: 

void XFE_PrefsMserverMoreDialog::show()
{
	// TODO: Initialize the dialog

	// Manage the top level

	XFE_Dialog::show();

	// Set focus to the OK button

	XmProcessTraversal(m_okButton, XmTRAVERSE_CURRENT);
}

// Member:       initPage
// Description:  Initializes page for MailNewsMserverMore
// Inputs:
// Side effects: 

void XFE_PrefsMserverMoreDialog::initPage()
{
	XP_ASSERT(m_prefsDataMserverMore);

	PrefsDataMserverMore  *fep = m_prefsDataMserverMore;
	XFE_GlobalPrefs       *prefs = &fe_globalPrefs;
	XP_Bool                b;
	char                   buf[1024];
	
	XtVaSetValues(fep->mail_dir_text, 
                  XmNsensitive, !PREF_PrefIsLocked("mail.directory"),
				  0);
	fe_SetTextField(fep->mail_dir_text, prefs->mail_directory);
	XtVaSetValues(fep->browse_button, 
                  XmNsensitive, !PREF_PrefIsLocked("mail.directory"),
				  0);
	XtVaSetValues(fep->imap_mail_dir_text, 
                  XmNsensitive, !PREF_PrefIsLocked("mail.imap.server_sub_directory"),
				  0);
	fe_SetTextField(fep->imap_mail_dir_text, prefs->imap_mail_directory);
	XtVaSetValues(fep->imap_mail_local_dir_text, 
                  XmNsensitive, !PREF_PrefIsLocked("mail.imap.root_dir"),
				  0);
	fe_SetTextField(fep->imap_mail_local_dir_text, prefs->imap_mail_local_directory);
	XtVaSetValues(fep->imap_mail_local_dir_browse_button, 
                  XmNsensitive, !PREF_PrefIsLocked("mail.imap.root_dir"),
				  0);

	b = prefs->auto_check_mail;
	XtVaSetValues(fep->check_for_mail_toggle, 
				  XmNset, b, 
                  XmNsensitive, !PREF_PrefIsLocked("mail.check_new_mail"),
				  0);
	PR_snprintf(buf, sizeof(buf), "%d", prefs->biff_interval);
	XtVaSetValues(fep->check_mail_interval_text,
				  XmNvalue, buf, 
                  XmNsensitive, !PREF_PrefIsLocked("mail.check_time"),
				  0);

#if 0
	XtVaSetValues(fep->enable_biff_toggle, 
                  XmNset, prefs->enable_biff, 
                  XmNsensitive, !PREF_PrefIsLocked("mail.play_sound"),
                  0);
#endif

	XtVaSetValues(fep->remember_email_passwd_toggle, 
				  XmNset, prefs->rememberPswd, 
                  XmNsensitive, !PREF_PrefIsLocked("mail.remember_password"),
				  0);

#if 0
	XtVaSetValues(fep->encrypted_passwd_toggle,
				  XmNset, prefs->support_skey, 
                  XmNsensitive, !PREF_PrefIsLocked("mail.support_skey"),
				  0);

	XtVaSetValues(fep->always_toggle, 
				  XmNset, prefs->use_ns_mapi_server, 
                  XmNsensitive, !PREF_PrefIsLocked("mail.use_mapi_server"),
				  0);
	XtVaSetValues(fep->never_toggle, 
				  XmNset, !prefs->use_ns_mapi_server, 
                  XmNsensitive, !PREF_PrefIsLocked("mail.use_mapi_server"),
				  0);
#endif
}

// Member:       verifyPage
// Description:  verify page for MailNewsMserverMore
// Inputs:
// Side effects: 

Boolean XFE_PrefsMserverMoreDialog::verifyPage()
{
	char         buf[10000];
	char        *buf2;
	char        *warning;
	struct stat  st;
	int          size;

	buf2 = buf;
	strcpy (buf, XP_GetString(XFE_WARNING));
	buf2 = buf + XP_STRLEN(buf);
	warning = buf2;
	size = buf + sizeof (buf) - warning;

	XP_ASSERT(m_prefsDataMserverMore);
	PrefsDataMserverMore  *fep = m_prefsDataMserverMore;

	PREFS_CHECK_DIR (fep->mail_dir_text,
					 XP_GetString(XFE_MAIL_DIR),
					 warning, size);

	if (*buf2) {
		FE_Alert(getContext(), fe_StringTrim (buf));
		return FALSE;
	}
	else {
		return TRUE;
	}
}

// Member:       installChanges
// Description:  install changes for MailNewsMserverMore
// Inputs:
// Side effects: 

void XFE_PrefsMserverMoreDialog::installChanges()
{
	//fe_installMserverMore();
}

// Member:       getContext
// Description:  returns context
// Inputs:
// Side effects: 

MWContext *XFE_PrefsMserverMoreDialog::getContext()
{
	return (m_prefsDialog->getContext());
}

// Member:       prefsMserverMoreCb_ok
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsMserverMoreDialog::cb_ok(Widget    w,
									   XtPointer closure,
									   XtPointer callData)
{
	XFE_PrefsMserverMoreDialog *theDialog = (XFE_PrefsMserverMoreDialog *)closure;
	PrefsDataMserverMore       *fep = theDialog->m_prefsDataMserverMore;
	Boolean                     b;
    char                        c;
	char                       *s = 0;
    int                         n;

	XP_ASSERT(fep);
	if (! theDialog->verifyPage()) return;

	// Mail directory

	// Backend doesn't store mail_directory. So it doesn't know if this
	// really changed or not when we install the prefs. We have to do this
	// explicitly here.

	XP_FREEIF(fe_globalPrefs.mail_directory);
	s = fe_GetTextField(fep->mail_dir_text);
    fe_globalPrefs.mail_directory = s ? s : XP_STRDUP("");

	XP_FREEIF(fe_globalPrefs.imap_mail_directory);
	s = fe_GetTextField(fep->imap_mail_dir_text);
    fe_globalPrefs.imap_mail_directory = s ? s : XP_STRDUP("");

	XP_FREEIF(fe_globalPrefs.imap_mail_local_directory);
	s = fe_GetTextField(fep->imap_mail_local_dir_text);
    fe_globalPrefs.imap_mail_local_directory = s ? s : XP_STRDUP("");

	// Check for mail

	XtVaGetValues(fep->check_for_mail_toggle, XmNset, &b, 0);
	fe_globalPrefs.auto_check_mail = b;
	if (b) {
		XtVaGetValues (fep->check_mail_interval_text, XmNvalue, &s, 0);
		fe_globalPrefs.biff_interval = 0;
		if (1 == sscanf (s, " %d %c", &n, &c))
			fe_globalPrefs.biff_interval = n;
		if (s) XtFree(s);
	}

    XtVaGetValues(fep->remember_email_passwd_toggle, XmNset, &b, 0);
	fe_globalPrefs.rememberPswd = b;

#if 0
    XtVaGetValues(fep->encrypted_passwd_toggle, XmNset, &b, 0);
	fe_globalPrefs.support_skey = b;
#endif

	//    XtVaGetValues(fep->always_toggle, XmNset, &b, 0);
	//	fe_globalPrefs.use_ns_mapi_server = b;

	// Simulate a cancel

	theDialog->cb_cancel(w, closure, callData);

	// Install preferences

	theDialog->installChanges();

	// Save the preferences at the end, so that if we've found some setting that
	// crashes, it won't get saved...

	if (!fe_CheckVersionAndSavePrefs((char *) fe_globalData.user_prefs_file, &fe_globalPrefs))
		fe_perror (theDialog->getContext(), XP_GetString( XFE_ERROR_SAVING_OPTIONS));
}

// Member:       prefsMserverMoreCb_cancel
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsMserverMoreDialog::cb_cancel(Widget    /* w */,
										   XtPointer closure,
										   XtPointer /* callData */)
{
	XFE_PrefsMserverMoreDialog *theDialog = (XFE_PrefsMserverMoreDialog *)closure;

	// Delete the dialog

	delete theDialog;
}

// Member:       cb_browseMailDir
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsMserverMoreDialog::cb_browseMailDir(Widget    w,
												  XtPointer closure,
												  XtPointer /* callData */)
{
	XFE_PrefsMserverMoreDialog   *theDialog = (XFE_PrefsMserverMoreDialog *)closure;
	PrefsDataMserverMore         *fep = theDialog->m_prefsDataMserverMore;
	XFE_PrefsDialog              *prefsDialog = theDialog->m_prefsDialog;
	Widget                        text_widget;
	
	XtVaGetValues(w, XmNuserData, &text_widget, 0);
	fe_browse_file_of_text(prefsDialog->getContext(), text_widget, True);
}

#if 0
// Member:       cb_toggleMapiServer
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsMserverMoreDialog::cb_toggleMapiServer(Widget    w,
													 XtPointer closure,
													 XtPointer callData)
{
	XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)callData;
	PrefsDataMserverMore         *fep = (PrefsDataMserverMore *)closure;

	if (! cb->set) {
		XtVaSetValues(w, XmNset, True, 0);
	}
	else if (w == fep->always_toggle) {
		XtVaSetValues(fep->never_toggle, XmNset, False, 0);
	}
	else if (w == fep->never_toggle) {
		XtVaSetValues(fep->always_toggle, XmNset, False, 0);
	}
	else
		abort();	
}

#endif
