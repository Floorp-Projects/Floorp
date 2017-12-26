/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsIBoxObject.h"
#include "nsTreeUtils.h"
#include "nsTreeContentView.h"
#include "ChildIterator.h"
#include "nsError.h"
#include "nsIXULSortService.h"
#include "nsTreeBodyFrame.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/TreeContentViewBinding.h"
#include "nsServiceManagerUtils.h"
#include "nsIDocument.h"

using namespace mozilla;

// A content model view implementation for the tree.

#define ROW_FLAG_CONTAINER      0x01
#define ROW_FLAG_OPEN           0x02
#define ROW_FLAG_EMPTY          0x04
#define ROW_FLAG_SEPARATOR      0x08

class Row
{
  public:
    Row(Element* aContent, int32_t aParentIndex)
      : mContent(aContent), mParentIndex(aParentIndex),
        mSubtreeSize(0), mFlags(0) {
    }

    ~Row() {
    }

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
    Element*            mContent;

    // The parent index of the item, set to -1 for the top level items.
    int32_t             mParentIndex;

    // Subtree size for this item.
    int32_t             mSubtreeSize;

  private:
    // State flags
    int8_t		mFlags;
};


// We don't reference count the reference to the document
// If the document goes away first, we'll be informed and we
// can drop our reference.
// If we go away first, we'll get rid of ourselves from the
// document's observer list.

nsTreeContentView::nsTreeContentView(void) :
  mBoxObject(nullptr),
  mSelection(nullptr),
  mRoot(nullptr),
  mDocument(nullptr)
{
}

nsTreeContentView::~nsTreeContentView(void)
{
  // Remove ourselves from mDocument's observers.
  if (mDocument)
    mDocument->RemoveObserver(this);
}

nsresult
NS_NewTreeContentView(nsITreeView** aResult)
{
  *aResult = new nsTreeContentView;
  if (! *aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsTreeContentView,
                                      mBoxObject,
                                      mSelection,
                                      mRoot,
                                      mBody)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsTreeContentView)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsTreeContentView)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsTreeContentView)
  NS_INTERFACE_MAP_ENTRY(nsITreeView)
  NS_INTERFACE_MAP_ENTRY(nsITreeContentView)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentObserver)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsITreeContentView)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END

JSObject*
nsTreeContentView::WrapObject(JSContext* aCx,
                              JS::Handle<JSObject*> aGivenProto)
{
  return TreeContentViewBinding::Wrap(aCx, this, aGivenProto);
}

nsISupports*
nsTreeContentView::GetParentObject()
{
  return mBoxObject;
}

NS_IMETHODIMP
nsTreeContentView::GetRowCount(int32_t* aRowCount)
{
  *aRowCount = mRows.Length();

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::GetSelection(nsITreeSelection** aSelection)
{
  NS_IF_ADDREF(*aSelection = GetSelection());

  return NS_OK;
}

bool
nsTreeContentView::CanTrustTreeSelection(nsISupports* aValue)
{
  // Untrusted content is only allowed to specify known-good views
  if (nsContentUtils::LegacyIsCallerChromeOrNativeCode())
    return true;
  nsCOMPtr<nsINativeTreeSelection> nativeTreeSel = do_QueryInterface(aValue);
  return nativeTreeSel && NS_SUCCEEDED(nativeTreeSel->EnsureNative());
}

NS_IMETHODIMP
nsTreeContentView::SetSelection(nsITreeSelection* aSelection)
{
  ErrorResult rv;
  SetSelection(aSelection, rv);
  return rv.StealNSResult();
}

void
nsTreeContentView::SetSelection(nsITreeSelection* aSelection, ErrorResult& aError)
{
  if (aSelection && !CanTrustTreeSelection(aSelection)) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  mSelection = aSelection;
}

void
nsTreeContentView::GetRowProperties(int32_t aRow, nsAString& aProperties,
                                    ErrorResult& aError)
{
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
    realRow->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::properties, aProperties);
  }
}

NS_IMETHODIMP
nsTreeContentView::GetRowProperties(int32_t aIndex, nsAString& aProps)
{
  ErrorResult rv;
  GetRowProperties(aIndex, aProps, rv);
  return rv.StealNSResult();
}

void
nsTreeContentView::GetCellProperties(int32_t aRow, nsTreeColumn& aColumn,
                                     nsAString& aProperties,
                                     ErrorResult& aError)
{
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
nsTreeContentView::GetCellProperties(int32_t aRow, nsITreeColumn* aCol,
                                     nsAString& aProps)
{
  RefPtr<nsTreeColumn> col = nsTreeColumn::From(aCol);
  NS_ENSURE_ARG(col);

  ErrorResult rv;
  GetCellProperties(aRow, *col, aProps, rv);
  return rv.StealNSResult();
}

void
nsTreeContentView::GetColumnProperties(nsTreeColumn& aColumn,
                                       nsAString& aProperties)
{
  nsCOMPtr<nsIDOMElement> element;
  aColumn.GetElement(getter_AddRefs(element));

  element->GetAttribute(NS_LITERAL_STRING("properties"), aProperties);
}

NS_IMETHODIMP
nsTreeContentView::GetColumnProperties(nsITreeColumn* aCol, nsAString& aProps)
{
  RefPtr<nsTreeColumn> col = nsTreeColumn::From(aCol);
  NS_ENSURE_ARG(col);

  GetColumnProperties(*col, aProps);
  return NS_OK;
}

bool
nsTreeContentView::IsContainer(int32_t aRow, ErrorResult& aError)
{
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return false;
  }

  return mRows[aRow]->IsContainer();
}

