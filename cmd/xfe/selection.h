/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   selection.h --- Selections: Cut, Copy, Paste, etc
   Created: Stephen Lamm <slamm@netscape.com>, 8 April 1997.
 */


#ifndef _SELECTION_H_
#define _SELECTION_H_

#include "mozilla.h"
#include "xfe.h"

XP_BEGIN_PROTOS

extern void fe_own_selection_1 (MWContext*, Time, Boolean, char *);

extern void fe_OwnSelection (MWContext *, Time, Boolean);
extern void fe_DisownSelection (MWContext *, Time, Boolean);

extern void fe_OwnEDTSelection (MWContext*, Time, Boolean, 
								char*, char*, char*, unsigned long);

extern void fe_cut_cb (Widget, XtPointer, XtPointer);
extern void fe_copy_cb (Widget, XtPointer, XtPointer);
extern void fe_paste_cb (Widget, XtPointer, XtPointer);
extern void fe_paste_quoted_cb (Widget, XtPointer, XtPointer);

extern Boolean fe_can_cut (MWContext *context);
extern Boolean fe_can_copy (MWContext *context);
extern Boolean fe_can_paste (MWContext *context);

extern void fe_OwnClipboard(Widget, Time,
							char*, char*, char*, unsigned long, 
							XP_Bool);
extern void fe_OwnPrimarySelection(Widget, Time,
								   char*, char*, char*, unsigned long,
								   MWContext*); 

extern void fe_DisownClipboard(Widget, Time);
extern void fe_DisownPrimarySelection(Widget, Time);

extern char* fe_GetPrimaryText(void);
extern char* fe_GetPrimaryHtml(void);
extern char* fe_GetPrimaryData(unsigned long*);

extern char* fe_GetInternalText(void);
extern char* fe_GetInternalHtml(void);
extern char* fe_GetInternalData(unsigned long*);

extern XP_Bool fe_InternalSelection(XP_Bool, 
									unsigned long*, 
									unsigned long*, 
									unsigned long*);

extern char* xfe2_GetClipboardText (Widget, int32 *);
extern int xfe_clipboard_retrieve_work(Widget focus, char* name,
                                       char** buf, unsigned long* buf_len_a,
                                       Time timestamp);

XP_END_PROTOS

#endif
