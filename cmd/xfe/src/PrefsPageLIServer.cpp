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
   PrefsPageLIServer.cpp -- LI server preferences.
   Created: Daniel Malmer <malmer@netscape.com>, 21-Apr-98.
 */
 
 
#include "felocale.h"
#include "prefapi.h"
#include "PrefsPageLIServer.h"
#include "Xfe/Geometry.h"
 
#include <Xm/Label.h>


XFE_PrefsPageLIServer::XFE_PrefsPageLIServer(XFE_PrefsDialog* dialog) : XFE_PrefsPage(dialog)
{
}


XFE_PrefsPageLIServer::XFE_PrefsPageLIServer(Widget w) : XFE_PrefsPage(w)
{
}


XFE_PrefsPageLIServer::~XFE_PrefsPageLIServer()
{
}


void
XFE_PrefsPageLIServer::create()
{
	int ac;
	Arg av[16];
	Widget frame_label;
	Widget form;
	Widget server_label;
	Widget ldap_addr_label;
	Widget ldap_base_label;
	Widget http_base_label;
	Dimension spacing;
	Dimension indicator_size;
	Dimension label_height;
	Dimension label_width;

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	m_wPage = XmCreateFrame(m_wPageForm, "liServer", av, ac);
	XtManageChild(m_wPage);

	ac = 0;
	XtSetArg(av[ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	frame_label = XmCreateLabelGadget(m_wPage, "frameLabel", av, ac);
	XtManageChild(frame_label);

	ac = 0;
	form = XmCreateForm(m_wPage, "form", av, ac);
	XtManageChild(form);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	server_label = XmCreateLabelGadget(form, "serverLabel", av, ac);
	XtManageChild(server_label);

	ac = 0;
	XtSetArg(av[ac], XmNindicatorType, XmONE_OF_MANY); ac++;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, server_label); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNtopOffset, 4); ac++;
	XtSetArg(av[ac], XmNleftOffset, 16); ac++;
	m_ldap_toggle = XmCreateToggleButtonGadget(form, "ldapToggle", av, ac);
	XtManageChild(m_ldap_toggle);

	XtVaGetValues(m_ldap_toggle, XmNindicatorSize, &indicator_size,
					XmNspacing, &spacing, NULL);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, m_ldap_toggle); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
	XtSetArg(av[ac], XmNleftWidget, m_ldap_toggle); ac++;
	XtSetArg(av[ac], XmNleftOffset, indicator_size + spacing); ac++;
	XtSetArg(av[ac], XmNalignment, XmALIGNMENT_END); ac++;
	ldap_addr_label = XmCreateLabelGadget(form, "ldapAddrLabel", av, ac);
	XtManageChild(ldap_addr_label);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, m_ldap_toggle); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNleftWidget, ldap_addr_label); ac++;
	m_ldap_addr_text = XmCreateTextField(form, "ldapAddrText", av, ac);
	XtManageChild(m_ldap_addr_text);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, ldap_addr_label); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
	XtSetArg(av[ac], XmNleftWidget, m_ldap_toggle); ac++;
	XtSetArg(av[ac], XmNleftOffset, indicator_size + spacing); ac++;
	XtSetArg(av[ac], XmNalignment, XmALIGNMENT_END); ac++;
	ldap_base_label = XmCreateLabelGadget(form, "ldapBaseLabel", av, ac);
	XtManageChild(ldap_base_label);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, m_ldap_addr_text); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNleftWidget, ldap_base_label); ac++;
	m_ldap_base_text = XmCreateTextField(form, "ldapBaseText", av, ac);
	XtManageChild(m_ldap_base_text);

	ac = 0;
	XtSetArg(av[ac], XmNindicatorType, XmONE_OF_MANY); ac++;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, ldap_base_label); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
	XtSetArg(av[ac], XmNleftWidget, m_ldap_toggle); ac++;
	XtSetArg(av[ac], XmNtopOffset, 4); ac++;
	m_http_toggle = XmCreateToggleButtonGadget(form, "httpToggle", av, ac);
	XtManageChild(m_http_toggle);

	XtVaGetValues(m_http_toggle, XmNindicatorSize, &indicator_size,
					XmNspacing, &spacing, NULL);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, m_http_toggle); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
	XtSetArg(av[ac], XmNleftWidget, m_ldap_toggle); ac++;
	XtSetArg(av[ac], XmNleftOffset, indicator_size + spacing); ac++;
	XtSetArg(av[ac], XmNalignment, XmALIGNMENT_END); ac++;
	http_base_label = XmCreateLabelGadget(form, "httpBaseLabel", av, ac);
	XtManageChild(http_base_label);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, m_http_toggle); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNleftWidget, http_base_label); ac++;
	m_http_base_text = XmCreateTextField(form, "httpBaseText", av, ac);
	XtManageChild(m_http_base_text);

	label_height = XfeVaGetTallestWidget(ldap_addr_label, m_ldap_addr_text, NULL);
	XtVaSetValues(ldap_addr_label, XmNheight, label_height, NULL);
	XtVaSetValues(m_ldap_addr_text, XmNheight, label_height, NULL);

	label_height = XfeVaGetTallestWidget(ldap_base_label, m_ldap_base_text, NULL);
	XtVaSetValues(ldap_base_label, XmNheight, label_height, NULL);
	XtVaSetValues(m_ldap_base_text, XmNheight, label_height, NULL);

	label_height = XfeVaGetTallestWidget(http_base_label, m_http_base_text, NULL);
	XtVaSetValues(http_base_label, XmNheight, label_height, NULL);
	XtVaSetValues(m_http_base_text, XmNheight, label_height, NULL);

	label_width = XfeVaGetWidestWidget(ldap_addr_label, ldap_base_label,
										http_base_label, NULL);
	XtVaSetValues(ldap_addr_label, XmNwidth, label_width, NULL);
	XtVaSetValues(ldap_base_label, XmNwidth, label_width, NULL);
	XtVaSetValues(http_base_label, XmNwidth, label_width, NULL);

	XtAddCallback(m_ldap_toggle, XmNvalueChangedCallback, cb_toggle, this);
	XtAddCallback(m_http_toggle, XmNvalueChangedCallback, cb_toggle, this);

	setCreated(TRUE);
}


