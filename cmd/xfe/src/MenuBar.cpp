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
   MenuBar.cpp -- implementation file for MenuBar
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
 */



#if DEBUG_toshok
#define D(x) x
#else
#define D(x)
#endif

#include "Frame.h"
#include "MenuBar.h"

#include <Xm/RowColumn.h>

XFE_MenuBar::XFE_MenuBar(XFE_Frame *parent_frame, MenuSpec *spec) 
  : XFE_Menu(parent_frame, spec, XmCreateMenuBar(parent_frame->getChromeParent(), 
						 "menuBar", NULL, 0))
{
}

XFE_MenuBar::~XFE_MenuBar()
{
  // nothing to do here..
}
