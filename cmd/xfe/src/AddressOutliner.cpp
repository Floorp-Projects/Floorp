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
AddressOutliner.cpp -- Object wrapper around outline widget hack.
   Created: Dora Hsu <dora@netscape.com>, Sept-30-96.
*/



#include "AddressOutliner.h"

#include "xp_core.h"
#include "ComposeView.h"
#include "ComposeFolderView.h"
#include "AddressFolderView.h"
#include "PopupMenu.h"

#if defined(USE_ABCOM)
#include "Frame.h"
#endif /* USE_ABCOM */

#include "xfe.h"

#include <Xfe/Xfe.h>

#include <Xm/TextF.h>
#include "felocale.h"
extern XtAppContext fe_XtAppContext;

#ifdef DEBUG_dora
#define XDEBUG(x) x
#else
#define XDEBUG(x)
#endif

extern int MSG_TotalHeaderCount();
extern MSG_HEADER_SET MSG_HeaderValue( int pos );

extern "C"
{

XP_List* FE_GetDirServers();
ABook*   fe_GetABook(MWContext *context);

static void KeyIn(
	Widget w,
         XEvent *event,
         String *params,
         Cardinal *nparam);

static void TableTraverse(
	Widget w,
         XEvent *event,
         String *params,
         Cardinal *nparam);

static void FieldTraverse(
	Widget w,
         XEvent *event,
         String *params,
         Cardinal *nparam);
}

extern "C" char* xfe_ExpandForNameCompletion(char *pString,
                                             ABook *pAddrBook,
                                             DIR_Server *pDirServer);

#if defined(USE_ABCOM)
/* C API
 */
extern "C"  XFE_ABComplPickerDlg*
fe_showComplPickerDlg(Widget     toplevel, 
					 MWContext *context,
					 MSG_Pane  *pane,
					 MWContext *pickerContext,
					 NCPickerExitFunc func,
					 void      *callData);

#endif /* USE_ABCOM */

const char *XFE_AddressOutliner::textFocusIn = "XFE_AddressOutliner::textFocusIn";
const char *XFE_AddressOutliner::typeFocusIn = "XFE_AddressOutliner::typeFocusIn";
const char *XFE_AddressOutliner::tabPrev = "XFE_AddressOutliner::tabPrev";
const char *XFE_AddressOutliner::tabNext = "XFE_AddressOutliner::tabNext";

// Customized Traversal Actions

static XtActionsRec actions[] =
{ {"TableTraverse",     TableTraverse     },
  {"FieldTraverse",     FieldTraverse     },
  {"KeyIn",     	KeyIn     }};

// XFE_AddressOutliner Constructor -----------------------------------------

XFE_AddressOutliner::XFE_AddressOutliner(
		const char *name,
		XFE_MNListView *parent_view,
		MAddressable *addressable_view,
		Widget widget_parent,
		Boolean constantSize,
		int numcolumns,
		int *column_widths,
		char *pos_prefname) :
 	XFE_Outliner(name, parent_view, parent_view->getToplevel(), 
		widget_parent,
        	constantSize, False,
        	numcolumns, numcolumns, column_widths, pos_prefname )
{
  Widget baseWidget;
  XmFontList fontList;
  Pixel fg, bg;
  EventMask event_mask =  FocusChangeMask|ButtonPressMask | ButtonReleaseMask;

  m_displayrows = 4; /* same default number of rows as windows */
  m_parentView = parent_view;
  m_addressable = addressable_view;
  m_popup = NULL;
  m_focusRow = 0;
  m_focusCol = XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT;
  m_cursorPos = 0;
  m_inPopup = False;
  m_lastFocus = NULL; /* Obselete ?! - dh */
  m_textTimer = 0;
#if defined(USE_ABCOM)
  m_pickerPane = 0;
  XFE_Frame *f = (XFE_Frame *) parent_view->getToplevel();
  XP_ASSERT(f);
  m_pickerContext = 
	  XFE_ABComplPickerDlg::cloneCntxtNcreatePane(f->getContext(),
												  &m_pickerPane);
  m_pickerDlg = 0;

  m_rtnIsPicker = False;
#endif /* USE_ABCOM */

#if !defined(USE_ABCOM)
  // get the list of directory servers & address books
  XP_List *pDirectories=FE_GetDirServers();
  XP_ASSERT(pDirectories);
  
  DIR_GetComposeNameCompletionAddressBook(pDirectories, &m_pCompleteServer);

  m_pAddrBook = fe_GetABook(0);
#endif /* USE_ABCOM */

  // Add actions
  XtAppAddActions( fe_XtAppContext, actions,  3);

  XtVaGetValues(m_widget, XmNfontList, &fontList,  
		XmNforeground, &fg,
		XmNbackground, &bg, 0 );

  m_typeWidget = XtVaCreateWidget("addressText", 
			xmTextFieldWidgetClass, m_widget,
			XmNfontList, fontList,
			XmNforeground, fg,	
			XmNbackground, bg,	
			XmNuserData, this,
			XmNcursorPositionVisible, False,
			XmNeditable, False,
			XmNmarginHeight, 0,
                        XmNmarginWidth, 3,
                        XmNshadowThickness, 0,
                        XmNhighlightThickness, 2,
                        XmNx, 0,
                        XmNy, 0,
                        XmNwidth, 40,
                        XmNheight, 40,
			0 );
  XmDropSiteUnregister(m_typeWidget); // prevent conflict with attachment dropsite
  
  m_textWidget = XtVaCreateWidget("addressText", 
			xmTextFieldWidgetClass, m_widget,
			XmNblinkRate, 200,
			XmNcursorPositionVisible, True,
			XmNtraversalOn, True,
			XmNeditable, True,
			XmNforeground, fg,	
			XmNbackground, bg,	
			XmNfontList, fontList,
			XmNuserData, this,
			XmNmarginHeight, 0,
                        XmNmarginWidth, 3,
                        XmNshadowThickness, 0,
                        XmNhighlightThickness, 2,
                        XmNx, 0,
                        XmNy, 0,
                        XmNwidth, 40,
                        XmNheight, 40,
			0);
  XtVaSetValues(m_widget,
		XmNuserData, this,
		XmNhighlightRowMode, False,
		XmNselectionPolicy, XmSELECT_NONE,
                XmNvisibleRows, m_displayrows,
		XmNcellMarginLeft, 1,
		XmNcellMarginRight, 1,
                XmNcellRightBorderType, XmBORDER_DASH,
                XmNcellBottomBorderType, XmBORDER_DASH,
                XtVaTypedArg, XmNcellBackground, XmRString, "white", 6,
                XtVaTypedArg, XmNcellRightBorderColor, XmRString, "Gray60", 7,
                XtVaTypedArg, XmNcellBottomBorderColor, XmRString, "Gray60", 7,
                XmNcellDefaults, True,
                XmNcellAlignment, XmALIGNMENT_LEFT,
                0);
  XmDropSiteUnregister(m_textWidget); // prevent conflict with attachment dropsite

  XmProcessTraversal(m_widget, XmTRAVERSE_CURRENT);
  int total = 0;

  if (m_addressable) 
	total =  ((XFE_AddressFolderView*)m_addressable)->getTotalData();

  change(0,0 ,total+1 );

  //MapText(0);

  getToplevel()->registerInterest(XFE_Component::afterRealizeCallback,
             this,
             (XFE_FunctionNotification)afterRealizeWidget_cb); 
  // Translations will be installed in the afterRealize routine,
  // so that they will override XFE_Frame::hackTranslations.

  baseWidget = m_parentView->getParent()->getBaseWidget();

  XmProcessTraversal(m_textWidget, XmTRAVERSE_CURRENT);

  XtAddCallback(m_widget, XmNscrollCallback, scrollCallback, this);
  XtAddCallback(m_textWidget, XmNactivateCallback, textActivateCallback, this);

  XtAddCallback(m_typeWidget, XmNfocusCallback, typeFocusCallback, this);
  XtAddCallback(m_textWidget, XmNfocusCallback, textFocusCallback, this);

  XtAddCallback(m_typeWidget, XmNlosingFocusCallback, textLosingFocusCallback, this);
  XtAddCallback(m_textWidget, XmNlosingFocusCallback, textLosingFocusCallback, this);
  XtAddCallback(m_textWidget, XmNmodifyVerifyCallback, textModifyCallback, this);

  // Insert our own event handler to the head of the list
  // XFE_Component::m_widget is the grid widget
  XtInsertEventHandler(m_widget,
		       		event_mask,
                    False,
                    eventHandler, this, XtListHead);
  // Add Interests

  m_parentView->getToplevel()->registerInterest(XFE_AddressFolderView::tabNext, this,
	(XFE_FunctionNotification)tabToGrid_cb);

  m_parentView->getParent()->getToplevel()->registerInterest(XFE_ComposeView::tabBeforeSubject, this,
	(XFE_FunctionNotification)tabToGrid_cb);

  setMultiSelectAllowed(True);
  m_firstSelected = -1;
  m_lastSelected = -1;
}

