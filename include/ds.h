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

#ifndef __DS_h_
#define __DS_h_
#ifdef XP_WIN32
#include <windows.h>
#endif /* XP_WIN32 */

#ifdef XP_OS2
#define INCL_WIN
#define INCL_GPI
#define TID OS2TID   /* global rename in OS2 H's!               */
#include <os2.h>
#undef TID           /* and restore                             */
#endif

#include "xp_mcom.h"

XP_BEGIN_PROTOS

/* Typedefs */
typedef struct DSArrayStr DSArray;
typedef struct DSLinkStr DSLink;
typedef struct DSListStr DSList;
typedef struct DSArenaStr DSArena;

#define DS_MIN(a,b) ((a)<(b)?(a):(b))
#define DS_MAX(a,b) ((a)>(b)?(a):(b))

/*
** Your basic boolean. Done as an enum to cause compiler warnings when a
** boolean procedure doesn't return the right value.
** LISA SEZ: Please do not use this; use PRBool instead.  Eventually
** (as soon as I can "make it so") DSBool is going away in favor of PRBool.
*/
typedef enum DSBoolEnum {
    DSTrue = 1,
    DSFalse = 0
} DSBool;

/*
** A status code. Status's are used by procedures that return status
** values. Again the motivation is so that a compiler can generate
** warnings when return values are wrong. Correct testing of status codes:
**
**	DSStatus rv;
**	rv = some_function (some_argument);
**	if (rv != DSSuccess)
**		do_an_error_thing();
**
*/
typedef enum DSStatusEnum {
    DSWouldBlock = -2,
    DSFailure = -1,
    DSSuccess = 0
} DSStatus;

/*
** A comparison code. Used for procedures that return comparision
** values. Again the motivation is so that a compiler can generate
** warnings when return values are wrong.
*/
typedef enum DSComparisonEnum {
    DSLessThan = -1,
    DSEqual = 0,
    DSGreaterThan = 1
} DSComparison;

typedef void (*DSElementFreeFunc)(void *e1, DSBool freeit);
typedef int (*DSElementCompareFunc)(void *e1, void *e2);

/************************************************************************/

/*
** Simple variable length array of pointers. The array keeps a NULL
** pointer at the end of the array.
*/
struct DSArrayStr {
    void **things;
    DSElementFreeFunc freeElement;
};

extern DSArray *DS_CreateArray(int slots);
extern DSStatus DS_GrowArray(DSArray *da, int slots);
extern void DS_SetArrayMethods(DSArray *da, DSElementFreeFunc free);
extern void DS_DestroyArray(DSArray *da, DSBool freeit);
extern void DS_Sort(DSArray *da, DSElementCompareFunc compare);
extern int DS_Elements(DSArray *da);
extern DSStatus DS_AddElement(DSArray *da, void *element);
extern void DS_RemoveElement(DSArray *da, void *element);

/************************************************************************/

/*
** Circular linked list. Each link contains a pointer to the object that
** is actually in the list.
*/
struct DSLinkStr {
    DSLink *next;
    DSLink *prev;
    void *thing;
};

struct DSListStr {
    DSLink link;
};

#define DS_InitList(lst)	     \
{				     \
    (lst)->link.next = &(lst)->link; \
    (lst)->link.prev = &(lst)->link; \
    (lst)->link.thing = 0;	     \
}

#define DS_ListEmpty(lst) \
    ((lst)->link.next == &(lst)->link)

#define DS_ListHead(lst) \
    ((lst)->link.next)

#define DS_ListTail(lst) \
    ((lst)->link.prev)

#define DS_ListIterDone(lst,lnk) \
    ((lnk) == &(lst)->link)

#define DS_AppendLink(lst,lnk)	    \
{				    \
    (lnk)->next = &(lst)->link;	    \
    (lnk)->prev = (lst)->link.prev; \
    (lst)->link.prev->next = (lnk); \
    (lst)->link.prev = (lnk);	    \
}

#define DS_InsertLink(lst,lnk)	    \
{				    \
    (lnk)->next = (lst)->link.next; \
    (lnk)->prev = &(lst)->link;	    \
    (lst)->link.next->prev = (lnk); \
    (lst)->link.next = (lnk);	    \
}

#define DS_RemoveLink(lnk)	     \
{				     \
    (lnk)->next->prev = (lnk)->prev; \
    (lnk)->prev->next = (lnk)->next; \
    (lnk)->next = 0;		     \
    (lnk)->prev = 0;		     \
}

extern DSLink *DS_NewLink(void *thing);
extern DSLink *DS_FindLink(DSList *lst, void *thing);
extern void DS_DestroyLink(DSLink *lnk, DSBool freeit);

/************************************************************************/

/*
** Memory manager
*/

/*
** at one time XP_Block was a float* to force clients to cast things
** before use. Now DSBlock is defined since that will be most convenient
** for almost all uses.
*/

typedef unsigned char *DSBlock;
/*
** Allocate some memory. Always allocates at least one byte of memory.
*/
extern void *DS_Alloc(size_t bytes);

/*
** Reallocate some memory, growing or shrinking the memory.
*/
extern void *DS_Realloc(void *oldptr, size_t bytes);

/*
** Allocate and then zero some memory. Always allocates at least one byte
** of memory.
*/
extern void *DS_Zalloc(size_t bytes);

/*
** Allocate a block of memory. Always allocates at least one byte of
** memory.
*/
extern DSBlock DS_AllocBlock(size_t bytes);

/*
** Reallocate a block of memory, growing or shrinking the memory block.
*/
extern DSBlock DS_ReallocBlock(DSBlock block, size_t newBytes);

/*
** Free a block of memory. Safe to use on null pointers.
*/
extern void DS_FreeBlock(DSBlock block);

/*
** Free a chunk of memory. Safe to use on null pointers.
*/
extern void DS_Free(void *ptr);

/*
** Zero and then free a chunk of memory. Safe to use on null pointers.
*/
extern void DS_Zfree(void *ptr, size_t bytes);

/*
 * Low cost Malloc Arenas.
 *
 * The chunks are a linked list.
 * The beginning of each chunk is a pointer to the next chunk.
 */
struct DSArenaStr {
    unsigned long chunkSize;	/* size of each chunk */
    unsigned int refCount;	/* reference count */
    void ** firstChunk;		/* pointer to first chunk */
    void ** lastChunk;		/* pointer to last chunk */
    void * pLast;		/* last item allocated */
    void * pCur;		/* beginning of free area */
    void * pCurEnd;		/* end of free area in current chunk */
};

/* make a new arena */
extern DSArena *
DS_NewArena(unsigned long chunkSize);

/* destroy an arena, and free all memory associated with it */
extern void
DS_FreeArena(DSArena *arena, DSBool zero);

/* malloc a chunk of data from the arena */
extern void *
DS_ArenaAlloc(DSArena *arena, unsigned long size);

/* malloc a chunk of data from the arena, zero filling it */
extern void *
DS_ArenaZalloc(DSArena *arena, unsigned long size);

/* change the size of an object, works best if it was the last object
 * allocated
 */
extern void *
DS_ArenaGrow(DSArena *arena, void *pOld, unsigned long oldsize,
	     unsigned long newsize);

XP_END_PROTOS

#endif /* __DS_h_ */
