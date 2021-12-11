/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsTreeUtils.h"
#include "nsTreeContentView.h"
#include "ChildIterator.h"
#include "nsError.h"
#include "nsXULSortService.h"
#include "nsTreeBodyFrame.h"
#include "nsTreeColumns.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/TreeContentViewBinding.h"
#include "mozilla/dom/XULTreeElement.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/dom/Document.h"

using namespace mozilla;
using namespace mozilla::dom;

// A content model view implementation for the tree.

#define ROW_FLAG_CONTAINER 0x01
#define ROW_FLAG_OPEN 0x02
#define ROW_FLAG_EMPTY 0x04
#define ROW_FLAG_SEPARATOR 0x08

class Row {
 public:
  Row(Element* aContent, int32_t aParentIndex)
      : mContent(aContent),
        mParentIndex(aParentIndex),
        mSubtreeSize(0),
        mFlags(0) {}

  ~Row() = default;

  void SetContainer(bool aContainer) {
    aContainer ? mFlags |= ROW_FLAG_CONTAINER : mFlags &= ~ROW_FLAG_CONTAINER;
  }
  bool IsContainer() { return mFlags & ROW_FLAG_CONTAINER; }

  void SetOpen(bool aOpen) {
    aOpen ? mFlags |= ROW_FLAG_OPEN : mFlags &= ~ROW_FLAG_OPEN;
  }
  bool IsOpen() { return !!(mFlags & ROW_FLAG_OPEN); }

  void SetEmpty(bool aEmpty) {
    aEmpty ? mFlags |= ROW_FLAG_EMPTY : mFlags &= ~ROW_FLAG_EMPTY;
  }
  bool IsEmpty() { return !!(mFlags & ROW_FLAG_EMPTY); }

  void SetSeparator(bool aSeparator) {
    aSeparator ? mFlags |= ROW_FLAG_SEPARATOR : mFlags &= ~ROW_FLAG_SEPARATOR;
  }
  bool IsSeparator() { return !!(mFlags & ROW_FLAG_SEPARATOR); }

  // Weak reference to a content item.
  Element* mContent;

  // The parent index of the item, set to -1 for the top level items.
  int32_t mParentIndex;

  // Subtree size for this item.
  int32_t mSubtreeSize;

 private:
  // State flags
  int8_t mFlags;
};

// We don't reference count the reference to the document
// If the document goes away first, we'll be informed and we
// can drop our reference.
// If we go away first, we'll get rid of ourselves from the
// document's observer list.

nsTreeContentView::nsTreeContentView(void)
    : mTree(nullptr), mSelection(nullptr), mDocument(nullptr) {}

nsTreeContentView::~nsTreeContentView(void) {
  // Remove ourselves from mDocument's observers.
  if (mDocument) mDocument->RemoveObserver(this);
}

nsresult NS_NewTreeContentView(nsITreeView** aResult) {
  *aResult = new nsTreeContentView;
  if (!*aResult) return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsTreeContentView, mTree, mSelection,
                                      mBody)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsTreeContentView)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsTreeContentView)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsTreeContentView)
  NS_INTERFACE_MAP_ENTRY(nsITreeView)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentObserver)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsITreeView)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END

JSObject* nsTreeContentView::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return TreeContentView_Binding::Wrap(aCx, this, aGivenProto);
}

nsISupports* nsTreeContentView::GetParentObject() { return mTree; }

NS_IMETHODIMP
nsTreeContentView::GetRowCount(int32_t* aRowCount) {
  *aRowCount = mRows.Length();

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::GetSelection(nsITreeSelection** aSelection) {
  NS_IF_ADDREF(*aSelection = GetSelection());

  return NS_OK;
}

bool nsTreeContentView::CanTrustTreeSelection(nsISupports* aValue) {
  // Untrusted content is only allowed to specify known-good views
  if (nsContentUtils::LegacyIsCallerChromeOrNativeCode()) return true;
  nsCOMPtr<nsINativeTreeSelection> nativeTreeSel = do_QueryInterface(aValue);
  return nativeTreeSel && NS_SUCCEEDED(nativeTreeSel->EnsureNative());
}

NS_IMETHODIMP
nsTreeContentView::SetSelection(nsITreeSelection* aSelection) {
  ErrorResult rv;
  SetSelection(aSelection, rv);
  return rv.StealNSResult();
}

void nsTreeContentView::SetSelection(nsITreeSelection* aSelection,
                                     ErrorResult& aError) {
  if (aSelection && !CanTrustTreeSelection(aSelection)) {
    aError.ThrowSecurityError("Not allowed to set tree selection");
    return;
  }

  mSelection = aSelection;
}

void nsTreeContentView::GetRowProperties(int32_t aRow, nsAString& aProperties,
                                         ErrorResult& aError) {
  aProperties.Truncate();
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  Row* row = mRows[aRow].get();
  nsIContent* realRow;
  if (row->IsSeparator())
    realRow = row->mContent;
  else
    realRow = nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treerow);

  if (realRow && realRow->IsElement()) {
    realRow->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::properties,
                                  aProperties);
  }
}