NS_IMETHODIMP
nsTreeContentView::IsContainer(int32_t aIndex, bool* _retval)
{
  ErrorResult rv;
  *_retval = IsContainer(aIndex, rv);
  return rv.StealNSResult();
}

bool
nsTreeContentView::IsContainerOpen(int32_t aRow, ErrorResult& aError)
{
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return false;
  }

  return mRows[aRow]->IsOpen();
}

NS_IMETHODIMP
nsTreeContentView::IsContainerOpen(int32_t aIndex, bool* _retval)
{
  ErrorResult rv;
  *_retval = IsContainerOpen(aIndex, rv);
  return rv.StealNSResult();
}

bool
nsTreeContentView::IsContainerEmpty(int32_t aRow, ErrorResult& aError)
{
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return false;
  }

  return mRows[aRow]->IsEmpty();
}

NS_IMETHODIMP
nsTreeContentView::IsContainerEmpty(int32_t aIndex, bool* _retval)
{
  ErrorResult rv;
  *_retval = IsContainerEmpty(aIndex, rv);
  return rv.StealNSResult();
}

bool
nsTreeContentView::IsSeparator(int32_t aRow, ErrorResult& aError)
{
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return false;
  }

  return mRows[aRow]->IsSeparator();
}

NS_IMETHODIMP
nsTreeContentView::IsSeparator(int32_t aIndex, bool *_retval)
{
  ErrorResult rv;
  *_retval = IsSeparator(aIndex, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
nsTreeContentView::IsSorted(bool *_retval)
{
  *_retval = IsSorted();

  return NS_OK;
}

bool
nsTreeContentView::CanDrop(int32_t aRow, int32_t aOrientation,
                           DataTransfer* aDataTransfer, ErrorResult& aError)
{
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
  }
  return false;
}

NS_IMETHODIMP
nsTreeContentView::CanDrop(int32_t aIndex, int32_t aOrientation,
                           nsIDOMDataTransfer* aDataTransfer, bool *_retval)
{
  ErrorResult rv;
  *_retval = CanDrop(aIndex, aOrientation, DataTransfer::Cast(aDataTransfer),
                     rv);
  return rv.StealNSResult();
}

void
nsTreeContentView::Drop(int32_t aRow, int32_t aOrientation,
                        DataTransfer* aDataTransfer, ErrorResult& aError)
{
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
  }
}

NS_IMETHODIMP
nsTreeContentView::Drop(int32_t aRow, int32_t aOrientation, nsIDOMDataTransfer* aDataTransfer)
{
  ErrorResult rv;
  Drop(aRow, aOrientation, DataTransfer::Cast(aDataTransfer), rv);
  return rv.StealNSResult();
}

int32_t
nsTreeContentView::GetParentIndex(int32_t aRow, ErrorResult& aError)
{
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return 0;
  }

  return mRows[aRow]->mParentIndex;
}

NS_IMETHODIMP
nsTreeContentView::GetParentIndex(int32_t aRowIndex, int32_t* _retval)
{
  ErrorResult rv;
  *_retval = GetParentIndex(aRowIndex, rv);
  return rv.StealNSResult();
}

bool
nsTreeContentView::HasNextSibling(int32_t aRow, int32_t aAfterIndex,
                                  ErrorResult& aError)
{
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
nsTreeContentView::HasNextSibling(int32_t aRowIndex, int32_t aAfterIndex, bool* _retval)
{
  ErrorResult rv;
  *_retval = HasNextSibling(aRowIndex, aAfterIndex, rv);
  return rv.StealNSResult();
}

int32_t
nsTreeContentView::GetLevel(int32_t aRow, ErrorResult& aError)
{
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
nsTreeContentView::GetLevel(int32_t aIndex, int32_t* _retval)
{
  ErrorResult rv;
  *_retval = GetLevel(aIndex, rv);
  return rv.StealNSResult();
}

void
nsTreeContentView::GetImageSrc(int32_t aRow, nsTreeColumn& aColumn,
                               nsAString& aSrc, ErrorResult& aError)
{
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  Row* row = mRows[aRow].get();

  nsIContent* realRow =
    nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treerow);
  if (realRow) {
    Element* cell = GetCell(realRow, aColumn);
    if (cell)
      cell->GetAttr(kNameSpaceID_None, nsGkAtoms::src, aSrc);
  }
}

NS_IMETHODIMP
nsTreeContentView::GetImageSrc(int32_t aRow, nsITreeColumn* aCol, nsAString& _retval)
{
  RefPtr<nsTreeColumn> col = nsTreeColumn::From(aCol);
  NS_ENSURE_ARG(col);

  ErrorResult rv;
  GetImageSrc(aRow, *col, _retval, rv);
  return rv.StealNSResult();
}

int32_t
nsTreeContentView::GetProgressMode(int32_t aRow, nsTreeColumn& aColumn,
                                   ErrorResult& aError)
{
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return 0;
  }

  Row* row = mRows[aRow].get();

  nsIContent* realRow =
    nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treerow);
  if (realRow) {
    Element* cell = GetCell(realRow, aColumn);
    if (cell) {
      static Element::AttrValuesArray strings[] =
        {&nsGkAtoms::normal, &nsGkAtoms::undetermined, nullptr};
      switch (cell->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::mode,
                                    strings, eCaseMatters)) {
        case 0:
        {
          return nsITreeView::PROGRESS_NORMAL;
        }
        case 1:
        {
          return nsITreeView::PROGRESS_UNDETERMINED;
        }
      }
    }
  }

  return nsITreeView::PROGRESS_NONE;
}

