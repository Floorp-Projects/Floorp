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
   PrefsDialogOffline.cpp -- the Offline pages in XFE preferences dialogs
   Created: Linda Wei <lwei@netscape.com>, 11-Dec-96.
 */

#include "felocale.h"
#include "structs.h"
#include "fonts.h"
#include "xpassert.h"
#include "prprf.h"
#include "xfe.h"
#include "PrefsDialog.h"

#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/CascadeBG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/ToggleB.h>
#include <Xm/ArrowBG.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/RowColumn.h>
#include <Xm/LabelG.h> 
#include <Xm/TextF.h> 
#include <Xm/ToggleBG.h> 
#include <XmL/Grid.h>
#include <XmL/Folder.h>
#include <Xfe/Xfe.h>

extern "C"
{
	char     *XP_GetString(int i);
	void      fe_installOffline();
	void      fe_installOfflineNews();

	static void fe_create_download_messages(PrefsDataOfflineNews *);
	static void fe_create_download_from(PrefsDataOfflineNews *);
}

extern int XFE_OFFLINE_NEWS_ALL_MSGS;
extern int XFE_OFFLINE_NEWS_UNREAD_MSGS;

extern int XFE_OFFLINE_NEWS_YESTERDAY;
extern int XFE_OFFLINE_NEWS_ONE_WK_AGO;
extern int XFE_OFFLINE_NEWS_TWO_WKS_AGO;
extern int XFE_OFFLINE_NEWS_ONE_MONTH_AGO;
extern int XFE_OFFLINE_NEWS_SIX_MONTHS_AGO;
extern int XFE_OFFLINE_NEWS_ONE_YEAR_AGO;

// Make sure the entries correspond to the prefs defined in prefs.h

int *downloadMessages[] = {
	&XFE_OFFLINE_NEWS_ALL_MSGS,
	&XFE_OFFLINE_NEWS_UNREAD_MSGS,
};

int *downloadFrom[] = {
	&XFE_OFFLINE_NEWS_YESTERDAY,
	&XFE_OFFLINE_NEWS_ONE_WK_AGO,
	&XFE_OFFLINE_NEWS_TWO_WKS_AGO,
	&XFE_OFFLINE_NEWS_ONE_MONTH_AGO,
	&XFE_OFFLINE_NEWS_SIX_MONTHS_AGO,
	&XFE_OFFLINE_NEWS_ONE_YEAR_AGO,
};

// ************************************************************************
// *************************       Offline        *************************
// ************************************************************************

// Member:       XFE_PrefsPageOffline
// Description:  Constructor
// Inputs:
// Side effects: 

XFE_PrefsPageOffline::XFE_PrefsPageOffline(XFE_PrefsDialog *dialog)
	: XFE_PrefsPage(dialog),
	  m_prefsDataOffline(0)
{
}

// Member:       ~XFE_PrefsPageOffline
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsPageOffline::~XFE_PrefsPageOffline()
{
	delete m_prefsDataOffline;
}

// Member:       create
// Description:  Creates page for Offline
// Inputs:
// Side effects: 

