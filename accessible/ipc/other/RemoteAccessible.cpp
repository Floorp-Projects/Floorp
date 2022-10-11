/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteAccessible.h"
#include "mozilla/a11y/DocAccessibleParent.h"
#include "DocAccessible.h"
#include "AccAttributes.h"
#include "mozilla/a11y/DocManager.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/Unused.h"
#include "mozilla/a11y/Platform.h"
#include "Relation.h"
#include "RelationType.h"
#include "mozilla/a11y/Role.h"
#include "mozilla/StaticPrefs_accessibility.h"

namespace mozilla {
namespace a11y {

uint64_t RemoteAccessible::State() {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::State();
  }
  uint64_t state = 0;
  Unused << mDoc->SendState(mID, &state);
  return state;
}

uint64_t RemoteAccessible::NativeState() const {
  uint64_t state = 0;
  Unused << mDoc->SendNativeState(mID, &state);
  return state;
}

ENameValueFlag RemoteAccessible::Name(nsString& aName) const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::Name(aName);
  }

  uint32_t flag = 0;
  Unused << mDoc->SendName(mID, &aName, &flag);
  return static_cast<ENameValueFlag>(flag);
}

void RemoteAccessible::Value(nsString& aValue) const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    RemoteAccessibleBase<RemoteAccessible>::Value(aValue);
    return;
  }

  Unused << mDoc->SendValue(mID, &aValue);
}

void RemoteAccessible::Help(nsString& aHelp) const {
  Unused << mDoc->SendHelp(mID, &aHelp);
}

void RemoteAccessible::Description(nsString& aDesc) const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    RemoteAccessibleBase<RemoteAccessible>::Description(aDesc);
    return;
  }

  Unused << mDoc->SendDescription(mID, &aDesc);
}

already_AddRefed<AccAttributes> RemoteAccessible::Attributes() {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::Attributes();
  }
  RefPtr<AccAttributes> attrs;
  Unused << mDoc->SendAttributes(mID, &attrs);
  return attrs.forget();
}

Relation RemoteAccessible::RelationByType(RelationType aType) const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::RelationByType(aType);
  }

  nsTArray<uint64_t> targetIDs;
  Unused << mDoc->SendRelationByType(mID, static_cast<uint32_t>(aType),
                                     &targetIDs);
  return Relation(new RemoteAccIterator(std::move(targetIDs), Document()));
}

void RemoteAccessible::Relations(
    nsTArray<RelationType>* aTypes,
    nsTArray<nsTArray<RemoteAccessible*>>* aTargetSets) const {
  nsTArray<RelationTargets> ipcRelations;
  Unused << mDoc->SendRelations(mID, &ipcRelations);

  size_t relationCount = ipcRelations.Length();
  aTypes->SetCapacity(relationCount);
  aTargetSets->SetCapacity(relationCount);
  for (size_t i = 0; i < relationCount; i++) {
    uint32_t type = ipcRelations[i].Type();
    if (type > static_cast<uint32_t>(RelationType::LAST)) continue;

    size_t targetCount = ipcRelations[i].Targets().Length();
    nsTArray<RemoteAccessible*> targets(targetCount);
    for (size_t j = 0; j < targetCount; j++) {
      if (RemoteAccessible* proxy =
              mDoc->GetAccessible(ipcRelations[i].Targets()[j])) {
        targets.AppendElement(proxy);
      }
    }

    if (targets.IsEmpty()) continue;

    aTargetSets->AppendElement(std::move(targets));
    aTypes->AppendElement(static_cast<RelationType>(type));
  }
}

bool RemoteAccessible::IsSearchbox() const {
  bool retVal = false;
  Unused << mDoc->SendIsSearchbox(mID, &retVal);
  return retVal;
}

nsAtom* RemoteAccessible::LandmarkRole() const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::LandmarkRole();
  }

  nsString landmark;
  Unused << mDoc->SendLandmarkRole(mID, &landmark);
  return NS_GetStaticAtom(landmark);
}

nsStaticAtom* RemoteAccessible::ARIARoleAtom() const {
  nsString role;
  Unused << mDoc->SendARIARoleAtom(mID, &role);
  return NS_GetStaticAtom(role);
}

