/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozTableAccessible.h"
#import "nsCocoaUtils.h"

@implementation mozTablePartAccessible

- (id)accessibilityAttributeValue:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ([attribute isEqualToString:NSAccessibilityTitleAttribute]) return @"";

  return [super accessibilityAttributeValue:attribute];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)isLayoutTablePart;
{
  if (Accessible* accWrap = [self getGeckoAccessible]) {
    while (accWrap) {
      if (accWrap->IsTable()) {
        return accWrap->AsTable()->IsProbablyLayoutTable();
      }
      accWrap = accWrap->Parent();
    }
    return false;
  }

  if (ProxyAccessible* proxy = [self getProxyAccessible]) {
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
    tableAttrs = [[NSArray alloc] initWithArray:tempArray];
    [tempArray release];
  }

  return [additionalAttributes arrayByAddingObjectsFromArray:tableAttrs];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  if (AccessibleWrap* accWrap = [self getGeckoAccessible]) {
    TableAccessible* table = accWrap->AsTable();
    if ([attribute isEqualToString:NSAccessibilityRowCountAttribute]) return @(table->RowCount());
    if ([attribute isEqualToString:NSAccessibilityColumnCountAttribute])
      return @(table->ColCount());
    if ([attribute isEqualToString:NSAccessibilityRowsAttribute]) {
      // Create a new array with the list of table rows.
      NSMutableArray* nativeArray = [[NSMutableArray alloc] init];
      uint32_t totalCount = accWrap->ChildCount();
      for (uint32_t i = 0; i < totalCount; i++) {
        if (accWrap->GetChildAt(i)->IsTableRow()) {
          mozAccessible* curNative = GetNativeFromGeckoAccessible(accWrap->GetChildAt(i));
          if (curNative) [nativeArray addObject:GetObjectOrRepresentedView(curNative)];
        }
      }
      return nativeArray;
    }
  } else if (ProxyAccessible* proxy = [self getProxyAccessible]) {
    if ([attribute isEqualToString:NSAccessibilityRowCountAttribute])
      return @(proxy->TableRowCount());
    if ([attribute isEqualToString:NSAccessibilityColumnCountAttribute])
      return @(proxy->TableColumnCount());
    if ([attribute isEqualToString:NSAccessibilityRowsAttribute]) {
      // Create a new array with the list of table rows.
      NSMutableArray* nativeArray = [[NSMutableArray alloc] init];
      uint32_t totalCount = proxy->ChildrenCount();
      for (uint32_t i = 0; i < totalCount; i++) {
        if (proxy->ChildAt(i)->IsTableRow()) {
          mozAccessible* curNative = GetNativeFromProxy(proxy->ChildAt(i));
          if (curNative) [nativeArray addObject:GetObjectOrRepresentedView(curNative)];
        }
      }
      return nativeArray;
    }
  }

  return [super accessibilityAttributeValue:attribute];
}
@end

@implementation mozTableRowAccessible
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
  if (AccessibleWrap* accWrap = [self getGeckoAccessible]) {
    if ([attribute isEqualToString:NSAccessibilityIndexAttribute]) {
      // Count the number of rows before that one to obtain the row index.
      uint32_t index = 0;
      Accessible* parent = accWrap->Parent();
      if (parent) {
        for (int32_t i = accWrap->IndexInParent() - 1; i >= 0; i--) {
          if (parent->GetChildAt(i)->IsTableRow()) {
            index++;
          }
        }
      }
      return [NSNumber numberWithUnsignedInteger:index];
    }
  } else if (ProxyAccessible* proxy = [self getProxyAccessible]) {
    if ([attribute isEqualToString:NSAccessibilityIndexAttribute]) {
      // Count the number of rows before that one to obtain the row index.
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
  if (AccessibleWrap* accWrap = [self getGeckoAccessible]) {
    TableCellAccessible* cell = accWrap->AsTableCell();
    if ([attribute isEqualToString:NSAccessibilityRowIndexRangeAttribute])
      return [NSValue valueWithRange:NSMakeRange(cell->RowIdx(), cell->RowExtent())];
    if ([attribute isEqualToString:NSAccessibilityColumnIndexRangeAttribute])
      return [NSValue valueWithRange:NSMakeRange(cell->ColIdx(), cell->ColExtent())];
    if ([attribute isEqualToString:NSAccessibilityRowHeaderUIElementsAttribute]) {
      AutoTArray<Accessible*, 10> headerCells;
      cell->RowHeaderCells(&headerCells);
      return ConvertToNSArray(headerCells);
    }
    if ([attribute isEqualToString:NSAccessibilityColumnHeaderUIElementsAttribute]) {
      AutoTArray<Accessible*, 10> headerCells;
      cell->ColHeaderCells(&headerCells);
      return ConvertToNSArray(headerCells);
    }
  } else if (ProxyAccessible* proxy = [self getProxyAccessible]) {
    if ([attribute isEqualToString:NSAccessibilityRowIndexRangeAttribute])
      return [NSValue valueWithRange:NSMakeRange(proxy->RowIdx(), proxy->RowExtent())];
    if ([attribute isEqualToString:NSAccessibilityColumnIndexRangeAttribute])
      return [NSValue valueWithRange:NSMakeRange(proxy->ColIdx(), proxy->ColExtent())];
    if ([attribute isEqualToString:NSAccessibilityRowHeaderUIElementsAttribute]) {
      nsTArray<ProxyAccessible*> headerCells;
      proxy->RowHeaderCells(&headerCells);
      return ConvertToNSArray(headerCells);
    }
    if ([attribute isEqualToString:NSAccessibilityColumnHeaderUIElementsAttribute]) {
      nsTArray<ProxyAccessible*> headerCells;
      proxy->ColHeaderCells(&headerCells);
      return ConvertToNSArray(headerCells);
    }
  }

  return [super accessibilityAttributeValue:attribute];
}
@end
