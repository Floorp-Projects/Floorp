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

#ifndef __SMGEN__
#define __SMGEN__

#include "smpool.h"
#include "smobj.h"

SM_BEGIN_EXTERN_C

/*******************************************************************************
 * Generations
 ******************************************************************************/

#define SM_GEN_COUNT                    (SMGenNum_Static - SMGenNum_Freshman + 1)

#define SM_IS_GC_SPACE(genNum)          ((genNum) < SMGenNum_Malloc)
#define SM_IS_MALLOC_SPACE(genNum)      ((genNum) == SMGenNum_Malloc)
#define SM_IS_FREE_SPACE(genNum)        ((genNum) == SMGenNum_Free)

typedef struct SMGen {
    SMPool              pool;
    PRUword             allocThreshold;
    PRUword             collectCount;
} SMGen;

#define SM_DEFAULT_ALLOC_THRESHOLD      (16 * 1024)     /* XXX ??? */

extern PRStatus
sm_InitGen(SMGen* gen, PRUword allocThreshold);

extern void
sm_FiniGen(SMGen* gen);

/* Like sm_PoolAllocObj, but is only used to allocate an object in a new 
   generation during gc. */
extern SMObjStruct*
sm_GenPromoteObj(SMGen* gen, SMBucket bucket);

/*******************************************************************************
 * Generation Operations
 * 
 * Unlike their pool equivalents, these operations update the card-table.
 ******************************************************************************/

#define SM_GEN_ALLOC_OBJ(result, gen, bucket)                                 \
    (result = (SMObjStruct*)sm_PoolAllocObj(&(gen)->pool, bucket),            \
     (result ? SM_DIRTY_PAGE(result) : (void)0))                              \

#define SM_GEN_DESTROY_PAGE(gen, freePd)                                      \
{                                                                             \
    SMPageDesc* _pd1 = (freePd);                                              \
    SMPage* _page1 = SM_PAGEDESC_PAGE(_pd1);                                  \
    SM_CLEAN_PAGE(_page1);                                                    \
    SM_POOL_DESTROY_PAGE(&(gen)->pool, _pd1);                                 \
}                                                                             \

#define SM_GEN_ALLOC_LARGE_OBJ(result, gen, genNum, size)                     \
    (result = (SMObjStruct*)sm_PoolAllocLargeObj(&(gen)->pool, genNum, size), \
     (result ? SM_DIRTY_PAGE(result) : (void)0))                              \

#define SM_GEN_FREE_LARGE_OBJ(gen, obj)                                       \
{                                                                             \
    SMObjStruct* _obj1 = (obj);                                               \
    SMPage* _page1 = (SMPage*)_obj1;                                          \
    SM_CLEAN_PAGE(_page1);                                                    \
    SM_POOL_FREE_LARGE_OBJ(&(gen)->pool, _obj1);                              \
}                                                                             \

/******************************************************************************/

#ifdef DEBUG

extern PRUword
sm_GenSpaceAvailable(SMGenNum genNum);

#endif /* DEBUG */

/******************************************************************************/

SM_END_EXTERN_C

#endif /* __SMGEN__ */
/******************************************************************************/
