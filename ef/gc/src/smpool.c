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

#include "smpool.h"
#include <string.h>

SMSmallObjSize  sm_ObjectSize[SM_ALLOC_BUCKETS];
SMSmallObjCount sm_ObjectsPerPage[SM_ALLOC_BUCKETS];
SMCompactProc sm_CompactHook;

#ifndef SM_NO_TABLE_DIVISION
static SMSmallObjCount sm_ObjectSizeDivTableMem[SM_DIVTABLE_ENTRY_COUNT];
SMSmallObjCount* sm_ObjectSizeDivTable[SM_ALLOC_BUCKETS];
#endif

void
sm_InitFastDivision(void)
{
    SMBucket bucket;
    SMSmallObjSize objSize;
    SMSmallObjCount objsPerPage;
#ifndef SM_NO_TABLE_DIVISION
    SMSmallObjCount* fill = sm_ObjectSizeDivTableMem;
    int i;
#endif

    /* Make sure each entry in sm_ObjectSizeDivTableMem
       is big enough for the page size chosen. */
    SM_ASSERT(SM_PAGE_REFS <= SM_MAX_VALUE(SMSmallObjCount));

    objSize = SM_MIN_SMALLOBJECT_SIZE;
    for (bucket = 0; bucket < SM_POWER_OF_TWO_BUCKETS; bucket++) {
        sm_ObjectSize[bucket] = objSize;
        objsPerPage = SM_PAGE_SIZE / objSize;
        sm_ObjectsPerPage[bucket] = objsPerPage;

#ifndef SM_NO_TABLE_DIVISION
        /* We don't use sm_ObjectSizeDivTable for power-of-two
           sized buckets -- we'll shift instead (in the SM_DIV
           macro). */
        sm_ObjectSizeDivTable[bucket] = NULL;
#endif
        objSize <<= 1;
    }
    objSize = (SM_MIN_SMALLOBJECT_SIZE * 3) / 2;
    for (; bucket < SM_ALLOC_BUCKETS - 1; bucket++) {
        sm_ObjectSize[bucket] = objSize;
        objsPerPage = SM_PAGE_SIZE / objSize;
        sm_ObjectsPerPage[bucket] = objsPerPage;
        
#ifndef SM_NO_TABLE_DIVISION
        sm_ObjectSizeDivTable[bucket] = fill;
        for (i = 0; i < SM_PAGE_REFS; i++) {
            *fill++ = (i << SM_REF_BITS) / objSize;            
        }
#endif
        objSize <<= 1;
    }
    sm_ObjectSize[SM_LARGE_OBJECT_BUCKET] = 0;
    sm_ObjectsPerPage[SM_LARGE_OBJECT_BUCKET] = 1;

#ifndef SM_NO_TABLE_DIVISION
    /* set up the last entry in the div table to handle large objects */
    sm_ObjectSizeDivTable[bucket] = fill;
    *fill++ = 0;
#endif
    
#ifdef DEBUG
    /* Make sure our fancy division stuff is working. */
    for (bucket = 0; bucket < SM_ALLOC_BUCKETS - 1; bucket++) {
        int i;
        for (i = 0; i < SM_PAGE_REFS; i++) {
            PRUword obj = i << SM_REF_BITS;
            PRUword objSize = sm_ObjectSize[bucket];
            SM_ASSERT(SM_DIV(obj, bucket) == obj / objSize);
        }
    }
    /* Make sure it works for large objects too */
    {
        int i;
        for (i = SM_PAGE_SIZE - 8; i < SM_PAGE_SIZE + 8; i++) {
            PRUword obj = i;
            PRUword value = SM_DIV(obj, SM_LARGE_OBJECT_BUCKET);
            SM_ASSERT(value == 0);
        }
    }
#endif
}

/******************************************************************************/

