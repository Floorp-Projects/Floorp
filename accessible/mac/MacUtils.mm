/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "MacUtils.h"

#include "LocalAccessible.h"
#include "nsCocoaUtils.h"
#include "mozilla/a11y/PDocAccessible.h"

namespace mozilla {
namespace a11y {
namespace utils {

/**
 * Get a localized string from the a11y string bundle.
 * Return nil if not found.
 */
NSString* LocalizedString(const nsString& aString) {
  nsString text;

  Accessible::TranslateString(aString, text);

  return text.IsEmpty() ? nil : nsCocoaUtils::ToNSString(text);
}

NSString* GetAccAttr(mozAccessible* aNativeAccessible, nsAtom* aAttrName) {
  nsAutoString result;
  Accessible* acc = [aNativeAccessible geckoAccessible];
  RefPtr<AccAttributes> attributes = acc->Attributes();

  if (!attributes) {
    return nil;
  }

  attributes->GetAttribute(aAttrName, result);

  if (!result.IsEmpty()) {
    return nsCocoaUtils::ToNSString(result);
  }

  return nil;
}
}
}
}
