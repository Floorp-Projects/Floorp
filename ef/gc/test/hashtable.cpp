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

#include "plhash.h"
#include "dynahash.h"
#include "openhash.h"
#include "prlog.h"
#include "FastHashTable.h"  // Electrical Fire s
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define ITERATIONS   10000

/******************************************************************************/

//#define HASH_ALLOC      1

static PLHashNumber
HashFunction(const void *key)
{
    return (PLHashNumber)key;
}

static PRIntn
HashKeyComparator(const void *v1, const void *v2)
{
    return (int)v1 == (int)v2;
}

static PRIntn
HashValueComparator(const void *v1, const void *v2)
{
#ifdef HASH_ALLOC
    return *(PRUword*)v1 == *(PRUword*)v2;
#else
    return (PRUword)v1 == (PRUword)v2;
#endif
}

/******************************************************************************/

static void
HashTableAdd(PLHashTable* ht, PRUword i)
{
    void* elem;
    void* value;
#ifdef HASH_ALLOC
    value = malloc(sizeof(PRUword));
    *(PRUword*)value = i;
#else
    value = (void*)i;
#endif
    elem = PL_HashTableAdd(ht, (const void*)i, value);
    PR_ASSERT(elem != NULL);
}

static void
HashTableLookup(PLHashTable* ht, PRUword i)
{
    void* value;
    PRUword v;
    value = PL_HashTableLookup(ht, (const void*)i);
#ifdef HASH_ALLOC
    v = *(PRUword*)value;
#else
    v = (PRUword)value;
#endif
    PR_ASSERT(v == i);
}

static void
HashTableDelete(PLHashTable* ht, PRUword i)
{
    PRBool wasFound;
#ifdef HASH_ALLOC
    PLHashEntry* entry = *PL_HashTableRawLookup(ht, i, (const void*)i);
    free(entry->value);
#endif
    wasFound = PL_HashTableRemove(ht, (const void*)i);
    PR_ASSERT(wasFound);
}

static void
TestHashTable(int initialSize)
{
    PRUword i;
    PRIntervalTime t1, t2;
    PLHashTable* ht;

    ht = PL_NewHashTable(initialSize, HashFunction, HashKeyComparator,
    					 HashValueComparator, NULL, NULL);
    PR_ASSERT(ht);
    fprintf(stdout, "prhash\t%d", initialSize);

    t1 = PR_IntervalNow();
    for (i = 0; i < ITERATIONS; i++) {
        HashTableAdd(ht, i);
    }
    t2 = PR_IntervalNow();
    fprintf(stdout, "\t%ldus", PR_IntervalToMicroseconds(t2 - t1));

    t1 = PR_IntervalNow();
    for (i = 0; i < ITERATIONS; i++) {
        HashTableLookup(ht, i);
    }
    t2 = PR_IntervalNow();
    fprintf(stdout, "\t%ldus", PR_IntervalToMicroseconds(t2 - t1));

    t1 = PR_IntervalNow();
    for (i = 0; i < ITERATIONS; i++) {
        HashTableDelete(ht, i);
    }
    t2 = PR_IntervalNow();
    fprintf(stdout, "\t%ldus\n", PR_IntervalToMicroseconds(t2 - t1));
    
    PL_HashTableDestroy(ht);
}

/******************************************************************************/

typedef struct MyHashElement {
    PLDynahashElement   hashElement;    /* must be first */
    PRUint32            key;
    PRUint32            value;
} MyHashElement;

static PRUint32
Dynahash_HashKey(void* context, MyHashElement* x)
{
#ifdef XP_MAC
#pragma unused(context)
#endif
    return x->key;
}

static PRBool
Dynahash_Equals(void* context, MyHashElement* x, MyHashElement* y)
{
#ifdef XP_MAC
#pragma unused(context)
#endif
    return (PRBool)(x->key == y->key);
}

static void
Dynahash_Destroy(void* context, MyHashElement* x)
{
#ifdef XP_MAC
#pragma unused(context)
#endif
    free(x);
}

/******************************************************************************/

