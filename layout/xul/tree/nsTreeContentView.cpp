/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsIBoxObject.h"
#include "nsTreeUtils.h"
#include "nsTreeContentView.h"
#include "ChildIterator.h"
#include "nsDOMClassInfoID.h"
#include "nsError.h"
#include "nsEventStates.h"
#include "nsINodeInfo.h"
#include "nsIXULSortService.h"
#include "nsContentUtils.h"
#include "nsTreeBodyFrame.h"
#include "mozilla/dom/Element.h"
#include "nsServiceManagerUtils.h"

using namespace mozilla;

#define NS_ENSURE_NATIVE_COLUMN(_col)                                \
  nsRefPtr<nsTreeColumn> col = nsTreeBodyFrame::GetColumnImpl(_col); \
  if (!col) {                                                        \
    return NS_ERROR_INVALID_ARG;                                     \
  }

// A content model view implementation for the tree.

#define ROW_FLAG_CONTAINER      0x01
#define ROW_FLAG_OPEN           0x02
#define ROW_FLAG_EMPTY          0x04
#define ROW_FLAG_SEPARATOR      0x08

class Row
{
  public:
    Row(nsIContent* aContent, int32_t aParentIndex)
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
    nsIContent*         mContent;

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

NS_IMPL_CYCLE_COLLECTION_4(nsTreeContentView,
                           mBoxObject,
                           mSelection,
                           mRoot,
                           mBody)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsTreeContentView)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsTreeContentView)

