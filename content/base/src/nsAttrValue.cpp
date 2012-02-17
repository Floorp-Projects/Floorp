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

/*
 * A struct that represents the value (type and actual data) of an
 * attribute.
 */

#include "nsAttrValue.h"
#include "nsIAtom.h"
#include "nsUnicharUtils.h"
#include "mozilla/css/StyleRule.h"
#include "mozilla/css/Declaration.h"
#include "nsIHTMLDocument.h"
#include "nsIDocument.h"
#include "nsContentUtils.h"
#include "nsReadableUtils.h"
#include "prprf.h"

namespace css = mozilla::css;

using mozilla::SVGAttrValueWrapper;

#define MISC_STR_PTR(_cont) \
  reinterpret_cast<void*>((_cont)->mStringBits & NS_ATTRVALUE_POINTERVALUE_MASK)

nsTArray<const nsAttrValue::EnumTable*>* nsAttrValue::sEnumTableArray = nsnull;

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

nsAttrValue::nsAttrValue(nsIAtom* aValue)
    : mBits(0)
{
  SetTo(aValue);
}

nsAttrValue::nsAttrValue(css::StyleRule* aValue, const nsAString* aSerialized)
    : mBits(0)
{
  SetTo(aValue, aSerialized);
}

nsAttrValue::nsAttrValue(const nsIntMargin& aValue)
    : mBits(0)
{
  SetTo(aValue);
}

nsAttrValue::~nsAttrValue()
{
  ResetIfSet();
}

/* static */
nsresult
nsAttrValue::Init()
{
  NS_ASSERTION(!sEnumTableArray, "nsAttrValue already initialized");

  sEnumTableArray = new nsTArray<const EnumTable*>;
  NS_ENSURE_TRUE(sEnumTableArray, NS_ERROR_OUT_OF_MEMORY);
  
  return NS_OK;
}

/* static */
void
nsAttrValue::Shutdown()
{
  delete sEnumTableArray;
  sEnumTableArray = nsnull;
}

nsAttrValue::ValueType
nsAttrValue::Type() const
{
  switch (BaseType()) {
    case eIntegerBase:
    {
      return static_cast<ValueType>(mBits & NS_ATTRVALUE_INTEGERTYPE_MASK);
    }
    case eOtherBase:
    {
      return GetMiscContainer()->mType;
    }
    default:
    {
      return static_cast<ValueType>(static_cast<PRUint16>(BaseType()));
    }
  }
}

