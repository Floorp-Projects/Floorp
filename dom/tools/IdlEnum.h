/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _IdlEnum_h__
#define _IdlEnum_h__

#include "IdlObject.h"

class IdlVariable;
class nsVoidArray;

class IdlEnum : public IdlObject {
private:
  nsVoidArray *mEnumerators;

public:
                  IdlEnum();
                  ~IdlEnum();

  void            AddEnumerator(IdlVariable *aEnumerator);
  long            EnumeratorCount();
  IdlVariable*    GetEnumeratorAt(long aIndex);
};


#ifdef XP_MAC
#include <ostream.h>			// required for namespace resolution
#else
class ostream;
#endif
ostream& operator<<(ostream &s, IdlEnum &aEnum);

#endif

