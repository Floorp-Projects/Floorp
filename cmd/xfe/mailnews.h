/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   mailnews.h --- includes for the X front end mail/news stuff.
   Created: Terry Weissman <terry@netscape.com>, 20-Jun-95.
 */


#ifndef __xfe_mailnews_h_
#define __xfe_mailnews_h_

#include "menu.h"  /* for fe_button */

extern void fe_msg_command(Widget, XtPointer, XtPointer);
extern void fe_undo_cb(Widget, XtPointer, XtPointer);
extern void fe_redo_cb(Widget, XtPointer, XtPointer);
extern Boolean fe_folder_datafunc(Widget, void*, int row,
				  fe_OutlineDesc* data, int tag);
extern void fe_folder_clickfunc(Widget, void*, int row, int column,
				const char* header, int button, int clicks,
				Boolean shift, Boolean ctrl, int tag);
extern void fe_folder_dropfunc(Widget dropw, void* closure, fe_dnd_Event,
			       fe_dnd_Source*, XEvent* event);
extern void fe_folder_iconfunc(Widget, void*, int row, int tag);

extern Boolean fe_message_datafunc(Widget, void*, int row,
				   fe_OutlineDesc* data, int tag);
extern void fe_message_clickfunc(Widget, void*, int row, int column,
				 const char* header, int button, int clicks,
				 Boolean shift, Boolean ctrl, int tag);
extern void fe_message_dropfunc(Widget dropw, void* closure, fe_dnd_Event,
			       fe_dnd_Source*, XEvent* event);
extern void fe_message_iconfunc(Widget, void*, int row, int tag);

extern void fe_mn_getnewmail(Widget, XtPointer, XtPointer);

extern char* fe_mn_getmailbox(void);

/* setup routines for the 4.0 ui */
extern Widget fe_mn_MakeMailNewsWindow(MWContext *context, Widget folder_parent, Widget toolbar);

/* function for getting the active message pane */
extern MSG_Pane *fe_mn_GetActiveMessagePane(MWContext *context);

#endif /* __xfe_mailnews_h_ */
