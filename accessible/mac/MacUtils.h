/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MacUtils_H_
#define _MacUtils_H_

#include "nsStringFwd.h"

@class NSString;
@class mozAccessible;

namespace mozilla {
namespace a11y {
namespace utils {

// convert an array of Gecko accessibles to an NSArray of native accessibles
NSArray<mozAccessible*>* ConvertToNSArray(nsTArray<LocalAccessible*>& aArray);

// convert an array of Gecko proxy accessibles to an NSArray of native
// accessibles
NSArray<mozAccessible*>* ConvertToNSArray(nsTArray<RemoteAccessible*>& aArray);

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
}
}
}

#endif
