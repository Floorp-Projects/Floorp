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

#include "smheap.h"
#include <string.h>

PRUint8* SM_CardTable;
SMHeap   sm_Heap;      /* _the_ global heap */

/******************************************************************************/

static PRBool gcInitialized = PR_FALSE;

SM_IMPLEMENT(PRStatus)
SM_InitGC(PRUword initialHeapSize, PRUword maxHeapSize,
          SMGCHookProc beforeGCHook, void* beforeGCClosure,
          SMGCHookProc afterGCHook, void* afterGCClosure,
          SMMarkRootsProc markRootsProc, void* markRootsClosure)
{
    PRStatus status;
    PRUword i;
#ifndef SM_NO_WRITE_BARRIER
    PRUword cardTableSize;
#endif

    if (gcInitialized)
        return PR_SUCCESS;
    gcInitialized = PR_TRUE;
    
    /* Make sure the page descriptor generation field is big enough. */
    SM_ASSERT(SMGenNum_Free <= (1 << SM_PAGEDESC_GEN_BITS));
    
    memset(&sm_Heap, 0, sizeof(SMHeap));    /* in case we fail */

#ifndef SM_NO_WRITE_BARRIER
    cardTableSize = SM_PAGE_COUNT(initialHeapSize) * sizeof(SMCardDesc);
    initialHeapSize += cardTableSize;
#endif
    
    status = SM_InitMalloc(initialHeapSize, maxHeapSize);
    if (status != PR_SUCCESS)
        return status;
        
    /* Initialize generations */
    for (i = SMGenNum_Freshman; i <= SMGenNum_Static; i++) {
        status = sm_InitGen(&sm_Heap.gen[i], SM_DEFAULT_ALLOC_THRESHOLD);
        if (status != PR_SUCCESS) goto fail;
    }

    /* Initialize tables */
#ifndef SM_NO_WRITE_BARRIER
    sm_Heap.cardTablePageCount = SM_PAGE_COUNT(cardTableSize);
    sm_Heap.cardTableMem = (SMCardDesc*)sm_NewCluster(&sm_PageMgr,
                                                      sm_Heap.cardTablePageCount);
    if (sm_Heap.cardTableMem == NULL) goto fail;
    SM_CardTable = sm_Heap.cardTableMem - (SM_PAGE_NUMBER(sm_PageMgr.heapBase) * sizeof(SMCardDesc));
    for (i = 0; i < sm_PageMgr.heapPageCount; i++) {
        SMCardDesc* cd = &sm_Heap.cardTableMem[i];
        SM_CARDDESC_INIT(cd);
    }
#endif

    SM_INIT_STACK(&sm_Heap.markStack);

    sm_Heap.markRootsProc = markRootsProc;
    sm_Heap.markRootsClosure = markRootsClosure;
    sm_Heap.beforeGCHook = beforeGCHook;
    sm_Heap.beforeGCClosure = beforeGCClosure;
    sm_Heap.afterGCHook = afterGCHook;
    sm_Heap.afterGCClosure = afterGCClosure;
    sm_Heap.collectingGenNum = SMGenNum_Free;
    
    status = sm_InitFinalizer();
    if (status != PR_SUCCESS)
        goto fail;

    sm_CompactHook = sm_CollectAndFinalizeAll;
    
    return PR_SUCCESS;

  fail:
    SM_CleanupGC(PR_FALSE);
    return PR_FAILURE;
}

SM_IMPLEMENT(PRStatus)
SM_CleanupGC(PRBool finalizeOnExit)
{
    int i;
    PRStatus status;

    status = sm_FiniFinalizer(finalizeOnExit);
    /* do the following even if finalize-on-exit failed */

    sm_CompactHook = NULL;
    
    sm_DestroyStack(&sm_Heap.markStack);

    for (i = SMGenNum_Freshman; i <= SMGenNum_Static; i++) {
        sm_FiniGen(&sm_Heap.gen[i]);
    }
    
#ifndef SM_NO_WRITE_BARRIER
    sm_DestroyCluster(&sm_PageMgr, 
                      (SMPage*)sm_Heap.cardTableMem,
                      sm_Heap.cardTablePageCount);
    SM_CardTable = NULL;
#endif

    SM_CleanupMalloc();

    gcInitialized = PR_FALSE;
    return status;
}

