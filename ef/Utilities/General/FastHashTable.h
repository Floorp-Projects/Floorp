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
#ifndef _FAST_HASH_TABLE_H_
#define _FAST_HASH_TABLE_H_ 

#include "HashTable.h"

/* A faster version of class Hashtable, which makes the assumption that
 * all keys are interned somewhere, ie., that there is only one copy
 * of each unique string in the universe. 
 */

/* Operations on HashTable keys, when keys are interned strings */
struct InternedStringKeyOps {
  static bool equals(const char * userKey, const char * hashTableKey) {
    return (userKey == hashTableKey);
  }

  static char *copyKey(Pool &/*pool*/, const char * userKey) {
    return const_cast<char *>(userKey);
  }

  static Uint32 hashCode(const char * userKey) {
    return reinterpret_cast<Uint32>(userKey);
  }
};

template <class N> class FastHashTable : public HashTable<N, const char *, InternedStringKeyOps> {
public:
  FastHashTable(Pool &p) : HashTable<N, const char *, InternedStringKeyOps>(p) { } 
};

#endif /* _FAST_HASH_TABLE_H_ */
