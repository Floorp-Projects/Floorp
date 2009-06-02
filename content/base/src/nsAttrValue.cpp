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
#include "nsICSSStyleRule.h"
#include "nsCSSDeclaration.h"
#include "nsIHTMLDocument.h"
#include "nsIDocument.h"
#include "nsTPtrArray.h"
#include "nsContentUtils.h"
#include "nsReadableUtils.h"
#include "nsIURI.h"
#include "prprf.h"
#ifdef MOZ_SVG
#include "nsISVGValue.h"
#endif

#define MISC_STR_PTR(_cont) \
  reinterpret_cast<void*>((_cont)->mStringBits & NS_ATTRVALUE_POINTERVALUE_MASK)

nsTPtrArray<const nsAttrValue::EnumTable>* nsAttrValue::sEnumTableArray = nsnull;

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

/* static */
nsresult
nsAttrValue::Init()
{
  NS_ASSERTION(!sEnumTableArray, "nsAttrValue already initialized");

  sEnumTableArray = new nsTPtrArray<const EnumTable>;
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
          !GetAtomArrayValue()->AppendObjects(*otherCont->mAtomArray)) {
        Reset();
        return;
      }
      break;
    }
#ifdef MOZ_SVG
    case eSVGValue:
    {
      NS_ADDREF(cont->mSVGValue = otherCont->mSVGValue);
      break;
    }
#endif
    case eFloatValue:
    {
      cont->mFloatValue = otherCont->mFloatValue;
      break;
    }
    case eLazyURIValue:
    {
      NS_IF_ADDREF(cont->mURI = otherCont->mURI);
      break;
    }
    default:
    {
      NS_NOTREACHED("unknown type stored in MiscContainer");
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
nsAttrValue::SetTo(PRInt16 aInt)
{
  ResetIfSet();
  SetIntValueAndType(aInt, eInteger, nsnull);
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
    case eColor:
    {
      nscolor v;
      GetColorValue(v);
      if (NS_GET_A(v) == 255) {
        char buf[10];
        PR_snprintf(buf, sizeof(buf), "#%02x%02x%02x",
                    NS_GET_R(v), NS_GET_G(v), NS_GET_B(v));
        CopyASCIItoUTF16(buf, aResult);
      } else if (v == NS_RGBA(0,0,0,0)) {
        aResult.AssignLiteral("transparent");
      } else {
        NS_NOTREACHED("translucent color attribute cannot be stringified");
        aResult.Truncate();
      }
      break;
    }
    case eEnum:
    {
      PRInt16 val = GetEnumValue();
      PRUint32 allEnumBits =
        cont ? cont->mEnumValue : static_cast<PRUint32>(GetIntInternal());
      const EnumTable* table = sEnumTableArray->
        ElementAt(allEnumBits & NS_ATTRVALUE_ENUMTABLEINDEX_MASK);
      while (table->tag) {
        if (table->value == val) {
          aResult.AssignASCII(table->tag);
          if (allEnumBits & NS_ATTRVALUE_ENUMTABLE_VALUE_NEEDS_TO_UPPER) {
            ToUpperCase(aResult);
          }
          return;
        }
        table++;
      }

      NS_NOTREACHED("couldn't find value in EnumTable");

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
      nsCSSDeclaration* decl = container->mCSSStyleRule->GetDeclaration();
      if (decl) {
        decl->ToString(aResult);
      }

      break;
    }
#ifdef MOZ_SVG
    case eSVGValue:
    {
      GetMiscContainer()->mSVGValue->GetValueString(aResult);
      break;
    }
#endif
    case eFloatValue:
    {
      nsAutoString str;
      str.AppendFloat(GetFloatValue());
      aResult = str;
      break;
    }
    // No need to do for eLazyURIValue, since that always stores the
    // original string.
#ifdef DEBUG
    case eLazyURIValue:
    {
      NS_NOTREACHED("Shouldn't get here");
      aResult.Truncate();
      break;
    }
#endif
    default:
    {
      aResult.Truncate();
      break;
    }
  }
}

const nsCheapString
nsAttrValue::GetStringValue() const
{
  NS_PRECONDITION(Type() == eString, "wrong type");

  return nsCheapString(static_cast<nsStringBuffer*>(GetPtr()));
}