GroupPos RemoteAccessible::GroupPosition() {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::GroupPosition();
  }

  GroupPos groupPos;
  Unused << mDoc->SendGroupPosition(mID, &groupPos.level, &groupPos.setSize,
                                    &groupPos.posInSet);
  return groupPos;
}

void RemoteAccessible::ScrollToPoint(uint32_t aScrollType, int32_t aX,
                                     int32_t aY) {
  Unused << mDoc->SendScrollToPoint(mID, aScrollType, aX, aY);
}

void RemoteAccessible::Announce(const nsString& aAnnouncement,
                                uint16_t aPriority) {
  Unused << mDoc->SendAnnounce(mID, aAnnouncement, aPriority);
}

int32_t RemoteAccessible::CaretLineNumber() {
  int32_t line = -1;
  Unused << mDoc->SendCaretOffset(mID, &line);
  return line;
}

int32_t RemoteAccessible::CaretOffset() const {
  int32_t offset = 0;
  Unused << mDoc->SendCaretOffset(mID, &offset);
  return offset;
}

uint32_t RemoteAccessible::CharacterCount() const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::CharacterCount();
  }
  int32_t count = 0;
  Unused << mDoc->SendCharacterCount(mID, &count);
  return count;
}

int32_t RemoteAccessible::SelectionCount() {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::SelectionCount();
  }
  int32_t count = 0;
  Unused << mDoc->SendSelectionCount(mID, &count);
  return count;
}

void RemoteAccessible::TextSubstring(int32_t aStartOffset, int32_t aEndOffset,
                                     nsAString& aText) const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::TextSubstring(
        aStartOffset, aEndOffset, aText);
  }

  bool valid;
  nsString text;
  Unused << mDoc->SendTextSubstring(mID, aStartOffset, aEndOffset, &text,
                                    &valid);
  aText = std::move(text);
}

void RemoteAccessible::TextAfterOffset(int32_t aOffset,
                                       AccessibleTextBoundary aBoundaryType,
                                       int32_t* aStartOffset,
                                       int32_t* aEndOffset, nsAString& aText) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::TextAfterOffset(
        aOffset, aBoundaryType, aStartOffset, aEndOffset, aText);
  }
  nsString text;
  Unused << mDoc->SendGetTextAfterOffset(mID, aOffset, aBoundaryType, &text,
                                         aStartOffset, aEndOffset);
  aText = std::move(text);
}

void RemoteAccessible::TextAtOffset(int32_t aOffset,
                                    AccessibleTextBoundary aBoundaryType,
                                    int32_t* aStartOffset, int32_t* aEndOffset,
                                    nsAString& aText) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::TextAtOffset(
        aOffset, aBoundaryType, aStartOffset, aEndOffset, aText);
  }
  nsString text;
  Unused << mDoc->SendGetTextAtOffset(mID, aOffset, aBoundaryType, &text,
                                      aStartOffset, aEndOffset);
  aText = std::move(text);
}

void RemoteAccessible::TextBeforeOffset(int32_t aOffset,
                                        AccessibleTextBoundary aBoundaryType,
                                        int32_t* aStartOffset,
                                        int32_t* aEndOffset, nsAString& aText) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::TextBeforeOffset(
        aOffset, aBoundaryType, aStartOffset, aEndOffset, aText);
  }
  nsString text;
  Unused << mDoc->SendGetTextBeforeOffset(mID, aOffset, aBoundaryType, &text,
                                          aStartOffset, aEndOffset);
  aText = std::move(text);
}

char16_t RemoteAccessible::CharAt(int32_t aOffset) {
  uint16_t retval = 0;
  Unused << mDoc->SendCharAt(mID, aOffset, &retval);
  return static_cast<char16_t>(retval);
}

already_AddRefed<AccAttributes> RemoteAccessible::TextAttributes(
    bool aIncludeDefAttrs, int32_t aOffset, int32_t* aStartOffset,
    int32_t* aEndOffset) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::TextAttributes(
        aIncludeDefAttrs, aOffset, aStartOffset, aEndOffset);
  }
  RefPtr<AccAttributes> attrs;
  Unused << mDoc->SendTextAttributes(mID, aIncludeDefAttrs, aOffset, &attrs,
                                     aStartOffset, aEndOffset);
  return attrs.forget();
}

