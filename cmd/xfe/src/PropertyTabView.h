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
   XmlTabView.h -- class definition for XFE_XmlTabView
   Created: Tao Cheng <tao@netscape.com>, 12-nov-96
 */



#ifndef _xfe_xmltabview_h
#define _xfe_xmltabview_h

#include "View.h"

/* This is a general wrapper of property tab form (goes in a property sheet) */

class XFE_PropertyTabView: public XFE_View {
public:
  XFE_PropertyTabView(XFE_Component *top, /* the parent folderDialog */
		 XFE_View *view, /* the parent view */
                 int tab_string_id);

  virtual ~XFE_PropertyTabView();

  virtual void setDlgValues() {};
  virtual void getDlgValues() {};
  virtual void apply(){};

protected:
  Dimension m_labelWidth;

private:
  /* m_widget is the tab form
   */
}; /* XFE_PropertyTabView */

/* Offsets between widgets when hardwired is needed
 */
const int labelWidth = 125;
const int labelHeight = 30;
const int textFWidth = 175;
const int textFHeight = 30;

const int separatorHeight = 10;

const int majorVSpac = 6;
const int minorVSpac = 3;
const int majorHSpac = 6;
const int minorHSpac = 3;

#endif /* _xfe_xmltabview_h */
