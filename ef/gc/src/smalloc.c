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

/******************************************************************************/

SMObjStruct*
sm_AddGCPage(SMGenNum genNum, SMBucket bucket)
{
    SMObjStruct* obj = NULL;
    SMPage* page = SM_NEW_PAGE(&sm_PageMgr);
    if (page) {
        SMSmallObjSize objSize = sm_ObjectSize[bucket];
        SMSmallObjCount objTableSize = sm_ObjectsPerPage[bucket];
        SMObjDesc* objTable;

        if (bucket >= SM_FIRST_MIDSIZE_BUCKET) {
            /* add an extra object descriptor for any extra space at the
               end of the page due to a non-integral number of objects */
            objTableSize++;
        }
        SM_ASSERT((objTableSize * objSize) >= SM_PAGE_SIZE);
        objTable = (SMObjDesc*)SM_Malloc(sizeof(SMObjDesc) * objTableSize);
        if (objTable == NULL) {
            SM_DESTROY_PAGE(&sm_PageMgr, page);
            return NULL;
        }
        obj = (SMObjStruct*)sm_PoolInitPage(&sm_Heap.gen[genNum].pool, 
                                            genNum, bucket, page, objTable, 0);
    }
    return obj;
}

/*******************************************************************************
 * Allocator
 ******************************************************************************/

SMObjStruct*
sm_Alloc(PRUword instanceSize, SMBucket bucket)
{
    SMObjStruct* obj;
    SMGen* gen = &sm_Heap.gen[SMGenNum_Freshman];
    SMGenNum collectGenNum = SMGenNum_Free;
    PRUword objSize;
    
    /* First force a collect if we've crossed our allocation threshold.
       Collect the oldest generations that has crossed its threshold,
       and all younger generations. */
    if (gen->pool.allocAmount >= gen->allocThreshold) {
        SMGenNum i;
        for (collectGenNum = SMGenNum_Freshman; collectGenNum <= SMGenNum_Static; collectGenNum++) {
            gen = &sm_Heap.gen[collectGenNum];
            if (gen->pool.allocAmount < gen->allocThreshold)
                break;
        }
        --collectGenNum;
        sm_Collect0(collectGenNum, SM_IS_COPY_CYCLE(collectGenNum));
        /* reset allocation cnnnounts */
        for (i = SMGenNum_Freshman; i < collectGenNum; i++) {
            sm_Heap.gen[i].pool.allocAmount = 0;
        }
    }

    gen = &sm_Heap.gen[SMGenNum_Freshman];

    if (instanceSize > SM_MAX_SMALLOBJECT_SIZE) {
        do {
            SM_GEN_ALLOC_LARGE_OBJ(obj, gen, SMGenNum_Freshman, instanceSize);
        } while (obj == NULL &&
                 (sm_CompactHook ? sm_CompactHook(instanceSize) : PR_FALSE));
        if (obj == NULL)
            return NULL;

        /* Don't add to the gen->pool.allocAmount for large objects --
           they don't contribute to triggering a gc. */

        objSize = SM_PAGE_WIDTH(SM_PAGE_COUNT(instanceSize));
        /* XXX Should large objects factor into the allocAmount that
           we use to cause a collection? */
    }
    else {
        do {
            SM_GEN_ALLOC_OBJ(obj, gen, bucket);
            if (obj == NULL) {
                /* no objects on the free list -- try allocating a new page */
                obj = sm_AddGCPage(SMGenNum_Freshman, bucket);
            }
        } while (obj == NULL &&
                 (sm_CompactHook ? sm_CompactHook(sm_ObjectSize[bucket]) : PR_FALSE));
        if (obj == NULL)
            return NULL;
        objSize = sm_ObjectSize[bucket];
        gen->pool.allocAmount += objSize;
    }
#ifdef DEBUG
    {
        SMPageDesc* pd = SM_OBJECT_PAGEDESC(obj);
        SMObjDesc* od = SM_OBJECT_HEADER_FROM_PAGEDESC(obj, pd);
        SM_ASSERT(SM_OBJDESC_IS_ALLOCATED(od));
    }
#endif
    SM_POOL_SET_ALLOC_STATS(&gen->pool, objSize, instanceSize);
    DBG_MEMSET(obj, SM_UNUSED_PATTERN, instanceSize);
    SM_SET_DEBUG_HEADER(obj, bucket, SMGenNum_Freshman);
    return obj;
}

#define SM_INIT_OBJECT(obj, clazz)                                   \
{                                                                    \
    SMObject* _obj = (obj);                                          \
    SMClass* _clazz = (SMClass*)(clazz);                             \
    /* Initialize reference fields */                                \
    SMFieldDesc* _fieldDesc = SM_CLASS_GET_INST_FIELD_DESCS(_clazz); \
    PRUint16 i, _fdCount = SM_CLASS_GET_INST_FIELDS_COUNT(_clazz);   \
    for (i = 0; i < _fdCount; i++) {                                 \
        PRWord _offset = _fieldDesc[i].offset;                       \
        PRUword _count = _fieldDesc[i].count;                        \
        SMObject** _fieldRefs = &SM_OBJECT_FIELDS(_obj)[_offset];    \
        SM_UNROLLED_WHILE(_count, {                                  \
            *_fieldRefs++ = NULL;                                    \
        });                                                          \
    }                                                                \
    SM_OBJECT_CLASS(_obj) = _clazz;                                  \
    if (SM_CLASS_IS_FINALIZABLE(_clazz)) {                           \
        SMObjStruct* _dobj = SM_DECRYPT_OBJECT(_obj);                \
        SMPageDesc* _pd = SM_OBJECT_PAGEDESC(_dobj);                 \
        SMObjDesc* _od = SM_OBJECT_HEADER_FROM_PAGEDESC(_dobj, _pd); \
        SM_OBJDESC_SET_FINALIZABLE(_od);                             \
    }                                                                \
}                                                                    \