NS_IMETHODIMP
nsTreeContentView::GetRowProperties(int32_t aIndex, nsAString& aProps) {
  ErrorResult rv;
  GetRowProperties(aIndex, aProps, rv);
  return rv.StealNSResult();
}

void nsTreeContentView::GetCellProperties(int32_t aRow, nsTreeColumn& aColumn,
                                          nsAString& aProperties,
                                          ErrorResult& aError) {
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  Row* row = mRows[aRow].get();
  nsIContent* realRow =
      nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treerow);
  if (realRow) {
    Element* cell = GetCell(realRow, aColumn);
    if (cell) {
      cell->GetAttr(kNameSpaceID_None, nsGkAtoms::properties, aProperties);
    }
  }
}

NS_IMETHODIMP
nsTreeContentView::GetCellProperties(int32_t aRow, nsTreeColumn* aCol,
                                     nsAString& aProps) {
  NS_ENSURE_ARG(aCol);

  ErrorResult rv;
  GetCellProperties(aRow, *aCol, aProps, rv);
  return rv.StealNSResult();
}

void nsTreeContentView::GetColumnProperties(nsTreeColumn& aColumn,
                                            nsAString& aProperties) {
  RefPtr<Element> element = aColumn.Element();

  if (element) {
    element->GetAttr(nsGkAtoms::properties, aProperties);
  }
}

NS_IMETHODIMP
nsTreeContentView::GetColumnProperties(nsTreeColumn* aCol, nsAString& aProps) {
  NS_ENSURE_ARG(aCol);

  GetColumnProperties(*aCol, aProps);
  return NS_OK;
}

bool nsTreeContentView::IsContainer(int32_t aRow, ErrorResult& aError) {
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return false;
  }

  return mRows[aRow]->IsContainer();
}

NS_IMETHODIMP
nsTreeContentView::IsContainer(int32_t aIndex, bool* _retval) {
  ErrorResult rv;
  *_retval = IsContainer(aIndex, rv);
  return rv.StealNSResult();
}

bool nsTreeContentView::IsContainerOpen(int32_t aRow, ErrorResult& aError) {
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return false;
  }

  return mRows[aRow]->IsOpen();
}

NS_IMETHODIMP
nsTreeContentView::IsContainerOpen(int32_t aIndex, bool* _retval) {
  ErrorResult rv;
  *_retval = IsContainerOpen(aIndex, rv);
  return rv.StealNSResult();
}

bool nsTreeContentView::IsContainerEmpty(int32_t aRow, ErrorResult& aError) {
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return false;
  }

  return mRows[aRow]->IsEmpty();
}

NS_IMETHODIMP
nsTreeContentView::IsContainerEmpty(int32_t aIndex, bool* _retval) {
  ErrorResult rv;
  *_retval = IsContainerEmpty(aIndex, rv);
  return rv.StealNSResult();
}

bool nsTreeContentView::IsSeparator(int32_t aRow, ErrorResult& aError) {
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return false;
  }

  return mRows[aRow]->IsSeparator();
}

NS_IMETHODIMP
nsTreeContentView::IsSeparator(int32_t aIndex, bool* _retval) {
  ErrorResult rv;
  *_retval = IsSeparator(aIndex, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
nsTreeContentView::IsSorted(bool* _retval) {
  *_retval = IsSorted();

  return NS_OK;
}

bool nsTreeContentView::CanDrop(int32_t aRow, int32_t aOrientation,
                                ErrorResult& aError) {
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
  }
  return false;
}

bool nsTreeContentView::CanDrop(int32_t aRow, int32_t aOrientation,
                                DataTransfer* aDataTransfer,
                                ErrorResult& aError) {
  return CanDrop(aRow, aOrientation, aError);
}

NS_IMETHODIMP
nsTreeContentView::CanDrop(int32_t aIndex, int32_t aOrientation,
                           DataTransfer* aDataTransfer, bool* _retval) {
  ErrorResult rv;
  *_retval = CanDrop(aIndex, aOrientation, rv);
  return rv.StealNSResult();
}

void nsTreeContentView::Drop(int32_t aRow, int32_t aOrientation,
                             ErrorResult& aError) {
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
  }
}

void nsTreeContentView::Drop(int32_t aRow, int32_t aOrientation,
                             DataTransfer* aDataTransfer, ErrorResult& aError) {
  Drop(aRow, aOrientation, aError);
}

NS_IMETHODIMP
nsTreeContentView::Drop(int32_t aRow, int32_t aOrientation,
                        DataTransfer* aDataTransfer) {
  ErrorResult rv;
  Drop(aRow, aOrientation, rv);
  return rv.StealNSResult();
}

int32_t nsTreeContentView::GetParentIndex(int32_t aRow, ErrorResult& aError) {
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return 0;
  }

  return mRows[aRow]->mParentIndex;
}

NS_IMETHODIMP
nsTreeContentView::GetParentIndex(int32_t aRowIndex, int32_t* _retval) {
  ErrorResult rv;
  *_retval = GetParentIndex(aRowIndex, rv);
  return rv.StealNSResult();
}

