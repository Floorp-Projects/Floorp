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
   MailFilterView.h -- view of user's mailfilters.
   Created: Chris Toshok <toshok@netscape.com>, 29-Aug-96.
 */



#include "MailFilterView.h"
#include "Frame.h"
#include "ViewGlue.h"

#include <Xm/ArrowBG.h>
#include <Xm/ToggleB.h>

/* for XP_GetString() */
#include <xpgetstr.h>
#include <felocale.h>

fe_icon XFE_MailFilterView::m_filterOnIcon = { 0 };
fe_icon XFE_MailFilterView::m_filterOffIcon = { 0 };

extern int XFE_MFILTER_INFO;
extern int XFE_CANNOT_EDIT_JS_MAILFILTERS;

#define OUTLINER_GEOMETRY_PREF "mail.mailfilter.outliner_geometry"

XFE_MailFilterView::XFE_MailFilterView(XFE_Component *toplevel_component,
									   Widget         parent, /* the dialog 
																 form */
									   XFE_View      *parent_view,
									   MWContext     *context,
									   MSG_Pane      *pane) 
	: XFE_MNListView(toplevel_component, parent_view, context, 
					 pane),
	  m_dataRow(-1),
	  m_goneFilter(0),
	  m_numGone(0)
{
	int ac;
	Arg av[20];

	m_pane=pane;

    /* Create main filter list dialog
     */
	m_filterlist = 0;

    /* Filter order. arrows
     */
    ac = 0;
    XtSetArg(av[ac], XmNorientation, XmVERTICAL); ac++;
    Widget orderBox = XmCreateRowColumn(parent, "orderBox", av, ac);

    /* Up button
     */
    m_upBtn = XtVaCreateManagedWidget("upArrow",
									  xmArrowButtonGadgetClass, orderBox,
									  XmNarrowDirection, XmARROW_UP,
									  NULL);
    XtAddCallback(m_upBtn, 
				  XmNactivateCallback, XFE_MailFilterView::upCallback, this);

    /* Down button
     */
    m_downBtn = XtVaCreateManagedWidget("downArrow",
										xmArrowButtonGadgetClass, orderBox,
										XmNarrowDirection, XmARROW_DOWN,
										NULL);
    XtAddCallback(m_downBtn,
				  XmNactivateCallback, XFE_MailFilterView::downCallback, this);


    m_server_dropdown = new XFE_FolderDropdown(toplevel_component,
                                               parent,
                                               TRUE, // allow server sel
                                               FALSE, // show newgroups
                                               FALSE, // boldwithnew
                                               FALSE); // showFolders
	/* In new 4.x spec
	 */    
	XmString info = XmStringCreateLtoR(XP_GetString(XFE_MFILTER_INFO),
									   XmFONTLIST_DEFAULT_TAG);
	Widget outlinerLabel = 
		XtVaCreateManagedWidget("outlinerLabel",
								xmLabelGadgetClass, parent,
								XmNlabelString, info,
								XmNalignment, XmALIGNMENT_BEGINNING,
								NULL);
	/* For outliner
	 */
	int num_columns = 3;
	static int column_widths[] = {3, 24, 3};
	m_outliner = new XFE_Outliner("mailFilterList",
								  this,
								  toplevel_component,
								  parent, //
								  False, // constantSize
								  False,  // hasHeadings
								  num_columns,
								  num_columns, /* num_visible */
								  column_widths,
								  OUTLINER_GEOMETRY_PREF);

	XtVaSetValues(m_outliner->getBaseWidget(),
				  XtVaTypedArg, XmNblankBackground, XmRString, "white", 6,
				  XmNselectionPolicy, XmSELECT_MULTIPLE_ROW,
				  XmNvisibleRows, 15,
				  NULL);

	XtVaSetValues(m_outliner->getBaseWidget(),
				  XmNcellDefaults, True,
				  XtVaTypedArg, XmNcellBackground, XmRString, "white", 6,
				  NULL);

	/* ICONS
	 */
  // initialize the icons if they haven't already been
  Pixel bg_pixel;
    
  XtVaGetValues(m_outliner->getBaseWidget(), XmNbackground, &bg_pixel, 0);
  if (!m_filterOnIcon.pixmap)
    fe_NewMakeIcon(getToplevel()->getBaseWidget(),
		   /* umm. fix me
		    */
		   BlackPixelOfScreen(XtScreen(m_outliner->getBaseWidget())),
		   bg_pixel,
		   &m_filterOnIcon,
		   NULL, 
		   MN_Check.width, 
		   MN_Check.height,
		   MN_Check.mono_bits, 
		   MN_Check.color_bits, 
		   MN_Check.mask_bits, 
		   FALSE);
  

  if (!m_filterOffIcon.pixmap)
    fe_NewMakeIcon(getToplevel()->getBaseWidget(),
		   /* umm. fix me
		    */
		   BlackPixelOfScreen(XtScreen(m_outliner->getBaseWidget())),
		   bg_pixel,
		   &m_filterOffIcon,
		   NULL, 
		   HUMarked.width, 
		   HUMarked.height,
		   HUMarked.mono_bits, 
		   HUMarked.color_bits, 
		   HUMarked.mask_bits, 
		   FALSE);
  

	/* 
	 * XFE_Outliner constructor does not allocate any content row
	 * XFE_Outliner::change(int first, int length, int newnumrows)
	 */

    /* Command buttons
     */
    Widget cmd_rowcol = XtVaCreateManagedWidget("btnrowcol", 
										   xmRowColumnWidgetClass, parent,
										   XmNorientation, XmVERTICAL,
										   NULL);
	/* always on 
	 */
    Widget newbtn = XtVaCreateWidget("newFilter", 
									 xmPushButtonGadgetClass, cmd_rowcol,
									 NULL);

    m_editBtn = XtVaCreateWidget("editFilter",
								 xmPushButtonGadgetClass, cmd_rowcol,
								 NULL);

    m_deleteBtn = XtVaCreateWidget("delFilter",
								   xmPushButtonGadgetClass, cmd_rowcol,
								   NULL);

	/* view log
	 */
    Widget log_rowcol = XtVaCreateManagedWidget("btnrowcol2", 
											xmRowColumnWidgetClass, parent,
											XmNorientation, XmVERTICAL,
											NULL);
    Widget viewLogBtn = XtVaCreateWidget("viewLog",
										 xmPushButtonGadgetClass, log_rowcol,
										 NULL);

    m_text = XtVaCreateWidget("text",
							  xmTextWidgetClass, parent,
							  XmNheight, 100,
							  XmNeditable, False,
							  NULL);

    m_logBtn = XtVaCreateWidget("logbtn",
								xmToggleButtonWidgetClass, parent,
								NULL);

    XtManageChild(m_text);
    XtManageChild(m_logBtn);
    XtManageChild(viewLogBtn);

    XtAddCallback(newbtn, 
				  XmNactivateCallback, XFE_MailFilterView::newCallback, 
				  this);
    XtAddCallback(m_editBtn, 
				  XmNactivateCallback, XFE_MailFilterView::editCallback, 
				  this);
    XtAddCallback(m_deleteBtn, 
				  XmNactivateCallback, XFE_MailFilterView::delCallback, 
				  this);
    XtAddCallback(m_logBtn, 
				  XmNvalueChangedCallback, XFE_MailFilterView::logCallback,
				  this);
    XtAddCallback(viewLogBtn, 
				  XmNactivateCallback, XFE_MailFilterView::viewLogCallback,
				  this);

    XtManageChild(cmd_rowcol);
    XtManageChild(newbtn);
    XtManageChild(m_editBtn);
    XtManageChild(m_deleteBtn);

    XtManageChild(orderBox);


    XtVaSetValues(m_text,
				  XmNtopAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_FORM,
				  0);
    XtVaSetValues(m_logBtn,
				  XmNtopAttachment, XmATTACH_NONE,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_WIDGET,
				  XmNbottomWidget, m_text,
				  0);
    XtVaSetValues(cmd_rowcol,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  0);
    XtVaSetValues(log_rowcol,
				  XmNtopAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_WIDGET,
				  XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNleftWidget, cmd_rowcol,
				  XmNbottomWidget, m_text,
				  0);
    XtVaSetValues(orderBox,
				  XmNtopAttachment, XmATTACH_NONE, 
				  XmNleftAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_WIDGET,
				  XmNrightWidget, cmd_rowcol,
				  XmNbottomAttachment, XmATTACH_WIDGET,
				  XmNbottomWidget, m_logBtn,
				  0);
    XtVaSetValues(m_server_dropdown->getBaseWidget(),
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
    XtVaSetValues(outlinerLabel,
				  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, m_server_dropdown->getBaseWidget(),
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_NONE,
				  XmNbottomAttachment, XmATTACH_NONE,
				  0);
    XtVaSetValues(m_outliner->getBaseWidget(),
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, outlinerLabel,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNrightAttachment, XmATTACH_WIDGET,
				  XmNrightWidget, orderBox,
				  XmNbottomAttachment, XmATTACH_WIDGET,
				  XmNbottomWidget, m_logBtn,
				  0);
    XtVaSetValues(m_upBtn, XmNsensitive, False, 0);
    XtVaSetValues(m_downBtn, XmNsensitive, False, 0);
    XtVaSetValues(m_deleteBtn, XmNsensitive, False, 0);
    XtVaSetValues(m_editBtn, XmNsensitive, False, 0);

	/* Now set Dlg values
	 */
	/* Get # of existing entry
	 */
	int32 count=0;
	MSG_FilterError err;
	/* the first argument is an enum, don't know why it isn't capped or
	   something to make it obvious...... backend stuff - bc */
	if (err = MSG_OpenFilterList(fe_getMNMaster(), 
								 filterInbox, 
								 &(m_filterlist))) {
		printf("\n  MSG_OpenFilterList: err=%d", err);
	}/* if */
	else {
		MSG_GetFilterCount(m_filterlist, &count);
		m_outliner->change(0, count, count);
		m_logOn = MSG_IsLoggingEnabled(m_filterlist);
		XmToggleButtonSetState(m_logBtn, m_logOn, False);
	}/* else */

	m_outliner->show();
	setBaseWidget(m_outliner->getBaseWidget());

	/* register interest
	 */
	getToplevel()->registerInterest(XFE_View::chromeNeedsUpdating,
									(XFE_NotificationCenter *) this,
									(XFE_FunctionNotification) updateCommands_cb);

}

void XFE_MailFilterView::listChanged(int which)
{
	int32 fcount = 0;
	MSG_GetFilterCount(m_filterlist, &fcount);

	if (which < 0)
		m_outliner->change(0, fcount, fcount);
	else if (which >= 0 && which < fcount)
		m_outliner->invalidateLine(which);

	// Notify interested
	getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating,
									this);

}/* XFE_MailFilterView::listChanged() */

