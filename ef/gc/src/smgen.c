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

#include "smgen.h"
#include "smheap.h"
#include <string.h>

PRStatus
sm_InitGen(SMGen* gen, PRUword allocThreshold)
{
    gen->allocThreshold = allocThreshold;
    gen->collectCount = 0;
    return sm_InitPool(&gen->pool);
}

void
sm_FiniGen(SMGen* gen)
{
    sm_FiniPool(&gen->pool);
    gen->allocThreshold = 0;
    gen->collectCount = 0;
}

/******************************************************************************/

SMObjStruct*
sm_GenPromoteObj(SMGen* gen, SMBucket bucket)
{
    SMSweepList* sweepList = &gen->pool.sweepList[bucket];
    SMSmallObjCount objsPerPage = sm_ObjectsPerPage[bucket];
    SMPageDesc* pd;

    /*SM_ASSERT(SM_IN_GC());*/
    
    /* pages on the sweep list should never be large */
    SM_ASSERT(bucket != SM_LARGE_OBJECT_BUCKET);
        
    /* lazy sweep -- start from where we left off, and only go until
       we find what we need */
    while ((pd = sweepList->sweepPage) != NULL) {
        SM_ASSERT(SM_PAGEDESC_BUCKET(pd) == bucket);
        /* The pd->allocCount may be zero here because we may be collecting
           the generation we're currently allocating in.
        SM_ASSERT(pd->allocCount != 0); */
        while (sweepList->sweepIndex < objsPerPage) {
            SMSmallObjCount i = sweepList->sweepIndex++;
            SMObjDesc* od = &pd->objTable[i];
            if (SM_OBJDESC_WAS_FREE(od)) { /* only line different from sm_PoolAllocObj */
                SMSmallObjSize objSize = sm_ObjectSize[bucket];
                SMObjStruct* obj = (SMObjStruct*)((char*)SM_PAGEDESC_PAGE(pd) + i * objSize);
                SM_OBJDESC_SET_ALLOCATED(od);
                if (++pd->allocCount == objsPerPage) {
                    /* if this allocation caused this page to become
                       full, remove the page from the sweep list */
                    sweepList->sweepPage = pd->next;
                    pd->next = NULL;
                    sweepList->sweepIndex = 0;
                }
                return obj;
            }
        }
        sweepList->sweepPage = pd->next;
        pd->next = NULL;
        sweepList->sweepIndex = 0;
    }
    return NULL;
}

/******************************************************************************/

#ifdef DEBUG

PRUword
sm_GenSpaceAvailable(SMGenNum genNum)
{
    SMPageCount i;
    PRUword space = 0;
    for (i = 0; i < sm_PageMgr.heapPageCount; i++) {
        SMPageDesc* pd = &sm_PageMgr.pageTableMem[i];
        SMGenNum pdGen = SM_PAGEDESC_GEN(pd);
        if (pdGen == genNum) {
            SMBucket bucket = SM_PAGEDESC_BUCKET(pd);
            if (bucket != SM_LARGE_OBJECT_BUCKET) {
                SMSmallObjCount objsPerPage = sm_ObjectsPerPage[bucket];
                SMSmallObjSize objSize = sm_ObjectSize[bucket];
                space += objSize * (objsPerPage - pd->allocCount);
            }
        }
    }
    return space;
}

#endif /* DEBUG */

/******************************************************************************/
