/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccAttributes.h"

using namespace mozilla::a11y;

void AccAttributes::SetAttribute(nsAtom* aAttrName,
                                 const nsAString& aAttrValue) {
  mData.InsertOrUpdate(aAttrName, aAttrValue);
}

void AccAttributes::SetAttribute(nsAtom* aAttrName, const nsAtom* aAttrValue) {
  nsAutoString value;
  aAttrValue->ToString(value);
  mData.InsertOrUpdate(aAttrName, value);
}

void AccAttributes::SetAttribute(const nsAString& aAttrName,
                                 const nsAString& aAttrValue) {
  RefPtr<nsAtom> attrAtom = NS_Atomize(aAttrName);
  mData.InsertOrUpdate(attrAtom, aAttrValue);
}

bool AccAttributes::GetAttribute(nsAtom* aAttrName, nsAString& aAttrValue) {
  if (auto value = mData.Lookup(aAttrName)) {
    aAttrValue.Assign(*value);
    return true;
  }

  return false;
}
