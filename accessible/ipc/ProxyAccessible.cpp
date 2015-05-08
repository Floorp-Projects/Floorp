/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProxyAccessible.h"
#include "DocAccessibleParent.h"
#include "mozilla/unused.h"
#include "mozilla/a11y/Platform.h"
#include "RelationType.h"
#include "mozilla/a11y/Role.h"

namespace mozilla {
namespace a11y {

void
ProxyAccessible::Shutdown()
{
  NS_ASSERTION(!mOuterDoc, "Why do we still have a child doc?");

  // XXX Ideally  this wouldn't be necessary, but it seems OuterDoc accessibles
  // can be destroyed before the doc they own.
  if (!mOuterDoc) {
    uint32_t childCount = mChildren.Length();
    for (uint32_t idx = 0; idx < childCount; idx++)
      mChildren[idx]->Shutdown();
  } else {
    if (mChildren.Length() != 1)
      MOZ_CRASH("outer doc doesn't own adoc!");

    static_cast<DocAccessibleParent*>(mChildren[0])->Unbind();
  }

  mChildren.Clear();
  ProxyDestroyed(this);
  mDoc->RemoveAccessible(this);
}

void
ProxyAccessible::SetChildDoc(DocAccessibleParent* aParent)
{
  if (aParent) {
    MOZ_ASSERT(mChildren.IsEmpty());
    mChildren.AppendElement(aParent);
    mOuterDoc = true;
  } else {
    MOZ_ASSERT(mChildren.Length() == 1);
    mChildren.Clear();
    mOuterDoc = false;
  }
}

bool
ProxyAccessible::MustPruneChildren() const
{
  // this is the equivalent to nsAccUtils::MustPrune for proxies and should be
  // kept in sync with that.
  if (mRole != roles::MENUITEM && mRole != roles::COMBOBOX_OPTION
      && mRole != roles::OPTION && mRole != roles::ENTRY
      && mRole != roles::FLAT_EQUATION && mRole != roles::PASSWORD_TEXT
      && mRole != roles::PUSHBUTTON && mRole != roles::TOGGLE_BUTTON
      && mRole != roles::GRAPHIC && mRole != roles::SLIDER
      && mRole != roles::PROGRESSBAR && mRole != roles::SEPARATOR)
    return false;

  if (mChildren.Length() != 1)
    return false;

  return mChildren[0]->Role() == roles::TEXT_LEAF
    || mChildren[0]->Role() == roles::STATICTEXT;
}

uint64_t
ProxyAccessible::State() const
{
  uint64_t state = 0;
  unused << mDoc->SendState(mID, &state);
  return state;
}

void
ProxyAccessible::Name(nsString& aName) const
{
  unused << mDoc->SendName(mID, &aName);
}

void
ProxyAccessible::Value(nsString& aValue) const
{
  unused << mDoc->SendValue(mID, &aValue);
}

void
ProxyAccessible::Description(nsString& aDesc) const
{
  unused << mDoc->SendDescription(mID, &aDesc);
}

void
ProxyAccessible::Attributes(nsTArray<Attribute> *aAttrs) const
{
  unused << mDoc->SendAttributes(mID, aAttrs);
}

nsTArray<ProxyAccessible*>
ProxyAccessible::RelationByType(RelationType aType) const
{
  nsTArray<uint64_t> targetIDs;
  unused << mDoc->SendRelationByType(mID, static_cast<uint32_t>(aType),
                                     &targetIDs);

  size_t targetCount = targetIDs.Length();
  nsTArray<ProxyAccessible*> targets(targetCount);
  for (size_t i = 0; i < targetCount; i++)
    if (ProxyAccessible* proxy = mDoc->GetAccessible(targetIDs[i]))
      targets.AppendElement(proxy);

  return Move(targets);
}

void
ProxyAccessible::Relations(nsTArray<RelationType>* aTypes,
                           nsTArray<nsTArray<ProxyAccessible*>>* aTargetSets)
  const
{
  nsTArray<RelationTargets> ipcRelations;
  unused << mDoc->SendRelations(mID, &ipcRelations);

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

    aTargetSets->AppendElement(Move(targets));
    aTypes->AppendElement(static_cast<RelationType>(type));
  }
}

int32_t
ProxyAccessible::CaretOffset()
{
  int32_t offset = 0;
  unused << mDoc->SendCaretOffset(mID, &offset);
  return offset;
}

bool
ProxyAccessible::SetCaretOffset(int32_t aOffset)
{
  bool valid = false;
  unused << mDoc->SendSetCaretOffset(mID, aOffset, &valid);
  return valid;
}

int32_t
ProxyAccessible::CharacterCount()
{
  int32_t count = 0;
  unused << mDoc->SendCharacterCount(mID, &count);
  return count;
}

int32_t
ProxyAccessible::SelectionCount()
{
  int32_t count = 0;
  unused << mDoc->SendSelectionCount(mID, &count);
  return count;
}

bool
ProxyAccessible::TextSubstring(int32_t aStartOffset, int32_t aEndOfset,
                               nsString& aText) const
{
  bool valid;
  unused << mDoc->SendTextSubstring(mID, aStartOffset, aEndOfset, &aText, &valid);
  return valid;
}

void
ProxyAccessible::GetTextAfterOffset(int32_t aOffset,
                                    AccessibleTextBoundary aBoundaryType,
                                    nsString& aText, int32_t* aStartOffset,
                                    int32_t* aEndOffset)
{
  unused << mDoc->SendGetTextAfterOffset(mID, aOffset, aBoundaryType,
                                         &aText, aStartOffset, aEndOffset);
}

void
ProxyAccessible::GetTextAtOffset(int32_t aOffset,
                                 AccessibleTextBoundary aBoundaryType,
                                 nsString& aText, int32_t* aStartOffset,
                                 int32_t* aEndOffset)
{
  unused << mDoc->SendGetTextAtOffset(mID, aOffset, aBoundaryType,
                                      &aText, aStartOffset, aEndOffset);
}

void
ProxyAccessible::GetTextBeforeOffset(int32_t aOffset,
                                     AccessibleTextBoundary aBoundaryType,
                                     nsString& aText, int32_t* aStartOffset,
                                     int32_t* aEndOffset)
{
  unused << mDoc->SendGetTextBeforeOffset(mID, aOffset, aBoundaryType,
                                          &aText, aStartOffset, aEndOffset);
}

char16_t
ProxyAccessible::CharAt(int32_t aOffset)
{
  uint16_t retval = 0;
  unused << mDoc->SendCharAt(mID, aOffset, &retval);
  return static_cast<char16_t>(retval);
}

void
ProxyAccessible::TextAttributes(bool aIncludeDefAttrs,
                                int32_t aOffset,
                                nsTArray<Attribute>* aAttributes,
                                int32_t* aStartOffset,
                                int32_t* aEndOffset)
{
  unused << mDoc->SendTextAttributes(mID, aIncludeDefAttrs, aOffset,
                                     aAttributes, aStartOffset, aEndOffset);
}

void
ProxyAccessible::DefaultTextAttributes(nsTArray<Attribute>* aAttrs)
{
  unused << mDoc->SendDefaultTextAttributes(mID, aAttrs);
}

nsIntRect
ProxyAccessible::TextBounds(int32_t aStartOffset, int32_t aEndOffset,
                            uint32_t aCoordType)
{
  nsIntRect rect;
  unused <<
    mDoc->SendTextBounds(mID, aStartOffset, aEndOffset, aCoordType, &rect);
  return rect;
}

nsIntRect
ProxyAccessible::CharBounds(int32_t aOffset, uint32_t aCoordType)
{
  nsIntRect rect;
  unused <<
    mDoc->SendCharBounds(mID, aOffset, aCoordType, &rect);
  return rect;
}

int32_t
ProxyAccessible::OffsetAtPoint(int32_t aX, int32_t aY, uint32_t aCoordType)
{
  int32_t retVal = -1;
  unused << mDoc->SendOffsetAtPoint(mID, aX, aY, aCoordType, &retVal);
  return retVal;
}

bool
ProxyAccessible::SelectionBoundsAt(int32_t aSelectionNum,
                                   nsString& aData,
                                   int32_t* aStartOffset,
                                   int32_t* aEndOffset)
{
  bool retVal = false;
  unused << mDoc->SendSelectionBoundsAt(mID, aSelectionNum, &retVal, &aData,
                                        aStartOffset, aEndOffset);
  return retVal;
}

bool
ProxyAccessible::SetSelectionBoundsAt(int32_t aSelectionNum,
                                      int32_t aStartOffset,
                                      int32_t aEndOffset)
{
  bool retVal = false;
  unused << mDoc->SendSetSelectionBoundsAt(mID, aSelectionNum, aStartOffset,
                                           aEndOffset, &retVal);
  return retVal;
}

bool
ProxyAccessible::AddToSelection(int32_t aStartOffset,
                                int32_t aEndOffset)
{
  bool retVal = false;
  unused << mDoc->SendAddToSelection(mID, aStartOffset, aEndOffset, &retVal);
  return retVal;
}

bool
ProxyAccessible::RemoveFromSelection(int32_t aSelectionNum)
{
  bool retVal = false;
  unused << mDoc->SendRemoveFromSelection(mID, aSelectionNum, &retVal);
  return retVal;
}

void
ProxyAccessible::ScrollSubstringTo(int32_t aStartOffset, int32_t aEndOffset,
                                   uint32_t aScrollType)
{
  unused << mDoc->SendScrollSubstringTo(mID, aStartOffset, aEndOffset, aScrollType);
}

void
ProxyAccessible::ScrollSubstringToPoint(int32_t aStartOffset,
                                        int32_t aEndOffset,
                                        uint32_t aCoordinateType,
                                        int32_t aX, int32_t aY)
{
  unused << mDoc->SendScrollSubstringToPoint(mID, aStartOffset, aEndOffset,
                                             aCoordinateType, aX, aY);
}

void
ProxyAccessible::ReplaceText(const nsString& aText)
{
  unused << mDoc->SendReplaceText(mID, aText);
}

bool
ProxyAccessible::InsertText(const nsString& aText, int32_t aPosition)
{
  bool valid;
  unused << mDoc->SendInsertText(mID, aText, aPosition, &valid);
  return valid;
}

bool
ProxyAccessible::CopyText(int32_t aStartPos, int32_t aEndPos)
{
  bool valid;
  unused << mDoc->SendCopyText(mID, aStartPos, aEndPos, &valid);
  return valid;
}

bool
ProxyAccessible::CutText(int32_t aStartPos, int32_t aEndPos)
{
  bool valid;
  unused << mDoc->SendCutText(mID, aStartPos, aEndPos, &valid);
  return valid;
}

bool
ProxyAccessible::DeleteText(int32_t aStartPos, int32_t aEndPos)
{
  bool valid;
  unused << mDoc->SendDeleteText(mID, aStartPos, aEndPos, &valid);
  return valid;
}

bool
ProxyAccessible::PasteText(int32_t aPosition)
{
  bool valid;
  unused << mDoc->SendPasteText(mID, aPosition, &valid);
  return valid;
}

nsIntPoint
ProxyAccessible::ImagePosition(uint32_t aCoordType)
{
  nsIntPoint retVal;
  unused << mDoc->SendImagePosition(mID, aCoordType, &retVal);
  return retVal;
}

nsIntSize
ProxyAccessible::ImageSize()
{
  nsIntSize retVal;
  unused << mDoc->SendImageSize(mID, &retVal);
  return retVal;
}

uint32_t
ProxyAccessible::StartOffset(bool* aOk)
{
  uint32_t retVal = 0;
  unused << mDoc->SendStartOffset(mID, &retVal, aOk);
  return retVal;
}

uint32_t
ProxyAccessible::EndOffset(bool* aOk)
{
  uint32_t retVal = 0;
  unused << mDoc->SendEndOffset(mID, &retVal, aOk);
  return retVal;
}

bool
ProxyAccessible::IsLinkValid()
{
  bool retVal = false;
  unused << mDoc->SendIsLinkValid(mID, &retVal);
  return retVal;
}

uint32_t
ProxyAccessible::AnchorCount(bool* aOk)
{
  uint32_t retVal = 0;
  unused << mDoc->SendAnchorCount(mID, &retVal, aOk);
  return retVal;
}

void
ProxyAccessible::AnchorURIAt(uint32_t aIndex, nsCString& aURI, bool* aOk)
{
  unused << mDoc->SendAnchorURIAt(mID, aIndex, &aURI, aOk);
}

ProxyAccessible*
ProxyAccessible::AnchorAt(uint32_t aIndex)
{
  uint64_t id = 0;
  bool ok = false;
  unused << mDoc->SendAnchorAt(mID, aIndex, &id, &ok);
  return ok ? mDoc->GetAccessible(id) : nullptr;
}

uint32_t
ProxyAccessible::LinkCount()
{
  uint32_t retVal = 0;
  unused << mDoc->SendLinkCount(mID, &retVal);
  return retVal;
}

ProxyAccessible*
ProxyAccessible::LinkAt(const uint32_t& aIndex)
{
  uint64_t linkID = 0;
  bool ok = false;
  unused << mDoc->SendLinkAt(mID, aIndex, &linkID, &ok);
  return ok ? mDoc->GetAccessible(linkID) : nullptr;
}

int32_t
ProxyAccessible::LinkIndexOf(ProxyAccessible* aLink)
{
  int32_t retVal = -1;
  if (aLink) {
    unused << mDoc->SendLinkIndexOf(mID, aLink->ID(), &retVal);
  }

  return retVal;
}

int32_t
ProxyAccessible::LinkIndexAtOffset(uint32_t aOffset)
{
  int32_t retVal = -1;
  unused << mDoc->SendLinkIndexAtOffset(mID, aOffset, &retVal);
  return retVal;
}

ProxyAccessible*
ProxyAccessible::TableOfACell()
{
  uint64_t tableID = 0;
  bool ok = false;
  unused << mDoc->SendTableOfACell(mID, &tableID, &ok);
  return ok ? mDoc->GetAccessible(tableID) : nullptr;
}

uint32_t
ProxyAccessible::ColIdx()
{
  uint32_t index = 0;
  unused << mDoc->SendColIdx(mID, &index);
  return index;
}

uint32_t
ProxyAccessible::RowIdx()
{
  uint32_t index = 0;
  unused << mDoc->SendRowIdx(mID, &index);
  return index;
}

uint32_t
ProxyAccessible::ColExtent()
{
  uint32_t extent = 0;
  unused << mDoc->SendColExtent(mID, &extent);
  return extent;
}

uint32_t
ProxyAccessible::RowExtent()
{
  uint32_t extent = 0;
  unused << mDoc->SendRowExtent(mID, &extent);
  return extent;
}

void
ProxyAccessible::ColHeaderCells(nsTArray<uint64_t>* aCells)
{
  unused << mDoc->SendColHeaderCells(mID, aCells);
}

void
ProxyAccessible::RowHeaderCells(nsTArray<uint64_t>* aCells)
{
  unused << mDoc->SendRowHeaderCells(mID, aCells);
}

bool
ProxyAccessible::IsCellSelected()
{
  bool selected = false;
  unused << mDoc->SendIsCellSelected(mID, &selected);
  return selected;
}

ProxyAccessible*
ProxyAccessible::TableCaption()
{
  uint64_t captionID = 0;
  bool ok = false;
  unused << mDoc->SendTableCaption(mID, &captionID, &ok);
  return ok ? mDoc->GetAccessible(captionID) : nullptr;
}

void
ProxyAccessible::TableSummary(nsString& aSummary)
{
  unused << mDoc->SendTableSummary(mID, &aSummary);
}

uint32_t
ProxyAccessible::TableColumnCount()
{
  uint32_t count = 0;
  unused << mDoc->SendTableColumnCount(mID, &count);
  return count;
}

uint32_t
ProxyAccessible::TableRowCount()
{
  uint32_t count = 0;
  unused << mDoc->SendTableRowCount(mID, &count);
  return count;
}

ProxyAccessible*
ProxyAccessible::TableCellAt(uint32_t aRow, uint32_t aCol)
{
  uint64_t cellID = 0;
  bool ok = false;
  unused << mDoc->SendTableCellAt(mID, aRow, aCol, &cellID, &ok);
  return ok ? mDoc->GetAccessible(cellID) : nullptr;
}

int32_t
ProxyAccessible::TableCellIndexAt(uint32_t aRow, uint32_t aCol)
{
  int32_t index = 0;
  unused << mDoc->SendTableCellIndexAt(mID, aRow, aCol, &index);
  return index;
}

int32_t
ProxyAccessible::TableColumnIndexAt(uint32_t aCellIndex)
{
  int32_t index = 0;
  unused << mDoc->SendTableColumnIndexAt(mID, aCellIndex, &index);
  return index;
}

int32_t
ProxyAccessible::TableRowIndexAt(uint32_t aCellIndex)
{
  int32_t index = 0;
  unused << mDoc->SendTableRowIndexAt(mID, aCellIndex, &index);
  return index;
}

void
ProxyAccessible::TableRowAndColumnIndicesAt(uint32_t aCellIndex,
                                            int32_t* aRow, int32_t* aCol)
{
  unused << mDoc->SendTableRowAndColumnIndicesAt(mID, aCellIndex, aRow, aCol);
}

uint32_t
ProxyAccessible::TableColumnExtentAt(uint32_t aRow, uint32_t aCol)
{
  uint32_t extent = 0;
  unused << mDoc->SendTableColumnExtentAt(mID, aRow, aCol, &extent);
  return extent;
}

uint32_t
ProxyAccessible::TableRowExtentAt(uint32_t aRow, uint32_t aCol)
{
  uint32_t extent = 0;
  unused << mDoc->SendTableRowExtentAt(mID, aRow, aCol, &extent);
  return extent;
}

void
ProxyAccessible::TableColumnDescription(uint32_t aCol, nsString& aDescription)
{
  unused << mDoc->SendTableColumnDescription(mID, aCol, &aDescription);
}

void
ProxyAccessible::TableRowDescription(uint32_t aRow, nsString& aDescription)
{
  unused << mDoc->SendTableRowDescription(mID, aRow, &aDescription);
}

bool
ProxyAccessible::TableColumnSelected(uint32_t aCol)
{
  bool selected = false;
  unused << mDoc->SendTableColumnSelected(mID, aCol, &selected);
  return selected;
}

bool
ProxyAccessible::TableRowSelected(uint32_t aRow)
{
  bool selected = false;
  unused << mDoc->SendTableRowSelected(mID, aRow, &selected);
  return selected;
}

bool
ProxyAccessible::TableCellSelected(uint32_t aRow, uint32_t aCol)
{
  bool selected = false;
  unused << mDoc->SendTableCellSelected(mID, aRow, aCol, &selected);
  return selected;
}

uint32_t
ProxyAccessible::TableSelectedCellCount()
{
  uint32_t count = 0;
  unused << mDoc->SendTableSelectedCellCount(mID, &count);
  return count;
}

uint32_t
ProxyAccessible::TableSelectedColumnCount()
{
  uint32_t count = 0;
  unused << mDoc->SendTableSelectedColumnCount(mID, &count);
  return count;
}

uint32_t
ProxyAccessible::TableSelectedRowCount()
{
  uint32_t count = 0;
  unused << mDoc->SendTableSelectedRowCount(mID, &count);
  return count;
}

void
ProxyAccessible::TableSelectedCells(nsTArray<ProxyAccessible*>* aCellIDs)
{
  nsAutoTArray<uint64_t, 30> cellIDs;
  unused << mDoc->SendTableSelectedCells(mID, &cellIDs);
  aCellIDs->SetCapacity(cellIDs.Length());
  for (uint32_t i = 0; i < cellIDs.Length(); ++i) {
    aCellIDs->AppendElement(mDoc->GetAccessible(cellIDs[i]));
  }
}

void
ProxyAccessible::TableSelectedCellIndices(nsTArray<uint32_t>* aCellIndices)
{
  unused << mDoc->SendTableSelectedCellIndices(mID, aCellIndices);
}

void
ProxyAccessible::TableSelectedColumnIndices(nsTArray<uint32_t>* aColumnIndices)
{
  unused << mDoc->SendTableSelectedColumnIndices(mID, aColumnIndices);
}

void
ProxyAccessible::TableSelectedRowIndices(nsTArray<uint32_t>* aRowIndices)
{
  unused << mDoc->SendTableSelectedRowIndices(mID, aRowIndices);
}

void
ProxyAccessible::TableSelectColumn(uint32_t aCol)
{
  unused << mDoc->SendTableSelectColumn(mID, aCol);
}

void
ProxyAccessible::TableSelectRow(uint32_t aRow)
{
  unused << mDoc->SendTableSelectRow(mID, aRow);
}

void
ProxyAccessible::TableUnselectColumn(uint32_t aCol)
{
  unused << mDoc->SendTableUnselectColumn(mID, aCol);
}

void
ProxyAccessible::TableUnselectRow(uint32_t aRow)
{
  unused << mDoc->SendTableUnselectRow(mID, aRow);
}

bool
ProxyAccessible::TableIsProbablyForLayout()
{
  bool forLayout = false;
  unused << mDoc->SendTableIsProbablyForLayout(mID, &forLayout);
  return forLayout;
}

void
ProxyAccessible::SelectedItems(nsTArray<ProxyAccessible*>* aSelectedItems)
{
  nsAutoTArray<uint64_t, 10> itemIDs;
  unused << mDoc->SendSelectedItems(mID, &itemIDs);
  aSelectedItems->SetCapacity(itemIDs.Length());
  for (size_t i = 0; i < itemIDs.Length(); ++i) {
    aSelectedItems->AppendElement(mDoc->GetAccessible(itemIDs[i]));
  }
}

uint32_t
ProxyAccessible::SelectedItemCount()
{
  uint32_t count = 0;
  unused << mDoc->SendSelectedItemCount(mID, &count);
  return count;
}

ProxyAccessible*
ProxyAccessible::GetSelectedItem(uint32_t aIndex)
{
  uint64_t selectedItemID = 0;
  bool ok = false;
  unused << mDoc->SendGetSelectedItem(mID, aIndex, &selectedItemID, &ok);
  return ok ? mDoc->GetAccessible(selectedItemID) : nullptr;
}

bool
ProxyAccessible::IsItemSelected(uint32_t aIndex)
{
  bool selected = false;
  unused << mDoc->SendIsItemSelected(mID, aIndex, &selected);
  return selected;
}
 
bool
ProxyAccessible::AddItemToSelection(uint32_t aIndex)
{
  bool success = false;
  unused << mDoc->SendAddItemToSelection(mID, aIndex, &success);
  return success;
}

bool
ProxyAccessible::RemoveItemFromSelection(uint32_t aIndex)
{
  bool success = false;
  unused << mDoc->SendRemoveItemFromSelection(mID, aIndex, &success);
  return success;
}

bool
ProxyAccessible::SelectAll()
{
  bool success = false;
  unused << mDoc->SendSelectAll(mID, &success);
  return success;
}

bool
ProxyAccessible::UnselectAll()
{
  bool success = false;
  unused << mDoc->SendUnselectAll(mID, &success);
  return success;
}

bool
ProxyAccessible::DoAction(uint8_t aIndex)
{
  bool success = false;
  unused << mDoc->SendDoAction(mID, aIndex, &success);
  return success;
}

uint8_t
ProxyAccessible::ActionCount()
{
  uint8_t count = 0;
  unused << mDoc->SendActionCount(mID, &count);
  return count;
}

void
ProxyAccessible::ActionDescriptionAt(uint8_t aIndex, nsString& aDescription)
{
  unused << mDoc->SendActionDescriptionAt(mID, aIndex, &aDescription);
}

void
ProxyAccessible::ActionNameAt(uint8_t aIndex, nsString& aName)
{
  unused << mDoc->SendActionNameAt(mID, aIndex, &aName);
}

KeyBinding
ProxyAccessible::AccessKey()
{
  uint32_t key = 0;
  uint32_t modifierMask = 0;
  unused << mDoc->SendAccessKey(mID, &key, &modifierMask);
  return KeyBinding(key, modifierMask);
}

KeyBinding
ProxyAccessible::KeyboardShortcut()
{
  uint32_t key = 0;
  uint32_t modifierMask = 0;
  unused << mDoc->SendKeyboardShortcut(mID, &key, &modifierMask);
  return KeyBinding(key, modifierMask);
}

double
ProxyAccessible::CurValue()
{
  double val = UnspecifiedNaN<double>();
  unused << mDoc->SendCurValue(mID, &val);
  return val;
}

bool
ProxyAccessible::SetCurValue(double aValue)
{
  bool success = false;
  unused << mDoc->SendSetCurValue(mID, aValue, &success);
  return success;
}

double
ProxyAccessible::MinValue()
{
  double val = UnspecifiedNaN<double>();
  unused << mDoc->SendMinValue(mID, &val);
  return val;
}

double
ProxyAccessible::MaxValue()
{
  double val = UnspecifiedNaN<double>();
  unused << mDoc->SendMaxValue(mID, &val);
  return val;
}

double
ProxyAccessible::Step()
{
  double step = UnspecifiedNaN<double>();
  unused << mDoc->SendStep(mID, &step);
  return step;
}

void
ProxyAccessible::TakeFocus()
{
  unused << mDoc->SendTakeFocus(mID);
}

ProxyAccessible*
ProxyAccessible::ChildAtPoint(int32_t aX, int32_t aY,
                              Accessible::EWhichChildAtPoint aWhichChild)
{
  uint64_t childID = 0;
  bool ok = false;
  unused << mDoc->SendChildAtPoint(mID, aX, aY,
                                   static_cast<uint32_t>(aWhichChild),
                                   &childID, &ok);
  return ok ? mDoc->GetAccessible(childID) : nullptr;
}

nsIntRect
ProxyAccessible::Bounds()
{
  nsIntRect rect;
  unused << mDoc->SendBounds(mID, &rect);
  return rect;
}

void
ProxyAccessible::Language(nsString& aLocale)
{
  unused << mDoc->SendLanguage(mID, &aLocale);
}

void
ProxyAccessible::DocType(nsString& aType)
{
  unused << mDoc->SendDocType(mID, &aType);
}

void
ProxyAccessible::URL(nsString& aURL)
{
  unused << mDoc->SendURL(mID, &aURL);
}

void
ProxyAccessible::MimeType(nsString aMime)
{
  unused << mDoc->SendMimeType(mID, &aMime);
}

void
ProxyAccessible::URLDocTypeMimeType(nsString& aURL, nsString& aDocType,
                                    nsString& aMimeType)
{
  unused << mDoc->SendURLDocTypeMimeType(mID, &aURL, &aDocType, &aMimeType);
}

}
}