NS_IMETHODIMP
nsTreeContentView::GetProgressMode(int32_t aRow, nsITreeColumn* aCol, int32_t* _retval)
{
  RefPtr<nsTreeColumn> col = nsTreeColumn::From(aCol);
  NS_ENSURE_ARG(col);

  ErrorResult rv;
  *_retval = GetProgressMode(aRow, *col, rv);
  return rv.StealNSResult();
}

void
nsTreeContentView::GetCellValue(int32_t aRow, nsTreeColumn& aColumn,
                                nsAString& aValue, ErrorResult& aError)
{
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  Row* row = mRows[aRow].get();

  nsIContent* realRow =
    nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treerow);
  if (realRow) {
    Element* cell = GetCell(realRow, aColumn);
    if (cell)
      cell->GetAttr(kNameSpaceID_None, nsGkAtoms::value, aValue);
  }
}

NS_IMETHODIMP
nsTreeContentView::GetCellValue(int32_t aRow, nsITreeColumn* aCol, nsAString& _retval)
{
  RefPtr<nsTreeColumn> col = nsTreeColumn::From(aCol);
  NS_ENSURE_ARG(col);

  ErrorResult rv;
  GetCellValue(aRow, *col, _retval, rv);
  return rv.StealNSResult();
}

void
nsTreeContentView::GetCellText(int32_t aRow, nsTreeColumn& aColumn,
                               nsAString& aText, ErrorResult& aError)
{
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
      if (cell)
        cell->GetAttr(kNameSpaceID_None, nsGkAtoms::label, aText);
    }
  }
}

NS_IMETHODIMP
nsTreeContentView::GetCellText(int32_t aRow, nsITreeColumn* aCol, nsAString& _retval)
{
  RefPtr<nsTreeColumn> col = nsTreeColumn::From(aCol);
  NS_ENSURE_ARG(col);

  ErrorResult rv;
  GetCellText(aRow, *col, _retval, rv);
  return rv.StealNSResult();
}

void
nsTreeContentView::SetTree(TreeBoxObject* aTree, ErrorResult& aError)
{
  aError = SetTree(aTree);
}

NS_IMETHODIMP
nsTreeContentView::SetTree(nsITreeBoxObject* aTree)
{
  ClearRows();

  mBoxObject = aTree;

  MOZ_ASSERT(!mRoot, "mRoot should have been cleared out by ClearRows");

  if (aTree) {
    // Get our root element
    nsCOMPtr<nsIBoxObject> boxObject = do_QueryInterface(mBoxObject);
    if (!boxObject) {
      mBoxObject = nullptr;
      return NS_ERROR_INVALID_ARG;
    }
    nsCOMPtr<nsIDOMElement> element;
    boxObject->GetElement(getter_AddRefs(element));

    mRoot = do_QueryInterface(element);
    NS_ENSURE_STATE(mRoot);

    // Add ourselves to document's observers.
    nsIDocument* document = mRoot->GetComposedDoc();
    if (document) {
      document->AddObserver(this);
      mDocument = document;
    }

    nsCOMPtr<nsIDOMElement> bodyElement;
    mBoxObject->GetTreeBody(getter_AddRefs(bodyElement));
    if (bodyElement) {
      mBody = do_QueryInterface(bodyElement);
      int32_t index = 0;
      Serialize(mBody, -1, &index, mRows);
    }
  }

  return NS_OK;
}

void
nsTreeContentView::ToggleOpenState(int32_t aRow, ErrorResult& aError)
{
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  // We don't serialize content right here, since content might be generated
  // lazily.
  Row* row = mRows[aRow].get();

  if (row->IsOpen())
    row->mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::open, NS_LITERAL_STRING("false"), true);
  else
    row->mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::open, NS_LITERAL_STRING("true"), true);
}

NS_IMETHODIMP
nsTreeContentView::ToggleOpenState(int32_t aIndex)
{
  ErrorResult rv;
  ToggleOpenState(aIndex, rv);
  return rv.StealNSResult();
}