already_AddRefed<AccAttributes> RemoteAccessible::DefaultTextAttributes() {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::DefaultTextAttributes();
  }
  RefPtr<AccAttributes> attrs;
  Unused << mDoc->SendDefaultTextAttributes(mID, &attrs);
  return attrs.forget();
}

LayoutDeviceIntRect RemoteAccessible::TextBounds(int32_t aStartOffset,
                                                 int32_t aEndOffset,
                                                 uint32_t aCoordType) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    MOZ_ASSERT(IsHyperText(), "is not hypertext?");
    return RemoteAccessibleBase<RemoteAccessible>::TextBounds(
        aStartOffset, aEndOffset, aCoordType);
  }

  LayoutDeviceIntRect rect;
  Unused << mDoc->SendTextBounds(mID, aStartOffset, aEndOffset, aCoordType,
                                 &rect);
  return rect;
}

LayoutDeviceIntRect RemoteAccessible::CharBounds(int32_t aOffset,
                                                 uint32_t aCoordType) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    MOZ_ASSERT(IsHyperText(), "is not hypertext?");
    return RemoteAccessibleBase<RemoteAccessible>::CharBounds(aOffset,
                                                              aCoordType);
  }

  LayoutDeviceIntRect rect;
  Unused << mDoc->SendCharBounds(mID, aOffset, aCoordType, &rect);
  return rect;
}

int32_t RemoteAccessible::OffsetAtPoint(int32_t aX, int32_t aY,
                                        uint32_t aCoordType) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    MOZ_ASSERT(IsHyperText(), "is not hypertext?");
    return RemoteAccessibleBase<RemoteAccessible>::OffsetAtPoint(aX, aY,
                                                                 aCoordType);
  }

  int32_t retVal = -1;
  Unused << mDoc->SendOffsetAtPoint(mID, aX, aY, aCoordType, &retVal);
  return retVal;
}

bool RemoteAccessible::SelectionBoundsAt(int32_t aSelectionNum, nsString& aData,
                                         int32_t* aStartOffset,
                                         int32_t* aEndOffset) {
  bool retVal = false;
  Unused << mDoc->SendSelectionBoundsAt(mID, aSelectionNum, &retVal, &aData,
                                        aStartOffset, aEndOffset);
  return retVal;
}

bool RemoteAccessible::SetSelectionBoundsAt(int32_t aSelectionNum,
                                            int32_t aStartOffset,
                                            int32_t aEndOffset) {
  bool retVal = false;
  Unused << mDoc->SendSetSelectionBoundsAt(mID, aSelectionNum, aStartOffset,
                                           aEndOffset, &retVal);
  return retVal;
}

bool RemoteAccessible::AddToSelection(int32_t aStartOffset,
                                      int32_t aEndOffset) {
  bool retVal = false;
  Unused << mDoc->SendAddToSelection(mID, aStartOffset, aEndOffset, &retVal);
  return retVal;
}

bool RemoteAccessible::RemoveFromSelection(int32_t aSelectionNum) {
  bool retVal = false;
  Unused << mDoc->SendRemoveFromSelection(mID, aSelectionNum, &retVal);
  return retVal;
}

void RemoteAccessible::ScrollSubstringTo(int32_t aStartOffset,
                                         int32_t aEndOffset,
                                         uint32_t aScrollType) {
  Unused << mDoc->SendScrollSubstringTo(mID, aStartOffset, aEndOffset,
                                        aScrollType);
}

void RemoteAccessible::ScrollSubstringToPoint(int32_t aStartOffset,
                                              int32_t aEndOffset,
                                              uint32_t aCoordinateType,
                                              int32_t aX, int32_t aY) {
  Unused << mDoc->SendScrollSubstringToPoint(mID, aStartOffset, aEndOffset,
                                             aCoordinateType, aX, aY);
}

void RemoteAccessible::Text(nsString* aText) {
  Unused << mDoc->SendText(mID, aText);
}

void RemoteAccessible::ReplaceText(const nsString& aText) {
  Unused << mDoc->SendReplaceText(mID, aText);
}

