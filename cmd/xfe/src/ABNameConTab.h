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
   ABNameConTabView.h -- class definition for ABNameConTabView
   Created: Tao Cheng <tao@netscape.com>, 12-nov-96
 */

#ifndef _xfe_abnamecontabview_h
#define _xfe_abnamecontabview_h

#include "PropertyTabView.h"

class XFE_ABNameConTabView: public XFE_PropertyTabView {
public:
  XFE_ABNameConTabView(XFE_Component *top,
		       XFE_View      *view /* the parent view */);
  virtual ~XFE_ABNameConTabView();


  virtual void setDlgValues();
#if 1
  enum {AB_ADDRESS = 0, 
		AB_CITY, 
		AB_STATE, 
		AB_ZIP, 
		AB_COUNTRY,
 
		AB_WORKPHONE, 
		AB_HOMEPHONE, 
		AB_FAXPHONE, 
		AB_PAGER, 
		AB_CELLULAR,

		AB_LAST
  } GEN_TEXTF;
#else
  enum {AB_ADDRESS = 0, 
		AB_CITY, 
		AB_STATE, 
		AB_ZIP, 
		AB_COUNTRY,
 
		AB_WORKPHONE, 
		AB_FAXPHONE, 
		AB_HOMEPHONE, 

		AB_LAST
  } GEN_TEXTF;
#endif

protected:
  virtual void apply();
  virtual void getDlgValues();

private:
  /* m_widget is the tab form
   */
  Widget m_textFs[AB_LAST];
  Widget m_labels[AB_LAST];
  Widget m_addr2TextF;	
}; /* XFE_ABNameConTabView */

#endif /* _xfe_abnamecontabview_h */







