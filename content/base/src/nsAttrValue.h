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

#ifndef nsAttrValue_h___
#define nsAttrValue_h___

#include "nscore.h"
#include "nsDependentSubstring.h"

typedef unsigned long PtrBits;
class nsHTMLValue;
class nsAString;
class nsIAtom;

#define NS_ATTRVALUE_MAX_STRINGLENGTH_ATOM 12
#define NS_ATTRVALUE_TYPE_MASK (PtrBits(3))
#define NS_ATTRVALUE_VALUE_MASK (~NS_ATTRVALUE_TYPE_MASK)

class nsAttrValue {
public:
  nsAttrValue();
  nsAttrValue(const nsAttrValue& aOther);
  explicit nsAttrValue(const nsAString& aValue);
  explicit nsAttrValue(const nsHTMLValue& aValue);
  explicit nsAttrValue(nsIAtom* aValue);
  ~nsAttrValue();

  void Reset();
  void SetTo(const nsAttrValue& aOther);
  void SetTo(const nsAString& aValue);
  void SetTo(const nsHTMLValue& aValue);
  void SetTo(nsIAtom* aValue);
  void SetToStringOrAtom(const nsAString& aValue);

  void SwapValueWith(nsAttrValue& aOther);

  void ToString(nsAString& aResult) const;

  // Methods to get value. These methods do not convert so only use them
  // to retrieve the datatype that this nsAttrValue has.
  const nsDependentSubstring GetStringValue() const;
  const nsHTMLValue* GetHTMLValue() const;
  nsIAtom* GetAtomValue() const;

  PRUint32 HashValue() const;
  PRBool Equals(const nsAttrValue& aOther) const;

  enum Type {
    eString = 0,
    eHTMLValue = 1,  // This should eventually die
    eAtom = 2
  };

  Type GetType() const
  {
    return NS_STATIC_CAST(Type, mBits & NS_ATTRVALUE_TYPE_MASK);
  }

private:
  void* GetPtr() const
  {
    return NS_REINTERPRET_CAST(void*, mBits & NS_ATTRVALUE_VALUE_MASK);
  }

  void SetValueAndType(void* aValue, Type aType)
  {
    mBits = NS_REINTERPRET_CAST(PtrBits, aValue) | aType;
  }

  PtrBits mBits;
};

#endif