void*
sm_PoolInitPage(SMPool* pool, SMGenNum genNum, SMBucket bucket,
                SMPage* page, SMObjDesc* objTable, SMSmallObjCount allocIndex)
{
    void* obj = NULL;
    SMPageCount pageNum = SM_PAGE_NUMBER(page);
    SMPageDesc* pd = &sm_PageMgr.pageTable[pageNum];
    SMSweepList* sweepList = &pool->sweepList[bucket];
    SMSmallObjSize objSize = sm_ObjectSize[bucket];
    SMSmallObjCount objsPerPage = sm_ObjectsPerPage[bucket];
    SMSmallObjCount i;
    SMObjDesc* od;

    SM_PAGEDESC_INIT(pd, bucket, genNum, objTable, allocIndex + 1);
    if (bucket >= SM_FIRST_MIDSIZE_BUCKET) {
        /* If we are dealing with inbetween sizes, then we must increment
           the number of objects per page to account for the extra unused 
           space at the end of the page. We need an object descriptor for 
           this in case a conservative pointer points to the space there. */
        objsPerPage++;
    }
    od = objTable;
    SM_UNROLLED_WHILE(objsPerPage, {
        SM_OBJDESC_SET_FREE(od++);
    });
    for (i = 0; i <= allocIndex; i++) {
        SM_OBJDESC_SET_ALLOCATED(&objTable[i]);
    }
    pd->allocCount = allocIndex + 1;
    
    /* link page into free list */
    SM_ASSERT(pd->next == NULL);
    pd->next = sweepList->sweepPage;
    sweepList->sweepPage = pd;
    sweepList->sweepIndex = allocIndex + 1;

    obj = (void*)((char*)page + allocIndex * objSize);
    return obj;
}

/******************************************************************************/

PRStatus
sm_InitPool(SMPool* pool)
{
    int i;

    memset(pool, 0, sizeof(pool));	/* in case we fail */
    for (i = 0; i < SM_ALLOC_BUCKETS; i++) {
        SMSweepList* sweepList = &pool->sweepList[i];
        PRMonitor* monitor = PR_NewMonitor();
        if (monitor == NULL) {
            sm_FiniPool(pool);
            return PR_FAILURE;
        }
        sweepList->monitor = monitor;
        sweepList->sweepPage = NULL;
        sweepList->sweepIndex = 0;
    }
    pool->allocAmount = 0;
    return PR_SUCCESS;
}

void
sm_FiniPool(SMPool* pool)
{
    int i;
    for (i = 0; i < SM_ALLOC_BUCKETS; i++) {
        SMSweepList* sweepList = &pool->sweepList[i];
        if (sweepList->monitor) {
            PR_DestroyMonitor(sweepList->monitor);
            sweepList->monitor = NULL;
            sweepList->sweepPage = NULL;
            sweepList->sweepIndex = 0;
        }
    }
    pool->allocAmount = 0;
}

/******************************************************************************/

void*
sm_PoolAllocObj(SMPool* pool, SMBucket bucket)
{
    SMSweepList* sweepList = &pool->sweepList[bucket];
    SMSmallObjCount objsPerPage = sm_ObjectsPerPage[bucket];
    SMPageDesc* pd;

    /*SM_ASSERT(PR_InMonitor(sweepList->monitor)); XXX not when called during gc */
    
    /* pages on the sweep list should never be large */
    SM_ASSERT(bucket != SM_LARGE_OBJECT_BUCKET);
        
    /* lazy sweep -- start from where we left off, and only go until
       we find what we need */
    while ((pd = sweepList->sweepPage) != NULL) {
        SM_ASSERT(SM_PAGEDESC_BUCKET(pd) == bucket);
        SM_ASSERT(pd->allocCount != 0);
        while (sweepList->sweepIndex < objsPerPage) {
            SMSmallObjCount i = sweepList->sweepIndex++;
            SMObjDesc* od = &pd->objTable[i];
            if (SM_OBJDESC_IS_FREE(od)) {
                SMSmallObjSize objSize = sm_ObjectSize[bucket];
                void* obj = ((char*)SM_PAGEDESC_PAGE(pd) + i * objSize);
                SM_OBJDESC_SET_ALLOCATED(od);
                if (++pd->allocCount == objsPerPage) {
                    /* if this allocation caused this page to become
                       full, remove the page from the sweep list */
                    sweepList->sweepPage = pd->next;
                    pd->next = NULL;
                    sweepList->sweepIndex = 0;
                }
                SM_POOL_VERIFY_PAGE(pool, bucket, pd);
                return obj;
            }
        }
        sweepList->sweepPage = pd->next;
        pd->next = NULL;
        sweepList->sweepIndex = 0;
    }
    return NULL;
}

