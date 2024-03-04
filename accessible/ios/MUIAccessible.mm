/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "MUIAccessible.h"

#include "nsString.h"
#include "RootAccessibleWrap.h"

using namespace mozilla;
using namespace mozilla::a11y;

@interface NSObject (AccessibilityPrivate)
- (void)_accessibilityUnregister;
@end

@implementation MUIAccessible

- (id)initWithAccessible:(Accessible*)aAcc {
  MOZ_ASSERT(aAcc, "Cannot init MUIAccessible with null");
  if ((self = [super init])) {
    mGeckoAccessible = aAcc;
  }

  return self;
}

- (void)expire {
  mGeckoAccessible = nullptr;
  if ([self respondsToSelector:@selector(_accessibilityUnregister)]) {
    [self _accessibilityUnregister];
  }
}

- (void)dealloc {
  [super dealloc];
}

@end
