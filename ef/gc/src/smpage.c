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

#include "smpage.h"
#include "sm.h"

#if defined(XP_PC)
#include <windows.h>
#elif defined(XP_MAC)
#include <Types.h>
#include <Memory.h>
#include <stdlib.h>
#elif defined(XP_UNIX)
#include <sys/mman.h>
#include <sys/fcntl.h>
#endif

SMPageMgr sm_PageMgr;      /* _the_ global page manager */

/******************************************************************************/

struct SMClusterDesc {
    SMClusterDesc*      next;   /* Link to next cluster of free pages */
    SMPage*             addr;   /* First page in cluster of free pages */
    SMPageCount         nPages; /* Total size of cluster of free pages in bytes */
};

/******************************************************************************/

static SMClusterDesc* unusedClusterDescs;

#define SM_CLUSTERDESC_CLUMPSIZE    1

static void
sm_DeleteFreeClusterDesc(SMPageMgr* pm, SMClusterDesc *desc)
{
#if 1
    desc->next = unusedClusterDescs;
    unusedClusterDescs = desc;
#else
    SM_Free(desc);
#endif
}

static SMClusterDesc*
sm_NewFreeClusterDesc(SMPageMgr* pm)
{
    SMClusterDesc *desc = unusedClusterDescs;
    if (desc)
        unusedClusterDescs = desc->next;
    else {
        /* Allocate a clump of cluster records at once, and link all except
           the first onto the list of unusedClusterDescs */
        desc = (SMClusterDesc*)SM_Malloc(SM_CLUSTERDESC_CLUMPSIZE * sizeof(SMClusterDesc));
        if (desc) {
            SMClusterDesc* desc2 = desc + (SM_CLUSTERDESC_CLUMPSIZE - 1);
            while (desc2 != desc) {
                sm_DeleteFreeClusterDesc(pm, desc2--);
            }
        }
    }
    return desc;
}

/* Search the freeClusters looking for the first cluster of consecutive free
   pages that is at least size bytes long.  If there is one, remove these pages
   from the free page list and return their address; if not, return nil. */
static SMPage*
sm_AllocClusterFromFreeList(SMPageMgr* pm, PRUword nPages)
{
    SMClusterDesc **p = &pm->freeClusters;
    SMClusterDesc *desc;
    while ((desc = *p) != NULL) {
        if (desc->nPages >= nPages) {
            SMPage* addr = desc->addr;
            if (desc->nPages == nPages) {
                *p = desc->next;
                sm_DeleteFreeClusterDesc(pm, desc);
            }
            else {
                desc->addr += nPages;
                desc->nPages -= nPages;
            }
            return addr;
        }
        p = &desc->next;
    }
    return NULL;
}

/* Add the segment to the SMClusterDesc list, coalescing it with any
   clusters already in the list when possible. */
static void
sm_AddClusterToFreeList(SMPageMgr* pm, SMPage* addr, PRWord nPages)
{
    SMClusterDesc **p = &pm->freeClusters;
    SMClusterDesc *desc;
    SMClusterDesc *newDesc;

    while ((desc = *p) != NULL) {
        if (desc->addr + desc->nPages == addr) {
            /* Coalesce with the previous cluster. */
            SMClusterDesc *next = desc->next;

            desc->nPages += nPages;
            if (next && next->addr == addr + nPages) {
                /* We can coalesce with both the previous and the next cluster. */
                desc->nPages += next->nPages;
                desc->next = next->next;
                sm_DeleteFreeClusterDesc(pm, next);
            }
            return;
        }
        if (desc->addr == addr + nPages) {
            /* Coalesce with the next cluster. */
            desc->addr -= nPages;
            desc->nPages += nPages;
            return;
        }
        if (desc->addr > addr) {
            PR_ASSERT(desc->addr > addr + nPages);
            break;
        }
        PR_ASSERT(desc->addr + desc->nPages < addr);
        p = &desc->next;
    }
    newDesc = sm_NewFreeClusterDesc(pm);
    /* In the unlikely event that this malloc fails, we drop the free cluster
       on the floor. The only consequence is that the memory mapping table
       becomes slightly larger. */
    if (newDesc) {
        newDesc->next = desc;
        newDesc->addr = addr;
        newDesc->nPages = nPages;
        *p = newDesc;
    }
}

