/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsObjCExceptions.h"
#include "RootAccessibleWrap.h"

#import "mozDocAccessible.h"

#import "mozView.h"

using namespace mozilla::a11y;

static id <mozAccessible, mozView> 
getNativeViewFromRootAccessible(nsAccessible* aAccessible)
{
  RootAccessibleWrap* root =
    static_cast<RootAccessibleWrap*>(aAccessible->AsRoot());
  id <mozAccessible, mozView> nativeView = nil;
  root->GetNativeWidget ((void**)&nativeView);
  return nativeView;
}

#pragma mark -

@implementation mozRootAccessible

- (NSArray*)accessibilityAttributeNames
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;
  
  // if we're expired, we don't support any attributes.
  if (mIsExpired)
    return [NSArray array];
  
  // standard attributes that are shared and supported by root accessible (AXMain) elements.
  static NSMutableArray* attributes = nil;
  
  if (!attributes) {
    attributes = [[super accessibilityAttributeNames] mutableCopy];
    [attributes addObject:NSAccessibilityMainAttribute];
    [attributes addObject:NSAccessibilityMinimizedAttribute];
  }

  return attributes;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)accessibilityAttributeValue:(NSString *)attribute
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;
  
  if ([attribute isEqualToString:NSAccessibilityMainAttribute])
    return [NSNumber numberWithBool:[[self window] isMainWindow]];
  if ([attribute isEqualToString:NSAccessibilityMinimizedAttribute])
    return [NSNumber numberWithBool:[[self window] isMiniaturized]];

  return [super accessibilityAttributeValue:attribute];
  
  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}


// return the AXParent that our parallell NSView tells us about.
- (id)parent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if (!mParallelView)
    mParallelView = (id<mozView, mozAccessible>)[self representedView];
  
  if (mParallelView)
    return [mParallelView accessibilityAttributeValue:NSAccessibilityParentAttribute];
  
  NSAssert(mParallelView, @"we're a root accessible w/o native view?");
  return [super parent];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)hasRepresentedView
{
  return YES;
}

// this will return our parallell NSView. see mozDocAccessible.h
- (id)representedView
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if (mParallelView)
    return (id)mParallelView;
  
  mParallelView = getNativeViewFromRootAccessible (mGeckoAccessible);
  
  NSAssert(mParallelView, @"can't return root accessible's native parallel view.");
  return mParallelView;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)isRoot
{
  return YES;
}

@end
