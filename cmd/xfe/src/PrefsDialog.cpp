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
/* */
/*
   PrefsDialog.cpp -- class for XFE preferences dialogs.
   Created: Linda Wei <lwei@netscape.com>, 17-Sep-96.
 */


#define TRUE   1
#define FALSE  0

#include "felocale.h"
#include "structs.h"
#include "fonts.h"
#include "xpassert.h"
#include "xfe.h"
#include "PrefsDialog.h"

#ifdef MOZ_MAIL_NEWS
#include "PrefsPageMessages.h"
#include "PrefsPageNServer.h"
#include "PrefsPageAddress.h"
#endif

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
#include <XmL/Grid.h>

#include <Xfe/Xfe.h>

extern int XFE_ERROR_SAVING_OPTIONS;
extern int XFE_OUT_OF_MEMORY_URL;

static const int   NUM_OUTLINER_COLS = 2;
static       int   OUTLINER_COLWIDTHS[] = {4, 20};
static const char *OUTLINER_COLUMN_TYPE = "";
static const char *OUTLINER_COLUMN_NAME = "Category";
static       char *TAG_NORMAL = "NORMAL";
static       char *TAG_BOLD = "BOLD";

#define CAT_LAST_CELL            0xFFFF

#define CAT_FONTS                "Fonts"
#define CAT_COLORS               "Colors"
#define CAT_APPS                 "Applications"
#define CAT_LANG                 "Languages"
#define CAT_CACHE                "Cache"
#define CAT_PROXIES              "Proxies"
#define CAT_MAILNEWS_IDENTITY    "Identity"
#define CAT_MAILNEWS_MESSAGES    "Messages"
#define CAT_MAILNEWS_MAILSERVER  "Mail Server"
#define CAT_MAILNEWS_NSERVER     "News Server"
#define CAT_MAILNEWS_ADDRESS     "Addressing"
#define CAT_MAILNEWS_COPIES      "Message Copies"
#define CAT_MAILNEWS_HTML        "HTML Formatting"
#define CAT_MAILNEWS_RECEIPTS    "Read Receipts"
#define CAT_EDITOR_PUBLISH       "Publish"
#define CAT_DISKSPACE            "Disk Space"
#define CAT_EDITOR_APPEARANCE    "Appearance"
#ifdef MOZ_LI
#define CAT_LI    "Location Independence"
#define CAT_LI_SERVER    "Location Independence Server"
#define CAT_LI_FILES    "Location Independence Files"
#endif /* MOZ_LI */
#ifdef PREFS_UNSUPPORTED
#define CAT_HELPFILES            "Help Files"
#define CAT_OFFLINE_NEWS         "News"
#endif /* PREFS_UNSUPPORTED */

#define CAT_RESNAME_APPEARANCE           "pref.appearance"
#define CAT_RESNAME_FONTS                "pref.fonts"
#define CAT_RESNAME_COLORS               "pref.colors"
#define CAT_RESNAME_BROWSER              "pref.browser"
#define CAT_RESNAME_LANG                 "pref.lang"
#define CAT_RESNAME_APPS                 "pref.applications"
#define CAT_RESNAME_MAILNEWS             "pref.mailNews"
#define CAT_RESNAME_MAILNEWS_IDENTITY    "pref.identity"
#define CAT_RESNAME_MAILNEWS_MESSAGES    "pref.messages"
#define CAT_RESNAME_MAILNEWS_MAILSERVER  "pref.mailServer"
#define CAT_RESNAME_MAILNEWS_NSERVER     "pref.newsServer"
#define CAT_RESNAME_MAILNEWS_ADDRESS     "pref.addressing"
#define CAT_RESNAME_MAILNEWS_COPIES      "pref.copies"
#define CAT_RESNAME_MAILNEWS_HTML        "pref.htmlmail"
#define CAT_RESNAME_MAILNEWS_RECEIPTS    "pref.readreceipts"
#define CAT_RESNAME_EDITOR               "pref.editor"
#define CAT_RESNAME_EDITOR_PUBLISH       "pref.editorPublish"
#define CAT_RESNAME_ADVANCED             "pref.advanced"
#define CAT_RESNAME_CACHE                "pref.cache"
#define CAT_RESNAME_PROXIES              "pref.proxies"
#define CAT_RESNAME_DISKSPACE            "pref.diskSpace"
#define CAT_RESNAME_EDITOR_APPEARANCE    "pref.editorAppearance"
#ifdef MOZ_LI
#define CAT_RESNAME_LI    "pref.liGeneral"
#define CAT_RESNAME_LI_SERVER    "pref.liServer"
#define CAT_RESNAME_LI_FILES    "pref.liFiles"
#endif /* MOZ_LI */
#ifdef PREFS_UNSUPPORTED
#define CAT_RESNAME_HELPFILES            "pref.helpFiles"
#define CAT_RESNAME_OFFLINE              "pref.offline"
#define CAT_RESNAME_OFFLINE_NEWS         "pref.offlineNews"
#endif /* PREFS_UNSUPPORTED */

#define CAT_RESCLASS_APPEARANCE           "Pref.Appearance"
#define CAT_RESCLASS_FONTS                "Pref.Fonts"
#define CAT_RESCLASS_COLORS               "Pref.Colors"
#define CAT_RESCLASS_BROWSER              "Pref.Browser"
#define CAT_RESCLASS_LANG                 "Pref.Lang"
#define CAT_RESCLASS_APPS                 "Pref.Applications"
#define CAT_RESCLASS_MAILNEWS             "Pref.MailNews"
#define CAT_RESCLASS_MAILNEWS_IDENTITY    "Pref.Identity"
#define CAT_RESCLASS_MAILNEWS_MESSAGES    "Pref.Messages"
#define CAT_RESCLASS_MAILNEWS_MAILSERVER  "Pref.MailServer"
#define CAT_RESCLASS_MAILNEWS_NSERVER     "Pref.NewsServer"
#define CAT_RESCLASS_MAILNEWS_ADDRESS     "Pref.Addressing"
#define CAT_RESCLASS_MAILNEWS_COPIES      "Pref.Copies"
#define CAT_RESCLASS_MAILNEWS_HTML        "Pref.HtmlMail"
#define CAT_RESCLASS_MAILNEWS_RECEIPTS    "Pref.ReadReceipts"
#define CAT_RESCLASS_EDITOR               "Pref.Editor"
#define CAT_RESCLASS_EDITOR_PUBLISH       "Pref.EditorPublish"
#define CAT_RESCLASS_ADVANCED             "Pref.Advanced"
#define CAT_RESCLASS_CACHE                "Pref.Cache"
#define CAT_RESCLASS_PROXIES              "Pref.Proxies"
#define CAT_RESCLASS_DISKSPACE            "Pref.DiskSpace"
#define CAT_RESCLASS_EDITOR_APPEARANCE    "Pref.EditorAppearance"
#ifdef MOZ_LI
#define CAT_RESCLASS_LI    "Pref.liGeneral"
#define CAT_RESCLASS_LI_SERVER    "Pref.liServer"
#define CAT_RESCLASS_LI_FILES    "Pref.liFiles"
#endif /* MOZ_LI */
#ifdef PREFS_UNSUPPORTED
#define CAT_RESCLASS_HELPFILES            "Pref.HelpFiles"
#define CAT_RESCLASS_OFFLINE              "Pref.Offline"
#define CAT_RESCLASS_OFFLINE_NEWS         "Pref.OfflineNews"
#endif