bool RemoteAccessible::InsertText(const nsString& aText, int32_t aPosition) {
  bool valid;
  Unused << mDoc->SendInsertText(mID, aText, aPosition, &valid);
  return valid;
}

bool RemoteAccessible::CopyText(int32_t aStartPos, int32_t aEndPos) {
  bool valid;
  Unused << mDoc->SendCopyText(mID, aStartPos, aEndPos, &valid);
  return valid;
}

bool RemoteAccessible::CutText(int32_t aStartPos, int32_t aEndPos) {
  bool valid;
  Unused << mDoc->SendCutText(mID, aStartPos, aEndPos, &valid);
  return valid;
}

bool RemoteAccessible::DeleteText(int32_t aStartPos, int32_t aEndPos) {
  bool valid;
  Unused << mDoc->SendDeleteText(mID, aStartPos, aEndPos, &valid);
  return valid;
}

bool RemoteAccessible::PasteText(int32_t aPosition) {
  bool valid;
  Unused << mDoc->SendPasteText(mID, aPosition, &valid);
  return valid;
}

uint32_t RemoteAccessible::StartOffset() {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::StartOffset();
  }
  uint32_t retVal = 0;
  bool ok;
  Unused << mDoc->SendStartOffset(mID, &retVal, &ok);
  return retVal;
}

uint32_t RemoteAccessible::EndOffset() {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::EndOffset();
  }
  bool ok;
  uint32_t retVal = 0;
  Unused << mDoc->SendEndOffset(mID, &retVal, &ok);
  return retVal;
}

bool RemoteAccessible::IsLinkValid() {
  bool retVal = false;
  Unused << mDoc->SendIsLinkValid(mID, &retVal);
  return retVal;
}

uint32_t RemoteAccessible::AnchorCount(bool* aOk) {
  uint32_t retVal = 0;
  Unused << mDoc->SendAnchorCount(mID, &retVal, aOk);
  return retVal;
}

void RemoteAccessible::AnchorURIAt(uint32_t aIndex, nsCString& aURI,
                                   bool* aOk) {
  Unused << mDoc->SendAnchorURIAt(mID, aIndex, &aURI, aOk);
}

RemoteAccessible* RemoteAccessible::AnchorAt(uint32_t aIndex) {
  uint64_t id = 0;
  bool ok = false;
  Unused << mDoc->SendAnchorAt(mID, aIndex, &id, &ok);
  return ok ? mDoc->GetAccessible(id) : nullptr;
}

uint32_t RemoteAccessible::LinkCount() {
  uint32_t retVal = 0;
  Unused << mDoc->SendLinkCount(mID, &retVal);
  return retVal;
}

RemoteAccessible* RemoteAccessible::LinkAt(const uint32_t& aIndex) {
  uint64_t linkID = 0;
  bool ok = false;
  Unused << mDoc->SendLinkAt(mID, aIndex, &linkID, &ok);
  return ok ? mDoc->GetAccessible(linkID) : nullptr;
}

int32_t RemoteAccessible::LinkIndexAtOffset(uint32_t aOffset) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::LinkIndexAtOffset(aOffset);
  }
  int32_t retVal = -1;
  Unused << mDoc->SendLinkIndexAtOffset(mID, aOffset, &retVal);
  return retVal;
}

RemoteAccessible* RemoteAccessible::TableOfACell() {
  uint64_t tableID = 0;
  bool ok = false;
  Unused << mDoc->SendTableOfACell(mID, &tableID, &ok);
  return ok ? mDoc->GetAccessible(tableID) : nullptr;
}

uint32_t RemoteAccessible::ColIdx() {
  uint32_t index = 0;
  Unused << mDoc->SendColIdx(mID, &index);
  return index;
}

uint32_t RemoteAccessible::RowIdx() {
  uint32_t index = 0;
  Unused << mDoc->SendRowIdx(mID, &index);
  return index;
}

void RemoteAccessible::GetColRowExtents(uint32_t* aColIdx, uint32_t* aRowIdx,
                                        uint32_t* aColExtent,
                                        uint32_t* aRowExtent) {
  Unused << mDoc->SendGetColRowExtents(mID, aColIdx, aRowIdx, aColExtent,
                                       aRowExtent);
}

