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

#include "IdlFunction.h"

#include "IdlObject.h"
#include "nsVoidArray.h"
#include "IdlParameter.h"


IdlFunction::IdlFunction() 
{
  mReturnValue = (IdlVariable*)0;
  mParameters = (nsVoidArray*)0;
  mExceptions = (nsVoidArray*)0;
}

IdlFunction::~IdlFunction()
{
  if (mReturnValue) {
    delete mReturnValue;
  }
  if (mParameters) {
    for (int i = 0; i < mParameters->Count(); i++) {
      IdlParameter *paramObj = (IdlParameter*)mParameters->ElementAt(i);
      delete paramObj;
    }
  }
  if (mExceptions) {
    for (int i = 0; i < mExceptions->Count(); i++) {
      IdlException *excObj = (IdlException*)mExceptions->ElementAt(i);
      delete excObj;
    }
  }
}

void IdlFunction::SetReturnValue(Type aType, char *aTypeName)
{
  if (mReturnValue) {
    delete mReturnValue;
  }

  mReturnValue = new IdlVariable();
  mReturnValue->SetType(aType);
  if (aTypeName) {
    mReturnValue->SetTypeName(aTypeName);
  }
}

IdlVariable* IdlFunction::GetReturnValue()
{
  return mReturnValue;
}

void IdlFunction::AddParameter(IdlParameter *aParameter)
{
  if (aParameter) {
    if (!mParameters) {
      mParameters = new nsVoidArray();
    }
    mParameters->AppendElement((void*)aParameter);
  }
}

long IdlFunction::ParameterCount()
{
  if (mParameters) {
    return mParameters->Count();
  }
  return 0;
}

IdlParameter* IdlFunction::GetParameterAt(long aIndex)
{
  IdlParameter *paramObj = (IdlParameter*)0;
  if (mParameters) {
    paramObj = (IdlParameter*)mParameters->ElementAt(aIndex);
  }
  return paramObj;
}

void IdlFunction::AddException(char *aException)
{
  if (aException) {
    if (!mExceptions) {
      mExceptions = new nsVoidArray();
    }
    mExceptions->AppendElement((void*)aException);
  }
}

long IdlFunction::ExceptionCount()
{
  if (mExceptions) {
    return mExceptions->Count();
  }
  return 0;
}

char* IdlFunction::GetExceptionAt(long aIndex)
{
  char *excName = (char*)0;
  if (mExceptions) {
    excName = (char*)mExceptions->ElementAt(aIndex);
  }
  return excName;
}