DOMCI_DATA(TreeContentView, nsTreeContentView)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsTreeContentView)
  NS_INTERFACE_MAP_ENTRY(nsITreeView)
  NS_INTERFACE_MAP_ENTRY(nsITreeContentView)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentObserver)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsITreeContentView)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(TreeContentView)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsTreeContentView::GetRowCount(int32_t* aRowCount)
{
  *aRowCount = mRows.Length();

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::GetSelection(nsITreeSelection** aSelection)
{
  NS_IF_ADDREF(*aSelection = mSelection);

  return NS_OK;
}

bool
nsTreeContentView::CanTrustTreeSelection(nsISupports* aValue)
{
  // Untrusted content is only allowed to specify known-good views
  if (nsContentUtils::IsCallerChrome())
    return true;
  nsCOMPtr<nsINativeTreeSelection> nativeTreeSel = do_QueryInterface(aValue);
  return nativeTreeSel && NS_SUCCEEDED(nativeTreeSel->EnsureNative());
}

NS_IMETHODIMP
nsTreeContentView::SetSelection(nsITreeSelection* aSelection)
{
  NS_ENSURE_TRUE(!aSelection || CanTrustTreeSelection(aSelection),
                 NS_ERROR_DOM_SECURITY_ERR);

  mSelection = aSelection;
  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::GetRowProperties(int32_t aIndex, nsAString& aProps)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < int32_t(mRows.Length()), "bad index");
  if (aIndex < 0 || aIndex >= int32_t(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  Row* row = mRows[aIndex];
  nsIContent* realRow;
  if (row->IsSeparator())
    realRow = row->mContent;
  else
    realRow = nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treerow);

  if (realRow) {
    realRow->GetAttr(kNameSpaceID_None, nsGkAtoms::properties, aProps);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::GetCellProperties(int32_t aRow, nsITreeColumn* aCol,
                                     nsAString& aProps)
{
  NS_ENSURE_NATIVE_COLUMN(aCol);
  NS_PRECONDITION(aRow >= 0 && aRow < int32_t(mRows.Length()), "bad row");
  if (aRow < 0 || aRow >= int32_t(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  Row* row = mRows[aRow];
  nsIContent* realRow =
    nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treerow);
  if (realRow) {
    nsIContent* cell = GetCell(realRow, aCol);
    if (cell) {
      cell->GetAttr(kNameSpaceID_None, nsGkAtoms::properties, aProps);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::GetColumnProperties(nsITreeColumn* aCol, nsAString& aProps)
{
  NS_ENSURE_NATIVE_COLUMN(aCol);
  nsCOMPtr<nsIDOMElement> element;
  aCol->GetElement(getter_AddRefs(element));

  element->GetAttribute(NS_LITERAL_STRING("properties"), aProps);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::IsContainer(int32_t aIndex, bool* _retval)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < int32_t(mRows.Length()), "bad index");
  if (aIndex < 0 || aIndex >= int32_t(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  *_retval = mRows[aIndex]->IsContainer();

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::IsContainerOpen(int32_t aIndex, bool* _retval)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < int32_t(mRows.Length()), "bad index");
  if (aIndex < 0 || aIndex >= int32_t(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  *_retval = mRows[aIndex]->IsOpen();

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::IsContainerEmpty(int32_t aIndex, bool* _retval)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < int32_t(mRows.Length()), "bad index");
  if (aIndex < 0 || aIndex >= int32_t(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  *_retval = mRows[aIndex]->IsEmpty();

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::IsSeparator(int32_t aIndex, bool *_retval)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < int32_t(mRows.Length()), "bad index");
  if (aIndex < 0 || aIndex >= int32_t(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  *_retval = mRows[aIndex]->IsSeparator();

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::IsSorted(bool *_retval)
{
  *_retval = false;

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::CanDrop(int32_t aIndex, int32_t aOrientation,
                           nsIDOMDataTransfer* aDataTransfer, bool *_retval)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < int32_t(mRows.Length()), "bad index");
  if (aIndex < 0 || aIndex >= int32_t(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  *_retval = false;
 
  return NS_OK;
}
 
NS_IMETHODIMP
nsTreeContentView::Drop(int32_t aRow, int32_t aOrientation, nsIDOMDataTransfer* aDataTransfer)
{
  NS_PRECONDITION(aRow >= 0 && aRow < int32_t(mRows.Length()), "bad row");
  if (aRow < 0 || aRow >= int32_t(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::GetParentIndex(int32_t aRowIndex, int32_t* _retval)
{
  NS_PRECONDITION(aRowIndex >= 0 && aRowIndex < int32_t(mRows.Length()),
                  "bad row index");
  if (aRowIndex < 0 || aRowIndex >= int32_t(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  *_retval = mRows[aRowIndex]->mParentIndex;

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::HasNextSibling(int32_t aRowIndex, int32_t aAfterIndex, bool* _retval)
{
  NS_PRECONDITION(aRowIndex >= 0 && aRowIndex < int32_t(mRows.Length()),
                  "bad row index");
  if (aRowIndex < 0 || aRowIndex >= int32_t(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  // We have a next sibling if the row is not the last in the subtree.
  int32_t parentIndex = mRows[aRowIndex]->mParentIndex;
  if (parentIndex >= 0) {
    // Compute the last index in this subtree.
    int32_t lastIndex = parentIndex + (mRows[parentIndex])->mSubtreeSize;
    Row* row = mRows[lastIndex];
    while (row->mParentIndex != parentIndex) {
      lastIndex = row->mParentIndex;
      row = mRows[lastIndex];
    }

    *_retval = aRowIndex < lastIndex;
  }
  else {
    *_retval = uint32_t(aRowIndex) < mRows.Length() - 1;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::GetLevel(int32_t aIndex, int32_t* _retval)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < int32_t(mRows.Length()), "bad index");
  if (aIndex < 0 || aIndex >= int32_t(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  int32_t level = 0;
  Row* row = mRows[aIndex];
  while (row->mParentIndex >= 0) {
    level++;
    row = mRows[row->mParentIndex];
  }
  *_retval = level;

  return NS_OK;
}

 NS_IMETHODIMP
nsTreeContentView::GetImageSrc(int32_t aRow, nsITreeColumn* aCol, nsAString& _retval)
{
  _retval.Truncate();
  NS_ENSURE_NATIVE_COLUMN(aCol);
  NS_PRECONDITION(aRow >= 0 && aRow < int32_t(mRows.Length()), "bad row");
  if (aRow < 0 || aRow >= int32_t(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  Row* row = mRows[aRow];

  nsIContent* realRow =
    nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treerow);
  if (realRow) {
    nsIContent* cell = GetCell(realRow, aCol);
    if (cell)
      cell->GetAttr(kNameSpaceID_None, nsGkAtoms::src, _retval);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::GetProgressMode(int32_t aRow, nsITreeColumn* aCol, int32_t* _retval)
{
  NS_ENSURE_NATIVE_COLUMN(aCol);
  NS_PRECONDITION(aRow >= 0 && aRow < int32_t(mRows.Length()), "bad row");
  if (aRow < 0 || aRow >= int32_t(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  *_retval = nsITreeView::PROGRESS_NONE;

  Row* row = mRows[aRow];

  nsIContent* realRow =
    nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treerow);
  if (realRow) {
    nsIContent* cell = GetCell(realRow, aCol);
    if (cell) {
      static nsIContent::AttrValuesArray strings[] =
        {&nsGkAtoms::normal, &nsGkAtoms::undetermined, nullptr};
      switch (cell->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::mode,
                                    strings, eCaseMatters)) {
        case 0: *_retval = nsITreeView::PROGRESS_NORMAL; break;
        case 1: *_retval = nsITreeView::PROGRESS_UNDETERMINED; break;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::GetCellValue(int32_t aRow, nsITreeColumn* aCol, nsAString& _retval)
{
  _retval.Truncate();
  NS_ENSURE_NATIVE_COLUMN(aCol);
  NS_PRECONDITION(aRow >= 0 && aRow < int32_t(mRows.Length()), "bad row");
  if (aRow < 0 || aRow >= int32_t(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  Row* row = mRows[aRow];

  nsIContent* realRow =
    nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treerow);
  if (realRow) {
    nsIContent* cell = GetCell(realRow, aCol);
    if (cell)
      cell->GetAttr(kNameSpaceID_None, nsGkAtoms::value, _retval);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::GetCellText(int32_t aRow, nsITreeColumn* aCol, nsAString& _retval)
{
  _retval.Truncate();
  NS_ENSURE_NATIVE_COLUMN(aCol);
  NS_PRECONDITION(aRow >= 0 && aRow < int32_t(mRows.Length()), "bad row");
  NS_PRECONDITION(aCol, "bad column");

  if (aRow < 0 || aRow >= int32_t(mRows.Length()) || !aCol)
    return NS_ERROR_INVALID_ARG;

  Row* row = mRows[aRow];

  // Check for a "label" attribute - this is valid on an <treeitem>
  // with a single implied column.
  if (row->mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::label, _retval)
      && !_retval.IsEmpty())
    return NS_OK;

  nsIAtom *rowTag = row->mContent->Tag();
  if (rowTag == nsGkAtoms::treeitem && row->mContent->IsXUL()) {
    nsIContent* realRow =
      nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treerow);
    if (realRow) {
      nsIContent* cell = GetCell(realRow, aCol);
      if (cell)
        cell->GetAttr(kNameSpaceID_None, nsGkAtoms::label, _retval);
    }
  }

  return NS_OK;
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
    nsIDocument* document = mRoot->GetDocument();
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

NS_IMETHODIMP
nsTreeContentView::ToggleOpenState(int32_t aIndex)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < int32_t(mRows.Length()), "bad index");
  if (aIndex < 0 || aIndex >= int32_t(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  // We don't serialize content right here, since content might be generated
  // lazily.
  Row* row = mRows[aIndex];

  if (row->IsOpen())
    row->mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::open, NS_LITERAL_STRING("false"), true);
  else
    row->mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::open, NS_LITERAL_STRING("true"), true);

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::CycleHeader(nsITreeColumn* aCol)
{
  NS_ENSURE_NATIVE_COLUMN(aCol);

  if (!mRoot)
    return NS_OK;

  nsCOMPtr<nsIDOMElement> element;
  aCol->GetElement(getter_AddRefs(element));
  if (element) {
    nsCOMPtr<nsIContent> column = do_QueryInterface(element);
    nsAutoString sort;
    column->GetAttr(kNameSpaceID_None, nsGkAtoms::sort, sort);
    if (!sort.IsEmpty()) {
      nsCOMPtr<nsIXULSortService> xs = do_GetService("@mozilla.org/xul/xul-sort-service;1");
      if (xs) {
        nsAutoString sortdirection;
        static nsIContent::AttrValuesArray strings[] =
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
        sortdirection.AppendLiteral(" ");
        sortdirection += hints;

        nsCOMPtr<nsIDOMNode> rootnode = do_QueryInterface(mRoot);
        xs->Sort(rootnode, sort, sortdirection);
      }
    }
  }

  return NS_OK;
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

NS_IMETHODIMP
nsTreeContentView::IsEditable(int32_t aRow, nsITreeColumn* aCol, bool* _retval)
{
  *_retval = false;
  NS_ENSURE_NATIVE_COLUMN(aCol);
  NS_PRECONDITION(aRow >= 0 && aRow < int32_t(mRows.Length()), "bad row");
  if (aRow < 0 || aRow >= int32_t(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  *_retval = true;

  Row* row = mRows[aRow];

  nsIContent* realRow =
    nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treerow);
  if (realRow) {
    nsIContent* cell = GetCell(realRow, aCol);
    if (cell && cell->AttrValueIs(kNameSpaceID_None, nsGkAtoms::editable,
                                  nsGkAtoms::_false, eCaseMatters)) {
      *_retval = false;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::IsSelectable(int32_t aRow, nsITreeColumn* aCol, bool* _retval)
{
  NS_ENSURE_NATIVE_COLUMN(aCol);
  NS_PRECONDITION(aRow >= 0 && aRow < int32_t(mRows.Length()), "bad row");
  if (aRow < 0 || aRow >= int32_t(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  *_retval = true;

  Row* row = mRows[aRow];

  nsIContent* realRow =
    nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treerow);
  if (realRow) {
    nsIContent* cell = GetCell(realRow, aCol);
    if (cell && cell->AttrValueIs(kNameSpaceID_None, nsGkAtoms::selectable,
                                  nsGkAtoms::_false, eCaseMatters)) {
      *_retval = false;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::SetCellValue(int32_t aRow, nsITreeColumn* aCol, const nsAString& aValue)
{
  NS_ENSURE_NATIVE_COLUMN(aCol);
  NS_PRECONDITION(aRow >= 0 && aRow < int32_t(mRows.Length()), "bad row");
  if (aRow < 0 || aRow >= int32_t(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  Row* row = mRows[aRow];

  nsIContent* realRow =
    nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treerow);
  if (realRow) {
    nsIContent* cell = GetCell(realRow, aCol);
    if (cell)
      cell->SetAttr(kNameSpaceID_None, nsGkAtoms::value, aValue, true);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::SetCellText(int32_t aRow, nsITreeColumn* aCol, const nsAString& aValue)
{
  NS_ENSURE_NATIVE_COLUMN(aCol);
  NS_PRECONDITION(aRow >= 0 && aRow < int32_t(mRows.Length()), "bad row");
  if (aRow < 0 || aRow >= int32_t(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  Row* row = mRows[aRow];

  nsIContent* realRow =
    nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treerow);
  if (realRow) {
    nsIContent* cell = GetCell(realRow, aCol);
    if (cell)
      cell->SetAttr(kNameSpaceID_None, nsGkAtoms::label, aValue, true);
  }

  return NS_OK;
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


NS_IMETHODIMP
nsTreeContentView::GetItemAtIndex(int32_t aIndex, nsIDOMElement** _retval)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < int32_t(mRows.Length()), "bad index");
  if (aIndex < 0 || aIndex >= int32_t(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  Row* row = mRows[aIndex];
  row->mContent->QueryInterface(NS_GET_IID(nsIDOMElement), (void**)_retval);

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::GetIndexOfItem(nsIDOMElement* aItem, int32_t* _retval)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aItem);
  *_retval = FindContent(content);

  return NS_OK;
}

void
nsTreeContentView::AttributeChanged(nsIDocument*  aDocument,
                                    dom::Element* aElement,
                                    int32_t       aNameSpaceID,
                                    nsIAtom*      aAttribute,
                                    int32_t       aModType)
{
  // Lots of codepaths under here that do all sorts of stuff, so be safe.
  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);

  // Make sure this notification concerns us.
  // First check the tag to see if it's one that we care about.
  nsIAtom* tag = aElement->Tag();

  if (mBoxObject && (aElement == mRoot || aElement == mBody)) {
    mBoxObject->ClearStyleAndImageCaches();
    mBoxObject->Invalidate();
  }

  // We don't consider non-XUL nodes.
  nsIContent* parent = nullptr;
  if (!aElement->IsXUL() ||
      ((parent = aElement->GetParent()) && !parent->IsXUL())) {
    return;
  }
  if (tag != nsGkAtoms::treecol &&
      tag != nsGkAtoms::treeitem &&
      tag != nsGkAtoms::treeseparator &&
      tag != nsGkAtoms::treerow &&
      tag != nsGkAtoms::treecell) {
    return;
  }

  // If we have a legal tag, go up to the tree/select and make sure
  // that it's ours.

  for (nsIContent* element = aElement; element != mBody; element = element->GetParent()) {
    if (!element)
      return; // this is not for us
    nsIAtom *parentTag = element->Tag();
    if (element->IsXUL() && parentTag == nsGkAtoms::tree)
      return; // this is not for us
  }

  // Handle changes of the hidden attribute.
  if (aAttribute == nsGkAtoms::hidden &&
     (tag == nsGkAtoms::treeitem || tag == nsGkAtoms::treeseparator)) {
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

  if (tag == nsGkAtoms::treecol) {
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
  else if (tag == nsGkAtoms::treeitem) {
    int32_t index = FindContent(aElement);
    if (index >= 0) {
      Row* row = mRows[index];
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
  else if (tag == nsGkAtoms::treeseparator) {
    int32_t index = FindContent(aElement);
    if (index >= 0) {
      if (aAttribute == nsGkAtoms::properties && mBoxObject) {
        mBoxObject->InvalidateRow(index);
      }
    }
  }
  else if (tag == nsGkAtoms::treerow) {
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
  else if (tag == nsGkAtoms::treecell) {
    if (aAttribute == nsGkAtoms::ref ||
        aAttribute == nsGkAtoms::properties ||
        aAttribute == nsGkAtoms::mode ||
        aAttribute == nsGkAtoms::src ||
        aAttribute == nsGkAtoms::value ||
        aAttribute == nsGkAtoms::label) {
      nsIContent* parent = aElement->GetParent();
      if (parent) {
        nsCOMPtr<nsIContent> grandParent = parent->GetParent();
        if (grandParent && grandParent->IsXUL()) {
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
                                   nsIContent* aFirstNewContent,
                                   int32_t     /* unused */)
{
  for (nsIContent* cur = aFirstNewContent; cur; cur = cur->GetNextSibling()) {
    // Our contentinserted doesn't use the index
    ContentInserted(aDocument, aContainer, cur, 0);
  }
}

void
nsTreeContentView::ContentInserted(nsIDocument *aDocument,
                                   nsIContent* aContainer,
                                   nsIContent* aChild,
                                   int32_t /* unused */)
{
  NS_ASSERTION(aChild, "null ptr");

  // Make sure this notification concerns us.
  // First check the tag to see if it's one that we care about.
  nsIAtom *childTag = aChild->Tag();

  // Don't allow non-XUL nodes.
  if (!aChild->IsXUL() || !aContainer->IsXUL())
    return;
  if (childTag != nsGkAtoms::treeitem &&
      childTag != nsGkAtoms::treeseparator &&
      childTag != nsGkAtoms::treechildren &&
      childTag != nsGkAtoms::treerow &&
      childTag != nsGkAtoms::treecell) {
    return;
  }

  // If we have a legal tag, go up to the tree/select and make sure
  // that it's ours.

  for (nsIContent* element = aContainer; element != mBody; element = element->GetParent()) {
    if (!element)
      return; // this is not for us
    nsIAtom *parentTag = element->Tag();
    if (element->IsXUL() && parentTag == nsGkAtoms::tree)
      return; // this is not for us
  }

  // Lots of codepaths under here that do all sorts of stuff, so be safe.
  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);

  if (childTag == nsGkAtoms::treechildren) {
    int32_t index = FindContent(aContainer);
    if (index >= 0) {
      Row* row = mRows[index];
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
  else if (childTag == nsGkAtoms::treeitem ||
           childTag == nsGkAtoms::treeseparator) {
    InsertRowFor(aContainer, aChild);
  }
  else if (childTag == nsGkAtoms::treerow) {
    int32_t index = FindContent(aContainer);
    if (index >= 0 && mBoxObject)
      mBoxObject->InvalidateRow(index);
  }
  else if (childTag == nsGkAtoms::treecell) {
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
                                  int32_t aIndexInContainer,
                                  nsIContent* aPreviousSibling)
{
  NS_ASSERTION(aChild, "null ptr");

  // Make sure this notification concerns us.
  // First check the tag to see if it's one that we care about.
  nsIAtom *tag = aChild->Tag();

  // We don't consider non-XUL nodes.
  if (!aChild->IsXUL() || !aContainer->IsXUL())
    return;
  if (tag != nsGkAtoms::treeitem &&
      tag != nsGkAtoms::treeseparator &&
      tag != nsGkAtoms::treechildren &&
      tag != nsGkAtoms::treerow &&
      tag != nsGkAtoms::treecell) {
    return;
  }

  // If we have a legal tag, go up to the tree/select and make sure
  // that it's ours.

  for (nsIContent* element = aContainer; element != mBody; element = element->GetParent()) {
    if (!element)
      return; // this is not for us
    nsIAtom *parentTag = element->Tag();
    if (element->IsXUL() && parentTag == nsGkAtoms::tree)
      return; // this is not for us
  }

  // Lots of codepaths under here that do all sorts of stuff, so be safe.
  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);

  if (tag == nsGkAtoms::treechildren) {
    int32_t index = FindContent(aContainer);
    if (index >= 0) {
      Row* row = mRows[index];
      row->SetEmpty(true);
      int32_t count = RemoveSubtree(index);
      // Invalidate also the row to update twisty.
      if (mBoxObject) {
        mBoxObject->InvalidateRow(index);
        mBoxObject->RowCountChanged(index + 1, -count);
      }
    }
  }
  else if (tag == nsGkAtoms::treeitem ||
           tag == nsGkAtoms::treeseparator
          ) {
    int32_t index = FindContent(aChild);
    if (index >= 0) {
      int32_t count = RemoveRow(index);
      if (mBoxObject)
        mBoxObject->RowCountChanged(index, -count);
    }
  }
  else if (tag == nsGkAtoms::treerow) {
    int32_t index = FindContent(aContainer);
    if (index >= 0 && mBoxObject)
      mBoxObject->InvalidateRow(index);
  }
  else if (tag == nsGkAtoms::treecell) {
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
                             int32_t* aIndex, nsTArray<nsAutoPtr<Row> >& aRows)
{
  // Don't allow non-XUL nodes.
  if (!aContent->IsXUL())
    return;

  dom::FlattenedChildIterator iter(aContent);
  for (nsIContent* content = iter.GetNextChild(); content; content = iter.GetNextChild()) {
    nsIAtom *tag = content->Tag();
    int32_t count = aRows.Length();

    if (content->IsXUL()) {
      if (tag == nsGkAtoms::treeitem)
        SerializeItem(content, aParentIndex, aIndex, aRows);
      else if (tag == nsGkAtoms::treeseparator)
        SerializeSeparator(content, aParentIndex, aIndex, aRows);
    }
    *aIndex += aRows.Length() - count;
  }
}

void
nsTreeContentView::SerializeItem(nsIContent* aContent, int32_t aParentIndex,
                                 int32_t* aIndex, nsTArray<nsAutoPtr<Row> >& aRows)
{
  if (aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::hidden,
                            nsGkAtoms::_true, eCaseMatters))
    return;

  Row* row = new Row(aContent, aParentIndex);
  aRows.AppendElement(row);

  if (aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::container,
                            nsGkAtoms::_true, eCaseMatters)) {
    row->SetContainer(true);
    if (aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::open,
                              nsGkAtoms::_true, eCaseMatters)) {
      row->SetOpen(true);
      nsIContent* child =
        nsTreeUtils::GetImmediateChild(aContent, nsGkAtoms::treechildren);
      if (child && child->IsXUL()) {
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
nsTreeContentView::SerializeSeparator(nsIContent* aContent,
                                      int32_t aParentIndex, int32_t* aIndex,
                                      nsTArray<nsAutoPtr<Row> >& aRows)
{
  if (aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::hidden,
                            nsGkAtoms::_true, eCaseMatters))
    return;

  Row* row = new Row(aContent, aParentIndex);
  row->SetSeparator(true);
  aRows.AppendElement(row);
}

void
nsTreeContentView::GetIndexInSubtree(nsIContent* aContainer,
                                     nsIContent* aContent, int32_t* aIndex)
{
  uint32_t childCount = aContainer->GetChildCount();
  
  if (!aContainer->IsXUL())
    return;

  for (uint32_t i = 0; i < childCount; i++) {
    nsIContent *content = aContainer->GetChildAt(i);

    if (content == aContent)
      break;

    nsIAtom *tag = content->Tag();

    if (content->IsXUL()) {
      if (tag == nsGkAtoms::treeitem) {
        if (! content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::hidden,
                                   nsGkAtoms::_true, eCaseMatters)) {
          (*aIndex)++;
          if (content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::container,
                                   nsGkAtoms::_true, eCaseMatters) &&
              content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::open,
                                   nsGkAtoms::_true, eCaseMatters)) {
            nsIContent* child =
              nsTreeUtils::GetImmediateChild(content, nsGkAtoms::treechildren);
            if (child && child->IsXUL())
              GetIndexInSubtree(child, aContent, aIndex);
          }
        }
      }
      else if (tag == nsGkAtoms::treeseparator) {
        if (! content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::hidden,
                                   nsGkAtoms::_true, eCaseMatters))
          (*aIndex)++;
      }
    }
  }
}

int32_t
nsTreeContentView::EnsureSubtree(int32_t aIndex)
{
  Row* row = mRows[aIndex];

  nsIContent* child;
  child = nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treechildren);
  if (!child || !child->IsXUL()) {
    return 0;
  }

  nsAutoTArray<nsAutoPtr<Row>, 8> rows;
  int32_t index = 0;
  Serialize(child, aIndex, &index, rows);
  // We can't use InsertElementsAt since the destination can't steal
  // ownership from its const source argument.
  for (nsTArray<Row>::index_type i = 0; i < rows.Length(); i++) {
    nsAutoPtr<Row>* newRow = mRows.InsertElementAt(aIndex + i + 1);
    *newRow = rows[i];
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
  Row* row = mRows[aIndex];
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
  nsIAtom* grandParentTag = grandParent->Tag();

  if (grandParent->IsXUL() && grandParentTag == nsGkAtoms::tree) {
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
  nsAutoTArray<nsAutoPtr<Row>, 8> rows;
  nsIAtom *tag = aContent->Tag();
  if (aContent->IsXUL()) {
    if (tag == nsGkAtoms::treeitem)
      SerializeItem(aContent, aParentIndex, &aIndex, rows);
    else if (tag == nsGkAtoms::treeseparator)
      SerializeSeparator(aContent, aParentIndex, &aIndex, rows);
  }

  // We can't use InsertElementsAt since the destination can't steal
  // ownership from its const source argument.
  for (nsTArray<Row>::index_type i = 0; i < rows.Length(); i++) {
    nsAutoPtr<Row>* newRow = mRows.InsertElementAt(aParentIndex + aIndex + i + 1);
    *newRow = rows[i];
  }
  int32_t count = rows.Length();

  UpdateSubtreeSizes(aParentIndex, count);

  // Update parent indexes, but skip added rows.
  // They already have correct values.
  UpdateParentIndexes(aParentIndex + aIndex, count + 1, count);

  return count;
}

int32_t
nsTreeContentView::RemoveRow(int32_t aIndex)
{
  Row* row = mRows[aIndex];
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
  Row* row = mRows[aIndex];
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
  Row* row = mRows[aIndex];
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
    Row* row = mRows[aParentIndex];
    row->mSubtreeSize += count;
    aParentIndex = row->mParentIndex;
  }
}

void
nsTreeContentView::UpdateParentIndexes(int32_t aIndex, int32_t aSkip, int32_t aCount)
{
  int32_t count = mRows.Length();
  for (int32_t i = aIndex + aSkip; i < count; i++) {
    Row* row = mRows[i];
    if (row->mParentIndex > aIndex) {
      row->mParentIndex += aCount;
    }
  }
}

nsIContent*
nsTreeContentView::GetCell(nsIContent* aContainer, nsITreeColumn* aCol)
{
  nsCOMPtr<nsIAtom> colAtom;
  int32_t colIndex;
  aCol->GetAtom(getter_AddRefs(colAtom));
  aCol->GetIndex(&colIndex);

  // Traverse through cells, try to find the cell by "ref" attribute or by cell
  // index in a row. "ref" attribute has higher priority.
  nsIContent* result = nullptr;
  int32_t j = 0;
  dom::FlattenedChildIterator iter(aContainer);
  for (nsIContent* cell = iter.GetNextChild(); cell; cell = iter.GetNextChild()) {
    if (cell->Tag() == nsGkAtoms::treecell) {
      if (colAtom && cell->AttrValueIs(kNameSpaceID_None, nsGkAtoms::ref,
                                       colAtom, eCaseMatters)) {
        result = cell;
        break;
      }
      else if (j == colIndex) {
        result = cell;
      }
      j++;
    }
  }

  return result;
}
