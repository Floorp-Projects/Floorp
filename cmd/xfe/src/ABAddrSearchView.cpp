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
   ABAddrSearchView.cpp -- class definition for XFE_AddrSearchView
   Created: Tao Cheng <tao@netscape.com>, 20-nov-96
 */

#include "ABAddrSearchView.h"
#include "ABAddresseeView.h"

#include "xpassert.h"

#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/TextF.h>


extern "C" {
#include "xfe.h"

XP_List* FE_GetDirServers();
ABook*   fe_GetABook(MWContext *context);
XtPointer fe_tooltip_mappee(Widget widget, XtPointer data);
}

#include "xpgetstr.h"

extern int XFE_AB_HEADER_NAME;
extern int XFE_AB_HEADER_EMAIL;

extern int XFE_AB_HEADER_NICKNAME;
extern int XFE_AB_HEADER_EMAIL;
extern int XFE_AB_HEADER_COMPANY;
extern int XFE_AB_HEADER_PHONE;
extern int XFE_AB_HEADER_LOCALITY;

#ifndef AB_MAX_STRLEN
#define AB_MAX_STRLEN 1024
#endif

#define OUTLINER_GEOMETRY_PREF "addressbook.searchpane.outliner_geometry"

XFE_AddrSearchView::XFE_AddrSearchView(XFE_Component *toplevel_component,
									   Widget parent /* the form */,
									   XFE_View *parent_view, 
									   MWContext *context,
									   XP_List   *directories):
	XFE_ABListSearchView(toplevel_component,
						 parent,
						 parent_view, 
						 context,
						 directories)
{
  /* Properties
   */
  m_propertyBtn = NULL;

  /* For outliner
   */
  int num_columns = OUTLINER_COLUMN_LAST;
  static int column_widths[] = {3, 20, 20, 10, 10, 8, 8};
  m_outliner = new XFE_Outliner("addressList",
								this,
								toplevel_component,
								parent,
								False, // constantSize
								True,  // hasHeadings
								num_columns,
								num_columns-2,// num_visible 
				column_widths,
				OUTLINER_GEOMETRY_PREF);
  m_outliner->setHideColumnsAllowed(True);

  /* BEGIN_3P: XmLGrid
   */
  XtVaSetValues(m_outliner->getBaseWidget(),
                XtVaTypedArg, XmNblankBackground, XmRString, "white", 6,
				XmNselectionPolicy, XmSELECT_MULTIPLE_ROW,
				XmNvisibleRows, 15,
				NULL);
  XtVaSetValues(m_outliner->getBaseWidget(),
				XmNcellDefaults, True,
                XtVaTypedArg, XmNcellBackground, XmRString, "white", 6,
				NULL);
  /* END_3P: XmLGrid
   */

  /* Attachment
   */
  layout();

  /* 
   * XFE_Outliner constructor does not allocate any content row
   * XFE_Outliner::change(int first, int length, int newnumrows)
   */
#if !defined(USE_ABCOM)
  uint32 count; 
  /* Get # of existing entry
   */
  AB_GetEntryCount (m_dir, m_AddrBook, &count, (ABID) ABTypeAll, 0);
  m_outliner->change(0, count, count);
#endif /* USE_ABCOM */
  m_outliner->show();
  setBaseWidget(m_outliner->getBaseWidget());

  /* register interest
   */
  getToplevel()->registerInterest(XFE_View::chromeNeedsUpdating,
								  (XFE_NotificationCenter *) this,
								  (XFE_FunctionNotification) updateCommands_cb);

}/* XFE_AddrSearchView::XFE_AddrSearchView */

XFE_AddrSearchView::~XFE_AddrSearchView()
{
	getToplevel()->unregisterInterest(XFE_View::chromeNeedsUpdating,
									  (XFE_NotificationCenter *)this,
									  (XFE_FunctionNotification)updateCommands_cb);

}

