/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/* This file is obsolete
   msgundo.h --- internal defs for the msg library, dealing with undo stuff
 */

#ifndef _MSGUNDO_H_
#define _MSGUNDO_H_


XP_BEGIN_PROTOS


/* Initialize the undo state.  This must be called when the context is
   initialized for mail/news.  It should also be called later whenever
   the undo state should be thrown away (because the user has just
   done an non-undoable operation. If this returns a negative value,
   then we failed big-time (ran out of memory), and we should abort
   everything.*/
extern int msg_undo_Initialize(MWContext* context);


/* Throw away the undo state, as part of destroying the mail/news context. */
extern void msg_undo_Cleanup(MWContext* context);



/* Log an "flag change" event.  The given flag bits indicate what to do to
   execute the undo. */

extern void msg_undo_LogFlagChange(MWContext* context, MSG_Folder* folder,
								   unsigned int message_offset,
								   uint16 flagson, uint16 flagsoff);


/* Mark the beginning of a bunch of actions that should be undone as
   one user action.  Should always be matched by a later call to
   msg_undo_EndBatch().  These calls can nest. */

extern void msg_undo_StartBatch(MWContext* context);

extern void msg_undo_EndBatch(MWContext* context);


extern XP_Bool msg_CanUndo(MWContext* context);
extern XP_Bool msg_CanRedo(MWContext* context);

XP_END_PROTOS

#endif /* !_MSGUNDO_H_ */
