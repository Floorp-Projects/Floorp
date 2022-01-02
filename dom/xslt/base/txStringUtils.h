/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef txStringUtils_h__
#define txStringUtils_h__

#include "nsAString.h"
#include "nsAtom.h"
#include "nsUnicharUtils.h"
#include "nsContentUtils.h"  // For ASCIIToLower().

/**
 * Check equality between a string and an atom containing ASCII.
 */
inline bool TX_StringEqualsAtom(const nsAString& aString, nsAtom* aAtom) {
  return aAtom->Equals(aString);
}

inline already_AddRefed<nsAtom> TX_ToLowerCaseAtom(nsAtom* aAtom) {
  nsAutoString str;
  aAtom->ToString(str);
  nsContentUtils::ASCIIToLower(str);
  return NS_Atomize(str);
}

#endif  // txStringUtils_h__
