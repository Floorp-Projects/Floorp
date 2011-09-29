/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla MathML Project.
 *
 * The Initial Developer of the Original Code is
 * The University Of Queensland.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 *   Karl Tomlinson <karlt+@karlt.net>, Mozilla Corporation
 *   Frederic Wang <fred.wang@free.fr>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsHashtable.h"
#include "nsTArray.h"

#include "nsIComponentManager.h"
#include "nsIPersistentProperties2.h"
#include "nsNetUtil.h"
#include "nsCRT.h"

#include "nsMathMLOperators.h"

// operator dictionary entry
struct OperatorData {
  OperatorData(void)
    : mFlags(0),
      mLeftSpace(0.0f),
      mRightSpace(0.0f)
  {
  }

  // member data
  nsString        mStr;
  nsOperatorFlags mFlags;
  float           mLeftSpace;   // unit is em
  float           mRightSpace;  // unit is em
};

static PRInt32         gTableRefCount = 0;
static PRUint32        gOperatorCount = 0;
static OperatorData*   gOperatorArray = nsnull;
static nsHashtable*    gOperatorTable = nsnull;
static bool            gInitialized   = false;
static nsTArray<nsString>*      gInvariantCharArray    = nsnull;

static const PRUnichar kNullCh  = PRUnichar('\0');
static const PRUnichar kDashCh  = PRUnichar('#');
static const PRUnichar kColonCh = PRUnichar(':');

static const char* const kMathVariant_name[] = {
  "normal",
  "bold",
  "italic",
  "bold-italic",
  "sans-serif",
  "bold-sans-serif",
  "sans-serif-italic",
  "sans-serif-bold-italic",
  "monospace",
  "script",
  "bold-script",
  "fraktur",
  "bold-fraktur",
  "double-struck"
};

static void
SetBooleanProperty(OperatorData* aOperatorData,
                   nsString      aName)
{
  if (aName.IsEmpty())
    return;

  if (aName.EqualsLiteral("stretchy") && (1 == aOperatorData->mStr.Length()))
    aOperatorData->mFlags |= NS_MATHML_OPERATOR_STRETCHY;
  else if (aName.EqualsLiteral("fence"))
    aOperatorData->mFlags |= NS_MATHML_OPERATOR_FENCE;
  else if (aName.EqualsLiteral("accent"))
    aOperatorData->mFlags |= NS_MATHML_OPERATOR_ACCENT;
  else if (aName.EqualsLiteral("largeop"))
    aOperatorData->mFlags |= NS_MATHML_OPERATOR_LARGEOP;
  else if (aName.EqualsLiteral("separator"))
    aOperatorData->mFlags |=  NS_MATHML_OPERATOR_SEPARATOR;
  else if (aName.EqualsLiteral("movablelimits"))
    aOperatorData->mFlags |= NS_MATHML_OPERATOR_MOVABLELIMITS;
  else if (aName.EqualsLiteral("symmetric"))
    aOperatorData->mFlags |= NS_MATHML_OPERATOR_SYMMETRIC;
  else if (aName.EqualsLiteral("integral"))
    aOperatorData->mFlags |= NS_MATHML_OPERATOR_INTEGRAL;
}

static void
SetProperty(OperatorData* aOperatorData,
            nsString      aName,
            nsString      aValue)
{
  if (aName.IsEmpty() || aValue.IsEmpty())
    return;

  // XXX These ones are not kept in the dictionary
  // Support for these requires nsString member variables 
  // maxsize (default: infinity)
  // minsize (default: 1)

  if (aName.EqualsLiteral("direction")) {
    if (aValue.EqualsLiteral("vertical"))
      aOperatorData->mFlags |= NS_MATHML_OPERATOR_DIRECTION_VERTICAL;
    else if (aValue.EqualsLiteral("horizontal"))
      aOperatorData->mFlags |= NS_MATHML_OPERATOR_DIRECTION_HORIZONTAL;
    else return; // invalid value
  } else {
    bool isLeftSpace;
    if (aName.EqualsLiteral("lspace"))
      isLeftSpace = PR_TRUE;
    else if (aName.EqualsLiteral("rspace"))
      isLeftSpace = PR_FALSE;
    else return;  // input is not applicable

    // aValue is assumed to be a digit from 0 to 7
    PRInt32 error = 0;
    float space = aValue.ToFloat(&error) / 18.0;
    if (error) return;

    if (isLeftSpace)
      aOperatorData->mLeftSpace = space;
    else
      aOperatorData->mRightSpace = space;
  }
}

