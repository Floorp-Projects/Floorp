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

#include "smhash.h"

#define SM_MASK(n)      ((1U << (n)) - 1)

#define SM_HASHTABLE_SMALLEST_POWER      6
#define SM_HASHTABLE_LARGEST_POWER       32

PRUword sm_HashMasks[SM_HASHTABLE_SIZES] = {
    SM_MASK(6),
    SM_MASK(7),
    SM_MASK(8),
    SM_MASK(9),
    SM_MASK(10),
    SM_MASK(11),
    SM_MASK(12),
    SM_MASK(13),
    SM_MASK(14),
    SM_MASK(15),
    SM_MASK(16),
    SM_MASK(17),
    SM_MASK(18),
    SM_MASK(19),
    SM_MASK(20),
    SM_MASK(21),
    SM_MASK(22),
    SM_MASK(23),
    SM_MASK(24),
    SM_MASK(25),
    SM_MASK(26),
    SM_MASK(27),
    SM_MASK(28),
    SM_MASK(29),
    SM_MASK(30),
    SM_MASK(31),
    0xffffffff
};

PRUword sm_HashPrimes[SM_HASHTABLE_SIZES] = {
    61,
    127,
    251,
    509,
    1021,
    2039,
    4093,
    8191,
    16381,
    32749,
    65521,
    131071,
    262139,
    524287,
    1048573,
    2097143,
    4194301,
    8388593,
    16777213,
    33554393,
    67108859,
    134217689,
    268435399,
    536870909,
    1073741789,
    2147483647,
    4294967291
};

static void
sm_TestHashTables(void)
{
    PRUword i;
    for (i = SM_HASHTABLE_SMALLEST_POWER; i <= SM_HASHTABLE_LARGEST_POWER; i++) {
        PRUword size = (1 << i);
        SM_ASSERT(sm_HashMasks[i] + 1 == size);
        SM_ASSERT(sm_HashPrimes[i] < size);
    }
}

/******************************************************************************/

/******************************************************************************/

#define SM_GOLDEN_RATIO         616161
#define SM_HASH1(key, size)     (((key) * SM_GOLDEN_RATIO) & size)
#define SM_HASH2(key, size)     (((key) * 97) + 1)

SMArrayClass sm_HashTableClass;

SM_IMPLEMENT(SMHashTable*)
SM_NewHashTable(PRUword initialSize, PRUword maxSize)
{
    SMArray* obj;
    static PRBool hashTableClassInitialized = PR_FALSE;

    if (!hashTableClassInitialized) {
        SMClass* elementClass = NULL;   /* XXX */
        SM_InitArrayClass(&sm_HashTableClass, NULL, elementClass);
        hashTableClassInitialized = PR_TRUE;
    }

    obj = SM_AllocArray(&sm_HashTableClass, initialSize);
    return (SMHashTable*)obj;
}

SM_IMPLEMENT(SMObject*)
SM_HashTableLookup(SMHashTable* self, PRUword key)
{
    PRUword htSize = self->array.size;
    PRUword hash1 = SM_HASH1(key, htSize);
    PRUword hash2 = SM_HASH2(key, htSize);
    PRUword i = hash1;
    SMHashEntry* elements = self->entry;

    SM_ASSERT(key != 0);
    while (elements[i].key != 0) {
        if (elements[i].key == key)
            return elements[i].value;
        else {
            i = (i + hash2) % htSize;
            SM_ASSERT(i != hash1);
        }
    }
    return NULL;
}

SM_IMPLEMENT(void)
SM_HashTableInsert(SMHashTable* self, PRUword key, SMObject* value)
{
    PRUword htSize = self->array.size;
    PRUword hash1 = SM_HASH1(key, htSize);
    PRUword hash2 = SM_HASH2(key, htSize);
    PRUword i = hash1;
    SMHashEntry* elements = self->entry;

    SM_ASSERT(key != 0);
    while (elements[i].key != 0) {
        i = (i + hash2) % htSize;
        SM_ASSERT(i != hash1);
    }
    elements[i].key = key;
    elements[i].value = value;
}

SM_IMPLEMENT(SMObject*)
SM_HashTableRemove(SMHashTable* self, PRUword key)
{
    return NULL;
}

SM_IMPLEMENT(SMObject*)
SM_HashTableDelete(SMHashTable* self, PRUword key)
{
    return NULL;
}

/******************************************************************************/
