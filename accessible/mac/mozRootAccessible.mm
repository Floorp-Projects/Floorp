/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RootAccessibleWrap.h"

#import "mozRootAccessible.h"

#import "mozView.h"

#include "gfxPlatform.h"
// This must be included last:
#include "nsObjCExceptions.h"

using namespace mozilla::a11y;

static id<mozAccessible, mozView> getNativeViewFromRootAccessible(
    LocalAccessible* aAccessible) {
  RootAccessibleWrap* root =
      static_cast<RootAccessibleWrap*>(aAccessible->AsRoot());
  id<mozAccessible, mozView> nativeView = nil;
  root->GetNativeWidget((void**)&nativeView);
  return nativeView;
}

#pragma mark -

@implementation mozRootAccessible

- (id)initWithAccessible:(mozilla::a11y::Accessible*)aAcc {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  MOZ_ASSERT(!aAcc->IsRemote(), "mozRootAccessible is never a proxy");

  mParallelView = getNativeViewFromRootAccessible(aAcc->AsLocal());

  return [super initWithAccessible:aAcc];

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (NSNumber*)moxMain {
  return @([[self moxWindow] isMainWindow]);
}

- (NSNumber*)moxMinimized {
  return @([[self moxWindow] isMiniaturized]);
}

// return the AXParent that our parallell NSView tells us about.
- (id)moxUnignoredParent {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  // If there is no represented view (eg. headless), this will return nil.
  return [[self representedView]
      accessibilityAttributeValue:NSAccessibilityParentAttribute];

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (BOOL)hasRepresentedView {
  return YES;
}

// this will return our parallell NSView. see mozDocAccessible.h
- (id)representedView {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  MOZ_ASSERT(mParallelView || gfxPlatform::IsHeadless(),
             "root accessible does not have a native parallel view.");

  return mParallelView;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (BOOL)isRoot {
  return YES;
}

@end
