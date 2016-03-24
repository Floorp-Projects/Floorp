/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULTreeAccessible.h"

#include "Accessible-inl.h"
#include "DocAccessible-inl.h"
#include "nsAccCache.h"
#include "nsAccUtils.h"
#include "nsCoreUtils.h"
#include "nsEventShell.h"
#include "DocAccessible.h"
#include "Relation.h"
#include "Role.h"
#include "States.h"
#include "XULTreeGridAccessible.h"
#include "nsQueryObject.h"

#include "nsComponentManagerUtils.h"
#include "nsIAccessibleRelation.h"
#include "nsIAutoCompleteInput.h"
#include "nsIAutoCompletePopup.h"
#include "nsIBoxObject.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMXULMenuListElement.h"
#include "nsIDOMXULMultSelectCntrlEl.h"
#include "nsIDOMXULTreeElement.h"
#include "nsITreeSelection.h"
#include "nsIMutableArray.h"
#include "nsTreeBodyFrame.h"
#include "nsTreeColumns.h"
#include "nsTreeUtils.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// XULTreeAccessible
////////////////////////////////////////////////////////////////////////////////

XULTreeAccessible::
  XULTreeAccessible(nsIContent* aContent, DocAccessible* aDoc,
                    nsTreeBodyFrame* aTreeFrame) :
  AccessibleWrap(aContent, aDoc),
  mAccessibleCache(kDefaultTreeCacheLength)
{
  mType = eXULTreeType;
  mGenericTypes |= eSelect;

  nsCOMPtr<nsITreeView> view = aTreeFrame->GetExistingView();
  mTreeView = view;

  mTree = nsCoreUtils::GetTreeBoxObject(aContent);
  NS_ASSERTION(mTree, "Can't get mTree!\n");

  nsIContent* parentContent = mContent->GetParent();
  if (parentContent) {
    nsCOMPtr<nsIAutoCompletePopup> autoCompletePopupElm =
      do_QueryInterface(parentContent);
    if (autoCompletePopupElm)
      mGenericTypes |= eAutoCompletePopup;
  }
}