#ifdef xDEBUG

#include <stdio.h>

#ifndef XP_PC
#define OutputDebugString(x)    puts(x)
#endif

static void
sm_VerifyClusters(SMPageMgr* pm, SMPageCount nPagesDelta)
{
    static PRWord expectedPagesUsed = 0;
    SMPageCount calculatedPagesUsed;
    SMPage* lastDescEnd = 0;
    SMClusterDesc* desc;
    char str[256];
    expectedPagesUsed += nPagesDelta;
    calculatedPagesUsed = pm->boundary - pm->memoryBase;
    sprintf(str, "[Clusters: %p", pm->memoryBase);
    OutputDebugString(str);
    for (desc = pm->freeClusters; desc; desc = desc->next) {
        PR_ASSERT(desc->addr > lastDescEnd);
        calculatedPagesUsed -= desc->nPages;
        lastDescEnd = desc->addr + desc->nPages;
        sprintf(str, "..%p, %p", desc->addr-1, desc->addr + desc->nPages);
        OutputDebugString(str);
    }
    sprintf(str, "..%p]\n", pm->boundary);
    OutputDebugString(str);
    PR_ASSERT(lastDescEnd < pm->boundary);
    PR_ASSERT(calculatedPagesUsed == expectedPagesUsed);
}

#define SM_VERIFYCLUSTERS(pm, nPagesDelta) sm_VerifyClusters(pm, nPagesDelta)

#else /* !DEBUG */

#define SM_VERIFYCLUSTERS(pm, nPagesDelta)       /* no-op */

#endif /* !DEBUG */

/*******************************************************************************
 * Machine-dependent stuff
 ******************************************************************************/
 
#if defined(XP_PC)

#define GC_VMBASE               0x40000000      /* XXX move */
#define GC_VMLIMIT              0x0FFFFFFF

#elif defined(XP_MAC)

#define SM_MAC_SEGMENT_SIZE     
#define SM_MAC_SEGMENT_COUNT    

struct SMSegmentDesc {
    Handle              handle;
    SMPage*             firstPage;
    SMPage*             lastPage;
};
    
#endif

