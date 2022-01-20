/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozTableAccessible.h"
#import "nsCocoaUtils.h"
#import "MacUtils.h"
#import "RotorRules.h"

#include "AccIterator.h"
#include "LocalAccessible.h"
#include "TableAccessible.h"
#include "TableCellAccessible.h"
#include "XULTreeAccessible.h"
#include "Pivot.h"
#include "Relation.h"

using namespace mozilla;
using namespace mozilla::a11y;

enum CachedBool { eCachedBoolMiss, eCachedTrue, eCachedFalse };

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

  if (LocalAccessible* acc = [mParent geckoAccessible]->AsLocal()) {
    TableAccessible* table = acc->AsTable();
    MOZ_ASSERT(table, "Got null table when fetching column children!");
    uint32_t numRows = table->RowCount();

    for (uint32_t j = 0; j < numRows; j++) {
      LocalAccessible* cell = table->CellAt(j, mIndex);
      mozAccessible* nativeCell =
          cell ? GetNativeFromGeckoAccessible(cell) : nil;
      if ([nativeCell isAccessibilityElement]) {
        [mChildren addObject:nativeCell];
      }
    }

  } else if (RemoteAccessible* proxy = [mParent geckoAccessible]->AsRemote()) {
    uint32_t numRows = proxy->TableRowCount();

    for (uint32_t j = 0; j < numRows; j++) {
      RemoteAccessible* cell = proxy->TableCellAt(j, mIndex);
      mozAccessible* nativeCell =
          cell ? GetNativeFromGeckoAccessible(cell) : nil;
      if ([nativeCell isAccessibilityElement]) {
        [mChildren addObject:nativeCell];
      }
    }
  }

  return mChildren;
}

