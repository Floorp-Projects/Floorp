/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>
#import "mozAccessible.h"

@interface mozSelectableAccessible : mozAccessible

- (NSArray*)selectableChildren;

// override
- (void)moxSetSelectedChildren:(NSArray*)selectedChildren;

// override
- (NSArray*)moxSelectedChildren;

@end

@interface mozSelectableChildAccessible : mozAccessible

// override
- (NSNumber*)moxSelected;

// override
- (void)moxSetSelected:(NSNumber*)selected;

@end

@interface mozTabGroupAccessible : mozSelectableAccessible

// override
- (NSArray*)moxTabs;

// override
- (NSArray*)moxContents;

// override
- (id)moxValue;

@end

@interface mozTabAccessible : mozSelectableChildAccessible

// override
- (NSString*)moxRoleDescription;

// override
- (id)moxValue;

@end

@interface mozListboxAccessible : mozSelectableAccessible

// override
- (BOOL)moxIgnoreChild:(mozAccessible*)child;

// override
- (BOOL)disableChild:(mozAccessible*)child;

// override
- (NSString*)moxOrientation;

@end

@interface mozOptionAccessible : mozSelectableChildAccessible

// override
- (NSString*)moxTitle;

// override
- (id)moxValue;

@end

@interface mozMenuAccessible : mozSelectableAccessible {
  BOOL mIsOpened;
}

// override
- (NSString*)moxTitle;

// override
- (NSString*)moxLabel;

// override
- (NSArray*)moxVisibleChildren;

// override
- (BOOL)moxIgnoreWithParent:(mozAccessible*)parent;

// override
- (id)moxTitleUIElement;

// override
- (void)moxPostNotification:(NSString*)notification;

// override
- (void)expire;

- (BOOL)isOpened;

@end

@interface mozMenuItemAccessible : mozSelectableChildAccessible

// override
- (NSString*)moxLabel;

// override
- (BOOL)moxIgnoreWithParent:(mozAccessible*)parent;

// override
- (NSString*)moxMenuItemMarkChar;

// override
- (NSNumber*)moxSelected;

// override
- (void)handleAccessibleEvent:(uint32_t)eventType;

// override
- (void)moxPerformPress;

@end
