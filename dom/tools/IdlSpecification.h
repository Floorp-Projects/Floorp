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

#ifndef _IdlSpecification_h__
#define _IdlSpecification_h__

#include "IdlObject.h"

class IdlInterface;
class nsVoidArray;

class IdlSpecification : public IdlObject {
private:
  nsVoidArray *mInterfaces;

public:
                  IdlSpecification();
                  ~IdlSpecification();

  void            AddInterface(IdlInterface *aInterface);
  int             ContainInterface(char *aInterfaceName);

  IdlInterface*   GetInterfaceAt(long aIndex);
  long            InterfaceCount();

};

#ifdef XP_MAC
#include <ostream.h>			// required for namespace resolution
#else
class ostream;
#endif
ostream& operator<<(ostream &s, IdlSpecification &aSpecification);

#endif