void RemoteAccessible::GetPosition(uint32_t* aColIdx, uint32_t* aRowIdx) {
  Unused << mDoc->SendGetPosition(mID, aColIdx, aRowIdx);
}

uint32_t RemoteAccessible::ColExtent() {
  uint32_t extent = 0;
  Unused << mDoc->SendColExtent(mID, &extent);
  return extent;
}

uint32_t RemoteAccessible::RowExtent() {
  uint32_t extent = 0;
  Unused << mDoc->SendRowExtent(mID, &extent);
  return extent;
}

void RemoteAccessible::ColHeaderCells(nsTArray<RemoteAccessible*>* aCells) {
  nsTArray<uint64_t> targetIDs;
  Unused << mDoc->SendColHeaderCells(mID, &targetIDs);

  size_t targetCount = targetIDs.Length();
  for (size_t i = 0; i < targetCount; i++) {
    aCells->AppendElement(mDoc->GetAccessible(targetIDs[i]));
  }
}

void RemoteAccessible::RowHeaderCells(nsTArray<RemoteAccessible*>* aCells) {
  nsTArray<uint64_t> targetIDs;
  Unused << mDoc->SendRowHeaderCells(mID, &targetIDs);

  size_t targetCount = targetIDs.Length();
  for (size_t i = 0; i < targetCount; i++) {
    aCells->AppendElement(mDoc->GetAccessible(targetIDs[i]));
  }
}

bool RemoteAccessible::IsCellSelected() {
  bool selected = false;
  Unused << mDoc->SendIsCellSelected(mID, &selected);
  return selected;
}

RemoteAccessible* RemoteAccessible::TableCaption() {
  uint64_t captionID = 0;
  bool ok = false;
  Unused << mDoc->SendTableCaption(mID, &captionID, &ok);
  return ok ? mDoc->GetAccessible(captionID) : nullptr;
}

void RemoteAccessible::TableSummary(nsString& aSummary) {
  Unused << mDoc->SendTableSummary(mID, &aSummary);
}

uint32_t RemoteAccessible::TableColumnCount() {
  uint32_t count = 0;
  Unused << mDoc->SendTableColumnCount(mID, &count);
  return count;
}

uint32_t RemoteAccessible::TableRowCount() {
  uint32_t count = 0;
  Unused << mDoc->SendTableRowCount(mID, &count);
  return count;
}

RemoteAccessible* RemoteAccessible::TableCellAt(uint32_t aRow, uint32_t aCol) {
  uint64_t cellID = 0;
  bool ok = false;
  Unused << mDoc->SendTableCellAt(mID, aRow, aCol, &cellID, &ok);
  return ok ? mDoc->GetAccessible(cellID) : nullptr;
}

int32_t RemoteAccessible::TableCellIndexAt(uint32_t aRow, uint32_t aCol) {
  int32_t index = 0;
  Unused << mDoc->SendTableCellIndexAt(mID, aRow, aCol, &index);
  return index;
}

int32_t RemoteAccessible::TableColumnIndexAt(uint32_t aCellIndex) {
  int32_t index = 0;
  Unused << mDoc->SendTableColumnIndexAt(mID, aCellIndex, &index);
  return index;
}

int32_t RemoteAccessible::TableRowIndexAt(uint32_t aCellIndex) {
  int32_t index = 0;
  Unused << mDoc->SendTableRowIndexAt(mID, aCellIndex, &index);
  return index;
}

void RemoteAccessible::TableRowAndColumnIndicesAt(uint32_t aCellIndex,
                                                  int32_t* aRow,
                                                  int32_t* aCol) {
  Unused << mDoc->SendTableRowAndColumnIndicesAt(mID, aCellIndex, aRow, aCol);
}

uint32_t RemoteAccessible::TableColumnExtentAt(uint32_t aRow, uint32_t aCol) {
  uint32_t extent = 0;
  Unused << mDoc->SendTableColumnExtentAt(mID, aRow, aCol, &extent);
  return extent;
}

uint32_t RemoteAccessible::TableRowExtentAt(uint32_t aRow, uint32_t aCol) {
  uint32_t extent = 0;
  Unused << mDoc->SendTableRowExtentAt(mID, aRow, aCol, &extent);
  return extent;
}

