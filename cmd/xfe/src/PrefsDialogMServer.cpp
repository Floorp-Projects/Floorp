/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/* -*- Mode: C++; tab-width: 4 -*- */
/*
   PrefsDialogMServer.cpp -- Multiple mail server preferences dialog
   Created: Arun Sharma <asharma@netscape.com>, Thu Mar 19 14:37:46 PST 1998
 */


#include "rosetta.h"
#include "MozillaApp.h"
#include "prefapi.h"
#include "felocale.h"
#include "xpgetstr.h"
#include "PrefsDialogMServer.h"

#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/ToggleB.h>
#include <Xm/ArrowBG.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/LabelG.h> 
#include <Xm/TextF.h> 
#include <Xm/ToggleBG.h> 
#include <Xfe/Xfe.h>

// IMAP pref stuff taken from WINFE. This code is XP really.
#define USERNAME ".userName"
#define REMEMBER_PASSWORD ".remember_password"
#define CHECK_NEW_MAIL ".check_new_mail"
#define CHECK_TIME ".check_time"
#define OVERRIDE_NAMESPACES ".override_namespaces"
#define DELETE_MODEL ".delete_model"
#define IS_SECURE ".isSecure"
#define NS_OTHER ".namespace.other_users"
#define NS_PERSONAL ".namespace.personal"
#define NS_PUBLIC ".namespace.public"
#define USE_SUB ".using_subscription"
#define EXPUNGE_QUIT ".cleanup_inbox_on_exit"
#define TRASH_QUIT ".empty_trash_on_exit"
#define IMAP_DOWNLOAD "mail.imap.new_mail_get_headers"

#define POP_SERVER "network.hosts.pop_server"
#define POP_NAME "mail.pop_name"
#define POP_REMEMBER_PASSWORD "mail.remember_password"
#define POP_CHECK_NEW_MAIL "mail.check_new_mail"
#define POP_CHECK_TIME "mail.check_time"
#define POP_LEAVE_ON_SERVER "mail.leave_on_server"
#define POP_DOWNLOAD "mail.pop3_gets_new_mail"

#define MAIL_SERVER_TYPE "mail.server_type"

extern int XFE_GENERAL_TAB;
extern int XFE_ADVANCED_TAB;
extern int XFE_IMAP_TAB;
extern int XFE_POP_TAB;
extern int XFE_MOVEMAIL_TAB;

static const char PrefTemplate[] = "mail.imap.server.";

// Make sure XP_FREE() from the caller function;
static char* 
IMAP_GetPrefName(const char *host_name, const char *pref)
{
    XP_ASSERT(host_name);
    if (!host_name) return NULL;
	int		pref_size=sizeof(PrefTemplate) +
        XP_STRLEN(host_name) +
        XP_STRLEN(pref) + 1;
	char	*pref_name= (char *) XP_CALLOC(pref_size, sizeof(char));

    XP_STRCPY(pref_name, PrefTemplate);
    XP_STRCAT(pref_name, host_name);
    XP_STRCAT(pref_name, pref);

	return pref_name;
}

static XP_Bool
IMAP_PrefIsLocked(const char *host_name, const char* pref)
{
	XP_Bool	result;
	char* pref_name = IMAP_GetPrefName(host_name, pref);
    
	if (!pref_name) return FALSE;

    result = PREF_PrefIsLocked(pref_name);
    XP_FREE(pref_name);

	return result;
}

static void 
IMAP_SetCharPref(const char *host_name, const char* pref, const char* value)
{
	char* pref_name = IMAP_GetPrefName(host_name, pref);
    
	if (!pref_name) return;
    
    PREF_SetCharPref(pref_name, value);
    XP_FREE(pref_name);

}

static void 
IMAP_SetIntPref(const char *host_name, const char* pref, int32 value)
{
	char* pref_name = IMAP_GetPrefName(host_name, pref);

	if (!pref_name) return;
    
    PREF_SetIntPref(pref_name, value);
    XP_FREE(pref_name);
}

static void 
IMAP_SetBoolPref(const char *host_name, const char *pref, XP_Bool value) 
{
	char*	pref_name = IMAP_GetPrefName(host_name, pref);

	if (!pref_name) return;

    PREF_SetBoolPref(pref_name, value);
    XP_FREE(pref_name);
}

static int
IMAP_CopyCharPref(const char *host_name, const char *pref, char **buf) 
{
    char*	pref_name = IMAP_GetPrefName(host_name, pref);
    if (!pref_name) return PREF_ERROR;
	if (pref_name)
	{
        int retval = PREF_CopyCharPref(pref_name, buf);
		XP_FREE(pref_name);
        return retval;
	}
    return PREF_ERROR;
}

static int
IMAP_GetIntPref(const char *host_name, const char* pref, int32 *intval)
{
	char*	pref_name = IMAP_GetPrefName(host_name,pref);
	if (!pref_name) return PREF_ERROR;

    int retval = PREF_GetIntPref(pref_name, intval);
    XP_FREE(pref_name);

    return retval;
}

static int
IMAP_GetBoolPref(const char *host_name, const char* pref, XP_Bool *boolval)
{
	char*	pref_name = IMAP_GetPrefName(host_name, pref);
	if (!pref_name) return PREF_ERROR;
    
    int retval = PREF_GetBoolPref(pref_name, boolval);
    XP_FREE(pref_name);
    
    return retval;
}


