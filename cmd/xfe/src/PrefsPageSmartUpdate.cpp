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
   PrefsPageSmartUpdate.cpp -- LI File preferences.
   Created: Daniel Malmer <malmer@netscape.com>, 21-Apr-98.
 */
 
 
#include "felocale.h"
#include "xpgetstr.h"
#include "prefapi.h"
#include "softupdt.h"
#include "PrefsPageSmartUpdate.h"
#include "Xfe/Geometry.h"
#include "NSReg.h"

extern int MK_OUT_OF_MEMORY;
extern int SU_CONTINUE_UNINSTALL;
extern int SU_ERROR_UNINSTALL;

// XFE_PrefsPageSmartUpdate

XFE_PrefsPageSmartUpdate::XFE_PrefsPageSmartUpdate(XFE_PrefsDialog* dialog) : XFE_PrefsPage(dialog)
{
}


XFE_PrefsPageSmartUpdate::~XFE_PrefsPageSmartUpdate()
{
}


void
XFE_PrefsPageSmartUpdate::create()
{
	int ac;
	Arg av[16];
	Widget top_frame;
	Widget bottom_frame;
	Widget column;
	Widget form;
	Widget uninstall_label;
	Widget uninstall_button;

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	m_wPage = XmCreateForm(m_wPageForm, "smartUpdate", av, ac);
	XtManageChild(m_wPage);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNtopOffset, 4); ac++;
	top_frame = XmCreateFrame(m_wPage, "topFrame", av, ac);
	XtManageChild(top_frame);

	ac = 0;
	column = XmCreateRowColumn(top_frame, "column", av, ac);
	XtManageChild(column);

	ac = 0;
	m_enable_toggle = XmCreateToggleButtonGadget(column, "enableToggle", av, ac);
	XtManageChild(m_enable_toggle);

	ac = 0;
	m_confirm_toggle = XmCreateToggleButtonGadget(column, "confirmToggle", av, ac);
	XtManageChild(m_confirm_toggle);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, top_frame); ac++;
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNtopOffset, 4); ac++;
	bottom_frame = XmCreateFrame(m_wPage, "bottomFrame", av, ac);
	XtManageChild(bottom_frame);

	ac = 0;
	form = XmCreateForm(bottom_frame, "form", av, ac);
	XtManageChild(form);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_NONE); ac++;
	uninstall_label = XmCreateLabelGadget(form, "uninstallLabel", av, ac);
	XtManageChild(uninstall_label);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, uninstall_label); ac++;
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_NONE); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	uninstall_button = XmCreatePushButton(form, "uninstallButton", av, ac);
	XtManageChild(uninstall_button);

	XtAddCallback(uninstall_button, XmNactivateCallback, uninstall_cb, this);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, uninstall_label); ac++;
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNrightWidget, uninstall_button); ac++;
	m_uninstall_list = XmCreateList(form, "uninstallList", av, ac);
	XtManageChild(m_uninstall_list);

	/* Leave this unmanaged */
	ac = 0;
	m_fake_list = XmCreateList(form, "fakeList", av, ac);

	setCreated(TRUE);
}


void
XFE_PrefsPageSmartUpdate::init()
{
	void* context = NULL;
	char packageName[1024];
	char regPackageName[1024];
	XP_Bool enabled;

	PREF_GetBoolPref("autoupdate.enabled", &enabled);
	XtVaSetValues(m_enable_toggle, XmNset, enabled,
			  XmNsensitive, !PREF_PrefIsLocked("autoupdate.enabled"), NULL);

	PREF_GetBoolPref("autoupdate.confirm_install", &enabled);
	XtVaSetValues(m_confirm_toggle, XmNset, enabled,
			  XmNsensitive, !PREF_PrefIsLocked("autoupdate.confirm"), NULL);

	XmListDeleteAllItems(m_uninstall_list);
	XmListDeleteAllItems(m_fake_list);

	while ( SU_EnumUninstall(&context, 
							 packageName, sizeof(packageName), 
							 regPackageName, sizeof(regPackageName)) == 
					REGERR_OK ) {
		XmString xm_str;

		/* Don't let them uninstall Communicator! */
		if ( !XP_STRCMP(packageName, UNINSTALL_NAV_STR) ) continue;

		/* If no package name exists, use the registry name */
		xm_str = XmStringCreate(packageName[0] ? packageName : 
								regPackageName, XmFONTLIST_DEFAULT_TAG);	
		XmListAddItem(m_uninstall_list, xm_str, 0);
		XmStringFree(xm_str);

		xm_str = XmStringCreate(regPackageName, XmFONTLIST_DEFAULT_TAG);	
		XmListAddItem(m_fake_list, xm_str, 0);
		XmStringFree(xm_str);
	}
}


void
XFE_PrefsPageSmartUpdate::install()
{
}


void
XFE_PrefsPageSmartUpdate::save()
{
	PREF_SetBoolPref("autoupdate.enabled",
					XmToggleButtonGetState(m_enable_toggle));
	PREF_SetBoolPref("autoupdate.confirm_install",
					XmToggleButtonGetState(m_confirm_toggle));
}


Boolean
XFE_PrefsPageSmartUpdate::verify()
{
	return TRUE;
}


void
XFE_PrefsPageSmartUpdate::uninstallCB(Widget /* w */, XtPointer /* calldata */)
{
	int* list = NULL;
	int count = 0;
	XmString* xm_pkg_names = NULL;
	XmString* xm_registry_names = NULL;
	char* pkg_name = NULL;
	char* registry_name = NULL;
	char* confirm_msg = NULL;

	XmListGetSelectedPos(m_uninstall_list, &list, &count);

	/* count should always be either '0' or '1' */
	if ( count == 1 ) {

		XtVaGetValues(m_uninstall_list, XmNitems, &xm_pkg_names, NULL);
		XtVaGetValues(m_fake_list, XmNitems, &xm_registry_names, NULL);

		XmStringGetLtoR(xm_pkg_names[(*list)-1], 
						XmFONTLIST_DEFAULT_TAG, &pkg_name);
		XmStringGetLtoR(xm_registry_names[(*list)-1], 
						XmFONTLIST_DEFAULT_TAG, &registry_name);

		if ( !pkg_name || !*pkg_name || !registry_name || !*registry_name ) {
			FE_Alert(getContext(), XP_GetString(MK_OUT_OF_MEMORY));
			return;
		}

		confirm_msg = PR_smprintf(XP_GetString(SU_CONTINUE_UNINSTALL), 
									pkg_name);

		if ( confirm_msg == NULL ) {
			FE_Alert(getContext(), XP_GetString(MK_OUT_OF_MEMORY));
			return;
		}

		if ( FE_Confirm(getContext(), confirm_msg) ) {
			if ( SU_Uninstall(registry_name) < 0 ) {
				FE_Alert(getContext(), XP_GetString(SU_ERROR_UNINSTALL));
			} else {
				XmListDeletePositions(m_uninstall_list, list, count);
				XmListDeletePositions(m_fake_list, list, count);
			}
		}

		if ( pkg_name ) XtFree(pkg_name);
		if ( registry_name ) XtFree(registry_name);
		XP_FREEIF(confirm_msg);
	}
}


void
XFE_PrefsPageSmartUpdate::uninstall_cb(Widget w, XtPointer closure, XtPointer calldata)
{
	((XFE_PrefsPageSmartUpdate*) closure)->uninstallCB(w, calldata);
}


