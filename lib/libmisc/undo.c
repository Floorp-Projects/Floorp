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
   undo.c --- undo facilities
 */

#include "xp.h"			/* For XP_Bool... */
#include "undo.h"


extern int MK_OUT_OF_MEMORY;


typedef enum {
  BOUNDARY,
  EVENT
} undo_event_type;

typedef struct undo_event 
{
  undo_event_type type;
  struct undo_event* next;
  struct undo_event* prev;
  void* closure;
  int (*undoit)(void*);
  void (*freeit)(void*);
  void* tag;  
  void (*freetag)(void*);  
} undo_event;


struct UndoState {
  undo_event* events;
  undo_event* redoevents;
  int depth;
  XP_Bool undoing;
  XP_Bool redoing;
  XP_Bool loggedsomething;
  int count;
  XP_AllocStructInfo allocinfo;
  int maxdepth;
  XP_Bool discardall;
};




#ifdef DEBUG
static void
undo_check_integrity(UndoState* state)
{
  int i;
  for (i=0 ; i<2 ; i++) {
    undo_event** start;
    undo_event* tmp;
    int count1, count2;
    if (i == 0) {
      start = &(state->redoevents);
    } else {
      start = &(state->events);
    }
    if (*start) {
      count1 = 0;
      for (tmp = (*start)->next ; tmp != *start ; tmp = tmp->next) {
		count1++;
      }
      count2 = 0;
      for (tmp = (*start)->prev ; tmp != *start ; tmp = tmp->prev) {
		count2++;
      }
      XP_ASSERT(count1 == count2);
    }
  }
}
#else
#define undo_check_integrity(state)	/* no-op */
#endif



UndoState*
UNDO_Create(int maxdepth)
{
  UndoState* state = XP_NEW_ZAP(UndoState);
  if (state) {
    state->maxdepth = maxdepth;
    XP_InitAllocStructInfo(&(state->allocinfo), sizeof(undo_event));
  }
  return state;
}


static void
undo_free_list(UndoState* state, undo_event** list)
{
  undo_event* tmp;
  undo_event* next;
  undo_check_integrity(state);
  if (*list) {
    (*list)->prev->next = NULL;
    for (tmp = *list ; tmp ; tmp = next) {
      next = tmp->next;
      if (tmp->freeit) (*tmp->freeit)(tmp->closure);
      if (tmp->freetag) (*tmp->freetag)(tmp->tag);      
      XP_FreeStruct(&state->allocinfo, tmp);
    }
    *list = NULL;
  }
  undo_check_integrity(state);
}

void
UNDO_Destroy(UndoState* state)
{
  undo_free_list(state, &state->events);
  undo_free_list(state, &state->redoevents);
  XP_FreeAllStructs(&state->allocinfo);
  XP_FREE(state);
}


void
UNDO_DiscardAll(UndoState* state)
{
  undo_free_list(state, &state->events);
  undo_free_list(state, &state->redoevents);
  if (state->depth > 0) state->discardall = TRUE;
  state->count = 0;
}


static int
undo_log(UndoState* state, undo_event* event)
{
  undo_event** start;
  int status;
  undo_check_integrity(state);
  if (event->type != BOUNDARY && state->depth == 0) {
    UNDO_StartBatch(state);
    status = undo_log(state, event);
    if (status < 0) {
      state->depth = 0;
      return status;
    }
    return UNDO_EndBatch(state, event->freetag, event->tag);
  }
  if (state->undoing) {
    start = &(state->redoevents);
  } else {
    start = &(state->events);
  }
  if (!*start) {
    event->next = event;
    event->prev = event;
  } else {
    event->next = *start;
    event->prev = event->next->prev;
    event->next->prev = event;
    event->prev->next = event;
  }
  *start = event;
  undo_check_integrity(state);
  return 0;
}


int
UNDO_LogEvent(UndoState* state, int (*undoit)(void*),
			  void (*freeit)(void*), void* closure,
              void (*freetag)(void*), void* tag)
{
  undo_event* tmp;
  if (state->discardall) {
    (*freeit)(closure);
    if (freetag) (*freetag)(tag);          
    return 0;
  }
  tmp = (undo_event*) XP_AllocStructZero(&state->allocinfo);
  if (!tmp) {
    UNDO_DiscardAll(state);
    (*freeit)(closure);
    if (freetag) (*freetag)(tag);
    return MK_OUT_OF_MEMORY;
  }
  
  state->loggedsomething = TRUE;
  
  tmp->type       = EVENT;
  tmp->undoit     = undoit;
  tmp->freeit     = freeit;
  tmp->closure    = closure;
  tmp->tag        = tag;
  tmp->freetag    = freetag;
  
  return undo_log(state, tmp);
}


