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
#ifndef _COLLECTOR_H_
#define _COLLECTOR_H_

#include "Pool.h"

/* A class that holds a collection of objects of class N of a fixed size that
 * may not all be added to the collection at the same time. Keeps track of
 * how many objects have been actually added, and whether or not the 
 * collection is full (ie all objects have been added).
 */
template<class N> class Collector {
private:
  N **entries;
  Uint32 numEntries;

  bool allEntriesBuilt;
  Uint32 numEntriesBuilt;

  Pool &pool;

public:
  Collector(Pool &p) : 
    entries(0), numEntries(0), allEntriesBuilt(false), numEntriesBuilt(0),
    pool(p) { }

  /* Set the size of the collection. You can only do this once */
  void setSize(Uint32 n) {
    assert(!numEntries);

    numEntries = n;
    entries = new (pool) N *[n];

    for (Uint32 i = 0; i < n; i++)
      entries[i] = 0;
  }

  /* Add an entry to the collection at index n. You can only do this once. */
  void set(Uint32 n, N *entry) {
    assert(n < numEntries && !entries[n] && entry);
    
    entries[n] = entry;
    if (++numEntriesBuilt == numEntries)
      allEntriesBuilt = true;
  }
    
  /* Get the entry at index n, NIL if none has been set yet. */
  N *get(Uint32 n) {
    assert(n < numEntries);
    return entries[n];
  }

  /* return true if all entries have been built */
  bool built() { return allEntriesBuilt; }

  /* Return number of entries built */
  Int32 getNumEntriesBuilt() { return numEntriesBuilt; }

  /* return all entries */
  N **getEntries() { return entries; }

  /* return number of entries */
  Uint32 getSize() { return numEntries; }
};


#endif /* _COLLECTOR_H_ */
