/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; -*-  
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

/*******************************************************************************
                            S P O R T   M O D E L    
                              _____
                         ____/_____\____
                        /__o____o____o__\        __
                        \_______________/       (@@)/
                         /\_____|_____/\       x~[]~
             ~~~~~~~~~~~/~~~~~~~|~~~~~~~\~~~~~~~~/\~~~~~~~~~

    Advanced Technology Garbage Collector
    Copyright (c) 1997 Netscape Communications, Inc. All rights reserved.
    Recovered by: Warren Harris
*******************************************************************************/

#ifndef __SMHEAP__
#define __SMHEAP__

#include "sm.h"
#include "smgen.h"
#include "smstack.h"
#include "prthread.h"
#include "prmon.h"

SM_BEGIN_EXTERN_C

/*******************************************************************************
 * Object Descriptors
 ******************************************************************************/

/* object is finalizable -- flag used both inside and outside of gc */

#define SM_OBJDESC_FINALIZABLE_MASK     (1 << SMObjectDescFlag_FinalizableBit)

#define SM_OBJDESC_IS_FINALIZABLE(od)                                     \
    ((od)->flags & SM_OBJDESC_FINALIZABLE_MASK)                           \

#define SM_OBJDESC_SET_FINALIZABLE(od)                                    \
    (SM_ASSERT(!SM_OBJDESC_IS_FREE(od)),                                  \
     (od)->flags |= SM_OBJDESC_FINALIZABLE_MASK)                          \

/******************************************************************************/

/* object is otherwise free but needs finalization */

#define SM_OBJDESC_NEEDS_FINALIZATION_MASK                                \
    (1 << SMObjectDescFlag_NeedsFinalizationBit)                          \

#define SM_OBJDESC_NEEDS_FINALIZATION(od)                                 \
    ((od)->flags & SM_OBJDESC_NEEDS_FINALIZATION_MASK)                    \

#define SM_OBJDESC_SET_NEEDS_FINALIZATION(od)                             \
    ((od)->flags |= SM_OBJDESC_NEEDS_FINALIZATION_MASK)                   \

#define SM_OBJDESC_CLEAR_FINALIZATION(od)                                 \
    ((od)->flags &= ~(SM_OBJDESC_NEEDS_FINALIZATION_MASK |                \
               SM_OBJDESC_FINALIZABLE_MASK))                              \

/******************************************************************************/

/* object was free before we started the gc -- gc-only state */

#define SM_OBJDESC_WAS_FREE(od)                                           \
    (((od)->flags & SM_OBJDESC_STATE_MASK) == SMObjectState_WasFree)      \

/******************************************************************************/

/* object is pinned -- gc-only flag */

#define SM_OBJDESC_PINNED_MASK          (1 << SMObjectDescFlag_PinnedBit)

/* we need a version of this without asserts or else we blow out CodeWarrior */
#define SM_OBJDESC_IS_PINNED0(od)                                         \
    ((od)->flags & SM_OBJDESC_PINNED_MASK)                                \

#define SM_OBJDESC_IS_PINNED(od)                                          \
    (SM_ASSERT(!SM_OBJDESC_WAS_FREE(od)),                                 \
     SM_OBJDESC_IS_PINNED0(od))                                           \

#define SM_OBJDESC_SET_PINNED(od)                                         \
    (SM_ASSERT(!SM_OBJDESC_WAS_FREE(od)),                                 \
     (od)->flags |= SM_OBJDESC_PINNED_MASK)                               \

#define SM_OBJDESC_CLEAR_PINNED(od)                                       \
    (SM_ASSERT(!SM_OBJDESC_WAS_FREE(od)),                                 \
     (od)->flags &= ~SM_OBJDESC_PINNED_MASK)                              \

/******************************************************************************/

/* object is marked -- gc-only state */

#define SM_OBJDESC_IS_MARKED(od)        ((od)->flags < 0)

#define SM_OBJDESC_UNTRACED_OR_UNMARKED_MASK                              \
    ((1 << SMObjectDescFlag_StateBit2) |                                  \
     (1 << SMObjectDescFlag_StateBit0))                                   \