#define DESC_RESNAME_APPEARANCE           "prefDesc.appearance"
#define DESC_RESNAME_FONTS                "prefDesc.fonts"
#define DESC_RESNAME_COLORS               "prefDesc.colors"
#define DESC_RESNAME_BROWSER              "prefDesc.browser"
#define DESC_RESNAME_LANG                 "prefDesc.lang"
#define DESC_RESNAME_APPS                 "prefDesc.applications"
#define DESC_RESNAME_MAILNEWS             "prefDesc.mailNews"
#define DESC_RESNAME_MAILNEWS_IDENTITY    "prefDesc.identity"
#define DESC_RESNAME_MAILNEWS_MESSAGES    "prefDesc.messages"
#define DESC_RESNAME_MAILNEWS_MAILSERVER  "prefDesc.mailServer"
#define DESC_RESNAME_MAILNEWS_NSERVER     "prefDesc.newsServer"
#define DESC_RESNAME_MAILNEWS_ADDRESS     "prefDesc.addressing"
#define DESC_RESNAME_MAILNEWS_COPIES      "prefDesc.copies"
#define DESC_RESNAME_MAILNEWS_HTML        "prefDesc.htmlMail"
#define DESC_RESNAME_MAILNEWS_RECEIPTS    "prefDesc.readReceipts"
#define DESC_RESNAME_EDITOR               "prefDesc.editor"
#define DESC_RESNAME_EDITOR_PUBLISH       "prefDesc.editorPublish"
#define DESC_RESNAME_ADVANCED             "prefDesc.advanced"
#define DESC_RESNAME_CACHE                "prefDesc.cache"
#define DESC_RESNAME_PROXIES              "prefDesc.proxies"
#define DESC_RESNAME_DISKSPACE            "prefDesc.diskSpace"
#define DESC_RESNAME_EDITOR_APPEARANCE    "prefDesc.editorAppearance"
#ifdef MOZ_LI
#define DESC_RESNAME_LI    "prefDesc.liGeneral"
#define DESC_RESNAME_LI_SERVER    "prefDesc.liServer"
#define DESC_RESNAME_LI_FILES    "prefDesc.liFiles"
#endif /* MOZ_LI */
#ifdef PREFS_UNSUPPORTED
#define DESC_RESNAME_HELPFILES            "prefDesc.helpFiles"
#define DESC_RESNAME_OFFLINE              "prefDesc.offline"
#define DESC_RESNAME_OFFLINE_NEWS         "prefDesc.offlineNews"
#endif /* PREFS_UNSUPPORTED */

#define DESC_RESCLASS_APPEARANCE           "PrefDesc.Appearance"
#define DESC_RESCLASS_FONTS                "PrefDesc.Fonts"
#define DESC_RESCLASS_COLORS               "PrefDesc.Colors"
#define DESC_RESCLASS_BROWSER              "PrefDesc.Browser"
#define DESC_RESCLASS_LANG                 "PrefDesc.Lang"
#define DESC_RESCLASS_APPS                 "PrefDesc.Applications"
#define DESC_RESCLASS_MAILNEWS             "PrefDesc.MailNews"
#define DESC_RESCLASS_MAILNEWS_IDENTITY    "PrefDesc.Identity"
#define DESC_RESCLASS_MAILNEWS_MESSAGES    "PrefDesc.Messages"
#define DESC_RESCLASS_MAILNEWS_MAILSERVER  "PrefDesc.MailServer"
#define DESC_RESCLASS_MAILNEWS_NSERVER     "PrefDesc.newsServer"
#define DESC_RESCLASS_MAILNEWS_ADDRESS     "PrefDesc.Addressing"
#define DESC_RESCLASS_MAILNEWS_COPIES      "PrefDesc.Copies"
#define DESC_RESCLASS_MAILNEWS_HTML        "PrefDesc.HTMLMail"
#define DESC_RESCLASS_MAILNEWS_RECEIPTS    "PrefDesc.readReceipts"
#define DESC_RESCLASS_EDITOR               "PrefDesc.Editor"
#define DESC_RESCLASS_EDITOR_PUBLISH       "PrefDesc.EditorPublish"
#define DESC_RESCLASS_ADVANCED             "PrefDesc.Advanced"
#define DESC_RESCLASS_CACHE                "PrefDesc.Cache"
#define DESC_RESCLASS_PROXIES              "PrefDesc.Proxies"
#define DESC_RESCLASS_DISKSPACE            "PrefDesc.DiskSpace"
#define DESC_RESCLASS_EDITOR_APPEARANCE    "PrefDesc.EditorAppearance"
#ifdef MOZ_LI
#define DESC_RESCLASS_LI    "PrefDesc.liGeneral"
#define DESC_RESCLASS_LI_SERVER    "PrefDesc.liServer"
#define DESC_RESCLASS_LI_FILES    "PrefDesc.liFiles"
#endif /* MOZ_LI */
#ifdef PREFS_UNSUPPORTED
#define DESC_RESCLASS_HELPFILES            "PrefDesc.HelpFiles"
#define DESC_RESCLASS_OFFLINE              "PrefDesc.Offline"
#define DESC_RESCLASS_OFFLINE_NEWS         "PrefDesc.OfflineNews"
#endif /* PREFS_UNSUPPORTED */

typedef enum _prefsPageType {
	PAGE_TYPE_APPEARANCE,
	PAGE_TYPE_FONTS,
	PAGE_TYPE_COLORS,
	PAGE_TYPE_BROWSER,
	PAGE_TYPE_LANG,
	PAGE_TYPE_APPS,
	PAGE_TYPE_MAILNEWS,
	PAGE_TYPE_MAILNEWS_IDENTITY,
	PAGE_TYPE_MAILNEWS_MESSAGES,
	PAGE_TYPE_MAILNEWS_MAILSERVER,
	PAGE_TYPE_MAILNEWS_NSERVER,
	PAGE_TYPE_MAILNEWS_ADDRESS,
	PAGE_TYPE_MAILNEWS_COPIES,
	PAGE_TYPE_MAILNEWS_HTML,
	PAGE_TYPE_MAILNEWS_RECEIPTS,
	PAGE_TYPE_EDITOR,
	PAGE_TYPE_EDITOR_APPEARANCE,
	PAGE_TYPE_EDITOR_PUBLISH,
	PAGE_TYPE_ADVANCED,
	PAGE_TYPE_CACHE,
	PAGE_TYPE_PROXIES,
	PAGE_TYPE_DISKSPACE
#ifdef MOZ_LI
	,
	PAGE_TYPE_LI,	
	PAGE_TYPE_LI_SERVER,	
	PAGE_TYPE_LI_FILES
#endif
#ifdef PREFS_UNSUPPORTED
	PAGE_TYPE_HELPFILES
	PAGE_TYPE_OFFLINE,
	PAGE_TYPE_OFFLINE_NEWS,
#endif /* PREFS_UNSUPPORTED */
} PrefsPageType;

struct _prefsCategory {
	char                  *name;
	char                  *catResName;
	char                  *catResClass;
	char                  *descResName;
	char                  *descResClass;
	Boolean                leaf;
	struct _prefsCategory *children;
	int                    numChildren;
	int                    type;
};

struct _prefsCategory prefsAppearance[] = {
	{
		CAT_FONTS,      
		CAT_RESNAME_FONTS,      
		CAT_RESCLASS_FONTS,      
		DESC_RESNAME_FONTS,      
		DESC_RESCLASS_FONTS,      
		TRUE,
		NULL,
		0,
		PAGE_TYPE_FONTS,
	},
	{
		CAT_COLORS,      
		CAT_RESNAME_COLORS,      
		CAT_RESCLASS_COLORS,      
		DESC_RESNAME_COLORS,      
		DESC_RESCLASS_COLORS,      
		TRUE,
		NULL,
		0,
		PAGE_TYPE_COLORS,
	},
};