bool nsTreeContentView::HasNextSibling(int32_t aRow, int32_t aAfterIndex,
                                       ErrorResult& aError) {
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return false;
  }

  // We have a next sibling if the row is not the last in the subtree.
  int32_t parentIndex = mRows[aRow]->mParentIndex;
  if (parentIndex < 0) {
    return uint32_t(aRow) < mRows.Length() - 1;
  }

  // Compute the last index in this subtree.
  int32_t lastIndex = parentIndex + (mRows[parentIndex])->mSubtreeSize;
  Row* row = mRows[lastIndex].get();
  while (row->mParentIndex != parentIndex) {
    lastIndex = row->mParentIndex;
    row = mRows[lastIndex].get();
  }

  return aRow < lastIndex;
}

NS_IMETHODIMP
nsTreeContentView::HasNextSibling(int32_t aRowIndex, int32_t aAfterIndex,
                                  bool* _retval) {
  ErrorResult rv;
  *_retval = HasNextSibling(aRowIndex, aAfterIndex, rv);
  return rv.StealNSResult();
}

int32_t nsTreeContentView::GetLevel(int32_t aRow, ErrorResult& aError) {
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return 0;
  }

  int32_t level = 0;
  Row* row = mRows[aRow].get();
  while (row->mParentIndex >= 0) {
    level++;
    row = mRows[row->mParentIndex].get();
  }
  return level;
}

NS_IMETHODIMP
nsTreeContentView::GetLevel(int32_t aIndex, int32_t* _retval) {
  ErrorResult rv;
  *_retval = GetLevel(aIndex, rv);
  return rv.StealNSResult();
}

void nsTreeContentView::GetImageSrc(int32_t aRow, nsTreeColumn& aColumn,
                                    nsAString& aSrc, ErrorResult& aError) {
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  Row* row = mRows[aRow].get();

  nsIContent* realRow =
      nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treerow);
  if (realRow) {
    Element* cell = GetCell(realRow, aColumn);
    if (cell) cell->GetAttr(kNameSpaceID_None, nsGkAtoms::src, aSrc);
  }
}

NS_IMETHODIMP
nsTreeContentView::GetImageSrc(int32_t aRow, nsTreeColumn* aCol,
                               nsAString& _retval) {
  NS_ENSURE_ARG(aCol);

  ErrorResult rv;
  GetImageSrc(aRow, *aCol, _retval, rv);
  return rv.StealNSResult();
}

void nsTreeContentView::GetCellValue(int32_t aRow, nsTreeColumn& aColumn,
                                     nsAString& aValue, ErrorResult& aError) {
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  Row* row = mRows[aRow].get();

  nsIContent* realRow =
      nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treerow);
  if (realRow) {
    Element* cell = GetCell(realRow, aColumn);
    if (cell) cell->GetAttr(kNameSpaceID_None, nsGkAtoms::value, aValue);
  }
}

NS_IMETHODIMP
nsTreeContentView::GetCellValue(int32_t aRow, nsTreeColumn* aCol,
                                nsAString& _retval) {
  NS_ENSURE_ARG(aCol);

  ErrorResult rv;
  GetCellValue(aRow, *aCol, _retval, rv);
  return rv.StealNSResult();
}

void nsTreeContentView::GetCellText(int32_t aRow, nsTreeColumn& aColumn,
                                    nsAString& aText, ErrorResult& aError) {
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  Row* row = mRows[aRow].get();

  // Check for a "label" attribute - this is valid on an <treeitem>
  // with a single implied column.
  if (row->mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::label, aText) &&
      !aText.IsEmpty()) {
    return;
  }

  if (row->mContent->IsXULElement(nsGkAtoms::treeitem)) {
    nsIContent* realRow =
        nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treerow);
    if (realRow) {
      Element* cell = GetCell(realRow, aColumn);
      if (cell) cell->GetAttr(kNameSpaceID_None, nsGkAtoms::label, aText);
    }
  }
}

NS_IMETHODIMP
nsTreeContentView::GetCellText(int32_t aRow, nsTreeColumn* aCol,
                               nsAString& _retval) {
  NS_ENSURE_ARG(aCol);

  ErrorResult rv;
  GetCellText(aRow, *aCol, _retval, rv);
  return rv.StealNSResult();
}

void nsTreeContentView::SetTree(XULTreeElement* aTree, ErrorResult& aError) {
  aError = SetTree(aTree);
}

NS_IMETHODIMP
nsTreeContentView::SetTree(XULTreeElement* aTree) {
  ClearRows();

  mTree = aTree;

  if (aTree) {
    // Add ourselves to document's observers.
    Document* document = mTree->GetComposedDoc();
    if (document) {
      document->AddObserver(this);
      mDocument = document;
    }

    RefPtr<dom::Element> bodyElement = mTree->GetTreeBody();
    if (bodyElement) {
      mBody = std::move(bodyElement);
      int32_t index = 0;
      Serialize(mBody, -1, &index, mRows);
    }
  }

  return NS_OK;
}

void nsTreeContentView::ToggleOpenState(int32_t aRow, ErrorResult& aError) {
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  // We don't serialize content right here, since content might be generated
  // lazily.
  Row* row = mRows[aRow].get();

  if (row->IsOpen())
    row->mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::open, u"false"_ns,
                           true);
  else
    row->mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::open, u"true"_ns,
                           true);
}