XFE_PrefsMServerDialog::XFE_PrefsMServerDialog(Widget parent, 
                                               char *server_name,
                                               XP_Bool allow_multiple,
                                               MWContext *context)
	: XFE_XmLFolderDialog((XFE_View *) NULL,
						  parent, 
						  "MailServerInfo",
						  context,
						  TRUE, // ok
						  TRUE, // cancel
						  FALSE, // help
						  FALSE, // apply
						  TRUE, // separator
						  TRUE // modal
				 ), 
      m_allow_multiple(allow_multiple),
      m_server_name(server_name)
{
    int32 server_type;
    PREF_GetIntPref(MAIL_SERVER_TYPE,&server_type);
    m_server_type = (MSG_SERVER_TYPE)server_type;

    create();
    init();

}

void XFE_PrefsMServerDialog::create() {
	XFE_XmLFolderView* folder_view = (XFE_XmLFolderView *) m_view;


    // create tabs
    m_general_tab =
        new XFE_PrefsMServerGeneralTab(this,folder_view,m_wParent,
                                       m_allow_multiple);
    m_imap_tab = new XFE_PrefsMServerIMAPTab(this, folder_view);
    m_imap_advanced_tab = new XFE_PrefsMServerAdvancedTab(this, folder_view);
    m_pop_tab = new XFE_PrefsMServerPOPTab(this, folder_view);
    m_movemail_tab = new XFE_PrefsMServerMovemailTab(this, folder_view);

    // add tabs
    folder_view->addTab(m_general_tab);

    // add these tabs but don't show them
    folder_view->addTab(m_imap_tab, FALSE);
    folder_view->addTab(m_imap_advanced_tab, FALSE);
    folder_view->addTab(m_pop_tab, FALSE);
    folder_view->addTab(m_movemail_tab, FALSE);
    
    // start with general tab
    m_general_tab->show();

    m_general_tab->setChangedCallback(changedType, (void *)this);
}

void XFE_PrefsMServerDialog::init() {

    m_general_tab->init(m_server_name, m_server_type);
    ChangedType(m_server_type);

    m_imap_advanced_tab->init(m_server_name);
    m_imap_tab->init(m_server_name);
    m_pop_tab->init();
    m_movemail_tab->init();

}

void XFE_PrefsMServerDialog::apply() {

    
    // we need to tell the other panes what server they should use
    // and only apply the relevant panes
    char *server_name=m_general_tab->getServerName();
    m_general_tab->apply();
    switch(m_server_type) {
    case MSG_Imap4:
        m_imap_tab->apply(server_name);
        m_imap_advanced_tab->apply(server_name);
        break;
    case MSG_Pop3:
        m_pop_tab->apply();
        break;
    case MSG_MoveMail:
        m_movemail_tab->apply();
        break;
    case MSG_Inbox:
        XP_ASSERT(0);
        break;
    }

}

void
XFE_PrefsMServerDialog::changedType(void *closure,MSG_SERVER_TYPE server_type)
{
    ((XFE_PrefsMServerDialog *)closure)->ChangedType(server_type);
}

void
XFE_PrefsMServerDialog::ChangedType(MSG_SERVER_TYPE server_type)
{
    m_server_type=server_type;
    
    // show/hide appropriate tabs
    if (server_type==MSG_Imap4) {
        m_imap_tab->show();
        m_imap_advanced_tab->show();
    } else {
        m_imap_tab->hide();
        m_imap_advanced_tab->hide();
    }

    if (server_type==MSG_Pop3) {
        m_pop_tab->show();
    } else {
        m_pop_tab->hide();
    }

    if (server_type==MSG_MoveMail) {
        m_movemail_tab->show();
    } else {
        m_movemail_tab->hide();
    }
    
}


// Create the body of the IMAP General Tab
XFE_PrefsMServerGeneralTab::XFE_PrefsMServerGeneralTab(
	XFE_Component *top,
	XFE_View *view,
    Widget parent,
    XP_Bool allow_multiple)
	: XFE_XmLTabView(top, view, XFE_GENERAL_TAB),
      m_server_label(0),
      m_server_text(0),
      m_server_type_menu(0),
      m_server_type_label(0),
      m_server_type_option(0),
      m_server_user_label(0),
      m_server_user_name(0),
      m_remember_password(0),
      m_check_mail(0),
      m_check_time(0),
      m_minute_label(0),
      m_imap_button(0),
      m_pop_button(0),
      m_movemail_button(0),
      m_changed_callback(0),
      m_changed_closure(0),
      m_server_name(0),
      m_parent(parent)
{
    create(allow_multiple);
}