struct _prefsCategory prefsMailNews[] = {
	{
		CAT_MAILNEWS_IDENTITY, 
		CAT_RESNAME_MAILNEWS_IDENTITY, 
		CAT_RESCLASS_MAILNEWS_IDENTITY, 
		DESC_RESNAME_MAILNEWS_IDENTITY, 
		DESC_RESCLASS_MAILNEWS_IDENTITY, 
		TRUE,
		NULL,
		0,
		PAGE_TYPE_MAILNEWS_IDENTITY,
	},
	{
		CAT_MAILNEWS_MAILSERVER, 
		CAT_RESNAME_MAILNEWS_MAILSERVER, 
		CAT_RESCLASS_MAILNEWS_MAILSERVER, 
		DESC_RESNAME_MAILNEWS_MAILSERVER, 
		DESC_RESCLASS_MAILNEWS_MAILSERVER, 
		TRUE,  
		NULL, 
		0,
		PAGE_TYPE_MAILNEWS_MAILSERVER,
	},
	{
		CAT_MAILNEWS_NSERVER, 
		CAT_RESNAME_MAILNEWS_NSERVER, 
		CAT_RESCLASS_MAILNEWS_NSERVER, 
		DESC_RESNAME_MAILNEWS_NSERVER, 
		DESC_RESCLASS_MAILNEWS_NSERVER, 
		TRUE,  
		NULL, 
		0,
		PAGE_TYPE_MAILNEWS_NSERVER,
	},
	{
		CAT_MAILNEWS_ADDRESS,    
		CAT_RESNAME_MAILNEWS_ADDRESS,    
		CAT_RESCLASS_MAILNEWS_ADDRESS,    
		DESC_RESNAME_MAILNEWS_ADDRESS,    
		DESC_RESCLASS_MAILNEWS_ADDRESS,    
		TRUE,  
		NULL,
		0, 
		PAGE_TYPE_MAILNEWS_ADDRESS,
	},
	{
		CAT_MAILNEWS_MESSAGES,
		CAT_RESNAME_MAILNEWS_MESSAGES,
		CAT_RESCLASS_MAILNEWS_MESSAGES,
		DESC_RESNAME_MAILNEWS_MESSAGES,
		DESC_RESCLASS_MAILNEWS_MESSAGES,
		TRUE,  
		NULL,
		0,
		PAGE_TYPE_MAILNEWS_MESSAGES,
	},
	{
        CAT_MAILNEWS_COPIES,
        CAT_RESNAME_MAILNEWS_COPIES,
        CAT_RESCLASS_MAILNEWS_COPIES,
        DESC_RESNAME_MAILNEWS_COPIES,
        DESC_RESNAME_MAILNEWS_COPIES,
        TRUE,
        NULL,
        0,
        PAGE_TYPE_MAILNEWS_COPIES,
    },
	{
        CAT_MAILNEWS_HTML,
        CAT_RESNAME_MAILNEWS_HTML,
        CAT_RESCLASS_MAILNEWS_HTML,
        DESC_RESNAME_MAILNEWS_HTML,
        DESC_RESNAME_MAILNEWS_HTML,
        TRUE,
        NULL,
        0,
        PAGE_TYPE_MAILNEWS_HTML,
    },
	{
        CAT_MAILNEWS_RECEIPTS,
        CAT_RESNAME_MAILNEWS_RECEIPTS,
        CAT_RESCLASS_MAILNEWS_RECEIPTS,
        DESC_RESNAME_MAILNEWS_RECEIPTS,
        DESC_RESNAME_MAILNEWS_RECEIPTS,
        TRUE,
        NULL,
        0,
        PAGE_TYPE_MAILNEWS_RECEIPTS,
    },
#ifdef MOZ_MAIL_NEWS
	{
		CAT_DISKSPACE,      
		CAT_RESNAME_DISKSPACE,      
		CAT_RESCLASS_DISKSPACE,      
		DESC_RESNAME_DISKSPACE,      
		DESC_RESCLASS_DISKSPACE,      
		TRUE,
		NULL,
		0,
		PAGE_TYPE_DISKSPACE,
	},
#endif // MOZ_MAIL_NEWS
};

struct _prefsCategory prefsEditor[] = {
	{
		CAT_EDITOR_APPEARANCE,      
		CAT_RESNAME_EDITOR_APPEARANCE,      
		CAT_RESCLASS_EDITOR_APPEARANCE,      
		DESC_RESNAME_EDITOR_APPEARANCE,      
		DESC_RESCLASS_EDITOR_APPEARANCE,      
		TRUE,
		NULL,
		0,
		PAGE_TYPE_EDITOR_APPEARANCE,
	},
	{
		CAT_EDITOR_PUBLISH,      
		CAT_RESNAME_EDITOR_PUBLISH,      
		CAT_RESCLASS_EDITOR_PUBLISH,      
		DESC_RESNAME_EDITOR_PUBLISH,      
		DESC_RESCLASS_EDITOR_PUBLISH,      
		TRUE,
		NULL,
		0,
		PAGE_TYPE_EDITOR_PUBLISH,
	},
};

#ifdef PREFS_UNSUPPORTED
struct _prefsCategory prefsOffline[] = {
	{
		CAT_OFFLINE_NEWS,      
		CAT_RESNAME_OFFLINE_NEWS,      
		CAT_RESCLASS_OFFLINE_NEWS,      
		DESC_RESNAME_OFFLINE_NEWS,      
		DESC_RESCLASS_OFFLINE_NEWS,      
		TRUE,
		NULL,
		0,
		PAGE_TYPE_OFFLINE_NEWS,
	},
};
#endif /* PREFS_UNSUPPORTED */

struct _prefsCategory prefsBrowser[] = {
	{
		CAT_LANG, 
		CAT_RESNAME_LANG, 
		CAT_RESCLASS_LANG, 
		DESC_RESNAME_LANG, 
		DESC_RESCLASS_LANG, 
		TRUE,
		NULL,
		0,
		PAGE_TYPE_LANG,
	},
	{
		CAT_APPS, 
		CAT_RESNAME_APPS, 
		CAT_RESCLASS_APPS, 
		DESC_RESNAME_APPS, 
		DESC_RESCLASS_APPS, 
		TRUE,
		NULL,
		0,
		PAGE_TYPE_APPS,
	},
};

struct _prefsCategory prefsAdvanced[] = {
	{
		CAT_CACHE,
		CAT_RESNAME_CACHE,
		CAT_RESCLASS_CACHE,
		DESC_RESNAME_CACHE,
		DESC_RESCLASS_CACHE,
		TRUE,
		NULL,
		0,
		PAGE_TYPE_CACHE,
	},
	{
		CAT_PROXIES,
		CAT_RESNAME_PROXIES,
		CAT_RESCLASS_PROXIES,
		DESC_RESNAME_PROXIES,
		DESC_RESCLASS_PROXIES,
		TRUE,
		NULL,
		0,
		PAGE_TYPE_PROXIES,
	},
#ifdef MOZ_MAIL_NEWS
	{
		CAT_DISKSPACE,      
		CAT_RESNAME_DISKSPACE,      
		CAT_RESCLASS_DISKSPACE,      
		DESC_RESNAME_DISKSPACE,      
		DESC_RESCLASS_DISKSPACE,      
		TRUE,
		NULL,
		0,
		PAGE_TYPE_DISKSPACE,
	},
#endif // MOZ_MAIL_NEWS
#ifdef PREFS_UNSUPPORTED
	{
		CAT_HELPFILES,      
		CAT_RESNAME_HELPFILES,      
		CAT_RESCLASS_HELPFILES,      
		DESC_RESNAME_HELPFILES,      
		DESC_RESCLASS_HELPFILES,      
		TRUE,
		NULL,
		0,
		PAGE_TYPE_HELPFILES,
	},
#endif /* PREFS_UNSUPPORTED */
};

#ifdef MOZ_LI
struct _prefsCategory prefsLI[] = {
	{
		CAT_LI_SERVER,
		CAT_RESNAME_LI_SERVER,      
		CAT_RESCLASS_LI_SERVER,      
		DESC_RESNAME_LI_SERVER,      
		DESC_RESCLASS_LI_SERVER,      
		TRUE,
		NULL,
		0,
		PAGE_TYPE_LI_SERVER,
	},
	{
		CAT_LI_FILES,
		CAT_RESNAME_LI_FILES,
		CAT_RESCLASS_LI_FILES,
		DESC_RESNAME_LI_FILES,
		DESC_RESCLASS_LI_FILES,
		TRUE,
		NULL,
		0,
		PAGE_TYPE_LI_FILES,
	}
};
#endif /* MOZ_LI */

struct _prefsCategory prefsCategories[] = {
	{
		CAT_APPEARANCE,     
		CAT_RESNAME_APPEARANCE,     
		CAT_RESCLASS_APPEARANCE,     
		DESC_RESNAME_APPEARANCE,     
		DESC_RESCLASS_APPEARANCE,     
		FALSE, 
		prefsAppearance,  
		XtNumber(prefsAppearance), 
		PAGE_TYPE_APPEARANCE,
	},
	{
		CAT_BROWSER,         
		CAT_RESNAME_BROWSER,         
		CAT_RESCLASS_BROWSER,         
		DESC_RESNAME_BROWSER,         
		DESC_RESCLASS_BROWSER,         
		FALSE,  
		prefsBrowser,
		XtNumber(prefsBrowser), 
		PAGE_TYPE_BROWSER,
	},
#ifndef MOZ_MAIL_NEWS
	{
		CAT_MAILNEWS_IDENTITY, 
		CAT_RESNAME_MAILNEWS_IDENTITY, 
		CAT_RESCLASS_MAILNEWS_IDENTITY, 
		DESC_RESNAME_MAILNEWS_IDENTITY, 
		DESC_RESCLASS_MAILNEWS_IDENTITY, 
		FALSE,
		NULL,
		0,
		PAGE_TYPE_MAILNEWS_IDENTITY,
	},
#endif // MOZ_MAIL_NEWS
#ifdef MOZ_MAIL_NEWS
	{
		CAT_MAILNEWS,    
		CAT_RESNAME_MAILNEWS,    
		CAT_RESCLASS_MAILNEWS,    
		DESC_RESNAME_MAILNEWS,    
		DESC_RESCLASS_MAILNEWS,    
		FALSE,   
		prefsMailNews, 
		XtNumber(prefsMailNews),
		PAGE_TYPE_MAILNEWS,
	},
#endif // MOZ_MAIL_NEWS
#ifdef MOZ_LI
	{
		CAT_LI,
		CAT_RESNAME_LI,
		CAT_RESCLASS_LI,      
		DESC_RESNAME_LI,      
		DESC_RESCLASS_LI,      
		FALSE,
		prefsLI,
		XtNumber(prefsLI),
		PAGE_TYPE_LI,
	},
#endif /* MOZ_LI */
#ifdef EDITOR
	{
		CAT_EDITOR,               
		CAT_RESNAME_EDITOR,               
		CAT_RESCLASS_EDITOR,               
		DESC_RESNAME_EDITOR,               
		DESC_RESCLASS_EDITOR,               
		FALSE,   
		prefsEditor, 
		XtNumber(prefsEditor),
		PAGE_TYPE_EDITOR,
	},
#endif // EDITOR
#ifdef PREFS_UNSUPPORTED
	{
		CAT_OFFLINE,               
		CAT_RESNAME_OFFLINE,               
		CAT_RESCLASS_OFFLINE,               
		DESC_RESNAME_OFFLINE,               
		DESC_RESCLASS_OFFLINE,               
		FALSE,   
		prefsOffline, 
		XtNumber(prefsOffline),
		PAGE_TYPE_OFFLINE,
	},
#endif /* PREFS_UNSUPPORTED */
	{
		CAT_ADVANCED,
		CAT_RESNAME_ADVANCED,               
		CAT_RESCLASS_ADVANCED,               
		DESC_RESNAME_ADVANCED,               
		DESC_RESCLASS_ADVANCED,               
		FALSE,   
		prefsAdvanced, 
		XtNumber(prefsAdvanced),
		PAGE_TYPE_ADVANCED,
	},
};

