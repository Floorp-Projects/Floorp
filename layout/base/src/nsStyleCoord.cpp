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

#include "nsStyleCoord.h"
#include "nsString.h"
#include "nsCRT.h"

nsStyleCoord::nsStyleCoord(void)
  : mUnit(eStyleUnit_Twips)
{
  mValue.mInt = 0;
}

nsStyleCoord::nsStyleCoord(nscoord aValue)
  : mUnit(eStyleUnit_Twips)
{
  mValue.mInt = aValue;
}

nsStyleCoord::nsStyleCoord(PRInt32 aValue, nsStyleUnit aUnit)
  : mUnit(aUnit)
{
  mValue.mInt = aValue;
}

nsStyleCoord::nsStyleCoord(float aValue)
  : mUnit(eStyleUnit_Percent)
{
  mValue.mFloat = aValue;
}

nsStyleCoord::nsStyleCoord(const nsStyleCoord& aCopy)
  : mUnit(aCopy.mUnit)
{
  if (eStyleUnit_Percent == mUnit) {
    mValue.mFloat = aCopy.mValue.mFloat;
  }
  else {
    mValue.mInt = aCopy.mValue.mInt;
  }
}

nsStyleCoord::~nsStyleCoord(void)
{
}

nsStyleCoord& nsStyleCoord::operator=(const nsStyleCoord& aCopy)
{
  mUnit = aCopy.mUnit;
  if (eStyleUnit_Percent == mUnit) {
    mValue.mFloat = aCopy.mValue.mFloat;
  }
  else {
    mValue.mInt = aCopy.mValue.mInt;
  }
  return *this;
}

PRBool nsStyleCoord::operator==(const nsStyleCoord& aOther) const
{
  if (mUnit == aOther.mUnit) {
    if (eStyleUnit_Percent == mUnit) {
      return PRBool(mValue.mFloat == aOther.mValue.mFloat);
    }
    else {
      return PRBool(mValue.mInt == aOther.mValue.mInt);
    }
  }
  return PR_FALSE;
}

void nsStyleCoord::Set(nscoord aValue)
{
  mUnit = eStyleUnit_Twips;
  mValue.mInt = aValue;
}

void nsStyleCoord::Set(PRInt32 aValue, nsStyleUnit aUnit)
{
  mUnit = aUnit;
  mValue.mInt = aValue;
}

void nsStyleCoord::Set(float aValue)
{
  mUnit = eStyleUnit_Percent;
  mValue.mFloat = aValue;
}

void nsStyleCoord::SetAuto(void)
{
  mUnit = eStyleUnit_Auto;
  mValue.mInt = 0;
}

void nsStyleCoord::SetInherit(void)
{
  mUnit = eStyleUnit_Inherit;
  mValue.mInt = 0;
}

void nsStyleCoord::AppendToString(nsString& aBuffer) const
{
  if (eStyleUnit_Percent == mUnit) {
    aBuffer.Append(mValue.mFloat);
  }
  else if ((eStyleUnit_Twips == mUnit) || 
           (eStyleUnit_Proportional == mUnit) ||
           (eStyleUnit_Enumerated == mUnit)) {
    aBuffer.Append(mValue.mInt, 10);
    aBuffer.Append("[0x");
    aBuffer.Append(mValue.mInt, 16);
    aBuffer.Append(']');
  }

  switch (mUnit) {
    case eStyleUnit_Twips:        aBuffer.Append("tw");   break;
    case eStyleUnit_Percent:      aBuffer.Append("%");    break;
    case eStyleUnit_Auto:         aBuffer.Append("Auto"); break;
    case eStyleUnit_Inherit:      aBuffer.Append("Inherit"); break;
    case eStyleUnit_Proportional: aBuffer.Append("*");    break;
    case eStyleUnit_Enumerated:   aBuffer.Append("enum"); break;
  }
  aBuffer.Append(' ');
}

void nsStyleCoord::ToString(nsString& aBuffer) const
{
  aBuffer.Truncate();
  AppendToString(aBuffer);
}

