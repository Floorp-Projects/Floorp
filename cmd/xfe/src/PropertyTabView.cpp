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
   PropertyTabView.cpp -- class definition for PropertyTabView
   Created: Tao Cheng <tao@netscape.com>, 12-nov-96
 */



#include "PropertyTabView.h"
#include <XmL/Folder.h>
#include "xpgetstr.h"


XFE_PropertyTabView::XFE_PropertyTabView(XFE_Component *top,
			       XFE_View *view, /* the parent view */
			       int       tab_string_id): XFE_View(top, view)
{
  m_labelWidth = 0;

  /* Create tab
   */
  XmString xmstr;
  xmstr = XmStringCreate(XP_GetString(tab_string_id ), 
			 XmFONTLIST_DEFAULT_TAG);
  Widget form = XmLFolderAddTabForm(view->getBaseWidget(), xmstr);
  XmStringFree(xmstr);

  setBaseWidget(form);
}

XFE_PropertyTabView::~XFE_PropertyTabView()
{
}