extern "C"
{
	Widget    fe_CreateFramedForm(Widget, char*, Arg*, Cardinal);
	char     *XP_GetString(int i);
}

extern "C" void
fe_showPreferences(XFE_Component *topLevel, MWContext *context)
{
	XFE_PrefsDialog   *theDialog = 0;
	Widget             mainw = topLevel->getBaseWidget();

	// Instantiate a preferences dialog

	if ((theDialog = new XFE_PrefsDialog(mainw, "prefs", context)) == 0) {
	    fe_perror(context, XP_GetString(XFE_OUT_OF_MEMORY_URL));
	    return;
	}

	// Open some categories

	theDialog->openCategory(CAT_APPEARANCE);

	// Pop up the preferences dialog

	theDialog->show();
	
	// Set the page to Appearance
	
	PrefsCategory *appearance = theDialog->getCategoryByName(CAT_APPEARANCE);
	theDialog->setCurrentCategory(appearance);
}

extern "C" void
fe_showMailNewsPreferences(XFE_Component *topLevel, MWContext *context)
{
	XFE_PrefsDialog   *theDialog = 0;
	Widget             mainw = topLevel->getBaseWidget();

	// Instantiate a preferences dialog

	if ((theDialog = new XFE_PrefsDialog(mainw, "prefs", context)) == 0) {
	    fe_perror(context, XP_GetString(XFE_OUT_OF_MEMORY_URL));
	    return;
	}

	// Open some categories

	theDialog->openCategory(CAT_APPEARANCE);
	theDialog->openCategory(CAT_MAILNEWS);

	// Pop up the preferences dialog

	theDialog->show();

	// Set the page to Appearance
	
	PrefsCategory *appearance = theDialog->getCategoryByName(CAT_MAILNEWS);
	theDialog->setCurrentCategory(appearance);
}

extern "C" void
fe_showMailServerPreferences(XFE_Component *topLevel, MWContext *context)
{
	XFE_PrefsDialog   *theDialog = 0;
	Widget             mainw = topLevel->getBaseWidget();

	// Instantiate a preferences dialog

	if ((theDialog = new XFE_PrefsDialog(mainw, "prefs", context)) == 0) {
	    fe_perror(context, XP_GetString(XFE_OUT_OF_MEMORY_URL));
	    return;
	}

	// Open some categories

	theDialog->openCategory(CAT_MAILNEWS);

	// Pop up the preferences dialog

	theDialog->show();

	// Set the page to Mail Servers
	
	PrefsCategory *mailserver_category = theDialog->getCategoryByName(CAT_MAILNEWS_MAILSERVER);
	theDialog->setCurrentCategory(mailserver_category);
}

extern "C" void
fe_showMailIdentityPreferences(XFE_Component *topLevel, MWContext *context)
{
	XFE_PrefsDialog   *theDialog = 0;
	Widget             mainw = topLevel->getBaseWidget();

	// Instantiate a preferences dialog

	if ((theDialog = new XFE_PrefsDialog(mainw, "prefs", context)) == 0) {
	    fe_perror(context, XP_GetString(XFE_OUT_OF_MEMORY_URL));
	    return;
	}

	// Open some categories

	theDialog->openCategory(CAT_MAILNEWS);

	// Pop up the preferences dialog

	theDialog->show();

	// Set the page to Mail Identity
	
	PrefsCategory *identity_category = theDialog->getCategoryByName(CAT_MAILNEWS_IDENTITY);
	theDialog->setCurrentCategory(identity_category);
}

extern "C" void
fe_showEditorPreferences(XFE_Component *topLevel, MWContext *context)
{
	XFE_PrefsDialog   *theDialog = 0;
	Widget             mainw = topLevel->getBaseWidget();

	// Instantiate a preferences dialog

	if ((theDialog = new XFE_PrefsDialog(mainw, "prefs", context)) == 0) {
	    fe_perror(context, XP_GetString(XFE_OUT_OF_MEMORY_URL));
	    return;
	}

	// Open some categories

	theDialog->openCategory(CAT_APPEARANCE);
	theDialog->openCategory(CAT_EDITOR);

	// Pop up the preferences dialog

	theDialog->show();

	// Set the page to Appearance
	
	PrefsCategory *appearance = theDialog->getCategoryByName(CAT_EDITOR);
	theDialog->setCurrentCategory(appearance);
}

// ==================== Prefs Page Member Functions ====================

// Member:       XFE_PrefsPage
// Description:  Constructor
// Inputs:
// Side effects: Creates a preferences page

XFE_PrefsPage::XFE_PrefsPage(XFE_PrefsDialog *prefsDialog) :  // prefs dialog
	m_context(0),
	m_prefsDialog(prefsDialog),
	m_wPageForm(0),
	m_wPage(0),
	m_created(FALSE),
	m_initialized(FALSE),
	m_doInitInMap(FALSE),
	m_changed(FALSE)
{
	m_wPageForm = m_prefsDialog->getPageForm();
	m_context = m_prefsDialog->getContext();
}

XFE_PrefsPage::XFE_PrefsPage(Widget w) :  // prefs dialog
	m_context(0),
	m_prefsDialog(0),
	m_wPageForm(w),
	m_wPage(0),
	m_created(FALSE),
	m_initialized(FALSE),
	m_doInitInMap(FALSE),
	m_changed(FALSE)
{
	m_wPageForm = w;
}

// Member:       ~XFE_PrefsPage
// Description:  Destructor
// Inputs:
// Side effects: Destoy a preferences page

XFE_PrefsPage::~XFE_PrefsPage()
{
}

// Member:       setCreated
// Description: 
// Inputs:
// Side effects: 

void XFE_PrefsPage::setCreated(Boolean flag)
{
	m_created = flag;
}

// Member:       setChanged
// Description: 
// Inputs:
// Side effects: 

void XFE_PrefsPage::setChanged(Boolean flag)
{
	m_changed = flag;
}

// Member:       setInitialized
// Description: 
// Inputs:
// Side effects: 

void XFE_PrefsPage::setInitialized(Boolean flag)
{
	m_initialized = flag;
}

// Member:       setDoInitInMap
// Description: 
// Inputs:
// Side effects: 

void XFE_PrefsPage::setDoInitInMap(Boolean flag)
{
	m_doInitInMap = flag;
}

// Member:       isCreated
// Description: 
// Inputs:
// Side effects: 

Boolean XFE_PrefsPage::isCreated() 
{
	return m_doInitInMap;
}

// Member:       isInitialized
// Description: 
// Inputs:
// Side effects: 

Boolean XFE_PrefsPage::isInitialized() 
{
	return m_initialized;
}

// Member:       doInitInMap
// Description: 
// Inputs:
// Side effects: 

Boolean XFE_PrefsPage::doInitInMap() 
{
	return m_doInitInMap;
}

// Member:       isChanged
// Description: 
// Inputs:
// Side effects: 

Boolean XFE_PrefsPage::isChanged() 
{
	return m_changed;
}

