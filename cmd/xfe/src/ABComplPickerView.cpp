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

/* -*- Mode: C++; tab-width: 4 -*-
   Created: Tao Cheng<tao@netscape.com>, 16-mar-1998
 */


#include "Frame.h"
#include "ViewGlue.h"
#include "ABComplPickerView.h"

#define COMPPCIKER_OUTLINER_GEOMETRY_PREF "comppicker.outliner_geometry"

const char *XFE_ABComplPickerView::confirmed = 
  "XFE_ABComplPickerView::confirmed";

static char a_line[AB_MAX_STRLEN];

// icons
fe_icon XFE_ABComplPickerView::m_pabIcon = { 0 };
fe_icon XFE_ABComplPickerView::m_ldapDirIcon = { 0 };
fe_icon XFE_ABComplPickerView::m_mListIcon = { 0 };

XFE_ABComplPickerView::XFE_ABComplPickerView(XFE_Component *toplevel_component, 
											 Widget         parent,
											 XFE_View      *parent_view, 
											 MWContext     *context, 
											 MSG_Pane      *p):
	XFE_MNListView(toplevel_component, parent_view, context, p),
	m_numAttribs(0),
	m_curIndex(MSG_VIEWINDEXNONE)
{
	XP_ASSERT(p);
	setPane(p);

	m_searchingDir = False;
	m_frmParent = True;

	/* For outliner
	 */
	int num_columns = AB_GetNumColumnsForPane(p);
	AB_AttribID *attribIDs = 
		(AB_AttribID *) XP_CALLOC(num_columns, 
								  sizeof(AB_AttribID));
	m_numAttribs = num_columns;
	int error = AB_GetColumnAttribIDsForPane(p,
											 attribIDs,
											 &m_numAttribs);
#if defined(DEBUG_tao_)
	AB_ColumnInfo *cInfo = 0;
	for (AB_ColumnID i=AB_ColumnID0; i < num_columns; i++) {
		cInfo = AB_GetColumnInfoForPane(p, i);
		if (cInfo) {
			printf("\nID=%d, attribID=%d, displayString=%s, sortable=%d\n",
				   i, 
				   cInfo->attribID, cInfo->displayString, cInfo->sortable);
			AB_FreeColumnInfo(cInfo);
		}/* if */
	}/* for i */
#endif

	XP_ASSERT(num_columns == m_numAttribs);
#if defined(DEBUG_tao)
	printf("\n--XFE_ABComplPickerView: num_columns=%d, m_numAttribs=%d\n", 
		   num_columns, m_numAttribs);
#endif
	static int column_widths[] = {3, 25, 25, 25, 8, 10, 12};
#if 0
	if (num_columns > 4)
		num_columns = 4;
#endif
	m_outliner = new XFE_Outliner("compPickerList",
								  this,
								  toplevel_component,
								  parent,
								  False, // constantSize
								  True,  // hasHeadings
								  num_columns,
								  4,// num_visible 
								  column_widths,
								  COMPPCIKER_OUTLINER_GEOMETRY_PREF);

	m_outliner->setMultiSelectAllowed(True);
	m_outliner->setHideColumnsAllowed(True);

	/* BEGIN_3P: XmLGrid
	 */
	XtVaSetValues(m_outliner->getBaseWidget(),
				  XmNselectionPolicy, XmSELECT_MULTIPLE_ROW,
				  XtVaTypedArg, XmNblankBackground, XmRString, "white", 6,
				  XmNvisibleRows, 8,
				  NULL);
	XtVaSetValues(m_outliner->getBaseWidget(),
				  XmNcellDefaults, True,
				  XtVaTypedArg, XmNcellBackground, XmRString, "white", 6,
				  NULL);
	/* END_3P: XmLGrid
	 */
	
	setBaseWidget(m_outliner->getBaseWidget());

	/* reflect the right count
	 */
	uint32 count = 0;
	count = MSG_GetNumLines(m_pane);
	m_outliner->change(0, count, count);
	if (count > 1)
		m_outliner->selectItemExclusive(1);

	/* 
	 * register interest in getting allconnectionsComplete
	 */
	MWContext *top = XP_GetNonGridContext (context);
	XFE_Frame *f = ViewGlue_getFrame(top);
	if (f) {
#if !defined(GLUE_COMPO_CONTEXT)
		f->registerInterest(XFE_Frame::allConnectionsCompleteCallback,
							this,
							(XFE_FunctionNotification)allConnectionsComplete_cb);

#endif /* GLUE_COMPO_CONTEXT */
		m_frmParent = (f->getType() == FRAME_MAILNEWS_COMPOSE)?True:False;
	}/* if */

#if defined(GLUE_COMPO_CONTEXT)
  /* use view
   */
  ViewGlue_addMappingForCompo(this, (void *) context);
  CONTEXT_WIDGET(context) = getBaseWidget();
#endif


	/* initialize the icons if they haven't already been
	 */
	Pixel bg_pixel;
	XtVaGetValues(m_outliner->getBaseWidget(), XmNbackground, &bg_pixel, 0);

	if (!m_pabIcon.pixmap)
		fe_NewMakeIcon(getToplevel()->getBaseWidget(),
					   /* umm. fix me
						*/
					   BlackPixelOfScreen(XtScreen(m_outliner->getBaseWidget())),
					   bg_pixel,
					   &m_pabIcon,
					   NULL, 
					   MNC_AddressSmall.width, 
					   MNC_AddressSmall.height,
					   MNC_AddressSmall.mono_bits, 
					   MNC_AddressSmall.color_bits, 
					   MNC_AddressSmall.mask_bits, 
					   FALSE);
  

	if (!m_ldapDirIcon.pixmap)
		fe_NewMakeIcon(getToplevel()->getBaseWidget(),
					   /* umm. fix me
						*/
					   BlackPixelOfScreen(XtScreen(m_outliner->getBaseWidget())),
					   bg_pixel,
					   &m_ldapDirIcon,
					   NULL, 
					   MN_FolderServer.width, 
					   MN_FolderServer.height,
					   MN_FolderServer.mono_bits, 
					   MN_FolderServer.color_bits, 
					   MN_FolderServer.mask_bits, 
					   FALSE);
  

	if (!m_mListIcon.pixmap)
		fe_NewMakeIcon(getToplevel()->getBaseWidget(),
					   /* umm. fix me
						*/
					   BlackPixelOfScreen(XtScreen(m_outliner->getBaseWidget())),
					   bg_pixel,
					   &m_mListIcon,
					   NULL, 
					   MN_People.width, 
					   MN_People.height,
					   MN_People.mono_bits, 
					   MN_People.color_bits, 
					   MN_People.mask_bits, 
					   FALSE);
  

}

