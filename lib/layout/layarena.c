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

#include "xp.h"
#include "layout.h"

#ifdef MEMORY_ARENAS


#define ARENA_SIZE	16384


void
lo_InitializeMemoryArena(lo_TopState *top_state)
{
	lo_arena *aptr;
	int32 arena_size;

	top_state->first_arena = NULL;
	top_state->current_arena = NULL;
	arena_size = (int32) (ARENA_SIZE + sizeof(lo_arena));
	arena_size = ((arena_size + 3) & ~3);
	aptr = (lo_arena *)XP_ALLOC(arena_size);
	if (aptr == NULL)
	{
		top_state->first_arena = NULL;
		top_state->current_arena = NULL;
		return;
	}

#ifdef XP_WIN16
	aptr->limit = (char *)((int32)((char *)aptr) + arena_size);
	aptr->avail = (char *)((int32)((char *)aptr) + (int32)sizeof(lo_arena));
#else
	aptr->limit = (char *)aptr + arena_size;
	aptr->avail = (char *)aptr + sizeof(lo_arena);
#endif
	aptr->next = NULL;
	top_state->first_arena = aptr;
	top_state->current_arena = aptr;
}


int32
lo_FreeMemoryArena(lo_arena *arena)
{
	int32 cnt;

	cnt = 0;
	while (arena != NULL)
	{
		lo_arena *tmp_aptr;

		tmp_aptr = arena;
		arena = arena->next;
		cnt += (int32 )(tmp_aptr->limit - (char *)tmp_aptr);
		XP_FREE((char *)tmp_aptr);
	}
	return(cnt);
}


char *
lo_MemoryArenaAllocate(lo_TopState *top_state, int32 size)
{
	lo_arena *aptr;
	char *ptr;

	if ((top_state == NULL)||(top_state->current_arena == NULL))
	{
		return(NULL);
	}

	aptr = top_state->current_arena;
#ifdef XP_WIN16
	while ((int32)((int32)((char *)aptr->avail) + size) >
		(int32)((char *)aptr->limit))
#else
	while ((aptr->avail + size) > aptr->limit)
#endif
	{
		if (aptr->next != NULL)
		{
			aptr = aptr->next;
#ifdef XP_WIN16
			aptr->avail = (char *)((int32)((char *)aptr) + (int32)sizeof(lo_arena));
#else
			aptr->avail = (char *)aptr + sizeof(lo_arena);
#endif
		}
		else
		{
			int32 arena_size;

			arena_size = (int32) (ARENA_SIZE + sizeof(lo_arena));
			arena_size = ((arena_size + 3) & ~3);
			aptr->next = (lo_arena *)XP_ALLOC(arena_size);
			if (aptr->next == NULL)
			{
				return(NULL);
			}
			aptr = aptr->next;
#ifdef XP_WIN16
			aptr->limit = (char *)((int32)((char *)aptr) + arena_size);
			aptr->avail = (char *)((int32)((char *)aptr) + (int32)sizeof(lo_arena));
#else
			aptr->limit = (char *)aptr + arena_size;
			aptr->avail = (char *)aptr + sizeof(lo_arena);
#endif
			aptr->next = NULL;
		}
	}
	top_state->current_arena = aptr;
	ptr = (char *)aptr->avail;
#ifdef XP_WIN16
	aptr->avail = (char *)((int32)((char *)aptr->avail) + size);
#else
	aptr->avail += size;
#endif
	return(ptr);
}

#endif /* MEMORY_ARENAS */