#if !defined(SM_NO_WRITE_BARRIER) && defined(DEBUG)

SM_IMPLEMENT(void)
SM_DirtyPage(void* addr)
{
    SMCardDesc* cd = &SM_CardTable[SM_PAGE_NUMBER(addr)];
    SMCardDesc* cardTableLimit = (SMCardDesc*)
        ((char*)sm_Heap.cardTableMem + SM_PAGE_WIDTH(sm_Heap.cardTablePageCount));
    SM_ASSERT(sm_Heap.cardTableMem <= cd && cd < cardTableLimit);
    *cd = 0;
}

SM_IMPLEMENT(void)
SM_CleanPage(void* addr)
{
    SMCardDesc* cd = &SM_CardTable[SM_PAGE_NUMBER(addr)];
    SMCardDesc* cardTableLimit = (SMCardDesc*)
        ((char*)sm_Heap.cardTableMem + SM_PAGE_WIDTH(sm_Heap.cardTablePageCount));
    SM_ASSERT(sm_Heap.cardTableMem <= cd && cd < cardTableLimit);
    *cd = SMGenNum_Free + 1;
}

#endif /* !defined(SM_NO_WRITE_BARRIER) && defined(DEBUG) */

/******************************************************************************/

#ifdef SM_STATS

SM_IMPLEMENT(void)
SM_Stats(SMStats* stats)
{
    SMPageCount i;
    PRUword totalAmountRequested = 0, totalRequests = 0;

    memset(stats, 0, sizeof(SMStats));

    for (i = 0; i < sm_PageMgr.heapPageCount; i++) {
        SMPageDesc* pd = &sm_PageMgr.pageTableMem[i];
        SMGenNum genNum = SM_PAGEDESC_GEN(pd);
        if (genNum != SMGenNum_Free) {
            SMBucket bucket = SM_PAGEDESC_BUCKET(pd);
            if (bucket == SM_LARGE_OBJECT_BUCKET) {
                SMSmallObjSize objSize = pd->allocCount * SM_PAGE_SIZE;
                PRUword overhead = pd->allocCount * sizeof(SMPageDesc);
                stats->perGen[genNum].amountUsed += objSize;
                stats->perGen[genNum].overhead += overhead;
            }
            else {
                SMSmallObjSize objSize = sm_ObjectSize[bucket];
                SMSmallObjCount objsPerPage = sm_ObjectsPerPage[bucket];
                PRUword amountUsed = pd->allocCount * objSize;
                PRUword amountFree = (objsPerPage - pd->allocCount) * objSize;
                PRUword overhead = sizeof(SMPageDesc)
                    + objsPerPage       /* object table */
                    + SM_PAGE_SIZE - (objsPerPage * objSize); /* unused space at end of page */
                stats->perGen[genNum].amountFree += amountFree;
                stats->perGen[genNum].amountUsed += amountUsed;
                stats->perGen[genNum].overhead += overhead;
            }
        }
    }

    stats->systemOverhead =
        sizeof(SMHeap) +
        sm_PageMgr.heapPageCount * sizeof(SMPageDesc) +
#ifndef SM_NO_WRITE_BARRIER
        sm_PageMgr.heapPageCount * sizeof(SMCardDesc) +
#endif
        sizeof(SMSmallObjSize[SM_ALLOC_BUCKETS]) +         /* sm_ObjectSize */
        sizeof(SMSmallObjCount[SM_ALLOC_BUCKETS]) +        /* sm_ObjectsPerPage */
#ifndef SM_NO_TABLE_DIVISION
        sizeof(SMSmallObjCount[SM_DIVTABLE_ENTRY_COUNT]) + /* sm_ObjectSizeDivTableMem */
        sizeof(SMSmallObjCount*[SM_ALLOC_BUCKETS]) +       /* sm_ObjectSizeDivTable */
#endif
        sizeof(SMBucket[SM_ALLOC_BUCKETS - 1]);            /* sm_AntiBucket */

    stats->systemOverhead += sm_StackSize(&sm_Heap.markStack);

    for (i = SMGenNum_Freshman; i <= SMGenNum_Malloc; i++) {
        stats->total.amountFree += stats->perGen[i].amountFree;
        stats->total.amountUsed += stats->perGen[i].amountUsed;
        stats->total.overhead += stats->perGen[i].overhead;
        if (i < SMGenNum_Malloc)
            stats->perGen[i].collections = sm_Heap.gen[i].collectCount;
        stats->total.collections += stats->perGen[i].collections;

        if (i < SMGenNum_Malloc) {
            stats->total.totalAllocated +=
                stats->perGen[i].totalAllocated = sm_Heap.gen[i].pool.totalAllocated;
            stats->total.totalRequested +=
                stats->perGen[i].totalRequested = sm_Heap.gen[i].pool.totalRequested;
        }
    }
    stats->total.overhead += stats->systemOverhead;
}