SM_IMPLEMENT(SMObject*)
SM_AllocObject(SMObjectClass* clazz)
{
    SMObjStruct* obj;
    PRUword instanceSize = SM_CLASS_INSTANCE_SIZE(clazz);
    SMBucket bucket = SM_CLASS_BUCKET(clazz);
    PRMonitor* mon = sm_Heap.gen[SMGenNum_Freshman].pool.sweepList[bucket].monitor;
    
    SM_ASSERT(SM_IS_OBJECT_CLASS(clazz));

    PR_EnterMonitor(mon);
    obj = sm_Alloc(instanceSize, bucket);
    if (obj) {
        SMObject* eobj = SM_ENCRYPT_OBJECT(obj);
        SM_INIT_OBJECT(eobj, clazz);
    }
    SM_VERIFY_HEAP();
    PR_ExitMonitor(mon);
    return SM_ENCRYPT_OBJECT(obj);
}

/******************************************************************************/

SM_IMPLEMENT(SMArray*)
SM_AllocArray(SMArrayClass* clazz, PRUword size)
{
    SMArrayStruct* arr;
    PRUword instanceSize;
    SMBucket bucket;
    PRMonitor* mon;
    int eltSize = SM_ELEMENT_SIZE(clazz);

    SM_ASSERT(SM_IS_ARRAY_CLASS(clazz));

    instanceSize = sizeof(SMArrayStruct) + (eltSize * size);
#ifdef DEBUG
    /* In debug mode, we'll store a pointer back to the beginning
       of the array in the last element of the array. This lets us
       easily see where the array ends in memory. */
    if (eltSize == sizeof(void*)) {
        instanceSize += sizeof(void*);
    }
#endif
    SM_GET_ALLOC_BUCKET(bucket, instanceSize);

    mon = sm_Heap.gen[SMGenNum_Freshman].pool.sweepList[bucket].monitor;
    PR_EnterMonitor(mon);
    arr = (SMArrayStruct*)sm_Alloc(instanceSize, bucket);
    if (arr) {
        SMArray* earr = SM_ENCRYPT_ARRAY(arr);
        SM_ARRAY_CLASS(earr) = (SMClass*)clazz;
        SM_ARRAY_SIZE(earr) = size;
        if (SM_IS_OBJECT_ARRAY_CLASS(clazz)) {
            SMObject** elements = SM_ARRAY_ELEMENTS(earr);
            SM_UNROLLED_WHILE(size, {
                SM_ASSERT(!SM_CHECK_DEBUG_HEADER(*elements));
                *elements++ = NULL;
            });
#ifdef DEBUG
            if (eltSize == sizeof(void*)) {
                *elements = (SMObject*)arr;
            }
#endif
        }
    }
    SM_VERIFY_HEAP();
    PR_ExitMonitor(mon);
    return SM_ENCRYPT_ARRAY(arr);
}

/******************************************************************************/

SM_IMPLEMENT(SMObject*)
SM_AllocObjectExtra(SMObjectClass* clazz, PRUword extraBytes)
{
    SMObjStruct* obj;
    PRUword instanceSize;
    SMBucket bucket;
    PRMonitor* mon;
    
    SM_ASSERT(SM_IS_OBJECT_CLASS(clazz));

    instanceSize = SM_CLASS_INSTANCE_SIZE(clazz) + extraBytes;
    SM_GET_ALLOC_BUCKET(bucket, instanceSize);
    
    mon = sm_Heap.gen[SMGenNum_Freshman].pool.sweepList[bucket].monitor;
    PR_EnterMonitor(mon);
    obj = sm_Alloc(instanceSize, bucket);
    if (obj) {
        SMObject* eobj = SM_ENCRYPT_OBJECT(obj);
        SM_INIT_OBJECT(eobj, clazz);
        memset(SM_OBJECT_EXTRA(eobj), 0, extraBytes);
    }
    SM_VERIFY_HEAP();
    PR_ExitMonitor(mon);
    return SM_ENCRYPT_OBJECT(obj);
}

/******************************************************************************/

SM_IMPLEMENT(PRUword)
SM_ObjectSize(SMObject* obj)
{
    SMClass* clazz = SM_OBJECT_CLASS(obj);
    PRUword size;
    SM_ASSERT(clazz);
    switch (SM_CLASS_KIND(clazz)) {
      case SMClassKind_ObjectClass: 
        size = SM_CLASS_INSTANCE_SIZE(clazz);
        break;
      case SMClassKind_ArrayClass:
        size = sizeof(SMArrayStruct) + 
            SM_ARRAY_SIZE((SMArray*)obj) * SM_ELEMENT_SIZE(clazz);
        break;
      default:
        size = 0;
    }
    SM_ASSERT(size <= sm_ObjectSize[SM_PAGEDESC_BUCKET(SM_OBJECT_PAGEDESC(SM_DECRYPT_OBJECT(obj)))]);
    return size;
}

SM_EXTERN(SMObject*)
SM_Clone(SMObject* obj)
{
    SMObjStruct* dobj = SM_DECRYPT_OBJECT(obj);
    SMPageDesc* pd = SM_OBJECT_PAGEDESC(dobj);
    SMBucket bucket = SM_PAGEDESC_BUCKET(pd);
    PRUword instanceSize;
    SMObjStruct* result;

    SM_OBJECT_GROSS_SIZE(&instanceSize, dobj);
    result = sm_Alloc(instanceSize, bucket);
    if (result) {
        memcpy(result, obj, instanceSize);
    }
    return SM_ENCRYPT_OBJECT(result);
}

/******************************************************************************/