PRBool
nsAttrValue::GetColorValue(nscolor& aColor) const
{
  NS_PRECONDITION(Type() == eColor || Type() == eString, "wrong type");
  switch (Type()) {
    case eString:
    {
      return GetPtr() && NS_ColorNameToRGB(GetStringValue(), &aColor);
    }
    case eColor:
    {
      aColor = GetMiscContainer()->mColor;
      
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
      nsStringBuffer* str = static_cast<nsStringBuffer*>(GetPtr());
      if (str) {
        PRUint32 len = str->StorageSize()/sizeof(PRUnichar) - 1;
        return nsCRT::BufferHashCode(static_cast<PRUnichar*>(str->Data()), len);
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
    case eFloatValue:
    {
      // XXX this is crappy, but oh well
      return cont->mFloatValue;
    }
    case eLazyURIValue:
    {
      NS_ASSERTION(static_cast<ValueBaseType>(cont->mStringBits &
                                              NS_ATTRVALUE_BASETYPE_MASK) ==
                   eStringBase,
                   "Unexpected type");
      nsStringBuffer* str = static_cast<nsStringBuffer*>(MISC_STR_PTR(cont));
      NS_ASSERTION(str, "How did that happen?");
      PRUint32 len = str->StorageSize()/sizeof(PRUnichar) - 1;
      return nsCRT::BufferHashCode(static_cast<PRUnichar*>(str->Data()), len);
    }
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

  PRBool needsStringComparison = PR_FALSE;
  
  switch (thisCont->mType) {
    case eInteger:
    {
      if (thisCont->mInteger == otherCont->mInteger) {
        needsStringComparison = PR_TRUE;
      }
      break;
    }
    case eEnum:
    {
      if (thisCont->mEnumValue == otherCont->mEnumValue) {
        needsStringComparison = PR_TRUE;
      }
      break;
    }
    case ePercent:
    {
      if (thisCont->mPercent == otherCont->mPercent) {
        needsStringComparison = PR_TRUE;
      }
      break;
    }
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
      needsStringComparison = PR_TRUE;
      break;
    }
#ifdef MOZ_SVG
    case eSVGValue:
    {
      return thisCont->mSVGValue == otherCont->mSVGValue;
    }
#endif
    case eFloatValue:
    {
      return thisCont->mFloatValue == otherCont->mFloatValue;
    }
    case eLazyURIValue:
    {
      needsStringComparison = PR_TRUE;
      break;
    }
    default:
    {
      NS_NOTREACHED("unknown type stored in MiscContainer");
      return PR_FALSE;
    }
  }
  if (needsStringComparison) {
    if (thisCont->mStringBits == otherCont->mStringBits) {
      return PR_TRUE;
    }
    if ((static_cast<ValueBaseType>(thisCont->mStringBits & NS_ATTRVALUE_BASETYPE_MASK) ==
         eStringBase) &&
        (static_cast<ValueBaseType>(otherCont->mStringBits & NS_ATTRVALUE_BASETYPE_MASK) ==
         eStringBase)) {
      return nsCheapString(reinterpret_cast<nsStringBuffer*>(thisCont->mStringBits)).Equals(
        nsCheapString(reinterpret_cast<nsStringBuffer*>(otherCont->mStringBits)));
    }
  }
  return PR_FALSE;
}

PRBool
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
      // Need a way to just do case-insensitive compares on atoms..
      if (aCaseSensitive == eCaseMatters) {
        return static_cast<nsIAtom*>(GetPtr())->Equals(aValue);;
      }
    default:
      break;
  }

  nsAutoString val;
  ToString(val);
  return aCaseSensitive == eCaseMatters ? val.Equals(aValue) :
    val.Equals(aValue, nsCaseInsensitiveStringComparator());
}

PRBool
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
      return aValue->EqualsUTF8(EmptyCString());
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

