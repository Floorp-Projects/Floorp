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
   EmbeddedEditorView.cpp -- class definition for the embedded editor class
   Created: Kin Blas <kin@netscape.com>, 01-Jul-98
 */

#ifdef ENDER

#include "Frame.h"
#include "EmbeddedEditorView.h"
#include "DisplayFactory.h"
#include "ViewGlue.h"
#include "il_util.h"
#include "layers.h"
#include "BrowserFrame.h"

#ifndef NO_WEB_FONTS
#include "Mnfrf.h"
#endif
#include "fonts.h"

extern "C"
{
XFE_Frame *fe_getFrameFromContext(MWContext* context);
void XFE_EmbeddedEditorViewFocus(MWContext* context);
}



XFE_EmbeddedEditorView::XFE_EmbeddedEditorView(XFE_Component *toplevel_component,
			       Widget parent,
			       XFE_View *parent_view,
			       MWContext *context) 
  : XFE_EditorView(toplevel_component, parent, parent_view, context)
{
  XtOverrideTranslations(CONTEXT_DATA(context)->drawing_area,
                         fe_globalData.editor_global_translations);

  if (parent_view)
      parent_view->addView(this);
}

XFE_EmbeddedEditorView::~XFE_EmbeddedEditorView()
{
  XFE_EmbeddedEditorViewFocus(0);

  XFE_View *parent_view = getParent();

  if (parent_view)
      parent_view->removeView(this);
}

extern "C" void
XFE_EmbeddedEditorViewFocus(MWContext* context)
{
  static Widget currentFrame = 0;
  if (currentFrame)
    XtVaSetValues(currentFrame, XmNshadowType, XmSHADOW_ETCHED_IN, 0);
  if (context == 0)
  {
    currentFrame = 0;
    return;
  }

  XFE_Frame *frame = fe_getFrameFromContext(context);
  XP_ASSERT(frame);

  XFE_EmbeddedEditorView* eev = 0;
  if (frame)
    eev = (XFE_EmbeddedEditorView*)frame->widgetToView(CONTEXT_DATA(context)->drawing_area);
  XP_ASSERT(eev);
  if (!eev)
    return;

  Widget embedFrame = XtParent(XtParent(CONTEXT_DATA(context)->drawing_area));
  XtVaSetValues(embedFrame, XmNshadowType, XmSHADOW_IN, 0);
  currentFrame = embedFrame;

#ifdef BROWSER_FRAME_SUPPORTS_ENDER_TOOLBAR

  XFE_BrowserFrame* bf = (XFE_BrowserFrame*)frame;
  if (bf)
    bf->showEditorToolbar(eev);

#endif /* BROWSER_FRAME_SUPPORTS_ENDER_TOOLBAR */
}

#endif /* ENDER */
