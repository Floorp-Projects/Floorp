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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   IBM Corporation
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsAttrValue.h"
#include "nsHTMLValue.h"
#include "nsIAtom.h"
#include "nsUnicharUtils.h"
#include "nsICSSStyleRule.h"
#include "nsCSSDeclaration.h"
#include "nsIHTMLDocument.h"
#include "nsIDocument.h"

#ifdef MOZ_SVG
#include "nsISVGValue.h"
#endif

nsAttrValue::nsAttrValue()
    : mBits(0)
{
}

nsAttrValue::nsAttrValue(const nsAttrValue& aOther)
    : mBits(0)
{
  SetTo(aOther);
}

nsAttrValue::nsAttrValue(const nsAString& aValue)
    : mBits(0)
{
  SetTo(aValue);
}

nsAttrValue::nsAttrValue(nsICSSStyleRule* aValue)
    : mBits(0)
{
  SetTo(aValue);
}

#ifdef MOZ_SVG
nsAttrValue::nsAttrValue(nsISVGValue* aValue)
    : mBits(0)
{
  SetTo(aValue);
}
#endif

nsAttrValue::~nsAttrValue()
{
  ResetIfSet();
}

nsAttrValue::ValueType
nsAttrValue::Type() const
{
  switch (BaseType()) {
    case eIntegerBase:
    {
      return NS_STATIC_CAST(ValueType, mBits & NS_ATTRVALUE_INTEGERTYPE_MASK);
    }
    case eOtherBase:
    {
      return GetMiscContainer()->mType;
    }
    default:
    {
      return NS_STATIC_CAST(ValueType, NS_STATIC_CAST(PRUint16, BaseType()));
    }
  }
}

void
nsAttrValue::Reset()
{
  switch(BaseType()) {
    case eStringBase:
    {
      PRUnichar* str = NS_STATIC_CAST(PRUnichar*, GetPtr());
      if (str) {
        nsCheapStringBufferUtils::Free(str);
      }

      break;
    }
    case eOtherBase:
    {
      EnsureEmptyMiscContainer();
      delete GetMiscContainer();

      break;
    }
    case eAtomBase:
    {
      nsIAtom* atom = GetAtomValue();
      NS_RELEASE(atom);

      break;
    }
    case eIntegerBase:
    {
      break;
    }
  }

  mBits = 0;
}

void
nsAttrValue::SetTo(const nsAttrValue& aOther)
{
  switch (aOther.BaseType()) {
    case eStringBase:
    {
      SetTo(aOther.GetStringValue());
      return;
    }
    case eOtherBase:
    {
      break;
    }
    case eAtomBase:
    {
      ResetIfSet();
      nsIAtom* atom = aOther.GetAtomValue();
      NS_ADDREF(atom);
      SetPtrValueAndType(atom, eAtomBase);
      return;
    }
    case eIntegerBase:
    {
      ResetIfSet();
      mBits = aOther.mBits;
      return;      
    }
  }

  MiscContainer* otherCont = aOther.GetMiscContainer();
  switch (otherCont->mType) {
    case eColor:
    {
      if (EnsureEmptyMiscContainer()) {
        MiscContainer* cont = GetMiscContainer();
        cont->mColor = otherCont->mColor;
        cont->mType = eColor;
      }
      break;
    }
    case eCSSStyleRule:
    {
      SetTo(otherCont->mCSSStyleRule);
      break;
    }
    case eAtomArray:
    {
      if (!EnsureEmptyAtomArray() ||
          !GetAtomArrayValue()->AppendObjects(*otherCont->mAtomArray)) {
        Reset();
      }
      break;
    }
#ifdef MOZ_SVG
    case eSVGValue:
    {
      SetTo(otherCont->mSVGValue);
    }
#endif
    default:
    {
      NS_NOTREACHED("unknown type stored in MiscContainer");
      break;
    }
  }
}