NS_IMETHODIMP
nsTreeContentView::ToggleOpenState(int32_t aIndex) {
  ErrorResult rv;
  ToggleOpenState(aIndex, rv);
  return rv.StealNSResult();
}

void nsTreeContentView::CycleHeader(nsTreeColumn& aColumn,
                                    ErrorResult& aError) {
  if (!mTree) return;

  RefPtr<Element> column = aColumn.Element();
  nsAutoString sort;
  column->GetAttr(kNameSpaceID_None, nsGkAtoms::sort, sort);
  if (!sort.IsEmpty()) {
    nsAutoString sortdirection;
    static Element::AttrValuesArray strings[] = {
        nsGkAtoms::ascending, nsGkAtoms::descending, nullptr};
    switch (column->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::sortDirection,
                                    strings, eCaseMatters)) {
      case 0:
        sortdirection.AssignLiteral("descending");
        break;
      case 1:
        sortdirection.AssignLiteral("natural");
        break;
      default:
        sortdirection.AssignLiteral("ascending");
        break;
    }

    nsAutoString hints;
    column->GetAttr(kNameSpaceID_None, nsGkAtoms::sorthints, hints);
    sortdirection.Append(' ');
    sortdirection += hints;

    XULWidgetSort(mTree, sort, sortdirection);
  }
}

NS_IMETHODIMP
nsTreeContentView::CycleHeader(nsTreeColumn* aCol) {
  NS_ENSURE_ARG(aCol);

  ErrorResult rv;
  CycleHeader(*aCol, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
nsTreeContentView::SelectionChangedXPCOM() { return NS_OK; }

NS_IMETHODIMP
nsTreeContentView::CycleCell(int32_t aRow, nsTreeColumn* aCol) { return NS_OK; }

bool nsTreeContentView::IsEditable(int32_t aRow, nsTreeColumn& aColumn,
                                   ErrorResult& aError) {
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return false;
  }

  Row* row = mRows[aRow].get();

  nsIContent* realRow =
      nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treerow);
  if (realRow) {
    Element* cell = GetCell(realRow, aColumn);
    if (cell && cell->AttrValueIs(kNameSpaceID_None, nsGkAtoms::editable,
                                  nsGkAtoms::_false, eCaseMatters)) {
      return false;
    }
  }

  return true;
}

NS_IMETHODIMP
nsTreeContentView::IsEditable(int32_t aRow, nsTreeColumn* aCol, bool* _retval) {
  NS_ENSURE_ARG(aCol);

  ErrorResult rv;
  *_retval = IsEditable(aRow, *aCol, rv);
  return rv.StealNSResult();
}

void nsTreeContentView::SetCellValue(int32_t aRow, nsTreeColumn& aColumn,
                                     const nsAString& aValue,
                                     ErrorResult& aError) {
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  Row* row = mRows[aRow].get();

  nsIContent* realRow =
      nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treerow);
  if (realRow) {
    Element* cell = GetCell(realRow, aColumn);
    if (cell) cell->SetAttr(kNameSpaceID_None, nsGkAtoms::value, aValue, true);
  }
}

NS_IMETHODIMP
nsTreeContentView::SetCellValue(int32_t aRow, nsTreeColumn* aCol,
                                const nsAString& aValue) {
  NS_ENSURE_ARG(aCol);

  ErrorResult rv;
  SetCellValue(aRow, *aCol, aValue, rv);
  return rv.StealNSResult();
}

void nsTreeContentView::SetCellText(int32_t aRow, nsTreeColumn& aColumn,
                                    const nsAString& aValue,
                                    ErrorResult& aError) {
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  Row* row = mRows[aRow].get();

  nsIContent* realRow =
      nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treerow);
  if (realRow) {
    Element* cell = GetCell(realRow, aColumn);
    if (cell) cell->SetAttr(kNameSpaceID_None, nsGkAtoms::label, aValue, true);
  }
}

NS_IMETHODIMP
nsTreeContentView::SetCellText(int32_t aRow, nsTreeColumn* aCol,
                               const nsAString& aValue) {
  NS_ENSURE_ARG(aCol);

  ErrorResult rv;
  SetCellText(aRow, *aCol, aValue, rv);
  return rv.StealNSResult();
}

Element* nsTreeContentView::GetItemAtIndex(int32_t aIndex,
                                           ErrorResult& aError) {
  if (!IsValidRowIndex(aIndex)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return nullptr;
  }

  return mRows[aIndex]->mContent;
}

int32_t nsTreeContentView::GetIndexOfItem(Element* aItem) {
  return FindContent(aItem);
}

