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

#ifndef __SMOBJ__
#define __SMOBJ__

#include "sm.h"
#include "smpool.h"
#include <stdio.h>

SM_BEGIN_EXTERN_C

/*******************************************************************************
 * Instances
 ******************************************************************************/

struct SMObjStruct {
    SMClass*    clazz;
    void*       subHeader;
    /* fields follow... */
};

struct SMArrayStruct {
    SMClass*    clazz;
    void*       subHeader;
    PRUword     size;
    /* elements follow... */
};

#if defined(DEBUG) && !defined(SM_CHECK_PTRS)

/* so we can see what's going on in the debugger... */

struct SMObject {
    SMClass*    clazz;
    void*       subHeader;
    SMObject*   field0;
    SMObject*   field1;
    SMObject*   field2;
    SMObject*   field3;
};

struct SMArray {
    SMClass*    clazz;
    void*       subHeader;
    PRUword     size;
    SMObject*   element0;
    SMObject*   element1;
    SMObject*   element2;
    SMObject*   element3;
};

#else

struct SMObject {
    PRUword     encrypted;
};

struct SMArray {
    PRUword     encrypted;
};

#endif

/******************************************************************************/

#define SM_OBJECT_CLASS(obj)    (SM_DECRYPT_OBJECT(obj)->clazz)
#define SM_OBJECT_FIELDS(obj)   ((SMObject**)SM_DECRYPT_OBJECT(obj))
#define SM_OBJECT_EXTRA(obj)                                    \
    ((void*)((char*)SM_DECRYPT_OBJECT(obj)                      \
             + SM_OBJECT_CLASS(obj)->object.data.instanceSize)) \

#define SM_ARRAY_CLASS(arr)     (SM_DECRYPT_ARRAY(arr)->clazz)
#define SM_ARRAY_SIZE(arr)      (SM_DECRYPT_ARRAY(arr)->size)
#define SM_ARRAY_ELEMENTS(arr)                                           \
    ((SMObject**)((char*)SM_DECRYPT_ARRAY(arr) + sizeof(SMArrayStruct))) \

#ifdef SM_DEBUG_HEADER

/* Just use the subHeader field for this for now (until we get the 
   new monitors). */
#define SM_OBJECT_DEBUG_CODE            0xFADE
#define SM_CHECK_DEBUG_HEADER(word)     (((PRUword)(word) >> 16) == SM_OBJECT_DEBUG_CODE)
#define SM_SET_DEBUG_HEADER(obj, bucket, gen)                         \
    (obj->subHeader =                                                 \
     (void*)((SM_OBJECT_DEBUG_CODE << 16) | ((bucket) << 8) | (gen))) \

#else

#define SM_CHECK_DEBUG_HEADER(word)             0
#define SM_SET_DEBUG_HEADER(obj, bucket, gen)   ((void)0)
 
#endif

/*******************************************************************************
 * Class Objects
 *
 * Class objects describe the layout of instances and act as vtables for 
 * method dispatch. They are lower-level than their Java equivalent and only
 * contain the necessary information for execution, not reflection, etc.
 *
 * The "info" field is private to the garbage collector. The "fieldsDescs" 
 * field points to an array of field descriptors (described above) and may be
 * NULL to indicate that there are no collectable fields in the object.
 *
 * Interface methods in Java are implemented by a mechanism external to this
 * representation.
 ******************************************************************************/

typedef struct SMClassStruct {
    SMObjStruct         object;
    PRUword             info;
    /* class-kind specific fields follow */
} SMClassStruct;

#define SM_CLASS_KIND_BITS      4
#define SM_CLASS_KIND_MASK      ((1 << SM_CLASS_KIND_BITS) - 1)
#define SM_CLASS_KIND(cl)       (((SMClassStruct*)(cl))->info & SM_CLASS_KIND_MASK)

#define SM_CLASS_BUCKET_BITS    4
#define SM_CLASS_BUCKET_MASK    ((1 << SM_CLASS_KIND_BITS) - 1)
#define SM_CLASS_BUCKET(cl)                                          \
    ((SMBucket)((((SMClassStruct*)(cl))->info >> SM_CLASS_KIND_BITS) \
                & SM_CLASS_BUCKET_MASK))                             \

