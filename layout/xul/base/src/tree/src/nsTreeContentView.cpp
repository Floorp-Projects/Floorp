/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsINameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsIBoxObject.h"
#include "nsTreeUtils.h"
#include "nsTreeContentView.h"
#include "nsChildIterator.h"
#include "nsDOMClassInfoID.h"
#include "nsEventStates.h"
#include "nsINodeInfo.h"
#include "nsIXULSortService.h"
#include "nsContentUtils.h"
#include "nsTreeBodyFrame.h"
#include "mozilla/dom/Element.h"
#include "nsServiceManagerUtils.h"

namespace dom = mozilla::dom;

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
    static Row*
    Create(nsFixedSizeAllocator& aAllocator,
           nsIContent* aContent, PRInt32 aParentIndex) {
      void* place = aAllocator.Alloc(sizeof(Row));
      return place ? ::new(place) Row(aContent, aParentIndex) : nsnull;
    }

    static void
    Destroy(nsFixedSizeAllocator& aAllocator, Row* aRow) {
      aRow->~Row();
      aAllocator.Free(aRow, sizeof(*aRow));
    }

    Row(nsIContent* aContent, PRInt32 aParentIndex)
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
    PRInt32             mParentIndex;

    // Subtree size for this item.
    PRInt32             mSubtreeSize;

  private:
    // Hide so that only Create() and Destroy() can be used to
    // allocate and deallocate from the heap
    static void* operator new(size_t) CPP_THROW_NEW { return 0; } 
    static void operator delete(void*, size_t) {}

    // State flags
    PRInt8		mFlags;
};


// We don't reference count the reference to the document
// If the document goes away first, we'll be informed and we
// can drop our reference.
// If we go away first, we'll get rid of ourselves from the
// document's observer list.