static void
sm_DumpStat(FILE* out, char* name, PRUword used, PRUword free, PRUword over,
            PRUword totalAllocated, PRUword totalRequested, PRUword collections)
{
    PRUword total = used + free + over;
    fprintf(out, "  %-8s %10u (%2u%%) %10u (%2u%%) %10u (%2u%%) ",
            name,
            used, (total ? (used * 100 / total) : 0),
            free, (total ? (free * 100 / total) : 0),
            over, (total ? (over * 100 / total) : 0),
            collections);
    if (totalAllocated) {
        PRUword frag = (totalAllocated
                        ? (totalAllocated - totalRequested) * 100 / totalAllocated
                        : 0);
        fprintf(out, " %3u%%", frag);
    }
    else
        fprintf(out, "     ");
    if (collections)
        fprintf(out, "  %7u", collections);
    fprintf(out, "\n");
}

SM_IMPLEMENT(void)
SM_DumpStats(FILE* out, PRBool detailed)
{
    int i;
    char* genName[SM_GEN_COUNT] =
        { "  Gen 0", "  Gen 1", "  Gen 2", "  Gen 3", "  Static" };
    PRUword gcUsed = 0, gcFree = 0, gcOver = 0;
    PRUword gcTotalAllocated = 0, gcTotalRequested = 0;
    PRUword used, free, over, frag;
    SMStats stats;
    SM_Stats(&stats);
    fprintf(out, "--SportModel-Statistics-----------------------------------------------------\n");
    fprintf(out, "  SPACE          USED             FREE         OVERHEAD        FRAG  COLLECT\n");
    for (i = SMGenNum_Freshman; i < SMGenNum_Malloc; i++) {
        used = stats.perGen[i].amountUsed;
        free = stats.perGen[i].amountFree;
        over = stats.perGen[i].overhead;
        if (detailed) {
            sm_DumpStat(out, genName[i], used, free, over,
                        stats.perGen[i].totalAllocated, stats.perGen[i].totalRequested,
                        stats.perGen[i].collections);
        }
        gcUsed += used;
        gcFree += free;
        gcOver += over;
        gcTotalAllocated += stats.perGen[i].totalAllocated;
        gcTotalRequested += stats.perGen[i].totalRequested;
    }
    frag = (gcTotalAllocated
            ? (gcTotalAllocated - gcTotalRequested) * 100 / gcTotalAllocated
            : 0);
    sm_DumpStat(out, "GC Total", gcUsed, gcFree, gcOver,
                gcTotalAllocated, gcTotalRequested,
                stats.perGen[SMGenNum_Freshman].collections);
    used = stats.perGen[SMGenNum_Malloc].amountUsed;
    free = stats.perGen[SMGenNum_Malloc].amountFree;
    over = stats.perGen[SMGenNum_Malloc].overhead;
    sm_DumpStat(out, "Malloc", used, free, over,
                stats.perGen[SMGenNum_Malloc].totalAllocated,
                stats.perGen[SMGenNum_Malloc].totalRequested,
                0);
    used = stats.total.amountUsed;
    free = stats.total.amountFree;
    over = stats.total.overhead;
    if (detailed) {
        sm_DumpStat(out, "  System", 0, 0, stats.systemOverhead, 0, 0, 0);
    }
    sm_DumpStat(out, "TOTAL", used, free, over,
                gcTotalAllocated + stats.perGen[SMGenNum_Malloc].totalAllocated,
                gcTotalRequested + stats.perGen[SMGenNum_Malloc].totalRequested,
                0);
    fprintf(out, "----------------------------------------------------------------------------\n");
}

