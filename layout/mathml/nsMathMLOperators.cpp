/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMathMLOperators.h"
#include "nsCOMPtr.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsTArray.h"

#include "nsIPersistentProperties2.h"
#include "nsNetUtil.h"
#include "nsCRT.h"

// operator dictionary entry
struct OperatorData {
  OperatorData(void)
    : mFlags(0),
      mLeadingSpace(0.0f),
      mTrailingSpace(0.0f)
  {
  }

  // member data
  nsString        mStr;
  nsOperatorFlags mFlags;
  float           mLeadingSpace;   // unit is em
  float           mTrailingSpace;  // unit is em
};

static int32_t         gTableRefCount = 0;
static uint32_t        gOperatorCount = 0;
static OperatorData*   gOperatorArray = nullptr;
static nsDataHashtable<nsStringHashKey, OperatorData*>* gOperatorTable = nullptr;
static bool            gGlobalsInitialized   = false;

static const char16_t kDashCh  = char16_t('#');
static const char16_t kColonCh = char16_t(':');

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
  else if (aName.EqualsLiteral("mirrorable"))
    aOperatorData->mFlags |= NS_MATHML_OPERATOR_MIRRORABLE;
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
    bool isLeadingSpace;
    if (aName.EqualsLiteral("lspace"))
      isLeadingSpace = true;
    else if (aName.EqualsLiteral("rspace"))
      isLeadingSpace = false;
    else return;  // input is not applicable

    // aValue is assumed to be a digit from 0 to 7
    nsresult error = NS_OK;
    float space = aValue.ToFloat(&error) / 18.0;
    if (NS_FAILED(error)) return;

    if (isLeadingSpace)
      aOperatorData->mLeadingSpace = space;
    else
      aOperatorData->mTrailingSpace = space;
  }
}

static bool
SetOperator(OperatorData*   aOperatorData,
            nsOperatorFlags aForm,
            const nsCString& aOperator,
            nsString&        aAttributes)