// Default Constructor
XFE_AddressOutliner::XFE_AddressOutliner()
{
}

// Destructor
XFE_AddressOutliner::~XFE_AddressOutliner()
{
#if defined(USE_ABCOM)
	if (m_pickerPane) {
		MSG_SetFEData(m_pickerPane, 0);
		AB_ClosePane(m_pickerPane);
	}/* if */
	if (m_pickerDlg) {
		delete m_pickerDlg;
		m_pickerDlg = 0;
	}/* if */
#endif /* USE_ABCOM */
		
}

// Install local translations -- this needs to be after realize time,
// otherwise uparrow and downarrow will be overridden by XFE_Frame.
XFE_CALLBACK_DEFN(XFE_AddressOutliner,
				  afterRealizeWidget)(XFE_NotificationCenter *, void *, void*)
{
  XtOverrideTranslations(m_textWidget, fe_globalData.editing_translations);
  XtOverrideTranslations(m_textWidget, fe_globalData.single_line_editing_translations);
  XtOverrideTranslations(m_textWidget, fe_globalData.address_outliner_traverse_translations);

  XtOverrideTranslations(m_typeWidget, fe_globalData.address_outliner_key_translations);

  // shift focus to the first text field in the outliner if we
  // don't already have a recipient:
  Boolean has_recip = False;

  // Only "To" or "Newsgroup" counts as recipient (not Bcc, Reply-To, etc.)
  int total = getTotalLines();
  int i;
  for (i=0; i<total; ++i)
  {
      char* type = GetCellValue(i,
                         XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_TYPE);
      if (type && (!XP_STRCMP(type, "To:") || !XP_STRCMP(type, "Newsgroup:")))
      {
          has_recip = True;
          delete [] type;
          break;
      }
      if (type)
          delete [] type;
  }
  m_focusRow = total-1;

  if (!has_recip)
  {
      m_focusCol = XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT;
      PlaceText(m_focusRow, m_focusCol);
      // PlaceText() is supposed to set the focus into the current field,
      // but it doesn't always.  We need to be sure:
      XmLGridSetFocus(m_widget, m_focusRow, m_focusCol);
  }
}

// Popup callback (for To: Cc: Followup: etc)
void
XFE_AddressOutliner::popupCallback(Widget w, XtPointer clientData, XtPointer)
{
	XFE_AddressOutliner *	obj = (XFE_AddressOutliner*) clientData;
	XmString				xmstr;

	XtVaGetValues(w,XmNlabelString,&xmstr,NULL);

	if (xmstr)
	{
		String str = NULL;

		XmStringGetLtoR(xmstr,XmFONTLIST_DEFAULT_TAG,&str);

		if (str)
		{
			obj->selectedMenu(str);

			XtFree(str);
		}
		
		XmStringFree(xmstr);
	}
}

// Static Methods for traverse actions
void 
XFE_AddressOutliner::fieldTraverse(
	Widget    /*w*/,
        XEvent*   /*event*/,
        String*   /*params*/,
        Cardinal* /*nparam*/)
{
   //XmLGridGetFocus(m_widget, &row, &col, &focus);
   PlaceText(m_focusRow,m_focusCol);
   if ( XtIsManaged(m_textWidget) )
   	XmProcessTraversal(m_textWidget, XmTRAVERSE_CURRENT);
   else if (XtIsManaged(m_typeWidget) )
   	XmProcessTraversal(m_typeWidget, XmTRAVERSE_CURRENT);
}


void 
XFE_AddressOutliner::keyIn(
		Widget /*w*/,
        XEvent * /*event*/,
        String *params,
        Cardinal *nparam)
{
   XrmQuark q;
   static int keyIn_quarksValid = 0;
   static XrmQuark qTo, qReply, qNewsgroup, qFollowup, qCc, qBcc;

   int row, col;
   Widget grid;
   grid = m_widget;

   if (*nparam != 1)
                return;
    if (!keyIn_quarksValid)
    {
        qTo = XrmStringToQuark(XFE_AddressFolderView::TO);
        qReply = XrmStringToQuark(XFE_AddressFolderView::REPLY);
        qFollowup = XrmStringToQuark(XFE_AddressFolderView::FOLLOWUP);
        qNewsgroup = XrmStringToQuark(XFE_AddressFolderView::NEWSGROUP);
        qCc = XrmStringToQuark(XFE_AddressFolderView::CC);
        qBcc = XrmStringToQuark(XFE_AddressFolderView::BCC);
        keyIn_quarksValid = 1;

    }

    q = XrmStringToQuark(params[0] );

    col = m_focusCol;
    row = m_focusRow;

    if (col== XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT ) 
	     col = XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_TYPE;

	if ( col == XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_TYPE )
    {
		if ( q == qTo )
		{
	   		XDEBUG(printf("*KEY IN TO * invalidate Line caused by %d\n", row);)
	   		setTypeHeader(row, MSG_TO_HEADER_MASK);
		}
		else if ( q == qReply)
		{
	   		XDEBUG(printf("*KEY IN Reply * invalidate Line caused by %d\n", row);)
	   		setTypeHeader(row, MSG_REPLY_TO_HEADER_MASK);
		}
		else if ( q == qNewsgroup)
		{
	   		XDEBUG(printf("*KEY IN Newsgroup * invalidate Line caused by %d\n", row);)
	   		setTypeHeader(row, MSG_NEWSGROUPS_HEADER_MASK);
		}
		else if ( q == qFollowup)
		{
	   		XDEBUG(printf("*KEY IN Followup * invalidate Line caused by %d\n", row);)
	   		setTypeHeader(row, MSG_FOLLOWUP_TO_HEADER_MASK);
		}
		else if (q == qCc)
    	{
	   		XDEBUG(printf("*KEY IN CC * invalidate Line caused by %d\n", row);)
	   		setTypeHeader(row, MSG_CC_HEADER_MASK);
		}
		else if ( q == qBcc)
    	{
	   		XDEBUG(printf("*KEY IN BCC * invalidate Line caused by %d\n", row);)
	   		setTypeHeader(row, MSG_BCC_HEADER_MASK);
		}
		else
		{
	   		XDEBUG(printf("*Invalide Key In value at %d\n", row);)
			XP_ASSERT(0);
		}
	} /* COLUMN_TYPE */
}

void 
XFE_AddressOutliner::tableTraverse(
	Widget /*w*/,
        XEvent *  event,
        String *  params,
        Cardinal* nparam)

{
   XrmQuark q;
   static int quarksValid = 0;
   static XrmQuark qLEFT, qRIGHT;
   static XrmQuark qINSERT, qDELETE, qBACKSPACE;
   static XrmQuark qNEXT, qPREVIOUS;
   static XrmQuark qHOME, qEND, qUP, qDOWN;

   Widget grid;
   int row, col;
   grid = m_widget;

   if (*nparam != 1)
                return;
    if (!quarksValid)
    {
        qINSERT = XrmStringToQuark("INSERT");
        qDELETE = XrmStringToQuark("DELETE");
        qBACKSPACE = XrmStringToQuark("BACKSPACE");
        qEND = XrmStringToQuark("END");
        qHOME = XrmStringToQuark("HOME");
        qUP = XrmStringToQuark("UP");
        qDOWN = XrmStringToQuark("DOWN");
        qLEFT = XrmStringToQuark("LEFT");
        qRIGHT = XrmStringToQuark("RIGHT");
        qNEXT = XrmStringToQuark("NEXT");
        qPREVIOUS = XrmStringToQuark("PREVIOUS");
	quarksValid = 1;

    }
    q = XrmStringToQuark(params[0] );

   row = m_focusRow;
   col = m_focusCol;


	if ( q == qINSERT)
			doInsert( row,col);
	else if ( q == qDELETE )
      		doDelete(row, col, event);
	else if ( q == qBACKSPACE )
      		doBackSpace(row, col, event);
    else if ( q == qEND)
			doEnd(row, col);
	else if ( q == qHOME)
			doHome(row, col);
    else if ( q == qUP )
			doUp(row, col);
    else if ( q == qDOWN )
			doDown(row, col);
	else if ( q == qLEFT )
			doLeft(row, col);
    else if ( q == qRIGHT)
			doRight(row, col);
    else if ( q == qNEXT)
			doNext(row, col);
	else if ( q == qPREVIOUS)
			doPrevious(row, col);
	else
			XP_ASSERT(0);
}

