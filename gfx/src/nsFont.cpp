/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsFont.h"
#include "nsString.h"

nsFont::nsFont(const char* aName, PRUint8 aStyle, PRUint8 aVariant,
               PRUint16 aWeight, PRUint8 aDecoration, nscoord aSize,
               float aSizeAdjust)
{
  name.AssignWithConversion(aName);
  style = aStyle;
  variant = aVariant;
  weight = aWeight;
  decorations = aDecoration;
  size = aSize;
  sizeAdjust = aSizeAdjust;
}

nsFont::nsFont(const nsString& aName, PRUint8 aStyle, PRUint8 aVariant,
               PRUint16 aWeight, PRUint8 aDecoration, nscoord aSize,
               float aSizeAdjust)
  : name(aName)
{
  style = aStyle;
  variant = aVariant;
  weight = aWeight;
  decorations = aDecoration;
  size = aSize;
  sizeAdjust = aSizeAdjust;
}

nsFont::nsFont(const nsFont& aOther)
  : name(aOther.name)
{
  style = aOther.style;
  variant = aOther.variant;
  weight = aOther.weight;
  decorations = aOther.decorations;
  size = aOther.size;
  sizeAdjust = aOther.sizeAdjust;
}

nsFont::nsFont()
{
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
      (sizeAdjust == aOther.sizeAdjust) &&
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
  sizeAdjust = aOther.sizeAdjust;
  return *this;
}

static PRBool IsGenericFontFamily(const nsString& aFamily)
{
  PRUint8 generic;
  nsFont::GetGenericID(aFamily, &generic);
  return generic != kGenericFont_NONE;
}

const PRUnichar kNullCh       = PRUnichar('\0');
const PRUnichar kSingleQuote  = PRUnichar('\'');
const PRUnichar kDoubleQuote  = PRUnichar('\"');
const PRUnichar kComma        = PRUnichar(',');

PRBool nsFont::EnumerateFamilies(nsFontFamilyEnumFunc aFunc, void* aData) const
{
  PRBool    running = PR_TRUE;

  nsAutoString  familyList; familyList.Assign(name); // copy to work buffer
  nsAutoString  familyStr;

  familyList.Append(kNullCh);  // put an extra null at the end

  // XXX This code is evil...
  PRUnichar* start = (PRUnichar*)(const PRUnichar*)familyList.get();
  PRUnichar* end   = start;

  while (running && (kNullCh != *start)) {
    PRBool  quoted = PR_FALSE;
    PRBool  generic = PR_FALSE;

    while ((kNullCh != *start) && nsCRT::IsAsciiSpace(*start)) {  // skip leading space
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

/*static*/
void nsFont::GetGenericID(const nsString& aGeneric, PRUint8* aID)
{
  *aID = kGenericFont_NONE;
  if (aGeneric.EqualsIgnoreCase("-moz-fixed"))      *aID = kGenericFont_moz_fixed;
  else if (aGeneric.EqualsIgnoreCase("serif"))      *aID = kGenericFont_serif;
  else if (aGeneric.EqualsIgnoreCase("sans-serif")) *aID = kGenericFont_sans_serif;
  else if (aGeneric.EqualsIgnoreCase("cursive"))    *aID = kGenericFont_cursive;
  else if (aGeneric.EqualsIgnoreCase("fantasy"))    *aID = kGenericFont_fantasy;
  else if (aGeneric.EqualsIgnoreCase("monospace"))  *aID = kGenericFont_monospace;
}
