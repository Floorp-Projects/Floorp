/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jan Varga (varga@utcru.sk)
 *   Brian Ryner <bryner@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsReadableUtils.h"
#include "nsINameSpaceManager.h"
#include "nsHTMLAtoms.h"
#include "nsXULAtoms.h"
#include "nsLayoutAtoms.h"
#include "nsIDOMDocument.h"
#include "nsIBoxObject.h"
#include "nsOutlinerUtils.h"
#include "nsOutlinerContentView.h"
#include "nsChildIterator.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMClassInfo.h"


// A content model view implementation for the outliner.


#define ROW_FLAG_CONTAINER      0x01
#define ROW_FLAG_OPEN           0x02
#define ROW_FLAG_EMPTY          0x04
#define ROW_FLAG_SEPARATOR      0x08

class Property
{
  public:
    Property(nsIAtom* aAtom)
      : mAtom(aAtom), mNext(nsnull) {
    }
    ~Property() {
      delete mNext;
    }

    nsCOMPtr<nsIAtom>   mAtom;
    Property*           mNext;
};

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
        mSubtreeSize(0), mProperty(nsnull), mFlags(0) {
    }

    ~Row() {
      delete mProperty;
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

    void SetProperty(Property* aProperty) {
      delete mProperty;
      mProperty = aProperty;
    }

    // Weak reference to a content item.
    nsIContent*         mContent;

    // The parent index of the item, set to -1 for the top level items.
    PRInt32             mParentIndex;

    // Subtree size for this item.
    PRInt32             mSubtreeSize;

    // List of parsed properties
    Property*           mProperty;

  private:
    // Hide so that only Create() and Destroy() can be used to
    // allocate and deallocate from the heap
    static void* operator new(size_t) { return 0; } 
    static void operator delete(void*, size_t) {}

    // State flags
    PRInt8		mFlags;
};


// We don't reference count the reference to the document
// If the document goes away first, we'll be informed and we
// can drop our reference.
// If we go away first, we'll get rid of ourselves from the
// document's observer list.

nsOutlinerContentView::nsOutlinerContentView(void) :
  mBoxObject(nsnull), mSelection(nsnull), mRoot(nsnull), mDocument(nsnull),
  mUpdateSelection(PR_FALSE)
{
  NS_INIT_ISUPPORTS();

  static const size_t kBucketSizes[] = {
    sizeof(Row)
  };
  static const PRInt32 kNumBuckets = sizeof(kBucketSizes) / sizeof(size_t);
  static const PRInt32 kInitialSize = 16;

  mAllocator.Init("nsOutlinerContentView", kBucketSizes, kNumBuckets, kInitialSize);
}

nsOutlinerContentView::~nsOutlinerContentView(void)
{
  // Remove ourselfs from document's observers.
  if (mDocument)
    mDocument->RemoveObserver(this);
}