void
XFE_PrefsPageLIServer::init()
{
	XP_Bool use_ldap;
	char* li_protocol = NULL;
	char* ldap_addr = NULL;
	char* ldap_base = NULL;
	char* http_base = NULL;

	PREF_CopyCharPref("li.protocol", &li_protocol);
	PREF_CopyCharPref("li.server.ldap.url", &ldap_addr);
	PREF_CopyCharPref("li.server.ldap.userbase", &ldap_base);
	PREF_CopyCharPref("li.server.http.baseURL", &http_base);

	fe_SetTextField(m_ldap_addr_text, ldap_addr);
	fe_SetTextField(m_ldap_base_text, ldap_base);
	fe_SetTextField(m_http_base_text, http_base);

	/* ldap is the default, so use ldap if li_protocol == NULL */
	use_ldap = (!li_protocol || !XP_STRCMP(li_protocol, "ldap"));

	XtVaSetValues(m_ldap_toggle, XmNset, use_ldap, 
				  XmNsensitive, !PREF_PrefIsLocked("li.protocol"), NULL);
	XtVaSetValues(m_ldap_addr_text, XmNsensitive, 
				  !PREF_PrefIsLocked("li.server.ldap.url") && use_ldap, NULL);
	XtVaSetValues(m_ldap_base_text, XmNsensitive,
				  !PREF_PrefIsLocked("li.server.ldap.userbase") && use_ldap, NULL);
	XtVaSetValues(m_http_toggle, XmNset, !use_ldap, 
				  XmNsensitive, !PREF_PrefIsLocked("li.protocol"), NULL);
	XtVaSetValues(m_http_base_text, XmNsensitive,
				  !PREF_PrefIsLocked("li.server.http.baseURL") && !use_ldap, NULL);

	setInitialized(TRUE);
}


void
XFE_PrefsPageLIServer::install()
{
}


void
XFE_PrefsPageLIServer::save()
{
	char* str;

	PREF_SetCharPref("li.protocol", 
					XmToggleButtonGetState(m_ldap_toggle) ? "ldap" : "http");

	str = fe_GetTextField(m_ldap_addr_text);
	PREF_SetCharPref("li.server.ldap.url", str);
	if ( str ) XtFree(str);
	str = fe_GetTextField(m_ldap_base_text);
	PREF_SetCharPref("li.server.ldap.userbase", str);
	if ( str ) XtFree(str);
	str = fe_GetTextField(m_http_base_text);
	PREF_SetCharPref("li.server.http.baseURL", str);
	if ( str ) XtFree(str);
}


void
XFE_PrefsPageLIServer::cb_toggle(Widget w, XtPointer closure, XtPointer data)
{
	((XFE_PrefsPageLIServer*)closure)->toggleCallback(w, data);	
}


void
XFE_PrefsPageLIServer::toggleCallback(Widget w, XtPointer call_data)
{
	XP_Bool use_ldap;
	XmToggleButtonCallbackStruct* cb = (XmToggleButtonCallbackStruct*) call_data;

	if ( !cb->set ) {
		XtVaSetValues(w, XmNset, True, NULL);
	} else {
		use_ldap = ( w == m_ldap_toggle );

		XmToggleButtonSetState(m_ldap_toggle, use_ldap, False);
		XmToggleButtonSetState(m_http_toggle, !use_ldap, False);

		XtSetSensitive(m_ldap_addr_text, 
					use_ldap && !PREF_PrefIsLocked("li.server.ldap.url"));
		XtSetSensitive(m_ldap_base_text, 
					use_ldap && !PREF_PrefIsLocked("li.server.ldap.userbase"));
		XtSetSensitive(m_http_base_text, 
					!use_ldap && !PREF_PrefIsLocked("li.server.http.baseURL"));
	}
}


Boolean
XFE_PrefsPageLIServer::verify()
{
	return TRUE;
}


Widget
XFE_PrefsPageLIServer::get_frame()
{
	return m_wPage;
}


