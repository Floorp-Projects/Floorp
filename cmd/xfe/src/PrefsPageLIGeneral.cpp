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



/*
   PrefsPageLIGeneral.cpp -- LI General preferences.
   Created: Daniel Malmer <malmer@netscape.com>, 21-Apr-98.
 */


#include "felocale.h"
#include "prefapi.h"
#include "secnav.h"
#include "PrefsPageLIGeneral.h"
#include "Xfe/Geometry.h"

#include <Xm/Label.h>


XFE_PrefsPageLIGeneral::XFE_PrefsPageLIGeneral(XFE_PrefsDialog* dialog, DialogUsage usage) : XFE_PrefsPage(dialog)
{
	m_usage = usage;
}


XFE_PrefsPageLIGeneral::XFE_PrefsPageLIGeneral(Widget w, DialogUsage usage) : XFE_PrefsPage(w)
{
	m_usage = usage;
}


XFE_PrefsPageLIGeneral::~XFE_PrefsPageLIGeneral()
{
}


// XFE_PrefsPageLIGeneral::create
// Contains general prefs and user info.
// General prefs consists of an 'enable' toggle, and a 'background sync'
// toggle, with a text field for the sync interval.
// User info consists of a text field for user name and password.
void
XFE_PrefsPageLIGeneral::create()
{
	int ac;
	Arg av[16];
	Widget top_label;
	Widget top_form;
	Widget bottom_label;
	Widget bottom_form;
	Widget user_info_label;
	Widget user_label;
	Widget password_label;
	Dimension label_height;
	Dimension label_width;
	int max_length;

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	m_wPage = XmCreateForm(m_wPageForm, "liGeneral", av, ac);
	XtManageChild(m_wPage);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	m_top_frame = XmCreateFrame(m_wPage, "topFrame", av, ac);
	XtManageChild(m_top_frame);

	ac = 0;
	XtSetArg(av[ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	top_label = XmCreateLabelGadget(m_top_frame, "topTitle", av, ac);
	XtManageChild(top_label);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	top_form = XmCreateForm(m_top_frame, "topForm", av, ac);
	XtManageChild(top_form);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	m_enable_toggle = XmCreateToggleButtonGadget(top_form, "enableToggle", av, ac);
	XtManageChild(m_enable_toggle);

#ifdef LI_BACKGROUND_SYNC_ENABLED
	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, m_enable_toggle); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	m_sync_toggle = XmCreateToggleButtonGadget(top_form, "syncToggle", av, ac);
	XtManageChild(m_sync_toggle);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, m_sync_toggle); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNleftWidget, m_sync_toggle); ac++;
	XtSetArg(av[ac], XmNcolumns, 4); ac++;
	m_sync_text = XmCreateTextField(top_form, "syncText", av, ac);
	XtManageChild(m_sync_text);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, m_sync_toggle); ac++;
	XtSetArg(av[ac], XmNtopWidget, m_sync_toggle); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNleftWidget, m_sync_text); ac++;
	unit_label = XmCreateLabelGadget(top_form, "unitLabel", av, ac);
	XtManageChild(unit_label);
#endif

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, m_top_frame); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	m_bottom_frame = XmCreateFrame(m_wPage, "bottomFrame", av, ac);
	XtManageChild(m_bottom_frame);

	ac = 0;
	XtSetArg(av[ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	bottom_label = XmCreateLabelGadget(m_bottom_frame, "bottomTitle", av, ac);
	XtManageChild(bottom_label);

#ifdef LI_BACKGROUND_SYNC_ENABLED
	label_height = XfeVaGetTallestWidget(m_sync_toggle, m_sync_text, unit_label, NULL);
	XtVaSetValues(m_sync_toggle, XmNheight, label_height, NULL);
	XtVaSetValues(m_sync_text, XmNheight, label_height, NULL);
	XtVaSetValues(unit_label, XmNheight, label_height, NULL);
#endif

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	bottom_form = XmCreateForm(m_bottom_frame, "bottomForm", av, ac);
	XtManageChild(bottom_form);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	user_info_label = XmCreateLabel(bottom_form, "userInfoLabel", av, ac);
	XtManageChild(user_info_label);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, user_info_label); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	user_label = XmCreateLabel(bottom_form, "userLabel", av, ac);
	XtManageChild(user_label);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, user_info_label); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNleftWidget, user_label); ac++;
	XtSetArg(av[ac], XmNalignment, XmALIGNMENT_END); ac++;
	m_user_text = XmCreateTextField(bottom_form, "userText", av, ac);
	XtManageChild(m_user_text);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, user_label); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNalignment, XmALIGNMENT_END); ac++;
	password_label = XmCreateLabel(bottom_form, "passwordLabel", av, ac);
	if ( m_usage == login ) XtManageChild(password_label);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, m_user_text); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
	XtSetArg(av[ac], XmNleftWidget, m_user_text); ac++;
	m_password_text = XmCreateTextField(bottom_form, "passwordText", av, ac);
	if ( m_usage == login ) XtManageChild(m_password_text);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, m_user_text); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
	XtSetArg(av[ac], XmNleftWidget, m_user_text); ac++;
	m_save_password_toggle = XmCreateToggleButtonGadget(bottom_form, "savePasswordToggle", av, ac);
	if ( m_usage == pref ) XtManageChild(m_save_password_toggle);

	label_height = XfeVaGetTallestWidget(user_label, m_user_text, NULL);
	XtVaSetValues(user_label, XmNheight, label_height, NULL);
	XtVaSetValues(m_user_text, XmNheight, label_height, NULL);

	label_height = XfeVaGetTallestWidget(password_label, m_password_text, m_save_password_toggle, NULL);
	XtVaSetValues(password_label, XmNheight, label_height, NULL);
	XtVaSetValues(m_password_text, XmNheight, label_height, NULL);
	XtVaSetValues(m_save_password_toggle, XmNheight, label_height, NULL);

	label_width = XfeVaGetWidestWidget(user_label, password_label, NULL);
	XtVaSetValues(user_label, XmNwidth, label_width, NULL);
	XtVaSetValues(password_label, XmNwidth, label_width, NULL);