- (void)dealloc {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  [self invalidateChildren];
  [super dealloc];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (void)expire {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  [self invalidateChildren];

  mParent = nil;

  [super expire];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (BOOL)isExpired {
  MOZ_ASSERT((mChildren == nil && mParent == nil) == mIsExpired);

  return [super isExpired];
}

- (void)invalidateChildren {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  // make room for new children
  if (mChildren) {
    [mChildren release];
    mChildren = nil;
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

@end

@implementation mozTablePartAccessible

- (NSString*)moxTitle {
  return @"";
}

- (NSString*)moxRole {
  return [self isLayoutTablePart] ? NSAccessibilityGroupRole : [super moxRole];
}

- (void)handleAccessibleEvent:(uint32_t)eventType {
  if (![self isKindOfClass:[mozTableAccessible class]]) {
    // If we are not a table, we are a cell or a row.
    // Check to see if the event we're handling should
    // invalidate the mIsLayoutTable cache on our parent
    // table.
    if (eventType == nsIAccessibleEvent::EVENT_REORDER ||
        eventType == nsIAccessibleEvent::EVENT_OBJECT_ATTRIBUTE_CHANGED ||
        eventType == nsIAccessibleEvent::EVENT_TABLE_STYLING_CHANGED) {
      // Invalidate the cache on our parent table
      [self invalidateLayoutTableCache];
    }
  }

  [super handleAccessibleEvent:eventType];
}

- (BOOL)isLayoutTablePart {
  // mIsLayoutTable is a cache on each mozTableAccessible that stores
  // the previous result of calling IsProbablyLayoutTable in core. To see
  // how/when the cache is invalidated, view handleAccessibleEvent.
  // The cache contains one of three values from the CachedBool enum
  // defined in mozTableAccessible.h
  mozAccessible* parent = (mozAccessible*)[self moxUnignoredParent];
  if ([parent isKindOfClass:[mozTablePartAccessible class]]) {
    return [(mozTablePartAccessible*)parent isLayoutTablePart];
  } else if ([parent isKindOfClass:[mozOutlineAccessible class]]) {
    return [(mozOutlineAccessible*)parent isLayoutTablePart];
  }

  return NO;
}

- (void)invalidateLayoutTableCache {
  mozAccessible* parent = (mozAccessible*)[self moxUnignoredParent];
  if ([parent isKindOfClass:[mozTablePartAccessible class]]) {
    // We do this to prevent dispatching invalidateLayoutTableCache
    // on outlines or outline parts. This is possible here because
    // outline rows subclass table rows, which are a table part.
    // This means `parent` could be an outline, and there is no
    // cache on outlines to invalidate.
    [(mozTablePartAccessible*)parent invalidateLayoutTableCache];
  }
}
@end

@implementation mozTableAccessible

- (void)invalidateLayoutTableCache {
  mIsLayoutTable = eCachedBoolMiss;
}

- (BOOL)isLayoutTablePart {
  if (mIsLayoutTable != eCachedBoolMiss) {
    return mIsLayoutTable == eCachedTrue;
  }

  if (mGeckoAccessible->Role() == roles::TREE_TABLE) {
    // tree tables are never layout tables, and we shouldn't
    // query IsProbablyLayoutTable() on them, so we short
    // circuit here
    mIsLayoutTable = eCachedFalse;
    return false;
  }

  bool tableGuess;
  if (LocalAccessible* acc = mGeckoAccessible->AsLocal()) {
    tableGuess = acc->AsTable()->IsProbablyLayoutTable();
  } else {
    RemoteAccessible* proxy = mGeckoAccessible->AsRemote();
    tableGuess = proxy->TableIsProbablyForLayout();
  }

  mIsLayoutTable = tableGuess ? eCachedTrue : eCachedFalse;
  return tableGuess;
}

- (void)handleAccessibleEvent:(uint32_t)eventType {
  if (eventType == nsIAccessibleEvent::EVENT_REORDER ||
      eventType == nsIAccessibleEvent::EVENT_OBJECT_ATTRIBUTE_CHANGED ||
      eventType == nsIAccessibleEvent::EVENT_TABLE_STYLING_CHANGED) {
    [self invalidateLayoutTableCache];
    [self invalidateColumns];
  }

  [super handleAccessibleEvent:eventType];
}

- (void)dealloc {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  [self invalidateColumns];
  [super dealloc];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (NSNumber*)moxRowCount {
  MOZ_ASSERT(mGeckoAccessible);

  return mGeckoAccessible->IsLocal()
             ? @(mGeckoAccessible->AsLocal()->AsTable()->RowCount())
             : @(mGeckoAccessible->AsRemote()->TableRowCount());
}

- (NSNumber*)moxColumnCount {
  MOZ_ASSERT(mGeckoAccessible);

  return mGeckoAccessible->IsLocal()
             ? @(mGeckoAccessible->AsLocal()->AsTable()->ColCount())
             : @(mGeckoAccessible->AsRemote()->TableColumnCount());
}

- (NSArray*)moxRows {
  // Create a new array with the list of table rows.
  return [[self moxChildren]
      filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(
                                                   mozAccessible* child,
                                                   NSDictionary* bindings) {
        return [child isKindOfClass:[mozTableRowAccessible class]];
      }]];
}

- (NSArray*)moxColumns {
  MOZ_ASSERT(mGeckoAccessible);

  if (mColContainers) {
    return mColContainers;
  }

  mColContainers = [[NSMutableArray alloc] init];
  uint32_t numCols = 0;

  if (LocalAccessible* acc = mGeckoAccessible->AsLocal()) {
    numCols = acc->AsTable()->ColCount();
  } else {
    numCols = mGeckoAccessible->AsRemote()->TableColumnCount();
  }

  for (uint32_t i = 0; i < numCols; i++) {
    mozColumnContainer* container =
        [[mozColumnContainer alloc] initWithIndex:i andParent:self];
    [mColContainers addObject:container];
  }

  return mColContainers;
}

- (NSArray*)moxUnignoredChildren {
  if (![self isLayoutTablePart]) {
    return [[super moxUnignoredChildren]
        arrayByAddingObjectsFromArray:[self moxColumns]];
  }

  return [super moxUnignoredChildren];
}

- (NSArray*)moxColumnHeaderUIElements {
  MOZ_ASSERT(mGeckoAccessible);

  uint32_t numCols = 0;
  TableAccessible* table = nullptr;

  if (LocalAccessible* acc = mGeckoAccessible->AsLocal()) {
    table = mGeckoAccessible->AsLocal()->AsTable();
    numCols = table->ColCount();
  } else {
    numCols = mGeckoAccessible->AsRemote()->TableColumnCount();
  }

  NSMutableArray* colHeaders =
      [[[NSMutableArray alloc] initWithCapacity:numCols] autorelease];

  for (uint32_t i = 0; i < numCols; i++) {
    Accessible* cell;
    if (table) {
      cell = table->CellAt(0, i);
    } else {
      cell = mGeckoAccessible->AsRemote()->TableCellAt(0, i);
    }

    if (cell && cell->Role() == roles::COLUMNHEADER) {
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

  MOZ_ASSERT(mGeckoAccessible);

  Accessible* cell;
  if (mGeckoAccessible->IsLocal()) {
    cell = mGeckoAccessible->AsLocal()->AsTable()->CellAt(row, col);
  } else {
    cell = mGeckoAccessible->AsRemote()->TableCellAt(row, col);
  }

  if (!cell) {
    return nil;
  }

  return GetNativeFromGeckoAccessible(cell);
}

- (void)invalidateColumns {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;
  if (mColContainers) {
    [mColContainers release];
    mColContainers = nil;
  }
  NS_OBJC_END_TRY_IGNORE_BLOCK;
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
  MOZ_ASSERT(mGeckoAccessible);

  if (mGeckoAccessible->IsLocal()) {
    TableCellAccessible* cell = mGeckoAccessible->AsLocal()->AsTableCell();
    return
        [NSValue valueWithRange:NSMakeRange(cell->RowIdx(), cell->RowExtent())];
  } else {
    RemoteAccessible* proxy = mGeckoAccessible->AsRemote();
    return [NSValue
        valueWithRange:NSMakeRange(proxy->RowIdx(), proxy->RowExtent())];
  }
}

- (NSValue*)moxColumnIndexRange {
  MOZ_ASSERT(mGeckoAccessible);

  if (mGeckoAccessible->IsLocal()) {
    TableCellAccessible* cell = mGeckoAccessible->AsLocal()->AsTableCell();
    return
        [NSValue valueWithRange:NSMakeRange(cell->ColIdx(), cell->ColExtent())];
  } else {
    RemoteAccessible* proxy = mGeckoAccessible->AsRemote();
    return [NSValue
        valueWithRange:NSMakeRange(proxy->ColIdx(), proxy->ColExtent())];
  }
}

- (NSArray*)moxRowHeaderUIElements {
  MOZ_ASSERT(mGeckoAccessible);

  if (mGeckoAccessible->IsLocal()) {
    TableCellAccessible* cell = mGeckoAccessible->AsLocal()->AsTableCell();
    AutoTArray<LocalAccessible*, 10> headerCells;
    cell->RowHeaderCells(&headerCells);
    return utils::ConvertToNSArray(headerCells);
  } else {
    RemoteAccessible* proxy = mGeckoAccessible->AsRemote();
    nsTArray<RemoteAccessible*> headerCells;
    proxy->RowHeaderCells(&headerCells);
    return utils::ConvertToNSArray(headerCells);
  }
}

- (NSArray*)moxColumnHeaderUIElements {
  MOZ_ASSERT(mGeckoAccessible);

  if (mGeckoAccessible->IsLocal()) {
    TableCellAccessible* cell = mGeckoAccessible->AsLocal()->AsTableCell();
    AutoTArray<LocalAccessible*, 10> headerCells;
    cell->ColHeaderCells(&headerCells);
    return utils::ConvertToNSArray(headerCells);
  } else {
    RemoteAccessible* proxy = mGeckoAccessible->AsRemote();
    nsTArray<RemoteAccessible*> headerCells;
    proxy->ColHeaderCells(&headerCells);
    return utils::ConvertToNSArray(headerCells);
  }
}

@end

@implementation mozOutlineAccessible

- (BOOL)isLayoutTablePart {
  return NO;
}

- (NSArray*)moxRows {
  // Create a new array with the list of outline rows. We
  // use pivot here to do a deep traversal of all rows nested
  // in this outline, not just those which are direct
  // children, since that's what VO expects.
  NSMutableArray* allRows = [[[NSMutableArray alloc] init] autorelease];
  Pivot p = Pivot(mGeckoAccessible);
  OutlineRule rule = OutlineRule();
  Accessible* firstChild = mGeckoAccessible->FirstChild();
  Accessible* match = p.Next(firstChild, rule, true);
  while (match) {
    [allRows addObject:GetNativeFromGeckoAccessible(match)];
    match = p.Next(match, rule);
  }
  return allRows;
}

- (NSArray*)moxColumns {
  if (LocalAccessible* acc = mGeckoAccessible->AsLocal()) {
    if (acc->IsContent() && acc->GetContent()->IsXULElement(nsGkAtoms::tree)) {
      XULTreeAccessible* treeAcc = (XULTreeAccessible*)acc;
      NSMutableArray* cols = [[[NSMutableArray alloc] init] autorelease];
      // XUL trees store their columns in a group at the tree's first
      // child. Here, we iterate over that group to get each column's
      // native accessible and add it to our col array.
      LocalAccessible* treeColumns = treeAcc->LocalChildAt(0);
      if (treeColumns) {
        uint32_t colCount = treeColumns->ChildCount();
        for (uint32_t i = 0; i < colCount; i++) {
          LocalAccessible* treeColumnItem = treeColumns->LocalChildAt(i);
          [cols addObject:GetNativeFromGeckoAccessible(treeColumnItem)];
        }
        return cols;
      }
    }
  }
  // Webkit says we shouldn't expose any cols for aria-tree
  // so we return an empty array here
  return @[];
}

- (NSArray*)moxSelectedRows {
  NSMutableArray* selectedRows = [[[NSMutableArray alloc] init] autorelease];
  NSArray* allRows = [self moxRows];
  for (mozAccessible* row in allRows) {
    if ([row stateWithMask:states::SELECTED] != 0) {
      [selectedRows addObject:row];
    }
  }

  return selectedRows;
}

- (NSString*)moxOrientation {
  return NSAccessibilityVerticalOrientationValue;
}

@end

@implementation mozOutlineRowAccessible

- (BOOL)isLayoutTablePart {
  return NO;
}

- (NSNumber*)moxDisclosing {
  return @([self stateWithMask:states::EXPANDED] != 0);
}

- (void)moxSetDisclosing:(NSNumber*)disclosing {
  // VoiceOver requires this to be settable, but doesn't
  // require it actually affect our disclosing state.
  // We expose the attr as settable with this method
  // but do nothing to actually set it.
  return;
}

- (NSNumber*)moxExpanded {
  return @([self stateWithMask:states::EXPANDED] != 0);
}

- (id)moxDisclosedByRow {
  // According to webkit: this attr corresponds to the row
  // that contains this row. It should be the same as the
  // first parent that is a treeitem. If the parent is the tree
  // itself, this should be nil. This is tricky for xul trees because
  // all rows are direct children of the outline; they use
  // relations to expose their heirarchy structure.

  // first we check the relations to see if we're in a xul tree
  // with weird row semantics
  NSArray<mozAccessible*>* disclosingRows =
      [self getRelationsByType:RelationType::NODE_CHILD_OF];
  mozAccessible* disclosingRow = [disclosingRows firstObject];

  if (disclosingRow) {
    // if we find a row from our relation check,
    // verify it isn't the outline itself and return
    // appropriately
    if ([[disclosingRow moxRole] isEqualToString:@"AXOutline"]) {
      return nil;
    }

    return disclosingRow;
  }

  mozAccessible* parent = (mozAccessible*)[self moxUnignoredParent];
  // otherwise, its likely we're in an aria tree, so we can use
  // these role and subrole checks
  if ([[parent moxRole] isEqualToString:@"AXOutline"]) {
    return nil;
  }

  if ([[parent moxSubrole] isEqualToString:@"AXOutlineRow"]) {
    disclosingRow = parent;
  }

  return nil;
}

- (NSNumber*)moxDisclosureLevel {
  GroupPos groupPos;
  if (LocalAccessible* acc = mGeckoAccessible->AsLocal()) {
    groupPos = acc->GroupPosition();
  } else if (RemoteAccessible* proxy = mGeckoAccessible->AsRemote()) {
    groupPos = proxy->GroupPosition();
  }
  // mac expects 0-indexed levels, but groupPos.level is 1-indexed
  // so we subtract 1 here for levels above 0
  return groupPos.level > 0 ? @(groupPos.level - 1) : @(groupPos.level);
}

- (NSArray*)moxDisclosedRows {
  // According to webkit: this attr corresponds to the rows
  // that are considered inside this row. Again, this is weird for
  // xul trees so we have to use relations first and then fall-back
  // to the children filter for non-xul outlines.

  // first we check the relations to see if we're in a xul tree
  // with weird row semantics
  if (NSArray* disclosedRows =
          [self getRelationsByType:RelationType::NODE_PARENT_OF]) {
    // if we find rows from our relation check, return them here
    return disclosedRows;
  }

  // otherwise, filter our children for outline rows
  return [[self moxChildren]
      filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(
                                                   mozAccessible* child,
                                                   NSDictionary* bindings) {
        return [child isKindOfClass:[mozOutlineRowAccessible class]];
      }]];
}

- (NSNumber*)moxIndex {
  id<MOXAccessible> outline =
      [self moxFindAncestor:^BOOL(id<MOXAccessible> moxAcc, BOOL* stop) {
        return [[moxAcc moxRole] isEqualToString:@"AXOutline"];
      }];

  NSUInteger index = [[outline moxRows] indexOfObjectIdenticalTo:self];
  return index == NSNotFound ? nil : @(index);
}

- (NSString*)moxLabel {
  nsAutoString title;
  mGeckoAccessible->Name(title);

  // XXX: When parsing outlines built with ul/lu's, we
  // include the bullet in this description even
  // though webkit doesn't. Not all outlines are built with
  // ul/lu's so we can't strip the first character here.

  return nsCocoaUtils::ToNSString(title);
}

enum CheckedState {
  kUncheckable = -1,
  kUnchecked = 0,
  kChecked = 1,
  kMixed = 2
};

- (int)checkedValue {
  uint64_t state = [self
      stateWithMask:(states::CHECKABLE | states::CHECKED | states::MIXED)];

  if (state & states::CHECKABLE) {
    if (state & states::CHECKED) {
      return kChecked;
    }

    if (state & states::MIXED) {
      return kMixed;
    }

    return kUnchecked;
  }

  return kUncheckable;
}

- (id)moxValue {
  int checkedValue = [self checkedValue];
  return checkedValue >= 0 ? @(checkedValue) : nil;
}

- (void)stateChanged:(uint64_t)state isEnabled:(BOOL)enabled {
  [super stateChanged:state isEnabled:enabled];

  if (state & states::EXPANDED) {
    // If the EXPANDED state is updated, fire appropriate events on the
    // outline row.
    [self moxPostNotification:(enabled
                                   ? NSAccessibilityRowExpandedNotification
                                   : NSAccessibilityRowCollapsedNotification)];
  }

  if (state & (states::CHECKED | states::CHECKABLE | states::MIXED)) {
    // If the MIXED, CHECKED or CHECKABLE state changes, update the value we
    // expose for the row, which communicates checked status.
    [self moxPostNotification:NSAccessibilityValueChangedNotification];
  }
}

@end