{
  static const char16_t kNullCh = char16_t('\0');

  // aOperator is in the expanded format \uNNNN\uNNNN ...
  // First compress these Unicode points to the internal nsString format
  int32_t i = 0;
  nsAutoString name, value;
  int32_t len = aOperator.Length();
  char16_t c = aOperator[i++];
  uint32_t state  = 0;
  char16_t uchar = 0;
  while (i <= len) {
    if (0 == state) {
      if (c != '\\')
        return false;
      if (i < len)
        c = aOperator[i];
      i++;
      if (('u' != c) && ('U' != c))
        return false;
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
      else return false;
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
  if (0 != state) return false;

  // Quick return when the caller doesn't care about the attributes and just wants
  // to know if this is a valid operator (this is the case at the first pass of the
  // parsing of the dictionary in InitOperators())
  if (!aForm) return true;

  // Add operator to hash table
  aOperatorData->mFlags |= aForm;
  aOperatorData->mStr.Assign(value);
  value.AppendInt(aForm, 10);
  gOperatorTable->Put(value, aOperatorData);

#ifdef DEBUG
  NS_LossyConvertUTF16toASCII str(aAttributes);
#endif
  // Loop over the space-delimited list of attributes to get the name:value pairs
  aAttributes.Append(kNullCh);  // put an extra null at the end
  char16_t* start = aAttributes.BeginWriting();
  char16_t* end   = start;
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
  return true;
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

  // Parse the Operator Dictionary in two passes.
  // The first pass is to count the number of operators; the second pass is to
  // allocate the necessary space for them and to add them in the hash table.
  for (int32_t pass = 1; pass <= 2; pass++) {
    OperatorData dummyData;
    OperatorData* operatorData = &dummyData;
    nsCOMPtr<nsISimpleEnumerator> iterator;
    if (NS_SUCCEEDED(mathfontProp->Enumerate(getter_AddRefs(iterator)))) {
      bool more;
      uint32_t index = 0;
      nsAutoCString name;
      nsAutoString attributes;
      while ((NS_SUCCEEDED(iterator->HasMoreElements(&more))) && more) {
        nsCOMPtr<nsISupports> supports;
        nsCOMPtr<nsIPropertyElement> element;
        if (NS_SUCCEEDED(iterator->GetNext(getter_AddRefs(supports)))) {
          element = do_QueryInterface(supports);
          if (NS_SUCCEEDED(element->GetKey(name)) &&
              NS_SUCCEEDED(element->GetValue(attributes))) {
            // expected key: operator.\uNNNN.{infix,postfix,prefix}
            if ((21 <= name.Length()) && (0 == name.Find("operator.\\u"))) {
              name.Cut(0, 9); // 9 is the length of "operator.";
              int32_t len = name.Length();
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
  gGlobalsInitialized = true;
  nsresult rv = NS_ERROR_OUT_OF_MEMORY;
  gOperatorTable = new nsDataHashtable<nsStringHashKey, OperatorData*>();
  if (gOperatorTable) {
    rv = InitOperators();
  }
  if (NS_FAILED(rv))
    nsMathMLOperators::CleanUp();
  return rv;
}

void
nsMathMLOperators::CleanUp()
{
  if (gOperatorArray) {
    delete[] gOperatorArray;
    gOperatorArray = nullptr;
  }
  if (gOperatorTable) {
    delete gOperatorTable;
    gOperatorTable = nullptr;
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
  return gOperatorTable->Get(key);
}

bool
nsMathMLOperators::LookupOperator(const nsString&       aOperator,
                                  const nsOperatorFlags aForm,
                                  nsOperatorFlags*      aFlags,
                                  float*                aLeadingSpace,
                                  float*                aTrailingSpace)
{
  if (!gGlobalsInitialized) {
    InitGlobals();
  }
  if (gOperatorTable) {
    NS_ASSERTION(aFlags && aLeadingSpace && aTrailingSpace, "bad usage");
    NS_ASSERTION(aForm > 0 && aForm < 4, "*** invalid call ***");

    // The MathML REC says:
    // If the operator does not occur in the dictionary with the specified form,
    // the renderer should use one of the forms which is available there, in the
    // order of preference: infix, postfix, prefix.

    OperatorData* found;
    int32_t form = NS_MATHML_OPERATOR_GET_FORM(aForm);
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
      *aLeadingSpace = found->mLeadingSpace;
      *aTrailingSpace = found->mTrailingSpace;
      *aFlags &= ~NS_MATHML_OPERATOR_FORM; // clear the form bits
      *aFlags |= found->mFlags; // just add bits without overwriting
      return true;
    }
  }
  return false;
}

void
nsMathMLOperators::LookupOperators(const nsString&       aOperator,
                                   nsOperatorFlags*      aFlags,
                                   float*                aLeadingSpace,
                                   float*                aTrailingSpace)
{
  if (!gGlobalsInitialized) {
    InitGlobals();
  }

  aFlags[NS_MATHML_OPERATOR_FORM_INFIX] = 0;
  aLeadingSpace[NS_MATHML_OPERATOR_FORM_INFIX] = 0.0f;
  aTrailingSpace[NS_MATHML_OPERATOR_FORM_INFIX] = 0.0f;

  aFlags[NS_MATHML_OPERATOR_FORM_POSTFIX] = 0;
  aLeadingSpace[NS_MATHML_OPERATOR_FORM_POSTFIX] = 0.0f;
  aTrailingSpace[NS_MATHML_OPERATOR_FORM_POSTFIX] = 0.0f;

  aFlags[NS_MATHML_OPERATOR_FORM_PREFIX] = 0;
  aLeadingSpace[NS_MATHML_OPERATOR_FORM_PREFIX] = 0.0f;
  aTrailingSpace[NS_MATHML_OPERATOR_FORM_PREFIX] = 0.0f;

  if (gOperatorTable) {
    OperatorData* found;
    found = GetOperatorData(aOperator, NS_MATHML_OPERATOR_FORM_INFIX);
    if (found) {
      aFlags[NS_MATHML_OPERATOR_FORM_INFIX] = found->mFlags;
      aLeadingSpace[NS_MATHML_OPERATOR_FORM_INFIX] = found->mLeadingSpace;
      aTrailingSpace[NS_MATHML_OPERATOR_FORM_INFIX] = found->mTrailingSpace;
    }
    found = GetOperatorData(aOperator, NS_MATHML_OPERATOR_FORM_POSTFIX);
    if (found) {
      aFlags[NS_MATHML_OPERATOR_FORM_POSTFIX] = found->mFlags;
      aLeadingSpace[NS_MATHML_OPERATOR_FORM_POSTFIX] = found->mLeadingSpace;
      aTrailingSpace[NS_MATHML_OPERATOR_FORM_POSTFIX] = found->mTrailingSpace;
    }
    found = GetOperatorData(aOperator, NS_MATHML_OPERATOR_FORM_PREFIX);
    if (found) {
      aFlags[NS_MATHML_OPERATOR_FORM_PREFIX] = found->mFlags;
      aLeadingSpace[NS_MATHML_OPERATOR_FORM_PREFIX] = found->mLeadingSpace;
      aTrailingSpace[NS_MATHML_OPERATOR_FORM_PREFIX] = found->mTrailingSpace;
    }
  }
}

bool
nsMathMLOperators::IsMutableOperator(const nsString& aOperator)
{
  if (!gGlobalsInitialized) {
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

/* static */ bool
nsMathMLOperators::IsMirrorableOperator(const nsString& aOperator)
{
  // LookupOperator will search infix, postfix and prefix forms of aOperator and
  // return the first form found. It is assumed that all these forms have same
  // mirrorability.
  nsOperatorFlags flags = 0;
  float dummy;
  nsMathMLOperators::LookupOperator(aOperator,
                                    NS_MATHML_OPERATOR_FORM_INFIX,
                                    &flags, &dummy, &dummy);
  return NS_MATHML_OPERATOR_IS_MIRRORABLE(flags);
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