void RemoteAccessible::TableColumnDescription(uint32_t aCol,
                                              nsString& aDescription) {
  Unused << mDoc->SendTableColumnDescription(mID, aCol, &aDescription);
}

void RemoteAccessible::TableRowDescription(uint32_t aRow,
                                           nsString& aDescription) {
  Unused << mDoc->SendTableRowDescription(mID, aRow, &aDescription);
}

bool RemoteAccessible::TableColumnSelected(uint32_t aCol) {
  bool selected = false;
  Unused << mDoc->SendTableColumnSelected(mID, aCol, &selected);
  return selected;
}

bool RemoteAccessible::TableRowSelected(uint32_t aRow) {
  bool selected = false;
  Unused << mDoc->SendTableRowSelected(mID, aRow, &selected);
  return selected;
}

bool RemoteAccessible::TableCellSelected(uint32_t aRow, uint32_t aCol) {
  bool selected = false;
  Unused << mDoc->SendTableCellSelected(mID, aRow, aCol, &selected);
  return selected;
}

uint32_t RemoteAccessible::TableSelectedCellCount() {
  uint32_t count = 0;
  Unused << mDoc->SendTableSelectedCellCount(mID, &count);
  return count;
}

uint32_t RemoteAccessible::TableSelectedColumnCount() {
  uint32_t count = 0;
  Unused << mDoc->SendTableSelectedColumnCount(mID, &count);
  return count;
}

uint32_t RemoteAccessible::TableSelectedRowCount() {
  uint32_t count = 0;
  Unused << mDoc->SendTableSelectedRowCount(mID, &count);
  return count;
}

void RemoteAccessible::TableSelectedCells(
    nsTArray<RemoteAccessible*>* aCellIDs) {
  AutoTArray<uint64_t, 30> cellIDs;
  Unused << mDoc->SendTableSelectedCells(mID, &cellIDs);
  aCellIDs->SetCapacity(cellIDs.Length());
  for (uint32_t i = 0; i < cellIDs.Length(); ++i) {
    aCellIDs->AppendElement(mDoc->GetAccessible(cellIDs[i]));
  }
}

void RemoteAccessible::TableSelectedCellIndices(
    nsTArray<uint32_t>* aCellIndices) {
  Unused << mDoc->SendTableSelectedCellIndices(mID, aCellIndices);
}

void RemoteAccessible::TableSelectedColumnIndices(
    nsTArray<uint32_t>* aColumnIndices) {
  Unused << mDoc->SendTableSelectedColumnIndices(mID, aColumnIndices);
}

void RemoteAccessible::TableSelectedRowIndices(
    nsTArray<uint32_t>* aRowIndices) {
  Unused << mDoc->SendTableSelectedRowIndices(mID, aRowIndices);
}

void RemoteAccessible::TableSelectColumn(uint32_t aCol) {
  Unused << mDoc->SendTableSelectColumn(mID, aCol);
}

void RemoteAccessible::TableSelectRow(uint32_t aRow) {
  Unused << mDoc->SendTableSelectRow(mID, aRow);
}

void RemoteAccessible::TableUnselectColumn(uint32_t aCol) {
  Unused << mDoc->SendTableUnselectColumn(mID, aCol);
}

void RemoteAccessible::TableUnselectRow(uint32_t aRow) {
  Unused << mDoc->SendTableUnselectRow(mID, aRow);
}

bool RemoteAccessible::TableIsProbablyForLayout() {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::TableIsProbablyForLayout();
  }

  bool forLayout = false;
  Unused << mDoc->SendTableIsProbablyForLayout(mID, &forLayout);
  return forLayout;
}

RemoteAccessible* RemoteAccessible::AtkTableColumnHeader(int32_t aCol) {
  uint64_t headerID = 0;
  bool ok = false;
  Unused << mDoc->SendAtkTableColumnHeader(mID, aCol, &headerID, &ok);
  return ok ? mDoc->GetAccessible(headerID) : nullptr;
}

RemoteAccessible* RemoteAccessible::AtkTableRowHeader(int32_t aRow) {
  uint64_t headerID = 0;
  bool ok = false;
  Unused << mDoc->SendAtkTableRowHeader(mID, aRow, &headerID, &ok);
  return ok ? mDoc->GetAccessible(headerID) : nullptr;
}

