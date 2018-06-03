/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProxyAccessible.h"
#include "mozilla/a11y/DocAccessibleParent.h"
#include "DocAccessible.h"
#include "mozilla/a11y/DocManager.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/Unused.h"
#include "mozilla/a11y/Platform.h"
#include "RelationType.h"
#include "mozilla/a11y/Role.h"
#include "xpcAccessibleDocument.h"

namespace mozilla {
namespace a11y {

uint64_t
ProxyAccessible::State() const
{
  uint64_t state = 0;
  Unused << mDoc->SendState(mID, &state);
  return state;
}

uint64_t
ProxyAccessible::NativeState() const
{
  uint64_t state = 0;
  Unused << mDoc->SendNativeState(mID, &state);
  return state;
}

void
ProxyAccessible::Name(nsString& aName) const
{
  Unused << mDoc->SendName(mID, &aName);
}

void
ProxyAccessible::Value(nsString& aValue) const
{
  Unused << mDoc->SendValue(mID, &aValue);
}

void
ProxyAccessible::Help(nsString& aHelp) const
{
  Unused << mDoc->SendHelp(mID, &aHelp);
}

void
ProxyAccessible::Description(nsString& aDesc) const
{
  Unused << mDoc->SendDescription(mID, &aDesc);
}

void
ProxyAccessible::Attributes(nsTArray<Attribute> *aAttrs) const
{
  Unused << mDoc->SendAttributes(mID, aAttrs);
}

nsTArray<ProxyAccessible*>
ProxyAccessible::RelationByType(RelationType aType) const
{
  nsTArray<uint64_t> targetIDs;
  Unused << mDoc->SendRelationByType(mID, static_cast<uint32_t>(aType),
                                     &targetIDs);

  size_t targetCount = targetIDs.Length();
  nsTArray<ProxyAccessible*> targets(targetCount);
  for (size_t i = 0; i < targetCount; i++)
    if (ProxyAccessible* proxy = mDoc->GetAccessible(targetIDs[i]))
      targets.AppendElement(proxy);

  return std::move(targets);
}

void
ProxyAccessible::Relations(nsTArray<RelationType>* aTypes,
                           nsTArray<nsTArray<ProxyAccessible*>>* aTargetSets)
  const
{
  nsTArray<RelationTargets> ipcRelations;
  Unused << mDoc->SendRelations(mID, &ipcRelations);

  size_t relationCount = ipcRelations.Length();
  aTypes->SetCapacity(relationCount);
  aTargetSets->SetCapacity(relationCount);
  for (size_t i = 0; i < relationCount; i++) {
    uint32_t type = ipcRelations[i].Type();
    if (type > static_cast<uint32_t>(RelationType::LAST))
      continue;

    size_t targetCount = ipcRelations[i].Targets().Length();
    nsTArray<ProxyAccessible*> targets(targetCount);
    for (size_t j = 0; j < targetCount; j++)
      if (ProxyAccessible* proxy = mDoc->GetAccessible(ipcRelations[i].Targets()[j]))
        targets.AppendElement(proxy);

    if (targets.IsEmpty())
      continue;

    aTargetSets->AppendElement(std::move(targets));
    aTypes->AppendElement(static_cast<RelationType>(type));
  }
}

bool
ProxyAccessible::IsSearchbox() const
{
  bool retVal = false;
  Unused << mDoc->SendIsSearchbox(mID, &retVal);
  return retVal;
}

nsAtom*
ProxyAccessible::LandmarkRole() const
{
  nsString landmark;
  Unused << mDoc->SendLandmarkRole(mID, &landmark);
  return NS_GetStaticAtom(landmark);
}

nsAtom*
ProxyAccessible::ARIARoleAtom() const
{
  nsString role;
  Unused << mDoc->SendARIARoleAtom(mID, &role);
  return NS_GetStaticAtom(role);
}

int32_t
ProxyAccessible::GetLevelInternal()
{
  int32_t level = 0;
  Unused << mDoc->SendGetLevelInternal(mID, &level);
  return level;
}

void
ProxyAccessible::ScrollTo(uint32_t aScrollType)
{
  Unused << mDoc->SendScrollTo(mID, aScrollType);
}

void
ProxyAccessible::ScrollToPoint(uint32_t aScrollType, int32_t aX, int32_t aY)
{
  Unused << mDoc->SendScrollToPoint(mID, aScrollType, aX, aY);
}

int32_t
ProxyAccessible::CaretLineNumber()
{
  int32_t line = -1;
  Unused << mDoc->SendCaretOffset(mID, &line);
  return line;
}

int32_t
ProxyAccessible::CaretOffset()
{
  int32_t offset = 0;
  Unused << mDoc->SendCaretOffset(mID, &offset);
  return offset;
}

void
ProxyAccessible::SetCaretOffset(int32_t aOffset)
{
  Unused << mDoc->SendSetCaretOffset(mID, aOffset);
}

int32_t
ProxyAccessible::CharacterCount()
{
  int32_t count = 0;
  Unused << mDoc->SendCharacterCount(mID, &count);
  return count;
}

int32_t
ProxyAccessible::SelectionCount()
{
  int32_t count = 0;
  Unused << mDoc->SendSelectionCount(mID, &count);
  return count;
}

bool
ProxyAccessible::TextSubstring(int32_t aStartOffset, int32_t aEndOfset,
                               nsString& aText) const
{
  bool valid;
  Unused << mDoc->SendTextSubstring(mID, aStartOffset, aEndOfset, &aText, &valid);
  return valid;
}

void
ProxyAccessible::GetTextAfterOffset(int32_t aOffset,
                                    AccessibleTextBoundary aBoundaryType,
                                    nsString& aText, int32_t* aStartOffset,
                                    int32_t* aEndOffset)
{
  Unused << mDoc->SendGetTextAfterOffset(mID, aOffset, aBoundaryType,
                                         &aText, aStartOffset, aEndOffset);
}

void
ProxyAccessible::GetTextAtOffset(int32_t aOffset,
                                 AccessibleTextBoundary aBoundaryType,
                                 nsString& aText, int32_t* aStartOffset,
                                 int32_t* aEndOffset)
{
  Unused << mDoc->SendGetTextAtOffset(mID, aOffset, aBoundaryType,
                                      &aText, aStartOffset, aEndOffset);
}

void
ProxyAccessible::GetTextBeforeOffset(int32_t aOffset,
                                     AccessibleTextBoundary aBoundaryType,
                                     nsString& aText, int32_t* aStartOffset,
                                     int32_t* aEndOffset)
{
  Unused << mDoc->SendGetTextBeforeOffset(mID, aOffset, aBoundaryType,
                                          &aText, aStartOffset, aEndOffset);
}

char16_t
ProxyAccessible::CharAt(int32_t aOffset)
{
  uint16_t retval = 0;
  Unused << mDoc->SendCharAt(mID, aOffset, &retval);
  return static_cast<char16_t>(retval);
}

void
ProxyAccessible::TextAttributes(bool aIncludeDefAttrs,
                                int32_t aOffset,
                                nsTArray<Attribute>* aAttributes,
                                int32_t* aStartOffset,
                                int32_t* aEndOffset)
{
  Unused << mDoc->SendTextAttributes(mID, aIncludeDefAttrs, aOffset,
                                     aAttributes, aStartOffset, aEndOffset);
}

void
ProxyAccessible::DefaultTextAttributes(nsTArray<Attribute>* aAttrs)
{
  Unused << mDoc->SendDefaultTextAttributes(mID, aAttrs);
}

nsIntRect
ProxyAccessible::TextBounds(int32_t aStartOffset, int32_t aEndOffset,
                            uint32_t aCoordType)
{
  nsIntRect rect;
  Unused <<
    mDoc->SendTextBounds(mID, aStartOffset, aEndOffset, aCoordType, &rect);
  return rect;
}

nsIntRect
ProxyAccessible::CharBounds(int32_t aOffset, uint32_t aCoordType)
{
  nsIntRect rect;
  Unused <<
    mDoc->SendCharBounds(mID, aOffset, aCoordType, &rect);
  return rect;
}

int32_t
ProxyAccessible::OffsetAtPoint(int32_t aX, int32_t aY, uint32_t aCoordType)
{
  int32_t retVal = -1;
  Unused << mDoc->SendOffsetAtPoint(mID, aX, aY, aCoordType, &retVal);
  return retVal;
}

bool
ProxyAccessible::SelectionBoundsAt(int32_t aSelectionNum,
                                   nsString& aData,
                                   int32_t* aStartOffset,
                                   int32_t* aEndOffset)
{
  bool retVal = false;
  Unused << mDoc->SendSelectionBoundsAt(mID, aSelectionNum, &retVal, &aData,
                                        aStartOffset, aEndOffset);
  return retVal;
}

bool
ProxyAccessible::SetSelectionBoundsAt(int32_t aSelectionNum,
                                      int32_t aStartOffset,
                                      int32_t aEndOffset)
{
  bool retVal = false;
  Unused << mDoc->SendSetSelectionBoundsAt(mID, aSelectionNum, aStartOffset,
                                           aEndOffset, &retVal);
  return retVal;
}

bool
ProxyAccessible::AddToSelection(int32_t aStartOffset,
                                int32_t aEndOffset)
{
  bool retVal = false;
  Unused << mDoc->SendAddToSelection(mID, aStartOffset, aEndOffset, &retVal);
  return retVal;
}

bool
ProxyAccessible::RemoveFromSelection(int32_t aSelectionNum)
{
  bool retVal = false;
  Unused << mDoc->SendRemoveFromSelection(mID, aSelectionNum, &retVal);
  return retVal;
}

void
ProxyAccessible::ScrollSubstringTo(int32_t aStartOffset, int32_t aEndOffset,
                                   uint32_t aScrollType)
{
  Unused << mDoc->SendScrollSubstringTo(mID, aStartOffset, aEndOffset, aScrollType);
}

void
ProxyAccessible::ScrollSubstringToPoint(int32_t aStartOffset,
                                        int32_t aEndOffset,
                                        uint32_t aCoordinateType,
                                        int32_t aX, int32_t aY)
{
  Unused << mDoc->SendScrollSubstringToPoint(mID, aStartOffset, aEndOffset,
                                             aCoordinateType, aX, aY);
}

void
ProxyAccessible::Text(nsString* aText)
{
  Unused << mDoc->SendText(mID, aText);
}

void
ProxyAccessible::ReplaceText(const nsString& aText)
{
  Unused << mDoc->SendReplaceText(mID, aText);
}

bool
ProxyAccessible::InsertText(const nsString& aText, int32_t aPosition)
{
  bool valid;
  Unused << mDoc->SendInsertText(mID, aText, aPosition, &valid);
  return valid;
}

bool
ProxyAccessible::CopyText(int32_t aStartPos, int32_t aEndPos)
{
  bool valid;
  Unused << mDoc->SendCopyText(mID, aStartPos, aEndPos, &valid);
  return valid;
}

bool
ProxyAccessible::CutText(int32_t aStartPos, int32_t aEndPos)
{
  bool valid;
  Unused << mDoc->SendCutText(mID, aStartPos, aEndPos, &valid);
  return valid;
}

bool
ProxyAccessible::DeleteText(int32_t aStartPos, int32_t aEndPos)
{
  bool valid;
  Unused << mDoc->SendDeleteText(mID, aStartPos, aEndPos, &valid);
  return valid;
}

bool
ProxyAccessible::PasteText(int32_t aPosition)
{
  bool valid;
  Unused << mDoc->SendPasteText(mID, aPosition, &valid);
  return valid;
}

nsIntPoint
ProxyAccessible::ImagePosition(uint32_t aCoordType)
{
  nsIntPoint retVal;
  Unused << mDoc->SendImagePosition(mID, aCoordType, &retVal);
  return retVal;
}

nsIntSize
ProxyAccessible::ImageSize()
{
  nsIntSize retVal;
  Unused << mDoc->SendImageSize(mID, &retVal);
  return retVal;
}

uint32_t
ProxyAccessible::StartOffset(bool* aOk)
{
  uint32_t retVal = 0;
  Unused << mDoc->SendStartOffset(mID, &retVal, aOk);
  return retVal;
}

uint32_t
ProxyAccessible::EndOffset(bool* aOk)
{
  uint32_t retVal = 0;
  Unused << mDoc->SendEndOffset(mID, &retVal, aOk);
  return retVal;
}

bool
ProxyAccessible::IsLinkValid()
{
  bool retVal = false;
  Unused << mDoc->SendIsLinkValid(mID, &retVal);
  return retVal;
}

uint32_t
ProxyAccessible::AnchorCount(bool* aOk)
{
  uint32_t retVal = 0;
  Unused << mDoc->SendAnchorCount(mID, &retVal, aOk);
  return retVal;
}

void
ProxyAccessible::AnchorURIAt(uint32_t aIndex, nsCString& aURI, bool* aOk)
{
  Unused << mDoc->SendAnchorURIAt(mID, aIndex, &aURI, aOk);
}

ProxyAccessible*
ProxyAccessible::AnchorAt(uint32_t aIndex)
{
  uint64_t id = 0;
  bool ok = false;
  Unused << mDoc->SendAnchorAt(mID, aIndex, &id, &ok);
  return ok ? mDoc->GetAccessible(id) : nullptr;
}

uint32_t
ProxyAccessible::LinkCount()
{
  uint32_t retVal = 0;
  Unused << mDoc->SendLinkCount(mID, &retVal);
  return retVal;
}

ProxyAccessible*
ProxyAccessible::LinkAt(const uint32_t& aIndex)
{
  uint64_t linkID = 0;
  bool ok = false;
  Unused << mDoc->SendLinkAt(mID, aIndex, &linkID, &ok);
  return ok ? mDoc->GetAccessible(linkID) : nullptr;
}

int32_t
ProxyAccessible::LinkIndexOf(ProxyAccessible* aLink)
{
  int32_t retVal = -1;
  if (aLink) {
    Unused << mDoc->SendLinkIndexOf(mID, aLink->ID(), &retVal);
  }

  return retVal;
}

int32_t
ProxyAccessible::LinkIndexAtOffset(uint32_t aOffset)
{
  int32_t retVal = -1;
  Unused << mDoc->SendLinkIndexAtOffset(mID, aOffset, &retVal);
  return retVal;
}

ProxyAccessible*
ProxyAccessible::TableOfACell()
{
  uint64_t tableID = 0;
  bool ok = false;
  Unused << mDoc->SendTableOfACell(mID, &tableID, &ok);
  return ok ? mDoc->GetAccessible(tableID) : nullptr;
}

uint32_t
ProxyAccessible::ColIdx()
{
  uint32_t index = 0;
  Unused << mDoc->SendColIdx(mID, &index);
  return index;
}

uint32_t
ProxyAccessible::RowIdx()
{
  uint32_t index = 0;
  Unused << mDoc->SendRowIdx(mID, &index);
  return index;
}

void
ProxyAccessible::GetColRowExtents(uint32_t* aColIdx, uint32_t* aRowIdx,
                                  uint32_t* aColExtent, uint32_t* aRowExtent)
{
  Unused << mDoc->SendGetColRowExtents(mID, aColIdx, aRowIdx, aColExtent, aRowExtent);
}

void
ProxyAccessible::GetPosition(uint32_t* aColIdx, uint32_t* aRowIdx)
{
  Unused << mDoc->SendGetPosition(mID, aColIdx, aRowIdx);
}

uint32_t
ProxyAccessible::ColExtent()
{
  uint32_t extent = 0;
  Unused << mDoc->SendColExtent(mID, &extent);
  return extent;
}

uint32_t
ProxyAccessible::RowExtent()
{
  uint32_t extent = 0;
  Unused << mDoc->SendRowExtent(mID, &extent);
  return extent;
}

void
ProxyAccessible::ColHeaderCells(nsTArray<ProxyAccessible*>* aCells)
{
  nsTArray<uint64_t> targetIDs;
  Unused << mDoc->SendColHeaderCells(mID, &targetIDs);

  size_t targetCount = targetIDs.Length();
  for (size_t i = 0; i < targetCount; i++) {
    aCells->AppendElement(mDoc->GetAccessible(targetIDs[i]));
  }
}

void
ProxyAccessible::RowHeaderCells(nsTArray<ProxyAccessible*>* aCells)
{
  nsTArray<uint64_t> targetIDs;
  Unused << mDoc->SendRowHeaderCells(mID, &targetIDs);

  size_t targetCount = targetIDs.Length();
  for (size_t i = 0; i < targetCount; i++) {
    aCells->AppendElement(mDoc->GetAccessible(targetIDs[i]));
  }
}

bool
ProxyAccessible::IsCellSelected()
{
  bool selected = false;
  Unused << mDoc->SendIsCellSelected(mID, &selected);
  return selected;
}

ProxyAccessible*
ProxyAccessible::TableCaption()
{
  uint64_t captionID = 0;
  bool ok = false;
  Unused << mDoc->SendTableCaption(mID, &captionID, &ok);
  return ok ? mDoc->GetAccessible(captionID) : nullptr;
}

void
ProxyAccessible::TableSummary(nsString& aSummary)
{
  Unused << mDoc->SendTableSummary(mID, &aSummary);
}

uint32_t
ProxyAccessible::TableColumnCount()
{
  uint32_t count = 0;
  Unused << mDoc->SendTableColumnCount(mID, &count);
  return count;
}

uint32_t
ProxyAccessible::TableRowCount()
{
  uint32_t count = 0;
  Unused << mDoc->SendTableRowCount(mID, &count);
  return count;
}

ProxyAccessible*
ProxyAccessible::TableCellAt(uint32_t aRow, uint32_t aCol)
{
  uint64_t cellID = 0;
  bool ok = false;
  Unused << mDoc->SendTableCellAt(mID, aRow, aCol, &cellID, &ok);
  return ok ? mDoc->GetAccessible(cellID) : nullptr;
}

int32_t
ProxyAccessible::TableCellIndexAt(uint32_t aRow, uint32_t aCol)
{
  int32_t index = 0;
  Unused << mDoc->SendTableCellIndexAt(mID, aRow, aCol, &index);
  return index;
}

int32_t
ProxyAccessible::TableColumnIndexAt(uint32_t aCellIndex)
{
  int32_t index = 0;
  Unused << mDoc->SendTableColumnIndexAt(mID, aCellIndex, &index);
  return index;
}

int32_t
ProxyAccessible::TableRowIndexAt(uint32_t aCellIndex)
{
  int32_t index = 0;
  Unused << mDoc->SendTableRowIndexAt(mID, aCellIndex, &index);
  return index;
}

void
ProxyAccessible::TableRowAndColumnIndicesAt(uint32_t aCellIndex,
                                            int32_t* aRow, int32_t* aCol)
{
  Unused << mDoc->SendTableRowAndColumnIndicesAt(mID, aCellIndex, aRow, aCol);
}

uint32_t
ProxyAccessible::TableColumnExtentAt(uint32_t aRow, uint32_t aCol)
{
  uint32_t extent = 0;
  Unused << mDoc->SendTableColumnExtentAt(mID, aRow, aCol, &extent);
  return extent;
}

uint32_t
ProxyAccessible::TableRowExtentAt(uint32_t aRow, uint32_t aCol)
{
  uint32_t extent = 0;
  Unused << mDoc->SendTableRowExtentAt(mID, aRow, aCol, &extent);
  return extent;
}

void
ProxyAccessible::TableColumnDescription(uint32_t aCol, nsString& aDescription)
{
  Unused << mDoc->SendTableColumnDescription(mID, aCol, &aDescription);
}

void
ProxyAccessible::TableRowDescription(uint32_t aRow, nsString& aDescription)
{
  Unused << mDoc->SendTableRowDescription(mID, aRow, &aDescription);
}

bool
ProxyAccessible::TableColumnSelected(uint32_t aCol)
{
  bool selected = false;
  Unused << mDoc->SendTableColumnSelected(mID, aCol, &selected);
  return selected;
}

bool
ProxyAccessible::TableRowSelected(uint32_t aRow)
{
  bool selected = false;
  Unused << mDoc->SendTableRowSelected(mID, aRow, &selected);
  return selected;
}

bool
ProxyAccessible::TableCellSelected(uint32_t aRow, uint32_t aCol)
{
  bool selected = false;
  Unused << mDoc->SendTableCellSelected(mID, aRow, aCol, &selected);
  return selected;
}

uint32_t
ProxyAccessible::TableSelectedCellCount()
{
  uint32_t count = 0;
  Unused << mDoc->SendTableSelectedCellCount(mID, &count);
  return count;
}

uint32_t
ProxyAccessible::TableSelectedColumnCount()
{
  uint32_t count = 0;
  Unused << mDoc->SendTableSelectedColumnCount(mID, &count);
  return count;
}

uint32_t
ProxyAccessible::TableSelectedRowCount()
{
  uint32_t count = 0;
  Unused << mDoc->SendTableSelectedRowCount(mID, &count);
  return count;
}

void
ProxyAccessible::TableSelectedCells(nsTArray<ProxyAccessible*>* aCellIDs)
{
  AutoTArray<uint64_t, 30> cellIDs;
  Unused << mDoc->SendTableSelectedCells(mID, &cellIDs);
  aCellIDs->SetCapacity(cellIDs.Length());
  for (uint32_t i = 0; i < cellIDs.Length(); ++i) {
    aCellIDs->AppendElement(mDoc->GetAccessible(cellIDs[i]));
  }
}

void
ProxyAccessible::TableSelectedCellIndices(nsTArray<uint32_t>* aCellIndices)
{
  Unused << mDoc->SendTableSelectedCellIndices(mID, aCellIndices);
}

void
ProxyAccessible::TableSelectedColumnIndices(nsTArray<uint32_t>* aColumnIndices)
{
  Unused << mDoc->SendTableSelectedColumnIndices(mID, aColumnIndices);
}

void
ProxyAccessible::TableSelectedRowIndices(nsTArray<uint32_t>* aRowIndices)
{
  Unused << mDoc->SendTableSelectedRowIndices(mID, aRowIndices);
}

void
ProxyAccessible::TableSelectColumn(uint32_t aCol)
{
  Unused << mDoc->SendTableSelectColumn(mID, aCol);
}

void
ProxyAccessible::TableSelectRow(uint32_t aRow)
{
  Unused << mDoc->SendTableSelectRow(mID, aRow);
}

void
ProxyAccessible::TableUnselectColumn(uint32_t aCol)
{
  Unused << mDoc->SendTableUnselectColumn(mID, aCol);
}

void
ProxyAccessible::TableUnselectRow(uint32_t aRow)
{
  Unused << mDoc->SendTableUnselectRow(mID, aRow);
}

bool
ProxyAccessible::TableIsProbablyForLayout()
{
  bool forLayout = false;
  Unused << mDoc->SendTableIsProbablyForLayout(mID, &forLayout);
  return forLayout;
}

ProxyAccessible*
ProxyAccessible::AtkTableColumnHeader(int32_t aCol)
{
  uint64_t headerID = 0;
  bool ok = false;
  Unused << mDoc->SendAtkTableColumnHeader(mID, aCol, &headerID, &ok);
  return ok ? mDoc->GetAccessible(headerID) : nullptr;
}

ProxyAccessible*
ProxyAccessible::AtkTableRowHeader(int32_t aRow)
{
  uint64_t headerID = 0;
  bool ok = false;
  Unused << mDoc->SendAtkTableRowHeader(mID, aRow, &headerID, &ok);
  return ok ? mDoc->GetAccessible(headerID) : nullptr;
}

void
ProxyAccessible::SelectedItems(nsTArray<ProxyAccessible*>* aSelectedItems)
{
  AutoTArray<uint64_t, 10> itemIDs;
  Unused << mDoc->SendSelectedItems(mID, &itemIDs);
  aSelectedItems->SetCapacity(itemIDs.Length());
  for (size_t i = 0; i < itemIDs.Length(); ++i) {
    aSelectedItems->AppendElement(mDoc->GetAccessible(itemIDs[i]));
  }
}

uint32_t
ProxyAccessible::SelectedItemCount()
{
  uint32_t count = 0;
  Unused << mDoc->SendSelectedItemCount(mID, &count);
  return count;
}

ProxyAccessible*
ProxyAccessible::GetSelectedItem(uint32_t aIndex)
{
  uint64_t selectedItemID = 0;
  bool ok = false;
  Unused << mDoc->SendGetSelectedItem(mID, aIndex, &selectedItemID, &ok);
  return ok ? mDoc->GetAccessible(selectedItemID) : nullptr;
}

bool
ProxyAccessible::IsItemSelected(uint32_t aIndex)
{
  bool selected = false;
  Unused << mDoc->SendIsItemSelected(mID, aIndex, &selected);
  return selected;
}

bool
ProxyAccessible::AddItemToSelection(uint32_t aIndex)
{
  bool success = false;
  Unused << mDoc->SendAddItemToSelection(mID, aIndex, &success);
  return success;
}

bool
ProxyAccessible::RemoveItemFromSelection(uint32_t aIndex)
{
  bool success = false;
  Unused << mDoc->SendRemoveItemFromSelection(mID, aIndex, &success);
  return success;
}

bool
ProxyAccessible::SelectAll()
{
  bool success = false;
  Unused << mDoc->SendSelectAll(mID, &success);
  return success;
}

bool
ProxyAccessible::UnselectAll()
{
  bool success = false;
  Unused << mDoc->SendUnselectAll(mID, &success);
  return success;
}

void
ProxyAccessible::TakeSelection()
{
  Unused << mDoc->SendTakeSelection(mID);
}

void
ProxyAccessible::SetSelected(bool aSelect)
{
  Unused << mDoc->SendSetSelected(mID, aSelect);
}

bool
ProxyAccessible::DoAction(uint8_t aIndex)
{
  bool success = false;
  Unused << mDoc->SendDoAction(mID, aIndex, &success);
  return success;
}

uint8_t
ProxyAccessible::ActionCount()
{
  uint8_t count = 0;
  Unused << mDoc->SendActionCount(mID, &count);
  return count;
}

void
ProxyAccessible::ActionDescriptionAt(uint8_t aIndex, nsString& aDescription)
{
  Unused << mDoc->SendActionDescriptionAt(mID, aIndex, &aDescription);
}

void
ProxyAccessible::ActionNameAt(uint8_t aIndex, nsString& aName)
{
  Unused << mDoc->SendActionNameAt(mID, aIndex, &aName);
}

KeyBinding
ProxyAccessible::AccessKey()
{
  uint32_t key = 0;
  uint32_t modifierMask = 0;
  Unused << mDoc->SendAccessKey(mID, &key, &modifierMask);
  return KeyBinding(key, modifierMask);
}

KeyBinding
ProxyAccessible::KeyboardShortcut()
{
  uint32_t key = 0;
  uint32_t modifierMask = 0;
  Unused << mDoc->SendKeyboardShortcut(mID, &key, &modifierMask);
  return KeyBinding(key, modifierMask);
}

void
ProxyAccessible::AtkKeyBinding(nsString& aBinding)
{
  Unused << mDoc->SendAtkKeyBinding(mID, &aBinding);
}

double
ProxyAccessible::CurValue()
{
  double val = UnspecifiedNaN<double>();
  Unused << mDoc->SendCurValue(mID, &val);
  return val;
}

bool
ProxyAccessible::SetCurValue(double aValue)
{
  bool success = false;
  Unused << mDoc->SendSetCurValue(mID, aValue, &success);
  return success;
}

double
ProxyAccessible::MinValue()
{
  double val = UnspecifiedNaN<double>();
  Unused << mDoc->SendMinValue(mID, &val);
  return val;
}

double
ProxyAccessible::MaxValue()
{
  double val = UnspecifiedNaN<double>();
  Unused << mDoc->SendMaxValue(mID, &val);
  return val;
}

double
ProxyAccessible::Step()
{
  double step = UnspecifiedNaN<double>();
  Unused << mDoc->SendStep(mID, &step);
  return step;
}

void
ProxyAccessible::TakeFocus()
{
  Unused << mDoc->SendTakeFocus(mID);
}

ProxyAccessible*
ProxyAccessible::FocusedChild()
{
  uint64_t childID = 0;
  bool ok = false;
  Unused << mDoc->SendFocusedChild(mID, &childID, &ok);
  return ok ? mDoc->GetAccessible(childID) : nullptr;
}

ProxyAccessible*
ProxyAccessible::ChildAtPoint(int32_t aX, int32_t aY,
                              Accessible::EWhichChildAtPoint aWhichChild)
{
  uint64_t childID = 0;
  bool ok = false;
  Unused << mDoc->SendAccessibleAtPoint(mID, aX, aY, false,
                                        static_cast<uint32_t>(aWhichChild),
                                        &childID, &ok);
  return ok ? mDoc->GetAccessible(childID) : nullptr;
}

nsIntRect
ProxyAccessible::Bounds()
{
  nsIntRect rect;
  Unused << mDoc->SendExtents(mID, false,
                              &(rect.x), &(rect.y),
                              &(rect.width), &(rect.height));
  return rect;
}

nsIntRect
ProxyAccessible::BoundsInCSSPixels()
{
  nsIntRect rect;
  Unused << mDoc->SendExtentsInCSSPixels(mID,
                                         &rect.x, &rect.y,
                                         &rect.width, &rect.height);
  return rect;
}

void
ProxyAccessible::Language(nsString& aLocale)
{
  Unused << mDoc->SendLanguage(mID, &aLocale);
}

void
ProxyAccessible::DocType(nsString& aType)
{
  Unused << mDoc->SendDocType(mID, &aType);
}

void
ProxyAccessible::Title(nsString& aTitle)
{
  Unused << mDoc->SendTitle(mID, &aTitle);
}

void
ProxyAccessible::URL(nsString& aURL)
{
  Unused << mDoc->SendURL(mID, &aURL);
}

void
ProxyAccessible::MimeType(nsString aMime)
{
  Unused << mDoc->SendMimeType(mID, &aMime);
}

void
ProxyAccessible::URLDocTypeMimeType(nsString& aURL, nsString& aDocType,
                                    nsString& aMimeType)
{
  Unused << mDoc->SendURLDocTypeMimeType(mID, &aURL, &aDocType, &aMimeType);
}

ProxyAccessible*
ProxyAccessible::AccessibleAtPoint(int32_t aX, int32_t aY,
                                   bool aNeedsScreenCoords)
{
  uint64_t childID = 0;
  bool ok = false;
  Unused <<
    mDoc->SendAccessibleAtPoint(mID, aX, aY, aNeedsScreenCoords,
                                static_cast<uint32_t>(Accessible::eDirectChild),
                                &childID, &ok);
  return ok ? mDoc->GetAccessible(childID) : nullptr;
}

void
ProxyAccessible::Extents(bool aNeedsScreenCoords, int32_t* aX, int32_t* aY,
                        int32_t* aWidth, int32_t* aHeight)
{
  Unused << mDoc->SendExtents(mID, aNeedsScreenCoords, aX, aY, aWidth, aHeight);
}

void
ProxyAccessible::DOMNodeID(nsString& aID)
{
  Unused << mDoc->SendDOMNodeID(mID, &aID);
}

}
}
