/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _STRING_POOL_H_
#define _STRING_POOL_H_

#include "HashTable.h"

class StrNode {
  int32 dummy;
};

class StringPool : private HashTable<StrNode> {
public:
  StringPool(Pool &p) : HashTable<StrNode>(p) { }
  
  /* Intern "str" if not already interned and return a pointer to
   * the interned string
   */
  const char *intern(const char *str) {
    int hashIndex;
    const char *key;
    
    if ((key = HashTable<StrNode>::get(str, 0, hashIndex)) != 0)
      return key;
    
    return add(str, node, hashIndex);
  }

  /* Returns a pointer to the interned string if it exists, NULL 
   * otherwise
   */
  const char *get(const char *str) {
    int hashIndex;

    return HashTable<StrNode>::get(str, 0, hashIndex);
  }

private:
  StrNode node;
};

#endif /* _STRING_POOL_H_ */


