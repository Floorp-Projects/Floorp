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

#ifndef _IdlFunction_h__
#define _IdlFunction_h__

#include "IdlObject.h"
#include "IdlVariable.h"

class nsVoidArray;
class IdlParameter;

class IdlFunction : public IdlObject {

private:
  IdlVariable *mReturnValue;
  nsVoidArray *mParameters;
  nsVoidArray *mExceptions;
  int         mHasEllipsis;
  int         mIsNoScript;

public:
                  IdlFunction();
                  ~IdlFunction();

  void            SetReturnValue(Type aType, char *aTypeName = (char*)0);
  IdlVariable*    GetReturnValue();

  void            AddParameter(IdlParameter *aParameter);
  long            ParameterCount();
  IdlParameter*   GetParameterAt(long aIndex);

  void            AddException(char *aException);
  long            ExceptionCount();
  char*           GetExceptionAt(long aIndex);

  int             GetHasEllipsis();
  void            SetHasEllipsis(int aHasEllipsis);

  int             GetIsNoScript();
  void            SetIsNoScript(int aIsNoScript);
};

#ifdef XP_MAC
#include <ostream.h>			// required for namespace resolution
#else
class ostream;
#endif
ostream& operator<<(ostream &s, IdlFunction &aFunction);

#endif