void
nsAttrValue::SetTo(const nsAString& aValue)
{
  PRUnichar* str = nsnull;
  PRBool empty = aValue.IsEmpty();
  void* ptr;
  if (BaseType() == eStringBase && (ptr = GetPtr())) {
    if (!empty) {
      nsCheapStringBufferUtils::
        CopyToExistingBuffer(str, NS_STATIC_CAST(PRUnichar*, ptr), aValue);
    }
    else {
      nsCheapStringBufferUtils::Free(NS_STATIC_CAST(PRUnichar*, ptr));
    }
  }
  else {
    ResetIfSet();
    if (!empty) {
      nsCheapStringBufferUtils::CopyToBuffer(str, aValue);
    }
  }
  SetPtrValueAndType(str, eStringBase);
}

void
nsAttrValue::SetTo(PRInt16 aInt, ValueType aType)
{
  ResetIfSet();
  SetIntValueAndType(aInt, aType);
}

void
nsAttrValue::SetTo(nsICSSStyleRule* aValue)
{
  if (EnsureEmptyMiscContainer()) {
    MiscContainer* cont = GetMiscContainer();
    NS_ADDREF(cont->mCSSStyleRule = aValue);
    cont->mType = eCSSStyleRule;
  }
}

#ifdef MOZ_SVG
void
nsAttrValue::SetTo(nsISVGValue* aValue)
{
  if (EnsureEmptyMiscContainer()) {
    MiscContainer* cont = GetMiscContainer();
    NS_ADDREF(cont->mSVGValue = aValue);
    cont->mType = eSVGValue;
  }
}
#endif

void
nsAttrValue::SwapValueWith(nsAttrValue& aOther)
{
  PtrBits tmp = aOther.mBits;
  aOther.mBits = mBits;
  mBits = tmp;
}

void
nsAttrValue::ToString(nsAString& aResult) const
{
  switch(Type()) {
    case eString:
    {
      PRUnichar* str = NS_STATIC_CAST(PRUnichar*, GetPtr());
      if (str) {
        aResult = nsCheapStringBufferUtils::GetDependentString(str);
      }
      else {
        aResult.Truncate();
      }
      break;
    }
    case eAtom:
    {
      NS_STATIC_CAST(nsIAtom*, GetPtr())->ToString(aResult);

      break;
    }
    case eInteger:
    {
      nsAutoString intStr;
      intStr.AppendInt(GetIntInternal());
      aResult = intStr;

      break;
    }
    case eColor:
    {
      nscolor v;
      GetColorValue(v);
      NS_RGBToHex(v, aResult);

      break;
    }
    case eProportional:
    {
      nsAutoString intStr;
      intStr.AppendInt(GetIntInternal());
      aResult = intStr + NS_LITERAL_STRING("*");

      break;
    }
    case eEnum:
    {
      NS_NOTREACHED("trying to convert enum to string");

      break;
    }
    case ePercent:
    {
      nsAutoString intStr;
      intStr.AppendInt(GetIntInternal());
      aResult = intStr + NS_LITERAL_STRING("%");

      break;
    }
    case eCSSStyleRule:
    {
      aResult.Truncate();
      nsCSSDeclaration* decl = 
        GetMiscContainer()->mCSSStyleRule->GetDeclaration();
      if (decl) {
        decl->ToString(aResult);
      }

      break;
    }
    case eAtomArray:
    {
      MiscContainer* cont = GetMiscContainer();
      PRInt32 count = cont->mAtomArray->Count();
      if (count) {
        cont->mAtomArray->ObjectAt(0)->ToString(aResult);
        nsAutoString tmp;
        PRInt32 i;
        for (i = 1; i < count; ++i) {
          cont->mAtomArray->ObjectAt(i)->ToString(tmp);
          aResult.Append(NS_LITERAL_STRING(" ") + tmp);
        }
      }
      else {
        aResult.Truncate();
      }
      break;
    }
#ifdef MOZ_SVG
    case eSVGValue:
    {
      GetMiscContainer()->mSVGValue->GetValueString(aResult);
    }
#endif
  }
}

