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
#ifdef XP_MAC
#include "prpriv.h"
#else
#include "private/prpriv.h"
#endif

#define SM_MARK_STACK_PUSH(obj)         SM_STACK_PUSH(&sm_Heap.markStack, obj)
#define SM_MARK_STACK_POP()             SM_STACK_POP(&sm_Heap.markStack)

static PRBool sm_conservOnly;

/*******************************************************************************
 * Collector
 ******************************************************************************/

#ifdef DEBUG

static void
SM_MARK_AND_PUSH(SMPageDesc* pd, SMBucket bucket, SMObjDesc* desc, SMObject* obj)
{
    SMPageDesc* _pd0 = (pd);
    SMBucket _bucket0 = (bucket);
    SMObjDesc* _desc0 = (desc);
    SMObject* _obj0 = (obj);
    SM_ASSERT(SM_OBJECT_CLASS(_obj0) != NULL);
    if (SM_OBJECT_CLASS(_obj0) == NULL) {
        int x = 0;
        x += 1;
    }
    SM_OBJDESC_SET_MARKED(_desc0);
    SM_ASSERT(SM_OBJECT_CLASS(_obj0));
    if (_bucket0 != SM_LARGE_OBJECT_BUCKET)
        _pd0->allocCount++;
    if (!SM_MARK_STACK_PUSH(_obj0)) {
        SM_OBJDESC_SET_UNTRACED(_desc0);
    }
}

static void
SM_COPY_OBJECT(SMObject** ref, SMPageDesc* pd, SMBucket bucket, SMGenNum objGen,
               SMObjDesc* desc, SMObject* obj)
{
    SMObject** _ref1 = (ref);
    SMPageDesc* _pd1 = (pd);
    SMBucket _bucket1 = (bucket);
    SMGenNum _objGen1 = (objGen);
    SMObjDesc* _desc1 = (desc);
    SMObject* _obj1 = (obj);
    if  (_objGen1 >= SMGenNum_Senior || !SM_OBJDESC_DO_COPY(_desc1)) {
        /* If the object is not yet copyable, then it hasn't lived */
        /* long enough to be copied. Just set the copyable bit, and */
        /* we'll try again next time. */
        SM_OBJDESC_SET_COPYABLE(_desc1);
        SM_MARK_AND_PUSH(_pd1, _bucket1, _desc1, _obj1);
    } 
    else if (_bucket1 == SM_LARGE_OBJECT_BUCKET) {
        /* For large objects, don't copy them but just increment the */
        /* generation number for all their pages. */
        SMPageCount _count1 = _pd1->allocCount;
        SM_ASSERT(!SM_OBJDESC_IS_FORWARDED(_desc1));
        SM_ASSERT(SM_PAGEDESC_IS_LARGE_OBJECT_START(_pd1));
        SM_UNROLLED_WHILE(_count1, {
            SM_PAGEDESC_INCR_GEN(_pd1);
            SM_ASSERT(SM_PAGEDESC_BUCKET(_pd1) == _bucket1);
            SM_ASSERT(SM_PAGEDESC_GEN(_pd1) < SMGenNum_Static);
            _pd1++;
        });
        SM_SET_DEBUG_HEADER(SM_DECRYPT_OBJECT(_obj1), _bucket1, _objGen1 + 1);
        /* We only dirty the first page of the object here because */
        /* currently the page scavenging routine processes all the */
        /* pages of a large object. We probably should make it smarter. */
        SM_MARK_AND_PUSH(_pd1, _bucket1, _desc1, _obj1);
    }
    else if (SM_OBJDESC_IS_FORWARDED(_desc1)) {
        *_ref1 = SM_OBJECT_FORWARDING_ADDR(_obj1);
    }
    else {
        /* Try to copy the object up too the next generation. */
        SMGenNum _newGenNum1 = (SMGenNum)((int)_objGen1 + 1);
        SMGen* _newGen1 = &sm_Heap.gen[_newGenNum1];
        PRUword _allocBefore1 = _newGen1->pool.allocAmount;
        SMObjStruct* _newObj1 = sm_GenPromoteObj(_newGen1, _bucket1);
        if (_newObj1 == NULL) {
            /* no objects on the free list -- try allocating a new page */
            _newObj1 = sm_AddGCPage(_newGenNum1, _bucket1);
        }
        if (_newObj1) {
            /* copy the object's contents */
            SMObject* _newEObj1 = SM_ENCRYPT_OBJECT(_newObj1);
            SMObjDesc* _newDesc1;
            SMSmallObjSize _objSize1 = sm_ObjectSize[_bucket1];
            SMObject** _from1 = SM_OBJECT_FIELDS(_obj1);
            SMObject** _to1 = SM_OBJECT_FIELDS(_newEObj1);
            SM_UNROLLED_WHILE(_objSize1 >> SM_REF_BITS,
                              *_to1++ = *_from1++);
            DBG_MEMSET(SM_DECRYPT_OBJECT(_obj1), SM_GC_FREE_PATTERN, _objSize1);
            SM_SET_DEBUG_HEADER(_newObj1, _bucket1, _newGenNum1);
            _newGen1->pool.allocAmount += _objSize1;
            SM_ASSERT(_allocBefore1 + _objSize1
                      == _newGen1->pool.allocAmount);
            SM_POOL_SET_ALLOC_STATS(
                &_newGen1->pool, _objSize1,
                SM_ObjectSize(_newEObj1));
            SM_OBJECT_SET_FORWARDING_ADDR(_obj1, _newEObj1);
            *_ref1 = _newEObj1;       /* update ref */
            
            /* finally, mark the new object */
            {
                SMPageCount _pageNum1 = SM_PAGE_NUMBER(_newObj1);
                SMSmallObjCount _pageOffset1 =
                    (SMSmallObjCount)SM_PAGE_OFFSET(_newObj1); 
                SMSmallObjCount _objNum1 = SM_DIV(_pageOffset1, _bucket1);
                _pd1 = &sm_PageMgr.pageTable[_pageNum1];
                _newDesc1 = &_pd1->objTable[_objNum1];
                if (SM_OBJDESC_IS_FINALIZABLE(_desc1)) {
                    SM_OBJDESC_SET_FINALIZABLE(_newDesc1);
                }
                SM_ASSERT(SM_OBJDESC_IS_MARKED(_newDesc1));
                if (!SM_MARK_STACK_PUSH(_newEObj1)) {
                    SM_OBJDESC_SET_UNTRACED(_newDesc1);
                }
                SM_OBJDESC_SET_FORWARDED(_desc1);
            }
        }
        else {
            /* If we couldn't allocate a new copy, then just mark it. */
            SM_MARK_AND_PUSH(_pd1, _bucket1, _desc1, _obj1);
        }
    }
}

