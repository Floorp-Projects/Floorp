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
   ABAddresseeView.h -- class definition for XFE_ABAddressingDlg
   Created: Tao Cheng <tao@netscape.com>, 20-nov-96
 */

#ifndef _ABADDRESSEEVIEW_H_
#define _ABADDRESSEEVIEW_H_

#include "View.h"
#include "Outliner.h"
#include "Outlinable.h"

#include "addrbk.h"

class XFE_AddresseeView: public XFE_View, public XFE_Outlinable
{

public:

  XFE_AddresseeView(XFE_Component *toplevel_component, 
		    Widget         parent,
		    XFE_View      *parent_view, 
		    MWContext     *context);
  virtual ~XFE_AddresseeView();
  // callbacks
  static void removeCallback(Widget, XtPointer, XtPointer);
  static void selectCallback(Widget, XtPointer, XtPointer);

 // The Outlinable interface.
  virtual void    *ConvFromIndex(int index);
  virtual int      ConvToIndex(void *item);
  //
  virtual char    *getColumnName(int column);
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
  virtual XFE_Outliner *getOutliner() { return m_outliner;}

  // Get tooltipString & docString; 
  // returned string shall be freed by the callee
  // row < 0 indicates heading row; otherwise it is a content row
  // (starting from 0)
  //
  virtual char *getCellTipString(int /* row */, int /* column */);
  virtual char *getCellDocString(int /* row */, int /* column */);

  // columns for the Outliner
  enum {OUTLINER_COLUMN_STATUS = 0,
	OUTLINER_COLUMN_NAME,
	OUTLINER_COLUMN_LAST
  };

  //
  void addEntry(StatusID_t* pair);
  StatusID_t** getPairs(int& count) { 
    count = m_numPairs; return m_pairs; }

  int getNEntries() { return m_numPairs;}

  void setOKBtn(Widget w);

  // icons for the outliner
  static fe_icon m_personIcon;
  static fe_icon m_listIcon;

protected:

  XFE_CALLBACK_DECL(updateCommands)
  Widget        m_okBtn;

  // callbacks
  virtual void removeCB(Widget w, XtPointer callData);
  virtual void selectCB(Widget w, XtPointer callData);

  //
  XFE_Outliner *m_outliner;

private:
  //
  Widget        m_removeBtn;

  StatusID_t   *m_pair;  
  StatusID_t  **m_pairs;
  int           m_numPairs;
};

#endif /* _ABADDRESSEEVIEW_H_ */
