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

#include "prinit.h"
#include "sm.h"
#include "smobj.h"
#include "smpage.h"
#include "smheap.h"
#include "smtrav.h"
#include <stdio.h>
#include <time.h>

#define K               1024
#define M               (K*K)
#define PAGE            (4*K)
#define MIN_HEAP_SIZE   (4*M)
#define MAX_HEAP_SIZE   (16*M)

FILE* out;
int toFile = 1;
int verbose = 0;
int testLargeObjects = 1;
#if defined(DEBUG) || defined(SM_DUMP)
//PRUword dumpFlags = SMDumpFlag_Detailed | SMDumpFlag_GCState | SMDumpFlag_Abbreviate;
PRUword dumpFlags = SMDumpFlag_Detailed;
//PRUword dumpFlags = 0;
#endif

/******************************************************************************/

SMObjectClass* MyClass;
SMObjectClass* MyLargeClass;
SMArrayClass*  MyArrayClass;

typedef struct MyClassStruct {
    SMObjStruct                 super;
    struct MyClassStruct*       next;
    int                         value;
//    void*                       foo[2]; /* to see internal fragmentation */
} MyClassStruct;

static void
MyClass_finalize(SMObject* self)
{
#if defined(DEBUG) || defined(SM_DUMP)
    fprintf(out, "finalizing %d\n", SM_GET_FIELD(MyClassStruct, self, value));
#endif
}

static void
InitClasses(void)
{
    SMFieldDesc* fds;
    MyClass = (SMObjectClass*)malloc(sizeof(SMObjectClass)
                                     + (1 * sizeof(SMFieldDesc)));
    SM_InitObjectClass(MyClass, NULL, sizeof(SMObjectClass),
                       sizeof(MyClassStruct), 1, 0);
    fds = SM_CLASS_GET_INST_FIELD_DESCS((SMClass*)MyClass);
    fds[0].offset = offsetof(MyClassStruct, next) / sizeof(void*);
    fds[0].count = 1;
    MyClass->data.finalize = MyClass_finalize;
    
    MyLargeClass = (SMObjectClass*)malloc(sizeof(SMObjectClass)
                                          + (1 * sizeof(SMFieldDesc)));
    SM_InitObjectClass(MyLargeClass, NULL, sizeof(SMObjectClass),
                       4097, 1, 0);
    fds = SM_CLASS_GET_INST_FIELD_DESCS((SMClass*)MyLargeClass);
    fds[0].offset = offsetof(MyClassStruct, next) / sizeof(void*);
    fds[0].count = 1;
    MyLargeClass->data.finalize = MyClass_finalize;

    MyArrayClass = (SMArrayClass*)malloc(sizeof(SMArrayClass));
    SM_InitArrayClass(MyArrayClass, NULL, (SMClass*)MyClass);
}

/******************************************************************************/

MyClassStruct* root = NULL;

static void
MarkRoots(SMGenNum genNum, PRBool copyCycle, void* data)
{
//    SM_MarkThreadsConservatively(genNum, copyCycle, data);      /* must be first */
    SM_MarkRoots((SMObject**)&root, 1, PR_FALSE);
}

static void
BeforeHook(SMGenNum genNum, PRUword collectCount, 
           PRBool copyCycle, void* closure)
{
#ifdef xDEBUG
    FILE* out = (FILE*)closure;
    if (verbose) {
        fprintf(out, "Before collection %u gen %u %s\n", collectCount, genNum,
                (copyCycle ? "(copy cycle)" : ""));
        SM_DumpGCHeap(out, dumpFlags);
    }
#endif
}

static void
AfterHook(SMGenNum genNum, PRUword collectCount, 
          PRBool copyCycle, void* closure)
{
#ifdef xDEBUG
    FILE* out = (FILE*)closure;
    if (verbose) {
        fprintf(out, "After collection %u gen %u %s\n",  collectCount, genNum,
                (copyCycle ? "(copy cycle)" : ""));
        SM_DumpGCHeap(out, dumpFlags);
    }
#endif
}

#define ITERATIONS 10000