#ifndef SM_NO_WRITE_BARRIER

static void
SM_UPDATE_WRITE_BARRIER(SMGenNum _objGen2, SMCardDesc* containingCardDesc)
{
    SMCardDesc _cd2 = *(containingCardDesc); 
    SMGenNum _cardGen2 = SM_CARDDESC_GEN(_cd2); 
    if (_objGen2 < _cardGen2) {
        SM_CARDDESC_SET_GEN(containingCardDesc, _objGen2); 
    } 
}

#else /* SM_NO_WRITE_BARRIER */

static void
SM_UPDATE_WRITE_BARRIER(SMGenNum _objGen2, SMCardDesc* containingCardDesc)
{
    /* no-op */
}

#endif /* SM_NO_WRITE_BARRIER */

static void
SM_SCAVENGE_OBJECT(SMObject** ref, SMGenNum genNum, PRBool copyCycle,
                   SMCardDesc* containingCardDesc)
{
    SMObject* _obj2 = *(ref); 
    SMObjStruct* _dobj2 = SM_DECRYPT_OBJECT(_obj2);
    if (_dobj2) {
        SMPageCount _pageNum2 = SM_PAGE_NUMBER(_dobj2); 
        SMPageDesc* _pd2 = &sm_PageMgr.pageTable[_pageNum2]; 
        SMGenNum _objGen2 = SM_PAGEDESC_GEN(_pd2); 
        if (copyCycle) {
            SM_UPDATE_WRITE_BARRIER(_objGen2, containingCardDesc);
        } 
        if (_objGen2 <= genNum) {
            SMSmallObjCount _pageOffset2 =
                (SMSmallObjCount)SM_PAGE_OFFSET(_dobj2); 
            SMBucket _bucket2 = SM_PAGEDESC_BUCKET(_pd2); 
            SMSmallObjCount _objNum2 = SM_DIV(_pageOffset2, _bucket2); 
            SMObjDesc* _desc2 = &_pd2->objTable[_objNum2]; 
            SM_ASSERT(_bucket2 != SM_LARGE_OBJECT_BUCKET 
                      || SM_PAGEDESC_IS_LARGE_OBJECT_START(_pd2)); 
            if (!SM_OBJDESC_IS_MARKED(_desc2)) {
                if (copyCycle) {
                    SM_COPY_OBJECT(ref, _pd2, _bucket2, _objGen2, 
                                   _desc2, _obj2); 
                } 
                else {
                    SM_MARK_AND_PUSH(_pd2, _bucket2, _desc2, _obj2); 
                } 
            } 
        } 
    } 
}

static void
SM_SCAVENGE_RANGE(SMObject** refs, PRUword count, SMGenNum genNum,
                  PRBool copyCycle, SMCardDesc* containingCardDesc)
{
    SMObject** _ref3 = (refs);
    PRUword _count3 = (count);
    while (_count3-- > 0) {                                            
        SM_SCAVENGE_OBJECT(_ref3, genNum, copyCycle, containingCardDesc);
        _ref3++;
    }
}

static void
SM_SCAVENGE_FIELDS(SMObject* obj, SMGenNum genNum, 
                   PRBool copyCycle, SMCardDesc* objCardDesc)
{
    SMObject* _obj4 = (obj);
    SMClass* _clazz4 = SM_OBJECT_CLASS(_obj4);

    SM_ASSERT(_clazz4 != NULL);
    switch (SM_CLASS_KIND(_clazz4)) {
      case SMClassKind_ObjectClass: {
          SMFieldDesc* _fieldDesc4 = SM_CLASS_GET_INST_FIELD_DESCS(_clazz4);
          PRUint16 _i4, _fdCount4 = SM_CLASS_GET_INST_FIELDS_COUNT(_clazz4);
          for (_i4 = 0; _i4 < _fdCount4; _i4++) {
              PRWord _offset4 = _fieldDesc4[_i4].offset; 
              PRUword _count4 = _fieldDesc4[_i4].count; 
              SMObject** _fieldRefs4 = &SM_OBJECT_FIELDS(_obj4)[_offset4];
              SM_SCAVENGE_RANGE(_fieldRefs4, _count4, genNum,
                                copyCycle, objCardDesc);
          }
          break;
      }
      case SMClassKind_ArrayClass: {
          if (SM_IS_OBJECT_ARRAY_CLASS(_clazz4)) {
              SMArray* _arr4 = (SMArray*)SM_ENSURE(SMObject, _obj4);
              PRUword _size4 = SM_ARRAY_SIZE(_arr4);
              SMObject** _elementRefs4 = SM_ARRAY_ELEMENTS(_arr4);
              SM_SCAVENGE_RANGE(_elementRefs4, _size4, genNum,
                                copyCycle, objCardDesc);
          }
          break;
      }
      default:
        break;
    }
}

static void
SM_DRAIN_MARK_STACK(SMGenNum genNum, PRBool copyCycle)
{
    SMObject* _obj5;
    while ((_obj5 = SM_MARK_STACK_POP()) != NULL) {
        SMCardDesc* _cardDesc5 = SM_OBJECT_CARDDESC(_obj5);
        SM_SCAVENGE_FIELDS(_obj5, genNum, copyCycle, _cardDesc5);
    }   
}

static void
SM_SCAVENGE_ALL(SMObject** ref, SMGenNum genNum, 
                PRBool copyCycle, SMCardDesc* containingCardDesc)
{
    SMObject** _ref6 = (ref);
    SM_SCAVENGE_OBJECT(_ref6, genNum, copyCycle, containingCardDesc);
    SM_DRAIN_MARK_STACK(genNum, copyCycle);
}

