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

#ifndef openhash_h__
#define openhash_h__

#include "prtypes.h"
             
#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/*******************************************************************************
 * PLOpenhash
 ******************************************************************************/

typedef struct PLOpenhashElement {
    PRUword     key;
    void*       value;
} PLOpenhashElement;

typedef PRUword
(PR_CALLBACK* PLOpenhashHashFun)(void* context,
                                 PLOpenhashElement* x);

typedef PRBool
(PR_CALLBACK* PLOpenhashEqualFun)(void* context,
                                  PLOpenhashElement* x, PLOpenhashElement* y);

typedef void
(PR_CALLBACK* PLOpenhashDestroyFun)(void* context,
                                    PLOpenhashElement* x);

typedef struct PLOpenhashHeader {
    PRUword                     size;
    PLOpenhashHashFun           hash;
    PLOpenhashEqualFun          equal;
    PLOpenhashDestroyFun        destroy;
    void*                       context;
} PLOpenhashHeader;

typedef struct PLOpenhash {
    PLOpenhashHeader    header;
    PLOpenhashElement   element[1];
    /* more elements follow */
} PLOpenhash;

/******************************************************************************/

PR_EXTERN(PRStatus)
PL_OpenhashInit(PLOpenhash* self, PRUint32 initialSize,
                PLOpenhashHashFun hash, PLOpenhashEqualFun equal,
                PLOpenhashDestroyFun del, void* context);

PR_EXTERN(void)
PL_OpenhashCleanup(PLOpenhash* self);
    
PR_EXTERN(PLOpenhash*)
PL_NewOpenhash(PRUint32 initialSize,
               PLOpenhashHashFun hash, PLOpenhashEqualFun equal,
               PLOpenhashDestroyFun del, void* context);

PR_EXTERN(void)
PL_OpenhashDestroy(PLOpenhash* self);

PR_EXTERN(void*)
PL_OpenhashLookup(PLOpenhash* self, PRUword key);
    
PR_EXTERN(void)
PL_OpenhashAdd(PLOpenhash* self, PRUword key, void* value);
    
PR_EXTERN(PRStatus)
PL_OpenhashRemove(PLOpenhash* self, PRUword key);

PR_EXTERN(void)
PL_OpenhashEmpty(PLOpenhash* self);

#define PL_OpenhashSize(self)        ((self)->keyCount)

/******************************************************************************/
#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* openhash_h__ */
/******************************************************************************/