void
nsAttrValue::ToHTMLValue(nsHTMLValue& aResult) const
{
  switch(Type()) {
    case eString:
    {
      aResult.SetStringValue(GetStringValue());
      break;
    }
    case eAtom:
    {
      nsAutoString tmp;
      GetAtomValue()->ToString(tmp);
      aResult.SetStringValue(tmp);
      break;
    }
    case eInteger:
    {
      aResult.SetIntValue(GetIntInternal(), eHTMLUnit_Integer);
      break;
    }
    case eColor:
    {
      nscolor v;
      GetColorValue(v);
      aResult.SetColorValue(v);
      break;
    }
    case eProportional:
    {
      aResult.SetIntValue(GetProportionalValue(), eHTMLUnit_Proportional);
      break;
    }
    case eEnum:
    {
      aResult.SetIntValue(GetEnumValue(), eHTMLUnit_Enumerated);
      break;
    }
    case ePercent:
    {
      aResult.SetPercentValue(GetPercentValue());
      break;
    }
    case eCSSStyleRule:
    {
      aResult.SetCSSStyleRuleValue(GetCSSStyleRuleValue());
      break;
    }
    case eAtomArray:
    {
      nsCOMArray<nsIAtom>* array = new nsCOMArray<nsIAtom>(*GetAtomArrayValue());
      aResult.SetAtomArrayValue(array);
      break;
    }
#ifdef MOZ_SVG
    case eSVGValue:
    {
      nsAutoString tmp;
      GetSVGValue()->GetValueString(tmp);
      aResult.SetStringValue(tmp);
    }
#endif
  }
}

const nsDependentSubstring
nsAttrValue::GetStringValue() const
{
  NS_PRECONDITION(Type() == eString, "wrong type");

  static const PRUnichar blankStr[] = { '\0' };
  void* ptr = GetPtr();
  return ptr
         ? nsCheapStringBufferUtils::GetDependentString(NS_STATIC_CAST(PRUnichar*, ptr))
         : Substring(blankStr, blankStr);
}

PRBool
nsAttrValue::GetColorValue(nscolor& aColor) const
{
  NS_PRECONDITION(Type() == eColor || Type() == eString, "wrong type");
  switch (BaseType()) {
    case eString:
    {
      return GetPtr() && NS_ColorNameToRGB(GetStringValue(), &aColor);
    }
    case eOtherBase:
    {
      aColor = GetMiscContainer()->mColor;
      
      break;
    }
    case eIntegerBase:
    {
      aColor = NS_STATIC_CAST(nscolor, GetIntInternal());
      
      break;
    }
    default:
    {
      NS_NOTREACHED("unexpected basetype");
      
      break;
    }
  }

  return PR_TRUE;
}

PRInt32
nsAttrValue::GetAtomCount() const
{
  ValueType type = Type();

  if (type == eAtom) {
    return 1;
  }

  if (type == eAtomArray) {
    return GetAtomArrayValue()->Count();
  }

  return 0;
}

nsIAtom*
nsAttrValue::AtomAt(PRInt32 aIndex) const
{
  NS_PRECONDITION(aIndex >= 0, "Index must not be negative");
  NS_PRECONDITION(GetAtomCount() > aIndex, "aIndex out of range");
  
  if (BaseType() == eAtomBase) {
    return GetAtomValue();
  }

  NS_ASSERTION(Type() == eAtomArray, "GetAtomCount must be confused");
  
  return GetAtomArrayValue()->ObjectAt(aIndex);
}

