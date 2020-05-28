/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozAccessible.h"

@interface mozColumnContainer : MOXAccessibleBase {
  uint32_t mIndex;
  mozAccessible* mParent;
  NSMutableArray* mChildren;
}

// override
- (id)initWithIndex:(uint32_t)aIndex andParent:(mozAccessible*)aParent;

// override
- (NSString*)moxRole;

// override
- (NSString*)moxRoleDescription;

// override
- (mozAccessible*)moxParent;

// override
- (NSArray*)moxChildren;

// override
- (void)dealloc;

// override
- (void)expire;

// override
- (BOOL)isExpired;

- (void)invalidateChildren;

@end

@interface mozTablePartAccessible : mozAccessible

// override
- (NSString*)moxTitle;

// override
- (NSString*)moxRole;

- (BOOL)isLayoutTablePart;

@end

@interface mozTableAccessible : mozTablePartAccessible {
  NSMutableArray* mColContainers;
}

- (void)invalidateColumns;

// override
- (void)handleAccessibleEvent:(uint32_t)eventType;

// override
- (void)dealloc;

// override
- (NSNumber*)moxRowCount;

// override
- (NSNumber*)moxColumnCount;

// override
- (NSArray*)moxRows;

// override
- (NSArray*)moxColumns;

// override
- (NSArray*)moxChildren;

@end

@interface mozTableRowAccessible : mozTablePartAccessible

// override
- (void)handleAccessibleEvent:(uint32_t)eventType;

// override
- (NSNumber*)moxIndex;

@end

@interface mozTableCellAccessible : mozTablePartAccessible

// override
- (NSValue*)moxRowIndexRange;

// override
- (NSValue*)moxColumnIndexRange;

// override
- (NSArray*)moxRowHeaderUIElements;

// override
- (NSArray*)moxColumnHeaderUIElements;

@end
