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
#ifdef XP_MAC
#include "pprthred.h"
#else
#include "private/pprthred.h"
#endif
#include "prgc.h"
#include <stdio.h>
#include <time.h>

#define K               1024
#define M               (K*K)
#define PAGE            (4*K)
#define MIN_HEAP_SIZE   (128*PAGE)
#define MAX_HEAP_SIZE   (16*M)

FILE* out;
int toFile = 1;
int verbose = 1;
int testLargeObjects = 1;

GCInfo *gcInfo = 0;

/******************************************************************************/

typedef void (PR_CALLBACK *FinalizeFun)(void* obj);

typedef struct fieldblock {
    int                 type;
    int                 access;
    int                 offset;
} fieldblock;

typedef enum AType {
    T_NORMAL_OBJECT,    /* must be 0 */
    T_CLASS,            /* aka, array of object */
    T_INT,
    T_LONG,
    T_DOUBLE
} AType;

#define fieldIsArray(fb)        ((fb)->type == T_CLASS)
#define fieldIsClass(fb)        ((fb)->type == T_NORMAL_OBJECT)

typedef struct ClassClass {
    PRUword             instanceSize;
    int                 fieldsCount;
    fieldblock*         fields;
    struct methodtable* methodTable;
    FinalizeFun         finalize;
} ClassClass;

#define cbSuperclass(cb)        NULL
#define cbFields(cb)            ((cb)->fields)
#define cbFieldsCount(cb)       ((cb)->fieldsCount)
#define ACC_STATIC              1
#define cbFinalizer(cb)         ((cb)->finalize)
#define cbInstanceSize(cb)      ((cb)->instanceSize)
#define cbMethodTable(cb)       ((cb)->methodTable)

#define class_offset(o)         obj_length(o)

struct methodblock;

struct methodtable {
    ClassClass*         classdescriptor;
	/* methodtables must be at 32-byte aligned -- adding 7 will get around this */
    struct methodblock* methods[7];
};

#define ALIGN_MT(mt)  ((struct methodtable*)((((PRUword)(mt)) + FLAG_MASK) & ~FLAG_MASK))

#define obj_methodtable(obj)    ((obj)->methods)
#define obj_classblock(obj)     ((obj)->methods->classdescriptor)

#define METHOD_FLAG_BITS 5L
#define FLAG_MASK	((1L<<METHOD_FLAG_BITS)-1L)  /* valid flag bits */
#define METHOD_MASK     (~FLAG_MASK)  /* valid mtable ptr bits */
#define LENGTH_MASK     METHOD_MASK
#define rawtype(o) (((unsigned long) (o)->methods) & FLAG_MASK)
#define obj_flags(o) rawtype(o)
#define obj_length(o)	\
    (((unsigned long) (o)->methods) >> METHOD_FLAG_BITS)

#define tt2(m) ((m) & FLAG_MASK)
#define atype(m) tt2(m)
#define mkatype(t,l) ((struct methodtable *) (((l) << METHOD_FLAG_BITS)|(t)))

typedef struct ClassObject ClassObject;

typedef struct JHandle {
    struct methodtable* methods;
    ClassObject*        obj;
} JHandle;

typedef JHandle HObject;
#define OBJECT HObject*
#define unhand(h)               ((h)->obj)

typedef struct HArrayOfObject {
    JHandle             super;
    PRUword             length;
    HObject*            body[1];
} HArrayOfObject;

static void PR_CALLBACK
ScanJavaHandle(void GCPTR *gch)
{
    JHandle *h = (JHandle *) gch;
    ClassClass *cb;
    void **sub;
    ClassObject *p;
    void (*livePointer)(void *base);
    void (*liveBlock)(void **base, PRInt32 count);
    int32 n;

    liveBlock = gcInfo->liveBlock;
    livePointer = gcInfo->livePointer;

    (*livePointer)((void*) h->obj);
    p = unhand(h);

    switch (obj_flags(h)) {
      case T_NORMAL_OBJECT:
        if (obj_methodtable(h)) {
            cb = obj_classblock(h);
	    do {
		struct fieldblock *fb = cbFields(cb);

		/* Scan the fields of the class */
		n = cbFieldsCount(cb);
		while (--n >= 0) {
		    if ((fieldIsArray(fb) || fieldIsClass(fb))
			&& !(fb->access & ACC_STATIC)) {
			sub = (void **) ((char *) p + fb->offset);
			(*livePointer)(*sub);
		    }
		    fb++;
		}
		if (cbSuperclass(cb) == 0) {
		    break;
		}
		cb = cbSuperclass(cb);
	    } while (cb);
	} else {
	    /*
            ** If the object doesn't yet have it's method table, we can't
	    ** scan it. This means the GC examined the handle before the
	    ** allocation code (realObjAlloc/AllocHandle) has initialized
	    ** it.
	    */
	}
	break;

      case T_CLASS:		/* an array of objects */
	/*
	** Have the GC scan all of the elements and the ClassClass*
	** stored at the end.
	*/
	n = class_offset(h) + 1;
	(*liveBlock)((void**)((HArrayOfObject *) h)->body, n);
	break;
    }
}

