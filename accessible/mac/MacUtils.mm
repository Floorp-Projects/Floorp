/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "MacUtils.h"

#include "Accessible.h"

#include "nsCocoaUtils.h"

namespace mozilla {
namespace a11y {
namespace utils {

/**
 * Get a localized string from the a11y string bundle.
 * Return nil if not found.
 */
NSString*
LocalizedString(const nsString& aString)
{
  nsString text;

  Accessible::TranslateString(aString, text);

  return text.IsEmpty() ? nil : nsCocoaUtils::ToNSString(text);
}

}
}
}
