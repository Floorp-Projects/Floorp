/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozTableAccessible.h"
#import "nsCocoaUtils.h"
#import "MacUtils.h"

#include "AccIterator.h"
#include "Accessible.h"
#include "TableAccessible.h"
#include "TableCellAccessible.h"

using namespace mozilla;
using namespace mozilla::a11y;

@implementation mozColumnContainer

- (id)initWithIndex:(uint32_t)aIndex andParent:(mozAccessible*)aParent {
  self = [super init];
  mIndex = aIndex;
  mParent = aParent;
  return self;
}

- (NSString*)moxRole {
  return NSAccessibilityColumnRole;
}

- (NSString*)moxRoleDescription {
  return NSAccessibilityRoleDescription(NSAccessibilityColumnRole, nil);
}

- (mozAccessible*)moxParent {
  return mParent;
}

- (NSArray*)moxUnignoredChildren {
  if (mChildren) return mChildren;

  mChildren = [[NSMutableArray alloc] init];

  if (Accessible* acc = [mParent geckoAccessible].AsAccessible()) {
    TableAccessible* table = acc->AsTable();
    MOZ_ASSERT(table, "Got null table when fetching column children!");
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

  [super expire];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (BOOL)isExpired {
  MOZ_ASSERT((mChildren == nil && mParent == nil) == mIsExpired);

  return [super isExpired];
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

@end

@implementation mozTablePartAccessible

- (NSString*)moxTitle {
  return @"";
}

- (NSString*)moxRole {
  return [self isLayoutTablePart] ? NSAccessibilityGroupRole : [super moxRole];
}

- (BOOL)isLayoutTablePart {
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

@end

@implementation mozTableAccessible

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

- (NSNumber*)moxRowCount {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  return mGeckoAccessible.IsAccessible() ? @(mGeckoAccessible.AsAccessible()->AsTable()->RowCount())
                                         : @(mGeckoAccessible.AsProxy()->TableRowCount());
}

- (NSNumber*)moxColumnCount {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  return mGeckoAccessible.IsAccessible() ? @(mGeckoAccessible.AsAccessible()->AsTable()->ColCount())
                                         : @(mGeckoAccessible.AsProxy()->TableColumnCount());
}

- (NSArray*)moxRows {
  // Create a new array with the list of table rows.
  return [[self moxChildren]
      filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(mozAccessible* child,
                                                                        NSDictionary* bindings) {
        return [child isKindOfClass:[mozTableRowAccessible class]];
      }]];
}

- (NSArray*)moxColumns {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

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

- (NSArray*)moxUnignoredChildren {
  return [[super moxUnignoredChildren] arrayByAddingObjectsFromArray:[self moxColumns]];
}

- (NSArray*)moxColumnHeaderUIElements {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  uint32_t numCols = 0;
  TableAccessible* table = nullptr;

  if (Accessible* acc = mGeckoAccessible.AsAccessible()) {
    table = mGeckoAccessible.AsAccessible()->AsTable();
    numCols = table->ColCount();
  } else {
    numCols = mGeckoAccessible.AsProxy()->TableColumnCount();
  }

  NSMutableArray* colHeaders = [[NSMutableArray alloc] initWithCapacity:numCols];

  for (uint32_t i = 0; i < numCols; i++) {
    AccessibleOrProxy cell;
    if (table) {
      cell = table->CellAt(0, i);
    } else {
      cell = mGeckoAccessible.AsProxy()->TableCellAt(0, i);
    }

    if (!cell.IsNull() && cell.Role() == roles::COLUMNHEADER) {
      mozAccessible* colHeader = GetNativeFromGeckoAccessible(cell);
      [colHeaders addObject:colHeader];
    }
  }

  return colHeaders;
}

- (id)moxCellForColumnAndRow:(NSArray*)columnAndRow {
  if (columnAndRow == nil || [columnAndRow count] != 2) {
    return nil;
  }

  uint32_t col = [[columnAndRow objectAtIndex:0] unsignedIntValue];
  uint32_t row = [[columnAndRow objectAtIndex:1] unsignedIntValue];

  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  AccessibleOrProxy cell;
  if (mGeckoAccessible.IsAccessible()) {
    cell = mGeckoAccessible.AsAccessible()->AsTable()->CellAt(row, col);
  } else {
    cell = mGeckoAccessible.AsProxy()->TableCellAt(row, col);
  }

  if (cell.IsNull()) {
    return nil;
  }

  return GetNativeFromGeckoAccessible(cell);
}

- (void)invalidateColumns {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;
  if (mColContainers) {
    [mColContainers release];
    mColContainers = nil;
  }
  NS_OBJC_END_TRY_ABORT_BLOCK;
}

@end

@implementation mozTableRowAccessible

- (void)handleAccessibleEvent:(uint32_t)eventType {
  if (eventType == nsIAccessibleEvent::EVENT_REORDER) {
    id parent = [self moxParent];
    if ([parent isKindOfClass:[mozTableAccessible class]]) {
      [parent invalidateColumns];
    }
  }

  [super handleAccessibleEvent:eventType];
}

- (NSNumber*)moxIndex {
  mozTableAccessible* parent = (mozTableAccessible*)[self moxParent];
  return @([[parent moxRows] indexOfObjectIdenticalTo:self]);
}

@end

@implementation mozTableCellAccessible

- (NSValue*)moxRowIndexRange {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  if (mGeckoAccessible.IsAccessible()) {
    TableCellAccessible* cell = mGeckoAccessible.AsAccessible()->AsTableCell();
    return [NSValue valueWithRange:NSMakeRange(cell->RowIdx(), cell->RowExtent())];
  } else {
    ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
    return [NSValue valueWithRange:NSMakeRange(proxy->RowIdx(), proxy->RowExtent())];
  }
}

- (NSValue*)moxColumnIndexRange {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  if (mGeckoAccessible.IsAccessible()) {
    TableCellAccessible* cell = mGeckoAccessible.AsAccessible()->AsTableCell();
    return [NSValue valueWithRange:NSMakeRange(cell->ColIdx(), cell->ColExtent())];
  } else {
    ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
    return [NSValue valueWithRange:NSMakeRange(proxy->ColIdx(), proxy->ColExtent())];
  }
}

- (NSArray*)moxRowHeaderUIElements {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

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

- (NSArray*)moxColumnHeaderUIElements {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

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

@end