nsTreeContentView::nsTreeContentView(void) :
  mBoxObject(nsnull),
  mSelection(nsnull),
  mRoot(nsnull),
  mDocument(nsnull)
{
  static const size_t kBucketSizes[] = {
    sizeof(Row)
  };
  static const PRInt32 kNumBuckets = sizeof(kBucketSizes) / sizeof(size_t);
  static const PRInt32 kInitialSize = 16;

  mAllocator.Init("nsTreeContentView", kBucketSizes, kNumBuckets, kInitialSize);
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
nsTreeContentView::GetRowCount(PRInt32* aRowCount)
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
  if (nsContentUtils::IsCallerTrustedForWrite())
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
nsTreeContentView::GetRowProperties(PRInt32 aIndex, nsISupportsArray* aProperties)
{
  NS_ENSURE_ARG_POINTER(aProperties);
  NS_PRECONDITION(aIndex >= 0 && aIndex < PRInt32(mRows.Length()), "bad index");
  if (aIndex < 0 || aIndex >= PRInt32(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  Row* row = mRows[aIndex];
  nsIContent* realRow;
  if (row->IsSeparator())
    realRow = row->mContent;
  else
    realRow = nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treerow);

  if (realRow) {
    nsAutoString properties;
    realRow->GetAttr(kNameSpaceID_None, nsGkAtoms::properties, properties);
    if (!properties.IsEmpty())
      nsTreeUtils::TokenizeProperties(properties, aProperties);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::GetCellProperties(PRInt32 aRow, nsITreeColumn* aCol, nsISupportsArray* aProperties)
{
  NS_ENSURE_NATIVE_COLUMN(aCol);
  NS_ENSURE_ARG_POINTER(aProperties);
  NS_PRECONDITION(aRow >= 0 && aRow < PRInt32(mRows.Length()), "bad row");
  if (aRow < 0 || aRow >= PRInt32(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  Row* row = mRows[aRow];
  nsIContent* realRow =
    nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treerow);
  if (realRow) {
    nsIContent* cell = GetCell(realRow, aCol);
    if (cell) {
      nsAutoString properties;
      cell->GetAttr(kNameSpaceID_None, nsGkAtoms::properties, properties);
      if (!properties.IsEmpty())
        nsTreeUtils::TokenizeProperties(properties, aProperties);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::GetColumnProperties(nsITreeColumn* aCol, nsISupportsArray* aProperties)
{
  NS_ENSURE_NATIVE_COLUMN(aCol);
  NS_ENSURE_ARG_POINTER(aProperties);
  nsCOMPtr<nsIDOMElement> element;
  aCol->GetElement(getter_AddRefs(element));

  nsAutoString properties;
  element->GetAttribute(NS_LITERAL_STRING("properties"), properties);

  if (!properties.IsEmpty())
    nsTreeUtils::TokenizeProperties(properties, aProperties);

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::IsContainer(PRInt32 aIndex, bool* _retval)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < PRInt32(mRows.Length()), "bad index");
  if (aIndex < 0 || aIndex >= PRInt32(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  *_retval = mRows[aIndex]->IsContainer();

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::IsContainerOpen(PRInt32 aIndex, bool* _retval)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < PRInt32(mRows.Length()), "bad index");
  if (aIndex < 0 || aIndex >= PRInt32(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  *_retval = mRows[aIndex]->IsOpen();

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::IsContainerEmpty(PRInt32 aIndex, bool* _retval)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < PRInt32(mRows.Length()), "bad index");
  if (aIndex < 0 || aIndex >= PRInt32(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  *_retval = mRows[aIndex]->IsEmpty();

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::IsSeparator(PRInt32 aIndex, bool *_retval)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < PRInt32(mRows.Length()), "bad index");
  if (aIndex < 0 || aIndex >= PRInt32(mRows.Length()))
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
nsTreeContentView::CanDrop(PRInt32 aIndex, PRInt32 aOrientation,
                           nsIDOMDataTransfer* aDataTransfer, bool *_retval)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < PRInt32(mRows.Length()), "bad index");
  if (aIndex < 0 || aIndex >= PRInt32(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  *_retval = false;
 
  return NS_OK;
}
 
NS_IMETHODIMP
nsTreeContentView::Drop(PRInt32 aRow, PRInt32 aOrientation, nsIDOMDataTransfer* aDataTransfer)
{
  NS_PRECONDITION(aRow >= 0 && aRow < PRInt32(mRows.Length()), "bad row");
  if (aRow < 0 || aRow >= PRInt32(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::GetParentIndex(PRInt32 aRowIndex, PRInt32* _retval)
{
  NS_PRECONDITION(aRowIndex >= 0 && aRowIndex < PRInt32(mRows.Length()),
                  "bad row index");
  if (aRowIndex < 0 || aRowIndex >= PRInt32(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  *_retval = mRows[aRowIndex]->mParentIndex;

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::HasNextSibling(PRInt32 aRowIndex, PRInt32 aAfterIndex, bool* _retval)
{
  NS_PRECONDITION(aRowIndex >= 0 && aRowIndex < PRInt32(mRows.Length()),
                  "bad row index");
  if (aRowIndex < 0 || aRowIndex >= PRInt32(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  // We have a next sibling if the row is not the last in the subtree.
  PRInt32 parentIndex = mRows[aRowIndex]->mParentIndex;
  if (parentIndex >= 0) {
    // Compute the last index in this subtree.
    PRInt32 lastIndex = parentIndex + (mRows[parentIndex])->mSubtreeSize;
    Row* row = mRows[lastIndex];
    while (row->mParentIndex != parentIndex) {
      lastIndex = row->mParentIndex;
      row = mRows[lastIndex];
    }

    *_retval = aRowIndex < lastIndex;
  }
  else {
    *_retval = PRUint32(aRowIndex) < mRows.Length() - 1;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::GetLevel(PRInt32 aIndex, PRInt32* _retval)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < PRInt32(mRows.Length()), "bad index");
  if (aIndex < 0 || aIndex >= PRInt32(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  PRInt32 level = 0;
  Row* row = mRows[aIndex];
  while (row->mParentIndex >= 0) {
    level++;
    row = mRows[row->mParentIndex];
  }
  *_retval = level;

  return NS_OK;
}

 NS_IMETHODIMP
nsTreeContentView::GetImageSrc(PRInt32 aRow, nsITreeColumn* aCol, nsAString& _retval)
{
  _retval.Truncate();
  NS_ENSURE_NATIVE_COLUMN(aCol);
  NS_PRECONDITION(aRow >= 0 && aRow < PRInt32(mRows.Length()), "bad row");
  if (aRow < 0 || aRow >= PRInt32(mRows.Length()))
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
nsTreeContentView::GetProgressMode(PRInt32 aRow, nsITreeColumn* aCol, PRInt32* _retval)
{
  NS_ENSURE_NATIVE_COLUMN(aCol);
  NS_PRECONDITION(aRow >= 0 && aRow < PRInt32(mRows.Length()), "bad row");
  if (aRow < 0 || aRow >= PRInt32(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  *_retval = nsITreeView::PROGRESS_NONE;

  Row* row = mRows[aRow];

  nsIContent* realRow =
    nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treerow);
  if (realRow) {
    nsIContent* cell = GetCell(realRow, aCol);
    if (cell) {
      static nsIContent::AttrValuesArray strings[] =
        {&nsGkAtoms::normal, &nsGkAtoms::undetermined, nsnull};
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
nsTreeContentView::GetCellValue(PRInt32 aRow, nsITreeColumn* aCol, nsAString& _retval)
{
  _retval.Truncate();
  NS_ENSURE_NATIVE_COLUMN(aCol);
  NS_PRECONDITION(aRow >= 0 && aRow < PRInt32(mRows.Length()), "bad row");
  if (aRow < 0 || aRow >= PRInt32(mRows.Length()))
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
nsTreeContentView::GetCellText(PRInt32 aRow, nsITreeColumn* aCol, nsAString& _retval)
{
  _retval.Truncate();
  NS_ENSURE_NATIVE_COLUMN(aCol);
  NS_PRECONDITION(aRow >= 0 && aRow < PRInt32(mRows.Length()), "bad row");
  NS_PRECONDITION(aCol, "bad column");

  if (aRow < 0 || aRow >= PRInt32(mRows.Length()) || !aCol)
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

  if (aTree && !mRoot) {
    // Get our root element
    nsCOMPtr<nsIBoxObject> boxObject = do_QueryInterface(mBoxObject);
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
      PRInt32 index = 0;
      Serialize(mBody, -1, &index, mRows);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::ToggleOpenState(PRInt32 aIndex)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < PRInt32(mRows.Length()), "bad index");
  if (aIndex < 0 || aIndex >= PRInt32(mRows.Length()))
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
          {&nsGkAtoms::ascending, &nsGkAtoms::descending, nsnull};
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
nsTreeContentView::CycleCell(PRInt32 aRow, nsITreeColumn* aCol)
{
  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::IsEditable(PRInt32 aRow, nsITreeColumn* aCol, bool* _retval)
{
  *_retval = false;
  NS_ENSURE_NATIVE_COLUMN(aCol);
  NS_PRECONDITION(aRow >= 0 && aRow < PRInt32(mRows.Length()), "bad row");
  if (aRow < 0 || aRow >= PRInt32(mRows.Length()))
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
nsTreeContentView::IsSelectable(PRInt32 aRow, nsITreeColumn* aCol, bool* _retval)
{
  NS_ENSURE_NATIVE_COLUMN(aCol);
  NS_PRECONDITION(aRow >= 0 && aRow < PRInt32(mRows.Length()), "bad row");
  if (aRow < 0 || aRow >= PRInt32(mRows.Length()))
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
nsTreeContentView::SetCellValue(PRInt32 aRow, nsITreeColumn* aCol, const nsAString& aValue)
{
  NS_ENSURE_NATIVE_COLUMN(aCol);
  NS_PRECONDITION(aRow >= 0 && aRow < PRInt32(mRows.Length()), "bad row");
  if (aRow < 0 || aRow >= PRInt32(mRows.Length()))
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
nsTreeContentView::SetCellText(PRInt32 aRow, nsITreeColumn* aCol, const nsAString& aValue)
{
  NS_ENSURE_NATIVE_COLUMN(aCol);
  NS_PRECONDITION(aRow >= 0 && aRow < PRInt32(mRows.Length()), "bad row");
  if (aRow < 0 || aRow >= PRInt32(mRows.Length()))
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
nsTreeContentView::PerformAction(const PRUnichar* aAction)
{
  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::PerformActionOnRow(const PRUnichar* aAction, PRInt32 aRow)
{
  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::PerformActionOnCell(const PRUnichar* aAction, PRInt32 aRow, nsITreeColumn* aCol)
{
  return NS_OK;
}


NS_IMETHODIMP
nsTreeContentView::GetItemAtIndex(PRInt32 aIndex, nsIDOMElement** _retval)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < PRInt32(mRows.Length()), "bad index");
  if (aIndex < 0 || aIndex >= PRInt32(mRows.Length()))
    return NS_ERROR_INVALID_ARG;   

  Row* row = mRows[aIndex];
  row->mContent->QueryInterface(NS_GET_IID(nsIDOMElement), (void**)_retval);

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::GetIndexOfItem(nsIDOMElement* aItem, PRInt32* _retval)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aItem);
  *_retval = FindContent(content);

  return NS_OK;
}

void
nsTreeContentView::AttributeChanged(nsIDocument*  aDocument,
                                    dom::Element* aElement,
                                    PRInt32       aNameSpaceID,
                                    nsIAtom*      aAttribute,
                                    PRInt32       aModType)
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
  nsIContent* parent = nsnull;
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
 
    PRInt32 index = FindContent(aElement);
    if (hidden && index >= 0) {
      // Hide this row along with its children.
      PRInt32 count = RemoveRow(index);
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
    PRInt32 index = FindContent(aElement);
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
    PRInt32 index = FindContent(aElement);
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
        PRInt32 index = FindContent(parent);
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
          PRInt32 index = FindContent(grandParent);
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
                                   PRInt32     /* unused */)
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
                                   PRInt32 /* unused */)
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
    PRInt32 index = FindContent(aContainer);
    if (index >= 0) {
      Row* row = mRows[index];
      row->SetEmpty(false);
      if (mBoxObject)
        mBoxObject->InvalidateRow(index);
      if (row->IsContainer() && row->IsOpen()) {
        PRInt32 count = EnsureSubtree(index);
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
    PRInt32 index = FindContent(aContainer);
    if (index >= 0 && mBoxObject)
      mBoxObject->InvalidateRow(index);
  }
  else if (childTag == nsGkAtoms::treecell) {
    nsCOMPtr<nsIContent> parent = aContainer->GetParent();
    if (parent) {
      PRInt32 index = FindContent(parent);
      if (index >= 0 && mBoxObject)
        mBoxObject->InvalidateRow(index);
    }
  }
}

void
nsTreeContentView::ContentRemoved(nsIDocument *aDocument,
                                  nsIContent* aContainer,
                                  nsIContent* aChild,
                                  PRInt32 aIndexInContainer,
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
    PRInt32 index = FindContent(aContainer);
    if (index >= 0) {
      Row* row = mRows[index];
      row->SetEmpty(true);
      PRInt32 count = RemoveSubtree(index);
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
    PRInt32 index = FindContent(aChild);
    if (index >= 0) {
      PRInt32 count = RemoveRow(index);
      if (mBoxObject)
        mBoxObject->RowCountChanged(index, -count);
    }
  }
  else if (tag == nsGkAtoms::treerow) {
    PRInt32 index = FindContent(aContainer);
    if (index >= 0 && mBoxObject)
      mBoxObject->InvalidateRow(index);
  }
  else if (tag == nsGkAtoms::treecell) {
    nsCOMPtr<nsIContent> parent = aContainer->GetParent();
    if (parent) {
      PRInt32 index = FindContent(parent);
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
nsTreeContentView::Serialize(nsIContent* aContent, PRInt32 aParentIndex,
                             PRInt32* aIndex, nsTArray<Row*>& aRows)
{
  // Don't allow non-XUL nodes.
  if (!aContent->IsXUL())
    return;

  ChildIterator iter, last;
  for (ChildIterator::Init(aContent, &iter, &last); iter != last; ++iter) {
    nsIContent* content = *iter;
    nsIAtom *tag = content->Tag();
    PRInt32 count = aRows.Length();

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
nsTreeContentView::SerializeItem(nsIContent* aContent, PRInt32 aParentIndex,
                                 PRInt32* aIndex, nsTArray<Row*>& aRows)
{
  if (aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::hidden,
                            nsGkAtoms::_true, eCaseMatters))
    return;

  Row* row = Row::Create(mAllocator, aContent, aParentIndex);
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
        PRInt32 count = aRows.Length();
        PRInt32 index = 0;
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
                                      PRInt32 aParentIndex, PRInt32* aIndex,
                                      nsTArray<Row*>& aRows)
{
  if (aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::hidden,
                            nsGkAtoms::_true, eCaseMatters))
    return;

  Row* row = Row::Create(mAllocator, aContent, aParentIndex);
  row->SetSeparator(true);
  aRows.AppendElement(row);
}

void
nsTreeContentView::GetIndexInSubtree(nsIContent* aContainer,
                                     nsIContent* aContent, PRInt32* aIndex)
{
  PRUint32 childCount = aContainer->GetChildCount();
  
  if (!aContainer->IsXUL())
    return;

  for (PRUint32 i = 0; i < childCount; i++) {
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

PRInt32
nsTreeContentView::EnsureSubtree(PRInt32 aIndex)
{
  Row* row = mRows[aIndex];

  nsIContent* child;
  child = nsTreeUtils::GetImmediateChild(row->mContent, nsGkAtoms::treechildren);
  if (!child || !child->IsXUL()) {
    return 0;
  }

  nsAutoTArray<Row*, 8> rows;
  PRInt32 index = 0;
  Serialize(child, aIndex, &index, rows);
  mRows.InsertElementsAt(aIndex + 1, rows);
  PRInt32 count = rows.Length();

  row->mSubtreeSize += count;
  UpdateSubtreeSizes(row->mParentIndex, count);

  // Update parent indexes, but skip newly added rows.
  // They already have correct values.
  UpdateParentIndexes(aIndex, count + 1, count);

  return count;
}

PRInt32
nsTreeContentView::RemoveSubtree(PRInt32 aIndex)
{
  Row* row = mRows[aIndex];
  PRInt32 count = row->mSubtreeSize;

  for(PRInt32 i = 0; i < count; i++) {
    Row* nextRow = mRows[aIndex + i + 1];
    Row::Destroy(mAllocator, nextRow);
  }
  mRows.RemoveElementsAt(aIndex + 1, count);

  row->mSubtreeSize -= count;
  UpdateSubtreeSizes(row->mParentIndex, -count);

  UpdateParentIndexes(aIndex, 0, -count);

  return count;
}

void
nsTreeContentView::InsertRowFor(nsIContent* aParent, nsIContent* aChild)
{
  PRInt32 grandParentIndex = -1;
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
    PRInt32 index = 0;
    GetIndexInSubtree(aParent, aChild, &index);

    PRInt32 count = InsertRow(grandParentIndex, index, aChild);
    if (mBoxObject)
      mBoxObject->RowCountChanged(grandParentIndex + index + 1, count);
  }
}

PRInt32
nsTreeContentView::InsertRow(PRInt32 aParentIndex, PRInt32 aIndex, nsIContent* aContent)
{
  nsAutoTArray<Row*, 8> rows;
  nsIAtom *tag = aContent->Tag();
  if (aContent->IsXUL()) {
    if (tag == nsGkAtoms::treeitem)
      SerializeItem(aContent, aParentIndex, &aIndex, rows);
    else if (tag == nsGkAtoms::treeseparator)
      SerializeSeparator(aContent, aParentIndex, &aIndex, rows);
  }

  mRows.InsertElementsAt(aParentIndex + aIndex + 1, rows);
  PRInt32 count = rows.Length();

  UpdateSubtreeSizes(aParentIndex, count);

  // Update parent indexes, but skip added rows.
  // They already have correct values.
  UpdateParentIndexes(aParentIndex + aIndex, count + 1, count);

  return count;
}

PRInt32
nsTreeContentView::RemoveRow(PRInt32 aIndex)
{
  Row* row = mRows[aIndex];
  PRInt32 count = row->mSubtreeSize + 1;
  PRInt32 parentIndex = row->mParentIndex;

  Row::Destroy(mAllocator, row);
  for(PRInt32 i = 1; i < count; i++) {
    Row* nextRow = mRows[aIndex + i];
    Row::Destroy(mAllocator, nextRow);
  }
  mRows.RemoveElementsAt(aIndex, count);

  UpdateSubtreeSizes(parentIndex, -count);
  
  UpdateParentIndexes(aIndex, 0, -count);

  return count;
}

void
nsTreeContentView::ClearRows()
{
  for (PRUint32 i = 0; i < mRows.Length(); i++)
    Row::Destroy(mAllocator, mRows[i]);
  mRows.Clear();
  mRoot = nsnull;
  mBody = nsnull;
  // Remove ourselves from mDocument's observers.
  if (mDocument) {
    mDocument->RemoveObserver(this);
    mDocument = nsnull;
  }
} 

void
nsTreeContentView::OpenContainer(PRInt32 aIndex)
{
  Row* row = mRows[aIndex];
  row->SetOpen(true);

  PRInt32 count = EnsureSubtree(aIndex);
  if (mBoxObject) {
    mBoxObject->InvalidateRow(aIndex);
    mBoxObject->RowCountChanged(aIndex + 1, count);
  }
}

void
nsTreeContentView::CloseContainer(PRInt32 aIndex)
{
  Row* row = mRows[aIndex];
  row->SetOpen(false);

  PRInt32 count = RemoveSubtree(aIndex);
  if (mBoxObject) {
    mBoxObject->InvalidateRow(aIndex);
    mBoxObject->RowCountChanged(aIndex + 1, -count);
  }
}

PRInt32
nsTreeContentView::FindContent(nsIContent* aContent)
{
  for (PRUint32 i = 0; i < mRows.Length(); i++) {
    if (mRows[i]->mContent == aContent) {
      return i;
    }
  }

  return -1;
}

void
nsTreeContentView::UpdateSubtreeSizes(PRInt32 aParentIndex, PRInt32 count)
{
  while (aParentIndex >= 0) {
    Row* row = mRows[aParentIndex];
    row->mSubtreeSize += count;
    aParentIndex = row->mParentIndex;
  }
}

void
nsTreeContentView::UpdateParentIndexes(PRInt32 aIndex, PRInt32 aSkip, PRInt32 aCount)
{
  PRInt32 count = mRows.Length();
  for (PRInt32 i = aIndex + aSkip; i < count; i++) {
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
  PRInt32 colIndex;
  aCol->GetAtom(getter_AddRefs(colAtom));
  aCol->GetIndex(&colIndex);

  // Traverse through cells, try to find the cell by "ref" attribute or by cell
  // index in a row. "ref" attribute has higher priority.
  nsIContent* result = nsnull;
  PRInt32 j = 0;
  ChildIterator iter, last;
  for (ChildIterator::Init(aContainer, &iter, &last); iter != last; ++iter) {
    nsIContent* cell = *iter;

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
