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
   EmbeddedEditorView.h -- class definition for the embedded editor class
   Created: Kin Blas <kin@netscape.com>, 01-Jul-98
 */



#ifndef _xfe_embeddededitorview_h
#define _xfe_embeddededitorview_h

#ifdef ENDER

#include "EditorView.h"

class XFE_EmbeddedEditorView : public XFE_EditorView
{
public:
  XFE_EmbeddedEditorView(XFE_Component *toplevel_component, Widget parent, XFE_View *parent_view, MWContext *context);
  ~XFE_EmbeddedEditorView();
};

#endif /* ENDER */

#endif /* _xfe_embeddededitorview_h */