void
XFE_AddressOutliner::selectedMenu(char *name)
{
   static Cardinal num = 1;
   char *keyInStr = NULL;

   m_inPopup= False;
   if ( name[0] == 'T')
   {
	keyInStr = XP_STRDUP(XFE_AddressFolderView::TO);
   }
   else if ( name[0] == 'C')
   {
	keyInStr = XP_STRDUP(XFE_AddressFolderView::CC);
   }
   else if ( name[0] == 'B')
   {
	keyInStr = XP_STRDUP(XFE_AddressFolderView::BCC);
   }
   else if ( name[0] == 'R' )
   {
	keyInStr = XP_STRDUP(XFE_AddressFolderView::REPLY);
   }
   else if ( name[0] == 'N' )
   {
	keyInStr = XP_STRDUP(XFE_AddressFolderView::NEWSGROUP);
   }
   else if ( name[0] == 'F' )
   {
	keyInStr = XP_STRDUP(XFE_AddressFolderView::FOLLOWUP);
   }
   else XP_ASSERT(0);

   keyIn(m_typeWidget, NULL, &keyInStr, &num);
   XP_FREE(keyInStr);

   // Make sure the popup menu is not posted
   if (XfeIsAlive(m_popup))
   {
	   XtUnmanageChild(m_popup);
   }

   XtAddCallback(m_typeWidget, 
		XmNlosingFocusCallback, textLosingFocusCallback, this);
}

// XFE_AddressOutliner ---------- Protected ---------------------

// NONE 

// XFE_AddressOutliner ---------- Private ---------------------


void
XFE_AddressOutliner::MapText(int row)
{
  int total;

  total = getTotalLines();

  if ( row >= total )
    change(row, total,total+1 );

  PlaceText(row, XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT, False);
}

void 
XFE_AddressOutliner::PlaceText(int row, int col, Boolean doUpdate)
{
#if defined(DEBUG_tao)
	printf("\nXFE_AddressOutliner::PlaceText:row=%d, col=%d, update=%d,m_focusRow=%d, m_focusCol=%d\n", 
		   row, col, doUpdate, m_focusRow, m_focusCol);
#endif
  XRectangle rect;
  Boolean removed = False;


  //XtVaGetValues(m_textWidget, XmNcursorPosition, &cursorPos, 0);

  if( XtIsManaged(m_textWidget) &&  doUpdate ) {
    (void) updateAddresses();

  XtRemoveCallback(m_textWidget, 
		XmNlosingFocusCallback, textLosingFocusCallback, this);
  removed = True;
  }
  if ( XtIsManaged(m_textWidget) ) XtUnmanageChild(m_textWidget);

  if ( XtIsManaged(m_typeWidget) ) XtUnmanageChild(m_typeWidget);

  // UPDATE FOCUS CELL CURSOR POSITION & ROW & COL 
  m_focusRow= row;
  m_focusCol= col;

  makeVisible(row);

  //XtVaSetValues(m_widget, XmNscrollRow, row, NULL);

  XmLGridRowColumnToXY( m_widget, XmCONTENT, row,  XmCONTENT, col, False, &rect);

  //fe_SetTextFieldAndCallBack(m_textWidget, NULL);

  if ( col == XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT )
  {
     char *str;
	        XtVaSetValues( m_textWidget, XmNx, rect.x,
						   XmNy, rect.y, XmNwidth, rect.width,
						   XmNheight, rect.height, 0);

     str = GetCellValue(row, col);

	 // Force an update may not be necessary
     XmUpdateDisplay(m_widget);

     fe_SetTextFieldAndCallBack(m_textWidget, str);

     if ( str ) 
		 /* allocated by new
		  */
		 delete str;

     XmUpdateDisplay(m_textWidget);
     XmTextFieldShowPosition( m_textWidget, m_cursorPos);
	 // Set insertion pos before managing child, otherwise
	 // cruft sometimes appears at beginning of line (SGI bug?):
	 XmTextFieldSetInsertionPosition( m_textWidget, m_cursorPos);
     XtManageChild(m_textWidget);
     XmProcessTraversal(m_textWidget, XmTRAVERSE_CURRENT);
  }
  else if ( col == XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_TYPE)
  {
     char *str;	
     XtVaSetValues( m_typeWidget, XmNx, rect.x,
					XmNy, rect.y, XmNwidth, rect.width,
					XmNheight, rect.height, 0);
     str = GetCellValue(row, col);

     fe_SetTextFieldAndCallBack(m_typeWidget, str);
     if ( str ) XtFree(str);

     XtManageChild(m_typeWidget);
     XmProcessTraversal(m_typeWidget, XmTRAVERSE_CURRENT);
  }
  XmLGridSetFocus(m_widget, m_focusRow, m_focusCol);
  if ( removed )
    XtAddCallback(m_textWidget, 
		XmNlosingFocusCallback, textLosingFocusCallback, this);
}

void
XFE_AddressOutliner::textFocus()
{
	m_parentView->getToplevel()->notifyInterested(XFE_AddressOutliner::textFocusIn, m_textWidget);
}

void
XFE_AddressOutliner::typeFocus()
{
	m_parentView->getToplevel()->notifyInterested(XFE_AddressOutliner::typeFocusIn, m_typeWidget);
}

void
XFE_AddressOutliner::textLosingFocus(XtPointer /* callData*/ )
{
#if 0
    int oldrow = m_focusRow;
    int oldcol = m_focusCol;
    
    XmTextPosition oldcurs = XmTextFieldGetInsertionPosition(m_textWidget);

    updateAddresses();
    invalidate();               // redraw all grid elements

    XmTextPosition lastpos = XmTextFieldGetLastPosition(m_textWidget);
    m_cursorPos = (oldcurs > lastpos ? lastpos : oldcurs);
    m_focusRow = oldrow;
    m_focusCol = oldcol;
    XmLGridSetFocus(m_widget, m_focusRow, m_focusCol);
    XmTextFieldSetInsertionPosition(m_textWidget, m_cursorPos);
#endif
    // To do: When we lose focus, we also lose the black highlight
    // around the active text widget.  This is the place to fix that.
}

void
XFE_AddressOutliner::textactivate(XtPointer /* callData */)
{
#if defined(DEBUG_tao) && defined(USE_ABCOM)
	printf("\nXFE_AddressOutliner::textactivate, m_rtnIsPicker=%d\n",
		   m_rtnIsPicker);
#endif /* DEBUG_tao */
    int oldrow = m_focusRow;
    int count = getTotalLines();

    // Don't do anything if there's nothing in the text field:
    if (XmTextFieldGetInsertionPosition(m_textWidget) <= 0)
        return;

#if defined(USE_ABCOM)
	if (m_rtnIsPicker) {
		/* pop up picker
		 */
		if (m_pickerDlg) {
			m_pickerDlg->show();
			m_pickerDlg->selectItem(1);
		}/* if */
		return;
	}/* if m_rtnIsPicker */
#endif

    //textLosingFocus(0);
    updateAddresses();

    count = getTotalLines() - count;
    int curline = m_focusRow + count;

    Boolean insertNewLine = !m_addressable->hasDataAt(curline+1);
    if ( insertNewLine )
    {
        selectLine(-1);
        m_addressable->insertNewDataAfter(curline);
    }

    MapText(m_focusRow + count + 1);
    m_focusRow = oldrow + count + 1;

	// As per the spec (don't blame me for the design!):
	// increment the Type field of the next line to the next
	// addressing mode, e.g. if this is To, make it Cc --
	// but only if there were multiple comma-separated names typed in.
    if ( insertNewLine && count > 0 )
    {
	    // Make it stop at Bcc:
	    int headerPos = 0;
		MSG_HEADER_SET mask;
		if ( headerPos == MSG_TotalHeaderCount()-1 ) 
		    mask = MSG_HeaderValue(0);
		else mask = MSG_HeaderValue(++headerPos);
		const char* header = MSG_HeaderMaskToString((MSG_HEADER_SET)mask);
		if (!XP_STRNCMP(header, "To", 2) || !XP_STRNCMP(header, "Cc", 2))
		    doDown(m_focusRow,
				   XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_TYPE);
    }
}

