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
   ABAddresseeView.cpp -- class definition for XFE_AddresseeView
   Created: Tao Cheng <tao@netscape.com>, 20-nov-96
 */

#include "ABAddresseeView.h"

#include <Xm/Form.h>
#include <Xm/PushB.h>

#include "icondata.h"

#include "xpgetstr.h"

extern int XFE_AB_STATE;
extern int XFE_AB_HEADER_NAME;
extern int XFE_AB_PICKER_TO;
extern int XFE_AB_PICKER_CC;
extern int XFE_AB_PICKER_BCC;

#ifndef AB_MAX_STRLEN
#define AB_MAX_STRLEN 1024
#endif

#define OUTLINER_GEOMETRY_PREF "addressbook.addresseepane.outliner_geometry"

// icons
fe_icon XFE_AddresseeView::m_personIcon = { 0 };
fe_icon XFE_AddresseeView::m_listIcon = { 0 };

XFE_AddresseeView::XFE_AddresseeView(XFE_Component *toplevel_component,
				     Widget parent /* the form */,
				     XFE_View *parent_view, MWContext *context
				     ):
  XFE_View(toplevel_component, parent_view, context),
  m_pair(0),
  m_pairs(0),
  m_numPairs(0)
{
  m_okBtn = NULL;

  int ac;
  Arg av[20];

  /* Create Type-in Filter Box
   */
  /* remove button
   */
  Widget cmdForm;

  cmdForm = XtVaCreateManagedWidget("cmdForm",
									 xmFormWidgetClass,
									 parent,
									 XmNleftAttachment, XmATTACH_FORM,
									 XmNtopAttachment, XmATTACH_NONE,
									 XmNrightAttachment, XmATTACH_NONE,
									 XmNbottomAttachment, XmATTACH_FORM,
									 NULL);
  
  Widget dummy = 
	  XtVaCreateManagedWidget("dummyRowcol", 
							  xmRowColumnWidgetClass, cmdForm,
							  XmNorientation, XmHORIZONTAL,
							  XmNleftAttachment, XmATTACH_FORM,
							  XmNtopAttachment, XmATTACH_FORM,
							  XmNrightAttachment, XmATTACH_FORM,
							  XmNbottomAttachment, XmATTACH_FORM,
							  NULL);
  ac = 0;
  XtSetArg(av[ac], XtNsensitive, False), ac++;
  m_removeBtn = XmCreatePushButton(dummy, 
								   "removeBtn", 
								   av, ac);
  XtManageChild(m_removeBtn);

  /* For outliner
   */
  Widget listForm;
  listForm = XtVaCreateManagedWidget("listForm",
									 xmFormWidgetClass,
									 parent,
									 XmNleftAttachment, XmATTACH_FORM,
									 XmNtopAttachment, XmATTACH_FORM,
									 XmNrightAttachment, XmATTACH_FORM,
									 XmNbottomAttachment, XmATTACH_WIDGET,
									 XmNbottomWidget, cmdForm,
									 NULL);
  int num_columns = OUTLINER_COLUMN_LAST;
  static int column_widths[] = {5, 45};
  int count = 5;
  m_outliner = new XFE_Outliner("addresseeList",
								this,
								toplevel_component,
								listForm,
								False, // constantSize
								False,  // hasHeadings
								num_columns,
                                num_columns,
								column_widths,
								OUTLINER_GEOMETRY_PREF);
  /* BEGIN_3P: XmLGrid
   */ 
  XtVaSetValues(m_outliner->getBaseWidget(),
                XtVaTypedArg, XmNblankBackground, XmRString, "white", 6,
				XmNselectionPolicy, XmSELECT_MULTIPLE_ROW,
				XmNvisibleRows, count,
				NULL);
  XtVaSetValues(m_outliner->getBaseWidget(),
				XmNcellDefaults, True,
                XtVaTypedArg, XmNcellBackground, XmRString, "white", 6,	
				NULL);
  /* END_3P: XmLGrid
   */

  /* 
   * XFE_Outliner constructor does not allocate any content row
   * XFE_Outliner::change(int first, int length, int newnumrows)
   */
  // m_outliner->change(0, count, count);
  m_outliner->show();
  setBaseWidget(m_outliner->getBaseWidget());

  /*
   * Provide callbacks as for entries outside world
   */
  XtAddCallback(m_removeBtn, 
		XmNactivateCallback, 
		XFE_AddresseeView::removeCallback, 
		this);

  XtAddCallback(m_outliner->getBaseWidget(), 
		XmNselectCallback, 
		XFE_AddresseeView::selectCallback, 
		this);


  /* register interest
   */
  getToplevel()->registerInterest(XFE_View::chromeNeedsUpdating,
								  (XFE_NotificationCenter *) this,
								  (XFE_FunctionNotification) updateCommands_cb);

  XtVaSetValues(m_outliner->getBaseWidget(),
  		XmNleftAttachment, XmATTACH_FORM,
		XmNtopAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_WIDGET,
		XmNbottomWidget, dummy,
		NULL);

  // initialize the icons if they haven't already been
  {
    Pixel bg_pixel;
    
    XtVaGetValues(m_outliner->getBaseWidget(), XmNbackground, &bg_pixel, 0);
    if (!m_personIcon.pixmap)
      fe_NewMakeIcon(getToplevel()->getBaseWidget(),
		     /* umm. fix me
		      */
		     BlackPixelOfScreen(XtScreen(m_outliner->getBaseWidget())),
		     bg_pixel,
		     &m_personIcon,
		     NULL, 
		     MN_Person.width, 
		     MN_Person.height,
		     MN_Person.mono_bits, 
		     MN_Person.color_bits, 
		     MN_Person.mask_bits, 
		     FALSE);

    if (!m_listIcon.pixmap)
      fe_NewMakeIcon(getToplevel()->getBaseWidget(),
		     /* umm. fix me
		      */
		     BlackPixelOfScreen(XtScreen(m_outliner->getBaseWidget())),
		     bg_pixel,
		     &m_listIcon,
		     NULL, 
		     MN_People.width, 
		     MN_People.height,
		     MN_People.mono_bits, 
		     MN_People.color_bits, 
		     MN_People.mask_bits, 
		     FALSE);

  }
}/* XFE_AddresseeView::XFE_AddresseeView */

