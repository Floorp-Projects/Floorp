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


#include "xp_hash.h"
#include "xp.h"

#ifdef PROFILE
#pragma profile on
#endif

/*
 * XP_StringHash() is from:
 * Aho, Sethi, and Ullman, "Compilers - Principles, Techniques, and Tools",
 * (the Dragon book), p436, hashpjw() by Peter J. Weinberger.
 * Permission has been kindly granted by Mr Weinberger to release this
 * function as part of the Mozilla public release.
 */
PUBLIC uint32
XP_StringHash (const void *xv)
{ 
  register uint32 h = 0;
  register uint32 g;
  register unsigned const char *x = (unsigned const char *) xv;

  assert(xv);

  if (!x) return 0;

  while (*x != 0)
  {
    h = (h << 4) + *x++;
    if ((g = h & 0xf0000000) != 0)
      h = (h ^ (g >> 24)) ^ g;
  }

  return h;
}

/*	phong's linear congruential hash  */
uint32
XP_StringHash2 (const char *ubuf)
{
	unsigned char * buf = (unsigned char*) ubuf;
	uint32 h=1;
	while(*buf)
	{
		h = 0x63c63cd9*h + 0x9c39c33d + (int32)*buf;
		buf++;
	}
	return h;
}



/* Hash tables.  For sure this time.

   These tables consist of a fixed number of buckets containing linked lists.
   The number of buckets is forced to be prime.  Links in the buckets are
   in no particular order.

   Since each link in the bucket is a seperate malloc'ed block, that incurs
   nontrivial overhead; a more memory efficient model would have us avoid
   colisions and enlarge and rehash the table when it got tight, but that
   requires the table cells to be contiguous in memory, andw ith large
   tables, that could get to be a problem, because of memory fragmentation.
   So it's probably better to use many small mallocs.

   In hash tables, comparison function is used only as an equality test, not
   an ordering test.  Hash lists use the order, but I don't see a benefit to
   keeping links in the buckets ordered.
 */

struct xp_HashTable
{
  XP_HashingFunction  hash_fn;
  XP_HashCompFunction compare_fn;
  uint32 size;
  struct xp_HashBucket **buckets;
};

struct xp_HashBucket
{
  const void *key;
  void *value;
  struct xp_HashBucket *next;
};


static const uint32 primes[] = {
  /* 3, 7, 11, 13, 29, 37, 47, */ 59, 71, 89, 107, 131, 163, 197, 239, 293,
  353, 431, 521, 631, 761, 919, 1103, 1327, 1597, 1931, 2333, 2801, 3371,
  4049, 4861, 5839, 7013, 8419, 10103, 12143, 14591, 17519, 21023, 25229,
  30293, 36353, 43627, 52361, 62851, 75431, 90523, 108631, 130363, 156437,
  187751, 225307, 270371, 324449, 389357, 467237, 560689, 672827, 807403,
  968897, 1162687, 1395263, 1674319, 2009191, 2411033, 2893249 };


static uint32
toprime (uint32 size)
{
  register unsigned int i;
  static unsigned int s = (sizeof (primes) / sizeof (*primes)) - 1;
  for (i = 0; i < s; i++)
	/* Return the smallest prime larger than SIZE, but don't return a
	   prime such that allocating an array of that many pointers will
	   allocate a block larger than 64k.  The toy computers can't do it,
	   and all platforms will assert().
	 */
    if (size <= primes[i] ||
		((primes[i+1] * sizeof(void *)) >= 64000))
	  return primes[i];
  return primes[s-1];
}


/* Create a new, empty hash table object.
 */
PUBLIC XP_HashTable
XP_HashTableNew (uint32 size, 
				 XP_HashingFunction  hash_fn, 
				 XP_HashCompFunction compare_fn)
{
  struct xp_HashTable *table = XP_NEW (struct xp_HashTable);
  if (!table) return 0;
  table->hash_fn = hash_fn;
  table->compare_fn = compare_fn;

  table->size = toprime (size);
  table->buckets = (struct xp_HashBucket **)
	XP_CALLOC (table->size, sizeof (struct xp_HashBucket *));
  if (!table->buckets)
	{
	  XP_FREE (table);
	  return 0;
	}
  return table;
}