static void
DynahashAdd(PLDynahash* ht, PRUword i)
{
    PRStatus status;
    MyHashElement* oldElem;
    MyHashElement* elem;
    elem = (MyHashElement*)malloc(sizeof(MyHashElement));
    PR_ASSERT(elem != NULL);
    elem->hashElement.next = NULL;
    elem->key = i;
    elem->value = i;
    status = PL_DynahashAdd(ht, (PLDynahashElement*)elem, PR_FALSE,
                            (PLDynahashElement**)&oldElem);
    PR_ASSERT(status == PR_SUCCESS);
    free(oldElem);
}

static void
DynahashLookup(PLDynahash* ht, PRUword i)
{
    MyHashElement elem;
    MyHashElement* found;
    PRBool wasFound;
    elem.hashElement.next = NULL;
    elem.key = i;
    found = (MyHashElement*)PL_DynahashLookup(ht, (PLDynahashElement*)&elem);
    PR_ASSERT(found);
    wasFound = (PRBool)(found->value == i);
    PR_ASSERT(wasFound);
}

static void
DynahashDelete(PLDynahash* ht, PRUword i)
{
    MyHashElement elem;
    MyHashElement* found;
    PRBool wasFound = PR_FALSE;
    elem.hashElement.next = NULL;
    elem.key = i;
    found = (MyHashElement*)PL_DynahashRemove(ht, (PLDynahashElement*)&elem);
    PR_ASSERT(found);
    wasFound = (PRBool)(found->value == i);
    PR_ASSERT(wasFound);
    free(found);
}

static void
TestDynahash(int initialSize)
{
    PRUword i;
    PRIntervalTime t1, t2;
    PLDynahash* ht;

    ht = PL_NewDynahash(initialSize,
                        (PLHashFun)Dynahash_HashKey,
                        (PLEqualFun)Dynahash_Equals,
                        (PLDestroyFun)Dynahash_Destroy,
                        NULL);
    PR_ASSERT(ht);
    fprintf(stdout, "dyna\t%d", initialSize);
                                    
    t1 = PR_IntervalNow();
    for (i = 0; i < ITERATIONS; i++) {
        DynahashAdd(ht, i);
    }
    t2 = PR_IntervalNow();
    fprintf(stdout, "\t%ldus", PR_IntervalToMicroseconds(t2 - t1));

    t1 = PR_IntervalNow();
    for (i = 0; i < ITERATIONS; i++) {
        DynahashLookup(ht, i);
    }
    t2 = PR_IntervalNow();
    fprintf(stdout, "\t%ldus", PR_IntervalToMicroseconds(t2 - t1));

    t1 = PR_IntervalNow();
    for (i = 0; i < ITERATIONS; i++) {
        DynahashDelete(ht, i);
    }
    t2 = PR_IntervalNow();
    fprintf(stdout, "\t%ldus\n", PR_IntervalToMicroseconds(t2 - t1));
    
    PL_DynahashDestroy(ht);
}

/******************************************************************************/

static void
Openhash_Destroy(void* context, PLOpenhashElement* elem)
{
#ifdef XP_MAC
#pragma unused(context)
#endif
#ifdef HASH_ALLOC
    free(elem->value);
#endif
}

/******************************************************************************/

static void
OpenhashAdd(PLOpenhash* ht, PRUword i)
{
#ifdef HASH_ALLOC
    void* elem = malloc(sizeof(PRUword));
    PR_ASSERT(elem != NULL);
    *(PRUword*)elem = i;
#else
    void* elem = (void*)i;
#endif
    PL_OpenhashAdd(ht, i, elem);
}

static void
OpenhashLookup(PLOpenhash* ht, PRUword i)
{
    void* elem = PL_OpenhashLookup(ht, i);
#ifdef HASH_ALLOC
    PR_ASSERT(elem);
    PR_ASSERT(*(PRUword*)elem == i);
#else
    PR_ASSERT((PRUword)elem == i);
#endif
}