XFE_AddresseeView::~XFE_AddresseeView()
{
	getToplevel()->unregisterInterest(XFE_View::chromeNeedsUpdating,
									  (XFE_NotificationCenter *)this,
									  (XFE_FunctionNotification)updateCommands_cb);

}

void XFE_AddresseeView::setOKBtn(Widget w)
{
	m_okBtn = w;
}

XFE_CALLBACK_DEFN(XFE_AddresseeView, updateCommands)(XFE_NotificationCenter*,
													  void * /* clientData */, 
													  void * /* callData */)
{


	/* check which is selected
	 */
	int count = 0; // = XmLGridGetSelectedRowCount(w);
	const int *indices = 0;
	m_outliner->getSelection(&indices, &count);
	if (m_numPairs > 0) {
		if (m_okBtn)
			XtSetSensitive(m_okBtn, True);		
		
		if (count > 0 && 
			indices) {
			XtSetSensitive(m_removeBtn, True);		
		}/* if */
		else
			XtSetSensitive(m_removeBtn, False);	
	}/* if */
	else {
		if (m_okBtn)
			XtSetSensitive(m_okBtn, False);	
		XtSetSensitive(m_removeBtn, False);	
	}/* else */	
}

void XFE_AddresseeView::addEntry(StatusID_t* pair)
{
  if (!m_pairs)
    m_pairs = (StatusID_t**) calloc(1, 
				    sizeof(StatusID_t*));
  else
    m_pairs = (StatusID_t**) realloc(m_pairs, 
				     (m_numPairs+1)*sizeof(StatusID_t*));

  m_pairs[m_numPairs++] = pair;
  m_outliner->change(0, m_numPairs, m_numPairs);
  if (m_numPairs)
	  m_outliner->makeVisible(m_numPairs-1);

  getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
}

