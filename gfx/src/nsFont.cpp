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


static PRBool IsGenericFontFamily(const nsString& aFamily)
{
  return PRBool(aFamily.EqualsIgnoreCase("serif") ||
                aFamily.EqualsIgnoreCase("sans-serif") ||
                aFamily.EqualsIgnoreCase("cursive") ||
                aFamily.EqualsIgnoreCase("fantasy") ||
                aFamily.EqualsIgnoreCase("monospace"));
}

const PRUnichar kNullCh       = PRUnichar('\0');
const PRUnichar kSingleQuote  = PRUnichar('\'');
const PRUnichar kDoubleQuote  = PRUnichar('\"');
const PRUnichar kComma        = PRUnichar(',');

PRBool nsFont::EnumerateFamilies(nsFontFamilyEnumFunc aFunc, void* aData) const
{
  PRBool    running = PR_TRUE;

  nsAutoString  familyList(name); // copy to work buffer
  nsAutoString  familyStr;

  familyList.Append(kNullCh);  // put an extra null at the end

  // XXX This code is evil...
  PRUnichar* start = (PRUnichar*)(const PRUnichar*)familyList.GetUnicode();
  PRUnichar* end   = start;

  while (running && (kNullCh != *start)) {
    PRBool  quoted = PR_FALSE;
    PRBool  generic = PR_FALSE;

    while ((kNullCh != *start) && nsString::IsSpace(*start)) {  // skip leading space
      start++;
    }

    if ((kSingleQuote == *start) || (kDoubleQuote == *start)) { // quoted string
      PRUnichar quote = *start++;
      quoted = PR_TRUE;
      end = start;
      while (kNullCh != *end) {
        if (quote == *end) {  // found closing quote
          *end++ = kNullCh;     // end string here
          while ((kNullCh != *end) && (kComma != *end)) { // keep going until comma
            end++;
          }
          break;
        }
        end++;
      }
    }
    else {  // non-quoted string or ended
      end = start;

      while ((kNullCh != *end) && (kComma != *end)) { // look for comma
        end++;
      }
      *end = kNullCh; // end string here
    }

    familyStr = start;

    if (PR_FALSE == quoted) {
      familyStr.CompressWhitespace(PR_FALSE, PR_TRUE);
      if (0 < familyStr.Length()) {
        generic = IsGenericFontFamily(familyStr);
      }
    }

    if (0 < familyStr.Length()) {
      running = (*aFunc)(familyStr, generic, aData);
    }

    start = ++end;
  }

  return running;
}

static PRBool FontEnumCallback(const nsString& aFamily, PRBool aGeneric, void *aData)
{
  *((nsString*)aData) = aFamily;
  return PR_FALSE;
}

void nsFont::GetFirstFamily(nsString& aFamily) const
{
  EnumerateFamilies(FontEnumCallback, &aFamily);
}