// Member:       getContext
// Description: 
// Inputs:
// Side effects: 

MWContext *XFE_PrefsPage::getContext() 
{
	return m_context;
}

// Member:       getPrefsDialog
// Description: 
// Inputs:
// Side effects: 

XFE_PrefsDialog *XFE_PrefsPage::getPrefsDialog() 
{
	return m_prefsDialog;
}

// Member:       getWidth
// Description: 
// Inputs:
// Side effects: 

Dimension XFE_PrefsPage::getWidth() 
{
	return XfeWidth(m_wPage);
}

// Member:       getHeight
// Description: 
// Inputs:
// Side effects: 

Dimension XFE_PrefsPage::getHeight()
{
	return XfeHeight(m_wPage);
}

// Member:       map
// Description:  Displays page
// Inputs:
// Side effects: 

void XFE_PrefsPage::map()
{
	// Create the page if not created already

	if (! isCreated()) create();
	
	// Initialize the page

	if (doInitInMap()){
		if (! isInitialized()) init();
		setChanged(TRUE);
	}
	
	// Display the page

	XtManageChild(m_wPage);
}

// Member:       unmap
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsPage::unmap()
{
	XtUnmanageChild(m_wPage);
}

// Member:       verify
// Description:  
// Inputs:
// Side effects: 

Boolean XFE_PrefsPage::verify()
{
	return TRUE;
}

// ==================== Prefs Dialog Member Functions ====================

// Member:       XFE_PrefsDialog
// Description:  Constructor
// Inputs:
// Side effects: Creates a preferences dialog

XFE_PrefsDialog::XFE_PrefsDialog(Widget     parent,      // dialog parent
								 char      *name,        // dialog name
								 MWContext *context,     // context
								 Boolean    modal)       // modal dialog?
	: XFE_Dialog(parent, 
				 name,
				 TRUE,     // ok
				 TRUE,     // cancel
				 TRUE,     // help
				 FALSE,    // apply
				 FALSE,    // separator
				 modal     // modal
				 ),
	  m_wPageLabel(0),
	  m_wPageTitle(0),
	  m_wPageForm(0),
	  m_wOutline(0),
	  m_context(context),
	  m_numPages(0),
	  m_pages(0),
	  m_columnWidths(0),
	  m_outlineHeaders(0),
	  m_numCategories(0),
	  m_categories(0),
	  m_visibleCategoryCount(0),
	  m_currentCategory(0),
	  m_initPageLabel(FALSE)
{
	// Create areas for use later

	createRegions();

	// Initialize categories

	initCategories();

	// Add callbacks

	XtAddCallback (m_chrome, XmNokCallback, prefsMainCb_ok, this);
	XtAddCallback (m_chrome, XmNcancelCallback, prefsMainCb_cancel, this);
	XtAddCallback (m_chrome, XmNhelpCallback, prefsMainCb_help, this);
	XtAddCallback (m_chrome, XmNdestroyCallback, prefsMainCb_destroy, this);
}

// Member:       prefsMainCb_ok
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsDialog::prefsMainCb_ok(Widget    w,
									 XtPointer closure,
									 XtPointer callData)
{
	XFE_PrefsDialog *theDialog = (XFE_PrefsDialog *)closure;

	// Verify the current page

	PrefsCategory *currentCategory = theDialog->getCurrentCategory();
	if (currentCategory &&
		currentCategory->page) {
		if (! currentCategory->page->verify())
			return;
	}

	// hide ourself so any dialogs popped up will become children
	// of the parent frame.
	theDialog->hide();

	// Save all the changes 

	theDialog->saveChanges();

	// Simulate a cancel on Ok
	theDialog->prefsMainCb_cancel(w, closure, callData);
}

// Member:       prefsMainCb_cancel
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsDialog::prefsMainCb_cancel(Widget    /* w */,
										 XtPointer closure,
										 XtPointer /* callData */)
{
	XFE_PrefsDialog *theDialog = (XFE_PrefsDialog *)closure;

	// Remove all the callbacks 

	XtRemoveCallback(theDialog->m_chrome, XmNokCallback, prefsMainCb_ok, theDialog);
	XtRemoveCallback(theDialog->m_chrome, XmNcancelCallback, prefsMainCb_cancel, theDialog);
	XtRemoveCallback(theDialog->m_chrome, XmNhelpCallback, prefsMainCb_help, theDialog);
	XtRemoveCallback(theDialog->m_chrome, XmNdestroyCallback, prefsMainCb_destroy, theDialog);

	// Delete the dialog

	delete theDialog;
}

// Member:       prefsMainCb_help
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsDialog::prefsMainCb_help(Widget    /* w */,
									   XtPointer /* closure */,
									   XtPointer /* callData */)
{
	//	XFE_PrefsDialog *theDialog = (XFE_PrefsDialog *)closure;
}

// Member:       prefsMainCb_destroy
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsDialog::prefsMainCb_destroy(Widget    w,
										  XtPointer closure,
										  XtPointer callData)
{
	XFE_PrefsDialog *theDialog = (XFE_PrefsDialog *)closure;

	// Simulate a cancel on destroy
	theDialog->prefsMainCb_cancel(w, closure, callData);
}

// Member:       ~XFE_PrefsDialog
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsDialog::~XFE_PrefsDialog()
{
	// Clean up

	for (int i = 0; i < m_numPages; i++)
		delete m_pages[i];
	delete [] m_pages;

	delete m_columnWidths;
	if (m_outlineHeaders->header_strings)
	    XP_FREE(m_outlineHeaders->header_strings);
	XP_FREE(m_outlineHeaders);
}

// Member:       getContext
// Description:  Gets MWContext (for now) 
// Inputs:
// Side effects: 

MWContext *XFE_PrefsDialog::getContext()
{
	return m_context;
}

// Member:       getVisibleCategoryCount
// Description:  Gets number of visible categories
// Inputs:
// Side effects: 

int XFE_PrefsDialog::getVisibleCategoryCount()
{
	if (m_visibleCategoryCount == 0) {
		calcVisibleCategoryCount();
	}

	return m_visibleCategoryCount;
}

// Member:       show
// Description:  Pop up dialog
// Inputs:
// Side effects: 

void XFE_PrefsDialog::show()
{
	int i;

	refreshCategories();

	// Make sure we create all the pages first, so we can determine
	// the height and width of the preferences dialog.
	// Do not initialize the page to save time

	setDoInitInSetPage(FALSE);

	for (i = 0; i < m_numPages; i++) {
		m_pages[i]->map();
	}

	setDoInitInSetPage(TRUE);

	// Manage the top level

	XFE_Dialog::show();

	// Determine the height and width of the preferences dialog

	int page_width = calculateWidth();

	int page_height = calculateHeight();

	Dimension margin_width;
	Dimension margin_height;

	XtVaGetValues(m_wPageForm,
				  XmNmarginWidth, &margin_width,
				  XmNmarginHeight, &margin_height,
				  NULL);

	XtVaSetValues(m_wPageForm,
				  XmNwidth, page_width + 2 * margin_width,
				  XmNheight, page_height + 2 * margin_height,
				  XmNresizePolicy, XmRESIZE_NONE,
				  NULL);

	// Reset the categories 

	for (i = 0; i < m_numPages; i++) {
		m_pages[i]->unmap();
	}

	setCurrentCategory(NULL);

	// Move focus to the OK button

	XmProcessTraversal(m_okButton, XmTRAVERSE_CURRENT);
}

// Member:       setContext
// Description:  Sets MWContext (for now) 
// Inputs:
// Side effects: 

void XFE_PrefsDialog::setContext(MWContext *context)
{
	m_context = context;
}

// Member:       calcVisibleCategoryCount
// Description:  Calculates number of visible categories
// Inputs:
// Side effects: 

void XFE_PrefsDialog::calcVisibleCategoryCount()
{
	int  i;
	int  count = 0;

	for (i = 0; i < m_numCategories; i++) {
		count++;
		if (m_categories[i].open) {
			count += m_categories[i].numChildren;
		}
	}
	m_visibleCategoryCount = count;
}

// Member:       setCurrentCategory
// Description:  Set currently selected category 
// Inputs:
// Side effects: 

