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
   PrefsDialogBrowser.cpp -- the Browser page in XFE preferences dialogs
   Created: Linda Wei <lwei@netscape.com>, 17-Sep-96.
 */

#include "felocale.h"
#include "structs.h"
#include "fonts.h"
#include "xpassert.h"
#include "MozillaApp.h"
#include "View.h"
#include "xfe.h"
#include "e_kit.h"
#include "prefapi.h"
#include "PrefsDialog.h"
#include "PrefsLang.h"
#include "prefapi.h"

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
#include <Xfe/Xfe.h>

extern "C"
{
	char     *XP_GetString(int i);
	void      fe_browse_file_of_text_in_url(MWContext *context, Widget text_field, Boolean dirp);
	void      fe_installBrowser();
	void      fe_installBrowserLang(char *);
}

static char *array_to_acceptLang(char **array, int cnt);
extern int   fe_stringPrefToArray(const char *string, char ***array_p);
extern char *fe_arrayToStringPref(char **array_p, int count);

#define OUTLINER_GEOMETRY_PREF "preferences.lang.outliner_geometry"

extern int XFE_LANG_COL_ORDER;
extern int XFE_LANG_COL_LANG;
extern int XFE_OUT_OF_MEMORY_URL;

// ************************************************************************
// *************************       Browser        *************************
// ************************************************************************

// Member:       XFE_PrefsPageBrowser
// Description:  Constructor
// Inputs:
// Side effects: 

XFE_PrefsPageBrowser::XFE_PrefsPageBrowser(XFE_PrefsDialog *dialog)
	: XFE_PrefsPage(dialog),
	  m_prefsDataBrowser(0),
	  m_home_changed(FALSE)
{
}

// Member:       ~XFE_PrefsPageBrowser
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsPageBrowser::~XFE_PrefsPageBrowser()
{
	delete m_prefsDataBrowser;
}

// Member:       create
// Description:  Creates page for Browser
// Inputs:
// Side effects: 