#endif /* SM_STATS */

/******************************************************************************/

#ifdef DEBUG

SM_IMPLEMENT(void)
SM_SetCollectThresholds(PRUword gen0Threshold, PRUword gen1Threshold,
                        PRUword gen2Threshold, PRUword gen3Threshold,
                        PRUword staticGenThreshold)
{
    if (gen0Threshold != 0)
        sm_Heap.gen[SMGenNum_Freshman].allocThreshold = gen0Threshold;
    if (gen1Threshold != 0)
        sm_Heap.gen[SMGenNum_Sophomore].allocThreshold = gen1Threshold;
    if (gen2Threshold != 0)
        sm_Heap.gen[SMGenNum_Junior].allocThreshold = gen2Threshold;
    if (gen3Threshold != 0)
        sm_Heap.gen[SMGenNum_Senior].allocThreshold = gen3Threshold;
    if (staticGenThreshold != 0)
        sm_Heap.gen[SMGenNum_Static].allocThreshold = staticGenThreshold;
}

#endif /* DEBUG */

#ifdef SM_VERIFY

#if SM_VERIFY == 2

static void
sm_VerifyRange(SMObject** refs, PRUword count)
{
    PRUword i;
    for (i = 0; i < count; i++) {
        SMObject* obj = refs[i];
        if (obj) {
            SMObjStruct* dobj = SM_DECRYPT_OBJECT(obj);
            SMPageDesc* pd = SM_OBJECT_PAGEDESC(dobj);
            SMObjDesc* od = SM_OBJECT_HEADER_FROM_PAGEDESC(dobj, pd);
            SM_ASSERT(SM_OBJDESC_IS_MARKED(od));
            SM_ASSERT(SM_OBJECT_CLASS(obj));
        }
    }
}

#endif

extern SMPool sm_MallocPool;

