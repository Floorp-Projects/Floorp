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
   EmbeddedEditor.h -- class definition for the embedded editor class
   Created: Kin Blas <kin@netscape.com>, 08-Sep-98
 */



#ifndef _xfe_embeddededitor_h
#define _xfe_embeddededitor_h

#ifdef ENDER

#include "EmbeddedEditorView.h"
#include "Toolbox.h"
#include "EditorToolbar.h"

class XFE_EmbeddedEditor : public XFE_View
{
  XFE_Toolbox            *m_toolbox;
  XFE_EditorToolbar      *m_toolbar;
  XFE_EmbeddedEditorView *m_editorView;
  MWContext              *m_editorContext;

public:
  XFE_EmbeddedEditor(XFE_Component *toplevel_component, Widget parent, XFE_View *parent_view, int32 cols, int32 rows, const char *default_url, MWContext *context);
  ~XFE_EmbeddedEditor();

  virtual MWContext *getEditorContext();
  virtual XFE_EmbeddedEditorView *getEditorView();
  virtual void setScrollbarsActive(XP_Bool b);
  virtual void showToolbar();
  virtual void hideToolbar();
};

#endif /* ENDER */

#endif /* _xfe_embeddededitor_h */