static void
TestOpenhash(int initialSize)
{
    PRUword i;
    PRIntervalTime t1, t2;
    PLOpenhash* ht;

    ht = PL_NewOpenhash(initialSize,
                        (PLOpenhashHashFun)NULL,
                        (PLOpenhashEqualFun)NULL,
                        (PLOpenhashDestroyFun)Openhash_Destroy,
                        NULL);
    PR_ASSERT(ht);
    fprintf(stdout, "open\t%d", initialSize);
                                    
    t1 = PR_IntervalNow();
    for (i = 0; i < ITERATIONS; i++) {
        OpenhashAdd(ht, i);
    }
    t2 = PR_IntervalNow();
    fprintf(stdout, "\t%ldus", PR_IntervalToMicroseconds(t2 - t1));

    t1 = PR_IntervalNow();
    for (i = 0; i < ITERATIONS; i++) {
        OpenhashLookup(ht, i);
    }
    t2 = PR_IntervalNow();
    fprintf(stdout, "\t%ldus\n", PR_IntervalToMicroseconds(t2 - t1));

    PL_OpenhashDestroy(ht);
}

/******************************************************************************/

class EFHash : public FastHashTable<void*> {
public:
    EFHash(Pool& pool) : FastHashTable<void*>(pool) {}
};

static void
EFHashAdd(EFHash* ht, PRUword i)
{
#ifdef HASH_ALLOC
    void* elem = malloc(sizeof(PRUword));
    PR_ASSERT(elem != NULL);
    *(PRUword*)elem = i;
#else
    void* elem = (void*)i;
#endif
    ht->add((const char*)i, elem);
}

static void
EFHashLookup(EFHash* ht, PRUword i)
{
    void* elem;
    bool found = ht->get((const char*)i, &elem);
    PR_ASSERT(found);
#ifdef HASH_ALLOC
    PR_ASSERT(*(PRUword*)elem == i);
#else
    PR_ASSERT((PRUword)elem == i);
#endif
}

static void
EFHashDelete(EFHash* ht, PRUword i)
{
#ifdef HASH_ALLOC
    void* elem;
    bool found = ht->get((const char*)i, &elem);
    free(elem);
#endif
    ht->remove((const char*)i);
}

static void
TestEFHash()
{
    PRUword i;
    PRIntervalTime t1, t2;
    EFHash* ht;

    Pool* pool = new Pool();
    PR_ASSERT(pool);
    ht = new EFHash(*pool);

    PR_ASSERT(ht);
    fprintf(stdout, "efhash\t");
                                    
    t1 = PR_IntervalNow();
    for (i = 0; i < ITERATIONS; i++) {
        EFHashAdd(ht, i);
    }
    t2 = PR_IntervalNow();
    fprintf(stdout, "\t%ldus", PR_IntervalToMicroseconds(t2 - t1));

    t1 = PR_IntervalNow();
    for (i = 0; i < ITERATIONS; i++) {
        EFHashLookup(ht, i);
    }
    t2 = PR_IntervalNow();
    fprintf(stdout, "\t%ldus", PR_IntervalToMicroseconds(t2 - t1));

    t1 = PR_IntervalNow();
    for (i = 0; i < ITERATIONS; i++) {
        EFHashDelete(ht, i);
    }
    t2 = PR_IntervalNow();
    fprintf(stdout, "\t%ldus\n", PR_IntervalToMicroseconds(t2 - t1));
    
    delete ht;
    delete pool;
}

/******************************************************************************/

int
main(void)
{
#if 0
    PRBool success = PL_DynahashTest();
    fprintf(stdout, "DynahashTest => %s\n", success ? "true" : "false");
#endif

    fprintf(stdout, "SportModel: Hash table comparison tests\n");

    fprintf(stdout, "\tsize\tadd\tlookup\tremove\n");

    TestHashTable(1024 * 8 - 8);
    TestHashTable(1024 - 8);
    TestHashTable(8);

    TestDynahash(1024 * 8 - 8);
    TestDynahash(1024 - 8);
    TestDynahash(8);

    TestOpenhash(1024 * 256);
    TestOpenhash(1024 * 128);
    TestOpenhash(1024 * 16);

    TestEFHash();

    return 0;
}

/******************************************************************************/
