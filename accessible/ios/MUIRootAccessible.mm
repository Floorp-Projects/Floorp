/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RootAccessibleWrap.h"

#import "MUIRootAccessible.h"
#import <UIKit/UIScreen.h>

using namespace mozilla::a11y;

static id<MUIRootAccessibleProtocol> getNativeViewFromRootAccessible(
    LocalAccessible* aAccessible) {
  RootAccessibleWrap* root =
      static_cast<RootAccessibleWrap*>(aAccessible->AsRoot());
  id<MUIRootAccessibleProtocol> nativeView = nil;
  root->GetNativeWidget((void**)&nativeView);
  return nativeView;
}

#pragma mark -

@implementation MUIRootAccessible

- (id)initWithAccessible:(mozilla::a11y::Accessible*)aAcc {
  MOZ_ASSERT(!aAcc->IsRemote(), "MUIRootAccessible is never remote");

  mParallelView = getNativeViewFromRootAccessible(aAcc->AsLocal());

  return [super initWithAccessible:aAcc];
}

- (BOOL)hasRepresentedView {
  return YES;
}

// this will return our parallel UIView.
- (id)representedView {
  return mParallelView;
}

@end
