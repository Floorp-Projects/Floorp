/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozAccessible.h"

@interface mozTablePartAccessible : mozAccessible
- (id)accessibilityAttributeValue:(NSString*)attribute;
- (BOOL)isLayoutTablePart;
- (NSString*)role;
@end

@interface mozTableAccessible : mozTablePartAccessible
- (NSArray*)additionalAccessibilityAttributeNames;
- (id)accessibilityAttributeValue:(NSString*)attribute;
@end

@interface mozTableRowAccessible : mozTablePartAccessible
- (NSArray*)additionalAccessibilityAttributeNames;
- (id)accessibilityAttributeValue:(NSString*)attribute;
@end

@interface mozTableCellAccessible : mozTablePartAccessible
- (NSArray*)additionalAccessibilityAttributeNames;
- (id)accessibilityAttributeValue:(NSString*)attribute;
@end