void nsTreeContentView::AttributeChanged(dom::Element* aElement,
                                         int32_t aNameSpaceID,
                                         nsAtom* aAttribute, int32_t aModType,
                                         const nsAttrValue* aOldValue) {
  // Lots of codepaths under here that do all sorts of stuff, so be safe.
  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);

  // Make sure this notification concerns us.
  // First check the tag to see if it's one that we care about.
  if (aElement == mTree || aElement == mBody) {
    mTree->ClearStyleAndImageCaches();
    mTree->Invalidate();
  }

  // We don't consider non-XUL nodes.
  nsIContent* parent = nullptr;
  if (!aElement->IsXULElement() ||
      ((parent = aElement->GetParent()) && !parent->IsXULElement())) {
    return;
  }
  if (!aElement->IsAnyOfXULElements(nsGkAtoms::treecol, nsGkAtoms::treeitem,
                                    nsGkAtoms::treeseparator,
                                    nsGkAtoms::treerow, nsGkAtoms::treecell)) {
    return;
  }

  // If we have a legal tag, go up to the tree/select and make sure
  // that it's ours.

  for (nsIContent* element = aElement; element != mBody;
       element = element->GetParent()) {
    if (!element) return;                                // this is not for us
    if (element->IsXULElement(nsGkAtoms::tree)) return;  // this is not for us
  }

  // Handle changes of the hidden attribute.
  if (aAttribute == nsGkAtoms::hidden &&
      aElement->IsAnyOfXULElements(nsGkAtoms::treeitem,
                                   nsGkAtoms::treeseparator)) {
    bool hidden = aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::hidden,
                                        nsGkAtoms::_true, eCaseMatters);

    int32_t index = FindContent(aElement);
    if (hidden && index >= 0) {
      // Hide this row along with its children.
      int32_t count = RemoveRow(index);
      if (mTree) mTree->RowCountChanged(index, -count);
    } else if (!hidden && index < 0) {
      // Show this row along with its children.
      nsCOMPtr<nsIContent> parent = aElement->GetParent();
      if (parent) {
        InsertRowFor(parent, aElement);
      }
    }

    return;
  }

  if (aElement->IsXULElement(nsGkAtoms::treecol)) {
    if (aAttribute == nsGkAtoms::properties) {
      if (mTree) {
        RefPtr<nsTreeColumns> cols = mTree->GetColumns();
        if (cols) {
          RefPtr<nsTreeColumn> col = cols->GetColumnFor(aElement);
          mTree->InvalidateColumn(col);
        }
      }
    }
  } else if (aElement->IsXULElement(nsGkAtoms::treeitem)) {
    int32_t index = FindContent(aElement);
    if (index >= 0) {
      Row* row = mRows[index].get();
      if (aAttribute == nsGkAtoms::container) {
        bool isContainer =
            aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::container,
                                  nsGkAtoms::_true, eCaseMatters);
        row->SetContainer(isContainer);
        if (mTree) mTree->InvalidateRow(index);
      } else if (aAttribute == nsGkAtoms::open) {
        bool isOpen = aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::open,
                                            nsGkAtoms::_true, eCaseMatters);
        bool wasOpen = row->IsOpen();
        if (!isOpen && wasOpen)
          CloseContainer(index);
        else if (isOpen && !wasOpen)
          OpenContainer(index);
      } else if (aAttribute == nsGkAtoms::empty) {
        bool isEmpty =
            aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::empty,
                                  nsGkAtoms::_true, eCaseMatters);
        row->SetEmpty(isEmpty);
        if (mTree) mTree->InvalidateRow(index);
      }
    }
  } else if (aElement->IsXULElement(nsGkAtoms::treeseparator)) {
    int32_t index = FindContent(aElement);
    if (index >= 0) {
      if (aAttribute == nsGkAtoms::properties && mTree) {
        mTree->InvalidateRow(index);
      }
    }
  } else if (aElement->IsXULElement(nsGkAtoms::treerow)) {
    if (aAttribute == nsGkAtoms::properties) {
      nsCOMPtr<nsIContent> parent = aElement->GetParent();
      if (parent) {
        int32_t index = FindContent(parent);
        if (index >= 0 && mTree) {
          mTree->InvalidateRow(index);
        }
      }
    }
  } else if (aElement->IsXULElement(nsGkAtoms::treecell)) {
    if (aAttribute == nsGkAtoms::properties || aAttribute == nsGkAtoms::mode ||
        aAttribute == nsGkAtoms::src || aAttribute == nsGkAtoms::value ||
        aAttribute == nsGkAtoms::label) {
      nsIContent* parent = aElement->GetParent();
      if (parent) {
        nsCOMPtr<nsIContent> grandParent = parent->GetParent();
        if (grandParent && grandParent->IsXULElement()) {
          int32_t index = FindContent(grandParent);
          if (index >= 0 && mTree) {
            // XXX Should we make an effort to invalidate only cell ?
            mTree->InvalidateRow(index);
          }
        }
      }
    }
  }
}

void nsTreeContentView::ContentAppended(nsIContent* aFirstNewContent) {
  for (nsIContent* cur = aFirstNewContent; cur; cur = cur->GetNextSibling()) {
    // Our contentinserted doesn't use the index
    ContentInserted(cur);
  }
}

