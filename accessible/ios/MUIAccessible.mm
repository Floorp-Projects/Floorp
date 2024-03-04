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

#ifdef A11Y_LOG
static NSString* ToNSString(const nsACString& aCString) {
  if (aCString.IsEmpty()) {
    return [NSString string];
  }
  return [[[NSString alloc] initWithBytes:aCString.BeginReading()
                                   length:aCString.Length()
                                 encoding:NSUTF8StringEncoding] autorelease];
}
#endif

#pragma mark -

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

- (mozilla::a11y::Accessible*)geckoAccessible {
  return mGeckoAccessible;
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

- (BOOL)isAccessibilityElement {
  if (!mGeckoAccessible || mGeckoAccessible->ChildCount()) {
    // XXX: Containers should return NO or their children are ignored.
    // I think that if the container has a child or descendant that
    // returns YES for isAccessibilityElement, then a UIKit accessible
    // is provided for the container, even if it returned NO.
    return NO;
  }

  return YES;
}

- (NSString*)accessibilityLabel {
  if (!mGeckoAccessible) {
    return @"";
  }

#ifdef A11Y_LOG
  // Just put in a debug description as the label so we get a clue about which
  // accessible ends up where.
  nsAutoCString desc;
  mGeckoAccessible->DebugDescription(desc);
  return ToNSString(desc);
#else
  return @"";
#endif
}

- (CGRect)accessibilityFrame {
  RootAccessibleWrap* rootAcc = static_cast<RootAccessibleWrap*>(
      mGeckoAccessible->IsLocal()
          ? mGeckoAccessible->AsLocal()->RootAccessible()
          : mGeckoAccessible->AsRemote()
                ->OuterDocOfRemoteBrowser()
                ->RootAccessible());

  if (!rootAcc) {
    return CGRectMake(0, 0, 0, 0);
  }

  LayoutDeviceIntRect rect = mGeckoAccessible->Bounds();
  return rootAcc->DevPixelsRectToUIKit(rect);
}

- (NSString*)accessibilityValue {
  return nil;
}

- (uint64_t)accessibilityTraits {
  return 1ul << 17;  // web content
}

- (NSInteger)accessibilityElementCount {
  return mGeckoAccessible ? mGeckoAccessible->ChildCount() : 0;
}

- (nullable id)accessibilityElementAtIndex:(NSInteger)index {
  if (!mGeckoAccessible) {
    return nil;
  }

  Accessible* child = mGeckoAccessible->ChildAt(index);
  return GetNativeFromGeckoAccessible(child);
}

- (NSInteger)indexOfAccessibilityElement:(id)element {
  Accessible* acc = [(MUIAccessible*)element geckoAccessible];
  if (!acc || mGeckoAccessible != acc->Parent()) {
    return -1;
  }

  return acc->IndexInParent();
}

- (NSArray* _Nullable)accessibilityElements {
  NSMutableArray* children = [[[NSMutableArray alloc] init] autorelease];
  uint32_t childCount = mGeckoAccessible->ChildCount();
  for (uint32_t i = 0; i < childCount; i++) {
    if (MUIAccessible* child =
            GetNativeFromGeckoAccessible(mGeckoAccessible->ChildAt(i))) {
      [children addObject:child];
    }
  }

  return children;
}

- (UIAccessibilityContainerType)accessibilityContainerType {
  return UIAccessibilityContainerTypeNone;
}

@end