char*
XFE_AddressOutliner::getLastEnterredName(char* str, int* comma_pos, int* len, Boolean *moreRecipients)
{
   char *pSubstring = NULL;
   char *string = NULL;
   char *substring = NULL;
   *len = 0;
   *comma_pos = 0;

   *moreRecipients = False;

   if ( str && strlen(str) > 0 )
   {
     string = strdup(str);

     // Returns the pointer to the first occurances of ','
     substring = strrchr(string, ',');
     if (substring && strlen(substring) > 1)
     {
	pSubstring = (char*)XP_CALLOC(strlen(substring)+1, sizeof(char));
	strcpy( pSubstring, substring+1);
	*len = strlen(pSubstring) ;
        *comma_pos  = strlen(str) - *len;
	*moreRecipients = True;
     }
     else if (!substring) 
     {
	pSubstring = (char*)XP_CALLOC(strlen(string)+1, sizeof(char));
	strcpy( pSubstring, string);
	*len = strlen(pSubstring) ;
     }
     else {
       *comma_pos = strlen(string);
       *moreRecipients = True;
     }
     free(string);
   }
   else
   {
	*len = 0;
        *comma_pos = 0;
   }
   return pSubstring;
}

static void
text_timer_func(XtPointer closure, XtIntervalId *)
{
    XFE_AddressOutliner* ao = (XFE_AddressOutliner*)closure;
    if (ao)
        ao->processInput();
}

// This is the routine that looks for name completion.
void
XFE_AddressOutliner::processInput()
{
    m_textTimer = 0;        // Make sure the timer is clear

    char* textStr = fe_GetTextField(m_textWidget);

    XmTextPosition lastPos = XmTextFieldGetLastPosition(m_textWidget);
    XmTextPosition insertPos = XmTextFieldGetInsertionPosition(m_textWidget);

    // if we're not at the end of the string, then we're done;
    // no need for name completion:
    if (insertPos < lastPos || textStr == 0)
        return;

    char* curName = XP_STRRCHR(textStr, ',');
    if (curName != 0)
        ++curName;
        // also need to skip past whitespace
    else
        curName = textStr;

#if defined(USE_ABCOM)
	m_lastPos = lastPos;
	m_curName = curName;
	m_textStr = textStr;

	if (!m_pickerDlg) {
		XFE_Frame *f = (XFE_Frame *) m_parentView->getToplevel();
		XP_ASSERT(f);
		m_pickerDlg = 
			fe_showComplPickerDlg(f->getBaseWidget(),
								  f->getContext(),
								  m_pickerPane,
								  m_pickerContext,
								  &nameCPickerExitFunc,
								  this);
	}/* if */

	int error = 
		AB_NameCompletionSearch(m_pickerPane,
								(const char *) curName,
								&nameCompletionExitFunc,
#ifdef FE_IMPLEMENTS_VISIBLE_NC
								True, // for now
#endif
								(void *) this);
#else
    char* completeName = nameCompletion(curName);
    if (completeName)
    {
        char* buf = (char*)XP_CALLOC((size_t)lastPos
                                     + XP_STRLEN(completeName) + 1,
                                     sizeof (char));
        if (curName != textStr)
        {
            strncpy(buf, textStr, curName - textStr);
            XP_STRCAT(buf, completeName);
        }
        else
            XP_STRCPY(buf, completeName);

        // Probably would be better to set all these
        // with a single XtSetValues command and avoid
        // having to worry about the callback from SetString.
        fe_SetTextFieldAndCallBack(m_textWidget, buf);
        XmTextFieldSetSelection(m_textWidget, lastPos,
                                XmTextFieldGetLastPosition(m_textWidget),
                            XtLastTimestampProcessed(XtDisplay(m_textWidget)));
        XmTextFieldSetInsertionPosition(m_textWidget, lastPos);
        XP_FREE(completeName);
        XP_FREE(buf);
    }
    XtFree(textStr);
#endif /* USE_ABCOM */
}

#if defined(USE_ABCOM)
void 
XFE_AddressOutliner::nameCPickerExitFunc(void *clientData, void *callData)
{
	XFE_AddressOutliner *obj = (XFE_AddressOutliner *) callData;
	obj->nameCPickerCB(clientData);
}

void 
XFE_AddressOutliner::nameCPickerCB(void *clientData)
{
	AB_NameCompletionCookie *cookie = (AB_NameCompletionCookie *) clientData;

	/* reset the flag
	 */		
	m_rtnIsPicker = False;

	char *completeName = AB_GetNameCompletionDisplayString(cookie);
	if (completeName && XP_STRLEN(completeName)) {
		m_lastPos = XP_STRLEN(completeName);
		fillNameCompletionStr(completeName);
		textactivate(NULL);
		XP_FREE(completeName);
	}/* if */

}

void 
XFE_AddressOutliner::fillNameCompletionStr(char *completeName)
{
	char* buf = (char*)XP_CALLOC((size_t)m_lastPos
								 + XP_STRLEN(completeName) + 1,
								 sizeof (char));
	if (m_curName != m_textStr) {
		strncpy(buf, m_textStr, m_curName - m_textStr);
		XP_STRCAT(buf, completeName);
	}
	else
		XP_STRCPY(buf, completeName);

	// Probably would be better to set all these
	// with a single XtSetValues command and avoid
	// having to worry about the callback from SetString.
	fe_SetTextFieldAndCallBack(m_textWidget, buf);
	XmTextFieldSetSelection(m_textWidget, m_lastPos,
							XmTextFieldGetLastPosition(m_textWidget),
                            XtLastTimestampProcessed(XtDisplay(m_textWidget)));
	XmTextFieldSetInsertionPosition(m_textWidget, m_lastPos);
	XmUpdateDisplay(m_textWidget);
	XP_FREE(buf);
}

int 
XFE_AddressOutliner::nameCompletionExitFunc(AB_NameCompletionCookie *cookie,
											int                      numResults, 
											void                    *FEcookie)
{
	XFE_AddressOutliner *obj = (XFE_AddressOutliner *) FEcookie;
	return obj->nameCompletionCB(cookie, numResults);
}

int 
XFE_AddressOutliner::nameCompletionCB(AB_NameCompletionCookie *cookie,
									  int                      numResults)
{
#if defined(DEBUG_tao)
	printf("\nnameCompletionCB,numResults=%d\n", numResults);
#endif 
	XP_ASSERT(m_pickerPane && m_pickerContext);
	if (numResults > 1) {
		/* set the flag
		 */		
		m_rtnIsPicker = True;

		/* 1. assemble the hint
		 */
		AB_AttribID attrib = AB_attribDisplayName;
		AB_ColumnInfo *cInfo = 
			AB_GetColumnInfoForPane(m_pickerPane,  
									(AB_ColumnID) 1);
		if (cInfo) {
			attrib = cInfo->attribID;
			AB_FreeColumnInfo(cInfo);

			AB_AttributeValue *value = 0;
			int error = 
				AB_GetEntryAttributeForPane(m_pickerPane, 
											0, 
											attrib, &value);
			char a_line[1024];
			a_line[0] = '\0';
			XP_SAFE_SPRINTF(a_line, sizeof(a_line),
							"%s <multiple matches found>",
							EMPTY_STRVAL(value)?m_curName:value->u.string);
			fillNameCompletionStr(a_line);
			AB_FreeEntryAttributeValue(value);
		}/* if */		
	}/* if */
	else if (numResults == 1){
		char *completeName = AB_GetNameCompletionDisplayString(cookie);
		if (completeName && XP_STRLEN(completeName)) {
			fillNameCompletionStr(completeName);
			XP_FREE(completeName);
		}/* if */
		
	}/* else */
	return numResults;
}
#endif /* USE_ABCOM */