void
sm_PoolRemovePage(SMPool* pool, SMBucket bucket, SMPageDesc* freePd)
{
    SMPageDesc** pd2Addr = &pool->sweepList[bucket].sweepPage;
    SMPage* page = SM_PAGEDESC_PAGE(freePd);
    SMPageDesc* pd2;

    while ((pd2 = *pd2Addr) != NULL) {
        if (pd2 == freePd) {
            *pd2Addr = pd2->next;
            SM_POOL_DESTROY_PAGE(pool, freePd);
            SM_ASSERT(freePd->allocCount == 0);
            SM_ASSERT(SM_PAGEDESC_GEN(freePd) == SMGenNum_Free);
            return;
        }
        else {
            pd2Addr = &pd2->next;
        }
    }
    SM_ASSERT(0);
}

/******************************************************************************/

#define SM_MAX_SMALL_OBJECTS_PER_PAGE (1 << (sizeof(SMSmallObjCount) * 8))
#define SM_MAX_PAGE_COUNT             (((PRUword)-1) / SM_PAGE_SIZE)

void*
sm_PoolAllocLargeObj(SMPool* pool, SMGenNum genNum, PRUword size)
{
    SMPageCount nPages, i;
    SMPage* obj;
    
    SM_ASSERT(size > SM_MAX_SMALLOBJECT_SIZE);
    
    nPages = SM_PAGE_COUNT(size);
    /* depending on compile-time values, this whole test might go away */
    if (SM_MAX_PAGE_COUNT > SM_MAX_SMALL_OBJECTS_PER_PAGE) {
        if (nPages > SM_MAX_SMALL_OBJECTS_PER_PAGE) {
            /* must fit in allocCount field -- for 32 bits, this allows
               objects up to 256M in size (4096 * 2^16) */
            return NULL;
        }
    }
    obj = sm_NewCluster(&sm_PageMgr, nPages);
    if (obj) {
        SMPageDesc* pd = SM_OBJECT_PAGEDESC(obj);
        SMObjDesc* objTable = &pd->largeObjDesc;
        SMPageDesc* pd1 = pd + 1;
        for (i = 1; i < nPages; i++) {
            SM_PAGEDESC_INIT(pd1, SM_LARGE_OBJECT_BUCKET, genNum, objTable, 0);
            pd1->next = pd;     /* the next field points all pages back to the head */
            pd1->allocCount = 0;        /* subsequent pages have allocCount == 0 */
            pd1++;
        }
        SM_PAGEDESC_INIT(pd, SM_LARGE_OBJECT_BUCKET, genNum, objTable, 0);
        pd->allocCount = (SMSmallObjCount)nPages;
        SM_OBJDESC_SET_ALLOCATED(objTable);
        pd->next = pool->sweepList[SM_LARGE_OBJECT_BUCKET].sweepPage;
        pool->sweepList[SM_LARGE_OBJECT_BUCKET].sweepPage = pd;
    }
    return obj;
}

/******************************************************************************/

SMBucket sm_AntiBucket[SM_ALLOC_BUCKETS - 1];
SMPool* sm_MallocPool = NULL;