static bool
SetOperator(OperatorData*   aOperatorData,
            nsOperatorFlags aForm,
            const nsCString& aOperator,
            nsString&        aAttributes)

{
  // aOperator is in the expanded format \uNNNN\uNNNN ...
  // First compress these Unicode points to the internal nsString format
  PRInt32 i = 0;
  nsAutoString name, value;
  PRInt32 len = aOperator.Length();
  PRUnichar c = aOperator[i++];
  PRUint32 state  = 0;
  PRUnichar uchar = 0;
  while (i <= len) {
    if (0 == state) {
      if (c != '\\')
        return PR_FALSE;
      if (i < len)
        c = aOperator[i];
      i++;
      if (('u' != c) && ('U' != c))
        return PR_FALSE;
      if (i < len)
        c = aOperator[i];
      i++;
      state++;
    }
    else {
      if (('0' <= c) && (c <= '9'))
         uchar = (uchar << 4) | (c - '0');
      else if (('a' <= c) && (c <= 'f'))
         uchar = (uchar << 4) | (c - 'a' + 0x0a);
      else if (('A' <= c) && (c <= 'F'))
         uchar = (uchar << 4) | (c - 'A' + 0x0a);
      else return PR_FALSE;
      if (i < len)
        c = aOperator[i];
      i++;
      state++;
      if (5 == state) {
        value.Append(uchar);
        uchar = 0;
        state = 0;
      }
    }
  }
  if (0 != state) return PR_FALSE;

  // Quick return when the caller doesn't care about the attributes and just wants
  // to know if this is a valid operator (this is the case at the first pass of the
  // parsing of the dictionary in InitOperators())
  if (!aForm) return PR_TRUE;

  // Add operator to hash table
  aOperatorData->mFlags |= aForm;
  aOperatorData->mStr.Assign(value);
  value.AppendInt(aForm, 10);
  nsStringKey key(value);
  gOperatorTable->Put(&key, aOperatorData);

#ifdef NS_DEBUG
  NS_LossyConvertUTF16toASCII str(aAttributes);
#endif
  // Loop over the space-delimited list of attributes to get the name:value pairs
  aAttributes.Append(kNullCh);  // put an extra null at the end
  PRUnichar* start = aAttributes.BeginWriting();
  PRUnichar* end   = start;
  while ((kNullCh != *start) && (kDashCh != *start)) {
    name.SetLength(0);
    value.SetLength(0);
    // skip leading space, the dash amounts to the end of the line
    while ((kNullCh!=*start) && (kDashCh!=*start) && nsCRT::IsAsciiSpace(*start)) {
      ++start;
    }
    end = start;
    // look for ':'
    while ((kNullCh!=*end) && (kDashCh!=*end) && !nsCRT::IsAsciiSpace(*end) &&
           (kColonCh!=*end)) {
      ++end;
    }
    // If ':' is not found, then it's a boolean property
    bool IsBooleanProperty = (kColonCh != *end);
    *end = kNullCh; // end segment here
    // this segment is the name
    if (start < end) {
      name.Assign(start);
    }
    if (IsBooleanProperty) {
      SetBooleanProperty(aOperatorData, name);
    } else {
      start = ++end;
      // look for space or end of line
      while ((kNullCh!=*end) && (kDashCh!=*end) &&
             !nsCRT::IsAsciiSpace(*end)) {
        ++end;
      }
      *end = kNullCh; // end segment here
      if (start < end) {
        // this segment is the value
        value.Assign(start);
      }
      SetProperty(aOperatorData, name, value);
    }
    start = ++end;
  }
  return PR_TRUE;
}