void
sm_VerifyHeap(void)
{
    SMGenNum genNum;
    SMPageCount i;

    //SM_ACQUIRE_MONITORS();
    //PR_SuspendAll();

    for (i = 0; i < sm_PageMgr.heapPageCount; i++) {
        SMPageDesc* pd = &sm_PageMgr.pageTableMem[i];
        genNum = SM_PAGEDESC_GEN(pd);
        if (genNum == SMGenNum_Free) {
            SM_ASSERT(pd->next == NULL);
            SM_ASSERT(pd->objTable == NULL);
            SM_ASSERT(pd->allocCount == 0);
            SM_ASSERT(pd->largeObjDesc.flags == 0);
        }
        else {
            SMBucket bucket = SM_PAGEDESC_BUCKET(pd);
            SMSmallObjCount objsPerPage = sm_ObjectsPerPage[bucket];
            SMPageDesc* pd2;
            SMSmallObjCount count = 0;
            SMSmallObjCount j;
            SMPool* pool = (genNum == SMGenNum_Malloc)
                ? &sm_MallocPool
                : &sm_Heap.gen[genNum].pool;

            if (bucket == SM_LARGE_OBJECT_BUCKET) {
                SM_ASSERT(pd->objTable == &pd->largeObjDesc);
                SM_ASSERT(SM_PAGEDESC_IS_LARGE_OBJECT_START(pd));
                SM_ASSERT(pd->allocCount != 0);
                pd2 = pd;
                count = pd->allocCount;
                while (--count > 0) {
                    i++;
                    pd2++;
                    SM_ASSERT(SM_PAGEDESC_GEN(pd2) == genNum);
                    SM_ASSERT(SM_PAGEDESC_BUCKET(pd2) == bucket);
                    SM_ASSERT(SM_PAGEDESC_LARGE_OBJECT_START(pd2) == pd);
                    SM_ASSERT(pd2->objTable == &pd->largeObjDesc);
                    SM_ASSERT(!SM_PAGEDESC_IS_LARGE_OBJECT_START(pd2));
                }
            }
            else {
                SMObjDesc* od = pd->objTable; 
                for (j = 0; j < objsPerPage; j++) {
                    if (SM_OBJDESC_IS_ALLOCATED(od)) {
                        SM_ASSERT((od->flags & SM_OBJDESC_STATE_MASK) == SMObjectState_Marked);
                        count++;
                    }
                    else {
                        SM_ASSERT(SM_OBJDESC_IS_FREE(od));
                    }
                    od++;
                }
                SM_ASSERT(pd->allocCount == count);
                pd2 = pool->sweepList[bucket].sweepPage;
                while (pd2 != NULL) {
                    if (pd2 == pd) {
                        SM_ASSERT(count <= objsPerPage);
                        goto next;
                    }
                    pd2 = pd2->next;
                }
                /* If we get here, there wasn't a pagedesc in the sweep list.
                   Figure out if it's somewhere else. */
                {
                    SMGenNum g;
                    for (g = SMGenNum_Freshman; g <= SMGenNum_Malloc; g++) {
                        SMBucket b;
                        for (b = 0; b < SM_ALLOC_BUCKETS; b++) {
                            SMPool* pool = (g == SMGenNum_Malloc)
                                ? &sm_MallocPool
                                : &sm_Heap.gen[g].pool;
                            SMPageDesc* pd2 = pool->sweepList[b].sweepPage;
                            while (pd2 != NULL) {
                                if (!(g == genNum && b == bucket)) {
                                    SM_ASSERT(pd != pd2);
                                }
                                pd2 = pd2->next;
                            }
                        }
                    }
                }
                /* If the page desc wasn't found anywhere, then it better be full. */
                SM_ASSERT(count == objsPerPage);
              next:;
                /* continue */
            }

#if SM_VERIFY == 2
            /* Verify that the gc objects on the page have all their fields marked */
            if (SM_IS_GC_SPACE(genNum)) {
                SMObjDesc* od = pd->objTable; 
                SMObjStruct* obj = (SMObjStruct*)SM_PAGEDESC_PAGE(pd);
                SMSmallObjSize objSize = sm_ObjectSize[bucket];
                for (j = 0; j < objsPerPage; j++) {
                    if (SM_OBJDESC_IS_MARKED(od)) {
                        SMClass* clazz = SM_OBJECT_CLASS(obj);
                        switch (SM_CLASS_KIND(clazz)) {
                          case SMClassKind_ObjectClass: {
                              SMFieldDesc* fieldDesc = SM_CLASS_GET_INST_FIELD_DESCS(clazz);
                              PRUint16 k, fdCount = SM_CLASS_GET_INST_FIELDS_COUNT(clazz);
                              for (k = 0; k < fdCount; k++) {
                                  PRWord offset = fieldDesc[k].offset; 
                                  PRUword count = fieldDesc[k].count; 
                                  SMObject** fieldRefs = &SM_OBJECT_FIELDS(obj)[offset];
                                  sm_VerifyRange(fieldRefs, count);
                              }
                              break;
                          }
                          case SMClassKind_ArrayClass: {
                              if (SM_IS_OBJECT_ARRAY_CLASS(clazz)) {
                                  PRUword size = SM_ARRAY_SIZE(obj);
                                  SMObject** elementRefs = SM_ARRAY_ELEMENTS(obj);
                                  sm_VerifyRange(elementRefs, size);
                              }
                              break;
                          }
                          default:
                            break;
                        }
                    }
                    obj = (SMObjStruct*)((char*)obj + objSize);
                    od++;
                }
            }
#endif
        }
    }

    /* also verify that all the sweep lists are consistent */
    for (genNum = SMGenNum_Freshman; genNum <= SMGenNum_Static; genNum++) {
        SMBucket bucket;
        for (bucket = 0; bucket < SM_ALLOC_BUCKETS; bucket++) {
            SMSweepList* sweepList = &sm_Heap.gen[genNum].pool.sweepList[bucket];
            SMPageDesc* pd = sweepList->sweepPage;
            SMSmallObjCount i;
            for (i = 0; i < sweepList->sweepIndex; i++) {
                SM_ASSERT(!SM_OBJDESC_IS_FREE(&pd->objTable[i]));
            }
            for (; pd != NULL; pd = pd->next) {
                SM_ASSERT(SM_PAGEDESC_BUCKET(pd) == bucket);
                SM_ASSERT(SM_PAGEDESC_GEN(pd) == genNum);
            }
        }
    }

    //SM_RELEASE_MONITORS();
    //PR_ResumeAll();
}

#endif /* SM_VERIFY */

/******************************************************************************/
