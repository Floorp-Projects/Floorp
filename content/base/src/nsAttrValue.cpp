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

nsAttrValue::nsAttrValue(const nsHTMLValue& aValue)
    : mBits(0)
{
  SetTo(aValue);
}

nsAttrValue::nsAttrValue(nsIAtom* aValue)
    : mBits(0)
{
  SetTo(aValue);
}

nsAttrValue::~nsAttrValue()
{
  Reset();
}

void
nsAttrValue::Reset()
{
  void* ptr = GetPtr();
  switch(GetType()) {
    case eString:
    {
      PRUnichar* str = NS_STATIC_CAST(PRUnichar*, ptr);
      if (str) {
        nsCheapStringBufferUtils::Free(str);
      }
      break;
    }
    case eHTMLValue:
    {
      delete NS_STATIC_CAST(nsHTMLValue*, ptr);
      break;
    }
    case eAtom:
    {
      nsIAtom* atom = NS_STATIC_CAST(nsIAtom*, ptr);
      NS_RELEASE(atom);
      break;
    }
  }

  mBits = 0;
}

void
nsAttrValue::SetTo(const nsAttrValue& aOther)
{
  switch (aOther.GetType()) {
    case eString:
    {
      SetTo(aOther.GetStringValue());
      break;
    }
    case eHTMLValue:
    {
      SetTo(*aOther.GetHTMLValue());
      break;
    }
    case eAtom:
    {
      SetTo(aOther.GetAtomValue());
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
  if (GetType() == eString && (ptr = GetPtr())) {
    if (!empty) {
      nsCheapStringBufferUtils::
        CopyToExistingBuffer(str, NS_STATIC_CAST(PRUnichar*, ptr), aValue);
    }
    else {
      nsCheapStringBufferUtils::Free(NS_STATIC_CAST(PRUnichar*, ptr));
    }
  }
  else {
    Reset();
    if (!empty) {
      nsCheapStringBufferUtils::CopyToBuffer(str, aValue);
    }
  }
  SetValueAndType(str, eString);
}

void
nsAttrValue::SetTo(const nsHTMLValue& aValue)
{
  Reset();
  nsHTMLValue* htmlValue = new nsHTMLValue(aValue);
  if (htmlValue) {
      SetValueAndType(htmlValue, eHTMLValue);
  }
}

void
nsAttrValue::SetTo(nsIAtom* aValue)
{
  NS_ASSERTION(aValue, "null value in nsAttrValue::SetTo");
  Reset();
  NS_ADDREF(aValue);
  SetValueAndType(aValue, eAtom);
}

void
nsAttrValue::SetToStringOrAtom(const nsAString& aValue)
{
  PRUint32 len = aValue.Length();
  // Don't bother with atoms if it's an empty string since
  // we can store those efficently anyway.
  if (len && len <= NS_ATTRVALUE_MAX_STRINGLENGTH_ATOM) {
    nsCOMPtr<nsIAtom> atom = do_GetAtom(aValue);
    if (atom) {
      SetTo(atom);
    }
  }
  else {
    SetTo(aValue);
  }
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
  void* ptr = GetPtr();

  switch(GetType()) {
    case eString:
    {
      PRUnichar* str = NS_STATIC_CAST(PRUnichar*, ptr);
      if (str) {
        aResult = nsCheapStringBufferUtils::GetDependentString(str);
      }
      else {
        aResult.Truncate();
      }
      break;
    }
    case eHTMLValue:
    {
      NS_STATIC_CAST(nsHTMLValue*, ptr)->ToString(aResult);
      break;
    }
    case eAtom:
    {
      NS_STATIC_CAST(nsIAtom*, ptr)->ToString(aResult);
      break;
    }
  }
}

const nsDependentSingleFragmentSubstring
nsAttrValue::GetStringValue() const
{
  NS_PRECONDITION(GetType() == eString,
                  "Some dork called GetStringValue() on a non-string!");

  static const PRUnichar blankStr[] = { '\0' };
  void* ptr = GetPtr();
  return ptr
         ? nsCheapStringBufferUtils::GetDependentString(NS_STATIC_CAST(PRUnichar*, ptr))
         : Substring(blankStr, blankStr);
}

const nsHTMLValue*
nsAttrValue::GetHTMLValue() const
{
  NS_PRECONDITION(GetType() == eHTMLValue,
                  "Some dork called GetHTMLValue() on a non-htmlvalue!");
  return NS_REINTERPRET_CAST(nsHTMLValue*, mBits & NS_ATTRVALUE_VALUE_MASK);
}

nsIAtom*
nsAttrValue::GetAtomValue() const
{
  NS_PRECONDITION(GetType() == eAtom,
                  "Some dork called GetAtomValue() on a non-atomvalue!");
  return NS_REINTERPRET_CAST(nsIAtom*, mBits & NS_ATTRVALUE_VALUE_MASK);
}

PRUint32
nsAttrValue::HashValue() const
{
  void* ptr = GetPtr();
  switch(GetType()) {
    case eString:
    {
      PRUnichar* str = NS_STATIC_CAST(PRUnichar*, ptr);
      return str ? nsCheapStringBufferUtils::HashCode(str) : 0;
    }
    case eHTMLValue:
    {
      return NS_STATIC_CAST(nsHTMLValue*, ptr)->HashValue();
    }
    case eAtom:
    {
      return NS_PTR_TO_INT32(ptr);
    }
  }

  NS_NOTREACHED("unknown attrvalue type");
  return 0;
}

PRBool
nsAttrValue::Equals(const nsAttrValue& aOther) const
{
  if (GetType() != aOther.GetType()) {
    return PR_FALSE;
  }

  switch(GetType()) {
    case eString:
    {
      return GetStringValue().Equals(aOther.GetStringValue());
    }
    case eHTMLValue:
    {
      return *GetHTMLValue() == *aOther.GetHTMLValue();
    }
    case eAtom:
    {
      return GetAtomValue() == aOther.GetAtomValue();
    }
  }

  NS_NOTREACHED("unknown attrvalue type");
  return PR_FALSE;
}