/* Methods for the outlinable interface.
 */

char *XFE_AddresseeView::getCellTipString(int row, int column)
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
		if (row >= m_numPairs)
			return 0;

		StatusID_t *pair = m_pair;
		m_pair = m_pairs[row];

		/* content 
		 */
		tmp = getColumnText(column);

		/* reset
		 */
		m_pair = pair;
	}/* else */
	if (tmp && 
		(!m_outliner->isColTextFit(tmp, row, column)))
		XP_STRCPY(tipstr,tmp);
	return tipstr;
}

char *XFE_AddresseeView::getCellDocString(int /* row */, int /* column */)
{
	return NULL;
}
// Converts between an index and some non positional data type.
// Used to maintain selection when the list is reordered.
void *
XFE_AddresseeView::ConvFromIndex(int /*index*/)
{
  return 0;
}

// Converts between an index and some non positional data type.
// Used to maintain selection when the list is reordered.
int
XFE_AddresseeView::ConvToIndex(void */*item*/)
{
  return 0;
}

/* This method acquires one line of data: entryID is set for getColumnText
 */
void*
XFE_AddresseeView::acquireLineData(int line)
{
  /* Test if 
   * 1. any pair has been added into addressee view;
   * 2. line # falls into the right range
   */
  if (!m_pairs || 
      (line >= m_numPairs))
    return 0;

  m_pair = m_pairs[line];
  return m_pair;
}

/* The Outlinable interface.
 */
/* Returns the text and/or icon to display at the top of the column.
 */
char*
XFE_AddresseeView::getColumnName(int column)
{
  switch (column) {
  case OUTLINER_COLUMN_STATUS:
	  return XP_GetString(XFE_AB_STATE);
  case OUTLINER_COLUMN_NAME:
	  return XP_GetString(XFE_AB_HEADER_NAME);
  }/* switch () */
  return "unDefined";
}

char*
XFE_AddresseeView::getColumnHeaderText(int /* column */)
{
  return 0;
}

// Returns the text and/or icon to display at the top of the column.
fe_icon*
XFE_AddresseeView::getColumnHeaderIcon(int /*column*/)
{
  return 0;
}

// Returns the text and/or icon to display at the top of the column.
EOutlinerTextStyle 
XFE_AddresseeView::getColumnHeaderStyle(int /*column*/)
{
  return OUTLINER_Default;
}

/*
 * The following 4 requests deal with the currently acquired line.
 */
EOutlinerTextStyle 
XFE_AddresseeView::getColumnStyle(int /*column*/)
{
  /* To be refined
   */
  return OUTLINER_Default;
}

char*
XFE_AddresseeView::getColumnText(int column)
{
  static char a_line[AB_MAX_STRLEN];
  XP_STRCPY(a_line, "");
  switch (column) {
  case OUTLINER_COLUMN_STATUS:
    if (m_pair->status == TO)
		XP_SAFE_SPRINTF(a_line, sizeof(a_line), "%s",
						XP_GetString(XFE_AB_PICKER_TO));
    else if (m_pair->status == CC)
 		XP_SAFE_SPRINTF(a_line, sizeof(a_line), "%s",
						XP_GetString(XFE_AB_PICKER_CC));
    else if (m_pair->status == BCC)
 		XP_SAFE_SPRINTF(a_line, sizeof(a_line), "%s",
						XP_GetString(XFE_AB_PICKER_BCC));
      
    break;
#if 0
  case OUTLINER_COLUMN_TYPE:
    break;
#endif
  case OUTLINER_COLUMN_NAME:
    XP_STRCPY(a_line, m_pair->dplyStr);

    break;

  }/* switch () */
  return a_line;
}

fe_icon*
XFE_AddresseeView::getColumnIcon(int /* column */)
{
  fe_icon *myIcon = 0;
#if US_WEST_01
  switch (column) {
  case OUTLINER_COLUMN_TYPE:
      if (m_pair) {
		  if (m_pair->type == ABTypePerson)
			  myIcon = &m_personIcon; /* shall call make/initialize icons */
		  else if (m_pair->type == ABTypeList)
			  myIcon = &m_listIcon;
      }/* if */
	  break;
  }/* switch () */
#endif
  return myIcon;
}