void XFE_PrefsMServerGeneralTab::create(XP_Bool allow_multiple) {
    Widget			kids[15];
	Arg				av[10];
	int				ac = 0, i  = 0;
	Widget			form;

	form = getBaseWidget();

	kids[i++] = m_server_label = 
		XmCreateLabelGadget(form, "ServerName", av, ac);

	kids[i++] = m_server_text =
		fe_CreateTextField(form, "ServerNameText", av, ac);


	Visual    *v = 0;
	Colormap   cmap = 0;
	Cardinal   depth = 0;

	ac = 0;

	XtVaGetValues (m_parent,
				   XtNvisual, &v,
				   XtNcolormap, &cmap,
				   XtNdepth, &depth, 
				   0);

	ac = 0;
	kids[i++] = m_server_type_label =
		XmCreateLabelGadget(form, "ServerType", av, ac);

	ac = 0;
	XtSetArg(av[ac], XmNvisual, v); ac++;
	XtSetArg(av[ac], XmNdepth, depth); ac++;
	XtSetArg(av[ac], XmNcolormap, cmap); ac++;
	m_server_type_menu = XmCreatePulldownMenu (form, "serverTypeMenu", av, ac);

	ac = 0;
	XtSetArg (av [ac], XmNsubMenuId, m_server_type_menu); ac++;
	kids[i++] = m_server_type_option = 
		fe_CreateOptionMenu (form, "serverTypeOption", av, ac);
	fe_UnmanageChild_safe(XmOptionLabelGadget(m_server_type_option));

	// Now add the entries

    // POP
    if (allow_multiple) {
        m_pop_button = XmCreatePushButtonGadget(m_server_type_menu,
                                                "popOption", av, ac);
        XtAddCallback(m_pop_button, XmNactivateCallback,
                      optionPop_cb,(XtPointer)this);
        XtManageChild(m_pop_button);

        m_movemail_button = XmCreatePushButtonGadget(m_server_type_menu,
                                                     "movemailOption", av, ac);
        XtAddCallback(m_movemail_button, XmNactivateCallback,
                      optionMovemail_cb, (XtPointer)this);
        XtManageChild(m_movemail_button);
    }
    
    // IMAP
	m_imap_button = XmCreatePushButtonGadget(m_server_type_menu,
                                             "imapOption", av, ac);
	XtAddCallback(m_imap_button, XmNactivateCallback,
				  optionImap_cb,(XtPointer)this);

	XtManageChild(m_imap_button);


    // IMAP is selected until init()
    XtVaSetValues (m_server_type_option,
                   XmNmenuHistory, m_imap_button,
                   NULL);

	ac = 0;
	kids[i++] = m_server_user_label = 
		XmCreateLabelGadget(form, "ServerUser", av, ac);

	kids[i++] = m_server_user_name = 
		fe_CreateTextField(form, "Username", av, ac);

	kids[i++] = m_remember_password = 
		XmCreateToggleButtonGadget(form, "RememberPass", av, ac);

	kids[i++] = m_check_mail = 
		XmCreateToggleButtonGadget(form, "CheckMail", av, ac);
	
	kids[i++] = m_check_time =
		fe_CreateTextField(form, "WaitTime", av, ac);

	kids[i++] = m_minute_label = 
		XmCreateLabelGadget(form, "MinuteLabel", av, ac);
    
    kids[i++] = m_download_toggle =
        XmCreateToggleButtonGadget(form, "downloadToggle", av, ac);
    
    int max_height1 = XfeVaGetTallestWidget(m_server_label,
                                            m_server_text,
                                            NULL);
    int max_height2 = XfeVaGetTallestWidget(m_server_type_label,
                                            m_server_type_option,
                                            NULL) + 15;
    int max_height3 = XfeVaGetTallestWidget(m_server_user_label,
                                            m_server_user_name,
                                            NULL);
    int max_height5 = XfeVaGetTallestWidget(m_check_mail,
                                            m_check_time,
                                            m_minute_label,
                                            NULL);
	// Specify the geometry constraints
	XtVaSetValues(m_server_label,
                  XmNheight, max_height1,
				  XmNalignment, XmALIGNMENT_END,
				  XmNleftAttachment, XmATTACH_FORM,
                  XmNtopAttachment, XmATTACH_FORM,
				  NULL);

	XtVaSetValues(m_server_text,
                  XmNheight, max_height1,
                  XmNleftAttachment, XmATTACH_WIDGET,
                  XmNleftWidget, m_server_label,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNtopWidget, m_server_label,
				  NULL);

	XtVaSetValues(m_server_type_label,
                  XmNheight, max_height2,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, m_server_label,
				  NULL);

	XtVaSetValues(m_server_type_option,
                  XmNheight, max_height2,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, m_server_type_label,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, m_server_type_label,
				  NULL);

	XtVaSetValues(m_server_user_label,
                  XmNheight, max_height3,
				  XmNalignment, XmALIGNMENT_END,
				  XmNleftAttachment, XmATTACH_FORM,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, m_server_type_label,
				  NULL);

	XtVaSetValues(m_server_user_name,
                  XmNheight, max_height3,
				  XmNrightAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_WIDGET,
                  XmNleftWidget, m_server_user_label,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, m_server_user_label,
				  NULL);

	XtVaSetValues(m_remember_password,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, m_server_user_label,
				  NULL);

	XtVaSetValues(m_check_mail,
                  XmNheight, max_height5,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, m_remember_password,
				  NULL);
											   
	XtVaSetValues(m_check_time,
                  XmNheight, max_height5,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, m_check_mail,
				  XmNtopAttachment,  XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, m_check_mail,
				  NULL);
				   
	XtVaSetValues(m_minute_label,
                  XmNheight, max_height5,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, m_check_time,
                  XmNrightWidget, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, m_check_time,
				  NULL);

    XtVaSetValues(m_download_toggle,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, m_check_mail,
                  XmNleftAttachment, XmATTACH_FORM,
                  NULL);
    
	// Manage the kids
	XtManageChildren(kids, i);
}