static void
SM_SCAVENGE_PAGE(SMPageDesc* pd, SMGenNum genNum,
                 PRBool copyCycle, SMCardDesc* objCardDesc)
{
    SMSmallObjCount _j7; 
    SMPageDesc* _pd7 = (pd); 
    SMBucket _bucket7 = SM_PAGEDESC_BUCKET(_pd7);
    SMSmallObjCount _objsPerPage7 = sm_ObjectsPerPage[_bucket7]; 
    SMSmallObjSize _objSize7 = sm_ObjectSize[_bucket7]; 
    SMObjStruct* _obj7 = (SMObjStruct*)SM_PAGEDESC_PAGE(_pd7); 
    SMObjDesc* _objTable7 = _pd7->objTable;
    SM_ASSERT(_bucket7 == SM_LARGE_OBJECT_BUCKET
              ? SM_PAGEDESC_IS_LARGE_OBJECT_START(_pd7)
              : 1);
    for (_j7 = 0; _j7 < _objsPerPage7; _j7++) {
        SMObjDesc* _od7 = &_objTable7[_j7]; 
        SMObject* _eobj7 = SM_ENCRYPT_OBJECT(_obj7);
        if (SM_OBJDESC_IS_ALLOCATED(_od7)) {
            SM_ASSERT(SM_OBJECT_CARDDESC(_eobj7) == objCardDesc);
            SM_SCAVENGE_FIELDS(_eobj7, genNum, copyCycle, objCardDesc);
        } 
        _obj7 = (SMObjStruct*)((char*)_obj7 + _objSize7); 
    } 
}

static void
SM_SCAVENGE_CONSERV(SMObject* obj, SMGenNum genNum, PRBool copyCycle)
{
    SMObject* _obj8 = (obj);
    SMObjStruct* _dobj8 = SM_DECRYPT_OBJECT(_obj8);

    if (_dobj8 && SM_IN_HEAP(_dobj8)) {
        SMPageCount _pageNum8 = SM_PAGE_NUMBER(_dobj8);                             
        SMPageDesc* _pd8 = &sm_PageMgr.pageTable[_pageNum8];
        SMGenNum _objGen8 = SM_PAGEDESC_GEN(_pd8);

        /* Blacklist pages even outside the generation we care about. This */
        /* allows the page manager to prevent giving out blacklisted pages */
        /* unless really necessary. */
        SM_PAGEDESC_SET_BLACKLISTED(_pd8);
        
        if (_objGen8 <= genNum) {
            SMSmallObjCount _pageOffset8 =
                (SMSmallObjCount)SM_PAGE_OFFSET(_dobj8); 
            SMBucket _bucket8 = SM_PAGEDESC_BUCKET(_pd8);               
            SMSmallObjCount _objNum8 = SM_DIV(_pageOffset8, _bucket8); 
            SMObjDesc* _desc8 = &_pd8->objTable[_objNum8];                 
            if (SM_OBJDESC_IS_UNMARKED(_desc8)) {
                /* If this is a conservative root, then all we can */
                /* do is pin the object, and allow it to get marked. */
                SM_ASSERT(!SM_OBJDESC_WAS_FREE(_desc8));
                if (copyCycle)
                    SM_OBJDESC_SET_COPYABLE_BUT_PINNED(_desc8);
                else
                    SM_OBJDESC_SET_PINNED(_desc8);
                    
                /* Adjust address of object so that we can find it's fields. */
                if (_bucket8 == SM_LARGE_OBJECT_BUCKET) {
                    if (!SM_PAGEDESC_IS_LARGE_OBJECT_START(_pd8))
                        /* go back to first page desc */
                        _pd8 = SM_PAGEDESC_LARGE_OBJECT_START(_pd8);
                    SM_ASSERT(_pd8->allocCount != 0);
                    _dobj8 = (SMObjStruct*)SM_PAGEDESC_PAGE(_pd8);
                }
                else {
                    SMPage* _page8 = SM_PAGEDESC_PAGE(_pd8);
                    SMSmallObjSize _objSize8 = sm_ObjectSize[_bucket8];
                    _dobj8 = (SMObjStruct*)((char*)_page8 + (_objSize8 * _objNum8));
                }
                _obj8 = SM_ENCRYPT_OBJECT(_dobj8);
                /* Finally, if the object isn't marked, mark it and push it */
                /* onto the mark stack. */
                SM_MARK_AND_PUSH(_pd8, _bucket8, _desc8, _obj8);

                /* If we ever overflow the mark stack, we'll catch it later */
                /* towards the end of the gc cycle. All that's important */
                /* now is that we pin all the conservative roots. */
            }            
        }
    }
}

#else /* !DEBUG */

xxx

#endif /* !DEBUG */

/******************************************************************************/

SM_IMPLEMENT(void)
SM_MarkRoots(SMObject** base, PRUword count, PRBool conservative)
{
    /* Interestingly, this routine does almost all the real work to collect
       the garbage -- and it's only called by the user's markRoots callback. */

    SMGenNum genNum = sm_Heap.collectingGenNum;
    PRBool copyCycle = SM_IS_COPY_CYCLE(genNum);
    PRUword i = count;
    
    /* The user better have given us an aligned pointer. */
    SM_ASSERT(SM_IS_ALIGNED(base, SM_POINTER_ALIGNMENT));

    /* And it better not be something that's already in the gc space. */
    SM_ASSERT(!SM_OVERLAPPING(sm_PageMgr.heapBase,
                              (sm_PageMgr.heapBase + sm_PageMgr.heapPageCount),
                              (SMPage*)base,
                              (SMPage*)(base + count)));

    if (conservative) {
        /* Make sure we don't try conservative scavenging after we've already */
        /* started our non-conservative phase. */
        SM_ASSERT(sm_conservOnly);

        while (i-- > 0) {
            SMObject* obj = *base++;
            SM_SCAVENGE_CONSERV(obj, genNum, copyCycle);
        }
    }
    else {
        /* roots don't have card-table entries: */
        SMCardDesc dummy;

        /* Once we start scavenging non-conservatively, we can't go back */
        sm_conservOnly = PR_FALSE;

        /* Next drain the mark stack from any conservative objects we've
           been saving up. (whew) */
        SM_DRAIN_MARK_STACK(genNum, copyCycle);

        /* Since SM_SCAVENGE_ALL is a macro, we can make two copies of it with
           the copyCycle test factored out. This speeds up the inner loop
           at the expence of expanding the code a bit. */
        if (copyCycle) {
            while (i-- > 0) {
                SM_SCAVENGE_ALL(base, genNum, PR_TRUE, &dummy);
                base++;
            }
        }
        else {
            while (i-- > 0) {
                SM_SCAVENGE_ALL(base, genNum, PR_FALSE, &dummy);
                base++;
            }
        }
    }
}

