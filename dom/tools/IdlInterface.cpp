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

#include "IdlInterface.h"

#include "nsVoidArray.h"
#include "IdlEnum.h"
#include "IdlAttribute.h"
#include "IdlFunction.h"

#include <string.h>

IdlInterface::IdlInterface()
{
  mBaseClasses = (nsVoidArray*)0;
  mAttributes = (nsVoidArray*)0;
  mFunctions = (nsVoidArray*)0;
  mEnums = (nsVoidArray*)0;
  mStructs = (nsVoidArray*)0;
  mUnions = (nsVoidArray*)0;
  mConsts = (nsVoidArray*)0;
  mTypedefs = (nsVoidArray*)0;
  mExceptions = (nsVoidArray*)0;
}

IdlInterface::~IdlInterface()
{
  if (mBaseClasses) {
    for (int i = 0; i < mBaseClasses->Count(); i++) {
      char *baseClass = (char*)mBaseClasses->ElementAt(i);
      delete[] baseClass;
    }
  }
  if (mAttributes) {
    for (int i = 0; i < mAttributes->Count(); i++) {
      IdlAttribute *attrObj = (IdlAttribute*)mAttributes->ElementAt(i);
      delete attrObj;
    }
  }
  if (mFunctions) {
    for (int i = 0; i < mFunctions->Count(); i++) {
      IdlFunction *funcObj = (IdlFunction*)mFunctions->ElementAt(i);
      delete funcObj;
    }
  }
  if (mEnums) {
    for (int i = 0; i < mEnums->Count(); i++) {
      IdlEnum *enumObj = (IdlEnum*)mEnums->ElementAt(i);
      delete enumObj;
    }
  }
  if (mStructs) {
    for (int i = 0; i < mStructs->Count(); i++) {
      IdlStruct *structObj = (IdlStruct*)mStructs->ElementAt(i);
      delete structObj;
    }
  }
  if (mUnions) {
    for (int i = 0; i < mUnions->Count(); i++) {
      IdlUnion *unionObj = (IdlUnion*)mUnions->ElementAt(i);
      delete unionObj;
    }
  }
  if (mConsts) {
    for (int i = 0; i < mConsts->Count(); i++) {
      IdlConst *constObj = (IdlConst*)mConsts->ElementAt(i);
      delete constObj;
    }
  }
  if (mTypedefs) {
    for (int i = 0; i < mTypedefs->Count(); i++) {
      IdlTypedef *typedefObj = (IdlTypedef*)mTypedefs->ElementAt(i);
      delete typedefObj;
    }
  }
  if (mExceptions) {
    for (int i = 0; i < mExceptions->Count(); i++) {
      IdlException *exceptionObj = (IdlException*)mExceptions->ElementAt(i);
      delete exceptionObj;
    }
  }
}

void IdlInterface::InheritsFrom(char *aBase)
{
  if (aBase) {
    size_t length = strlen(aBase) + 1;
    char *baseName = new char[length];
    strcpy(baseName, aBase);
    if (!mBaseClasses) {
      mBaseClasses = new nsVoidArray();
    }
    mBaseClasses->AppendElement((void*)baseName);
  }
}

long IdlInterface::BaseClasseCount()
{
  if (mBaseClasses) {
    return mBaseClasses->Count();
  }
  return 0;
}

char* IdlInterface::GetBaseClassAt(long aIndex)
{
  char *base = (char*)0;

  if (mBaseClasses) {
    base = (char*)mBaseClasses->ElementAt(aIndex);
  }

  return base;
}

void IdlInterface::AddTypedef(IdlTypedef *aTypedef)
{
  if (aTypedef) {
    if (!mTypedefs) {
      mTypedefs = new nsVoidArray();
    }
    mTypedefs->AppendElement((void*)aTypedef);
  }
}

long IdlInterface::TypedefCount()
{
  if (mTypedefs) {
    return mTypedefs->Count();
  }
  return 0;
}

IdlTypedef* IdlInterface::GetTypedefAt(long aIndex)
{
  IdlTypedef *typedefObj = (IdlTypedef*)0;
  if (mTypedefs) {
    typedefObj = (IdlTypedef*)mTypedefs->ElementAt(aIndex);
  }
  return typedefObj;
}

void IdlInterface::AddStruct(IdlStruct *aStruct)
{
  if (aStruct) {
    if (!mStructs) {
      mStructs = new nsVoidArray();
    }
    mStructs->AppendElement((void*)aStruct);
  }
}