static nsresult
InitOperators(void)
{
  // Load the property file containing the Operator Dictionary
  nsresult rv;
  nsCOMPtr<nsIPersistentProperties> mathfontProp;
  rv = NS_LoadPersistentPropertiesFromURISpec(getter_AddRefs(mathfontProp),
       NS_LITERAL_CSTRING("resource://gre/res/fonts/mathfont.properties"));
  if (NS_FAILED(rv)) return rv;

  // Get the list of invariant chars
  for (PRInt32 i = 0; i < eMATHVARIANT_COUNT; ++i) {
    nsCAutoString key(NS_LITERAL_CSTRING("mathvariant."));
    key.Append(kMathVariant_name[i]);
    nsAutoString value;
    mathfontProp->GetStringProperty(key, value);
    gInvariantCharArray->AppendElement(value); // i.e., gInvariantCharArray[i] holds this list
  }

  // Parse the Operator Dictionary in two passes.
  // The first pass is to count the number of operators; the second pass is to
  // allocate the necessary space for them and to add them in the hash table.
  for (PRInt32 pass = 1; pass <= 2; pass++) {
    OperatorData dummyData;
    OperatorData* operatorData = &dummyData;
    nsCOMPtr<nsISimpleEnumerator> iterator;
    if (NS_SUCCEEDED(mathfontProp->Enumerate(getter_AddRefs(iterator)))) {
      bool more;
      PRUint32 index = 0;
      nsCAutoString name;
      nsAutoString attributes;
      while ((NS_SUCCEEDED(iterator->HasMoreElements(&more))) && more) {
        nsCOMPtr<nsIPropertyElement> element;
        if (NS_SUCCEEDED(iterator->GetNext(getter_AddRefs(element)))) {
          if (NS_SUCCEEDED(element->GetKey(name)) &&
              NS_SUCCEEDED(element->GetValue(attributes))) {
            // expected key: operator.\uNNNN.{infix,postfix,prefix}
            if ((21 <= name.Length()) && (0 == name.Find("operator.\\u"))) {
              name.Cut(0, 9); // 9 is the length of "operator.";
              PRInt32 len = name.Length();
              nsOperatorFlags form = 0;
              if (kNotFound != name.RFind(".infix")) {
                form = NS_MATHML_OPERATOR_FORM_INFIX;
                len -= 6;  // 6 is the length of ".infix";
              }
              else if (kNotFound != name.RFind(".postfix")) {
                form = NS_MATHML_OPERATOR_FORM_POSTFIX;
                len -= 8; // 8 is the length of ".postfix";
              }
              else if (kNotFound != name.RFind(".prefix")) {
                form = NS_MATHML_OPERATOR_FORM_PREFIX;
                len -= 7; // 7 is the length of ".prefix";
              }
              else continue; // input is not applicable
              name.SetLength(len);
              if (2 == pass) { // allocate space and start the storage
                if (!gOperatorArray) {
                  if (0 == gOperatorCount) return NS_ERROR_UNEXPECTED;
                  gOperatorArray = new OperatorData[gOperatorCount];
                  if (!gOperatorArray) return NS_ERROR_OUT_OF_MEMORY;
                }
                operatorData = &gOperatorArray[index];
              }
              else {
                form = 0; // to quickly return from SetOperator() at pass 1
              }
              // See if the operator should be retained
              if (SetOperator(operatorData, form, name, attributes)) {
                index++;
                if (1 == pass) gOperatorCount = index;
              }
            }
          }
        }
      }
    }
  }
  return NS_OK;
}

static nsresult
InitGlobals()
{
  gInitialized = PR_TRUE;
  nsresult rv = NS_ERROR_OUT_OF_MEMORY;
  gInvariantCharArray = new nsTArray<nsString>();
  if (gInvariantCharArray) {
    gOperatorTable = new nsHashtable();
    if (gOperatorTable) {
      rv = InitOperators();
    }
  }
  if (NS_FAILED(rv))
    nsMathMLOperators::CleanUp();
  return rv;
}

void
nsMathMLOperators::CleanUp()
{
  if (gInvariantCharArray) {
    delete gInvariantCharArray;
    gInvariantCharArray = nsnull;
  }
  if (gOperatorArray) {
    delete[] gOperatorArray;
    gOperatorArray = nsnull;
  }
  if (gOperatorTable) {
    delete gOperatorTable;
    gOperatorTable = nsnull;
  }
}

void
nsMathMLOperators::AddRefTable(void)
{
  gTableRefCount++;
}

