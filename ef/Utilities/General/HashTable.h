/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _HASH_TABLE_
#define _HASH_TABLE_

#include <string.h> // for strdup, strcmp

#include "Fundamentals.h"
#include "DoublyLinkedList.h"
#include "Pool.h"
#include "Vector.h"

#define _NBUCKETS_ 128

/* Operations on HashTable keys, when the keys are (non-interned) strings */
struct StringKeyOps {
  static bool equals(const char * userKey, const char * hashTableKey) {
    return (strcmp(userKey, hashTableKey) == 0);
  }

  static char *copyKey(Pool &pool, const char * userKey) {
    char *hashTableKey = new (pool) char[strlen(userKey)+1];
    strcpy(hashTableKey, userKey);
    return hashTableKey;
  }

  static Uint32 hashCode(const char * userKey) {
    const char *ptr;
    Uint32 hashCode;
    
    for (hashCode = 0, ptr = userKey; *ptr != '\0'; ptr++) {
      hashCode = (hashCode * 7) + *ptr;
    }
    return hashCode;
  }
};

typedef const char * DefaultKeyClass;

template <class N, class KEY = DefaultKeyClass>
struct HashTableEntry : public DoublyLinkedEntry<HashTableEntry<N, KEY> > {
  HashTableEntry(KEY k, N val):key(k),value(val) {};
  KEY key;
  N	 value;
};

template <class N, class KEY = DefaultKeyClass, class KEYOPS = StringKeyOps>
class HashTable {
protected:
  /* Adds key and returns pointer to interned version of the key */
  KEY add(KEY key, const N &value, int hashIndex);

  /* if found, returns the interned version of the key, NULL otherwise.
   * Returns hash index of bucket(key) via hashIndex, and
   * if value is non-null, returns matched value on success
   */
  KEY get(KEY key, N *value, int &hashIndex) const;

  /* If found, sets the value corresponding to given key to N. Does 
  * nothing if an entry corresponding to key was not found.
  */
  void set(KEY key, N *value, int hashIndex);

  /* Function that will compare user's key to key in hash-table. 
   * equals returns true if the userKey is "equal" to hashTableKey,
   * false otherwise.
   */

  Uint32 hashIndexOf(KEY key) const;

  DoublyLinkedList<HashTableEntry<N, KEY> > buckets[_NBUCKETS_];
  Pool &pool;                 /* Pool used to allocate hash-table entries */

private:
  HashTable(const HashTable<N, KEY, KEYOPS>&);			// Copying forbidden
  void operator=(const HashTable<N, KEY, KEYOPS>&);		// Copying forbidden

public:
  explicit HashTable(Pool &p): pool(p) {}

  void add(KEY key, const N &value) {
    (void) add(key, value, hashIndexOf(key));
  }

  bool exists(KEY key) {
    int hashIndex;

    return (get(key, (N *) 0, hashIndex) != 0);
  }

  /* return true if key is valid, and corresponding data; 
   * else return false
   */
  bool get(KEY key, N *data) {
    int hashIndex;
    return (get(key, data, hashIndex) != 0);
  }
  
  N &operator[](KEY key) const;

  //  Vector<N>& operator ()();
  operator Vector<N>& () const;

  // Get all entries matching given key and append them into vector.
  // Return number of matching entries found.
  Int32 getAll(KEY key, Vector<N> &vector) const;

  void remove(KEY key);
};

// Implementation
template <class N, class KEY = DefaultKeyClass, class KEYOPS = StringKeyOps>
Uint32 HashTable<N, KEY, KEYOPS>::hashIndexOf(KEY key) const
{
  Uint32 hashCode = KEYOPS::hashCode(key);
  hashCode = hashCode ^ (hashCode >> 16);
  hashCode = hashCode ^ (hashCode >> 8);
  return (hashCode % _NBUCKETS_);
}
  
template <class N, class KEY = DefaultKeyClass, class KEYOPS = StringKeyOps>
KEY HashTable<N, KEY, KEYOPS>::add(KEY key, 
						           const N& value,
						           int hashIndex)
{
  HashTableEntry<N, KEY> *newEntry =
    new (pool) HashTableEntry<N, KEY>(KEYOPS::copyKey(pool, key), value);
  buckets[hashIndex].addLast(*newEntry);
  return newEntry->key;
}