static PRStatus
sm_ScanStack(PRThread* t, void** addr, PRUword count, void* closure)
{
#if defined(XP_MAC)
#pragma unused (t, closure)
#endif

    SM_MarkRoots((SMObject**)addr, count, PR_TRUE);
    return PR_SUCCESS;
}

SM_IMPLEMENT(void)
SM_MarkThreadsConservatively(SMGenNum collectingGenNum, PRBool copyCycle,
                             void* closure)
{
    /* Mark the roots from the thread stacks. Do this first so
       that the marked objects get their pinned bit set. */
    (void)PR_ScanStackPointers(sm_ScanStack, NULL);
}

SM_IMPLEMENT(void)
SM_MarkThreadConservatively(PRThread* thread, SMGenNum collectingGenNum,
                            PRBool copyCycle, void* closure)
{
    /* Mark the roots from the thread stacks. Do this first so
       that the marked objects get their pinned bit set. */
    (void)PR_ThreadScanStackPointers(thread, sm_ScanStack, NULL);
}

/******************************************************************************/

#if !defined(SM_NO_WRITE_BARRIER) && defined(DEBUG)

static void
sm_CheckObject(SMObject* obj, SMGenNum cardGen)
{
    if (obj) {
        SMObjStruct* dobj = SM_DECRYPT_OBJECT(obj);
        SMPageDesc* pd = SM_OBJECT_PAGEDESC(dobj);
        SMObjDesc* od = SM_OBJECT_HEADER_FROM_PAGEDESC(dobj, pd);
        SMGenNum objGen = SM_PAGEDESC_GEN(pd);
        SM_ASSERT(cardGen <= objGen);
        SM_ASSERT(SM_OBJDESC_IS_MARKED(od));
    }
}

static void
sm_CheckRange(SMObject** refs, PRUword count, SMGenNum cardGen)
{
    SMObject** ref = refs;
    PRUword i = count;
    while (i-- > 0) {                                 
        sm_CheckObject(*ref, cardGen);
        ref++;
    }
}

static void
sm_CheckObjectFields(SMObject* obj, SMGenNum cardGen)
{
    SMClass* clazz = SM_OBJECT_CLASS(obj);
    SM_ASSERT(clazz != NULL);
    switch (SM_CLASS_KIND(clazz)) {
      case SMClassKind_ObjectClass: {
          SMFieldDesc* fieldDesc = SM_CLASS_GET_INST_FIELD_DESCS(clazz);
          PRUint16 k, fdCount = SM_CLASS_GET_INST_FIELDS_COUNT(clazz);
          for (k = 0; k < fdCount; k++) {
              PRWord offset = fieldDesc[k].offset; 
              PRUword count = fieldDesc[k].count; 
              SMObject** fieldRefs = &SM_OBJECT_FIELDS(obj)[offset];
              sm_CheckRange(fieldRefs, count, cardGen);
          }
          break;
      }
      case SMClassKind_ArrayClass: {
          if (SM_IS_OBJECT_ARRAY_CLASS(clazz)) {
              SMArray* arr = (SMArray*)obj;
              PRUword size = SM_ARRAY_SIZE(arr);
              SMObject** elementRefs = SM_ARRAY_ELEMENTS(arr);
              sm_CheckRange(elementRefs, size, cardGen);
          }
          break;
      }
      default:
        break;
    }
}

static void
sm_CheckWriteBarrier(SMGenNum collectingGenNum)
{
    SMPageCount i;
    for (i = 0; i < sm_PageMgr.heapPageCount; i++) {
        SMPageDesc* pd = &sm_PageMgr.pageTableMem[i];
        SMGenNum pdGen = SM_PAGEDESC_GEN(pd);
        if (collectingGenNum < pdGen && SM_IS_GC_SPACE(pdGen)) {
            SMSmallObjCount j; 
            SMBucket bucket = SM_PAGEDESC_BUCKET(pd); 
            if (!(bucket == SM_LARGE_OBJECT_BUCKET
                  && !SM_PAGEDESC_IS_LARGE_OBJECT_START(pd))) {
                SMSmallObjCount objsPerPage = sm_ObjectsPerPage[bucket]; 
                SMSmallObjSize objSize = sm_ObjectSize[bucket]; 
                SMObjStruct* dobj = (SMObjStruct*)SM_PAGEDESC_PAGE(pd); 
                SMObject* obj = SM_ENCRYPT_OBJECT(dobj);
                SMObjDesc* objTable = pd->objTable;
                SMCardDesc* cardDesc = SM_OBJECT_CARDDESC(obj);
                SMGenNum cardGen = SM_CARDDESC_GEN(*cardDesc);

                /* Card should have been processed */
                SM_ASSERT(!SM_CARDDESC_IS_DIRTY(*cardDesc));

                /* Make sure all the objects on the page refer to objects
                   that have been marked */
                for (j = 0; j < objsPerPage; j++) {
                    SMObjDesc* od = &objTable[j]; 
                    if (SM_OBJDESC_IS_ALLOCATED(od)) {
                        sm_CheckObjectFields(obj, cardGen);
                    }
                    dobj = (SMObjStruct*)((char*)dobj + objSize); 
                    obj = SM_ENCRYPT_OBJECT(dobj);
                }
            }
        }
    }
}

#define SM_CHECK_WRITE_BARRIER(genNum)  sm_CheckWriteBarrier(genNum)

