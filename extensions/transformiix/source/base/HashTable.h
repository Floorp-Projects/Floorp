/*
 * (C) Copyright The MITRE Corporation 1999  All rights reserved.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * The program provided "as is" without any warranty express or
 * implied, including the warranty of non-infringement and the implied
 * warranties of merchantibility and fitness for a particular purpose.
 * The Copyright owner will not be liable for any damages suffered by
 * you as a result of using the Program. In no event will the Copyright
 * owner be liable for any special, indirect or consequential damages or
 * lost profits even if the Copyright owner has been advised of the
 * possibility of their occurrence.
 *
 * Please see release.txt distributed with this file for more information.
 *
 */

// Tom Kneeland (01/21/2000)
//
//  Implementation of a simple Hash Table.  All objects are stored as 
//  MITREObject  pointers and hashed using the provided value.  Collisions
//  are solved using a linked list for each bin.
//
//  NOTE:  MITREObjects are used for storage to ease destruction.  Now the
//         HashTable can clean itself up, all object don't need to be removed
//         manually before the table is destroyed.
//
// Modification History:
// Who  When        What

#ifndef MITRE_HASHTABLE
#define MITRE_HASHTABLE

#include "baseutils.h"
#include "MITREObject.h"

#ifndef NULL
 #define NULL 0
#endif

#define HASHTABLE_SIZE 10

class HashTable
{
 public: 
  HashTable(); 
  ~HashTable();

  //Added the provided object to the hash table.  If the item already exists,
  //replace it, and delete the old copy.
#ifdef MOZ_XSL
  void add(MITREObject* obj, void* hashValue);
#else
  void add(MITREObject* obj, Int32 hashValue);
#endif

  //Locate and Retrieve the specified object based on the provided value.
#ifdef MOZ_XSL
  MITREObject* retrieve(void* hashValue);
#else
  MITREObject* retrieve(Int32 hashValue);
#endif

  //Locate, Remove, and Return the specified object based on the provided value.
#ifdef MOZ_XSL
  MITREObject* remove(void* hashValue);
#else
  MITREObject* remove(Int32 hashValue);
#endif

 private:
  typedef struct _HashItem
  {
    _HashItem* prevItem;
    _HashItem* nextItem;
#ifdef MOZ_XSL
    void* hashValue;
#else
    Int32 hashValue;
#endif
    MITREObject* objPtr;
  }HashItem;


  HashItem* table[HASHTABLE_SIZE];

#ifdef MOZ_XSL
  HashItem* retrieveHashItem(void* hashValue);
#else
  HashItem* retrieveHashItem(Int32 hashValue);
#endif
};

#endif
