/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozTableAccessible.h"
#import "nsCocoaUtils.h"
#import "AccIterator.h"
#import "TableAccessible.h"

@implementation mozColumnContainer

- (id)initWithIndex:(uint32_t)aIndex andParent:(mozAccessible*)aParent {
  self = [super init];
  mIndex = aIndex;
  mParent = aParent;
  return self;
}

- (NSString*)accessibilityRole {
  return NSAccessibilityColumnRole;
}

- (NSString*)accessibilityRoleDescription {
  return NSAccessibilityRoleDescription(NSAccessibilityColumnRole, nil);
}

- (mozAccessible*)accessibilityParent {
  return mParent;
}

- (NSArray*)accessibilityChildren {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if (mChildren) return mChildren;

  mChildren = [[NSMutableArray alloc] init];

  if (Accessible* acc = [mParent geckoAccessible].AsAccessible()) {
    TableAccessible* table = acc->AsTable();
    NSAssert(table, @"Got null table when fetching column children!");
    uint32_t numRows = table->RowCount();

    for (uint32_t j = 0; j < numRows; j++) {
      Accessible* cell = table->CellAt(j, mIndex);
      mozAccessible* nativeCell = cell ? GetNativeFromGeckoAccessible(cell) : nil;
      if ([nativeCell isAccessibilityElement]) {
        [mChildren addObject:nativeCell];
      }
    }

  } else if (ProxyAccessible* proxy = [mParent geckoAccessible].AsProxy()) {
    uint32_t numRows = proxy->TableRowCount();

    for (uint32_t j = 0; j < numRows; j++) {
      ProxyAccessible* cell = proxy->TableCellAt(j, mIndex);
      mozAccessible* nativeCell = cell ? GetNativeFromGeckoAccessible(cell) : nil;
      if ([nativeCell isAccessibilityElement]) {
        [mChildren addObject:nativeCell];
      }
    }
  }

  return mChildren;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)isAccessibilityElement {
  return YES;
}