int
UNDO_StartBatch(UndoState* state)
{
 #if 0 /* This is bad.  The redo stack is not purged when a new event is logged. */
  if (state->depth == 0) {
 #else /* A better way: Only reset loggedsomething if we're in the midst of an undo or redo. */
  if( state->undoing || state->redoing ) {
 #endif
	state->loggedsomething = FALSE;
  }
  state->depth++;
  undo_check_integrity(state);
  return 0;
}


int
UNDO_EndBatch(UndoState* state, void (*freetag)(void*), void* tag)
{
  int status;
  undo_check_integrity(state);
  XP_ASSERT(state->depth > 0);
  state->depth--;
  if (state->depth == 0) {
    undo_event** start;
    if (state->discardall) {
      UNDO_DiscardAll(state);
      state->discardall = FALSE;
      return 0;
    }
    if (state->undoing) {
      start = &(state->redoevents);
    } else {
      start = &(state->events);
	  if (state->loggedsomething) {
		undo_free_list(state, &state->redoevents);
	  }
    }
    if (*start && (*start)->type != BOUNDARY) {
      undo_event* tmp = (undo_event*)XP_AllocStructZero(&state->allocinfo);
      if (!tmp) {
		UNDO_DiscardAll(state);
		return MK_OUT_OF_MEMORY;
      }
      tmp->type     = BOUNDARY;
      tmp->tag      = tag;
      tmp->freetag  = freetag;
      status = undo_log(state, tmp);
      if (status < 0) {
		UNDO_DiscardAll(state);
		return status;
      }
      if (!state->undoing) {
		if (state->count >= state->maxdepth) {
		  /* exceeded undo count - pull one off the bottom of the stack. */
		  undo_event* prev;
		  for (tmp = state->events->prev ; ; tmp = prev) {
			undo_event_type type = tmp->type;
			/* better not be at the top of the stack, */
			XP_ASSERT(tmp != state->events);
			prev = tmp->prev;
			tmp->prev->next = tmp->next;
			tmp->next->prev = tmp->prev;
			if (type == EVENT) {
			  if (tmp->freeit) (*tmp->freeit)(tmp->closure);
              if (tmp->freetag) (*tmp->freetag)(tmp->tag);                    
			}
			XP_FreeStruct(&state->allocinfo, tmp);
			/* stop at the next boundary, which makes a whole Batch */
			if (type == BOUNDARY) break;
		  }
		}
		else
		{
		  state->count++;
		}
      }
    }
	undo_check_integrity(state);
  }
  return 0;
}



static int
undo_doone(UndoState* state, undo_event** from)
{
  int status = 0;
  undo_event* tmp = *from;
  undo_check_integrity(state);
  XP_ASSERT(tmp != NULL);
  switch (tmp->type) {
  case BOUNDARY:
    break;
  case EVENT:
	if (tmp->undoit) {
	  status = (*tmp->undoit)(tmp->closure);
	}
    break;
  default:
    XP_ASSERT(0);
  }
  *from = tmp->next;
  if (*from == tmp) {
    *from = NULL;
  } else {
    (*from)->prev = tmp->prev;
    (*from)->prev->next = (*from);
  }
  undo_check_integrity(state);
  XP_FreeStruct(&state->allocinfo, tmp);
  return status;
}
  



static int
undo_doit(UndoState* state, undo_event** from)
{
  int status;
  void (*tmp_freetag)(void*);
  void *tmp_tag;
  
  XP_ASSERT(state->depth == 0);
  if (!*from) return 0;
  status = UNDO_StartBatch(state);
  if (status < 0) return status;
  
  XP_ASSERT((*from)->type == BOUNDARY);
  
  /* Save the tag information so it persists in the opposing undo/redo stack */
  tmp_freetag = (*from)->freetag;  
  tmp_tag     = (*from)->tag;
  
  do {
    status = undo_doone(state, from);
    if (status < 0) break;
  } while (*from && (*from)->type != BOUNDARY);
  if (status >= 0) status = UNDO_EndBatch(state, tmp_freetag, tmp_tag);
  if (status < 0) {
    UNDO_DiscardAll(state);
  }
  return status;
}
  



int
UNDO_DoUndo(UndoState* state)
{
  int status;
  XP_ASSERT(state->depth == 0 && !state->undoing);
  state->undoing = TRUE;
  status = undo_doit(state, &(state->events));
  state->undoing = FALSE;
  state->count--;
  return status;
}


int
UNDO_DoRedo(UndoState* state)
{
  int status;
  undo_event* tmp = state->redoevents;
  state->redoevents = NULL;	/* Prevent any code from throwing away the
							   remaining redo events. */
  XP_ASSERT(state->depth == 0 && !state->undoing);
  state->redoing = TRUE;  
  status = undo_doit(state, &tmp);
  state->redoing = FALSE;    
  XP_ASSERT(state->redoevents == NULL);
  state->redoevents = tmp;
  return status;
}


static XP_Bool
undo_has_event(undo_event* event)
{
  undo_event* tmp = event;
  if (tmp) {
    do {
      if (tmp->type == EVENT) return TRUE;
      tmp = tmp->next;
    } while (tmp != event);
  }
  return FALSE;
}    

XP_Bool
UNDO_CanUndo(UndoState* state)
{
  return undo_has_event(state->events);
}

XP_Bool
UNDO_CanRedo(UndoState* state)
{
  return undo_has_event(state->redoevents);
}

static void *
undo_get_tag(undo_event* event)
{
  undo_event* tmp = event;
  if (tmp) {
    do {
      if (tmp->type == BOUNDARY)
         return tmp->tag;
      tmp = tmp->next;
    } while (tmp != event);
  }
  return NULL;
}    

void *
UNDO_PeekUndoTag(UndoState* state)
{
  return state ? undo_get_tag(state->events) : NULL;
}

void *
UNDO_PeekRedoTag(UndoState* state)
{
  return state ? undo_get_tag(state->redoevents) : NULL;
}
