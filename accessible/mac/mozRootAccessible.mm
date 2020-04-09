/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RootAccessibleWrap.h"

#import "mozRootAccessible.h"

#import "mozView.h"

// This must be included last:
#include "nsObjCExceptions.h"

using namespace mozilla::a11y;

static id<mozAccessible, mozView> getNativeViewFromRootAccessible(Accessible* aAccessible) {
  RootAccessibleWrap* root = static_cast<RootAccessibleWrap*>(aAccessible->AsRoot());
  id<mozAccessible, mozView> nativeView = nil;
  root->GetNativeWidget((void**)&nativeView);
  return nativeView;
}

#pragma mark -

@implementation mozRootAccessible

- (id)initWithAccessible:(uintptr_t)aGeckoAccessible {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  NSAssert((aGeckoAccessible & IS_PROXY) == 0, @"mozRootAccessible is never a proxy");

  mParallelView = getNativeViewFromRootAccessible((Accessible*)aGeckoAccessible);

  return [super initWithAccessible:aGeckoAccessible];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSArray*)accessibilityAttributeNames {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  // if we're expired, we don't support any attributes.
  if (![self getGeckoAccessible]) return [NSArray array];

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

- (id)accessibilityAttributeValue:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ([attribute isEqualToString:NSAccessibilityMainAttribute])
    return [NSNumber numberWithBool:[[self window] isMainWindow]];
  if ([attribute isEqualToString:NSAccessibilityMinimizedAttribute])
    return [NSNumber numberWithBool:[[self window] isMiniaturized]];

  return [super accessibilityAttributeValue:attribute];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

// return the AXParent that our parallell NSView tells us about.
- (id)parent {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if (!mParallelView) mParallelView = (id<mozView, mozAccessible>)[self representedView];

  if (mParallelView)
    return [mParallelView accessibilityAttributeValue:NSAccessibilityParentAttribute];

  NSAssert(mParallelView, @"we're a root accessible w/o native view?");
  return [super parent];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)hasRepresentedView {
  return YES;
}

- (NSArray*)children {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  return [[super children]
      filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(mozAccessible* child,
                                                                        NSDictionary* bindings) {
        AccessibleWrap* childAcc = [child getGeckoAccessible];
        if (childAcc) {
          if (((childAcc->VisibilityState() & states::INVISIBLE) != 0)) {
            // Filter out all invisible XUL popup menus, dialogs, alerts and panes. Invisible
            // elements in our browser chrome are unique in the sense that we want screen readers to
            // ignore them. These only exist in the top level process so we don't do a similar check
            // on proxies.
            return NO;
          }
        }

        return YES;
      }]];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

// this will return our parallell NSView. see mozDocAccessible.h
- (id)representedView {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  NSAssert(mParallelView, @"root accessible does not have a native parallel view.");

  return mParallelView;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)isRoot {
  return YES;
}

@end
