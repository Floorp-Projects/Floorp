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
   PrefsDialogGeneral.cpp -- the General page in XFE preferences dialogs
   Created: Linda Wei <lwei@netscape.com>, 17-Sep-96.
 */

#include "Frame.h"
#include "felocale.h"
#include "structs.h"
#include "fonts.h"
#include "xpassert.h"
#include "MozillaApp.h"
#include "View.h"
#include "prprf.h"
#include "net.h"
#include "libmocha.h"
#ifdef JAVA
#include "java.h"
#endif
#include "PrefsDialogAppl.h"
#include "PrefsProxiesView.h"
#include "PrefsApplEdit.h"
#include "Netcaster.h"

#include "PrefsDialog.h"
#include "ColorDialog.h"
#include "ViewGlue.h"
#include "e_kit.h"
#include "prefapi.h"
#include "nf.h"

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
#include <Xm/DrawnB.h>
#include <DtWidgets/ComboBox.h>
#include <Xfe/Xfe.h>
#include "Netcaster.h"

#define DEFAULT_COLUMN_WIDTH 	35
#define MAX_COLUMN_WIDTH 	    100

#define DEF_COLOR_BUTTON_WIDTH  40
#define DEF_COLOR_BUTTON_HEIGHT 20

extern int XFE_HELPERS_LIST_HEADER;
extern int XFE_PRIVATE_MIMETYPE_RELOAD;
extern int XFE_PRIVATE_MAILCAP_RELOAD;
extern int XFE_PRIVATE_RELOADED_MIMETYPE;
extern int XFE_PRIVATE_RELOADED_MAILCAP;
extern int XFE_HELPERS_NETSCAPE;
extern int XFE_HELPERS_UNKNOWN;
extern int XFE_HELPERS_SAVE;
extern int XFE_HELPERS_PLUGIN;
extern int XFE_OUT_OF_MEMORY_URL;
extern int XFE_HELPERS_CANNOT_DEL_STATIC_APPS;
extern int XFE_PREFS_RESTART_FOR_FONT_CHANGES;

extern "C"
{
	void      GH_ClearGlobalHistory(void);
	void      NET_CleanupFileFormat(char *filename);
	void      NET_CleanupMailCapList(char* filename);
	void      NET_ReloadProxyConfig(MWContext *window_id);
	void      NET_SelectProxyStyle(NET_ProxyStyle style);
	void      NET_SetCacheUseMethod(CacheUseEnum method);
	void      NET_SetDiskCacheSize(int32 new_size);
	void      NET_SetMemoryCacheSize(int32 new_size);
	void      NET_SetProxyServer(NET_ProxyType type, const char *org_host_port);
	char     *XP_GetString(int i);
	void      fe_browse_file_of_text(MWContext *context, Widget text_field, Boolean dirp);
	void      fe_browse_file_of_text_in_url(MWContext *context, Widget text_field, Boolean dirp);
	void      fe_installGeneralAppearance();
	void      fe_installGeneral();
	void      fe_installGeneralFonts();
	void      fe_installGeneralColors();
	void      fe_installGeneralAdvanced();
	void      fe_installGeneralAppl();
	void      fe_installGeneralCache();
	void      fe_installGeneralProxies();
	void      fe_installBrowserAppl();
	void      fe_installDiskSpace();
	void      fe_SwatchSetColor(Widget widget, LO_Color* color);
	XP_Bool   fe_IsCalendarInstalled();
	XP_Bool   fe_IsConferenceInstalled();

	static void fe_InvalidateFontData(void);
	static void prefsGeneralFontsCb_fontFamily(Widget, XtPointer, XtPointer);
	static void prefsGeneralFontsCb_fontSize(Widget, XtPointer, XtPointer);
	static void fe_set_new_font_sizes(PrefsDataGeneralFonts *, fe_FontPitch *);
	static void fe_set_new_font_families(PrefsDataGeneralFonts *, fe_FontPitch *, 
										 Widget, Widget);
	static void fe_get_scaled_font_size(PrefsDataGeneralFonts *fep);
	static Boolean xfe_is_asian_encoding(int);
}

// ************************************************************************
// *************************     Appearance       *************************
// ************************************************************************

// Member:       XFE_PrefsPageGeneralAppearance
// Description:  Constructor
// Inputs:
// Side effects: 

XFE_PrefsPageGeneralAppearance::XFE_PrefsPageGeneralAppearance(XFE_PrefsDialog *dialog)
	: XFE_PrefsPage(dialog),
	  m_prefsDataGeneralAppearance(0),
	  m_toolbar_needs_updating(FALSE)
{
  m_toolbar_needs_updating = FALSE;
}

// Member:       ~XFE_PrefsPageGeneralAppearance
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsPageGeneralAppearance::~XFE_PrefsPageGeneralAppearance()
{
	delete m_prefsDataGeneralAppearance;
}

