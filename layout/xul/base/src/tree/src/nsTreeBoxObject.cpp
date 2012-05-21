/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTreeBoxObject.h"
#include "nsCOMPtr.h"
#include "nsIDOMXULElement.h"
#include "nsIXULTemplateBuilder.h"
#include "nsTreeContentView.h"
#include "nsITreeSelection.h"
#include "nsChildIterator.h"
#include "nsContentUtils.h"
#include "nsDOMError.h"
#include "nsTreeBodyFrame.h"

NS_IMPL_CYCLE_COLLECTION_1(nsTreeBoxObject, mView)

NS_IMPL_ADDREF_INHERITED(nsTreeBoxObject, nsBoxObject)
NS_IMPL_RELEASE_INHERITED(nsTreeBoxObject, nsBoxObject)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsTreeBoxObject)
  NS_INTERFACE_MAP_ENTRY(nsITreeBoxObject)
NS_INTERFACE_MAP_END_INHERITING(nsBoxObject)

void
nsTreeBoxObject::Clear()
{
  ClearCachedValues();

  // Drop the view's ref to us.
  if (mView) {
    nsCOMPtr<nsITreeSelection> sel;
    mView->GetSelection(getter_AddRefs(sel));
    if (sel)
      sel->SetTree(nsnull);
    mView->SetTree(nsnull); // Break the circular ref between the view and us.
  }
  mView = nsnull;

  nsBoxObject::Clear();
}


nsTreeBoxObject::nsTreeBoxObject()
  : mTreeBody(nsnull)
{
}

nsTreeBoxObject::~nsTreeBoxObject()
{
  /* destructor code */
}


static void FindBodyElement(nsIContent* aParent, nsIContent** aResult)
{
  *aResult = nsnull;
  ChildIterator iter, last;
  for (ChildIterator::Init(aParent, &iter, &last); iter != last; ++iter) {
    nsCOMPtr<nsIContent> content = *iter;

    nsINodeInfo *ni = content->NodeInfo();
    if (ni->Equals(nsGkAtoms::treechildren, kNameSpaceID_XUL)) {
      *aResult = content;
      NS_ADDREF(*aResult);
      break;
    } else if (ni->Equals(nsGkAtoms::tree, kNameSpaceID_XUL)) {
      // There are nesting tree elements. Only the innermost should
      // find the treechilren.
      break;
    } else if (content->IsElement() &&
               !ni->Equals(nsGkAtoms::_template, kNameSpaceID_XUL)) {
      FindBodyElement(content, aResult);
      if (*aResult)
        break;
    }
  }
}

nsTreeBodyFrame*
nsTreeBoxObject::GetTreeBody(bool aFlushLayout)
{
  // Make sure our frames are up to date, and layout as needed.  We
  // have to do this before checking for our cached mTreeBody, since
  // it might go away on style flush, and in any case if aFlushLayout
  // is true we need to make sure to flush no matter what.
  // XXXbz except that flushing style when we were not asked to flush
  // layout here breaks things.  See bug 585123.
  nsIFrame* frame;
  if (aFlushLayout) {
    frame = GetFrame(aFlushLayout);
    if (!frame)
      return nsnull;
  }

  if (mTreeBody) {
    // Have one cached already.
    return mTreeBody;
  }

  if (!aFlushLayout) {
    frame = GetFrame(aFlushLayout);
    if (!frame)
      return nsnull;
  }

  // Iterate over our content model children looking for the body.
  nsCOMPtr<nsIContent> content;
  FindBodyElement(frame->GetContent(), getter_AddRefs(content));
  if (!content)
    return nsnull;

  frame = content->GetPrimaryFrame();
  if (!frame)
     return nsnull;

  // Make sure that the treebodyframe has a pointer to |this|.
  nsTreeBodyFrame *treeBody = do_QueryFrame(frame);
  NS_ENSURE_TRUE(treeBody && treeBody->GetTreeBoxObject() == this, nsnull);

  mTreeBody = treeBody;
  return mTreeBody;
}

