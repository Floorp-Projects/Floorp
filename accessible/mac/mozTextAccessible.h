/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozAccessible.h"

@interface mozTextAccessible : mozAccessible

// override
- (NSString*)moxTitle;

// override
- (id)moxValue;

// override
- (id)moxRequired;

// override
- (NSNumber*)moxInvalid;

// override
- (NSNumber*)moxInsertionPointLineNumber;

// override
- (NSString*)moxRole;

// override
- (NSString*)moxSubrole;

// override
- (NSNumber*)moxNumberOfCharacters;

// override
- (NSString*)moxSelectedText;

// override
- (NSValue*)moxSelectedTextRange;

// override
- (NSValue*)moxVisibleCharacterRange;

// override
- (BOOL)moxBlockSelector:(SEL)selector;

// override
- (void)moxSetValue:(id)value;

// override
- (void)moxSetSelectedText:(NSString*)text;

// override
- (void)moxSetSelectedTextRange:(NSValue*)range;

// override
- (void)moxSetVisibleCharacterRange:(NSValue*)range;

// override
- (NSString*)moxStringForRange:(NSValue*)range;

// override
- (NSAttributedString*)moxAttributedStringForRange:(NSValue*)range;

// override
- (NSValue*)moxRangeForLine:(NSNumber*)line;

// override
- (NSNumber*)moxLineForIndex:(NSNumber*)index;

// override
- (NSValue*)moxBoundsForRange:(NSValue*)range;

#pragma mark - mozAccessible

// override
- (void)handleAccessibleTextChangeEvent:(NSString*)change
                               inserted:(BOOL)isInserted
                            inContainer:(mozilla::a11y::Accessible*)container
                                     at:(int32_t)start;

// override
- (void)handleAccessibleEvent:(uint32_t)eventType;

@end

@interface mozTextLeafAccessible : mozAccessible

// override
- (BOOL)moxBlockSelector:(SEL)selector;

// override
- (NSString*)moxValue;

// override
- (NSString*)moxTitle;

// override
- (NSString*)moxLabel;

// override
- (BOOL)moxIgnoreWithParent:(mozAccessible*)parent;

// override
- (NSString*)moxStringForRange:(NSValue*)range;

// override
- (NSAttributedString*)moxAttributedStringForRange:(NSValue*)range;

// override
- (NSValue*)moxBoundsForRange:(NSValue*)range;

@end