XFE_CALLBACK_DEFN(XFE_AddrSearchView, updateCommands)(XFE_NotificationCenter*,
													  void * /* clientData */, 
													  void * /* callData */)
{
	if (!m_propertyBtn || 
		!m_toBtn || 
		!m_ccBtn || 
		!m_bccBtn)
		return;

#if defined(USE_ABCOM)
	uint32 count32 = MSG_GetNumLines(m_pane);
	// XtSetSensitive(m_propertyBtn, True);
#else
	/* properties
	 */
	XtSetSensitive(m_propertyBtn, m_dir?True:False);		

	/* check which is selected
	 */
	uint32 count32 = 0; 
	/* Get # of existing entry
	 */
	AB_GetEntryCount (m_dir, m_AddrBook, &count32, (ABID) ABTypeAll, 0);
#endif /* USE_ABCOM */


	int count = 0; 
	const int *indices = 0;
	m_outliner->getSelection(&indices, &count);
	if (count32 > 0 &&
		count > 0 && 
		indices) {
		XtSetSensitive(m_toBtn, True);
		XtSetSensitive(m_ccBtn, True);
		XtSetSensitive(m_bccBtn, True);
#if 0
		if (m_dir->dirType == LDAPDirectory)
			XtSetSensitive(m_addToAddressBtn, True);
		else
			XtSetSensitive(m_addToAddressBtn, False);
#endif
	}/* if */
	else {
		XtSetSensitive(m_toBtn, False);
		XtSetSensitive(m_ccBtn, False);
		XtSetSensitive(m_bccBtn, False);
#if 0
		XtSetSensitive(m_addToAddressBtn, False);
#endif		
	}/* else */
}

void XFE_AddrSearchView::doubleClickBody(const OutlineButtonFuncData */* data */)
{
	toCB(NULL, NULL);
}

// callbacks
void XFE_AddrSearchView::toCallback(Widget w, 
				 XtPointer clientData, 
				 XtPointer callData)
{
  XFE_AddrSearchView *obj = (XFE_AddrSearchView *) clientData;
  obj->toCB(w, callData);
}

void XFE_AddrSearchView::ccCallback(Widget w, 
				 XtPointer clientData, 
				 XtPointer callData)
{
  XFE_AddrSearchView *obj = (XFE_AddrSearchView *) clientData;
  obj->ccCB(w, callData);
}

void XFE_AddrSearchView::bccCallback(Widget w, 
				 XtPointer clientData, 
				 XtPointer callData)
{
  XFE_AddrSearchView *obj = (XFE_AddrSearchView *) clientData;
  obj->bccCB(w, callData);
}

StatusID_t*
XFE_AddrSearchView::makePair(const int ind, SEND_STATUS status)
{
	ABID entry = MSG_VIEWINDEXNONE;
	int error = AB_GetABIDForIndex(m_pane,
								   (MSG_ViewIndex) ind,
								   &entry);
    StatusID_t *pair = (StatusID_t *) XP_CALLOC(1, sizeof(StatusID_t));
    pair->status = status;
    pair->dir = m_dir;
    pair->id = entry;

	uint16      numItems = 2;
	AB_AttribID *attribs = (AB_AttribID *) XP_CALLOC(numItems, 
													 sizeof(AB_AttribID));
	attribs[0] = AB_attribEntryType;
	attribs[1] = AB_attribFullAddress;


	AB_AttributeValue *values = NULL;
	error = 
		AB_GetEntryAttributesForPane(m_pane,
									 ind,
									 attribs,
									 &values,
									 &numItems);
	XP_ASSERT(values);

    int i;
	for (i=0; i < numItems; i++) {
		switch (values[i].attrib) {
		case AB_attribEntryType:
			pair->type = values[i].u.entryType;
			break;

		case AB_attribFullAddress:
			pair->dplyStr = 
				!EMPTY_STRVAL(&(values[i]))?XP_STRDUP(values[i].u.string)
				                           :(char *)NULL;
			break;
		default:
			XP_ASSERT(0);
			break;
		}/* switch */
	}/* for i */
    return pair;
}

void
XFE_AddrSearchView::toCB(Widget /*w*/,
			   XtPointer /* callData */)
{
  /* check which is selected
   */
  int count = 0;
  const int *indices = 0;
  m_outliner->getSelection(&indices, &count);
  if (count > 0 && indices) {
#if defined(USE_ABCOM)
	  StatusID_t *pair = makePair(indices[0], TO);
#else
    /* Take the first one 
     */
    ABID entry = AB_GetEntryIDAt((AddressPane *) m_abPane, 
				 (uint32) indices[0]);
    
    if (entry == MSG_VIEWINDEXNONE) 
      return;
    

    StatusID_t *pair = (StatusID_t *) XP_CALLOC(1, sizeof(StatusID_t));
    pair->status = TO;
    pair->dir = m_dir;
    pair->id = entry;
    AB_GetType(m_dir, m_AddrBook, entry, &(pair->type));

    //email
    char a_line[AB_MAX_STRLEN];
    a_line[0] = '\0';
    AB_GetEmailAddress(m_dir, m_AddrBook, entry, a_line);
    pair->emailAddr = XP_STRDUP(a_line);

    // assemble
	pair->dplyStr = NULL;
	AB_GetExpandedName(m_dir, m_AddrBook, entry, &(pair->dplyStr));
	if (!pair->dplyStr) {
		// fullname
		a_line[0] = '\0';
		AB_GetFullName(m_dir, m_AddrBook, entry, a_line);
		pair->dplyStr = XP_STRDUP(a_line);
	}/* if */
#endif /* USE_ABCOM */
	  m_addresseeView->addEntry(pair);
  }/* if */
  
}/* XFE_AddrSearchView::toCB() */