void
nsTreeContentView::CycleHeader(nsTreeColumn& aColumn, ErrorResult& aError)
{
  if (!mRoot)
    return;

  nsCOMPtr<nsIDOMElement> element;
  aColumn.GetElement(getter_AddRefs(element));
  if (element) {
    nsCOMPtr<Element> column = do_QueryInterface(element);
    nsAutoString sort;
    column->GetAttr(kNameSpaceID_None, nsGkAtoms::sort, sort);
    if (!sort.IsEmpty()) {
      nsCOMPtr<nsIXULSortService> xs = do_GetService("@mozilla.org/xul/xul-sort-service;1");
      if (xs) {
        nsAutoString sortdirection;
        static Element::AttrValuesArray strings[] =
          {&nsGkAtoms::ascending, &nsGkAtoms::descending, nullptr};
        switch (column->FindAttrValueIn(kNameSpaceID_None,
                                        nsGkAtoms::sortDirection,
                                        strings, eCaseMatters)) {
          case 0: sortdirection.AssignLiteral("descending"); break;
          case 1: sortdirection.AssignLiteral("natural"); break;
          default: sortdirection.AssignLiteral("ascending"); break;
        }

        nsAutoString hints;
        column->GetAttr(kNameSpaceID_None, nsGkAtoms::sorthints, hints);
        sortdirection.Append(' ');
        sortdirection += hints;

        nsCOMPtr<nsIDOMNode> rootnode = do_QueryInterface(mRoot);
        xs->Sort(rootnode, sort, sortdirection);
      }
    }
  }
}

