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
  g-history-frame.h -- history windows.
  Created: Chris Toshok <toshok@hungry.com>, 9-Apr-98.
*/

#ifndef _moz_history_frame_h
#define _moz_history_frame_h

#include "g-frame.h"
#include "g-html-view.h"

struct _MozHistoryFrame {
  /* our superclass */
  MozFrame _frame;

  /* MozHistoryFrame specific data members. */

};

extern void moz_history_frame_init(MozHistoryFrame *frame);
extern void moz_history_frame_deinit(MozHistoryFrame *frame);

extern MozHistoryFrame* moz_history_frame_create();

#define MOZ_HISTORYFRAME(f) &((f)->_history_frame)

#endif _moz_history_frame_h 
