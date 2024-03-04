/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MUIAccessible_H_
#define _MUIAccessible_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIAccessibility.h>

#include "AccessibleWrap.h"
#include "RemoteAccessible.h"

@class MUIAccessible;

namespace mozilla {
namespace a11y {

inline MUIAccessible* _Nullable GetNativeFromGeckoAccessible(
    mozilla::a11y::Accessible* _Nullable aAcc) {
  if (!aAcc) {
    return nil;
  }
  if (LocalAccessible* localAcc = aAcc->AsLocal()) {
    MUIAccessible* native = nil;
    localAcc->GetNativeInterface((void**)&native);
    return native;
  }

  RemoteAccessible* remoteAcc = aAcc->AsRemote();
  return reinterpret_cast<MUIAccessible*>(remoteAcc->GetWrapper());
}

}  // namespace a11y
}  // namespace mozilla

@interface MUIAccessible : NSObject {
  mozilla::a11y::Accessible* mGeckoAccessible;
}

// inits with the given accessible
- (nonnull id)initWithAccessible:(nonnull mozilla::a11y::Accessible*)aAcc;

// allows for gecko accessible access outside of the class
- (mozilla::a11y::Accessible* _Nullable)geckoAccessible;

- (void)expire;

// override
- (void)dealloc;

// UIAccessibility
- (BOOL)isAccessibilityElement;
- (nullable NSString*)accessibilityLabel;
- (nullable NSString*)accessibilityHint;
- (CGRect)accessibilityFrame;
- (nullable NSString*)accessibilityValue;
- (uint64_t)accessibilityTraits;

// UIAccessibilityContainer
- (NSInteger)accessibilityElementCount;
- (nullable id)accessibilityElementAtIndex:(NSInteger)index;
- (NSInteger)indexOfAccessibilityElement:(nonnull id)element;
- (nullable NSArray*)accessibilityElements;
- (UIAccessibilityContainerType)accessibilityContainerType;

@end

#endif