XULTreeAccessible::~XULTreeAccessible()
{
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeAccessible: nsISupports and cycle collection implementation

NS_IMPL_CYCLE_COLLECTION_INHERITED(XULTreeAccessible, Accessible,
                                   mTree, mAccessibleCache)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(XULTreeAccessible)
NS_INTERFACE_MAP_END_INHERITING(Accessible)

NS_IMPL_ADDREF_INHERITED(XULTreeAccessible, Accessible)
NS_IMPL_RELEASE_INHERITED(XULTreeAccessible, Accessible)

////////////////////////////////////////////////////////////////////////////////
// XULTreeAccessible: Accessible implementation

uint64_t
XULTreeAccessible::NativeState()
{
  // Get focus status from base class.
  uint64_t state = Accessible::NativeState();

  // readonly state
  state |= states::READONLY;

  // multiselectable state.
  if (!mTreeView)
    return state;

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_TRUE(selection, state);

  bool isSingle = false;
  nsresult rv = selection->GetSingle(&isSingle);
  NS_ENSURE_SUCCESS(rv, state);

  if (!isSingle)
    state |= states::MULTISELECTABLE;

  return state;
}

void
XULTreeAccessible::Value(nsString& aValue)
{
  aValue.Truncate();
  if (!mTreeView)
    return;

  // Return the value is the first selected child.
  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (!selection)
    return;

  int32_t currentIndex;
  nsCOMPtr<nsIDOMElement> selectItem;
  selection->GetCurrentIndex(&currentIndex);
  if (currentIndex >= 0) {
    nsCOMPtr<nsITreeColumn> keyCol;

    nsCOMPtr<nsITreeColumns> cols;
    mTree->GetColumns(getter_AddRefs(cols));
    if (cols)
      cols->GetKeyColumn(getter_AddRefs(keyCol));

    mTreeView->GetCellText(currentIndex, keyCol, aValue);
  }

}

////////////////////////////////////////////////////////////////////////////////
// XULTreeAccessible: Accessible implementation

void
XULTreeAccessible::Shutdown()
{
  if (!mDoc->IsDefunct()) {
    UnbindCacheEntriesFromDocument(mAccessibleCache);
  }

  mTree = nullptr;
  mTreeView = nullptr;

  AccessibleWrap::Shutdown();
}

role
XULTreeAccessible::NativeRole()
{
  // No primary column means we're in a list. In fact, history and mail turn off
  // the primary flag when switching to a flat view.

  nsIContent* child = nsTreeUtils::GetDescendantChild(mContent, nsGkAtoms::treechildren);
  NS_ASSERTION(child, "tree without treechildren!");
  nsTreeBodyFrame* treeFrame = do_QueryFrame(child->GetPrimaryFrame());
  NS_ASSERTION(treeFrame, "xul tree accessible for tree without a frame!");
  if (!treeFrame)
    return roles::LIST;

  RefPtr<nsTreeColumns> cols = treeFrame->Columns();
  nsCOMPtr<nsITreeColumn> primaryCol;
  cols->GetPrimaryColumn(getter_AddRefs(primaryCol));

  return primaryCol ? roles::OUTLINE : roles::LIST;
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeAccessible: Accessible implementation (DON'T put methods here)

Accessible*
XULTreeAccessible::ChildAtPoint(int32_t aX, int32_t aY,
                                EWhichChildAtPoint aWhichChild)
{
  nsIFrame *frame = GetFrame();
  if (!frame)
    return nullptr;

  nsPresContext *presContext = frame->PresContext();
  nsIPresShell* presShell = presContext->PresShell();

  nsIFrame *rootFrame = presShell->GetRootFrame();
  NS_ENSURE_TRUE(rootFrame, nullptr);

  nsIntRect rootRect = rootFrame->GetScreenRect();

  int32_t clientX = presContext->DevPixelsToIntCSSPixels(aX) - rootRect.x;
  int32_t clientY = presContext->DevPixelsToIntCSSPixels(aY) - rootRect.y;

  int32_t row = -1;
  nsCOMPtr<nsITreeColumn> column;
  nsAutoString childEltUnused;
  mTree->GetCellAt(clientX, clientY, &row, getter_AddRefs(column),
                   childEltUnused);

  // If we failed to find tree cell for the given point then it might be
  // tree columns.
  if (row == -1 || !column)
    return AccessibleWrap::ChildAtPoint(aX, aY, aWhichChild);

  Accessible* child = GetTreeItemAccessible(row);
  if (aWhichChild == eDeepestChild && child) {
    // Look for accessible cell for the found item accessible.
    RefPtr<XULTreeItemAccessibleBase> treeitem = do_QueryObject(child);

    Accessible* cell = treeitem->GetCellAccessible(column);
    if (cell)
      child = cell;
  }

  return child;
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeAccessible: SelectAccessible

Accessible*
XULTreeAccessible::CurrentItem()
{
  if (!mTreeView)
    return nullptr;

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (selection) {
    int32_t currentIndex = -1;
    selection->GetCurrentIndex(&currentIndex);
    if (currentIndex >= 0)
      return GetTreeItemAccessible(currentIndex);
  }

  return nullptr;
}

void
XULTreeAccessible::SetCurrentItem(Accessible* aItem)
{
  NS_ERROR("XULTreeAccessible::SetCurrentItem not implemented");
}

void
XULTreeAccessible::SelectedItems(nsTArray<Accessible*>* aItems)
{
  if (!mTreeView)
    return;

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (!selection)
    return;

  int32_t rangeCount = 0;
  selection->GetRangeCount(&rangeCount);
  for (int32_t rangeIdx = 0; rangeIdx < rangeCount; rangeIdx++) {
    int32_t firstIdx = 0, lastIdx = -1;
    selection->GetRangeAt(rangeIdx, &firstIdx, &lastIdx);
    for (int32_t rowIdx = firstIdx; rowIdx <= lastIdx; rowIdx++) {
      Accessible* item = GetTreeItemAccessible(rowIdx);
      if (item)
        aItems->AppendElement(item);
    }
  }
}

uint32_t
XULTreeAccessible::SelectedItemCount()
{
  if (!mTreeView)
    return 0;

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (selection) {
    int32_t count = 0;
    selection->GetCount(&count);
    return count;
  }

  return 0;
}

bool
XULTreeAccessible::AddItemToSelection(uint32_t aIndex)
{
  if (!mTreeView)
    return false;

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (selection) {
    bool isSelected = false;
    selection->IsSelected(aIndex, &isSelected);
    if (!isSelected)
      selection->ToggleSelect(aIndex);

    return true;
  }
  return false;
}

bool
XULTreeAccessible::RemoveItemFromSelection(uint32_t aIndex)
{
  if (!mTreeView)
    return false;

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (selection) {
    bool isSelected = false;
    selection->IsSelected(aIndex, &isSelected);
    if (isSelected)
      selection->ToggleSelect(aIndex);

    return true;
  }
  return false;
}

bool
XULTreeAccessible::IsItemSelected(uint32_t aIndex)
{
  if (!mTreeView)
    return false;

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (selection) {
    bool isSelected = false;
    selection->IsSelected(aIndex, &isSelected);
    return isSelected;
  }
  return false;
}

bool
XULTreeAccessible::UnselectAll()
{
  if (!mTreeView)
    return false;

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (!selection)
    return false;

  selection->ClearSelection();
  return true;
}

Accessible*
XULTreeAccessible::GetSelectedItem(uint32_t aIndex)
{
  if (!mTreeView)
    return nullptr;

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (!selection)
    return nullptr;

  uint32_t selCount = 0;
  int32_t rangeCount = 0;
  selection->GetRangeCount(&rangeCount);
  for (int32_t rangeIdx = 0; rangeIdx < rangeCount; rangeIdx++) {
    int32_t firstIdx = 0, lastIdx = -1;
    selection->GetRangeAt(rangeIdx, &firstIdx, &lastIdx);
    for (int32_t rowIdx = firstIdx; rowIdx <= lastIdx; rowIdx++) {
      if (selCount == aIndex)
        return GetTreeItemAccessible(rowIdx);

      selCount++;
    }
  }

  return nullptr;
}

bool
XULTreeAccessible::SelectAll()
{
  // see if we are multiple select if so set ourselves as such
  if (!mTreeView)
    return false;

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (selection) {
    bool single = false;
    selection->GetSingle(&single);
    if (!single) {
      selection->SelectAll();
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeAccessible: Accessible implementation

Accessible*
XULTreeAccessible::GetChildAt(uint32_t aIndex) const
{
  uint32_t childCount = Accessible::ChildCount();
  if (aIndex < childCount)
    return Accessible::GetChildAt(aIndex);

  return GetTreeItemAccessible(aIndex - childCount);
}

uint32_t
XULTreeAccessible::ChildCount() const
{
  // Tree's children count is row count + treecols count.
  uint32_t childCount = Accessible::ChildCount();
  if (!mTreeView)
    return childCount;

  int32_t rowCount = 0;
  mTreeView->GetRowCount(&rowCount);
  childCount += rowCount;

  return childCount;
}

Relation
XULTreeAccessible::RelationByType(RelationType aType)
{
  if (aType == RelationType::NODE_PARENT_OF) {
    if (mTreeView)
      return Relation(new XULTreeItemIterator(this, mTreeView, -1));

    return Relation();
  }

  return Accessible::RelationByType(aType);
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeAccessible: Widgets

bool
XULTreeAccessible::IsWidget() const
{
  return true;
}

bool
XULTreeAccessible::IsActiveWidget() const
{
  if (IsAutoCompletePopup()) {
    nsCOMPtr<nsIAutoCompletePopup> autoCompletePopupElm =
      do_QueryInterface(mContent->GetParent());

    if (autoCompletePopupElm) {
      bool isOpen = false;
      autoCompletePopupElm->GetPopupOpen(&isOpen);
      return isOpen;
    }
  }
  return FocusMgr()->HasDOMFocus(mContent);
}

bool
XULTreeAccessible::AreItemsOperable() const
{
  if (IsAutoCompletePopup()) {
    nsCOMPtr<nsIAutoCompletePopup> autoCompletePopupElm =
      do_QueryInterface(mContent->GetParent());

    if (autoCompletePopupElm) {
      bool isOpen = false;
      autoCompletePopupElm->GetPopupOpen(&isOpen);
      return isOpen;
    }
  }
  return true;
}

Accessible*
XULTreeAccessible::ContainerWidget() const
{
  if (IsAutoCompletePopup()) {
    // This works for XUL autocompletes. It doesn't work for HTML forms
    // autocomplete because of potential crossprocess calls (when autocomplete
    // lives in content process while popup lives in chrome process). If that's
    // a problem then rethink Widgets interface.
    nsCOMPtr<nsIDOMXULMenuListElement> menuListElm =
      do_QueryInterface(mContent->GetParent());
    if (menuListElm) {
      nsCOMPtr<nsIDOMNode> inputElm;
      menuListElm->GetInputField(getter_AddRefs(inputElm));
      if (inputElm) {
        nsCOMPtr<nsINode> inputNode = do_QueryInterface(inputElm);
        if (inputNode) {
          Accessible* input = 
            mDoc->GetAccessible(inputNode);
          return input ? input->ContainerWidget() : nullptr;
        }
      }
    }
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeAccessible: public implementation

Accessible*
XULTreeAccessible::GetTreeItemAccessible(int32_t aRow) const
{
  if (aRow < 0 || IsDefunct() || !mTreeView)
    return nullptr;

  int32_t rowCount = 0;
  nsresult rv = mTreeView->GetRowCount(&rowCount);
  if (NS_FAILED(rv) || aRow >= rowCount)
    return nullptr;

  void *key = reinterpret_cast<void*>(intptr_t(aRow));
  Accessible* cachedTreeItem = mAccessibleCache.GetWeak(key);
  if (cachedTreeItem)
    return cachedTreeItem;

  RefPtr<Accessible> treeItem = CreateTreeItemAccessible(aRow);
  if (treeItem) {
    mAccessibleCache.Put(key, treeItem);
    Document()->BindToDocument(treeItem, nullptr);
    return treeItem;
  }

  return nullptr;
}

void
XULTreeAccessible::InvalidateCache(int32_t aRow, int32_t aCount)
{
  if (IsDefunct())
    return;

  if (!mTreeView) {
    UnbindCacheEntriesFromDocument(mAccessibleCache);
    return;
  }

  // Do not invalidate the cache if rows have been inserted.
  if (aCount > 0)
    return;

  DocAccessible* document = Document();

  // Fire destroy event for removed tree items and delete them from caches.
  for (int32_t rowIdx = aRow; rowIdx < aRow - aCount; rowIdx++) {

    void* key = reinterpret_cast<void*>(intptr_t(rowIdx));
    Accessible* treeItem = mAccessibleCache.GetWeak(key);

    if (treeItem) {
      RefPtr<AccEvent> event =
        new AccEvent(nsIAccessibleEvent::EVENT_HIDE, treeItem);
      nsEventShell::FireEvent(event);

      // Unbind from document, shutdown and remove from tree cache.
      document->UnbindFromDocument(treeItem);
      mAccessibleCache.Remove(key);
    }
  }

  // We dealt with removed tree items already however we may keep tree items
  // having row indexes greater than row count. We should remove these dead tree
  // items silently from caches.
  int32_t newRowCount = 0;
  nsresult rv = mTreeView->GetRowCount(&newRowCount);
  if (NS_FAILED(rv))
    return;

  int32_t oldRowCount = newRowCount - aCount;

  for (int32_t rowIdx = newRowCount; rowIdx < oldRowCount; ++rowIdx) {

    void *key = reinterpret_cast<void*>(intptr_t(rowIdx));
    Accessible* treeItem = mAccessibleCache.GetWeak(key);

    if (treeItem) {
      // Unbind from document, shutdown and remove from tree cache.
      document->UnbindFromDocument(treeItem);
      mAccessibleCache.Remove(key);
    }
  }
}

void
XULTreeAccessible::TreeViewInvalidated(int32_t aStartRow, int32_t aEndRow,
                                       int32_t aStartCol, int32_t aEndCol)
{
  if (IsDefunct())
    return;

  if (!mTreeView) {
    UnbindCacheEntriesFromDocument(mAccessibleCache);
    return;
  }

  int32_t endRow = aEndRow;

  nsresult rv;
  if (endRow == -1) {
    int32_t rowCount = 0;
    rv = mTreeView->GetRowCount(&rowCount);
    if (NS_FAILED(rv))
      return;

    endRow = rowCount - 1;
  }

  nsCOMPtr<nsITreeColumns> treeColumns;
  mTree->GetColumns(getter_AddRefs(treeColumns));
  if (!treeColumns)
    return;

  int32_t endCol = aEndCol;

  if (endCol == -1) {
    int32_t colCount = 0;
    rv = treeColumns->GetCount(&colCount);
    if (NS_FAILED(rv))
      return;

    endCol = colCount - 1;
  }

  for (int32_t rowIdx = aStartRow; rowIdx <= endRow; ++rowIdx) {

    void *key = reinterpret_cast<void*>(intptr_t(rowIdx));
    Accessible* accessible = mAccessibleCache.GetWeak(key);

    if (accessible) {
      RefPtr<XULTreeItemAccessibleBase> treeitemAcc = do_QueryObject(accessible);
      NS_ASSERTION(treeitemAcc, "Wrong accessible at the given key!");

      treeitemAcc->RowInvalidated(aStartCol, endCol);
    }
  }
}

void
XULTreeAccessible::TreeViewChanged(nsITreeView* aView)
{
  if (IsDefunct())
    return;

  // Fire reorder event on tree accessible on accessible tree (do not fire
  // show/hide events on tree items because it can be expensive to fire them for
  // each tree item.
  RefPtr<AccReorderEvent> reorderEvent = new AccReorderEvent(this);
  Document()->FireDelayedEvent(reorderEvent);

  // Clear cache.
  UnbindCacheEntriesFromDocument(mAccessibleCache);

  mTreeView = aView;
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeAccessible: protected implementation

already_AddRefed<Accessible>
XULTreeAccessible::CreateTreeItemAccessible(int32_t aRow) const
{
  RefPtr<Accessible> accessible =
    new XULTreeItemAccessible(mContent, mDoc, const_cast<XULTreeAccessible*>(this),
                              mTree, mTreeView, aRow);

  return accessible.forget();
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeItemAccessibleBase
////////////////////////////////////////////////////////////////////////////////

XULTreeItemAccessibleBase::
  XULTreeItemAccessibleBase(nsIContent* aContent, DocAccessible* aDoc,
                            Accessible* aParent, nsITreeBoxObject* aTree,
                            nsITreeView* aTreeView, int32_t aRow) :
  AccessibleWrap(aContent, aDoc),
  mTree(aTree), mTreeView(aTreeView), mRow(aRow)
{
  mParent = aParent;
  mStateFlags |= eSharedNode;
}

XULTreeItemAccessibleBase::~XULTreeItemAccessibleBase()
{
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeItemAccessibleBase: nsISupports implementation

NS_IMPL_CYCLE_COLLECTION_INHERITED(XULTreeItemAccessibleBase, Accessible,
                                   mTree)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(XULTreeItemAccessibleBase)
  NS_INTERFACE_TABLE_INHERITED(XULTreeItemAccessibleBase,
                               XULTreeItemAccessibleBase)
NS_INTERFACE_TABLE_TAIL_INHERITING(Accessible)
NS_IMPL_ADDREF_INHERITED(XULTreeItemAccessibleBase, Accessible)
NS_IMPL_RELEASE_INHERITED(XULTreeItemAccessibleBase, Accessible)

////////////////////////////////////////////////////////////////////////////////
// XULTreeItemAccessibleBase: Accessible

Accessible*
XULTreeItemAccessibleBase::FocusedChild()
{
  return FocusMgr()->FocusedAccessible() == this ? this : nullptr;
}

nsIntRect
XULTreeItemAccessibleBase::Bounds() const
{
  // Get x coordinate and width from treechildren element, get y coordinate and
  // height from tree cell.

  nsCOMPtr<nsIBoxObject> boxObj = nsCoreUtils::GetTreeBodyBoxObject(mTree);
  if (!boxObj)
    return nsIntRect();

  nsCOMPtr<nsITreeColumn> column = nsCoreUtils::GetFirstSensibleColumn(mTree);

  int32_t x = 0, y = 0, width = 0, height = 0;
  nsresult rv = mTree->GetCoordsForCellItem(mRow, column, EmptyString(),
                                            &x, &y, &width, &height);
  if (NS_FAILED(rv))
    return nsIntRect();

  boxObj->GetWidth(&width);

  int32_t tcX = 0, tcY = 0;
  boxObj->GetScreenX(&tcX);
  boxObj->GetScreenY(&tcY);

  x = tcX;
  y += tcY;

  nsPresContext* presContext = mDoc->PresContext();
  return nsIntRect(presContext->CSSPixelsToDevPixels(x),
                   presContext->CSSPixelsToDevPixels(y),
                   presContext->CSSPixelsToDevPixels(width),
                   presContext->CSSPixelsToDevPixels(height));
}

void
XULTreeItemAccessibleBase::SetSelected(bool aSelect)
{
  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (selection) {
    bool isSelected = false;
    selection->IsSelected(mRow, &isSelected);
    if (isSelected != aSelect)
      selection->ToggleSelect(mRow);
  }
}

void
XULTreeItemAccessibleBase::TakeFocus()
{
  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (selection)
    selection->SetCurrentIndex(mRow);

  // focus event will be fired here
  Accessible::TakeFocus();
}

Relation
XULTreeItemAccessibleBase::RelationByType(RelationType aType)
{

  switch (aType) {
    case RelationType::NODE_CHILD_OF: {
      int32_t parentIndex = -1;
      if (!NS_SUCCEEDED(mTreeView->GetParentIndex(mRow, &parentIndex)))
        return Relation();

      if (parentIndex == -1)
        return Relation(mParent);

      XULTreeAccessible* treeAcc = mParent->AsXULTree();
      return Relation(treeAcc->GetTreeItemAccessible(parentIndex));
    }

    case RelationType::NODE_PARENT_OF: {
      bool isTrue = false;
      if (NS_FAILED(mTreeView->IsContainerEmpty(mRow, &isTrue)) || isTrue)
        return Relation();

      if (NS_FAILED(mTreeView->IsContainerOpen(mRow, &isTrue)) || !isTrue)
        return Relation();

      XULTreeAccessible* tree = mParent->AsXULTree();
      return Relation(new XULTreeItemIterator(tree, mTreeView, mRow));
    }

    default:
      return Relation();
  }
}

uint8_t
XULTreeItemAccessibleBase::ActionCount()
{
  // "activate" action is available for all treeitems, "expand/collapse" action
  // is avaible for treeitem which is container.
  return IsExpandable() ? 2 : 1;
}

void
XULTreeItemAccessibleBase::ActionNameAt(uint8_t aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click) {
    aName.AssignLiteral("activate");
    return;
  }

  if (aIndex == eAction_Expand && IsExpandable()) {
    bool isContainerOpen = false;
    mTreeView->IsContainerOpen(mRow, &isContainerOpen);
    if (isContainerOpen)
      aName.AssignLiteral("collapse");
    else
      aName.AssignLiteral("expand");
  }
}

bool
XULTreeItemAccessibleBase::DoAction(uint8_t aIndex)
{
  if (aIndex != eAction_Click &&
      (aIndex != eAction_Expand || !IsExpandable()))
    return false;

  DoCommand(nullptr, aIndex);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeItemAccessibleBase: Accessible implementation

void
XULTreeItemAccessibleBase::Shutdown()
{
  mTree = nullptr;
  mTreeView = nullptr;
  mRow = -1;

  AccessibleWrap::Shutdown();
}

GroupPos
XULTreeItemAccessibleBase::GroupPosition()
{
  GroupPos groupPos;

  int32_t level;
  nsresult rv = mTreeView->GetLevel(mRow, &level);
  NS_ENSURE_SUCCESS(rv, groupPos);

  int32_t topCount = 1;
  for (int32_t index = mRow - 1; index >= 0; index--) {
    int32_t lvl = -1;
    if (NS_SUCCEEDED(mTreeView->GetLevel(index, &lvl))) {
      if (lvl < level)
        break;

      if (lvl == level)
        topCount++;
    }
  }

  int32_t rowCount = 0;
  rv = mTreeView->GetRowCount(&rowCount);
  NS_ENSURE_SUCCESS(rv, groupPos);

  int32_t bottomCount = 0;
  for (int32_t index = mRow + 1; index < rowCount; index++) {
    int32_t lvl = -1;
    if (NS_SUCCEEDED(mTreeView->GetLevel(index, &lvl))) {
      if (lvl < level)
        break;

      if (lvl == level)
        bottomCount++;
    }
  }

  groupPos.level = level + 1;
  groupPos.setSize = topCount + bottomCount;
  groupPos.posInSet = topCount;

  return groupPos;
}

uint64_t
XULTreeItemAccessibleBase::NativeState()
{

  // focusable and selectable states
  uint64_t state = NativeInteractiveState();

  // expanded/collapsed state
  if (IsExpandable()) {
    bool isContainerOpen;
    mTreeView->IsContainerOpen(mRow, &isContainerOpen);
    state |= isContainerOpen ? states::EXPANDED : states::COLLAPSED;
  }

  // selected state
  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (selection) {
    bool isSelected;
    selection->IsSelected(mRow, &isSelected);
    if (isSelected)
      state |= states::SELECTED;
  }

  // focused state
  if (FocusMgr()->IsFocused(this))
    state |= states::FOCUSED;

  // invisible state
  int32_t firstVisibleRow, lastVisibleRow;
  mTree->GetFirstVisibleRow(&firstVisibleRow);
  mTree->GetLastVisibleRow(&lastVisibleRow);
  if (mRow < firstVisibleRow || mRow > lastVisibleRow)
    state |= states::INVISIBLE;

  return state;
}

uint64_t
XULTreeItemAccessibleBase::NativeInteractiveState() const
{
  return states::FOCUSABLE | states::SELECTABLE;
}

int32_t
XULTreeItemAccessibleBase::IndexInParent() const
{
  return mParent ? mParent->ContentChildCount() + mRow : -1;
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeItemAccessibleBase: Widgets

Accessible*
XULTreeItemAccessibleBase::ContainerWidget() const
{
  return mParent;
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeItemAccessibleBase: Accessible protected methods

void
XULTreeItemAccessibleBase::DispatchClickEvent(nsIContent* aContent,
                                              uint32_t aActionIndex)
{
  if (IsDefunct())
    return;

  nsCOMPtr<nsITreeColumns> columns;
  mTree->GetColumns(getter_AddRefs(columns));
  if (!columns)
    return;

  // Get column and pseudo element.
  nsCOMPtr<nsITreeColumn> column;
  nsAutoString pseudoElm;

  if (aActionIndex == eAction_Click) {
    // Key column is visible and clickable.
    columns->GetKeyColumn(getter_AddRefs(column));
  } else {
    // Primary column contains a twisty we should click on.
    columns->GetPrimaryColumn(getter_AddRefs(column));
    pseudoElm = NS_LITERAL_STRING("twisty");
  }

  if (column)
    nsCoreUtils::DispatchClickEvent(mTree, mRow, column, pseudoElm);
}

Accessible*
XULTreeItemAccessibleBase::GetSiblingAtOffset(int32_t aOffset,
                                              nsresult* aError) const
{
  if (aError)
    *aError = NS_OK; // fail peacefully

  return mParent->GetChildAt(IndexInParent() + aOffset);
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeItemAccessibleBase: protected implementation

bool
XULTreeItemAccessibleBase::IsExpandable()
{

  bool isContainer = false;
  mTreeView->IsContainer(mRow, &isContainer);
  if (isContainer) {
    bool isEmpty = false;
    mTreeView->IsContainerEmpty(mRow, &isEmpty);
    if (!isEmpty) {
      nsCOMPtr<nsITreeColumns> columns;
      mTree->GetColumns(getter_AddRefs(columns));
      nsCOMPtr<nsITreeColumn> primaryColumn;
      if (columns) {
        columns->GetPrimaryColumn(getter_AddRefs(primaryColumn));
        if (primaryColumn &&
            !nsCoreUtils::IsColumnHidden(primaryColumn))
          return true;
      }
    }
  }

  return false;
}

void
XULTreeItemAccessibleBase::GetCellName(nsITreeColumn* aColumn, nsAString& aName)
{

  mTreeView->GetCellText(mRow, aColumn, aName);

  // If there is still no name try the cell value:
  // This is for graphical cells. We need tree/table view implementors to
  // implement FooView::GetCellValue to return a meaningful string for cases
  // where there is something shown in the cell (non-text) such as a star icon;
  // in which case GetCellValue for that cell would return "starred" or
  // "flagged" for example.
  if (aName.IsEmpty())
    mTreeView->GetCellValue(mRow, aColumn, aName);
}


////////////////////////////////////////////////////////////////////////////////
// XULTreeItemAccessible
////////////////////////////////////////////////////////////////////////////////

XULTreeItemAccessible::
  XULTreeItemAccessible(nsIContent* aContent, DocAccessible* aDoc,
                        Accessible* aParent, nsITreeBoxObject* aTree,
                        nsITreeView* aTreeView, int32_t aRow) :
  XULTreeItemAccessibleBase(aContent, aDoc, aParent, aTree, aTreeView, aRow)
{
  mColumn = nsCoreUtils::GetFirstSensibleColumn(mTree);
  GetCellName(mColumn, mCachedName);
}

XULTreeItemAccessible::~XULTreeItemAccessible()
{
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeItemAccessible: nsISupports implementation

NS_IMPL_CYCLE_COLLECTION_INHERITED(XULTreeItemAccessible,
                                   XULTreeItemAccessibleBase,
                                   mColumn)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(XULTreeItemAccessible)
NS_INTERFACE_MAP_END_INHERITING(XULTreeItemAccessibleBase)
NS_IMPL_ADDREF_INHERITED(XULTreeItemAccessible, XULTreeItemAccessibleBase)
NS_IMPL_RELEASE_INHERITED(XULTreeItemAccessible, XULTreeItemAccessibleBase)

////////////////////////////////////////////////////////////////////////////////
// XULTreeItemAccessible: nsIAccessible implementation

ENameValueFlag
XULTreeItemAccessible::Name(nsString& aName)
{
  aName.Truncate();

  GetCellName(mColumn, aName);
  return eNameOK;
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeItemAccessible: Accessible implementation

void
XULTreeItemAccessible::Shutdown()
{
  mColumn = nullptr;
  XULTreeItemAccessibleBase::Shutdown();
}

role
XULTreeItemAccessible::NativeRole()
{
  nsCOMPtr<nsITreeColumns> columns;
  mTree->GetColumns(getter_AddRefs(columns));
  if (!columns) {
    NS_ERROR("No tree columns object in the tree!");
    return roles::NOTHING;
  }

  nsCOMPtr<nsITreeColumn> primaryColumn;
  columns->GetPrimaryColumn(getter_AddRefs(primaryColumn));

  return primaryColumn ? roles::OUTLINEITEM : roles::LISTITEM;
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeItemAccessible: XULTreeItemAccessibleBase implementation

void
XULTreeItemAccessible::RowInvalidated(int32_t aStartColIdx, int32_t aEndColIdx)
{
  nsAutoString name;
  Name(name);

  if (name != mCachedName) {
    nsEventShell::FireEvent(nsIAccessibleEvent::EVENT_NAME_CHANGE, this);
    mCachedName = name;
  }
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeItemAccessible: Accessible protected implementation

void
XULTreeItemAccessible::CacheChildren()
{
}


////////////////////////////////////////////////////////////////////////////////
//  XULTreeColumAccessible
////////////////////////////////////////////////////////////////////////////////

XULTreeColumAccessible::
  XULTreeColumAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  XULColumAccessible(aContent, aDoc)
{
}

Accessible*
XULTreeColumAccessible::GetSiblingAtOffset(int32_t aOffset,
                                           nsresult* aError) const
{
  if (aOffset < 0)
    return XULColumAccessible::GetSiblingAtOffset(aOffset, aError);

  if (aError)
    *aError =  NS_OK; // fail peacefully

  nsCOMPtr<nsITreeBoxObject> tree = nsCoreUtils::GetTreeBoxObject(mContent);
  if (tree) {
    nsCOMPtr<nsITreeView> treeView;
    tree->GetView(getter_AddRefs(treeView));
    if (treeView) {
      int32_t rowCount = 0;
      treeView->GetRowCount(&rowCount);
      if (rowCount > 0 && aOffset <= rowCount) {
        XULTreeAccessible* treeAcc = Parent()->AsXULTree();

        if (treeAcc)
          return treeAcc->GetTreeItemAccessible(aOffset - 1);
      }
    }
  }

  return nullptr;
}
