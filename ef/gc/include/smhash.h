/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; -*-  
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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

#ifndef __SMHASH__
#define __SMHASH__

#include "smobj.h"

SM_BEGIN_EXTERN_C

/******************************************************************************/

typedef struct SMHashEntry {
    PRUword             key;
    SMObject*           value;
} SMHashEntry;

typedef struct SMHashTable {
    SMArrayStruct       array;
    SMHashEntry         entry[1];
    /* other hash table entries follow */
} SMHashTable;

#define SM_HASHTABLE_ENTRIES(ht)        (&(ht)->entry0)

#define SM_HASHTABLE_SIZES              32
extern PRUword sm_HashPrimes[SM_HASHTABLE_SIZES];
extern PRUword sm_HashMasks[SM_HASHTABLE_SIZES];

#define SM_HASHTABLE_HASH1(ht, k) \
    (((k) * sm_HashPrimes[ht->inherit.size]) & sm_HashMasks[ht->inherit.size])

#define SM_HASHTABLE_HASH2(h) \
    (((h) >> sm_HashMasks[ht->inherit.size - 1]) | 1)

/******************************************************************************/

SM_EXTERN(SMHashTable*)
SM_NewHashTable(PRUword initialSize, PRUword maxSize);

SM_EXTERN(SMObject*)
SM_HashTableLookup(SMHashTable* self, PRUword key);

SM_EXTERN(void)
SM_HashTableInsert(SMHashTable* self, PRUword key, SMObject* value);

SM_EXTERN(SMObject*)
SM_HashTableRemove(SMHashTable* self, PRUword key);

SM_EXTERN(SMObject*)
SM_HashTableDelete(SMHashTable* self, PRUword key);

/******************************************************************************/

SM_END_EXTERN_C

#endif /* __SMHASH__ */
/******************************************************************************/
