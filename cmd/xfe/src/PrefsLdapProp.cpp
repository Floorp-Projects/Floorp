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
   PrefsLdapProp.cpp -- The LDAP server properties dialog
   Created: Linda Wei <lwei@netscape.com>, 7-Feb-97.
 */



#include "felocale.h"
#include "structs.h"
#include "fonts.h"
#include "xpassert.h"
#include "xfe.h"
#include "PrefsDialog.h"
#include "PrefsLdapProp.h"

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

extern "C"
{
	char *XP_GetString(int i);
}

// ==================== Public Member Functions ====================

// Member:       XFE_PrefsLdapPropDialog
// Description:  Constructor
// Inputs:
// Side effects: Creates the LDAP Server Properties dialog

XFE_PrefsLdapPropDialog::XFE_PrefsLdapPropDialog(XFE_PrefsDialog *prefsDialog,
												 XFE_PrefsPageMailNewsAddrBook *addrBookPage,
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
	  m_addrBookPage(addrBookPage),
	  m_prefsDataLdapProp(0)
{
	PrefsDataLdapProp *fep = NULL;

	fep = new PrefsDataLdapProp;
	memset(fep, 0, sizeof(PrefsDataLdapProp));
	m_prefsDataLdapProp = fep;

	Widget     kids[100];
	Arg        av[50];
	int        ac;
	int        i;

	Widget     form;
	form = XtVaCreateWidget("form", xmFormWidgetClass, m_chrome,
							XmNmarginWidth, 8,
							XmNtopAttachment, XmATTACH_FORM,
							XmNleftAttachment, XmATTACH_FORM,
							XmNrightAttachment, XmATTACH_FORM,
							XmNbottomAttachment, XmATTACH_FORM,
							NULL);
	XtManageChild (form);

	Widget   desc_label;
	Widget   server_label;
	Widget   root_label;
	Widget   port_number_label;
	Widget   number_of_hit_label;
	Widget   desc_text;
	Widget   server_text;
	Widget   root_text;
	Widget   port_number_text;
	Widget   number_of_hit_text;
	Widget   secure_toggle;
#if 0
	Widget   save_passwd_toggle;
#endif

	ac = 0;
	i = 0;

	kids[i++] = desc_label = 
		XmCreateLabelGadget(form, "descLabel", av, ac);

	kids[i++] = server_label = 
		XmCreateLabelGadget(form, "serverLabel", av, ac);

	kids[i++] = root_label = 
		XmCreateLabelGadget(form, "rootLabel", av, ac);

	kids[i++] = port_number_label = 
		XmCreateLabelGadget(form, "portNumberLabel", av, ac);

	kids[i++] = number_of_hit_label = 
		XmCreateLabelGadget(form, "numHitLabel", av, ac);

	kids[i++] = desc_text = 
		fe_CreateTextField(form, "descText", av, ac);

	kids[i++] = server_text = 
		fe_CreateTextField(form, "serverText", av, ac);

	kids[i++] = root_text = 
		fe_CreateTextField(form, "rootText", av, ac);

	kids[i++] = port_number_text = 
		fe_CreateTextField(form, "portNumberText", av, ac);

	kids[i++] = number_of_hit_text = 
		fe_CreateTextField(form, "numberOfHitText", av, ac);

	kids[i++] = secure_toggle =
		XmCreateToggleButtonGadget(form, "secure", av, ac);

#if 0
	kids[i++] = save_passwd_toggle =
		XmCreateToggleButtonGadget(form, "savePasswd", av, ac);
#endif

	fep->desc_text = desc_text;
	fep->server_text = server_text;
	fep->root_text = root_text;
	fep->port_number_text = port_number_text;
	fep->number_of_hit_text = number_of_hit_text;
	fep->secure_toggle = secure_toggle;
#if 0
	fep->save_passwd_toggle = save_passwd_toggle;
#endif

	int labels_width;
	labels_width = XfeVaGetWidestWidget(desc_label,
										server_label,
										root_label,
										port_number_label,
										number_of_hit_label,
										NULL);

	int labels_height;
	labels_height = XfeVaGetTallestWidget(desc_label,
										  desc_text,
										  secure_toggle,
										  NULL);

	XtVaSetValues(desc_label,
				  XmNheight, labels_height,
				  RIGHT_JUSTIFY_VA_ARGS(desc_label,labels_width),
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	XtVaSetValues(desc_text,
				  XmNcolumns, 35,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, labels_width,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	XtVaSetValues(server_label,
				  XmNheight, labels_height,
				  RIGHT_JUSTIFY_VA_ARGS(server_label,labels_width),
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, desc_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	XtVaSetValues(server_text,
				  XmNcolumns, 35,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, server_label,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, desc_text,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	XtVaSetValues(root_label,
				  XmNheight, labels_height,
				  RIGHT_JUSTIFY_VA_ARGS(root_label,labels_width),
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, server_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	XtVaSetValues(root_text,
				  XmNcolumns, 35,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, root_label,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, desc_text,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	XtVaSetValues(port_number_label,
				  XmNheight, labels_height,
				  RIGHT_JUSTIFY_VA_ARGS(port_number_label,labels_width),
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, root_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	XtVaSetValues(port_number_text,
				  XmNcolumns, 6,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, port_number_label,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, desc_text,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	XtVaSetValues(number_of_hit_label,
				  XmNheight, labels_height,
				  RIGHT_JUSTIFY_VA_ARGS(number_of_hit_label,labels_width),
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, port_number_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	XtVaSetValues(number_of_hit_text,
				  XmNcolumns, 6,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, number_of_hit_label,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, desc_text,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	XtVaSetValues(secure_toggle,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, port_number_text,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, port_number_text,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
#if 0
	XtVaSetValues(save_passwd_toggle,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, number_of_hit_text,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, secure_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
#endif
	
	// Add callbacks

	XtAddCallback(m_chrome, XmNokCallback, cb_ok, this);
	XtAddCallback(m_chrome, XmNcancelCallback, cb_cancel, this);

	XtManageChildren(kids, i);
	XtManageChild(form);
}

// Member:       ~XFE_PrefsLdapPropDialog
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsLdapPropDialog::~XFE_PrefsLdapPropDialog()
{
	delete m_prefsDataLdapProp;
}

// Member:       show
// Description:  Pop up dialog
// Inputs:
// Side effects: 

void XFE_PrefsLdapPropDialog::show()
{
	// Manage the top level

	XFE_Dialog::show();

	// Set focus to the OK button

	XmProcessTraversal(m_okButton, XmTRAVERSE_CURRENT);
}

// Member:       initPage
// Description:  Initializes page for MailNewsLang
// Inputs:
// Side effects: 

void XFE_PrefsLdapPropDialog::initPage(DIR_Server *server)
{
	XP_ASSERT(m_prefsDataLdapProp);
	PrefsDataLdapProp    *fep = m_prefsDataLdapProp;
	char                  buf[256];

	m_server = server;
	if (server) {
		fe_SetTextField(fep->desc_text, server->description);
		fe_SetTextField(fep->server_text, server->serverName);
		fe_SetTextField(fep->root_text, server->searchBase);
		PR_snprintf(buf, sizeof(buf), "%d", server->port);
		XtVaSetValues(fep->port_number_text, XmNvalue, buf, 0);
		PR_snprintf(buf, sizeof(buf), "%d", server->maxHits);
		XtVaSetValues(fep->number_of_hit_text, XmNvalue, buf, 0);
		XtVaSetValues(fep->secure_toggle, XmNset, server->isSecure, 0);
#if 0
		XtVaSetValues(fep->save_passwd_toggle, XmNset, server->savePassword, 0);
#endif
	}
}

// Member:       verifyPage
// Description:  verify page for MailNewsLang
// Inputs:
// Side effects: 

Boolean XFE_PrefsLdapPropDialog::verifyPage()
{
	return TRUE;
}

// Member:       getContext
// Description:  returns context
// Inputs:
// Side effects: 

MWContext *XFE_PrefsLdapPropDialog::getContext()
{
	return (m_prefsDialog->getContext());
}

// Member:       getData
// Description:  returns data
// Inputs:
// Side effects: 

PrefsDataLdapProp *XFE_PrefsLdapPropDialog::getData()
{
	return (m_prefsDataLdapProp);
}

// Member:       getEditDir
// Description:  returns the directory being edited
// Inputs:
// Side effects: 

DIR_Server *XFE_PrefsLdapPropDialog::getEditDir()
{
	return m_server;
}

// Member:       getAddrBookPage
// Description:  returns the Address Book prefs page
// Inputs:
// Side effects: 

XFE_PrefsPageMailNewsAddrBook *XFE_PrefsLdapPropDialog::getAddrBookPage()
{
	return m_addrBookPage;
}

// Member:       getPrefsDialog
// Description:  returns preferences dialog
// Inputs:
// Side effects: 

XFE_PrefsDialog *XFE_PrefsLdapPropDialog::getPrefsDialog()
{
	return (m_prefsDialog);
}

// Friend:       prefsLangCb_ok
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsLdapPropDialog::cb_ok(Widget    w,
									XtPointer closure,
									XtPointer callData)
{
	XFE_PrefsLdapPropDialog *theDialog = (XFE_PrefsLdapPropDialog *)closure;
	PrefsDataLdapProp       *fep = theDialog->getData();
	Bool                     create = theDialog->getEditDir() ? False : True;
	DIR_Server              *dir = 0;
	
	if (create) {
		dir = (DIR_Server *)XP_ALLOC(sizeof(DIR_Server));
		DIR_InitServer(dir);
	}
	else {
		dir = theDialog->getEditDir();
		XP_FREEIF(dir->description);
		XP_FREEIF(dir->serverName);
		XP_FREEIF(dir->searchBase);
		DIR_InitServer(dir);
	}

	char   *desc = 0;
	char   *server = 0;
	char   *root = 0;
	char   *port_num_text = 0;
	char   *num_hits_text = 0;
	char    dummy;
	int     port_num = 0;
	int     num_hits = 0;
	Boolean b;
	char    temp[1024];
	char   *ptr;

	// TODO: error checking

	desc = fe_GetTextField(fep->desc_text);
	server = fe_GetTextField(fep->server_text);
	root = fe_GetTextField(fep->root_text);
    XtVaGetValues(fep->port_number_text, XmNvalue, &port_num_text, 0);
    XtVaGetValues(fep->number_of_hit_text, XmNvalue, &num_hits_text, 0);

    if (1 != sscanf(port_num_text, " %d %c", &port_num, &dummy) ||
		port_num < 0) {
		// TODO: error
	}
	if (port_num_text) XtFree(port_num_text);

    if (1 == sscanf(num_hits_text, " %d %c", &num_hits, &dummy) &&
		num_hits < 0) {
		// TODO: error
	}
	if (num_hits_text) XtFree(num_hits_text);

	if (ptr = XP_STRCHR(server, '.')) {
		XP_STRCPY(temp, ptr+1);
		if (ptr = XP_STRCHR(temp, '.')) {
			*ptr = '\0';
		}
	}
	else {
		XP_STRCPY(temp, server);
	}

	dir->description = desc ? desc : XP_STRDUP("");
	dir->serverName =  server ? server : XP_STRDUP("");
	dir->searchBase =  root ? root : XP_STRDUP("");
	// dir->htmlGateway =  NULL; // no loner use
	dir->fileName = WH_FileName(WH_TempName(xpAddrBook, temp), xpAddrBook);
	dir->port = port_num;
	dir->maxHits = num_hits;
	XtVaGetValues(fep->secure_toggle, XmNset, &b, 0);
	dir->isSecure = b;
#if 0
	XtVaGetValues(fep->save_passwd_toggle, XmNset, &b, 0);
	dir->savePassword = b;
#endif
	dir->dirType = LDAPDirectory; 

	// Insert into list if this is create

	if (create) {
		XFE_PrefsPageMailNewsAddrBook *dir_page = theDialog->getAddrBookPage();
		dir_page->insertDir(dir);
	}

	// Simulate a cancel

	theDialog->cb_cancel(w, closure, callData);
}

// Member:       prefsLangCb_cancel
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsLdapPropDialog::cb_cancel(Widget    /* w */,
										XtPointer closure,
										XtPointer /* callData */)
{
	XFE_PrefsLdapPropDialog *theDialog = (XFE_PrefsLdapPropDialog *)closure;

	XtRemoveCallback(theDialog->m_chrome, XmNokCallback, cb_ok, theDialog);
	XtRemoveCallback(theDialog->m_chrome, XmNcancelCallback, cb_cancel, theDialog);

	// Delete the dialog

	delete theDialog;
}