#ifdef LI_BACKGROUND_SYNC_ENABLED
	/* TODO - add callback to validate sync_text */
	XtAddCallback(m_sync_toggle, XmNvalueChangedCallback, cb_toggle, this);
#endif

	XtVaGetValues(m_password_text, XmNmaxLength, &max_length, NULL);
	fe_SetupPasswdText(m_password_text, max_length);

	setCreated(TRUE);
}


#ifdef LI_BACKGROUND_SYNC_ENABLED
void
XFE_PrefsPageLIGeneral::syncToggle(Widget w, XtPointer calldata)
{
	XmToggleButtonCallbackStruct* cb = (XmToggleButtonCallbackStruct*) calldata;

	if ( w == m_sync_toggle ) {
		XtSetSensitive(m_sync_text, 
						!PREF_PrefIsLocked("li.sync.time") && cb->set);
	} else {
		abort();
	}
}


void
XFE_PrefsPageLIGeneral::cb_toggle(Widget w, XtPointer closure, XtPointer call_data)
{
	((XFE_PrefsPageLIGeneral*)closure)->syncToggle(w, call_data);
}
#endif /* LI_BACKGROUND_SYNC_ENABLED */


void
XFE_PrefsPageLIGeneral::init()
{
	XP_Bool li_enabled = FALSE;
#ifdef LI_BACKGROUND_SYNC_ENABLED
	XP_Bool li_sync_enabled = FALSE;
	int32 li_sync_time = 0;
#endif
	char* li_login_name = NULL;

	PREF_GetBoolPref("li.enabled", &li_enabled);
	PREF_CopyCharPref("li.login.name", &li_login_name);
#ifdef LI_BACKGROUND_SYNC_ENABLED
	PREF_GetBoolPref("li.sync.enabled", &li_sync_enabled);
	PREF_GetIntPref("li.sync.time", &li_sync_time);
#endif

	XtVaSetValues(m_enable_toggle, XmNset, li_enabled, 
				  XmNsensitive, !PREF_PrefIsLocked("li.enabled"), NULL);
#ifdef LI_BACKGROUND_SYNC_ENABLED
	XtVaSetValues(m_sync_toggle, XmNset, li_sync_enabled, 
				  XmNsensitive, !PREF_PrefIsLocked("li.sync.enabled"), NULL);
	XtVaSetValues(m_sync_text, XmNsensitive, 
				  !PREF_PrefIsLocked("li.sync.time") && li_sync_enabled, NULL);

	/* TODO - convert li_sync_time to str */
	fe_SetTextField(m_sync_text, "");
#endif /* LI_BACKGROUND_SYNC_ENABLED */
	fe_SetTextField(m_user_text, li_login_name);
	fe_SetTextField(m_password_text, "");

	XtSetSensitive(m_user_text, !PREF_PrefIsLocked("li.login.name"));
	XtSetSensitive(m_password_text, !PREF_PrefIsLocked("li.login.password"));

	setInitialized(TRUE);
}


void
XFE_PrefsPageLIGeneral::install()
{
}


void
XFE_PrefsPageLIGeneral::save()
{
	char* str;
	char* munged_str;

	PREF_SetBoolPref("li.enabled", XmToggleButtonGetState(m_enable_toggle));
#ifdef LI_BACKGROUND_SYNC_ENABLED
	PREF_SetBoolPref("li.sync.enabled", XmToggleButtonGetState(m_sync_toggle));
	/* TODO - background sync time */
#endif
	str = fe_GetTextField(m_user_text);
	PREF_SetCharPref("li.login.name", str);
	if ( str ) XtFree(str);

	if ( XtIsManaged(m_password_text) ) {
		str = fe_GetPasswdText(m_password_text);
		munged_str = str ? SECNAV_MungeString(str) : (char *)NULL;
		PREF_SetCharPref("li.login.password", munged_str);
		XP_FREEIF(munged_str);
		if ( str ) XtFree(str);
	}

	if ( XtIsManaged(m_save_password_toggle) ) {
		PREF_SetBoolPref("li.login.save_password", 
						  XmToggleButtonGetState(m_save_password_toggle));
	}
}


Boolean
XFE_PrefsPageLIGeneral::verify()
{
	return TRUE;
}


Widget
XFE_PrefsPageLIGeneral::get_top_frame()
{
	return m_top_frame;
}


Widget
XFE_PrefsPageLIGeneral::get_bottom_frame()
{
	return m_bottom_frame;
}