#else /* !defined(SM_NO_WRITE_BARRIER) && defined(DEBUG) */

#define SM_CHECK_WRITE_BARRIER(genNum)  /* no-op */

#endif /* !defined(SM_NO_WRITE_BARRIER) && defined(DEBUG) */

/******************************************************************************/

static void
sm_FinishMarking(SMGenNum collectingGenNum, 
                 SMGenNum overflowGenNum, PRBool copyCycle)
{
    SMPageCount i;
    SMSmallObjCount j;

    /* If we ever overflowed the mark stack during the marking process,
       look for untraced objects and mark them too. */
    do {
        if (sm_Heap.markStack.overflowCount > 0) {
            for (i = 0; i < sm_PageMgr.heapPageCount; i++) {
                SMPageDesc* pd = &sm_PageMgr.pageTableMem[i];
                SMGenNum pdGen = SM_PAGEDESC_GEN(pd);
                if (pdGen <= overflowGenNum) {
                    SMBucket bucket = SM_PAGEDESC_BUCKET(pd);
                    SMSmallObjCount objsPerPage = sm_ObjectsPerPage[bucket];
                    SMSmallObjSize objSize = sm_ObjectSize[bucket];
                    SMObjDesc* objTable = pd->objTable;
                    SMObjStruct* obj = (SMObjStruct*)SM_PAGEDESC_PAGE(pd);
                    SMObject* eobj = SM_ENCRYPT_OBJECT(obj);
                    SMCardDesc* cd = SM_OBJECT_CARDDESC(eobj);
                    for (j = 0; j < objsPerPage; j++) {
                        SMObjDesc* od = &objTable[j];
                        if (SM_OBJDESC_IS_UNTRACED(od)) {
                            eobj = SM_ENCRYPT_OBJECT(obj);
                            SM_OBJDESC_CLEAR_UNTRACED(od);
                            SM_SCAVENGE_FIELDS(eobj, collectingGenNum, copyCycle, cd);
                            if (--sm_Heap.markStack.overflowCount == 0) {
                                goto finish; /* XXX should assert that there aren't more */
                            }
                        }
                        obj = (SMObjStruct*)((char*)obj + objSize);
                    }
                }
            }
        }
      finish:
        SM_DRAIN_MARK_STACK(collectingGenNum, copyCycle);
    } while (sm_Heap.markStack.overflowCount > 0);
    SM_ASSERT(SM_STACK_IS_EMPTY(&sm_Heap.markStack));
}