NS_IMETHODIMP
nsTreeContentView::CycleHeader(nsITreeColumn* aCol)
{
  RefPtr<nsTreeColumn> col = nsTreeColumn::From(aCol);
  NS_ENSURE_ARG(col);

  ErrorResult rv;
  CycleHeader(*col, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
nsTreeContentView::SelectionChanged()
{
  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::CycleCell(int32_t aRow, nsITreeColumn* aCol)
{
  return NS_OK;
}

bool
nsTreeContentView::IsEditable(int32_t aRow, nsTreeColumn& aColumn,
                              ErrorResult& aError)
{
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
nsTreeContentView::IsEditable(int32_t aRow, nsITreeColumn* aCol, bool* _retval)
{
  RefPtr<nsTreeColumn> col = nsTreeColumn::From(aCol);
  NS_ENSURE_ARG(col);

  ErrorResult rv;
  *_retval = IsEditable(aRow, *col, rv);
  return rv.StealNSResult();
}

bool
nsTreeContentView::IsSelectable(int32_t aRow, nsTreeColumn& aColumn,
                                ErrorResult& aError)
{
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return false;
  }

  Row* row = mRows[aRow].get();

  nsIContent* realRow =
    nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treerow);
  if (realRow) {
    Element* cell = GetCell(realRow, aColumn);
    if (cell && cell->AttrValueIs(kNameSpaceID_None, nsGkAtoms::selectable,
                                  nsGkAtoms::_false, eCaseMatters)) {
      return false;
    }
  }

  return true;
}

NS_IMETHODIMP
nsTreeContentView::IsSelectable(int32_t aRow, nsITreeColumn* aCol, bool* _retval)
{
  RefPtr<nsTreeColumn> col = nsTreeColumn::From(aCol);
  NS_ENSURE_ARG(col);

  ErrorResult rv;
  *_retval = IsSelectable(aRow, *col, rv);
  return rv.StealNSResult();
}

void
nsTreeContentView::SetCellValue(int32_t aRow, nsTreeColumn& aColumn,
                                const nsAString& aValue, ErrorResult& aError)
{
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  Row* row = mRows[aRow].get();

  nsIContent* realRow =
    nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treerow);
  if (realRow) {
    Element* cell = GetCell(realRow, aColumn);
    if (cell)
      cell->SetAttr(kNameSpaceID_None, nsGkAtoms::value, aValue, true);
  }
}

NS_IMETHODIMP
nsTreeContentView::SetCellValue(int32_t aRow, nsITreeColumn* aCol, const nsAString& aValue)
{
  RefPtr<nsTreeColumn> col = nsTreeColumn::From(aCol);
  NS_ENSURE_ARG(col);

  ErrorResult rv;
  SetCellValue(aRow, *col, aValue, rv);
  return rv.StealNSResult();
}

void
nsTreeContentView::SetCellText(int32_t aRow, nsTreeColumn& aColumn,
                               const nsAString& aValue, ErrorResult& aError)
{
  if (!IsValidRowIndex(aRow)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  Row* row = mRows[aRow].get();

  nsIContent* realRow =
    nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treerow);
  if (realRow) {
    Element* cell = GetCell(realRow, aColumn);
    if (cell)
      cell->SetAttr(kNameSpaceID_None, nsGkAtoms::label, aValue, true);
  }
}

NS_IMETHODIMP
nsTreeContentView::SetCellText(int32_t aRow, nsITreeColumn* aCol, const nsAString& aValue)
{
  RefPtr<nsTreeColumn> col = nsTreeColumn::From(aCol);
  NS_ENSURE_ARG(col);

  ErrorResult rv;
  SetCellText(aRow, *col, aValue, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
nsTreeContentView::PerformAction(const char16_t* aAction)
{
  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::PerformActionOnRow(const char16_t* aAction, int32_t aRow)
{
  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::PerformActionOnCell(const char16_t* aAction, int32_t aRow, nsITreeColumn* aCol)
{
  return NS_OK;
}

Element*
nsTreeContentView::GetItemAtIndex(int32_t aIndex, ErrorResult& aError)
{
  if (!IsValidRowIndex(aIndex)) {
    aError.Throw(NS_ERROR_INVALID_ARG);
    return nullptr;
  }

  return mRows[aIndex]->mContent;
}

NS_IMETHODIMP
nsTreeContentView::GetItemAtIndex(int32_t aIndex, nsIDOMElement** _retval)
{
  ErrorResult rv;
  Element* element = GetItemAtIndex(aIndex, rv);
  if (rv.Failed()) {
    return rv.StealNSResult();
  }

  if (!element) {
    *_retval = nullptr;
    return NS_OK;
  }

  return CallQueryInterface(element, _retval);
}

int32_t
nsTreeContentView::GetIndexOfItem(Element* aItem)
{
  return FindContent(aItem);
}

NS_IMETHODIMP
nsTreeContentView::GetIndexOfItem(nsIDOMElement* aItem, int32_t* _retval)
{
  nsCOMPtr<Element> element = do_QueryInterface(aItem);

  *_retval = GetIndexOfItem(element);

  return NS_OK;
}

void
nsTreeContentView::AttributeChanged(nsIDocument*  aDocument,
                                    dom::Element* aElement,
                                    int32_t       aNameSpaceID,
                                    nsAtom*      aAttribute,
                                    int32_t       aModType,
                                    const nsAttrValue* aOldValue)
{
  // Lots of codepaths under here that do all sorts of stuff, so be safe.
  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);

  // Make sure this notification concerns us.
  // First check the tag to see if it's one that we care about.

  if (mBoxObject && (aElement == mRoot || aElement == mBody)) {
    mBoxObject->ClearStyleAndImageCaches();
    mBoxObject->Invalidate();
  }

  // We don't consider non-XUL nodes.
  nsIContent* parent = nullptr;
  if (!aElement->IsXULElement() ||
      ((parent = aElement->GetParent()) && !parent->IsXULElement())) {
    return;
  }
  if (!aElement->IsAnyOfXULElements(nsGkAtoms::treecol,
                                    nsGkAtoms::treeitem,
                                    nsGkAtoms::treeseparator,
                                    nsGkAtoms::treerow,
                                    nsGkAtoms::treecell)) {
    return;
  }

  // If we have a legal tag, go up to the tree/select and make sure
  // that it's ours.

  for (nsIContent* element = aElement; element != mBody; element = element->GetParent()) {
    if (!element)
      return; // this is not for us
    if (element->IsXULElement(nsGkAtoms::tree))
      return; // this is not for us
  }

  // Handle changes of the hidden attribute.
  if (aAttribute == nsGkAtoms::hidden &&
      aElement->IsAnyOfXULElements(nsGkAtoms::treeitem,
                                   nsGkAtoms::treeseparator)) {
    bool hidden = aElement->AttrValueIs(kNameSpaceID_None,
                                          nsGkAtoms::hidden,
                                          nsGkAtoms::_true, eCaseMatters);

    int32_t index = FindContent(aElement);
    if (hidden && index >= 0) {
      // Hide this row along with its children.
      int32_t count = RemoveRow(index);
      if (mBoxObject)
        mBoxObject->RowCountChanged(index, -count);
    }
    else if (!hidden && index < 0) {
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
      if (mBoxObject) {
        nsCOMPtr<nsITreeColumns> cols;
        mBoxObject->GetColumns(getter_AddRefs(cols));
        if (cols) {
          nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aElement);
          nsCOMPtr<nsITreeColumn> col;
          cols->GetColumnFor(element, getter_AddRefs(col));
          mBoxObject->InvalidateColumn(col);
        }
      }
    }
  }
  else if (aElement->IsXULElement(nsGkAtoms::treeitem)) {
    int32_t index = FindContent(aElement);
    if (index >= 0) {
      Row* row = mRows[index].get();
      if (aAttribute == nsGkAtoms::container) {
        bool isContainer =
          aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::container,
                                nsGkAtoms::_true, eCaseMatters);
        row->SetContainer(isContainer);
        if (mBoxObject)
          mBoxObject->InvalidateRow(index);
      }
      else if (aAttribute == nsGkAtoms::open) {
        bool isOpen =
          aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::open,
                                nsGkAtoms::_true, eCaseMatters);
        bool wasOpen = row->IsOpen();
        if (! isOpen && wasOpen)
          CloseContainer(index);
        else if (isOpen && ! wasOpen)
          OpenContainer(index);
      }
      else if (aAttribute == nsGkAtoms::empty) {
        bool isEmpty =
          aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::empty,
                                nsGkAtoms::_true, eCaseMatters);
        row->SetEmpty(isEmpty);
        if (mBoxObject)
          mBoxObject->InvalidateRow(index);
      }
    }
  }
  else if (aElement->IsXULElement(nsGkAtoms::treeseparator)) {
    int32_t index = FindContent(aElement);
    if (index >= 0) {
      if (aAttribute == nsGkAtoms::properties && mBoxObject) {
        mBoxObject->InvalidateRow(index);
      }
    }
  }
  else if (aElement->IsXULElement(nsGkAtoms::treerow)) {
    if (aAttribute == nsGkAtoms::properties) {
      nsCOMPtr<nsIContent> parent = aElement->GetParent();
      if (parent) {
        int32_t index = FindContent(parent);
        if (index >= 0 && mBoxObject) {
          mBoxObject->InvalidateRow(index);
        }
      }
    }
  }
  else if (aElement->IsXULElement(nsGkAtoms::treecell)) {
    if (aAttribute == nsGkAtoms::ref ||
        aAttribute == nsGkAtoms::properties ||
        aAttribute == nsGkAtoms::mode ||
        aAttribute == nsGkAtoms::src ||
        aAttribute == nsGkAtoms::value ||
        aAttribute == nsGkAtoms::label) {
      nsIContent* parent = aElement->GetParent();
      if (parent) {
        nsCOMPtr<nsIContent> grandParent = parent->GetParent();
        if (grandParent && grandParent->IsXULElement()) {
          int32_t index = FindContent(grandParent);
          if (index >= 0 && mBoxObject) {
            // XXX Should we make an effort to invalidate only cell ?
            mBoxObject->InvalidateRow(index);
          }
        }
      }
    }
  }
}