void
XFE_PrefsMServerGeneralTab::init(char *server_name,
                                 MSG_SERVER_TYPE server_type)
{

    m_is_new=FALSE;
    m_server_name=server_name;
    m_original_type=server_type;

    setServerType(server_type);

    // this is ugly - m_is_new has to be set correctly
    // in order for lockWidgets() to behave correctly
    if (!server_name) {
        m_is_new=TRUE;
        lockWidgets();
    }
    
    char *user_name=NULL;
    XP_Bool remember_password=FALSE;
    XP_Bool check_mail=TRUE;
    XP_Bool download=TRUE;
    int32 check_time=10;

    if (server_name) fe_SetTextField(m_server_text, server_name);
    // map Prefs->widget values
    switch(m_server_type) {
    case MSG_Imap4:
        if (server_name) {
            IMAP_CopyCharPref(server_name, USERNAME,&user_name);
            IMAP_GetBoolPref(server_name, REMEMBER_PASSWORD,&remember_password);
            IMAP_GetBoolPref(server_name, CHECK_NEW_MAIL,&check_mail);
            IMAP_GetIntPref(server_name, CHECK_TIME, &check_time);
        }
        PREF_GetBoolPref(IMAP_DOWNLOAD, &download);
        break;
        
    case MSG_Pop3:
        PREF_CopyCharPref(POP_NAME,&user_name);
        PREF_GetBoolPref(POP_REMEMBER_PASSWORD,&remember_password);
        PREF_GetBoolPref(POP_CHECK_NEW_MAIL,&check_mail);
        PREF_GetIntPref(POP_CHECK_TIME, &check_time);
        PREF_GetBoolPref(POP_DOWNLOAD, &download);
        XP_ASSERT(m_pop_button);
        break;
        
    case MSG_MoveMail:
        user_name=XP_STRDUP("");
        PREF_GetBoolPref(POP_CHECK_NEW_MAIL,&check_mail);
        PREF_GetIntPref(POP_CHECK_TIME, &check_time);
        PREF_GetBoolPref(POP_DOWNLOAD, &download);
        break;

    case MSG_Inbox:
        XP_ASSERT(0);
        break;
    }        


    if (user_name) fe_SetTextField(m_server_user_name,user_name);
    XmToggleButtonGadgetSetState(m_remember_password, remember_password,TRUE);
    XmToggleButtonGadgetSetState(m_check_mail, check_mail,TRUE);
    XmToggleButtonGadgetSetState(m_download_toggle, download, TRUE);

    
    char *check_time_buf=PR_smprintf("%d",check_time);
    fe_SetTextField(m_check_time,check_time_buf);
    XP_FREE(check_time_buf);
    XP_FREE(user_name);
        
}

void
XFE_PrefsMServerGeneralTab::setServerType(MSG_SERVER_TYPE type)
{
    m_server_type=type;
    switch(m_server_type) {
    case MSG_Imap4:
        XtVaSetValues (m_server_type_option,
                       XmNmenuHistory, m_imap_button,
                       NULL);
        break;
        
    case MSG_Pop3:
        XtVaSetValues (m_server_type_option,
                       XmNmenuHistory, m_pop_button,
                       NULL);
        break;
        
    case MSG_MoveMail:
        XtVaSetValues (m_server_type_option,
                       XmNmenuHistory, m_movemail_button,
                       NULL);
        break;
        
    case MSG_Inbox:
        XP_ASSERT(0);
        break;
    }
}

void
XFE_PrefsMServerGeneralTab::lockWidgets()
{
    XP_Bool lock_server=FALSE,
        lock_user=FALSE,
        lock_password=FALSE,
        lock_check_mail=FALSE,
        lock_download=FALSE,
        lock_check_time=FALSE;
    
    switch (m_server_type) {
    case MSG_Imap4:
        if (m_is_new || (m_original_type!=MSG_Imap4))
            lock_server = FALSE;
        else
            lock_server = TRUE;

        lock_download=PREF_PrefIsLocked(IMAP_DOWNLOAD);
        if (m_server_name) {
            lock_user = IMAP_PrefIsLocked(m_server_name, USERNAME);
            lock_password = IMAP_PrefIsLocked(m_server_name, REMEMBER_PASSWORD);
            lock_check_mail = IMAP_PrefIsLocked(m_server_name, CHECK_NEW_MAIL);
            lock_check_time = IMAP_PrefIsLocked(m_server_name, CHECK_TIME);
        }
        break;

    case MSG_Pop3:
        lock_server = PREF_PrefIsLocked(POP_SERVER);
        lock_user = PREF_PrefIsLocked(POP_NAME);
        lock_password = PREF_PrefIsLocked(POP_REMEMBER_PASSWORD);
        lock_check_mail = PREF_PrefIsLocked(POP_CHECK_NEW_MAIL);
        lock_check_time = PREF_PrefIsLocked(POP_CHECK_TIME);
        lock_download = PREF_PrefIsLocked(POP_DOWNLOAD);
        break;

    case MSG_MoveMail:
        lock_server=TRUE;
        lock_user=TRUE;
        lock_password=TRUE;
        lock_check_mail = PREF_PrefIsLocked(POP_CHECK_NEW_MAIL);
        lock_check_time = PREF_PrefIsLocked(POP_CHECK_TIME);
        lock_download = PREF_PrefIsLocked(POP_DOWNLOAD);
        break;
        
    case MSG_Inbox:
        XP_ASSERT(0);
        break;
    }        
        
    XtSetSensitive(m_server_text, !lock_server);
    XtSetSensitive(m_server_user_name, !lock_user);
    XtSetSensitive(m_remember_password, !lock_password);
    XtSetSensitive(m_check_mail, !lock_check_mail);
    XtSetSensitive(m_check_time, !lock_check_time);
    XtSetSensitive(m_download_toggle, !lock_download);

}


