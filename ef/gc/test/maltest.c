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

#include "prinit.h"
#include "sm.h"
#include "smgen.h"
#include "smheap.h"
#include <stdio.h>
#include <time.h>

extern void* malloc(size_t);
extern void free(void*);

#define K               1024
#define M               (K*K)
#define PAGE            (4*K)
#define MIN_HEAP_SIZE   (K*PAGE)
#define MAX_HEAP_SIZE   (16*M)

#define ITERATIONS      (16*K)
#define KEEP_AMOUNT     K

FILE* out;
int toFile = 1;
int verbose = 0;

int
main(void)
{
    PRStatus status;
    PRIntervalTime t1, t2;
    void* foo;
    void* bar;
    int i, ms, keepIndex = 0;
    struct tm *newtime;
    time_t aclock;
    void* keep[KEEP_AMOUNT];
    
    if (toFile) {
        out = fopen("maltest.out", "w+");
        if (out == NULL) {
            perror("Can't open maltest.out");
            return -1;
        }
    }
    else
        out = stdout;

    time(&aclock);
    newtime = localtime(&aclock);
    fprintf(out, "SportModel Malloc Test Program: %s\n", asctime(newtime));
//    fprintf(out, "SM_ALLOC_BUCKETS=%u\n", SM_ALLOC_BUCKETS);
    
    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    status = SM_InitGC(MIN_HEAP_SIZE, MAX_HEAP_SIZE,
                       NULL, NULL, NULL, NULL, NULL, NULL);
    if (status != PR_SUCCESS)
        return -1;
        
    foo = SM_Malloc(32);
    SM_Free(foo);
    foo = SM_Malloc(33);
    SM_Free(foo);

    foo = SM_Malloc(32);
    bar = SM_Malloc(32);
    SM_Free(foo);
    SM_Free(bar);
    
    /* try to malloc some large objects */
    foo = SM_Malloc(8 * K);
    bar = SM_Malloc(9 * K);
    SM_Free(foo);
    SM_Free(bar);
    
    foo = SM_Calloc(sizeof(char), 4 * K);
    memset(foo, 'a', 4*K - 1);
    bar = SM_Strdup(foo);
    if (strcmp(foo, bar) != 0)
        fprintf(out, "strdup failed!\n");
    if (verbose) {
        for (i = 0; i < 4*K-1; i++) {
            fprintf(out, "%c", ((char*)bar)[i]);
            if (i % 32 == 31)
                fprintf(out, "\n");
        }
        fprintf(out, "\n");
    }
    SM_Free(foo);
    SM_Free(bar);
    
    for (i = 0; i < KEEP_AMOUNT; i++)
        keep[i] = NULL;

    /* time the platform malloc/free */
    t1 = PR_IntervalNow();
    for (i = 0; i < ITERATIONS; i++) {
        int size = i % SM_MAX_SMALLOBJECT_SIZE;
        int j = i % KEEP_AMOUNT;
        if (keep[j])
            free(keep[j]);
        keep[j] = malloc(size);
    }
    for (i = 0; i < KEEP_AMOUNT; i++) {
        if (keep[i]) {
            free(keep[i]);
            keep[i] = NULL;
        }
    }
    t2 = PR_IntervalNow();
    ms = PR_IntervalToMilliseconds(t2 - t1);
    fprintf(out, "time for %uk malloc/free operations: %ums\n", ITERATIONS / K, ms);
    
    /* time the SportModel malloc/free */
    t1 = PR_IntervalNow();
    for (i = 0; i < ITERATIONS; i++) {
        int size = i % SM_MAX_SMALLOBJECT_SIZE;
        int j = i % KEEP_AMOUNT;
        if (keep[j])
            SM_Free(keep[j]);
        keep[j] = SM_Malloc(size);
    }
    for (i = 0; i < KEEP_AMOUNT; i++) {
        if (keep[i]) {
            SM_Free(keep[i]);
            keep[i] = NULL;
        }
    }
    t2 = PR_IntervalNow();
    ms = PR_IntervalToMilliseconds(t2 - t1);
    fprintf(out, "time for %uk SM_Malloc/SM_Free operations: %ums\n", ITERATIONS / K, ms);

#ifdef DEBUG
    SM_DumpStats(out, PR_TRUE);
#endif

    fflush(out);
    
    SM_CleanupGC(PR_FALSE);
    status = PR_Cleanup();
    if (status != PR_SUCCESS)
        return -1;
    return 0;
}

/******************************************************************************/