PRUint32
nsAttrValue::HashValue() const
{
  switch(BaseType()) {
    case eStringBase:
    {
      PRUnichar* str = NS_STATIC_CAST(PRUnichar*, GetPtr());
      return str ? nsCheapStringBufferUtils::HashCode(str) : 0;
    }
    case eOtherBase:
    {
      break;
    }
    case eAtomBase:
    case eIntegerBase:
    {
      // mBits and PRUint32 might have different size. This should silence
      // any warnings or compile-errors. This is what the implementation of
      // NS_PTR_TO_INT32 does to take care of the same problem.
      return mBits - 0;
    }
  }

  MiscContainer* cont = GetMiscContainer();
  switch (cont->mType) {
    case eColor:
    {
      return cont->mColor;
    }
    case eCSSStyleRule:
    {
      return NS_PTR_TO_INT32(cont->mCSSStyleRule);
    }
    case eAtomArray:
    {
      PRUint32 retval = 0;
      PRInt32 i, count = cont->mAtomArray->Count();
      for (i = 0; i < count; ++i) {
        retval ^= NS_PTR_TO_INT32(cont->mAtomArray->ObjectAt(i));
      }
      return retval;
    }
#ifdef MOZ_SVG
    case eSVGValue:
    {
      return NS_PTR_TO_INT32(cont->mSVGValue);
    }
#endif
    default:
    {
      NS_NOTREACHED("unknown type stored in MiscContainer");
      return 0;
    }
  }
}

PRBool
nsAttrValue::Equals(const nsAttrValue& aOther) const
{
  if (BaseType() != aOther.BaseType()) {
    return PR_FALSE;
  }

  switch(BaseType()) {
    case eStringBase:
    {
      return GetStringValue().Equals(aOther.GetStringValue());
    }
    case eOtherBase:
    {
      break;
    }
    case eAtomBase:
    case eIntegerBase:
    {
      return mBits == aOther.mBits;
    }
  }

  MiscContainer* thisCont = GetMiscContainer();
  MiscContainer* otherCont = aOther.GetMiscContainer();
  if (thisCont->mType != otherCont->mType) {
    return PR_FALSE;
  }

  switch (thisCont->mType) {
    case eColor:
    {
      return thisCont->mColor == otherCont->mColor;
    }
    case eCSSStyleRule:
    {
      return thisCont->mCSSStyleRule == otherCont->mCSSStyleRule;
    }
    case eAtomArray:
    {
      // For classlists we could be insensitive to order, however
      // classlists are never mapped attributes so they are never compared.

      PRInt32 count = thisCont->mAtomArray->Count();
      if (count != otherCont->mAtomArray->Count()) {
        return PR_FALSE;
      }

      PRInt32 i;
      for (i = 0; i < count; ++i) {
        if (thisCont->mAtomArray->ObjectAt(i) !=
            otherCont->mAtomArray->ObjectAt(i)) {
          return PR_FALSE;
        }
      }
      return PR_TRUE;
    }
#ifdef MOZ_SVG
    case eSVGValue:
    {
      return thisCont->mSVGValue == otherCont->mSVGValue;
    }
#endif
    default:
    {
      NS_NOTREACHED("unknown type stored in MiscContainer");
      return PR_FALSE;
    }
  }
}

void
nsAttrValue::ParseAtom(const nsAString& aValue)
{
  ResetIfSet();

  nsIAtom* atom = NS_NewAtom(aValue);
  if (atom) {
    SetPtrValueAndType(atom, eAtomBase);
  }
}