void nsTreeContentView::ContentInserted(nsIContent* aChild) {
  NS_ASSERTION(aChild, "null ptr");
  nsIContent* container = aChild->GetParent();

  // Make sure this notification concerns us.
  // First check the tag to see if it's one that we care about.

  // Don't allow non-XUL nodes.
  if (!aChild->IsXULElement() || !container->IsXULElement()) return;

  if (!aChild->IsAnyOfXULElements(nsGkAtoms::treeitem, nsGkAtoms::treeseparator,
                                  nsGkAtoms::treechildren, nsGkAtoms::treerow,
                                  nsGkAtoms::treecell)) {
    return;
  }

  // If we have a legal tag, go up to the tree/select and make sure
  // that it's ours.

  for (nsIContent* element = container; element != mBody;
       element = element->GetParent()) {
    if (!element) return;                                // this is not for us
    if (element->IsXULElement(nsGkAtoms::tree)) return;  // this is not for us
  }

  // Lots of codepaths under here that do all sorts of stuff, so be safe.
  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);

  if (aChild->IsXULElement(nsGkAtoms::treechildren)) {
    int32_t index = FindContent(container);
    if (index >= 0) {
      Row* row = mRows[index].get();
      row->SetEmpty(false);
      if (mTree) mTree->InvalidateRow(index);
      if (row->IsContainer() && row->IsOpen()) {
        int32_t count = EnsureSubtree(index);
        if (mTree) mTree->RowCountChanged(index + 1, count);
      }
    }
  } else if (aChild->IsAnyOfXULElements(nsGkAtoms::treeitem,
                                        nsGkAtoms::treeseparator)) {
    InsertRowFor(container, aChild);
  } else if (aChild->IsXULElement(nsGkAtoms::treerow)) {
    int32_t index = FindContent(container);
    if (index >= 0 && mTree) mTree->InvalidateRow(index);
  } else if (aChild->IsXULElement(nsGkAtoms::treecell)) {
    nsCOMPtr<nsIContent> parent = container->GetParent();
    if (parent) {
      int32_t index = FindContent(parent);
      if (index >= 0 && mTree) mTree->InvalidateRow(index);
    }
  }
}

void nsTreeContentView::ContentRemoved(nsIContent* aChild,
                                       nsIContent* aPreviousSibling) {
  NS_ASSERTION(aChild, "null ptr");

  nsIContent* container = aChild->GetParent();
  // Make sure this notification concerns us.
  // First check the tag to see if it's one that we care about.

  // We don't consider non-XUL nodes.
  if (!aChild->IsXULElement() || !container->IsXULElement()) return;

  if (!aChild->IsAnyOfXULElements(nsGkAtoms::treeitem, nsGkAtoms::treeseparator,
                                  nsGkAtoms::treechildren, nsGkAtoms::treerow,
                                  nsGkAtoms::treecell)) {
    return;
  }

  // If we have a legal tag, go up to the tree/select and make sure
  // that it's ours.

  for (nsIContent* element = container; element != mBody;
       element = element->GetParent()) {
    if (!element) return;                                // this is not for us
    if (element->IsXULElement(nsGkAtoms::tree)) return;  // this is not for us
  }

  // Lots of codepaths under here that do all sorts of stuff, so be safe.
  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);

  if (aChild->IsXULElement(nsGkAtoms::treechildren)) {
    int32_t index = FindContent(container);
    if (index >= 0) {
      Row* row = mRows[index].get();
      row->SetEmpty(true);
      int32_t count = RemoveSubtree(index);
      // Invalidate also the row to update twisty.
      if (mTree) {
        mTree->InvalidateRow(index);
        mTree->RowCountChanged(index + 1, -count);
      }
    }
  } else if (aChild->IsAnyOfXULElements(nsGkAtoms::treeitem,
                                        nsGkAtoms::treeseparator)) {
    int32_t index = FindContent(aChild);
    if (index >= 0) {
      int32_t count = RemoveRow(index);
      if (mTree) mTree->RowCountChanged(index, -count);
    }
  } else if (aChild->IsXULElement(nsGkAtoms::treerow)) {
    int32_t index = FindContent(container);
    if (index >= 0 && mTree) mTree->InvalidateRow(index);
  } else if (aChild->IsXULElement(nsGkAtoms::treecell)) {
    nsCOMPtr<nsIContent> parent = container->GetParent();
    if (parent) {
      int32_t index = FindContent(parent);
      if (index >= 0 && mTree) mTree->InvalidateRow(index);
    }
  }
}

void nsTreeContentView::NodeWillBeDestroyed(const nsINode* aNode) {
  // XXXbz do we need this strong ref?  Do we drop refs to self in ClearRows?
  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
  ClearRows();
}

// Recursively serialize content, starting with aContent.
void nsTreeContentView::Serialize(nsIContent* aContent, int32_t aParentIndex,
                                  int32_t* aIndex,
                                  nsTArray<UniquePtr<Row>>& aRows) {
  // Don't allow non-XUL nodes.
  if (!aContent->IsXULElement()) return;

  dom::FlattenedChildIterator iter(aContent);
  for (nsIContent* content = iter.GetNextChild(); content;
       content = iter.GetNextChild()) {
    int32_t count = aRows.Length();

    if (content->IsXULElement(nsGkAtoms::treeitem)) {
      SerializeItem(content->AsElement(), aParentIndex, aIndex, aRows);
    } else if (content->IsXULElement(nsGkAtoms::treeseparator)) {
      SerializeSeparator(content->AsElement(), aParentIndex, aIndex, aRows);
    }

    *aIndex += aRows.Length() - count;
  }
}