void XFE_PrefsDialog::setCurrentCategory(PrefsCategory *entry)
{
	XmString labelString;

	// If the page label is not set reverse video, do it now.
	// Reverse video the label if we are setting a real page.

	if (entry != NULL) {
		if (m_initPageLabel == FALSE) {
			Pixel bg;
			Pixel fg;
			XtVaGetValues(m_wPageTitle, 
						  XmNbackground, &bg,
						  XmNforeground, &fg,
						  NULL);
			XtVaSetValues(m_wPageTitle,
						  XmNforeground, bg,
						  XmNbackground, fg,
						  NULL);
			m_initPageLabel = TRUE;
		}
	}

	// Unset old page

	if (m_currentCategory) {
		if (m_currentCategory->page != 0) {
			if (! m_currentCategory->page->verify())
				return;
			m_currentCategory->page->unmap();
		}
	}

	m_currentCategory = entry;

	if (! entry) {

		// unset the page label and description

		labelString =
			XmStringCreateLtoR(" ", (XmStringCharSet)XmFONTLIST_DEFAULT_TAG);
		XtVaSetValues(m_wPageLabel,
					  XmNlabelString, labelString,
					  NULL);
		XmStringFree(labelString);

		labelString =
			XmStringCreateLtoR(" ", (XmStringCharSet)XmFONTLIST_DEFAULT_TAG);
		XtVaSetValues(m_wPageTitle,
					  XmNlabelString, labelString,
					  NULL);
		XmStringFree(labelString);

		return;
	}

	// Set the page label and description.
	// Page label is not displayed any more.

    labelString = XmStringCreateLtoR(" ", (XmStringCharSet)XmFONTLIST_DEFAULT_TAG);
	XtVaSetValues(m_wPageLabel,
				  XmNlabelString, labelString,
				  NULL);
	XmStringFree(labelString);

	XmString s1;
	XmString s2;
	XmString s3;
	XmString labelString2;
	
	s1 = XmStringCreate(entry->categoryName, TAG_BOLD);
	s2 = XmStringCreate("   ", TAG_NORMAL);
	if (entry->categoryDesc)
		s3 = XmStringCreate(entry->categoryDesc, TAG_NORMAL);
	else
		s3 = XmStringCreate(" ", TAG_NORMAL);
	labelString = XmStringConcat(s1, s2);
	labelString2 = XmStringConcat(labelString, s3);
	XtVaSetValues(m_wPageTitle,
				  XmNlabelString, labelString2,
				  NULL);

	XmStringFree(s1);
	XmStringFree(s2);
	XmStringFree(s3);
	XmStringFree(labelString);
	XmStringFree(labelString2);

	// Set the page

	if (entry->page != 0) {
		entry->page->map();
		refreshCategories();
	}
}

// Member:       openCategory
// Description:  Open a category
// Inputs:
// Side effects: 

void XFE_PrefsDialog::openCategory(const char *catName)
{
	PrefsCategory *entry = getCategoryByName(catName);
	
	if (entry) {
		entry->open = TRUE;
	}
}

// Member:       closeCategory
// Description:  Close a category
// Inputs:
// Side effects: 

void XFE_PrefsDialog::closeCategory(const char *catName)
{
	PrefsCategory *entry = getCategoryByName(catName);
	
	if (entry) {
		entry->open = FALSE;
	}
}

// Member:       setDoInitInSetPage
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsDialog::setDoInitInSetPage(Boolean doInit)
{
	for (int i = 0; i < m_numPages; i++) {
		m_pages[i]->setDoInitInMap(doInit);
	}
}

// Member:       saveChanges
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsDialog::saveChanges()
{
	int32 tempInt; // for news xaction pref below

	for (int i = 0; i < m_numPages; i++) {
		if (m_pages[i]->isChanged()) {
			m_pages[i]->save();
		}
	}

	// Save the preferences at the end, so that if we've found some 
	// setting that crashes, it won't get saved...

	if (! fe_CheckVersionAndSavePrefs((char *) fe_globalData.user_prefs_file, &fe_globalPrefs))
        fe_perror(getContext(), XP_GetString( XFE_ERROR_SAVING_OPTIONS));

	// ### mwelch 97 July
	// Since the default news server preference spans across three different
	// preferences, we need to always tweak the "news.server_change_xaction"
	// pref, in order to signal to the back end that the news server prefs
	// have settled into a steady state.
	PREF_GetIntPref("news.server_change_xaction", &tempInt);
	PREF_SetIntPref("news.server_change_xaction", ++tempInt);
}

// Member:       calculateWidth
// Description:  
// Inputs:
// Side effects: 

Dimension XFE_PrefsDialog::calculateWidth()
{
	Dimension widest = 0;
	Dimension width;

    if (!m_pages) return 0;
	for (int i = 0; i < m_numPages; i++) {
		if (m_pages[i]) {
            width = m_pages[i]->getWidth();
			if (width > widest) widest = width;
		}
	}
	return widest;
}

// Member:       calculateHeight
// Description:  
// Inputs:
// Side effects: 

Dimension XFE_PrefsDialog::calculateHeight()
{
	Dimension tallest = 0;
	Dimension height;

	for (int i = 0; i < m_numPages; i++) {
		if (m_pages[i]) {
			height = m_pages[i]->getHeight();
			if (height > tallest) tallest = height;
		}
	}
	return tallest;
}

// ==================== Prefs Dialog Protected Member Functions ====================


// ==================== Prefs Dialog Private Member Functions ====================

// Member:       createRegions
// Description:  Creates regions for the preferences dialog
// Inputs:
// Side effects: 

void XFE_PrefsDialog::createRegions()
{
	Arg      args[50];
	Cardinal n;

	// Top level form

	Widget form;

	form = XtVaCreateWidget("form", xmFormWidgetClass, m_chrome,
							XmNtopAttachment, XmATTACH_FORM,
							XmNleftAttachment, XmATTACH_FORM,
							XmNrightAttachment, XmATTACH_FORM,
							XmNbottomAttachment, XmATTACH_FORM,
							NULL);

	// Category area

	Widget leftPane;

	leftPane = XtVaCreateWidget("leftPane", xmFormWidgetClass, form,
								XmNtopAttachment, XmATTACH_FORM,
								XmNleftAttachment, XmATTACH_FORM,
								XmNbottomAttachment, XmATTACH_FORM,
								NULL);

	// Page area

	Widget rightPane;

	rightPane = XtVaCreateWidget("rightPane", xmFormWidgetClass, form,
								 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
								 XmNtopWidget, leftPane,
								 XmNleftAttachment, XmATTACH_WIDGET,
								 XmNleftWidget, leftPane,
								 XmNrightAttachment, XmATTACH_FORM,
								 XmNbottomAttachment, XmATTACH_FORM,
								 NULL);

	// Category area - category label

	Widget categoryLabel;

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	categoryLabel = XmCreateLabelGadget(leftPane, "categoryLabel", args, n);
	XtManageChild(categoryLabel);

	// Category area - Categories

#if 0
	m_outlinable = new XFE_PrefsOutlinable;

	// TODO: pass attachments and name to outliner
	m_outliner = new XFE_Outliner(m_outlinable,        /* outlinable */
				      this, /* toplevel */
								  leftPane,            /* parent */
								  TRUE,                /* constantSize */
								  NUM_OUTLINER_COLS,   /* number of columns */
								  OUTLINER_COLWIDTHS); /* column widths */
#endif

#if 0
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, categoryLabel); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, categoryLabel); n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_NONE); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_NONE); n++;
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_NONE); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_NONE); n++;
#endif

	n = 0;
	m_wOutline = fe_GridCreate(m_context,        /* context */
							   leftPane,         /* parent */
							   "categoryList",   /* name */
							   args,             /* av */
							   n,                /* ac */
							   0,                /* maxindentdepth */
							   NUM_OUTLINER_COLS,/* num columns */
							   OUTLINER_COLWIDTHS,/* column widths */
							   categoryDataFunc, /* data function */
							   categoryClickFunc,/* click function */
							   categoryIconFunc, /* icon function */
							   this,             /* closure */
							   &m_columnWidths,  /* posinfo */
							   TRUE,             /* tag */
							   FALSE);           /* grid */
  
	// Category should grow horizontally to show all categories.
	XtVaSetValues(m_wOutline,
				  XmNhorizontalSizePolicy, XmVARIABLE,
				  NULL);

	m_outlineHeaders = XP_NEW_ZAP(fe_OutlineHeaderDesc);
	m_outlineHeaders->header_strings = 
		(const char **)XP_CALLOC(NUM_OUTLINER_COLS, sizeof(char*));
	m_outlineHeaders->type[0] = FE_OUTLINE_String;
	m_outlineHeaders->header_strings[0] = OUTLINER_COLUMN_TYPE;
	m_outlineHeaders->type[1] = FE_OUTLINE_String;
	m_outlineHeaders->header_strings[1] = OUTLINER_COLUMN_NAME; 

	fe_OutlineSetHeaders(m_wOutline, m_outlineHeaders);
	XtVaSetValues(m_wOutline, 
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_FORM,
				  NULL);
	XtManageChild(m_wOutline);

	// Page area - page label

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	m_wPageLabel = XmCreateLabelGadget(rightPane, "pageLabel", args, n);
	XtManageChild(m_wPageLabel);

	// Page area - page title

	m_wPageTitle = XtVaCreateManagedWidget("pageTitle", xmLabelWidgetClass, rightPane,
										   XmNtopAttachment, XmATTACH_WIDGET,
										   XmNtopWidget, m_wPageLabel,
										   XmNleftAttachment, XmATTACH_FORM,
										   XmNrightAttachment, XmATTACH_FORM,
										   NULL);

	// page area - Page Form

	m_wPageForm = XtVaCreateManagedWidget("pageForm", xmFormWidgetClass, rightPane,
										  XmNtopAttachment, XmATTACH_WIDGET,
										  XmNtopWidget, m_wPageTitle,
										  XmNleftAttachment, XmATTACH_FORM,
										  XmNrightAttachment, XmATTACH_FORM,
										  XmNbottomAttachment, XmATTACH_FORM,
										  NULL);

	XtManageChild(leftPane);
	XtManageChild(rightPane);
	XtManageChild(form);
}

