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

#include "smobj.h"
#include "smgen.h"
#include "smheap.h"

PRUint8 sm_ClassKindSize[SM_CLASS_KIND_COUNT] = {
    /* Primitive class kinds: */
    0,                  /* SMClassKind_VoidClass */
    sizeof(PRInt8),     /* SMClassKind_BooleanClass */
    sizeof(PRInt8),     /* SMClassKind_ByteClass */
    sizeof(PRInt16),    /* SMClassKind_CharClass */
    sizeof(PRInt16),    /* SMClassKind_ShortClass */
    sizeof(PRInt32),    /* SMClassKind_IntClass */
    sizeof(PRInt64),    /* SMClassKind_LongClass */
    sizeof(float),      /* SMClassKind_FloatClass */
    sizeof(double),     /* SMClassKind_DoubleClass */
    /* Structured class kinds: */
    sizeof(SMArray*),   /* SMClassKind_ArrayClass */
    sizeof(SMObject*),  /* SMClassKind_ObjectClass */
    sizeof(SMObject*)   /* SMClassKind_InterfaceClass */
};

SM_IMPLEMENT_ENSURE(SMObject)
SM_IMPLEMENT_ENSURE(SMObjStruct)
SM_IMPLEMENT_ENSURE(SMArray)
SM_IMPLEMENT_ENSURE(SMArrayStruct)

/******************************************************************************/

SM_IMPLEMENT(void)
SM_InitObjectClass(SMObjectClass* clazz, SMObjectClass* metaClass, 
                   PRUword classSize, PRUword instanceSize,
                   PRUint16 nInstFieldDescs, PRUint16 nWeakFieldDescs)
{
    SMBucket bucket;

    SM_ASSERT(classSize <= (1 << (sizeof(PRUint16) * 8)));

    clazz->inherit.object.clazz = (SMClass*)metaClass;
    SM_GET_ALLOC_BUCKET(bucket, instanceSize);
    SM_CLASS_INIT_INFO(clazz, bucket, SMClassKind_ObjectClass, 0);
    clazz->data.staticFields = NULL;
    clazz->data.instFieldDescs.offset = (PRUint16)classSize;
    clazz->data.instFieldDescs.count = nInstFieldDescs;
    clazz->data.weakFieldDescs.offset =
        (PRUint16)classSize + nInstFieldDescs * sizeof(SMFieldDesc);
    clazz->data.weakFieldDescs.count = nWeakFieldDescs;
    clazz->data.instanceSize = instanceSize;
    clazz->data.finalize = NULL;
}

SM_IMPLEMENT(void)
SM_InitArrayClass(SMArrayClass* clazz, SMObjectClass* metaClass,
                  SMClass* elementClass)
{
    clazz->inherit.object.clazz = (SMClass*)metaClass;
    SM_CLASS_INIT_INFO(clazz, 0, SMClassKind_ArrayClass, 0);
    clazz->data.elementClass = elementClass;
}

SM_IMPLEMENT(void)
SM_InitPrimClass(SMPrimClass* clazz, SMObjectClass* metaClass,
                 SMClassKind primKind)
{
    clazz->inherit.object.clazz = (SMClass*)metaClass;
    SM_CLASS_INIT_INFO(clazz, 0, primKind, 0);
}

/******************************************************************************/