void RemoteAccessible::SelectedItems(nsTArray<Accessible*>* aSelectedItems) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    RemoteAccessibleBase<RemoteAccessible>::SelectedItems(aSelectedItems);
    return;
  }

  AutoTArray<uint64_t, 10> itemIDs;
  Unused << mDoc->SendSelectedItems(mID, &itemIDs);
  aSelectedItems->SetCapacity(itemIDs.Length());
  for (size_t i = 0; i < itemIDs.Length(); ++i) {
    aSelectedItems->AppendElement(mDoc->GetAccessible(itemIDs[i]));
  }
}

uint32_t RemoteAccessible::SelectedItemCount() {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::SelectedItemCount();
  }

  uint32_t count = 0;
  Unused << mDoc->SendSelectedItemCount(mID, &count);
  return count;
}

Accessible* RemoteAccessible::GetSelectedItem(uint32_t aIndex) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::GetSelectedItem(aIndex);
  }

  uint64_t selectedItemID = 0;
  bool ok = false;
  Unused << mDoc->SendGetSelectedItem(mID, aIndex, &selectedItemID, &ok);
  return ok ? mDoc->GetAccessible(selectedItemID) : nullptr;
}

bool RemoteAccessible::IsItemSelected(uint32_t aIndex) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::IsItemSelected(aIndex);
  }

  bool selected = false;
  Unused << mDoc->SendIsItemSelected(mID, aIndex, &selected);
  return selected;
}

bool RemoteAccessible::AddItemToSelection(uint32_t aIndex) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::AddItemToSelection(aIndex);
  }

  bool success = false;
  Unused << mDoc->SendAddItemToSelection(mID, aIndex, &success);
  return success;
}

bool RemoteAccessible::RemoveItemFromSelection(uint32_t aIndex) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::RemoveItemFromSelection(
        aIndex);
  }

  bool success = false;
  Unused << mDoc->SendRemoveItemFromSelection(mID, aIndex, &success);
  return success;
}

bool RemoteAccessible::SelectAll() {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::SelectAll();
  }

  bool success = false;
  Unused << mDoc->SendSelectAll(mID, &success);
  return success;
}

bool RemoteAccessible::UnselectAll() {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::UnselectAll();
  }

  bool success = false;
  Unused << mDoc->SendUnselectAll(mID, &success);
  return success;
}

bool RemoteAccessible::DoAction(uint8_t aIndex) const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::DoAction(aIndex);
  }

  bool success = false;
  Unused << mDoc->SendDoAction(mID, aIndex, &success);
  return success;
}

uint8_t RemoteAccessible::ActionCount() const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::ActionCount();
  }

  uint8_t count = 0;
  Unused << mDoc->SendActionCount(mID, &count);
  return count;
}

void RemoteAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    RemoteAccessibleBase<RemoteAccessible>::ActionNameAt(aIndex, aName);
    return;
  }

  nsAutoString name;
  Unused << mDoc->SendActionNameAt(mID, aIndex, &name);

  aName.Assign(name);
}

KeyBinding RemoteAccessible::AccessKey() const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::AccessKey();
  }
  uint32_t key = 0;
  uint32_t modifierMask = 0;
  Unused << mDoc->SendAccessKey(mID, &key, &modifierMask);
  return KeyBinding(key, modifierMask);
}

void RemoteAccessible::AtkKeyBinding(nsString& aBinding) {
  Unused << mDoc->SendAtkKeyBinding(mID, &aBinding);
}

double RemoteAccessible::CurValue() const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::CurValue();
  }

  double val = UnspecifiedNaN<double>();
  Unused << mDoc->SendCurValue(mID, &val);
  return val;
}

bool RemoteAccessible::SetCurValue(double aValue) {
  bool success = false;
  Unused << mDoc->SendSetCurValue(mID, aValue, &success);
  return success;
}

double RemoteAccessible::MinValue() const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::MinValue();
  }

  double val = UnspecifiedNaN<double>();
  Unused << mDoc->SendMinValue(mID, &val);
  return val;
}