void
sm_InitAntiBuckets(void)
{
    SMBucket i;

    SM_ASSERT(SM_PAGEDESC_BUCKET_COUNT <= SM_MAX_VALUE(SMBucket));
    SM_ASSERT(SM_ALLOC_BUCKETS <= SM_PAGEDESC_BUCKET_COUNT);

    for (i = 0; i < SM_ALLOC_BUCKETS - 1; i++) {
        SMSmallObjSize size = sm_ObjectSize[i];
        SMSmallObjSize antiSize = SM_PAGE_SIZE / size;
        SMBucket antiBucket;
        if (i >= SM_FIRST_MIDSIZE_BUCKET)
            antiSize++; /* add one for non-integral number of objects per page */
        SM_GET_ALLOC_BUCKET(antiBucket, antiSize);
        sm_AntiBucket[i] = antiBucket;
    }
}

/******************************************************************************/

static PRBool mallocInitialized = PR_FALSE;
static SMPool sm_StaticMallocPool;

SM_IMPLEMENT(PRStatus)
SM_InitMalloc(PRUword initialHeapSize, PRUword maxHeapSize)
{
    PRStatus status;

    if (sm_MallocPool)
        return PR_SUCCESS;
    sm_MallocPool = &sm_StaticMallocPool;
    
    /* Make sure we don't need more size buckets than we allotted for. */
    SM_ASSERT(SM_ALLOC_BUCKETS <= SM_PAGEDESC_BUCKET_COUNT);
    
    sm_InitFastDivision();
    sm_InitAntiBuckets();

    status = sm_InitPageMgr(&sm_PageMgr,
                            SM_PAGE_COUNT(initialHeapSize),
                            SM_PAGE_COUNT(maxHeapSize));
    if (status != PR_SUCCESS) return status;

    return sm_InitPool(sm_MallocPool);
}

SM_IMPLEMENT(PRStatus)
SM_CleanupMalloc(void)
{
    sm_FiniPool(sm_MallocPool);
    sm_FiniPageMgr(&sm_PageMgr);
    sm_MallocPool = NULL;
    return PR_SUCCESS;
}

/******************************************************************************/

/* We add a pair of pages simultaneously to malloc space
   so that they can hold each other's object descriptors. */
static void*
sm_AddMallocPagePair(SMBucket bucket)
{
    void* obj = NULL;
    SMSmallObjCount allocIndex = 0;
    SMPage* page = SM_NEW_PAGE(&sm_PageMgr);
    if (page) {
        SMBucket antiBucket = sm_AntiBucket[bucket];
        SMObjDesc* objTable = (SMObjDesc*)
            sm_PoolAllocObj(sm_MallocPool, antiBucket);
        if (objTable == NULL) {
            /* allocate an anti-page too */
            SMObjDesc* antiObjTable;
            SMPage* antiPage = SM_NEW_PAGE(&sm_PageMgr);
            if (antiPage == NULL) {
                /* can't get the anti-page */
                SM_DESTROY_PAGE(&sm_PageMgr, page);
                return NULL;
            }
            antiObjTable = (SMObjDesc*)page;
            objTable = (SMObjDesc*)
                sm_PoolInitPage(sm_MallocPool, SMGenNum_Malloc, antiBucket, 
                                antiPage, antiObjTable, allocIndex++);
        }
        obj = sm_PoolInitPage(sm_MallocPool, SMGenNum_Malloc, bucket,
                              page, objTable, allocIndex);
    }
    return obj;
}

/*******************************************************************************
 * Memory Pool variation of Malloc and Friends
 ******************************************************************************/

SM_IMPLEMENT(SMPool*)
SM_NewPool(void)
{
    SMPool* pool;
    SM_ASSERT(sm_MallocPool);
    pool = (SMPool*)SM_Malloc(sizeof(SMPool));
    if (pool == NULL) return NULL;
    sm_InitPool(pool);
    return pool;
}

SM_IMPLEMENT(void)
SM_DeletePool(SMPool* pool)
{
    if (pool) {
        sm_FiniPool(pool);
        SM_Free(pool);
    }
}