static PRStatus
sm_InitPages(SMPageMgr* pm, SMPageCount minPages, SMPageCount maxPages)
{
#if defined(XP_PC)

    SMPage* addr = NULL;
    SMPageCount size = maxPages;

#ifdef DEBUG
    /* first try to place the heap at a well-known address for debugging */
    addr = (SMPage*)VirtualAlloc((void*)GC_VMBASE, size << SM_PAGE_BITS,
                                 MEM_RESERVE, PAGE_READWRITE);
#endif
    while (addr == NULL) {
        /* let the system place the heap */
        addr = (SMPage*)VirtualAlloc(0, size << SM_PAGE_BITS,
                                     MEM_RESERVE, PAGE_READWRITE);
        if (addr == NULL) {
            size--;
            if (size < minPages) {
                return PR_FAILURE;
            }
        }
    }
    SM_ASSERT(SM_IS_ALIGNED(addr, SM_PAGE_BITS));
    pm->memoryBase = addr;
    pm->pageCount = size;
    pm->boundary = addr;
    pm->minPage = SM_PAGE_NUMBER(addr);

    return PR_SUCCESS;

#elif defined(XP_MAC)
    
    OSErr err;
    void* seg;
    void* segLimit;
    Handle h;
    PRUword segSize = (minPages + 1) * SM_PAGE_SIZE;
    SMPage* firstPage;
    SMPage* lastPage;
    SMSegmentDesc* segTable;
    int segTableCount, otherCount;

    h = TempNewHandle(segSize, &err);
    if (err || h == NULL) goto fail;
    MoveHHi(h);
    TempHLock(h, &err);
    if (err) goto fail;
    seg = *h;
    segLimit = (void*)((char*)seg + segSize);
    firstPage = SM_PAGE_ROUNDUP(seg);
    lastPage = SM_PAGE_ROUNDDN(((char*)seg + segSize));

    /* Put the segment table in the otherwise wasted space at one
       end of the segment. We'll put it at which ever end is bigger. */
    segTable = (SMSegmentDesc*)seg;
    segTableCount = ((char*)firstPage - (char*)seg) / sizeof(SMSegmentDesc);
    otherCount = ((char*)segLimit - (char*)lastPage) / sizeof(SMSegmentDesc);
    if (otherCount > segTableCount) {
        segTable = (SMSegmentDesc*)lastPage;
        segTableCount = otherCount;
    }
    else if (segTableCount == 0) {
        segTable = (SMSegmentDesc*)firstPage;
        firstPage++;
        segTableCount = SM_PAGE_SIZE / sizeof(SMSegmentDesc);
    }
    SM_ASSERT(segTableCount > 0);
    pm->segTable = segTable;
    pm->segTableCount = segTableCount;
    
    segTable[0].handle = h;
    segTable[0].firstPage = firstPage;
    segTable[0].lastPage = lastPage;

    /* XXX hack for now -- just one segment */
    pm->memoryBase = firstPage;
    pm->boundary = firstPage;
    pm->minPage = SM_PAGE_NUMBER(firstPage);
    pm->pageCount = lastPage - firstPage;

    return PR_SUCCESS;
    
  fail:
    if (h) {
        TempDisposeHandle(h, &err);
    }
    return PR_FAILURE;
    
#else

    SMPage* addr = NULL;
    SMPageCount size = maxPages;
    int zero_fd;

#ifdef MAP_ANON
    zero_fd = -1;
#else
    zero_fd = open("/dev/zero", O_RDWR);
#endif

    while (addr == NULL) {
        /* let the system place the heap */
        addr = (SMPage*)mmap(0, size << SM_PAGE_BITS,
                             PROT_READ | PROT_WRITE,
#ifdef MAP_ANON
                             MAP_ANON | 
#endif
                             MAP_PRIVATE,
                             zero_fd, 0);
        if (addr == (SMPage*)MAP_FAILED) {
            addr = NULL;
            size--;
            if (size < minPages) {
                return PR_FAILURE;
            }
        }
    }

#ifndef MAP_ANON
    close(zero_fd);
#endif

    SM_ASSERT(SM_IS_ALIGNED(addr, SM_PAGE_BITS));
    pm->memoryBase = addr;
    pm->pageCount = size;
    pm->boundary = addr;
    pm->minPage = SM_PAGE_NUMBER(addr);

    return PR_SUCCESS;
#endif
}

static void
sm_FiniPages(SMPageMgr* pm)
{
#if defined(XP_PC)

    BOOL ok;
    ok = VirtualFree((void*)pm->memoryBase, 0, MEM_RELEASE);
    SM_ASSERT(ok);
    pm->memoryBase = NULL;
    pm->pageCount = 0;
    PR_DestroyMonitor(pm->monitor);
    pm->monitor == NULL;

#elif defined(XP_MAC)
    
#else
    munmap((caddr_t)pm->memoryBase, pm->pageCount << SM_PAGE_BITS);
#endif
}

/*******************************************************************************
 * Page Manager
 ******************************************************************************/
 
PRStatus
sm_InitPageMgr(SMPageMgr* pm, SMPageCount minPages, SMPageCount maxPages)
{
    PRStatus status;
    PRUword pageTableSize, pageTablePages;
    SMSmallObjCount i;
    PRUword allocPages = minPages;
    
    memset(pm, 0, sizeof(pm));
    
    pm->monitor = PR_NewMonitor();
    if (pm->monitor == NULL)
        return PR_FAILURE;
    pm->alreadyLocked = PR_FALSE;

    pageTableSize = minPages * sizeof(SMPageDesc);
    pageTablePages = SM_PAGE_COUNT(pageTableSize);
    
    allocPages += pageTablePages;
    if (allocPages > maxPages) {
	    pageTableSize = maxPages * sizeof(SMPageDesc);
	    pageTablePages = SM_PAGE_COUNT(pageTableSize);
	    allocPages = maxPages;
    }

    status = sm_InitPages(pm, allocPages, allocPages);
    if (status != PR_SUCCESS)
        return status;

    /* make sure these got set */
    SM_ASSERT(pm->memoryBase);
    SM_ASSERT(pm->boundary);
    SM_ASSERT(pm->minPage);

    pm->freeClusters = NULL;
    pm->heapBase = pm->memoryBase + pageTablePages;
    pm->heapPageCount = pm->pageCount - pageTablePages;
    
    /* Initialize page table */
    pm->pageTableMem = (SMPageDesc*)sm_NewCluster(pm, pageTablePages);
    if (pm->pageTableMem == NULL) {
        sm_FiniPageMgr(pm);
        return PR_FAILURE;
    }
    pm->pageTable = pm->pageTableMem - (pm->minPage + pageTablePages);
    for (i = 0; i < pm->heapPageCount; i++) {
        SMPageDesc* pd = &pm->pageTableMem[i];
        SM_PAGEDESC_INIT(pd, 0, SMGenNum_Free, NULL, 0);
    }
    return PR_SUCCESS;
}