XFE_ABComplPickerView::~XFE_ABComplPickerView()
{
#if !defined(GLUE_COMPO_CONTEXT)
	/* 
	 * un register interest in getting allconnectionsComplete
	 */
	MWContext *top = XP_GetNonGridContext (m_contextData);
	XFE_Frame *f = ViewGlue_getFrame(top);
	if (f)
		f->unregisterInterest(XFE_Frame::allConnectionsCompleteCallback,
							  this,
							  (XFE_FunctionNotification)allConnectionsComplete_cb);
#endif /* GLUE_COMPO_CONTEXT */

}

XFE_CALLBACK_DEFN(XFE_ABComplPickerView, allConnectionsComplete)(XFE_NotificationCenter */*obj*/, 
																 void */*clientData*/, 
																 void *callData)
{
	MWContext *c = (MWContext *) callData;
	if (c == m_contextData) {
		int error = AB_FinishSearchAB2(m_pane);
	}/* if */
}

void 
XFE_ABComplPickerView::allConnectionsComplete(MWContext  *context)
{
	if (context == m_contextData) {
		int error = AB_FinishSearchAB2(m_pane);
	}/* if */
}

void 
XFE_ABComplPickerView::listChangeFinished(XP_Bool          /* asynchronous */,
										  MSG_NOTIFY_CODE  notify, 
										  MSG_ViewIndex    where,
										  int32            num)
{
	switch (notify) {
	case MSG_NotifyLDAPTotalContentChanged:
		{
#if defined(DEBUG_tao)
			printf("\nXFE_ABComplPickerView::MSG_NotifyLDAPTotalContentChanged, where=%d, num=%d", where, num);
#endif
			m_outliner->change(0, num, num);
			m_outliner->scroll2Item((int) where);
		}
		break;

	case MSG_NotifyInsertOrDelete:
#if defined(DEBUG_tao)
			printf("\nXFE_ABComplPickerView::MSG_NotifyInsertOrDelete, where=%d, num=%d", where, num);
#endif
		if (m_searchingDir)	{
			int error = AB_LDAPSearchResultsAB2(m_pane,
												where, num);
		}
		break;
	}/* switch notify */
}