int
main(void)
{
    PRStatus status;
    SMArray* arr;
    MyClassStruct* obj;
    MyClassStruct* conserv;
    int i, cnt = 0;
    struct tm *newtime;
    time_t aclock;
    void* randomPtr;
    PRIntervalTime t1, t2;
    int failure = 0;

    if (toFile) {
        out = fopen("smtest.out", "w+");
        if (out == NULL) {
            perror("Can't open smtest.out");
            return -1;
        }
    }
    else
        out = stdout;

#if defined(DEBUG) || defined(SM_DUMP)
    SM_SetTraceOutputFile(out);
#endif

    time(&aclock);
    newtime = localtime(&aclock);
    fprintf(out, "SportModel Garbage Collector Test Program: %s\n", asctime(newtime));

    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    PR_SetThreadGCAble();
    status = SM_InitGC(MIN_HEAP_SIZE, MAX_HEAP_SIZE, 
                       BeforeHook, out,
                       AfterHook, out,
                       MarkRoots, NULL);
    if (status != PR_SUCCESS)
        return -1;

    InitClasses();

    t1 = PR_IntervalNow();

    arr = SM_AllocArray(MyArrayClass, 13);        /* gets collected */

    randomPtr = (char*)SM_DECRYPT_ARRAY(arr) + (4096 * 200);

    arr = SM_AllocArray(MyArrayClass, 13);        /* try an array */
    SM_ARRAY_ELEMENTS(arr)[3] = (SMObject*)root;
    root = (MyClassStruct*)arr;
    arr = NULL;
            
    SM_Collect();

    if (testLargeObjects) {
        /* test large object allocation */
        obj = (MyClassStruct*)SM_AllocObject(MyLargeClass);      /* gets collected */
        if (obj) {
            ((MyClassStruct*)SM_DECRYPT(obj))->value = cnt++;
        }
        else {
            fprintf(out, "ALLOCATION FAILURE: Large Object\n");
            failure = 1;
            goto done;
        }
        
        obj = (MyClassStruct*)SM_AllocObject(MyLargeClass);
        if (obj) {
            ((MyClassStruct*)SM_DECRYPT(obj))->value = cnt++;
            SM_SET_FIELD(MyClassStruct, obj, next, root);
            root = obj;
        }
        else {
            fprintf(out, "ALLOCATION FAILURE: Large Object\n");
            failure = 1;
            goto done;
        }
        
        SM_Collect();
    }

    obj = (MyClassStruct*)SM_AllocObject(MyClass);           /* gets collected */
    if (obj) {
        ((MyClassStruct*)SM_DECRYPT(obj))->value = cnt++;
    }
    else {
        fprintf(out, "ALLOCATION FAILURE: Small Object\n");
        failure = 1;
        goto done;
    }
    obj = (MyClassStruct*)SM_AllocObject(MyClass);
    if (obj) {
        ((MyClassStruct*)SM_DECRYPT(obj))->value = cnt++;
        SM_SET_FIELD(MyClassStruct, obj, next, root);   /* link this into the root list */
        root = obj;
    }
    else {
        fprintf(out, "ALLOCATION FAILURE: Small Object\n");
        failure = 1;
        goto done;
    }
    conserv = (MyClassStruct*)SM_AllocObject(MyClass);   /* keep a conservative root */
    ((MyClassStruct*)SM_DECRYPT(conserv))->value = cnt++;

    arr = SM_AllocArray(MyArrayClass, 13);        /* try an array */
    SM_ARRAY_ELEMENTS(arr)[3] = (SMObject*)root;
    root = (MyClassStruct*)arr;
    arr = NULL;
            
    obj = NULL;
    SM_Collect();
    SM_RunFinalization();

    /* Force collection to occur for every subsequent object,
       for testing purposes. */
#if 0
    SM_SetCollectThresholds(1*M/*sizeof(MyClassStruct)*/, 
                            sizeof(MyClassStruct), 
                            sizeof(MyClassStruct), 
                            sizeof(MyClassStruct), 
                            0);
#endif

    for (i = 0; i < ITERATIONS; i++) {
        int size = rand() / 100 + 4;
        /* allocate some random garbage */
//        fprintf(out, "allocating array of size %d\n", size);
        arr = SM_AllocArray(MyArrayClass, size);
        if (arr) {
            SM_ARRAY_ELEMENTS(arr)[3] = (SMObject*)root;
            root = (MyClassStruct*)arr;
        }
        else {
            fprintf(out, "ALLOCATION FAILURE: Array size %u index %d\n", size, i);
            /* continue */
            root = NULL;
#if defined(DEBUG) || defined(SM_DUMP)
//            SM_DumpStats(out, dumpFlags);
            SM_DumpPageMap(out, dumpFlags);
#endif
        }

        obj = (MyClassStruct*)SM_AllocObject(MyClass);
        if (obj) {
            ((MyClassStruct*)SM_DECRYPT(obj))->value = cnt++;
            SM_SET_FIELD(MyClassStruct, obj, next, root);   /* link this into the root list */
            root = obj;
        }
        else {
            fprintf(out, "ALLOCATION FAILURE: Small Object index %d\n", i);
            /* continue */
            root = NULL;
#if defined(DEBUG) || defined(SM_DUMP)
//            SM_DumpStats(out, dumpFlags);
            SM_DumpPageMap(out, dumpFlags);
#endif
        }

        //SM_DumpStats(out, dumpFlags);
    }
    
    for (i = 0; i < 10; i++) {
        SM_Collect();
    }
    SM_RunFinalization();
    SM_Collect();
    SM_RunFinalization();

    t2 = PR_IntervalNow();
    fprintf(out, "Test completed in %ums\n", PR_IntervalToMilliseconds(t2 - t1));

  done:
#if defined(DEBUG) || defined(SM_DUMP)
//    SM_DumpStats(out, dumpFlags);
    SM_DumpPageMap(out, dumpFlags);
    if (failure) SM_DumpGCHeap(out, dumpFlags);
#endif

    if (verbose) {
        /* list out all the objects */
        i = 0;
        obj = root;
        while (obj) {
            MyClassStruct* next;
            if (SM_IS_OBJECT((SMObject*)obj)) {
                fprintf(out, "%4d ", SM_GET_FIELD(MyClassStruct, obj, value));
                next = SM_GET_FIELD(MyClassStruct, obj, next);
            }
            else {
                fprintf(out, "[arr %p] ", obj);
                next = (MyClassStruct*)SM_ARRAY_ELEMENTS((SMArray*)obj)[3];
            }
            if (i++ % 20 == 19)
                fprintf(out, "\n");
            obj = next;
        }
        fprintf(out, "\nconserv = %d\n", SM_GET_FIELD(MyClassStruct, conserv, value));
    }

    randomPtr = NULL;
    root = NULL;
    obj = NULL;
    conserv = NULL;

    SM_Collect();
    SM_RunFinalization();
#if defined(DEBUG) || defined(SM_DUMP)
//    SM_DumpStats(out, dumpFlags);
//    SM_DumpPageMap(out, dumpFlags);
    if (failure) SM_DumpGCHeap(out, dumpFlags);
#endif

    SM_Collect();
    SM_RunFinalization();
#if 0
#if defined(DEBUG) || defined(SM_DUMP)
//    SM_DumpStats(out, dumpFlags);
//    SM_DumpPageMap(out, dumpFlags);
    if (failure) SM_DumpGCHeap(out, dumpFlags);
#endif
#endif

    SM_Collect();
    SM_RunFinalization();
#if 0
#if defined(DEBUG) || defined(SM_DUMP)
//    SM_DumpStats(out, dumpFlags);
//    SM_DumpPageMap(out, dumpFlags);
    if (failure) SM_DumpGCHeap(out, dumpFlags);
#endif
#endif

    SM_Collect();
    SM_RunFinalization();
#if defined(DEBUG) || defined(SM_DUMP)
//    SM_DumpStats(out, dumpFlags);
    SM_DumpPageMap(out, dumpFlags);
    if (failure) SM_DumpGCHeap(out, dumpFlags);
#endif

    SM_CleanupGC(PR_TRUE);
    status = PR_Cleanup();
    if (status != PR_SUCCESS)
        return -1;
    return 0;
}

/******************************************************************************/