void
nsTreeContentView::ContentAppended(nsIDocument *aDocument,
                                   nsIContent* aContainer,
                                   nsIContent* aFirstNewContent)
{
  for (nsIContent* cur = aFirstNewContent; cur; cur = cur->GetNextSibling()) {
    // Our contentinserted doesn't use the index
    ContentInserted(aDocument, aContainer, cur);
  }
}

void
nsTreeContentView::ContentInserted(nsIDocument *aDocument,
                                   nsIContent* aContainer,
                                   nsIContent* aChild)
{
  NS_ASSERTION(aChild, "null ptr");

  // Make sure this notification concerns us.
  // First check the tag to see if it's one that we care about.

  // Don't allow non-XUL nodes.
  if (!aChild->IsXULElement() || !aContainer->IsXULElement())
    return;

  if (!aChild->IsAnyOfXULElements(nsGkAtoms::treeitem,
                                  nsGkAtoms::treeseparator,
                                  nsGkAtoms::treechildren,
                                  nsGkAtoms::treerow,
                                  nsGkAtoms::treecell)) {
    return;
  }

  // If we have a legal tag, go up to the tree/select and make sure
  // that it's ours.

  for (nsIContent* element = aContainer; element != mBody; element = element->GetParent()) {
    if (!element)
      return; // this is not for us
    if (element->IsXULElement(nsGkAtoms::tree))
      return; // this is not for us
  }

  // Lots of codepaths under here that do all sorts of stuff, so be safe.
  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);

  if (aChild->IsXULElement(nsGkAtoms::treechildren)) {
    int32_t index = FindContent(aContainer);
    if (index >= 0) {
      Row* row = mRows[index].get();
      row->SetEmpty(false);
      if (mBoxObject)
        mBoxObject->InvalidateRow(index);
      if (row->IsContainer() && row->IsOpen()) {
        int32_t count = EnsureSubtree(index);
        if (mBoxObject)
          mBoxObject->RowCountChanged(index + 1, count);
      }
    }
  }
  else if (aChild->IsAnyOfXULElements(nsGkAtoms::treeitem,
                                      nsGkAtoms::treeseparator)) {
    InsertRowFor(aContainer, aChild);
  }
  else if (aChild->IsXULElement(nsGkAtoms::treerow)) {
    int32_t index = FindContent(aContainer);
    if (index >= 0 && mBoxObject)
      mBoxObject->InvalidateRow(index);
  }
  else if (aChild->IsXULElement(nsGkAtoms::treecell)) {
    nsCOMPtr<nsIContent> parent = aContainer->GetParent();
    if (parent) {
      int32_t index = FindContent(parent);
      if (index >= 0 && mBoxObject)
        mBoxObject->InvalidateRow(index);
    }
  }
}

void
nsTreeContentView::ContentRemoved(nsIDocument *aDocument,
                                  nsIContent* aContainer,
                                  nsIContent* aChild,
                                  nsIContent* aPreviousSibling)
{
  NS_ASSERTION(aChild, "null ptr");

  // Make sure this notification concerns us.
  // First check the tag to see if it's one that we care about.

  // We don't consider non-XUL nodes.
  if (!aChild->IsXULElement() || !aContainer->IsXULElement())
    return;

  if (!aChild->IsAnyOfXULElements(nsGkAtoms::treeitem,
                                  nsGkAtoms::treeseparator,
                                  nsGkAtoms::treechildren,
                                  nsGkAtoms::treerow,
                                  nsGkAtoms::treecell)) {
    return;
  }

  // If we have a legal tag, go up to the tree/select and make sure
  // that it's ours.

  for (nsIContent* element = aContainer; element != mBody; element = element->GetParent()) {
    if (!element)
      return; // this is not for us
    if (element->IsXULElement(nsGkAtoms::tree))
      return; // this is not for us
  }

  // Lots of codepaths under here that do all sorts of stuff, so be safe.
  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);

  if (aChild->IsXULElement(nsGkAtoms::treechildren)) {
    int32_t index = FindContent(aContainer);
    if (index >= 0) {
      Row* row = mRows[index].get();
      row->SetEmpty(true);
      int32_t count = RemoveSubtree(index);
      // Invalidate also the row to update twisty.
      if (mBoxObject) {
        mBoxObject->InvalidateRow(index);
        mBoxObject->RowCountChanged(index + 1, -count);
      }
    }
  }
  else if (aChild->IsAnyOfXULElements(nsGkAtoms::treeitem,
                                      nsGkAtoms::treeseparator)) {
    int32_t index = FindContent(aChild);
    if (index >= 0) {
      int32_t count = RemoveRow(index);
      if (mBoxObject)
        mBoxObject->RowCountChanged(index, -count);
    }
  }
  else if (aChild->IsXULElement(nsGkAtoms::treerow)) {
    int32_t index = FindContent(aContainer);
    if (index >= 0 && mBoxObject)
      mBoxObject->InvalidateRow(index);
  }
  else if (aChild->IsXULElement(nsGkAtoms::treecell)) {
    nsCOMPtr<nsIContent> parent = aContainer->GetParent();
    if (parent) {
      int32_t index = FindContent(parent);
      if (index >= 0 && mBoxObject)
        mBoxObject->InvalidateRow(index);
    }
  }
}

