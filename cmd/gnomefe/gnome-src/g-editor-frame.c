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
  g-editor-frame.c -- editor windows.
  Created: Chris Toshok <toshok@hungry.com>, 13-Apr-98.
*/

#include "xp_mem.h"
#include "structs.h"
#include "ntypes.h"
#include "g-editor-frame.h"

void
moz_editor_frame_init(MozEditorFrame *frame)
{
  printf("moz_editor_frame_init (empty)\n");
}

void
moz_editor_frame_deinit(MozEditorFrame *frame)
{
  printf("moz_editor_frame_deinit (empty)\n");
}


MozEditorFrame*
moz_editor_frame_create()
{
  MozEditorFrame *frame = XP_NEW_ZAP(MozEditorFrame);
  MozHTMLView *view;

  moz_editor_frame_init(frame);

  MOZ_FRAME(frame)->context->type = MWContextEditor;

  gtk_widget_realize(MOZ_COMPONENT(frame)->base_widget);

  view = moz_html_view_create(MOZ_FRAME(frame), MOZ_FRAME(frame)->context);

  MOZ_FRAME(frame)->top_view = MOZ_VIEW(view);

  moz_frame_set_viewarea(MOZ_FRAME(frame),
                         MOZ_COMPONENT(view)->base_widget);

  gtk_widget_set_usize(MOZ_COMPONENT(frame)->base_widget,
                       300, 400); /* XXX save off the default editor window size. */
                                                    
  return frame;
}