void 
XFE_PrefsMServerGeneralTab::optionImap_cb(Widget    /* w */,
										 XtPointer clientData,
                                          XtPointer /* callData */)
{
    XFE_PrefsMServerGeneralTab *tab=(XFE_PrefsMServerGeneralTab*)clientData;
    if (!tab) return;
    tab->ChangedType(MSG_Imap4);
}

void 
XFE_PrefsMServerGeneralTab::optionPop_cb(Widget    /* w */,
										 XtPointer clientData,
										 XtPointer /* callData */)
{
    XFE_PrefsMServerGeneralTab *tab=(XFE_PrefsMServerGeneralTab*)clientData;
    if (!tab) return;
    tab->ChangedType(MSG_Pop3);
}

void
XFE_PrefsMServerGeneralTab::optionMovemail_cb(Widget /* w */,
                                              XtPointer clientData,
                                              XtPointer /* callData*/)
{
    XFE_PrefsMServerGeneralTab *tab=(XFE_PrefsMServerGeneralTab*)clientData;
    if (!tab) return;
    tab->ChangedType(MSG_MoveMail);
}

void
XFE_PrefsMServerGeneralTab::ChangedType(MSG_SERVER_TYPE type)
{
    m_server_type=type;
    if (m_changed_callback) m_changed_callback(m_changed_closure, type);
    lockWidgets();
}

void
XFE_PrefsMServerGeneralTab::setChangedCallback(serverTypeCallback cb,
                                               void* closure)
{
    m_changed_callback=cb;
    m_changed_closure=closure;
}

void
XFE_PrefsMServerGeneralTab::apply()
{

    // pull actual values out of the widgets
	char *server_name         = getServerName();	
	char *user_name           = getUserName();
    XP_Bool remember_password = getRememberPassword();
    XP_Bool check_mail        = getCheckMail();
    XP_Bool download          = getDownload();
    int32 check_time           = getWaitTime();
    MSG_Master *master        = fe_getMNMaster();
    switch (m_server_type) {
    case MSG_Imap4:

        if (m_original_type==MSG_Imap4) {
            // create if necessary
            // use dumb defaults & let pref panes handle it correctly
            if (m_is_new)
                MSG_CreateIMAPHost(master, server_name, FALSE, user_name,
                                   check_mail, check_time, remember_password,
                                   TRUE, TRUE, "", "","");
        } else {                // originally POP or movemail
            // implicitly create by setting POP server and switching type
            PREF_SetCharPref(POP_SERVER, server_name);
            PREF_SetIntPref(MAIL_SERVER_TYPE, (int32)MSG_Imap4);
        }

        IMAP_SetCharPref(server_name, USERNAME, user_name);
        IMAP_SetBoolPref(server_name, REMEMBER_PASSWORD, remember_password);
        IMAP_SetBoolPref(server_name, CHECK_NEW_MAIL, check_mail);
        IMAP_SetIntPref(server_name, CHECK_TIME, check_time);
        PREF_SetBoolPref(IMAP_DOWNLOAD, download);
        break;
        
    case MSG_Pop3:

        PREF_SetIntPref(MAIL_SERVER_TYPE, (int32)MSG_Pop3);
        PREF_SetCharPref(POP_SERVER, server_name);
        PREF_SetCharPref(POP_NAME, user_name);
        PREF_SetBoolPref(POP_REMEMBER_PASSWORD, remember_password);
        PREF_SetBoolPref(POP_CHECK_NEW_MAIL, check_mail);
        PREF_SetIntPref(POP_CHECK_TIME, check_time);
        PREF_SetBoolPref(POP_DOWNLOAD, download);
        break;
        
    case MSG_MoveMail:
        PREF_SetIntPref(MAIL_SERVER_TYPE, (int32)MSG_MoveMail);
        PREF_SetBoolPref(POP_CHECK_NEW_MAIL, check_mail);
        PREF_SetIntPref(POP_CHECK_TIME, check_time);
        PREF_SetBoolPref(POP_DOWNLOAD, download);
        break;

    case MSG_Inbox:
        XP_ASSERT(0);
    }
        
	// Free all the strings
	XtFree(server_name);
	XtFree(user_name); 

}

// Create the body of the Imap Tab
XFE_PrefsMServerIMAPTab::XFE_PrefsMServerIMAPTab(
	XFE_Component *top,
	XFE_View *view)
	: XFE_XmLTabView(top, view, XFE_IMAP_TAB),
      m_delete_trash_toggle(0),
      m_delete_mark_toggle(0),
      m_delete_remove_toggle(0),
      m_use_ssl(0)
{
#if 0
    m_use_sub=0;
#endif
    create();
}

