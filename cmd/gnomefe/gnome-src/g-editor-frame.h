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
  g-editor-frame.h -- editor windows.
  Created: Chris Toshok <toshok@hungry.com>, 9-Apr-98.
*/

#ifndef _moz_editor_frame_h
#define _moz_editor_frame_h

#include "g-frame.h"
#include "g-html-view.h"

struct _MozEditorFrame {
  /* our superclass */
  MozFrame _frame;

  /* MozEditorFrame specific data members. */

};

extern void moz_editor_frame_init(MozEditorFrame *frame);
extern void moz_editor_frame_deinit(MozEditorFrame *frame);

extern MozEditorFrame* moz_editor_frame_create();

#define MOZ_EDITORFRAME(f) &((f)->_editor_frame)

#endif _moz_editor_frame_h 