void
nsTreeContentView::NodeWillBeDestroyed(const nsINode* aNode)
{
  // XXXbz do we need this strong ref?  Do we drop refs to self in ClearRows?
  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
  ClearRows();
}


// Recursively serialize content, starting with aContent.
void
nsTreeContentView::Serialize(nsIContent* aContent, int32_t aParentIndex,
                             int32_t* aIndex, nsTArray<UniquePtr<Row>>& aRows)
{
  // Don't allow non-XUL nodes.
  if (!aContent->IsXULElement())
    return;

  dom::FlattenedChildIterator iter(aContent);
  for (nsIContent* content = iter.GetNextChild(); content; content = iter.GetNextChild()) {
    int32_t count = aRows.Length();

    if (content->IsXULElement(nsGkAtoms::treeitem)) {
      SerializeItem(content->AsElement(), aParentIndex, aIndex, aRows);
    } else if (content->IsXULElement(nsGkAtoms::treeseparator)) {
      SerializeSeparator(content->AsElement(), aParentIndex, aIndex, aRows);
    }

    *aIndex += aRows.Length() - count;
  }
}

void
nsTreeContentView::SerializeItem(Element* aContent, int32_t aParentIndex,
                                 int32_t* aIndex, nsTArray<UniquePtr<Row>>& aRows)
{
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
      }
      else
        row->SetEmpty(true);
    } else if (aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::empty,
                                     nsGkAtoms::_true, eCaseMatters)) {
      row->SetEmpty(true);
    }
  }
}

void
nsTreeContentView::SerializeSeparator(Element* aContent,
                                      int32_t aParentIndex, int32_t* aIndex,
                                      nsTArray<UniquePtr<Row>>& aRows)
{
  if (aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::hidden,
                            nsGkAtoms::_true, eCaseMatters))
    return;

  auto row = MakeUnique<Row>(aContent, aParentIndex);
  row->SetSeparator(true);
  aRows.AppendElement(Move(row));
}

void
nsTreeContentView::GetIndexInSubtree(nsIContent* aContainer,
                                     nsIContent* aContent, int32_t* aIndex)
{
  uint32_t childCount = aContainer->GetChildCount();

  if (!aContainer->IsXULElement())
    return;

  for (uint32_t i = 0; i < childCount; i++) {
    nsIContent *content = aContainer->GetChildAt(i);

    if (content == aContent)
      break;

    if (content->IsXULElement(nsGkAtoms::treeitem)) {
      if (!content->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::hidden,
                                             nsGkAtoms::_true, eCaseMatters)) {
        (*aIndex)++;
        if (content->AsElement()->AttrValueIs(kNameSpaceID_None,
                                              nsGkAtoms::container,
                                              nsGkAtoms::_true, eCaseMatters) &&
            content->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::open,
                                              nsGkAtoms::_true, eCaseMatters)) {
          nsIContent* child =
            nsTreeUtils::GetImmediateChild(content, nsGkAtoms::treechildren);
          if (child && child->IsXULElement())
            GetIndexInSubtree(child, aContent, aIndex);
        }
      }
    }
    else if (content->IsXULElement(nsGkAtoms::treeseparator)) {
      if (!content->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::hidden,
                                             nsGkAtoms::_true, eCaseMatters))
        (*aIndex)++;
    }
  }
}

int32_t
nsTreeContentView::EnsureSubtree(int32_t aIndex)
{
  Row* row = mRows[aIndex].get();

  nsIContent* child;
  child = nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treechildren);
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
  UniquePtr<Row>* newRows = mRows.InsertElementsAt(aIndex + 1,
                                                   rows.Length());
  for (nsTArray<Row>::index_type i = 0; i < rows.Length(); i++) {
    newRows[i] = Move(rows[i]);
  }
  int32_t count = rows.Length();

  row->mSubtreeSize += count;
  UpdateSubtreeSizes(row->mParentIndex, count);

  // Update parent indexes, but skip newly added rows.
  // They already have correct values.
  UpdateParentIndexes(aIndex, count + 1, count);

  return count;
}

int32_t
nsTreeContentView::RemoveSubtree(int32_t aIndex)
{
  Row* row = mRows[aIndex].get();
  int32_t count = row->mSubtreeSize;

  mRows.RemoveElementsAt(aIndex + 1, count);

  row->mSubtreeSize -= count;
  UpdateSubtreeSizes(row->mParentIndex, -count);

  UpdateParentIndexes(aIndex, 0, -count);

  return count;
}