void XFE_PrefsPageOffline::create()
{
	Widget            kids[100];
	Arg               av[50];
	int               ac;
	int               i;

	PrefsDataOffline *fep = NULL;

	fep = new PrefsDataOffline;
	memset(fep, 0, sizeof(PrefsDataOffline));
	m_prefsDataOffline = fep;

	fep->context = getContext();
	fep->prompt_dialog = getPrefsDialog()->getDialogChrome();

	Widget form;

	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	form = XmCreateForm (m_wPageForm, "offline", av, ac);
	XtManageChild (form);
	m_wPage = fep->page = form;

	// Startup 

	Widget frame1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame1 = XmCreateFrame (form, "startupFrame", av, ac);

	Widget form1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form1 = XmCreateForm (frame1, "startupBox", av, ac);

	Widget label1;
	Widget online_toggle;
	Widget offline_toggle;
	Widget ask_toggle;
	Widget online_desc;
	Widget offline_desc;
	Widget ask_desc;

	ac = 0;
	XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	label1 = XmCreateLabelGadget (frame1, "startupBoxLabel", av, ac);

	ac = 0;
	i = 0;

	kids[i++] = online_toggle = XmCreateToggleButtonGadget (form1,
															"online",
															av, ac);
	fep->online_toggle = online_toggle;

	kids[i++] = online_desc = XmCreateLabelGadget (form1, "onlineDesc",
												   av, ac);

	kids[i++] = offline_toggle = XmCreateToggleButtonGadget (form1,
															 "offline",
															 av, ac);
	fep->offline_toggle = offline_toggle;

	kids[i++] = offline_desc = XmCreateLabelGadget (form1, "offlineDesc",
													av, ac);

	kids[i++] = ask_toggle = XmCreateToggleButtonGadget (form1,
														 "ask",
														 av, ac);
	fep->ask_toggle = ask_toggle;

	kids[i++] = ask_desc = XmCreateLabelGadget (form1, "askDesc",
												av, ac);

	Dimension spacing;
	Dimension toggle_width;
	XtVaGetValues(online_toggle,
				  XmNindicatorSize, &toggle_width,
				  XmNspacing, &spacing,
				  NULL);

	XtVaSetValues (online_toggle,
				   XmNindicatorType, XmONE_OF_MANY,
				   XmNtopAttachment, XmATTACH_FORM,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);
	XtVaSetValues (online_desc,
				   XmNalignment, XmALIGNMENT_BEGINNING,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, online_toggle,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNleftOffset, toggle_width+spacing,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);
	XtVaSetValues (offline_toggle,
				   XmNindicatorType, XmONE_OF_MANY,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, online_desc,
				   XmNtopOffset, 4,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, online_toggle,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);
	XtVaSetValues (offline_desc,
				   XmNalignment, XmALIGNMENT_BEGINNING,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, offline_toggle,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, online_desc,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);
	XtVaSetValues (ask_toggle,
				   XmNindicatorType, XmONE_OF_MANY,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, offline_desc,
				   XmNtopOffset, 4,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, online_toggle,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);
	XtVaSetValues (ask_desc,
				   XmNalignment, XmALIGNMENT_BEGINNING,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, ask_toggle,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, online_desc,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtManageChildren (kids, i);
	XtManageChild (label1);
	XtManageChild (form1);
	XtManageChild(frame1);

	XtAddCallback (online_toggle, XmNvalueChangedCallback,
				   cb_toggleStartup, fep);
	XtAddCallback (offline_toggle, XmNvalueChangedCallback,
				   cb_toggleStartup, fep);
	XtAddCallback (ask_toggle, XmNvalueChangedCallback,
				   cb_toggleStartup, fep);

	setCreated(TRUE);
}

// Member:       init
// Description:  Initializes page for Offline
// Inputs:
// Side effects: 

void XFE_PrefsPageOffline::init()
{
	XP_ASSERT(m_prefsDataOffline);

	PrefsDataOffline *fep = m_prefsDataOffline;
	XFE_GlobalPrefs  *prefs = &fe_globalPrefs;

	// Start up

	XtVaSetValues(fep->online_toggle, 
				  XmNset, (prefs->offline_startup_mode == OFFLINE_STARTUP_ONLINE),
				  0);

	XtVaSetValues(fep->offline_toggle, 
				  XmNset, (prefs->offline_startup_mode == OFFLINE_STARTUP_OFFLINE),
				  0);

	XtVaSetValues(fep->ask_toggle, 
				  XmNset, (prefs->offline_startup_mode == OFFLINE_STARTUP_ASKME),
				  0);

	setInitialized(TRUE);
}

// Member:       install
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageOffline::install()
{
	fe_installOffline();
}

// Member:       save
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageOffline::save()
{
	PrefsDataOffline *fep = m_prefsDataOffline;

	XP_ASSERT(fep);
	if (! verify()) return;

	Boolean b;

    XtVaGetValues(fep->online_toggle, XmNset, &b, 0);
    if (b) fe_globalPrefs.offline_startup_mode = OFFLINE_STARTUP_ONLINE;

    XtVaGetValues(fep->offline_toggle, XmNset, &b, 0);
    if (b) fe_globalPrefs.offline_startup_mode = OFFLINE_STARTUP_OFFLINE;

    XtVaGetValues(fep->ask_toggle, XmNset, &b, 0);
    if (b) fe_globalPrefs.offline_startup_mode = OFFLINE_STARTUP_ASKME;

	// Install preferences

	install();
}

// Member:       getData
// Description:  
// Inputs:
// Side effects: 

PrefsDataOffline *XFE_PrefsPageOffline::getData()
{
	return m_prefsDataOffline;
}

void XFE_PrefsPageOffline::cb_toggleStartup(Widget    widget,
											XtPointer closure,
											XtPointer call_data)
{
	PrefsDataOffline *fep = (PrefsDataOffline *)closure;
	XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)call_data;

	if (! cb->set) {
		XtVaSetValues(widget, XmNset, True, 0);
	}
	else if (widget == fep->online_toggle) {
		XtVaSetValues(fep->offline_toggle, XmNset, False, 0);
		XtVaSetValues(fep->ask_toggle, XmNset, False, 0);
	}
	else if (widget == fep->offline_toggle)	{
		XtVaSetValues(fep->online_toggle, XmNset, False, 0);
		XtVaSetValues(fep->ask_toggle, XmNset, False, 0);
	}
	else if (widget == fep->ask_toggle)	{
		XtVaSetValues(fep->online_toggle, XmNset, False, 0);
		XtVaSetValues(fep->offline_toggle, XmNset, False, 0);
	}
	else
		abort();
}