void
XFE_AddrSearchView::ccCB(Widget /*w*/,
			   XtPointer /* callData */)
{
  /* check which is selected
   */
  int count = 0;
  const int *indices = 0;
  m_outliner->getSelection(&indices, &count);
  if (count > 0 && indices) {
#if defined(USE_ABCOM)
	  StatusID_t *pair = makePair(indices[0], CC);
#else
    /* Take the first one 
     */
    ABID entry = AB_GetEntryIDAt((AddressPane *) m_abPane, 
				 (uint32) indices[0]);
    
    if (entry == MSG_VIEWINDEXNONE) 
      return;
    
    StatusID_t *pair = (StatusID_t *) XP_CALLOC(1, sizeof(StatusID_t));
    pair->status = CC;
    pair->id = entry;
    AB_GetType(m_dir, m_AddrBook, entry, &(pair->type));

    //email
    char a_line[AB_MAX_STRLEN];
    a_line[0] = '\0';
    AB_GetEmailAddress(m_dir, m_AddrBook, entry, a_line);
    pair->emailAddr = XP_STRDUP(a_line);

    // assemble
	pair->dplyStr = NULL;
	AB_GetExpandedName(m_dir, m_AddrBook, entry, &(pair->dplyStr));
	if (!pair->dplyStr) {
		// fullname
		a_line[0] = '\0';
		AB_GetFullName(m_dir, m_AddrBook, entry, a_line);
		pair->dplyStr = XP_STRDUP(a_line);
	}/* if */
#endif /* USE_ABCOM */
    m_addresseeView->addEntry(pair);
  }/* if */
  
}/* XFE_AddrSearchView::ccCB() */

void
XFE_AddrSearchView::bccCB(Widget /*w*/,
			   XtPointer /* callData */)
{
  /* check which is selected
   */
  int count = 0;
  const int *indices = 0;
  m_outliner->getSelection(&indices, &count);
  if (count > 0 && indices) {
#if defined(USE_ABCOM)
	  StatusID_t *pair = makePair(indices[0], BCC);
#else
    /* Take the first one 
     */
    ABID entry = AB_GetEntryIDAt((AddressPane *) m_abPane, (uint32) indices[0]);
    
    if (entry == MSG_VIEWINDEXNONE) 
      return;
    
    StatusID_t *pair = (StatusID_t *) XP_CALLOC(1, sizeof(StatusID_t));
    pair->status = BCC;
    pair->id = entry;
    AB_GetType(m_dir, m_AddrBook, entry, &(pair->type));

    //email
    char a_line[AB_MAX_STRLEN];
    a_line[0] = '\0';
    AB_GetEmailAddress(m_dir, m_AddrBook, entry, a_line);
    pair->emailAddr = XP_STRDUP(a_line);

    // assemble
	pair->dplyStr = NULL;
	AB_GetExpandedName(m_dir, m_AddrBook, entry, &(pair->dplyStr));
	if (!pair->dplyStr) {
		// fullname
		a_line[0] = '\0';
		AB_GetFullName(m_dir, m_AddrBook, entry, a_line);
		pair->dplyStr = XP_STRDUP(a_line);
	}/* if */
#endif /* USE_ABCOM */
    m_addresseeView->addEntry(pair);
  }/* if */
  
}/* XFE_AddrSearchView::bccCB() */

void XFE_AddrSearchView::addToAddressCallback(Widget w, 
				 XtPointer clientData, 
				 XtPointer callData)
{
  XFE_AddrSearchView *obj = (XFE_AddrSearchView *) clientData;
  obj->addToAddressCB(w, callData);
}

void
XFE_AddrSearchView::addToAddressCB(Widget /*w*/,
			   XtPointer /* callData */)
{
	addToAddressBook();
}/* XFE_AddrSearchView::bccCB() */