void 
XFE_ABComplPickerView::paneChanged(XP_Bool asynchronous,
								  MSG_PANE_CHANGED_NOTIFY_CODE notify_code,
								  int32 value)
{
#if defined(DEBUG_tao)
	printf("\nXFE_ABComplPickerView::paneChanged, asynchronous=%d, notify_code=%d, value=0x%x", asynchronous, notify_code, value);
#endif
	switch (notify_code) {
	case MSG_PaneChanged: 
		break;

	case MSG_PaneClose: 
		break;

	case MSG_PaneNotifyStartSearching:
		m_searchingDir = True;
		//notifyInterested(XFE_Component::progressBarCylonStart);
		break;

	case MSG_PaneNotifyStopSearching:
		//notifyInterested(XFE_Component::progressBarCylonStop);
		m_searchingDir = False;
		break;

	}/* notify_code */
}

// We're nice to our subclasses :)
char *
XFE_ABComplPickerView::getCellTipString(int row, int column)
{
	if (column == 0 ) {
		AB_ContainerAttribValue *value = 0;
		int error = 
			AB_GetContainerAttributeForPane(m_pane,
											(MSG_ViewIndex) row,
											attribName, 
											&value);
		
		XP_SAFE_SPRINTF(a_line, sizeof(a_line),
						"%s",
						EMPTY_STRVAL(value)?"":value->u.string);
#if defined(DEBUG_tao)
		printf("\n##XFE_ABComplPickerView::getCellTipString:%s\n", 
			   a_line);
#endif
		if (m_frmParent)
			return value?a_line:0;
		else {
			notifyInterested(XFE_View::statusNeedsUpdating,
							 value?a_line:""); 
			
			return 0;
		}/* else */
		AB_FreeContainerAttribValue(value);
	}/* if */
	else
		return 0;
}

Boolean 
XFE_ABComplPickerView::handlesCommand(CommandType cmd, void *calldata,
									  XFE_CommandInfo* i)
{
	return False;
}

Boolean 
XFE_ABComplPickerView::isCommandEnabled(CommandType cmd, void *calldata,
										XFE_CommandInfo* i)
{
}

Boolean 
XFE_ABComplPickerView::isCommandSelected(CommandType cmd, void *calldata,
										 XFE_CommandInfo* i)
{
}

void 
XFE_ABComplPickerView::doCommand(CommandType cmd, void *calldata,
								 XFE_CommandInfo* i)
{
}

char *
XFE_ABComplPickerView::commandToString(CommandType cmd, void *calldata,
									   XFE_CommandInfo* i)
{
}
  
// The Outlinable interface.
char *
XFE_ABComplPickerView::getColumnName(int column)
{
	return getColumnHeaderText(column);
}

char *
XFE_ABComplPickerView::getColumnHeaderText(int column)
{
	a_line[0] = '\0';
	if (column >= 0 &&
		column < m_numAttribs) {
		AB_ColumnInfo *cInfo = 
			AB_GetColumnInfoForPane(m_pane,
									(AB_ColumnID) column);
		if (cInfo) {
			XP_SAFE_SPRINTF(a_line, sizeof(a_line),
							"%s",
							cInfo->displayString?cInfo->displayString:"");
			AB_FreeColumnInfo(cInfo);
		}/* if */
	}/* if */
	return a_line;
}

