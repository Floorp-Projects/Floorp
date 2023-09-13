/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozTableAccessible.h"
#import "nsCocoaUtils.h"
#import "MacUtils.h"

#include "AccIterator.h"
#include "LocalAccessible.h"
#include "mozilla/a11y/TableAccessible.h"
#include "mozilla/a11y/TableCellAccessible.h"
#include "nsAccessibilityService.h"
#include "nsIAccessiblePivot.h"
#include "XULTreeAccessible.h"
#include "Pivot.h"
#include "nsAccUtils.h"
#include "Relation.h"

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

  TableAccessible* table = [mParent geckoAccessible]->AsTable();
  MOZ_ASSERT(table, "Got null table when fetching column children!");
  uint32_t numRows = table->RowCount();

  for (uint32_t j = 0; j < numRows; j++) {
    Accessible* cell = table->CellAt(j, mIndex);
    mozAccessible* nativeCell = cell ? GetNativeFromGeckoAccessible(cell) : nil;
    if ([nativeCell isAccessibilityElement]) {
      [mChildren addObject:nativeCell];
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

- (BOOL)isLayoutTablePart {
  mozAccessible* parent = (mozAccessible*)[self moxUnignoredParent];
  if ([parent isKindOfClass:[mozTablePartAccessible class]]) {
    return [(mozTablePartAccessible*)parent isLayoutTablePart];
  } else if ([parent isKindOfClass:[mozOutlineAccessible class]]) {
    return [(mozOutlineAccessible*)parent isLayoutTablePart];
  }

  return NO;
}
@end

@implementation mozTableAccessible

- (BOOL)isLayoutTablePart {
  if (mGeckoAccessible->Role() == roles::TREE_TABLE) {
    // tree tables are never layout tables, and we shouldn't
    // query IsProbablyLayoutTable() on them, so we short
    // circuit here
    return false;
  }

  // For LocalAccessible and cached RemoteAccessible, we could use
  // AsTable()->IsProbablyLayoutTable(). However, if the cache is enabled,
  // that would build the table cache, which is pointless for layout tables on
  // Mac because layout tables are AXGroups and do not expose table properties
  // like AXRows, AXColumns, etc.
  if (LocalAccessible* acc = mGeckoAccessible->AsLocal()) {
    return acc->AsTable()->IsProbablyLayoutTable();
  }
  RemoteAccessible* proxy = mGeckoAccessible->AsRemote();
  return proxy->TableIsProbablyForLayout();
}

- (void)handleAccessibleEvent:(uint32_t)eventType {
  if (eventType == nsIAccessibleEvent::EVENT_REORDER ||
      eventType == nsIAccessibleEvent::EVENT_OBJECT_ATTRIBUTE_CHANGED) {
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

- (void)expire {
  [self invalidateColumns];
  [super expire];
}

- (NSNumber*)moxRowCount {
  MOZ_ASSERT(mGeckoAccessible);

  return @(mGeckoAccessible->AsTable()->RowCount());
}

- (NSNumber*)moxColumnCount {
  MOZ_ASSERT(mGeckoAccessible);

  return @(mGeckoAccessible->AsTable()->ColCount());
}

- (NSArray*)moxRows {
  // Create a new array with the list of table rows.
  NSArray* children = [self moxChildren];
  NSMutableArray* rows = [[[NSMutableArray alloc] init] autorelease];
  for (mozAccessible* curr : children) {
    if ([curr isKindOfClass:[mozTableRowAccessible class]]) {
      [rows addObject:curr];
    } else if ([[curr moxRole] isEqualToString:@"AXGroup"]) {
      // Plain thead/tbody elements are removed from the core a11y tree and
      // replaced with their subtree, but thead/tbody elements with click
      // handlers are not -- they remain as groups. We need to expose any
      // rows they contain as rows of the parent table.
      [rows
          addObjectsFromArray:[[curr moxChildren]
                                  filteredArrayUsingPredicate:
                                      [NSPredicate predicateWithBlock:^BOOL(
                                                       mozAccessible* child,
                                                       NSDictionary* bindings) {
                                        return [child
                                            isKindOfClass:[mozTableRowAccessible
                                                              class]];
                                      }]]];
    }
  }

  return rows;
}

- (NSArray*)moxColumns {
  MOZ_ASSERT(mGeckoAccessible);

  if (mColContainers) {
    return mColContainers;
  }

  mColContainers = [[NSMutableArray alloc] init];
  uint32_t numCols = 0;

  numCols = mGeckoAccessible->AsTable()->ColCount();
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

  table = mGeckoAccessible->AsTable();
  numCols = table->ColCount();
  NSMutableArray* colHeaders =
      [[[NSMutableArray alloc] initWithCapacity:numCols] autorelease];

  for (uint32_t i = 0; i < numCols; i++) {
    Accessible* cell = table->CellAt(0, i);
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

  Accessible* cell = mGeckoAccessible->AsTable()->CellAt(row, col);
  if (!cell) {
    return nil;
  }

  return GetNativeFromGeckoAccessible(cell);
}

- (void)invalidateColumns {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;
  if (mColContainers) {
    for (mozColumnContainer* col in mColContainers) {
      [col expire];
    }
    [mColContainers release];
    mColContainers = nil;
  }
  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

@end

@interface mozTableRowAccessible ()
- (mozTableAccessible*)getTableParent;
@end

@implementation mozTableRowAccessible

- (mozTableAccessible*)getTableParent {
  id tableParent = static_cast<mozTableAccessible*>(
      [self moxFindAncestor:^BOOL(id curr, BOOL* stop) {
        if ([curr isKindOfClass:[mozOutlineAccessible class]]) {
          // Outline rows are a kind of table row, so it's possible
          // we're trying to call getTableParent on an outline row here.
          // Stop searching.
          *stop = YES;
        }
        return [curr isKindOfClass:[mozTableAccessible class]];
      }]);

  return [tableParent isKindOfClass:[mozTableAccessible class]] ? tableParent
                                                                : nil;
}

- (void)handleAccessibleEvent:(uint32_t)eventType {
  if (eventType == nsIAccessibleEvent::EVENT_REORDER) {
    // It is possible for getTableParent to return nil if we're
    // handling a reorder on an outilne row. Outlines don't have
    // columns, so there's nothing to do here and this will no-op.
    [[self getTableParent] invalidateColumns];
  }

  [super handleAccessibleEvent:eventType];
}

- (NSNumber*)moxIndex {
  return @([[[self getTableParent] moxRows] indexOfObjectIdenticalTo:self]);
}

@end

@implementation mozTableCellAccessible

- (NSValue*)moxRowIndexRange {
  MOZ_ASSERT(mGeckoAccessible);

  TableCellAccessible* cell = mGeckoAccessible->AsTableCell();
  return
      [NSValue valueWithRange:NSMakeRange(cell->RowIdx(), cell->RowExtent())];
}

- (NSValue*)moxColumnIndexRange {
  MOZ_ASSERT(mGeckoAccessible);

  TableCellAccessible* cell = mGeckoAccessible->AsTableCell();
  return
      [NSValue valueWithRange:NSMakeRange(cell->ColIdx(), cell->ColExtent())];
}

- (NSArray*)moxRowHeaderUIElements {
  MOZ_ASSERT(mGeckoAccessible);

  TableCellAccessible* cell = mGeckoAccessible->AsTableCell();
  AutoTArray<Accessible*, 10> headerCells;
  if (cell) {
    cell->RowHeaderCells(&headerCells);
  }
  return utils::ConvertToNSArray(headerCells);
}

- (NSArray*)moxColumnHeaderUIElements {
  MOZ_ASSERT(mGeckoAccessible);

  TableCellAccessible* cell = mGeckoAccessible->AsTableCell();
  AutoTArray<Accessible*, 10> headerCells;
  if (cell) {
    cell->ColHeaderCells(&headerCells);
  }
  return utils::ConvertToNSArray(headerCells);
}

@end

/**
 * This rule matches all accessibles with roles::OUTLINEITEM. If
 * outlines are nested, it ignores the nested subtree and returns
 * only items which are descendants of the primary outline.
 */
class OutlineRule : public PivotRule {
 public:
  uint16_t Match(Accessible* aAcc) override {
    uint16_t result = nsIAccessibleTraversalRule::FILTER_IGNORE;

    if (nsAccUtils::MustPrune(aAcc)) {
      result |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
    }

    if (![GetNativeFromGeckoAccessible(aAcc) isAccessibilityElement]) {
      return result;
    }

    if (aAcc->Role() == roles::OUTLINE) {
      // if the accessible is an outline, we ignore all children
      result |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
    } else if (aAcc->Role() == roles::OUTLINEITEM) {
      // if the accessible is not an outline item, we match here
      result |= nsIAccessibleTraversalRule::FILTER_MATCH;
    }

    return result;
  }
};

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
  GroupPos groupPos = mGeckoAccessible->GroupPosition();

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
