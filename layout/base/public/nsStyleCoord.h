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
#ifndef nsStyleCoord_h___
#define nsStyleCoord_h___

#include "nscore.h"
#include "nsCoord.h"
class nsString;

enum nsStyleUnit {
  eStyleUnit_Twips        = 0,      // (nscoord) value is twips
  eStyleUnit_Percent      = 1,      // (float) 1.0 == 100%
  eStyleUnit_Auto         = 2,      // (no value)
  eStyleUnit_Inherit      = 3,      // (no value) value should be inherited
  eStyleUnit_Proportional = 4,      // (int) value has proportional meaning
  eStyleUnit_Enumerated   = 5,      // (int) value has enumerated meaning
};

class nsStyleCoord {
public:
  nsStyleCoord(void); // sets to 0 twips
  nsStyleCoord(nscoord aValue);
  nsStyleCoord(PRInt32 aValue, nsStyleUnit aUnit);
  nsStyleCoord(float aValue);
  nsStyleCoord(const nsStyleCoord& aCopy);
  ~nsStyleCoord(void);

  nsStyleCoord&  operator=(const nsStyleCoord& aCopy);
  PRBool         operator==(const nsStyleCoord& aOther) const;

  nsStyleUnit GetUnit(void) const { return mUnit; }
  nscoord     GetCoordValue(void) const;
  PRInt32     GetIntValue(void) const;
  float       GetFloatValue(void) const;

  void  Set(nscoord aValue);
  void  Set(PRInt32 aValue, nsStyleUnit aUnit);
  void  Set(float aValue);
  void  SetAuto(void);
  void  SetInherit(void);

  void  AppendToString(nsString& aBuffer) const;
  void  ToString(nsString& aBuffer) const;

protected:
  nsStyleUnit mUnit;
  union {
    PRInt32     mInt;   // nscoord is a PRInt32 for now
    float       mFloat;
  }           mValue;
};

inline PRInt32 nsStyleCoord::GetCoordValue(void) const
{
  NS_ASSERTION((mUnit == eStyleUnit_Twips), "not a coord value");
  if (mUnit == eStyleUnit_Twips) {
    return mValue.mInt;
  }
  return 0;
}

inline PRInt32 nsStyleCoord::GetIntValue(void) const
{
  NS_ASSERTION((mUnit == eStyleUnit_Proportional) ||
               (mUnit == eStyleUnit_Enumerated), "not an int value");
  if ((mUnit == eStyleUnit_Proportional) ||
      (mUnit == eStyleUnit_Enumerated)) {
    return mValue.mInt;
  }
  return 0;
}

inline float nsStyleCoord::GetFloatValue(void) const
{
  NS_ASSERTION(mUnit == eStyleUnit_Percent, "not a float value");
  if (mUnit == eStyleUnit_Percent) {
    return mValue.mFloat;
  }
  return 0.0f;
}



#endif /* nsStyleCoord_h___ */
