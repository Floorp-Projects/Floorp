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

/* -*- Mode: C++; tab-width: 4 -*-
   PrefsDialogSmart.cpp -- the Smart Browsing preferences pane
   Created: Ramiro Estrugo <ramiro@netscape.com>, 20-May-98.
 */

#include "xpassert.h"
#include "MozillaApp.h"
#include "xfe.h"
#include "prefapi.h"
#include "PrefsDialog.h"
#include "prefapi.h"

#include <Xm/LabelG.h> 
#include <Xm/ToggleBG.h> 
#include <Xm/Text.h> 

#include <Xfe/Xfe.h> 

//
// Smart Browsing preferences
//

//////////////////////////////////////////////////////////////////////////
XFE_PrefsPageBrowserSmart::XFE_PrefsPageBrowserSmart(XFE_PrefsDialog *dialog)
	: XFE_PrefsPage(dialog),
	  m_prefsDataBrowserSmart(0)
{
	m_rl_need_chrome_update = FALSE;
}
//////////////////////////////////////////////////////////////////////////
XFE_PrefsPageBrowserSmart::~XFE_PrefsPageBrowserSmart()
{
	delete m_prefsDataBrowserSmart;
}
//////////////////////////////////////////////////////////////////////////
void 
XFE_PrefsPageBrowserSmart::create()
{
	Widget            kids[100];
	Arg               av[50];
	int               ac;
	int               i;

	PrefsDataBrowserSmart *fep = NULL;

	fep = new PrefsDataBrowserSmart;
	memset(fep, 0, sizeof(PrefsDataBrowserSmart));
	m_prefsDataBrowserSmart = fep;

	fep->context = getContext();
	fep->prompt_dialog = getPrefsDialog()->getDialogChrome();

	// Smart browsing page form
	Widget form;

	ac = 0;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	form = XmCreateForm (m_wPageForm, "smart", av, ac);
	XtManageChild (form);
	m_wPage = fep->page = form;

	Widget frame1;

	// Related links frame
	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
	frame1 = XmCreateFrame (form, "relatedFrame", av, ac);

	// Related links box
	Widget form1;

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form1 = XmCreateForm (frame1, "relatedBox", av, ac);

	// The "Related Links" frame title
	Widget label1;

	ac = 0;
	XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	label1 = XmCreateLabelGadget (frame1, "relatedBoxLabel", av, ac);

	XtManageChild (label1);

	// top portion
	ac = 0;
	i = 0;

 	// Related links enabled
 	kids[i++] = fep->rl_enabled_toggle = 
 		XmCreateToggleButtonGadget(form1, "enableRelated", av, ac);

	// Related links autoload label
	kids[i++] = fep->rl_autoload_label = 
		XmCreateLabelGadget(form1, "autoloadLabel", av, ac);

	// Related links autoload always
	kids[i++] = fep->rl_autoload_radio_always = 
		XmCreateToggleButtonGadget(form1, "autoloadAlways", av, ac);

	// Related links autoload adaptive
	kids[i++] = fep->rl_autoload_radio_adaptive = 
		XmCreateToggleButtonGadget(form1, "autoloadAdaptive", av, ac);

	// Related links autoload never
	kids[i++] = fep->rl_autoload_radio_never = 
		XmCreateToggleButtonGadget(form1, "autoloadNever", av, ac);

	// Related links excluded domains label
	kids[i++] = fep->rl_excluded_label = 
		XmCreateLabelGadget(form1, "excludedLabel", av, ac);

	// Related links excluded domains text
	kids[i++] = fep->rl_excluded_text = 
		fe_CreateText(form1, "excludedText", av, ac);

	// Related links enabled
	XtVaSetValues(fep->rl_enabled_toggle,
				  XmNindicatorType,		XmN_OF_MANY,
				  XmNalignment,			XmALIGNMENT_BEGINNING,
				  XmNtopAttachment,		XmATTACH_FORM,
				  XmNbottomAttachment,	XmATTACH_NONE,
				  XmNleftAttachment,	XmATTACH_FORM,
				  XmNrightAttachment,	XmATTACH_NONE,
				  0);

	// Related links autoload label
	XtVaSetValues(fep->rl_autoload_label,
				  XmNalignment,			XmALIGNMENT_BEGINNING,
				  XmNtopAttachment,		XmATTACH_WIDGET,
				  XmNtopWidget,			fep->rl_enabled_toggle,
				  XmNtopOffset,			10,
				  XmNbottomAttachment,	XmATTACH_NONE,
				  XmNleftAttachment,	XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget,		fep->rl_enabled_toggle,
				  XmNrightAttachment,	XmATTACH_NONE,
				  0);

	// Related links autoload always
	XtVaSetValues(fep->rl_autoload_radio_always,
				  XmNindicatorType,		XmONE_OF_MANY,
				  XmNalignment,			XmALIGNMENT_BEGINNING,
				  XmNtopAttachment,		XmATTACH_WIDGET,
				  XmNtopWidget,			fep->rl_autoload_label,
				  XmNbottomAttachment,	XmATTACH_NONE,
				  XmNleftAttachment,	XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget,		fep->rl_enabled_toggle,
				  XmNrightAttachment,	XmATTACH_NONE,
				  XmNleftOffset,		16,
				  0);

	// Related links autoload adaptive
	XtVaSetValues(fep->rl_autoload_radio_adaptive,
				  XmNindicatorType,		XmONE_OF_MANY,
				  XmNalignment,			XmALIGNMENT_BEGINNING,
				  XmNtopAttachment,		XmATTACH_WIDGET,
				  XmNtopWidget,			fep->rl_autoload_radio_always,
				  XmNbottomAttachment,	XmATTACH_NONE,
				  XmNleftAttachment,	XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget,		fep->rl_enabled_toggle,
				  XmNrightAttachment,	XmATTACH_NONE,
				  XmNleftOffset,		16,
				  0);

	// Related links autoload never
	XtVaSetValues(fep->rl_autoload_radio_never,
				  XmNindicatorType,		XmONE_OF_MANY,
				  XmNalignment,			XmALIGNMENT_BEGINNING,
				  XmNtopAttachment,		XmATTACH_WIDGET,
				  XmNtopWidget,			fep->rl_autoload_radio_adaptive,
				  XmNbottomAttachment,	XmATTACH_NONE,
				  XmNleftAttachment,	XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget,		fep->rl_enabled_toggle,
				  XmNrightAttachment,	XmATTACH_NONE,
				  XmNleftOffset,		16,
				  0);

	// Related links excluded domains label
	XtVaSetValues(fep->rl_excluded_label,
				  XmNalignment,			XmALIGNMENT_BEGINNING,
				  XmNtopAttachment,		XmATTACH_WIDGET,
				  XmNtopWidget,			fep->rl_autoload_radio_never,
				  XmNtopOffset,			10,
				  XmNbottomAttachment,	XmATTACH_NONE,
				  XmNleftAttachment,	XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget,		fep->rl_enabled_toggle,
				  XmNrightAttachment,	XmATTACH_NONE,
				  0);

	// Related links excluded domains text
	XtVaSetValues(fep->rl_excluded_text,
				  XmNtopAttachment,		XmATTACH_WIDGET,
				  XmNtopWidget,			fep->rl_excluded_label,
				  XmNbottomAttachment,	XmATTACH_FORM,
				  XmNleftAttachment,	XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget,		fep->rl_enabled_toggle,
				  XmNrightAttachment,	XmATTACH_FORM,

				  XmNeditable,			True,
				  XmNeditMode,			XmMULTI_LINE_EDIT,
				  XmNrows,				5,
				  XmNwordWrap,			True,
				  
				  0);

	XtManageChildren (kids, i);
	XtManageChild (form1);
	XtManageChild (frame1);
	
	// Keywords
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
	frame2 = XmCreateFrame (form, "keywordFrame", av, ac);

	ac = 0;
	XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	form2 = XmCreateForm (frame2, "keywordBox", av, ac);
	
	ac = 0;
	XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	label2 = XmCreateLabelGadget (frame2, "keywordBoxLabel", av, ac);

	ac = 0;
	i = 0;

	kids[i++] = fep->enable_internet_keywords =
		XmCreateToggleButtonGadget(form2, "enableKeywords", av, ac);

	XtVaSetValues(fep->enable_internet_keywords,
				  XmNindicatorType,		XmN_OF_MANY,
				  XmNalignment,			XmALIGNMENT_BEGINNING,
				  XmNtopAttachment,		XmATTACH_FORM,
				  XmNbottomAttachment,	XmATTACH_NONE,
				  XmNleftAttachment,	XmATTACH_FORM,
				  XmNrightAttachment,	XmATTACH_NONE,
				  NULL);

	XtManageChildren (kids, i);

	XtManageChild (label2);
	XtManageChild (form2);
	XtManageChild (frame2);

	// Add callbacks to automatic related links toggle button
 	XtAddCallback(fep->rl_enabled_toggle, 
				  XmNvalueChangedCallback,
 				  cb_toggleRelatedEnabled,
				  (XtPointer) this);

	// Add callbacks to automatic related links toggle button
	XtAddCallback(fep->rl_autoload_radio_always, 
				  XmNvalueChangedCallback,
				  cb_toggleRelatedAutoload, 
				  (XtPointer) fep);

	XtAddCallback(fep->rl_autoload_radio_adaptive, 
				  XmNvalueChangedCallback,
				  cb_toggleRelatedAutoload, 
				  (XtPointer) fep);

	XtAddCallback(fep->rl_autoload_radio_never, 
				  XmNvalueChangedCallback,
				  cb_toggleRelatedAutoload, 
				  (XtPointer) fep);

	// Add callbacks to internet keyword toggle button
// 	XtAddCallback(fep->enable_internet_keywords, XmNvalueChangedCallback,
// 				  cb_toggleInternetKeywords, fep);

	setCreated(TRUE);
}
//////////////////////////////////////////////////////////////////////////
void 
XFE_PrefsPageBrowserSmart::init()
{
	XP_ASSERT( m_prefsDataBrowserSmart != NULL );

	PrefsDataBrowserSmart *		fep = m_prefsDataBrowserSmart;

	// Related links enabled
	XtVaSetValues(fep->rl_enabled_toggle, 
                  XmNset,			fe_globalPrefs.related_links_enabled, 
                  XmNsensitive,		!PREF_PrefIsLocked("browser.related.enabled"),
                  NULL);

	// Excluded domaings
	XmTextSetString(fep->rl_excluded_text,
					fe_globalPrefs.related_disabled_domains);

	updateRelatedAutoloadSensitive();

	updateInternetKeywordsSensitive();

	setInitialized(TRUE);
}
//////////////////////////////////////////////////////////////////////////
void 
XFE_PrefsPageBrowserSmart::install()
{
	if (m_rl_need_chrome_update) 
	{
		// Notify whoever is interested in updating the related links
		XFE_MozillaApp::theApp()->notifyInterested(XFE_MozillaApp::updateRelatedLinksShowing);
		m_rl_need_chrome_update = FALSE;
	}
}
//////////////////////////////////////////////////////////////////////////
void 
XFE_PrefsPageBrowserSmart::save()
{
	PrefsDataBrowserSmart *		fep = m_prefsDataBrowserSmart;

	XP_ASSERT(fep);

	// Related links
	XP_Bool	old_related_links_enabled = fe_globalPrefs.related_links_enabled;

	// Related links enabled
	fe_globalPrefs.related_links_enabled = 
		XfeToggleButtonIsSet(fep->rl_enabled_toggle);

	// Related links autoload
	if (old_related_links_enabled != fe_globalPrefs.related_links_enabled) 
	{
		m_rl_need_chrome_update = TRUE;
	}

	// Related links autoload
	if (XfeToggleButtonIsSet(fep->rl_autoload_radio_always))
	{
		fe_globalPrefs.related_links_autoload = RELATED_LINKS_AUTOLOAD_ALWAYS;
	}

	if (XfeToggleButtonIsSet(fep->rl_autoload_radio_adaptive))
	{
		fe_globalPrefs.related_links_autoload = RELATED_LINKS_AUTOLOAD_ADAPTIVE;
	}

	if (XfeToggleButtonIsSet(fep->rl_autoload_radio_never))
	{
		fe_globalPrefs.related_links_autoload = RELATED_LINKS_AUTOLOAD_NEVER;
	}

	// Excluded domains
	fe_globalPrefs.related_disabled_domains = 
		XmTextGetString(fep->rl_excluded_text);


	// Internet Keywords
	fe_globalPrefs.internet_keywords_enabled = 
		XfeToggleButtonIsSet(fep->enable_internet_keywords);


	// Install preferences
	install();
}
//////////////////////////////////////////////////////////////////////////
PrefsDataBrowserSmart *
XFE_PrefsPageBrowserSmart::getData()
{
	return m_prefsDataBrowserSmart;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_PrefsPageBrowserSmart::updateRelatedAutoloadSensitive()
{
	XP_ASSERT( m_prefsDataBrowserSmart != NULL );

	PrefsDataBrowserSmart *		fep = m_prefsDataBrowserSmart;

	Boolean sensitive = 
		!PREF_PrefIsLocked("browser.related.enabled") && 
		fe_globalPrefs.related_links_enabled;

	// autoload label
	XtVaSetValues(fep->rl_autoload_label, 
                  XmNsensitive,	sensitive,
				  NULL);

	// autoload always
	XtVaSetValues(fep->rl_autoload_radio_always, 
				  XmNset,		(fe_globalPrefs.related_links_autoload == RELATED_LINKS_AUTOLOAD_ALWAYS), 
                  XmNsensitive,	sensitive,
				  NULL);

	// autoload adaptive
	XtVaSetValues(fep->rl_autoload_radio_adaptive, 
				  XmNset,		(fe_globalPrefs.related_links_autoload == RELATED_LINKS_AUTOLOAD_ADAPTIVE),
                  XmNsensitive,	sensitive,
				  NULL);

	// autoload never
	XtVaSetValues(fep->rl_autoload_radio_never,
				  XmNset,		(fe_globalPrefs.related_links_autoload == RELATED_LINKS_AUTOLOAD_NEVER),
                  XmNsensitive,	sensitive,
				  NULL);

	// excluded domains label
	XtVaSetValues(fep->rl_excluded_label,
                  XmNsensitive,	sensitive,
				  NULL);

	// excluded domains text
	XtVaSetValues(fep->rl_excluded_text,
                  XmNsensitive,	sensitive,
				  NULL);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_PrefsPageBrowserSmart::updateInternetKeywordsSensitive()
{
	XP_ASSERT( m_prefsDataBrowserSmart != NULL );

	PrefsDataBrowserSmart *		fep = m_prefsDataBrowserSmart;

	Boolean keywords_sensitive = 
		!PREF_PrefIsLocked("browser.goBrowsing.enabled");

	// enable keywords
	XtVaSetValues(fep->enable_internet_keywords, 
				  XmNset,			fe_globalPrefs.internet_keywords_enabled,
				  XmNsensitive,		keywords_sensitive,
				  NULL);
}
//////////////////////////////////////////////////////////////////////////
void 
XFE_PrefsPageBrowserSmart::cb_toggleRelatedEnabled(Widget    w,
												   XtPointer clientData,
												   XtPointer callData)
{
	XFE_PrefsPageBrowserSmart *		obj = 
		(XFE_PrefsPageBrowserSmart *) clientData;

	XmToggleButtonCallbackStruct *	cb = 
		(XmToggleButtonCallbackStruct *) callData;

	PrefsDataBrowserSmart     *		fep = obj->m_prefsDataBrowserSmart;
	
	XP_ASSERT( obj != NULL );
	XP_ASSERT( fep != NULL );
	XP_ASSERT( cb != NULL );
 	XP_ASSERT( w == fep->rl_enabled_toggle );

	fe_globalPrefs.related_links_enabled = (XP_Bool) cb->set;
	
	obj->m_rl_need_chrome_update = True;

	obj->updateRelatedAutoloadSensitive();
}
//////////////////////////////////////////////////////////////////////////
void 
XFE_PrefsPageBrowserSmart::cb_toggleRelatedAutoload(Widget    w,
													XtPointer closure,
													XtPointer callData)
{
	XmToggleButtonCallbackStruct *	cb = 
		(XmToggleButtonCallbackStruct *) callData;

	PrefsDataBrowserSmart     *		fep = (PrefsDataBrowserSmart *) closure;

 	if (! cb->set) 
	{
 		XtVaSetValues(w, XmNset, True, 0);
 	}
 	else if (w == fep->rl_autoload_radio_always)
	{
 		XtVaSetValues(fep->rl_autoload_radio_adaptive, XmNset, False, 0);
 		XtVaSetValues(fep->rl_autoload_radio_never, XmNset, False, 0);
	}
 	else if (w == fep->rl_autoload_radio_adaptive)
	{
 		XtVaSetValues(fep->rl_autoload_radio_always, XmNset, False, 0);
 		XtVaSetValues(fep->rl_autoload_radio_never, XmNset, False, 0);
	}
 	else if (w == fep->rl_autoload_radio_never)
	{
 		XtVaSetValues(fep->rl_autoload_radio_always, XmNset, False, 0);
 		XtVaSetValues(fep->rl_autoload_radio_adaptive, XmNset, False, 0);
	}
 	else
	{
 		abort();
	}
}
//////////////////////////////////////////////////////////////////////////