// ************************************************************************
// *************************    Offline/News      *************************
// ************************************************************************

// Member:       XFE_PrefsPageOfflineNews
// Description:  Constructor
// Inputs:
// Side effects: 

XFE_PrefsPageOfflineNews::XFE_PrefsPageOfflineNews(XFE_PrefsDialog *dialog)
	: XFE_PrefsPage(dialog),
	  m_prefsDataOfflineNews(0)
{
}

// Member:       ~XFE_PrefsPageOfflineNews
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsPageOfflineNews::~XFE_PrefsPageOfflineNews()
{
	PrefsDataOfflineNews         *fep = m_prefsDataOfflineNews;

	if (fep->msg_menu_items) XP_FREE(fep->msg_menu_items);
	if (fep->from_menu_items) XP_FREE(fep->from_menu_items);

	delete m_prefsDataOfflineNews;
}

// Member:       create
// Description:  Creates page for OfflineNews
// Inputs:
// Side effects: 

void XFE_PrefsPageOfflineNews::create()
{
	Widget            kids[100];
	Arg               av[50];
	int               ac;
	int               i;

	PrefsDataOfflineNews *fep = NULL;

	fep = new PrefsDataOfflineNews;
	memset(fep, 0, sizeof(PrefsDataOfflineNews));
	m_prefsDataOfflineNews = fep;

	fep->context = getContext();
	fep->prompt_dialog = getPrefsDialog()->getDialogChrome();

	Widget form;

	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	form = XmCreateForm (m_wPageForm, "offlineNews", av, ac);
	XtManageChild (form);
	m_wPage = fep->page = form;

	// Message Download

	Widget frame1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame1 = XmCreateFrame (form, "downloadFrame", av, ac);

	Widget form1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form1 = XmCreateForm (frame1, "downloadBox", av, ac);

	Widget label1;

	ac = 0;
	XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	label1 = XmCreateLabelGadget (frame1, "downloadBoxLabel", av, ac);

	Widget     download_msgs_label;
	Widget     download_by_date_toggle;
	Widget     download_date_from_toggle;
	Widget     download_date_since_toggle;
	Widget     msg_pulldown;
	Widget     msg_option;
	Widget     from_pulldown;
	Widget     from_option;
	Widget     num_days_text;
	Widget     msgs_label;
	Widget     days_ago_label;

	ac = 0;
	i = 0;

	kids[i++] = download_msgs_label = XmCreateLabelGadget(form1,
														  "downloadMsgs",
														  av, ac);

	kids[i++] = download_by_date_toggle = XmCreateToggleButtonGadget (form1,
																	  "downloadByDate",
																	  av, ac);
	fep->download_by_date_toggle = download_by_date_toggle;

	kids[i++] = download_date_from_toggle = XmCreateToggleButtonGadget (form1,
																		"downloadDateFrom",
																		av, ac);
	fep->download_date_from_toggle = download_date_from_toggle;

	kids[i++] = download_date_since_toggle = XmCreateToggleButtonGadget (form1,
																		 "downloadDateSince",
																		 av, ac);
	fep->download_date_since_toggle = download_date_since_toggle;

	kids[i++] = msgs_label = XmCreateLabelGadget (form1, "msgsLabel", av, ac);
	kids[i++] = days_ago_label = XmCreateLabelGadget (form1, "daysAgoLabel", av, ac);

	kids[i++] = num_days_text = fe_CreateTextField (form1, "numDays", av, ac);
	fep->num_days_text = num_days_text;

	Visual    *v = 0;
	Colormap   cmap = 0;
	Cardinal   depth = 0;
	
	XtVaGetValues (getPrefsDialog()->getPrefsParent(),
				   XtNvisual, &v,
				   XtNcolormap, &cmap,
				   XtNdepth, &depth, 
				   0);

	ac = 0;
	XtSetArg (av[ac], XmNvisual, v); ac++;
	XtSetArg (av[ac], XmNdepth, depth); ac++;
	XtSetArg (av[ac], XmNcolormap, cmap); ac++;
	msg_pulldown = XmCreatePulldownMenu (form1, "msgPullDown", av, ac);
	fep->msg_pulldown = msg_pulldown;

	ac = 0;
	XtSetArg (av [ac], XmNsubMenuId, fep->msg_pulldown); ac++;
	kids[i++] = msg_option = fe_CreateOptionMenu (form1, "msgMenu", av, ac);
	fep->msg_option = msg_option;
	fe_create_download_messages(fep);
	fe_UnmanageChild_safe(XmOptionLabelGadget(fep->msg_option));

	ac = 0;
	XtSetArg (av[ac], XmNvisual, v); ac++;
	XtSetArg (av[ac], XmNdepth, depth); ac++;
	XtSetArg (av[ac], XmNcolormap, cmap); ac++;
	from_pulldown = XmCreatePulldownMenu (form1, "fromPullDown", av, ac);
	fep->from_pulldown = from_pulldown;

	ac = 0;
	XtSetArg (av [ac], XmNsubMenuId, fep->from_pulldown); ac++;
	kids[i++] = from_option = fe_CreateOptionMenu (form1, "fromMenu", av, ac);
	fep->from_option = from_option;
	fe_create_download_from(fep);
	fe_UnmanageChild_safe(XmOptionLabelGadget(fep->from_option));

	int labels_height;
	labels_height = XfeVaGetTallestWidget(msg_option,
										  download_msgs_label,
										  num_days_text,
										  msgs_label,
										  NULL);
	
	XtVaSetValues (download_msgs_label,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_FORM,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (download_by_date_toggle,
				   XmNheight, labels_height,
				   XmNindicatorType, XmN_OF_MANY,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, download_msgs_label,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (msg_option,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, download_msgs_label,
				   XmNleftAttachment, XmATTACH_WIDGET,
				   XmNleftWidget, download_msgs_label,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (msgs_label,
				   XmNalignment, XmALIGNMENT_BEGINNING,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, download_msgs_label,
				   XmNleftAttachment, XmATTACH_WIDGET,
				   XmNleftWidget, msg_option,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	Dimension spacing;
	Dimension toggle_width;
	XtVaGetValues(download_by_date_toggle,
				  XmNindicatorSize, &toggle_width,
				  XmNspacing, &spacing,
				  NULL);

	XtVaSetValues(download_date_from_toggle,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, download_by_date_toggle,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, toggle_width+spacing,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_NONE,
				  0);

	XtVaSetValues(download_date_since_toggle,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget,download_date_from_toggle,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, download_date_from_toggle,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_NONE,
				  0);

	XtVaSetValues(from_option,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget,download_date_from_toggle,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, download_date_from_toggle,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_NONE,
				  0);

	XtVaSetValues(num_days_text,
				  XmNcolumns, 6,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget,download_date_since_toggle,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, download_date_since_toggle,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_NONE,
				  0);

	XtVaSetValues(days_ago_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget,download_date_since_toggle,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, num_days_text,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_NONE,
				  0);

	XtManageChildren (kids, i);
	XtManageChild (label1);
	XtManageChild (form1);
	XtManageChild(frame1);

	Widget frame2;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg (av [ac], XmNtopWidget, frame1); ac++;
	XtSetArg (av [ac], XmNtopOffset, 8); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame2 = XmCreateFrame (form, "frame2", av, ac);

	Widget form2;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form2 = XmCreateForm (frame2, "form2", av, ac);

	Widget discussion_label;
	Widget discussion_label2;
	Widget select_discussion_button;

	ac = 0;
	i = 0;

	kids[i++] = discussion_label = 
		XmCreateLabelGadget(form2, "discussionLabel", av, ac);

	kids[i++] = discussion_label2 = 
		XmCreateLabelGadget(form2, "discussionLabel2", av, ac);

	kids[i++] = select_discussion_button =
		XmCreatePushButtonGadget(form2, "selectDiscussion", av, ac);

	XtVaSetValues(discussion_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_NONE,
				  0);

	Dimension label_width = XfeWidth(discussion_label);
	Dimension button_width = XfeWidth(select_discussion_button);
	int offset = (label_width - button_width) / 2;
	if (offset < 0) offset = 0;

	XtVaSetValues(select_discussion_button,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, discussion_label,
				  XmNtopOffset, 8,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, discussion_label,
				  XmNleftOffset, offset,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_NONE,
				  0);

	label_width = XfeWidth(discussion_label);
	Dimension label_width2 = XfeWidth(discussion_label2);
	offset = (label_width - label_width2) / 2;
	if (offset < 0) offset = 0;

	XtVaSetValues(discussion_label2,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, select_discussion_button,
				  XmNtopOffset, 4,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, discussion_label,
				  XmNleftOffset, offset,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_NONE,
				  0);

	XtManageChildren(kids, i);
	XtManageChild(form2);
	XtManageChild(frame2);

	XtAddCallback(download_date_from_toggle, XmNvalueChangedCallback, cb_toggleDownLoadByDate, fep);
	XtAddCallback(download_date_since_toggle, XmNvalueChangedCallback, cb_toggleDownLoadByDate, fep);

	setCreated(TRUE);
}

// Member:       init
// Description:  Initializes page for OfflineNews
// Inputs:
// Side effects: 

void XFE_PrefsPageOfflineNews::init()
{
	XP_ASSERT(m_prefsDataOfflineNews);

	PrefsDataOfflineNews *fep = m_prefsDataOfflineNews;
	XFE_GlobalPrefs  *prefs = &fe_globalPrefs;

	// Message download

	// messages

	if (prefs->offline_news_download_unread) 
		XtVaSetValues(fep->msg_option,
					  XmNmenuHistory, fep->msg_menu_items[OFFLINE_NEWS_DL_UNREAD_ONLY], 
					  0);
	else 
		XtVaSetValues(fep->msg_option,
					  XmNmenuHistory, fep->msg_menu_items[OFFLINE_NEWS_DL_ALL], 
					  0);

	// download by date

	XtVaSetValues(fep->download_by_date_toggle,
				  XmNset, prefs->offline_news_download_by_date,
				  0);

	XtVaSetValues(fep->download_date_from_toggle,
				  XmNset, ! prefs->offline_news_download_use_days,
				  0);

	XtVaSetValues(fep->download_date_since_toggle,
				  XmNset, prefs->offline_news_download_use_days,
				  0);

	XtVaSetValues(fep->from_option,
				  XmNmenuHistory, fep->from_menu_items[prefs->offline_news_download_inc], 
				  0);

	char buf[256];
	PR_snprintf(buf, sizeof(buf), "%d", prefs->offline_news_download_days);
	XtVaSetValues(fep->num_days_text, XmNvalue, buf, 0);

	setInitialized(TRUE);
}

// Member:       install
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageOfflineNews::install()
{
	fe_installOfflineNews();
}

// Member:       save
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageOfflineNews::save()
{
	PrefsDataOfflineNews *fep = m_prefsDataOfflineNews;

	XP_ASSERT(fep);
	if (! verify()) return;

	Widget  button;
	int     user_data;
	Boolean b;

	XtVaGetValues(fep->msg_option, XmNmenuHistory, &button, 0);
	if (button) {
		XtVaGetValues(button, XmNuserData, &user_data, 0);
		fe_globalPrefs.offline_news_download_unread = (user_data == OFFLINE_NEWS_DL_UNREAD_ONLY);
	}

	XtVaGetValues(fep->download_by_date_toggle, XmNset, &b, 0);
	fe_globalPrefs.offline_news_download_by_date = b;

	XtVaGetValues(fep->download_date_since_toggle, XmNset, &b, 0);
	fe_globalPrefs.offline_news_download_use_days = b;

	XtVaGetValues(fep->from_option, XmNmenuHistory, &button, 0);
	if (button) {
		XtVaGetValues(button, XmNuserData, &user_data, 0);
		fe_globalPrefs.offline_news_download_inc = user_data;
	}

    char *text;
	char  dummy;
    int   size = 0;

    XtVaGetValues(fep->num_days_text, XmNvalue, &text, 0);
    if (1 == sscanf(text, " %d %c", &size, &dummy) &&
		size >= 0)
		fe_globalPrefs.offline_news_download_days = size;
	if (text) XtFree(text);

	// Install preferences

	install();
}

// Member:       getData
// Description:  
// Inputs:
// Side effects: 

PrefsDataOfflineNews *XFE_PrefsPageOfflineNews::getData()
{
	return m_prefsDataOfflineNews;
}

// Member:       cb_toggleDownLoadByDate
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageOfflineNews::cb_toggleDownLoadByDate(Widget    w,
													   XtPointer closure,
													   XtPointer callData)
{
	XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)callData;
	PrefsDataOfflineNews         *fep = (PrefsDataOfflineNews *)closure;

	if (! cb->set) {
		XtVaSetValues(w, XmNset, True, 0);
	}
	else if (w == fep->download_date_from_toggle) {
		XtVaSetValues(fep->download_date_since_toggle, XmNset, False, 0);
	}
	else if (w == fep->download_date_since_toggle) {
		XtVaSetValues(fep->download_date_from_toggle, XmNset, False, 0);
	}
	else
		abort();	
}

static void fe_create_download_messages(PrefsDataOfflineNews *fep)
{
	int              ac;
	int              i;
	int              count;
	Arg              av[50];
	char            *label;
	Widget           button;
	XmString         xms;

	count = XtNumber(downloadMessages); 
	fep->msg_menu_items = (Widget *)XP_CALLOC(count, sizeof(Widget));

	for (i = 0; i < count; i++)	{
		label = XP_GetString(*downloadMessages[i]);

		ac = 0;
		xms = XmStringCreateLtoR(label, XmFONTLIST_DEFAULT_TAG);
		XtSetArg(av[ac], XmNlabelString, xms); ac++;
		button = XmCreatePushButtonGadget(fep->msg_pulldown, label, av, ac);
		fep->msg_menu_items[i] = button;
		XmStringFree(xms);

		XtVaSetValues(button, XmNuserData, i, NULL);
		XtManageChild(button);
	}
}

static void fe_create_download_from(PrefsDataOfflineNews *fep)
{
	int              ac;
	int              i;
	int              count;
	Arg              av[50];
	char            *label;
	Widget           button;
	XmString         xms;

	count = XtNumber(downloadFrom); 
	fep->from_menu_items = (Widget *)XP_CALLOC(count, sizeof(Widget));

	for (i = 0; i < count; i++)	{
		label = XP_GetString(*downloadFrom[i]);

		ac = 0;
		xms = XmStringCreateLtoR(label, XmFONTLIST_DEFAULT_TAG);
		XtSetArg(av[ac], XmNlabelString, xms); ac++;
		button = XmCreatePushButtonGadget(fep->from_pulldown, label, av, ac);
		fep->from_menu_items[i] = button;
		XmStringFree(xms);

		XtVaSetValues(button, XmNuserData, i, NULL);
		XtManageChild(button);
	}
}