void *
XFE_ABComplPickerView::acquireLineData(int line)
{
#if 1
	m_curIndex = (MSG_ViewIndex) line;
	return (void *) &m_curIndex; // a non-zero val
#else
	int error = AB_GetABIDForIndex(m_pane,
								   (MSG_ViewIndex) line,
								   &m_entryID);

	if (m_entryID == MSG_VIEWINDEXNONE)
		return 0;
	else
		return (void *) m_entryID;
#endif
}

char *
XFE_ABComplPickerView::getColumnText(int column)
{
	a_line[0] = '\0';
	AB_AttributeValue *value = 0;
	AB_AttribID attrib = AB_attribEntryType;
	if (column > 0 &&
		column < m_numAttribs) {
		AB_ColumnInfo *cInfo = 
			AB_GetColumnInfoForPane(m_pane,  
									(AB_ColumnID) column);
		if (cInfo) {
			attrib = cInfo->attribID;
			AB_FreeColumnInfo(cInfo);

			if (attrib != AB_attribEntryType) {
				int error = 
					AB_GetEntryAttributeForPane(m_pane, 
												m_curIndex, 
												attrib, &value);

				XP_SAFE_SPRINTF(a_line, sizeof(a_line),
								"%s",
								EMPTY_STRVAL(value)?"":value->u.string);

				AB_FreeEntryAttributeValue(value);
			}/* if */

		}/* if */
	}/* if */
	else if (column == 0 &&
			 !m_frmParent) {
		AB_ContainerAttribValue *value = 0;
		int error = 
			AB_GetContainerAttributeForPane(m_pane,
											(MSG_ViewIndex) m_curIndex,
											attribName, 
											&value);
		
		XP_SAFE_SPRINTF(a_line, sizeof(a_line),
						"%s",
						EMPTY_STRVAL(value)?"":value->u.string);
		AB_FreeContainerAttribValue(value);
	}/* else if */
	return a_line;
}

fe_icon *
XFE_ABComplPickerView::getColumnIcon(int column)
{
	fe_icon *myIcon = 0;
	if (column == 0) {
		AB_ContainerType type = AB_GetEntryContainerType(m_pane,
														 m_curIndex);
		switch (type) {
		case AB_LDAPContainer:
			myIcon = &m_ldapDirIcon;
			break;
			
		case AB_PABContainer:
			myIcon = &m_pabIcon;
			break;
			
		case AB_MListContainer:
			myIcon = &m_mListIcon;
			break;
		}/* switch */
	}/* if */
	return myIcon;
}
	
void XFE_ABComplPickerView::doubleClickBody(const OutlineButtonFuncData *data)
{
	// notifyInterest()
	notifyInterested(XFE_ABComplPickerView::confirmed);
}


void 
XFE_ABComplPickerView::Buttonfunc(const OutlineButtonFuncData *data)
{
	int row = data->row, 
		clicks = data->clicks;
	
	if (row < 0) {
		// clickHeader(data);
		return;
	} 
	else {
		/* content row 
		 */
		if (clicks == 2) {
			m_outliner->selectItemExclusive(data->row);
			doubleClickBody(data);
		}/* clicks == 2 */
		else if (clicks == 1) {
			/* assume single only for now
			 */
			m_outliner->selectItemExclusive(data->row);
			getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
		}/* clicks == 1 */
	}/* else */
}

void 
XFE_ABComplPickerView::releaseLineData()
{
}


AB_NameCompletionCookie *
XFE_ABComplPickerView::getSelection()
{
	int count = 0;
	const int *indices = 0;
	m_outliner->getSelection(&indices, &count);

	if (count && indices)
		return AB_GetNameCompletionCookieForIndex(m_pane,
												  indices[0]);
	else
		return AB_GetNameCompletionCookieForIndex(m_pane,
												  1);
	
}