XFE_CALLBACK_DEFN(XFE_MailFilterView, updateCommands)(XFE_NotificationCenter*,
													  void * /* clientData */, 
													  void * /* callData */)
{

	if (!m_filterlist)
		return;

	const int *selected = 0;
	int32 fcount = 0;
	MSG_GetFilterCount(m_filterlist, &fcount);

	int count;  
	m_outliner->getSelection(&selected, &count);
	if (fcount > 0 && count > 0) {
		if (count == 1) {

			XtVaSetValues(m_upBtn, XmNsensitive, 
						  (selected[0] > 0)?True:False, 0);
			XtVaSetValues(m_downBtn, XmNsensitive, 
						  (selected[0] < (fcount-1))?True:False, 0);
			XtVaSetValues(m_editBtn, XmNsensitive, True, 0);
			if ((selected[0] >= 0) &&
				FilterError_Success == MSG_GetFilterAt(m_filterlist, 
													   selected[0],
													   &m_filter)) {
				/* set text 
				 */
				char *text;
				MSG_GetFilterDesc(m_filter, &text);
				fe_SetTextField(m_text, text);
			}/* if */
		}/* if */
		else {
			XtVaSetValues(m_upBtn, XmNsensitive, False, 0);
			XtVaSetValues(m_downBtn, XmNsensitive, False, 0);
			XtVaSetValues(m_editBtn, XmNsensitive, False, 0);
		}/* else */
		XtVaSetValues(m_deleteBtn, XmNsensitive, True, 0);
	}/* if */
	else {
		XtVaSetValues(m_upBtn, XmNsensitive, False, 0);
		XtVaSetValues(m_downBtn, XmNsensitive, False, 0);
		XtVaSetValues(m_deleteBtn, XmNsensitive, False, 0);
		XtVaSetValues(m_editBtn, XmNsensitive, False, 0);
	}/* else */
}

