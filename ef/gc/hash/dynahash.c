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
 * Hash Tables: Based on Larson's in-memory adaption of linear hashing
 * [CACM, vol 31, #4, 1988 pp 446-457]
 ******************************************************************************/

#include "dynahash.h"
#include "prlog.h"

#define MUL(x, y)        ((x) << (y##_SHIFT))
#define DIV(x, y)        ((x) >> (y##_SHIFT))
#define MOD(x, y)        ((x) & ((y) - 1))

PR_IMPLEMENT(PLDynahash*)
PL_NewDynahash(PRUint32 initialSize,
    PLHashFun hash, PLEqualFun equal,
    PLDestroyFun del, void* context)
{
    PRStatus status;
    PLDynahash* self = (PLDynahash*)malloc(sizeof(PLDynahash));
    if (self == NULL)
        return self;
    status = PL_DynahashInit(self, initialSize, hash, equal, del, context);
    if (status != PR_SUCCESS) {
        PL_DynahashDestroy(self);
        return NULL;
    }
    return self;
}

PR_IMPLEMENT(void)
PL_DynahashDestroy(PLDynahash* self)
{
    PL_DynahashEmpty(self);
    free(self);
}

PR_IMPLEMENT(PRStatus)
PL_DynahashInit(PLDynahash* self, PRUint32 initialSize, PLHashFun hash,
                PLEqualFun equal, PLDestroyFun del, void* context)
{
    PRUint32 count = DIV(initialSize, PL_DYNAHASH_BUCKETSIZE) + 1;
    self->directory = (PLHashDir*)count;
    self->hash = hash;
    self->equal = equal;
    self->destroy = del;
    self->keyCount = 0;
    self->nextBucketToSplit = 0;
    self->maxSize = (PRUint16)MUL(count, PL_DYNAHASH_BUCKETSIZE);
    self->bucketCount = 0;
    self->context = context;
#ifdef PL_DYNAHASH_STATISTICS
    self->accesses = 0;
    self->collisions = 0;
    self->expansions = 0;
#endif /* PL_DYNAHASH_STATISTICS */
    return PR_SUCCESS;
}
    
static PRStatus
pl_DynahashAllocBucket(PLDynahash* self)
{
    PLHashDir* newDir;
    PRUint32 i, size;
    PRUint16 count = 1;

    if (self->bucketCount == 0) {
        count = (PRUint16)(PRInt32)self->directory;
    }
    
    /* realloc directory */
    size = self->bucketCount + count;
    newDir = (PLHashDir*)calloc(sizeof(PLHashDir*), size);
    if (newDir == NULL)
        return PR_FAILURE;
//    memset((void*)newDir, 0, size * sizeof(PLHashBucket*));
    if (self->bucketCount) {
        memcpy((void*)newDir, self->directory, self->bucketCount
               * sizeof(PLHashBucket*));
    }
    
    /* add new bucket(s) */
    for (i = self->bucketCount; i < size; i++) {
        PLHashBucket* newBucket;
        newBucket = (PLHashBucket*)calloc(sizeof(PLHashBucket*), PL_DYNAHASH_BUCKETSIZE);
        if (newBucket == NULL) return PR_FAILURE;
//        memset((void*)newBucket, 0, sizeof(PLDynahashElement*)
//               * PL_DYNAHASH_BUCKETSIZE);
        newDir[i] = newBucket;
    }
    
    if (self->bucketCount != 0) {
        free(self->directory);
    }
    self->directory = newDir;
    self->bucketCount += count;
    return PR_SUCCESS;
}
    
PR_IMPLEMENT(void)
PL_DynahashCleanup(PLDynahash* self)
{
    PL_DynahashEmpty(self);
}
    
#define PL_DYNAHASH_HASH(resultVar, self, key)                         \
{                                                                      \
    PRUword _h_ = self->hash(self->context, key);                      \
    PRUword _a_ = MOD(_h_, self->maxSize);                             \
    if (_a_ < self->nextBucketToSplit) {                               \
        _a_ = MOD(_h_, self->maxSize << 1);    /* h % (2 * maxSize) */ \
    }                                                                  \
    resultVar = _a_;                                                   \
}                                                                      \

PR_IMPLEMENT(PLDynahashElement*)
PL_DynahashLookup(PLDynahash* self, PLDynahashElement* element)
{
    PRUword h;
    PRUint32 bucketIndex, binIndex;
    PLHashBucket* bin;
    PLDynahashElement* result = NULL;
    PLHashDir* dir;
    PLDynahashElement* e;

    if (self->bucketCount == 0) return NULL;
    
    PL_DYNAHASH_HASH(h, self, element);
    bucketIndex = DIV(h, PL_DYNAHASH_BUCKETSIZE);
    binIndex = MOD(h, PL_DYNAHASH_BUCKETSIZE);

    /* hash ensures valid segment */
    dir = self->directory;
    bin = dir[bucketIndex];
    PR_ASSERT(bin);

    for (e = bin[binIndex]; e != NULL; e = e->next) {
        if (self->equal(self->context, e, element)) {
            return e;
        }
	}																		

    return result;
}

static PRStatus
pl_DynahashExpand(PLDynahash* self)
{
    PRStatus err;
    if (self->maxSize + self->nextBucketToSplit
            < MUL(PL_DYNAHASH_MAX_DIR_SIZE, PL_DYNAHASH_BUCKETSIZE)) {
        PRUint32 oldDirIndex, oldBinIndex, newDirIndex, newBinIndex;
        PLHashBucket* oldSegment;
        PLHashBucket* newSegment;
        PRUword newAddress;
        PLDynahashElement** previous;
        PLDynahashElement* current;
        PLDynahashElement** lastOfNew;
        
        /* locate the bucket to split */
        oldDirIndex = DIV(self->nextBucketToSplit, PL_DYNAHASH_BUCKETSIZE);
        oldSegment = self->directory[oldDirIndex];
        oldBinIndex = MOD(self->nextBucketToSplit, PL_DYNAHASH_BUCKETSIZE);
        
        /* expand address space */
        newAddress = self->maxSize + self->nextBucketToSplit;
        newDirIndex = DIV(newAddress, PL_DYNAHASH_BUCKETSIZE);
        newBinIndex = MOD(newAddress, PL_DYNAHASH_BUCKETSIZE);
        
        /* we need to unlock the directory handle here in case AllocBucket grows
           the directory */
        if (newBinIndex == 0) {
            err = pl_DynahashAllocBucket(self);
            if (err != PR_SUCCESS) return err;
        }
        PR_ASSERT(newDirIndex < self->bucketCount);
        
        newSegment = self->directory[newDirIndex];
        
        /* adjust state */
        self->nextBucketToSplit++;
        if (self->nextBucketToSplit == self->maxSize) {
            self->maxSize <<= 1;    /* maxSize *= 2 */
            self->nextBucketToSplit = 0;
        }
        
        /* relocate records to the new bucket */
        previous = &oldSegment[oldBinIndex];
        current = *previous;
        lastOfNew = &newSegment[newBinIndex];
        *lastOfNew = NULL;
        while (current != NULL) {
            PRUword hash;
            PL_DYNAHASH_HASH(hash, self, current);
            if (hash == newAddress) {
                /* attach to the end of the new chain */
                *lastOfNew = current;
                /* remove from old chain */
                *previous = current->next;
                lastOfNew = &current->next;
                current = current->next;
                *lastOfNew = NULL;
            }
            else {
                /* leave it on the old chain */
                previous = &current->next;
                current = current->next;
            }
            
        }
    }
    return PR_SUCCESS;
}
    
PR_IMPLEMENT(PRStatus)
PL_DynahashAdd(PLDynahash* self, PLDynahashElement* element, PRBool replace,
                    PLDynahashElement* *elementToDestroy)
{
    /* Returns a pointer to the unused thing (either the key or the replaced key). */
    PRStatus err = PR_SUCCESS;
    PRUword h;
    PRUint32 bucketIndex, binIndex;
    PLHashBucket* b;
    PLDynahashElement** p;
    PLDynahashElement* q;
    PLDynahashElement* n;
    
    *elementToDestroy = NULL;    
    if (self->bucketCount == 0) {
        err = pl_DynahashAllocBucket(self);
        if (err != PR_SUCCESS) return err;
    }
    
    PL_DYNAHASH_HASH(h, self, element);
    bucketIndex = DIV(h, PL_DYNAHASH_BUCKETSIZE);
    binIndex = MOD(h, PL_DYNAHASH_BUCKETSIZE);
    
    /* hash ensures valid segment */
    b = self->directory[bucketIndex];
    PR_ASSERT(b);
    
    /* follow collision chain */
    p = &b[binIndex];
    q = *p;
    
    while (q && !self->equal(self->context, q, element)) {
        p = &q->next;
        q = *p;
    }
    if (q && !replace) {
        *elementToDestroy = element;
        goto done;
    }
    
    n = element;
    *p = n;
    if (q) {
        /* replace q with n */
        n->next = q->next;
    }
    else {
        /* table over full? expand it: */
        if (++self->keyCount / MUL(self->bucketCount, PL_DYNAHASH_BUCKETSIZE)
                > PL_DYNAHASH_LOAD_FACTOR) {
            err = pl_DynahashExpand(self);
            if (err != PR_SUCCESS) return err;
        }
    }
    *elementToDestroy = q;
  done:;
    return err;
}
    
PR_IMPLEMENT(PLDynahashElement*)
PL_DynahashRemove(PLDynahash* self, PLDynahashElement* element)
{
    PRUword h;
    PRUint32 bucketIndex, binIndex;
    PLHashBucket* b;
    PLDynahashElement** p;
    PLDynahashElement* q;
    
    if (self->bucketCount == 0)
        return NULL;
    
    PL_DYNAHASH_HASH(h, self, element);
    bucketIndex = DIV(h, PL_DYNAHASH_BUCKETSIZE);
    binIndex = MOD(h, PL_DYNAHASH_BUCKETSIZE);
    b = self->directory[bucketIndex];
    PR_ASSERT(b);
    
    p = &b[binIndex];
    q = *p;
    while (q && !self->equal(self->context, q, element)) {
        p = &q->next;
        q = *p;
    }
    if (q) {
        /* remove q */
        *p = q->next;
        --self->keyCount;
        /* XXX table can be shrunk? shrink it */
    }
    return q;
}

static PLDynahashElement*
PL_DynahashRemove1(PLDynahash* self, PRUint32 bucketIndex, PRInt32 binIndex,
                      int elemIndex)
{
    /* only used by PLDynahashIterator, below */
    PLHashBucket* b;
    PLDynahashElement** p;
    PLDynahashElement* q;
    
    if (self->bucketCount == 0)
        return NULL;
    
    b = self->directory[bucketIndex];
    p = &b[binIndex];
    q = *p;
    
    while (elemIndex--) {
        p = &q->next;
        q = *p;
    }
    if (q) {
        *p = q->next;
        --self->keyCount;
    }
    return q;
}

PR_IMPLEMENT(void)
PL_DynahashEmpty(PLDynahash* self)
{
    PRUint32 i, j;
    for (i = 0; i < self->bucketCount; i++) {
        PLHashBucket* b = self->directory[i];
        for (j = 0; j < PL_DYNAHASH_BUCKETSIZE; j++) {
            PLDynahashElement* q;
            PLDynahashElement* p = b[j];
            while (p) {
                q = p->next;
                self->destroy(self->context, p);
                p = q;
            }
        }
        free(b);
    }
    if (self->bucketCount != 0) {
        free(self->directory);
    }
    self->keyCount = 0;
    self->nextBucketToSplit = 0;
    self->bucketCount = 0;
    self->directory = (PLHashDir*)1;
    self->maxSize = MUL(1, PL_DYNAHASH_BUCKETSIZE);
}

#ifdef DEBUG

PR_IMPLEMENT(PRBool)
PL_DynahashIsValid(PLDynahash* self)
{
    PLDynahashElement* hep;
    PLDynahashIterator iter;
    PL_DynahashIteratorInit(&iter, self);
    while ((hep = PL_DynahashIteratorNext(&iter)) != NULL) {
        if (PL_DynahashLookup(self, hep) != hep) {
            /* something else matches */
            return PR_FALSE;
        }
    }
    return PR_TRUE;
}

#endif /* DEBUG */
    
/*******************************************************************************
 * Hash Table Iterator
 ******************************************************************************/

PR_IMPLEMENT(void)
PL_DynahashIteratorInit(PLDynahashIterator* self, PLDynahash* ht)
{
    self->table = ht;
    self->bucket = 0;
    self->bin = -1;
    self->curno = 0;
    self->lbucket = 0;
    self->lbin = 0;
    self->lcurno = 0;
    self->nextp = NULL;
    self->curp = NULL;
    if (ht->bucketCount == 0) {
        self->done = PR_TRUE;
        self->b = NULL;
    }
    else {
        self->done = PR_FALSE;
        self->b = ht->directory[self->bucket];
        PL_DynahashIteratorNext(self);    /* setup nextp */
    }
}

static void
pl_DynahashIteratorNext1(PLDynahashIterator* self)
{
    if (!self->done) {
        /* save last position in case we have to reset */
        self->lbucket = self->bucket; 
        self->lbin = self->bin;
        self->lcurno = self->curno;
        if (self->nextp) {
            self->nextp = self->nextp->next;
            self->curno++;
        }
        while (!self->nextp) {
            if (++self->bin >= PL_DYNAHASH_BUCKETSIZE) {
                self->bin = 0;
                if (++self->bucket < self->table->bucketCount) {
                    self->b = self->table->directory[self->bucket];
                }
                else {
                    self->done = PR_TRUE;
                    break;
                }
            }
            self->nextp = self->b[self->bin];
            self->curno = 0;
        }
    }
}

PR_IMPLEMENT(PLDynahashElement*)
PL_DynahashIteratorNext(PLDynahashIterator* self)
{
    /* This iterator precomputes one next element ahead so that we can detect
       when we dropped the current element and not skip elements. */
    if (self->curp != self->nextp) {
        self->done = PR_TRUE;
    }
    else {
        pl_DynahashIteratorNext1(self);
    }
    return self->curp;
}

PR_IMPLEMENT(PRUword)
PL_DynahashIteratorCurrentAddress(PLDynahashIterator* self)
{
    return (PRUword)(MUL(self->lbucket, PL_DYNAHASH_BUCKETSIZE) + self->lbin);
}

static PLDynahashElement*
PL_DynahashIteratorRemoveCurrent(PLDynahashIterator* self)
{
    PLDynahashElement* dp = PL_DynahashRemove1(self->table,
                                              self->lbucket,
                                              self->lbin,
                                              (int) self->lcurno);
    if (dp != NULL) {
        if ((self->bin == self->lbin) && (self->bucket == self->lbucket)) {
            self->curno = self->lcurno;
        }
    }
    return dp;
}

PR_IMPLEMENT(void)
PL_DynahashIteratorReset(PLDynahashIterator* self, PLDynahash* ht)
{
    PRInt32 i;
    
    self->table = ht;
    
    if (self->done) return;
    
    if (self->lbin == -1) {
        /* a single next has occurred, will emit first element twice */
        self->lbin = 0;
    }
    
    /* recompute curp from lbucket/lbin/lcurno */
    if (self->lbucket >= self->table->bucketCount) {
        /* we're done */
        self->curp = self->nextp = 0;
        self->done = PR_TRUE;
        return;
    }
    self->b = self->table->directory[self->lbucket];
    self->curp = self->b[self->lbin];
    for (i = 0; i < self->lcurno && self->curp; i++) {
        self->curp = self->curp->next;
    }
    /* nextp also cannot be trusted */
    self->bucket = self->lbucket;
    self->bin = self->lbin;
    self->curno = self->lcurno;
    
    if (self->curp == self->nextp) {
        /* special case where curp has been destroyed and nextp was the next one
           in the bin */
        return;
    }
    
    /* curp mibht be zero, it doesn't matter */
    self->nextp = self->curp;
    pl_DynahashIteratorNext1(self);
}

/*******************************************************************************
 * Test
 ******************************************************************************/

#ifdef DEBUG

typedef struct MyHashElement {
    PLDynahashElement       hashElement;    /* must be first */
    PRUint32            key;
    PRUint32            value;
} MyHashElement;

PRUint32
Dynahash_Test_Hash(void* context, MyHashElement* x)
{
#ifdef XP_MAC
#pragma unused(context)
#endif
    return x->key;
}

PRBool
Dynahash_Test_equal(void* context, MyHashElement* x, MyHashElement* y)
{
#ifdef XP_MAC
#pragma unused(context)
#endif
    return (PRBool)(x->key == y->key);
}

void
Dynahash_Test_destroy(void* context, MyHashElement* x)
{
#ifdef XP_MAC
#pragma unused(context)
#endif
    free(x);
}

#define kDynahash_Test_Iterations    1000

#include <stdio.h>

PR_IMPLEMENT(PRBool)
PL_DynahashTest(void)
{
    PRStatus err;
    PRUint32 i;
    PRBool result = PR_FALSE;
    PLDynahash* ht = PL_NewDynahash(14,
                                    (PLHashFun)Dynahash_Test_Hash,
                                    (PLEqualFun)Dynahash_Test_equal,
                                    (PLDestroyFun)Dynahash_Test_destroy,
                                    NULL);
    
    for (i = 0; i < kDynahash_Test_Iterations; i++) {
        MyHashElement* oldElem;
        MyHashElement* elem;
        printf("seeding %ld\n", i);
        elem = (MyHashElement*)malloc(sizeof(MyHashElement));
        if (elem == NULL) goto done;
        elem->hashElement.next = NULL;
        elem->key = i;
        elem->value = i;
        err = PL_DynahashAdd(ht, (PLDynahashElement*)elem, PR_FALSE,
                                  (PLDynahashElement**)&oldElem);
        if (err != PR_SUCCESS) goto done;
        free(oldElem);
    }
    for (i = 0; i < kDynahash_Test_Iterations; i += 2) {
        MyHashElement elem;
        MyHashElement* found;
        PRBool wasFound = PR_FALSE;
        printf("deleting %ld\n", i);
        elem.hashElement.next = NULL;
        elem.key = i;
        found = (MyHashElement*)PL_DynahashRemove(ht, (PLDynahashElement*)&elem);
        if (found == NULL)
            goto done;
        wasFound = (PRBool)(found->value == i);
        if (!wasFound)
            goto done;
    }
    for (i = 0; i < kDynahash_Test_Iterations; i++) {
        MyHashElement elem;
        MyHashElement* found;
        printf("getting %ld\n", i);
        elem.hashElement.next = NULL;
        elem.key = i;
        found = (MyHashElement*)PL_DynahashLookup(ht, (PLDynahashElement*)&elem);
        if (found) {
            if (i & 1 == 1) {
                PRBool wasFound = (PRBool)(found->value == i);
                if (!wasFound) goto done;
            }
            else {
                goto done;
            }
        }
        else {
            if (i & 1 == 1) {
                goto done;
            }
            /* else ok */
        }
    }
    result = PR_TRUE;
  done:;
    PL_DynahashDestroy(ht);
    PR_ASSERT(result);
    return result;
}

#endif /* DEBUG */

/******************************************************************************/
