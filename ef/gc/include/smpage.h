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

#ifndef __SMPAGE__
#define __SMPAGE__

#include "sm.h"
#include "prmon.h"

SM_BEGIN_EXTERN_C

/*******************************************************************************
 * Constants
 ******************************************************************************/

#define SM_PAGE_BITS            12                      /* 4k pages */
#define SM_PAGE_SIZE            (1 << SM_PAGE_BITS)
#define SM_PAGE_MASK            (SM_PAGE_SIZE - 1)
#define SM_PAGE_REFS            (SM_PAGE_SIZE / sizeof(void*))

/* (1 << SM_REF_BITS) == sizeof(void*) */
#if defined(XP_WIN) && !defined(_WIN32)
#define SM_REF_BITS             1
#elif defined(IS_64)
#define SM_REF_BITS             3
#else
#define SM_REF_BITS             2
#endif

/*******************************************************************************
 * Types
 ******************************************************************************/

typedef PRUint8  SMPage[SM_PAGE_SIZE];
typedef PRUword SMPageCount;    /* int big enough to count pages */

/* SMSmallObjCount: int big enough to count objects per page */
#if SM_PAGE_BITS <= 8
typedef PRUint8 SMSmallObjCount;
#elif SM_PAGE_BITS <= 16
typedef PRUint16 SMSmallObjCount;
#else
typedef PRUint32 SMSmallObjCount;
#endif

/*******************************************************************************
 * Macros
 ******************************************************************************/

#define SM_PAGE_NUMBER(addr)    ((SMPageCount)(addr) >> SM_PAGE_BITS)
#define SM_PAGE_ADDR(num)       ((SMPage*)((SMPageCount)(num) << SM_PAGE_BITS))
#define SM_PAGE_OFFSET(addr)    ((PRUword)(addr) & SM_PAGE_MASK)
#define SM_PAGE_REFOFFSET(addr) (SM_PAGE_OFFSET(addr) >> SM_REF_BITS)
#define SM_PAGE_BASE(addr)      ((SMPage*)((PRUword)(addr) & ~SM_PAGE_MASK))
#define SM_PAGE_COUNT(size)     SM_PAGE_NUMBER((size) + SM_PAGE_MASK)
#define SM_PAGE_WIDTH(count)    ((PRUword)((SMPageCount)(count) << SM_PAGE_BITS))

#define SM_PAGE_ROUNDDN(addr)   ((SMPage*)SM_ALIGN((PRUword)(addr), SM_PAGE_BITS))
#define SM_PAGE_ROUNDUP(addr)   SM_PAGE_ROUNDDN((PRUword)(addr) + SM_PAGE_SIZE)

/*******************************************************************************
 * Object Descriptors
 ******************************************************************************/

typedef struct SMObjDesc {
    PRInt8         flags;        /* must be signed */
} SMObjDesc;

typedef enum SMObjectDescFlag {
    SMObjectDescFlag_FinalizableBit,
    SMObjectDescFlag_NeedsFinalizationBit,
    SMObjectDescFlag_PinnedBit,
    SMObjectDescFlag_CopyableBit,
    
    SMObjectDescFlag_Unused0,
    SMObjectDescFlag_StateBit0,
    SMObjectDescFlag_StateBit1,
    SMObjectDescFlag_StateBit2 /* high bit -- can use test for negative */
} SMObjectDescFlag;

/* Be careful about changing these SMObjectState values. The macros below
   are highly dependent on them. */
typedef enum SMObjectState {
    SMObjectState_WasFree       = 0x00,    /* 00000000 */
    SMObjectState_Unmarked      = 0x20,    /* 00100000 */
    SMObjectState_Untraced      = 0x60,    /* 01100000 */
    SMObjectState_Forwarded     = 0x80,    /* 10000000 */
    SMObjectState_Marked        = 0xE0     /* 11100000 */
} SMObjectState;

#define SM_OBJDESC_STATE_MASK           ((1 << SMObjectDescFlag_StateBit2) | \
                                         (1 << SMObjectDescFlag_StateBit1) | \
                                         (1 << SMObjectDescFlag_StateBit0))

/******************************************************************************/

/* (space for) object is really free -- non-gc state */

#define SM_OBJDESC_FREE_MASK            (1 << SMObjectDescFlag_StateBit1)

#define SM_OBJDESC_IS_FREE(od)                                            \
    (((od)->flags & SM_OBJDESC_FREE_MASK) == 0)                           \

#define SM_OBJDESC_SET_FREE(od)         ((od)->flags = 0)

/******************************************************************************/

/* object is allocated -- non-gc state */

#define SM_OBJDESC_ALLOCATED_MASK       ((1 << SMObjectDescFlag_StateBit2) | \
                                         (1 << SMObjectDescFlag_StateBit1))

#define SM_OBJDESC_IS_ALLOCATED(od)                                       \
    (((od)->flags & SM_OBJDESC_ALLOCATED_MASK)                            \
     == SM_OBJDESC_ALLOCATED_MASK)                                        \

#define SM_OBJDESC_SET_ALLOCATED(od)                                      \
    (SM_ASSERT(SM_OBJDESC_IS_FREE(od)),                                   \
     ((od)->flags |= SMObjectState_Marked))                               \

/*******************************************************************************
 * Page Descriptors
 ******************************************************************************/

typedef struct SMPageDesc SMPageDesc;

struct SMPageDesc {
    SMPageDesc*         next;
    SMObjDesc*          objTable;
    SMSmallObjCount     allocCount;
    PRUint8             flags;
    SMObjDesc           largeObjDesc;
};