/* Methods for the outlinable interface.
 */
char *XFE_MailFilterView::getCellTipString(int row, int column)
{
	char *tmp = 0;
	static char tipstr[128];
	tipstr[0] = '\0';

	if (row < 0) {
		/* header
		 */
		tmp = getColumnHeaderText(column);
	}/* if */
	else {
		MSG_Filter *filter = m_filter;
		if ((row >= 0) &&
			FilterError_Success == MSG_GetFilterAt(m_filterlist, 
												   row, &m_filter))
			/* content 
			 */
			tmp = getColumnText(column);
		
		/* reset
		 */
		m_filter = filter;
	}/* else */
	if (tmp && 
		(!m_outliner->isColTextFit(tmp, row, column)))
		XP_STRCPY(tipstr,tmp);
	return tipstr;
}

char *XFE_MailFilterView::getCellDocString(int /* row */, int /* column */)
{
	return NULL;
}

// Converts between an index and some non positional data type.
// Used to maintain selection when the list is reordered.
void *
XFE_MailFilterView::ConvFromIndex(int /*index*/)
{
  return 0;
}

// Converts between an index and some non positional data type.
// Used to maintain selection when the list is reordered.
int
XFE_MailFilterView::ConvToIndex(void */*item*/)
{
  return 0;
}