SM_IMPLEMENT(void*)
SM_PoolMalloc(SMPool* pool, PRUword size)
{
    void* result;
    SMBucket bucket;
    PRMonitor* mon;
    PRUword allocSize;
    SM_GET_ALLOC_BUCKET(bucket, size);

    SM_ASSERT(pool);
    mon = pool->sweepList[bucket].monitor;
    if (!sm_PageMgr.alreadyLocked) /* XXX is this right? */
        PR_EnterMonitor(mon);
    
    if (bucket == SM_LARGE_OBJECT_BUCKET) {
        result = sm_PoolAllocLargeObj(pool, SMGenNum_Malloc, size);
        /* XXX do a gc if we fail? */
        allocSize = SM_PAGE_COUNT(size) * SM_PAGE_SIZE;
    }
    else {
        do {
            result = sm_PoolAllocObj(pool, bucket);
            allocSize = sm_ObjectSize[bucket];
            if (result == NULL) {
                result = sm_AddMallocPagePair(bucket);
            }
        } while (result == NULL &&
                 (sm_CompactHook ? sm_CompactHook(allocSize) : PR_FALSE));
        /* XXX need to account for allocAmount due to page pair */
    }

    if (result) {
        SM_POOL_SET_ALLOC_STATS(pool, allocSize, size);
        pool->allocAmount += allocSize;
        DBG_MEMSET(result, SM_MALLOC_PATTERN, size);
        DBG_MEMSET((char*)result + size, SM_UNUSED_PATTERN,
                   allocSize - size);
    }

    if (!sm_PageMgr.alreadyLocked) /* XXX is this right? */
        PR_ExitMonitor(mon);
    return result;
}

SM_IMPLEMENT(void)
SM_PoolFree(SMPool* pool, void* ptr)
{
    SMPageCount pageNum = SM_PAGE_NUMBER(ptr);
    SMPageDesc* pd = &sm_PageMgr.pageTable[pageNum];
    SMBucket bucket = SM_PAGEDESC_BUCKET(pd);
    SMSweepList* sweepList;
    PRMonitor* mon;
    PRUword allocSize;
    
    SM_ASSERT(pool);
    SM_ASSERT(ptr);
    if (ptr == NULL)
        return;

    sweepList = &pool->sweepList[bucket];
    mon = sweepList->monitor;
    DBG_MEMSET(ptr, SM_FREE_PATTERN, sm_ObjectSize[bucket]);

    if (!sm_PageMgr.alreadyLocked) /* XXX is this right? */
        PR_EnterMonitor(mon);

    if (bucket == SM_LARGE_OBJECT_BUCKET) {
        SMPageDesc** pdAddr = &sweepList->sweepPage;
        SMPageDesc* pd2;
        while ((pd2 = *pdAddr) != NULL) {
            if (pd2 == pd) {
                *pdAddr = pd2->next;
                goto found;
            }
            pdAddr = &pd2->next;
        }
        SM_ASSERT(0);
      found:
        SM_POOL_FREE_LARGE_OBJ(pool, ptr);
        allocSize = pd->allocCount * SM_PAGE_SIZE;
    }
    else {
        SM_POOL_FREE_OBJ(pool, ptr);
        if (pd->allocCount == 1) {
            SMPageCount objTabPageNum = SM_PAGE_NUMBER(pd->objTable);
            SMPageDesc* objTabPd = &sm_PageMgr.pageTable[objTabPageNum];
            if (objTabPd->allocCount == 1
                && SM_PAGE_NUMBER(objTabPd->objTable) == pageNum) {
                /* Then we found a malloc page pair -- free them both,
                   but first break the malloc pair cycle */
                void* ot1 = pd->objTable;
                void* ot2 = objTabPd->objTable;
                
                pd->objTable = NULL;
                objTabPd->objTable = NULL;
                sm_PoolRemovePage(pool, bucket, pd);
                sm_PoolRemovePage(pool, sm_AntiBucket[bucket], objTabPd);
                /* XXX need to account for allocAmount due to page pair */
            }
        }
        allocSize = sm_ObjectSize[bucket];
    }

    pool->allocAmount -= allocSize;

    if (!sm_PageMgr.alreadyLocked) /* XXX is this right? */
        PR_ExitMonitor(mon);
}

