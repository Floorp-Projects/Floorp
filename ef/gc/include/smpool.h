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

#ifndef __SMPOOL__
#define __SMPOOL__

#include "sm.h"
#include "smpage.h"
#include "prmon.h"

SM_BEGIN_EXTERN_C

/*******************************************************************************
 * Allocation
 ******************************************************************************/

#define SM_MIN_SMALLOBJECT_BITS         3       /* 8 bytes */
#define SM_MIN_SMALLOBJECT_SIZE         (1 << SM_MIN_SMALLOBJECT_BITS)
#define SM_MAX_SMALLOBJECT_DENOM_BITS   2       /* one forth of a page */
#define SM_MAX_SMALLOBJECT_BITS         (SM_PAGE_BITS - SM_MAX_SMALLOBJECT_DENOM_BITS)
#define SM_MAX_SMALLOBJECT_SIZE         (1 << SM_MAX_SMALLOBJECT_BITS)

/* Number of allocation buckets needed for SM_MIN_SMALLOBJECT_SIZE <= 
   size <= SM_MAX_SMALLOBJECT_SIZE. The value is really:
     ln(SM_MAX_SMALLOBJECT_SIZE) - ln(SM_MIN_SMALLOBJECT_SIZE) + 1
       == SM_MAX_SMALLOBJECT_BITS - SM_MIN_SMALLOBJECT_BITS + 1
   Be careful to keep these all in sync. */
#define SM_POWER_OF_TWO_BUCKETS \
    (SM_MAX_SMALLOBJECT_BITS - SM_MIN_SMALLOBJECT_BITS + 1)

#define SM_MIDSIZE_BUCKETS      (SM_POWER_OF_TWO_BUCKETS - 1)
#define SM_FIRST_MIDSIZE_BUCKET SM_POWER_OF_TWO_BUCKETS
#define SM_LARGE_OBJECT_BUCKET  (SM_POWER_OF_TWO_BUCKETS + SM_MIDSIZE_BUCKETS)
#define SM_ALLOC_BUCKETS        (SM_LARGE_OBJECT_BUCKET + 1)

typedef PRUint8 SMBucket;

#ifdef IS_64
#error "fix me"
#else

/* Find the bin number for a given size (in bytes). This rounds
   up as values from (2^n)+1 to (2^(n+1)) share the same bin. */
#define SM_SIZE_BIN(bin, size)                          \
{                                                       \
    PRUword _t, _n = (PRUword)(size);                   \
    bin = 0;                                            \
    if ((_t = (_n >> 16)) != 0) { bin += 16; _n = _t; } \
    if ((_t = (_n >>  8)) != 0) { bin +=  8; _n = _t; } \
    if ((_t = (_n >>  4)) != 0) { bin +=  4; _n = _t; } \
    if ((_t = (_n >>  2)) != 0) { bin +=  2; _n = _t; } \
    if ((_t = (_n >>  1)) != 0) { bin +=  1; _n = _t; } \
    if (_n != 0) bin++;                                 \
}                                                       \

/* Returns the bucket for a given size. */
#define SM_GET_ALLOC_BUCKET(bucket, size)               \
{                                                       \
    PRUword _mid, _size = (PRUword)(size);              \
    if (_size <= SM_MIN_SMALLOBJECT_SIZE)               \
        bucket = 0;                                     \
    else if (_size > SM_MAX_SMALLOBJECT_SIZE)           \
        bucket = SM_LARGE_OBJECT_BUCKET;                \
    else {                                              \
        SM_SIZE_BIN(bucket, _size - 1);                 \
        _mid = (PRUword)(3 << (bucket - 2));            \
        if (_size <= _mid) {                            \
            bucket += SM_FIRST_MIDSIZE_BUCKET - 1;      \
        }                                               \
        bucket -= SM_MIN_SMALLOBJECT_BITS;              \
    }                                                   \
}                                                       \

#endif

/*******************************************************************************
 * Object Sizes and Counts
 ******************************************************************************/

/* Smallest type able to hold the size of a small object. Must be kept in 
   sync with SM_MAX_SMALLOBJECT_SIZE, above. */
#if SM_MAX_SMALLOBJECT_SIZE <= 256
typedef PRUint8 SMSmallObjSize;
#elif SM_MAX_SMALLOBJECT_SIZE <= 65536
typedef PRUint16 SMSmallObjSize;
#else
typedef PRUint32 SMSmallObjSize;
#endif