fe_icon *
XFE_MailFilterView::getColumnIcon(int column)
{
	fe_icon *myIcon = 0;

	if (column == OUTLINER_COLUMN_ON) {
		XP_Bool enabled;

		if (FilterError_Success == MSG_IsFilterEnabled(m_filter, &enabled))
			myIcon = (enabled?(&m_filterOnIcon):(&m_filterOffIcon));
	}/* if */
	return myIcon;
}

char *
XFE_MailFilterView::getColumnText(int column)
{
	if (!m_filter || m_dataRow < 0)	
		return 0;

	char *locale = 0;
	static char name[1024];
	name[0] = '\0';
	switch (column) {
	case OUTLINER_COLUMN_ORDER :
		  sprintf(name, "%d", m_dataRow+1);

		  locale = (char *) fe_ConvertToLocaleEncoding
			(INTL_DefaultWinCharSetID(NULL), (unsigned char *) name);
		  PR_snprintf(name, sizeof(name), "%s", locale);
		  if ((char *) locale != name) {
			  XP_FREE(locale);
		  }/* if */

		break;
			
	case OUTLINER_COLUMN_NAME :
		{
			char *filterName = 0;
			MSG_GetFilterName(m_filter, &filterName);
			sprintf(name, "%s", filterName);
			locale = (char *) fe_ConvertToLocaleEncoding
				(INTL_DefaultWinCharSetID(NULL), (unsigned char *) name);
			PR_snprintf(name, sizeof(name), "%s", locale);
			if ((char *) locale != name) {
				XP_FREE(locale);
			}/* if */
		}
		break;
		
	case OUTLINER_COLUMN_ON :
	break;
	  
	default :
		XP_ASSERT(0);
		break;
		
	}/* switch */
	return name;
}