void
sm_Collect0(SMGenNum collectingGenNum, PRBool copyCycle)
{
    SMPageCount i;
    SMSmallObjCount j;
    SMGenNum g, overflowGenNum;
    
    overflowGenNum = copyCycle && collectingGenNum < SMGenNum_Senior
        ? (SMGenNum)((int)collectingGenNum + 1)
        : collectingGenNum;

    /**************************************************************************
     * 1. Enter monitors that we need to hold during gc
     *************************************************************************/

    if (sm_Heap.beforeGCHook)
        sm_Heap.beforeGCHook(collectingGenNum,
                             sm_Heap.gen[SMGenNum_Freshman].collectCount,
                             copyCycle, sm_Heap.beforeGCClosure);
    PR_EnterMonitor(sm_Heap.finalizerMon);
    PR_EnterMonitor(sm_PageMgr.monitor);
    sm_Heap.collectingGenNum = collectingGenNum; /* set after being in the finalizerMon */
    sm_PageMgr.alreadyLocked = PR_TRUE; /* set after locking */
    PR_SuspendAll();
    sm_conservOnly = PR_TRUE;

    /**************************************************************************
     * 2. Reset alloc counts and clear mark bits
     *************************************************************************/

    for (g = SMGenNum_Freshman; g <= collectingGenNum; g++) {
        SMGen* gen = &sm_Heap.gen[g];
        SMBucket b;

        /* Increment the collect count for all generations we've looked at. This
           allows us to figure out when each generation needs a copy cycle. */
        gen->collectCount++;
        gen->pool.allocAmount = 0;

        /* Finish sweeping and set the state of the previously unmarked objects
           to freed. That way we can distinguish a free object from one we simply
           haven't marked yet. */
        for (b = 0; b < SM_LARGE_OBJECT_BUCKET; b++) {
            SMSweepList* sweepList = &gen->pool.sweepList[b];
#if 0
            SMPageDesc* pd = sweepList->sweepPage;
            SMObjDesc* objTable = pd->objTable;
            SMSmallObjCount objsPerPage = sm_ObjectsPerPage[b];
            for (j = sweepList->sweepIndex; j < objsPerPage; j++) {
                SMObjDesc* od = &objTable[j];
                if (!SM_OBJDESC_IS_MARKED(od)) {
                    SM_OBJDESC_SET_FREE(od);
                }
            }
#endif
            /* Reset the sweep lists -- we'll rebuild them below */
            sweepList->sweepPage = NULL;
            sweepList->sweepIndex = 0;
        }
    }

    /* first clear all the mark bits */
    for (i = 0; i < sm_PageMgr.heapPageCount; i++) {
        SMPageDesc* pd = &sm_PageMgr.pageTableMem[i];
        SMGenNum pageGenNum = SM_PAGEDESC_GEN(pd);
        SM_PAGEDESC_CLEAR_BLACKLISTED(pd);
        if (pageGenNum <= collectingGenNum) {
            SMBucket bucket = SM_PAGEDESC_BUCKET(pd);
            SMSmallObjCount objsPerPage = sm_ObjectsPerPage[bucket];
            SMObjDesc* objTable = pd->objTable;
            SMGen* gen = &sm_Heap.gen[pageGenNum];
            SMSweepList* sweepList = &gen->pool.sweepList[bucket];

            SM_ASSERT(SM_IS_GC_SPACE(pageGenNum));

            /* XXX We can eliminate this loop by swapping the marked and 
               unmarked colors. */
            for (j = 0; j < objsPerPage; j++) {
                SMObjDesc* od = &objTable[j];
                SM_OBJDESC_SET_UNMARKED(od);
            }

            if (bucket != SM_LARGE_OBJECT_BUCKET) {
                pd->allocCount = 0;

                /* Thread all pages onto the sweep list. During sweeping
                   we'll free up any completely empty pages. */
                pd->next = sweepList->sweepPage;
                sweepList->sweepPage = pd;
            }
        }
    }
    
    /**************************************************************************
     * 3. Mark roots
     *************************************************************************/

    if (sm_Heap.markRootsProc)
        sm_Heap.markRootsProc(collectingGenNum, copyCycle, sm_Heap.markRootsClosure);

    sm_FinishMarking(collectingGenNum, overflowGenNum, copyCycle);

    /**************************************************************************
     * 4. Process the write-barrier
     *************************************************************************/

#ifdef SM_NO_WRITE_BARRIER
    /* No write barrier -- so check all senior pages. */
    for (i = 0; i < sm_PageMgr.heapPageCount; i++) {
        SMPageDesc* pd = &sm_PageMgr.pageTableMem[i];
        SMGenNum pdGen = SM_PAGEDESC_GEN(pd);
        if (collectingGenNum < pdGen && SM_IS_GC_SPACE(pdGen)) {
            SMBucket bucket = SM_PAGEDESC_BUCKET(pd);
            if (bucket == SM_LARGE_OBJECT_BUCKET) {
                SM_ASSERT(SM_PAGEDESC_IS_LARGE_OBJECT_START(pd));
                i += (pd->allocCount - 1);    /* skip over the rest of the large obj */
            }
            SM_SCAVENGE_PAGE(pd, collectingGenNum, copyCycle, NULL);
        }
    }
#else /* !SM_NO_WRITE_BARRIER */
    /* Mark the roots from the dirty pages. */
    for (i = 0; i < sm_PageMgr.heapPageCount; i++) {
        SMCardDesc* cardDesc = &sm_Heap.cardTableMem[i];
        PRInt8 cardDescGen = SM_CARDDESC_GEN(*cardDesc); /* must be signed for comparison */
        if (cardDescGen <= collectingGenNum) {  /* card references gen that we're collecting */
            SMPageDesc* pd = &sm_PageMgr.pageTableMem[i];       /* page of this card */
            SMGenNum pdGen = SM_PAGEDESC_GEN(pd);
            SMPage* page = SM_PAGEDESC_PAGE(pd);
            SMPageCount pageNum = SM_PAGE_NUMBER(page);

            SM_ASSERT(&SM_CardTable[pageNum] == cardDesc);

            /* First set the min gen for this card to be the card's gen. This
               indicates that we've examined this card, and it's no longer dirty. */
            if (SM_CARDDESC_IS_DIRTY(*cardDesc)) {
                SM_CARDDESC_SET_GEN(cardDesc, pdGen);
            }

            /* Then scavenge the objects on the card while looking for an even 
               younger generation to which the card refers. */
            if (collectingGenNum < pdGen && SM_IS_GC_SPACE(pdGen)) {
                SMBucket bucket = SM_PAGEDESC_BUCKET(pd);
                if (bucket == SM_LARGE_OBJECT_BUCKET) {
                    SM_ASSERT(SM_PAGEDESC_IS_LARGE_OBJECT_START(pd));
                    i += (pd->allocCount - 1);    /* skip over the rest of the large obj */
                }
                SM_SCAVENGE_PAGE(pd, collectingGenNum, copyCycle, cardDesc);
            }
        }
    }
#endif /* !SM_NO_WRITE_BARRIER */

    sm_FinishMarking(collectingGenNum, overflowGenNum, copyCycle);

    /* Check that we haven't missed any pages that should have been dirty. */
    SM_CHECK_WRITE_BARRIER(collectingGenNum);

    /**************************************************************************
     * 5. Process finalizable objects
     *************************************************************************/

    /* Look for any finalizable objects and mark them as needing finalization. */
    for (i = 0; i < sm_PageMgr.heapPageCount; i++) {
        SMPageDesc* pd = &sm_PageMgr.pageTableMem[i];
        SMGenNum pdGen = SM_PAGEDESC_GEN(pd);
        if (pdGen <= collectingGenNum) {
            SMBucket bucket = SM_PAGEDESC_BUCKET(pd);
            if (bucket == SM_LARGE_OBJECT_BUCKET) {
                SMObjDesc* od = &pd->objTable[0];
                if (!SM_OBJDESC_IS_ALLOCATED(od) && SM_OBJDESC_IS_FINALIZABLE(od)) {
                    SM_OBJDESC_SET_NEEDS_FINALIZATION(od);
                }
                i += (pd->allocCount - 1);    /* skip over the rest of the large obj */
            }
            else {
                SMSmallObjCount objsPerPage = sm_ObjectsPerPage[bucket];
                SMSmallObjSize objSize = sm_ObjectSize[bucket];
                SMObjDesc* objTable = pd->objTable;
                for (j = 0; j < objsPerPage; j++) {
                    SMObjDesc* od = &objTable[j];
                    if (!SM_OBJDESC_IS_ALLOCATED(od) && SM_OBJDESC_IS_FINALIZABLE(od)) {
                        SM_OBJDESC_SET_NEEDS_FINALIZATION(od);
                    }
                }
            }
        }
    }
    /* Then mark any objects that are going to have their finalizers run so that
       they're kept alive until the finalizers have finished. */
    for (i = 0; i < sm_PageMgr.heapPageCount; i++) {
        SMPageDesc* pd = &sm_PageMgr.pageTableMem[i];
        SMGenNum pdGen = SM_PAGEDESC_GEN(pd);
        if (pdGen <= collectingGenNum) {
            SMBucket bucket = SM_PAGEDESC_BUCKET(pd);
            if (bucket == SM_LARGE_OBJECT_BUCKET) {
                SMObjDesc* od = &pd->objTable[0];
                SMObjStruct* obj = (SMObjStruct*)SM_PAGEDESC_PAGE(pd);
                if (!SM_OBJDESC_IS_ALLOCATED(od) && SM_OBJDESC_IS_FINALIZABLE(od)) {
                    SMObject* eobj = SM_ENCRYPT_OBJECT(obj);
                    SMCardDesc dummy;
                    /* Scavenge the object, but don't allow it to be copied.
                       It's going to get collected soon anyway. */
                    SM_SCAVENGE_ALL(&eobj, collectingGenNum, PR_FALSE, &dummy);
                }
                i += (pd->allocCount - 1);    /* skip over the rest of the large obj */
            }
            else {
                SMSmallObjCount objsPerPage = sm_ObjectsPerPage[bucket];
                SMSmallObjSize objSize = sm_ObjectSize[bucket];
                SMObjDesc* objTable = pd->objTable;
                SMObjStruct* obj = (SMObjStruct*)SM_PAGEDESC_PAGE(pd);
                for (j = 0; j < objsPerPage; j++) {
                    SMObjDesc* od = &objTable[j];
                    if (!SM_OBJDESC_IS_ALLOCATED(od) && SM_OBJDESC_IS_FINALIZABLE(od)) {
                        SMObject* eobj = SM_ENCRYPT_OBJECT(obj);
                        SMCardDesc dummy;
                        /* Scavenge the object, but don't allow it to be copied.
                           It's going to get collected soon anyway. */
                        SM_SCAVENGE_ALL(&eobj, collectingGenNum, PR_FALSE, &dummy);
                    }
                    obj = (SMObjStruct*)((char*)obj + objSize);
                }
            }
        }
    }

    /* We need to again check for mark stack overflow and drain the mark stack
       due to marking finalizable objects. */
    sm_FinishMarking(collectingGenNum, overflowGenNum, copyCycle);

    /**************************************************************************
     * 6. Return free pages to the page manager
     *************************************************************************/

    for (g = SMGenNum_Freshman; g <= collectingGenNum; g++) {
        SMGen* gen = &sm_Heap.gen[g];
        SMPool* pool = &gen->pool;
        SMBucket b;
        SMSweepList* sweepList;
        SMPageDesc** pdAddr;
        SMPageDesc* pd;
        /* Small object pages */
        for (b = 0; b < SM_LARGE_OBJECT_BUCKET; b++) {
            sweepList = &pool->sweepList[b];
            pdAddr = &sweepList->sweepPage;
            while ((pd = *pdAddr) != NULL) {
                if (pd->allocCount == 0) {
                    SMObjStruct* obj = (SMObjStruct*)SM_PAGEDESC_PAGE(pd);
                    *pdAddr = pd->next;     /* unlink the page */
                    SM_GEN_DESTROY_PAGE(gen, pd);
                }
                else {
                    pdAddr = &pd->next;
                }
            }
        }
        /* Large object pages */
        sweepList = &pool->sweepList[SM_LARGE_OBJECT_BUCKET];
        pdAddr = &sweepList->sweepPage;
        while ((pd = *pdAddr) != NULL) {
            if (!SM_OBJDESC_IS_ALLOCATED(&pd->objTable[0])) {
                SMObjStruct* obj = (SMObjStruct*)SM_PAGEDESC_PAGE(pd);
                SM_ASSERT(SM_PAGEDESC_IS_LARGE_OBJECT_START(pd));
                *pdAddr = pd->next;     /* unlink the large object */
                SM_GEN_FREE_LARGE_OBJ(gen, obj);
            }
            else {
                pdAddr = &pd->next;
            }
        }
    }
#ifdef DEBUG
    for (i = 0; i < sm_PageMgr.heapPageCount; i++) {
        SMPageDesc* pd = &sm_PageMgr.pageTableMem[i];
        SMGenNum pdGen = SM_PAGEDESC_GEN(pd);
        if (pdGen <= collectingGenNum) {
            SMBucket bucket = SM_PAGEDESC_BUCKET(pd);
            if (bucket == SM_LARGE_OBJECT_BUCKET) {
                SMObjDesc* od = &pd->objTable[0];
                SMObjStruct* obj = (SMObjStruct*)SM_PAGEDESC_PAGE(pd);
                PRUword objSize = pd->allocCount * SM_PAGE_SIZE;
                if (!SM_OBJDESC_IS_ALLOCATED(od)) {
                    DBG_MEMSET(obj, SM_GC_FREE_PATTERN, objSize);
                }
                i += (pd->allocCount - 1);    /* skip over the rest of the large obj */
            }
            else {
                SMSmallObjCount objsPerPage = sm_ObjectsPerPage[bucket];
                SMSmallObjSize objSize = sm_ObjectSize[bucket];
                SMObjDesc* objTable = pd->objTable;
                SMObjStruct* obj = (SMObjStruct*)SM_PAGEDESC_PAGE(pd);
                for (j = 0; j < objsPerPage; j++) {
                    SMObjDesc* od = &objTable[j];
                    if (!SM_OBJDESC_IS_ALLOCATED(od)) {
                        DBG_MEMSET(obj, SM_GC_FREE_PATTERN, objSize);
                    }
                    obj = (SMObjStruct*)((char*)obj + objSize);
                }
            }
        }
    }
#endif
    SM_ASSERT(SM_STACK_IS_EMPTY(&sm_Heap.markStack));
    SM_VERIFY_HEAP();
    
    /**************************************************************************
     * 7. Release monitors
     *************************************************************************/

    PR_ResumeAll();
    sm_PageMgr.alreadyLocked = PR_FALSE; /* set before unlocking */
    PR_ExitMonitor(sm_PageMgr.monitor);
    PR_Notify(sm_Heap.finalizerMon);
    PR_ExitMonitor(sm_Heap.finalizerMon);
    if (sm_Heap.afterGCHook)
        sm_Heap.afterGCHook(collectingGenNum,
                            sm_Heap.gen[SMGenNum_Freshman].collectCount,
                            copyCycle, sm_Heap.afterGCClosure);
}