void
nsTreeContentView::InsertRowFor(nsIContent* aParent, nsIContent* aChild)
{
  int32_t grandParentIndex = -1;
  bool insertRow = false;

  nsCOMPtr<nsIContent> grandParent = aParent->GetParent();

  if (grandParent->IsXULElement(nsGkAtoms::tree)) {
    // Allow insertion to the outermost container.
    insertRow = true;
  }
  else {
    // Test insertion to an inner container.

    // First try to find this parent in our array of rows, if we find one
    // we can be sure that all other parents are open too.
    grandParentIndex = FindContent(grandParent);
    if (grandParentIndex >= 0) {
      // Got it, now test if it is open.
      if (mRows[grandParentIndex]->IsOpen())
        insertRow = true;
    }
  }

  if (insertRow) {
    int32_t index = 0;
    GetIndexInSubtree(aParent, aChild, &index);

    int32_t count = InsertRow(grandParentIndex, index, aChild);
    if (mBoxObject)
      mBoxObject->RowCountChanged(grandParentIndex + index + 1, count);
  }
}

int32_t
nsTreeContentView::InsertRow(int32_t aParentIndex, int32_t aIndex, nsIContent* aContent)
{
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
    mRows.InsertElementAt(aParentIndex + aIndex + i + 1, Move(rows[i]));
  }

  UpdateSubtreeSizes(aParentIndex, count);

  // Update parent indexes, but skip added rows.
  // They already have correct values.
  UpdateParentIndexes(aParentIndex + aIndex, count + 1, count);

  return count;
}

int32_t
nsTreeContentView::RemoveRow(int32_t aIndex)
{
  Row* row = mRows[aIndex].get();
  int32_t count = row->mSubtreeSize + 1;
  int32_t parentIndex = row->mParentIndex;

  mRows.RemoveElementsAt(aIndex, count);

  UpdateSubtreeSizes(parentIndex, -count);

  UpdateParentIndexes(aIndex, 0, -count);

  return count;
}

void
nsTreeContentView::ClearRows()
{
  mRows.Clear();
  mRoot = nullptr;
  mBody = nullptr;
  // Remove ourselves from mDocument's observers.
  if (mDocument) {
    mDocument->RemoveObserver(this);
    mDocument = nullptr;
  }
}

void
nsTreeContentView::OpenContainer(int32_t aIndex)
{
  Row* row = mRows[aIndex].get();
  row->SetOpen(true);

  int32_t count = EnsureSubtree(aIndex);
  if (mBoxObject) {
    mBoxObject->InvalidateRow(aIndex);
    mBoxObject->RowCountChanged(aIndex + 1, count);
  }
}

void
nsTreeContentView::CloseContainer(int32_t aIndex)
{
  Row* row = mRows[aIndex].get();
  row->SetOpen(false);

  int32_t count = RemoveSubtree(aIndex);
  if (mBoxObject) {
    mBoxObject->InvalidateRow(aIndex);
    mBoxObject->RowCountChanged(aIndex + 1, -count);
  }
}

int32_t
nsTreeContentView::FindContent(nsIContent* aContent)
{
  for (uint32_t i = 0; i < mRows.Length(); i++) {
    if (mRows[i]->mContent == aContent) {
      return i;
    }
  }

  return -1;
}

void
nsTreeContentView::UpdateSubtreeSizes(int32_t aParentIndex, int32_t count)
{
  while (aParentIndex >= 0) {
    Row* row = mRows[aParentIndex].get();
    row->mSubtreeSize += count;
    aParentIndex = row->mParentIndex;
  }
}

void
nsTreeContentView::UpdateParentIndexes(int32_t aIndex, int32_t aSkip, int32_t aCount)
{
  int32_t count = mRows.Length();
  for (int32_t i = aIndex + aSkip; i < count; i++) {
    Row* row = mRows[i].get();
    if (row->mParentIndex > aIndex) {
      row->mParentIndex += aCount;
    }
  }
}

Element*
nsTreeContentView::GetCell(nsIContent* aContainer, nsTreeColumn& aCol)
{
  RefPtr<nsAtom> colAtom(aCol.GetAtom());
  int32_t colIndex(aCol.GetIndex());

  // Traverse through cells, try to find the cell by "ref" attribute or by cell
  // index in a row. "ref" attribute has higher priority.
  Element* result = nullptr;
  int32_t j = 0;
  dom::FlattenedChildIterator iter(aContainer);
  for (nsIContent* cell = iter.GetNextChild(); cell; cell = iter.GetNextChild()) {
    if (cell->IsXULElement(nsGkAtoms::treecell)) {
      if (colAtom &&
          cell->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::ref,
                                         colAtom, eCaseMatters)) {
        result = cell->AsElement();
        break;
      }
      else if (j == colIndex) {
        result = cell->AsElement();
      }
      j++;
    }
  }

  return result;
}

bool
nsTreeContentView::IsValidRowIndex(int32_t aRowIndex)
{
  return aRowIndex >= 0 && aRowIndex < int32_t(mRows.Length());
}