/* Remove all entries from the hash table.
 */
PUBLIC void
XP_Clrhash (XP_HashTable table)
{
  XP_ASSERT (table);
  if (!table)
	return;

  XP_ASSERT (table->buckets);
  if (table->buckets)
	{
	  uint32 i;
	  struct xp_HashBucket *bucket, *next;
	  for (i = 0; i < table->size; i++)
		for (bucket = table->buckets[i], next = bucket ? bucket->next : 0;
			 bucket;
			 bucket = next, next = bucket ? bucket->next : 0)
		  XP_FREE (bucket);
	  XP_MEMSET (table->buckets, 0, table->size * sizeof (*table->buckets));
	}
}


/* Clear and free the hash table.
 */
PUBLIC void
XP_HashTableDestroy (XP_HashTable table)
{
  XP_ASSERT (table);
  if (!table)
	return;
  XP_Clrhash (table);
  XP_ASSERT (table->buckets);
  if (table->buckets)
	XP_FREE (table->buckets);
  XP_FREE (table);
}

/* Add an association between KEY and VALUE to the hash table.
   An existing association will be replaced.
   (Note that 0 is a legal value.)
   This can only fail if we run out of memory.
 */
PUBLIC int
XP_Puthash (XP_HashTable table, const void *key, void *value)
{
  register uint32 bucket_num;
  register struct xp_HashBucket *bucket, *prev;
  XP_ASSERT (table);
  if (! table) return -1;

  bucket_num = ((*table->hash_fn) (key)) % table->size;

  /* Iterate over all entries in this bucket.
   */
  for (prev = 0, bucket = table->buckets [bucket_num];
	   bucket;
	   prev = bucket, bucket = bucket->next)

	if (key == bucket->key ||
		0 == (*table->compare_fn) (key, bucket->key))  /* We have a winner! */
	  {
		bucket->value = value;
		return 0;
	  }

  /* If we get here, there was no entry for this key in the table.
   */
  bucket = XP_NEW (struct xp_HashBucket);
  if (! bucket)
	return -1;

  bucket->key = key;
  bucket->value = value;
  bucket->next = 0;

  if (prev)
	/* If prev has a value, it is the last bucket entry in the chain. */
	prev->next = bucket;
  else
	table->buckets [bucket_num] = bucket;

  return 0;
}

/* Remove the for KEY in the table, if it exists.
   Returns FALSE if the key wasn't in the table.
 */
PUBLIC XP_Bool
XP_Remhash (XP_HashTable table, const void *key)
{
  register uint32 bucket_num;
  register struct xp_HashBucket *bucket, *prev;
  XP_ASSERT (table);
  if (! table) return -1;

  bucket_num = ((*table->hash_fn) (key)) % table->size;

  /* Iterate over all entries in this bucket.
   */
  for (prev = 0, bucket = table->buckets [bucket_num];
	   bucket;
	   prev = bucket, bucket = bucket->next)

	if (key == bucket->key ||
		0 == (*table->compare_fn) (key, bucket->key))  /* We have a winner! */
	  {
		if (prev)
		  prev->next = bucket->next;
		else
		  table->buckets [bucket_num] = bucket->next;
		XP_FREE (bucket);
		return TRUE;
	  }

  return FALSE;
}


/* Looks up KEY in the table and returns the corresponding value.
   If KEY is not in the table, `default_value' will be returned instead.
   (This is necessary since 0 is a valid value with which a key can be
   associated.)
 */
