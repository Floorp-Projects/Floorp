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
   PrefsFont.cpp -- The fontuage dialog for preference, packaged from 3.0 code
   Created: Linda Wei <lwei@netscape.com>, 25-Feb-97.
 */


#include "felocale.h"
#include "structs.h"
#include "fonts.h"
#include "xpassert.h"
#include "xfe.h"
#include "e_kit.h"
#include "PrefsFont.h"
#include "nf.h"
#include "Mnffbu.h"

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

extern "C"
{
	char *XP_GetString(int i);
}

#define OUTLINER_GEOMETRY_PREF "preferences.font.outliner_geometry"

extern int XFE_FONT_COL_ORDER;
extern int XFE_FONT_COL_DISPLAYER;

const int XFE_PrefsFontDialog::OUTLINER_COLUMN_ORDER     = 0;
const int XFE_PrefsFontDialog::OUTLINER_COLUMN_DISPLAYER = 1;

const int XFE_PrefsFontDialog::OUTLINER_COLUMN_MAX_LENGTH = 256;
const int XFE_PrefsFontDialog::OUTLINER_INIT_POS = (-1);

#define STRING_COL_ORDER      "Order"
#define STRING_COL_DISPLAYER  "Displayer"


// ==================== Public Member Functions ====================

// Member:       XFE_PrefsFontDialog
// Description:  Constructor
// Inputs:
// Side effects: Creates the Font Displayer dialog

