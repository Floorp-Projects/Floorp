/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Jan Varga.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Ryner <bryner@brianryner.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsINameSpaceManager.h"
#include "nsHTMLAtoms.h"
#include "nsXULAtoms.h"
#include "nsIBoxObject.h"
#include "nsTreeUtils.h"
#include "nsTreeContentView.h"
#include "nsChildIterator.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIDOMClassInfo.h"
#include "nsIEventStateManager.h"
#include "nsINodeInfo.h"

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

    void SetContainer(PRBool aContainer) {
      aContainer ? mFlags |= ROW_FLAG_CONTAINER : mFlags &= ~ROW_FLAG_CONTAINER;
    }
    PRBool IsContainer() { return mFlags & ROW_FLAG_CONTAINER; }

    void SetOpen(PRBool aOpen) {
      aOpen ? mFlags |= ROW_FLAG_OPEN : mFlags &= ~ROW_FLAG_OPEN;
    }
    PRBool IsOpen() { return mFlags & ROW_FLAG_OPEN; }

    void SetEmpty(PRBool aEmpty) {
      aEmpty ? mFlags |= ROW_FLAG_EMPTY : mFlags &= ~ROW_FLAG_EMPTY;
    }
    PRBool IsEmpty() { return mFlags & ROW_FLAG_EMPTY; }

    void SetSeparator(PRBool aSeparator) {
      aSeparator ? mFlags |= ROW_FLAG_SEPARATOR : mFlags &= ~ROW_FLAG_SEPARATOR;
    }
    PRBool IsSeparator() { return mFlags & ROW_FLAG_SEPARATOR; }

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
  mDocument(nsnull),
  mUpdateSelection(PR_FALSE)
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
NS_NewTreeContentView(nsITreeContentView** aResult)
{
  *aResult = new nsTreeContentView;
  if (! *aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMPL_ADDREF(nsTreeContentView)
NS_IMPL_RELEASE(nsTreeContentView)

NS_INTERFACE_MAP_BEGIN(nsTreeContentView)
  NS_INTERFACE_MAP_ENTRY(nsITreeView)
  NS_INTERFACE_MAP_ENTRY(nsITreeContentView)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsITreeContentView)
  NS_INTERFACE_MAP_ENTRY_DOM_CLASSINFO(TreeContentView)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsTreeContentView::GetRowCount(PRInt32* aRowCount)
{
  *aRowCount = mRows.Count();

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::GetSelection(nsITreeSelection** aSelection)
{
  NS_IF_ADDREF(*aSelection = mSelection);

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::SetSelection(nsITreeSelection* aSelection)
{
  mSelection = aSelection;

  if (mUpdateSelection) {
    mUpdateSelection = PR_FALSE;

    mSelection->SetSelectEventsSuppressed(PR_TRUE);
    for (PRInt32 i = 0; i < mRows.Count(); ++i) {
      Row* row = (Row*)mRows[i];
      nsCOMPtr<nsIDOMHTMLOptionElement> optEl = do_QueryInterface(row->mContent);
      if (optEl) {
        PRBool isSelected;
        optEl->GetSelected(&isSelected);
        if (isSelected)
          mSelection->ToggleSelect(i);
      }
    }
    mSelection->SetSelectEventsSuppressed(PR_FALSE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::GetRowProperties(PRInt32 aIndex, nsISupportsArray* aProperties)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad index");
  if (aIndex < 0 || aIndex >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  Row* row = (Row*)mRows[aIndex];
  nsCOMPtr<nsIContent> realRow;
  if (row->IsSeparator())
    realRow = row->mContent;
  else
    nsTreeUtils::GetImmediateChild(row->mContent, nsXULAtoms::treerow, getter_AddRefs(realRow));

  if (realRow) {
    nsAutoString properties;
    realRow->GetAttr(kNameSpaceID_None, nsXULAtoms::properties, properties);
    if (!properties.IsEmpty())
      nsTreeUtils::TokenizeProperties(properties, aProperties);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::GetCellProperties(PRInt32 aRow, nsITreeColumn* aCol, nsISupportsArray* aProperties)
{
  NS_PRECONDITION(aRow >= 0 && aRow < mRows.Count(), "bad row");
  if (aRow < 0 || aRow >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  Row* row = (Row*)mRows[aRow];
  nsCOMPtr<nsIContent> realRow;
  nsTreeUtils::GetImmediateChild(row->mContent, nsXULAtoms::treerow, getter_AddRefs(realRow));
  if (realRow) {
    nsIContent* cell = GetCell(realRow, aCol);
    if (cell) {
      nsAutoString properties;
      cell->GetAttr(kNameSpaceID_None, nsXULAtoms::properties, properties);
      if (!properties.IsEmpty())
        nsTreeUtils::TokenizeProperties(properties, aProperties);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::GetColumnProperties(nsITreeColumn* aCol, nsISupportsArray* aProperties)
{
  nsCOMPtr<nsIDOMElement> element;
  aCol->GetElement(getter_AddRefs(element));

  nsAutoString properties;
  element->GetAttribute(NS_LITERAL_STRING("properties"), properties);

  if (!properties.IsEmpty())
    nsTreeUtils::TokenizeProperties(properties, aProperties);

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::IsContainer(PRInt32 aIndex, PRBool* _retval)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad index");
  if (aIndex < 0 || aIndex >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  *_retval = ((Row*)mRows[aIndex])->IsContainer();

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::IsContainerOpen(PRInt32 aIndex, PRBool* _retval)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad index");
  if (aIndex < 0 || aIndex >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  *_retval = ((Row*)mRows[aIndex])->IsOpen();

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::IsContainerEmpty(PRInt32 aIndex, PRBool* _retval)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad index");
  if (aIndex < 0 || aIndex >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  *_retval = ((Row*)mRows[aIndex])->IsEmpty();

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::IsSeparator(PRInt32 aIndex, PRBool *_retval)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad index");
  if (aIndex < 0 || aIndex >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  *_retval = ((Row*)mRows[aIndex])->IsSeparator();

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::IsSorted(PRBool *_retval)
{
  *_retval = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::CanDrop(PRInt32 aIndex, PRInt32 aOrientation, PRBool *_retval)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad index");
  if (aIndex < 0 || aIndex >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  *_retval = PR_FALSE;
 
  return NS_OK;
}
 
NS_IMETHODIMP
nsTreeContentView::Drop(PRInt32 aRow, PRInt32 aOrientation)
{
  NS_PRECONDITION(aRow >= 0 && aRow < mRows.Count(), "bad row");
  if (aRow < 0 || aRow >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::GetParentIndex(PRInt32 aRowIndex, PRInt32* _retval)
{
  NS_PRECONDITION(aRowIndex >= 0 && aRowIndex < mRows.Count(), "bad row index");
  if (aRowIndex < 0 || aRowIndex >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  *_retval = ((Row*)mRows[aRowIndex])->mParentIndex;

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::HasNextSibling(PRInt32 aRowIndex, PRInt32 aAfterIndex, PRBool* _retval)
{
  NS_PRECONDITION(aRowIndex >= 0 && aRowIndex < mRows.Count(), "bad row index");
  if (aRowIndex < 0 || aRowIndex >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  // We have a next sibling if the row is not the last in the subtree.
  PRInt32 parentIndex = ((Row*)mRows[aRowIndex])->mParentIndex;
  if (parentIndex >= 0) {
    // Compute the last index in this subtree.
    PRInt32 lastIndex = parentIndex + ((Row*)mRows[parentIndex])->mSubtreeSize;
    Row* row = (Row*)mRows[lastIndex];
    while (row->mParentIndex != parentIndex) {
      lastIndex = row->mParentIndex;
      row = (Row*)mRows[lastIndex];
    }

    *_retval = aRowIndex < lastIndex;
  }
  else {
    *_retval = aRowIndex < mRows.Count() - 1;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::GetLevel(PRInt32 aIndex, PRInt32* _retval)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad index");
  if (aIndex < 0 || aIndex >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  PRInt32 level = 0;
  Row* row = (Row*)mRows[aIndex];
  while (row->mParentIndex >= 0) {
    level++;
    row = (Row*)mRows[row->mParentIndex];
  }
  *_retval = level;

  return NS_OK;
}

 NS_IMETHODIMP
nsTreeContentView::GetImageSrc(PRInt32 aRow, nsITreeColumn* aCol, nsAString& _retval)
{
  NS_PRECONDITION(aRow >= 0 && aRow < mRows.Count(), "bad row");
  if (aRow < 0 || aRow >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  _retval.SetCapacity(0);

  Row* row = (Row*)mRows[aRow];

  nsCOMPtr<nsIContent> realRow;
  nsTreeUtils::GetImmediateChild(row->mContent, nsXULAtoms::treerow, getter_AddRefs(realRow));
  if (realRow) {
    nsIContent* cell = GetCell(realRow, aCol);
    if (cell)
      cell->GetAttr(kNameSpaceID_None, nsHTMLAtoms::src, _retval);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::GetProgressMode(PRInt32 aRow, nsITreeColumn* aCol, PRInt32* _retval)
{
  NS_PRECONDITION(aRow >= 0 && aRow < mRows.Count(), "bad row");
  if (aRow < 0 || aRow >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  *_retval = nsITreeView::PROGRESS_NONE;

  Row* row = (Row*)mRows[aRow];

  nsCOMPtr<nsIContent> realRow;
  nsTreeUtils::GetImmediateChild(row->mContent, nsXULAtoms::treerow, getter_AddRefs(realRow));
  if (realRow) {
    nsIContent* cell = GetCell(realRow, aCol);
    if (cell) {
      nsAutoString state;
      cell->GetAttr(kNameSpaceID_None, nsXULAtoms::mode, state);
      if (state.EqualsLiteral("normal"))
        *_retval = nsITreeView::PROGRESS_NORMAL;
      else if (state.EqualsLiteral("undetermined"))
        *_retval = nsITreeView::PROGRESS_UNDETERMINED;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::GetCellValue(PRInt32 aRow, nsITreeColumn* aCol, nsAString& _retval)
{
  NS_PRECONDITION(aRow >= 0 && aRow < mRows.Count(), "bad row");
  if (aRow < 0 || aRow >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  _retval.SetCapacity(0);

  Row* row = (Row*)mRows[aRow];

  nsCOMPtr<nsIContent> realRow;
  nsTreeUtils::GetImmediateChild(row->mContent, nsXULAtoms::treerow, getter_AddRefs(realRow));
  if (realRow) {
    nsIContent* cell = GetCell(realRow, aCol);
    if (cell)
      cell->GetAttr(kNameSpaceID_None, nsHTMLAtoms::value, _retval);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::GetCellText(PRInt32 aRow, nsITreeColumn* aCol, nsAString& _retval)
{
  NS_PRECONDITION(aRow >= 0 && aRow < mRows.Count(), "bad row");
  if (aRow < 0 || aRow >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  _retval.SetCapacity(0);

  Row* row = (Row*)mRows[aRow];

  // Check for a "label" attribute - this is valid on an <treeitem>
  // or an <option>, with a single implied column.
  if (row->mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::label, _retval)
      && !_retval.IsEmpty())
    return NS_OK;

  nsIAtom *rowTag = row->mContent->Tag();
  if (rowTag == nsHTMLAtoms::option &&
      row->mContent->IsContentOfType(nsIContent::eHTML)) {
    // Use the text node child as the label
    nsCOMPtr<nsIDOMHTMLOptionElement> elem = do_QueryInterface(row->mContent);
    elem->GetText(_retval);
  }
  else if (rowTag == nsHTMLAtoms::optgroup &&
           row->mContent->IsContentOfType(nsIContent::eHTML)) {
    nsCOMPtr<nsIDOMHTMLOptGroupElement> elem = do_QueryInterface(row->mContent);
    elem->GetLabel(_retval);
  }
  else if (rowTag == nsXULAtoms::treeitem &&
           row->mContent->IsContentOfType(nsIContent::eXUL)) {
    nsCOMPtr<nsIContent> realRow;
    nsTreeUtils::GetImmediateChild(row->mContent, nsXULAtoms::treerow,
                                   getter_AddRefs(realRow));
    if (realRow) {
      nsIContent* cell = GetCell(realRow, aCol);
      if (cell)
        cell->GetAttr(kNameSpaceID_None, nsHTMLAtoms::label, _retval);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::SetTree(nsITreeBoxObject* aTree)
{
  mBoxObject = aTree;

  if (aTree && !mRoot) {
    // Get our root element
    nsCOMPtr<nsIBoxObject> boxObject = do_QueryInterface(mBoxObject);
    nsCOMPtr<nsIDOMElement> element;
    boxObject->GetElement(getter_AddRefs(element));

    mRoot = do_QueryInterface(element);

    // Add ourselves to document's observers.
    nsIDocument* document = mRoot->GetDocument();
    if (document) {
      document->AddObserver(this);
      mDocument = document;
    }

    nsCOMPtr<nsIDOMElement> bodyElement;
    mBoxObject->GetTreeBody(getter_AddRefs(bodyElement));
    if (bodyElement) {
      nsCOMPtr<nsIContent> bodyContent = do_QueryInterface(bodyElement);
      PRInt32 index = 0;
      Serialize(bodyContent, -1, &index, mRows);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::ToggleOpenState(PRInt32 aIndex)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad index");
  if (aIndex < 0 || aIndex >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  // We don't serialize content right here, since content might be generated
  // lazily.
  Row* row = (Row*)mRows[aIndex];

  if (row->mContent->Tag() == nsHTMLAtoms::optgroup &&
      row->mContent->IsContentOfType(nsIContent::eHTML)) {
    // we don't use an attribute for optgroup's open state
    if (row->IsOpen())
      CloseContainer(aIndex);
    else
      OpenContainer(aIndex);
  }
  else {
    if (row->IsOpen())
      row->mContent->SetAttr(kNameSpaceID_None, nsXULAtoms::open, NS_LITERAL_STRING("false"), PR_TRUE);
    else
      row->mContent->SetAttr(kNameSpaceID_None, nsXULAtoms::open, NS_LITERAL_STRING("true"), PR_TRUE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::CycleHeader(nsITreeColumn* aCol)
{
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
nsTreeContentView::IsEditable(PRInt32 aRow, nsITreeColumn* aCol, PRBool* _retval)
{
  NS_PRECONDITION(aRow >= 0 && aRow < mRows.Count(), "bad row");
  if (aRow < 0 || aRow >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  *_retval = PR_TRUE;

  Row* row = (Row*)mRows[aRow];

  nsCOMPtr<nsIContent> realRow;
  nsTreeUtils::GetImmediateChild(row->mContent, nsXULAtoms::treerow, getter_AddRefs(realRow));
  if (realRow) {
    nsIContent* cell = GetCell(realRow, aCol);
    if (cell) {
      nsAutoString editable;
      cell->GetAttr(kNameSpaceID_None, nsXULAtoms::editable, editable);
      if (editable.EqualsLiteral("false"))
        *_retval = PR_FALSE;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::SetCellValue(PRInt32 aRow, nsITreeColumn* aCol, const nsAString& aValue)
{
  NS_PRECONDITION(aRow >= 0 && aRow < mRows.Count(), "bad row");
  if (aRow < 0 || aRow >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  Row* row = (Row*)mRows[aRow];

  nsCOMPtr<nsIContent> realRow;
  nsTreeUtils::GetImmediateChild(row->mContent, nsXULAtoms::treerow, getter_AddRefs(realRow));
  if (realRow) {
    nsIContent* cell = GetCell(realRow, aCol);
    if (cell)
      cell->SetAttr(kNameSpaceID_None, nsHTMLAtoms::value, aValue, PR_TRUE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTreeContentView::SetCellText(PRInt32 aRow, nsITreeColumn* aCol, const nsAString& aValue)
{
  NS_PRECONDITION(aRow >= 0 && aRow < mRows.Count(), "bad row");
  if (aRow < 0 || aRow >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  Row* row = (Row*)mRows[aRow];

  nsCOMPtr<nsIContent> realRow;
  nsTreeUtils::GetImmediateChild(row->mContent, nsXULAtoms::treerow, getter_AddRefs(realRow));
  if (realRow) {
    nsIContent* cell = GetCell(realRow, aCol);
    if (cell)
      cell->SetAttr(kNameSpaceID_None, nsHTMLAtoms::label, aValue, PR_TRUE);
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
  NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad index");
  if (aIndex < 0 || aIndex >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  Row* row = (Row*)mRows[aIndex];
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
nsTreeContentView::ContentStatesChanged(nsIDocument* aDocument,
                                        nsIContent* aContent1,
                                        nsIContent* aContent2,
                                        PRInt32 aStateMask)
{
  if (!aContent1 || !mSelection ||
      !aContent1->IsContentOfType(nsIContent::eHTML) ||
      !(aStateMask & NS_EVENT_STATE_CHECKED))
    return;

  if (aContent1->Tag() == nsHTMLAtoms::option) {
    // update the selected state for this node
    PRInt32 index = FindContent(aContent1);
    if (index >= 0)
      mSelection->ToggleSelect(index);
  }
}

void
nsTreeContentView::AttributeChanged(nsIDocument *aDocument,
                                    nsIContent*  aContent,
                                    PRInt32      aNameSpaceID,
                                    nsIAtom*     aAttribute,
                                    PRInt32      aModType)
{
  // Make sure this notification concerns us.
  // First check the tag to see if it's one that we care about.
  nsIAtom *tag = aContent->Tag();

  if (aContent->IsContentOfType(nsIContent::eXUL)) {
    if (tag != nsXULAtoms::treecol &&
        tag != nsXULAtoms::treeitem &&
        tag != nsXULAtoms::treeseparator &&
        tag != nsXULAtoms::treerow &&
        tag != nsXULAtoms::treecell)
      return;
  }
  else {
    return;
  }

  // If we have a legal tag, go up to the tree and make sure that it's ours.
  nsCOMPtr<nsIContent> parent = aContent;
  nsINodeInfo *ni = nsnull;
  do {
    parent = parent->GetParent();
    if (parent)
      ni = parent->GetNodeInfo();
  } while (parent && !ni->Equals(nsXULAtoms::tree, kNameSpaceID_XUL));

  if (parent != mRoot) {
    // This is not for us, we can bail out.
    return;
  }

  // Handle changes of the hidden attribute.
  if (aAttribute == nsHTMLAtoms::hidden &&
     (tag == nsXULAtoms::treeitem || tag == nsXULAtoms::treeseparator)) {
    nsAutoString hiddenString;
    aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::hidden, hiddenString);
    PRBool hidden = hiddenString.EqualsLiteral("true");
 
    PRInt32 index = FindContent(aContent);
    if (hidden && index >= 0) {
      // Hide this row along with its children.
      PRInt32 count = RemoveRow(index);
      if (mBoxObject)
        mBoxObject->RowCountChanged(index, -count);
    }
    else if (!hidden && index < 0) {
      // Show this row along with its children.
      nsCOMPtr<nsIContent> parent = aContent->GetParent();
      if (parent) {
        InsertRowFor(parent, aContent);
      }
    }

    return;
  }

  if (tag == nsXULAtoms::treecol) {
    if (aAttribute == nsXULAtoms::properties) {
      if (mBoxObject) {
        nsCOMPtr<nsITreeColumns> cols;
        mBoxObject->GetColumns(getter_AddRefs(cols));
        if (cols) {
          nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aContent);
          nsCOMPtr<nsITreeColumn> col;
          cols->GetColumnFor(element, getter_AddRefs(col));
          mBoxObject->InvalidateColumn(col);
        }
      }
    }
  }
  else if (tag == nsXULAtoms::treeitem) {
    PRInt32 index = FindContent(aContent);
    if (index >= 0) {
      Row* row = (Row*)mRows[index];
      if (aAttribute == nsXULAtoms::container) {
        nsAutoString container;
        aContent->GetAttr(kNameSpaceID_None, nsXULAtoms::container, container);
        PRBool isContainer = container.EqualsLiteral("true");
        row->SetContainer(isContainer);
        if (mBoxObject)
          mBoxObject->InvalidateRow(index);
      }
      else if (aAttribute == nsXULAtoms::open) {
        nsAutoString open;
        aContent->GetAttr(kNameSpaceID_None, nsXULAtoms::open, open);
        PRBool isOpen = open.EqualsLiteral("true");
        PRBool wasOpen = row->IsOpen();
        if (! isOpen && wasOpen)
          CloseContainer(index);
        else if (isOpen && ! wasOpen)
          OpenContainer(index);
      }
      else if (aAttribute == nsXULAtoms::empty) {
        nsAutoString empty;
        aContent->GetAttr(kNameSpaceID_None, nsXULAtoms::empty, empty);
        PRBool isEmpty = empty.EqualsLiteral("true");
        row->SetEmpty(isEmpty);
        if (mBoxObject)
          mBoxObject->InvalidateRow(index);
      }
    }
  }
  else if (tag == nsXULAtoms::treeseparator) {
    PRInt32 index = FindContent(aContent);
    if (index >= 0) {
      if (aAttribute == nsXULAtoms::properties && mBoxObject) {
        mBoxObject->InvalidateRow(index);
      }
    }
  }
  else if (tag == nsXULAtoms::treerow) {
    if (aAttribute == nsXULAtoms::properties) {
      nsCOMPtr<nsIContent> parent = aContent->GetParent();
      if (parent) {
        PRInt32 index = FindContent(parent);
        if (index >= 0 && mBoxObject) {
          mBoxObject->InvalidateRow(index);
        }
      }
    }
  }
  else if (tag == nsXULAtoms::treecell) {
    if (aAttribute == nsXULAtoms::ref ||
        aAttribute == nsXULAtoms::properties ||
        aAttribute == nsXULAtoms::mode ||
        aAttribute == nsHTMLAtoms::src ||
        aAttribute == nsHTMLAtoms::value ||
        aAttribute == nsHTMLAtoms::label) {
      nsIContent* parent = aContent->GetParent();
      if (parent) {
        nsCOMPtr<nsIContent> grandParent = parent->GetParent();
        if (grandParent) {
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
                                   PRInt32     aNewIndexInContainer)
{
  nsIContent *child = aContainer->GetChildAt(aNewIndexInContainer);
  ContentInserted(aDocument, aContainer, child, aNewIndexInContainer);
}

void
nsTreeContentView::ContentInserted(nsIDocument *aDocument,
                                   nsIContent* aContainer,
                                   nsIContent* aChild,
                                   PRInt32 aIndexInContainer)
{
  NS_ASSERTION(aChild, "null ptr");

  // Make sure this notification concerns us.
  // First check the tag to see if it's one that we care about.
  nsIAtom *childTag = aChild->Tag();

  if (aChild->IsContentOfType(nsIContent::eHTML)) {
    if (childTag != nsHTMLAtoms::option &&
        childTag != nsHTMLAtoms::optgroup)
      return;
  }
  else if (aChild->IsContentOfType(nsIContent::eXUL)) {
    if (childTag != nsXULAtoms::treeitem &&
        childTag != nsXULAtoms::treeseparator &&
        childTag != nsXULAtoms::treechildren &&
        childTag != nsXULAtoms::treerow &&
        childTag != nsXULAtoms::treecell)
      return;
  }
  else {
    return;
  }

  // If we have a legal tag, go up to the tree/select and make sure
  // that it's ours.

  for (nsIContent* element = aContainer; element; element = element->GetParent()) {
    nsIAtom *parentTag = element->Tag();
    if ((element->IsContentOfType(nsIContent::eXUL) && parentTag == nsXULAtoms::tree) ||
        (element->IsContentOfType(nsIContent::eHTML) && parentTag == nsHTMLAtoms::select)) {
      if (element == mRoot) // this is for us, stop looking
        break;
      else // this is not for us, we can bail out
        return;
    }
  }

  if (childTag == nsXULAtoms::treechildren) {
    PRInt32 index = FindContent(aContainer);
    if (index >= 0) {
      Row* row = (Row*)mRows[index];
      row->SetEmpty(PR_FALSE);
      if (mBoxObject)
        mBoxObject->InvalidateRow(index);
      if (row->IsContainer() && row->IsOpen()) {
        PRInt32 count = EnsureSubtree(index);
        if (mBoxObject)
          mBoxObject->RowCountChanged(index + 1, count);
      }
    }
  }
  else if (childTag == nsXULAtoms::treeitem ||
           childTag == nsXULAtoms::treeseparator) {
    InsertRowFor(aContainer, aChild);
  }
  else if (childTag == nsXULAtoms::treerow) {
    PRInt32 index = FindContent(aContainer);
    if (index >= 0 && mBoxObject)
      mBoxObject->InvalidateRow(index);
  }
  else if (childTag == nsXULAtoms::treecell) {
    nsCOMPtr<nsIContent> parent = aContainer->GetParent();
    if (parent) {
      PRInt32 index = FindContent(parent);
      if (index >= 0 && mBoxObject)
        mBoxObject->InvalidateRow(index);
    }
  }
  else if (childTag == nsHTMLAtoms::optgroup) {
    InsertRowFor(aContainer, aChild);
  }
  else if (childTag == nsHTMLAtoms::option) {
    PRInt32 parentIndex = FindContent(aContainer);
    PRInt32 count = InsertRow(parentIndex, aIndexInContainer, aChild);
    if (mBoxObject)
      mBoxObject->RowCountChanged(parentIndex + aIndexInContainer + 1, count);
  }
}

void
nsTreeContentView::ContentRemoved(nsIDocument *aDocument,
                                     nsIContent* aContainer,
                                     nsIContent* aChild,
                                     PRInt32 aIndexInContainer)
{
  NS_ASSERTION(aChild, "null ptr");

  // Make sure this notification concerns us.
  // First check the tag to see if it's one that we care about.
  nsIAtom *tag = aChild->Tag();

  if (aChild->IsContentOfType(nsIContent::eHTML)) {
    if (tag != nsHTMLAtoms::option &&
        tag != nsHTMLAtoms::optgroup)
      return;
  }
  else if (aChild->IsContentOfType(nsIContent::eXUL)) {
    if (tag != nsXULAtoms::treeitem &&
        tag != nsXULAtoms::treeseparator &&
        tag != nsXULAtoms::treechildren &&
        tag != nsXULAtoms::treerow &&
        tag != nsXULAtoms::treecell)
      return;
  }
  else {
    return;
  }

  // If we have a legal tag, go up to the tree/select and make sure
  // that it's ours.
  for (nsIContent* element = aContainer; element; element = element->GetParent()) {
    nsIAtom *parentTag = element->Tag();
    if ((element->IsContentOfType(nsIContent::eXUL) && parentTag == nsXULAtoms::tree) || 
        (element->IsContentOfType(nsIContent::eHTML) && parentTag == nsHTMLAtoms::select)) {
      if (element == mRoot) // this is for us, stop looking
        break;
      else // this is not for us, we can bail out
        return;
    }
  }

  if (tag == nsXULAtoms::treechildren) {
    PRInt32 index = FindContent(aContainer);
    if (index >= 0) {
      Row* row = (Row*)mRows[index];
      row->SetEmpty(PR_TRUE);
      PRInt32 count = RemoveSubtree(index);
      // Invalidate also the row to update twisty.
      if (mBoxObject) {
        mBoxObject->InvalidateRow(index);
        mBoxObject->RowCountChanged(index + 1, -count);
      }
    }
    else if (aContainer->Tag() == nsXULAtoms::tree) {
      PRInt32 count = mRows.Count();
      ClearRows();
      if (count && mBoxObject)
        mBoxObject->RowCountChanged(0, -count);
    }
  }
  else if (tag == nsXULAtoms::treeitem ||
           tag == nsXULAtoms::treeseparator ||
           tag == nsHTMLAtoms::option ||
           tag == nsHTMLAtoms::optgroup
          ) {
    PRInt32 index = FindContent(aChild);
    if (index >= 0) {
      PRInt32 count = RemoveRow(index);
      if (mBoxObject)
        mBoxObject->RowCountChanged(index, -count);
    }
  }
  else if (tag == nsXULAtoms::treerow) {
    PRInt32 index = FindContent(aContainer);
    if (index >= 0 && mBoxObject)
      mBoxObject->InvalidateRow(index);
  }
  else if (tag == nsXULAtoms::treecell) {
    nsCOMPtr<nsIContent> parent = aContainer->GetParent();
    if (parent) {
      PRInt32 index = FindContent(parent);
      if (index >= 0 && mBoxObject)
        mBoxObject->InvalidateRow(index);
    }
  }
}

void
nsTreeContentView::DocumentWillBeDestroyed(nsIDocument *aDocument)
{
  // Remove ourselves from mDocument's observers.
  if (mDocument) {
    mDocument->RemoveObserver(this);
    mDocument = nsnull;
  }

  ClearRows();
}


// Recursively serialize content, starting with aContent.
void
nsTreeContentView::Serialize(nsIContent* aContent, PRInt32 aParentIndex, PRInt32* aIndex, nsVoidArray& aRows)
{
  ChildIterator iter, last;
  for (ChildIterator::Init(aContent, &iter, &last); iter != last; ++iter) {
    nsCOMPtr<nsIContent> content = *iter;
    nsIAtom *tag = content->Tag();
    PRInt32 count = aRows.Count();

    if (content->IsContentOfType(nsIContent::eXUL)) {
      if (tag == nsXULAtoms::treeitem)
        SerializeItem(content, aParentIndex, aIndex, aRows);
      else if (tag == nsXULAtoms::treeseparator)
        SerializeSeparator(content, aParentIndex, aIndex, aRows);
    }
    else if (content->IsContentOfType(nsIContent::eHTML)) {
      if (tag == nsHTMLAtoms::option)
        SerializeOption(content, aParentIndex, aIndex, aRows);
      else if (tag == nsHTMLAtoms::optgroup)
        SerializeOptGroup(content, aParentIndex, aIndex, aRows);
    }
    *aIndex += aRows.Count() - count;
  }
}

void
nsTreeContentView::SerializeItem(nsIContent* aContent, PRInt32 aParentIndex, PRInt32* aIndex, nsVoidArray& aRows)
{
  nsAutoString hidden;
  aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::hidden, hidden);
  if (hidden.EqualsLiteral("true"))
    return;

  Row* row = Row::Create(mAllocator, aContent, aParentIndex);
  aRows.AppendElement(row);

  nsAutoString container;
  aContent->GetAttr(kNameSpaceID_None, nsXULAtoms::container, container);
  if (container.EqualsLiteral("true")) {
    row->SetContainer(PR_TRUE);
    nsAutoString open;
    aContent->GetAttr(kNameSpaceID_None, nsXULAtoms::open, open);
    if (open.EqualsLiteral("true")) {
      row->SetOpen(PR_TRUE);
      nsCOMPtr<nsIContent> child;
      nsTreeUtils::GetImmediateChild(aContent, nsXULAtoms::treechildren, getter_AddRefs(child));
      if (child) {
        // Now, recursively serialize our child.
        PRInt32 count = aRows.Count();
        PRInt32 index = 0;
        Serialize(child, aParentIndex + *aIndex + 1, &index, aRows);
        row->mSubtreeSize += aRows.Count() - count;
      }
      else
        row->SetEmpty(PR_TRUE);
    } else {
      nsAutoString empty;
      aContent->GetAttr(kNameSpaceID_None, nsXULAtoms::empty, empty);
      if (empty.EqualsLiteral("true"))
        row->SetEmpty(PR_TRUE);
    }
  } 
}

void
nsTreeContentView::SerializeSeparator(nsIContent* aContent, PRInt32 aParentIndex, PRInt32* aIndex, nsVoidArray& aRows)
{
  nsAutoString hidden;
  aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::hidden, hidden);
  if (hidden.EqualsLiteral("true"))
    return;

  Row* row = Row::Create(mAllocator, aContent, aParentIndex);
  row->SetSeparator(PR_TRUE);
  aRows.AppendElement(row);
}

void
nsTreeContentView::SerializeOption(nsIContent* aContent, PRInt32 aParentIndex,
                                   PRInt32* aIndex, nsVoidArray& aRows)
{
  Row* row = Row::Create(mAllocator, aContent, aParentIndex);
  aRows.AppendElement(row);

  // This will happen before the TreeSelection is hooked up.  So, cache the selected
  // state in the row properties and update the selection when it is attached.

  nsCOMPtr<nsIDOMHTMLOptionElement> optEl = do_QueryInterface(aContent);
  PRBool isSelected;
  optEl->GetSelected(&isSelected);
  if (isSelected)
    mUpdateSelection = PR_TRUE;
}

void
nsTreeContentView::SerializeOptGroup(nsIContent* aContent, PRInt32 aParentIndex,
                                     PRInt32* aIndex, nsVoidArray& aRows)
{
  Row* row = Row::Create(mAllocator, aContent, aParentIndex);
  aRows.AppendElement(row);
  row->SetContainer(PR_TRUE);
  row->SetOpen(PR_TRUE);

  nsCOMPtr<nsIContent> child;
  nsTreeUtils::GetImmediateChild(aContent, nsHTMLAtoms::option, getter_AddRefs(child));
  if (child) {
    // Now, recursively serialize our child.
    PRInt32 count = aRows.Count();
    PRInt32 index = 0;
    Serialize(aContent, aParentIndex + *aIndex + 1, &index, aRows);
    row->mSubtreeSize += aRows.Count() - count;
  }
  else
    row->SetEmpty(PR_TRUE);
}

void
nsTreeContentView::GetIndexInSubtree(nsIContent* aContainer,
                                     nsIContent* aContent, PRInt32* aIndex)
{
  PRUint32 childCount = aContainer->GetChildCount();
  for (PRUint32 i = 0; i < childCount; i++) {
    nsIContent *content = aContainer->GetChildAt(i);

    if (content == aContent)
      break;

    nsIAtom *tag = content->Tag();

    if (content->IsContentOfType(nsIContent::eXUL)) {
      if (tag == nsXULAtoms::treeitem) {
        nsAutoString hidden;
        content->GetAttr(kNameSpaceID_None, nsHTMLAtoms::hidden, hidden);
        if (! hidden.EqualsLiteral("true")) {
          (*aIndex)++;
          nsAutoString container;
          content->GetAttr(kNameSpaceID_None, nsXULAtoms::container, container);
          if (container.EqualsLiteral("true")) {
            nsAutoString open;
            content->GetAttr(kNameSpaceID_None, nsXULAtoms::open, open);
            if (open.EqualsLiteral("true")) {
              nsCOMPtr<nsIContent> child;
              nsTreeUtils::GetImmediateChild(content, nsXULAtoms::treechildren, getter_AddRefs(child));
              if (child)
                GetIndexInSubtree(child, aContent, aIndex);
            }
          }
        }
      }
      else if (tag == nsXULAtoms::treeseparator) {
        nsAutoString hidden;
        content->GetAttr(kNameSpaceID_None, nsHTMLAtoms::hidden, hidden);
        if (! hidden.EqualsLiteral("true"))
          (*aIndex)++;
      }
    }
    else if (content->IsContentOfType(nsIContent::eHTML)) {
      if (tag == nsHTMLAtoms::optgroup) {
        (*aIndex)++;
        GetIndexInSubtree(content, aContent, aIndex);
      }
      else if (tag == nsHTMLAtoms::option)
        (*aIndex)++;
    }
  }
}

PRInt32
nsTreeContentView::EnsureSubtree(PRInt32 aIndex)
{
  Row* row = (Row*)mRows[aIndex];

  nsCOMPtr<nsIContent> child;
  if (row->mContent->Tag() == nsHTMLAtoms::optgroup)
    child = row->mContent;
  else {
    nsTreeUtils::GetImmediateChild(row->mContent, nsXULAtoms::treechildren, getter_AddRefs(child));
    if (! child) {
      return 0;
    }
  }

  nsAutoVoidArray rows;
  PRInt32 index = 0;
  Serialize(child, aIndex, &index, rows);
  mRows.InsertElementsAt(rows, aIndex + 1);
  PRInt32 count = rows.Count();

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
  Row* row = (Row*)mRows[aIndex];
  PRInt32 count = row->mSubtreeSize;

  for(PRInt32 i = 0; i < count; i++) {
    Row* nextRow = (Row*)mRows[aIndex + i + 1];
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
  PRBool insertRow = PR_FALSE;

  nsCOMPtr<nsIContent> grandParent = aParent->GetParent();
  nsIAtom* grandParentTag = grandParent->Tag();

  if ((grandParent->IsContentOfType(nsIContent::eXUL) && grandParentTag == nsXULAtoms::tree) ||
      (grandParent->IsContentOfType(nsIContent::eHTML) && grandParentTag == nsHTMLAtoms::select)
     ) {
    // Allow insertion to the outermost container.
    insertRow = PR_TRUE;
  }
  else {
    // Test insertion to an inner container.

    // First try to find this parent in our array of rows, if we find one
    // we can be sure that all other parents are open too.
    grandParentIndex = FindContent(grandParent);
    if (grandParentIndex >= 0) {
      // Got it, now test if it is open.
      if (((Row*)mRows[grandParentIndex])->IsOpen())
        insertRow = PR_TRUE;
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
  nsAutoVoidArray rows;
  nsIAtom *tag = aContent->Tag();
  if (aContent->IsContentOfType(nsIContent::eXUL)) {
    if (tag == nsXULAtoms::treeitem)
      SerializeItem(aContent, aParentIndex, &aIndex, rows);
    else if (tag == nsXULAtoms::treeseparator)
      SerializeSeparator(aContent, aParentIndex, &aIndex, rows);
  }
  else if (aContent->IsContentOfType(nsIContent::eHTML)) {
    if (tag == nsHTMLAtoms::option)
      SerializeOption(aContent, aParentIndex, &aIndex, rows);
    else if (tag == nsHTMLAtoms::optgroup)
      SerializeOptGroup(aContent, aParentIndex, &aIndex, rows);
  }

  mRows.InsertElementsAt(rows, aParentIndex + aIndex + 1);
  PRInt32 count = rows.Count();

  UpdateSubtreeSizes(aParentIndex, count);

  // Update parent indexes, but skip added rows.
  // They already have correct values.
  UpdateParentIndexes(aParentIndex + aIndex, count + 1, count);

  return count;
}

PRInt32
nsTreeContentView::RemoveRow(PRInt32 aIndex)
{
  Row* row = (Row*)mRows[aIndex];
  PRInt32 count = row->mSubtreeSize + 1;
  PRInt32 parentIndex = row->mParentIndex;

  Row::Destroy(mAllocator, row);
  for(PRInt32 i = 1; i < count; i++) {
    Row* nextRow = (Row*)mRows[aIndex + i];
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
  for (PRInt32 i = 0; i < mRows.Count(); i++)
    Row::Destroy(mAllocator, (Row*)mRows[i]);
  mRows.Clear();
  mRoot = nsnull;
} 

void
nsTreeContentView::OpenContainer(PRInt32 aIndex)
{
  Row* row = (Row*)mRows[aIndex];
  row->SetOpen(PR_TRUE);

  PRInt32 count = EnsureSubtree(aIndex);
  if (mBoxObject) {
    mBoxObject->InvalidateRow(aIndex);
    mBoxObject->RowCountChanged(aIndex + 1, count);
  }
}

void
nsTreeContentView::CloseContainer(PRInt32 aIndex)
{
  Row* row = (Row*)mRows[aIndex];
  row->SetOpen(PR_FALSE);

  PRInt32 count = RemoveSubtree(aIndex);
  if (mBoxObject) {
    mBoxObject->InvalidateRow(aIndex);
    mBoxObject->RowCountChanged(aIndex + 1, -count);
  }
}

PRInt32
nsTreeContentView::FindContent(nsIContent* aContent)
{
  for (PRInt32 i = 0; i < mRows.Count(); i++) {
    if (((Row*)mRows[i])->mContent == aContent) {
      return i;
    }
  }

  return -1;
}

void
nsTreeContentView::UpdateSubtreeSizes(PRInt32 aParentIndex, PRInt32 count)
{
  while (aParentIndex >= 0) {
    Row* row = (Row*)mRows[aParentIndex];
    row->mSubtreeSize += count;
    aParentIndex = row->mParentIndex;
  }
}

void
nsTreeContentView::UpdateParentIndexes(PRInt32 aIndex, PRInt32 aSkip, PRInt32 aCount)
{
  PRInt32 count = mRows.Count();
  for (PRInt32 i = aIndex + aSkip; i < count; i++) {
    Row* row = (Row*)mRows[i];
    if (row->mParentIndex > aIndex) {
      row->mParentIndex += aCount;
    }
  }
}

nsIContent*
nsTreeContentView::GetCell(nsIContent* aContainer, nsITreeColumn* aCol)
{
  const PRUnichar* colID;
  PRInt32 colIndex;
  aCol->GetIdConst(&colID);
  aCol->GetIndex(&colIndex);

  // Traverse through cells, try to find the cell by "ref" attribute or by cell
  // index in a row. "ref" attribute has higher priority.
  nsIContent* result = nsnull;
  PRInt32 j = 0;
  ChildIterator iter, last;
  for (ChildIterator::Init(aContainer, &iter, &last); iter != last; ++iter) {
    nsCOMPtr<nsIContent> cell = *iter;

    if (cell->Tag() == nsXULAtoms::treecell) {
      nsAutoString ref;
      cell->GetAttr(kNameSpaceID_None, nsXULAtoms::ref, ref);
      if (!ref.IsEmpty() && ref.Equals(colID)) {
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
