/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _XP_HASH_
#define _XP_HASH_

#include "xp_list.h"

XP_BEGIN_PROTOS

typedef uint32 (*XP_HashingFunction) (const void *ele);

/* A hash compare function is like strcmp - it should return negative, zero,
   or positive depending on the ordering and equality of its arguments.
 */
typedef int (*XP_HashCompFunction) (const void *ele1, const void *ele2);

/* get hash number from a string */
extern uint32 XP_StringHash (const void *xv);
/* Phong's linear congruential hash  */
extern uint32 XP_StringHash2 (const char *ubuf);

/* Hash Tables.
 */

typedef struct xp_HashTable *XP_HashTable;   /* opaque */

typedef XP_Bool (*XP_HashTableMapper) (XP_HashTable table,
				       const void *key, void *value, 
				       void *closure);

/* Create a new, empty hash table object.
   SIZE should be your best guess at how many items will go into this
   table; if SIZE is too small, that's ok, but there will be a small
   performance hit.  The size need not be prime.
 */
extern XP_HashTable XP_HashTableNew (uint32 size,
				     XP_HashingFunction  hash_fn, 
				     XP_HashCompFunction compare_fn);

/* Clear and free the hash table.
 */
extern void XP_HashTableDestroy (XP_HashTable table);

/* Remove all entries from the hash table.
 */
extern void XP_Clrhash (XP_HashTable table);

/* Add an association between KEY and VALUE to the hash table.
   An existing association will be replaced.
   (Note that 0 is a legal value.)
   This can only fail if we run out of memory.
 */
extern int XP_Puthash (XP_HashTable table, const void *key, void *value);

/* Remove the for KEY in the table, if it exists.
   Returns FALSE if the key wasn't in the table.
 */
extern XP_Bool XP_Remhash (XP_HashTable table, const void *key);

/* Looks up KEY in the table and returns the corresponding value.
   If KEY is not in the table, `default_value' will be returned instead.
   (This is necessary since 0 is a valid value with which a key can be
   associated.)
 */
extern void *XP_Gethash (XP_HashTable table, const void *key,
			 void *default_value);

/* Apply a function to each pair of elements in the hash table.
   If that function returns FALSE, then the mapping stops prematurely.
   The mapping function may call XP_Remhash() on the *current* key, but
   not on any other key in this table.  It also may not clear or destroy
   the table.
 */
extern void XP_Maphash (XP_HashTable table, XP_HashTableMapper mapper,
			void *closure);

/* Apply a function to each pair of elements in the hash table.
   After calling the function, that pair will be removed from the table.
   If the function returns FALSE, then the mapping stops prematurely.
   Any items which were not mapped over will still remain in the table,
   but those items which were mapped over will have been freed.

   This could also be done by having the mapper function unconditionally
   call XP_Remhash(), but using this function will be slightly more efficient.
 */
extern void XP_MapRemhash (XP_HashTable table, XP_HashTableMapper mapper,
			   void *closure);


/* ===========================================================================
   Hash Lists, which aren't really hash tables.
 */


#define XP_HASH_DUPLICATE_OBJECT -99

typedef struct _XP_HashList {
    XP_List             **list;
    int                  size;
    XP_HashingFunction  hash_func;
    XP_HashCompFunction comp_func;
} XP_HashList;

/* create a hash list
 */
extern XP_HashList *
XP_HashListNew (int size, XP_HashingFunction hash_func, XP_HashCompFunction comp_func);

/* free a hash list
 */
extern void
XP_HashListDestroy (XP_HashList * hash_struct);

/* add an element to a hash list
 *
 * returns positive on success and negative on failure
 *
 * ERROR return codes
 *
 *  XP_HASH_DUPLICATE_OBJECT
 */
extern int
XP_HashListAddObject (XP_HashList * hash_struct, void * new_ele);

/* finds an object by name in the hash list
 */
extern void *
XP_HashListFindObject (XP_HashList * hash_struct, void * ele);

/* removes an object by name from the hash list
 * and returns the object if found
 */
extern void *
XP_HashListRemoveObject (XP_HashList * hash_struct, void * ele);

XP_END_PROTOS

#endif /* _XP_HASH_ */
