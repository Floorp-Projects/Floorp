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
   EditorBar.cpp
   Created: Richard Hess <rhess@netscape.com>, 10-Dec-96.
   */



#include "mozilla.h"
#include "xfe.h"
#include "EditorBar.h"

#include <Xm/Form.h>
#include <Xm/LabelG.h>
#include <Xm/TextF.h>
#include "DtWidgets/ComboBox.h"

extern "C" {
  Widget fe_EditorCreatePropertiesToolbar(MWContext*, Widget, char*);
  Widget fe_EditorCreateComposeToolbar(MWContext*, Widget, char*);
  XtPointer fe_tooltip_mappee(Widget widget, XtPointer data);
}

XFE_EditorBar::XFE_EditorBar(XFE_Component *toplevel_component,
			     Widget parent, MWContext* context)
: XFE_Component(toplevel_component), m_eToolbar(NULL)
{
  Widget url_frame;

  url_frame = XmCreateFrame(parent, "editBarFrame", NULL, 0);
  XtVaSetValues(url_frame,
				XmNshadowType,      XmSHADOW_OUT,
				XmNshadowThickness, 1,
				NULL);

  m_eToolbar = fe_EditorCreatePropertiesToolbar(context, url_frame, "editBar");

  XtManageChild(m_eToolbar);

  /* Add tooltip
   */
  fe_WidgetTreeWalk(m_eToolbar, fe_tooltip_mappee, NULL);

  setBaseWidget(url_frame);
}

XFE_EditorBar::~XFE_EditorBar()
{
}

