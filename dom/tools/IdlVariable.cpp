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

#include "IdlVariable.h"

#include <string.h>

IdlVariable::IdlVariable()
{
  mType = (Type)0;
  mTypeName = 0;
  memset(&mValue, 0, sizeof(mValue));
}

IdlVariable::~IdlVariable()
{
  if (mTypeName) {
    delete[] mTypeName;
  }

  if (TYPE_STRING == mType) {
    delete[] mValue.vString;
  }
}

void IdlVariable::SetType(Type aType)
{
  mType = aType;
}

Type IdlVariable::GetType()
{
  return mType;
}

void IdlVariable::SetTypeName(char *aTypeName)
{
  if (mTypeName) {
    delete[] mTypeName;
    mTypeName = 0;
  }

  if (aTypeName) {
    size_t length = strlen(aTypeName) + 1;
    mTypeName = new char[length];
    strcpy(mTypeName, aTypeName);
  }
}

char* IdlVariable::GetTypeName()
{
  return mTypeName;
}

void IdlVariable::GetTypeAsString(char *aString, size_t aStringSize)
{
  switch(mType) {
    case TYPE_BOOLEAN:
      if (aStringSize > 7) {
        strcpy(aString, "boolean");
      }
      break;
    case TYPE_FLOAT:
      if (aStringSize > 5) {
        strcpy(aString, "float");
      }
      break;
    case TYPE_DOUBLE:
      if (aStringSize > 6) {
        strcpy(aString, "double");
      }
      break;
    case TYPE_LONG:
      if (aStringSize > 4) {
        strcpy(aString, "long");
      }
      break;
    case TYPE_SHORT:
      if (aStringSize > 5) {
        strcpy(aString, "short");
      }
      break;
    case TYPE_ULONG:
      if (aStringSize > 13) {
        strcpy(aString, "unsigned long");
      }
      break;
    case TYPE_USHORT:
      if (aStringSize > 14) {
        strcpy(aString, "unsigned short");
      }
      break;
    case TYPE_CHAR:
      if (aStringSize > 4) {
        strcpy(aString, "char");
      }
      break;
    case TYPE_INT:
      if (aStringSize > 3) {
        strcpy(aString, "int");
      }
      break;
    case TYPE_UINT:
      if (aStringSize > 12) {
        strcpy(aString, "unsigned int");
      }
      break;
    case TYPE_STRING:
      if (aStringSize > 6) {
        strcpy(aString, "wstring");
      }
      break;
    case TYPE_OBJECT:
      if (aStringSize > strlen(mTypeName)) {
        strcpy(aString, mTypeName);
      }
      break;
    case TYPE_VOID:
      if (aStringSize > 4) {
        strcpy(aString, "void");
      }
      break;
  }
}

void IdlVariable::SetValue(unsigned long aValue)
{
  DeleteStringType();
  mValue.vLong = aValue;
}

void IdlVariable::SetValue(char aValue)
{
  DeleteStringType();
  mValue.vChar = aValue;
}

void IdlVariable::SetValue(char *aValue)
{
  DeleteStringType();
  size_t length = strlen(aValue) + 1;
  mValue.vString = new char[length];
  strcpy(mValue.vString, aValue);
}

void IdlVariable::SetValue(double aValue)
{
  DeleteStringType();
  mValue.vDouble = aValue;
}

void IdlVariable::SetValue(void *aValue)
{
  DeleteStringType();
  mValue.vObject = aValue;
}


unsigned long IdlVariable::GetLongValue()
{
  return mValue.vLong;
}

char IdlVariable::GetCharValue()
{
  return mValue.vChar;
}

char* IdlVariable::GetStringValue()
{
  return mValue.vString;
}

double IdlVariable::GetDoubleValue()
{
  return mValue.vDouble;
}

void* IdlVariable::GetObjectValue()
{
  return mValue.vObject;
}

void IdlVariable::DeleteStringType()
{
  if (TYPE_STRING == mType) {
    if (mValue.vString) {
      delete[] mValue.vString;
    }
  }
}

