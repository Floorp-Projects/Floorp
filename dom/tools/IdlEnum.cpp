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

#include "IdlEnum.h"
#include "nsVoidArray.h"
#include "IdlVariable.h"
#include <ostream.h>

ostream& operator<<(ostream &s, IdlEnum &aEnum)
{
  s << "enum { \n";

  long count = aEnum.EnumeratorCount();
  if (count) {
    for (int i = 0; i < count - 1; i++) {
      IdlVariable *enumerator = aEnum.GetEnumeratorAt(i);
      s << "  " << enumerator->GetName();
      if (TYPE_INT == enumerator->GetType()) {
        s << " = " << enumerator->GetLongValue();
      }
      s << ",\n";
    }

    IdlVariable *enumerator = aEnum.GetEnumeratorAt(i);
    s << "  " << enumerator->GetName();
    if (TYPE_INT == enumerator->GetType()) {
      s << " = " << enumerator->GetLongValue();
    }
    s << "\n";
  }

  return s << "}; \n";
}

IdlEnum::IdlEnum()
{
  mEnumerators = (nsVoidArray*)0;
}

IdlEnum::~IdlEnum()
{
  if (mEnumerators) {
    for (int i = 0; i < mEnumerators->Count(); i++) {
      IdlVariable *varObj = (IdlVariable*)mEnumerators->ElementAt(i);
      delete varObj;
    }
  }
}

void IdlEnum::AddEnumerator(IdlVariable *aEnumerator)
{
  if (aEnumerator) {
    if (!mEnumerators) {
      mEnumerators = new nsVoidArray();
    }
    mEnumerators->AppendElement((void*)aEnumerator);
  }
}

long IdlEnum::EnumeratorCount()
{
  if (mEnumerators) {
    return mEnumerators->Count();
  }
  return 0;
}

IdlVariable* IdlEnum::GetEnumeratorAt(long aIndex)
{
  IdlVariable *varObj = (IdlVariable*)0;
  if (mEnumerators) {
    varObj = (IdlVariable*)mEnumerators->ElementAt(aIndex);
  }
  return varObj;
}