void
nsAttrValue::ParseAtomArray(const nsAString& aValue)
{
  nsAString::const_iterator iter, end;
  aValue.BeginReading(iter);
  aValue.EndReading(end);

  // skip initial whitespace
  while (iter != end && nsCRT::IsAsciiSpace(*iter)) {
    ++iter;
  }

  if (iter == end) {
    ResetIfSet();
    return;
  }

  nsAString::const_iterator start(iter);

  // get first - and often only - atom
  do {
    ++iter;
  } while (iter != end && !nsCRT::IsAsciiSpace(*iter));

  nsCOMPtr<nsIAtom> classAtom = do_GetAtom(Substring(start, iter));
  if (!classAtom) {
    Reset();
    return;
  }

  // skip whitespace
  while (iter != end && nsCRT::IsAsciiSpace(*iter)) {
    ++iter;
  }

  if (iter == end) {
    // we only found one classname so don't bother storing a list
    ResetIfSet();
    nsIAtom* atom = nsnull;
    classAtom.swap(atom);
    SetPtrValueAndType(atom, eAtomBase);
    return;
  }

  if (!EnsureEmptyAtomArray()) {
    return;
  }

  nsCOMArray<nsIAtom>* array = GetAtomArrayValue();
  
  if (!array->AppendObject(classAtom)) {
    Reset();
    return;
  }

  // parse the rest of the classnames
  do {
    start = iter;

    do {
      ++iter;
    } while (iter != end && !nsCRT::IsAsciiSpace(*iter));

    classAtom = do_GetAtom(Substring(start, iter));

    if (!array->AppendObject(classAtom)) {
      Reset();
      return;
    }

    // skip whitespace
    while (iter != end && nsCRT::IsAsciiSpace(*iter)) {
      ++iter;
    }
  } while (iter != end);

  return;
}

void
nsAttrValue::ParseStringOrAtom(const nsAString& aValue)
{
  PRUint32 len = aValue.Length();
  // Don't bother with atoms if it's an empty string since
  // we can store those efficently anyway.
  if (len && len <= NS_ATTRVALUE_MAX_STRINGLENGTH_ATOM) {
    ParseAtom(aValue);
  }
  else {
    SetTo(aValue);
  }
}

PRBool
nsAttrValue::ParseEnumValue(const nsAString& aValue,
                            const nsHTMLValue::EnumTable* aTable,
                            PRBool aCaseSensitive)
{
  ResetIfSet();

  nsAutoString val(aValue);
  while (aTable->tag) {
    if (aCaseSensitive ? val.EqualsASCII(aTable->tag) :
                         val.EqualsIgnoreCase(aTable->tag)) {
      SetIntValueAndType(aTable->value, eEnum);

      return PR_TRUE;
    }
    aTable++;
  }

  return PR_FALSE;
}

PRBool
nsAttrValue::ParseSpecialIntValue(const nsAString& aString,
                                  PRBool aCanBePercent,
                                  PRBool aCanBeProportional)
{
  ResetIfSet();

  PRInt32 ec;
  nsAutoString tmp(aString);
  PRInt32 val = tmp.ToInteger(&ec);

  if (NS_FAILED(ec)) {
    if (aCanBeProportional) {
      // Even if the integer could not be parsed, it might just be "*"
      tmp.CompressWhitespace(PR_TRUE, PR_TRUE);
      if (tmp.Length() == 1 && tmp.Last() == '*') {
        // special case: HTML spec says a value '*' == '1*'
        // see http://www.w3.org/TR/html4/types.html#type-multi-length
        // bug 29061
        SetIntValueAndType(1, eProportional);
        return PR_TRUE;
      }
    }
    return PR_FALSE;
  }

  val = PR_MAX(val, 0);
  val = PR_MIN(val, NS_ATTRVALUE_INTEGERTYPE_MAXVALUE);

  // % (percent)
  // XXX RFindChar means that 5%x will be parsed!
  if (aCanBePercent && tmp.RFindChar('%') >= 0) {
    if (val > 100) {
      val = 100;
    }
    SetIntValueAndType(val, ePercent);
    return PR_TRUE;
  }

  // * (proportional) 
  // XXX RFindChar means that 5*x will be parsed!
  if (aCanBeProportional && tmp.RFindChar('*') >= 0) {
    SetIntValueAndType(val, eProportional);
    return PR_TRUE;
  }

  // Straight number is interpreted as integer
  SetIntValueAndType(val, eInteger);
  return PR_TRUE;
}