#define SM_OBJDESC_UNTRACED_OR_UNMARKED(od)                               \
    (((od)->flags & SM_OBJDESC_UNTRACED_OR_UNMARKED_MASK)                 \
     == SMObjectState_Unmarked)                                           \

#define SM_OBJDESC_SET_MARKED(od)                                         \
    (SM_ASSERT(SM_OBJDESC_UNTRACED_OR_UNMARKED(od)),                      \
     ((od)->flags |= SMObjectState_Marked))                               \

/******************************************************************************/

/* done before a gc (works for free objects too) -- gc-only state */

#define SM_OBJDESC_IS_UNMARKED(od)                                        \
    (((od)->flags & SM_OBJDESC_STATE_MASK) == SMObjectState_Unmarked)     \

#define SM_OBJDESC_SET_UNMARKED(od)                                       \
    ((od)->flags &= ~(SM_OBJDESC_ALLOCATED_MASK | SM_OBJDESC_PINNED_MASK))\

/******************************************************************************/

/* done when the mark stack overflows -- gc-only state */

#define SM_OBJDESC_IS_UNTRACED(od)                                        \
    (((od)->flags & SM_OBJDESC_STATE_MASK) == SMObjectState_Untraced)     \

#define SM_OBJDESC_SET_UNTRACED(od)                                       \
    (SM_ASSERT(SM_OBJDESC_IS_MARKED(od)),                                 \
     (od)->flags |= SMObjectState_Untraced)                               \

#define SM_OBJDESC_CLEAR_UNTRACED(od)                                     \
    (SM_ASSERT(SM_OBJDESC_IS_UNTRACED(od)),                               \
     (od)->flags &= ~(1 << SMObjectDescFlag_StateBit2))                   \

/******************************************************************************/

/* done when an object is promoted -- gc-only state */

#define SM_OBJDESC_IS_FORWARDED(od)                                       \
    (((od)->flags & SM_OBJDESC_STATE_MASK) == SMObjectState_Forwarded)    \

#define SM_OBJDESC_SET_FORWARDED(od)                                      \
    (SM_ASSERT(SM_OBJDESC_IS_UNMARKED(od)),                               \
     SM_ASSERT(!SM_OBJDESC_IS_PINNED0(od)),                               \
     (od)->flags = (PRUint8)SMObjectState_Forwarded)                      \

/******************************************************************************/

/* done when an object survives a copy cycle -- gc-only state */

#define SM_OBJDESC_COPYABLE_MASK        (1 << SMObjectDescFlag_CopyableBit)

#define SM_OBJDESC_IS_COPYABLE(od)                                        \
    (((od)->flags & SM_OBJDESC_COPYABLE_MASK) == SM_OBJDESC_COPYABLE_MASK)\

#define SM_OBJDESC_SET_COPYABLE(od)                                       \
    ((od)->flags |= SM_OBJDESC_COPYABLE_MASK)                             \

#define SM_OBJDESC_SET_COPYABLE_BUT_PINNED(od)                            \
    ((od)->flags |= (SM_OBJDESC_COPYABLE_MASK | SM_OBJDESC_PINNED_MASK))  \

/* copyable and not pinned */
#define SM_OBJDESC_DO_COPY(od)                                            \
    (SM_ASSERT(!SM_OBJDESC_WAS_FREE(od)),                                 \
     ((od)->flags & (SM_OBJDESC_PINNED_MASK | SM_OBJDESC_COPYABLE_MASK))  \
      == SM_OBJDESC_COPYABLE_MASK)                                        \

/******************************************************************************/

#ifdef DEBUG
#define SM_COPY_CYCLE_BITS              1  /* copy every other collect */
#else
#define SM_COPY_CYCLE_BITS              3  /* copy every 8th collect */
#endif
#define SM_COPY_CYCLE_MASK              ((1 << SM_COPY_CYCLE_BITS) - 1)
#define SM_IS_COPY_CYCLE(genNum)        \
    ((PRBool)((sm_Heap.gen[genNum].collectCount & SM_COPY_CYCLE_MASK) == 0))

/*******************************************************************************
 * Object Operations
 ******************************************************************************/

#define SM_OBJECT_FORWARDING_ADDR(obj)                                    \
    ((SMObject*)SM_OBJECT_CLASS(obj))                                     \

