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
   PrefsCacheMore.cpp -- The more dialog for cache preferences

   Created: Linda Wei <lwei@netscape.com>, 04-Dec-96.
 */


#include "felocale.h"
#include "structs.h"
#include "fonts.h"
#include "xpassert.h"
#include "xfe.h"
#include "e_kit.h"
#include "PrefsCacheMore.h"
#include "prefapi.h"

#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/ToggleB.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/TextF.h> 
#include <Xm/ToggleBG.h> 
#include <Xfe/Xfe.h>

extern int XFE_ERROR_SAVING_OPTIONS;

extern "C"
{
	void        NET_SetDiskCacheSize(int32 new_size);
	void        NET_SetMemoryCacheSize(int32 new_size);
	int         NET_CleanupCacheDirectory(char * dir_name, const char * prefix);
	void        NET_WriteCacheFAT(char *filename, Bool final_call);

	Dimension   XfeWidth(Widget w);
	char       *XP_GetString(int i);
}

// ==================== Public Member Functions ====================

// Member:       XFE_PrefsCacheMoreDialog
// Description:  Constructor
// Inputs:
// Side effects: Creates the More dialog for Cache Preferences

XFE_PrefsCacheMoreDialog::XFE_PrefsCacheMoreDialog(
						  XFE_PrefsDialog *prefsDialog,
						  Widget     parent,      // dialog parent
						  char      *name,        // dialog name
						  Boolean    modal)       // modal dialog?
	: XFE_Dialog(parent, 
				 name,
				 TRUE,     // ok
				 TRUE,     // cancel
				 FALSE,    // help
				 FALSE,    // apply
				 FALSE,    // separator
				 modal     // modal
				 ),
	  m_prefsDialog(prefsDialog),
	  m_prefsDataCacheMore(0)
{
	PrefsDataCacheMore *fep = NULL;

	fep = new PrefsDataCacheMore;
	memset(fep, 0, sizeof(PrefsDataCacheMore));
	m_prefsDataCacheMore = fep;
	
	Widget     kids[100];
	Arg        av[50];
	int        ac;
	int        i;

	Widget     form;
	form = XtVaCreateWidget("form", xmFormWidgetClass, m_chrome,
							XmNtopAttachment, XmATTACH_FORM,
							XmNleftAttachment, XmATTACH_FORM,
							XmNrightAttachment, XmATTACH_FORM,
							XmNbottomAttachment, XmATTACH_FORM,
							NULL);
	XtManageChild (form);

	Widget label;

	ac = 0;
	XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
	label = XmCreateLabelGadget (form, "cacheLabel", av, ac);
	XtManageChild (label);

	Widget clear_disk_cache_button;
	Widget clear_mem_cache_button;

	ac = 0;
	i = 0;

	kids[i++] = clear_disk_cache_button = 
		XmCreatePushButtonGadget (form, "clearDiskCache", av, ac);

	kids[i++] = clear_mem_cache_button =  
		XmCreatePushButtonGadget (form, "clearMemCache", av, ac);

	int labels_width;
	labels_width = XfeVaGetWidestWidget(clear_disk_cache_button,
										clear_mem_cache_button,
										NULL);

	XtVaSetValues(label,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	Dimension form_width = XfeWidth(label);
	int button_offset = (form_width - labels_width) / 2;
	if (button_offset < 0) button_offset = 0;

	XtVaSetValues(clear_disk_cache_button,
				  XmNwidth, labels_width,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, label,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, button_offset,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	XtVaSetValues(clear_mem_cache_button,
				  XmNwidth, labels_width,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, clear_disk_cache_button,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, button_offset,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	// Add callbacks

	XtAddCallback(clear_disk_cache_button, XmNactivateCallback,
				  prefsCacheMoreCb_clearDisk, this);
	XtAddCallback(clear_mem_cache_button, XmNactivateCallback,
				  prefsCacheMoreCb_clearMem, this);

	XtAddCallback(m_chrome, XmNokCallback, prefsCacheMoreCb_ok, this);
	XtAddCallback(m_chrome, XmNcancelCallback, prefsCacheMoreCb_cancel, this);

	XtManageChildren(kids, i);
}

// Member:       ~XFE_PrefsCacheMoreDialog
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsCacheMoreDialog::~XFE_PrefsCacheMoreDialog()
{
	// Clean up

	delete m_prefsDataCacheMore;
}

// Member:       show
// Description:  Pop up dialog
// Inputs:
// Side effects: 

void XFE_PrefsCacheMoreDialog::show()
{
	// TODO: Initialize the dialog

	// Manage the top level

	XFE_Dialog::show();

	// Set focus to the OK button

	XmProcessTraversal(m_okButton, XmTRAVERSE_CURRENT);
}

// Member:       initPage
// Description:  Initializes page for MailNewsCacheMore
// Inputs:
// Side effects: 

void XFE_PrefsCacheMoreDialog::initPage()
{
}

// Member:       verifyPage
// Description:  verify page for MailNewsCacheMore
// Inputs:
// Side effects: 

Boolean XFE_PrefsCacheMoreDialog::verifyPage()
{
	return TRUE;
}

// Member:       installChanges
// Description:  install changes for CacheMore
// Inputs:
// Side effects: 

void XFE_PrefsCacheMoreDialog::installChanges()
{
}

// Member:       getContext
// Description:  returns context
// Inputs:
// Side effects: 

MWContext *XFE_PrefsCacheMoreDialog::getContext()
{
	return (m_prefsDialog->getContext());
}

// Friend:       prefsCacheMoreCb_ok
// Description:  
// Inputs:
// Side effects: 

void prefsCacheMoreCb_ok(Widget    w,
						 XtPointer closure,
						 XtPointer callData)
{
	// Simulate a cancel

	prefsCacheMoreCb_cancel(w, closure, callData);
}

// Friend:       prefsCacheMoreCb_cancel
// Description:  
// Inputs:
// Side effects: 

void prefsCacheMoreCb_cancel(Widget    /* w */,
							 XtPointer closure,
							 XtPointer /* callData */)
{
	XFE_PrefsCacheMoreDialog *theDialog = (XFE_PrefsCacheMoreDialog *)closure;

	// Delete the dialog

	delete theDialog;
}

// Friend:       prefsCacheMoreCb_clearDisk
// Description:  
// Inputs:
// Side effects: 

void prefsCacheMoreCb_clearDisk(Widget    /* w */,
								XtPointer closure,
								XtPointer /* callData */)
{
	XFE_PrefsCacheMoreDialog     *theDialog = (XFE_PrefsCacheMoreDialog *)closure;
	MWContext                    *context = theDialog->getContext();
	int32                         size;

	if (XFE_Confirm (context, fe_globalData.clear_disk_cache_message))	{
		PREF_GetIntPref("browser.cache.disk_cache_size", &size);
		PREF_SetIntPref("browser.cache.disk_cache_size", 0);
		PREF_SetIntPref("browser.cache.disk_cache_size", size);
    }
}

// Friend:       prefsCacheMoreCb_clearMem
// Description:  
// Inputs:
// Side effects: 

void prefsCacheMoreCb_clearMem(Widget    /* w */,
							   XtPointer closure,
							   XtPointer /* callData */)
{
	XFE_PrefsCacheMoreDialog     *theDialog = (XFE_PrefsCacheMoreDialog *)closure;
	MWContext                    *context = theDialog->getContext();
	int32                         size;

	if (XFE_Confirm (context, fe_globalData.clear_mem_cache_message)) {
		PREF_GetIntPref("browser.cache.memory_cache_size", &size);
		PREF_SetIntPref("browser.cache.memory_cache_size", 0);
		PREF_SetIntPref("browser.cache.memory_cache_size", size);
    }
}


