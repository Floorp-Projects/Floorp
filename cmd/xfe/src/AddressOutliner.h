/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   AddressOutliner.h -- class definition for the AddressOutliner object
   Created: Dora Hsu<dora@netscape.com>, Sept-30-96.
 */



#ifndef _xfe_addressoutliner_h
#define _xfe_addressoutliner_h

#include "NotificationCenter.h"
#include "Outliner.h"
#include "MNListView.h"
#include "Addressable.h"

class XFE_AddressOutliner : public XFE_Outliner
{
public:
  XFE_AddressOutliner(
		const char *name,
		XFE_MNListView *parent_view,
		MAddressable *addressable_view,
	       	Widget widget_parent,
	       	Boolean constantSize,
	       	int num_columns,
	       	int *column_widths,
	       	char *pos_prefname);

  XFE_AddressOutliner();
  virtual ~XFE_AddressOutliner();

  XFE_CALLBACK_DECL(afterRealizeWidget)

  void keyIn(
        Widget w,
         XEvent *event,
         String *params,
         Cardinal *nparam);

  void fieldTraverse(
        Widget w,
         XEvent *event,
         String *params,
         Cardinal *nparam);

  void tableTraverse(
        Widget w,
         XEvent *event,
         String *params,
         Cardinal *nparam);

  void selectedMenu(char*);

  void onEnter();

  /* pretend the user types in text
   */
  void fillText(char *text);
  void PlaceText(int row, int col, Boolean doUpdate = True);

  /* callback routine for the timer on user input
   */
  void processInput();

    /* Moved updateAddresses() to public because it's the only way
     * to ensure that the last-typed-in address gets recorded
     */
  int  updateAddresses();

  static const char *tabPrev;
  static const char *tabNext;
  static const char *textFocusIn;
  static const char *typeFocusIn;

protected:
  void selectLine(int line);         // call with -1 to clear all selections
  void extendSelection(int line);

private:

  // Data
  int m_displayrows;
  int m_numrows;
  int m_rowindex;
  Widget m_textWidget;
  Widget m_typeWidget;
  Widget m_lastFocus;
  Widget m_popup;

  XFE_MNListView *m_parentView;
  MAddressable *m_addressable;
  int		m_focusRow;
  int		m_focusCol;
  XmTextPosition m_cursorPos;
  Boolean	m_inPopup;
  int       m_firstSelected;
  int       m_lastSelected;
  XtIntervalId m_textTimer;

  // Methods
  void handleEvent(XEvent *event, Boolean *c); 
  void textLosingFocus(XtPointer);
  void textFocus();
  void typeFocus();
  void textactivate(XtPointer);
  void textmodify(Widget, XtPointer);
  void textmotion(XtPointer);
  void MapText(int row);
  void scroll(XtPointer);

  char *GetCellValue(int row, int col);
  XFE_CALLBACK_DECL(tabToGrid)
  char *nameCompletion(char *pString);
  char *getLastEnterredName(char* str, int* pos, int* len, Boolean* moreRecipients);


  // Static Methods
  static void eventHandler(Widget, XtPointer, XEvent *, Boolean *);
  static void typeFocusCallback(Widget, XtPointer clientData, XtPointer);
  static void textFocusCallback(Widget, XtPointer clientData, XtPointer);
  static void textLosingFocusCallback(Widget, XtPointer clientData, XtPointer
callData);
  static void textActivateCallback(Widget, XtPointer clientData, XtPointer
callData);
  static void scrollCallback(Widget, XtPointer clientData, XtPointer);
  static void textModifyCallback(Widget, XtPointer clientData, XtPointer
callData);
  static void textMotionCallback(Widget, XtPointer clientData, XtPointer
callData);

  static void popupCallback(Widget, XtPointer clientData, XtPointer);

  // New Communication methods with Parent View
  void setTypeHeader(int row, MSG_HEADER_SET header, Boolean redisplay = True);
  void setAddress(int row, char* pAddressesStr, Boolean redisplay = True);
  void deleteRow(int row, int col);
  void deleteRows(int startrow, int endrow);

  void doInsert(int row, int col);
  void doDelete(int row, int col, XEvent *event);
  void doBackSpace(int row, int col, XEvent *event);
  void doEnd(int row, int col);
  void doHome(int row, int col);
  void doUp(int row, int col);
  void doDown(int row, int col);
  void doLeft(int row, int col);
  void doRight(int row, int col);
  void doNext(int row, int col);
  void doPrevious(int row, int col);
  void onLeave();
};

#endif