char *
XFE_MailFilterView::getColumnName(int column)
{
	switch (column) {
	case OUTLINER_COLUMN_ORDER :
		return XP_STRDUP("Order");
	case OUTLINER_COLUMN_NAME :
		return XP_STRDUP("Name");
	case OUTLINER_COLUMN_ON :
		return XP_STRDUP("OnOff");
	}/* switch */
	return XP_STRDUP("unDefined");
}

/* This method acquires one line of data: entryID is set for getColumnText
 */
void*
XFE_MailFilterView::acquireLineData(int line)
{
	m_dataRow = line;
	m_filter = 0;
	
	if ((line >= 0) &&
		FilterError_Success == MSG_GetFilterAt(m_filterlist, 
											   line, &m_filter))
		return (void *) m_filter;
	else
		return (void*) 0;
}

// Returns the text and/or icon to display at the top of the column.
EOutlinerTextStyle 
XFE_MailFilterView::getColumnHeaderStyle(int /*column*/)
{
  return OUTLINER_Default;
}

/*
 * The following 4 requests deal with the currently acquired line.
 */
EOutlinerTextStyle 
XFE_MailFilterView::getColumnStyle(int /*column*/)
{
  /* To be refined
   */
  return OUTLINER_Default;
}

//
void
XFE_MailFilterView::getTreeInfo(XP_Bool */*expandable*/,
				   XP_Bool */*is_expanded*/, 
				   int *depth, 
				   OutlinerAncestorInfo **/*ancestor*/)
{
  depth = 0;
}

//
void 
XFE_MailFilterView::Buttonfunc(const OutlineButtonFuncData* data)
{
	int row = data->row,
		col = data->column,
		clicks = data->clicks;

	if (row < 0)
		return;
	else {
		/* content row 
		 */
		if (clicks == 2) {
			m_outliner->selectItemExclusive(row);
			doubleClickBody(data);
		}/* clicks == 2 */
		else if (clicks == 1) {
			if (FilterError_Success == 
				MSG_GetFilterAt(m_filterlist, row, &m_filter)) {
				
				if (col == OUTLINER_COLUMN_ON) {
					XP_Bool enabled;
					if (FilterError_Success ==
						MSG_IsFilterEnabled(m_filter, &enabled)) {  
						if (FilterError_Success ==
							MSG_EnableFilter(m_filter, !enabled))
							m_outliner->invalidateLine(row);
					}/* if MSG_IsFilterEnabled */
				}/* if col == OUTLINER_COLUMN_ON */
			}/* if */
			else {
				printf("\n MSG_GetFilterAt err");
			}/* else */
			m_outliner->selectItemExclusive(row);
			getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating,
											this);
		}/* clicks == 1 */
	}/* else */
}

void 
XFE_MailFilterView::Flippyfunc(const OutlineFlippyFuncData */*data*/)
{
}

/* Tells the Outlinable object that the line data is no
 * longer needed, and it can free any storage associated with it.
 */ 
void
XFE_MailFilterView::releaseLineData()
{
}

void XFE_MailFilterView::doubleClickBody(const OutlineButtonFuncData * /* data */)
{
	editCB(NULL, NULL);

}/* doubleClickBody() */

// callbacks
void XFE_MailFilterView::upCallback(Widget w, 
					     XtPointer clientData, 
					     XtPointer callData)
{
  XFE_MailFilterView *obj = (XFE_MailFilterView *) clientData;
  obj->upCB(w, callData);
}/* XFE_MailFilterView::upCallback() */