static void PR_CALLBACK
DumpJavaHandle(FILE *out, void GCPTR *gch, PRBool detailed, int indent)
{
}

static void PR_CALLBACK 
SummarizeJavaHandle(void GCPTR *obj, size_t bytes)
{
}

static void PR_CALLBACK
FinalJavaHandle(void GCPTR *gch)
{
    JHandle *handle = (JHandle GCPTR *) gch;
    ClassClass* cb = obj_classblock(handle);
    cbFinalizer(cb)(handle);
}

GCType unscannedType = {
    0,
    0,
    DumpJavaHandle,
    SummarizeJavaHandle,
    0,
    0,
    'U',
    0
};
int unscannedTIX;

GCType scannedType = {
    ScanJavaHandle,
    0,
    DumpJavaHandle,
    SummarizeJavaHandle,
    0,
    0,
    'S',
    NULL
};
int scannedTIX;

GCType scannedFinalType = {
    ScanJavaHandle,
    FinalJavaHandle,
    DumpJavaHandle,
    SummarizeJavaHandle,
    0,
    0,
    'F',
    NULL
};
int scannedFinalTIX;

static void*
realObjAlloc(struct methodtable *mptr, long bytes, PRBool inIsClass)
{
    ClassClass *cb;
    HObject *h;
    int tix, flags;

    /* See if the object requires finalization or scanning */
    if (!inIsClass) {
        switch (atype((unsigned long) mptr)) {
          case T_NORMAL_OBJECT:
	    /* Normal object that may require finalization or double alignment. */
	    /* XXX pessimistic assumption for now */
	    flags = PR_ALLOC_DOUBLE | PR_ALLOC_CLEAN;
	    cb = mptr->classdescriptor;
	    if (cbFinalizer(cb)) {
		tix = scannedFinalTIX;
#if 0
	    } else if (cbFlags(cb) & CCF_IsWeakLink) {
		tix = weakLinkTIX;
#endif
	    } else {
		tix = scannedTIX;
	    }
	    break;
	  case T_CLASS:		/* An array of objects */
	    flags =  PR_ALLOC_CLEAN;
	    tix = scannedTIX;
	    break;

	  case T_LONG:              /* An array of java long's */
	  case T_DOUBLE:            /* An array of double's */
	    flags = PR_ALLOC_DOUBLE | PR_ALLOC_CLEAN;
	    tix = unscannedTIX;
	    break;

          default:
            /* An array of something (char, byte, ...) */
            flags = PR_ALLOC_CLEAN;
            tix = unscannedTIX;

            break;
	}
    }
    else {
	flags = PR_ALLOC_CLEAN;
	tix = unscannedTIX; // classClassTIX;
    }

    if (flags & PR_ALLOC_DOUBLE) {
	/* Double align bytes */
	bytes = (bytes + BYTES_PER_DWORD - 1) >> BYTES_PER_DWORD_LOG2;
	bytes <<= BYTES_PER_DWORD_LOG2;
    } else {
	/* Word align bytes */
	bytes = (bytes + BYTES_PER_WORD - 1) >> BYTES_PER_WORD_LOG2;
	bytes <<= BYTES_PER_WORD_LOG2;
    }

    /*
    ** Add in space for the handle (which is two words so we won't mess
    ** up the double alignment)
    */
    /*
      Achtung Explorer!! Proceed carefully!!!

      On certain machines a JHandle MUST be double aligned.

     */
    bytes += sizeof(JHandle);

    /* Allocate object and handle memory */
    h = (JHandle*) PR_AllocMemory(bytes, tix, flags);
    if (!h) {
	return 0;
    }

    /*
    ** Fill in handle.
    **
    ** Note: if the gc runs before these two stores happen, it's ok
    ** because it will find the reference to the new object on the C
    ** stack. The reference to the class object will be found in the
    ** calling frames C stack (see ObjAlloc below).
    */
    h->methods = mptr;
    
    h->obj = (ClassObject*) (h + 1);

    return h;
}

#define T_ELEMENT_SIZE(t) \
    (((t) == T_DOUBLE) ? sizeof(double) : sizeof(void*))

