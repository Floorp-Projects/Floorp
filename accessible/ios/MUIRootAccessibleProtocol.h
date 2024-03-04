/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Foundation/Foundation.h>
#import <UIKit/UIAccessibility.h>

/* This protocol's primary use is so widget/cocoa can talk back to us
   properly.

   ChildView owns the topmost MUIRootAccessible, and needs to take care of
   setting up that parent/child relationship.

   This protocol is thus used to make sure it knows it's talking to us, and not
   just some random |id|.
*/

@protocol MUIRootAccessibleProtocol <NSObject>

- (BOOL)hasRepresentedView;

- (nullable id)representedView;

// UIAccessibility

- (BOOL)isAccessibilityElement;

- (nullable NSString*)accessibilityLabel;

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