void XFE_MailFilterView::upCB(Widget , XtPointer)
{
	const int *selected = 0;
	int count;  
	m_outliner->getSelection(&selected, &count);

	if (count > 0 && selected && selected[0] > 0) {
		MSG_MoveFilterAt(m_filterlist, (MSG_FilterIndex) selected[0], filterUp);
		m_outliner->invalidate();
		m_outliner->selectItemExclusive(selected[0]-1);
		// Notify interested
		getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating,
										this);
	}/* if */
}/* XFE_MailFilterView::upCB() */

void XFE_MailFilterView::downCallback(Widget w, 
					     XtPointer clientData, 
					     XtPointer callData)
{
  XFE_MailFilterView *obj = (XFE_MailFilterView *) clientData;
  obj->downCB(w, callData);
}/* XFE_MailFilterView::downCallback() */

void XFE_MailFilterView::downCB(Widget , XtPointer)
{
	int32 filterCount;
	MSG_GetFilterCount(m_filterlist, &filterCount);

	const int *selected = 0;
	int count;  
	m_outliner->getSelection(&selected, &count);

	if (count && 
		selected && 
		(selected[0] < (filterCount-1))) {
		MSG_MoveFilterAt(m_filterlist, (MSG_FilterIndex) selected[0], filterDown);
		m_outliner->invalidate();
		m_outliner->selectItemExclusive(selected[0]+1);
		// Notify interested
		getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating,
										this);
	}/* if */
}/* XFE_MailFilterView::downCB() */

void XFE_MailFilterView::newCallback(Widget w, 
					     XtPointer clientData, 
					     XtPointer callData)
{
  XFE_MailFilterView *obj = (XFE_MailFilterView *) clientData;
  obj->newCB(w, callData);
}/* XFE_MailFilterView::newCallback() */

void XFE_MailFilterView::editCallback(Widget w, 
					     XtPointer clientData, 
					     XtPointer callData)
{
  XFE_MailFilterView *obj = (XFE_MailFilterView *) clientData;
  obj->editCB(w, callData);
}/* XFE_MailFilterView::editCallback() */

void XFE_MailFilterView::delCallback(Widget w, 
					     XtPointer clientData, 
					     XtPointer callData)
{
  XFE_MailFilterView *obj = (XFE_MailFilterView *) clientData;
  obj->delCB(w, callData);
}/* XFE_MailFilterView::delCallback() */

void XFE_MailFilterView::delCB(Widget , XtPointer)
{
	const int *selected = 0;
	int count;  
	m_outliner->getSelection(&selected, &count);
	if (count && selected) {

		MSG_Filter *f;
		MSG_GetFilterAt(m_filterlist, selected[0],&f);
		MSG_RemoveFilterAt(m_filterlist, selected[0]);

		if (!m_numGone)
			m_goneFilter = 
				(MSG_Filter **) XP_CALLOC(1, sizeof(MSG_Filter*));
		else
			m_goneFilter = 
				(MSG_Filter **) XP_REALLOC(m_goneFilter, 
											   (m_numGone+1)*sizeof(MSG_Filter*));
		if (m_goneFilter) {
			m_goneFilter[m_numGone] = f;
			m_numGone++;
		}/* if */

		/* Clear Descr
		 */
		fe_SetTextField(m_text, "");
      
		int32 fcount = 0;
		MSG_GetFilterCount(m_filterlist, &fcount);
		m_outliner->change(0, fcount, fcount);
		if (selected[0] <= (fcount-1))
			m_outliner->selectItemExclusive(selected[0]);
		else 
			m_outliner->selectItemExclusive(fcount-1);    

		getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating,
										this);
	}/* if count && selected */
  
}/* XFE_MailFilterView::delCB() */