void
nsMathMLOperators::ReleaseTable(void)
{
  if (0 == --gTableRefCount) {
    CleanUp();
  }
}

static OperatorData*
GetOperatorData(const nsString& aOperator, nsOperatorFlags aForm)
{
  nsAutoString key(aOperator);
  key.AppendInt(aForm);
  nsStringKey hkey(key);
  return (OperatorData*)gOperatorTable->Get(&hkey);
}

bool
nsMathMLOperators::LookupOperator(const nsString&       aOperator,
                                  const nsOperatorFlags aForm,
                                  nsOperatorFlags*      aFlags,
                                  float*                aLeftSpace,
                                  float*                aRightSpace)
{
  if (!gInitialized) {
    InitGlobals();
  }
  if (gOperatorTable) {
    NS_ASSERTION(aFlags && aLeftSpace && aRightSpace, "bad usage");
    NS_ASSERTION(aForm > 0 && aForm < 4, "*** invalid call ***");

    // The MathML REC says:
    // If the operator does not occur in the dictionary with the specified form,
    // the renderer should use one of the forms which is available there, in the
    // order of preference: infix, postfix, prefix.

    OperatorData* found;
    PRInt32 form = NS_MATHML_OPERATOR_GET_FORM(aForm);
    if (!(found = GetOperatorData(aOperator, form))) {
      if (form == NS_MATHML_OPERATOR_FORM_INFIX ||
          !(found =
            GetOperatorData(aOperator, NS_MATHML_OPERATOR_FORM_INFIX))) {
        if (form == NS_MATHML_OPERATOR_FORM_POSTFIX ||
            !(found =
              GetOperatorData(aOperator, NS_MATHML_OPERATOR_FORM_POSTFIX))) {
          if (form != NS_MATHML_OPERATOR_FORM_PREFIX) {
            found = GetOperatorData(aOperator, NS_MATHML_OPERATOR_FORM_PREFIX);
          }
        }
      }
    }
    if (found) {
      NS_ASSERTION(found->mStr.Equals(aOperator), "bad setup");
      *aLeftSpace = found->mLeftSpace;
      *aRightSpace = found->mRightSpace;
      *aFlags &= ~NS_MATHML_OPERATOR_FORM; // clear the form bits
      *aFlags |= found->mFlags; // just add bits without overwriting
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

void
nsMathMLOperators::LookupOperators(const nsString&       aOperator,
                                   nsOperatorFlags*      aFlags,
                                   float*                aLeftSpace,
                                   float*                aRightSpace)
{
  if (!gInitialized) {
    InitGlobals();
  }

  aFlags[NS_MATHML_OPERATOR_FORM_INFIX] = 0;
  aLeftSpace[NS_MATHML_OPERATOR_FORM_INFIX] = 0.0f;
  aRightSpace[NS_MATHML_OPERATOR_FORM_INFIX] = 0.0f;

  aFlags[NS_MATHML_OPERATOR_FORM_POSTFIX] = 0;
  aLeftSpace[NS_MATHML_OPERATOR_FORM_POSTFIX] = 0.0f;
  aRightSpace[NS_MATHML_OPERATOR_FORM_POSTFIX] = 0.0f;

  aFlags[NS_MATHML_OPERATOR_FORM_PREFIX] = 0;
  aLeftSpace[NS_MATHML_OPERATOR_FORM_PREFIX] = 0.0f;
  aRightSpace[NS_MATHML_OPERATOR_FORM_PREFIX] = 0.0f;

  if (gOperatorTable) {
    OperatorData* found;
    found = GetOperatorData(aOperator, NS_MATHML_OPERATOR_FORM_INFIX);
    if (found) {
      aFlags[NS_MATHML_OPERATOR_FORM_INFIX] = found->mFlags;
      aLeftSpace[NS_MATHML_OPERATOR_FORM_INFIX] = found->mLeftSpace;
      aRightSpace[NS_MATHML_OPERATOR_FORM_INFIX] = found->mRightSpace;
    }
    found = GetOperatorData(aOperator, NS_MATHML_OPERATOR_FORM_POSTFIX);
    if (found) {
      aFlags[NS_MATHML_OPERATOR_FORM_POSTFIX] = found->mFlags;
      aLeftSpace[NS_MATHML_OPERATOR_FORM_POSTFIX] = found->mLeftSpace;
      aRightSpace[NS_MATHML_OPERATOR_FORM_POSTFIX] = found->mRightSpace;
    }
    found = GetOperatorData(aOperator, NS_MATHML_OPERATOR_FORM_PREFIX);
    if (found) {
      aFlags[NS_MATHML_OPERATOR_FORM_PREFIX] = found->mFlags;
      aLeftSpace[NS_MATHML_OPERATOR_FORM_PREFIX] = found->mLeftSpace;
      aRightSpace[NS_MATHML_OPERATOR_FORM_PREFIX] = found->mRightSpace;
    }
  }
}

bool
nsMathMLOperators::IsMutableOperator(const nsString& aOperator)
{
  if (!gInitialized) {
    InitGlobals();
  }
  // lookup all the variants of the operator and return true if there
  // is a variant that is stretchy or largeop
  nsOperatorFlags flags[4];
  float lspace[4], rspace[4];
  nsMathMLOperators::LookupOperators(aOperator, flags, lspace, rspace);
  nsOperatorFlags allFlags =
    flags[NS_MATHML_OPERATOR_FORM_INFIX] |
    flags[NS_MATHML_OPERATOR_FORM_POSTFIX] |
    flags[NS_MATHML_OPERATOR_FORM_PREFIX];
  return NS_MATHML_OPERATOR_IS_STRETCHY(allFlags) ||
         NS_MATHML_OPERATOR_IS_LARGEOP(allFlags);
}

/* static */ nsStretchDirection
nsMathMLOperators::GetStretchyDirection(const nsString& aOperator)
{
  // LookupOperator will search infix, postfix and prefix forms of aOperator and
  // return the first form found. It is assumed that all these forms have same
  // direction.
  nsOperatorFlags flags = 0;
  float dummy;
  nsMathMLOperators::LookupOperator(aOperator,
                                    NS_MATHML_OPERATOR_FORM_INFIX,
                                    &flags, &dummy, &dummy);

  if (NS_MATHML_OPERATOR_IS_DIRECTION_VERTICAL(flags)) {
      return NS_STRETCH_DIRECTION_VERTICAL;
  } else if (NS_MATHML_OPERATOR_IS_DIRECTION_HORIZONTAL(flags)) {
    return NS_STRETCH_DIRECTION_HORIZONTAL;
  } else {
    return NS_STRETCH_DIRECTION_UNSUPPORTED;
  }
}

/* static */ eMATHVARIANT
nsMathMLOperators::LookupInvariantChar(const nsAString& aChar)
{
  if (!gInitialized) {
    InitGlobals();
  }
  if (gInvariantCharArray) {
    for (PRInt32 i = gInvariantCharArray->Length()-1; i >= 0; --i) {
      const nsString& list = gInvariantCharArray->ElementAt(i);
      nsString::const_iterator start, end;
      list.BeginReading(start);
      list.EndReading(end);
      // Style-invariant characters are at offset 3*j + 1.
      if (FindInReadable(aChar, start, end) &&
          start.size_backward() % 3 == 1) {
        return eMATHVARIANT(i);
      }
    }
  }
  return eMATHVARIANT_NONE;
}

/* static */ const nsDependentSubstring
nsMathMLOperators::TransformVariantChar(const PRUnichar& aChar,
                                        eMATHVARIANT aVariant)
{
  if (!gInitialized) {
    InitGlobals();
  }
  if (gInvariantCharArray) {
    nsString list = gInvariantCharArray->ElementAt(aVariant);
    PRInt32 index = list.FindChar(aChar);
    // BMP characters are at offset 3*j
    if (index != kNotFound && index % 3 == 0 && list.Length() - index >= 2 ) {
      // The style-invariant character is the next character
      // (and list should contain padding if the next character is in the BMP).
      ++index;
      PRUint32 len = NS_IS_HIGH_SURROGATE(list.CharAt(index)) ? 2 : 1;
      return nsDependentSubstring(list, index, len);
    }
  }
  return nsDependentSubstring(&aChar, &aChar + 1);  
}