nsresult
NS_NewOutlinerContentView(nsIOutlinerContentView** aResult)
{
  *aResult = new nsOutlinerContentView;
  if (! *aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMPL_ADDREF(nsOutlinerContentView)
NS_IMPL_RELEASE(nsOutlinerContentView)

NS_INTERFACE_MAP_BEGIN(nsOutlinerContentView)
  NS_INTERFACE_MAP_ENTRY(nsIOutlinerView)
  NS_INTERFACE_MAP_ENTRY(nsIOutlinerContentView)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIOutlinerContentView)
  NS_INTERFACE_MAP_ENTRY_DOM_CLASSINFO(OutlinerContentView)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsOutlinerContentView::GetRowCount(PRInt32* aRowCount)
{
  *aRowCount = mRows.Count();

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::GetSelection(nsIOutlinerSelection** aSelection)
{
  NS_IF_ADDREF(*aSelection = mSelection);

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::SetSelection(nsIOutlinerSelection* aSelection)
{
  mSelection = aSelection;
  if (mUpdateSelection) {
    mUpdateSelection = PR_FALSE;

    mSelection->SetSelectEventsSuppressed(PR_TRUE);
    for (PRInt32 i = 0; i < mRows.Count(); ++i) {
      Row* row = (Row*)mRows[i];
      if (row->mContent->HasAttr(kNameSpaceID_None, nsLayoutAtoms::optionSelectedPseudo))
        mSelection->ToggleSelect(i);
    }
    mSelection->SetSelectEventsSuppressed(PR_FALSE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::GetRowProperties(PRInt32 aIndex, nsISupportsArray* aProperties)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad index");
  if (aIndex < 0 || aIndex >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  Row* row = (Row*)mRows[aIndex];
  Property* property = row->mProperty;
  while (property) {
    aProperties->AppendElement(property->mAtom);
    property = property->mNext;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::GetCellProperties(PRInt32 aRow, const PRUnichar* aColID, nsISupportsArray* aProperties)
{
  NS_PRECONDITION(aRow >= 0 && aRow < mRows.Count(), "bad row");
  if (aRow < 0 || aRow >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  Row* row = (Row*)mRows[aRow];
  nsCOMPtr<nsIContent> realRow;
  nsOutlinerUtils::GetImmediateChild(row->mContent, nsXULAtoms::outlinerrow, getter_AddRefs(realRow));
  if (realRow) {
    nsCOMPtr<nsIContent> cell;
    GetNamedCell(realRow, aColID, getter_AddRefs(cell));
    if (cell) {
      nsAutoString properties;
      cell->GetAttr(kNameSpaceID_None, nsXULAtoms::properties, properties);
      if (properties.Length())
        nsOutlinerUtils::TokenizeProperties(properties, aProperties);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::GetColumnProperties(const PRUnichar* aColID, nsIDOMElement* aColElt, nsISupportsArray* aProperties)
{
  nsAutoString properties;
  aColElt->GetAttribute(NS_LITERAL_STRING("properties"), properties);
  if (properties.Length())
    nsOutlinerUtils::TokenizeProperties(properties, aProperties);

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::IsContainer(PRInt32 aIndex, PRBool* _retval)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad index");
  if (aIndex < 0 || aIndex >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  *_retval = ((Row*)mRows[aIndex])->IsContainer();

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::IsContainerOpen(PRInt32 aIndex, PRBool* _retval)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad index");
  if (aIndex < 0 || aIndex >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  *_retval = ((Row*)mRows[aIndex])->IsOpen();

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::IsContainerEmpty(PRInt32 aIndex, PRBool* _retval)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad index");
  if (aIndex < 0 || aIndex >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  *_retval = ((Row*)mRows[aIndex])->IsEmpty();

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::IsSeparator(PRInt32 aIndex, PRBool *_retval)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad index");
  if (aIndex < 0 || aIndex >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  *_retval = ((Row*)mRows[aIndex])->IsSeparator();

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::IsSorted(PRBool *_retval)
{
  *_retval = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::CanDropOn(PRInt32 aIndex, PRBool *_retval)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad index");
  if (aIndex < 0 || aIndex >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  *_retval = PR_FALSE;
 
  return NS_OK;
}
 
NS_IMETHODIMP
nsOutlinerContentView::CanDropBeforeAfter(PRInt32 aIndex, PRBool aBefore, PRBool* _retval)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad index");
  if (aIndex < 0 || aIndex >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  *_retval = PR_FALSE;
 
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::Drop(PRInt32 aRow, PRInt32 aOrientation)
{
  NS_PRECONDITION(aRow >= 0 && aRow < mRows.Count(), "bad row");
  if (aRow < 0 || aRow >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::GetParentIndex(PRInt32 aRowIndex, PRInt32* _retval)
{
  NS_PRECONDITION(aRowIndex >= 0 && aRowIndex < mRows.Count(), "bad row index");
  if (aRowIndex < 0 || aRowIndex >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  *_retval = ((Row*)mRows[aRowIndex])->mParentIndex;

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::HasNextSibling(PRInt32 aRowIndex, PRInt32 aAfterIndex, PRBool* _retval)
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
  else
    *_retval = aRowIndex < mRows.Count() - 1;

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::GetLevel(PRInt32 aIndex, PRInt32* _retval)
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
nsOutlinerContentView::GetCellText(PRInt32 aRow, const PRUnichar* aColID, nsAString& _retval)
{
  NS_PRECONDITION(aRow >= 0 && aRow < mRows.Count(), "bad row");
  if (aRow < 0 || aRow >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  _retval.SetCapacity(0);

  Row* row = (Row*)mRows[aRow];

  // Check for a "label" attribute - this is valid on an <outlineritem>
  // or an <option>, with a single implied column.
  if (NS_SUCCEEDED(row->mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::label, _retval))
      && !_retval.IsEmpty())
    return NS_OK;

  nsCOMPtr<nsIAtom> rowTag;
  row->mContent->GetTag(*getter_AddRefs(rowTag));
  if (rowTag == nsHTMLAtoms::option) {
    // Use the text node child as the label
    nsCOMPtr<nsIDOMHTMLOptionElement> elem = do_QueryInterface(row->mContent);
    elem->GetText(_retval);
    return NS_OK;
  } else if (rowTag == nsXULAtoms::outlineritem) {
    nsCOMPtr<nsIContent> realRow;
    nsOutlinerUtils::GetImmediateChild(row->mContent, nsXULAtoms::outlinerrow, getter_AddRefs(realRow));
    if (realRow) {
      nsCOMPtr<nsIContent> cell;
      GetNamedCell(realRow, aColID, getter_AddRefs(cell));
      if (cell)
        cell->GetAttr(kNameSpaceID_None, nsHTMLAtoms::label, _retval);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::SetOutliner(nsIOutlinerBoxObject* aOutliner)
{
  mBoxObject = aOutliner;

  if (!mRoot) {
    // Get our root element
    nsCOMPtr<nsIBoxObject> boxObject = do_QueryInterface(mBoxObject);
    nsCOMPtr<nsIDOMElement> element;
    boxObject->GetElement(getter_AddRefs(element));

    mRoot = do_QueryInterface(element);

    // Add ourselfs to document's observers.
    nsCOMPtr<nsIDocument> document;
    mRoot->GetDocument(*getter_AddRefs(document));
    if (document) {
      document->AddObserver(this);
      mDocument = document;
    }

    nsCOMPtr<nsIDOMElement> bodyElement;
    mBoxObject->GetOutlinerBody(getter_AddRefs(bodyElement));
    if (bodyElement) {
      nsCOMPtr<nsIContent> bodyContent = do_QueryInterface(bodyElement);
      PRInt32 index = 0;
      Serialize(bodyContent, -1, &index, mRows);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::ToggleOpenState(PRInt32 aIndex)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad index");
  if (aIndex < 0 || aIndex >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  // We don't serialize content right here, since content might be generated
  // lazily.
  Row* row = (Row*)mRows[aIndex];
  if (row->IsOpen())
    row->mContent->UnsetAttr(kNameSpaceID_None, nsXULAtoms::open, PR_TRUE);
  else
    row->mContent->SetAttr(kNameSpaceID_None, nsXULAtoms::open, NS_LITERAL_STRING("true"), PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::CycleHeader(const PRUnichar* aColID, nsIDOMElement *aColElt)
{
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::SelectionChanged()
{
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::CycleCell(PRInt32 aRow, const PRUnichar* aColID)
{
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::IsEditable(PRInt32 aRow, const PRUnichar* aColID, PRBool* _retval)
{
  *_retval = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::SetCellText(PRInt32 aRow, const PRUnichar* aColID, const PRUnichar* aValue)
{
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::PerformAction(const PRUnichar* aAction)
{
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::PerformActionOnRow(const PRUnichar* aAction, PRInt32 aRow)
{
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::PerformActionOnCell(const PRUnichar* aAction, PRInt32 aRowconst, const PRUnichar* aColID)
{
  return NS_OK;
}


NS_IMETHODIMP
nsOutlinerContentView::GetRoot(nsIDOMElement** aRoot)
{
  if (mRoot)
    mRoot->QueryInterface(NS_GET_IID(nsIDOMElement), (void**)aRoot);
  else
    *aRoot = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::GetItemAtIndex(PRInt32 aIndex, nsIDOMElement** _retval)
{
  NS_PRECONDITION(aIndex >= 0 && aIndex < mRows.Count(), "bad index");
  if (aIndex < 0 || aIndex >= mRows.Count())
    return NS_ERROR_INVALID_ARG;   

  Row* row = (Row*)mRows[aIndex];
  row->mContent->QueryInterface(NS_GET_IID(nsIDOMElement), (void**)_retval);

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::GetIndexOfItem(nsIDOMElement* aItem, PRInt32* _retval)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aItem);
  *_retval = FindContent(content);

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::BeginUpdate(nsIDocument *aDocument)
{
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::EndUpdate(nsIDocument *aDocument)
{
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::BeginLoad(nsIDocument *aDocument)
{
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::EndLoad(nsIDocument *aDocument)
{
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::BeginReflow(nsIDocument *aDocument, nsIPresShell* aShell)
{
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::EndReflow(nsIDocument *aDocument, nsIPresShell* aShell)
{
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::ContentChanged(nsIDocument *aDocument,
                                     nsIContent* aContent,
                                     nsISupports* aSubContent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::ContentStatesChanged(nsIDocument* aDocument,
                                           nsIContent* aContent1,
                                           nsIContent* aContent2)
{
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::AttributeChanged(nsIDocument *aDocument,
                                       nsIContent*  aContent,
                                       PRInt32      aNameSpaceID,
                                       nsIAtom*     aAttribute,
                                       PRInt32      aModType, 
                                       PRInt32      aHint)
{
  // First, get the element on which the attribute has changed
  // and then try to find content item in our array of rows.
  nsCOMPtr<nsIAtom> tag;
  aContent->GetTag(*getter_AddRefs(tag));

  if (tag == nsXULAtoms::outlinercol) {
    if (aAttribute == nsXULAtoms::properties) {
      nsAutoString id;
      aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::id, id);
      mBoxObject->InvalidateColumn(id.get());
    }
  }
  else if (tag == nsXULAtoms::outlineritem) {
    if (aAttribute == nsXULAtoms::open) {
      PRInt32 index = FindContent(aContent);
      if (index >= 0) {
        Row* row = (Row*)mRows[index];
        nsAutoString open;
        aContent->GetAttr(kNameSpaceID_None, nsXULAtoms::open, open);
        PRBool isOpen = open.Equals(NS_LITERAL_STRING("true"));
        PRBool wasOpen = row->IsOpen();
        if (! isOpen && wasOpen)
          CloseContainer(index);
        else if (isOpen && ! wasOpen)
          OpenContainer(index);
      }
    }
  }
  else if (tag == nsXULAtoms::outlinerseparator) {
    if (aAttribute == nsXULAtoms::properties) {
      PRInt32 index = FindContent(aContent);
      if (index >= 0) {
        Row* row = (Row*)mRows[index];
        row->SetProperty(nsnull);
        ParseProperties(aContent, &row->mProperty);
        mBoxObject->InvalidateRow(index);
      }
    }
  }
  else if (tag == nsXULAtoms::outlinerrow) {
    if (aAttribute == nsXULAtoms::properties) {
      nsCOMPtr<nsIContent> parent;
      aContent->GetParent(*getter_AddRefs(parent));
      if (parent) {
        PRInt32 index = FindContent(parent);
        if (index >= 0) {
          Row* row = (Row*)mRows[index];
          row->SetProperty(nsnull);
          ParseProperties(aContent, &row->mProperty);
          mBoxObject->InvalidateRow(index);
        }
      }
    }
  }
  else if (tag == nsXULAtoms::outlinercell) {
    if (aAttribute == nsXULAtoms::ref ||
        aAttribute == nsHTMLAtoms::label ||
        aAttribute == nsXULAtoms::properties) {
      nsCOMPtr<nsIContent> parent;
      aContent->GetParent(*getter_AddRefs(parent));
      if (parent) {
        nsCOMPtr<nsIContent> grandParent;
        parent->GetParent(*getter_AddRefs(grandParent));
        if (grandParent) {
          PRInt32 index = FindContent(grandParent);
          if (index >= 0) {
            // XXX Should we make an effort to invalidate only cell ?
            mBoxObject->InvalidateRow(index);
          }
        }
      }
    }
  }
  else if (tag == nsHTMLAtoms::option) {
    if (aAttribute == nsLayoutAtoms::optionSelectedPseudo) {
      PRInt32 index = FindContent(aContent);
      if (index >= 0) {
        NS_ASSERTION(mSelection, "Need to handle optionSelected change with no OutlinerSelection");
        if (mSelection)
          mSelection->ToggleSelect(index);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::ContentAppended(nsIDocument *aDocument,
                                      nsIContent* aContainer,
                                      PRInt32     aNewIndexInContainer)
{
  nsCOMPtr<nsIContent> child;
  aContainer->ChildAt(aNewIndexInContainer, *getter_AddRefs(child));
  ContentInserted(aDocument, aContainer, child, aNewIndexInContainer);

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::ContentInserted(nsIDocument *aDocument,
                                      nsIContent* aContainer,
                                      nsIContent* aChild,
                                      PRInt32 aIndexInContainer)
{
  // Make sure this notification concerns us.
  // First check the tag to see if it's one that we care about.
  nsCOMPtr<nsIAtom> childTag;
  aChild->GetTag(*getter_AddRefs(childTag));

  if ((childTag != nsXULAtoms::outlineritem) &&
      (childTag != nsXULAtoms::outlinerseparator) &&
      (childTag != nsHTMLAtoms::option) &&
      (childTag != nsXULAtoms::outlinerchildren) &&
      (childTag != nsXULAtoms::outlinerrow) &&
      (childTag != nsXULAtoms::outlinercell))
    return NS_OK;

  // If we have a legal tag, go up to the outliner/select and make sure
  // that it's ours.
  nsCOMPtr<nsIContent> element = aContainer;
  nsCOMPtr<nsIAtom> parentTag;
  
  while (element) {
    element->GetTag(*getter_AddRefs(parentTag));
    if (parentTag == nsXULAtoms::outliner || parentTag == nsHTMLAtoms::select)
      if (element == mRoot) // this is for us, stop looking
        break;
      else // this is not for us, we can bail out
        return NS_OK;
    nsCOMPtr<nsIContent> temp = element;
    temp->GetParent(*getter_AddRefs(element));
  }

  if (childTag == nsXULAtoms::outlineritem ||
      childTag == nsXULAtoms::outlinerseparator) {
    PRInt32 parentIndex = -1;

    nsCOMPtr<nsIContent> parent;
    aContainer->GetParent(*getter_AddRefs(parent));
    nsCOMPtr<nsIAtom> parentTag;
    parent->GetTag(*getter_AddRefs(parentTag));
    if (parentTag != nsXULAtoms::outliner)
      parentIndex = FindContent(parent);

    PRInt32 index = 0;
    GetIndexInSubtree(aContainer, aChild, &index);

    PRInt32 count;
    InsertRow(parentIndex, index, aChild, &count);
    mBoxObject->RowCountChanged(parentIndex + index + 1, count);
  }
  else if (childTag == nsHTMLAtoms::option) {
    PRInt32 count;
    PRInt32 parentIndex = FindContent(aContainer);
    InsertRow(parentIndex, aIndexInContainer, aChild, &count);
    mBoxObject->RowCountChanged(parentIndex + aIndexInContainer + 1, count);
  }
  else if (childTag == nsXULAtoms::outlinerchildren) {
    PRInt32 index = FindContent(aContainer);
    if (index >= 0) {
      Row* row = (Row*)mRows[index];
      row->SetEmpty(PR_FALSE);
      mBoxObject->InvalidateRow(index);
      if (row->IsContainer() && row->IsOpen()) {
        PRInt32 count;
        EnsureSubtree(index, &count);
        mBoxObject->RowCountChanged(index + 1, count);
      }
    }
  }
  else if (childTag == nsXULAtoms::outlinerrow) {
    PRInt32 index = FindContent(aContainer);
    if (index >= 0)
      mBoxObject->InvalidateRow(index);
  }
  else if (childTag == nsXULAtoms::outlinercell) {
    nsCOMPtr<nsIContent> parent;
    aContainer->GetParent(*getter_AddRefs(parent));
    if (parent) {
      PRInt32 index = FindContent(parent);
      if (index >= 0)
        mBoxObject->InvalidateRow(index);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::ContentReplaced(nsIDocument *aDocument,
                                      nsIContent* aContainer,
                                      nsIContent* aOldChild,
                                      nsIContent* aNewChild,
                                      PRInt32 aIndexInContainer)
{
  ContentRemoved(aDocument, aContainer, aOldChild, aIndexInContainer);
  ContentInserted(aDocument, aContainer, aNewChild, aIndexInContainer);

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::ContentRemoved(nsIDocument *aDocument,
                                     nsIContent* aContainer,
                                     nsIContent* aChild,
                                     PRInt32 aIndexInContainer)
{
  // Make sure this notification concerns us.
  // First check the tag to see if it's one that we care about.
  nsCOMPtr<nsIAtom> tag;
  aChild->GetTag(*getter_AddRefs(tag));

  if ((tag != nsXULAtoms::outlineritem) &&
      (tag != nsXULAtoms::outlinerseparator) &&
      (tag != nsHTMLAtoms::option) &&
      (tag != nsXULAtoms::outlinerchildren) &&
      (tag != nsXULAtoms::outlinerrow) &&
      (tag != nsXULAtoms::outlinercell))
    return NS_OK;

  // If we have a legal tag, go up to the outliner/select and make sure
  // that it's ours.
  nsCOMPtr<nsIContent> element = aContainer;
  nsCOMPtr<nsIAtom> parentTag;
  
  while (element) {
    element->GetTag(*getter_AddRefs(parentTag));
    if (parentTag == nsXULAtoms::outliner || parentTag == nsHTMLAtoms::select)
      if (element == mRoot) // this is for us, stop looking
        break;
      else // this is not for us, we can bail out
        return NS_OK;
    nsCOMPtr<nsIContent> temp = element;
    temp->GetParent(*getter_AddRefs(element));
  }

  if (tag == nsXULAtoms::outlineritem ||
      tag == nsXULAtoms::outlinerseparator ||
      tag == nsHTMLAtoms::option) {
    PRInt32 index = FindContent(aChild);
    if (index >= 0) {
      PRInt32 count;
      RemoveRow(index, &count);
      mBoxObject->RowCountChanged(index, -count);
    }
  }
  else if (tag == nsXULAtoms::outlinerchildren) {
    PRInt32 index = FindContent(aContainer);
    if (index >= 0) {
      Row* row = (Row*)mRows[index];
      row->SetEmpty(PR_TRUE);
      PRInt32 count;
      RemoveSubtree(index, &count);
      // Invalidate also the row to update twisty.
      mBoxObject->InvalidateRow(index);
      mBoxObject->RowCountChanged(index + 1, -count);
    }
  }
  else if (tag == nsXULAtoms::outlinerrow) {
    PRInt32 index = FindContent(aContainer);
    if (index >= 0)
      mBoxObject->InvalidateRow(index);
  }
  else if (tag == nsXULAtoms::outlinercell) {
    nsCOMPtr<nsIContent> parent;
    aContainer->GetParent(*getter_AddRefs(parent));
    if (parent) {
      PRInt32 index = FindContent(parent);
      if (index >= 0)
        mBoxObject->InvalidateRow(index);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::StyleSheetAdded(nsIDocument *aDocument,
                                      nsIStyleSheet* aStyleSheet)
{
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::StyleSheetRemoved(nsIDocument *aDocument,
                                        nsIStyleSheet* aStyleSheet)
{
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::StyleSheetDisabledStateChanged(nsIDocument *aDocument,
                                                     nsIStyleSheet* aStyleSheet,
                                                     PRBool aDisabled)
{
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::StyleRuleChanged(nsIDocument *aDocument,
                                       nsIStyleSheet* aStyleSheet,
                                       nsIStyleRule* aStyleRule,
                                       PRInt32 aHint)
{
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::StyleRuleAdded(nsIDocument *aDocument,
                                     nsIStyleSheet* aStyleSheet,
                                     nsIStyleRule* aStyleRule)
{
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::StyleRuleRemoved(nsIDocument *aDocument,
                                       nsIStyleSheet* aStyleSheet,
                                       nsIStyleRule* aStyleRule)
{
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerContentView::DocumentWillBeDestroyed(nsIDocument *aDocument)
{
  // Remove ourselfs from document's observers.
  if (mDocument) {
    mDocument->RemoveObserver(this);
    mDocument = nsnull;
  }

  ClearRows();

  return NS_OK;
}


// Recursively serialize content, starting with aContent.
void
nsOutlinerContentView::Serialize(nsIContent* aContent, PRInt32 aParentIndex, PRInt32* aIndex, nsVoidArray& aRows)
{
  ChildIterator iter, last;
  for (ChildIterator::Init(aContent, &iter, &last); iter != last; ++iter) {
    nsCOMPtr<nsIContent> content = *iter;
    nsCOMPtr<nsIAtom> tag;
    content->GetTag(*getter_AddRefs(tag));
    PRInt32 count = aRows.Count();
    if (tag == nsXULAtoms::outlineritem)
      SerializeItem(content, aParentIndex, aIndex, aRows);
    else if (tag == nsXULAtoms::outlinerseparator)
      SerializeSeparator(content, aParentIndex, aIndex, aRows);
    else if (tag == nsHTMLAtoms::option)
      SerializeOption(content, aParentIndex, aIndex, aRows);
    *aIndex += aRows.Count() - count;
  }
}

void
nsOutlinerContentView::SerializeItem(nsIContent* aContent, PRInt32 aParentIndex, PRInt32* aIndex, nsVoidArray& aRows)
{
  Row* row = Row::Create(mAllocator, aContent, aParentIndex);
  aRows.AppendElement(row);

  nsCOMPtr<nsIContent> realRow;
  nsOutlinerUtils::GetImmediateChild(aContent, nsXULAtoms::outlinerrow, getter_AddRefs(realRow));
  if (realRow)
    ParseProperties(realRow, &row->mProperty);

  nsAutoString container;
  aContent->GetAttr(kNameSpaceID_None, nsXULAtoms::container, container);
  if (container.Equals(NS_LITERAL_STRING("true"))) {
    row->SetContainer(PR_TRUE);
    nsAutoString open;
    aContent->GetAttr(kNameSpaceID_None, nsXULAtoms::open, open);
    if (open.Equals(NS_LITERAL_STRING("true"))) {
      row->SetOpen(PR_TRUE);
      nsCOMPtr<nsIContent> child;
      nsOutlinerUtils::GetImmediateChild(aContent, nsXULAtoms::outlinerchildren, getter_AddRefs(child));
      if (child) {
        // Now, recursively serialize our child.
        PRInt32 count = aRows.Count();
        PRInt32 index = 0;
        Serialize(child, aParentIndex + *aIndex + 1, &index, aRows);
        row->mSubtreeSize += aRows.Count() - count;
      }
      else
        row->SetEmpty(PR_TRUE);
    }
  } 
}

void
nsOutlinerContentView::SerializeSeparator(nsIContent* aContent, PRInt32 aParentIndex, PRInt32* aIndex, nsVoidArray& aRows)
{
  Row* row = Row::Create(mAllocator, aContent, aParentIndex);
  row->SetSeparator(PR_TRUE);
  aRows.AppendElement(row);

  ParseProperties(aContent, &row->mProperty);
}

void
nsOutlinerContentView::SerializeOption(nsIContent* aContent, PRInt32 aParentIndex,
                                       PRInt32* aIndex, nsVoidArray& aRows)
{
  Row* row = Row::Create(mAllocator, aContent, aParentIndex);
  aRows.AppendElement(row);

  // This will happen before the OutlinerSelection is hooked up.  So, cache the selected
  // state in the row properties and update the selection when it is attached.

  if (aContent->HasAttr(kNameSpaceID_None, nsLayoutAtoms::optionSelectedPseudo))
    mUpdateSelection = PR_TRUE;
}

void
nsOutlinerContentView::GetIndexInSubtree(nsIContent* aContainer, nsIContent* aContent, PRInt32* aIndex)
{
  ChildIterator iter, last;
  for (ChildIterator::Init(aContainer, &iter, &last); iter != last; ++iter) {
    nsCOMPtr<nsIContent> content = *iter;
    if (content == aContent)
      break;

    nsCOMPtr<nsIAtom> tag;
    content->GetTag(*getter_AddRefs(tag));
    if (tag == nsXULAtoms::outlineritem) {
      (*aIndex)++;
      nsAutoString container;
      content->GetAttr(kNameSpaceID_None, nsXULAtoms::container, container);
      if (container.Equals(NS_LITERAL_STRING("true"))) {
        nsAutoString open;
        content->GetAttr(kNameSpaceID_None, nsXULAtoms::open, open);
        if (open.Equals(NS_LITERAL_STRING("true"))) {
          nsCOMPtr<nsIContent> child;
          nsOutlinerUtils::GetImmediateChild(content, nsXULAtoms::outlinerchildren, getter_AddRefs(child));
          if (child)
            GetIndexInSubtree(child, aContent, aIndex);
        }
      }
    }
    else if (tag == nsXULAtoms::outlinerseparator)
      (*aIndex)++;
  }
}

void
nsOutlinerContentView::EnsureSubtree(PRInt32 aIndex, PRInt32* aCount)
{
  Row* row = (Row*)mRows[aIndex];
  nsCOMPtr<nsIContent> child;
  nsOutlinerUtils::GetImmediateChild(row->mContent, nsXULAtoms::outlinerchildren, getter_AddRefs(child));
  if (! child) {
    *aCount = 0;
    return;
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

  *aCount = count;
}

void
nsOutlinerContentView::RemoveSubtree(PRInt32 aIndex, PRInt32* aCount)
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

  *aCount = count;
}

void
nsOutlinerContentView::InsertRow(PRInt32 aParentIndex, PRInt32 aIndex, nsIContent* aContent, PRInt32* aCount)
{
  nsAutoVoidArray rows;
  nsCOMPtr<nsIAtom> tag;
  aContent->GetTag(*getter_AddRefs(tag));
  if (tag == nsXULAtoms::outlineritem)
    SerializeItem(aContent, aParentIndex, &aIndex, rows);
  else if (tag == nsXULAtoms::outlinerseparator)
    SerializeSeparator(aContent, aParentIndex, &aIndex, rows);
  else if (tag == nsHTMLAtoms::option)
    SerializeOption(aContent, aParentIndex, &aIndex, rows);
  mRows.InsertElementsAt(rows, aParentIndex + aIndex + 1);
  PRInt32 count = rows.Count();

  UpdateSubtreeSizes(aParentIndex, count);

  // Update parent indexes, but skip added rows.
  // They already have correct values.
  UpdateParentIndexes(aParentIndex + aIndex, count + 1, count);

  *aCount = count;
}

void
nsOutlinerContentView::RemoveRow(PRInt32 aIndex, PRInt32* aCount)
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

  *aCount = count;
}

void
nsOutlinerContentView::ClearRows()
{
  for (PRInt32 i = 0; i < mRows.Count(); i++)
    Row::Destroy(mAllocator, (Row*)mRows[i]);
  mRows.Clear();
  mRoot = nsnull;
} 

void
nsOutlinerContentView::OpenContainer(PRInt32 aIndex)
{
  Row* row = (Row*)mRows[aIndex];
  row->SetOpen(PR_TRUE);

  PRInt32 count;
  EnsureSubtree(aIndex, &count);
  mBoxObject->InvalidateRow(aIndex);
  mBoxObject->RowCountChanged(aIndex + 1, count);
}


void
nsOutlinerContentView::CloseContainer(PRInt32 aIndex)
{
  Row* row = (Row*)mRows[aIndex];
  row->SetOpen(PR_FALSE);

  PRInt32 count;
  RemoveSubtree(aIndex, &count);
  mBoxObject->InvalidateRow(aIndex);
  mBoxObject->RowCountChanged(aIndex + 1, -count);
}

PRInt32
nsOutlinerContentView::FindContent(nsIContent* aContent)
{
  for (PRInt32 i = 0; i < mRows.Count(); i++)
    if (((Row*)mRows[i])->mContent == aContent)
      return i;

  return -1;
}

void
nsOutlinerContentView::UpdateSubtreeSizes(PRInt32 aParentIndex, PRInt32 count)
{
  while (aParentIndex >= 0) {
    Row* row = (Row*)mRows[aParentIndex];
    row->mSubtreeSize += count;
    aParentIndex = row->mParentIndex;
  }
}

void
nsOutlinerContentView::UpdateParentIndexes(PRInt32 aIndex, PRInt32 aSkip, PRInt32 aCount)
{
  PRInt32 count = mRows.Count();
  for (PRInt32 i = aIndex + aSkip; i < count; i++) {
    Row* row = (Row*)mRows[i];
    if (row->mParentIndex > aIndex)
      row->mParentIndex += aCount;
  }
}

nsresult
nsOutlinerContentView::GetNamedCell(nsIContent* aContainer, const PRUnichar* aColID, nsIContent** aResult)
{
  PRInt32 colIndex;
  mBoxObject->GetColumnIndex(aColID, &colIndex);

  // Traverse through cells, try to find the cell by "ref" attribute or by cell
  // index in a row. "ref" attribute has higher priority.
  *aResult = nsnull;
  PRInt32 j = 0;
  ChildIterator iter, last;
  for (ChildIterator::Init(aContainer, &iter, &last); iter != last; ++iter) {
    nsCOMPtr<nsIContent> cell = *iter;
    nsCOMPtr<nsIAtom> tag;
    cell->GetTag(*getter_AddRefs(tag));
    if (tag == nsXULAtoms::outlinercell) {
      nsAutoString ref;
      cell->GetAttr(kNameSpaceID_None, nsXULAtoms::ref, ref);
      if (!ref.IsEmpty() && ref.Equals(aColID)) {
        *aResult = cell;
        break;
      }
      else if (j == colIndex)
        *aResult = cell;
      j++;
    }
  }
  NS_IF_ADDREF(*aResult);

  return NS_OK;
}

nsresult
nsOutlinerContentView::ParseProperties(nsIContent* aContent, Property** aProperty)
{
  nsAutoString properties;
  aContent->GetAttr(kNameSpaceID_None, nsXULAtoms::properties, properties);
  if (properties.Length()) {
    Property* lastProperty = *aProperty;

    nsAString::const_iterator end;
    properties.EndReading(end);

    nsAString::const_iterator iter;
    properties.BeginReading(iter);

    do {
      // Skip whitespace
      while (iter != end && nsCRT::IsAsciiSpace(*iter))
        ++iter;

      // If only whitespace, we're done
      if (iter == end)
        break;

      // Note the first non-whitespace character
      nsAString::const_iterator first = iter;

      // Advance to the next whitespace character
      while (iter != end && ! nsCRT::IsAsciiSpace(*iter))
        ++iter;

      nsCOMPtr<nsIAtom> atom = dont_AddRef(NS_NewAtom(Substring(first, iter)));
      Property* newProperty = new Property(atom);
      if (lastProperty)
        lastProperty->mNext = newProperty;
      else
        *aProperty = newProperty;
      lastProperty = newProperty;

    } while (iter != end);
  }

  return NS_OK;
}