extern SMSmallObjSize  sm_ObjectSize[];
extern SMSmallObjCount sm_ObjectsPerPage[];

#define SM_OBJECT_GROSS_SIZE(sizeResult, obj)              \
{                                                          \
    SMPageCount _pageNum = SM_PAGE_NUMBER(obj);            \
    SMPageDesc* _pd = &sm_PageMgr.pageTable[_pageNum];     \
    SMBucket _bucket = SM_PAGEDESC_BUCKET(_pd);            \
    if (_bucket == SM_LARGE_OBJECT_BUCKET) {               \
        SM_ASSERT(SM_PAGEDESC_IS_LARGE_OBJECT_START(_pd)); \
        *(sizeResult) = SM_PAGE_WIDTH(_pd->allocCount);    \
    }                                                      \
    else {                                                 \
        SMBucket _bucket = SM_PAGEDESC_BUCKET(_pd);        \
        *(sizeResult) = sm_ObjectSize[_bucket];            \
    }                                                      \
}                                                          \

/*******************************************************************************
 * Fast Division
 ******************************************************************************/

#ifndef SM_NO_TABLE_DIVISION

/* Without SM_NO_TABLE_DIVISION defined, division is done by table lookup
   which looks the fastest. However it takes up the space of the table, and 
   could contribute to higher level-two cache miss rates for large working
   sets (we need to take some real world measurements). */

#define SM_DIVTABLE_ENTRY_COUNT (SM_MIDSIZE_BUCKETS * SM_PAGE_REFS + 1)

extern SMSmallObjCount* sm_ObjectSizeDivTable[SM_ALLOC_BUCKETS];

#define SM_DIV(offset, bucket)                                     \
    (((bucket) == SM_LARGE_OBJECT_BUCKET)                          \
     ? 0                                                           \
     : (((bucket) < SM_FIRST_MIDSIZE_BUCKET)                       \
        ? ((offset) >> ((bucket) + SM_MIN_SMALLOBJECT_BITS))       \
        : sm_ObjectSizeDivTable[bucket][(offset) >> SM_REF_BITS])) \

#else /* SM_NO_TABLE_DIVISION */

/* With SM_NO_TABLE_DIVISION defined, division is done by a fancy fixed-point
   algorithm that's hard-coded to divide by 3/2 times some power of 2. (Ask
   Waldemar if you want to know how it works.) It looks like it may be a little
   slower (although we have to measure it in the real-world) but it takes
   less space because there's no division table. */

#define SM_DIV_3_2(offset, bucket)                           \
    ((((offset) + 1) * 21845)                                \
     >> ((bucket) - SM_FIRST_MIDSIZE_BUCKET+ 18))            \

#define SM_DIV(offset, bucket)                               \
    (((bucket) == SM_LARGE_OBJECT_BUCKET)                    \
     ? 0                                                     \
     : (((bucket) < SM_FIRST_MIDSIZE_BUCKET)                 \
        ? ((offset) >> ((bucket) + SM_MIN_SMALLOBJECT_BITS)) \
        : SM_DIV_3_2(offset, bucket)))                       \

#endif /* SM_NO_TABLE_DIVISION */

/*******************************************************************************
 * Pools
 ******************************************************************************/

typedef struct SMSweepList {
    SMPageDesc*         sweepPage;
    SMSmallObjCount     sweepIndex;
    PRMonitor*          monitor;
} SMSweepList;

struct SMPool {
    SMSweepList         sweepList[SM_ALLOC_BUCKETS];
    PRUword             allocAmount;
#ifdef SM_STATS
    PRUword             totalAllocated;
    PRUword             totalRequested;
#endif
};

#ifdef SM_STATS

#define SM_POOL_SET_ALLOC_STATS(pool, allocated, requested) \
{                                                           \
    PRUword _alloc = (pool)->totalAllocated;                \
    (pool)->totalAllocated += (allocated);                  \
    (pool)->totalRequested += (requested);                  \
    SM_ASSERT(_alloc <= (pool)->totalAllocated);            \
}                                                           \

#else

#define SM_POOL_SET_ALLOC_STATS(pool, allocated, requested)

#endif