long IdlInterface::StructCount()
{
  if (mStructs) {
    return mStructs->Count();
  }
  return 0;
}

IdlStruct* IdlInterface::GetStructAt(long aIndex)
{
  IdlStruct *structObj = (IdlStruct*)0;
  if (mStructs) {
    structObj = (IdlStruct*)mStructs->ElementAt(aIndex);
  }
  return structObj;
}

void IdlInterface::AddEnum(IdlEnum *aEnum)
{
  if (aEnum) {
    if (!mEnums) {
      mEnums = new nsVoidArray();
    }
    mEnums->AppendElement((void*)aEnum);
  }
}

long IdlInterface::EnumCount()
{
  if (mEnums) {
    return mEnums->Count();
  }
  return 0;
}

IdlEnum* IdlInterface::GetEnumAt(long aIndex)
{
  IdlEnum *enumObj = (IdlEnum*)0;
  if (mEnums) {
    enumObj = (IdlEnum*)mEnums->ElementAt(aIndex);
  }
  return enumObj;
}

void IdlInterface::AddUnion(IdlUnion *aUnion)
{
  if (aUnion) {
    if (!mUnions) {
      mUnions = new nsVoidArray();
    }
    mUnions->AppendElement((void*)aUnion);
  }
}

long IdlInterface::UnionCount()
{
  if (mUnions) {
    return mUnions->Count();
  }
  return 0;
}

IdlUnion* IdlInterface::GetUnionAt(long aIndex)
{
  IdlUnion *unionObj = (IdlUnion*)0;
  if (mUnions) {
    unionObj = (IdlUnion*)mUnions->ElementAt(aIndex);
  }
  return unionObj;
}

void IdlInterface::AddConst(IdlConst *aConst)
{
  if (aConst) {
    if (!mConsts) {
      mConsts = new nsVoidArray();
    }
    mConsts->AppendElement((void*)aConst);
  }
}

long IdlInterface::ConstCount()
{
  if (mConsts) {
    return mConsts->Count();
  }
  return 0;
}

IdlConst* IdlInterface::GetConstAt(long aIndex)
{
  IdlConst *constObj = (IdlConst*)0;
  if (mConsts) {
    constObj = (IdlConst*)mConsts->ElementAt(aIndex);
  }
  return constObj;
}

void IdlInterface::AddException(IdlException *aException)
{
  if (aException) {
    if (!mExceptions) {
      mExceptions = new nsVoidArray();
    }
    mExceptions->AppendElement((void*)aException);
  }
}

long IdlInterface::ExceptionCount()
{
  if (mExceptions) {
    return mExceptions->Count();
  }
  return 0;
}

IdlException* IdlInterface::GetExceptionAt(long aIndex)
{
  IdlException *excObj = (IdlException*)0;
  if (mExceptions) {
    excObj = (IdlException*)mExceptions->ElementAt(aIndex);
  }
  return excObj;
}

void IdlInterface::AddAttribute(IdlAttribute *aAttribute)
{
  if (aAttribute) {
    if (!mAttributes) {
      mAttributes = new nsVoidArray();
    }
    mAttributes->AppendElement((void*)aAttribute);
  }
}

long IdlInterface::AttributeCount()
{
  if (mAttributes) {
    return mAttributes->Count();
  }
  return 0;
}

IdlAttribute* IdlInterface::GetAttributeAt(long aIndex)
{
  IdlAttribute *attrObj = (IdlAttribute*)0;
  if (mAttributes) {
    attrObj = (IdlAttribute*)mAttributes->ElementAt(aIndex);
  }
  return attrObj;
}

void IdlInterface::AddFunction(IdlFunction *aFunction)
{
  if (aFunction) {
    if (!mFunctions) {
      mFunctions = new nsVoidArray();
    }
    mFunctions->AppendElement((void*)aFunction);
  }
}

long IdlInterface::FunctionCount()
{
  if (mFunctions) {
    return mFunctions->Count();
  }
  return 0;
}

IdlFunction* IdlInterface::GetFunctionAt(long aIndex)
{
  IdlFunction *funcObj = (IdlFunction*)0;
  if (mFunctions) {
    funcObj = (IdlFunction*)mFunctions->ElementAt(aIndex);
  }
  return funcObj;
}