SM_IMPLEMENT(void*)
SM_PoolCalloc(SMPool* pool, PRUword size, PRUword count)
{
    PRUword fullSize = size * count;
    void* result = SM_PoolMalloc(pool, fullSize);
    if (result == NULL)
        return NULL;
    memset(result, 0, fullSize);
    return result;
}

SM_IMPLEMENT(void*)
SM_PoolRealloc(SMPool* pool, void* ptr, PRUword size)
{
    PRUword curSize;
    PRWord diff;
    void* result;
    
    SM_OBJECT_GROSS_SIZE(&curSize, ptr);
    result = SM_PoolMalloc(pool, size);
    if (result == NULL)
        return NULL;
    memcpy(result, ptr, curSize);
    diff = size - curSize;
    if (diff > 0)
        memset((char*)result + curSize, 0, diff);
    SM_PoolFree(pool, ptr);
    return result;
}

SM_IMPLEMENT(void*)
SM_PoolStrdup(SMPool* pool, void* str)
{
    PRUword curSize;
    SM_OBJECT_GROSS_SIZE(&curSize, str);
    return SM_PoolStrndup(pool, str, curSize);
}

SM_IMPLEMENT(void*)
SM_PoolStrndup(SMPool* pool, void* str, PRUword size)
{
    void* result = SM_PoolMalloc(pool, size);
    if (result == NULL) return NULL;
    memcpy(result, str, size);
    return result;
}

/*******************************************************************************
 * Malloc and Friends
 ******************************************************************************/

SM_IMPLEMENT(void*)
SM_Malloc(PRUword size)
{
    return SM_PoolMalloc(sm_MallocPool, size);
}

SM_IMPLEMENT(void)
SM_Free(void* ptr)
{
    SM_PoolFree(sm_MallocPool, ptr);
}

SM_IMPLEMENT(void*)
SM_Calloc(PRUword size, PRUword count)
{
    return SM_PoolCalloc(sm_MallocPool, size, count);
}

SM_IMPLEMENT(void*)
SM_Realloc(void* ptr, PRUword size)
{
    return SM_PoolRealloc(sm_MallocPool, ptr, size);
}

SM_IMPLEMENT(void*)
SM_Strdup(void* str)
{
    return SM_PoolStrdup(sm_MallocPool, str);
}

SM_IMPLEMENT(void*)
SM_Strndup(void* str, PRUword size)
{
    return SM_PoolStrndup(sm_MallocPool, str, size);
}

/******************************************************************************/

#ifdef DEBUG

#include "smheap.h"

void
sm_PoolVerifyPage(SMPool* pool, SMBucket bucket, SMPageDesc* pd)
{
    SMSmallObjCount objsPerPage = sm_ObjectsPerPage[bucket];
    SMSweepList* sweepList = &pool->sweepList[bucket];
    SMPageDesc* pd2 = sweepList->sweepPage;
    while (pd2 != NULL) {
        if (pd2 == pd) {
            int j, cnt = 0;
            SMObjDesc* objTable = pd->objTable;
            SMGenNum genNum = SM_PAGEDESC_GEN(pd);
            SMPool* pool2 = (genNum == SMGenNum_Malloc)
                ? sm_MallocPool
                : &sm_Heap.gen[genNum].pool;
            SM_ASSERT(pool == pool2);
            SM_ASSERT(SM_PAGEDESC_BUCKET(pd) == bucket);
            for (j = 0; j < objsPerPage; j++) {
                if (SM_OBJDESC_IS_ALLOCATED(&objTable[j]))
                    cnt++;
            }
            SM_ASSERT(0 < cnt && cnt < objsPerPage);
            SM_ASSERT(cnt == pd->allocCount);
            return;
        }
        pd2 = pd2->next;
    }
    SM_ASSERT(pd->allocCount == 0 || pd->allocCount == objsPerPage);
}

#endif /* DEBUG */

/******************************************************************************/