void nsTreeContentView::SerializeItem(Element* aContent, int32_t aParentIndex,
                                      int32_t* aIndex,
                                      nsTArray<UniquePtr<Row>>& aRows) {
  if (aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::hidden,
                            nsGkAtoms::_true, eCaseMatters))
    return;

  aRows.AppendElement(MakeUnique<Row>(aContent, aParentIndex));
  Row* row = aRows.LastElement().get();

  if (aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::container,
                            nsGkAtoms::_true, eCaseMatters)) {
    row->SetContainer(true);
    if (aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::open,
                              nsGkAtoms::_true, eCaseMatters)) {
      row->SetOpen(true);
      nsIContent* child =
          nsTreeUtils::GetImmediateChild(aContent, nsGkAtoms::treechildren);
      if (child && child->IsXULElement()) {
        // Now, recursively serialize our child.
        int32_t count = aRows.Length();
        int32_t index = 0;
        Serialize(child, aParentIndex + *aIndex + 1, &index, aRows);
        row->mSubtreeSize += aRows.Length() - count;
      } else
        row->SetEmpty(true);
    } else if (aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::empty,
                                     nsGkAtoms::_true, eCaseMatters)) {
      row->SetEmpty(true);
    }
  }
}

void nsTreeContentView::SerializeSeparator(Element* aContent,
                                           int32_t aParentIndex,
                                           int32_t* aIndex,
                                           nsTArray<UniquePtr<Row>>& aRows) {
  if (aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::hidden,
                            nsGkAtoms::_true, eCaseMatters))
    return;

  auto row = MakeUnique<Row>(aContent, aParentIndex);
  row->SetSeparator(true);
  aRows.AppendElement(std::move(row));
}

void nsTreeContentView::GetIndexInSubtree(nsIContent* aContainer,
                                          nsIContent* aContent,
                                          int32_t* aIndex) {
  if (!aContainer->IsXULElement()) return;

  for (nsIContent* content = aContainer->GetFirstChild(); content;
       content = content->GetNextSibling()) {
    if (content == aContent) break;

    if (content->IsXULElement(nsGkAtoms::treeitem)) {
      if (!content->AsElement()->AttrValueIs(kNameSpaceID_None,
                                             nsGkAtoms::hidden,
                                             nsGkAtoms::_true, eCaseMatters)) {
        (*aIndex)++;
        if (content->AsElement()->AttrValueIs(kNameSpaceID_None,
                                              nsGkAtoms::container,
                                              nsGkAtoms::_true, eCaseMatters) &&
            content->AsElement()->AttrValueIs(kNameSpaceID_None,
                                              nsGkAtoms::open, nsGkAtoms::_true,
                                              eCaseMatters)) {
          nsIContent* child =
              nsTreeUtils::GetImmediateChild(content, nsGkAtoms::treechildren);
          if (child && child->IsXULElement())
            GetIndexInSubtree(child, aContent, aIndex);
        }
      }
    } else if (content->IsXULElement(nsGkAtoms::treeseparator)) {
      if (!content->AsElement()->AttrValueIs(kNameSpaceID_None,
                                             nsGkAtoms::hidden,
                                             nsGkAtoms::_true, eCaseMatters))
        (*aIndex)++;
    }
  }
}

int32_t nsTreeContentView::EnsureSubtree(int32_t aIndex) {
  Row* row = mRows[aIndex].get();

  nsIContent* child;
  child =
      nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treechildren);
  if (!child || !child->IsXULElement()) {
    return 0;
  }

  AutoTArray<UniquePtr<Row>, 8> rows;
  int32_t index = 0;
  Serialize(child, aIndex, &index, rows);
  // Insert |rows| into |mRows| at position |aIndex|, by first creating empty
  // UniquePtr entries and then Move'ing |rows|'s entries into them. (Note
  // that we can't simply use InsertElementsAt with an array argument, since
  // the destination can't steal ownership from its const source argument.)
  UniquePtr<Row>* newRows = mRows.InsertElementsAt(aIndex + 1, rows.Length());
  for (nsTArray<Row>::index_type i = 0; i < rows.Length(); i++) {
    newRows[i] = std::move(rows[i]);
  }
  int32_t count = rows.Length();

  row->mSubtreeSize += count;
  UpdateSubtreeSizes(row->mParentIndex, count);

  // Update parent indexes, but skip newly added rows.
  // They already have correct values.
  UpdateParentIndexes(aIndex, count + 1, count);

  return count;
}

int32_t nsTreeContentView::RemoveSubtree(int32_t aIndex) {
  Row* row = mRows[aIndex].get();
  int32_t count = row->mSubtreeSize;

  mRows.RemoveElementsAt(aIndex + 1, count);

  row->mSubtreeSize -= count;
  UpdateSubtreeSizes(row->mParentIndex, -count);

  UpdateParentIndexes(aIndex, 0, -count);

  return count;
}