#define SM_OBJECT_SET_FORWARDING_ADDR(obj, newAddr)                       \
    (SM_OBJECT_CLASS(obj) = (SMClass*)(SM_ENSURE(SMObject, newAddr)))     \

/*******************************************************************************
 * Card Descriptors
 ******************************************************************************/

typedef PRUint8  SMCardDesc;

#define SM_CARDDESC_IS_DIRTY(cd)                ((cd) == 0)
#define SM_CARDDESC_SET_GEN(cdAddr, genNum)     (*(cdAddr) = SM_GEN_CARDDESC(genNum))
#define SM_CARDDESC_INIT(cdAddr)                SM_CARDDESC_SET_GEN(cdAddr, SMGenNum_Free)

/*******************************************************************************
 * Heap
 ******************************************************************************/

struct SMHeap {
    SMGen               gen[SM_GEN_COUNT];
    SMStack             markStack;
    SMMarkRootsProc     markRootsProc;
    void*               markRootsClosure;
    SMGCHookProc        beforeGCHook;
    void*               beforeGCClosure;
    SMGCHookProc        afterGCHook;
    void*               afterGCClosure;
    SMGenNum            collectingGenNum;
    /* finalizer stuff */
    PRThread*           finalizer;
    PRMonitor*          finalizerMon;
    PRBool              keepRunning;
#ifndef SM_NO_WRITE_BARRIER
    SMCardDesc*         cardTableMem;
    PRUword             cardTablePageCount;
#endif
};

/* So here's the story: I was going to allow multiple heaps to exist
   simultaneously (not strictly necessary, but it might be a useful feature
   someday) but I backed it out because of the extra dereferences it introduces
   in order to get at any of the heap global data. If we ever need it, we can
   go back and introduce a heap argument to most of the routines, and eliminate
   this global. We'd also have to make SM_Init return a new heap. */
extern SMHeap sm_Heap;      /* _the_ global heap */

/*******************************************************************************
 * Debug Methods
 ******************************************************************************/

#ifdef SM_STATS	/* define this if you want stat code in the final product */

#include <stdio.h>

typedef struct SMStatRecord {
    PRUword             amountUsed;
    PRUword             amountFree;
    PRUword             overhead;
    PRUword             collections;
    /* for figuring internal fragmentation of allocation requests */
    PRUword             totalAllocated;
    PRUword             totalRequested;
} SMStatRecord;

typedef struct SMStats {
    SMStatRecord        total;
    SMStatRecord        perGen[SM_GEN_COUNT + 1];
    PRUword             systemOverhead;
} SMStats;

SM_EXTERN(void)
SM_Stats(SMStats* stats);

SM_EXTERN(void)
SM_DumpStats(FILE* out, PRBool detailed);

#endif /* SM_STATS */

/******************************************************************************/

#ifdef DEBUG

SM_EXTERN(void)
SM_SetCollectThresholds(PRUword gen0Threshold, PRUword gen1Threshold,
                        PRUword gen2Threshold, PRUword gen3Threshold,
                        PRUword staticGenThreshold);

#endif /* DEBUG */

#ifdef SM_VERIFY

extern void
sm_VerifyHeap(void);

#define SM_VERIFY_HEAP()        sm_VerifyHeap()

#else /* !SM_VERIFY */

#define SM_VERIFY_HEAP()        /* no-op */

#endif /* !SM_VERIFY */

/*******************************************************************************
 * Private Methods
 ******************************************************************************/

extern PRStatus
sm_InitFinalizer(void);

extern PRStatus
sm_FiniFinalizer(PRBool finalizeOnExit);

extern SMObjStruct*
sm_AddGCPage(SMGenNum genNum, SMBucket bucket);

extern SMObjStruct*
sm_Alloc(PRUword instanceSize, SMBucket bucket);

extern void
sm_Collect0(SMGenNum collectingGenNum, PRBool copyCycle);

extern PRBool
sm_CollectAndFinalizeAll(PRUword spaceNeeded);

/******************************************************************************/

SM_END_EXTERN_C

#endif /* __SMHEAP__ */
/******************************************************************************/