- (void)invalidateChildren {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // make room for new children
  if (mChildren) {
    [mChildren release];
    mChildren = nil;
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)dealloc {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [self invalidateChildren];
  [super dealloc];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)expire {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [self invalidateChildren];

  mParent = nil;

  NSAccessibilityPostNotification(self, NSAccessibilityUIElementDestroyedNotification);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (BOOL)isExpired {
  return (mChildren == nil) && (mParent == nil);
}

- (BOOL)accessibilityNotifiesWhenDestroyed {
  return YES;
}

@end

@implementation mozTablePartAccessible

- (id)accessibilityAttributeValue:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ([attribute isEqualToString:NSAccessibilityTitleAttribute]) return @"";

  return [super accessibilityAttributeValue:attribute];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)isLayoutTablePart;
{
  if (Accessible* acc = mGeckoAccessible.AsAccessible()) {
    while (acc) {
      if (acc->IsTable()) {
        return acc->AsTable()->IsProbablyLayoutTable();
      }
      acc = acc->Parent();
    }
    return false;
  }

  if (ProxyAccessible* proxy = mGeckoAccessible.AsProxy()) {
    while (proxy) {
      if (proxy->IsTable()) {
        return proxy->TableIsProbablyForLayout();
      }
      proxy = proxy->Parent();
    }
  }

  return false;
}

- (NSString*)role {
  return [self isLayoutTablePart] ? NSAccessibilityGroupRole : [super role];
}
@end

@implementation mozTableAccessible

- (void)invalidateColumns {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;
  if (mColContainers) {
    [mColContainers release];
    mColContainers = nil;
  }
  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)handleAccessibleEvent:(uint32_t)eventType {
  if (eventType == nsIAccessibleEvent::EVENT_REORDER) {
    [self invalidateColumns];
  }

  [super handleAccessibleEvent:eventType];
}

- (void)dealloc {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [self invalidateColumns];
  [super dealloc];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (NSArray*)additionalAccessibilityAttributeNames {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  NSArray* additionalAttributes = [super additionalAccessibilityAttributeNames];
  if ([self isLayoutTablePart]) {
    return additionalAttributes;
  }

  static NSArray* tableAttrs = nil;
  if (!tableAttrs) {
    NSMutableArray* tempArray = [NSMutableArray new];
    [tempArray addObject:NSAccessibilityRowCountAttribute];
    [tempArray addObject:NSAccessibilityColumnCountAttribute];
    [tempArray addObject:NSAccessibilityRowsAttribute];
    [tempArray addObject:NSAccessibilityColumnsAttribute];
    tableAttrs = [[NSArray alloc] initWithArray:tempArray];
    [tempArray release];
  }

  return [additionalAttributes arrayByAddingObjectsFromArray:tableAttrs];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  if ([attribute isEqualToString:NSAccessibilityColumnsAttribute]) {
    return [self getColContainerList];
  }

  if ([attribute isEqualToString:NSAccessibilityRowsAttribute]) {
    // Create a new array with the list of table rows.
    return [[self children]
        filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(mozAccessible* child,
                                                                          NSDictionary* bindings) {
          return [child isKindOfClass:[mozTableRowAccessible class]];
        }]];
  }

  ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
  Accessible* acc = mGeckoAccessible.AsAccessible();
  MOZ_ASSERT(!acc || acc->AsTable(), "Got null table from table accessible");

  if ([attribute isEqualToString:NSAccessibilityColumnCountAttribute]) {
    return acc ? @(acc->AsTable()->ColCount()) : @(proxy->TableColumnCount());
  }

  if ([attribute isEqualToString:NSAccessibilityRowCountAttribute]) {
    return acc ? @(acc->AsTable()->RowCount()) : @(proxy->TableRowCount());
  }

  return [super accessibilityAttributeValue:attribute];
}

- (NSArray*)children {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  return [[super children] arrayByAddingObjectsFromArray:[self getColContainerList]];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSArray*)getColContainerList {
  if (mColContainers) {
    return mColContainers;
  }

  mColContainers = [[NSMutableArray alloc] init];
  uint32_t numCols = 0;

  if (Accessible* acc = mGeckoAccessible.AsAccessible()) {
    numCols = acc->AsTable()->ColCount();
  } else {
    numCols = mGeckoAccessible.AsProxy()->TableColumnCount();
  }

  for (uint32_t i = 0; i < numCols; i++) {
    mozColumnContainer* container = [[mozColumnContainer alloc] initWithIndex:i andParent:self];
    [mColContainers addObject:container];
  }

  return mColContainers;
}

@end

@implementation mozTableRowAccessible
- (void)handleAccessibleEvent:(uint32_t)eventType {
  if (eventType == nsIAccessibleEvent::EVENT_REORDER) {
    id parent = [self parent];
    if ([parent isKindOfClass:[mozTableAccessible class]]) {
      [parent invalidateColumns];
    }
  }

  [super handleAccessibleEvent:eventType];
}

- (NSArray*)additionalAccessibilityAttributeNames {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  NSArray* additionalAttributes = [super additionalAccessibilityAttributeNames];
  if ([self isLayoutTablePart]) {
    return additionalAttributes;
  }

  static NSArray* tableRowAttrs = nil;
  if (!tableRowAttrs) {
    NSMutableArray* tempArray = [NSMutableArray new];
    [tempArray addObject:NSAccessibilityIndexAttribute];
    tableRowAttrs = [[NSArray alloc] initWithArray:tempArray];
    [tempArray release];
  }

  return [additionalAttributes arrayByAddingObjectsFromArray:tableRowAttrs];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  if ([attribute isEqualToString:NSAccessibilityIndexAttribute]) {
    if (Accessible* acc = mGeckoAccessible.AsAccessible()) {
      // Count the number of rows before that one to obtain the row index.
      uint32_t index = 0;
      Accessible* parent = acc->Parent();
      if (parent) {
        for (int32_t i = acc->IndexInParent() - 1; i >= 0; i--) {
          if (parent->GetChildAt(i)->IsTableRow()) {
            index++;
          }
        }
      }
      return [NSNumber numberWithUnsignedInteger:index];
    } else {
      ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
      uint32_t index = 0;
      ProxyAccessible* parent = proxy->Parent();
      if (parent) {
        for (int32_t i = proxy->IndexInParent() - 1; i >= 0; i--) {
          if (parent->ChildAt(i)->IsTableRow()) {
            index++;
          }
        }
      }
      return [NSNumber numberWithUnsignedInteger:index];
    }
  }

  return [super accessibilityAttributeValue:attribute];
}
@end

@implementation mozTableCellAccessible
- (NSArray*)additionalAccessibilityAttributeNames {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  NSArray* additionalAttributes = [super additionalAccessibilityAttributeNames];
  if ([self isLayoutTablePart]) {
    return additionalAttributes;
  }

  static NSArray* tableCellAttrs = nil;
  if (!tableCellAttrs) {
    NSMutableArray* tempArray = [NSMutableArray new];
    [tempArray addObject:NSAccessibilityRowIndexRangeAttribute];
    [tempArray addObject:NSAccessibilityColumnIndexRangeAttribute];
    [tempArray addObject:NSAccessibilityRowHeaderUIElementsAttribute];
    [tempArray addObject:NSAccessibilityColumnHeaderUIElementsAttribute];
    tableCellAttrs = [[NSArray alloc] initWithArray:tempArray];
    [tempArray release];
  }

  return [additionalAttributes arrayByAddingObjectsFromArray:tableCellAttrs];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  if ([attribute isEqualToString:NSAccessibilityRowIndexRangeAttribute]) {
    if (mGeckoAccessible.IsAccessible()) {
      TableCellAccessible* cell = mGeckoAccessible.AsAccessible()->AsTableCell();
      return [NSValue valueWithRange:NSMakeRange(cell->RowIdx(), cell->RowExtent())];
    } else {
      ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
      return [NSValue valueWithRange:NSMakeRange(proxy->RowIdx(), proxy->RowExtent())];
    }
  }

  if ([attribute isEqualToString:NSAccessibilityColumnIndexRangeAttribute]) {
    if (mGeckoAccessible.IsAccessible()) {
      TableCellAccessible* cell = mGeckoAccessible.AsAccessible()->AsTableCell();
      return [NSValue valueWithRange:NSMakeRange(cell->ColIdx(), cell->ColExtent())];
    } else {
      ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
      return [NSValue valueWithRange:NSMakeRange(proxy->ColIdx(), proxy->ColExtent())];
    }
  }

  if ([attribute isEqualToString:NSAccessibilityRowHeaderUIElementsAttribute]) {
    if (mGeckoAccessible.IsAccessible()) {
      TableCellAccessible* cell = mGeckoAccessible.AsAccessible()->AsTableCell();
      AutoTArray<Accessible*, 10> headerCells;
      cell->RowHeaderCells(&headerCells);
      return utils::ConvertToNSArray(headerCells);
    } else {
      ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
      nsTArray<ProxyAccessible*> headerCells;
      proxy->RowHeaderCells(&headerCells);
      return utils::ConvertToNSArray(headerCells);
    }
  }

  if ([attribute isEqualToString:NSAccessibilityColumnHeaderUIElementsAttribute]) {
    if (mGeckoAccessible.IsAccessible()) {
      TableCellAccessible* cell = mGeckoAccessible.AsAccessible()->AsTableCell();
      AutoTArray<Accessible*, 10> headerCells;
      cell->ColHeaderCells(&headerCells);
      return utils::ConvertToNSArray(headerCells);
    } else {
      ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
      nsTArray<ProxyAccessible*> headerCells;
      proxy->ColHeaderCells(&headerCells);
      return utils::ConvertToNSArray(headerCells);
    }
  }

  return [super accessibilityAttributeValue:attribute];
}
@end