#define MAX_INT         0x7fffL

static PRInt32
sizearray(PRInt32 t, PRInt32 l)
{
    PRInt32 size = 0;

    switch(t){
    case T_CLASS:
	size = sizeof(OBJECT);
	break;
    default:
	size = T_ELEMENT_SIZE(t);
	break;
    }
    /* Check for overflow of PRInt32 size */
    if ( l && (size > MAX_INT/l )) {
        return -1;
    }

    size *= l;
    return size;
}

static HObject*
AllocArray(PRInt32 t, PRInt32 l)
{
    HObject *handle;

    PRInt32 bytes;

    bytes = sizearray(t, l);

    if (bytes < 0) {
	/* This is probably an overflow error - t*l > MAX_INT */
	return NULL;
    }

    bytes += (t == T_CLASS ? sizeof(OBJECT) : 0);
    handle = realObjAlloc((struct methodtable *) mkatype(t, l), bytes, FALSE);
    return handle;
}


static HObject*
AllocObject(ClassClass *cb, long n0)
{
    HObject *handle;

    n0 = cbInstanceSize(cb);
    handle = realObjAlloc(cbMethodTable(cb), n0, FALSE);
    return handle;
}

PR_EXTERN(void)
PR_PrintGCStats(void);

/******************************************************************************/

ClassClass* MyClass;
ClassClass* MyLargeClass;

typedef struct MyClassStruct {
    HObject                 super;
    struct MyClassStruct*   next;
    int                     value;
//    void*                   foo[2]; /* to see internal fragmentation */
} MyClassStruct;

static void
MyClass_finalize(HObject* self)
{
#ifdef DEBUG
    fprintf(out, "finalizing %d\n", ((MyClassStruct*)self)->value);
#endif
}

static void
InitClasses(void)
{
    MyClass = (ClassClass*)malloc(sizeof(ClassClass));
    MyClass->instanceSize = sizeof(MyClassStruct);
    MyClass->fieldsCount = 2;
    MyClass->fields = (fieldblock*)calloc(sizeof(fieldblock), 2);
    MyClass->fields[0].type = T_NORMAL_OBJECT;
    MyClass->fields[0].access = 0;
    MyClass->fields[0].offset = offsetof(MyClassStruct, next) - sizeof(HObject);
    MyClass->fields[1].type = T_INT;
    MyClass->fields[1].access = 0;
    MyClass->fields[1].offset = offsetof(MyClassStruct, value) - sizeof(HObject);
    MyClass->methodTable = ALIGN_MT(malloc(sizeof(struct methodtable)));
    MyClass->methodTable->classdescriptor = MyClass;
    MyClass->finalize = MyClass_finalize;
    
    MyLargeClass = (ClassClass*)malloc(sizeof(ClassClass));
    MyLargeClass->instanceSize = 4097;
    MyLargeClass->fieldsCount = 2;
    MyLargeClass->fields = (fieldblock*)calloc(sizeof(fieldblock), 2);
    MyLargeClass->fields[0].type = T_NORMAL_OBJECT;
    MyLargeClass->fields[0].access = 0;
    MyLargeClass->fields[0].offset = offsetof(MyClassStruct, next) - sizeof(HObject);
    MyLargeClass->fields[1].type = T_INT;
    MyLargeClass->fields[1].access = 0;
    MyLargeClass->fields[1].offset = offsetof(MyClassStruct, value) - sizeof(HObject);
    MyLargeClass->methodTable = ALIGN_MT(malloc(sizeof(struct methodtable)));
    MyLargeClass->methodTable->classdescriptor = MyLargeClass;
    MyLargeClass->finalize = MyClass_finalize;
}

/******************************************************************************/

MyClassStruct* root = NULL;

static void PR_CALLBACK
MarkRoots(void* data)
{
    void (*livePointer)(void *base);
    livePointer = gcInfo->livePointer;
    gcInfo->livePointer(root);
}

#if 0
static void
BeforeHook(SMGenNum genNum, PRUword collectCount, 
           PRBool copyCycle, void* closure)
{
#ifdef xDEBUG
    FILE* out = (FILE*)closure;
    if (verbose) {
        fprintf(out, "Before collection %u gen %u %s\n", collectCount, genNum,
                (copyCycle ? "(copy cycle)" : ""));
        PR_DumpGCHeap(out, dumpFlags);
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
        PR_DumpGCHeap(out, dumpFlags);
    }
#endif
}
#endif

#define ITERATIONS 10000