template <class N, class KEY = DefaultKeyClass, class KEYOPS = StringKeyOps>
KEY HashTable<N, KEY, KEYOPS>::get(KEY key, 
						           N *data,
						           int &hashIndex) const
{
  hashIndex = hashIndexOf(key);
  const DoublyLinkedList<HashTableEntry<N, KEY> >& list = buckets[hashIndex];
  
  for (DoublyLinkedNode *i = list.begin(); !list.done(i); 
    i = list.advance(i)) {
    KEY candidateKey = list.get(i).key;
    
    if (KEYOPS::equals(candidateKey, key)) {
      if (data)
        *data = list.get(i).value;
      return candidateKey;
    }
  }
  
  return 0;
}

template <class N, class KEY = DefaultKeyClass, class KEYOPS = StringKeyOps>
void HashTable<N, KEY, KEYOPS>::set(KEY key, 
                                    N *data,
                                    int hashIndex)
{
  hashIndex = hashIndexOf(key);
  DoublyLinkedList<HashTableEntry<N, KEY> >& list = buckets[hashIndex];
  
  for (DoublyLinkedNode *i = list.begin(); !list.done(i); 
       i = list.advance(i)) {
    KEY candidateKey = list.get(i).key;
    
    if (KEYOPS::equals(candidateKey, key)) {
      if (data)
	list.get(i).value = *data;

      break;
    }
  }
  
}

template <class N, class KEY = DefaultKeyClass, class KEYOPS = StringKeyOps>
Int32 HashTable<N, KEY, KEYOPS>::getAll(KEY key,
                                        Vector<N> &vector) const
{
  Int32 hashIndex = hashIndexOf(key);
  Int32 numMatches = 0;
  const DoublyLinkedList<HashTableEntry<N, KEY> >& list = buckets[hashIndex];

  for (DoublyLinkedNode *i = list.begin(); !list.done(i); 
    i = list.advance(i)) {
    KEY candidateKey = list.get(i).key;
    
    if (KEYOPS::equals(candidateKey, key)) {
      vector.append(list.get(i).value);
      numMatches++;
    }
  }
  
  return numMatches;
}

template <class N, class KEY = DefaultKeyClass, class KEYOPS = StringKeyOps>
N& HashTable<N, KEY, KEYOPS>::operator[](KEY key) const
{
  const DoublyLinkedList<HashTableEntry<N, KEY> >& list = buckets[hashIndexOf(key)];
  for (DoublyLinkedNode *i = list.begin(); !list.done(i); 
       i = list.advance(i)) {
    if (KEYOPS::equals(list.get(i).key, key))
      return list.get(i).value;
  }
  // should never get here.
  return list.get(list.begin()).value;
}

//template<class N> Vector<N> &HashTable<N>::operator()()
template <class N, class KEY = DefaultKeyClass, class KEYOPS = StringKeyOps>
HashTable<N, KEY, KEYOPS>::operator Vector<N>& () const
{
  Vector<N> *vector = new Vector<N>;

  for (Uint32 i = 0; i < _NBUCKETS_; i++) {
    const DoublyLinkedList<HashTableEntry<N, KEY> >& list = buckets[i];
    if (!list.empty())
      for (DoublyLinkedNode *j = list.begin(); !list.done(j); 
	   j = list.advance(j)) {
		vector->append(list.get(j).value);
      }   
  }
  
  return *vector;
}

template <class N, class KEY = DefaultKeyClass, class KEYOPS = StringKeyOps>
void HashTable<N, KEY, KEYOPS>::remove(KEY key)
{
  DoublyLinkedList<HashTableEntry<N, KEY> >& list = buckets[hashIndexOf(key)];
  for (DoublyLinkedNode *i = list.begin(); !list.done(i); 
       i = list.advance(i)) {
    if (KEYOPS::equals(list.get(i).key, key)) {
      list.get(i).remove();;
      return;
    }
  } 
}


#endif