SM_IMPLEMENT(void)
SM_Collect(void)
{
    sm_Collect0(SMGenNum_Senior, SM_IS_COPY_CYCLE(SMGenNum_Senior));
}

/*******************************************************************************
 * Finalizer
 ******************************************************************************/

static PRBool sm_didFinalizationWork = PR_FALSE;

static void PR_CALLBACK
sm_FinalizerLoop(void* unused)
{
#ifdef XP_MAC
#pragma unused (unused)
#endif

    PR_EnterMonitor(sm_Heap.finalizerMon);
    while (PR_TRUE) {
        SMPageCount i;
        SMGenNum genNum;

        PR_Notify(sm_Heap.finalizerMon); /* ready */
        if (!sm_Heap.keepRunning)
            break;

        /* wait to be told to finalize */
        PR_Wait(sm_Heap.finalizerMon, PR_INTERVAL_NO_TIMEOUT);

        /* set this variable back to false after we've allowed
           our notifier to read it (during PR_Wait) */
        sm_didFinalizationWork = PR_FALSE;
        
//        SM_VERIFY_HEAP();
		
        genNum = sm_Heap.collectingGenNum;
        if (genNum >= SMGenNum_Static)
            continue; /* nothing to do yet */

        for (i = 0; i < sm_PageMgr.heapPageCount; i++) {
            SMPageDesc* pd = &sm_PageMgr.pageTableMem[i];
            SMGenNum pageGenNum = SM_PAGEDESC_GEN(pd);
            if (pageGenNum <= genNum) {
                /* found a page belonging to the generation we just collected */
                SMSmallObjCount j; 
                SMBucket bucket = SM_PAGEDESC_BUCKET(pd); 
                SMSmallObjCount objsPerPage = sm_ObjectsPerPage[bucket]; 
                SMSmallObjSize objSize = sm_ObjectSize[bucket]; 
                SMObjStruct* obj = (SMObjStruct*)SM_PAGEDESC_PAGE(pd); 
                SMObjDesc* objTable = pd->objTable; 
                for (j = 0; j < objsPerPage; j++) {
                    SMObjDesc* od = &objTable[j]; 
                    if (SM_OBJDESC_NEEDS_FINALIZATION(od)) {
                        SMObject* eobj = SM_ENCRYPT_OBJECT(obj);
                        SMClass* clazz = SM_OBJECT_CLASS(eobj);
                        SMFinalizeFun finalize = SM_CLASS_FINALIZER(clazz);
                        PR_ExitMonitor(sm_Heap.finalizerMon);

                        /* run the finalizer */
                        finalize(eobj);

                        PR_EnterMonitor(sm_Heap.finalizerMon);
                        SM_OBJDESC_CLEAR_FINALIZATION(od);
                        sm_didFinalizationWork = PR_TRUE;
                    } 
                    obj = (SMObjStruct*)((char*)obj + objSize); 
                } 
            }
        }
        sm_Heap.collectingGenNum = SMGenNum_Free;
//        SM_VERIFY_HEAP();
    }
    PR_ExitMonitor(sm_Heap.finalizerMon);
}

