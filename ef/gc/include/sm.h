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

/*
** SportModel is a hybrid, mark-and-sweep, mostly-copying, card-marking 
** generational garbage collector designed for very low level-two cache miss
** rates. The intended use is for the ElectricalFire Java implementation, 
** although it is not ElectricalFire or Java specific. It also includes an 
** integrated malloc/free package.
*/

#ifndef __SM__
#define __SM__

#include "smpriv.h"

SM_BEGIN_EXTERN_C

/*******************************************************************************
 * Types and Constants
 ******************************************************************************/

typedef struct SMHeap           SMHeap;
typedef union  SMClass          SMClass;
typedef struct SMObjectClass    SMObjectClass;
typedef struct SMArrayClass     SMArrayClass;
typedef struct SMPrimClass      SMPrimClass;
typedef struct SMObject         SMObject;
typedef struct SMArray          SMArray;
typedef struct SMObjStruct      SMObjStruct;
typedef struct SMArrayStruct    SMArrayStruct;

typedef enum SMGenNum {
    SMGenNum_Freshman = 0,      /* for new allocations */
    SMGenNum_Sophomore,         /* first survivor generation */
    SMGenNum_Junior,            /* oldest generation for applets */
    SMGenNum_Senior,            /* oldest generation for system code */
    SMGenNum_Static,            /* static space -- scanned, but not marked */
    SMGenNum_Malloc,            /* malloc space -- explicitly freed */
    SMGenNum_Free = 7           /* pages unused by malloc or gc */
} SMGenNum;

/******************************************************************************/

SM_DECLARE_ENSURE(SMObject)
SM_DECLARE_ENSURE(SMObjStruct)
SM_DECLARE_ENSURE(SMArray)
SM_DECLARE_ENSURE(SMArrayStruct)

#define SM_ENCRYPT_OBJECT(obj)  ((SMObject*)SM_ENCRYPT(SM_ENSURE(SMObjStruct, obj)))
#define SM_DECRYPT_OBJECT(obj)  ((SMObjStruct*)SM_DECRYPT(SM_ENSURE(SMObject, obj)))

#define SM_ENCRYPT_ARRAY(arr)   ((SMArray*)SM_ENCRYPT(SM_ENSURE(SMArrayStruct, arr)))
#define SM_DECRYPT_ARRAY(arr)   ((SMArrayStruct*)SM_DECRYPT(SM_ENSURE(SMArray, arr)))

/* XXX add ifdef SM_COLLECT_CLASSES */
#define SM_ENCRYPT_CLASS(cl)    (cl)
#define SM_DECRYPT_CLASS(cl)    (cl)

/*******************************************************************************
 * Startup/Shutdown Routines
 ******************************************************************************/

SM_EXTERN(void)
SM_MarkRoots(SMObject** base, PRUword count, PRBool conservative);

/* The user must define a root marking routine with the following signature.
   It's job is to call SM_MarkRoots for all roots in the program. This routine
   is registered by calling SM_InitHeap. */
typedef void
(*SMMarkRootsProc)(SMGenNum collectingGenNum, PRBool copyCycle, void* closure);

/* This function must be called as the first thing in the user's SMMarkRootsProc
   (or _be_ the user's SMMarkRootsProc if there are no other roots) if NSPR
   threads are to be scanned conservatively. Failure to call this indicates
   either that you know what you're doing, or you have a bug. */
SM_EXTERN(void)
SM_MarkThreadsConservatively(SMGenNum collectingGenNum, PRBool copyCycle,
                             void* closure);

/* This function allows finer grained control over exactly which threads are
   scanned conservatively. */
SM_EXTERN(void)
SM_MarkThreadConservatively(PRThread* thread, SMGenNum collectingGenNum,
                            PRBool copyCycle, void* closure);

/* The SMGCHookProc defines the signature of the before and after gc hook
   procedures. These are called before and after a collection on the thread 
   which caused the collection. The primary function of these hooks is to allow
   any locks needed by the root marking procedure to be entered (and exited) 
   because once the collection process has started, all other threads are
   suspended, and no locks (or monitors) can be waited on. */
typedef void
(*SMGCHookProc)(SMGenNum collectingGenNum, PRUword collectCount,
                PRBool copyCycle, void* closure);

