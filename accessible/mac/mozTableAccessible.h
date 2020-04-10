/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozAccessible.h"

@interface mozColumnContainer : NSObject {
  uint32_t mIndex;
  mozAccessible* mParent;
  NSMutableArray* mChildren;
}

- (id)initWithIndex:(uint32_t)aIndex andParent:(id<mozAccessible>)aParent;
- (NSString*)accessibilityRole;
- (NSString*)accessibilityRoleDescription;
- (mozAccessible*)accessibilityParent;
- (NSArray*)accessibilityChildren;
- (BOOL)accessibilityIsIgnored;
- (void)invalidateChildren;
- (void)dealloc;
- (void)expire;
- (BOOL)isExpired;
- (BOOL)accessibilityNotifiesWhenDestroyed;
@end

@interface mozTablePartAccessible : mozAccessible
- (id)accessibilityAttributeValue:(NSString*)attribute;
- (BOOL)isLayoutTablePart;
- (NSString*)role;
@end

@interface mozTableAccessible : mozTablePartAccessible {
  NSMutableArray* mColContainers;
}
- (NSArray*)children;
- (NSArray*)additionalAccessibilityAttributeNames;
- (id)accessibilityAttributeValue:(NSString*)attribute;
- (void)invalidateColumns;
- (void)handleAccessibleEvent:(uint32_t)eventType;
- (void)dealloc;
@end

@interface mozTableRowAccessible : mozTablePartAccessible
- (NSArray*)additionalAccessibilityAttributeNames;
- (void)handleAccessibleEvent:(uint32_t)eventType;
- (id)accessibilityAttributeValue:(NSString*)attribute;
@end

@interface mozTableCellAccessible : mozTablePartAccessible
- (NSArray*)additionalAccessibilityAttributeNames;
- (id)accessibilityAttributeValue:(NSString*)attribute;
@end
