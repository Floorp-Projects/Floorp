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
                            Author: Warren Harris
*******************************************************************************/

#include "sm.h"
#include "smpage.h"
#include "prinrval.h"
#include <stdio.h>
#include <stdlib.h>

#define PG_COUNT                1024
SMPage* pg[PG_COUNT];
int pgIndex = 0;
int allocCount = 0;
int allocSize = 0;
int failed = 0;
int failedSize = 0;

#define MAX_CLUSTER_SIZE        128
#define ITERATIONS              (10*1024)
#define ALLOC_WEIGHT            8

#ifdef xDEBUG
#define FPRINTF(args) fprintf args
#else
#define FPRINTF(args) 
#endif

int
main()
{
    int i;
    SMPageMgr* pm;
    PRIntervalTime t1, t2;
    int time;

    fprintf(stdout, "SportModel Garbage Collector: Page Test\n");
    
    pm = (SMPageMgr*)malloc(sizeof(SMPageMgr));
    SM_InitPageMgr(pm, 1, PG_COUNT * MAX_CLUSTER_SIZE / 2);

    t1 = PR_IntervalNow();

    for (i = 0; i < ITERATIONS; i++) {
        int alloc = rand() & ((1 << ALLOC_WEIGHT) - 1);
        if ((!alloc && allocCount > 0) || allocCount >= PG_COUNT) {       /* then do a deallocation */
            /* find a page to deallocate */
            int p = rand() * PG_COUNT / RAND_MAX;
            while (pg[p] == NULL) {
                p++;
                if (p >= PG_COUNT)
                    p = 0;
            }
            allocCount--;
            allocSize -= *(int*)pg[p];
            FPRINTF((stdout, "%5d Dealloc size %4d index %4d count %5d total %10d addr %8p\n",
                     i, *(int*)pg[p], p, allocCount, allocSize, pg[p]));
            SM_DestroyCluster(pm, pg[p], *(int*)(pg[p]));
            pg[p] = NULL;
        }
        else {          /* else do an allocation */
            PRWord x = rand() * MAX_CLUSTER_SIZE / RAND_MAX + 1;
            while (pg[pgIndex] != NULL) {
                pgIndex++;
                if (pgIndex >= PG_COUNT)
                    pgIndex = 0;
            }
            pg[pgIndex] = SM_NewCluster(pm, x);
            if (pg[pgIndex]) {
                *(int*)(pg[pgIndex]) = x;
                allocCount++;
                allocSize += x;
            }
            else {
                failed++;
                failedSize += x;
            }
            FPRINTF((stdout, "%5d Alloc   size %4d index %4d count %5d total %10d addr %8p\n",
                     i, x, pgIndex, allocCount, allocSize, pg[pgIndex]));
        }
    }

    t2 = PR_IntervalNow();

    SM_FiniPageMgr(pm);

    time = PR_IntervalToMilliseconds(t2 - t1);
    fprintf(stdout, "Time = %dms, live pages = %d\n", time, allocCount);
    fprintf(stdout, "Failures = %d, Avg failure size = %d\n", failed, failedSize/failed);
    fprintf(stdout, "Avg Time = %fms\n", (float)time / ITERATIONS);
    return 0;
}