/* Initialize the garbage collector system. */
SM_EXTERN(PRStatus)
SM_InitGC(PRUword initialHeapSize, PRUword maxHeapSize,
          SMGCHookProc beforeGCHook, void* beforeGCClosure,
          SMGCHookProc afterGCHook, void* afterGCClosure,
          SMMarkRootsProc markRootsProc, void* markRootsClosure);

/* Shutdown the garbage collector system. */
SM_EXTERN(PRStatus)
SM_CleanupGC(PRBool finalizeOnExit);

/* Initialize the malloc system. This is done implicitly when SM_InitGC is called. */
SM_EXTERN(PRStatus)
SM_InitMalloc(PRUword initialHeapSize, PRUword maxHeapSize);

/* Shutdown the malloc system. This is done implicitly when SM_CleanupGC is called. */
SM_EXTERN(PRStatus)
SM_CleanupMalloc(void);

/*******************************************************************************
 * Allocation and Collection
 ******************************************************************************/

/* Allocates a garbage-collectable object of a given class.
   The class must be constructed via SM_InitObjectClass. */
SM_EXTERN(SMObject*)
SM_AllocObject(SMObjectClass* clazz);

/* Allocates a garbage-collectable array object of a given class.
   The class must be constructed via SM_InitArrayClass. */
SM_EXTERN(SMArray*)
SM_AllocArray(SMArrayClass* clazz, PRUword size);

/* Allocates a garbage-collectable object of a given class, but with extra space
   at the end of the object. The size of the extra space is specified in bytes
   and is not examined by the garbage collector. A pointer to the beginning of
   the extra space can be obtained by SM_OBJECT_EXTRA(obj);
   The class must be constructed via SM_InitObjectClass. */
SM_EXTERN(SMObject*)
SM_AllocObjectExtra(SMObjectClass* clazz, PRUword extraBytes);

/* Returns the size of an object in bytes. This function cannot be used on 
   malloc'd objects. */
SM_EXTERN(PRUword)
SM_ObjectSize(SMObject* obj);

/* Returns a new object which is a shallow copy of the original. */
SM_EXTERN(SMObject*)
SM_Clone(SMObject* obj);

/* Forces a garbage collection of the entire heap. */
SM_EXTERN(void)
SM_Collect(void);

/* Forces finalization to occur. */
SM_EXTERN(void)
SM_RunFinalization(void);

/*******************************************************************************
 * Malloc and Friends
 *
 * These memory management routines may be used as an alternative to malloc,
 * free and associated routines. It is an exercise to the application 
 * programmer to cause these routines to be linked as _the_ malloc and free
 * routines for the application. The garbage collector itself uses these
 * routines internally and does not otherwise depend on malloc, free, etc.
 ******************************************************************************/

SM_EXTERN(void*)
SM_Malloc(PRUword size);

SM_EXTERN(void)
SM_Free(void* ptr);

SM_EXTERN(void*)
SM_Calloc(PRUword size, PRUword count);

SM_EXTERN(void*)
SM_Realloc(void* ptr, PRUword size);

SM_EXTERN(void*)
SM_Strdup(void* str);

SM_EXTERN(void*)
SM_Strndup(void* str, PRUword size);

/*******************************************************************************
 * Memory Pool variation of Malloc and Friends
 ******************************************************************************/

typedef struct SMPool SMPool;

SM_EXTERN(SMPool*)
SM_NewPool(void);

SM_EXTERN(void)
SM_DeletePool(SMPool* pool);

SM_EXTERN(void*)
SM_PoolMalloc(SMPool* pool, PRUword size);

SM_EXTERN(void)
SM_PoolFree(SMPool* pool, void* ptr);

SM_EXTERN(void*)
SM_PoolCalloc(SMPool* pool, PRUword size, PRUword count);

SM_EXTERN(void*)
SM_PoolRealloc(SMPool* pool, void* ptr, PRUword size);

SM_EXTERN(void*)
SM_PoolStrdup(SMPool* pool, void* str);

SM_EXTERN(void*)
SM_PoolStrndup(SMPool* pool, void* str, PRUword size);

