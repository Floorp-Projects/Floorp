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
   PrefsDialogMServer.h -- Headers for multiple mail server preferences dialog
   Created: Arun Sharma <asharma@netscape.com>, Thu Mar 19 14:37:46 PST 1998
 */

#ifndef _xfe_prefsdialog_mserver_h
#define _xfe_prefsdialog_mserver_h

#include "rosetta.h"
#include "XmLFolderDialog.h"
#include "XmLFolderView.h"
#include "TabView.h"
#include "structs.h"

#define TYPE_POP 0
#define TYPE_IMAP 1


class XFE_PrefsMServerGeneralTab : public XFE_XmLTabView
{
public:
    typedef void (serverTypeCallback)(void *, MSG_SERVER_TYPE);
    
private:
	Widget m_server_label;
	Widget m_server_text;
	Widget m_server_type_menu;
	Widget m_server_type_label;
	Widget m_server_type_option;
	Widget m_server_user_label;
	Widget m_server_user_name;
	Widget m_remember_password;
	Widget m_check_mail;
	Widget m_check_time;
	Widget m_minute_label;
    Widget m_imap_button;
    Widget m_pop_button;
    Widget m_movemail_button;
    Widget m_download_toggle;
    
    serverTypeCallback *m_changed_callback;
    void *m_changed_closure;

    char *m_server_name;
    Widget m_parent;
    MSG_SERVER_TYPE m_server_type;
    XP_Bool m_is_new;
    MSG_SERVER_TYPE m_original_type;

public:
    
	XFE_PrefsMServerGeneralTab(XFE_Component *top, XFE_View *view,
                               Widget parent, XP_Bool allow_multiple);
	virtual ~XFE_PrefsMServerGeneralTab() {};
    void create(XP_Bool allow_multiple);
    void init(char *server_name, MSG_SERVER_TYPE server_type);
	void apply();

    void setChangedCallback(serverTypeCallback cb, void *closure);
    void setServerType(MSG_SERVER_TYPE type);
    void lockWidgets();
	XP_Bool	getRememberPassword() { 
		return XmToggleButtonGadgetGetState(m_remember_password);
	};

	XP_Bool getCheckMail() {
		return XmToggleButtonGadgetGetState(m_check_mail);
	};

    XP_Bool getDownload() {
        return XmToggleButtonGadgetGetState(m_download_toggle);
    }
    
    // Need to be careful here not to leak memory
	char 	*getServerName() {
		return XmTextFieldGetString(m_server_text);
	};

	char 	*getUserName() {
		return XmTextFieldGetString(m_server_user_name);
	};

	int		getWaitTime() {
		char *p = XmTextFieldGetString(m_check_time);
		int i = atoi(p);
		XtFree(p);
		return i;
	};

	static void optionImap_cb(Widget, XtPointer, XtPointer);
	static void optionPop_cb(Widget, XtPointer, XtPointer);
    static void optionMovemail_cb(Widget, XtPointer, XtPointer);
    void ChangedType(MSG_SERVER_TYPE);
};

class XFE_PrefsMServerIMAPTab : public XFE_XmLTabView
{

private:
	Widget m_delete_trash_toggle;
    Widget m_delete_mark_toggle;
    Widget m_delete_remove_toggle;
	HG18966
    Widget m_expunge_toggle;
    Widget m_trash_toggle;
#if 0
    Widget m_use_sub;
#endif

public:
	XFE_PrefsMServerIMAPTab(XFE_Component *top, XFE_View *view);
	virtual ~XFE_PrefsMServerIMAPTab() {};
    void create();
    void init(char *server_name);
    virtual void apply() {};
	void apply(char *server_name);

	HG27311

#if 0
    XP_Bool getUseSub() {
        return XmToggleButtonGadgetGetState(m_use_sub);
    };
#endif

    XP_Bool getExpungeQuit() {
        return XmToggleButtonGadgetGetState(m_expunge_toggle);
    }

    XP_Bool getTrashQuit() {
        return XmToggleButtonGadgetGetState(m_trash_toggle);
    }

};

class XFE_PrefsMServerAdvancedTab : public XFE_XmLTabView
{
private:
	Widget m_path_prefs_label;
	Widget m_personal_dir_label;
	Widget m_personal_dir_text;
	Widget m_public_dir_label;
	Widget m_public_dir_text;
	Widget m_other_label;
	Widget m_other_text;
	Widget m_allow_server;

public:
	XFE_PrefsMServerAdvancedTab(XFE_Component *top, XFE_View *view);
	virtual ~XFE_PrefsMServerAdvancedTab() {};
    void create();
    void init(char *server_name);
    virtual void apply() {};
	void apply(char *server_name);

	char 	*getImapPersonalDir() {
		return XmTextFieldGetString(m_personal_dir_text);
	};

	char 	*getImapPublicDir() {
		return XmTextFieldGetString(m_public_dir_text);
	};

	char 	*getImapOthersDir() {
		return XmTextFieldGetString(m_other_text);
	};

	XP_Bool getOverrideNamespaces() {
		return XmToggleButtonGadgetGetState(m_allow_server);
	};

};


class XFE_PrefsMServerPOPTab : public XFE_XmLTabView
{
private:
	Widget m_leave_messages;

public:
	XFE_PrefsMServerPOPTab(XFE_Component *top, XFE_View *view);
	virtual ~XFE_PrefsMServerPOPTab() {};

	XP_Bool	getLeaveMessages() { 
		return XmToggleButtonGadgetGetState(m_leave_messages);
	};

    void create();
    void init();
	void apply();
};

class XFE_PrefsMServerMovemailTab : public XFE_XmLTabView
{
private:
    Widget m_builtin_toggle;
    Widget m_external_toggle;

    Widget m_program_text;
    Widget m_choose_button;

public:
    XFE_PrefsMServerMovemailTab(XFE_Component *top, XFE_View *view);
    virtual ~XFE_PrefsMServerMovemailTab() {};

    static void toggleBuiltin_cb(Widget, XtPointer, XtPointer);
    static void chooseMovemail_cb(Widget, XtPointer, XtPointer);

    void toggleBuiltin(XmToggleButtonCallbackStruct *);
    void chooseMovemail();
    
    void create();
    void init();
    void apply();
    
};



class XFE_PrefsMServerDialog : public XFE_XmLFolderDialog
{
private:
	// Real data
    MSG_SERVER_TYPE m_server_type;
    XP_Bool m_allow_multiple;
    char *m_server_name;


	// Various tabs
	XFE_PrefsMServerGeneralTab *m_general_tab;
	XFE_PrefsMServerIMAPTab *m_imap_tab;
	XFE_PrefsMServerAdvancedTab *m_imap_advanced_tab;
	XFE_PrefsMServerPOPTab *m_pop_tab;
    XFE_PrefsMServerMovemailTab *m_movemail_tab;
    static void changedType(void *closure, MSG_SERVER_TYPE server_type);
    
public:
	XFE_PrefsMServerDialog(Widget parent, char *name,
                           XP_Bool allow_multiple,
                           MWContext *context);

    Widget getChrome() { return m_chrome; };
    void create();
    void init();
    virtual void apply();
    void ChangedType(MSG_SERVER_TYPE server_type);
};

#endif /* _xfe_prefsdialog_mserver_h */