void
nsAttrValue::Reset()
{
  switch(BaseType()) {
    case eStringBase:
    {
      nsStringBuffer* str = static_cast<nsStringBuffer*>(GetPtr());
      if (str) {
        str->Release();
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
  if (this == &aOther) {
    return;
  }

  switch (aOther.BaseType()) {
    case eStringBase:
    {
      ResetIfSet();
      nsStringBuffer* str = static_cast<nsStringBuffer*>(aOther.GetPtr());
      if (str) {
        str->AddRef();
        SetPtrValueAndType(str, eStringBase);
      }
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
  if (!EnsureEmptyMiscContainer()) {
    return;
  }

  MiscContainer* cont = GetMiscContainer();
  switch (otherCont->mType) {
    case eInteger:
    {
      cont->mInteger = otherCont->mInteger;
      break;
    }
    case eEnum:
    {
      cont->mEnumValue = otherCont->mEnumValue;
      break;
    }
    case ePercent:
    {
      cont->mPercent = otherCont->mPercent;
      break;
    }
    case eColor:
    {
      cont->mColor = otherCont->mColor;
      break;
    }
    case eCSSStyleRule:
    {
      NS_ADDREF(cont->mCSSStyleRule = otherCont->mCSSStyleRule);
      break;
    }
    case eAtomArray:
    {
      if (!EnsureEmptyAtomArray() ||
          !GetAtomArrayValue()->AppendElements(*otherCont->mAtomArray)) {
        Reset();
        return;
      }
      break;
    }
    case eDoubleValue:
    {
      cont->mDoubleValue = otherCont->mDoubleValue;
      break;
    }
    case eIntMarginValue:
    {
      if (otherCont->mIntMargin)
        cont->mIntMargin = new nsIntMargin(*otherCont->mIntMargin);
      break;
    }
    default:
    {
      if (IsSVGType(otherCont->mType)) {
        // All SVG types are just pointers to classes and will therefore have
        // the same size so it doesn't really matter which one we assign
        cont->mSVGAngle = otherCont->mSVGAngle;
      } else {
        NS_NOTREACHED("unknown type stored in MiscContainer");
      }
      break;
    }
  }

  void* otherPtr = MISC_STR_PTR(otherCont);
  if (otherPtr) {
    if (static_cast<ValueBaseType>(otherCont->mStringBits & NS_ATTRVALUE_BASETYPE_MASK) ==
        eStringBase) {
      static_cast<nsStringBuffer*>(otherPtr)->AddRef();
    } else {
      static_cast<nsIAtom*>(otherPtr)->AddRef();
    }
    cont->mStringBits = otherCont->mStringBits;
  }
  // Note, set mType after switch-case, otherwise EnsureEmptyAtomArray doesn't
  // work correctly.
  cont->mType = otherCont->mType;
}

void
nsAttrValue::SetTo(const nsAString& aValue)
{
  ResetIfSet();
  nsStringBuffer* buf = GetStringBuffer(aValue);
  if (buf) {
    SetPtrValueAndType(buf, eStringBase);
  }
}

void
nsAttrValue::SetTo(nsIAtom* aValue)
{
  ResetIfSet();
  if (aValue) {
    NS_ADDREF(aValue);
    SetPtrValueAndType(aValue, eAtomBase);
  }
}

void
nsAttrValue::SetTo(PRInt16 aInt)
{
  ResetIfSet();
  SetIntValueAndType(aInt, eInteger, nsnull);
}

void
nsAttrValue::SetTo(PRInt32 aInt, const nsAString* aSerialized)
{
  ResetIfSet();
  SetIntValueAndType(aInt, eInteger, aSerialized);
}

void
nsAttrValue::SetTo(double aValue, const nsAString* aSerialized)
{
  if (EnsureEmptyMiscContainer()) {
    MiscContainer* cont = GetMiscContainer();
    cont->mDoubleValue = aValue;
    cont->mType = eDoubleValue;
    SetMiscAtomOrString(aSerialized);
  }
}

void
nsAttrValue::SetTo(css::StyleRule* aValue, const nsAString* aSerialized)
{
  if (EnsureEmptyMiscContainer()) {
    MiscContainer* cont = GetMiscContainer();
    NS_ADDREF(cont->mCSSStyleRule = aValue);
    cont->mType = eCSSStyleRule;
    SetMiscAtomOrString(aSerialized);
  }
}

void
nsAttrValue::SetTo(const nsIntMargin& aValue)
{
  if (EnsureEmptyMiscContainer()) {
    MiscContainer* cont = GetMiscContainer();
    cont->mIntMargin = new nsIntMargin(aValue);
    cont->mType = eIntMarginValue;
  }
}

void
nsAttrValue::SetToSerialized(const nsAttrValue& aOther)
{
  if (aOther.Type() != nsAttrValue::eString &&
      aOther.Type() != nsAttrValue::eAtom) {
    nsAutoString val;
    aOther.ToString(val);
    SetTo(val);
  } else {
    SetTo(aOther);
  }
}

void
nsAttrValue::SetTo(const nsSVGAngle& aValue, const nsAString* aSerialized)
{
  SetSVGType(eSVGAngle, &aValue, aSerialized);
}

void
nsAttrValue::SetTo(const nsSVGIntegerPair& aValue, const nsAString* aSerialized)
{
  SetSVGType(eSVGIntegerPair, &aValue, aSerialized);
}

void
nsAttrValue::SetTo(const nsSVGLength2& aValue, const nsAString* aSerialized)
{
  SetSVGType(eSVGLength, &aValue, aSerialized);
}

void
nsAttrValue::SetTo(const mozilla::SVGLengthList& aValue,
                   const nsAString* aSerialized)
{
  // While an empty string will parse as a length list, there's no need to store
  // it (and SetMiscAtomOrString will assert if we try)
  if (aSerialized && aSerialized->IsEmpty()) {
    aSerialized = nsnull;
  }
  SetSVGType(eSVGLengthList, &aValue, aSerialized);
}

void
nsAttrValue::SetTo(const mozilla::SVGNumberList& aValue,
                   const nsAString* aSerialized)
{
  // While an empty string will parse as a number list, there's no need to store
  // it (and SetMiscAtomOrString will assert if we try)
  if (aSerialized && aSerialized->IsEmpty()) {
    aSerialized = nsnull;
  }
  SetSVGType(eSVGNumberList, &aValue, aSerialized);
}

void
nsAttrValue::SetTo(const nsSVGNumberPair& aValue, const nsAString* aSerialized)
{
  SetSVGType(eSVGNumberPair, &aValue, aSerialized);
}

void
nsAttrValue::SetTo(const mozilla::SVGPathData& aValue,
                   const nsAString* aSerialized)
{
  // While an empty string will parse as path data, there's no need to store it
  // (and SetMiscAtomOrString will assert if we try)
  if (aSerialized && aSerialized->IsEmpty()) {
    aSerialized = nsnull;
  }
  SetSVGType(eSVGPathData, &aValue, aSerialized);
}

void
nsAttrValue::SetTo(const mozilla::SVGPointList& aValue,
                   const nsAString* aSerialized)
{
  // While an empty string will parse as a point list, there's no need to store
  // it (and SetMiscAtomOrString will assert if we try)
  if (aSerialized && aSerialized->IsEmpty()) {
    aSerialized = nsnull;
  }
  SetSVGType(eSVGPointList, &aValue, aSerialized);
}

void
nsAttrValue::SetTo(const mozilla::SVGAnimatedPreserveAspectRatio& aValue,
                   const nsAString* aSerialized)
{
  SetSVGType(eSVGPreserveAspectRatio, &aValue, aSerialized);
}

void
nsAttrValue::SetTo(const mozilla::SVGStringList& aValue,
                   const nsAString* aSerialized)
{
  // While an empty string will parse as a string list, there's no need to store
  // it (and SetMiscAtomOrString will assert if we try)
  if (aSerialized && aSerialized->IsEmpty()) {
    aSerialized = nsnull;
  }
  SetSVGType(eSVGStringList, &aValue, aSerialized);
}

void
nsAttrValue::SetTo(const mozilla::SVGTransformList& aValue,
                   const nsAString* aSerialized)
{
  // While an empty string will parse as a transform list, there's no need to
  // store it (and SetMiscAtomOrString will assert if we try)
  if (aSerialized && aSerialized->IsEmpty()) {
    aSerialized = nsnull;
  }
  SetSVGType(eSVGTransformList, &aValue, aSerialized);
}

void
nsAttrValue::SetTo(const nsSVGViewBox& aValue, const nsAString* aSerialized)
{
  SetSVGType(eSVGViewBox, &aValue, aSerialized);
}

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
  MiscContainer* cont = nsnull;
  if (BaseType() == eOtherBase) {
    cont = GetMiscContainer();
    void* ptr = MISC_STR_PTR(cont);
    if (ptr) {
      if (static_cast<ValueBaseType>(cont->mStringBits & NS_ATTRVALUE_BASETYPE_MASK) ==
          eStringBase) {
        nsStringBuffer* str = static_cast<nsStringBuffer*>(ptr);
        if (str) {
          str->ToString(str->StorageSize()/sizeof(PRUnichar) - 1, aResult);
          return;
        }
      } else {
        nsIAtom *atom = static_cast<nsIAtom*>(ptr);
        atom->ToString(aResult);
        return;
      }
    }
  }

  switch(Type()) {
    case eString:
    {
      nsStringBuffer* str = static_cast<nsStringBuffer*>(GetPtr());
      if (str) {
        str->ToString(str->StorageSize()/sizeof(PRUnichar) - 1, aResult);
      }
      else {
        aResult.Truncate();
      }
      break;
    }
    case eAtom:
    {
      nsIAtom *atom = static_cast<nsIAtom*>(GetPtr());
      atom->ToString(aResult);

      break;
    }
    case eInteger:
    {
      nsAutoString intStr;
      intStr.AppendInt(GetIntegerValue());
      aResult = intStr;

      break;
    }
#ifdef DEBUG
    case eColor:
    {
      NS_NOTREACHED("color attribute without string data");
      aResult.Truncate();
      break;
    }
#endif
    case eEnum:
    {
      GetEnumString(aResult, false);
      break;
    }
    case ePercent:
    {
      nsAutoString intStr;
      intStr.AppendInt(cont ? cont->mPercent : GetIntInternal());
      aResult = intStr + NS_LITERAL_STRING("%");

      break;
    }
    case eCSSStyleRule:
    {
      aResult.Truncate();
      MiscContainer *container = GetMiscContainer();
      css::Declaration *decl = container->mCSSStyleRule->GetDeclaration();
      if (decl) {
        decl->ToString(aResult);
      }
      const_cast<nsAttrValue*>(this)->SetMiscAtomOrString(&aResult);

      break;
    }
    case eDoubleValue:
    {
      aResult.Truncate();
      aResult.AppendFloat(GetDoubleValue());
      break;
    }
    case eSVGAngle:
    {
      SVGAttrValueWrapper::ToString(GetMiscContainer()->mSVGAngle, aResult);
      break;
    }
    case eSVGIntegerPair:
    {
      SVGAttrValueWrapper::ToString(GetMiscContainer()->mSVGIntegerPair,
                                    aResult);
      break;
    }
    case eSVGLength:
    {
      SVGAttrValueWrapper::ToString(GetMiscContainer()->mSVGLength, aResult);
      break;
    }
    case eSVGLengthList:
    {
      SVGAttrValueWrapper::ToString(GetMiscContainer()->mSVGLengthList,
                                    aResult);
      break;
    }
    case eSVGNumberList:
    {
      SVGAttrValueWrapper::ToString(GetMiscContainer()->mSVGNumberList,
                                    aResult);
      break;
    }
    case eSVGNumberPair:
    {
      SVGAttrValueWrapper::ToString(GetMiscContainer()->mSVGNumberPair,
                                    aResult);
      break;
    }
    case eSVGPathData:
    {
      SVGAttrValueWrapper::ToString(GetMiscContainer()->mSVGPathData, aResult);
      break;
    }
    case eSVGPointList:
    {
      SVGAttrValueWrapper::ToString(GetMiscContainer()->mSVGPointList, aResult);
      break;
    }
    case eSVGPreserveAspectRatio:
    {
      SVGAttrValueWrapper::ToString(GetMiscContainer()->mSVGPreserveAspectRatio,
                                    aResult);
      break;
    }
    case eSVGStringList:
    {
      SVGAttrValueWrapper::ToString(GetMiscContainer()->mSVGStringList,
                                    aResult);
      break;
    }
    case eSVGTransformList:
    {
      SVGAttrValueWrapper::ToString(GetMiscContainer()->mSVGTransformList,
                                    aResult);
      break;
    }
    case eSVGViewBox:
    {
      SVGAttrValueWrapper::ToString(GetMiscContainer()->mSVGViewBox, aResult);
      break;
    }
    default:
    {
      aResult.Truncate();
      break;
    }
  }
}

already_AddRefed<nsIAtom>
nsAttrValue::GetAsAtom() const
{
  switch (Type()) {
    case eString:
      return do_GetAtom(GetStringValue());

    case eAtom:
      {
        nsIAtom* atom = GetAtomValue();
        NS_ADDREF(atom);
        return atom;
      }

    default:
      {
        nsAutoString val;
        ToString(val);
        return do_GetAtom(val);
      }
  }
}

const nsCheapString
nsAttrValue::GetStringValue() const
{
  NS_PRECONDITION(Type() == eString, "wrong type");

  return nsCheapString(static_cast<nsStringBuffer*>(GetPtr()));
}

bool
nsAttrValue::GetColorValue(nscolor& aColor) const
{
  if (Type() != eColor) {
    // Unparseable value, treat as unset.
    NS_ASSERTION(Type() == eString, "unexpected type for color-valued attr");
    return false;
  }

  aColor = GetMiscContainer()->mColor;
  return true;
}

void
nsAttrValue::GetEnumString(nsAString& aResult, bool aRealTag) const
{
  NS_PRECONDITION(Type() == eEnum, "wrong type");

  PRUint32 allEnumBits =
    (BaseType() == eIntegerBase) ? static_cast<PRUint32>(GetIntInternal())
                                   : GetMiscContainer()->mEnumValue;
  PRInt16 val = allEnumBits >> NS_ATTRVALUE_ENUMTABLEINDEX_BITS;
  const EnumTable* table = sEnumTableArray->
    ElementAt(allEnumBits & NS_ATTRVALUE_ENUMTABLEINDEX_MASK);

  while (table->tag) {
    if (table->value == val) {
      aResult.AssignASCII(table->tag);
      if (!aRealTag && allEnumBits & NS_ATTRVALUE_ENUMTABLE_VALUE_NEEDS_TO_UPPER) {
        ToUpperCase(aResult);
      }
      return;
    }
    table++;
  }

  NS_NOTREACHED("couldn't find value in EnumTable");
}

PRUint32
nsAttrValue::GetAtomCount() const
{
  ValueType type = Type();

  if (type == eAtom) {
    return 1;
  }

  if (type == eAtomArray) {
    return GetAtomArrayValue()->Length();
  }

  return 0;
}

nsIAtom*
nsAttrValue::AtomAt(PRInt32 aIndex) const
{
  NS_PRECONDITION(aIndex >= 0, "Index must not be negative");
  NS_PRECONDITION(GetAtomCount() > PRUint32(aIndex), "aIndex out of range");
  
  if (BaseType() == eAtomBase) {
    return GetAtomValue();
  }

  NS_ASSERTION(Type() == eAtomArray, "GetAtomCount must be confused");
  
  return GetAtomArrayValue()->ElementAt(aIndex);
}

PRUint32
nsAttrValue::HashValue() const
{
  switch(BaseType()) {
    case eStringBase:
    {
      nsStringBuffer* str = static_cast<nsStringBuffer*>(GetPtr());
      if (str) {
        PRUint32 len = str->StorageSize()/sizeof(PRUnichar) - 1;
        return nsCRT::HashCode(static_cast<PRUnichar*>(str->Data()), len);
      }

      return 0;
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
  if (static_cast<ValueBaseType>(cont->mStringBits & NS_ATTRVALUE_BASETYPE_MASK)
      == eAtomBase) {
    return cont->mStringBits - 0;
  }

  switch (cont->mType) {
    case eInteger:
    {
      return cont->mInteger;
    }
    case eEnum:
    {
      return cont->mEnumValue;
    }
    case ePercent:
    {
      return cont->mPercent;
    }
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
      PRUint32 count = cont->mAtomArray->Length();
      for (nsCOMPtr<nsIAtom> *cur = cont->mAtomArray->Elements(),
                             *end = cur + count;
           cur != end; ++cur) {
        retval ^= NS_PTR_TO_INT32(cur->get());
      }
      return retval;
    }
    case eDoubleValue:
    {
      // XXX this is crappy, but oh well
      return cont->mDoubleValue;
    }
    case eIntMarginValue:
    {
      return NS_PTR_TO_INT32(cont->mIntMargin);
    }
    default:
    {
      if (IsSVGType(cont->mType)) {
        // All SVG types are just pointers to classes so we can treat them alike
        return NS_PTR_TO_INT32(cont->mSVGAngle);
      }
      NS_NOTREACHED("unknown type stored in MiscContainer");
      return 0;
    }
  }
}

bool
nsAttrValue::Equals(const nsAttrValue& aOther) const
{
  if (BaseType() != aOther.BaseType()) {
    return false;
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
    return false;
  }

  bool needsStringComparison = false;

  switch (thisCont->mType) {
    case eInteger:
    {
      if (thisCont->mInteger == otherCont->mInteger) {
        needsStringComparison = true;
      }
      break;
    }
    case eEnum:
    {
      if (thisCont->mEnumValue == otherCont->mEnumValue) {
        needsStringComparison = true;
      }
      break;
    }
    case ePercent:
    {
      if (thisCont->mPercent == otherCont->mPercent) {
        needsStringComparison = true;
      }
      break;
    }
    case eColor:
    {
      if (thisCont->mColor == otherCont->mColor) {
        needsStringComparison = true;
      }
      break;
    }
    case eCSSStyleRule:
    {
      return thisCont->mCSSStyleRule == otherCont->mCSSStyleRule;
    }
    case eAtomArray:
    {
      // For classlists we could be insensitive to order, however
      // classlists are never mapped attributes so they are never compared.

      if (!(*thisCont->mAtomArray == *otherCont->mAtomArray)) {
        return false;
      }

      needsStringComparison = true;
      break;
    }
    case eDoubleValue:
    {
      return thisCont->mDoubleValue == otherCont->mDoubleValue;
    }
    case eIntMarginValue:
    {
      return thisCont->mIntMargin == otherCont->mIntMargin;
    }
    default:
    {
      if (IsSVGType(thisCont->mType)) {
        // Currently this method is never called for nsAttrValue objects that
        // point to SVG data types.
        // If that changes then we probably want to add methods to the
        // corresponding SVG types to compare their base values.
        // As a shortcut, however, we can begin by comparing the pointers.
        NS_ABORT_IF_FALSE(false, "Comparing nsAttrValues that point to SVG "
          "data");
        return false;
      }
      NS_NOTREACHED("unknown type stored in MiscContainer");
      return false;
    }
  }
  if (needsStringComparison) {
    if (thisCont->mStringBits == otherCont->mStringBits) {
      return true;
    }
    if ((static_cast<ValueBaseType>(thisCont->mStringBits & NS_ATTRVALUE_BASETYPE_MASK) ==
         eStringBase) &&
        (static_cast<ValueBaseType>(otherCont->mStringBits & NS_ATTRVALUE_BASETYPE_MASK) ==
         eStringBase)) {
      return nsCheapString(reinterpret_cast<nsStringBuffer*>(thisCont->mStringBits)).Equals(
        nsCheapString(reinterpret_cast<nsStringBuffer*>(otherCont->mStringBits)));
    }
  }
  return false;
}

bool
nsAttrValue::Equals(const nsAString& aValue,
                    nsCaseTreatment aCaseSensitive) const
{
  switch (BaseType()) {
    case eStringBase:
    {
      nsStringBuffer* str = static_cast<nsStringBuffer*>(GetPtr());
      if (str) {
        nsDependentString dep(static_cast<PRUnichar*>(str->Data()),
                              str->StorageSize()/sizeof(PRUnichar) - 1);
        return aCaseSensitive == eCaseMatters ? aValue.Equals(dep) :
          aValue.Equals(dep, nsCaseInsensitiveStringComparator());
      }
      return aValue.IsEmpty();
    }
    case eAtomBase:
      if (aCaseSensitive == eCaseMatters) {
        return static_cast<nsIAtom*>(GetPtr())->Equals(aValue);
      }
      return nsDependentAtomString(static_cast<nsIAtom*>(GetPtr())).
        Equals(aValue, nsCaseInsensitiveStringComparator());
    default:
      break;
  }

  nsAutoString val;
  ToString(val);
  return aCaseSensitive == eCaseMatters ? val.Equals(aValue) :
    val.Equals(aValue, nsCaseInsensitiveStringComparator());
}

bool
nsAttrValue::Equals(nsIAtom* aValue, nsCaseTreatment aCaseSensitive) const
{
  if (aCaseSensitive != eCaseMatters) {
    // Need a better way to handle this!
    nsAutoString value;
    aValue->ToString(value);
    return Equals(value, aCaseSensitive);
  }
  
  switch (BaseType()) {
    case eStringBase:
    {
      nsStringBuffer* str = static_cast<nsStringBuffer*>(GetPtr());
      if (str) {
        nsDependentString dep(static_cast<PRUnichar*>(str->Data()),
                              str->StorageSize()/sizeof(PRUnichar) - 1);
        return aValue->Equals(dep);
      }
      return aValue == nsGkAtoms::_empty;
    }
    case eAtomBase:
    {
      return static_cast<nsIAtom*>(GetPtr()) == aValue;
    }
    default:
      break;
  }

  nsAutoString val;
  ToString(val);
  return aValue->Equals(val);
}

bool
nsAttrValue::EqualsAsStrings(const nsAttrValue& aOther) const
{
  if (Type() == aOther.Type()) {
    return Equals(aOther);
  }

  // We need to serialize at least one nsAttrValue before passing to
  // Equals(const nsAString&), but we can avoid unnecessarily serializing both
  // by checking if one is already of a string type.
  bool thisIsString = (BaseType() == eStringBase || BaseType() == eAtomBase);
  const nsAttrValue& lhs = thisIsString ? *this : aOther;
  const nsAttrValue& rhs = thisIsString ? aOther : *this;

  switch (rhs.BaseType()) {
    case eAtomBase:
      return lhs.Equals(rhs.GetAtomValue(), eCaseMatters);

    case eStringBase:
      return lhs.Equals(rhs.GetStringValue(), eCaseMatters);

    default:
    {
      nsAutoString val;
      rhs.ToString(val);
      return lhs.Equals(val, eCaseMatters);
    }
  }
}

bool
nsAttrValue::Contains(nsIAtom* aValue, nsCaseTreatment aCaseSensitive) const
{
  switch (BaseType()) {
    case eAtomBase:
    {
      nsIAtom* atom = GetAtomValue();

      if (aCaseSensitive == eCaseMatters) {
        return aValue == atom;
      }

      // For performance reasons, don't do a full on unicode case insensitive
      // string comparison. This is only used for quirks mode anyway.
      return
        nsContentUtils::EqualsIgnoreASCIICase(nsDependentAtomString(aValue),
                                              nsDependentAtomString(atom));
    }
    default:
    {
      if (Type() == eAtomArray) {
        AtomArray* array = GetAtomArrayValue();
        if (aCaseSensitive == eCaseMatters) {
          return array->Contains(aValue);
        }

        nsDependentAtomString val1(aValue);

        for (nsCOMPtr<nsIAtom> *cur = array->Elements(),
                               *end = cur + array->Length();
             cur != end; ++cur) {
          // For performance reasons, don't do a full on unicode case
          // insensitive string comparison. This is only used for quirks mode
          // anyway.
          if (nsContentUtils::EqualsIgnoreASCIICase(val1,
                nsDependentAtomString(*cur))) {
            return true;
          }
        }
      }
    }
  }

  return false;
}

struct AtomArrayStringComparator {
  bool Equals(nsIAtom* atom, const nsAString& string) const {
    return atom->Equals(string);
  }
};

bool
nsAttrValue::Contains(const nsAString& aValue) const
{
  switch (BaseType()) {
    case eAtomBase:
    {
      nsIAtom* atom = GetAtomValue();
      return atom->Equals(aValue);
    }
    default:
    {
      if (Type() == eAtomArray) {
        AtomArray* array = GetAtomArrayValue();
        return array->Contains(aValue, AtomArrayStringComparator());
      }
    }
  }

  return false;
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
  bool hasSpace = false;
  
  // skip initial whitespace
  while (iter != end && nsContentUtils::IsHTMLWhitespace(*iter)) {
    hasSpace = true;
    ++iter;
  }

  if (iter == end) {
    SetTo(aValue);
    return;
  }

  nsAString::const_iterator start(iter);

  // get first - and often only - atom
  do {
    ++iter;
  } while (iter != end && !nsContentUtils::IsHTMLWhitespace(*iter));

  nsCOMPtr<nsIAtom> classAtom = do_GetAtom(Substring(start, iter));
  if (!classAtom) {
    Reset();
    return;
  }

  // skip whitespace
  while (iter != end && nsContentUtils::IsHTMLWhitespace(*iter)) {
    hasSpace = true;
    ++iter;
  }

  if (iter == end && !hasSpace) {
    // we only found one classname and there was no whitespace so
    // don't bother storing a list
    ResetIfSet();
    nsIAtom* atom = nsnull;
    classAtom.swap(atom);
    SetPtrValueAndType(atom, eAtomBase);
    return;
  }

  if (!EnsureEmptyAtomArray()) {
    return;
  }

  AtomArray* array = GetAtomArrayValue();
  
  if (!array->AppendElement(classAtom)) {
    Reset();
    return;
  }

  // parse the rest of the classnames
  while (iter != end) {
    start = iter;

    do {
      ++iter;
    } while (iter != end && !nsContentUtils::IsHTMLWhitespace(*iter));

    classAtom = do_GetAtom(Substring(start, iter));

    if (!array->AppendElement(classAtom)) {
      Reset();
      return;
    }

    // skip whitespace
    while (iter != end && nsContentUtils::IsHTMLWhitespace(*iter)) {
      ++iter;
    }
  }

  SetMiscAtomOrString(&aValue);
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

void
nsAttrValue::SetIntValueAndType(PRInt32 aValue, ValueType aType,
                                const nsAString* aStringValue)
{
  if (aStringValue || aValue > NS_ATTRVALUE_INTEGERTYPE_MAXVALUE ||
      aValue < NS_ATTRVALUE_INTEGERTYPE_MINVALUE) {
    if (EnsureEmptyMiscContainer()) {
      MiscContainer* cont = GetMiscContainer();
      switch (aType) {
        case eInteger:
        {
          cont->mInteger = aValue;
          break;
        }
        case ePercent:
        {
          cont->mPercent = aValue;
          break;
        }
        case eEnum:
        {
          cont->mEnumValue = aValue;
          break;
        }
        default:
        {
          NS_NOTREACHED("unknown integer type");
          break;
        }
      }
      cont->mType = aType;
      SetMiscAtomOrString(aStringValue);
    }
  } else {
    NS_ASSERTION(!mBits, "Reset before calling SetIntValueAndType!");
    mBits = (aValue * NS_ATTRVALUE_INTEGERTYPE_MULTIPLIER) | aType;
  }
}

PRInt16
nsAttrValue::GetEnumTableIndex(const EnumTable* aTable)
{
  PRInt16 index = sEnumTableArray->IndexOf(aTable);
  if (index < 0) {
    index = sEnumTableArray->Length();
    NS_ASSERTION(index <= NS_ATTRVALUE_ENUMTABLEINDEX_MAXVALUE,
        "too many enum tables");
    sEnumTableArray->AppendElement(aTable);
  }

  return index;
}

PRInt32
nsAttrValue::EnumTableEntryToValue(const EnumTable* aEnumTable,
                                   const EnumTable* aTableEntry)
{
  PRInt16 index = GetEnumTableIndex(aEnumTable);
  PRInt32 value = (aTableEntry->value << NS_ATTRVALUE_ENUMTABLEINDEX_BITS) +
                  index;
  return value;
}

bool
nsAttrValue::ParseEnumValue(const nsAString& aValue,
                            const EnumTable* aTable,
                            bool aCaseSensitive,
                            const EnumTable* aDefaultValue)
{
  ResetIfSet();
  const EnumTable* tableEntry = aTable;

  while (tableEntry->tag) {
    if (aCaseSensitive ? aValue.EqualsASCII(tableEntry->tag) :
                         aValue.LowerCaseEqualsASCII(tableEntry->tag)) {
      PRInt32 value = EnumTableEntryToValue(aTable, tableEntry);

      bool equals = aCaseSensitive || aValue.EqualsASCII(tableEntry->tag);
      if (!equals) {
        nsAutoString tag;
        tag.AssignASCII(tableEntry->tag);
        ToUpperCase(tag);
        if ((equals = tag.Equals(aValue))) {
          value |= NS_ATTRVALUE_ENUMTABLE_VALUE_NEEDS_TO_UPPER;
        }
      }
      SetIntValueAndType(value, eEnum, equals ? nsnull : &aValue);
      NS_ASSERTION(GetEnumValue() == tableEntry->value,
                   "failed to store enum properly");

      return true;
    }
    tableEntry++;
  }

  if (aDefaultValue) {
    NS_PRECONDITION(aTable <= aDefaultValue && aDefaultValue < tableEntry,
                    "aDefaultValue not inside aTable?");
    SetIntValueAndType(EnumTableEntryToValue(aTable, aDefaultValue),
                       eEnum, &aValue);
    return true;
  }

  return false;
}

bool
nsAttrValue::ParseSpecialIntValue(const nsAString& aString)
{
  ResetIfSet();

  PRInt32 ec;
  bool strict;
  bool isPercent = false;
  nsAutoString tmp(aString);
  PRInt32 originalVal = StringToInteger(aString, &strict, &ec, true, &isPercent);

  if (NS_FAILED(ec)) {
    return false;
  }

  PRInt32 val = NS_MAX(originalVal, 0);

  // % (percent)
  if (isPercent || tmp.RFindChar('%') >= 0) {
    isPercent = true;
  }

  strict = strict && (originalVal == val);

  SetIntValueAndType(val,
                     isPercent ? ePercent : eInteger,
                     strict ? nsnull : &aString);
  return true;
}

bool
nsAttrValue::ParseIntWithBounds(const nsAString& aString,
                                PRInt32 aMin, PRInt32 aMax)
{
  NS_PRECONDITION(aMin < aMax, "bad boundaries");

  ResetIfSet();

  PRInt32 ec;
  bool strict;
  PRInt32 originalVal = StringToInteger(aString, &strict, &ec);
  if (NS_FAILED(ec)) {
    return false;
  }

  PRInt32 val = NS_MAX(originalVal, aMin);
  val = NS_MIN(val, aMax);
  strict = strict && (originalVal == val);
  SetIntValueAndType(val, eInteger, strict ? nsnull : &aString);

  return true;
}

bool
nsAttrValue::ParseNonNegativeIntValue(const nsAString& aString)
{
  ResetIfSet();

  PRInt32 ec;
  bool strict;
  PRInt32 originalVal = StringToInteger(aString, &strict, &ec);
  if (NS_FAILED(ec) || originalVal < 0) {
    return false;
  }

  SetIntValueAndType(originalVal, eInteger, strict ? nsnull : &aString);

  return true;
}

bool
nsAttrValue::ParsePositiveIntValue(const nsAString& aString)
{
  ResetIfSet();

  PRInt32 ec;
  bool strict;
  PRInt32 originalVal = StringToInteger(aString, &strict, &ec);
  if (NS_FAILED(ec) || originalVal <= 0) {
    return false;
  }

  SetIntValueAndType(originalVal, eInteger, strict ? nsnull : &aString);

  return true;
}

void
nsAttrValue::SetColorValue(nscolor aColor, const nsAString& aString)
{
  nsStringBuffer* buf = GetStringBuffer(aString);
  if (!buf) {
    return;
  }

  if (!EnsureEmptyMiscContainer()) {
    buf->Release();
    return;
  }

  MiscContainer* cont = GetMiscContainer();
  cont->mColor = aColor;
  cont->mType = eColor;

  // Save the literal string we were passed for round-tripping.
  cont->mStringBits = reinterpret_cast<PtrBits>(buf) | eStringBase;
}

bool
nsAttrValue::ParseColor(const nsAString& aString)
{
  ResetIfSet();

  // FIXME (partially, at least): HTML5's algorithm says we shouldn't do
  // the whitespace compression, trimming, or the test for emptiness.
  // (I'm a little skeptical that we shouldn't do the whitespace
  // trimming; WebKit also does it.)
  nsAutoString colorStr(aString);
  colorStr.CompressWhitespace(true, true);
  if (colorStr.IsEmpty()) {
    return false;
  }

  nscolor color;
  // No color names begin with a '#'; in standards mode, all acceptable
  // numeric colors do.
  if (colorStr.First() == '#') {
    nsDependentString withoutHash(colorStr.get() + 1, colorStr.Length() - 1);
    if (NS_HexToRGB(withoutHash, &color)) {
      SetColorValue(color, aString);
      return true;
    }
  } else {
    if (NS_ColorNameToRGB(colorStr, &color)) {
      SetColorValue(color, aString);
      return true;
    }
  }

  // FIXME (maybe): HTML5 says we should handle system colors.  This
  // means we probably need another storage type, since we'd need to
  // handle dynamic changes.  However, I think this is a bad idea:
  // http://lists.whatwg.org/pipermail/whatwg-whatwg.org/2010-May/026449.html

  // Use NS_LooseHexToRGB as a fallback if nothing above worked.
  if (NS_LooseHexToRGB(colorStr, &color)) {
    SetColorValue(color, aString);
    return true;
  }

  return false;
}

bool nsAttrValue::ParseDoubleValue(const nsAString& aString)
{
  ResetIfSet();

  PRInt32 ec;
  double val = PromiseFlatString(aString).ToDouble(&ec);
  if (NS_FAILED(ec)) {
    return false;
  }
  if (EnsureEmptyMiscContainer()) {
    MiscContainer* cont = GetMiscContainer();
    cont->mDoubleValue = val;
    cont->mType = eDoubleValue;
    nsAutoString serializedFloat;
    serializedFloat.AppendFloat(val);
    SetMiscAtomOrString(serializedFloat.Equals(aString) ? nsnull : &aString);
    return true;
  }

  return false;
}

bool
nsAttrValue::ParseIntMarginValue(const nsAString& aString)
{
  ResetIfSet();

  nsIntMargin margins;
  if (!nsContentUtils::ParseIntMarginValue(aString, margins))
    return false;

  if (EnsureEmptyMiscContainer()) {
    MiscContainer* cont = GetMiscContainer();
    cont->mIntMargin = new nsIntMargin(margins);
    cont->mType = eIntMarginValue;
    SetMiscAtomOrString(&aString);
    return true;
  }

  return false;
}

void
nsAttrValue::SetMiscAtomOrString(const nsAString* aValue)
{
  NS_ASSERTION(GetMiscContainer(), "Must have MiscContainer!");
  NS_ASSERTION(!GetMiscContainer()->mStringBits,
               "Trying to re-set atom or string!");
  if (aValue) {
    PRUint32 len = aValue->Length();
    // * We're allowing eCSSStyleRule attributes to store empty strings as it
    //   can be beneficial to store an empty style attribute as a parsed rule.
    // * We're allowing enumerated values because sometimes the empty
    //   string corresponds to a particular enumerated value, especially
    //   for enumerated values that are not limited enumerated.
    // Add other types as needed.
    NS_ASSERTION(len || Type() == eCSSStyleRule || Type() == eEnum,
                 "Empty string?");
    MiscContainer* cont = GetMiscContainer();
    if (len <= NS_ATTRVALUE_MAX_STRINGLENGTH_ATOM) {
      nsIAtom* atom = NS_NewAtom(*aValue);
      if (atom) {
        cont->mStringBits = reinterpret_cast<PtrBits>(atom) | eAtomBase;
      }
    } else {
      nsStringBuffer* buf = GetStringBuffer(*aValue);
      if (buf) {
        cont->mStringBits = reinterpret_cast<PtrBits>(buf) | eStringBase;
      }
    }
  }
}

void
nsAttrValue::ResetMiscAtomOrString()
{
  MiscContainer* cont = GetMiscContainer();
  void* ptr = MISC_STR_PTR(cont);
  if (ptr) {
    if (static_cast<ValueBaseType>(cont->mStringBits & NS_ATTRVALUE_BASETYPE_MASK) ==
        eStringBase) {
      static_cast<nsStringBuffer*>(ptr)->Release();
    } else {
      static_cast<nsIAtom*>(ptr)->Release();
    }
    cont->mStringBits = 0;
  }
}

void
nsAttrValue::SetSVGType(ValueType aType, const void* aValue,
                        const nsAString* aSerialized) {
  NS_ABORT_IF_FALSE(IsSVGType(aType), "Not an SVG type");
  if (EnsureEmptyMiscContainer()) {
    MiscContainer* cont = GetMiscContainer();
    // All SVG types are just pointers to classes so just setting any of them
    // will do. We'll lose type-safety but the signature of the calling
    // function should ensure we don't get anything unexpected, and once we
    // stick aValue in a union we lose type information anyway.
    cont->mSVGAngle = static_cast<const nsSVGAngle*>(aValue);
    cont->mType = aType;
    SetMiscAtomOrString(aSerialized);
  }
}

bool
nsAttrValue::EnsureEmptyMiscContainer()
{
  MiscContainer* cont;
  if (BaseType() == eOtherBase) {
    ResetMiscAtomOrString();
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
      case eIntMarginValue:
      {
        delete cont->mIntMargin;
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
    NS_ENSURE_TRUE(cont, false);

    SetPtrValueAndType(cont, eOtherBase);
  }

  cont->mType = eColor;
  cont->mStringBits = 0;
  cont->mColor = 0;

  return true;
}

bool
nsAttrValue::EnsureEmptyAtomArray()
{
  if (Type() == eAtomArray) {
    ResetMiscAtomOrString();
    GetAtomArrayValue()->Clear();
    return true;
  }

  if (!EnsureEmptyMiscContainer()) {
    // should already be reset
    return false;
  }

  AtomArray* array = new AtomArray;
  if (!array) {
    Reset();
    return false;
  }

  MiscContainer* cont = GetMiscContainer();
  cont->mAtomArray = array;
  cont->mType = eAtomArray;

  return true;
}

nsStringBuffer*
nsAttrValue::GetStringBuffer(const nsAString& aValue) const
{
  PRUint32 len = aValue.Length();
  if (!len) {
    return nsnull;
  }

  nsStringBuffer* buf = nsStringBuffer::FromString(aValue);
  if (buf && (buf->StorageSize()/sizeof(PRUnichar) - 1) == len) {
    buf->AddRef();
    return buf;
  }

  buf = nsStringBuffer::Alloc((len + 1) * sizeof(PRUnichar));
  if (!buf) {
    return nsnull;
  }
  PRUnichar *data = static_cast<PRUnichar*>(buf->Data());
  CopyUnicodeTo(aValue, 0, data, len);
  data[len] = PRUnichar(0);
  return buf;
}

PRInt32
nsAttrValue::StringToInteger(const nsAString& aValue, bool* aStrict,
                             PRInt32* aErrorCode,
                             bool aCanBePercent,
                             bool* aIsPercent) const
{
  *aStrict = true;
  *aErrorCode = NS_ERROR_ILLEGAL_VALUE;
  if (aCanBePercent) {
    *aIsPercent = false;
  }

  nsAString::const_iterator iter, end;
  aValue.BeginReading(iter);
  aValue.EndReading(end);

  while (iter != end && nsContentUtils::IsHTMLWhitespace(*iter)) {
    *aStrict = false;
    ++iter;
  }

  if (iter == end) {
    return 0;
  }

  bool negate = false;
  if (*iter == PRUnichar('-')) {
    negate = true;
    ++iter;
  } else if (*iter == PRUnichar('+')) {
    *aStrict = false;
    ++iter;
  }

  PRInt32 value = 0;
  PRInt32 pValue = 0; // Previous value, used to check integer overflow
  while (iter != end) {
    if (*iter >= PRUnichar('0') && *iter <= PRUnichar('9')) {
      value = (value * 10) + (*iter - PRUnichar('0'));
      ++iter;
      // Checking for integer overflow.
      if (pValue > value) {
        *aStrict = false;
        *aErrorCode = NS_ERROR_ILLEGAL_VALUE;
        break;
      } else {
        pValue = value;
        *aErrorCode = NS_OK;
      }
    } else if (aCanBePercent && *iter == PRUnichar('%')) {
      ++iter;
      *aIsPercent = true;
      if (iter != end) {
        *aStrict = false;
        break;
      }
    } else {
      *aStrict = false;
      break;
    }
  }
  if (negate) {
    value = -value;
    // Checking the special case of -0.
    if (!value) {
      *aStrict = false;
    }
  }

  return value;
}

PRInt64
nsAttrValue::SizeOf() const
{
  PRInt64 size = sizeof(*this);

  switch (BaseType()) {
    case eStringBase:
    {
      // TODO: we might be counting the string size more than once.
      // This should be fixed with bug 677487.
      nsStringBuffer* str = static_cast<nsStringBuffer*>(GetPtr());
      size += str ? str->StorageSize() : 0;
      break;
    }
    case eOtherBase:
    {
      MiscContainer* container = GetMiscContainer();

      if (!container) {
        break;
      }

      size += sizeof(*container);

      void* otherPtr = MISC_STR_PTR(container);
      // We only count the size of the object pointed by otherPtr if it's a
      // string. When it's an atom, it's counted separatly.
      if (otherPtr &&
          static_cast<ValueBaseType>(container->mStringBits & NS_ATTRVALUE_BASETYPE_MASK) == eStringBase) {
        // TODO: we might be counting the string size more than once.
        // This should be fixed with bug 677487.
        nsStringBuffer* str = static_cast<nsStringBuffer*>(otherPtr);
        size += str ? str->StorageSize() : 0;
      }

      // TODO: mCSSStyleRule might be owned by another object
      // which would make us count them twice, bug 677493.
      if (Type() == eCSSStyleRule && container->mCSSStyleRule) {
        // TODO: Add SizeOf() to StyleRule, bug 677503.
        size += sizeof(*container->mCSSStyleRule);
      } else if (Type() == eAtomArray && container->mAtomArray) {
        size += sizeof(container->mAtomArray) + sizeof(nsTArrayHeader);
        size += container->mAtomArray->Capacity() * sizeof(nsCOMPtr<nsIAtom>);
        // Don't count the size of each nsIAtom, they are counted separatly.
      }

      break;
    }
    case eAtomBase:    // Atoms are counted separatly.
    case eIntegerBase: // The value is in mBits, nothing to do.
      break;
  }

  return size;
}