/*******************************************************************************
 * Write Barrier
 *
 * All assignments to garbage collected object fields must go through the write
 * barrier. This allows the collector to focus on a smaller portion of the heap
 * during a collection (a "generation") while remaining informed about changes
 * made to objects of other generations.
 *
 * To set a field of an object or an element of an array, rather than writing:
 *
 *      obj->fieldName = value;
 *      arr[i] = value;
 *
 * one must instead write the following:
 *
 *      SM_SET_FIELD(ObjType, obj, fieldName, value);
 *      SM_SET_ELEMENT(ObjType, arr, i, value);
 *
 * Failure to use the SM_SET macros for all assignments will ultimately result
 * in system failure (i.e. a crash). But in order to aid in tracking down 
 * problems related to this, the SM_CHECK_PTRS preprocessor symbol may be
 * defined which causes all object pointers to be "encrypted" in such a way
 * that any attempt to access an object pointer outside of the SM_SET
 * macros will cause an immediate crash. Crashes of this sort point to places
 * in the code where the macros were forgotten. However, use of this encryption
 * technique also requires all read accesses to go through an equivalent pair
 * of macros, e.g. rather than writing:
 *
 *      obj->fieldName
 *      arr[i]
 *
 * one must instead write the following:
 *
 *      SM_GET_FIELD(ObjType, obj, fieldName)
 *      SM_GET_ELEMENT(ObjType, arr, i)
 *
 ******************************************************************************/
 
SM_EXTERN(PRUint8*) SM_CardTable;

/* Accessors for debugging: */

#define SM_GET_ELEMENT(ObjType, obj, index)                      \
    (((ObjType*)SM_DECRYPT(obj))[index])                         \

#define SM_GET_FIELD(ObjType, obj, fieldName)                    \
    (((ObjType*)SM_DECRYPT(obj))->fieldName)                     \

/* Unsafe assignment -- only to be used if you really know what you're 
   doing, e.g. initializing a newly created object's fields: */

#define SM_SET_ELEMENT_UNSAFE(ObjType, obj, index, value)        \
    (SM_GET_ELEMENT(ObjType, obj, index) = (value))              \

#define SM_SET_FIELD_UNSAFE(ObjType, obj, fieldName, value)      \
    (SM_GET_FIELD(ObjType, obj, fieldName) = (value))            \

#ifdef SM_NO_WRITE_BARRIER

#define SM_OBJECT_CARDDESC(obj)         NULL 
#define SM_DIRTY_PAGE(addr)             ((void)0)
#define SM_CLEAN_PAGE(addr)             ((void)0)

#else /* !SM_NO_WRITE_BARRIER */

#define SM_OBJECT_CARDDESC(obj)         (&SM_CardTable[SM_PAGE_NUMBER(SM_DECRYPT_OBJECT(obj))])
#define SM_CARDDESC_GEN(cd)             ((SMGenNum)((cd) - 1))
#define SM_GEN_CARDDESC(genNum)         ((PRUint8)(genNum) + 1)

#ifdef DEBUG
SM_EXTERN(void) SM_DirtyPage(void* addr);
SM_EXTERN(void) SM_CleanPage(void* addr);
#define SM_DIRTY_PAGE(addr)     SM_DirtyPage(addr)
#define SM_CLEAN_PAGE(addr)     SM_CleanPage(addr)
#else /* DEBUG */
#define SM_DIRTY_PAGE(addr)     (*SM_OBJECT_CARDDESC(addr) = 0)
#define SM_CLEAN_PAGE(addr)     (*SM_OBJECT_CARDDESC(addr) = SM_GEN_CARDDESC(SMGenNum_Free))
#endif /* DEBUG */

#endif /* !SM_NO_WRITE_BARRIER */

/* Safe assignment that uses the write barrier: */

#define SM_SET_ELEMENT(ObjType, obj, index, value)        \
{                                                         \
    ObjType* _obj = obj;                                  \
    SM_SET_ELEMENT_UNSAFE(ObjType, _obj, index, value);   \
    SM_DIRTY_PAGE(_obj);                                  \
}                                                         \

#define SM_SET_FIELD(ObjType, obj, fieldName, value)      \
{                                                         \
    ObjType* _obj = obj;                                  \
    SM_SET_FIELD_UNSAFE(ObjType, _obj, fieldName, value); \
    SM_DIRTY_PAGE(_obj);                                  \
}                                                         \

/******************************************************************************/

#if defined(DEBUG) || defined(SM_DUMP)

SM_EXTERN(void)
SM_SetTraceOutputFile(FILE* out);

#endif /* defined(DEBUG) || defined(SM_DUMP) */

/******************************************************************************/

SM_END_EXTERN_C

#endif /* __SM__ */
/******************************************************************************/