NS_IMETHODIMP nsTreeBoxObject::GetView(nsITreeView * *aView)
{
  if (!mTreeBody) {
    if (!GetTreeBody()) {
      // Don't return an uninitialised view
      *aView = nsnull;
      return NS_OK;
    }

    if (mView)
      // Our new frame needs to initialise itself
      return mTreeBody->GetView(aView);
  }
  if (!mView) {
    nsCOMPtr<nsIDOMXULElement> xulele = do_QueryInterface(mContent);
    if (xulele) {
      // See if there is a XUL tree builder associated with the element
      nsCOMPtr<nsIXULTemplateBuilder> builder;
      xulele->GetBuilder(getter_AddRefs(builder));
      mView = do_QueryInterface(builder);

      if (!mView) {
        // No tree builder, create a tree content view.
        nsresult rv = NS_NewTreeContentView(getter_AddRefs(mView));
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Initialise the frame and view
      mTreeBody->SetView(mView);
    }
  }
  NS_IF_ADDREF(*aView = mView);
  return NS_OK;
}

static bool
CanTrustView(nsISupports* aValue)
{
  // Untrusted content is only allowed to specify known-good views
  if (nsContentUtils::IsCallerTrustedForWrite())
    return true;
  nsCOMPtr<nsINativeTreeView> nativeTreeView = do_QueryInterface(aValue);
  if (!nativeTreeView || NS_FAILED(nativeTreeView->EnsureNative())) {
    // XXX ERRMSG need a good error here for developers
    return false;
  }
  return true;
}

NS_IMETHODIMP nsTreeBoxObject::SetView(nsITreeView * aView)
{
  if (!CanTrustView(aView))
    return NS_ERROR_DOM_SECURITY_ERR;
  
  mView = aView;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    body->SetView(aView);

  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetFocused(bool* aFocused)
{
  *aFocused = false;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->GetFocused(aFocused);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::SetFocused(bool aFocused)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->SetFocused(aFocused);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetTreeBody(nsIDOMElement** aElement)
{
  *aElement = nsnull;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body) 
    return body->GetTreeBody(aElement);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetColumns(nsITreeColumns** aColumns)
{
  *aColumns = nsnull;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body) 
    return body->GetColumns(aColumns);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetRowHeight(PRInt32* aRowHeight)
{
  *aRowHeight = 0;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body) 
    return body->GetRowHeight(aRowHeight);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetRowWidth(PRInt32 *aRowWidth)
{
  *aRowWidth = 0;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body) 
    return body->GetRowWidth(aRowWidth);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetFirstVisibleRow(PRInt32 *aFirstVisibleRow)
{
  *aFirstVisibleRow = 0;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->GetFirstVisibleRow(aFirstVisibleRow);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetLastVisibleRow(PRInt32 *aLastVisibleRow)
{
  *aLastVisibleRow = 0;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->GetLastVisibleRow(aLastVisibleRow);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetHorizontalPosition(PRInt32 *aHorizontalPosition)
{
  *aHorizontalPosition = 0;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->GetHorizontalPosition(aHorizontalPosition);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetPageLength(PRInt32 *aPageLength)
{
  *aPageLength = 0;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->GetPageLength(aPageLength);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetSelectionRegion(nsIScriptableRegion **aRegion)
{
 *aRegion = nsnull;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->GetSelectionRegion(aRegion);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeBoxObject::EnsureRowIsVisible(PRInt32 aRow)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->EnsureRowIsVisible(aRow);
  return NS_OK;
}

NS_IMETHODIMP 
nsTreeBoxObject::EnsureCellIsVisible(PRInt32 aRow, nsITreeColumn* aCol)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->EnsureCellIsVisible(aRow, aCol);
  return NS_OK;
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsTreeBoxObject::ScrollToRow(PRInt32 aRow)
{
  nsTreeBodyFrame* body = GetTreeBody(true);
  if (body)
    return body->ScrollToRow(aRow);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeBoxObject::ScrollByLines(PRInt32 aNumLines)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->ScrollByLines(aNumLines);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeBoxObject::ScrollByPages(PRInt32 aNumPages)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->ScrollByPages(aNumPages);
  return NS_OK;
}

NS_IMETHODIMP 
nsTreeBoxObject::ScrollToCell(PRInt32 aRow, nsITreeColumn* aCol)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->ScrollToCell(aRow, aCol);
  return NS_OK;
}

NS_IMETHODIMP 
nsTreeBoxObject::ScrollToColumn(nsITreeColumn* aCol)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->ScrollToColumn(aCol);
  return NS_OK;
}

NS_IMETHODIMP 
nsTreeBoxObject::ScrollToHorizontalPosition(PRInt32 aHorizontalPosition)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->ScrollToHorizontalPosition(aHorizontalPosition);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::Invalidate()
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->Invalidate();
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::InvalidateColumn(nsITreeColumn* aCol)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->InvalidateColumn(aCol);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::InvalidateRow(PRInt32 aIndex)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->InvalidateRow(aIndex);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::InvalidateCell(PRInt32 aRow, nsITreeColumn* aCol)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->InvalidateCell(aRow, aCol);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::InvalidateRange(PRInt32 aStart, PRInt32 aEnd)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->InvalidateRange(aStart, aEnd);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::InvalidateColumnRange(PRInt32 aStart, PRInt32 aEnd, nsITreeColumn* aCol)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->InvalidateColumnRange(aStart, aEnd, aCol);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetRowAt(PRInt32 x, PRInt32 y, PRInt32 *aRow)
{
  *aRow = 0;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->GetRowAt(x, y, aRow);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetCellAt(PRInt32 aX, PRInt32 aY, PRInt32 *aRow, nsITreeColumn** aCol,
                                         nsACString& aChildElt)
{
  *aRow = 0;
  *aCol = nsnull;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->GetCellAt(aX, aY, aRow, aCol, aChildElt);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeBoxObject::GetCoordsForCellItem(PRInt32 aRow, nsITreeColumn* aCol, const nsACString& aElement, 
                                      PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight)
{
  *aX = *aY = *aWidth = *aHeight = 0;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->GetCoordsForCellItem(aRow, aCol, aElement, aX, aY, aWidth, aHeight);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeBoxObject::IsCellCropped(PRInt32 aRow, nsITreeColumn* aCol, bool *aIsCropped)
{  
  *aIsCropped = false;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->IsCellCropped(aRow, aCol, aIsCropped);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::RowCountChanged(PRInt32 aIndex, PRInt32 aDelta)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->RowCountChanged(aIndex, aDelta);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::BeginUpdateBatch()
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->BeginUpdateBatch();
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::EndUpdateBatch()
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->EndUpdateBatch();
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::ClearStyleAndImageCaches()
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->ClearStyleAndImageCaches();
  return NS_OK;
}

void
nsTreeBoxObject::ClearCachedValues()
{
  mTreeBody = nsnull;
}    

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewTreeBoxObject(nsIBoxObject** aResult)
{
  *aResult = new nsTreeBoxObject;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