// Member:       initCategories
// Description:  Initializes categories
// Inputs:
// Side effects: 

void XFE_PrefsDialog::initCategories()
{
	XrmDatabase  db = XtDatabase(XtDisplay(m_chrome));
	char         name[1024];
	char         clas[1024];
	XrmValue     value;
	char        *type = NULL;
	int          i;
#ifdef MOZ_LI
	XP_Bool      li_ui_enabled = FALSE;
#endif

	if (m_categories) deleteCategories();

	m_numCategories = XtNumber(prefsCategories);

#ifdef MOZ_LI
	/* If li.ui.enabled is false, we hide the LI prefs */
	PREF_GetBoolPref("li.ui.enabled", &li_ui_enabled);
	if ( li_ui_enabled == FALSE ) {
		int found_it = 0;
		for ( i = 0; i < m_numCategories; i++ ) {
			if ( prefsCategories[i].type == PAGE_TYPE_LI ) found_it = 1;
			if ( found_it ) prefsCategories[i] = prefsCategories[i+1];
		}
		m_numCategories--;
	}
#endif
	
	// Calculate the number of pages we have

	m_numPages = 0;
	for (i = 0; i < m_numCategories; i++) {
		m_numPages++;
		if (prefsCategories[i].numChildren > 0);
			m_numPages += prefsCategories[i].numChildren;
	}

	m_pages = new XFE_PrefsPage *[m_numPages];
	int page_index = 0;

	// Well, this is kinda quick and dirty. I am assuming there are only
	// two levels in the hierarchy.

	m_categories = new PrefsCategory[m_numCategories];

	for (i = 0; i < m_numCategories; i++) {
		newPage(m_pages[page_index], prefsCategories[i].type);
		m_categories[i].page = m_pages[page_index];
		page_index++;
		m_categories[i].open = FALSE;
		m_categories[i].name = (char *)XtNewString(prefsCategories[i].name);

		// Get the resources for category name and description

		PR_snprintf(clas, sizeof (clas),
					"%s.%s", fe_progclass, prefsCategories[i].catResClass);
		PR_snprintf(name, sizeof (name),
					"%s.%s", fe_progclass, prefsCategories[i].catResName);
		if (XrmGetResource(db, name, clas, &type, &value))
			m_categories[i].categoryName = (char *)XtNewString(value.addr);
		else
			m_categories[i].categoryName = (char *)XtNewString(prefsCategories[i].name);

		PR_snprintf(clas, sizeof (clas),
					"%s.%s", fe_progclass, prefsCategories[i].descResClass);
		PR_snprintf(name, sizeof (name),
					"%s.%s", fe_progclass, prefsCategories[i].descResName);
		if (XrmGetResource(db, name, clas, &type, &value))
			m_categories[i].categoryDesc = (char *)XtNewString(value.addr);
		else
			m_categories[i].categoryDesc = (char *)XtNewString(prefsCategories[i].name);

		m_categories[i].parent = NULL;
		m_categories[i].leaf = prefsCategories[i].leaf;
		int numChildren = m_categories[i].numChildren = prefsCategories[i].numChildren;
		if (numChildren > 0) {
			m_categories[i].children = new PrefsCategory[numChildren];

			struct _prefsCategory *prefs = prefsCategories[i].children;
			PrefsCategory         *children = m_categories[i].children;

			for (int j = 0; j < numChildren; j++) {
				newPage(m_pages[page_index], prefs[j].type);

				children[j].page = m_pages[page_index];
				page_index++;
				children[j].open = FALSE;
				children[j].name = (char *)XtNewString(prefs[j].name);

				// Get the resources for category name and description

				PR_snprintf(clas, sizeof (clas),
							"%s.%s", fe_progclass, prefs[j].catResClass);
				PR_snprintf(name, sizeof (name),
							"%s.%s", fe_progclass, prefs[j].catResName);
				if (XrmGetResource(db, name, clas, &type, &value))
					children[j].categoryName = (char *)XtNewString(value.addr);
				else
					children[j].categoryName = (char *)XtNewString(prefs[j].name);

				PR_snprintf(clas, sizeof (clas),
							"%s.%s", fe_progclass, prefs[j].descResClass);
				PR_snprintf(name, sizeof (name),
							"%s.%s", fe_progclass, prefs[j].descResName);
				if (XrmGetResource(db, name, clas, &type, &value))
					children[j].categoryDesc = (char *)XtNewString(value.addr);
				else
					children[j].categoryDesc = (char *)XtNewString(prefs[j].name);

				children[j].leaf = prefs[j].leaf;
				children[j].parent = &m_categories[i];
				children[j].numChildren = 0;
				children[j].children = 0;
			}
		}
		else {
			m_categories[i].children = 0;
		}
	}
}

// Member:       deleteCategories
// Description:  Deletes categories
// Inputs:
// Side effects: 

void XFE_PrefsDialog::deleteCategories()
{
	// Delete

	for (int i = 0; i < m_numCategories; i++) {
		if (m_categories[i].children) {
			PrefsCategory *children = m_categories[i].children;
			int            numChildren = m_categories[i].numChildren;
			for (int j = 0; j < numChildren; j++) {
				if (children[j].name) free (children[j].name);
				if (children[j].categoryName) free (children[j].categoryName);
				if (children[j].categoryDesc) free (children[j].categoryDesc);
			}
			delete (m_categories[i].children);
		}
		if (m_categories[i].name) free (m_categories[i].name);
		if (m_categories[i].categoryName) free (m_categories[i].categoryName);
		if (m_categories[i].categoryDesc) free (m_categories[i].categoryDesc);
	}
	delete m_categories;
	m_categories = 0;
	m_numCategories = 0;
}