#define SM_CLASS_SET_BUCKET(cl, b)                                   \
    (SM_ASSERT((b) <= SM_CLASS_BUCKET_MASK),                         \
     ((SMClassStruct*)(cl))->info |= ((b) << SM_CLASS_KIND_BITS))    \

#define SM_CLASS_INIT_INFO(cl, bucket, kind, userData)               \
    (SM_ASSERT((bucket) <= SM_CLASS_BUCKET_MASK),                    \
     SM_ASSERT((kind) <= SM_CLASS_KIND_MASK),                        \
     (((SMClassStruct*)(cl))->info =                                 \
      (userData) | ((bucket) << SM_CLASS_KIND_BITS) | (kind)))       \

/* The upper bits of the info field may be freely used by the user program */
#define SM_CLASS_USER_INFO_FIRST_BIT    (SM_CLASS_KIND_BITS - SM_CLASS_BUCKET_BITS)
#define SM_CLASS_USER_INFO_BITS \
    ((sizeof(PRUword) * 8) - SM_CLASS_USER_INFO_FIRST_BIT)
#define SM_CLASS_USER_INFO(cl)  ((cl)->info >> SM_CLASS_USER_INFO_BITS)

typedef enum SMClassKind {
    /* Primitive class kinds: */
    SMClassKind_VoidClass,
    SMClassKind_BooleanClass,
    SMClassKind_UByteClass,     /* Waldemar likes this type although it seems out of place */
    SMClassKind_ByteClass,
    SMClassKind_CharClass,
    SMClassKind_ShortClass,
    SMClassKind_IntClass,
    SMClassKind_LongClass,
    SMClassKind_FloatClass,
    SMClassKind_DoubleClass,
    /* Structured class kinds: */
    SMClassKind_ArrayClass,
    SMClassKind_ObjectClass,
    SMClassKind_InterfaceClass
} SMClassKind;

#define SM_CLASS_KIND_COUNT     (SMClassKind_InterfaceClass + 1)

extern PRUint8 sm_ClassKindSize[SM_CLASS_KIND_COUNT];

#define SM_ELEMENT_SIZE(cl)     (sm_ClassKindSize[SM_CLASS_KIND(cl)])

/*******************************************************************************
 * Field Descriptors
 *
 * Classes contain an array of field descriptors, one for each contiguous 
 * group of garbage-collectable fields. Field offsets are relative to the start
 * of the object (i.e. the class descriptor), and are in units of SMObject* 
 * pointers (not bytes).
 ******************************************************************************/

typedef struct SMFieldDesc {
    PRWord      offset;         /* number of SMObject* pointers from base */
    PRUword     count;          /* number of pointers at offset */
} SMFieldDesc;

/* This data structure describes where to find the  beginning of an array
   of field descriptors relative to the beginning of a class object. It
   also contains the count of descriptors. */
typedef struct SMFieldDescInfo {
    PRUint16    offset; /* start of field descriptors relative to beginning of class */
    PRUint16    count;  /* number of field descriptors at offset */
} SMFieldDescInfo;

#define SM_CLASS_GET_FIELD_DESCS(cl, fdInfo)         \
    ((SMFieldDesc*)((char*)(cl) + (fdInfo)->offset)) \

/*******************************************************************************
 * Standard Methods
 *
 * These methods exist on all instances, some of which are used by the garbage
 * collector internally.
 ******************************************************************************/

typedef void
(PR_CALLBACK *SMFinalizeFun)(SMObject* obj);

/*******************************************************************************
 * Object Class Objects
 ******************************************************************************/

typedef struct SMObjectClassData {
    SMObject*           staticFields; /* an array object */
    SMFieldDescInfo     instFieldDescs;
    SMFieldDescInfo     weakFieldDescs;
    PRUword             instanceSize;
    SMFinalizeFun       finalize;
} SMObjectClassData;

struct SMObjectClass {
    SMClassStruct       inherit;
    SMObjectClassData   data;
};