#define SM_POOL_DESTROY_PAGE(pool, pd)                      \
{                                                           \
    SMPageDesc* _pd = (pd);                                 \
    SMPageCount _pageNum = _pd - sm_PageMgr.pageTable;      \
    SMPage* _page = SM_PAGE_ADDR(_pageNum);                 \
    SMObjDesc* _objTable = _pd->objTable;                   \
    if (_objTable)                                          \
        SM_Free(_objTable);                                 \
    SM_DESTROY_PAGE(&sm_PageMgr, _page);                    \
}                                                           \

/******************************************************************************/

extern void
sm_InitAntiBuckets(void);

extern void
sm_InitFastDivision(void);

extern void*
sm_PoolInitPage(SMPool* pool, SMGenNum genNum, SMBucket bucket,
                SMPage* page, SMObjDesc* objTable, SMSmallObjCount allocIndex);

extern PRStatus
sm_InitPool(SMPool* pool);

extern void
sm_FiniPool(SMPool* pool);

extern void*
sm_PoolAllocObj(SMPool* pool, SMBucket bucket);

#define SM_POOL_FREE_OBJ(pool, obj)                                     \
{                                                                       \
    SMPool* _pool = (pool);                                             \
    void* _obj = (obj);                                                 \
    SMPageCount _pageNum = SM_PAGE_NUMBER(_obj);                        \
    SMSmallObjCount _objOffset = (SMSmallObjCount)SM_PAGE_OFFSET(_obj); \
    SMPageDesc* _pd = &sm_PageMgr.pageTable[_pageNum];                  \
    SMBucket _bucket = SM_PAGEDESC_BUCKET(_pd);                         \
    SMSmallObjSize _objIndex = SM_DIV(_objOffset, _bucket);             \
    SMObjDesc* _od = &_pd->objTable[_objIndex];                         \
    SMSmallObjCount _objsPerPage = sm_ObjectsPerPage[_bucket];          \
    SMSweepList* _sweepList = &_pool->sweepList[bucket];                \
    SM_ASSERT(!SM_OBJDESC_IS_FREE(_od));                                \
    SM_OBJDESC_SET_FREE(_od);                                           \
    if (_pd->allocCount-- == _objsPerPage) {                            \
        /* page was full -- add it to the front of the sweep list */    \
        SM_ASSERT(_pd->next == NULL);                                   \
        _pd->next = _sweepList->sweepPage;                              \
        _sweepList->sweepPage = _pd;                                    \
        _sweepList->sweepIndex = _objIndex;                             \
    }                                                                   \
    else if (_pd == _sweepList->sweepPage                               \
             && _objIndex < _sweepList->sweepIndex) {                   \
        /* if the object was on the first sweep page, and before */     \
        /* the sweep index, move the sweep index back */                \
        _sweepList->sweepIndex = _objIndex;                             \
    }                                                                   \
    else if (_pd->allocCount == 0) {                                    \
        sm_PoolRemovePage(_pool, _bucket, _pd);                         \
    }                                                                   \
    SM_POOL_VERIFY_PAGE(_pool, _bucket, _pd);                           \
}                                                                       \

/* used by malloc, not the gc */
extern void
sm_PoolRemovePage(SMPool* pool, SMBucket bucket, SMPageDesc* freePd);

extern void*
sm_PoolAllocLargeObj(SMPool* pool, SMGenNum genNum, PRUword size);

#define SM_POOL_FREE_LARGE_OBJ(pool, obj)                               \
{                                                                       \
    SMPage* _page = (SMPage*)(obj);                                     \
    SMPageDesc* _pd = SM_OBJECT_PAGEDESC(_page);                        \
    SM_ASSERT(SM_PAGEDESC_IS_LARGE_OBJECT_START(_pd));                  \
    sm_DestroyCluster(&sm_PageMgr, _page, _pd->allocCount);             \
}                                                                       \

typedef PRBool
(PR_CALLBACK* SMCompactProc)(PRUword spaceNeeded);

extern SMCompactProc sm_CompactHook;

/******************************************************************************/

#ifdef DEBUG

extern void
sm_PoolVerifyPage(SMPool* pool, SMBucket bucket, SMPageDesc* pd);

#define SM_POOL_VERIFY_PAGE(pool, bucket, pd) sm_PoolVerifyPage(pool, bucket, pd)

#else /* !DEBUG */

#define SM_POOL_VERIFY_PAGE(pool, bucket, pd) /* no-op */

#endif /* !DEBUG */

/******************************************************************************/

SM_END_EXTERN_C

#endif /* __SMPOOL__ */
/******************************************************************************/
