/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
};

class ostream;
ostream& operator<<(ostream &s, IdlFunction &aFunction);

#endif