#define SM_CLASS_INSTANCE_SIZE(cl)      (((SMClass*)(cl))->object.data.instanceSize)
#define SM_CLASS_FINALIZER(cl)          (((SMClass*)(cl))->object.data.finalize)
#define SM_CLASS_IS_FINALIZABLE(cl)     (SM_CLASS_FINALIZER(cl) != NULL)

#define SM_CLASS_GET_INST_FIELDS_COUNT(cl)                          \
    (((SMClass*)(cl))->object.data.instFieldDescs.count)            \

#define SM_CLASS_GET_INST_FIELD_DESCS(cl)                           \
    SM_CLASS_GET_FIELD_DESCS(cl, &(cl)->object.data.instFieldDescs) \

#define SM_CLASS_GET_WEAK_FIELDS_COUNT(cl)                          \
    (((SMClass*)(cl))->object.data.weakFieldDescs.count)            \

#define SM_CLASS_GET_WEAK_FIELD_DESCS(cl)                           \
    SM_CLASS_GET_FIELD_DESCS(cl, &(cl)->object.data.weakFieldDescs) \

/*******************************************************************************
 * Array Class Objects
 ******************************************************************************/

typedef struct SMArrayClassData {
    SMClass*            elementClass; /* same position as staticFields, above */
} SMArrayClassData;

struct SMArrayClass {
    SMClassStruct       inherit;
    SMArrayClassData    data;
};

#define SM_CLASS_ELEMENT_CLASS(cl)      (((SMClass*)(cl))->array.data.elementClass)

/*******************************************************************************
 * Primitive Class Objects
 ******************************************************************************/

typedef struct SMPrimClassData {
    /* no other fields */
    char                dummy;
} SMPrimClassData;

struct SMPrimClass {
    SMClassStruct       inherit;
    SMPrimClassData     data;
};

/******************************************************************************/

union SMClass {
    SMObjectClass       object;
    SMArrayClass        array;
    SMPrimClass         prim;
};

/* Class structure excluding portion from SMObject: */
typedef struct SMClassData {
    PRUword                 info;
    union {
        SMObjectClassData   object;
        SMArrayClassData    array;
        SMPrimClassData     prim;
    } type;
} SMClassData;

/******************************************************************************/

#define SM_IS_OBJECT_CLASS(cl)                         \
    (SM_CLASS_KIND(cl) > SMClassKind_ArrayClass)       \

#define SM_IS_ARRAY_CLASS(cl)                          \
    (SM_CLASS_KIND(cl) == SMClassKind_ArrayClass)      \

#define SM_IS_PRIM_CLASS(cl)                           \
    (SM_CLASS_KIND(cl) < SMClassKind_ArrayClass)       \

#define SM_IS_OBJECT_ARRAY_CLASS(cl)                   \
    (SM_CLASS_KIND(cl) == SMClassKind_ArrayClass       \
     && !SM_IS_PRIM_CLASS(SM_CLASS_ELEMENT_CLASS(cl))) \

#define SM_IS_PRIMITIVE_ARRAY_CLASS(cl)                \
    (SM_CLASS_KIND(cl) == SMClassKind_ArrayClass       \
     && SM_IS_PRIM_CLASS(SM_CLASS_ELEMENT_CLASS(cl)))  \

#define SM_IS_OBJECT(obj)                              \
    (SM_IS_OBJECT_CLASS(SM_OBJECT_CLASS(obj)))         \

#define SM_IS_ARRAY(obj)                               \
    (SM_IS_ARRAY_CLASS(SM_OBJECT_CLASS(obj)))          \

/******************************************************************************/

SM_EXTERN(void)
SM_InitObjectClass(SMObjectClass* clazz, SMObjectClass* metaClass, 
                   PRUword classSize, PRUword instanceSize,
                   PRUint16 nInstFieldDescs, PRUint16 nWeakFieldDescs);

SM_EXTERN(void)
SM_InitArrayClass(SMArrayClass* clazz, SMObjectClass* metaClass,
                  SMClass* elementClass);

SM_EXTERN(void)
SM_InitPrimClass(SMPrimClass* clazz, SMObjectClass* metaClass,
                 SMClassKind primKind);

/******************************************************************************/

SM_END_EXTERN_C

#endif /* __SMOBJ__ */
/******************************************************************************/
