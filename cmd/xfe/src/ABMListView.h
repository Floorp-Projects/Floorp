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
   ABSearchDlg.h -- class definition for XFE_ABSearchDlg
   Created: Tao Cheng <tao@netscape.com>, 11-nov-96
 */

#ifndef _ABMLISTVIEW_H_
#define _ABMLISTVIEW_H_

#include "MNView.h"
#include "Outliner.h"
#include "Outlinable.h"

#include "addrbook.h"

class XFE_AddrBookView;

class XFE_ABMListView  : public XFE_MNView, public XFE_Outlinable 
{
public:
  XFE_ABMListView(XFE_Component *toplevel_component, 
		  Widget         parent,
		  DIR_Server    *dir, 
		  ABook         *pABook,
		  XFE_View	    *parent_view,
		  MWContext     *context,
		  MSG_Master*    master);
  ~XFE_ABMListView();


  // The Outlinable interface.
  virtual void    *ConvFromIndex(int index);
  virtual int      ConvToIndex(void *item);
  //
  virtual char	  *getColumnName(int column);
  //
  virtual char    *getColumnHeaderText(int column);
  virtual fe_icon *getColumnHeaderIcon(int column);
  virtual EOutlinerTextStyle 
                   getColumnHeaderStyle(int column);
  virtual EOutlinerTextStyle 
                   getColumnStyle(int column);
  virtual char    *getColumnText(int column);
  virtual fe_icon *getColumnIcon(int column);
  //
  virtual void     getTreeInfo(XP_Bool *expandable, 
			       XP_Bool *is_expanded, 
			       int *depth, 
			       OutlinerAncestorInfo **ancestor);
  //
  virtual void     Buttonfunc(const OutlineButtonFuncData *data);
  virtual void     Flippyfunc(const OutlineFlippyFuncData *data);
  //
  virtual void     releaseLineData();
  //
  // we implement this one for all subclasses.
  //
  virtual void         *acquireLineData(int line);
  virtual XFE_Outliner *getOutliner();

  // Get tooltipString & docString; 
  // returned string shall be freed by the callee
  // row < 0 indicates heading row; otherwise it is a content row
  // (starting from 0)
  //
  virtual char *getCellTipString(int /* row */, int /* column */);
  virtual char *getCellDocString(int /* row */, int /* column */);

  // columns for the Outliner
  enum {OUTLINER_COLUMN_TYPE = 0,
	OUTLINER_COLUMN_NAME,
	OUTLINER_COLUMN_NICKNAME,
	OUTLINER_COLUMN_EMAIL,
	OUTLINER_COLUMN_COMPANY,
	OUTLINER_COLUMN_LOCALITY,
	OUTLINER_COLUMN_LAST
  };

  MLPane* getMListPane() { return m_mListPane;}

  /* return listID
   */
  ABID setValues(ABID m_listID);
  void remove();
  int  apply(char *pNickName, char *pFullName, char *pInfo);

  // icons for the outliner
  static fe_icon m_personIcon;
  static fe_icon m_listIcon;
  static void entryTTYActivateCallback(Widget, XtPointer, XtPointer);
  static void entryTTYValChgCallback(Widget, XtPointer, XtPointer);

  //
  void tableTraverse(Widget w,
					 XEvent *event,
					 String *params,
					 Cardinal *nparam);

protected:

  XFE_Outliner *m_outliner;

  //
  MSG_Master   *m_master;
  XP_List      *m_directories;
  DIR_Server   *m_dir;
  MLPane       *m_mListPane;
  ABook        *m_AddrBook; 
  ABPane       *m_abPane;
  ABID          m_listID;
  ABID          m_entryID; /* entryID for current line */
  ABID          m_memberID;

  virtual void entryTTYValChgCB(Widget, XtPointer);
  virtual void entryTTYActivateCB(Widget, XtPointer);

  // internal drop callback for addressbook drops
  static void AddressDropCb(Widget,void*,fe_dnd_Event,fe_dnd_Source*,XEvent*);
  void addressDropCb(fe_dnd_Source*);
  void processAddressBookDrop(XFE_Outliner*,DIR_Server *abDir,ABook *abBook,
                              AddressPane*);

private:
  char          m_preStr[128];
  uint32        m_count; 

  Widget        m_textF;

  void doBackSpace(XEvent *event);
};

#endif /*  _ABMLISTVIEW_H_ */