PRBool
nsAttrValue::ParseIntWithBounds(const nsAString& aString,
                                PRInt32 aMin, PRInt32 aMax)
{
  NS_PRECONDITION(aMin < aMax &&
                  aMin >= NS_ATTRVALUE_INTEGERTYPE_MINVALUE &&
                  aMax <= NS_ATTRVALUE_INTEGERTYPE_MAXVALUE, "bad boundaries");

  ResetIfSet();

  PRInt32 ec;
  PRInt32 val = PromiseFlatString(aString).ToInteger(&ec);
  if (NS_FAILED(ec)) {
    return PR_FALSE;
  }

  val = PR_MAX(val, aMin);
  val = PR_MIN(val, aMax);
  SetIntValueAndType(val, eInteger);

  return PR_TRUE;
}

PRBool
nsAttrValue::ParseColor(const nsAString& aString, nsIDocument* aDocument)
{
  nsAutoString colorStr(aString);
  colorStr.CompressWhitespace(PR_TRUE, PR_TRUE);
  if (colorStr.IsEmpty()) {
    Reset();
    return PR_FALSE;
  }

  nscolor color;
  // No color names begin with a '#', but numerical colors do so
  // it is a very common first char
  if ((colorStr.CharAt(0) != '#') && NS_ColorNameToRGB(colorStr, &color)) {
    SetTo(colorStr);
    return PR_TRUE;
  }

  // Check if we are in compatibility mode
  // XXX evil NS_HexToRGB and NS_LooseHexToRGB take nsString as argument!
  nsCOMPtr<nsIHTMLDocument> doc(do_QueryInterface(aDocument));
  if (doc && doc->GetCompatibilityMode() == eCompatibility_NavQuirks) {
    NS_LooseHexToRGB(colorStr, &color);
  }
  else {
    if (colorStr.First() != '#') {
      Reset();
      return PR_FALSE;
    }
    colorStr.Cut(0, 1);
    if (!NS_HexToRGB(colorStr, &color)) {
      Reset();
      return PR_FALSE;
    }
  }

  PRInt32 colAsInt = NS_STATIC_CAST(PRInt32, color);
  PRInt32 tmp = colAsInt * NS_ATTRVALUE_INTEGERTYPE_MULTIPLIER;
  if (tmp / NS_ATTRVALUE_INTEGERTYPE_MULTIPLIER == colAsInt) {
    ResetIfSet();
    SetIntValueAndType(colAsInt, eColor);
  }
  else if (EnsureEmptyMiscContainer()) {
    MiscContainer* cont = GetMiscContainer();
    cont->mColor = color;
    cont->mType = eColor;
  }

  return PR_TRUE;
}

PRBool
nsAttrValue::EnsureEmptyMiscContainer()
{
  MiscContainer* cont;
  if (BaseType() == eOtherBase) {
    cont = GetMiscContainer();
    switch (cont->mType) {
      case eCSSStyleRule:
      {
        NS_RELEASE(cont->mCSSStyleRule);
        break;
      }
      case eAtomArray:
      {
        delete cont->mAtomArray;
        break;
      }
      default:
      {
        break;
      }
    }
  }
  else {
    ResetIfSet();

    cont = new MiscContainer;
    NS_ENSURE_TRUE(cont, PR_FALSE);

    SetPtrValueAndType(cont, eOtherBase);
  }

  cont->mType = eColor;
  cont->mColor = 0;

  return PR_TRUE;
}

PRBool
nsAttrValue::EnsureEmptyAtomArray()
{
  if (Type() == eAtomArray) {
    GetAtomArrayValue()->Clear();
    return PR_TRUE;
  }

  if (!EnsureEmptyMiscContainer()) {
    // should already be reset
    return PR_FALSE;
  }

  nsCOMArray<nsIAtom>* array = new nsCOMArray<nsIAtom>;
  if (!array) {
    Reset();
    return PR_FALSE;
  }

  MiscContainer* cont = GetMiscContainer();
  cont->mAtomArray = array;
  cont->mType = eAtomArray;

  return PR_TRUE;
}
