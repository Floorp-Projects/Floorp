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

nsAttrValue::nsAttrValue(const nsAString& aValue)
    : mBits(0)
{
  SetTo(aValue);
}

nsAttrValue::nsAttrValue(nsHTMLValue* aValue)
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
  }

  mBits = 0;
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
nsAttrValue::SetTo(nsHTMLValue* aValue)
{
  NS_ASSERTION(aValue, "null value in nsAttrValue::SetTo");
  Reset();
  SetValueAndType(aValue, eHTMLValue);
}

void
nsAttrValue::ToString(nsAString& aResult) const
{
  aResult.Truncate();
  void* ptr = GetPtr();

  switch(GetType()) {
    case eString:
    {
      PRUnichar* str = NS_STATIC_CAST(PRUnichar*, ptr);
      if (str) {
        aResult = nsCheapStringBufferUtils::GetDependentString(str);
      }
      break;
    }
    case eHTMLValue:
    {
      NS_STATIC_CAST(nsHTMLValue*, ptr)->ToString(aResult);
      break;
    }
  }
}