void XFE_MailFilterView::logCallback(Widget w, 
					     XtPointer clientData, 
					     XtPointer callData)
{
  XFE_MailFilterView *obj = (XFE_MailFilterView *) clientData;
  obj->logCB(w, callData);
}/* XFE_MailFilterView::logCallback() */

void XFE_MailFilterView::logCB(Widget w, XtPointer)
{
  if (XmToggleButtonGetState(w))
    m_logOn = True;
  else
    m_logOn = False;
}/* XFE_MailFilterView::logCB() */

void XFE_MailFilterView::viewLogCallback(Widget w, 
					     XtPointer clientData, 
					     XtPointer callData)
{
  XFE_MailFilterView *obj = (XFE_MailFilterView *) clientData;
  obj->viewLogCB(w, callData);
}/* XFE_MailFilterView::viewLogCallback() */

void XFE_MailFilterView::viewLogCB(Widget, XtPointer)
{
    MSG_ViewFilterLog(m_pane);
}/* XFE_MailFilterView::viewLogCB() */

void XFE_MailFilterView::apply()
{
  MSG_FilterError err;
  if (m_filterlist &&
      (err = MSG_EnableLogging(m_filterlist, m_logOn)))
    printf("\n  MSG_EnableLogging: err=%d", err);

  for (int i = 0; i < m_numGone; i++) {
		MSG_DestroyFilter(m_goneFilter[i]);
  }/* for i */

  if (m_goneFilter)
	XP_FREE((MSG_Filter **) m_goneFilter);

  if (err = MSG_CloseFilterList(m_filterlist))
	  printf("MSG_CloseFilterList: err=%d", err);

}/* XFE_MailFilterView::apply() */

void XFE_MailFilterView::cancel()
{  
	MSG_CancelFilterList( m_filterlist );
}/* XFE_MailFilterView::cancel() */

extern "C" void
fe_mailfilt_edit(MWContext *context, XP_Bool isNew, XP_Bool isJavaS);

extern "C"  void
fe_showMailFilterRulesDlg(XFE_Component *subscriber,
						  Widget toplevel, MWContext *context,
						  MSG_FilterList *list, MSG_FilterIndex at,
						  XP_Bool isNew);

void XFE_MailFilterView::newCB(Widget , XtPointer)
{
	fe_showMailFilterRulesDlg(getToplevel(),
							  getToplevel()->getBaseWidget(), 
							  m_contextData,
							  m_filterlist, -1, True);

}/* XFE_MailFilterView::newCB() */

void XFE_MailFilterView::editCB(Widget , XtPointer)
{
	const int *selected = 0;
	int count;  
	m_outliner->getSelection(&selected, &count);
	if (count && selected) {
		MSG_FilterType filter_type = filterInboxJavaScript;
		MSG_Filter *f;
		MSG_FilterError err = MSG_GetFilterAt(m_filterlist, 
											  (MSG_FilterIndex) selected[0], &f);

		if (err)
			return;

		MSG_GetFilterType(f, &filter_type);
		if ( filter_type == filterInboxJavaScript || filter_type == filterNewsJavaScript )
			{
				FE_Alert(m_contextData, XP_GetString(XFE_CANNOT_EDIT_JS_MAILFILTERS));
				return;
			}

		fe_showMailFilterRulesDlg(getToplevel(),
								  getToplevel()->getBaseWidget(),
								  m_contextData,
								  m_filterlist, selected[0], 
								  False);
	}/* if */
}/* XFE_MailFilterView::editCB() */

XFE_MailFilterView::~XFE_MailFilterView()
{
#if 0
	fe_mailfilt_data *d = FILTER_DATA(m_contextData);
	if (d)
	  XP_FREE((fe_mailfilt_data *) d);
	FILTER_DATA(m_contextData) = 0;
#endif
	getToplevel()->unregisterInterest(XFE_View::chromeNeedsUpdating,
									  (XFE_NotificationCenter *)this,
									  (XFE_FunctionNotification)updateCommands_cb);


}