#if defined(XP_PC) && defined(DEBUG)

static PRUword sm_LastPageAllocTries = 0;
static PRUword sm_LastPageAllocHits = 0;
static PRUword sm_LastPageFreeTries = 0;
static PRUword sm_LastPageFreeHits = 0;

#endif

void
sm_FiniPageMgr(SMPageMgr* pm)
{
#if 0
    if (pm->pageTableMem) {
        sm_DestroyCluster(pm, (SMPage*)pm->pageTableMem,
                          SM_PAGE_COUNT(pm->heapPageCount));
    }
#endif
#if defined(XP_PC) && defined(DEBUG)
    if (sm_DebugOutput) {
        fprintf(sm_DebugOutput, "Page Manager Cache: alloc hits: %u/%u %u%%, free hits: %u/%u %u%%\n",
                sm_LastPageAllocHits, sm_LastPageAllocTries, 
                (sm_LastPageAllocHits * 100 / sm_LastPageAllocTries), 
                sm_LastPageFreeHits, sm_LastPageFreeTries, 
                (sm_LastPageFreeHits * 100 / sm_LastPageFreeTries)); 
    }
#endif
    sm_FiniPages(pm);
}

/******************************************************************************/

#ifdef XP_PC

#define SM_PAGE_HYSTERESIS      /* undef if you want to compare */
#ifdef SM_PAGE_HYSTERESIS

/* one-page hysteresis */
static void* sm_LastPageFreed = NULL;
static PRUword sm_LastPageFreedSize = 0;
static void* sm_LastPageTemp = NULL;
static PRUword sm_LastPageTempSize = 0;

#ifdef DEBUG
//#define SM_COMMIT_TRACE

static void*
SM_COMMIT_CLUSTER(void* addr, PRUword size) 
{
    sm_LastPageAllocTries++;
    if (sm_LastPageFreed == (void*)(addr) && sm_LastPageFreedSize == (size)) {
#ifdef SM_COMMIT_TRACE
        char buf[64];
        PR_snprintf(buf, sizeof(buf), "lalloc %p %u\n",
                    sm_LastPageFreed, sm_LastPageFreedSize);
        OutputDebugString(buf);
#endif
        DBG_MEMSET(sm_LastPageFreed, SM_PAGE_ALLOC_PATTERN, sm_LastPageFreedSize);
        sm_LastPageTemp = sm_LastPageFreed;
        sm_LastPageFreed = NULL;
        sm_LastPageFreedSize = 0;
        sm_LastPageAllocHits++;
        return sm_LastPageTemp; 
    } 
    else {
        /* If the cached pages intersect the current request, we lose. 
           Just free the cached request instead of trying to split it up. */
        if (sm_LastPageFreed &&
            SM_OVERLAPPING((char*)sm_LastPageFreed, ((char*)sm_LastPageFreed + sm_LastPageFreedSize),
                           (char*)(addr), ((char*)(addr) + (size)))
//            ((char*)sm_LastPageFreed < ((char*)(addr) + (size))
//             && ((char*)sm_LastPageFreed + sm_LastPageFreedSize) > (char*)(addr))
            ) {
#ifdef SM_COMMIT_TRACE
            char buf[64];
            PR_snprintf(buf, sizeof(buf), "valloc %p %u (vfree  %p %u last=%u:%u req=%u:%u)\n",
                        addr, size,
                        sm_LastPageFreed, sm_LastPageFreedSize,
                        (char*)sm_LastPageFreed, ((char*)sm_LastPageFreed + sm_LastPageFreedSize),
                        (char*)(addr), ((char*)(addr) + (size)));
            OutputDebugString(buf);
#endif
            VirtualFree(sm_LastPageFreed, sm_LastPageFreedSize, MEM_DECOMMIT);
            sm_LastPageFreed = NULL;
            sm_LastPageFreedSize = 0;
            sm_LastPageFreeHits--;      /* lost after all */
        }
        else {
#ifdef SM_COMMIT_TRACE
            char buf[64];
            PR_snprintf(buf, sizeof(buf), "valloc %p %u (skipping %p %u)\n",
                        addr, size,
                        sm_LastPageFreed, sm_LastPageFreedSize);
            OutputDebugString(buf);
#endif
        }
        return VirtualAlloc((void*)(addr), (size), MEM_COMMIT, PAGE_READWRITE); 
    } 
} 

