/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
  undo.h --- creating and maintaining an undo list.
  Created: Terry Weissman <terry@netscape.com>, 9-Sept-95.
*/

#ifndef _UNDO_H_
#define _UNDO_H_



typedef struct UndoState UndoState;


XP_BEGIN_PROTOS

/* Create a new undo state. (Returns NULL if no memory available). State will
   be saved to execute up to "maxdepth" undos in a row.*/
extern UndoState* UNDO_Create(int maxdepth);


/* Throw away the undo state. */
extern void UNDO_Destroy(UndoState* state);


/* Throw away all the queued events in the undo state.  If we are in the middle
   of a batch (there are outstanding calls to UNDO_StartBatch()), then future
   events in the batch are also thrown away. */
extern void UNDO_DiscardAll(UndoState* state);

/* Mark the beginning of a bunch of actions that should be undone as one user
   action.  Should always be matched by a later call to UNDO_EndBatch().
   These calls can nest.  They fail only if running out of memory, in which
   case they will call UNDO_DiscardAll(). */
   
/* Note you can use the tag arguments to associate a batch of events with 
   some user defined tag e.g., a name or object identifying the batch of events */
   
extern int UNDO_StartBatch(UndoState* state);

extern int UNDO_EndBatch(UndoState* state, void (*freetag)(void*), void* tag);


/* Returns TRUE if undoing will do something (i.e., the menu item for "Undo"
   should be selectable). */

extern XP_Bool UNDO_CanUndo(UndoState* state);


/* Returns TRUE if redoing will do something (i.e., the menu item for "Redo"
   should be selectable). */

extern XP_Bool UNDO_CanRedo(UndoState* state);


/* Actually do an undo.  Should only be called if UNDO_CanUndo returned TRUE.
   May not be called if there are any pending calls to UNDO_StartBatch. */

extern int UNDO_DoUndo(UndoState* state);


/* Actually do an redo.  Should only be called if UNDO_CanRedo returned TRUE.
   May not be called if there are any pending calls to UNDO_StartBatch. */

extern int UNDO_DoRedo(UndoState* state);


/* Log an event.  The "undoit" function is to be called with the closure to
   undo an event that just happened.  It returns a success code; if negative,
   the code will be propagated up to the call to UNDO_DoUndo and UNDO_DoRedo,
   and UNDO_DiscardAll will be called.  Note that the undoit function almost
   always ends up calling UNDO_LogEvent again, to log the function to undo thie
   undoing of this action.  If you get my drift.

   The "freeit" function is called when the undo library decides it will never
   ever call undoit function.  It is called with the closure to free storage
   used by the closure.

   If this fails (we ran out of memory), then it will return a negative failure
   code, and call UNDO_DiscardAll() and the freeit func. */
   
/* Note you can use the tag arguments to associate the event with 
   some user defined tag e.g., a name or object identifying the event. */

#ifdef XP_OS2
typedef int (*PNSLUFN)(void *);
typedef void (*PNSLFFN)(void *);
extern int UNDO_LogEvent(UndoState* state, PNSLUFN undoit,
						 PNSLFFN freeit, void* closure,
                         PNSLFFN freetag, void* tag);
#else
extern int UNDO_LogEvent(UndoState* state, int (*undoit)(void*),
						 void (*freeit)(void*), void* closure,
                         void (*freetag)(void*), void* tag);
#endif

/* Retrieve the undo/redo tag from the top of the corresponding stack.  The
   tag is the "thing" YOU assigned during either an UNDO_EndBatch() or
   UNDO_LogEvent call.  You most likely use the tag to identify the event[s]
   that can be undone/redone.
   e.g., Label the Edit|Undo menu item with, say, Edit|Undo Delete. */
   
extern void *UNDO_PeekUndoTag(UndoState* state);
extern void *UNDO_PeekRedoTag(UndoState* state);

XP_END_PROTOS

#endif /* !_UNDO_H_ */