void
XFE_AddressOutliner::textmodify(Widget, XtPointer callData)
{
    char *pCompleteName = NULL;
    XmTextVerifyCallbackStruct *cbs;
    char *pName = NULL;
    char *pCurrString = NULL;
    int len;
    int comma_pos, length;
    char *pNewName = NULL;
    Boolean moreRecipients;
    XmTextPosition lastpos = 0;

    cbs = (XmTextVerifyCallbackStruct *)callData;

    if (m_lastSelected > -1)     // there's a multiple selection
    {
        // Do nothing unless this is backspace or delete
        if (cbs->text->ptr != NULL)
            return;

        deleteRows(m_firstSelected, m_lastSelected);
        return;
    }

    // If one line is selected, deselect once we start typing:
    else if (m_firstSelected > -1)
        selectLine(-1);

    if ( cbs->text->length == 1 )
    {
        pCurrString = fe_GetTextField(m_textWidget);
        lastpos = XmTextFieldGetLastPosition(m_textWidget);
        XmTextPosition left, right;
        if (XmTextFieldGetSelectionPosition(m_textWidget, &left, &right)
            && lastpos <= right)
            lastpos = left;

        if ( cbs->currInsert < lastpos ) 
        {
            XtFree(pCurrString);
            pCurrString = NULL;
            cbs->doit = True;
        }
        if ( pCurrString )
        {
            XmTextPosition insertPos;

            // We only want to match the part of pCurrString the user
            // actually entered, not the part matched from the addrbook:
            char* enteredString = (char*)XP_CALLOC(lastpos+1, sizeof(char));
            strncpy(enteredString, pCurrString, lastpos);
            pNewName = getLastEnterredName(enteredString, &comma_pos, &length,
                                           &moreRecipients);
            XP_FREE(enteredString);
            len = length + cbs->text->length + 1;
            pName = (char*)XP_CALLOC(len, sizeof(char));

            // If the field was empt before, it's easy:
            if ( cbs->currInsert == 0 || (moreRecipients && length == 0))
            {
                strcpy(pName, cbs->text->ptr);
                insertPos = 0;
            }
            else if ( length > 0 )
            {
                XmTextPosition left, right;
                if (!XmTextFieldGetSelectionPosition(m_textWidget,
                                                     &left, &right))
                    left = cbs->currInsert;
                // "left", not cbs->currInsert, is really the insert position.

                if (!moreRecipients)
                    strncpy(pName, pCurrString, (size_t)left);
                else
                    strncpy(pName, pNewName,
                            (unsigned int)(cbs->currInsert - comma_pos));

                strcat(pName, cbs->text->ptr);

                // Now reset the timer -- when it runs out we'll try
                // name completion, rather than doing it on every char:
                if (m_textTimer)
                    XtRemoveTimeOut(m_textTimer);
                m_textTimer = XtAppAddTimeOut(fe_XtAppContext, 650,
                                              text_timer_func, this);
                pCompleteName = 0;
               
                insertPos = left;
            }

            cbs->doit = True;
            XP_FREE (pName);
        }
        else
        {
            cbs->doit = True;
        }
        XtFree(pCurrString);
    }
}

void
XFE_AddressOutliner::textmotion(XtPointer callData)
{
   XmTextVerifyCallbackStruct *cbs;


  cbs = (XmTextVerifyCallbackStruct *)callData;

}

void
XFE_AddressOutliner::scroll(XtPointer)
{
    selectLine(-1);

    // If we keep the child managed, then the Outliner will try to
    // scroll back to keep the last focused row visible (makeVisible).
    // We can either unmanage the text widget (so nothing has focus)
    // or guess where to shift the focus upon scrolling.
    XtUnmanageChild(m_textWidget);

#if 0
    int focus_r, focus_c;
    Boolean focusIn;
    XRectangle rect;
     XtUnmanageChild(m_textWidget);
     XmLGridGetFocus(m_widget, &focus_r, &focus_c, &focusIn);
     if ( XmLGridRowIsVisible(m_widget, focus_r) )
     {
     	XmLGridRowColumnToXY( m_widget, XmCONTENT,
		focus_r,  XmCONTENT, 2, False, &rect);
     	XtVaSetValues( m_textWidget, XmNx, rect.x,
			XmNy, rect.y, XmNwidth, rect.width,
			XmNheight, rect.height, 0);
     	XtManageChild(m_textWidget);
     	XmProcessTraversal(m_textWidget, XmTRAVERSE_CURRENT);
     }
#endif
}

void
XFE_AddressOutliner::handleEvent(XEvent *event, Boolean *c)
{
  int x = event->xbutton.x;
  int y = event->xbutton.y;
  unsigned char rowtype;
  unsigned char coltype;
  int row, column;
  static int press_row=0, press_col = 0;
  // only handles button clicks for now.  See outline.c:ButtonEvent for details. 
  *c = True;
  switch (event->type)
    {
    case KeyPress:
      	XDEBUG(printf("KeyPressMask...\n");)
        selectLine(-1);
	break;
    case FocusOut:
      	XDEBUG(printf("Focus out Mask...\n");)
	break;
    case FocusIn:
      	XDEBUG(printf("Focus In Mask...\n");)
		*c = False;
		//XmLGridGetFocus(m_widget, &row, &column, &focus);
		if ( m_focusCol == XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT ) {
			XmProcessTraversal(m_textWidget, XmTRAVERSE_CURRENT);
		}
	break;
    case EnterNotify: // Enter Grid Widget and Leave the text widget
      XDEBUG(printf("Enter Notify-Detail-%d\n", ((XCrossingEvent*)event)->detail);)
      /***************************************************************/
      // This is a HACK, we rely on details to give us more info
      //  about the crossing events.

      // Here are some traces. But, details might be skipped if
      // cursor pointer moves too fast. Aack.. it's X... dh

      // Leave from Grid to Outside Grid, details = NotifyAncestor(0)
      // From Outsidget Gri enter to Grid, details = NotifyAncestor

      // Leave from Grid to Text Widget, details = NotifyInferior(2)
      // Enter to Grid from Text Widget, details = NotifyInferior
      /***************************************************************/

      // Only when one actually enter to the grid widget
      // from outside the grid widget. 
      if ( ((XCrossingEvent*)event)->detail == NotifyAncestor ) onEnter();
      break;
    case LeaveNotify:
      // Don't do anthing now
      break;
    case MotionNotify:
      XDEBUG(printf("Motion Notify..........\n");)
      break;
    case ButtonPress:
      // Always ignore btn3. Btn3 is for popups. - dp 
      *c = False;
      if (event->xbutton.button == 3) break;
      XDEBUG(printf("Button Press....\n");)

      if (XmLGridXYToRowColumn(m_widget, x, y,
                               &rowtype, &row, &coltype, &column) >= 0)
        {
	  	m_inPopup = False;
          if (rowtype == XmHEADING)
            {
              // let microline handle the resizing of rows.
              if (XmLGridPosIsResize(m_widget, x, y))
                {
  		*c = True;
                  return;
                }
            }
	  else if (  column == XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_ICON )
	{

      // If it's a shift-click, we need to do multi selection,
      // otherwise select it exclusively:
      if ((event->xbutton.state & ShiftMask) != 0 && m_firstSelected >= 0)
          extendSelection(row);
      else
          selectLine(row);

      *c = True;
      return;
	}
	  else if (column == XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_TYPE
               || column == XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_BTN)
          m_inPopup = True;
          else if ( column > 0 )
	  {
      		press_row = row;
      		press_col = column;
          }

          // We had a click somewhere other than the icon, so clear selection:
          selectLine(-1);

	  if ( m_inPopup )
	  {
		*c = False;

  		XtRemoveCallback(m_typeWidget, 
		XmNlosingFocusCallback, textLosingFocusCallback, this);
        // This call to PlaceText sets the focus into the type field,
        // which allows changing the type via keyboard but has the
        // unfortunate side effect that we can't pull down a menu
        // from this widget a second time.  But that's arguably better
        // than never allowing keyboard-based changes ...
        // ToDo: place a similar event handler on the m_typeWidget.
        PlaceText(row, XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_TYPE);
		if ( m_popup == NULL )
		{
			int i;

			m_popup = XFE_PopupMenu::CreatePopupMenu(FE_GetToplevelWidget(),
													 "AddressOutlinerPopup",
													 NULL,0);
			
			XtVaSetValues(m_popup,XmNmenuPost,"<Btn1Down>",NULL);
			
			int count = MSG_TotalHeaderCount();

			for ( i = 0; i < count; i++ )
			{
				char *maskStr = XP_STRDUP(MSG_HeaderMaskToString(MSG_HeaderValue(i)));
				
				XmString xmstr = XmStringCreate(maskStr,
												XmFONTLIST_DEFAULT_TAG);
				
				Widget item = XtVaCreateManagedWidget("PopupItem",
													  xmPushButtonWidgetClass,
													  m_popup,
													  XmNlabelString,	xmstr,
													  NULL);
			
				XmStringFree(xmstr);
				
				XtAddCallback(item,XmNactivateCallback,popupCallback,this);
				
				XP_FREE(maskStr);
			}
		}

		Position x_root, y_root;
		XRectangle rect;
		XButtonPressedEvent press_event;
                XmLGridRowColumnToXY(m_widget,
			XmCONTENT, row,
			XmCONTENT, 0,
			True,
			&rect);
		XtTranslateCoords(m_widget, 0,0, &x_root, &y_root);

		press_event.x_root = x_root+rect.x;
		press_event.y_root = y_root+rect.y + rect.height;

		// Position and post the popup menu
		XmMenuPosition(m_popup,(XButtonPressedEvent *) &press_event);
		XtManageChild(m_popup);
	  }
        }
      break;
    case ButtonRelease:
      *c = False;

      if (event->xbutton.button == 3) break;
      if (XmLGridXYToRowColumn(m_widget, x, y,
                               &rowtype, &row, &coltype, &column) >= 0)
        {
          if (rowtype == XmHEADING)
            {
              // let microline handle the resizing of rows.
              if (XmLGridPosIsResize(m_widget, x, y))
                {
  		*c = True;
                  return;
                }
            }
          else 
            {
		if (row == press_row && column == press_col ) 
		{
		  if ( column == XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_ICON )
		    *c = True;
		  else if ( column > 0 )
		  {
			  PlaceText(row, column);

			  // Translate mouse event coordinate to the text field char pos:
			  if (column == XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT)
			  {
				  Position x_grid, y_grid;	// root pos of grid origin
				  Position x_text, y_text;	// root pos of textf origin
				  XtTranslateCoords(m_widget, 0,0, &x_grid, &y_grid);
				  XtTranslateCoords(m_textWidget, 0,0, &x_text, &y_text);

				  XmTextFieldSetCursorPosition(m_textWidget,
						XmTextFieldXYToPos(m_textWidget,
										   x + x_grid - x_text,
										   y + y_grid - y_text));
				  XmProcessTraversal(m_textWidget, XmTRAVERSE_CURRENT);
			  }
		  }
		  else *c = False;
		}
            }
        }
      break;
 }
}