PUBLIC void *
XP_Gethash (XP_HashTable table, const void *key, void *default_value)
{
  register uint32 bucket_num;
  register struct xp_HashBucket *bucket;
  XP_ASSERT (table);
  if (! table) return default_value;

  bucket_num = ((*table->hash_fn) (key)) % table->size;
  for (bucket = table->buckets [bucket_num]; bucket; bucket = bucket->next)
	if (key == bucket->key ||
		0 == (*table->compare_fn) (key, bucket->key))  /* We have a winner! */
	  return bucket->value;

  return default_value;
}


static void
xp_maphash (XP_HashTable table, XP_HashTableMapper mapper, void *closure,
			XP_Bool remhash_p)
{
  struct xp_HashBucket *bucket, *next;
  uint32 i;
  XP_ASSERT (table);
  XP_ASSERT (mapper);
  if (!table || !mapper) return;
  /* map over buckets. */
  for (i = 0; i < table->size; i++)
	/* map over bucket entries.  Remember the "next" pointer in case
	   remhash is called. */
	for (bucket = table->buckets[i], next = bucket ? bucket->next :0;
		 bucket;
		 bucket = next, next = bucket ? bucket->next : 0)
	  {
		/* Call the mapper, and terminate early if it returns FALSE.
		   After calling the mapper, but before returning, free and
		   unlink the bucket if we're in remhash-mode.
		 */
		XP_Bool status = (*mapper) (table, bucket->key, bucket->value,closure);
		if (remhash_p)
		  {
			XP_FREE (bucket);
			/* It always becomes the top of the list, since we've freed
			   the others. */
			table->buckets [i] = next;
		  }
		if (status == FALSE)
		  return;
	  }
}


/* Apply a function to each pair of elements in the hash table.
   If that function returns FALSE, then the mapping stops prematurely.
   The mapping function may call XP_Remhash() on the *current* key, but
   not on any other key in this table.  It also may not clear or destroy
   the table.
 */
PUBLIC void
XP_Maphash (XP_HashTable table, XP_HashTableMapper mapper, void *closure)
{
  xp_maphash (table, mapper, closure, FALSE);
}

/* Apply a function to each pair of elements in the hash table.
   After calling the function, that pair will be removed from the table.
   If the function returns FALSE, then the mapping stops prematurely.
   Any items which were not mapped over will still remain in the table,
   but those items which were mapped over will have been freed.

   This could also be done by having the mapper function unconditionally
   call XP_Remhash(), but using this function will be slightly more efficient.
 */
PUBLIC void
XP_MapRemhash (XP_HashTable table, XP_HashTableMapper mapper, void *closure)
{
  xp_maphash (table, mapper, closure, TRUE);
}



/* create a hash list, which isn't really a table.
 */
PUBLIC XP_HashList *
XP_HashListNew (int size, 
				  XP_HashingFunction  hash_func, 
				  XP_HashCompFunction comp_func)
{
    XP_HashList * hash_struct = XP_NEW(XP_HashList);
    XP_List **hash_list;
	int new_size;

    if(!hash_struct)
	    return(NULL);

	new_size = toprime (size);
	XP_ASSERT (new_size >= size);

    hash_list = (XP_List **) XP_ALLOC(sizeof(XP_List *) * new_size);

    if(!hash_list)
	  {
		XP_FREE(hash_struct);
		return(NULL);
	  }

    /* zero out the list
     */
    XP_MEMSET(hash_list, 0, sizeof(XP_List *) * new_size);

    hash_struct->list          = hash_list;
    hash_struct->size          = new_size;
    hash_struct->hash_func     = hash_func;
    hash_struct->comp_func     = comp_func;
    
    return(hash_struct);
}

/* free a hash list, which isn't really a table.
 */
PUBLIC void
XP_HashListDestroy (XP_HashList * hash_struct)
{
    if(!hash_struct)
	   return;

    XP_FREE(hash_struct->list);
    XP_FREE(hash_struct);
}

/* add an element to a hash list, which isn't really a table.
 *
 * returns positive on success and negative on failure
 *
 * ERROR return codes
 *
 *  XP_HASH_DUPLICATE_OBJECT
 */