static int
SM_DECOMMIT_CLUSTER(void* addr, PRUword size) 
{
    sm_LastPageFreeTries++;
    SM_ASSERT(sm_LastPageFreed != (void*)(addr));
    if (sm_LastPageFreed) {
        /* If we've already got a cached page, just keep it. Heuristically,
           this tends to give us a higher hit rate because of the order in
           which pages are decommitted. */
#ifdef SM_COMMIT_TRACE
        char buf[64];
        PR_snprintf(buf, sizeof(buf), "vfree  %p %u (cached %p %u)\n", 
                    addr, size, sm_LastPageFreed, sm_LastPageFreedSize);
        OutputDebugString(buf);
#endif
        return VirtualFree(addr, size, MEM_DECOMMIT); 
    }
    sm_LastPageFreed = (void*)(addr); 
    sm_LastPageFreedSize = (size); 
    DBG_MEMSET(sm_LastPageFreed, SM_PAGE_FREE_PATTERN, sm_LastPageFreedSize); 
#ifdef SM_COMMIT_TRACE
    {
        char buf[64];
        PR_snprintf(buf, sizeof(buf), "lfree  %p %u\n",
                    sm_LastPageFreed, sm_LastPageFreedSize);
        OutputDebugString(buf);
    }
#endif
    sm_LastPageFreeHits++;
    return 1; 
} 

#else /* !DEBUG */

#define SM_COMMIT_CLUSTER(addr, size)                                               \
    (SM_ASSERT((void*)(addr) != NULL),                                              \
     ((sm_LastPageFreed == (void*)(addr) && sm_LastPageFreedSize == (size))         \
      ? (DBG_MEMSET(sm_LastPageFreed, SM_PAGE_ALLOC_PATTERN, sm_LastPageFreedSize), \
         sm_LastPageTemp = sm_LastPageFreed,                                        \
         sm_LastPageFreed = NULL,                                                   \
         sm_LastPageFreedSize = 0,                                                  \
         sm_LastPageTemp)                                                           \
      : (((sm_LastPageFreed &&                                                      \
           ((char*)sm_LastPageFreed < ((char*)(addr) + (size))                      \
            && ((char*)sm_LastPageFreed + sm_LastPageFreedSize) > (char*)(addr)))   \
          ? (VirtualFree(sm_LastPageFreed, sm_LastPageFreedSize, MEM_DECOMMIT),     \
             sm_LastPageFreed = NULL,                                               \
             sm_LastPageFreedSize = 0)                                              \
          : ((void)0)),                                                             \
         VirtualAlloc((void*)(addr), (size), MEM_COMMIT, PAGE_READWRITE))))         \

#define SM_DECOMMIT_CLUSTER(addr, size)                                             \
    (SM_ASSERT(sm_LastPageFreed != (void*)(addr)),                                  \
     (sm_LastPageFreed                                                              \
      ? (VirtualFree(addr, size, MEM_DECOMMIT))                                     \
      : (sm_LastPageFreed = (addr),                                                 \
         sm_LastPageFreedSize = (size),                                             \
         DBG_MEMSET(sm_LastPageFreed, SM_PAGE_FREE_PATTERN, sm_LastPageFreedSize),  \
         1)))                                                                       \

