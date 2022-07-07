/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>
#import "mozAccessible.h"

/* Simple subclasses for things like checkboxes, buttons, etc. */

@interface mozButtonAccessible : mozAccessible

// override
- (NSNumber*)moxHasPopup;

// override
- (NSString*)moxPopupValue;

@end

@interface mozPopupButtonAccessible : mozButtonAccessible

// override
- (NSString*)moxTitle;

// override
- (BOOL)moxBlockSelector:(SEL)selector;

// override
- (NSArray*)moxChildren;

// override
- (void)stateChanged:(uint64_t)state isEnabled:(BOOL)enabled;

@end

@interface mozCheckboxAccessible : mozButtonAccessible

// override
- (id)moxValue;

// override
- (void)stateChanged:(uint64_t)state isEnabled:(BOOL)enabled;

@end

// LocalAccessible for a radio button
@interface mozRadioButtonAccessible : mozCheckboxAccessible

// override
- (NSArray*)moxLinkedUIElements;

@end

/**
 * Accessible for a PANE
 */
@interface mozPaneAccessible : mozAccessible

// override
- (NSArray*)moxChildren;

@end

/**
 * Base accessible for an incrementable
 */
@interface mozIncrementableAccessible : mozAccessible

// override
- (id)moxValue;

// override
- (NSString*)moxValueDescription;

// override
- (id)moxMinValue;

// override
- (id)moxMaxValue;

// override
- (void)moxSetValue:(id)value;

// override
- (void)moxPerformIncrement;

// override
- (void)moxPerformDecrement;

// override
- (NSString*)moxOrientation;

// override
- (void)handleAccessibleEvent:(uint32_t)eventType;

- (void)changeValueBySteps:(int)factor;

@end