XFE_PrefsFontDialog::XFE_PrefsFontDialog(XFE_PrefsDialog *prefsDialog,
										 Widget     parent,  // dialog parent
										 char      *name,    // dialog name
										 Boolean    modal)   // modal dialog?
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
	  m_prefsDataFont(0)
{
	PrefsDataFont *fep = NULL;

	fep = new PrefsDataFont;
	memset(fep, 0, sizeof(PrefsDataFont));
	m_prefsDataFont = fep;

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

	Widget         font_label;
	Widget         font_list;
	Widget         up_button;
	Widget         down_button;

	ac = 0;
	i = 0;

	kids[i++] = font_label = XmCreateLabelGadget(form, "fontLabel", av, ac);

	kids[i++] = up_button = XmCreateArrowButtonGadget(form, "upButton", av, ac);

	kids[i++] = down_button = XmCreateArrowButtonGadget(form, "downButton", av, ac);

	fep->up_button = up_button;
	fep->down_button = down_button;

	// Outliner

	int           num_columns = 2;
	static int    default_column_widths[] = {8, 40};
	XFE_Outliner *outliner;

	outliner = new XFE_Outliner("fontList",            // name
								this,                  // outlinable
								getPrefsDialog(),      // top level								  
								form,                  // parent
								FALSE,                 // constant size
								FALSE,                 // has headings
								num_columns,           // number of columns
								num_columns,           // number of visible columns
								default_column_widths, // default column widths
								OUTLINER_GEOMETRY_PREF
								);

	font_list = fep->font_list = outliner->getBaseWidget();
	fep->font_outliner = outliner;

	XtVaSetValues(outliner->getBaseWidget(),
				  XtVaTypedArg, XmNblankBackground, XmRString, "white", 6,
				  XmNvisibleRows, 10,
				  NULL);
	XtVaSetValues(outliner->getBaseWidget(),
				  XmNcellDefaults, True,
				  XtVaTypedArg, XmNcellBackground, XmRString, "white", 6,
				  NULL);

	outliner->setColumnResizable(OUTLINER_COLUMN_ORDER, False);
	outliner->setColumnResizable(OUTLINER_COLUMN_DISPLAYER, False);
	outliner->show();
	
	XtVaSetValues(font_label,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	XtVaSetValues(fep->font_list,
				  XmNtopOffset, 8,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, font_label,
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

	list_height = XfeHeight(font_list);
	up_button_height = XfeHeight(up_button);
	up_button_top_offset = list_height/2 - up_button_height - down_button_top_offset/2;

	XtVaSetValues(up_button,
				  XmNarrowDirection, XmARROW_UP,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, font_list,
				  XmNtopOffset, up_button_top_offset,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, font_list,
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

	// Add callbacks

	XtAddCallback(m_chrome, XmNokCallback, cb_ok, this);
	XtAddCallback(m_chrome, XmNcancelCallback, cb_cancel, this);
	XtAddCallback(up_button, XmNactivateCallback, cb_promote, this);
	XtAddCallback(down_button, XmNactivateCallback, cb_demote, this);

	XtManageChildren(kids, i);
	XtManageChild(form);
}

// Member:       ~XFE_PrefsFontDialog
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsFontDialog::~XFE_PrefsFontDialog()
{
	if (m_prefsDataFont) {
		if (m_prefsDataFont->font_displayers) 
			nffbu_free(WF_fbu, m_prefsDataFont->font_displayers, NULL);
	}

	delete m_prefsDataFont;
}

// Member:       show
// Description:  Pop up dialog
// Inputs:
// Side effects: 

void XFE_PrefsFontDialog::show()
{
	// TODO: Initialize the dialog

	// Manage the top level

	XFE_Dialog::show();

	// Set focus to the OK button

	XmProcessTraversal(m_okButton, XmTRAVERSE_CURRENT);
}

// Member:       initPage
// Description:  Initializes page for Font
// Inputs:
// Side effects: 

void XFE_PrefsFontDialog::initPage()
{
	XP_ASSERT(m_prefsDataFont);

	PrefsDataFont    *fep = m_prefsDataFont;

	// Retrieve the list of available font displayers

	fep->font_displayers = (char **)nffbu_ListFontDisplayers(WF_fbu, NULL);

	fep->font_displayers_count = 0;
	if (fep->font_displayers != 0) {
		for (int i=0; fep->font_displayers[i] == 0; i++) {
			if (fep->font_displayers[i] != 0) fep->font_displayers_count++;
		}
	}

	fep->font_outliner->change(0, fep->font_displayers_count, fep->font_displayers_count);
	setSelectionPos(OUTLINER_INIT_POS);
}

// Member:       verifyPage
// Description:  verify page for Font
// Inputs:
// Side effects: 

Boolean XFE_PrefsFontDialog::verifyPage()
{
	return TRUE;
}

// Member:       getContext
// Description:  returns context
// Inputs:
// Side effects: 

MWContext *XFE_PrefsFontDialog::getContext()
{
	return (m_prefsDialog->getContext());
}

// Member:       getPrefsDialog
// Description:  returns preferences dialog
// Inputs:
// Side effects: 

XFE_PrefsDialog *XFE_PrefsFontDialog::getPrefsDialog()
{
	return (m_prefsDialog);
}

// Member:       setSelelctionPos
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsFontDialog::setSelectionPos(int pos)
{
	PrefsDataFont *fep = m_prefsDataFont;
	int            count = fep->font_displayers_count;

	if (pos >= count) return;

	if (pos == OUTLINER_INIT_POS) {
		XtVaSetValues(fep->up_button, XmNsensitive, False, NULL);
		XtVaSetValues(fep->down_button, XmNsensitive, False, NULL);
	}
	else {
		fep->font_outliner->selectItemExclusive(pos);
		XtVaSetValues(fep->up_button, XmNsensitive, (pos != 0), NULL);
		XtVaSetValues(fep->down_button, XmNsensitive, (pos != (count-1)), NULL);
	}
}

// Member:       deselelctionPos
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsFontDialog::deselectPos(int pos)
{
	PrefsDataFont *fep = m_prefsDataFont;
	int            count = fep->font_displayers_count;

	if (pos >= count) return;

	fep->font_outliner->deselectItem(pos);
}

// Member:       prefsFontCb_ok
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsFontDialog::cb_ok(Widget    w,
								XtPointer closure,
								XtPointer callData)
{
	XFE_PrefsFontDialog *theDialog = (XFE_PrefsFontDialog *)closure;
	//	PrefsDataFont       *fep = theDialog->m_prefsDataFont;

	// Simulate a cancel

	theDialog->cb_cancel(w, closure, callData);
}

// Member:       prefsFontCb_cancel
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsFontDialog::cb_cancel(Widget    /* w */,
									XtPointer closure,
									XtPointer /* callData */)
{
	XFE_PrefsFontDialog *theDialog = (XFE_PrefsFontDialog *)closure;

	XtRemoveCallback(theDialog->m_chrome, XmNokCallback, cb_ok, theDialog);
	XtRemoveCallback(theDialog->m_chrome, XmNcancelCallback, cb_cancel, theDialog);

	// Delete the dialog

	delete theDialog;
}

// Member:       prefsFontCb_promote
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsFontDialog::cb_promote(Widget    /* w */,
									 XtPointer /* closure */,
									 XtPointer /* callData */)
{
	// XFE_PrefsFontDialog *theDialog = (XFE_PrefsFontDialog *)closure;
}

// Member:       prefsFontCb_demote
// Description:  
// Inputs:
// Side effects: 

void XFE_PrefsFontDialog::cb_demote(Widget    /* w */,
									XtPointer /* closure */,
									XtPointer /* callData */)
{
	// XFE_PrefsFontDialog *theDialog = (XFE_PrefsFontDialog *)closure;
}

/* Outlinable interface methods */

void *XFE_PrefsFontDialog::ConvFromIndex(int /* index */)
{
	return (void *)NULL;
}

int XFE_PrefsFontDialog::ConvToIndex(void * /* item */)
{
	return 0;
}

char*XFE_PrefsFontDialog::getColumnName(int column)
{
	switch (column){
	case OUTLINER_COLUMN_ORDER:
		return STRING_COL_ORDER;
	case OUTLINER_COLUMN_DISPLAYER:
		return STRING_COL_DISPLAYER;
    default:
		XP_ASSERT(0); 
		return 0;
    }
}

char *XFE_PrefsFontDialog::getColumnHeaderText(int column)
{
  switch (column) 
    {
    case OUTLINER_COLUMN_ORDER:
    case OUTLINER_COLUMN_DISPLAYER:
      return "";
    default:
      XP_ASSERT(0);
      return 0;
    }
}

fe_icon *XFE_PrefsFontDialog::getColumnHeaderIcon(int /* column */)
{
	return 0;

}

EOutlinerTextStyle XFE_PrefsFontDialog::getColumnHeaderStyle(int /* column */)
{
	return OUTLINER_Default;
}

// This method acquires one line of data.
void *XFE_PrefsFontDialog::acquireLineData(int line)
{
	PrefsDataFont *fep = m_prefsDataFont;
	fep->m_rowIndex = line;
	return (void *)1;
}

void XFE_PrefsFontDialog::getTreeInfo(Boolean * /* expandable */,
									  Boolean * /* is_expanded */,
									  int     * /* depth */,
									  OutlinerAncestorInfo ** /* ancestor */)
{
	// No-op
}


EOutlinerTextStyle XFE_PrefsFontDialog::getColumnStyle(int /*column*/)
{
    return OUTLINER_Default;
}

char *XFE_PrefsFontDialog::getColumnText(int column)
{ 
	PrefsDataFont *fep = m_prefsDataFont;
	static char    line[OUTLINER_COLUMN_MAX_LENGTH+1];

	*line = 0;

	switch (column) {

    case OUTLINER_COLUMN_ORDER:
		sprintf(line, "%d", fep->m_rowIndex+1); 
		break;

    case OUTLINER_COLUMN_DISPLAYER:
		sprintf(line, "%s", fep->font_displayers[fep->m_rowIndex]);
		break;

    default:
		break;
    }

	return line;
}

fe_icon *XFE_PrefsFontDialog::getColumnIcon(int /* column */)
{
	return 0;
}

void XFE_PrefsFontDialog::releaseLineData()
{
	// No-op
}

void XFE_PrefsFontDialog::Buttonfunc(const OutlineButtonFuncData *data)
{
	int row = data->row;

	if (row < 0) {
		// header
		return;
	} 

	setSelectionPos(data->row);
}

void XFE_PrefsFontDialog::Flippyfunc(const OutlineFlippyFuncData * /* data */)
{
	// No-op
}

XFE_Outliner *XFE_PrefsFontDialog::getOutliner()
{
	return m_prefsDataFont->font_outliner;
}