//
void
XFE_AddresseeView::getTreeInfo(XP_Bool */*expandable*/,
				   XP_Bool */*is_expanded*/, 
				   int *depth, 
				   OutlinerAncestorInfo **/*ancestor*/)
{
  depth = 0;
}
 
//
void 
XFE_AddresseeView::Buttonfunc(const OutlineButtonFuncData *data)
{
	int row = data->row, 
		clicks = data->clicks;

	if (row < 0) {
		return;
	} 
	else {
		/* content row 
		 */
		ABID entry = m_pairs[row]->id;
    
		if (entry == MSG_VIEWINDEXNONE) 
			return;

		if (clicks == 2) {
			m_outliner->selectItemExclusive(data->row);
		}/* clicks == 2 */
		else if (clicks == 1) {
			if (data->shift) {
				// select the range.
				const int *selected;
				int count;
  
				m_outliner->getSelection(&selected, &count);
				
				if (count == 0) { /* there wasn't anything selected yet. */
	  
					m_outliner->selectItemExclusive(data->row);
				}/* if count == 0 */
				else if (count == 1) {
					/* there was only one, so we select the range from
					   that item to the new one. */
					
					m_outliner->selectRangeByIndices(selected[0], 
													 data->row);
				}/* count == 1 */
				else {
					/* we had a range of items selected, 
					 * so let's do something really
					 * nice with them. */
	  
					m_outliner->trimOrExpandSelection(data->row);
				}/* else */
			}/* if */
			else {
				m_outliner->selectItemExclusive(data->row);
			}/* else */
			
			getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
		}/* clicks == 1 */
	}/* else */
	
}/* XFE_AddresseeView::Buttonfunc() */

void 
XFE_AddresseeView::Flippyfunc(const OutlineFlippyFuncData */*data*/)
{
}

/* Tells the Outlinable object that the line data is no
 * longer needed, and it can free any storage associated with it.
 */ 
void
XFE_AddresseeView::releaseLineData()
{
}

void
XFE_AddresseeView::removeCallback(Widget w, 
				 XtPointer clientData, 
				 XtPointer callData)
{
  XFE_AddresseeView *obj = (XFE_AddresseeView *) clientData;
  obj->removeCB(w, callData);
}

void
XFE_AddresseeView::removeCB(Widget /* w */, 
			   XtPointer /* callData */)
{
  /* Get selection
   */
  /* check which is selected
   */
  int count = 0; // = XmLGridGetSelectedRowCount(w);
  const int *indices = 0;
  m_outliner->getSelection(&indices, &count);
  if (count > 0 && indices && m_numPairs) {
    StatusID_t **tmp = (StatusID_t **) XP_CALLOC(m_numPairs-1, 
						 sizeof(StatusID_t*));
    int j = 0;
    for (int i=0; i < m_numPairs; i++)
      if (i != indices[0]) 
	tmp[j++] = m_pairs[i];
      else
	XP_FREE((StatusID_t *) m_pairs[i]);
    XP_FREE((StatusID_t **) m_pairs);
    m_pairs = tmp;
    m_numPairs--;
	m_outliner->change(0, m_numPairs, m_numPairs);
	getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
  }/* if */

}/* XFE_AddresseeView::removeCB() */

void
XFE_AddresseeView::selectCallback(Widget w, 
				 XtPointer clientData, 
				 XtPointer callData)
{
  XFE_AddresseeView *obj = (XFE_AddresseeView *) clientData;
  obj->selectCB(w, callData);
}

void
XFE_AddresseeView::selectCB(Widget /*w*/,
			   XtPointer /* callData */)
{
  int count = 0; // = XmLGridGetSelectedRowCount(w);
  const int *indices = 0;
  m_outliner->getSelection(&indices, &count);
}/* XFE_AddresseeView::selectCB() */