void nsTreeContentView::InsertRowFor(nsIContent* aParent, nsIContent* aChild) {
  int32_t grandParentIndex = -1;
  bool insertRow = false;

  nsCOMPtr<nsIContent> grandParent = aParent->GetParent();

  if (grandParent->IsXULElement(nsGkAtoms::tree)) {
    // Allow insertion to the outermost container.
    insertRow = true;
  } else {
    // Test insertion to an inner container.

    // First try to find this parent in our array of rows, if we find one
    // we can be sure that all other parents are open too.
    grandParentIndex = FindContent(grandParent);
    if (grandParentIndex >= 0) {
      // Got it, now test if it is open.
      if (mRows[grandParentIndex]->IsOpen()) insertRow = true;
    }
  }

  if (insertRow) {
    int32_t index = 0;
    GetIndexInSubtree(aParent, aChild, &index);

    int32_t count = InsertRow(grandParentIndex, index, aChild);
    if (mTree) mTree->RowCountChanged(grandParentIndex + index + 1, count);
  }
}

int32_t nsTreeContentView::InsertRow(int32_t aParentIndex, int32_t aIndex,
                                     nsIContent* aContent) {
  AutoTArray<UniquePtr<Row>, 8> rows;
  if (aContent->IsXULElement(nsGkAtoms::treeitem)) {
    SerializeItem(aContent->AsElement(), aParentIndex, &aIndex, rows);
  } else if (aContent->IsXULElement(nsGkAtoms::treeseparator)) {
    SerializeSeparator(aContent->AsElement(), aParentIndex, &aIndex, rows);
  }

  // We can't use InsertElementsAt since the destination can't steal
  // ownership from its const source argument.
  int32_t count = rows.Length();
  for (nsTArray<Row>::index_type i = 0; i < size_t(count); i++) {
    mRows.InsertElementAt(aParentIndex + aIndex + i + 1, std::move(rows[i]));
  }

  UpdateSubtreeSizes(aParentIndex, count);

  // Update parent indexes, but skip added rows.
  // They already have correct values.
  UpdateParentIndexes(aParentIndex + aIndex, count + 1, count);

  return count;
}

int32_t nsTreeContentView::RemoveRow(int32_t aIndex) {
  Row* row = mRows[aIndex].get();
  int32_t count = row->mSubtreeSize + 1;
  int32_t parentIndex = row->mParentIndex;

  mRows.RemoveElementsAt(aIndex, count);

  UpdateSubtreeSizes(parentIndex, -count);

  UpdateParentIndexes(aIndex, 0, -count);

  return count;
}

void nsTreeContentView::ClearRows() {
  mRows.Clear();
  mBody = nullptr;
  // Remove ourselves from mDocument's observers.
  if (mDocument) {
    mDocument->RemoveObserver(this);
    mDocument = nullptr;
  }
}

void nsTreeContentView::OpenContainer(int32_t aIndex) {
  Row* row = mRows[aIndex].get();
  row->SetOpen(true);

  int32_t count = EnsureSubtree(aIndex);
  if (mTree) {
    mTree->InvalidateRow(aIndex);
    mTree->RowCountChanged(aIndex + 1, count);
  }
}

void nsTreeContentView::CloseContainer(int32_t aIndex) {
  Row* row = mRows[aIndex].get();
  row->SetOpen(false);

  int32_t count = RemoveSubtree(aIndex);
  if (mTree) {
    mTree->InvalidateRow(aIndex);
    mTree->RowCountChanged(aIndex + 1, -count);
  }
}

int32_t nsTreeContentView::FindContent(nsIContent* aContent) {
  for (uint32_t i = 0; i < mRows.Length(); i++) {
    if (mRows[i]->mContent == aContent) {
      return i;
    }
  }

  return -1;
}

void nsTreeContentView::UpdateSubtreeSizes(int32_t aParentIndex,
                                           int32_t count) {
  while (aParentIndex >= 0) {
    Row* row = mRows[aParentIndex].get();
    row->mSubtreeSize += count;
    aParentIndex = row->mParentIndex;
  }
}

void nsTreeContentView::UpdateParentIndexes(int32_t aIndex, int32_t aSkip,
                                            int32_t aCount) {
  int32_t count = mRows.Length();
  for (int32_t i = aIndex + aSkip; i < count; i++) {
    Row* row = mRows[i].get();
    if (row->mParentIndex > aIndex) {
      row->mParentIndex += aCount;
    }
  }
}

Element* nsTreeContentView::GetCell(nsIContent* aContainer,
                                    nsTreeColumn& aCol) {
  int32_t colIndex(aCol.GetIndex());

  // Traverse through cells, try to find the cell by index in a row.
  Element* result = nullptr;
  int32_t j = 0;
  dom::FlattenedChildIterator iter(aContainer);
  for (nsIContent* cell = iter.GetNextChild(); cell;
       cell = iter.GetNextChild()) {
    if (cell->IsXULElement(nsGkAtoms::treecell)) {
      if (j == colIndex) {
        result = cell->AsElement();
        break;
      }
      j++;
    }
  }

  return result;
}

bool nsTreeContentView::IsValidRowIndex(int32_t aRowIndex) {
  return aRowIndex >= 0 && aRowIndex < int32_t(mRows.Length());
}