void
XFE_PrefsMServerIMAPTab::create() {
	Widget			kids[10];
	int				i  = 0;
	Widget			form;
    Widget          delete_radiobox;
    Widget          delete_label;
    Widget          sep1;
    
	form = getBaseWidget();

	HG19271
    
    kids[i++] = delete_label =
        XmCreateLabelGadget(form, "deleteLabel", NULL, 0);
    kids[i++] = delete_radiobox =
        XmCreateRadioBox(form, "deleteRadioBox", NULL, 0);
    
	m_delete_trash_toggle =
		XmCreateToggleButtonGadget(delete_radiobox, "trashToggle", NULL, 0);
    m_delete_mark_toggle =
		XmCreateToggleButtonGadget(delete_radiobox, "markToggle", NULL, 0);
    m_delete_remove_toggle =
		XmCreateToggleButtonGadget(delete_radiobox, "removeToggle", NULL, 0);


    kids[i++] = m_expunge_toggle =
        XmCreateToggleButtonGadget(form, "expungeExitToggle", NULL, 0);
    kids[i++] = m_trash_toggle =
        XmCreateToggleButtonGadget(form, "trashExitToggle", NULL, 0);

#if 0
    kids[i++] = m_use_sub =
        XmCreateToggleButtonGadget(form, "UseSub", NULL, 0);
#endif    

    
	HG10977

    XtVaSetValues(delete_label,
				  XmNtopAttachment, XmATTACH_WIDGET,
                  HG18162
                  XmNleftAttachment, XmATTACH_FORM,
                  NULL);
                  
	XtVaSetValues(delete_radiobox,
				  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, delete_label,
				  XmNleftAttachment, XmATTACH_FORM,
				  NULL);

    XtVaSetValues(m_expunge_toggle,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, delete_radiobox,
                  XmNleftWidget, XmATTACH_FORM,
                  NULL);
    
    XtVaSetValues(m_trash_toggle,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, m_expunge_toggle,
                  XmNleftWidget, XmATTACH_FORM,
                  NULL);
                  
#if 0
    XtVaSetValues(m_use_sub,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, m_trash_toggle,
                  XmNleftAttachment, XmATTACH_FORM,
                  NULL);
#endif

	// Manage the kids
    XtManageChild(m_delete_trash_toggle);
    XtManageChild(m_delete_mark_toggle);
    XtManageChild(m_delete_remove_toggle);
	XtManageChildren(kids, i);
    
    // set an initial default (this should be done in init(),
    // but I'm not sure how)
    XmToggleButtonGadgetSetState(m_delete_trash_toggle, True, True);
}

void
XFE_PrefsMServerIMAPTab::init(char *server_name) {
    
    if (!server_name) return;

    XP_Bool HG19711 use_sub;
    XP_Bool expunge_quit, trash_quit;
    int32 intval;

    IMAP_GetIntPref(server_name, DELETE_MODEL, &intval);
    HG19189
    IMAP_GetBoolPref(server_name, USE_SUB, &use_sub);
    IMAP_GetBoolPref(server_name, EXPUNGE_QUIT, &expunge_quit);
    IMAP_GetBoolPref(server_name, TRASH_QUIT, &trash_quit);

    MSG_IMAPDeleteModel delete_model=(MSG_IMAPDeleteModel)intval;
    
    switch (delete_model) {
    case MSG_IMAPDeleteIsIMAPDelete:
        XmToggleButtonGadgetSetState(m_delete_mark_toggle, True, True);
        break;
    case MSG_IMAPDeleteIsMoveToTrash:
        XmToggleButtonGadgetSetState(m_delete_trash_toggle, True, True);
        break;
    case MSG_IMAPDeleteIsDeleteNoTrash:
        XmToggleButtonGadgetSetState(m_delete_remove_toggle, True, True);
        break;
    }

    HG71676
    XmToggleButtonGadgetSetState(m_expunge_toggle, expunge_quit, TRUE);
    XmToggleButtonGadgetSetState(m_trash_toggle, trash_quit, TRUE);
#if 0
    XmToggleButtonGadgetSetState(m_use_sub, use_sub,TRUE);
#endif

}

void
XFE_PrefsMServerIMAPTab::apply(char *server_name)
{
    XP_ASSERT(server_name);
    if (!server_name) return;

    int32 intval=(int32)MSG_IMAPDeleteIsMoveToTrash;
    
    if (XmToggleButtonGadgetGetState(m_delete_mark_toggle))
        intval=(int32)MSG_IMAPDeleteIsIMAPDelete;
    else if (XmToggleButtonGadgetGetState(m_delete_trash_toggle))
        intval=(int32)MSG_IMAPDeleteIsMoveToTrash;
    else if (XmToggleButtonGadgetGetState(m_delete_remove_toggle))
        intval=(int32)MSG_IMAPDeleteIsDeleteNoTrash;

    IMAP_SetIntPref(server_name, DELETE_MODEL, intval);
	HG71851
    IMAP_SetBoolPref(server_name, EXPUNGE_QUIT, getExpungeQuit());
    IMAP_SetBoolPref(server_name, TRASH_QUIT, getTrashQuit());
#if 0
    IMAP_SetBoolPref(server_name, USE_SUB, getUseSub());
#endif

}