// Member:       newPage
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsDialog::newPage(XFE_PrefsPage *&page, 
							  int             type)
{
	switch (type) {
	case PAGE_TYPE_APPEARANCE:
		page = new XFE_PrefsPageGeneralAppearance(this);
		break;
	case PAGE_TYPE_FONTS:
		page = new XFE_PrefsPageGeneralFonts(this);
		break;
	case PAGE_TYPE_COLORS:
		page = new XFE_PrefsPageGeneralColors(this);
		break;
	case PAGE_TYPE_APPS:
		page = new XFE_PrefsPageGeneralAppl(this);
		break;
	case PAGE_TYPE_CACHE:
		page = new XFE_PrefsPageGeneralCache(this);
		break;
	case PAGE_TYPE_PROXIES:
		page = new XFE_PrefsPageGeneralProxies(this);
		break;
	case PAGE_TYPE_BROWSER:
		page = new XFE_PrefsPageBrowser(this);
		break;
	case PAGE_TYPE_LANG:
		page = new XFE_PrefsPageBrowserLang(this);
		break;
	case PAGE_TYPE_MAILNEWS_IDENTITY:
		page = new XFE_PrefsPageMailNewsIdentity(this);
		break;
#ifdef MOZ_MAIL_NEWS
	case PAGE_TYPE_MAILNEWS:
		page = new XFE_PrefsPageMailNews(this);
		break;
	case PAGE_TYPE_MAILNEWS_MESSAGES:
		page = new XFE_PrefsPageMessages(this);
		break;
	case PAGE_TYPE_MAILNEWS_MAILSERVER:
		page = new XFE_PrefsPageMailNewsMserver(this);
		break;
	case PAGE_TYPE_MAILNEWS_NSERVER:
		page = new XFE_PrefsPageNServer(this);
		break;
        case PAGE_TYPE_MAILNEWS_ADDRESS:
        	page = new XFE_PrefsPageAddress(this);
        	break;
	case PAGE_TYPE_MAILNEWS_COPIES:
	    	page = new XFE_PrefsPageMailNewsCopies(this);
		break;
	case PAGE_TYPE_MAILNEWS_HTML:
	    	page = new XFE_PrefsPageMailNewsHTML(this);
		break;
	case PAGE_TYPE_MAILNEWS_RECEIPTS:
	    	page = new XFE_PrefsPageMailNewsReceipts(this);
		break;
#endif  // MOZ_MAIL_NEWS
#ifdef EDITOR
	case PAGE_TYPE_EDITOR:
		page = new XFE_PrefsPageEditor(this);
		break;
	case PAGE_TYPE_EDITOR_APPEARANCE:
		page = new XFE_PrefsPageEditorAppearance(this);
		break;
	case PAGE_TYPE_EDITOR_PUBLISH:
		page = new XFE_PrefsPageEditorPublish(this);
		break;
	case PAGE_TYPE_DISKSPACE:
		page = new XFE_PrefsPageDiskSpace(this);
		break;
#endif  // EDITOR
	case PAGE_TYPE_ADVANCED:
		page = new XFE_PrefsPageGeneralAdvanced(this);
		break;
#ifdef PREFS_UNSUPPORTED
	case PAGE_TYPE_HELPFILES:
		page = new XFE_PrefsPageHelpFiles(this);
		break;
	case PAGE_TYPE_OFFLINE:
		page = new XFE_PrefsPageOffline(this);
		break;
	case PAGE_TYPE_OFFLINE_NEWS:
		page = new XFE_PrefsPageOfflineNews(this);
		break;
#endif /* PREFS_UNSUPPORTED */
#ifdef MOZ_LI
	case PAGE_TYPE_LI:
		page = new XFE_PrefsPageLIGeneral(this);
		break;
	case PAGE_TYPE_LI_SERVER:
		page = new XFE_PrefsPageLIServer(this);
		break;
	case PAGE_TYPE_LI_FILES:
		page = new XFE_PrefsPageLIFiles(this);
		break;
#endif
	}
}

// Member:       getCategoryByIndex
// Description:  Returns category given an index
// Inputs:
// Side effects: 

PrefsCategory *XFE_PrefsDialog::getCategoryByIndex(int index)
{
	PrefsCategory   *item = 0;
	int              count = 0;
	int              i;
	int              j;
	Boolean          found = FALSE;

	for (i = 0; i < m_numCategories; i++) {
		count++;
		if ((index + 1) == count) {
			item = &m_categories[i];
			found = TRUE;
			break;
		}
		if (! m_categories[i].open) continue;
		if ((index + 1 - count) <= m_categories[i].numChildren) {
			j = index - count;
			item = &(m_categories[i].children[j]);
			found = TRUE;
			break;
		}
		count += m_categories[i].numChildren;
	}

	return item;
}

// Member:       getCategoryByName
// Description:  Returns category given a name
// Inputs:
// Side effects: 

PrefsCategory *XFE_PrefsDialog::getCategoryByName(const char *name)
{
	PrefsCategory   *item = 0;
	int              i;
	int              j;
	int              numChildren;
	PrefsCategory   *children;
	Boolean          found = FALSE;

	for (i = 0; i < m_numCategories; i++) {
		if (strcmp(m_categories[i].name, name) == 0) {
			item = &m_categories[i];
			found = TRUE;
			break;
		}
		if (m_categories[i].numChildren > 0) {
			numChildren = m_categories[i].numChildren;
			for (j = 0; j < numChildren; j++) {
				children = m_categories[i].children;
				if (strcmp(children[j].name, name) == 0) {
					item = &children[j];
					found = TRUE;
					break;
				}
			}
		}
	}

	return item;
}

// Member:       getCurrentCategory
// Description:  Returns currently selected category 
// Inputs:
// Side effects: 

PrefsCategory *XFE_PrefsDialog::getCurrentCategory()
{
	return m_currentCategory;
}

// Member:       refreshCategories
// Description:  Refresh categories
// Inputs:
// Side effects: 

void XFE_PrefsDialog::refreshCategories()
{
	fe_OutlineChange(m_wOutline, 1, CAT_LAST_CELL, getVisibleCategoryCount());
}

// Member:       isCurrentCategory
// Description:  Is category the one being displayed?
// Inputs:
// Side effects: 

Boolean XFE_PrefsDialog::isCurrentCategory(PrefsCategory *entry)
{
	return (entry == m_currentCategory);
}

// Member:       getOutline
// Description:  Returns the outline widget
// Inputs:
// Side effects: 

Widget XFE_PrefsDialog::getOutline()
{
	return m_wOutline;
}

// Member:       getPrefsParent
// Description:  Returns the widget from which a prefs dialog is popped up
// Inputs:
// Side effects: 

Widget XFE_PrefsDialog::getPrefsParent()
{
	return m_wParent;
}

// Member:       getPageForm
// Description:  Returns the page form
// Inputs:
// Side effects: 

Widget XFE_PrefsDialog::getPageForm()
{
	return m_wPageForm;
}

// Member:       getDialogChrome
// Description:  Returns the dialog chrome
// Inputs:
// Side effects: 

Widget XFE_PrefsDialog::getDialogChrome()
{
	return m_chrome;
}

// ************************************************************************
// *************************       Outline        *************************
// ************************************************************************

Boolean XFE_PrefsDialog::categoryDataFunc(Widget          /* widget */, 
										  void           *closure, 
										  int             row,
										  fe_OutlineDesc *data,
										  int             /* tag */)
{
	int               c;
	int               dataColumn;
	XFE_PrefsDialog  *prefsDialog = (XFE_PrefsDialog *)closure;

	if (row >= prefsDialog->getVisibleCategoryCount()) return False;
	PrefsCategory *entry = prefsDialog->getCategoryByIndex(row);
	if (entry == 0) return False;

	// Handle selections

	if (prefsDialog->isCurrentCategory(entry)) {
		fe_OutlineSelect(prefsDialog->getOutline(), row, True);
    } 
	else {
		fe_OutlineUnselect(prefsDialog->getOutline(), row);
    }

	data->style = FE_OUTLINE_Default;
	if ((entry->leaf) ||
		(entry->children == 0)) {
		data->flippy = FE_OUTLINE_Leaf;
	}
	else {
		data->flippy = entry->open ? FE_OUTLINE_Expanded : FE_OUTLINE_Folded;
	}
	data->depth = entry->leaf ? 1 : 0;

	for (c = 1, dataColumn = 0; c < NUM_OUTLINER_COLS ; c++, dataColumn++) {
		if (data->column_headers[c] == OUTLINER_COLUMN_NAME) {
			static char label[256];
			data->type[dataColumn] = FE_OUTLINE_String;
			if (entry->leaf) 
				PR_snprintf(label, sizeof(label),"   %s", entry->categoryName);
			else
				strcpy(label, entry->categoryName);
			data->str[dataColumn] = label;
			// data->str[dataColumn] = entry->categoryName;
		}
	}

	return True;
}

void XFE_PrefsDialog::categoryClickFunc(Widget  /* widget */, 
										void   *closure,
										int     row,
										int     /* column */, 
										const char* /* header */,
										int     /* button */,
										int     /* clicks */,
										Boolean /* shift */, 
										Boolean /* ctrl */,
										int     /* tag */)
{
	if (row == (-1)) {
		// The user is clicking on the header
		// Do nothing here.
		return;
	}

	XFE_PrefsDialog  *prefsDialog = (XFE_PrefsDialog *)closure;
	PrefsCategory    *entry = prefsDialog->getCategoryByIndex(row);

	prefsDialog->setCurrentCategory(entry);
	XmUpdateDisplay(prefsDialog->getBaseWidget());
}

void XFE_PrefsDialog::categoryIconFunc(Widget   /* widget */,
									   void    *closure,
									   int      row,
									   int      /*tag*/)
{
	XFE_PrefsDialog  *prefsDialog = (XFE_PrefsDialog *)closure;
	PrefsCategory    *entry = prefsDialog->getCategoryByIndex(row);

	if (! entry) return;

	entry->open = (entry->open) ? FALSE : TRUE;
	prefsDialog->calcVisibleCategoryCount();
	prefsDialog->refreshCategories();

	// Make this the current selection
	prefsDialog->setCurrentCategory(entry);
	XmUpdateDisplay(prefsDialog->getBaseWidget());
}