#endif /* !DEBUG */

#else /* !SM_PAGE_HYSTERESIS */

#define SM_COMMIT_CLUSTER(addr, size) \
    VirtualAlloc((void*)(addr), (size), MEM_COMMIT, PAGE_READWRITE)

#define SM_DECOMMIT_CLUSTER(addr, size) \
    VirtualFree((void*)(addr), (size), MEM_DECOMMIT)

#endif /* !SM_PAGE_HYSTERESIS */

#else /* !XP_PC */

#define SM_COMMIT_CLUSTER(addr, size)      (addr)
#define SM_DECOMMIT_CLUSTER(addr, size)    1

#endif /* !XP_PC */

SMPage*
sm_NewCluster(SMPageMgr* pm, SMPageCount nPages)
{
    SMPage* addr;

    SM_ASSERT(nPages > 0);
    if (!pm->alreadyLocked)
        PR_EnterMonitor(pm->monitor);
    else {
        SM_ASSERT(PR_InMonitor(pm->monitor));
    }
    addr = sm_AllocClusterFromFreeList(pm, nPages);
    if (!addr && pm->boundary + nPages <= pm->memoryBase + pm->pageCount) {
        addr = pm->boundary;
        pm->boundary += nPages;
    }
    if (addr) {
        /* Extend the mapping */
        SMPage* vaddr;
        PRUword size = nPages << SM_PAGE_BITS;
        SM_ASSERT(SM_IS_ALIGNED(addr, SM_PAGE_BITS));
        vaddr = (SMPage*)SM_COMMIT_CLUSTER((void*)addr, size);
        SM_VERIFYCLUSTERS(pm, nPages);
        if (addr) {
            PR_ASSERT(vaddr == addr);
        }
        else {
            sm_DestroyCluster(pm, addr, nPages);
        }
        DBG_MEMSET(addr, SM_PAGE_ALLOC_PATTERN, size);
    }
    if (!pm->alreadyLocked)
        PR_ExitMonitor(pm->monitor);
    return (SMPage*)addr;
}

void
sm_DestroyCluster(SMPageMgr* pm, SMPage* basePage, SMPageCount nPages)
{
    int freeResult;
    PRUword size = nPages << SM_PAGE_BITS;

    SM_ASSERT(nPages > 0);
    SM_ASSERT(SM_IS_ALIGNED(basePage, SM_PAGE_BITS));
    SM_ASSERT(pm->memoryBase <= basePage);
    SM_ASSERT(basePage + nPages <= pm->memoryBase + pm->pageCount);
    DBG_MEMSET(basePage, SM_PAGE_FREE_PATTERN, size);
    freeResult = SM_DECOMMIT_CLUSTER((void*)basePage, size);
    SM_ASSERT(freeResult);
    if (!pm->alreadyLocked)
        PR_EnterMonitor(pm->monitor);
    else {
        SM_ASSERT(PR_InMonitor(pm->monitor));
    }
    if (basePage + nPages == pm->boundary) {
        SMClusterDesc **p;
        SMClusterDesc *desc;
        /* We deallocated the last set of clusters. Move the boundary lower. */
        pm->boundary = basePage;
        /* The last free cluster might now be adjacent to the boundary; if so,
           move the boundary before that cluster and delete that cluster
           altogether. */
        p = &pm->freeClusters;
        while ((desc = *p) != NULL) {
            if (!desc->next && desc->addr + desc->nPages == pm->boundary) {
                *p = 0;
                pm->boundary = desc->addr;
                sm_DeleteFreeClusterDesc(pm, desc);
            }
            else {
                p = &desc->next;
            }
        }
    }
    else {
        sm_AddClusterToFreeList(pm, basePage, nPages);
    }
    /* reset page descriptors */
    {
        SMPageDesc* pd = SM_OBJECT_PAGEDESC(basePage);
        SMPageCount i;
        for (i = 0; i < nPages; i++) {
            SM_PAGEDESC_INIT(pd, 0, SMGenNum_Free, NULL, 0);
            pd++;
        }
    }
    SM_VERIFYCLUSTERS(pm, -nPages);
    if (!pm->alreadyLocked)
        PR_ExitMonitor(pm->monitor);
}

/******************************************************************************/