void
XFE_AddressOutliner::selectLine(int row)
{
    Pixel bg;

    Time time = XtLastTimestampProcessed(XtDisplay(m_textWidget));
    XmTextFieldClearSelection(m_textWidget, time);

    XmLGridDeselectAllRows(getBaseWidget(), False);

    if (row < 0)        // clear selection
    {
        XmLGridRow rowp  = XmLGridGetRow(m_widget, XmCONTENT, 0);
        XmLGridColumn colp = XmLGridGetColumn(m_widget, XmCONTENT, 0);
        XtVaGetValues(m_widget,
                      XmNrowPtr, rowp,
                      XmNcolumnPtr, colp,
                      XmNcellBackground,&bg,
                      NULL);
        XmTextFieldClearSelection(m_textWidget, time);
        XtVaSetValues(m_textWidget, XmNbackground, bg, 0 );
        m_firstSelected = m_lastSelected = -1;
        return;
    }

    // highlight everything in the text field
    PlaceText(row, XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT);
    XmTextPosition last = XmTextFieldGetLastPosition(m_textWidget);
    XmTextFieldSetSelection(m_textWidget, 0, last, time);
    XmTextFieldSetCursorPosition(m_textWidget, 0);

    XmLGridSelectRow(getBaseWidget(), row, False);

    XtVaGetValues(m_widget,
                  XmNselectBackground, &bg,
                  NULL);
    XtVaSetValues(m_textWidget, XmNbackground, bg, 0 );

    m_firstSelected = row;
    m_lastSelected = -1;
}

void
XFE_AddressOutliner::extendSelection(int row)
{
    Time time = XtLastTimestampProcessed(XtDisplay(m_textWidget));

    // XmLGridSelectRow() may overwrite the selected text,
    // so make sure nothing is selected:
    XmTextFieldClearSelection(m_textWidget, time);

    XmLGridDeselectAllRows(getBaseWidget(), False);

    if (m_lastSelected < 0)     // only one selected item previously
    {
        if (row < m_firstSelected)
        {
            m_lastSelected = m_firstSelected;
            m_firstSelected = row;
        }
        else
            m_lastSelected = row;
    }
    else if (row < m_firstSelected)
        m_firstSelected = row;
    else /* if (row > m_lastSelected) */
        m_lastSelected = row;

    // Select everything in the text field -- not because we
    // really want to, but because it will force the text field
    // to pass both backspace and delete through to textmodify.
    // What a total hack!
    // Worse, if the text field is blank, we can't select it at all! :-(
    XmTextPosition last = XmTextFieldGetLastPosition(m_textWidget);
#ifdef notdef
    // This PlaceText causes lines to be deleted from the outliner
    // if the last line is blank!
    // It's not clear why it was needed, anyway.
    PlaceText(row, XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT);
#endif
    XmTextFieldSetSelection(m_textWidget, 0, last, time);
    XmTextFieldSetCursorPosition(m_textWidget, 0);

    int i;
    for (i = m_firstSelected; i <= m_lastSelected; ++i)
        XmLGridSelectRow(getBaseWidget(), i, False);

    Pixel bg;
    XtVaGetValues(m_widget,
                  XmNselectBackground, &bg,
                  NULL);
    XtVaSetValues(m_textWidget, XmNbackground, bg, 0 );
}


/* Caller has to free this return value */
char *
XFE_AddressOutliner::GetCellValue(int row, int col)
{
   XmString xmStr = 0;
   XmLGridRow rowp;
   XmLGridColumn colp;
   char *str;
   Pixel bg;


   m_addressable->getDataAt(row, col, &str);
   rowp = XmLGridGetRow(m_widget, XmCONTENT, row);
   colp = XmLGridGetColumn(m_widget, XmCONTENT, col);

   XtVaGetValues(m_widget,
                XmNrowPtr, rowp,
                XmNcolumnPtr, colp,
                XmNcellString, &xmStr,
		XmNcellBackground,&bg,
                NULL);
   XtVaSetValues(m_textWidget, XmNbackground, bg, 0 );
   XtVaSetValues(m_typeWidget, XmNbackground, bg, 0 );
   return str;
}

//
// This routine returns malloced memory which should be freed with XtFree()
//
static char*
getUnhighlightedText(Widget textW)
{
    XmTextPosition left, right;
    char* textStr = fe_GetTextField(textW);
    if (XmTextFieldGetSelectionPosition(textW, &left,&right))
        textStr[left] = 0;
    return textStr;
}

int
XFE_AddressOutliner::updateAddresses()
{
  int totalAddresses = 1;

  int focus_r, focus_c;

  char *textStr;
  int total = getTotalLines();

  //XmLGridGetFocus(m_widget, &focus_r, &focus_c, &focusIn);

  focus_r = m_focusRow;
  focus_c = m_focusCol;

  if ( focus_c == XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT )
  {
    textStr = getUnhighlightedText(m_textWidget);

    setAddress(focus_r, (char*)textStr, True);
    XtFree(textStr);
    totalAddresses = getTotalLines();
    totalAddresses = totalAddresses - total + 1; // Include current line

    // Reset the value of the text field in case it has changed
    // from plural to singular:
    char* str = GetCellValue(m_focusRow,
                    XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT);
    if (str)
    {
        // Only update the string if it changed
        // (so we don't change the X selection unnecessesarily)
        char* curstr = fe_GetTextField(m_textWidget);
        if (!curstr || XP_STRCMP(curstr, str))
            fe_SetTextFieldAndCallBack(m_textWidget, str);
        delete [] str;
        if (curstr)
            XtFree(curstr);
    }

    XtVaGetValues(m_textWidget, XmNcursorPosition, &m_cursorPos, 0);
  }
  return totalAddresses;
}


// XFE_AddressOutliner -------- Private ------ static functions -----------

XFE_CALLBACK_DEFN(XFE_AddressOutliner, tabToGrid)(XFE_NotificationCenter*, void*, void*)
{
  if ( m_textWidget && XtIsManaged(m_textWidget))
	XmProcessTraversal(m_textWidget, XmTRAVERSE_CURRENT);
  else if ( m_typeWidget && XtIsManaged(m_typeWidget))
	XmProcessTraversal(m_typeWidget, XmTRAVERSE_CURRENT);
}


