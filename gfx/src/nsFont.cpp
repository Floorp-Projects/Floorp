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

#include "nsFont.h"
#include "nsString.h"

nsFont::nsFont(const char* aName, PRUint8 aStyle, PRUint8 aVariant,
               PRUint16 aWeight, PRUint8 aDecoration, nscoord aSize)
  : name(aName)
{
  style = aStyle;
  variant = aVariant;
  weight = aWeight;
  decorations = aDecoration;
  size = aSize;
}

nsFont::nsFont(const nsString& aName, PRUint8 aStyle, PRUint8 aVariant,
               PRUint16 aWeight, PRUint8 aDecoration, nscoord aSize)
  : name(aName)
{
  style = aStyle;
  variant = aVariant;
  weight = aWeight;
  decorations = aDecoration;
  size = aSize;
}

nsFont::nsFont(const nsFont& aOther)
  : name(aOther.name)
{
  style = aOther.style;
  variant = aOther.variant;
  weight = aOther.weight;
  decorations = aOther.decorations;
  size = aOther.size;
}

nsFont::~nsFont()
{
}

PRBool nsFont::Equals(const nsFont& aOther) const
{
  if ((style == aOther.style) &&
      (variant == aOther.variant) &&
      (weight == aOther.weight) &&
      (decorations == aOther.decorations) &&
      (size == aOther.size) &&
      name.EqualsIgnoreCase(aOther.name)) {
    return PR_TRUE;
  }
  return PR_FALSE;
}

nsFont& nsFont::operator=(const nsFont& aOther)
{
  name = aOther.name;
  style = aOther.style;
  variant = aOther.variant;
  weight = aOther.weight;
  decorations = aOther.decorations;
  size = aOther.size;
  return *this;
}