int
main(void)
{
    PRStatus status;
    HObject* arr;
    MyClassStruct* obj;
    MyClassStruct* conserv;
    int i, cnt = 0;
    struct tm *newtime;
    time_t aclock;
    void* randomPtr;
    PRIntervalTime t1, t2;

    if (toFile) {
        out = fopen("msgctest.out", "w+");
        if (out == NULL) {
            perror("Can't open msgctest.out");
            return -1;
        }
    }
    else
        out = stdout;

    time(&aclock);
    newtime = localtime(&aclock);
    fprintf(out, "SportModel Garbage Collector Test Program: %s\n", asctime(newtime));

    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    PR_SetThreadGCAble();
    PR_InitGC(0, MIN_HEAP_SIZE, 4*K, 0);
    gcInfo = PR_GetGCInfo();
    PR_RegisterRootFinder(MarkRoots, "mark msgctest roots", 0);
    scannedTIX = PR_RegisterType(&scannedType);
    scannedFinalTIX = PR_RegisterType(&scannedFinalType);

    InitClasses();

    t1 = PR_IntervalNow();

    arr = AllocArray(T_CLASS, 13);        /* gets collected */

    randomPtr = (char*)arr + (4096 * 200);

    arr = AllocArray(T_CLASS, 13);        /* try an array */
    ((HArrayOfObject*)arr)->body[3] = (HObject*)root;
    root = (MyClassStruct*)arr;
    arr = NULL;
            
    PR_GC();

    if (testLargeObjects) {
        /* test large object allocation */
        obj = (MyClassStruct*)AllocObject(MyLargeClass, 0);      /* gets collected */
        obj->value = cnt++;
        
        obj = (MyClassStruct*)AllocObject(MyLargeClass, 0);
        obj->value = cnt++;
        obj->next = root;
        root = obj;
        
        PR_GC();
    }

    obj = (MyClassStruct*)AllocObject(MyClass, 0);           /* gets collected */
    obj->value = cnt++;
    obj = (MyClassStruct*)AllocObject(MyClass, 0);
    obj->value = cnt++;
    obj->next = root;   /* link this into the root list */
    root = obj;
    conserv = (MyClassStruct*)AllocObject(MyClass, 0);   /* keep a conservative root */
    conserv->value = cnt++;

    arr = AllocArray(T_CLASS, 13);        /* try an array */
    ((HArrayOfObject*)arr)->body[3] = (HObject*)root;
    root = (MyClassStruct*)arr;
    arr = NULL;
            
    obj = NULL;
    PR_GC();
    PR_ForceFinalize();

    /* Force collection to occur for every subsequent object,
       for testing purposes. */
#if 0
    SM_SetCollectThresholds(sizeof(MyClassStruct), 
                            sizeof(MyClassStruct), 
                            sizeof(MyClassStruct), 
                            sizeof(MyClassStruct), 
                            0);
#endif

    for (i = 0; i < ITERATIONS; i++) {
        int size = rand() / 100;
        /* allocate some random garbage */
//        fprintf(out, "allocating array of size %d\n", size);
        AllocArray(T_CLASS, size);

        obj = (MyClassStruct*)AllocObject(MyClass, 0);   /* gets collected */
        obj->value = cnt++;
        obj->next = root;
        root = obj;

        //SM_DumpStats(out, dumpFlags);
    }
    
#ifdef DEBUG
    for (i = 0; i < 10; i++) {
        PR_GC();
    }
//    PR_PrintGCStats();
#endif

    t2 = PR_IntervalNow();
    fprintf(out, "Test completed in %ums\n", PR_IntervalToMilliseconds(t2 - t1));

    if (verbose) {
        /* list out all the objects */
        i = 0;
        obj = root;
        while (obj != NULL) {
            MyClassStruct* next;
            if (obj_flags((HObject*)obj) == T_NORMAL_OBJECT) {
                fprintf(out, "%4d ", obj->value);
                next = obj->next;
            }
            else {
                fprintf(out, "[arr %p] ", obj);
                next = (MyClassStruct*)((HArrayOfObject*)obj)->body[3];
            }
            if (i++ % 20 == 19)
                fprintf(out, "\n");
            obj = next;
        }
        fprintf(out, "\nconserv = %d\n", conserv->value);
    }

    randomPtr = NULL;
    root = NULL;
    obj = NULL;
    conserv = NULL;
    PR_GC();
    PR_ForceFinalize();
    PR_GC();
    PR_ForceFinalize();

#ifdef DEBUG
//    PR_DumpGCHeap(out, 0);
#endif

    PR_ShutdownGC(PR_TRUE);
    status = PR_Cleanup();
    if (status != PR_SUCCESS)
        return -1;
    return 0;
}

/******************************************************************************/
