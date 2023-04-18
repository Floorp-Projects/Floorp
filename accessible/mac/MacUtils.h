/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MacUtils_H_
#define _MacUtils_H_

#include "nsStringFwd.h"
#include "mozAccessible.h"
#include "MOXAccessibleBase.h"

@class NSString;
@class mozAccessible;

namespace mozilla {
namespace a11y {
namespace utils {

// convert an array of Gecko accessibles to an NSArray of native accessibles
template <typename AccArray>
NSArray<mozAccessible*>* ConvertToNSArray(AccArray& aArray) {
  NSMutableArray* nativeArray = [[[NSMutableArray alloc] init] autorelease];

  // iterate through the list, and get each native accessible.
  for (Accessible* curAccessible : aArray) {
    mozAccessible* curNative = GetNativeFromGeckoAccessible(curAccessible);
    if (curNative)
      [nativeArray addObject:GetObjectOrRepresentedView(curNative)];
  }

  return nativeArray;
}

/**
 * Get a localized string from the string bundle.
 * Return nil if not found.
 */
NSString* LocalizedString(const nsString& aString);

/**
 * Gets an accessible atttribute from the mozAccessible's associated
 * accessible wrapper or proxy, and returns the value as an NSString.
 * nil if no attribute is found.
 */
NSString* GetAccAttr(mozAccessible* aNativeAccessible, nsAtom* aAttrName);

/**
 * Return true if the passed raw pointer is a live document accessible. Uses
 * the provided root doc accessible to check for current documents.
 */
bool DocumentExists(Accessible* aDoc, uintptr_t aDocPtr);

NSDictionary* StringAttributesFromAccAttributes(AccAttributes* aAttributes,
                                                Accessible* aContainer);
}  // namespace utils
}  // namespace a11y
}  // namespace mozilla

#endif