// Create the body of the Advanced Tab
XFE_PrefsMServerAdvancedTab::XFE_PrefsMServerAdvancedTab(
	XFE_Component *top,
	XFE_View *view)
	: XFE_XmLTabView(top, view, XFE_ADVANCED_TAB),
       m_path_prefs_label(0),
	 m_personal_dir_label(0),
	 m_personal_dir_text(0),
	 m_public_dir_label(0),
	 m_public_dir_text(0),
	 m_other_label(0),
	 m_other_text(0),
	 m_allow_server(0)
{
    create();
}

void XFE_PrefsMServerAdvancedTab::create() {
	Widget			kids[10];
	Arg				av[10];
	int				ac = 0, i  = 0;
	Widget			form;

	form = getBaseWidget();

	kids[i++] = m_path_prefs_label = 
		XmCreateLabelGadget(form, "PathPrefsLabel", av, ac);

	kids[i++] = m_personal_dir_label = 
		XmCreateLabelGadget(form, "PersonalDir", av, ac);

	kids[i++] = m_personal_dir_text =
		fe_CreateTextField(form, "PersonalDirText", av, ac);

	kids[i++] = m_public_dir_label = 
		XmCreateLabelGadget(form, "PublicDir", av, ac);

	kids[i++] = m_public_dir_text =
		fe_CreateTextField(form, "PublicDirText", av, ac);

	kids[i++] = m_other_label = 
		XmCreateLabelGadget(form, "OtherUsers", av, ac);

	kids[i++] = m_other_text =
		fe_CreateTextField(form, "OtherUsersText", av, ac);

	kids[i++] = m_allow_server = 
		XmCreateToggleButtonGadget(form, "AllowServer", av, ac);
	
	// Specify the geometry constraints

	XtVaSetValues(m_path_prefs_label,
                  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_FORM,
				  NULL);

	XtVaSetValues(m_personal_dir_label,
				  XmNalignment, XmALIGNMENT_END,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, m_personal_dir_text,
				  XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNbottomWidget, m_personal_dir_text,
				  NULL);

	XtVaSetValues(m_personal_dir_text,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, m_path_prefs_label,
				  NULL);

	XtVaSetValues(m_public_dir_label,
				  XmNalignment, XmALIGNMENT_END,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, m_public_dir_text,
				  XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNbottomWidget, m_public_dir_text,
				  NULL);

	XtVaSetValues(m_public_dir_text,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, m_personal_dir_text,
				  NULL);

	XtVaSetValues(m_other_label,
				  XmNalignment, XmALIGNMENT_END,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, m_other_text,
				  XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNbottomWidget, m_other_text,
				  NULL);

	XtVaSetValues(m_other_text,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, m_public_dir_text,
				  NULL);

	XtVaSetValues(m_allow_server,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, m_other_label,
				  NULL);

	// Manage the kids
	XtManageChildren(kids, i);
}

void
XFE_PrefsMServerAdvancedTab::init(char *server_name) {
    if (!server_name) return;
                          
    char *personal_dir=NULL,
        *public_dir=NULL,
        *other_dir=NULL;
    XP_Bool override=FALSE;

    if (IMAP_CopyCharPref(server_name, NS_PERSONAL, &personal_dir)==PREF_OK) {
        fe_SetTextField(m_personal_dir_text, personal_dir);
        XP_FREE(personal_dir);
    }
    
    if (IMAP_CopyCharPref(server_name, NS_PUBLIC, &public_dir)==PREF_OK) {
        fe_SetTextField(m_public_dir_text, public_dir);
        XP_FREE(public_dir);
    }
    if (IMAP_CopyCharPref(server_name, NS_OTHER, &other_dir)==PREF_OK) {
        fe_SetTextField(m_other_text, other_dir);
        XP_FREE(other_dir);
    }
    
    IMAP_GetBoolPref (server_name, OVERRIDE_NAMESPACES, &override);
    XmToggleButtonGadgetSetState(m_allow_server, override,TRUE);

}


void
XFE_PrefsMServerAdvancedTab::apply(char *server_name)
{

    XP_ASSERT(server_name);
    if (!server_name) return;
    
	char *personal_dir = getImapPersonalDir();
	char *public_dir = getImapPublicDir();
	char *others_dir = getImapOthersDir();
	XP_Bool override = getOverrideNamespaces();

	IMAP_SetCharPref(server_name, NS_PERSONAL, personal_dir);
	IMAP_SetCharPref(server_name, NS_PUBLIC, public_dir);
	IMAP_SetCharPref(server_name, NS_OTHER, others_dir);
	IMAP_SetBoolPref(server_name, OVERRIDE_NAMESPACES, override);

	XtFree(personal_dir);
	XtFree(public_dir); 
	XtFree(others_dir);

}


// Create the body of the POP Tab
XFE_PrefsMServerPOPTab::XFE_PrefsMServerPOPTab(XFE_Component *top,
											   XFE_View *view)
	: XFE_XmLTabView(top, view, XFE_POP_TAB),
      m_leave_messages(0)
{
    create();
}

void
XFE_PrefsMServerPOPTab::create() {

	Widget			kids[10];
	Arg				av[10];
	int				ac = 0, i  = 0;
	Widget			form;

	form = getBaseWidget();

	kids[i++] = m_leave_messages = 
		XmCreateToggleButtonGadget(form, "LeaveMessages", av, ac);

	XtVaSetValues(m_leave_messages,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_FORM,
				  NULL);

	// Manage the kids
	XtManageChildren(kids, i);
}