double RemoteAccessible::MaxValue() const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::MaxValue();
  }

  double val = UnspecifiedNaN<double>();
  Unused << mDoc->SendMaxValue(mID, &val);
  return val;
}

double RemoteAccessible::Step() const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::Step();
  }

  double step = UnspecifiedNaN<double>();
  Unused << mDoc->SendStep(mID, &step);
  return step;
}

Accessible* RemoteAccessible::ChildAtPoint(
    int32_t aX, int32_t aY, LocalAccessible::EWhichChildAtPoint aWhichChild) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::ChildAtPoint(aX, aY,
                                                                aWhichChild);
  }

  RemoteAccessible* target = this;
  do {
    if (target->IsOuterDoc()) {
      if (target->ChildCount() == 0) {
        // Return the OuterDoc if the requested point is within its bounds.
        LayoutDeviceIntRect rect = target->Bounds();
        if (rect.Contains(aX, aY)) {
          return target;
        }
        return nullptr;
      }
      DocAccessibleParent* childDoc = target->RemoteChildAt(0)->AsDoc();
      MOZ_ASSERT(childDoc);
      if (childDoc->IsTopLevelInContentProcess()) {
        // This is an OOP iframe. Remote calls can only work within their
        // process, so they stop at OOP iframes.
        if (aWhichChild == Accessible::EWhichChildAtPoint::DirectChild) {
          // Return the child document if it's within the bounds of the iframe.
          LayoutDeviceIntRect docRect = target->Bounds();
          if (docRect.Contains(aX, aY)) {
            return childDoc;
          }
          return nullptr;
        }
        // We must continue the search from the child document.
        target = childDoc;
      }
    }
    PDocAccessibleParent* resultDoc = nullptr;
    uint64_t resultID = 0;
    Unused << target->mDoc->SendChildAtPoint(target->mID, aX, aY,
                                             static_cast<uint32_t>(aWhichChild),
                                             &resultDoc, &resultID);
    // If resultDoc is null, this means there is no child at this point.
    auto useDoc = static_cast<DocAccessibleParent*>(resultDoc);
    target = resultDoc ? useDoc->GetAccessible(resultID) : nullptr;
  } while (target && target->IsOuterDoc() &&
           aWhichChild == Accessible::EWhichChildAtPoint::DeepestChild);
  return target;
}

LayoutDeviceIntRect RemoteAccessible::Bounds() const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::Bounds();
  }

  LayoutDeviceIntRect rect;
  Unused << mDoc->SendExtents(mID, false, &(rect.x), &(rect.y), &(rect.width),
                              &(rect.height));
  return rect;
}

nsIntRect RemoteAccessible::BoundsInCSSPixels() const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::BoundsInCSSPixels();
  }

  nsIntRect rect;
  Unused << mDoc->SendExtentsInCSSPixels(mID, &rect.x, &rect.y, &rect.width,
                                         &rect.height);
  return rect;
}

void RemoteAccessible::Language(nsString& aLocale) {
  Unused << mDoc->SendLanguage(mID, &aLocale);
}

void RemoteAccessible::DocType(nsString& aType) {
  Unused << mDoc->SendDocType(mID, &aType);
}

void RemoteAccessible::Title(nsString& aTitle) {
  Unused << mDoc->SendTitle(mID, &aTitle);
}

void RemoteAccessible::MimeType(nsString aMime) {
  Unused << mDoc->SendMimeType(mID, &aMime);
}

void RemoteAccessible::URLDocTypeMimeType(nsString& aURL, nsString& aDocType,
                                          nsString& aMimeType) {
  Unused << mDoc->SendURLDocTypeMimeType(mID, &aURL, &aDocType, &aMimeType);
}

void RemoteAccessible::Extents(bool aNeedsScreenCoords, int32_t* aX,
                               int32_t* aY, int32_t* aWidth, int32_t* aHeight) {
  Unused << mDoc->SendExtents(mID, aNeedsScreenCoords, aX, aY, aWidth, aHeight);
}

void RemoteAccessible::DOMNodeID(nsString& aID) const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::DOMNodeID(aID);
  }
  Unused << mDoc->SendDOMNodeID(mID, &aID);
}

}  // namespace a11y
}  // namespace mozilla
