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
   PrefsPageLIFiles.cpp -- LI File preferences.
   Created: Daniel Malmer <malmer@netscape.com>, 21-Apr-98.
 */
 
#include "rosetta.h"
#include "felocale.h"
#include "prefapi.h"
#include "PrefsPageLIFiles.h"
#include "Xfe/Geometry.h"

// XFE_PrefsPageLIFiles

XFE_PrefsPageLIFiles::XFE_PrefsPageLIFiles(XFE_PrefsDialog* dialog) : XFE_PrefsPage(dialog)
{
}


XFE_PrefsPageLIFiles::XFE_PrefsPageLIFiles(Widget w) : XFE_PrefsPage(w)
{
}


XFE_PrefsPageLIFiles::~XFE_PrefsPageLIFiles()
{
}


void
XFE_PrefsPageLIFiles::create()
{
	int ac;
	Arg av[16];
	Widget frame_label;
	Widget form;
	Widget file_label;
	Widget row_column;

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	m_wPage = XmCreateFrame(m_wPageForm, "liFiles", av, ac);
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
	file_label = XmCreateLabelGadget(form, "fileLabel", av, ac);
	XtManageChild(file_label);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, file_label); ac++;
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNpacking, XmPACK_COLUMN); ac++;
	XtSetArg(av[ac], XmNnumColumns, 2); ac++;
	row_column = XmCreateRowColumn(form, "row_column", av, ac);
	XtManageChild(row_column);

	ac = 0;
	m_bookmark_toggle = XmCreateToggleButtonGadget(row_column, "bookmarkToggle", av, ac);
	XtManageChild(m_bookmark_toggle);

	ac = 0;
	m_cookies_toggle = XmCreateToggleButtonGadget(row_column, "cookiesToggle", av, ac);
	XtManageChild(m_cookies_toggle);

	ac = 0;
	m_filter_toggle = XmCreateToggleButtonGadget(row_column, "filterToggle", av, ac);
	XtManageChild(m_filter_toggle);

	ac = 0;
	m_addrbook_toggle = XmCreateToggleButtonGadget(row_column, "addrbookToggle", av, ac);
	XtManageChild(m_addrbook_toggle);

	ac = 0;
	m_navcenter_toggle = XmCreateToggleButtonGadget(row_column, "navcenterToggle", av, ac);
	XtManageChild(m_navcenter_toggle);

	ac = 0;
	m_prefs_toggle = XmCreateToggleButtonGadget(row_column, "prefsToggle", av, ac);
	XtManageChild(m_prefs_toggle);

	ac = 0;
	m_javasec_toggle = XmCreateToggleButtonGadget(row_column, "javasecToggle", av, ac);
	XtManageChild(m_javasec_toggle);

	HG82167

	setCreated(TRUE);
}


void
XFE_PrefsPageLIFiles::init()
{
	XP_Bool enabled = FALSE;
	PREF_GetBoolPref("li.client.bookmarks", &enabled);
	XtVaSetValues(m_bookmark_toggle, XmNset, enabled,
			  XmNsensitive, !PREF_PrefIsLocked("li.client.bookmarks"), NULL);

	PREF_GetBoolPref("li.client.cookies", &enabled);
	XtVaSetValues(m_cookies_toggle, XmNset, enabled,
				  XmNsensitive, !PREF_PrefIsLocked("li.client.cookies"), NULL);

	PREF_GetBoolPref("li.client.filters", &enabled);
	XtVaSetValues(m_filter_toggle, XmNset, enabled,
				  XmNsensitive, !PREF_PrefIsLocked("li.client.filters"), NULL);

	PREF_GetBoolPref("li.client.addressbook", &enabled);
	XtVaSetValues(m_addrbook_toggle, XmNset, enabled,
			  XmNsensitive, !PREF_PrefIsLocked("li.client.addressbook"), NULL);

	PREF_GetBoolPref("li.client.navcntr", &enabled);
	XtVaSetValues(m_navcenter_toggle, XmNset, enabled,
				  XmNsensitive, !PREF_PrefIsLocked("li.client.navcntr"), NULL);

	PREF_GetBoolPref("li.client.liprefs", &enabled);
	XtVaSetValues(m_prefs_toggle, XmNset, enabled,
				  XmNsensitive, !PREF_PrefIsLocked("li.client.liprefs"), NULL);

	PREF_GetBoolPref("li.client.javasecurity", &enabled);
	XtVaSetValues(m_javasec_toggle, XmNset, enabled,
			  XmNsensitive, !PREF_PrefIsLocked("li.client.javasecurity"), NULL);

	HG21761
}


void
XFE_PrefsPageLIFiles::install()
{
}


void
XFE_PrefsPageLIFiles::save()
{
	PREF_SetBoolPref("li.client.bookmarks", 
					XmToggleButtonGetState(m_bookmark_toggle));
	PREF_SetBoolPref("li.client.cookies", 
					XmToggleButtonGetState(m_cookies_toggle));
	PREF_SetBoolPref("li.client.filters", 
					XmToggleButtonGetState(m_filter_toggle));
	PREF_SetBoolPref("li.client.addressbook", 
					XmToggleButtonGetState(m_addrbook_toggle));
	PREF_SetBoolPref("li.client.navcntr", 
					XmToggleButtonGetState(m_navcenter_toggle));
	PREF_SetBoolPref("li.client.liprefs", 
					XmToggleButtonGetState(m_prefs_toggle));
	PREF_SetBoolPref("li.client.javasecurity", 
					XmToggleButtonGetState(m_javasec_toggle));
	HG10280
}


Boolean
XFE_PrefsPageLIFiles::verify()
{
	return TRUE;
}


Widget
XFE_PrefsPageLIFiles::get_frame()
{
	return m_wPage;
}