static PRBool
sm_ForceFinalization(void)
{
    PRBool didFinalizationWork;
    PR_EnterMonitor(sm_Heap.finalizerMon);
    PR_Notify(sm_Heap.finalizerMon);
    PR_Wait(sm_Heap.finalizerMon, PR_INTERVAL_NO_TIMEOUT);
    didFinalizationWork = sm_didFinalizationWork;
    PR_ExitMonitor(sm_Heap.finalizerMon);
    return didFinalizationWork;
}

SM_IMPLEMENT(void)
SM_RunFinalization(void)
{
    (void)sm_ForceFinalization();
}

PRStatus
sm_InitFinalizer(void)
{
    sm_Heap.finalizerMon = PR_NewMonitor();
    if (sm_Heap.finalizerMon == NULL)
        return PR_FAILURE;
    /* set keepRunning before creating the finalizer loop */
    sm_Heap.keepRunning = PR_TRUE;
    PR_EnterMonitor(sm_Heap.finalizerMon);
    sm_Heap.finalizer = PR_CreateThread(PR_SYSTEM_THREAD,
                                        sm_FinalizerLoop, NULL,
                                        PR_PRIORITY_LOW, PR_GLOBAL_THREAD,
                                        PR_JOINABLE_THREAD, 0);
    if (sm_Heap.finalizer == NULL) {
        PR_ExitMonitor(sm_Heap.finalizerMon);
        PR_DestroyMonitor(sm_Heap.finalizerMon);
        return PR_FAILURE;
    }
    /* Wait for the finalizer loop to be ready. */
    PR_Wait(sm_Heap.finalizerMon, PR_INTERVAL_NO_TIMEOUT);
    PR_ExitMonitor(sm_Heap.finalizerMon);
    return PR_SUCCESS;
}

PRBool
sm_CollectAndFinalizeAll(PRUword spaceNeeded)
{
    PRBool doingFinalizationWork, doCopy = PR_TRUE;
    static PRUword lastCollectCount = 0;
    
    if (lastCollectCount == sm_Heap.gen[SMGenNum_Freshman].collectCount) {
        /* Heuristic: Returning false from this routine causes
           allocation to fail -- all the way up to the user program.
           This gives the user the opportunity to free up some stuff
           (hopefully), so if we come through here again and we haven't
           collected yet for other reasons, we don't want to fail again
           without trying to collect and finalize again. So reset the
           lastCollectCount so that we don't continuously fail. */
        lastCollectCount = 0;
        return PR_FALSE;
    }
        
    PR_EnterMonitor(sm_Heap.finalizerMon);

    do {
        /* force copying to free space in gen 0 */
        sm_Collect0(SMGenNum_Senior, doCopy);
        doingFinalizationWork = sm_ForceFinalization();
        doCopy = PR_FALSE;      /* only copy the first time */
    } while (doingFinalizationWork);

    lastCollectCount = sm_Heap.gen[SMGenNum_Freshman].collectCount;

    PR_ExitMonitor(sm_Heap.finalizerMon);
    return PR_TRUE;
}

PRStatus
sm_FiniFinalizer(PRBool finalizeOnExit)
{
    if (finalizeOnExit) {
        (void)sm_CollectAndFinalizeAll(0);
    }
    
    /* shut down the finalizer thread */
    sm_Heap.keepRunning = PR_FALSE;
    (void)sm_ForceFinalization();       /* one last time */

    /* and wait for it to finish */
    PR_JoinThread(sm_Heap.finalizer);

    PR_DestroyMonitor(sm_Heap.finalizerMon);
    return PR_SUCCESS;
}

/******************************************************************************/