PRBool
nsAttrValue::Contains(nsIAtom* aValue, nsCaseTreatment aCaseSensitive) const
{
  switch (BaseType()) {
    case eAtomBase:
    {
      nsIAtom* atom = GetAtomValue();

      if (aCaseSensitive == eCaseMatters) {
        return aValue == atom;
      }

      const char *val1, *val2;
      aValue->GetUTF8String(&val1);
      atom->GetUTF8String(&val2);

      return nsCRT::strcasecmp(val1, val2) == 0;
    }
    default:
    {
      if (Type() == eAtomArray) {
        nsCOMArray<nsIAtom>* array = GetAtomArrayValue();
        if (aCaseSensitive == eCaseMatters) {
          return array->IndexOf(aValue) >= 0;
        }

        const char *val1, *val2;
        aValue->GetUTF8String(&val1);

        for (PRInt32 i = 0, count = array->Count(); i < count; ++i) {
          array->ObjectAt(i)->GetUTF8String(&val2);
          if (nsCRT::strcasecmp(val1, val2) == 0) {
            return PR_TRUE;
          }
        }
      }
    }
  }

  return PR_FALSE;
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
  PRBool hasSpace = PR_FALSE;
  
  // skip initial whitespace
  while (iter != end && nsContentUtils::IsHTMLWhitespace(*iter)) {
    hasSpace = PR_TRUE;
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
    hasSpace = PR_TRUE;
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

  nsCOMArray<nsIAtom>* array = GetAtomArrayValue();
  
  if (!array->AppendObject(classAtom)) {
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

    if (!array->AppendObject(classAtom)) {
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

PRBool
nsAttrValue::ParseEnumValue(const nsAString& aValue,
                            const EnumTable* aTable,
                            PRBool aCaseSensitive)
{
  ResetIfSet();

  while (aTable->tag) {
    if (aCaseSensitive ? aValue.EqualsASCII(aTable->tag) :
                         aValue.LowerCaseEqualsASCII(aTable->tag)) {

      // Find index of EnumTable
      PRInt16 index = sEnumTableArray->IndexOf(aTable);
      if (index < 0) {
        index = sEnumTableArray->Length();
        NS_ASSERTION(index <= NS_ATTRVALUE_ENUMTABLEINDEX_MAXVALUE,
                     "too many enum tables");
        if (!sEnumTableArray->AppendElement(aTable)) {
          return PR_FALSE;
        }
      }

      PRInt32 value = (aTable->value << NS_ATTRVALUE_ENUMTABLEINDEX_BITS) +
                      index;

      PRBool equals = aCaseSensitive || aValue.EqualsASCII(aTable->tag);
      if (!equals) {
        nsAutoString tag;
        tag.AssignASCII(aTable->tag);
        ToUpperCase(tag);
        if ((equals = tag.Equals(aValue))) {
          value |= NS_ATTRVALUE_ENUMTABLE_VALUE_NEEDS_TO_UPPER;
        }
      }
      SetIntValueAndType(value, eEnum, equals ? nsnull : &aValue);
      NS_ASSERTION(GetEnumValue() == aTable->value,
                   "failed to store enum properly");

      return PR_TRUE;
    }
    aTable++;
  }

  return PR_FALSE;
}

PRBool
nsAttrValue::ParseSpecialIntValue(const nsAString& aString,
                                  PRBool aCanBePercent)
{
  ResetIfSet();

  PRInt32 ec;
  PRBool strict;
  PRBool isPercent = PR_FALSE;
  nsAutoString tmp(aString);
  PRInt32 originalVal = StringToInteger(aString, &strict, &ec, aCanBePercent, &isPercent);

  if (NS_FAILED(ec)) {
    return PR_FALSE;
  }

  PRInt32 val = PR_MAX(originalVal, 0);

  // % (percent)
  // XXX RFindChar means that 5%x will be parsed!
  if (aCanBePercent && (isPercent || tmp.RFindChar('%') >= 0)) {
    if (val > 100) {
      val = 100;
    }
    isPercent = PR_TRUE;
  }

  strict = strict && (originalVal == val);

  SetIntValueAndType(val,
                     isPercent ? ePercent : eInteger,
                     strict ? nsnull : &aString);
  return PR_TRUE;
}

PRBool
nsAttrValue::ParseIntWithBounds(const nsAString& aString,
                                PRInt32 aMin, PRInt32 aMax)
{
  NS_PRECONDITION(aMin < aMax, "bad boundaries");

  ResetIfSet();

  PRInt32 ec;
  PRBool strict;
  PRInt32 originalVal = StringToInteger(aString, &strict, &ec);
  if (NS_FAILED(ec)) {
    return PR_FALSE;
  }

  PRInt32 val = PR_MAX(originalVal, aMin);
  val = PR_MIN(val, aMax);
  strict = strict && (originalVal == val);
  SetIntValueAndType(val, eInteger, strict ? nsnull : &aString);

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
  if (aDocument->GetCompatibilityMode() == eCompatibility_NavQuirks) {
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

  if (EnsureEmptyMiscContainer()) {
    MiscContainer* cont = GetMiscContainer();
    cont->mColor = color;
    cont->mType = eColor;
  }

  return PR_TRUE;
}

PRBool nsAttrValue::ParseFloatValue(const nsAString& aString)
{
  ResetIfSet();

  PRInt32 ec;
  float val = PromiseFlatString(aString).ToFloat(&ec);
  if (NS_FAILED(ec)) {
    return PR_FALSE;
  }
  if (EnsureEmptyMiscContainer()) {
    MiscContainer* cont = GetMiscContainer();
    cont->mFloatValue = val;
    cont->mType = eFloatValue;
    nsAutoString serializedFloat;
    serializedFloat.AppendFloat(val);
    SetMiscAtomOrString(serializedFloat.Equals(aString) ? nsnull : &aString);
    return PR_TRUE;
  }

  return PR_FALSE;
}

PRBool nsAttrValue::ParseLazyURIValue(const nsAString& aString)
{
  ResetIfSet();

  if (EnsureEmptyMiscContainer()) {
    MiscContainer* cont = GetMiscContainer();
    cont->mURI = nsnull;
    cont->mType = eLazyURIValue;

    // Don't use SetMiscAtomOrString because atomizing URIs is not
    // likely to do us much good.
    nsStringBuffer* buf = GetStringBuffer(aString);
    if (!buf) {
      return PR_FALSE;
    }
    cont->mStringBits = reinterpret_cast<PtrBits>(buf) | eStringBase;
    
    return PR_TRUE;
  }

  return PR_FALSE;
}

void
nsAttrValue::CacheURIValue(nsIURI* aURI)
{
  NS_PRECONDITION(Type() == eLazyURIValue, "wrong type");
  NS_PRECONDITION(!GetMiscContainer()->mURI, "Why are we being called?");
  NS_IF_ADDREF(GetMiscContainer()->mURI = aURI);
}

void
nsAttrValue::DropCachedURI()
{
  NS_PRECONDITION(Type() == eLazyURIValue, "wrong type");
  NS_IF_RELEASE(GetMiscContainer()->mURI);
}

const nsCheapString
nsAttrValue::GetURIStringValue() const
{
  NS_PRECONDITION(Type() == eLazyURIValue, "wrong type");
  NS_PRECONDITION(static_cast<ValueBaseType>(GetMiscContainer()->mStringBits &
                                             NS_ATTRVALUE_BASETYPE_MASK) ==
                  eStringBase,
                  "Unexpected type");
  NS_PRECONDITION(MISC_STR_PTR(GetMiscContainer()),
                  "Should have a string buffer here!");
  return nsCheapString(static_cast<nsStringBuffer*>
                                  (MISC_STR_PTR(GetMiscContainer())));
}

void
nsAttrValue::SetMiscAtomOrString(const nsAString* aValue)
{
  NS_ASSERTION(GetMiscContainer(), "Must have MiscContainer!");
  NS_ASSERTION(!GetMiscContainer()->mStringBits,
               "Trying to re-set atom or string!");
  if (aValue) {
    PRUint32 len = aValue->Length();
    NS_ASSERTION(len, "Empty string?");
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

PRBool
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
#ifdef MOZ_SVG
      case eSVGValue:
      {
        NS_RELEASE(cont->mSVGValue);
        break;
      }
#endif
      case eLazyURIValue:
      {
        NS_IF_RELEASE(cont->mURI);
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
  cont->mStringBits = 0;
  cont->mColor = 0;

  return PR_TRUE;
}

PRBool
nsAttrValue::EnsureEmptyAtomArray()
{
  if (Type() == eAtomArray) {
    ResetMiscAtomOrString();
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
nsAttrValue::StringToInteger(const nsAString& aValue, PRBool* aStrict,
                             PRInt32* aErrorCode,
                             PRBool aCanBePercent,
                             PRBool* aIsPercent) const
{
  *aStrict = PR_FALSE;
  *aErrorCode = NS_ERROR_ILLEGAL_VALUE;
  if (aCanBePercent) {
    *aIsPercent = PR_FALSE;
  }

  nsAString::const_iterator iter, end;
  aValue.BeginReading(iter);
  aValue.EndReading(end);
  PRBool negate = PR_FALSE;
  PRInt32 value = 0;
  if (iter != end) {
    if (*iter == PRUnichar('-')) {
      negate = PR_TRUE;
      ++iter;
    }
    if (iter != end) {
      if ((*iter >= PRUnichar('1') || (*iter == PRUnichar('0') && !negate)) &&
          *iter <= PRUnichar('9')) {
        value = *iter - PRUnichar('0');
        ++iter;
        *aStrict = (value != 0 || iter == end ||
                    (aCanBePercent && *iter == PRUnichar('%')));
        while (iter != end && *aStrict) {
          if (*iter >= PRUnichar('0') && *iter <= PRUnichar('9')) {
            value = (value * 10) + (*iter - PRUnichar('0'));
            ++iter;
            if (iter != end && value > ((PR_INT32_MAX / 10) - 9)) {
              *aStrict = PR_FALSE;
            }
          } else if (aCanBePercent && *iter == PRUnichar('%')) {
            ++iter;
            if (iter == end) {
              *aIsPercent = PR_TRUE;
            } else {
              *aStrict = PR_FALSE;
            }
          } else {
            *aStrict = PR_FALSE;
          }
        }
        if (*aStrict) {
          if (negate) {
            value = -value;
          }
          if (!aCanBePercent || !*aIsPercent) {
            *aErrorCode = NS_OK;
#ifdef DEBUG
            nsAutoString stringValue;
            stringValue.AppendInt(value);
            if (aCanBePercent && *aIsPercent) {
              stringValue.AppendLiteral("%");
            }
            NS_ASSERTION(stringValue.Equals(aValue), "Wrong conversion!");
#endif
            return value;
          }
        }
      }
    }
  }

  nsAutoString tmp(aValue);
  return tmp.ToInteger(aErrorCode);
}