void XFE_PrefsPageBrowser::create()
{
	Widget            kids[100];
	Arg               av[50];
	int               ac;
	int               i;

	PrefsDataBrowser *fep = NULL;

	fep = new PrefsDataBrowser;
	memset(fep, 0, sizeof(PrefsDataBrowser));
	m_prefsDataBrowser = fep;

	fep->context = getContext();
	fep->prompt_dialog = getPrefsDialog()->getDialogChrome();

	Widget form;

	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	form = XmCreateForm (m_wPageForm, "browser", av, ac);
	XtManageChild (form);
	m_wPage = fep->page = form;

	// Browser starts with

	Widget frame1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame1 = XmCreateFrame (form, "browserFrame", av, ac);

	Widget form1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form1 = XmCreateForm (frame1, "browserBox", av, ac);

	Widget label1;

	ac = 0;
	XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	label1 = XmCreateLabelGadget (frame1, "browserBoxLabel", av, ac);

	// Browser starts with

	ac = 0;
	i = 0;

	kids[i++] = fep->blank_page_toggle =
		XmCreateToggleButtonGadget(form1, "blankPage", av, ac);

	kids[i++] = fep->home_page_toggle =
		XmCreateToggleButtonGadget(form1, "homePage", av, ac);

	kids[i++] = fep->last_page_toggle =
		XmCreateToggleButtonGadget(form1, "lastPage", av, ac);

	XtVaSetValues(fep->blank_page_toggle,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(fep->home_page_toggle,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, fep->blank_page_toggle,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, fep->blank_page_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(fep->last_page_toggle,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, fep->home_page_toggle,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, fep->blank_page_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtManageChildren(kids, i);
	XtManageChild(label1);
	XtManageChild(form1);
	XtManageChild(frame1);

	// Home page location and History expiration

	Widget frame2;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg (av [ac], XmNtopWidget, frame1); ac++;
	XtSetArg (av [ac], XmNtopOffset, 8); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame2 = XmCreateFrame (form, "homePageFrame", av, ac);

	Widget form2;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form2 = XmCreateForm (frame2, "homePageBox", av, ac);

	Widget label2;

	ac = 0;
	XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	label2 = XmCreateLabelGadget (frame2, "homePageBoxLabel", av, ac);

	Widget home_page_label;
	Widget loc_label;
	Widget home_page_text;
	Widget browse_button;
	Widget use_current_button;

	ac = 0;
	i = 0;

	kids[i++] = home_page_label = XmCreateLabelGadget(form2, "homePageLabel", av, ac);

	kids[i++] = loc_label = XmCreateLabelGadget(form2, "locLabel", av, ac);

	kids[i++] = home_page_text = fe_CreateTextField (form2, "homePageText", av, ac);

	kids[i++] = browse_button = XmCreatePushButtonGadget(form2, "browse", av, ac);

	kids[i++] = use_current_button = XmCreatePushButtonGadget(form2, "useCurrent", av, ac);

	fep->home_page_text = home_page_text;
	fep->browse_button = browse_button;
	fep->use_current_button = use_current_button;

	int labels_height;

	labels_height = XfeVaGetTallestWidget(loc_label,
										  home_page_text,
										  browse_button,
										  NULL);

	XtVaSetValues(home_page_label,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(loc_label,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, home_page_label,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(home_page_text,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, loc_label,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, loc_label,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(browse_button,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, loc_label,
				  XmNleftAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(use_current_button,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, browse_button,
				  XmNleftAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_WIDGET,
				  XmNrightWidget, browse_button,
				  XmNrightOffset, 8,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtManageChildren (kids, i);
	XtManageChild(label2);
	XtManageChild(form2);
	XtManageChild(frame2);

	// History

	Widget frame3;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg (av [ac], XmNtopWidget, frame2); ac++;
	XtSetArg (av [ac], XmNtopOffset, 8); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame3 = XmCreateFrame (form, "historyFrame", av, ac);

	Widget form3;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form3 = XmCreateForm (frame3, "historyBox", av, ac);

	Widget label3;

	ac = 0;
	XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	label3 = XmCreateLabelGadget (frame3, "historyBoxLabel", av, ac);

	Widget expire_label;
	Widget expire_days_text;
	Widget days_label;
	Widget expire_now_button;

	ac = 0;
	i = 0;

	kids[i++] = expire_label = XmCreateLabelGadget(form3, "expireLabel", av, ac);

	kids[i++] = days_label = XmCreateLabelGadget(form3, "daysLabel", av, ac);

	kids[i++] = expire_days_text = fe_CreateTextField (form3, "expireDays", av, ac);

	kids[i++] = expire_now_button = XmCreatePushButtonGadget(form3, "expireNow", av, ac);

	fep->expire_days_text = expire_days_text;

	XtVaSetValues(expire_label,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(expire_days_text,
				  XmNcolumns, 3,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, expire_label,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, expire_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(days_label,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, expire_label,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, expire_days_text,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(expire_now_button,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, expire_label,
				  XmNleftAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtManageChildren (kids, i);
	XtManageChild(label3);
	XtManageChild(form3);
	XtManageChild(frame3);

	XtAddCallback(fep->blank_page_toggle, XmNvalueChangedCallback,
				  cb_setStartupPage, fep);
	XtAddCallback(fep->home_page_toggle, XmNvalueChangedCallback,
				  cb_setStartupPage, fep);
	XtAddCallback(fep->last_page_toggle, XmNvalueChangedCallback,
				  cb_setStartupPage, fep);

	XtAddCallback(browse_button, XmNactivateCallback,
				  cb_browse, this);
	XtAddCallback(use_current_button, XmNactivateCallback,
				  cb_useCurrent, this);
	XtAddCallback(expire_now_button, XmNactivateCallback,
				  cb_expireNow, this);

	setCreated(TRUE);
}

// Member:       init
// Description:  Initializes page for Browser
// Inputs:
// Side effects: 

void XFE_PrefsPageBrowser::init()
{
	XP_ASSERT(m_prefsDataBrowser);
	
	PrefsDataBrowser   *fep = m_prefsDataBrowser;
	XFE_GlobalPrefs    *prefs = &fe_globalPrefs;
    Boolean            sensitive;
	
	// Browser starts with
    sensitive = !PREF_PrefIsLocked("browser.startup.page");
	XtVaSetValues(fep->blank_page_toggle, 
				  XmNset, (prefs->browser_startup_page == BROWSER_STARTUP_BLANK),
                  XmNsensitive, sensitive,
				  0);

	XtVaSetValues(fep->home_page_toggle, 
				  XmNset, (prefs->browser_startup_page == BROWSER_STARTUP_HOME),
                  XmNsensitive, sensitive,
				  0);

	XtVaSetValues(fep->last_page_toggle,
				  XmNset, (prefs->browser_startup_page == BROWSER_STARTUP_LAST),
                  XmNsensitive, sensitive,
				  0);

    sensitive = !PREF_PrefIsLocked("browser.startup.homepage");
	XtVaSetValues(fep->home_page_text, XmNsensitive, sensitive, 0);
	fe_SetTextField(fep->home_page_text, prefs->home_document);
    XtSetSensitive(fep->use_current_button, sensitive);
    XtSetSensitive(fep->browse_button, sensitive);

	// History expires

	char buf [1024];
	if (prefs->global_history_expiration >= 0)
		PR_snprintf(buf, sizeof(buf), "%d", prefs->global_history_expiration);
	else
		*buf = 0;

    sensitive = !PREF_PrefIsLocked("browser.link_expiration");
	XtVaSetValues(fep->expire_days_text,
				  XmNvalue, buf,
                  XmNsensitive, sensitive,
				  0);

	setInitialized(TRUE);
}

// Member:       install
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageBrowser::install()
{
	fe_installBrowser();

	if (m_home_changed) {
		// Notify whoever is interested in updating home button/menu item
		XFE_MozillaApp::theApp()->notifyInterested(XFE_View::commandNeedsUpdating,
												   (void*)xfeCmdHome);
		m_home_changed = FALSE;
	}
}

// Member:       save
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageBrowser::save()
{
	PrefsDataBrowser *fep = m_prefsDataBrowser;

	XP_ASSERT(fep);

    char *old_home = fe_globalPrefs.home_document;
	Boolean b;

    XtVaGetValues(fep->blank_page_toggle, XmNset, &b, 0);
    if (b) fe_globalPrefs.browser_startup_page = BROWSER_STARTUP_BLANK;

    XtVaGetValues(fep->home_page_toggle, XmNset, &b, 0);
    if (b) fe_globalPrefs.browser_startup_page = BROWSER_STARTUP_HOME;

    XtVaGetValues(fep->last_page_toggle, XmNset, &b, 0);
    if (b) fe_globalPrefs.browser_startup_page = BROWSER_STARTUP_LAST;

	char *s = fe_GetTextField(fep->home_page_text);
    fe_globalPrefs.home_document = s ? s : XP_STRDUP("");

    // Send out notification to change sensitivity of the home button/menu item

	if ((old_home == 0) ||
		(*old_home == '\0')){
		if (XP_STRLEN(fe_globalPrefs.home_document) > 0) {
			m_home_changed = TRUE;
		}
	} 
	else {
		if (XP_STRLEN(fe_globalPrefs.home_document) == 0) {
			m_home_changed = TRUE;
		}
	}

	if (old_home) XP_FREE(old_home);

	// History expires

	int old_history_expiration = fe_globalPrefs.global_history_expiration;

	fe_globalPrefs.global_history_expiration = -1;

	char *text = 0;
	char  dummy;
	XtVaGetValues(fep->expire_days_text, XmNvalue, &text, 0);
	sscanf(text, " %d %c", &fe_globalPrefs.global_history_expiration, &dummy);
	if (fe_globalPrefs.global_history_expiration < 0)
		fe_globalPrefs.global_history_expiration = -1;
	if (text) XtFree(text);

	if (old_history_expiration != fe_globalPrefs.global_history_expiration) {
		fe_RefreshAllAnchors();
	}

	// Install preferences

	install();
}

// Member:       getData
// Description:  
// Inputs:
// Side effects: 

PrefsDataBrowser *XFE_PrefsPageBrowser::getData()
{
	return m_prefsDataBrowser;
}

// Member:       cb_setStartupPage
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageBrowser::cb_setStartupPage(Widget    w,
											 XtPointer closure,
											 XtPointer callData)
{
	PrefsDataBrowser             *fep = (PrefsDataBrowser *)closure;
	XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)callData;

	if (! cb->set) {
		XtVaSetValues(w, XmNset, True, 0);
	}
	else if (w == fep->blank_page_toggle){
		XtVaSetValues(fep->home_page_toggle, XmNset, False, 0);
		XtVaSetValues(fep->last_page_toggle, XmNset, False, 0);
    }
	else if (w == fep->home_page_toggle){
		XtVaSetValues(fep->blank_page_toggle, XmNset, False, 0);
		XtVaSetValues(fep->last_page_toggle, XmNset, False, 0);
    }
	else if (w == fep->last_page_toggle){
		XtVaSetValues(fep->blank_page_toggle, XmNset, False, 0);
		XtVaSetValues(fep->home_page_toggle, XmNset, False, 0);
    }
	else
		abort();
}

// Member:       cb_browse
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageBrowser::cb_browse(Widget    /* w */,
									 XtPointer closure,
									 XtPointer /* callData */)
{
	XFE_PrefsPageBrowser *thePage = (XFE_PrefsPageBrowser *)closure;
	XFE_PrefsDialog      *theDialog = thePage->getPrefsDialog();
	PrefsDataBrowser     *fep = thePage->getData();

	fe_browse_file_of_text_in_url(theDialog->getContext(), fep->home_page_text, False);
}

// Member:       cb_useCurrent
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageBrowser::cb_useCurrent(Widget    /* w */,
										 XtPointer closure,
										 XtPointer /* callData */)
{
	XFE_PrefsPageBrowser *thePage = (XFE_PrefsPageBrowser *)closure;
	XFE_PrefsDialog      *theDialog = thePage->getPrefsDialog();
	PrefsDataBrowser     *fep = thePage->getData();
	MWContext            *context = theDialog->getContext();
	History_entry        *h = SHIST_GetCurrent (&context->hist);

	if (!h) return;
      
	if ((h->address) && 
		(XP_STRLEN(h->address) > 0))
		fe_SetTextField(fep->home_page_text, h->address);
}

// Member:       cb_expireNow
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageBrowser::cb_expireNow(Widget    /* widget */,
										XtPointer closure,
										XtPointer /* call_data */)
{
	XFE_PrefsPageBrowser *thePage = (XFE_PrefsPageBrowser *)closure;
	PrefsDataBrowser     *fep = thePage->getData();

	if (XFE_Confirm(fep->context, fe_globalData.expire_now_message)) {
		GH_ClearGlobalHistory();
		fe_RefreshAllAnchors();
	}
}

// ************************************************************************
// *************************  Browser/Languages   *************************
// ************************************************************************

// Member:       XFE_PrefsPageBrowserLang
// Description:  Constructor
// Inputs:
// Side effects: 

XFE_PrefsPageBrowserLang::XFE_PrefsPageBrowserLang(XFE_PrefsDialog *dialog)
	: XFE_PrefsPage(dialog),
	  m_prefsDataBrowserLang(0),
	  m_rowIndex(0)
{
}

// Member:       ~XFE_PrefsPageBrowserLang
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsPageBrowserLang::~XFE_PrefsPageBrowserLang()
{
	int                     i;
	PrefsDataBrowserLang   *fep = m_prefsDataBrowserLang;

	if (fep) {
		delete fep->lang_outliner;

		if (fep->pref_lang_regs) {
			for (i = 0; i < fep->pref_lang_count; i++) {
				XP_FREE(fep->pref_lang_regs[i]);
			}
			XP_FREE(m_prefsDataBrowserLang->pref_lang_regs);
		}
	}
	delete m_prefsDataBrowserLang;
}

const int XFE_PrefsPageBrowserLang::OUTLINER_COLUMN_ORDER = 0;
const int XFE_PrefsPageBrowserLang::OUTLINER_COLUMN_LANG = 1;

const int XFE_PrefsPageBrowserLang::OUTLINER_COLUMN_MAX_LENGTH = 256;
const int XFE_PrefsPageBrowserLang::OUTLINER_INIT_POS = (-1);

#define STRING_COL_ORDER "Order"
#define STRING_COL_LANG  "Language"

// Member:       create
// Description:  Creates page for BrowserLang
// Inputs:
// Side effects: 

void XFE_PrefsPageBrowserLang::create()
{
	Widget            kids[100];
	Arg               av[50];
	int               ac;
	int               i;

	PrefsDataBrowserLang *fep = NULL;

	fep = new PrefsDataBrowserLang;
	memset(fep, 0, sizeof(PrefsDataBrowserLang));
	m_prefsDataBrowserLang = fep;

	fep->context = getContext();
	fep->prompt_dialog = getPrefsDialog()->getDialogChrome();

	Widget form;

	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	form = XmCreateForm (m_wPageForm, "lang", av, ac);
	XtManageChild (form);
	m_wPage = fep->page = form;

	Widget frame1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame1 = XmCreateFrame (form, "langFrame", av, ac);

	Widget form1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form1 = XmCreateForm (frame1, "langBox", av, ac);

	Widget lang_label;
	Widget up_button;
	Widget down_button;
	Widget add_button;
	Widget delete_button;

	ac = 0;
	i = 0;
	
	kids[i++] = lang_label = 
		XmCreateLabelGadget(form1, "langLabel", av, ac);

	kids[i++] = up_button = 
		XmCreateArrowButtonGadget(form1, "upButton", av, ac);

	kids[i++] = down_button = 
		XmCreateArrowButtonGadget(form1, "downButton", av, ac);

	kids[i++] = add_button =
		XmCreatePushButtonGadget(form1, "addButton", av, ac);

	kids[i++] = delete_button =
		XmCreatePushButtonGadget(form1, "deleteButton", av, ac);

	fep->add_button = add_button;
	fep->delete_button = delete_button;
	fep->up_button = up_button;
	fep->down_button = down_button;

	// Outliner

	int           num_columns = 2;
	static int    default_column_widths[] = {8, 40};
	XFE_Outliner *outliner;
	Widget        lang_list;

	outliner = new XFE_Outliner("langList",            // name
								this,                  // outlinable
								getPrefsDialog(),      // top level								  
								form1,                 // parent
								FALSE,                 // constant size
								TRUE,                  // has headings
								num_columns,           // number of columns
								num_columns,           // number of visible columns
								default_column_widths, // default column widths
								OUTLINER_GEOMETRY_PREF
								);

	fep->lang_outliner = outliner;
	lang_list = fep->lang_list = outliner->getBaseWidget();

	XtVaSetValues(outliner->getBaseWidget(),
				  XtVaTypedArg, XmNblankBackground, XmRString, "white", 6,
				  XmNvisibleRows, 10,
				  NULL);
	XtVaSetValues(outliner->getBaseWidget(),
				  XmNcellDefaults, True,
				  XtVaTypedArg, XmNcellBackground, XmRString, "white", 6,
				  NULL);

	outliner->setColumnResizable(OUTLINER_COLUMN_ORDER, False);
	outliner->setColumnResizable(OUTLINER_COLUMN_LANG, False);
	outliner->show();
	
	XtVaSetValues(lang_label,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(fep->lang_list,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, lang_label,
				  XmNtopOffset, 4,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	Dimension list_height;
	Dimension up_button_height;
	int       down_button_top_offset;
	int       up_button_top_offset;

	XtVaGetValues(down_button,
				  XmNtopOffset, &down_button_top_offset,
				  NULL);

	list_height = XfeHeight(lang_list);
	up_button_height = XfeHeight(up_button);
	up_button_top_offset = list_height/2 - up_button_height - down_button_top_offset/2;

	XtVaSetValues(up_button,
				  XmNarrowDirection, XmARROW_UP,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, lang_list,
				  XmNtopOffset, up_button_top_offset,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, lang_list,
				  XmNleftOffset, 8,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
				  
	XtVaSetValues(down_button,
				  XmNarrowDirection, XmARROW_DOWN,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, up_button,
				  XmNtopOffset, 4,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, up_button,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	int labels_width;

	labels_width = XfeVaGetWidestWidget(add_button,
										delete_button,
										NULL);

	XtVaSetValues(add_button,
				  XmNwidth, labels_width,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, lang_list,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, up_button,
				  XmNleftOffset, 24,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(delete_button,
				  XmNwidth, labels_width,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, add_button,
				  XmNtopOffset, 4,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, add_button,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtAddCallback(add_button, XmNactivateCallback, cb_add, this);
	XtAddCallback(delete_button, XmNactivateCallback, cb_delete, this);
	XtAddCallback(up_button, XmNactivateCallback, cb_promote, this);
	XtAddCallback(down_button, XmNactivateCallback, cb_demote, this);

	XtManageChildren(kids, i);
	XtManageChild(form1);
	XtManageChild(frame1);

	setCreated(TRUE);
}

// Member:       init
// Description:  Initializes page for BrowserLang
// Inputs:
// Side effects: 

void XFE_PrefsPageBrowserLang::init()
{
	XP_ASSERT(m_prefsDataBrowserLang);
	PrefsDataBrowserLang   *fep = m_prefsDataBrowserLang;
	XFE_GlobalPrefs        *prefs = &fe_globalPrefs;

	// Retrieve the preferred lang/region list from preferences

	if (fep->pref_lang_regs) {
		for (int i = 0; i < fep->pref_lang_count; i++) {
			XP_FREE(fep->pref_lang_regs[i]);
		}
		XP_FREE(m_prefsDataBrowserLang->pref_lang_regs);
		fep->pref_lang_regs = 0;
		fep->pref_lang_count = 0;
	}

	fep->pref_lang_count = fe_stringPrefToArray(prefs->lang_regions, &fep->pref_lang_regs);
	fep->lang_outliner->change(0, fep->pref_lang_count, fep->pref_lang_count);
	setSelectionPos(OUTLINER_INIT_POS);

	setInitialized(TRUE);
}

// Member:       install
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageBrowserLang::install()
{
	XP_ASSERT(m_prefsDataBrowserLang);
	PrefsDataBrowserLang   *fep = m_prefsDataBrowserLang;
	char                   *new_accept_lang;

	if (fep->pref_lang_count > 0) {
		new_accept_lang = array_to_acceptLang(fep->pref_lang_regs, fep->pref_lang_count);
	}
	else {
		new_accept_lang = NULL;
	}

	fe_installBrowserLang(new_accept_lang);
}

// Member:       save
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageBrowserLang::save()
{
	PrefsDataBrowserLang *fep = m_prefsDataBrowserLang;

	XP_ASSERT(fep);

	char *new_regions = fe_arrayToStringPref(fep->pref_lang_regs, fep->pref_lang_count);
	XP_ASSERT(new_regions);

	XP_FREE(fe_globalPrefs.lang_regions);
	fe_globalPrefs.lang_regions = new_regions;

	// Install preferences

	install();
}

// Member:       insertLang
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageBrowserLang::insertLang(char *lang)
{
	// First figure out where to insert it

	PrefsDataBrowserLang *fep = m_prefsDataBrowserLang;
	int                   pos = 0;
	int                   count = fep->pref_lang_count;
	
	if (count > 0) {
		// Check if there is any selection
		uint32     sel_count = 0;
		const int *indices = 0;
		fep->lang_outliner->getSelection(&indices, (int *) &sel_count);
		if (sel_count > 0 && indices) {
			pos = indices[0];
		}
	}

	// Insert at pos

	insertLangAtPos(pos, lang);
}

// Member:       insertLangAtPos
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageBrowserLang::insertLangAtPos(int pos, char *lang)
{
	PrefsDataBrowserLang *fep = m_prefsDataBrowserLang;
	int                   count = fep->pref_lang_count;
	int                   i;
	
	// Insert language at position

	if (count > 0) {
		for (i = 0; i < count; i++) {
			if (XP_STRCMP(lang, fep->pref_lang_regs[i]) == 0)
				return;
		}

		char **pref_lang_regs = 0;
		pref_lang_regs = (char **)XP_CALLOC(count+1, sizeof(char *));

		for (i = 0; i < pos; i++) {
			pref_lang_regs[i] = fep->pref_lang_regs[i];
		}
		for (i = pos; i < count; i++) {
			pref_lang_regs[i+1] = fep->pref_lang_regs[i];
		}
		pref_lang_regs[pos] = XP_STRDUP(lang);

		XP_FREE(m_prefsDataBrowserLang->pref_lang_regs);		
		fep->pref_lang_regs = pref_lang_regs;
		fep->pref_lang_count++;
	}
	else {
		fep->pref_lang_regs = (char **)XP_CALLOC(1, sizeof(char *));
		fep->pref_lang_regs[0] = XP_STRDUP(lang);
		fep->pref_lang_count = 1;
	}

	// Repaint 

	fep->lang_outliner->change(0, fep->pref_lang_count, fep->pref_lang_count);

	// Set selection

	setSelectionPos(pos);
}

// Member:       deleteLangAtPos
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageBrowserLang::deleteLangAtPos(int pos)
{
	PrefsDataBrowserLang *fep = m_prefsDataBrowserLang;
	int                   count = fep->pref_lang_count;
	int                   i;

	if (pos >= count) return;

	// Remove the lang at pos

	if (count == 1) {
		fep->pref_lang_count = 0;
		XP_FREE(fep->pref_lang_regs[0]);
		XP_FREE(m_prefsDataBrowserLang->pref_lang_regs);
		m_prefsDataBrowserLang->pref_lang_regs = 0;
	}
	else {
		char **pref_lang_regs = 0;
		pref_lang_regs = (char **)XP_CALLOC(count-1, sizeof(char *));
		for (i = 0; i < pos; i++) {
			pref_lang_regs[i] = fep->pref_lang_regs[i];
		}
		for (i = pos; i < count - 1; i++) {
			pref_lang_regs[i] = fep->pref_lang_regs[i+1];
		}
		XP_FREE(m_prefsDataBrowserLang->pref_lang_regs[pos]);		
		XP_FREE(m_prefsDataBrowserLang->pref_lang_regs);		
		m_prefsDataBrowserLang->pref_lang_regs = pref_lang_regs;
		fep->pref_lang_count--;
	}

	// Repaint

	fep->lang_outliner->change(0, fep->pref_lang_count, fep->pref_lang_count);

	// Set selection if there is more than one entry 

	if (fep->pref_lang_count > 0) {
		if (pos >= fep->pref_lang_count) pos--;
		setSelectionPos(pos);
	}
	else {
		setSelectionPos(OUTLINER_INIT_POS);
	}
}

// Member:       swapLangs
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageBrowserLang::swapLangs(int pos1, int pos2, int sel_pos)
{
	PrefsDataBrowserLang *fep = m_prefsDataBrowserLang;
	int                   count = fep->pref_lang_count;
	char                 *temp;

	if ((pos1 >= count) ||
		(pos2 >= count) ||
		(sel_pos >= count))
		return;

	// Swap

	temp = m_prefsDataBrowserLang->pref_lang_regs[pos1];
	m_prefsDataBrowserLang->pref_lang_regs[pos1] = m_prefsDataBrowserLang->pref_lang_regs[pos2];
	m_prefsDataBrowserLang->pref_lang_regs[pos2] = temp;

	// Repaint

	fep->lang_outliner->change(0, fep->pref_lang_count, fep->pref_lang_count);

	// Set selection if there is more than one entry 

	setSelectionPos(sel_pos);
}

// Member:       setSelelctionPos
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageBrowserLang::setSelectionPos(int pos)
{
	PrefsDataBrowserLang *fep = m_prefsDataBrowserLang;
	int                   count = fep->pref_lang_count;

	if (pos >= count) return;

	if (pos == OUTLINER_INIT_POS) {
		XtVaSetValues(fep->delete_button, XmNsensitive, False, NULL);
		XtVaSetValues(fep->up_button, XmNsensitive, False, NULL);
		XtVaSetValues(fep->down_button, XmNsensitive, False, NULL);
	}
	else {
		fep->lang_outliner->selectItemExclusive(pos);
		XtVaSetValues(fep->delete_button, XmNsensitive, (count != 0), NULL);
		XtVaSetValues(fep->up_button, XmNsensitive, (pos != 0), NULL);
		XtVaSetValues(fep->down_button, XmNsensitive, (pos != (count-1)), NULL);
	}
}

// Member:       deselelctionPos
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageBrowserLang::deselectPos(int pos)
{
	PrefsDataBrowserLang *fep = m_prefsDataBrowserLang;
	int                   count = fep->pref_lang_count;

	if (pos >= count) return;

	fep->lang_outliner->deselectItem(pos);
}

// Member:       getData
// Description:  
// Inputs:
// Side effects: 

PrefsDataBrowserLang *XFE_PrefsPageBrowserLang::getData()
{
	return m_prefsDataBrowserLang;
}

// Member:       cb_add
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageBrowserLang::cb_add(Widget    /* widget */,
									  XtPointer closure,
									  XtPointer /* call_data */)
{
	XFE_PrefsPageBrowserLang *thePage = (XFE_PrefsPageBrowserLang *)closure;
	XFE_PrefsDialog          *theDialog = thePage->getPrefsDialog();
	Widget                    mainw = theDialog->getBaseWidget();

	// Instantiate a language dialog

	XFE_PrefsLangDialog *theLangDialog = 0;

	if ((theLangDialog =
		 new XFE_PrefsLangDialog(theDialog, thePage, mainw, "prefsLang")) == 0) {
	    fe_perror(thePage->getContext(), XP_GetString(XFE_OUT_OF_MEMORY_URL));
	    return;
	}

	// Initialize and pop up the lang dialog

	theLangDialog->initPage();
	theLangDialog->show();
}

// Member:       cb_delete
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageBrowserLang::cb_delete(Widget    /* widget */,
										 XtPointer closure,
										 XtPointer /* call_data */)
{
	XFE_PrefsPageBrowserLang *thePage = (XFE_PrefsPageBrowserLang *)closure;
	PrefsDataBrowserLang     *fep = thePage->getData();
	int                       count = fep->pref_lang_count;
	int                       pos = 0;
	uint32                    sel_count = 0;
	const int                *indices = 0;

	if (count == 0) return;

	fep->lang_outliner->getSelection(&indices, (int *) &sel_count);
	if (sel_count > 0 && indices) {
		pos = indices[0];
		thePage->deleteLangAtPos(pos);
	}
}

// Member:       cb_promote
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageBrowserLang::cb_promote(Widget    /* widget */,
										  XtPointer closure,
										  XtPointer /* call_data */)
{
	XFE_PrefsPageBrowserLang *thePage = (XFE_PrefsPageBrowserLang *)closure;
	PrefsDataBrowserLang     *fep = thePage->getData();
	int                       count = fep->pref_lang_count;
	int                       pos = 0;
	uint32                    sel_count = 0;
	const int                *indices = 0;

	if (count == 0) return;

	fep->lang_outliner->getSelection(&indices, (int *) &sel_count);
	if (sel_count > 0 && indices) {
		pos = indices[0];
		if (pos != 0) {
			thePage->deselectPos(pos);
			thePage->swapLangs(pos, pos-1, pos-1);
		}
	}
}

// Member:       cb_demote
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageBrowserLang::cb_demote(Widget    /* widget */,
										 XtPointer closure,
										 XtPointer /* call_data */)
{
	XFE_PrefsPageBrowserLang *thePage = (XFE_PrefsPageBrowserLang *)closure;
	PrefsDataBrowserLang     *fep = thePage->getData();
	int                       count = fep->pref_lang_count;
	int                       pos = 0;
	uint32                    sel_count = 0;
	const int                *indices = 0;

	if (count == 0) return;

	fep->lang_outliner->getSelection(&indices, (int *) &sel_count);
	if (sel_count > 0 && indices) {
		pos = indices[0];
		if (pos != (count-1)) {
			thePage->deselectPos(pos);
			thePage->swapLangs(pos, pos+1, pos+1);
		}
	}
}

/* Outlinable interface methods */

void *XFE_PrefsPageBrowserLang::ConvFromIndex(int /* index */)
{
	return (void *)NULL;
}

int XFE_PrefsPageBrowserLang::ConvToIndex(void * /* item */)
{
	return 0;
}

char*XFE_PrefsPageBrowserLang::getColumnName(int column)
{
	switch (column){
	case OUTLINER_COLUMN_ORDER:
		return STRING_COL_ORDER;
	case OUTLINER_COLUMN_LANG:
		return STRING_COL_LANG;
    default:
		XP_ASSERT(0); 
		return 0;
    }
}

char *XFE_PrefsPageBrowserLang::getColumnHeaderText(int column)
{
  switch (column) 
    {
    case OUTLINER_COLUMN_ORDER:
      return XP_GetString(XFE_LANG_COL_ORDER);
    case OUTLINER_COLUMN_LANG:
      return XP_GetString(XFE_LANG_COL_LANG);
    default:
      XP_ASSERT(0);
      return 0;
    }
}

fe_icon *XFE_PrefsPageBrowserLang::getColumnHeaderIcon(int /* column */)
{
	return 0;

}

EOutlinerTextStyle XFE_PrefsPageBrowserLang::getColumnHeaderStyle(int /* column */)
{
	return OUTLINER_Default;
}

// This method acquires one line of data.
void *XFE_PrefsPageBrowserLang::acquireLineData(int line)
{
	m_rowIndex = line;
	return (void *)1;
}

void XFE_PrefsPageBrowserLang::getTreeInfo(Boolean * /* expandable */,
										   Boolean * /* is_expanded */,
										   int     * /* depth */,
										   OutlinerAncestorInfo ** /* ancestor */)
{
	// No-op
}


EOutlinerTextStyle XFE_PrefsPageBrowserLang::getColumnStyle(int /*column*/)
{
    return OUTLINER_Default;
}

char *XFE_PrefsPageBrowserLang::getColumnText(int column)
{ 
	PrefsDataBrowserLang *fep = m_prefsDataBrowserLang;
	static char           line[OUTLINER_COLUMN_MAX_LENGTH+1];

	*line = 0;

	// if (m_rowIndex >= fep->pref_lang_count) return line;

	switch (column) {
    case OUTLINER_COLUMN_ORDER:
		sprintf(line, "%d", m_rowIndex+1); 
		break;

    case OUTLINER_COLUMN_LANG:
		sprintf(line, "%s", fep->pref_lang_regs[m_rowIndex]);
		break;

    default:
		break;
    }

	return line;
}

fe_icon *XFE_PrefsPageBrowserLang::getColumnIcon(int /* column */)
{
	return 0;
}

void XFE_PrefsPageBrowserLang::releaseLineData()
{
	// No-op
}

void XFE_PrefsPageBrowserLang::Buttonfunc(const OutlineButtonFuncData *data)
{
	int row = data->row;

	if (row < 0) {
		// header
		return;
	} 

	setSelectionPos(data->row);
}

void XFE_PrefsPageBrowserLang::Flippyfunc(const OutlineFlippyFuncData * /* data */)
{
	// No-op
}

XFE_Outliner *XFE_PrefsPageBrowserLang::getOutliner()
{
	return m_prefsDataBrowserLang->lang_outliner;
}

// ************************************************************************
// *************************         Misc         *************************
// ************************************************************************

/*
 * Get the tag part from a string
 * The tag is bracketed by '[' and ']'
 * and has no white space.
 *
 * Returns a pointer to the start of the tag.
 * also returns the tag length.
 *
 * Any error return NULL
 */
static char *
fe_lang_reg_get_tag(char *tag, int *len_p)
{
	char *q1, *q2;

  /* init the returned tag length */
	*len_p = 0;

	/* bstell: need to fix this to use Istrchr */
	/* use INTL_DefaultWinCharSetID to get the csid */
	/* (really need to store the csid of the list in preferences) */
	/* tag must start with '[' */
	q1 = strchr(tag, '[');
	if (q1 == NULL)
		return NULL;
	q1 += 1; /* point past the '[' */

	/* tag must end with ']' */
	q2 = strchr(q1, ']');
	if (q2 == NULL)
		return NULL;

  /* check length of tag (stop on any white-space) */
	*len_p = strcspn(q1, "] \t\n");
	if (*len_p < 1) /* need some text */
		return NULL;

	if (*len_p != (q2-q1)) {
		*len_p = 0;
		return NULL;
	}

	return q1;
}

static const char *accept_lang_seperator = ", ";
/*
 * Convert an array of strings
 * to HTTP Accept-Language string
 *
 *  returns a string built up from the array
 *
 * The caller must free the array
 */
static char *array_to_acceptLang(char **array, int cnt)
{
	char *al_string, *p, *q;
	int i, str_len, tag_len;

	/*
	 * Handle an empty array
	 */
	if (cnt == 0) {
		return NULL;
	}

	/*
   * Get the maximum possible length of the result
   */
	str_len = 0;
	for (i=0; i<cnt; i++) {
		if (i != 0)
			str_len += strlen(accept_lang_seperator);
		str_len += strlen(array[i]);
	}
	str_len += 1; /* space for the terminator */
	al_string = (char *)XP_ALLOC(str_len);

	/*
   * build up the tag from the parts
   */
	for (p=al_string, i=0; i<cnt; i++) {
		/*
		 * Validity check this string
		 */
		q = fe_lang_reg_get_tag(array[i], &tag_len);
		if (q == NULL)
			continue;

		/*
		 * Add this tag part to the tag
		 */
		if (i != 0) {
			strcpy(p, accept_lang_seperator);
			p += strlen(accept_lang_seperator);
		}
		strncpy(p, q, tag_len);
		p += tag_len;
		*p = '\0';
	}

	return al_string;
}

