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
   PrefsLang.cpp -- The language dialog for preference, packaged from 3.0 code
 */

#include "felocale.h"
#include "structs.h"
#include "fonts.h"
#include "xpassert.h"
#include "xfe.h"
#include "e_kit.h"
#include "PrefsLang.h"

#include <Xm/ArrowBG.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/ScrolledW.h>
#include <Xm/ToggleB.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/TextF.h> 
#include <Xm/ToggleBG.h> 

#include <Xfe/Xfe.h>

extern int XFE_ERROR_SAVING_OPTIONS;

extern "C"
{
	char *XP_GetString(int i);

	static int multiline_str_to_array(const char *string, char ***array_p);
	static int multiline_pref_to_array(const char *string, char ***array_p);
}

// ==================== Public Member Functions ====================

// Member:       XFE_PrefsLangDialog
// Description:  Constructor
// Inputs:
// Side effects: Creates the More dialog for Mail Server Preferences

XFE_PrefsLangDialog::XFE_PrefsLangDialog(XFE_PrefsDialog *prefsDialog,
										 XFE_PrefsPageBrowserLang *langPage,
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
	  m_langPage(langPage),
	  m_prefsDataLang(0)
{
	PrefsDataLang *fep = NULL;

	fep = new PrefsDataLang;
	memset(fep, 0, sizeof(PrefsDataLang));
	m_prefsDataLang = fep;

	fep->avail_cnt = multiline_pref_to_array(fe_globalData.language_region_list, 
											 &fep->avail_lang_regs);

	Widget     kids[100];
	Arg        av[50];
	int        ac;
	int        i;

	Widget     form;
	form = XtVaCreateWidget("form", xmFormWidgetClass, m_chrome,
							XmNmarginWidth, 8,
							XmNtopAttachment, XmATTACH_FORM,
							XmNleftAttachment, XmATTACH_FORM,
							XmNrightAttachment, XmATTACH_FORM,
							XmNbottomAttachment, XmATTACH_FORM,
							NULL);
	XtManageChild (form);

	Widget lang_label;
	Widget lang_list;
	Widget other_label;
	Widget other_text;

	ac = 0;
	i = 0;

	kids[i++] = lang_label = 
		XmCreateLabelGadget(form, "langLabel", av, ac);

	lang_list =	XmCreateScrolledList(form, "langList", av, ac);
	XtManageChild(lang_list);
	kids[i++] = XtParent(lang_list);

	kids[i++] = other_label = 
		XmCreateLabelGadget(form, "otherLabel", av, ac);

	kids[i++] = other_text = 
		fe_CreateTextField(form, "otherText", av, ac);

	XtVaSetValues(lang_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	XtVaSetValues(XtParent(lang_list),
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, lang_label,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	int labels_height;
	labels_height = XfeVaGetTallestWidget(other_label,
										  other_text,
										  NULL);

	XtVaSetValues(other_label,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, XtParent(lang_list),
				  XmNtopOffset, 8,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	XtVaSetValues(other_text,
				  XmNcolumns, 35,
				  XmNheight, labels_height,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, other_label,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, other_label,
				  XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNrightWidget, XtParent(lang_list),
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	
	fep->lang_list = lang_list;
	fep->other_text = other_text;

	// Add callbacks

	XtAddCallback(m_chrome, XmNokCallback, cb_ok, this);
	XtAddCallback(m_chrome, XmNcancelCallback, cb_cancel, this);
	XtAddCallback(lang_list, XmNdefaultActionCallback, cb_ok, this);
	XtAddCallback(other_text, XmNactivateCallback, cb_ok, this);

	XtManageChildren(kids, i);
	XtManageChild(form);
}

// Member:       ~XFE_PrefsLangDialog
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsLangDialog::~XFE_PrefsLangDialog()
{
	XP_ASSERT(m_prefsDataLang);
	PrefsDataLang    *fep = m_prefsDataLang;

	// Clean up

	if (fep->avail_lang_regs != 0) {
		for (int i = 0; i < fep->avail_cnt; i++) {
			XP_FREE(fep->avail_lang_regs[i]);
		}
		XP_FREE(fep->avail_lang_regs);
	}

	delete m_prefsDataLang;
}

// Member:       show
// Description:  Pop up dialog
// Inputs:
// Side effects: 

void XFE_PrefsLangDialog::show()
{
	// TODO: Initialize the dialog

	// Manage the top level

	XFE_Dialog::show();

	// Set focus to the OK button

	XmProcessTraversal(m_okButton, XmTRAVERSE_CURRENT);
}

// Member:       initPage
// Description:  Initializes page for MailNewsLang
// Inputs:
// Side effects: 

void XFE_PrefsLangDialog::initPage()
{
	XP_ASSERT(m_prefsDataLang);

	PrefsDataLang    *fep = m_prefsDataLang;

	// Retrieve the list of available languages

	char          **avail_lang_regs = fep->avail_lang_regs;
	int             avail_cnt = fep->avail_cnt;

	// Put the list into the available languages/regions list

    XmStringTable   str_list = NULL;
	int             i;

    str_list = (XmStringTable)XP_ALLOC(sizeof (XmString) * avail_cnt+1);
    for (i=0; i<avail_cnt; i++) {
		str_list[i] = XmStringCreateLocalized(avail_lang_regs[i]);
    }
    XmListAddItemsUnselected(fep->lang_list, str_list, avail_cnt, 1);
    for (i=0; i<avail_cnt; i++) {
      XmStringFree(str_list[i]);
    }
    XP_FREE(str_list);

    // we always want something selected in the language/region list if possible 

    if (avail_cnt > 0)
		XmListSelectPos(fep->lang_list, 1, False);
}

// Member:       verifyPage
// Description:  verify page for MailNewsLang
// Inputs:
// Side effects: 

Boolean XFE_PrefsLangDialog::verifyPage()
{
	return TRUE;
}

// Member:       getContext
// Description:  returns context
// Inputs:
// Side effects: 

MWContext *XFE_PrefsLangDialog::getContext()
{
	return (m_prefsDialog->getContext());
}

// Member:       getLangPage
// Description:  returns the Languages prefs page
// Inputs:
// Side effects: 

XFE_PrefsPageBrowserLang *XFE_PrefsLangDialog::getLangPage()
{
	return m_langPage;
}

// Member:       getPrefsDialog
// Description:  returns preferences dialog
// Inputs:
// Side effects: 

XFE_PrefsDialog *XFE_PrefsLangDialog::getPrefsDialog()
{
	return (m_prefsDialog);
}

// Friend:       prefsLangCb_ok
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsLangDialog::cb_ok(Widget    w,
								XtPointer closure,
								XtPointer callData)
{
	XFE_PrefsLangDialog *theDialog = (XFE_PrefsLangDialog *)closure;
	PrefsDataLang       *fep = theDialog->m_prefsDataLang;
	int                 *pos_list;
	int                  pos_count;
	char                *value = 0;

	value = fe_GetTextField(fep->other_text);
	if (value && (XP_STRLEN(value) > 0)) {
		XFE_PrefsPageBrowserLang *lang_page = theDialog->getLangPage();
		lang_page->insertLang(value);
	}
	else if (XmListGetSelectedPos(fep->lang_list, &pos_list, &pos_count)) {
		int   pos = pos_list[0] - 1;
		char *lang = fep->avail_lang_regs[pos];
		XFE_PrefsPageBrowserLang *lang_page = theDialog->getLangPage();
		lang_page->insertLang(lang);
		XtFree((char *)pos_list);
	}
	if (value) XP_FREE(value);

	// Simulate a cancel

	theDialog->cb_cancel(w, closure, callData);
}

// Member:       prefsLangCb_cancel
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsLangDialog::cb_cancel(Widget    /* w */,
									XtPointer closure,
									XtPointer /* callData */)
{
	XFE_PrefsLangDialog *theDialog = (XFE_PrefsLangDialog *)closure;

	XtRemoveCallback(theDialog->m_chrome, XmNokCallback, cb_ok, theDialog);
	XtRemoveCallback(theDialog->m_chrome, XmNcancelCallback, cb_cancel, theDialog);

	// Delete the dialog

	delete theDialog;
}

// Member:       prefsLangCb_focusOther
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsLangDialog::cb_focusOther(Widget    /* w */,
										XtPointer closure,
										XtPointer /* callData */)
{
	XFE_PrefsLangDialog *theDialog = (XFE_PrefsLangDialog *)closure;
	XFE_PrefsDialog     *thePrefsDialog = theDialog->getPrefsDialog();
	PrefsDataLang       *fep = theDialog->m_prefsDataLang;

	fep->temp_default_button = XmSelectionBoxGetChild(thePrefsDialog->getDialogChrome(),
													  XmDIALOG_DEFAULT_BUTTON);
	XtVaSetValues(thePrefsDialog->getDialogChrome(), XmNdefaultButton, NULL, NULL);
}

// Member:       prefsLangCb_loseFocusOther
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsLangDialog::cb_loseFocusOther(Widget    /* w */,
											XtPointer closure,
											XtPointer /* callData */)
{
	XFE_PrefsLangDialog *theDialog = (XFE_PrefsLangDialog *)closure;
	XFE_PrefsDialog     *thePrefsDialog = theDialog->getPrefsDialog();
	PrefsDataLang       *fep = theDialog->m_prefsDataLang;

	if (fep->temp_default_button) {
		XtVaSetValues(thePrefsDialog->getDialogChrome(),
					  XmNdefaultButton, fep->temp_default_button,
					  NULL);
	}
}

// ==================== Private Functions ====================

/*
 * This belongs in a utility module
 *
 * Convert a multiline string (embedded new-lines)
 * to an array of strings
 *  create the array
 *  copies the lines into the array
 *  returns length of the array
 *
 * To free the array:
 *   for (i=0; i<cnt; i++)
 *      XP_FREE(array[i]);
 *   XP_FREE(array);
 */

static int multiline_str_to_array(const char *string, char ***array_p)
{
    char *p, *q, **array;
    int i, cnt, len;
    Boolean at_start_of_line;
    
    /* handle null string */
    if (string == NULL) {
	/* always allocate an array so the caller can free it */
	array = (char **)XP_ALLOC(sizeof(char *));
	array[0] = NULL;
	*array_p = array;
	return 0;
    }

    /* 
     * count the number of lines 
     * (count beginnings so we will count any last line without a '\n')
	 * This supports multibyte text only because '\n' is less than 0x40
     */
    cnt = 0;
    at_start_of_line = True;
    for (p=(char *)string; *p; p++) {
	if (at_start_of_line) {
	    cnt += 1;
	    at_start_of_line = False;
	}
	if (*p == '\n') {
	    at_start_of_line = True;
	}
    }

    /* copy lines into the array */
    array = (char **)XP_ALLOC((cnt+1) * sizeof(char *));
    i = 0;
    len = 0;
    for (p=q=(char *)string; *p; p++) {
	if (*p == '\n') {
	    array[i] = (char *)XP_ALLOC(len+1);
	    strncpy(array[i], q, len);
	    /* add a string terminator */
	    array[i][len] = '\0';
	    i += 1;
	    len = 0;
	    q = p + 1;
	}
	else
	    len += 1;
    }
    if (len) { /* include ending chars with no newline */
	array[i] = (char *)XP_ALLOC(len+1);
	strncpy(array[i], q, len);
	/* add a string terminator */
	array[i][len] = '\0';
    }

    *array_p = array;
    return cnt;
}

/*
 * This belongs in a utility module
 *
 * Convert a multiline preference (embedded new-lines)
 * to an array of strings
 *  create the array
 *  copies the lines into the array
 *  remove leading/trailing white space
 *  remove blank lines
 *  returns length of the array
 *
 * To free the array:
 *   for (i=0; i<cnt; i++)
 *      XP_FREE(array[i]);
 *   XP_FREE(array);
 */

static int multiline_pref_to_array(const char *string, char ***array_p)
{
  int i, j, cnt;
  char **array;

  /*
   * Convert the multiline string to an array
   * (with leading/trailing white space and blank lines)
   */
  cnt = multiline_str_to_array(string, array_p);

  /*
   * trim any leading/training white space
   */
  array = *array_p;
  for (i=0; i<cnt; i++) {
    char *tmp;
    tmp = XP_StripLine(array[i]);
    /* we can't lose the malloc address else free will fail */
    /* so if there was white space at the beginning we make a copy */
    if (tmp != array[i]) {
	char *tmp2 = array[i];
      	array[i] = strdup(tmp);
      	XP_FREE(tmp2);
    }
  }

  /*
   * remove any blank lines
   */
  for (i=0; i<cnt; i++) {
    if (array[i][0] == '\0') {
	XP_FREE(array[i]);
	for (j=i+1; j<cnt; j++)
	    array[j-1] = array[j];
	cnt -= 1;
    }
  }

  return cnt;
}

