/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/* 
   NavCenterFrame.h -- class definition for the NavCenter frame class
   Created: Stephen Lamm <slamm@netscape.com>, 05-Nov-97.
 */



#ifndef _xfe_navcenterframe_h
#define _xfe_navcenterframe_h

#include "Frame.h"

class XFE_NavCenterView;

class XFE_NavCenterFrame : public XFE_Frame
{
public:
  XFE_NavCenterFrame(Widget toplevel, XFE_Frame *parent_frame,
                     Chrome *chromespec);
  virtual ~XFE_NavCenterFrame();

  static void     showBookmarks   (Widget toplevel, XFE_Frame *parent_frame);
  static void     showHistory     (Widget toplevel, XFE_Frame *parent_frame);

  static void     editToolbars    (Widget toplevel, XFE_Frame *parent_frame);

  XFE_NavCenterView * getNavCenterView() {return (XFE_NavCenterView*)m_view;}

private:
  static MenuSpec menu_bar_spec[];
  static MenuSpec file_menu_spec[];
  static MenuSpec view_menu_spec[];
};

extern "C" MWContext *fe_showNavCenter(Widget toplevel, 
                                       XFE_Frame *parent_frame,
                                       Chrome *chromespec, URL_Struct *url);

#endif /* _xfe_navcenterframe_h */
