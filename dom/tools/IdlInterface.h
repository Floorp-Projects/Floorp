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

#ifndef _IdlInterface_h__
#define _IdlInterface_h__

#include "IdlObject.h"

class nsVoidArray;
class IdlTypedef;
class IdlStruct;
class IdlUnion;
class IdlEnum;
class IdlAttribute;
class IdlFunction;
class IdlConst;
class IdlException;

class IdlInterface : public IdlObject {
private:
  nsVoidArray *mBaseClasses;
  nsVoidArray *mAttributes;
  nsVoidArray *mFunctions;
  nsVoidArray *mEnums;
  nsVoidArray *mStructs;
  nsVoidArray *mUnions;
  nsVoidArray *mConsts;
  nsVoidArray *mTypedefs;
  nsVoidArray *mExceptions;

public:
                  IdlInterface();
                  ~IdlInterface();

  void            InheritsFrom(char *aBase);
  long            BaseClasseCount();
  char*           GetBaseClassAt(long aIndex);
  void            AddTypedef(IdlTypedef *aTypedef);
  long            TypedefCount();
  IdlTypedef*     GetTypedefAt(long aIndex);
  void            AddStruct(IdlStruct *aStruct);
  long            StructCount();
  IdlStruct*      GetStructAt(long aIndex);
  void            AddEnum(IdlEnum *aEnum);
  long            EnumCount();
  IdlEnum*        GetEnumAt(long aIndex);
  void            AddUnion(IdlUnion *aUnion);
  long            UnionCount();
  IdlUnion*       GetUnionAt(long aIndex);
  void            AddConst(IdlConst *aConst);
  long            ConstCount();
  IdlConst*       GetConstAt(long aIndex);
  void            AddException(IdlException *aException);
  long            ExceptionCount();
  IdlException*   GetExceptionAt(long aIndex);
  void            AddAttribute(IdlAttribute *aAttribute);
  long            AttributeCount();
  IdlAttribute*   GetAttributeAt(long aIndex);
  void            AddFunction(IdlFunction *aFunction);
  long            FunctionCount();
  IdlFunction*    GetFunctionAt(long aIndex);

};

#endif

