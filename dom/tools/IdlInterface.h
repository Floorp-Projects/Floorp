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
class IdlVariable;
class IdlException;

class IdlInterface : public IdlObject {
private:
  nsVoidArray *mIIDs;
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
  
  char*           GetIIDAt(long aIndex);
  long            IIDCount();
  void            AddIID(char *aIID);
  void            InheritsFrom(char *aBase);
  long            BaseClassCount();
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
  void            AddConst(IdlVariable *aConst);
  long            ConstCount();
  IdlVariable*    GetConstAt(long aIndex);
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

#ifdef XP_MAC
#include <ostream.h>			// required for namespace resolution
#else
class ostream;
#endif
ostream& operator<<(ostream &s, IdlInterface &aInterface);

#endif