void
XFE_AddressOutliner::textFocusCallback(Widget, XtPointer clientData, XtPointer)
{
  XFE_AddressOutliner *obj = (XFE_AddressOutliner*)clientData;
 

  obj->textFocus();
}

void
XFE_AddressOutliner::typeFocusCallback(Widget, XtPointer clientData, XtPointer)
{
  XFE_AddressOutliner *obj = (XFE_AddressOutliner*)clientData;
 

  obj->typeFocus();
}

void
XFE_AddressOutliner::textLosingFocusCallback(Widget, XtPointer clientData, XtPointer callData)
{
  XFE_AddressOutliner *obj = (XFE_AddressOutliner*)clientData;
  obj->textLosingFocus(callData);
}
void
XFE_AddressOutliner::textActivateCallback(Widget, XtPointer clientData, XtPointer callData)
{
  XFE_AddressOutliner *obj = (XFE_AddressOutliner*)clientData;
  obj->textactivate(callData);
}

void
XFE_AddressOutliner::textMotionCallback(Widget, XtPointer clientData, XtPointer callData)
{
  XFE_AddressOutliner *obj = (XFE_AddressOutliner*)clientData;
  obj->textmotion(callData);
}

void
XFE_AddressOutliner::textModifyCallback(Widget w, XtPointer clientData, XtPointer callData)
{
  XFE_AddressOutliner *obj = (XFE_AddressOutliner*)clientData;
  obj->textmodify(w, callData);
}

void
XFE_AddressOutliner::scrollCallback(Widget, XtPointer clientData, XtPointer callData)
{
  XFE_AddressOutliner *obj = (XFE_AddressOutliner*)clientData;
   obj->scroll(callData);
}

void
XFE_AddressOutliner::eventHandler(Widget, XtPointer clientData, XEvent *event, Boolean *cont)
{
  XFE_AddressOutliner *obj = (XFE_AddressOutliner*)clientData;
  obj->handleEvent(event, cont);
}

char *
XFE_AddressOutliner::nameCompletion(char *pString)
{
#if defined(USE_ABCOM)
	return 0;
#else
   return xfe_ExpandForNameCompletion(pString, m_pAddrBook, m_pCompleteServer);
#endif
}

//----------------------------  Static Functions for action table
static void FieldTraverse(
	Widget w,
         XEvent *event,
         String *params,
         Cardinal *nparam)

{
  
   XtPointer client;
   XFE_AddressOutliner *obj ;

   XtVaGetValues( w, XmNuserData, &client, 0);
   obj = (XFE_AddressOutliner *) client;
   obj->fieldTraverse(w, event, params, nparam);

}

static void KeyIn(
	Widget w,
         XEvent *event,
         String *params,
         Cardinal *nparam)
{
   XtPointer client;
   XFE_AddressOutliner *obj ;

   XtVaGetValues( w, XmNuserData, &client, 0);

   obj = (XFE_AddressOutliner *) client;
   obj->keyIn(w, event, params, nparam);
}

static void TableTraverse(
	Widget w,
         XEvent *event,
         String *params,
         Cardinal *nparam)
{
   XtPointer client;
   XFE_AddressOutliner *obj ;

   XtVaGetValues( w, XmNuserData, &client, 0);

   obj = (XFE_AddressOutliner *) client;
   obj->tableTraverse(w, event, params, nparam);
}

//----------- Address Book stuff here ------------

extern "C" char * xfe_ExpandForNameCompletion(char * pString,
                                              ABook *pAddrBook,
                                              DIR_Server *pDirServer)
{
#if !defined(MOZ_NEWADDR)

    ABID entryID;
    ABID field;
    char *pName = XP_STRDUP(pString);

#if defined(DEBUG_tao)
	printf("\n  xfe_ExpandForNameCompletion");
#endif

    XP_ASSERT(pAddrBook);
    XP_ASSERT(pDirServer);
    AB_GetIDForNameCompletion(
        pAddrBook,
     	pDirServer,
        &entryID,&field,(const char*)pName);

    XP_FREE(pName);

    if (entryID != (unsigned long)-1)
    {
	if ( field == ABNickname )
	{
		/* abdefn.h:const int kMaxFullNameLength = 256;
		 */
		char szNickname[kMaxFullNameLength+1];
	    szNickname[0] = '\0';
            AB_GetNickname(
	    pDirServer,
            pAddrBook, entryID, szNickname);
                if (strlen(szNickname))
                            return strdup(szNickname);
        }
        else if ( field == ABFullName )
	{
		char *szFullname;
		szFullname = (char*)XP_CALLOC(kMaxFullNameLength+1, sizeof(char));
		szFullname[0] = '\0';
		AB_GetFullName(pDirServer, 
					   pAddrBook,
					   entryID,
					   szFullname);
		if (szFullname && strlen(szFullname))
			{
				return szFullname;
			}
		
	}
    }
#endif
        return NULL;
}

// New Communication methods with parent view

void 
XFE_AddressOutliner::setTypeHeader(int row, MSG_HEADER_SET header, Boolean redisplay)
{
   const char *type = MSG_HeaderMaskToString(header);
   m_focusRow = row;

   // Set Data
   m_addressable->setHeader(row, header);

   // Update the Type Field Display 

   if ( redisplay ) invalidateLine(row); // Should be invalidateCell
   if ( type )
   {
     fe_SetTextFieldAndCallBack(m_typeWidget, (char*)type );
     XmTextFieldSetHighlight(m_typeWidget, 0, 
					strlen(type), XmHIGHLIGHT_SELECTED); 
   }
   else XP_ASSERT(0);
}



void
XFE_AddressOutliner::setAddress(int row, 
				char *pAddressStr, Boolean redisplay)
{
   int oldCount = getTotalLines();
   int newReceipients = 1;
	
   m_focusRow = row;

   newReceipients = m_addressable->setReceipient(row, pAddressStr);
   if ( newReceipients > 1 )
   {
       change(m_focusRow, oldCount, oldCount+newReceipients-1);
   }
   else if (redisplay)
  {
    invalidate();
  }
}

void
XFE_AddressOutliner::doInsert(int row, int /* col */)
{

  (void)updateAddresses();
  m_addressable->insertNewDataAfter(row);
  m_cursorPos = 0;
  MapText(row+1);
  invalidate();
}

void
XFE_AddressOutliner::doDelete(int row, int col, XEvent *event)
{
  XmTextPosition cpos;
  XmTextPosition pos, newPos;
  char *textStr;
  Boolean deleterow = False;

  if ( col ==  XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT)
  {
	   XtVaGetValues(m_textWidget, XmNcursorPosition, &cpos, 0);
	   pos = XmTextFieldGetLastPosition(m_textWidget);

	   if (pos ==0 && cpos== 0 && row+1 == getTotalLines() ) deleterow = False;
	   else if (pos ==0 && cpos== 0) deleterow = True;
	   else if ( pos == cpos ) deleterow = False;
	   else if (pos > 0) 
	   { 
		// Hack text widget action
	    XtCallActionProc(m_textWidget, "delete-next-character", event,
							NULL, 0 );
	    newPos = XmTextFieldGetLastPosition(m_textWidget);
        textStr = fe_GetTextField(m_textWidget);

	    if (newPos == pos && newPos == 0 ) deleterow = True;
	    else{
	       setAddress(row, textStr, False);
               invalidateLine(row);
            }
	    XtFree(textStr);
	   } /* pos > 0 */
   } /* col = */

  if (deleterow
	  || col == XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_TYPE)
	  deleteRow(row, col);
}

void
XFE_AddressOutliner::doBackSpace(int row, int col, XEvent *event)
{
  XmTextPosition cpos;
  XmTextPosition pos;
  Boolean deleterow = False;

  if ( col ==  XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT)
  {
	   XtVaGetValues(m_textWidget, XmNcursorPosition, &cpos, 0);
	   pos = XmTextFieldGetLastPosition(m_textWidget);

	   if (pos == 0 && cpos == 0 )
		   deleterow = True;
	   else if (pos > 0) 
	   { 
		   // Hack text widget action
		   XtCallActionProc(m_textWidget, "delete-previous-character",
							event, NULL, 0 );
	   } /* pos > 0 */
   } /* col = */

  if (deleterow
	  || col == XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_TYPE)
  {
	  deleteRow(row, col);

	  // shift focus to end of previous row
	  if ( row > 0 ) {
		  PlaceText(row-1,
					XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT);
		  m_cursorPos = XmTextFieldGetLastPosition(m_textWidget);
		  XmTextFieldSetInsertionPosition(m_textWidget, m_cursorPos);
	  }
  }
}

