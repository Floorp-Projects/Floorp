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
   Outlinable.h -- interface definition for views which can
    use the Outliner class to display their data.
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
 */


#ifndef _xfe_outlinable_h
#define _xfe_outlinable_h

#include "Outliner.h"
#include "mozilla.h" /* for MWContext! :( */
#include "icons.h"

typedef struct OutlineFlippyFuncData {
	int row;
	XP_Bool do_selection;
} OutlineFlippyFuncData;

typedef struct OutlinerAncestorInfo {
  XP_Bool has_next;
  XP_Bool has_prev;
} OutlinerAncestorInfo;

typedef struct OutlineButtonFuncData {
  int row; // where the click happened
  int column;

  /* more specific information (with the origin being the x,y of
     the row/column. */
  int x;
  int y;

  XP_Bool ctrl;  // modifiers
  XP_Bool shift;
  XP_Bool alt;

  int clicks;  // the number of clicks
  int button;  // which button: Button1, Button2,...,Button5
} OutlineButtonFuncData;

/*interface*/ class XFE_Outlinable
{
public:
  // Converts between an index and some non positional data type.
  // Used to maintain selection when the list is reordered.
  virtual void *ConvFromIndex(int index) = 0;
  virtual int ConvToIndex(void *item) = 0; // should return -1 if item does not map to an index

  // Returns the name of a column -- used for saving the column sizes,position,visibilities
  // shall return non-zero length string
  virtual char *getColumnName(int column) = 0;

  // Returns the text and/or icon to display at the top of the column.
  virtual char *getColumnHeaderText(int column) = 0;
  virtual fe_icon *getColumnHeaderIcon(int column) = 0;
  virtual EOutlinerTextStyle getColumnHeaderStyle(int column) = 0;

  // Tells the Outlinable object that it should get the data
  // for a line.  This line will be the focus of all requests
  // by the Outliner.  The return value is not interrogated, and
  // is only checked for NULL-ness.  return NULL if there isn't
  // any information for this line, the line is invalid, etc, etc.
  virtual void *acquireLineData(int line) = 0;

  //
  // The following 4 requests deal with the currently acquired line.
  //
  virtual void getTreeInfo(XP_Bool *expandable, XP_Bool *is_expanded, int *depth, 
			   OutlinerAncestorInfo **ancestor) = 0;
  virtual EOutlinerTextStyle getColumnStyle(int column) = 0;
  // subclass need to allocate memory for the return string 
  virtual char *getColumnText(int column) = 0;
  virtual fe_icon *getColumnIcon(int column) = 0;

  // Tells the Outlinable object that the line data is no
  // longer needed, and it can free any storage associated with
  // it.
  virtual void releaseLineData() = 0;

  virtual void Buttonfunc(const OutlineButtonFuncData *data) = 0;
  virtual void Flippyfunc(const OutlineFlippyFuncData *data) = 0;

  // return the outliner associated with this outlinable.
  virtual XFE_Outliner *getOutliner() = 0;

  // Get tooltipString & docString; 
  // returned string shall be freed by the callee
  // row < 0 indicates heading row; otherwise it is a content row
  // (starting from 0)
  //
  virtual char *getCellTipString(int row, int column) = 0;
  virtual char *getCellDocString(int row, int column) = 0;

};

#endif /* _xfe_outlinable_h */