// Member:       create
// Description:  Creates page for GeneralAppearance
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralAppearance::create()
{
	Widget            kids[100];
	Arg               av[50];
	int               ac;
	int               i;

	PrefsDataGeneralAppearance *fep = NULL;

	fep = new PrefsDataGeneralAppearance;
	memset(fep, 0, sizeof(PrefsDataGeneralAppearance));
	m_prefsDataGeneralAppearance = fep;

	fep->context = getContext();
	fep->prompt_dialog = getPrefsDialog()->getDialogChrome();

	Widget form;

	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	form = XmCreateForm (m_wPageForm, "appearance", av, ac);
	XtManageChild (form);
	m_wPage = fep->page = form;

	// Launch on startup

	Widget frame1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame1 = XmCreateFrame (form, "launchFrame", av, ac);

	Widget form1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form1 = XmCreateForm (frame1, "launchBox", av, ac);

	Widget label1;

	ac = 0;
	XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	label1 = XmCreateLabelGadget (frame1, "launchBoxLabel", av, ac);

// we have the following possible toggle buttons:
//      widget         define's required      fe_* calls may say not installed?
//      ------         -----------------      ---------------------------------
//      navigator      <none>                 no
//      composer       EDITOR                 no
//      netcaster      <none>                 yes
//      messenger      MOZ_MAIL_NEWS          no
//      collabra       MOZ_MAIL_NEWS          no
//      conference     MOZ_MAIL_NEWS          yes
//      calendar       MOZ_MAIL_NEWS          yes

	Widget     navigator_toggle;
#ifdef EDITOR
	Widget     composer_toggle;
#endif
	Widget     netcaster_toggle = 0;
#ifdef MOZ_MAIL_NEWS
	Widget     messenger_toggle;
	Widget     collabra_toggle;
	Widget     conference_toggle = 0;
	Widget     calendar_toggle = 0;
#endif // MOZ_MAIL_NEWS
    Widget     widget_above;

	ac = 0;
	i = 0;

	kids[i++] = navigator_toggle = 
		XmCreateToggleButtonGadget (form1, "navigator", av, ac);

#ifdef EDITOR
	kids[i++] = composer_toggle = 
		XmCreateToggleButtonGadget (form1, "composer", av, ac);
#endif

	if (fe_IsNetcasterInstalled()) {
		kids[i++] = netcaster_toggle = 
			XmCreateToggleButtonGadget (form1, "netcaster", av, ac);
	}

#ifdef MOZ_MAIL_NEWS
	kids[i++] = messenger_toggle = 
		XmCreateToggleButtonGadget (form1, "messenger", av, ac);

	kids[i++] = collabra_toggle = 
		XmCreateToggleButtonGadget (form1, "collabra", av, ac);

	if (fe_IsConferenceInstalled()) {
		kids[i++] = conference_toggle = 
			XmCreateToggleButtonGadget (form1, "conference", av, ac);
	}

	if (fe_IsCalendarInstalled()) {
		kids[i++] = calendar_toggle = 
			XmCreateToggleButtonGadget (form1, "calendar", av, ac);
	}
#endif // MOZ_MAIL_NEWS

	fep->navigator_toggle = navigator_toggle;
#ifdef EDITOR
	fep->composer_toggle = composer_toggle;
#endif
	fep->netcaster_toggle = netcaster_toggle;

#ifdef MOZ_MAIL_NEWS
	fep->messenger_toggle = messenger_toggle;
	fep->collabra_toggle = collabra_toggle;
	fep->conference_toggle = conference_toggle;
	fep->calendar_toggle = calendar_toggle;
#endif // MOZ_MAIL_NEWS


// attempt a layout like this:
//       Navigator          Messenger
//       Composer           Collabra
//       Netcaster          Conference
//                          Calendar
// with things shifting appropriately if various components 
// are missing.
	XtVaSetValues (navigator_toggle,
				   XmNindicatorType, XmN_OF_MANY,
				   XmNtopAttachment, XmATTACH_FORM,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);
    widget_above = navigator_toggle;

#ifdef EDITOR
	XtVaSetValues (composer_toggle,
				   XmNindicatorType, XmN_OF_MANY,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, widget_above,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);
    widget_above = composer_toggle;
#endif

	if (netcaster_toggle) {
        XtVaSetValues (netcaster_toggle,
					   XmNindicatorType, XmN_OF_MANY,
					   XmNtopAttachment, XmATTACH_WIDGET,
					   XmNtopWidget, widget_above,
					   XmNbottomAttachment, XmATTACH_NONE,
					   XmNleftAttachment, XmATTACH_FORM,
					   XmNrightAttachment, XmATTACH_NONE,
					   0);
	}

#ifdef MOZ_MAIL_NEWS
	XtVaSetValues (messenger_toggle,
				   XmNindicatorType, XmN_OF_MANY,
				   XmNtopAttachment, XmATTACH_FORM,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, navigator_toggle,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);
    widget_above = messenger_toggle;

	XtVaSetValues (collabra_toggle,
				   XmNindicatorType, XmN_OF_MANY,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, widget_above,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, navigator_toggle,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);
    widget_above = collabra_toggle;

	if (conference_toggle) {
		XtVaSetValues (conference_toggle,
					   XmNindicatorType, XmN_OF_MANY,
					   XmNtopAttachment, XmATTACH_WIDGET,
					   XmNtopWidget, widget_above,
					   XmNbottomAttachment, XmATTACH_NONE,
					   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
					   XmNleftWidget, navigator_toggle,
					   XmNrightAttachment, XmATTACH_NONE,
					   0);
        widget_above = conference_toggle;
	}

	if (netcaster_toggle) {
		XtVaSetValues (netcaster_toggle,
					   XmNindicatorType, XmN_OF_MANY,
					   XmNtopAttachment, XmATTACH_WIDGET,
					   XmNtopWidget, widget_above,
					   XmNbottomAttachment, XmATTACH_NONE,
					   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
					   XmNleftWidget, navigator_toggle,
					   XmNrightAttachment, XmATTACH_NONE,
					   0);
        widget_above = netcaster_toggle;
	}

	if (calendar_toggle) {
		XtVaSetValues (calendar_toggle,
					   XmNindicatorType, XmN_OF_MANY,
					   XmNtopAttachment, XmATTACH_WIDGET,
					   XmNtopWidget, widget_above,
					   XmNbottomAttachment, XmATTACH_NONE,
					   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
					   XmNleftWidget, navigator_toggle,
					   XmNrightAttachment, XmATTACH_NONE,
					   0);
	}
#endif // MOZ_MAIL_NEWS

	XtManageChildren (kids, i);
	XtManageChild (label1);
	XtManageChild (form1);

	// Show Toolbar As

	Widget frame2;

	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg (av [ac], XmNtopWidget, frame1); ac++;
	XtSetArg (av [ac], XmNtopOffset, 8); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame2 = XmCreateFrame (form, "frame2", av, ac);

	Widget form2;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form2 = XmCreateForm (frame2, "toolbarBox", av, ac);

	Widget label2;

	ac = 0;
	XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	label2 = XmCreateLabelGadget (frame2, "toolbarBoxLabel", av, ac);

	ac = 0;
	i = 0;

	kids[i++] = fep->pic_and_text_toggle = 
		XmCreateToggleButtonGadget (form2, "picAndText", av, ac);

	kids[i++] = fep->pic_only_toggle =
		XmCreateToggleButtonGadget (form2, "picOnly", av, ac);

	kids[i++] = fep->text_only_toggle =
		XmCreateToggleButtonGadget (form2, "textOnly", av, ac);

	kids[i++] = fep->show_tooltips_toggle =
		fe_CreateToolTipsDemoToggle (form2, "showTooltips", av, ac);

	XtVaSetValues (fep->pic_and_text_toggle,
				   XmNindicatorType, XmONE_OF_MANY,
				   XmNtopAttachment, XmATTACH_FORM,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);
	XtVaSetValues (fep->pic_only_toggle,
				   XmNindicatorType, XmONE_OF_MANY,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, fep->pic_and_text_toggle,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, fep->pic_and_text_toggle,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (fep->text_only_toggle,
				   XmNindicatorType, XmONE_OF_MANY,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, fep->pic_only_toggle,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, fep->pic_and_text_toggle,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (fep->show_tooltips_toggle,
				   XmNindicatorType, XmN_OF_MANY,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, fep->pic_and_text_toggle,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_POSITION,
				   XmNleftPosition, 50,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtManageChildren (kids, i);
	XtManageChild (label2);
	XtManageChild (form2);
	XtManageChild (frame1);
	XtManageChild (frame2);

	XtAddCallback (fep->pic_and_text_toggle, XmNvalueChangedCallback,
				   cb_setToolbarAttr, fep);
	XtAddCallback (fep->pic_only_toggle, XmNvalueChangedCallback,
				   cb_setToolbarAttr, fep);
	XtAddCallback (fep->text_only_toggle, XmNvalueChangedCallback,
				   cb_setToolbarAttr, fep);

	setCreated(TRUE);
}

// Member:       init
// Description:  Initializes page for Appearance
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralAppearance::init()
{
	XP_ASSERT(m_prefsDataGeneralAppearance);

	PrefsDataGeneralAppearance *fep = m_prefsDataGeneralAppearance;
	XFE_GlobalPrefs            *prefs = &fe_globalPrefs;
    Boolean                    sensitive; 

	// Launch on startup
	
	XtVaSetValues(fep->navigator_toggle, XmNset, prefs->startup_browser_p, 0);
#ifdef EDITOR
	XtVaSetValues(fep->composer_toggle,  XmNset, prefs->startup_editor_p, 0);
#endif
	if (fep->netcaster_toggle) {
		XtVaSetValues(fep->netcaster_toggle,
					  XmNset, prefs->startup_netcaster_p, 0);
	}

#ifdef MOZ_MAIL_NEWS
	XtVaSetValues(fep->messenger_toggle, XmNset, prefs->startup_mail_p, 0);
	XtVaSetValues(fep->collabra_toggle,  XmNset, prefs->startup_news_p, 0);
	if (fep->conference_toggle) {
		XtVaSetValues(fep->conference_toggle,
					  XmNset, prefs->startup_conference_p, 0);
	}
	if (fep->calendar_toggle) {
		XtVaSetValues(fep->calendar_toggle,
					  XmNset, prefs->startup_calendar_p, 0);
	}
#endif // MOZ_MAIL_NEWS

    sensitive = !PREF_PrefIsLocked("general.startup.browser");
	XtSetSensitive(fep->navigator_toggle, sensitive);
#ifdef EDITOR
    sensitive = !PREF_PrefIsLocked("general.startup.editor");
	XtSetSensitive(fep->composer_toggle, sensitive);
#endif
	if (fep->netcaster_toggle) {
		sensitive = !PREF_PrefIsLocked("general.startup.netcaster");
		XtSetSensitive(fep->netcaster_toggle, sensitive);
	}
#ifdef MOZ_MAIL_NEWS
    sensitive = !PREF_PrefIsLocked("general.startup.mail");
	XtSetSensitive(fep->messenger_toggle, sensitive);
    sensitive = !PREF_PrefIsLocked("general.startup.news");
	XtSetSensitive(fep->collabra_toggle, sensitive);
	if (fep->conference_toggle) {
		sensitive = !PREF_PrefIsLocked("general.startup.conference");
		XtSetSensitive(fep->conference_toggle, sensitive);
	}
	if (fep->calendar_toggle) {
		sensitive = !PREF_PrefIsLocked("general.startup.calendar");
		XtSetSensitive(fep->calendar_toggle, sensitive);
	}
#endif // MOZ_MAIL_NEWS

	// Show toolbar as

	XtVaSetValues(fep->pic_and_text_toggle, 
				  XmNset, (prefs->toolbar_style == BROWSER_TOOLBAR_ICONS_AND_TEXT), 
				  0);
	XtVaSetValues(fep->pic_only_toggle, 
				  XmNset, (prefs->toolbar_style == BROWSER_TOOLBAR_ICONS_ONLY),
				  0);
	XtVaSetValues(fep->text_only_toggle,
				  XmNset, (prefs->toolbar_style == BROWSER_TOOLBAR_TEXT_ONLY),
				  0);

    sensitive = !PREF_PrefIsLocked("browser.chrome.toolbar_style");
    XtSetSensitive(fep->pic_and_text_toggle, sensitive);
    XtSetSensitive(fep->pic_only_toggle, sensitive);
    XtSetSensitive(fep->text_only_toggle, sensitive);

	XtVaSetValues(fep->show_tooltips_toggle, XmNset, prefs->toolbar_tips_p, 0);

    sensitive = !PREF_PrefIsLocked("browser.chrome.toolbar_tips");
	XtSetSensitive(fep->show_tooltips_toggle, sensitive);

	setInitialized(TRUE);
}

// Member:       install
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralAppearance::install()
{
	fe_installGeneralAppearance();

	if (m_toolbar_needs_updating) {
		// Notify whoever is interested in updating toolbar appearance
		XFE_MozillaApp::theApp()->notifyInterested(XFE_MozillaApp::updateToolbarAppearance);
		m_toolbar_needs_updating = FALSE;
	}
}

// Member:       save
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralAppearance::save()
{
	PrefsDataGeneralAppearance *fep = m_prefsDataGeneralAppearance;
	Boolean                     b;

	XP_ASSERT(fep);

	// Launch on startup

	XtVaGetValues(fep->navigator_toggle, XmNset, &b, 0);
	fe_globalPrefs.startup_browser_p = b;
#ifdef EDITOR
	XtVaGetValues(fep->composer_toggle, XmNset, &b, 0);
	fe_globalPrefs.startup_editor_p = b;
#endif
	if (fep->netcaster_toggle) {
		XtVaGetValues(fep->netcaster_toggle, XmNset, &b, 0);
		fe_globalPrefs.startup_netcaster_p = b;
	}

#ifdef MOZ_MAIL_NEWS
	XtVaGetValues(fep->messenger_toggle, XmNset, &b, 0);
	fe_globalPrefs.startup_mail_p = b;
	XtVaGetValues(fep->collabra_toggle, XmNset, &b, 0);
	fe_globalPrefs.startup_news_p = b;
	if (fep->conference_toggle) {
		XtVaGetValues(fep->conference_toggle, XmNset, &b, 0);
		fe_globalPrefs.startup_conference_p = b;
	}
	if (fep->calendar_toggle) {
		XtVaGetValues(fep->calendar_toggle, XmNset, &b, 0);
		fe_globalPrefs.startup_calendar_p = b;
	}
#endif // MOZ_MAIL_NEWS

	// Show Toolbar as

    Boolean picsNtext;
    Boolean picsOnly;
	int     old_toolbar_style = fe_globalPrefs.toolbar_style;

	XtVaGetValues(fep->pic_and_text_toggle, XmNset, &picsNtext, 0);
    XtVaGetValues(fep->pic_only_toggle, XmNset, &picsOnly, 0);

    if (picsNtext)
      {
		fe_globalPrefs.toolbar_style   = BROWSER_TOOLBAR_ICONS_AND_TEXT;
		fe_globalPrefs.toolbar_icons_p = True;
        fe_globalPrefs.toolbar_text_p  = True;
      } 
    else if (picsOnly)
      {
		fe_globalPrefs.toolbar_style   = BROWSER_TOOLBAR_ICONS_ONLY;
        fe_globalPrefs.toolbar_icons_p = True;
        fe_globalPrefs.toolbar_text_p  = False;
      }
    else
      {
		fe_globalPrefs.toolbar_style   = BROWSER_TOOLBAR_TEXT_ONLY;
        fe_globalPrefs.toolbar_icons_p = False;
        fe_globalPrefs.toolbar_text_p  = True;
      }

	if (old_toolbar_style != fe_globalPrefs.toolbar_style) {
		m_toolbar_needs_updating = TRUE;
	}

	XtVaGetValues(fep->show_tooltips_toggle, XmNset, &b, 0);
	fe_globalPrefs.toolbar_tips_p = b;

	// Install preferences

	install();
}

// Member:       getData
// Description:  
// Inputs:
// Side effects: 

PrefsDataGeneralAppearance *XFE_PrefsPageGeneralAppearance::getData()
{
	return m_prefsDataGeneralAppearance;
}

void XFE_PrefsPageGeneralAppearance::cb_setToolbarAttr(Widget    widget,
													   XtPointer closure,
													   XtPointer call_data)
{
	PrefsDataGeneralAppearance *fep = (PrefsDataGeneralAppearance *)closure;
	XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)call_data;

	if (! cb->set) {
		XtVaSetValues(widget, XmNset, True, 0);
	}
	else if (widget == fep->pic_and_text_toggle) {
		XtVaSetValues(fep->pic_only_toggle, XmNset, False, 0);
		XtVaSetValues(fep->text_only_toggle, XmNset, False, 0);
	}
	else if (widget == fep->pic_only_toggle)	{
		XtVaSetValues(fep->pic_and_text_toggle, XmNset, False, 0);
		XtVaSetValues(fep->text_only_toggle, XmNset, False, 0);
	}
	else if (widget == fep->text_only_toggle)	{
		XtVaSetValues(fep->pic_and_text_toggle, XmNset, False, 0);
		XtVaSetValues(fep->pic_only_toggle, XmNset, False, 0);
	}
	else
		abort();
}

// ************************************************************************
// *************************    General/Fonts     *************************
// ************************************************************************

// Member:       XFE_PrefsPageGeneralFonts
// Description:  Constructor
// Inputs:
// Side effects: 

XFE_PrefsPageGeneralFonts::XFE_PrefsPageGeneralFonts(XFE_PrefsDialog *dialog)
	: XFE_PrefsPage(dialog),
	  m_prefsDataGeneralFonts(0)
{
}

// Member:       ~XFE_PrefsPageGeneralFonts
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsPageGeneralFonts::~XFE_PrefsPageGeneralFonts()
{
	if (m_prefsDataGeneralFonts)
		XP_FREE(m_prefsDataGeneralFonts->encoding_menu_csid);
	delete m_prefsDataGeneralFonts;
}

void XFE_PrefsPageGeneralFonts::relayout(PrefsDataGeneralFonts *fep)
{
	int family_pos, size_pos;
	int family_label_width, family_width, size_label_width;
	int toggle_text_height;

	/*
     * Layout the encoding widgets
     *
     *   family label  family selector   size label  size selector
     *     column          column          column      column
     *
     *                + family_pos
     *                |                             + size_pos
     *                |                             |
     *                v                             v
     *   family_label:family              size_label:size_selector 
     *                                        toggle:size_text
     *
     */
	family_label_width = XfeVaGetWidestWidget(fep->encoding_label,
							fep->proportional_label, fep->fixed_label, NULL);

	family_pos = family_label_width;

	family_width = XfeVaGetWidestWidget(fep->prop_family_option, 
										fep->fixed_family_option, NULL);

	size_label_width = XfeVaGetWidestWidget(fep->prop_size_label,
							fep->fixed_size_label, NULL);

	size_pos = family_pos + family_width + size_label_width;

	toggle_text_height = XfeVaGetTallestWidget(fep->prop_size_toggle,
											   fep->prop_size_field,
											   NULL);

	/*
	 * Layout the family labels and family selectors
	 */

	/* encoding */
	XtVaSetValues (fep->encoding_label,
				   XmNleftAttachment, XmATTACH_NONE,
				   RIGHT_JUSTIFY_VA_ARGS(fep->encoding_label,family_pos),
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, fep->encoding_menu,
				   XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNbottomWidget, fep->encoding_menu,
				   0);
	XtVaSetValues (fep->encoding_menu,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNleftOffset, family_pos,
				   XmNrightAttachment, XmATTACH_NONE,
				   XmNtopAttachment, XmATTACH_FORM,
				   XmNbottomAttachment, XmATTACH_NONE,
				   0);

	/* proportional font */
	XtVaSetValues (fep->proportional_label,
				   XmNleftAttachment, XmATTACH_NONE,
				   RIGHT_JUSTIFY_VA_ARGS(fep->proportional_label,family_pos),
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, fep->prop_family_option,
				   XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNbottomWidget, fep->prop_family_option,
				   0);
	XtVaSetValues (fep->prop_family_option,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNleftOffset, family_pos,
				   XmNrightAttachment, XmATTACH_NONE,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, fep->encoding_menu,
				   XmNbottomAttachment, XmATTACH_NONE,
				   0);

	/* monospaced font */
	XtVaSetValues (fep->fixed_label,
				   XmNleftAttachment, XmATTACH_NONE,
				   RIGHT_JUSTIFY_VA_ARGS(fep->fixed_label,family_pos),
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, fep->fixed_family_option,
				   XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNbottomWidget, fep->fixed_family_option,
				   0);
	XtVaSetValues (fep->fixed_family_option,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNleftOffset, family_pos,
				   XmNrightAttachment, XmATTACH_NONE,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, fep->prop_size_field,
				   XmNbottomAttachment, XmATTACH_NONE,
				   0);

	/*
	 * Layout the size labels and family selectors
	 */
	/* proportional font */
	XtVaSetValues (fep->prop_size_label,
				   XmNleftAttachment, XmATTACH_NONE,
				   RIGHT_JUSTIFY_VA_ARGS(fep->prop_size_label,size_pos),
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, fep->prop_family_option,
				   XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNbottomWidget, fep->prop_family_option,
				   0);
	XtVaSetValues (fep->prop_size_option,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNleftOffset, size_pos,
				   XmNrightAttachment, XmATTACH_NONE,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, fep->prop_family_option,
				   XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNbottomWidget, fep->prop_family_option,
				   0);
	/* monospaced font */
	XtVaSetValues (fep->fixed_size_label,
				   XmNleftAttachment, XmATTACH_NONE,
				   RIGHT_JUSTIFY_VA_ARGS(fep->fixed_size_label,size_pos),
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, fep->fixed_family_option,
				   XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNbottomWidget, fep->fixed_family_option,
				   0);
	XtVaSetValues (fep->fixed_size_option,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNleftOffset, size_pos,
				   XmNrightAttachment, XmATTACH_NONE,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, fep->fixed_family_option,
				   XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNbottomWidget, fep->fixed_family_option,
				   0);

	/*
	 * Layout the scaled size toggle and text field
	 */
	/* proportional font */
	XtVaSetValues (fep->prop_size_toggle,
				   XmNleftAttachment, XmATTACH_NONE,
				   RIGHT_JUSTIFY_VA_ARGS(fep->prop_size_toggle,size_pos),
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNheight, toggle_text_height,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, fep->prop_size_field,
				   XmNbottomAttachment, XmATTACH_NONE,
				   0);
	XtVaSetValues (fep->prop_size_field,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNleftOffset, size_pos,
				   XmNrightAttachment, XmATTACH_NONE,
				   XmNheight, toggle_text_height,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, fep->prop_size_option,
				   XmNbottomAttachment, XmATTACH_NONE,
				   0);

	/* monospaced font */
	XtVaSetValues (fep->fixed_size_toggle,
				   XmNleftAttachment, XmATTACH_NONE,
				   RIGHT_JUSTIFY_VA_ARGS(fep->fixed_size_toggle,size_pos),
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNheight, toggle_text_height,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, fep->fixed_size_field,
				   XmNbottomAttachment, XmATTACH_NONE,
				   0);
	XtVaSetValues (fep->fixed_size_field,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNleftOffset, size_pos,
				   XmNrightAttachment, XmATTACH_NONE,
				   XmNheight, toggle_text_height,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, fep->fixed_size_option,
				   XmNbottomAttachment, XmATTACH_NONE,
				   0);
}

// Member:       create
// Description:  Creates page for GeneralFonts
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralFonts::create()
{
	Widget            kids[100];
	Arg               av[50];
	int               ac;
	int               i;
	int               j;

	PrefsDataGeneralFonts *fep = NULL;

	fep = new PrefsDataGeneralFonts;
	memset(fep, 0, sizeof(PrefsDataGeneralFonts));
	m_prefsDataGeneralFonts = fep;

	fep->context = getContext();
	fep->prompt_dialog = getPrefsDialog()->getDialogChrome();
	fep->encoding_menu_csid = (int*)XP_CALLOC(INTL_CHAR_SET_MAX, sizeof(int));

	Widget form;

	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	form = XmCreateForm (m_wPageForm, "fonts", av, ac);
	XtManageChild (form);
	m_wPage = fep->page = form;

	// Fonts and Encodings 

	Widget frame1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame1 = XmCreateFrame (form, "fontsFrame", av, ac);

	Widget form1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form1 = XmCreateForm (frame1, "fontsBox", av, ac);

	Widget label1;

	ac = 0;
	XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	label1 = XmCreateLabelGadget (frame1, "fontsBoxLabel", av, ac);

	Visual    *v = 0;
	Colormap   cmap = 0;
	Cardinal   depth = 0;

	ac = 0;
	i = 0;
	kids[i++] = fep->encoding_label = XmCreateLabelGadget (form1, "encodingLabel",
													   av, ac);

	XtVaGetValues (getPrefsDialog()->getPrefsParent(),
				   XtNvisual, &v,
				   XtNcolormap, &cmap,
				   XtNdepth, &depth, 
				   0);

	/* Create a combobox for storing encoding choices */

	ac = 0;
	XtSetArg (av[ac], XmNmoveSelectedItemUp, False); ac++;
	XtSetArg (av[ac], XmNtype, XmDROP_DOWN_LIST_BOX); ac++;
	XtSetArg (av[ac], XmNarrowType, XmMOTIF); ac++;
	XtSetArg (av[ac], XmNvisual, v); ac++;
	XtSetArg (av[ac], XmNcolormap, cmap); ac++;
	XtSetArg (av[ac], XmNdepth, depth); ac++;
	kids[i++] = fep->encoding_menu = DtCreateComboBox(form1, "encodingMenu", av,ac);
	XtAddCallback(fep->encoding_menu, XmNselectionCallback, cb_charSet, this);


	fe_FontCharSet *charset;
	int             selectedCharSet;

	selectedCharSet = 0;
	ac = 0;
	int num_encodings = 0;
	for (j = 0; j < INTL_CHAR_SET_MAX; j++) {
		XmString xmstr;

		charset = &fe_FontCharSets[fe_SortedFontCharSets[j]];
		if (!charset->name) {
			continue;
		}
		fep->encoding_menu_csid[j] = fe_SortedFontCharSets[j];
		xmstr = XmStringCreateLocalized(charset->name);
		DtComboBoxAddItem(fep->encoding_menu, xmstr, 0, True);
		num_encodings += 1;
		if (charset->selected) {
			DtComboBoxSelectItem(fep->encoding_menu, xmstr);
			selectedCharSet = j;
		}
		XmStringFree(xmstr);
	}
	if (num_encodings > 15)
		num_encodings = 15;
	XtVaSetValues(fep->encoding_menu, XmNvisibleItemCount, num_encodings, NULL);
	charset = &fe_FontCharSets[fe_SortedFontCharSets[selectedCharSet]];
	fep->selected_encoding = fe_SortedFontCharSets[selectedCharSet];

	ac = 0;
	kids[i++] = fep->proportional_label = XmCreateLabelGadget (form1,
												  "proportionalLabel", av, ac);

	ac = 0;
	XtSetArg (av[ac], XmNarrowType, XmMOTIF); ac++;
	XtSetArg (av[ac], XmNvisual, v); ac++;
	XtSetArg (av[ac], XmNcolormap, cmap); ac++;
	XtSetArg (av[ac], XmNdepth, depth); ac++;
	kids[i++] = fep->prop_family_option = DtCreateComboBox(form1,
												   "proportionalMenu", av, ac);
	/* need to add a callback */
  	XtAddCallback (fep->prop_family_option, XmNselectionCallback,
  								prefsGeneralFontsCb_fontFamily, fep);
  
  
  	ac = 0;
 	kids[i++] = fep->prop_size_label = XmCreateLabelGadget (form1, "propSizeLabel",
  														av, ac);
  
	ac = 0;
	XtSetArg (av[ac], XmNarrowType, XmMOTIF); ac++;
	XtSetArg (av[ac], XmNvisual, v); ac++;
	XtSetArg (av[ac], XmNcolormap, cmap); ac++;
	XtSetArg (av[ac], XmNdepth, depth); ac++;
	kids[i++] = fep->prop_size_option = DtCreateComboBox (form1, "propSizeMenu",
														  av, ac);
	XtAddCallback(fep->prop_size_option, XmNselectionCallback,
							prefsGeneralFontsCb_fontSize, fep);

	ac = 0;
	XtSetArg (av[ac], XmNcolumns, 5); ac++;
	kids [i++] = fep->prop_size_field =
		fe_CreateTextField (form1, "propSizeField",	av, ac);

	ac = 0;
	kids[i++] = fep->prop_size_toggle = 
		XmCreateToggleButtonGadget(form1, "propSizeToggle", av, ac);

	XtAddCallback(fep->prop_size_toggle, XmNvalueChangedCallback,
				  cb_allowScaling, this);

	fe_set_new_font_families(fep, &charset->pitches[0], fep->prop_family_option,
							 fep->prop_size_option);

	ac = 0;
	kids[i++] = fep->fixed_label = XmCreateLabelGadget(form1,
												  "fixedLabel", av, ac);

	ac = 0;
	XtSetArg (av[ac], XmNarrowType, XmMOTIF); ac++;
	XtSetArg (av[ac], XmNvisual, v); ac++;
	XtSetArg (av[ac], XmNcolormap, cmap); ac++;
	XtSetArg (av[ac], XmNdepth, depth); ac++;
	kids[i++] = fep->fixed_family_option = DtCreateComboBox (form1,
															 "fixedMenu", av, ac);
	XtAddCallback (fep->fixed_family_option, XmNselectionCallback,
					prefsGeneralFontsCb_fontFamily, fep);


	ac = 0;
	kids[i++] = fep->fixed_size_label = XmCreateLabelGadget (form1, "fixedSizeLabel",
														av, ac);

	ac = 0;
	XtSetArg (av[ac], XmNarrowType, XmMOTIF); ac++;
	XtSetArg (av[ac], XmNvisual, v); ac++;
	XtSetArg (av[ac], XmNcolormap, cmap); ac++;
	XtSetArg (av[ac], XmNdepth, depth); ac++;
	kids[i++] = fep->fixed_size_option = DtCreateComboBox (form1, "fixedSizeMenu",
														   av, ac);
	XtAddCallback(fep->fixed_size_option, XmNselectionCallback,
				  prefsGeneralFontsCb_fontSize, fep);

	ac = 0;
	XtSetArg (av[ac], XmNcolumns, 5); ac++;
	kids [i++] = fep->fixed_size_field = 
		fe_CreateTextField (form1, "fixedSizeField", av, ac);

	ac = 0;
	kids [i++] = fep->fixed_size_toggle =
		XmCreateToggleButtonGadget (form1, "fixedSizeToggle", av, ac);

	XtAddCallback(fep->fixed_size_toggle, XmNvalueChangedCallback,
				  cb_allowScaling, this);

	fe_set_new_font_families(fep, &charset->pitches[1],
							 fep->fixed_family_option, fep->fixed_size_option);

 	relayout(fep);

	XtManageChildren (kids, i);
	XtManageChild (label1);
	XtManageChild (form1);
	XtManageChild (frame1);

	// Web Font

	Widget frame2;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg (av [ac], XmNtopWidget, frame1); ac++;
	XtSetArg (av [ac], XmNtopOffset, 8); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame2 = XmCreateFrame (form, "webFontFrame", av, ac);

	Widget form2;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form2 = XmCreateForm (frame2, "webFontBox", av, ac);

  	ac = 0;
	i = 0;

 	kids[i++] = fep->use_font_label =
  		XmCreateLabelGadget (form2, "useFontLabel", av, ac);
 	kids[i++] = fep->use_my_font_toggle = 
  		XmCreateToggleButtonGadget(form2, "useMyFont", av, ac);
 	kids[i++] = fep->use_doc_font_selective_toggle = 
  		XmCreateToggleButtonGadget(form2, "useDocFontSelective", av, ac);
 	kids[i++] = fep->use_doc_font_whenever_toggle = 
  		XmCreateToggleButtonGadget(form2, "useDocFontWhenever", av, ac);
    
	/*
	 * Layout the webfont enable toggles
	 */
	XtVaSetValues(fep->use_font_label,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  0);

	XtVaSetValues(fep->use_my_font_toggle,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, fep->use_font_label,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, fep->use_font_label,
				  XmNleftOffset, 16,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  0);

	XtVaSetValues(fep->use_doc_font_selective_toggle,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, fep->use_my_font_toggle,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, fep->use_my_font_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  0);

	XtVaSetValues(fep->use_doc_font_whenever_toggle,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, fep->use_doc_font_selective_toggle,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, fep->use_my_font_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  0);

	XtManageChildren (kids, i);
	XtManageChild (form2);
	XtManageChild (frame2);

 	XtAddCallback(fep->use_my_font_toggle, XmNvalueChangedCallback,
  				  cb_toggleUseFont, fep);
 	XtAddCallback(fep->use_doc_font_selective_toggle, XmNvalueChangedCallback,
  				  cb_toggleUseFont, fep);
 	XtAddCallback(fep->use_doc_font_whenever_toggle, XmNvalueChangedCallback,
  				  cb_toggleUseFont, fep);

	setCreated(TRUE);
}

// Member:       init
// Description:  Initializes page for GeneralFonts
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralFonts::init()
{
	XP_ASSERT(m_prefsDataGeneralFonts);

	PrefsDataGeneralFonts  *fep = m_prefsDataGeneralFonts;
	XFE_GlobalPrefs        *prefs = &fe_globalPrefs;
    Boolean                sensitive;

	// TODO: Fonts & Encoding

	// Web font

	XtVaSetValues(fep->use_my_font_toggle, 
				  XmNset, (prefs->use_doc_fonts == DOC_FONTS_NEVER),
				  0);

	XtVaSetValues(fep->use_doc_font_selective_toggle, 
				  XmNset, (prefs->use_doc_fonts == DOC_FONTS_QUICK),
				  0);

	XtVaSetValues(fep->use_doc_font_whenever_toggle, 
				  XmNset, (prefs->use_doc_fonts == DOC_FONTS_ALWAYS),
				  0);

    // If pref is locked, grey it out.
    sensitive = !PREF_PrefIsLocked("browser.use_document_fonts");
	XtSetSensitive(fep->use_my_font_toggle, sensitive);
	XtSetSensitive(fep->use_doc_font_selective_toggle, sensitive);
	XtSetSensitive(fep->use_doc_font_whenever_toggle, sensitive);

	setInitialized(TRUE);
}

// Member:       install
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralFonts::install()
{
	fe_installGeneralFonts();
}

// Member:       save
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralFonts::save()
{
	PrefsDataGeneralFonts *fep = m_prefsDataGeneralFonts;

	XP_ASSERT(fep);

	fe_get_scaled_font_size(fep);

	if (fep->fonts_changed) {
		// Notify whoever is interested in changes in default font
		XFE_MozillaApp::theApp()->notifyInterested(XFE_MozillaApp::defaultFontChanged);
		fep->fonts_changed = 0;
    }

	Boolean b;

    XtVaGetValues(fep->use_my_font_toggle, XmNset, &b, 0);
    if (b) fe_globalPrefs.use_doc_fonts = DOC_FONTS_NEVER;

    XtVaGetValues(fep->use_doc_font_selective_toggle, XmNset, &b, 0);
    if (b) fe_globalPrefs.use_doc_fonts = DOC_FONTS_QUICK;

    XtVaGetValues(fep->use_doc_font_whenever_toggle, XmNset, &b, 0);
    if (b) fe_globalPrefs.use_doc_fonts = DOC_FONTS_ALWAYS;

	// Install preferences

	install();
}

// Member:       getData
// Description:  
// Inputs:
// Side effects: 

PrefsDataGeneralFonts *XFE_PrefsPageGeneralFonts::getData()
{
	return m_prefsDataGeneralFonts;
}

void prefsGeneralFontsCb_fontFamily(Widget     widget,
									XtPointer  closure,
									XtPointer  call_data)
{
	PrefsDataGeneralFonts *fep;
	XtPointer              data;
	fe_FontFamily         *family;
	fe_FontPitch	      *pitch;
	int	                   i;
	DtComboBoxCallbackStruct *cbs;

	fep = (PrefsDataGeneralFonts *)closure;
	fep->fonts_changed = 1;
	cbs = (DtComboBoxCallbackStruct *)call_data;

	XtVaGetValues(widget, XmNuserData, &data, 0);
	pitch = (fe_FontPitch *) data;

	pitch->selectedFamily = NULL;
	for (i = 0; i < pitch->numberOfFamilies; i++)
	{
		family = &pitch->families[i];
		if (i == cbs->item_position)
		{
			family->selected = 1;
		}
		else
		{
			family->selected = 0;
		}
	}

	fe_set_new_font_sizes(fep, pitch);
	fe_InvalidateFontData();
	// Notify whoever is interested in changes in default font
	XFE_MozillaApp::theApp()->notifyInterested(XFE_MozillaApp::defaultFontChanged);
	fep->fonts_changed = 0;

	// For Asian encoding, the user should exit communicator in order for
	// font prefs to take effect (workaround for bug 72939)

	if (xfe_is_asian_encoding(fep->selected_encoding)) {
		FE_Alert(fep->context, XP_GetString(XFE_PREFS_RESTART_FOR_FONT_CHANGES));
	}
}

void prefsGeneralFontsCb_fontSize(Widget    widget,
								  XtPointer closure,
								  XtPointer call_data)
{
	PrefsDataGeneralFonts *fep;
	XtPointer              data;
	fe_FontFamily         *family;
	fe_FontPitch          *pitch;
	fe_FontSize	          *size;
	int                    i;
	DtComboBoxCallbackStruct  *cbs;

	fep = (PrefsDataGeneralFonts *) closure;
	fep->fonts_changed = 1;
	cbs = (DtComboBoxCallbackStruct *)call_data;

	XtVaGetValues(widget, XmNuserData, &data, 0);
	pitch = (fe_FontPitch *) data;

	family = NULL;
	for (i = 0; i < pitch->numberOfFamilies; i++)
	{
		family = &pitch->families[i];
		if (family->selected)
		{
			break;
		}
		else
		{
			family = NULL;
		}
	}
	if (!family)
	{
		return;
	}

	for (i = 0; i < family->numberOfPointSizes; i++)
	{
		size = &family->pointSizes[i];
		if (i == cbs->item_position)
			size->selected = 1;
		else
			size->selected = 0;
	}

	fe_FreeFontSizeTable(family);
	fe_InvalidateFontData();
	// Notify whoever is interested in changes in default font
	XFE_MozillaApp::theApp()->notifyInterested(XFE_MozillaApp::defaultFontChanged);
	fep->fonts_changed = 0;

	// For Asian encoding, the user should exit communicator in order for
	// font prefs to take effect (workaround for bug 72939)

	if (xfe_is_asian_encoding(fep->selected_encoding)) {
		FE_Alert(fep->context, XP_GetString(XFE_PREFS_RESTART_FOR_FONT_CHANGES));
	}
}

void XFE_PrefsPageGeneralFonts::cb_charSet(Widget      /* widget */,
										   XtPointer   closure,
										   XtPointer   call_data)
{
	PrefsDataGeneralFonts *fep;
	fe_FontCharSet        *charset;
	int	                   charsetID;
	int	                   i;
	DtComboBoxCallbackStruct *cbs;
	XFE_PrefsPageGeneralFonts *thePage = (XFE_PrefsPageGeneralFonts *)closure;


	fep = thePage->getData();
	cbs = (DtComboBoxCallbackStruct *)call_data;
	charsetID = fep->encoding_menu_csid[cbs->item_position];
	fep->selected_encoding = charsetID;

	for (i = 0; i < INTL_CHAR_SET_MAX; i++)
	{
		fe_FontCharSets[i].selected = 0;
	}
	charset = &fe_FontCharSets[charsetID];
	charset->selected = 1;

	fe_set_new_font_families(fep, &charset->pitches[0],
							 fep->prop_family_option, fep->prop_size_option);

	fe_set_new_font_families(fep, &charset->pitches[1],
							 fep->fixed_family_option, fep->fixed_size_option);

	thePage->relayout(fep);
}

void XFE_PrefsPageGeneralFonts::cb_toggleUseFont(Widget    widget,
												 XtPointer closure,
												 XtPointer call_data)
{
	PrefsDataGeneralFonts        *fep = (PrefsDataGeneralFonts *)closure;
	XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)call_data;

	if (! cb->set) {
		XtVaSetValues(widget, XmNset, True, 0);
	}
	else if (widget == fep->use_my_font_toggle) {
		XtVaSetValues(fep->use_doc_font_selective_toggle, XmNset, False, 0);
		XtVaSetValues(fep->use_doc_font_whenever_toggle, XmNset, False, 0);
	}
	else if (widget == fep->use_doc_font_selective_toggle) {
		XtVaSetValues(fep->use_my_font_toggle, XmNset, False, 0);
		XtVaSetValues(fep->use_doc_font_whenever_toggle, XmNset, False, 0);
	}
	else if (widget == fep->use_doc_font_whenever_toggle) {
		XtVaSetValues(fep->use_my_font_toggle, XmNset, False, 0);
		XtVaSetValues(fep->use_doc_font_selective_toggle, XmNset, False, 0);
	}
	else
		abort();
}

// Member:       cb_allowScaling
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralFonts::cb_allowScaling(Widget    w,
												XtPointer closure,
												XtPointer callData)
{
	XFE_PrefsPageGeneralFonts    *thePage = (XFE_PrefsPageGeneralFonts *)closure;
	PrefsDataGeneralFonts        *fep = thePage->getData();
	XmToggleButtonCallbackStruct *cb;
	XtPointer                     data;
	fe_FontFamily                *family;
	fe_FontPitch                 *pitch;
	unsigned int                  allowScaling;
	int                           i;
	
	cb = (XmToggleButtonCallbackStruct *)callData;
	fep->fonts_changed = 1;

	XtVaGetValues(w, XmNuserData, &data, 0);
	pitch = (fe_FontPitch *) data;

	family = NULL;
	for (i = 0; i < pitch->numberOfFamilies; i++) {
		family = &pitch->families[i];
		if (family->selected) {
			break;
		}
		else {
			family = NULL;
		}
	}
	if (!family) {
		return;
	}
	
	if (cb->set) {
		allowScaling = 1;
	}
	else {
		allowScaling = 0;
	}

	/*
	 * sanity check
	 */

	if (family->pointSizes->size) {
		allowScaling = 0;
		XtVaSetValues(w, XmNset, False, 0);
		XtVaSetValues(w, XmNsensitive, False, 0);
	}
	else if (family->numberOfPointSizes == 1) {
		allowScaling = 1;
		XtVaSetValues(w, XmNset, True, 0);
		XtVaSetValues(w, XmNsensitive, False, 0);
	}

	if (allowScaling != family->allowScaling) {
	 	fe_FreeFontSizeTable(family);
		family->allowScaling = allowScaling;
		fe_InvalidateFontData();
		// Notify whoever is interested in changes in default font
		XFE_MozillaApp::theApp()->notifyInterested(XFE_MozillaApp::defaultFontChanged);
		fep->fonts_changed = 0;
	}
}

/* This disabled the backends notion of the font cache.
 *
 * FE caches fonts for each LO_TextAttr in the LO_TextAttr->FE_Data. Once
 * the user changes either the default family/face, size, proportional/fixed,
 * scaled/non-scaled, we will need to clear the font cache that backend
 * maintains.
 */
static void fe_InvalidateFontData(void)
{
	/*
	 * free the Asian font groups, which are also caches
	 */
	fe_FreeFontGroups();
}

static void fe_set_new_font_families(PrefsDataGeneralFonts *fep,
									 fe_FontPitch          *pitch,
									 Widget                 familyCombobox,
									 Widget                 sizeCombobox)
{
	int			    i;
	fe_FontFamily  *family;
	Widget          list;

	pitch->sizeMenu = sizeCombobox;

	DtComboBoxDeleteAllItems(familyCombobox);
	XtVaGetValues(familyCombobox, XmNlist, &list, NULL);
	XtVaSetValues(list, XmNuserData, pitch, NULL);

	for (i = 0; i < pitch->numberOfFamilies; i++)
	{
		XmString xmstr;

		family = &pitch->families[i];
		xmstr = XmStringCreateLocalized(family->name);
		DtComboBoxAddItem(familyCombobox, xmstr, 0, True);
		if ((i == 0) || (family->selected))
			DtComboBoxSelectItem(familyCombobox, xmstr);
		XmStringFree(xmstr);
	}
	if (pitch->numberOfFamilies > 15)
		XtVaSetValues(familyCombobox, XmNvisibleItemCount, 15, NULL);
	else
		XtVaSetValues(familyCombobox, XmNvisibleItemCount, pitch->numberOfFamilies, NULL);

	fe_set_new_font_sizes(fep, pitch);
}

static void fe_set_new_font_sizes(PrefsDataGeneralFonts *fep, 
								  fe_FontPitch          *pitch)
{
	int			    i;
	char			name[16];
	fe_FontFamily  *family;
	fe_FontSize	   *size;
	Widget          list;
	Widget          toggle;
	XmString		xms;

    if (pitch->sizeMenu == fep->prop_size_option)
		toggle = fep->prop_size_toggle;
	else
		toggle = fep->fixed_size_toggle;

	DtComboBoxDeleteAllItems(pitch->sizeMenu);

	family = NULL;
	for (i = 0; i < pitch->numberOfFamilies; i++)
	{
		family = &pitch->families[i];
		if (family->selected)
			break;
		else
			family = NULL;
	}
	if (!family)
		return;

	XtVaGetValues(pitch->sizeMenu, XmNlist, &list, NULL);
	XtVaSetValues(list, XmNuserData, pitch, NULL);

	if (family->allowScaling) {
		XtVaSetValues(toggle, XmNset, True, 0);
	}
	else {
		XtVaSetValues(toggle, XmNset, False, 0);
	}

	if (family->pointSizes->size) {
		XtVaSetValues(toggle, XmNsensitive, False, 0);
	}
	else {
		if (family->numberOfPointSizes == 1) {
			XtVaSetValues(toggle, XmNsensitive, False, 0);
		}
		else {
			XtVaSetValues(toggle, XmNsensitive, True, 0);
		}
	}
	XtVaSetValues(toggle, XmNuserData, pitch, NULL);

	for (i = 0; i < family->numberOfPointSizes; i++)
	{
		size = &family->pointSizes[i];
		if (size->size)
			(void) PR_snprintf(name, sizeof(name), "%d.%d ",
									size->size / 10, size->size % 10);
		else
			(void) PR_snprintf(name, sizeof(name), "%d ", size->size);
		xms = XmStringCreateLtoR(name, XmFONTLIST_DEFAULT_TAG);
		DtComboBoxAddItem(pitch->sizeMenu, xms, 0, True);
		if ((i == 0) || (size->selected))
			DtComboBoxSelectItem(pitch->sizeMenu, xms);
		XmStringFree(xms);
	}
}

static void fe_get_scaled_font_size(PrefsDataGeneralFonts *fep)
{
	fe_FontCharSet  *charset = NULL;
	fe_FontFamily   *family;
	Widget           field;
	fe_FontPitch    *pitch;
	char            *endptr;
	char            *buf;
	int              i;
	int              j;
	int              oldSize;

	for (i = 0; i < INTL_CHAR_SET_MAX; i++) {
		if (fe_FontCharSets[i].selected) {
			charset = &fe_FontCharSets[i];
		}
	}

	if (!charset){
		return;
	}

	for (i = 0; i < 2; i++) {
		field = (i ? fep->fixed_size_field : fep->prop_size_field);
		pitch = &charset->pitches[i];
		XtVaGetValues(field, XmNvalue, &buf, 0);
		family = NULL;
		for (j = 0; j < pitch->numberOfFamilies; j++) {
			family = &pitch->families[j];
			if (family->selected) {
				break;
			}
			else {
				family = NULL;
			}
		}
		if (!family) {
			continue;
		}
		oldSize = family->scaledSize;
		errno = 0;
		family->scaledSize = ((int) (strtod(buf, &endptr) * 10));
		if (errno || (family->scaledSize < 0)){
			family->scaledSize = 0;
		}
		if (family->scaledSize != oldSize){
			fep->fonts_changed = 1;
			fe_FreeFontSizeTable(family);
			fe_InvalidateFontData();
		}
		XtFree(buf);
	}
}

static Boolean xfe_is_asian_encoding(int encoding) 
{
	char    *encoding_name = fe_FontCharSets[encoding].mimeName;
	
	if ((! XP_STRCASECMP(encoding_name, "gb_2312-80")) ||          // Simplified Chinese
		(! XP_STRCASECMP(encoding_name, "jis_x0201")) ||           // Japanese
		(! XP_STRCASECMP(encoding_name, "jis_x0208-1983")) ||      // Japanese
		(! XP_STRCASECMP(encoding_name, "jis_x0212-1990")) ||      // Japanese
		(! XP_STRCASECMP(encoding_name, "ks_c_5601-1987")) ||      // Korean
		(! XP_STRCASECMP(encoding_name, "x-cns11643-1110")) ||     // traditional Chinese
		(! XP_STRCASECMP(encoding_name, "x-cns11643-1")) ||        // traditional Chinese
		(! XP_STRCASECMP(encoding_name, "x-cns11643-2")) ||        // traditional Chinese
		(! XP_STRCASECMP(encoding_name, "x-gb2312-11")) ||         // simplified Chinese
		(! XP_STRCASECMP(encoding_name, "x-jisx0208-11")) ||       // Japanese
		(! XP_STRCASECMP(encoding_name, "x-ksc5601-11")) ||        // Korean
		(! XP_STRCASECMP(encoding_name, "x-x-big5")))              // Traditional Chinese
		return TRUE;
	else
		return FALSE;
}

// ************************************************************************
// *************************    General/Colors     *************************
// ************************************************************************

// Member:       XFE_PrefsPageGeneralColors
// Description:  Constructor
// Inputs:
// Side effects: 

XFE_PrefsPageGeneralColors::XFE_PrefsPageGeneralColors(XFE_PrefsDialog *dialog)
	: XFE_PrefsPage(dialog),
	  m_prefsDataGeneralColors(0),
	  m_underlinelinks_changed(FALSE),
	  m_colors_changed(FALSE)
{
}

// Member:       ~XFE_PrefsPageGeneralColors
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsPageGeneralColors::~XFE_PrefsPageGeneralColors()
{
	PrefsDataGeneralColors  *fep = m_prefsDataGeneralColors;
	char                    *orig_color;

	XtVaGetValues(fep->text_color_button, XmNuserData, &orig_color, NULL);
	if (orig_color) XtFree(orig_color);

	XtVaGetValues(fep->bg_color_button, XmNuserData, &orig_color, NULL);
	if (orig_color) XtFree(orig_color);

	XtVaGetValues(fep->links_color_button, XmNuserData, &orig_color, NULL);
	if (orig_color) XtFree(orig_color);

	XtVaGetValues(fep->vlinks_color_button, XmNuserData, &orig_color, NULL);
	if (orig_color) XtFree(orig_color);

	delete m_prefsDataGeneralColors;
}

// Member:       create
// Description:  Creates page for GeneralColors
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralColors::create()
{
	Widget            kids[100];
	Arg               av[50];
	int               ac;
	int               i;

	PrefsDataGeneralColors *fep = NULL;

	fep = new PrefsDataGeneralColors;
	memset(fep, 0, sizeof(PrefsDataGeneralColors));
	m_prefsDataGeneralColors = fep;

	fep->context = getContext();
	fep->prompt_dialog = getPrefsDialog()->getDialogChrome();

	Widget form;

	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	form = XmCreateForm (m_wPageForm, "colors", av, ac);
	XtManageChild (form);
	m_wPage = fep->page = form;

	// Colors

	Widget frame1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_POSITION); ac++;
	XtSetArg (av [ac], XmNrightPosition, 50); ac++;
	XtSetArg (av [ac], XmNrightOffset, 4); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame1 = XmCreateFrame (form, "colorFrame", av, ac);

	Widget form1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form1 = XmCreateForm (frame1, "colorBox", av, ac);

	Widget label1;

	ac = 0;
	XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	label1 = XmCreateLabelGadget (frame1, "colorBoxLabel", av, ac);

	Widget     text_color_label;
	Widget     bg_color_label;
	Widget     text_color_button;
	Widget     bg_color_button;
	Widget     use_default_colors_button;

	ac = 0;
	i = 0;

	kids[i++] = text_color_label =
		XmCreateLabelGadget(form1, "textColorLabel", av, ac);

	kids[i++] = bg_color_label =
		XmCreateLabelGadget(form1, "bgColorLabel", av, ac);

	ac = 0;
	XtSetArg (av[ac], XmNwidth, DEF_COLOR_BUTTON_WIDTH); ac++;
	XtSetArg (av[ac], XmNheight, DEF_COLOR_BUTTON_HEIGHT); ac++;
	kids[i++] = text_color_button =
		XmCreateDrawnButton(form1, "textColorButton", av, ac);

	ac = 0;
	XtSetArg (av[ac], XmNwidth, DEF_COLOR_BUTTON_WIDTH); ac++;
	XtSetArg (av[ac], XmNheight, DEF_COLOR_BUTTON_HEIGHT); ac++;
	kids[i++] = bg_color_button =
		XmCreateDrawnButton(form1, "bgColorButton", av, ac);

	ac = 0;
	kids[i++] = use_default_colors_button = 
		XmCreatePushButtonGadget(form1, "useDefColors", av, ac);

	fep->text_color_button = text_color_button;
	fep->bg_color_button = bg_color_button;
	fep->use_default_colors_button = use_default_colors_button;

	int labels_height;
	labels_height = XfeVaGetTallestWidget(text_color_label,
										  text_color_button,
										  NULL);
	int labels_width;
	labels_width = XfeVaGetWidestWidget(text_color_label,
										bg_color_label,
										NULL);

	XtVaSetValues(text_color_label,
				  XmNheight, labels_height,
				  RIGHT_JUSTIFY_VA_ARGS(text_color_label,labels_width),
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	XtVaSetValues(bg_color_label,
				  XmNheight, labels_height,
				  RIGHT_JUSTIFY_VA_ARGS(bg_color_label,labels_width),
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, text_color_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	XtVaSetValues(text_color_button,
				  XmNheight, labels_height,
				  XmNshadowThickness, 2,
				  XmNuserData, 0,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, text_color_label,
				  XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNbottomWidget, text_color_label,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, text_color_label,
				  XmNleftOffset, 4,
				  XmNrightAttachment, XmATTACH_NONE,
				  0);

	XtVaSetValues(bg_color_button,
				  XmNheight, labels_height,
				  XmNshadowThickness, 2,
				  XmNuserData, 0,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, bg_color_label,
				  XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNbottomWidget, bg_color_label,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, text_color_button,
				  XmNrightAttachment, XmATTACH_NONE,
				  0);

	XtVaSetValues(use_default_colors_button,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, bg_color_label,
				  XmNtopOffset, 4,
				  XmNbottomAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_FORM,
				  0);

	XtAddCallback(text_color_button, XmNactivateCallback, cb_activateColor, this);
	XtAddCallback(bg_color_button, XmNactivateCallback, cb_activateColor, this);
	XtAddCallback(use_default_colors_button, XmNactivateCallback, cb_defaultColor, this);

	XtManageChildren (kids, i);
	XtManageChild (label1);
	XtManageChild (form1);
	XtManageChild (frame1);

	// Links

	Widget frame2;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg (av [ac], XmNleftWidget, frame1); ac++;
	XtSetArg (av [ac], XmNleftOffset, 8); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;	
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
	XtSetArg (av [ac], XmNbottomWidget, frame1); ac++;
	frame2 = XmCreateFrame (form, "linksFrame", av, ac);

	Widget form2;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form2 = XmCreateForm (frame2, "linksBox", av, ac);

	Widget label2;

	ac = 0;
	XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	label2 = XmCreateLabelGadget (frame2, "linksBoxLabel", av, ac);

	Widget     links_color_label;
	Widget     vlinks_color_label;
	Widget     links_color_button;
	Widget     vlinks_color_button;
	Widget     underline_links_toggle;

	ac = 0;
	i = 0;

	kids[i++] = links_color_label =
		XmCreateLabelGadget(form2, "linksLabel", av, ac);

	kids[i++] = vlinks_color_label =
		XmCreateLabelGadget(form2, "vlinksLabel", av, ac);

	ac = 0;
	XtSetArg (av[ac], XmNwidth, DEF_COLOR_BUTTON_WIDTH); ac++;
	XtSetArg (av[ac], XmNheight, DEF_COLOR_BUTTON_HEIGHT); ac++;
	kids[i++] = links_color_button =
		XmCreateDrawnButton(form2, "linksColorButton", av, ac);

	ac = 0;
	XtSetArg (av[ac], XmNwidth, DEF_COLOR_BUTTON_WIDTH); ac++;
	XtSetArg (av[ac], XmNheight, DEF_COLOR_BUTTON_HEIGHT); ac++;
	kids[i++] = vlinks_color_button =
		XmCreateDrawnButton(form2, "vlinksColorButton", av, ac);

	ac = 0;
	kids[i++] = underline_links_toggle = 
		XmCreateToggleButtonGadget(form2, "underline", av, ac);

	fep->links_color_button = links_color_button;
	fep->vlinks_color_button = vlinks_color_button;
	fep->underline_links_toggle = underline_links_toggle;

	labels_height = XfeVaGetTallestWidget(links_color_label,
										  links_color_button,
										  NULL);
	labels_width = XfeVaGetWidestWidget(links_color_label,
										vlinks_color_label,
										NULL);

	XtVaSetValues(links_color_label,
				  XmNheight, labels_height,
				  RIGHT_JUSTIFY_VA_ARGS(links_color_label,labels_width),
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	XtVaSetValues(vlinks_color_label,
				  XmNheight, labels_height,
				  RIGHT_JUSTIFY_VA_ARGS(vlinks_color_label,labels_width),
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, links_color_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	XtVaSetValues(links_color_button,
				  XmNheight, labels_height,
				  XmNshadowThickness, 2,
				  XmNuserData, 0,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, links_color_label,
				  XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNbottomWidget, links_color_label,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, links_color_label,
				  XmNleftOffset, 4,
				  XmNrightAttachment, XmATTACH_NONE,
				  0);

	XtVaSetValues(vlinks_color_button,
				  XmNheight, labels_height,
				  XmNshadowThickness, 2,
				  XmNuserData, 0,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, vlinks_color_label,
				  XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNbottomWidget, vlinks_color_label,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, links_color_button,
				  XmNrightAttachment, XmATTACH_NONE,
				  0);

	XtVaSetValues(underline_links_toggle,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, vlinks_color_label,
				  XmNtopOffset, 4,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  0);

	XtAddCallback(links_color_button, XmNactivateCallback, cb_activateColor, this);
	XtAddCallback(vlinks_color_button, XmNactivateCallback, cb_activateColor, this);

	XtManageChildren (kids, i);
	XtManageChild (label2);
	XtManageChild (form2);
	XtManageChild (frame2);

	// Use my colors

	Widget frame3;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg (av [ac], XmNtopWidget, frame1); ac++;
	XtSetArg (av [ac], XmNtopOffset, 8); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame3 = XmCreateFrame (form, "frame3", av, ac);

	Widget form3;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form3 = XmCreateForm (frame3, "box3", av, ac);

	Widget     use_color_label;
	Widget     use_my_color_toggle;

	ac = 0;
	i = 0;

	kids[i++] = use_color_label =
		XmCreateLabelGadget(form3, "useColor", av, ac);

	kids[i++] = use_my_color_toggle = 
		XmCreateToggleButtonGadget(form3, "useMyColor", av, ac);

	fep->use_my_color_toggle = use_my_color_toggle;

	XtVaSetValues(use_color_label,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  NULL);
	
	XtVaSetValues(use_my_color_toggle,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, use_color_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, 8,
				  XmNrightAttachment, XmATTACH_NONE,
				  NULL);

	XtManageChildren(kids, i);
	XtManageChild(form3);
	XtManageChild(frame3);

	setCreated(TRUE);
}

// Member:       init
// Description:  Initializes page for GeneralColors
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralColors::init()
{
	XP_ASSERT(m_prefsDataGeneralColors);

	PrefsDataGeneralColors *fep = m_prefsDataGeneralColors;
	XFE_GlobalPrefs        *prefs = &fe_globalPrefs;
	LO_Color               *color;
	char                   *orig_color;
	char                    buf[32];

	// Colors - foreground

	XtVaGetValues(fep->text_color_button, XmNuserData, &orig_color, NULL);
	if (orig_color) XtFree(orig_color);
	color = &prefs->text_color;
    XP_SAFE_SPRINTF(buf, sizeof(buf), "#%02x%02x%02x", color->red, color->green, color->blue);
	XtVaSetValues(fep->text_color_button, 
				  XmNsensitive, !PREF_PrefIsLocked("browser.foreground_color"),
				  XmNuserData, XtNewString(buf),
				  0);
	fe_SwatchSetColor(fep->text_color_button, &prefs->text_color);

	// Colors - background

	XtVaGetValues(fep->bg_color_button, XmNuserData, &orig_color, NULL);
	if (orig_color) XtFree(orig_color);
	color = &prefs->background_color;
    XP_SAFE_SPRINTF(buf, sizeof(buf), "#%02x%02x%02x", color->red, color->green, color->blue);
	XtVaSetValues(fep->bg_color_button, 
				  XmNsensitive, !PREF_PrefIsLocked("browser.background_color"),
				  XmNuserData, XtNewString(buf),
				  0);
	fe_SwatchSetColor(fep->bg_color_button, &prefs->background_color);

	// Use default colors

	XtVaSetValues(fep->use_default_colors_button, 
				  XmNsensitive, 
				  !PREF_PrefIsLocked("browser.foreground_color") && 
				  !PREF_PrefIsLocked("browser.background_color"),
				  0);

	// Links - unvisited links

	XtVaGetValues(fep->links_color_button, XmNuserData, &orig_color, NULL);
	if (orig_color) XtFree(orig_color);
	color = &prefs->links_color;
    XP_SAFE_SPRINTF(buf, sizeof(buf), "#%02x%02x%02x", color->red, color->green, color->blue);
	XtVaSetValues(fep->links_color_button, 
				  XmNsensitive, !PREF_PrefIsLocked("browser.anchor_color"),
				  XmNuserData, XtNewString(buf),
				  0);
	fe_SwatchSetColor(fep->links_color_button, &prefs->links_color);

	// Links - visited links

	XtVaGetValues(fep->vlinks_color_button, XmNuserData, &orig_color, NULL);
	if (orig_color) XtFree(orig_color);
	color = &prefs->vlinks_color;
    XP_SAFE_SPRINTF(buf, sizeof(buf), "#%02x%02x%02x", color->red, color->green, color->blue);
	XtVaSetValues(fep->vlinks_color_button, 
				  XmNsensitive, !PREF_PrefIsLocked("browser.visited_color"),
				  XmNuserData, XtNewString(buf),
				  0);
	fe_SwatchSetColor(fep->vlinks_color_button, &prefs->vlinks_color);

	// Underline links

	XtVaSetValues(fep->underline_links_toggle, 
				  XmNset, prefs->underline_links_p,
				  XmNsensitive, !PREF_PrefIsLocked("browser.underline_anchors"),
				  0);

	// Use doc colors

	XtVaSetValues(fep->use_my_color_toggle,
                  XmNset, !prefs->use_doc_colors, 
                  XmNsensitive, !PREF_PrefIsLocked("browser.use_document_colors"),
                  0);

	setInitialized(TRUE);
}

// Member:       install
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralColors::install()
{
	fe_installGeneralColors();

	if (m_colors_changed) {
		XFE_MozillaApp::theApp()->notifyInterested(XFE_MozillaApp::defaultColorsChanged);
		m_colors_changed = FALSE;
	}
	else if (m_underlinelinks_changed) {
		XFE_MozillaApp::theApp()->notifyInterested(XFE_MozillaApp::linksAttributeChanged);
		m_underlinelinks_changed = FALSE;
	}
}

// Member:       save
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralColors::save()
{
	PrefsDataGeneralColors *fep = m_prefsDataGeneralColors;
	Boolean                 b;
	LO_Color                orig_color;
	LO_Color                new_color;
	char                   *selected_color;

	// Colors - foreground

	orig_color = fe_globalPrefs.text_color;
	XtVaGetValues(fep->text_color_button, XmNuserData, &selected_color, NULL);
	LO_ParseRGB(selected_color, &new_color.red, &new_color.green, &new_color.blue);
	if ((new_color.red != orig_color.red) ||
		(new_color.green != orig_color.green) ||
		(new_color.blue != orig_color.blue))
		m_colors_changed = TRUE;
	fe_globalPrefs.text_color = new_color;

	// Colors - background

	orig_color = fe_globalPrefs.background_color;
	XtVaGetValues(fep->bg_color_button, XmNuserData, &selected_color, NULL);
	LO_ParseRGB(selected_color, &new_color.red, &new_color.green, &new_color.blue);
	if ((new_color.red != orig_color.red) ||
		(new_color.green != orig_color.green) ||
		(new_color.blue != orig_color.blue))
		m_colors_changed = TRUE;
	fe_globalPrefs.background_color = new_color;

	// Links - unvisited links

	orig_color = fe_globalPrefs.links_color;
	XtVaGetValues(fep->links_color_button, XmNuserData, &selected_color, NULL);
	LO_ParseRGB(selected_color, &new_color.red, &new_color.green, &new_color.blue);
	if ((new_color.red != orig_color.red) ||
		(new_color.green != orig_color.green) ||
		(new_color.blue != orig_color.blue))
		m_colors_changed = TRUE;
	fe_globalPrefs.links_color = new_color;

	// Links - visited links

	orig_color = fe_globalPrefs.vlinks_color;
	XtVaGetValues(fep->vlinks_color_button, XmNuserData, &selected_color, NULL);
	LO_ParseRGB(selected_color, &new_color.red, &new_color.green, &new_color.blue);
	if ((new_color.red != orig_color.red) ||
		(new_color.green != orig_color.green) ||
		(new_color.blue != orig_color.blue))
		m_colors_changed = TRUE;
	fe_globalPrefs.vlinks_color = new_color;

	// Underline links

	XP_Bool old_underline_links = fe_globalPrefs.underline_links_p;
	XtVaGetValues(fep->underline_links_toggle, XmNset, &b, 0);
	fe_globalPrefs.underline_links_p = b;

	if (old_underline_links != fe_globalPrefs.underline_links_p) {
		m_underlinelinks_changed = TRUE;
	}
		
	// Use doc colors

	XtVaGetValues(fep->use_my_color_toggle, XmNset, &b, 0);
	fe_globalPrefs.use_doc_colors = !b;

	// Install preferences

	install();
}

// Member:       getData
// Description:  
// Inputs:
// Side effects: 

PrefsDataGeneralColors *XFE_PrefsPageGeneralColors::getData()
{
	return m_prefsDataGeneralColors;
}

void XFE_PrefsPageGeneralColors::cb_activateColor(Widget      w,
											  XtPointer   closure,
											  XtPointer   /* call_data */)
{
	XFE_PrefsPageGeneralColors *thePage = (XFE_PrefsPageGeneralColors *)closure;
	XFE_PrefsDialog            *theDialog = thePage->getPrefsDialog();

	char   selected_color_name[128];
	Pixel  selected_pixel;
	char  *orig_color_name;

	XtVaGetValues(w,
				  XmNuserData, &orig_color_name,
				  NULL);

	if (orig_color_name != NULL)
		XP_STRCPY(selected_color_name, orig_color_name);
	else
		selected_color_name[0] = '\0';

	if (fe_colorDialog(theDialog->getBaseWidget(), theDialog->getContext(),
					   selected_color_name, &selected_pixel)) {
		if (strlen(selected_color_name) > 0) {
			if (orig_color_name &&
				(! XP_STRCMP(orig_color_name, selected_color_name)))
				return;
			if (orig_color_name) XtFree(orig_color_name);
			XtVaSetValues(w,
						  XmNbackground, selected_pixel,
						  XmNuserData, XtNewString(selected_color_name),
						  NULL);
		}
	}
}

void XFE_PrefsPageGeneralColors::cb_defaultColor(Widget      /* w */,
												 XtPointer   closure,
												 XtPointer   /* call_data */)
{
	XFE_PrefsPageGeneralColors *thePage = (XFE_PrefsPageGeneralColors *)closure;
	PrefsDataGeneralColors     *fep = thePage->getData();
	char                        buf[32];
    LO_Color                    color;
	char                       *orig_color;
	
	XtVaGetValues(fep->text_color_button, XmNuserData, &orig_color, NULL);
	if (orig_color) XtFree(orig_color);
	PREF_GetDefaultColorPref("browser.foreground_color", 
							 &color.red, &color.green, &color.blue);
    XP_SAFE_SPRINTF(buf, sizeof(buf), "#%02x%02x%02x", color.red, color.green, color.blue);
	XtVaSetValues(fep->text_color_button, 
				  XmNuserData, XtNewString(buf),
				  0);
	fe_SwatchSetColor(fep->text_color_button, &color);
	
	XtVaGetValues(fep->bg_color_button, XmNuserData, &orig_color, NULL);
	if (orig_color) XtFree(orig_color);
	PREF_GetDefaultColorPref("browser.background_color", 
							 &color.red, &color.green, &color.blue);
    XP_SAFE_SPRINTF(buf, sizeof(buf), "#%02x%02x%02x", color.red, color.green, color.blue);
	XtVaSetValues(fep->bg_color_button, 
				  XmNuserData, XtNewString(buf),
				  0);
	fe_SwatchSetColor(fep->bg_color_button, &color);
}

// ************************************************************************
// *************************   General/Advanced   *************************
// ************************************************************************

// Member:       XFE_PrefsPageGeneralAdvanced
// Description:  Constructor
// Inputs:
// Side effects: 

XFE_PrefsPageGeneralAdvanced::XFE_PrefsPageGeneralAdvanced(XFE_PrefsDialog *dialog)
	: XFE_PrefsPage(dialog),
	  m_prefsDataGeneralAdvanced(0)
{
  m_toolbar_needs_updating = FALSE;
}

// Member:       ~XFE_PrefsPageGeneralAdvanced
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsPageGeneralAdvanced::~XFE_PrefsPageGeneralAdvanced()
{
	delete m_prefsDataGeneralAdvanced;
}

// Member:       create
// Description:  Creates page for GeneralAdvanced
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralAdvanced::create()
{
	Widget            kids[100];
	Arg               av[50];
	int               ac;
	int               i;

	PrefsDataGeneralAdvanced *fep = NULL;

	fep = new PrefsDataGeneralAdvanced;
	memset(fep, 0, sizeof(PrefsDataGeneralAdvanced));
	m_prefsDataGeneralAdvanced = fep;

	fep->context = getContext();
	fep->prompt_dialog = getPrefsDialog()->getDialogChrome();

	Widget form;

	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	form = XmCreateForm (m_wPageForm, "advanced", av, ac);
	XtManageChild (form);
	m_wPage = fep->page = form;

	Widget frame1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame1 = XmCreateFrame (form, "advancedFrame", av, ac);

	Widget form1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form1 = XmCreateForm (frame1, "advancedBox", av, ac);

	// top portion

	Widget enable_js_top_attachment_widget = NULL;

	ac = 0;
	i = 0;

	kids[i++] = fep->show_image_toggle =
		XmCreateToggleButtonGadget(form1, "showImage", av, ac);

	enable_js_top_attachment_widget = fep->show_image_toggle;

#ifdef JAVA
	kids[i++] = fep->enable_java_toggle = 
		XmCreateToggleButtonGadget(form1, "enableJava", av, ac);

	enable_js_top_attachment_widget = fep->enable_java_toggle;
#endif /* JAVA */

	kids[i++] = fep->enable_js_toggle = 
		XmCreateToggleButtonGadget(form1, "enableJs", av, ac);

	kids[i++] = fep->enable_style_sheet_toggle = 
		XmCreateToggleButtonGadget(form1, "enableStyleSheet", av, ac);

	kids[i++] = fep->auto_install_toggle = 
		XmCreateToggleButtonGadget(form1, "autoInstall", av, ac);

	kids[i++] = fep->email_anonftp_toggle = 
		XmCreateToggleButtonGadget(form1, "emailAnonFtp", av, ac);

	XtVaSetValues(fep->show_image_toggle,
				  XmNindicatorType, XmN_OF_MANY,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  0);

#ifdef JAVA
	XtVaSetValues(fep->enable_java_toggle,
				  XmNindicatorType, XmN_OF_MANY,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, fep->show_image_toggle,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, fep->show_image_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  0);
#endif /* JAVA */

	XtVaSetValues(fep->enable_js_toggle,
				  XmNindicatorType, XmN_OF_MANY,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, enable_js_top_attachment_widget,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, fep->show_image_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  0);

	XtVaSetValues(fep->enable_style_sheet_toggle,
				  XmNindicatorType, XmN_OF_MANY,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, fep->enable_js_toggle,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, fep->show_image_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  0);

	XtVaSetValues(fep->auto_install_toggle,
				  XmNindicatorType, XmN_OF_MANY,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, fep->enable_style_sheet_toggle,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, fep->show_image_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  0);

	XtVaSetValues(fep->email_anonftp_toggle,
				  XmNindicatorType, XmN_OF_MANY,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, fep->auto_install_toggle,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, fep->show_image_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  0);

	XtManageChildren (kids, i);
	XtManageChild (form1);
	XtManageChild (frame1);
	
	// Cookies

	Widget frame2;
	Widget label2;
	Widget form2;

	ac = 0;
	XtSetArg (av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg (av[ac], XmNtopWidget, frame1); ac++;
	XtSetArg (av[ac], XmNtopOffset, 8); ac++;
	XtSetArg (av[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame2 = XmCreateFrame (form, "cookieFrame", av, ac);

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form2 = XmCreateForm (frame2, "cookieBox", av, ac);

	ac = 0;
	XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	label2 = XmCreateLabelGadget (frame2, "cookieBoxLabel", av, ac);

	ac = 0;
	i = 0;

	kids[i++] = fep->always_accept_cookie_toggle =
		XmCreateToggleButtonGadget(form2, "alwaysAcceptCookie", av, ac);

	kids[i++] = fep->no_foreign_cookie_toggle =
		XmCreateToggleButtonGadget(form2, "noForeignCookie", av, ac);

	kids[i++] = fep->never_accept_cookie_toggle =
		XmCreateToggleButtonGadget(form2, "neverAcceptCookie", av, ac);

	kids[i++] = fep->warn_cookie_toggle =
		XmCreateToggleButtonGadget(form2, "warnCookie", av, ac);

	XtVaSetValues(fep->always_accept_cookie_toggle,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  0);

	XtVaSetValues(fep->no_foreign_cookie_toggle,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, fep->always_accept_cookie_toggle,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, fep->always_accept_cookie_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  0);

	XtVaSetValues(fep->never_accept_cookie_toggle,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, fep->no_foreign_cookie_toggle,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, fep->always_accept_cookie_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  0);

	XtVaSetValues(fep->warn_cookie_toggle,
				  XmNindicatorType, XmN_OF_MANY,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, fep->never_accept_cookie_toggle,
				  XmNtopOffset, 8,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, fep->always_accept_cookie_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  0);

	XtManageChildren (kids, i);
	XtManageChild (label2);
	XtManageChild (form2);
	XtManageChild (frame2);

	// Add callbacks

	XtAddCallback(fep->always_accept_cookie_toggle, XmNvalueChangedCallback,
				  cb_toggleCookieState, fep);
	XtAddCallback(fep->no_foreign_cookie_toggle, XmNvalueChangedCallback,
				  cb_toggleCookieState, fep);
	XtAddCallback(fep->never_accept_cookie_toggle, XmNvalueChangedCallback,
				  cb_toggleCookieState, fep);

	setCreated(TRUE);
}

// Member:       init
// Description:  Initializes page for GeneralAdvanced
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralAdvanced::init()
{
	XP_ASSERT(m_prefsDataGeneralAdvanced);	

	PrefsDataGeneralAdvanced *fep = m_prefsDataGeneralAdvanced;
	XFE_GlobalPrefs          *prefs = &fe_globalPrefs;
    Boolean                  sensitive;

	XtVaSetValues(fep->show_image_toggle, 
                  XmNset, prefs->autoload_images_p, 
                  XmNsensitive, !PREF_PrefIsLocked("general.always_load_images"),
                  0);
	XtVaSetValues(fep->enable_style_sheet_toggle, 
                  XmNset, prefs->enable_style_sheet, 
                  XmNsensitive, !PREF_PrefIsLocked("browser.enable_style_sheets"),
                  0);

#ifdef JAVA
	XtVaSetValues(fep->enable_java_toggle, 
                  XmNset, prefs->enable_java, 
                  XmNsensitive, !PREF_PrefIsLocked("security.enable_java"),
                  0);
#endif /* JAVA */

	XtVaSetValues(fep->enable_js_toggle, 
                  XmNset, prefs->enable_javascript, 
                  XmNsensitive, !PREF_PrefIsLocked("javascript.enabled"),
                  0);

	XtVaSetValues(fep->auto_install_toggle, 
                  XmNset, prefs->auto_install, 
                  XmNsensitive, !PREF_PrefIsLocked("autoupdate.enabled"),
                  0);
	XtVaSetValues(fep->email_anonftp_toggle, 
                  XmNset, prefs->email_anonftp, 
                  XmNsensitive, !PREF_PrefIsLocked("security.email_as_ftp_password"),
                  0);

	// Cookie

    sensitive = !PREF_PrefIsLocked("network.cookie.cookieBehavior");
	XtVaSetValues(fep->always_accept_cookie_toggle, 
				  XmNset, prefs->accept_cookie == NET_Accept,
				  XmNsensitive, sensitive,
				  0);
	XtVaSetValues(fep->no_foreign_cookie_toggle, 
				  XmNset, prefs->accept_cookie == NET_DontAcceptForeign,
				  XmNsensitive, sensitive,
				  0);
	XtVaSetValues(fep->never_accept_cookie_toggle, 
				  XmNset, prefs->accept_cookie == NET_DontUse,
				  XmNsensitive, sensitive,
				  0);

    sensitive = !PREF_PrefIsLocked("network.cookie.warnAboutCookies");
	XtVaSetValues(fep->warn_cookie_toggle, 
				  XmNset, prefs->warn_accept_cookie,
				  XmNsensitive, sensitive,
				  0);

	setInitialized(TRUE);
}

// Member:       install
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralAdvanced::install()
{
	fe_installGeneralAdvanced();

	if (m_toolbar_needs_updating) {
		// Notify whoever is interested in updating toolbar appearance
		XFE_MozillaApp::theApp()->notifyInterested(XFE_MozillaApp::updateToolbarAppearance);
		m_toolbar_needs_updating = FALSE;
	}
}

// Member:       save
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralAdvanced::save()
{
	PrefsDataGeneralAdvanced *fep = m_prefsDataGeneralAdvanced;
	Boolean                  b;

	int old_autoload_images = fe_globalPrefs.autoload_images_p;

	XP_ASSERT(fep);

	XtVaGetValues(fep->show_image_toggle, XmNset, &b, 0);
	fe_globalPrefs.autoload_images_p = b;

	XtVaGetValues(fep->enable_style_sheet_toggle, XmNset, &b, 0);
	fe_globalPrefs.enable_style_sheet = b;

#ifdef JAVA
	XtVaGetValues(fep->enable_java_toggle, XmNset, &b, 0);
	fe_globalPrefs.enable_java = b;
#endif /* JAVA */

	XtVaGetValues(fep->enable_js_toggle, XmNset, &b, 0);
	fe_globalPrefs.enable_javascript = b;

	XtVaGetValues(fep->auto_install_toggle, XmNset, &b, 0);
	fe_globalPrefs.auto_install = b;

	XtVaGetValues(fep->email_anonftp_toggle, XmNset, &b, 0);
	fe_globalPrefs.email_anonftp = b;

	// Cookies

	XtVaGetValues(fep->always_accept_cookie_toggle, XmNset, &b, 0);
	if (b) fe_globalPrefs.accept_cookie = NET_Accept;

	XtVaGetValues(fep->no_foreign_cookie_toggle, XmNset, &b, 0);
	if (b) fe_globalPrefs.accept_cookie = NET_DontAcceptForeign;

	XtVaGetValues(fep->never_accept_cookie_toggle, XmNset, &b, 0);

	if (b) fe_globalPrefs.accept_cookie = NET_DontUse;

	XtVaGetValues(fep->warn_cookie_toggle, XmNset, &b, 0);
	fe_globalPrefs.warn_accept_cookie = b;

	if (old_autoload_images != fe_globalPrefs.autoload_images_p) {
      m_toolbar_needs_updating = TRUE;
    }



	// Install preferences

	install();
}

// Member:       getData
// Description:  
// Inputs:
// Side effects: 

PrefsDataGeneralAdvanced *XFE_PrefsPageGeneralAdvanced::getData()
{
	return m_prefsDataGeneralAdvanced;
}

// Member:       cb_toggleCookieState
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralAdvanced::cb_toggleCookieState(Widget    w,
														XtPointer closure,
														XtPointer callData)
{
	XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)callData;
	PrefsDataGeneralAdvanced     *fep = (PrefsDataGeneralAdvanced *)closure;

	if (! cb->set) {
		XtVaSetValues(w, XmNset, True, 0);
	}
	else if (w == fep->always_accept_cookie_toggle) {
		XtVaSetValues(fep->no_foreign_cookie_toggle, XmNset, False, 0);
		XtVaSetValues(fep->never_accept_cookie_toggle, XmNset, False, 0);
	}
	else if (w == fep->no_foreign_cookie_toggle) {
		XtVaSetValues(fep->always_accept_cookie_toggle, XmNset, False, 0);
		XtVaSetValues(fep->never_accept_cookie_toggle, XmNset, False, 0);
	}
	else if (w == fep->never_accept_cookie_toggle) {
		XtVaSetValues(fep->always_accept_cookie_toggle, XmNset, False, 0);
		XtVaSetValues(fep->no_foreign_cookie_toggle, XmNset, False, 0);
	}
	else
		abort();	
}

// ************************************************************************
// **************************    General/Appl   ***************************
// ************************************************************************

// Member:       XFE_PrefsPageGeneralAppl
// Description:  Constructor
// Inputs:
// Side effects: 

XFE_PrefsPageGeneralAppl::XFE_PrefsPageGeneralAppl(XFE_PrefsDialog *dialog)
	: XFE_PrefsPage(dialog),
	  m_prefsDataGeneralAppl(0)
{
}

// Member:       ~XFE_PrefsPageGeneralAppl
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsPageGeneralAppl::~XFE_PrefsPageGeneralAppl()
{
	delete m_prefsDataGeneralAppl;
}

// Member:       create
// Description:  Creates page for GeneralAppl
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralAppl::create()
{
	Widget            kids[100];
	Arg               av[50];
	int               ac;
	int               i;

	PrefsDataGeneralAppl *fep = NULL;

	fep = new PrefsDataGeneralAppl;
	memset(fep, 0, sizeof(PrefsDataGeneralAppl));
	m_prefsDataGeneralAppl = fep;

	fep->context = getContext();
	fep->prompt_dialog = getPrefsDialog()->getDialogChrome();

	Widget form;

	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	form = XmCreateForm (m_wPageForm, "appl", av, ac);
	XtManageChild (form);
	m_wPage = fep->page = form;

	Widget frame1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame1 = XmCreateFrame (form, "applFrame", av, ac);

	Widget form1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form1 = XmCreateForm (frame1, "applBox", av, ac);

	// Application list

	Widget helpers_list;

	ac = 0;
	i = 0;
	XtSetArg(av[ac], XmNselectionPolicy, XmSELECT_BROWSE_ROW); ac++;
	XtSetArg(av[ac], XmNhorizontalSizePolicy, XmCONSTANT); ac++;
	XtSetArg(av[ac], XmNallowColumnResize, True); ac++;
	XtSetArg(av[ac], XmNheight, 250); ac++;
	kids[i++] = helpers_list = XmLCreateGrid(form1, "helperApp", av, ac);
	fep->helpers_list = helpers_list;
	XtVaSetValues(helpers_list, XmNcolumnSizePolicy, XmVARIABLE, NULL);
	XtVaSetValues(helpers_list, XmNlayoutFrozen, True, NULL);

	XmLGridAddColumns(helpers_list, XmCONTENT, -1, 2);

	// First Column
	XtVaSetValues(helpers_list, 
				  XmNrowType, XmHEADING,
				  XmNcolumn, 0,
				  XmNcellDefaults, True,
				  XmNcolumnWidth,  DEFAULT_COLUMN_WIDTH,
				  XmNcellType, XmSTRING_CELL,
				  XmNcellEditable, True,
				  XmNcellAlignment, XmALIGNMENT_LEFT,
				  NULL);
 
	// Second Column
	XtVaSetValues(helpers_list, 
				  XmNrowType, XmHEADING,
				  XmNcolumn, 1,
				  XmNcellDefaults, True,
				  XmNcellType, XmSTRING_CELL,
				  XmNcellEditable, True,
				  XmNcolumnWidth, MAX_COLUMN_WIDTH,
				  XmNcellAlignment, XmALIGNMENT_LEFT, 
				  NULL);
 
	XmLGridAddRows(helpers_list, XmHEADING, -1, 1);
	XmLGridSetStrings(helpers_list, XP_GetString(XFE_HELPERS_LIST_HEADER));
 
	// Get the font from *XmLGrid*fontList at this point.  - XXX dp 

	XtVaSetValues(helpers_list, 
				  XmNcellDefaults, True,
				  XmNcellType, XmSTRING_CELL,
				  XmNcellEditable, False,
				  XmNcellLeftBorderType, XmBORDER_NONE,
				  XmNcellRightBorderType, XmBORDER_NONE,
				  XmNcellTopBorderType, XmBORDER_NONE,
				  XmNcellBottomBorderType, XmBORDER_NONE,
				  XmNcellAlignment, XmALIGNMENT_LEFT, 
				  NULL);

	XtVaSetValues(helpers_list,
				  XmNlayoutFrozen, False,
				  NULL);

	// New, Edit, Delete buttons
 
	ac = 0;

	Widget new_button;
	Widget edit_button;
	Widget delete_button;
	Widget folder_label;
	Widget folder_text;
	Widget browse_button;
	
	kids[i++] = new_button =
		XmCreatePushButtonGadget(form1, "newButton", av, ac);

	kids[i++] = edit_button =
		XmCreatePushButtonGadget(form1, "editButton", av, ac);

	kids[i++] = delete_button =
		XmCreatePushButtonGadget(form1, "deleteButton", av, ac);

	// Downloads folder

	kids[i++] = folder_label = 
		XmCreateLabelGadget(form1, "folderLabel", av, ac);

	ac = 0;
	XtSetArg(av[ac], XmNcolumns, 25); ac++;
	kids[i++] = folder_text = 
		fe_CreateTextField(form1, "folderText", av, ac);

	ac = 0;
	kids[i++] = browse_button =
		XmCreatePushButtonGadget(form1, "browseButton", av, ac);

	fep->folder_text = folder_text;
	fep->browse_button = browse_button;
	
	// Attachments

	Dimension width = XfeWidth(folder_label) +
		XfeWidth(folder_text) +
		XfeWidth(browse_button);

	XtVaSetValues(helpers_list,
				  XmNwidth, width,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNtopAttachment, XmATTACH_FORM,
				  NULL);
 
	int labels_width;
	int edit_left_offset;

	labels_width = XfeVaGetWidestWidget(new_button,
										edit_button,
										delete_button,
										NULL);
	edit_left_offset = -(labels_width/2);
	
	XtVaSetValues(edit_button,
				  XmNwidth, labels_width,
				  XmNleftAttachment, XmATTACH_POSITION,
				  XmNleftPosition, 50,
				  XmNleftOffset, edit_left_offset,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, helpers_list,
				  XmNtopOffset, 8,
				  NULL);

	XtVaSetValues(new_button,
				  XmNwidth, labels_width,
				  XmNleftAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_WIDGET,
				  XmNrightWidget, edit_button,
				  XmNrightOffset, 8,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, edit_button,
				  NULL);

	XtVaSetValues(delete_button,
				  XmNwidth, labels_width,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, edit_button,
				  XmNleftOffset, 8,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, edit_button,
				  NULL);

	int labels_height;

	labels_height = XfeVaGetTallestWidget(folder_label,
										  folder_text,
										  browse_button,
										  NULL);

	XtVaSetValues(folder_label,
				  XmNheight, labels_height,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, new_button,
				  XmNtopOffset, 8,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(folder_text,
				  XmNheight, labels_height,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, folder_label,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, folder_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(browse_button,
				  XmNheight, labels_height,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, folder_text,
				  XmNleftOffset, 4,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, folder_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	// Add callbacks

	XtAddCallback(new_button, XmNactivateCallback, cb_new, this);
	XtAddCallback(edit_button, XmNactivateCallback, cb_edit, this);
	XtAddCallback(delete_button, XmNactivateCallback, cb_delete, this);
	XtAddCallback(browse_button, XmNactivateCallback, cb_browse, this);

	XtManageChildren(kids, i);
	XtManageChild(form1);
	XtManageChild(frame1);

	// Set up Column Width 

	XmFontList fontList;
	short      avgwidth;
	short      avgheight;

	XtVaGetValues(helpers_list,
				  XmNfontList, &fontList,
				  XmNwidth, &width, 
				  NULL);
	XmLFontListGetDimensions(fontList, &avgwidth, &avgheight, TRUE);

	setCreated(TRUE);
}

// Member:       init
// Description:  Initializes page for GeneralAppl
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralAppl::init()
{
	XFE_GlobalPrefs      *prefs = &fe_globalPrefs;
	XP_ASSERT(m_prefsDataGeneralAppl);
	PrefsDataGeneralAppl *fep = m_prefsDataGeneralAppl;
    Boolean sensitive;
	
	// Before making the helpers page see if we want to reload the
	// mailcap and mimetype files if they changed on disk.

	XP_Bool needs_reload = FALSE;

	/* Mimetypes file */

	if (fe_globalData.privateMimetypeFileModifiedTime != 0 &&
		fe_isFileExist(fe_globalPrefs.private_mime_types_file) &&
		fe_isFileChanged(fe_globalPrefs.private_mime_types_file,
						 fe_globalData.privateMimetypeFileModifiedTime, NULL)) {
		char *msg = PR_smprintf(XP_GetString(XFE_PRIVATE_RELOADED_MIMETYPE),
								fe_globalPrefs.private_mime_types_file);
		if (msg) {
			fe_Alert_2(getPrefsDialog()->getDialogChrome(), msg);
			XP_FREE(msg);
		}
		needs_reload = TRUE;
	}

	// The mailcap File 

	if (fe_globalData.privateMailcapFileModifiedTime != 0 &&
		fe_isFileExist(fe_globalPrefs.private_mailcap_file) &&
		fe_isFileChanged(fe_globalPrefs.private_mailcap_file,
						 fe_globalData.privateMailcapFileModifiedTime, NULL)) {
		char *msg = PR_smprintf(XP_GetString(XFE_PRIVATE_RELOADED_MAILCAP),
								fe_globalPrefs.private_mailcap_file);
		if (msg) {
			fe_Alert_2(getPrefsDialog()->getDialogChrome(), msg);
			XP_FREE(msg);
		}
		needs_reload = TRUE;
	}

	if (needs_reload) {
		NET_CleanupMailCapList(NULL);
		NET_CleanupFileFormat(NULL);
		fe_RegisterConverters();
	}

	// Add static applications

	xfe_prefsDialogAppl_build_static_list(fep);

	// Parse the .mime.types and .mailcap files.
	xfe_prefsDialogAppl_build_mime_list(fep);

	// browser download directory
	fe_SetTextField(fep->folder_text, prefs->tmp_dir);

    sensitive = !PREF_PrefIsLocked("browser.download_directory");
    XtSetSensitive(fep->folder_text, sensitive);
    XtSetSensitive(fep->browse_button, sensitive);

	setInitialized(TRUE);
}

// Member:       unmap
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralAppl::unmap()
{
	PrefsDataGeneralAppl *fep = m_prefsDataGeneralAppl;
	XP_ASSERT(fep);

	if (fep->helpers_changed) {
		/* Throw away the current information about mime types */
		NET_CleanupFileFormat(NULL);
		fe_globalData.privateMimetypeFileModifiedTime = 0;
		NET_CleanupMailCapList(NULL);
		fe_globalData.privateMailcapFileModifiedTime = 0;
 
		/* Re-load the default settings */
		fe_RegisterConverters();
	}

	XFE_PrefsPage::unmap();
}

// Member:       install
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralAppl::install()
{
	fe_installBrowserAppl();

#ifdef NEW_DECODERS
	PrefsDataGeneralAppl *fep = m_prefsDataGeneralAppl;
	XP_ASSERT(fep);

	if (fep->helpers_changed) {
		XP_Bool reload = FALSE;

		/* Save the current preferences out */
		if (fe_globalData.privateMimetypeFileModifiedTime != 0 &&
			fe_isFileExist(fe_globalPrefs.private_mime_types_file) &&
			fe_isFileChanged(fe_globalPrefs.private_mime_types_file,
							 fe_globalData.privateMimetypeFileModifiedTime, 0)) {
			/* Ask users about overwriting or reloading */
			char *msg = PR_smprintf(XP_GetString(XFE_PRIVATE_MIMETYPE_RELOAD),
									fe_globalPrefs.private_mime_types_file);
			if (msg) {
				reload = XFE_Confirm (fep->context, msg);
				XP_FREE(msg);
			}
		}
		if (reload)
			NET_CleanupFileFormat(NULL);
		else
			NET_CleanupFileFormat(fe_globalPrefs.private_mime_types_file);

		reload = FALSE;
		if (fe_globalData.privateMailcapFileModifiedTime != 0 &&
			fe_isFileExist(fe_globalPrefs.private_mailcap_file) &&
			fe_isFileChanged(fe_globalPrefs.private_mailcap_file,
							 fe_globalData.privateMailcapFileModifiedTime, NULL)) {
			/* Ask users about overwriting or reloading */
			char *msg = PR_smprintf(XP_GetString(XFE_PRIVATE_MAILCAP_RELOAD),
									fe_globalPrefs.private_mailcap_file);
			if (msg) {
				reload = XFE_Confirm(fep->context, msg);
				XP_FREE(msg);
			}
		}
		if (reload)
			NET_CleanupMailCapList(NULL);
		else
			NET_CleanupMailCapList(fe_globalPrefs.private_mailcap_file);

		/* Load the preferences file again */
		fe_RegisterConverters ();
	}
#endif /* NEW_DECODERS */
}

// Member:       markGeneralApplModified
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralAppl::setModified(Boolean flag)
{
	XP_ASSERT(m_prefsDataGeneralAppl);
	PrefsDataGeneralAppl *fep = m_prefsDataGeneralAppl;
	
	fep->helpers_changed = flag;
}

// Member:       save
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralAppl::save()
{
	XP_ASSERT(m_prefsDataGeneralAppl);
	PrefsDataGeneralAppl *fep = m_prefsDataGeneralAppl;

	XP_FREEIF(fe_globalPrefs.tmp_dir);
	char *s = fe_GetTextField(fep->folder_text);
    fe_globalPrefs.tmp_dir = s ? s : XP_STRDUP("");

	// Install preferences

	install();
}

// Member:       getData
// Description:  
// Inputs:
// Side effects: 

PrefsDataGeneralAppl *XFE_PrefsPageGeneralAppl::getData()
{
	return m_prefsDataGeneralAppl;
}

// Member:       cb_new
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralAppl::cb_new(Widget    /* w */,
									  XtPointer closure,
									  XtPointer /* callData */)
{
	XFE_PrefsPageGeneralAppl *thePage = (XFE_PrefsPageGeneralAppl *)closure;
	XFE_PrefsDialog          *theDialog = thePage->getPrefsDialog();
	Widget                    mainw = theDialog->getBaseWidget();

	// while (!XtIsWMShell(mainw) && (XtParent(mainw)!=0))
	//		mainw = XtParent(mainw);

	// Instantiate an applications new dialog

	XFE_PrefsApplEditDialog *theNewDialog = 0;

	if ((theNewDialog =
		 new XFE_PrefsApplEditDialog(theDialog, mainw, "prefsApplEdit", thePage)) == 0) {
	    fe_perror(thePage->getContext(), XP_GetString(XFE_OUT_OF_MEMORY_URL));
	    return;
	}

	// Initialize and pop up the dialog

	theNewDialog->initPage(TRUE, 0);
	theNewDialog->show();
}

// Member:       cb_edit
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralAppl::cb_edit(Widget    w,
									   XtPointer closure,
									   XtPointer /* callData */)
{
	XFE_PrefsPageGeneralAppl *thePage = (XFE_PrefsPageGeneralAppl *)closure;
	XFE_PrefsDialog          *theDialog = thePage->getPrefsDialog();
	PrefsDataGeneralAppl     *fep = thePage->getData();
	Widget                    mainw = theDialog->getBaseWidget();

	// Instantiate an applications new dialog

	XFE_PrefsApplEditDialog *theEditDialog = 0;
	PrefsDataGeneralAppl    *prefsDataGeneralAppl;

	XtVaGetValues(w, XmNuserData, &prefsDataGeneralAppl, NULL);

	if ((theEditDialog =
		 new XFE_PrefsApplEditDialog(theDialog, mainw, "prefsApplEdit", thePage)) == 0) {
	    fe_perror(thePage->getContext(), XP_GetString(XFE_OUT_OF_MEMORY_URL));
	    return;
	}

	// Initialize and pop up the dialog

	int    selectedCount;
	int    pos[1]; /* we are browse select, so only 1 row is selected */

	selectedCount = XmLGridGetSelectedRowCount(fep->helpers_list);
	if ( selectedCount != 1 )
		return;

	XmLGridGetSelectedRows(fep->helpers_list, pos, selectedCount);

	theEditDialog->initPage(FALSE, pos[0]);
	theEditDialog->show();
}

// Member:       cb_delete
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralAppl::cb_delete(Widget    /* w */,
										 XtPointer closure,
										 XtPointer /* callData */)
{
	XFE_PrefsPageGeneralAppl *thePage = (XFE_PrefsPageGeneralAppl *)closure;
	PrefsDataGeneralAppl     *fep = thePage->getData();
	char                     *old_str = 0;
	char                     *src_str = 0;
	NET_cdataStruct          *cd = NULL;
	NET_mdataStruct          *old_md= NULL;
	int                       selectedCount;
	int                       pos[1]; /* we are browse select, so only 1 row is selected */
 
	selectedCount = XmLGridGetSelectedRowCount(fep->helpers_list);
 	if ( selectedCount != 1 )
		return;
 
	XmLGridGetSelectedRows(fep->helpers_list, pos, selectedCount);

	int position = pos[0];
	if (position < fep->static_apps_count) {
		/* cannot remove static applications */
		FE_Alert(thePage->getContext(), XP_GetString(XFE_HELPERS_CANNOT_DEL_STATIC_APPS));
		return;
	}
  
	/* adjust position */
	position = position - fep->static_apps_count;
	/* pos in the list always start from 0 */
	cd = xfe_prefsDialogAppl_get_mime_data_at_pos(position);
	if ( fep ) {
		if (XFE_Confirm (fep->context, fe_globalData.helper_app_delete_message)) {
			thePage->setModified(TRUE);
			old_md = xfe_prefsDialogAppl_get_mailcap_from_type(cd->ci.type); 

			if ( old_md ) {
				/* If there is a mailcap entry, 
				   then update to reflect delete */
				old_str = old_md->src_string;
				old_md->src_string = 0;
			}
			if ( !old_str || !strlen(old_str)) {
				src_str = 
					xfe_prefsDialogAppl_construct_new_mailcap_string(old_md, 
														 cd->ci.type, 
														 NULL, 
														 NET_COMMAND_DELETED);
			}
			else {
				src_str = xfe_prefsDialogAppl_deleteCommand(old_str);
				XP_FREE(old_str);

				old_str = src_str;
				src_str = xfe_prefsDialogAppl_updateKey(src_str, 
													NET_MOZILLA_FLAGS, 
													NET_COMMAND_DELETED, 1 );
				XP_FREE(old_str);
			}

			if ( old_md ) {
				if (old_md->src_string ) XP_FREE(old_md->src_string);
				old_md->src_string = 0;

				if ( src_str && *src_str )
					old_md->src_string = src_str;

				if (old_md->xmode) XP_FREE(old_md->xmode);
				old_md->xmode = strdup(NET_COMMAND_DELETED);
				old_md->is_modified = 1;
			}
			else {
				xfe_prefsDialogAppl_add_new_mailcap_data(cd->ci.type, 
														 src_str, NULL,  
														 NET_COMMAND_DELETED, 
														 1);
				XP_FREE(src_str);
			}

			/* delete the row at pos[0], because position is now adjusted
			 * with static applications in mind...
			 */
			XmLGridDeleteRows(fep->helpers_list, XmCONTENT, pos[0], 1);

			if (cd->is_new) {
				NET_cdataRemove(cd);
				if ( old_md )
					NET_mdataRemove(old_md);
			}
		}
	}
}

// Member:       cb_browse
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralAppl::cb_browse(Widget    /* w */,
										 XtPointer closure,
										 XtPointer /* callData */)
{
	XFE_PrefsPageGeneralAppl *thePage = (XFE_PrefsPageGeneralAppl *)closure;
	XFE_PrefsDialog          *theDialog = thePage->getPrefsDialog();
	PrefsDataGeneralAppl     *fep = thePage->getData();

	fe_browse_file_of_text(theDialog->getContext(), fep->folder_text, True);
}

// ************************************************************************
// *************************     General/Cache    *************************
// ************************************************************************

// Member:       XFE_PrefsPageGeneralCache
// Description:  Constructor
// Inputs:
// Side effects: 

XFE_PrefsPageGeneralCache::XFE_PrefsPageGeneralCache(XFE_PrefsDialog *dialog)
	: XFE_PrefsPage(dialog),
	  m_prefsDataGeneralCache(0)
{
}

// Member:       ~XFE_PrefsPageGeneralCache
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsPageGeneralCache::~XFE_PrefsPageGeneralCache()
{
	delete m_prefsDataGeneralCache;
}

// Member:       create
// Description:  Creates page for GeneralCache
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralCache::create()
{
	Widget            kids[100];
	Arg               av[50];
	int               ac;
	int               i;

	PrefsDataGeneralCache *fep = NULL;

	fep = new PrefsDataGeneralCache;
	memset(fep, 0, sizeof(PrefsDataGeneralCache));
	m_prefsDataGeneralCache = fep;

	fep->context = getContext();
	fep->prompt_dialog = getPrefsDialog()->getDialogChrome();

	Widget form;

	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	form = XmCreateForm (m_wPageForm, "cache", av, ac);
	XtManageChild (form);
	m_wPage = fep->page = form;

	Widget frame1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame1 = XmCreateFrame (form, "cacheFrame", av, ac);

	Widget form1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form1 = XmCreateForm (frame1, "cacheBox", av, ac);

	Widget cacheLabel;

	cacheLabel = XtVaCreateManagedWidget("cacheLabel",
										 xmLabelGadgetClass, form1,
										 XmNalignment, XmALIGNMENT_BEGINNING,
										 XmNtopAttachment, XmATTACH_FORM,
										 XmNleftAttachment, XmATTACH_FORM,
										 XmNrightAttachment, XmATTACH_NONE,
										 XmNbottomAttachment, XmATTACH_NONE,
										 NULL);

	Widget cache_dir_label;
	Widget cache_dir_text;
	Widget browse_button;
	Widget mem_label;
	Widget mem_text;
	Widget mem_k;
	Widget disk_label;
	Widget disk_text;
	Widget disk_k;
	Widget clear_disk_button;
	Widget clear_mem_button;

	ac = 0;
	i = 0;
	
	kids [i++] = cache_dir_label = XmCreateLabelGadget (form1, "cacheDirLabel", av, ac);
	kids [i++] = cache_dir_text = fe_CreateTextField (form1, "cacheDirText", av, ac);

	kids [i++] = mem_label = XmCreateLabelGadget (form1, "memoryLabel", av, ac);
	kids [i++] = mem_text = fe_CreateTextField (form1, "memoryText", av, ac);
	kids [i++] = mem_k = XmCreateLabelGadget (form1, "memoryK", av, ac);

	kids [i++] = disk_label = XmCreateLabelGadget (form1, "diskLabel", av, ac);
	kids [i++] = disk_text = fe_CreateTextField (form1, "diskText", av, ac);
	kids [i++] = disk_k = XmCreateLabelGadget (form1, "diskK", av, ac);

	kids[i++] = browse_button =	XmCreatePushButtonGadget(form1, "browse", av, ac);
	kids[i++] = clear_mem_button =	XmCreatePushButtonGadget(form1, "clearMem", av, ac);
	kids[i++] = clear_disk_button =	XmCreatePushButtonGadget(form1, "clearDisk", av, ac);

	fep->cache_dir_text = cache_dir_text;
	fep->browse_button = browse_button;
	fep->mem_text = mem_text;
	fep->disk_text = disk_text;

	int labels_width;
	labels_width = XfeVaGetWidestWidget(cache_dir_label,
										mem_label,
										disk_label,
										NULL);
	int labels_height;
	labels_height = XfeVaGetTallestWidget(cache_dir_label,
										  cache_dir_text,
										  browse_button,
										  NULL);
	int button_width;
	button_width = XfeVaGetWidestWidget(clear_mem_button,
										clear_disk_button,
										NULL);

	XtVaSetValues (disk_label,
				   XmNheight, labels_height,
				   RIGHT_JUSTIFY_VA_ARGS(disk_label,labels_width),
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, cacheLabel,
				   XmNbottomAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (disk_text,
				   XmNcolumns, 6,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, disk_label,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, cache_dir_text,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (disk_k,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, disk_text,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_WIDGET,
				   XmNleftWidget, disk_text,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (clear_disk_button,
				   XmNheight, labels_height,
				   XmNwidth, button_width,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, disk_k,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_NONE,
				   XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNrightWidget, browse_button,
				   0);

	XtVaSetValues (mem_label,
				   XmNheight, labels_height,
				   RIGHT_JUSTIFY_VA_ARGS(mem_label,labels_width),
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, disk_label,
				   XmNbottomAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (mem_text,
				   XmNcolumns, 6,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, mem_label,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, cache_dir_text,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (mem_k,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, mem_text,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_WIDGET,
				   XmNleftWidget, mem_text,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (clear_mem_button,
				   XmNheight, labels_height,
				   XmNwidth, button_width,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, mem_k,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_NONE,
				   XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNrightWidget, browse_button,
				   0);

	XtVaSetValues (cache_dir_label,
				   XmNheight, labels_height,
				   RIGHT_JUSTIFY_VA_ARGS(cache_dir_label, labels_width),
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, mem_label,
				   XmNbottomAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (cache_dir_text,
				   XmNcolumns, 25,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, cache_dir_label,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNleftOffset, labels_width,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (browse_button,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, cache_dir_label,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_WIDGET,
				   XmNleftWidget, cache_dir_text,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtManageChildren (kids, i);
	
	// Verify

	Widget verifyLabel;

	verifyLabel = XtVaCreateManagedWidget("verifyLabel",
										  xmLabelGadgetClass, form1,
										  XmNtopAttachment, XmATTACH_WIDGET,
										  XmNtopWidget, cache_dir_label,
										  XmNtopOffset, 8,
										  XmNleftAttachment, XmATTACH_FORM,
										  XmNrightAttachment, XmATTACH_NONE,
										  XmNbottomAttachment, XmATTACH_NONE,
										  NULL);

	Widget verifyRc;

	verifyRc = XtVaCreateManagedWidget("verifyRc",
									   xmRowColumnWidgetClass, form1,
									   XmNradioBehavior, TRUE,
									   XmNtopAttachment, XmATTACH_WIDGET,
									   XmNtopWidget, verifyLabel,
									   XmNleftAttachment, XmATTACH_FORM,
									   XmNleftOffset, 16,
									   XmNrightAttachment, XmATTACH_NONE,
									   XmNbottomAttachment, XmATTACH_NONE,
									   NULL);

	i = 0;

	ac = 0;
	XtSetArg (av [ac], XmNindicatorType, XmONE_OF_MANY); ac++;
	kids[i++] = fep->every_toggle =
		XmCreateToggleButtonGadget(verifyRc, "every", av, ac);

	ac = 0;
	XtSetArg (av [ac], XmNindicatorType, XmONE_OF_MANY); ac++;
	kids[i++] = fep->once_toggle =
		XmCreateToggleButtonGadget(verifyRc, "once", av, ac);

	ac = 0;
	XtSetArg (av [ac], XmNindicatorType, XmONE_OF_MANY); ac++;
	kids[i++] = fep->never_toggle =
		XmCreateToggleButtonGadget(verifyRc, "never", av, ac);

	XtManageChildren (kids, i);

	i = 0;
	kids[i++] = verifyRc;
	XtManageChildren (kids, i);

	XtAddCallback(browse_button, XmNactivateCallback, cb_browse, this);
	XtAddCallback(clear_disk_button, XmNactivateCallback, cb_clearDisk, this);
	XtAddCallback(clear_mem_button, XmNactivateCallback, cb_clearMem, this);

	XtManageChild(form1);
	XtManageChild(frame1);

	setCreated(TRUE);
}

// Member:       init
// Description:  Initializes page for GeneralCache
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralCache::init()
{
	XP_ASSERT(m_prefsDataGeneralCache);

	PrefsDataGeneralCache *fep = m_prefsDataGeneralCache;
	XFE_GlobalPrefs       *prefs = &fe_globalPrefs;
    Boolean               sensitive;
	
	// Cache directory, Memory cache, disk cache

	fe_SetTextField(fep->cache_dir_text, prefs->cache_dir);

    sensitive = !PREF_PrefIsLocked("browser.cache.directory");
    XtSetSensitive(fep->cache_dir_text, sensitive);
    XtSetSensitive(fep->browse_button, sensitive);

	char buf [255];

	PR_snprintf(buf, sizeof(buf), "%d", prefs->memory_cache_size);
	XtVaSetValues(fep->mem_text, XmNvalue, buf, 0);
    sensitive = !PREF_PrefIsLocked("browser.cache.memory_cache_size");
	XtSetSensitive(fep->mem_text, sensitive);

	PR_snprintf(buf, sizeof(buf), "%d", prefs->disk_cache_size);
	XtVaSetValues(fep->disk_text, XmNvalue, buf, 0);
    sensitive = !PREF_PrefIsLocked("browser.cache.disk_cache_size");
	XtSetSensitive(fep->disk_text, sensitive);

	// Verify cache

	XtVaSetValues(fep->once_toggle, XmNset, prefs->verify_documents == 0, 0);
	XtVaSetValues(fep->every_toggle, XmNset, prefs->verify_documents == 1, 0);
	XtVaSetValues(fep->never_toggle, XmNset, prefs->verify_documents == 2, 0);

    // If pref is locked, grey it out.
    sensitive = !PREF_PrefIsLocked("browser.cache.check_doc_frequency");
	XtSetSensitive(fep->once_toggle, sensitive);
	XtSetSensitive(fep->every_toggle, sensitive);
	XtSetSensitive(fep->never_toggle, sensitive);

	setInitialized(TRUE);
}

// Member:       install
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralCache::install()
{
	fe_installGeneralCache();
}

// Member:       save
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralCache::save()
{
	PrefsDataGeneralCache *fep = m_prefsDataGeneralCache;
	Boolean                b;

	XP_ASSERT(fep);

    char *text;
	char  dummy;
    int   size = 0;
	char *s = 0;

	XP_FREEIF(fe_globalPrefs.cache_dir);
	s = fe_GetTextField(fep->cache_dir_text);
    fe_globalPrefs.cache_dir = s ? s : XP_STRDUP("");

	// Memory cache

	text = 0;
    XtVaGetValues(fep->mem_text, XmNvalue, &text, 0);
    if (1 == sscanf (text, " %d %c", &size, &dummy) &&
		size >= 0)
		fe_globalPrefs.memory_cache_size = size;
	if (text) XtFree(text);

	// Disk cache

	text = 0;
    XtVaGetValues(fep->disk_text, XmNvalue, &text, 0);
    if (1 == sscanf (text, " %d %c", &size, &dummy) &&
		size >= 0)
		fe_globalPrefs.disk_cache_size = size;
	if (text) XtFree(text);

	// Verify document in cache

	XtVaGetValues(fep->once_toggle, XmNset, &b, 0);
	if (b) fe_globalPrefs.verify_documents = 0;
	XtVaGetValues(fep->every_toggle, XmNset, &b, 0);
	if (b) fe_globalPrefs.verify_documents = 1;
	XtVaGetValues(fep->never_toggle, XmNset, &b, 0);
	if (b) fe_globalPrefs.verify_documents = 2;
  
	// Install preferences

	install();
}

// Member:       getData
// Description:  
// Inputs:
// Side effects: 

PrefsDataGeneralCache *XFE_PrefsPageGeneralCache::getData()
{
	return m_prefsDataGeneralCache;
}

// Member:       cb_browse
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralCache::cb_browse(Widget    /* w */,
										  XtPointer closure,
										  XtPointer /* callData */)
{
	XFE_PrefsPageGeneralCache    *thePage = (XFE_PrefsPageGeneralCache *)closure;
	XFE_PrefsDialog              *theDialog = thePage->getPrefsDialog();
	PrefsDataGeneralCache        *fep = thePage->getData();

	fe_browse_file_of_text(theDialog->getContext(), fep->cache_dir_text, True);
}

// Member:       cb_browse
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralCache::cb_clearDisk(Widget    /* w */,
											 XtPointer closure,
											 XtPointer /* callData */)
{
	XFE_PrefsPageGeneralCache    *thePage = (XFE_PrefsPageGeneralCache *)closure;
	XFE_PrefsDialog              *theDialog = thePage->getPrefsDialog();
	MWContext                    *context = theDialog->getContext();
	int32                         size;

	if (XFE_Confirm (context, fe_globalData.clear_disk_cache_message))	{
		PREF_GetIntPref("browser.cache.disk_cache_size", &size);
		PREF_SetIntPref("browser.cache.disk_cache_size", 0);
		PREF_SetIntPref("browser.cache.disk_cache_size", size);
    }
}

// Member:       cb_clearMem
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralCache::cb_clearMem(Widget    /* w */,
											XtPointer closure,
											XtPointer /* callData */)
{
	XFE_PrefsPageGeneralCache    *thePage = (XFE_PrefsPageGeneralCache *)closure;
	XFE_PrefsDialog              *theDialog = thePage->getPrefsDialog();
	MWContext                    *context = theDialog->getContext();
	int32                         size;

	if (XFE_Confirm (context, fe_globalData.clear_mem_cache_message)) {
		PREF_GetIntPref("browser.cache.memory_cache_size", &size);
		PREF_SetIntPref("browser.cache.memory_cache_size", 0);
		PREF_SetIntPref("browser.cache.memory_cache_size", size);
    }
}

// ************************************************************************
// *************************    General/Proxies   *************************
// ************************************************************************

// Member:       XFE_PrefsPageGeneralProxies
// Description:  Constructor
// Inputs:
// Side effects: 

XFE_PrefsPageGeneralProxies::XFE_PrefsPageGeneralProxies(XFE_PrefsDialog *dialog)
	: XFE_PrefsPage(dialog),
	  m_prefsDataGeneralProxies(0)
{
}

// Member:       ~XFE_PrefsPageGeneralProxies
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsPageGeneralProxies::~XFE_PrefsPageGeneralProxies()
{
	delete m_prefsDataGeneralProxies;
}

// Member:       create
// Description:  Creates page for GeneralProxies
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralProxies::create()
{
	Widget            kids[100];
	Arg               av[50];
	int               ac;
	int               i;

	PrefsDataGeneralProxies *fep = NULL;

	fep = new PrefsDataGeneralProxies;
	memset(fep, 0, sizeof(PrefsDataGeneralProxies));
	m_prefsDataGeneralProxies = fep;

	fep->context = getContext();
	fep->prompt_dialog = getPrefsDialog()->getDialogChrome();

	Widget form;

	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	form = XmCreateForm (m_wPageForm, "proxies", av, ac);
	XtManageChild (form);
	m_wPage = fep->page = form;

	Widget frame1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame1 = XmCreateFrame (form, "proxiesFrame", av, ac);

	Widget form1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form1 = XmCreateForm (frame1, "proxiesBox", av, ac);

	Widget proxiesLabel;

	proxiesLabel = XtVaCreateManagedWidget("proxiesLabel",
										   xmLabelGadgetClass, form1,
										   XmNalignment, XmALIGNMENT_BEGINNING,
										   XmNtopAttachment, XmATTACH_FORM,
										   XmNleftAttachment, XmATTACH_FORM,
										   XmNrightAttachment, XmATTACH_NONE,
										   XmNbottomAttachment, XmATTACH_NONE,
										   NULL);

	ac = 0;
	i = 0;

	kids[i++] = fep->direct_toggle =
		XmCreateToggleButtonGadget(form1, "direct", av, ac);

	kids[i++] = fep->manual_toggle =
		XmCreateToggleButtonGadget(form1, "manual", av, ac);

	kids[i++] = fep->auto_toggle =
		XmCreateToggleButtonGadget(form1, "auto", av, ac);

	Widget config_label;
	kids[i++] = config_label =
		XmCreateLabelGadget(form1, "config", av, ac);

	kids[i++] = fep->config_url_text =
		fe_CreateTextField (form1, "configUrl", av, ac);

	kids[i++] = fep->view_button =
		XmCreatePushButtonGadget(form1, "view", av, ac);

	kids[i++] = fep->reload_button =
		XmCreatePushButtonGadget(form1, "reload", av, ac);

	XtVaSetValues(fep->direct_toggle,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, proxiesLabel,
				  XmNtopOffset, 4,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, 16,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(fep->manual_toggle,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, fep->direct_toggle,
				  XmNtopOffset, 4,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, fep->direct_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(fep->auto_toggle,
				  XmNindicatorType, XmONE_OF_MANY,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, fep->manual_toggle,
				  XmNtopOffset, 4,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, fep->direct_toggle,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	Dimension  indicator_size;
	Dimension  spacing;

	XtVaGetValues(fep->auto_toggle,
				  XmNindicatorSize, &indicator_size,
				  XmNspacing, &spacing,
				  NULL);

	int labels_height;
	labels_height = XfeVaGetTallestWidget(fep->config_url_text,
										  config_label,
										  NULL);

	XtVaSetValues(config_label,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, fep->auto_toggle,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, fep->direct_toggle,
				  XmNleftOffset, indicator_size+spacing,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(fep->config_url_text,
				  XmNcolumns, 25,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, config_label,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, config_label,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(fep->view_button,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, fep->manual_toggle,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, fep->manual_toggle,
				  XmNleftOffset, 8,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(fep->reload_button,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, fep->config_url_text,
				  XmNleftAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNrightWidget, fep->config_url_text,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtAddCallback(fep->direct_toggle, XmNvalueChangedCallback,
				  cb_setProxies, fep);
	XtAddCallback(fep->manual_toggle, XmNvalueChangedCallback,
				  cb_setProxies, fep);
	XtAddCallback(fep->auto_toggle, XmNvalueChangedCallback,
				  cb_setProxies, fep);

	XtAddCallback(fep->view_button, XmNactivateCallback, cb_viewProxies, this);
	XtAddCallback(fep->reload_button, XmNactivateCallback, cb_reloadProxies, this);

	XtManageChildren (kids, i);
	XtManageChild(form1);
	XtManageChild(frame1);

	setCreated(TRUE);
}

// Member:       init
// Description:  Initializes page for GeneralProxies
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralProxies::init()
{
	XP_ASSERT(m_prefsDataGeneralProxies);

	PrefsDataGeneralProxies *fep = m_prefsDataGeneralProxies;
	XFE_GlobalPrefs         *prefs = &fe_globalPrefs;
	int                      state =  prefs->proxy_mode;
    Boolean                  sensitive;
	
    sensitive = !PREF_PrefIsLocked("network.proxy.type");
	XtVaSetValues(fep->direct_toggle, 
                  XmNset, ((state == PROXY_STYLE_UNSET) || 
									(state == PROXY_STYLE_NONE)), 
                  XmNsensitive, sensitive,
                  0);
	XtVaSetValues(fep->manual_toggle, 
                  XmNset, (state == PROXY_STYLE_MANUAL), 
                  XmNsensitive, sensitive,
                  0);
	XtVaSetValues(fep->view_button,  XmNsensitive, 
								(state == PROXY_STYLE_MANUAL), 0);
	XtVaSetValues(fep->auto_toggle, 
                  XmNset, (state == PROXY_STYLE_AUTOMATIC), 
                  XmNsensitive, sensitive,
                  0);
	fe_SetTextField(fep->config_url_text, prefs->proxy_url);

	sensitive = !PREF_PrefIsLocked("network.proxy.autoconfig_url");
	XtVaSetValues(fep->config_url_text, 
                  XmNsensitive, (state==PROXY_STYLE_AUTOMATIC) && sensitive, 0);
	XtVaSetValues(fep->reload_button, XmNsensitive, 
								(state==PROXY_STYLE_AUTOMATIC), 0);

	setInitialized(TRUE);
}

// Member:       install
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralProxies::install()
{
	fe_installGeneralProxies();
}

// Member:       save
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralProxies::save()
{
	PrefsDataGeneralProxies *fep = m_prefsDataGeneralProxies;

	XP_ASSERT(fep);

	XP_FREEIF(fe_globalPrefs.proxy_url);
	char *s1 = fe_GetTextField(fep->config_url_text);
	fe_globalPrefs.proxy_url = s1 ? s1 : XP_STRDUP("");
	if (*s1) {
		NET_SetProxyServer(PROXY_AUTOCONF_URL, (const char *)s1);
	}

	Boolean b;

	XtVaGetValues(fep->direct_toggle, XmNset, &b, 0);
	if (b) {
		NET_SelectProxyStyle(PROXY_STYLE_NONE);
		fe_globalPrefs.proxy_mode = PROXY_STYLE_NONE;
	}

	XtVaGetValues(fep->manual_toggle, XmNset, &b, 0);
	if (b) {
		NET_SelectProxyStyle(PROXY_STYLE_MANUAL);
		fe_globalPrefs.proxy_mode = PROXY_STYLE_MANUAL;
	}

	XtVaGetValues(fep->auto_toggle, XmNset, &b, 0);
	if (b) {
		NET_SelectProxyStyle(PROXY_STYLE_AUTOMATIC);
		fe_globalPrefs.proxy_mode = PROXY_STYLE_AUTOMATIC;
	}

	// Install preferences

	install();
}

// Member:       getData
// Description:  
// Inputs:
// Side effects: 

PrefsDataGeneralProxies *XFE_PrefsPageGeneralProxies::getData()
{
	return m_prefsDataGeneralProxies;
}

// Member:       cb_setProxies
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralProxies::cb_setProxies(Widget    w,
												XtPointer closure,
												XtPointer callData)
{
	PrefsDataGeneralProxies      *fep = (PrefsDataGeneralProxies *)closure;
	XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)callData;

	if (! cb->set) {
		XtVaSetValues(w, XmNset, True, 0);
	}
	else if (w == fep->direct_toggle){
		XtVaSetValues(fep->manual_toggle, XmNset, False, 0);
		XtVaSetValues(fep->auto_toggle, XmNset, False, 0);
		XtVaSetValues(fep->config_url_text, XmNsensitive, False, 0);
		XtVaSetValues(fep->view_button, XmNsensitive, False, 0);
		XtVaSetValues(fep->reload_button, XmNsensitive, False, 0);
    }
	else if (w == fep->manual_toggle){
		XtVaSetValues(fep->direct_toggle, XmNset, False, 0);
		XtVaSetValues(fep->auto_toggle, XmNset, False, 0);
		XtVaSetValues(fep->view_button, XmNsensitive, True, 0);
		XtVaSetValues(fep->config_url_text, XmNsensitive, False, 0);
		XtVaSetValues(fep->reload_button, XmNsensitive, False, 0);
    }
	else if (w == fep->auto_toggle){
		XtVaSetValues(fep->direct_toggle, XmNset, False, 0);
		XtVaSetValues(fep->manual_toggle, XmNset, False, 0);
		XtVaSetValues(fep->view_button, XmNsensitive, False, 0);
		XtVaSetValues(fep->config_url_text, 
                      XmNsensitive, !PREF_PrefIsLocked("network.proxy.autoconfig_url"), 
                      0);
		XtVaSetValues(fep->reload_button, XmNsensitive, True, 0);
    }
	else
		abort();
}

// Member:       cb_viewProxies
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralProxies::cb_viewProxies(Widget    /* w */,
												 XtPointer closure,
												 XtPointer /* callData */)
{
	XFE_PrefsPageGeneralProxies *thePage = (XFE_PrefsPageGeneralProxies *)closure;
	XFE_PrefsDialog             *theDialog = thePage->getPrefsDialog();
	Widget                       mainw = theDialog->getBaseWidget();

	//	while (!XtIsWMShell(mainw) && (XtParent(mainw)!=0))
	//		mainw = XtParent(mainw);

	// Instantiate a view proxies dialog

	XFE_PrefsProxiesViewDialog *theViewDialog = 0;

	if ((theViewDialog =
		 new XFE_PrefsProxiesViewDialog(theDialog, mainw, "prefsProxiesView")) == 0) {
	    fe_perror(thePage->getContext(), XP_GetString(XFE_OUT_OF_MEMORY_URL));
	    return;
	}

	// Initialize and pop up the view dialog

	theViewDialog->initPage();
	theViewDialog->show();
}

// Member:       cb_reloadProxies
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageGeneralProxies::cb_reloadProxies(Widget    /* w */,
												   XtPointer closure,
												   XtPointer /* callData */)
{
	XFE_PrefsPageGeneralProxies *thePage = (XFE_PrefsPageGeneralProxies *)closure;
	XFE_PrefsDialog             *theDialog = thePage->getPrefsDialog();
	PrefsDataGeneralProxies     *fep = thePage->getData();
	char                        *s1 = 0;

	s1 = fe_GetTextField(fep->config_url_text);
	if (s1 && *s1) {
		NET_SetProxyServer(PROXY_AUTOCONF_URL, s1);
		// fe_globalPrefs.proxy_url = XP_STRDUP(s1);
 	}
	if (s1) XtFree(s1);
	NET_ReloadProxyConfig(theDialog->getContext());
}

// ************************************************************************
// ************************  Advanced/DiskSpace   *************************
// ************************************************************************

// Member:       XFE_PrefsPageDiskSpace
// Description:  Constructor
// Inputs:
// Side effects: 

XFE_PrefsPageDiskSpace::XFE_PrefsPageDiskSpace(XFE_PrefsDialog *dialog)
	: XFE_PrefsPage(dialog),
	  m_prefsDataDiskSpace(0)
{
}

// Member:       ~XFE_PrefsPageDiskSpace
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsPageDiskSpace::~XFE_PrefsPageDiskSpace()
{
	delete m_prefsDataDiskSpace;
}

// Member:       create
// Description:  Creates page for DiskSpace
// Inputs:
// Side effects: 

void XFE_PrefsPageDiskSpace::create()
{
	Widget            kids[100];
	Arg               av[50];
	int               ac;
	int               i;

	PrefsDataDiskSpace *fep = NULL;

	fep = new PrefsDataDiskSpace;
	memset(fep, 0, sizeof(PrefsDataDiskSpace));
	m_prefsDataDiskSpace = fep;

	fep->context = getContext();
	fep->prompt_dialog = getPrefsDialog()->getDialogChrome();

	Widget form;

	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	form = XmCreateForm (m_wPageForm, "diskSpace", av, ac);
	XtManageChild (form);
	m_wPage = fep->page = form;

	// All messages

	Widget frame1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame1 = XmCreateFrame (form, "allMsgsFrame", av, ac);

	Widget form1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form1 = XmCreateForm (frame1, "allMsgsBox", av, ac);

	Widget label1;
	Widget max_msg_size_toggle;
	Widget max_msg_size_text;
	Widget k_label;
	Widget ask_threshold_toggle;
	Widget threshold_text;
	Widget k_label2;

	ac = 0;
	XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	label1 = XmCreateLabelGadget (frame1, "allMsgsBoxLabel", av, ac);

	ac = 0;
	i = 0;

	kids[i++] = max_msg_size_toggle =
		XmCreateToggleButtonGadget (form1, "maxMsgSize", av, ac);

	kids[i++] = max_msg_size_text = fe_CreateTextField (form1, "msgSize", av, ac);

	kids[i++] = k_label = XmCreateLabelGadget(form1, "k", av, ac);

	kids[i++] = ask_threshold_toggle =
		XmCreateToggleButtonGadget (form1, "askThreshold", av, ac);

	kids[i++] = threshold_text = fe_CreateTextField (form1, "threshold", av, ac);

	kids[i++] = k_label2 = XmCreateLabelGadget(form1, "k2", av, ac);

	fep->max_msg_size_toggle = max_msg_size_toggle;
	fep->max_msg_size_text = max_msg_size_text;
	fep->ask_threshold_toggle = ask_threshold_toggle;
	fep->threshold_text = threshold_text;

	int labels_height;
	labels_height = XfeVaGetTallestWidget(max_msg_size_toggle,
										  max_msg_size_text,
										  k_label,
										  NULL);
	
	XtVaSetValues (max_msg_size_toggle,
				   XmNindicatorType, XmN_OF_MANY,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_FORM,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (max_msg_size_text,
				   XmNcolumns, 6,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, max_msg_size_toggle,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_WIDGET,
				   XmNleftWidget, max_msg_size_toggle,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (k_label,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, max_msg_size_toggle,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_WIDGET,
				   XmNleftWidget, max_msg_size_text,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (ask_threshold_toggle,
				   XmNindicatorType, XmN_OF_MANY,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, max_msg_size_toggle,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, max_msg_size_toggle,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (threshold_text,
				   XmNcolumns, 6,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, ask_threshold_toggle,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_WIDGET,
				   XmNleftWidget, ask_threshold_toggle,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (k_label2,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, ask_threshold_toggle,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_WIDGET,
				   XmNleftWidget, threshold_text,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtManageChildren (kids, i);
	XtManageChild (label1);
	XtManageChild (form1);
	XtManageChild(frame1);

	// News Messages Only

	Widget frame2;

	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg (av [ac], XmNtopWidget, frame1); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame2 = XmCreateFrame (form, "newsMsgsFrame", av, ac);

	Widget form2;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form2 = XmCreateForm (frame2, "newsMsgsBox", av, ac);

	Widget label2;
	Widget news_msgs_label;
	Widget keep_news_by_age_toggle;
	Widget keep_all_news_toggle;
	Widget keep_news_by_count_toggle;
	Widget keep_news_days_text;
	Widget keep_news_count_text;
	Widget keep_unread_news_toggle;
	Widget days_label;
	Widget msgs_label;

	ac = 0;
	XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	label2 = XmCreateLabelGadget (frame2, "newsMsgsBoxLabel", av, ac);

	ac = 0;
	i = 0;

	kids[i++] = news_msgs_label = XmCreateLabelGadget (form2, "newsMsgsLabel", av, ac);

	kids[i++] = keep_news_by_age_toggle = 
		XmCreateToggleButtonGadget(form2, "keepNewsByAge", av, ac);

	kids[i++] = keep_all_news_toggle = 
		XmCreateToggleButtonGadget(form2, "keepAllNews", av, ac);

	kids[i++] = keep_news_by_count_toggle = 
		XmCreateToggleButtonGadget(form2, "keepNewsByCount", av, ac);

	kids[i++] = keep_unread_news_toggle = 
		XmCreateToggleButtonGadget(form2, "keepUnreadNews", av, ac);

	kids[i++] = keep_news_days_text = fe_CreateTextField (form2, "numDays", av, ac);

	kids[i++] = keep_news_count_text = fe_CreateTextField (form2, "numMsgs", av, ac);

	kids[i++] = days_label = XmCreateLabelGadget (form2, "daysLabel", av, ac);

	kids[i++] = msgs_label = XmCreateLabelGadget (form2, "msgsLabel", av, ac);

	fep->keep_all_news_toggle = keep_all_news_toggle;
	fep->keep_news_by_age_toggle = keep_news_by_age_toggle;
	fep->keep_news_by_count_toggle = keep_news_by_count_toggle;
	fep->keep_news_days_text = keep_news_days_text;
	fep->keep_news_count_text = keep_news_count_text;
	fep->keep_unread_news_toggle = keep_unread_news_toggle;

	labels_height = XfeVaGetTallestWidget(keep_news_by_age_toggle,
										  keep_news_days_text,
										  days_label,
										  NULL);
	
	XtVaSetValues (news_msgs_label,
				   XmNtopAttachment, XmATTACH_FORM,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (keep_news_by_age_toggle,
				   XmNindicatorType, XmONE_OF_MANY,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, news_msgs_label,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNleftOffset, 16,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (keep_all_news_toggle,
				   XmNindicatorType, XmONE_OF_MANY,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, keep_news_by_age_toggle,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, keep_news_by_age_toggle,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (keep_news_by_count_toggle,
				   XmNindicatorType, XmONE_OF_MANY,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, keep_all_news_toggle,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, keep_news_by_age_toggle,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (keep_unread_news_toggle,
				   XmNindicatorType, XmN_OF_MANY,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, keep_news_by_count_toggle,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, keep_news_by_age_toggle,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (keep_news_days_text,
				   XmNcolumns, 6,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, keep_news_by_age_toggle,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_WIDGET,
				   XmNleftWidget, keep_news_by_age_toggle,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (days_label,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, keep_news_by_age_toggle,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_WIDGET,
				   XmNleftWidget, keep_news_days_text,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (keep_news_count_text,
				   XmNcolumns, 6,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, keep_news_by_count_toggle,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_WIDGET,
				   XmNleftWidget, keep_news_by_count_toggle,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (msgs_label,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, keep_news_count_text,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_WIDGET,
				   XmNleftWidget, keep_news_count_text,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtManageChildren (kids, i);
	XtManageChild (label2);
	XtManageChild (form2);
	XtManageChild(frame2);

	XtAddCallback(keep_news_by_age_toggle, XmNvalueChangedCallback, cb_toggleKeepNews, fep);
	XtAddCallback(keep_all_news_toggle, XmNvalueChangedCallback, cb_toggleKeepNews, fep);
	XtAddCallback(keep_news_by_count_toggle, XmNvalueChangedCallback, cb_toggleKeepNews, fep);

	setCreated(TRUE);
}

// Member:       init
// Description:  Initializes page for DiskSpace
// Inputs:
// Side effects: 

void XFE_PrefsPageDiskSpace::init()
{
	XP_ASSERT(m_prefsDataDiskSpace);

    /* malmer */

	PrefsDataDiskSpace *fep = m_prefsDataDiskSpace;
	XFE_GlobalPrefs    *prefs = &fe_globalPrefs;
	char                buf[1024];

	// Do not download any message larger than...

	XtVaSetValues(fep->max_msg_size_toggle, 
                  XmNset, prefs->pop3_msg_size_limit_p, 
                  XmNsensitive, !PREF_PrefIsLocked("mail.limit_message_size"),
                  0);
	PR_snprintf(buf, sizeof(buf), "%d", abs(prefs->pop3_msg_size_limit));
	XtVaSetValues(fep->max_msg_size_text, 
                  XmNvalue, buf, 
#if 0
                  // This can be used when the toggle callback is added.
                  XmNsensitive, prefs->pop3_msg_size_limit_p && 
                                !PREF_PrefIsLocked("mail.max_size"),
#else
                  XmNsensitive, !PREF_PrefIsLocked("mail.max_size"),
#endif
                  0);

	XtVaSetValues(fep->ask_threshold_toggle, 
                  XmNset, prefs->msg_prompt_purge_threshold, 
                  XmNsensitive, !PREF_PrefIsLocked("mail.prompt_purge_threshhold"),
                  0);
	PR_snprintf(buf, sizeof(buf), "%d", prefs->msg_purge_threshold);
	XtVaSetValues(fep->threshold_text, 
                  XmNvalue, buf, 
#if 0
                  // This can be used when the toggle callback is added.
                  XmNsensitive, prefs->msg_prompt_purge_threshold && 
                                !PREF_PrefIsLocked("mail.purge_threshhold"),
#else
                  XmNsensitive, !PREF_PrefIsLocked("mail.purge_threshhold"),
#endif
                  0);

	// Clean up messages 

	XtVaSetValues(fep->keep_all_news_toggle, 
				  XmNset, (prefs->news_keep_method == KEEP_ALL_NEWS),
                  XmNsensitive, !PREF_PrefIsLocked("news.keep.method"),
				  0);
	XtVaSetValues(fep->keep_news_by_age_toggle, 
				  XmNset, (prefs->news_keep_method == KEEP_NEWS_BY_AGE),
                  XmNsensitive, !PREF_PrefIsLocked("news.keep.method"),
				  0);
	XtVaSetValues(fep->keep_news_by_count_toggle, 
				  XmNset, (prefs->news_keep_method == KEEP_NEWS_BY_COUNT),
                  XmNsensitive, !PREF_PrefIsLocked("news.keep.method"),
				  0);

	PR_snprintf(buf, sizeof(buf), "%d", prefs->news_keep_days);
	XtVaSetValues(fep->keep_news_days_text, 
                  XmNvalue, buf, 
#if 0
                  // This can be used when the toggle callback is added.
                  XmNsensitive, prefs->news_keep_method == KEEP_NEWS_BY_AGE && 
                                !PREF_PrefIsLocked("news.keep.days"),
#else
                  XmNsensitive, !PREF_PrefIsLocked("news.keep.days"),
#endif
                  0);

	PR_snprintf(buf, sizeof(buf), "%d", prefs->news_keep_count);
	XtVaSetValues(fep->keep_news_count_text, 
                  XmNvalue, buf, 
#if 0
                  // This can be used when the toggle callback is added.
                  XmNsensitive, prefs->news_keep_method == KEEP_NEWS_BY_COUNT &&
                                !PREF_PrefIsLocked("news.keep.count"),
#else
                  XmNsensitive, !PREF_PrefIsLocked("news.keep.count"),
#endif
                  0);

	// Keep unread messages

	XtVaSetValues(fep->keep_unread_news_toggle, 
                  XmNset, prefs->news_keep_only_unread, 
                  XmNsensitive, !PREF_PrefIsLocked("news.keep.only_unread"),
                  0);

	setInitialized(TRUE);
}

// Member:       install
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageDiskSpace::install()
{
	fe_installDiskSpace();
}

// Member:       save
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageDiskSpace::save()
{
	PrefsDataDiskSpace *fep = m_prefsDataDiskSpace;

	XP_ASSERT(fep);

	Boolean                     b;
    char                        c;
	char                       *s = 0;
    int                         n;

	// All messages

	XtVaGetValues(fep->max_msg_size_toggle, XmNset, &b, 0);
	fe_globalPrefs.pop3_msg_size_limit_p = b;

    XtVaGetValues(fep->max_msg_size_text, XmNvalue, &s, 0);
    fe_globalPrefs.pop3_msg_size_limit = 0;
    if (1 == sscanf (s, " %d %c", &n, &c))
      fe_globalPrefs.pop3_msg_size_limit = n;
	if (s) XtFree(s);

	XtVaGetValues(fep->ask_threshold_toggle, XmNset, &b, 0);
	fe_globalPrefs.msg_prompt_purge_threshold = b;

    XtVaGetValues(fep->threshold_text, XmNvalue, &s, 0);
    fe_globalPrefs.msg_purge_threshold = 0;
    if (1 == sscanf (s, " %d %c", &n, &c))
      fe_globalPrefs.msg_purge_threshold = n;
	if (s) XtFree(s);

	// News messages

    XtVaGetValues(fep->keep_all_news_toggle, XmNset, &b, 0);
    if (b) fe_globalPrefs.news_keep_method = KEEP_ALL_NEWS;

    XtVaGetValues(fep->keep_news_by_age_toggle, XmNset, &b, 0);
    if (b) fe_globalPrefs.news_keep_method = KEEP_NEWS_BY_AGE;

    XtVaGetValues(fep->keep_news_by_count_toggle, XmNset, &b, 0);
    if (b) fe_globalPrefs.news_keep_method = KEEP_NEWS_BY_COUNT;

    XtVaGetValues(fep->keep_news_days_text, XmNvalue, &s, 0);
    fe_globalPrefs.news_keep_days = 0;
    if (1 == sscanf (s, " %d %c", &n, &c))
      fe_globalPrefs.news_keep_days = n;
	if (s) XtFree(s);

    XtVaGetValues(fep->keep_news_count_text, XmNvalue, &s, 0);
    fe_globalPrefs.news_keep_count = 0;
    if (1 == sscanf (s, " %d %c", &n, &c))
      fe_globalPrefs.news_keep_count = n;
	if (s) XtFree(s);

	XtVaGetValues(fep->keep_unread_news_toggle, XmNset, &b, 0);
	fe_globalPrefs.news_keep_only_unread = b;

	// Install preferences

	install();
}

// Member:       getData
// Description:  
// Inputs:
// Side effects: 

PrefsDataDiskSpace *XFE_PrefsPageDiskSpace::getData()
{
	return m_prefsDataDiskSpace;
}

#if 0
// Member:       cb_more
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageDiskSpace::cb_more(Widget    /* w */,
											XtPointer closure,
											XtPointer /* callData */)
{
	XFE_PrefsPageDiskSpace *thePage = (XFE_PrefsPageDiskSpace *)closure;
	XFE_PrefsDialog               *theDialog = thePage->getPrefsDialog();
	Widget                         mainw = theDialog->getBaseWidget();

	//	while (!XtIsWMShell(mainw) && (XtParent(mainw)!=0))
	//		mainw = XtParent(mainw);

	// Instantiate a mail server more dialog

	XFE_PrefsDiskMoreDialog *theMoreDialog = 0;

	if ((theMoreDialog =
		 new XFE_PrefsDiskMoreDialog(theDialog, mainw, "prefsDiskMore")) == 0) {
	    fe_perror(thePage->getContext(), XP_GetString(XFE_OUT_OF_MEMORY_URL));
	    return;
	}

	// Initialize and pop up the more dialog

	theMoreDialog->initPage();
	theMoreDialog->show();
}
#endif

void XFE_PrefsPageDiskSpace::cb_toggleKeepNews(Widget    widget,
											   XtPointer closure,
											   XtPointer call_data)
{
	PrefsDataDiskSpace           *fep = (PrefsDataDiskSpace *)closure;
	XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)call_data;

	if (! cb->set) {
		XtVaSetValues(widget, XmNset, True, 0);
	}
	else if (widget == fep->keep_news_by_age_toggle) {
		XtVaSetValues(fep->keep_all_news_toggle, XmNset, False, 0);
		XtVaSetValues(fep->keep_news_by_count_toggle, XmNset, False, 0);
	}
	else if (widget == fep->keep_all_news_toggle) {
		XtVaSetValues(fep->keep_news_by_age_toggle, XmNset, False, 0);
		XtVaSetValues(fep->keep_news_by_count_toggle, XmNset, False, 0);
	}
	else if (widget == fep->keep_news_by_count_toggle) {
		XtVaSetValues(fep->keep_news_by_age_toggle, XmNset, False, 0);
		XtVaSetValues(fep->keep_all_news_toggle, XmNset, False, 0);
	}
	else
		abort();
}

#ifdef PREFS_UNSUPPORTED

// ************************************************************************
// ************************  Advanced/Help Files  *************************
// ************************************************************************

// Member:       XFE_PrefsPageHelpFiles
// Description:  Constructor
// Inputs:
// Side effects: 

XFE_PrefsPageHelpFiles::XFE_PrefsPageHelpFiles(XFE_PrefsDialog *dialog)
	: XFE_PrefsPage(dialog),
	  m_prefsDataHelpFiles(0)
{
}

// Member:       ~XFE_PrefsPageHelpFiles
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsPageHelpFiles::~XFE_PrefsPageHelpFiles()
{
	delete m_prefsDataHelpFiles;
}

// Member:       create
// Description:  Creates page for HelpFiles
// Inputs:
// Side effects: 

void XFE_PrefsPageHelpFiles::create()
{
	Widget            kids[100];
	Arg               av[50];
	int               ac;
	int               i;

	PrefsDataHelpFiles *fep = NULL;

	fep = new PrefsDataHelpFiles;
	memset(fep, 0, sizeof(PrefsDataHelpFiles));
	m_prefsDataHelpFiles = fep;

	fep->context = getContext();
	fep->prompt_dialog = getPrefsDialog()->getDialogChrome();

	Widget form;

	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	form = XmCreateForm (m_wPageForm, "helpFiles", av, ac);
	XtManageChild (form);
	m_wPage = fep->page = form;

	Widget frame1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame1 = XmCreateFrame (form, "helpFrame", av, ac);

	Widget form1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form1 = XmCreateForm (frame1, "helpBox", av, ac);

	Widget help_label;
	Widget netscape_toggle;
	Widget install_toggle;
	Widget custom_toggle;
	Widget custom_url_text;
	Widget browse_button;

	ac = 0;
	i = 0;

	kids[i++] = help_label = XmCreateLabelGadget(form1, "helpLabel", av, ac);

	kids[i++] = netscape_toggle =
		XmCreateToggleButtonGadget(form1, "netscapeToggle", av, ac);

	kids[i++] = install_toggle =
		XmCreateToggleButtonGadget(form1, "installToggle", av, ac);

	kids[i++] = custom_toggle =
		XmCreateToggleButtonGadget(form1, "customToggle", av, ac);

	kids[i++] = custom_url_text = 
		fe_CreateTextField (form1, "customUrl", av, ac);

	kids[i++] = browse_button = XmCreatePushButtonGadget(form1, "browse", av, ac);

	fep->netscape_toggle = netscape_toggle;
	fep->install_toggle = install_toggle;
	fep->custom_toggle = custom_toggle;
	fep->custom_url_text = custom_url_text;
	fep->browse_button = browse_button;

	int labels_height;
	labels_height = XfeVaGetTallestWidget(custom_toggle,
										  custom_url_text,
										  NULL);

	XtVaSetValues (help_label,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_FORM,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (netscape_toggle,
				   XmNindicatorType, XmONE_OF_MANY,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, help_label,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNleftOffset, 16,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (install_toggle,
				   XmNindicatorType, XmONE_OF_MANY,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, netscape_toggle,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, netscape_toggle,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (custom_toggle,
				   XmNindicatorType, XmONE_OF_MANY,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, install_toggle,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNleftWidget, install_toggle,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (custom_url_text,
				   XmNcolumns, 35,
				   XmNheight, labels_height,
				   XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNtopWidget, custom_toggle,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_WIDGET,
				   XmNleftWidget, custom_toggle,
				   XmNrightAttachment, XmATTACH_NONE,
				   0);

	XtVaSetValues (browse_button,
				   XmNtopAttachment, XmATTACH_WIDGET,
				   XmNtopWidget, custom_url_text,
				   XmNtopOffset, 8,
				   XmNbottomAttachment, XmATTACH_NONE,
				   XmNleftAttachment, XmATTACH_NONE,
				   XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET,
				   XmNrightWidget, custom_url_text,
				   0);

	XtManageChildren (kids, i);
	XtManageChild (form1);
	XtManageChild(frame1);

	XtAddCallback(netscape_toggle, XmNvalueChangedCallback, cb_toggleHelpSite, fep);
	XtAddCallback(install_toggle, XmNvalueChangedCallback, cb_toggleHelpSite, fep);
	XtAddCallback(custom_toggle, XmNvalueChangedCallback, cb_toggleHelpSite, fep);

	XtAddCallback(browse_button, XmNactivateCallback, cb_browse, this);

	setCreated(TRUE);
}

// Member:       init
// Description:  Initializes page for HelpFiles
// Inputs:
// Side effects: 

void XFE_PrefsPageHelpFiles::init()
{
	XP_ASSERT(m_prefsDataHelpFiles);

	PrefsDataHelpFiles *fep = m_prefsDataHelpFiles;
	XFE_GlobalPrefs    *prefs = &fe_globalPrefs;
    Boolean             sensitive;

    sensitive = !PREF_PrefIsLocked("general.help_source.site");

	XtVaSetValues(fep->netscape_toggle, 
				  XmNset, (prefs->help_source_site == HELPFILE_SITE_NETSCAPE),
				  XmNsensitive, sensitive,
				  0);
	XtVaSetValues(fep->install_toggle, 
				  XmNset, (prefs->help_source_site == HELPFILE_SITE_INSTALLED),
				  XmNsensitive, sensitive,
				  0);
	XtVaSetValues(fep->custom_toggle, 
				  XmNset, (prefs->help_source_site == HELPFILE_SITE_CUSTOM),
				  XmNsensitive, sensitive,
				  0);

    sensitive = prefs->help_source_site == HELPFILE_SITE_CUSTOM &&
                !PREF_PrefIsLocked("general.help_source.url");
	XtVaSetValues(fep->custom_url_text, 
				  XmNsensitive, sensitive,
                  0);
	fe_SetTextField(fep->custom_url_text, prefs->help_source_url);
    XtSetSensitive(fep->browse_button, sensitive);

	setInitialized(TRUE);
}

// Member:       install
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageHelpFiles::install()
{
}

// Member:       save
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageHelpFiles::save()
{
	PrefsDataHelpFiles *fep = m_prefsDataHelpFiles;

	XP_ASSERT(fep);

	Boolean b;

	XtVaGetValues(fep->netscape_toggle, XmNset, &b, 0);
	if (b) fe_globalPrefs.help_source_site = HELPFILE_SITE_NETSCAPE;

	XtVaGetValues(fep->install_toggle, XmNset, &b, 0);
	if (b) fe_globalPrefs.help_source_site = HELPFILE_SITE_INSTALLED;

	XtVaGetValues(fep->custom_toggle, XmNset, &b, 0);
	if (b) fe_globalPrefs.help_source_site = HELPFILE_SITE_CUSTOM;

	XP_FREEIF(fe_globalPrefs.help_source_url);
	char *s = fe_GetTextField(fep->custom_url_text);
    fe_globalPrefs.help_source_url = s ? s : XP_STRDUP("");

	// Install preferences

	install();
}

// Member:       getData
// Description:  
// Inputs:
// Side effects: 

PrefsDataHelpFiles *XFE_PrefsPageHelpFiles::getData()
{
	return m_prefsDataHelpFiles;
}

// Member:       cb_toggleHelpSite
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageHelpFiles::cb_toggleHelpSite(Widget    w,
											   XtPointer closure,
											   XtPointer callData)
{
	XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)callData;
	PrefsDataHelpFiles           *fep = (PrefsDataHelpFiles *)closure;
    Boolean                       sensitive;

	if (! cb->set) {
		XtVaSetValues(w, XmNset, True, 0);
	}
	else if (w == fep->netscape_toggle) {
		XtVaSetValues(fep->install_toggle, XmNset, False, 0);
		XtVaSetValues(fep->custom_toggle, XmNset, False, 0);
	}
	else if (w == fep->install_toggle) {
		XtVaSetValues(fep->netscape_toggle, XmNset, False, 0);
		XtVaSetValues(fep->custom_toggle, XmNset, False, 0);
	}
	else if (w == fep->custom_toggle) {
		XtVaSetValues(fep->netscape_toggle, XmNset, False, 0);
		XtVaSetValues(fep->install_toggle, XmNset, False, 0);
	}
	else
		abort();	

    sensitive = ( w == fep->custom_toggle &&
                  !PREF_PrefIsLocked("general.help_source.url") );
	XtSetSensitive(fep->custom_url_text, sensitive);
    XtSetSensitive(fep->browse_button, sensitive);
}

// Member:       cb_more
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPageHelpFiles::cb_browse(Widget    /* w */,
									   XtPointer closure,
									   XtPointer /* callData */)
{
	XFE_PrefsPageHelpFiles *thePage = (XFE_PrefsPageHelpFiles *)closure;
	XFE_PrefsDialog        *theDialog = thePage->getPrefsDialog();
	PrefsDataHelpFiles     *fep = thePage->getData();

	fe_browse_file_of_text_in_url(theDialog->getContext(), fep->custom_url_text, True);
}

#endif /* PREFS_UNSUPPORTED */