void
XFE_PrefsMServerPOPTab::init() {
    XP_Bool leave_messages;
    PREF_GetBoolPref(POP_LEAVE_ON_SERVER, &leave_messages);
    XmToggleButtonGadgetSetState(m_leave_messages, leave_messages,TRUE);
}

void
XFE_PrefsMServerPOPTab::apply()
{
	PREF_SetBoolPref(POP_LEAVE_ON_SERVER, getLeaveMessages());
}

XFE_PrefsMServerMovemailTab::XFE_PrefsMServerMovemailTab(XFE_Component *top,
                                                              XFE_View *view)
    : XFE_XmLTabView(top, view, XFE_MOVEMAIL_TAB)
{
    create();
}

void
XFE_PrefsMServerMovemailTab::create()
{
    Widget kids[5];
    int i;
    Widget spacer;
    Widget toggle_radiobox;
    Widget form = getBaseWidget();
    
    Dimension lmargin, spacing;

    i=0;
    kids[i++] = toggle_radiobox =
        XmCreateRadioBox(form, "movemailRadio", NULL,0);
    m_builtin_toggle =
        XmCreateToggleButtonGadget(toggle_radiobox,"builtinToggle", NULL,0);
    m_external_toggle =
        XmCreateToggleButtonGadget(toggle_radiobox,"externalToggle",NULL,0);

    // spacer so text box lines up with label in bottom radio button
    kids[i++] = spacer =
        XmCreateLabelGadget(form, "", NULL, 0);
    kids[i++] = m_program_text =
        fe_CreateTextField(form, "externalApp", NULL, 0);
    kids[i++] = m_choose_button =
        XmCreatePushButtonGadget(form, "appChoose", NULL, 0);

    XtVaGetValues(m_external_toggle,
                  XmNmarginLeft, &lmargin,
                  XmNspacing, &spacing,
                  NULL);

    XtVaSetValues(toggle_radiobox,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_FORM,
                  NULL);
    XtVaSetValues(spacer,
                  XmNwidth, lmargin+spacing,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, toggle_radiobox,
                  XmNleftAttachment, XmATTACH_FORM,
                  NULL);
    XtVaSetValues(m_program_text,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, toggle_radiobox,
                  XmNleftAttachment, XmATTACH_WIDGET,
                  XmNleftWidget, spacer,
                  NULL);
    XtVaSetValues(m_choose_button,
                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNtopWidget, m_program_text,
                  XmNleftAttachment, XmATTACH_WIDGET,
                  XmNleftWidget, m_program_text,
                  NULL);
    
    XtManageChild(m_builtin_toggle);
    XtManageChild(m_external_toggle);
    XtManageChildren(kids, i);

    XtAddCallback(m_builtin_toggle, XmNvalueChangedCallback,
                  toggleBuiltin_cb, this);
    XtAddCallback(m_choose_button, XmNactivateCallback,
                  chooseMovemail_cb, this);
}

void
XFE_PrefsMServerMovemailTab::init()
{
    XP_Bool boolval;
    XP_Bool locked;
    char *charval;

    locked=PREF_PrefIsLocked("mail.use_builtin_movemail");
    XtSetSensitive(m_builtin_toggle, !locked);
    XtSetSensitive(m_external_toggle,!locked);
    PREF_GetBoolPref("mail.use_builtin_movemail", &boolval);
    XmToggleButtonGadgetSetState(m_builtin_toggle, boolval, True);
    XmToggleButtonGadgetSetState(m_external_toggle, !boolval, True);

    locked=PREF_PrefIsLocked("mail.movemail_program");
    XtSetSensitive(m_program_text, !locked);
    
    PREF_CopyCharPref("mail.movemail_program", &charval);
    fe_SetTextField(m_program_text, charval);
    XP_FREE(charval);

}

void
XFE_PrefsMServerMovemailTab::apply()
{
    XP_Bool boolval = (XP_Bool)XmToggleButtonGetState(m_builtin_toggle);
    PREF_SetBoolPref("mail.use_builtin_movemail", boolval);

    char *str=fe_GetTextField(m_program_text);
    PREF_SetCharPref("mail.movemail_program", str);
    if (str) XtFree(str);
    
}

void
XFE_PrefsMServerMovemailTab::toggleBuiltin_cb(Widget,
                                              XtPointer closure,
                                              XtPointer callData)
{
    XFE_PrefsMServerMovemailTab *tab=(XFE_PrefsMServerMovemailTab *)closure;
    tab->toggleBuiltin((XmToggleButtonCallbackStruct *)callData);
}

void
XFE_PrefsMServerMovemailTab::chooseMovemail_cb(Widget,
                                               XtPointer closure,
                                               XtPointer)
{
    XFE_PrefsMServerMovemailTab *tab=(XFE_PrefsMServerMovemailTab *)closure;
    tab->chooseMovemail();
}



void
XFE_PrefsMServerMovemailTab::toggleBuiltin(XmToggleButtonCallbackStruct *cbs)
{
    XtSetSensitive(m_program_text, !cbs->set);
    XtSetSensitive(m_choose_button, !cbs->set);
}

void
XFE_PrefsMServerMovemailTab::chooseMovemail()
{


}
