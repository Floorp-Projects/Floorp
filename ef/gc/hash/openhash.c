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

#include "openhash.h"
#include "prlog.h"
             
PR_IMPLEMENT(PRStatus)
PL_OpenhashInit(PLOpenhash* self, PRUint32 initialSize, 
                PLOpenhashHashFun hash, PLOpenhashEqualFun equal,
                PLOpenhashDestroyFun del, void* context)
{
    self->header.size = initialSize;
    self->header.hash = hash;
    self->header.equal = equal;
    self->header.destroy = del;
    self->header.context = context;
    memset(self->element, 0, sizeof(PLOpenhashElement) * initialSize);
    return PR_SUCCESS;
}

PR_IMPLEMENT(void)
PL_OpenhashCleanup(PLOpenhash* self)
{
    PL_OpenhashEmpty(self);
}
    
PR_IMPLEMENT(PLOpenhash*)
PL_NewOpenhash(PRUint32 initialSize,
               PLOpenhashHashFun hash, PLOpenhashEqualFun equal,
               PLOpenhashDestroyFun del, void* context)
{
    PRStatus status;
    PLOpenhash* self = malloc(sizeof(PLOpenhashHeader)
                              + initialSize * sizeof(PLOpenhashElement));
    if (self == NULL) return NULL;
    status = PL_OpenhashInit(self, initialSize, hash, equal, del, context);
    if (status != PR_SUCCESS) {
        PL_OpenhashDestroy(self);
        return NULL;
    }
    return self;
}

PR_IMPLEMENT(void)
PL_OpenhashDestroy(PLOpenhash* self)
{
    PL_OpenhashCleanup(self);
    free((void*)self);
}

#define PR_GOLDEN_RATIO         616161
#define PR_HASH1(key, size)     (((key) * PR_GOLDEN_RATIO) & ((size) - 1))
#define PR_HASH2(key, size)     (((key) * 97) + 1)

PR_IMPLEMENT(void*)
PL_OpenhashLookup(PLOpenhash* self, PRUword key)
{
    PRUword htSize = self->header.size;
    PRUword hash1 = PR_HASH1(++key, htSize);
    PRUword hash2 = PR_HASH2(key, htSize);
    PRUword i = hash1;
    PLOpenhashElement* elements = self->element;

    while (elements[i].key != 0) {
        if (elements[i].key == key)
            return elements[i].value;
        else {
            i = (i + hash2) % htSize;
            PR_ASSERT(i != hash1);
        }
    }
    return NULL;
}
    
PR_IMPLEMENT(void)
PL_OpenhashAdd(PLOpenhash* self, PRUword key, void* value)
{
    PRUword htSize = self->header.size;
    PRUword hash1 = PR_HASH1(++key, htSize);
    PRUword hash2 = PR_HASH2(key, htSize);
    PRUword i = hash1;
    PLOpenhashElement* elements = self->element;

    while (elements[i].key != 0) {
        i = (i + hash2) % htSize;
        PR_ASSERT(i != hash1);
    }
    elements[i].key = key;
    elements[i].value = value;
}
    
PR_IMPLEMENT(PRStatus)
PL_OpenhashRemove(PLOpenhash* self, PRUword key)
{
    PR_ASSERT(0);
    return PR_FAILURE;
}

PR_IMPLEMENT(void)
PL_OpenhashEmpty(PLOpenhash* self)
{
    PRUword i;
    PLOpenhashElement* element = self->element;
    for (i = 0; i < self->header.size; i++) {
        if (element->key != 0) {
            self->header.destroy(self->header.context, element);
            element->key = 0;
        }
        element++;
    }
}

/******************************************************************************/