void
XFE_AddressOutliner::deleteRow(int row, int col)
{
   selectLine(-1);

   if ( getTotalLines() == 1)
   {
	  m_cursorPos= 0;
	  setAddress(row, NULL, False);
	  setTypeHeader(row, MSG_TO_HEADER_MASK, False);
	  invalidateLine(row);

      PlaceText(0,XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT, False );
   }
   else
   {
	  //m_cursorPos = 0;
	  if (row >= 0 && m_addressable->removeDataAt(row) )
      {
		XtRemoveCallback(m_textWidget, XmNlosingFocusCallback,
						 textLosingFocusCallback, this);
		int total = getTotalLines();
		if ( total > row+1)
		{
		   change(row, total, total-1);
    		   PlaceText(row, col, False);
		}
		else
		{
		   change(row, total, total-1);
    		   PlaceText(row-1, col, False);
		}
		XtAddCallback(m_textWidget, XmNlosingFocusCallback,
					  textLosingFocusCallback, this);
		m_cursorPos = 0;
		XmTextFieldSetCursorPosition(m_textWidget, 0); 
      }
	   col = XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT;
	}
}

void
XFE_AddressOutliner::deleteRows(int startrow, int endrow)
{
   selectLine(-1);

   XtUnmanageChild(m_textWidget);

   if (endrow >= getTotalLines()-1)
       endrow = getTotalLines()-1;

   for (int i = endrow; i >= startrow; --i)
       m_addressable->removeDataAt(i);

   // Make sure we have at least one row:
   Boolean deletedAll = False;
   if (startrow == 0 && endrow == getTotalLines()-1)
   {
       startrow = 1;
       deletedAll = True;
   }
   change(startrow, endrow-startrow+1, getTotalLines()-endrow+startrow-1);

   // Should get the text widget managed again ...

   if (deletedAll)
   {
       PlaceText(0, XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT);
       fe_SetTextFieldAndCallBack(m_textWidget, "");
   }
}

void
XFE_AddressOutliner::doEnd(int row, int col)
{
	if (col == XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT)
	{
		m_cursorPos = XmTextFieldGetLastPosition(m_textWidget);
		XmTextFieldSetCursorPosition(m_textWidget, m_cursorPos);
	}
	else
	{
		row = getTotalLines(); 
		col = XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT;
		PlaceText(row, col);
	}
}

void
XFE_AddressOutliner::doHome(int row, int col)
{
	if (col == XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT)
	{
		XmTextFieldSetCursorPosition(m_textWidget, (XmTextPosition)0);
	}
	else
	{
		row = 0; col = 1;
		PlaceText(row, col);
	}
}

void
XFE_AddressOutliner::doUp(int row, int col)
{
	const char *header;
	int headerPos = 0;
    MSG_HEADER_SET mask;

	if ( col == XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT )
	{
	    if ( row > 0 ) row--;
	    PlaceText(row,XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT);
	}
	else
	{
 	   static Cardinal doUp_num = 1;
	   char *headerStr;

	   headerPos = m_addressable->getHeader(row);
	   
	   if ( headerPos == 0 ) 
			mask = MSG_HeaderValue(MSG_TotalHeaderCount()-1);
	   else mask = MSG_HeaderValue(--headerPos);

           header = MSG_HeaderMaskToString((MSG_HEADER_SET)mask);

	   headerStr = XP_STRDUP(header);
   	   keyIn( m_typeWidget, NULL, &headerStr, &doUp_num );
	   XP_FREE(headerStr);
	}
}

void
XFE_AddressOutliner::doDown(int row, int col)
{	
  const char *header;
  MSG_HEADER_SET mask;
  int headerPos;

  if ( col == XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT )
  {
	updateAddresses();

	if ( row+1 < getTotalLines() ) 
        {
      	  row++;
    	}
	PlaceText(row,
	    XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT, False);
  }
  else 
  {
 	   static Cardinal doDown_num = 1;
	   char *headerStr;
	   headerPos = m_addressable->getHeader(row);
	   
	   if ( headerPos == MSG_TotalHeaderCount()-1 ) 
			mask = MSG_HeaderValue(0);
	   else mask = MSG_HeaderValue(++headerPos);

           header = MSG_HeaderMaskToString((MSG_HEADER_SET)mask);

	   headerStr = XP_STRDUP(header);
       keyIn( m_typeWidget, NULL, &headerStr, &doDown_num );
	   XP_FREE(headerStr);
  }
}

void 
XFE_AddressOutliner::doLeft(int row, int col)
{
	XmTextPosition pos;

	if ( col == XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT )
    	{
 	   pos = XmTextFieldGetInsertionPosition(m_textWidget);
	   if ( pos == 0 )
	     PlaceText(row, XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_TYPE);
	   else 
	   {
	     m_cursorPos = pos-1;
	     XmTextFieldSetInsertionPosition(m_textWidget, m_cursorPos);
	   }
	}
}


void
XFE_AddressOutliner::doRight(int row, int col)
{
	XmTextPosition pos;
	XmTextPosition lastpos;

	if ( col == XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_TYPE )
	{
	   PlaceText(row, XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT);
	   XmTextFieldSetInsertionPosition(m_textWidget, 0 );
	}
	else if ( col == XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT )
	{
 	   lastpos = XmTextFieldGetLastPosition(m_textWidget);
 	   pos = XmTextFieldGetInsertionPosition(m_textWidget);
	   if ( pos < lastpos )
	   {
	      m_cursorPos = pos+1;
	      XmTextFieldSetInsertionPosition(m_textWidget, m_cursorPos);
	   }
	   else if ( row+1 < getTotalLines() )
	      PlaceText(row+1, XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_TYPE);
	   else
	      PlaceText(row, XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT);
	
	}
}

void
XFE_AddressOutliner::doNext(int row, int col)
{
  updateAddresses();

  if ( col == XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT )
  {
	  if ( row+1 == getTotalLines() )
	  {
    	// Register Action

  		XtUnmanageChild(m_textWidget);
  		XtUnmanageChild(m_typeWidget);
		m_parentView->getToplevel()->notifyInterested(XFE_AddressOutliner::tabNext, this);
		invalidate();
		return;
	  }
	  else if ( row < getTotalLines() ) row++;
	  col = XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_TYPE;
  }
  else 
	col= XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT;
  PlaceText(row, col, False);
}


void
XFE_AddressOutliner::doPrevious(int row, int col)
{
  updateAddresses();

  if ( col == XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_TYPE )
  {
	 if ( row == 0 )
	 {
		m_parentView->getToplevel()->notifyInterested(XFE_AddressOutliner::tabPrev, this);
		return;
	 }
	 else 
	    row --;
     col = XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT;
  }
  else col = XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_TYPE;
  PlaceText(row, col, False);
}

void
XFE_AddressOutliner::onLeave()
{
  XtVaGetValues(m_textWidget, XmNcursorPosition, &m_cursorPos, 0);
  XtUnmanageChild(m_textWidget);
  XtUnmanageChild(m_typeWidget);

  updateAddresses();
  invalidate();

  XmLGridSetFocus(m_widget, m_focusRow, m_focusCol);
  XmUpdateDisplay(m_widget);
  if ( m_lastFocus )
	XmProcessTraversal(m_lastFocus, XmTRAVERSE_CURRENT);
  else 
  m_parentView->getToplevel()->notifyInterested(XFE_AddressOutliner::tabNext, this);
}

void
XFE_AddressOutliner::onEnter()
{
  int row, column;
  Widget base;

  base =  m_parentView->getParent()->getParent()->getBaseWidget();
  row = m_focusRow;
  column = m_focusCol;

  if ( XtIsManaged(m_textWidget) || XtIsManaged(m_typeWidget) )
  {
	return;
  }

  if ( column == XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_RECEIPIENT ) 
  {
	// Value has been updated on Leave. No need to update it again
	PlaceText(row, column, False );
  }
  else if ( column == XFE_AddressFolderView::ADDRESS_OUTLINER_COLUMN_TYPE)
  { 
       XtManageChild(m_typeWidget);
       XmProcessTraversal(m_typeWidget, XmTRAVERSE_CURRENT);
  }
//  m_parentView->getToplevel()->notifyInterested(XFE_AddressFolderView::tabNext, this);
}

void
XFE_AddressOutliner::fillText(char *text)
{
	if (m_textWidget && text)
		fe_SetTextFieldAndCallBack(m_textWidget, text);
}