PUBLIC int
XP_HashListAddObject (XP_HashList * hash_struct, void * new_ele)
{
    uint32   bucket_num;
    int      result;
    XP_List * list_ptr;
    void   * obj_ptr;

    if(!hash_struct)
	   return -1;

    /* get an integer from the hashing function 
     */
    bucket_num = (*hash_struct->hash_func)(new_ele);
 
    /* adjust the integer to the size of the hash table
     */
    bucket_num = bucket_num % hash_struct->size;

    list_ptr = hash_struct->list[bucket_num];

    if(!list_ptr)
	  {
	    list_ptr = XP_ListNew();
	
		if(!list_ptr)
			return -1;

		hash_struct->list[bucket_num] = list_ptr;
	  }

    /* run through the list and find an object that returns
     * greater than 0 when compared
     */
    while((obj_ptr = XP_ListNextObject(list_ptr)) != 0)
	  {
		result = (*hash_struct->comp_func)(obj_ptr, new_ele);
		if(result > -1)
		   break;
	  }

    if(obj_ptr && result == 0)
	  {
		/* the objects are the same!
		 */
		return(XP_HASH_DUPLICATE_OBJECT);
	  }

    if(obj_ptr)
	  {
		/* insert right before the obj_ptr 
	     */
        XP_ListInsertObject(hash_struct->list[bucket_num], obj_ptr, new_ele);
	  }
	else
	  {
		/* it's either the first element to go into the list
         * or it is the last by comparison
	     */
        XP_ListAddObjectToEnd(hash_struct->list[bucket_num], new_ele);
	  }

    return 0; /* #### what should return value be? */
}

/* finds an object by name in the hash list, which isn't really a table.
 */
PUBLIC void *
XP_HashListFindObject (XP_HashList * hash_struct, void * ele)
{
    uint32   bucket_num;
    int      result;
    XP_List * list_ptr;
    void   * obj_ptr;

    if(!hash_struct)
	   return(NULL);

    /* get an integer from the hashing function
     */
    bucket_num = (*hash_struct->hash_func)(ele);

    /* adjust the integer to the size of the hash table
     */
    bucket_num = bucket_num % hash_struct->size;

    list_ptr = hash_struct->list[bucket_num];

    /* run through the list and find the object
	 */
    while((obj_ptr = XP_ListNextObject(list_ptr)) != 0)
      {
		result = (*hash_struct->comp_func)(obj_ptr, ele);

        if(result == 0)
           return(obj_ptr);

        if(result > 0)
           return(NULL);

      }

    return(NULL);
}

/* removes an object by name from the hash list, which isn't really a table,
 * and returns the object if found
 */
PUBLIC void *
XP_HashListRemoveObject (XP_HashList * hash_struct, void * ele)
{
    uint32   bucket_num;
    int      result;
    XP_List * list_ptr;
    void   * obj_ptr;

    if(!hash_struct || !ele)
	   return(NULL);

    /* get an integer from the hashing function
     */
    bucket_num = (*hash_struct->hash_func)(ele);

    /* adjust the integer to the size of the hash table
     */
    bucket_num = bucket_num % hash_struct->size;

    list_ptr = hash_struct->list[bucket_num];

    /* run through the list and find the object
     */
    while((obj_ptr = XP_ListNextObject(list_ptr)) != 0)
      {
		result = (*hash_struct->comp_func)(obj_ptr, ele);

		if(result == 0)
		  {
	        XP_ListRemoveObject(hash_struct->list[bucket_num], obj_ptr);

/* ALEKS. Bucket needs to be freed here if there are no more objects in it*/
	        if (hash_struct->list[bucket_num]->next == NULL)
	        {
				XP_ListDestroy(hash_struct->list[bucket_num]);
				hash_struct->list[bucket_num] = NULL;
			}
			return(obj_ptr);
		  }
		
		if(result > 0)
		   return(NULL);
      }
    return(NULL);
}

#ifdef PROFILE
#pragma profile on
#endif