typedef enum SMPageFlag {
    SMPageFlag_GenBit0,
    SMPageFlag_GenBit1,
    SMPageFlag_GenBit2,
    SMPageFlag_BlackListedBit,
    SMPageFlag_BucketBit0,
    SMPageFlag_BucketBit1,
    SMPageFlag_BucketBit2,
    SMPageFlag_BucketBit3
} SMPageFlag;

#define SM_PAGEDESC_GEN_BITS            (SMPageFlag_GenBit2 + 1)
#define SM_PAGEDESC_GEN_MASK            ((1 << SM_PAGEDESC_GEN_BITS) - 1) 
#define SM_PAGEDESC_GEN(pd)             ((SMGenNum)((pd)->flags & SM_PAGEDESC_GEN_MASK))
#define SM_PAGEDESC_INCR_GEN(pd)        ((pd)->flags += 1)
#define SM_PAGEDESC_BUCKET(pd)          ((pd)->flags >> SMPageFlag_BucketBit0)

#define SM_PAGEDESC_BLACKLISTED_MASK    (1 << SMPageFlag_BlackListedBit)
#define SM_PAGEDESC_IS_BLACKLISTED(pd)  ((pd)->flags & SM_PAGEDESC_BLACKLISTED_MASK)
#define SM_PAGEDESC_SET_BLACKLISTED(pd) ((pd)->flags |= SM_PAGEDESC_BLACKLISTED_MASK)
#define SM_PAGEDESC_CLEAR_BLACKLISTED(pd) ((pd)->flags &= ~SM_PAGEDESC_BLACKLISTED_MASK)

#define SM_PAGEDESC_INIT(pd, bucket, genNum, objTab, allocCnt)     \
    ((pd)->next = NULL,                                            \
     (pd)->objTable = (objTab),                                    \
     (pd)->allocCount = (allocCnt),                                \
     (pd)->flags = ((bucket) << SMPageFlag_BucketBit0) | (genNum), \
     (pd)->largeObjDesc.flags = 0)                                 \

#define SM_PAGEDESC_BUCKET_BITS         4
#define SM_PAGEDESC_BUCKET_COUNT        (1 << SM_PAGEDESC_BUCKET_BITS)
#define SM_PAGEDESC_PAGENUM(pd)         (pd - sm_PageMgr.pageTable)
#define SM_PAGEDESC_PAGE(pd)            SM_PAGE_ADDR(SM_PAGEDESC_PAGENUM(pd))

/******************************************************************************/

#define SM_PAGEDESC_IS_LARGE_OBJECT_START(pd)   ((pd)->allocCount != 0)

/* The following only works if pd is not the start of a large object: */
#define SM_PAGEDESC_LARGE_OBJECT_START(pd)      ((pd)->next)

/*******************************************************************************
 * Page Manager
 ******************************************************************************/

typedef struct SMClusterDesc SMClusterDesc;
typedef struct SMSegmentDesc SMSegmentDesc;

typedef struct SMPageMgr {
    SMClusterDesc*      freeClusters;
    PRMonitor*          monitor;
    SMPage*             memoryBase;
    SMPage*             boundary;
    SMPageCount         minPage;
    SMPageCount         pageCount;
    PRBool              alreadyLocked;
#if defined(XP_MAC)
    PRUint8*            segMap;
    SMSegmentDesc*      segTable;
    PRWord              segTableCount;
#endif

    /* the page table */
    SMPageDesc*         pageTableMem;
    SMPageDesc*         pageTable;      /* one per page */
    SMPage*             heapBase;
    PRUword             heapPageCount;
} SMPageMgr;

#define SM_PAGEMGR_IN_RANGE(pm, ptr) \
    ((void*)(pm)->heapBase <= (ptr) && (ptr) < (void*)(pm)->boundary)

/* So here's the story: I was going to allow multiple heaps to exist
   simultaneously (not strictly necessary, but it might be a useful feature
   someday) but I backed it out because of the extra dereferences it introduces
   in order to get at any of the heap global data. If we ever need it, we can
   go back and introduce a heap argument to most of the routines, and eliminate
   this global. We'd also have to make SM_Init return a new heap. */
extern SMPageMgr sm_PageMgr;      /* _the_ global page manager */

/*******************************************************************************
 * Object Operations
 ******************************************************************************/

#define SM_OBJECT_PAGEDESC(obj)                                           \
    (&sm_PageMgr.pageTable[SM_PAGE_NUMBER(obj)])                          \

#define SM_OBJECT_HEADER_FROM_PAGEDESC(obj, pageDesc)                     \
    (&(pageDesc)->objTable[SM_DIV((SMSmallObjCount)SM_PAGE_OFFSET(obj),   \
                                  SM_PAGEDESC_BUCKET(pageDesc))])         \

#define SM_IN_HEAP(obj)                                                   \
    SM_PAGEMGR_IN_RANGE(&sm_PageMgr, (void*)SM_ENSURE(SMObjStruct, obj))  \

/*******************************************************************************
 * Functions
 ******************************************************************************/

extern PRStatus
sm_InitPageMgr(SMPageMgr* pm, SMPageCount minPages, SMPageCount maxPages);

extern void
sm_FiniPageMgr(SMPageMgr* pm);

extern SMPage*
sm_NewCluster(SMPageMgr* pm, SMPageCount nPages);

extern void
sm_DestroyCluster(SMPageMgr* pm, SMPage* basePage, SMPageCount nPages);

#define SM_NEW_PAGE(pm)                 sm_NewCluster((pm), 1)
#define SM_DESTROY_PAGE(pm, page)       sm_DestroyCluster((pm), (page), 1)

/******************************************************************************/

SM_END_EXTERN_C

#endif /* __SMPAGE__ */
/******************************************************************************/
