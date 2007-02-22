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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Hyatt <hyatt@mozilla.org> (Original Author)
 *   Ben Goodger <ben@netscape.com>
 *   Joe Hewitt <hewitt@netscape.com>
 *   Jan Varga <varga@ku.sk>
 *   Dean Tessman <dean_tessman@hotmail.com>
 *   Brian Ryner <bryner@brianryner.com>
 *   Blake Ross <blaker@netscape.com>
 *   Pierre Chanial <pierrechanial@netscape.net>
 *   Rene Pronk <r.pronk@its.tudelft.nl>
 *   Nate Nielsen <nielsen@memberwebs.com>
 *   Mark Banner <mark@standard8.demon.co.uk>
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

#include "nsCOMPtr.h"
#include "nsISupportsArray.h"
#include "nsPresContext.h"
#include "nsINameSpaceManager.h"
#include "nsIScrollbarFrame.h"

#include "nsTreeBodyFrame.h"
#include "nsTreeSelection.h"

#include "nsGkAtoms.h"
#include "nsCSSAnonBoxes.h"

#include "nsIContent.h"
#include "nsStyleContext.h"
#include "nsIBoxObject.h"
#include "nsGUIEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMXULElement.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsICSSStyleRule.h"
#include "nsCSSRendering.h"
#include "nsIFontMetrics.h"
#include "nsIDeviceContext.h"
#include "nsIXULTemplateBuilder.h"
#include "nsXPIDLString.h"
#include "nsHTMLContainerFrame.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsWidgetsCID.h"
#include "nsBoxFrame.h"
#include "nsBoxObject.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsBoxLayoutState.h"
#include "nsIDragService.h"
#include "nsTreeContentView.h"
#include "nsTreeUtils.h"
#include "nsChildIterator.h"
#include "nsIScrollableView.h"
#include "nsITheme.h"
#include "nsITimelineService.h"
#include "imgIRequest.h"
#include "imgIContainer.h"
#include "imgIContainerObserver.h"
#include "imgILoader.h"
#include "nsINodeInfo.h"
#include "nsContentUtils.h"
#include "nsLayoutUtils.h"
#include "nsIScrollableFrame.h"
#include "nsEventDispatcher.h"
#include "nsDisplayList.h"

#ifdef IBMBIDI
#include "nsBidiPresUtils.h"
#endif

#define ELLIPSIS "..."

static NS_DEFINE_CID(kWidgetCID, NS_CHILD_CID);

// Enumeration function that cancels all the image requests in our cache
PR_STATIC_CALLBACK(PLDHashOperator)
CancelImageRequest(const nsAString& aKey,
                   nsTreeImageCacheEntry aEntry, void* aData)
{
  aEntry.request->Cancel(NS_BINDING_ABORTED);
  return PL_DHASH_NEXT;
}

//
// NS_NewTreeFrame
//
// Creates a new tree frame
//
nsIFrame*
NS_NewTreeBodyFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsTreeBodyFrame(aPresShell, aContext);
} // NS_NewTreeFrame


//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsTreeBodyFrame)
  NS_INTERFACE_MAP_ENTRY(nsITreeBoxObject)
  NS_INTERFACE_MAP_ENTRY(nsICSSPseudoComparator)
  NS_INTERFACE_MAP_ENTRY(nsIScrollbarMediator)
  NS_INTERFACE_MAP_ENTRY(nsIReflowCallback)
NS_INTERFACE_MAP_END_INHERITING(nsLeafBoxFrame)



// Constructor
nsTreeBodyFrame::nsTreeBodyFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
:nsLeafBoxFrame(aPresShell, aContext),
 mTopRowIndex(0), 
 mHorzPosition(0),
 mHorzWidth(0),
 mRowHeight(0),
 mIndentation(0),
 mStringWidth(-1),
 mFocused(PR_FALSE),
 mHasFixedRowCount(PR_FALSE),
 mVerticalOverflow(PR_FALSE),
 mHorizontalOverflow(PR_FALSE),
 mReflowCallbackPosted(PR_FALSE),
 mUpdateBatchNest(0),
 mRowCount(0),
 mSlots(nsnull)
{
  mColumns = new nsTreeColumns(nsnull);
  NS_NewISupportsArray(getter_AddRefs(mScratchArray));
}

// Destructor
nsTreeBodyFrame::~nsTreeBodyFrame()
{
  mImageCache.EnumerateRead(CancelImageRequest, nsnull);
  delete mSlots;
}

NS_IMETHODIMP_(nsrefcnt) 
nsTreeBodyFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsTreeBodyFrame::Release(void)
{
  return NS_OK;
}

static void
GetBorderPadding(nsStyleContext* aContext, nsMargin& aMargin)
{
  aMargin.SizeTo(0, 0, 0, 0);
  if (!aContext->GetStylePadding()->GetPadding(aMargin)) {
    NS_NOTYETIMPLEMENTED("percentage padding");
  }
  aMargin += aContext->GetStyleBorder()->GetBorder();
}

static void
AdjustForBorderPadding(nsStyleContext* aContext, nsRect& aRect)
{
  nsMargin borderPadding(0, 0, 0, 0);
  GetBorderPadding(aContext, borderPadding);
  aRect.Deflate(borderPadding);
}

NS_IMETHODIMP
nsTreeBodyFrame::Init(nsIContent*     aContent,
                      nsIFrame*       aParent,
                      nsIFrame*       aPrevInFlow)
{
  nsresult rv = nsLeafBoxFrame::Init(aContent, aParent, aPrevInFlow);
  nsBoxFrame::CreateViewForFrame(GetPresContext(), this, GetStyleContext(), PR_TRUE);
  nsLeafBoxFrame::GetView()->CreateWidget(kWidgetCID);

  mIndentation = GetIndentation();
  mRowHeight = GetRowHeight();

  NS_ENSURE_TRUE(mImageCache.Init(16), NS_ERROR_OUT_OF_MEMORY);
  return rv;
}

nsSize
nsTreeBodyFrame::GetMinSize(nsBoxLayoutState& aBoxLayoutState)
{
  EnsureView();

  nsIContent* baseElement = GetBaseElement();

  nsSize min(0,0);
  PRInt32 desiredRows;
  if (NS_UNLIKELY(!baseElement)) {
    desiredRows = 0;
  }
  else if (baseElement->Tag() == nsGkAtoms::select &&
           baseElement->IsNodeOfType(nsINode::eHTML)) {
    min.width = CalcMaxRowWidth();
    nsAutoString size;
    baseElement->GetAttr(kNameSpaceID_None, nsGkAtoms::size, size);
    if (!size.IsEmpty()) {
      PRInt32 err;
      desiredRows = size.ToInteger(&err);
      mHasFixedRowCount = PR_TRUE;
      mPageLength = desiredRows;
    }
    else {
      desiredRows = 1;
    }
  }
  else {
    // tree
    nsAutoString rows;
    baseElement->GetAttr(kNameSpaceID_None, nsGkAtoms::rows, rows);
    if (!rows.IsEmpty()) {
      PRInt32 err;
      desiredRows = rows.ToInteger(&err);
      mPageLength = desiredRows;
    }
    else {
      desiredRows = 0;
    }
  }

  min.height = mRowHeight * desiredRows;

  AddBorderAndPadding(min);
  nsIBox::AddCSSMinSize(aBoxLayoutState, this, min);

  return min;
}

nscoord
nsTreeBodyFrame::CalcMaxRowWidth()
{
  if (mStringWidth != -1)
    return mStringWidth;

  if (!mView)
    return 0;

  nsStyleContext* rowContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreerow);
  nsMargin rowMargin(0,0,0,0);
  GetBorderPadding(rowContext, rowMargin);

  nscoord rowWidth;
  nsTreeColumn* col;

  nsCOMPtr<nsIRenderingContext> rc;
  GetPresContext()->PresShell()->CreateRenderingContext(this, getter_AddRefs(rc));

  for (PRInt32 row = 0; row < mRowCount; ++row) {
    rowWidth = 0;

    for (col = mColumns->GetFirstColumn(); col; col = col->GetNext()) {
      nscoord desiredWidth, currentWidth;
      nsresult rv = GetCellWidth(row, col, rc, desiredWidth, currentWidth);
      if (NS_FAILED(rv)) {
        NS_NOTREACHED("invalid column");
        continue;
      }
      rowWidth += desiredWidth;
    }

    if (rowWidth > mStringWidth)
      mStringWidth = rowWidth;
  }

  mStringWidth += rowMargin.left + rowMargin.right;
  return mStringWidth;
}

void
nsTreeBodyFrame::Destroy()
{
  // Make sure we cancel any posted callbacks. 
  if (mReflowCallbackPosted) {
    GetPresContext()->PresShell()->CancelReflowCallback(this);
    mReflowCallbackPosted = PR_FALSE;
  }

  if (mColumns)
    mColumns->SetTree(nsnull);

  // Save off our info into the box object.
  EnsureBoxObject();
  nsCOMPtr<nsPIBoxObject> box(do_QueryInterface(mTreeBoxObject));
  if (box) {
    if (mTopRowIndex > 0) {
      nsAutoString topRowStr; topRowStr.AssignLiteral("topRow");
      nsAutoString topRow;
      topRow.AppendInt(mTopRowIndex);
      box->SetProperty(topRowStr.get(), topRow.get());
    }

    // Always null out the cached tree body frame.
    box->ClearCachedValues();

    mTreeBoxObject = nsnull; // Drop our ref here.
  }

  if (mView) {
    nsCOMPtr<nsITreeSelection> sel;
    mView->GetSelection(getter_AddRefs(sel));
    if (sel)
      sel->SetTree(nsnull);
    mView->SetTree(nsnull);
    mView = nsnull;
  }

  nsLeafBoxFrame::Destroy();
}

void
nsTreeBodyFrame::EnsureBoxObject()
{
  if (!mTreeBoxObject) {
    nsIContent* parent = GetBaseElement();
    if (parent) {
      nsCOMPtr<nsIDOMNSDocument> nsDoc = do_QueryInterface(parent->GetDocument());
      if (!nsDoc) // there may be no document, if we're called from Destroy()
        return;
      nsCOMPtr<nsIBoxObject> box;
      nsCOMPtr<nsIDOMElement> domElem = do_QueryInterface(parent);
      nsDoc->GetBoxObjectFor(domElem, getter_AddRefs(box));
      // Ensure that we got a native box object.
      nsCOMPtr<nsPIBoxObject> pBox = do_QueryInterface(box);
      if (pBox) {
        mTreeBoxObject = do_QueryInterface(pBox);
        mColumns->SetTree(mTreeBoxObject);
      }
    }
  }
}

void
nsTreeBodyFrame::EnsureView()
{
  if (!mView) {
    EnsureBoxObject();
    nsCOMPtr<nsIBoxObject> box = do_QueryInterface(mTreeBoxObject);
    if (box) {
      nsCOMPtr<nsITreeView> treeView;
      mTreeBoxObject->GetView(getter_AddRefs(treeView));
      if (treeView) {
        nsXPIDLString rowStr;
        box->GetProperty(NS_LITERAL_STRING("topRow").get(),
                         getter_Copies(rowStr));
        nsAutoString rowStr2(rowStr);
        PRInt32 error;
        PRInt32 rowIndex = rowStr2.ToInteger(&error);

        // Set our view.
        SetView(treeView);

        // Scroll to the given row.
        // XXX is this optimal if we haven't laid out yet?
        ScrollToRow(rowIndex);

        // Clear out the property info for the top row, but we always keep the
        // view current.
        box->RemoveProperty(NS_LITERAL_STRING("topRow").get());
      }
    }
  }
}

void
nsTreeBodyFrame::SetBounds(nsBoxLayoutState& aBoxLayoutState, const nsRect& aRect,
                           PRBool aRemoveOverflowArea)
{
  nscoord horzWidth = CalcHorzWidth(GetScrollParts());
  if ((aRect != mRect || mHorzWidth != horzWidth) && !mReflowCallbackPosted) {
    mReflowCallbackPosted = PR_TRUE;
    GetPresContext()->PresShell()->PostReflowCallback(this);
  }

  mHorzWidth = horzWidth;

  nsLeafBoxFrame::SetBounds(aBoxLayoutState, aRect, aRemoveOverflowArea);
}


NS_IMETHODIMP
nsTreeBodyFrame::ReflowFinished(nsIPresShell* aPresShell, PRBool* aFlushFlag)
{
  if (mView) {
    CalcInnerBox();
    ScrollParts parts = GetScrollParts();
    mHorzWidth = CalcHorzWidth(parts);
    if (!mHasFixedRowCount) {
      mPageLength = mInnerBox.height / mRowHeight;
    }

    PRInt32 lastPageTopRow = PR_MAX(0, mRowCount - mPageLength);
    if (mTopRowIndex > lastPageTopRow)
      ScrollToRowInternal(parts, lastPageTopRow);

    // make sure that the current selected item is still
    // visible after the tree changes size.
    nsCOMPtr<nsITreeSelection> sel;
    mView->GetSelection(getter_AddRefs(sel));
    if (sel) {
      PRInt32 currentIndex;
      sel->GetCurrentIndex(&currentIndex);
      if (currentIndex != -1)
        EnsureRowIsVisibleInternal(parts, currentIndex);
    }

    InvalidateScrollbars(parts);
    CheckOverflow(parts);
  }

  mReflowCallbackPosted = PR_FALSE;
  *aFlushFlag = PR_FALSE;

  return NS_OK;
}


NS_IMETHODIMP nsTreeBodyFrame::GetView(nsITreeView * *aView)
{
  EnsureView();
  NS_IF_ADDREF(*aView = mView);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBodyFrame::SetView(nsITreeView * aView)
{
  // First clear out the old view.
  EnsureBoxObject();
  
  if (mView) {
    nsCOMPtr<nsITreeSelection> sel;
    mView->GetSelection(getter_AddRefs(sel));
    if (sel)
      sel->SetTree(nsnull);
    mView->SetTree(nsnull);

    // Only reset the top row index and delete the columns if we had an old non-null view.
    mTopRowIndex = 0;
  }

  // Tree, meet the view.
  mView = aView;
 
  // Changing the view causes us to refetch our data.  This will
  // necessarily entail a full invalidation of the tree.
  Invalidate();
 
  nsIContent *treeContent = GetBaseElement();
  if (treeContent) {
    FireDOMEvent(NS_LITERAL_STRING("TreeViewChanged"), treeContent);
  }

  if (mView) {
    // Give the view a new empty selection object to play with, but only if it
    // doesn't have one already.
    nsCOMPtr<nsITreeSelection> sel;
    mView->GetSelection(getter_AddRefs(sel));
    if (sel) {
      sel->SetTree(mTreeBoxObject);
    }
    else {
      NS_NewTreeSelection(mTreeBoxObject, getter_AddRefs(sel));
      mView->SetSelection(sel);
    }

    // View, meet the tree.
    mView->SetTree(mTreeBoxObject);
    mView->GetRowCount(&mRowCount);
 
    ScrollParts parts = GetScrollParts();
    // The scrollbar will need to be updated.
    InvalidateScrollbars(parts);

    // Reset scrollbar position.
    UpdateScrollbars(parts);

    CheckOverflow(parts);
  }
 
  return NS_OK;
}

NS_IMETHODIMP 
nsTreeBodyFrame::GetFocused(PRBool* aFocused)
{
  *aFocused = mFocused;
  return NS_OK;
}

NS_IMETHODIMP 
nsTreeBodyFrame::SetFocused(PRBool aFocused)
{
  if (mFocused != aFocused) {
    mFocused = aFocused;
    if (mView) {
      nsCOMPtr<nsITreeSelection> sel;
      mView->GetSelection(getter_AddRefs(sel));
      if (sel)
        sel->InvalidateSelection();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsTreeBodyFrame::GetTreeBody(nsIDOMElement** aElement)
{
  //NS_ASSERTION(mContent, "no content, see bug #104878");
  if (!mContent)
    return NS_ERROR_NULL_POINTER;

  return mContent->QueryInterface(NS_GET_IID(nsIDOMElement), (void**)aElement);
}

NS_IMETHODIMP 
nsTreeBodyFrame::GetColumns(nsITreeColumns** aColumns)
{
  EnsureBoxObject();
  NS_IF_ADDREF(*aColumns = mColumns);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeBodyFrame::GetRowHeight(PRInt32* _retval)
{
  *_retval = nsPresContext::AppUnitsToIntCSSPixels(mRowHeight);
  return NS_OK;
}

NS_IMETHODIMP 
nsTreeBodyFrame::GetRowWidth(PRInt32 *aRowWidth)
{
  *aRowWidth = nsPresContext::AppUnitsToIntCSSPixels(CalcHorzWidth(GetScrollParts()));
  return NS_OK;
}

NS_IMETHODIMP
nsTreeBodyFrame::GetFirstVisibleRow(PRInt32 *_retval)
{
  *_retval = mTopRowIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsTreeBodyFrame::GetLastVisibleRow(PRInt32 *_retval)
{
  *_retval = GetLastVisibleRow();
  return NS_OK;
}

NS_IMETHODIMP 
nsTreeBodyFrame::GetHorizontalPosition(PRInt32 *aHorizontalPosition)
{
  *aHorizontalPosition = nsPresContext::AppUnitsToIntCSSPixels(mHorzPosition); 
  return NS_OK;
}

NS_IMETHODIMP
nsTreeBodyFrame::GetPageLength(PRInt32 *_retval)
{
  *_retval = mPageLength;
  return NS_OK;
}

NS_IMETHODIMP
nsTreeBodyFrame::Invalidate()
{
  if (mUpdateBatchNest)
    return NS_OK;

  nsIFrame::Invalidate(GetOverflowRect(), PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP
nsTreeBodyFrame::InvalidateColumn(nsITreeColumn* aCol)
{
  if (mUpdateBatchNest)
    return NS_OK;

  nsRefPtr<nsTreeColumn> col = GetColumnImpl(aCol);
  if (!col)
    return NS_ERROR_INVALID_ARG;

  nsRect columnRect;
  nsresult rv = col->GetRect(this, mInnerBox.y, mInnerBox.height, &columnRect);
  NS_ENSURE_SUCCESS(rv, rv);

  // When false then column is out of view
  if (OffsetForHorzScroll(columnRect, PR_TRUE))
      nsIFrame::Invalidate(columnRect, PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP
nsTreeBodyFrame::InvalidateRow(PRInt32 aIndex)
{
  if (mUpdateBatchNest)
    return NS_OK;

  aIndex -= mTopRowIndex;
  if (aIndex < 0 || aIndex > mPageLength)
    return NS_OK;

  nsRect rowRect(mInnerBox.x, mInnerBox.y+mRowHeight*aIndex, mInnerBox.width, mRowHeight);
#if defined(XP_MAC) || defined(XP_MACOSX)
  // Mac can't process the event loop during a drag, so if we're dragging,
  // invalidate synchronously.
  nsLeafBoxFrame::Invalidate(rowRect, mSlots && mSlots->mDragSession ? PR_TRUE : PR_FALSE);
#else
  nsLeafBoxFrame::Invalidate(rowRect, PR_FALSE);
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsTreeBodyFrame::InvalidateCell(PRInt32 aIndex, nsITreeColumn* aCol)
{
  if (mUpdateBatchNest)
    return NS_OK;

  aIndex -= mTopRowIndex;
  if (aIndex < 0 || aIndex > mPageLength)
    return NS_OK;

  nsRefPtr<nsTreeColumn> col = GetColumnImpl(aCol);
  if (!col)
    return NS_ERROR_INVALID_ARG;

  nsRect cellRect;
  nsresult rv = col->GetRect(this, mInnerBox.y+mRowHeight*aIndex, mRowHeight,
                             &cellRect);
  NS_ENSURE_SUCCESS(rv, rv);

  if (OffsetForHorzScroll(cellRect, PR_TRUE))
    nsIFrame::Invalidate(cellRect, PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP
nsTreeBodyFrame::InvalidateRange(PRInt32 aStart, PRInt32 aEnd)
{
  if (mUpdateBatchNest)
    return NS_OK;

  if (aStart == aEnd)
    return InvalidateRow(aStart);

  PRInt32 last = GetLastVisibleRow();
  if (aStart > aEnd || aEnd < mTopRowIndex || aStart > last)
    return NS_OK;

  if (aStart < mTopRowIndex)
    aStart = mTopRowIndex;

  if (aEnd > last)
    aEnd = last;

  nsRect rangeRect(mInnerBox.x, mInnerBox.y+mRowHeight*(aStart-mTopRowIndex), mInnerBox.width, mRowHeight*(aEnd-aStart+1));
  nsIFrame::Invalidate(rangeRect, PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP
nsTreeBodyFrame::InvalidateColumnRange(PRInt32 aStart, PRInt32 aEnd, nsITreeColumn* aCol)
{
  if (mUpdateBatchNest)
    return NS_OK;

  nsRefPtr<nsTreeColumn> col = GetColumnImpl(aCol);
  if (!col)
    return NS_ERROR_INVALID_ARG;

  if (aStart == aEnd)
    return InvalidateCell(aStart, col);

  PRInt32 last = GetLastVisibleRow();
  if (aStart > aEnd || aEnd < mTopRowIndex || aStart > last)
    return NS_OK;

  if (aStart < mTopRowIndex)
    aStart = mTopRowIndex;

  if (aEnd > last)
    aEnd = last;

  nsRect rangeRect;
  nsresult rv = col->GetRect(this, 
                             mInnerBox.y+mRowHeight*(aStart-mTopRowIndex),
                             mRowHeight*(aEnd-aStart+1),
                             &rangeRect);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIFrame::Invalidate(rangeRect, PR_FALSE);

  return NS_OK;
}

static void
FindScrollParts(nsIFrame* aCurrFrame, nsTreeBodyFrame::ScrollParts* aResult)
{
  if (!aResult->mColumnsScrollableView) {
    nsIScrollableFrame* f;
    CallQueryInterface(aCurrFrame, &f);
    if (f) {
      aResult->mColumnsScrollableView = f->GetScrollableView();
    }
  }
  
  nsIScrollbarFrame *sf = nsnull;
  CallQueryInterface(aCurrFrame, &sf);
  if (sf) {
    if (!aCurrFrame->IsHorizontal()) {
      if (!aResult->mVScrollbar) {
        aResult->mVScrollbar = sf;
      }
    } else {
      if (!aResult->mHScrollbar) {
        aResult->mHScrollbar = sf;
      }
    }
    // don't bother searching inside a scrollbar
    return;
  }
  
  nsIFrame* child = aCurrFrame->GetFirstChild(nsnull);
  while (child &&
         (!aResult->mVScrollbar || !aResult->mHScrollbar ||
          !aResult->mColumnsScrollableView)) {
    FindScrollParts(child, aResult);
    child = child->GetNextSibling();
  }
}

nsTreeBodyFrame::ScrollParts nsTreeBodyFrame::GetScrollParts()
{
  nsPresContext* presContext = GetPresContext();
  ScrollParts result = { nsnull, nsnull, nsnull, nsnull, nsnull };
  nsIContent* baseElement = GetBaseElement();
  nsIFrame* treeFrame =
    baseElement ? presContext->PresShell()->GetPrimaryFrameFor(baseElement) : nsnull;
  if (treeFrame) {
    // The way we do this, searching through the entire frame subtree, is pretty
    // dumb! We should know where these frames are.
    FindScrollParts(treeFrame, &result);
    if (result.mHScrollbar) {
      result.mHScrollbar->SetScrollbarMediatorContent(GetContent());
      nsIFrame* f;
      CallQueryInterface(result.mHScrollbar, &f);
      result.mHScrollbarContent = f->GetContent();
    }
    if (result.mVScrollbar) {
      result.mVScrollbar->SetScrollbarMediatorContent(GetContent());
      nsIFrame* f;
      CallQueryInterface(result.mVScrollbar, &f);
      result.mVScrollbarContent = f->GetContent();
    }
  }
  return result;
}

void
nsTreeBodyFrame::UpdateScrollbars(const ScrollParts& aParts)
{
  nscoord rowHeightAsPixels = nsPresContext::AppUnitsToIntCSSPixels(mRowHeight);

  // Keep strong ref.
  nsCOMPtr<nsIContent> hScroll = aParts.mHScrollbarContent;

  if (aParts.mVScrollbar) {
    nsAutoString curPos;
    curPos.AppendInt(mTopRowIndex*rowHeightAsPixels);
    aParts.mVScrollbarContent->SetAttr(kNameSpaceID_None, nsGkAtoms::curpos, curPos, PR_TRUE);
  }

  if (aParts.mHScrollbar) {
    nsAutoString curPos;
    curPos.AppendInt(mHorzPosition);
    hScroll->SetAttr(kNameSpaceID_None, nsGkAtoms::curpos, curPos, PR_TRUE);
  }
}

void
nsTreeBodyFrame::CheckOverflow(const ScrollParts& aParts)
{
  PRBool verticalOverflowChanged = PR_FALSE;

  if (!mVerticalOverflow && mRowCount > mPageLength) {
    mVerticalOverflow = PR_TRUE;
    verticalOverflowChanged = PR_TRUE;
  }
  else if (mVerticalOverflow && mRowCount <= mPageLength) {
    mVerticalOverflow = PR_FALSE;
    verticalOverflowChanged = PR_TRUE;
  }

  nsPresContext* presContext = GetPresContext();

  if (verticalOverflowChanged) {
    nsScrollPortEvent event(PR_TRUE, mVerticalOverflow ? NS_SCROLLPORT_OVERFLOW
                            : NS_SCROLLPORT_UNDERFLOW, nsnull);
    event.orient = nsScrollPortEvent::vertical;

    nsEventStatus status = nsEventStatus_eIgnore;
    nsEventDispatcher::Dispatch(mContent, presContext, &event, nsnull, &status);
  }

  if (!aParts.mColumnsScrollableView)
    return;

  nsRect bounds = aParts.mColumnsScrollableView->View()->GetBounds();
  if (bounds.width == 0)
    return;

  /* Ignore overflows that are less than half a pixel. Yes these happen
     all over the place when flex boxes are compressed real small. 
     Probably a result of a rounding errors somewhere in the layout code. */
  bounds.width += nsPresContext::CSSPixelsToAppUnits(0.5f);
  
  PRBool horizontalOverflowChanged = PR_FALSE;
  if (!mHorizontalOverflow && bounds.width < mHorzWidth) {
    mHorizontalOverflow = PR_TRUE;
    horizontalOverflowChanged = PR_TRUE;
  } else if (mHorizontalOverflow && bounds.width >= mHorzWidth) {
    mHorizontalOverflow = PR_FALSE;
    horizontalOverflowChanged = PR_TRUE;
  }

  if (horizontalOverflowChanged) {
    nsScrollPortEvent event(PR_TRUE,
                            mHorizontalOverflow ? NS_SCROLLPORT_OVERFLOW
                                                : NS_SCROLLPORT_UNDERFLOW,
                            nsnull);
    event.orient = nsScrollPortEvent::horizontal;

    nsEventStatus status = nsEventStatus_eIgnore;
    nsEventDispatcher::Dispatch(mContent, presContext, &event, nsnull, &status);
  }
}

void
nsTreeBodyFrame::InvalidateScrollbars(const ScrollParts& aParts)
{
  if (mUpdateBatchNest || !mView)
    return;
  nsWeakFrame weakFrame(this);

  nsCOMPtr<nsIContent> vScrollbar = aParts.mVScrollbarContent;
  nsCOMPtr<nsIContent> hScrollbar = aParts.mHScrollbarContent;
  if (aParts.mVScrollbar) {
    // Do Vertical Scrollbar 
    nsAutoString maxposStr;

    nscoord rowHeightAsPixels = nsPresContext::AppUnitsToIntCSSPixels(mRowHeight);

    PRInt32 size = rowHeightAsPixels * (mRowCount > mPageLength ? mRowCount - mPageLength : 0);
    maxposStr.AppendInt(size);
    vScrollbar->SetAttr(kNameSpaceID_None, nsGkAtoms::maxpos, maxposStr, PR_TRUE);
    ENSURE_TRUE(weakFrame.IsAlive());

    // Also set our page increment and decrement.
    nscoord pageincrement = mPageLength*rowHeightAsPixels;
    nsAutoString pageStr;
    pageStr.AppendInt(pageincrement);
    vScrollbar->SetAttr(kNameSpaceID_None, nsGkAtoms::pageincrement, pageStr, PR_TRUE);
    ENSURE_TRUE(weakFrame.IsAlive());
  }

  if (aParts.mHScrollbar && aParts.mColumnsScrollableView) {
    // And now Horizontal scrollbar
    nsRect bounds = aParts.mColumnsScrollableView->View()->GetBounds();
    nsAutoString maxposStr;

    maxposStr.AppendInt(mHorzWidth > bounds.width ? mHorzWidth - bounds.width : 0);
    hScrollbar->SetAttr(kNameSpaceID_None, nsGkAtoms::maxpos, maxposStr, PR_TRUE);
    ENSURE_TRUE(weakFrame.IsAlive());
  
    nsAutoString pageStr;
    pageStr.AppendInt(bounds.width);
    hScrollbar->SetAttr(kNameSpaceID_None, nsGkAtoms::pageincrement, pageStr, PR_TRUE);
    ENSURE_TRUE(weakFrame.IsAlive());
  
    pageStr.Truncate();
    pageStr.AppendInt(nsPresContext::CSSPixelsToAppUnits(16));
    hScrollbar->SetAttr(kNameSpaceID_None, nsGkAtoms::increment, pageStr, PR_TRUE);
  }
}

// Takes client x/y in pixels, converts them to twips, and massages them to be
// in our coordinate system.
void
nsTreeBodyFrame::AdjustClientCoordsToBoxCoordSpace(PRInt32 aX, PRInt32 aY,
                                                   nscoord* aResultX,
                                                   nscoord* aResultY)
{
  nsPresContext* presContext = GetPresContext();

  nsPoint point(nsPresContext::CSSPixelsToAppUnits(aX),
                nsPresContext::CSSPixelsToAppUnits(aY));

  // Now get our client offset, in twips, and subtract if from the
  // point to get it in our coordinates
  nsPoint clientOffset;
  nsIView* closestView = GetClosestView(&clientOffset);
  point -= clientOffset;

  nsIView* rootView;
  presContext->GetViewManager()->GetRootView(rootView);
  NS_ASSERTION(closestView && rootView, "No view?");
  point -= closestView->GetOffsetTo(rootView);

  // Adjust by the inner box coords, so that we're in the inner box's
  // coordinate space.
  point -= mInnerBox.TopLeft();

  *aResultX = point.x;
  *aResultY = point.y;
} // AdjustClientCoordsToBoxCoordSpace

NS_IMETHODIMP
nsTreeBodyFrame::GetRowAt(PRInt32 aX, PRInt32 aY, PRInt32* _retval)
{
  if (!mView)
    return NS_OK;

  nscoord x;
  nscoord y;
  AdjustClientCoordsToBoxCoordSpace(aX, aY, &x, &y);

  // Check if the coordinates are above our visible space.
  if (y < 0) {
    *_retval = -1;
    return NS_OK;
  }

  *_retval = GetRowAt(x, y);

  return NS_OK;
}

NS_IMETHODIMP
nsTreeBodyFrame::GetCellAt(PRInt32 aX, PRInt32 aY, PRInt32* aRow, nsITreeColumn** aCol,
                           nsACString& aChildElt)
{
  if (!mView)
    return NS_OK;

  nscoord x;
  nscoord y;
  AdjustClientCoordsToBoxCoordSpace(aX, aY, &x, &y);

  // Check if the coordinates are above our visible space.
  if (y < 0) {
    *aRow = -1;
    return NS_OK;
  }

  nsTreeColumn* col;
  nsIAtom* child;
  GetCellAt(x, y, aRow, &col, &child);

  if (col) {
    NS_ADDREF(*aCol = col);
    if (child == nsCSSAnonBoxes::moztreecell)
      aChildElt.AssignLiteral("cell");
    else if (child == nsCSSAnonBoxes::moztreetwisty)
      aChildElt.AssignLiteral("twisty");
    else if (child == nsCSSAnonBoxes::moztreeimage)
      aChildElt.AssignLiteral("image");
    else if (child == nsCSSAnonBoxes::moztreecelltext)
      aChildElt.AssignLiteral("text");
  }

  return NS_OK;
}


//
// GetCoordsForCellItem
//
// Find the x/y location and width/height (all in PIXELS) of the given object
// in the given column. 
//
// XXX IMPORTANT XXX:
// Hyatt says in the bug for this, that the following needs to be done:
// (1) You need to deal with overflow when computing cell rects.  See other column 
// iteration examples... if you don't deal with this, you'll mistakenly extend the 
// cell into the scrollbar's rect.
//
// (2) You are adjusting the cell rect by the *row" border padding.  That's 
// wrong.  You need to first adjust a row rect by its border/padding, and then the 
// cell rect fits inside the adjusted row rect.  It also can have border/padding 
// as well as margins.  The vertical direction isn't that important, but you need 
// to get the horizontal direction right.
//
// (3) GetImageSize() does not include margins (but it does include border/padding).  
// You need to make sure to add in the image's margins as well.
//
NS_IMETHODIMP
nsTreeBodyFrame::GetCoordsForCellItem(PRInt32 aRow, nsITreeColumn* aCol, const nsACString& aElement, 
                                      PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight)
{
  *aX = 0;
  *aY = 0;
  *aWidth = 0;
  *aHeight = 0;

  nscoord currX = mInnerBox.x;

  // The Rect for the requested item. 
  nsRect theRect;

  nsPresContext* presContext = GetPresContext();

  for (nsTreeColumn* currCol = mColumns->GetFirstColumn(); currCol && currX < mInnerBox.x + mInnerBox.width;
       currCol = currCol->GetNext()) {

    // The Rect for the current cell.
    nscoord colWidth;
    nsresult rv = currCol->GetWidthInTwips(this, &colWidth);
    NS_ASSERTION(NS_SUCCEEDED(rv), "invalid column");

    nsRect cellRect(currX, mInnerBox.y + mRowHeight * (aRow - mTopRowIndex),
                    colWidth, mRowHeight);

    // Check the ID of the current column to see if it matches. If it doesn't 
    // increment the current X value and continue to the next column.
    if (currCol != aCol) {
      currX += cellRect.width;
      continue;
    }

    // Now obtain the properties for our cell.
    PrefillPropertyArray(aRow, currCol);
    mView->GetCellProperties(aRow, currCol, mScratchArray);

    nsStyleContext* rowContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreerow);

    // We don't want to consider any of the decorations that may be present
    // on the current row, so we have to deflate the rect by the border and 
    // padding and offset its left and top coordinates appropriately. 
    AdjustForBorderPadding(rowContext, cellRect);

    nsStyleContext* cellContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreecell);

    NS_NAMED_LITERAL_CSTRING(cell, "cell");
    if (currCol->IsCycler() || cell.Equals(aElement)) {
      // If the current Column is a Cycler, then the Rect is just the cell - the margins. 
      // Similarly, if we're just being asked for the cell rect, provide it. 

      theRect = cellRect;
      nsMargin cellMargin;
      cellContext->GetStyleMargin()->GetMargin(cellMargin);
      theRect.Deflate(cellMargin);
      break;
    }

    // Since we're not looking for the cell, and since the cell isn't a cycler,
    // we're looking for some subcomponent, and now we need to subtract the 
    // borders and padding of the cell from cellRect so this does not 
    // interfere with our computations.
    AdjustForBorderPadding(cellContext, cellRect);

    nsCOMPtr<nsIRenderingContext> rc;
    presContext->PresShell()->CreateRenderingContext(this, getter_AddRefs(rc));

    // Now we'll start making our way across the cell, starting at the edge of 
    // the cell and proceeding until we hit the right edge. |cellX| is the 
    // working X value that we will increment as we crawl from left to right.
    nscoord cellX = cellRect.x;
    nscoord remainWidth = cellRect.width;

    if (currCol->IsPrimary()) {
      // If the current Column is a Primary, then we need to take into account the indentation
      // and possibly a twisty. 

      // The amount of indentation is the indentation width (|mIndentation|) by the level. 
      PRInt32 level;
      mView->GetLevel(aRow, &level);
      cellX += mIndentation * level;
      remainWidth -= mIndentation * level;

      // Find the twisty rect by computing its size. 
      nsRect imageRect;
      nsRect twistyRect(cellRect);
      nsStyleContext* twistyContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreetwisty);
      GetTwistyRect(aRow, currCol, imageRect, twistyRect, presContext,
                    *rc, twistyContext);

      if (NS_LITERAL_CSTRING("twisty").Equals(aElement)) {
        // If we're looking for the twisty Rect, just return the size
        theRect = twistyRect;
        break;
      }
      
      // Now we need to add in the margins of the twisty element, so that we 
      // can find the offset of the next element in the cell. 
      nsMargin twistyMargin;
      twistyContext->GetStyleMargin()->GetMargin(twistyMargin);
      twistyRect.Inflate(twistyMargin);

      // Adjust our working X value with the twisty width (image size, margins,
      // borders, padding. 
      cellX += twistyRect.width;
    }

    // Cell Image
    nsStyleContext* imageContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreeimage);

    nsRect imageSize = GetImageSize(aRow, currCol, PR_FALSE, imageContext);
    if (NS_LITERAL_CSTRING("image").Equals(aElement)) {
      theRect = imageSize;
      theRect.x = cellX;
      theRect.y = cellRect.y;
      break;
    }

    // Add in the margins of the cell image.
    nsMargin imageMargin;
    imageContext->GetStyleMargin()->GetMargin(imageMargin);
    imageSize.Inflate(imageMargin);

    // Increment cellX by the image width
    cellX += imageSize.width;
    
    // Cell Text 
    nsAutoString cellText;
    mView->GetCellText(aRow, currCol, cellText);
    // We're going to measure this text so we need to ensure bidi is enabled if
    // necessary
    CheckTextForBidi(cellText);

    // Create a scratch rect to represent the text rectangle, with the current 
    // X and Y coords, and a guess at the width and height. The width is the 
    // remaining width we have left to traverse in the cell, which will be the
    // widest possible value for the text rect, and the row height. 
    nsRect textRect(cellX, cellRect.y, remainWidth, cellRect.height);

    // Measure the width of the text. If the width of the text is greater than 
    // the remaining width available, then we just assume that the text has 
    // been cropped and use the remaining rect as the text Rect. Otherwise,
    // we add in borders and padding to the text dimension and give that back. 
    nsStyleContext* textContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreecelltext);

    nsCOMPtr<nsIFontMetrics> fm;
    presContext->DeviceContext()->
      GetMetricsFor(textContext->GetStyleFont()->mFont, *getter_AddRefs(fm));
    nscoord height;
    fm->GetHeight(height);

    nsMargin textMargin;
    textContext->GetStyleMargin()->GetMargin(textMargin);
    textRect.Deflate(textMargin);

    // Center the text. XXX Obey vertical-align style prop?
    if (height < textRect.height) {
      textRect.y += (textRect.height - height) / 2;
      textRect.height = height;
    }

    nsMargin bp(0,0,0,0);
    GetBorderPadding(textContext, bp);
    textRect.height += bp.top + bp.bottom;

    rc->SetFont(fm);
    nscoord width =
      nsLayoutUtils::GetStringWidth(this, rc, cellText.get(), cellText.Length());

    nscoord totalTextWidth = width + bp.left + bp.right;
    if (totalTextWidth < remainWidth) {
      // If the text is not cropped, the text is smaller than the available 
      // space and we set the text rect to be that width. 
      textRect.width = totalTextWidth;
    }

    theRect = textRect;
  }

  *aX = nsPresContext::AppUnitsToIntCSSPixels(theRect.x);
  *aY = nsPresContext::AppUnitsToIntCSSPixels(theRect.y);
  *aWidth = nsPresContext::AppUnitsToIntCSSPixels(theRect.width);
  *aHeight = nsPresContext::AppUnitsToIntCSSPixels(theRect.height);

  return NS_OK;
}

PRInt32
nsTreeBodyFrame::GetRowAt(PRInt32 aX, PRInt32 aY)
{
  // Now just mod by our total inner box height and add to our top row index.
  PRInt32 row = (aY/mRowHeight)+mTopRowIndex;

  // Check if the coordinates are below our visible space (or within our visible
  // space but below any row).
  if (row > mTopRowIndex + mPageLength || row >= mRowCount)
    return -1;

  return row;
}

void
nsTreeBodyFrame::CheckTextForBidi(nsAutoString& aText)
{
  // We could check to see whether the prescontext already has bidi enabled,
  // but usually it won't, so it's probably faster to avoid the call to
  // GetPresContext() when it's not needed.
  const PRUnichar* text = aText.get();
  PRUint32 length = aText.Length();
  PRUint32 i;
  PRBool maybeRTL = PR_FALSE;
  for (i = 0; i < length; ++i) {
    PRUnichar ch = text[i];
    // To simplify things, anything that could be a surrogate or RTL
    // presentation form is covered just by testing >= 0xD800). It's fine to
    // enable bidi in rare cases where it actually isn't needed.
    if (ch >= 0xD800 || IS_IN_BMP_RTL_BLOCK(ch)) {
      maybeRTL = PR_TRUE;
      break;
    }
  }
  if (!maybeRTL)
    return;

  GetPresContext()->SetBidiEnabled(PR_TRUE);
}

void
nsTreeBodyFrame::AdjustForCellText(nsAutoString& aText,
                                   PRInt32 aRowIndex,  nsTreeColumn* aColumn,
                                   nsIRenderingContext& aRenderingContext,
                                   nsRect& aTextRect)
{
  NS_PRECONDITION(aColumn && aColumn->GetFrame(this), "invalid column passed");

  nscoord width =
    nsLayoutUtils::GetStringWidth(this, &aRenderingContext, aText.get(), aText.Length());
  nscoord maxWidth = aTextRect.width;

  if (aColumn->Overflow()) {
    nsresult rv;
    nsTreeColumn* nextColumn = aColumn->GetNext();
    while (nextColumn && width > maxWidth) {
      while (nextColumn) {
        nscoord width;
        rv = nextColumn->GetWidthInTwips(this, &width);
        NS_ASSERTION(NS_SUCCEEDED(rv), "nextColumn is invalid");

        if (width != 0)
          break;

        nextColumn = nextColumn->GetNext();
      }

      if (nextColumn) {
        nsAutoString nextText;
        mView->GetCellText(aRowIndex, nextColumn, nextText);
        // We don't measure or draw this text so no need to check it for
        // bidi-ness

        if (nextText.Length() == 0) {
          nscoord width;
          rv = nextColumn->GetWidthInTwips(this, &width);
          NS_ASSERTION(NS_SUCCEEDED(rv), "nextColumn is invalid");

          maxWidth += width;

          nextColumn = nextColumn->GetNext();
        }
        else {
          nextColumn = nsnull;
        }
      }
    }
  }

  if (width > maxWidth) {
    // See if the width is even smaller than the ellipsis
    // If so, clear the text completely.
    nscoord ellipsisWidth;
    aRenderingContext.SetTextRunRTL(PR_FALSE);
    aRenderingContext.GetWidth(ELLIPSIS, ellipsisWidth);

    nscoord width = aTextRect.width;
    if (ellipsisWidth > width)
      aText.SetLength(0);
    else if (ellipsisWidth == width)
      aText.AssignLiteral(ELLIPSIS);
    else {
      // We will be drawing an ellipsis, thank you very much.
      // Subtract out the required width of the ellipsis.
      // This is the total remaining width we have to play with.
      width -= ellipsisWidth;

      // Now we crop.
      switch (aColumn->GetCropStyle()) {
        default:
        case 0: {
          // Crop right.
          nscoord cwidth;
          nscoord twidth = 0;
          int length = aText.Length();
          int i;
          for (i = 0; i < length; ++i) {
            PRUnichar ch = aText[i];
            // XXX this is horrible and doesn't handle clusters
            aRenderingContext.GetWidth(ch,cwidth);
            if (twidth + cwidth > width)
              break;
            twidth += cwidth;
          }
          aText.Truncate(i);
          aText.AppendLiteral(ELLIPSIS);
        }
        break;

        case 2: {
          // Crop left.
          nscoord cwidth;
          nscoord twidth = 0;
          int length = aText.Length();
          int i;
          for (i=length-1; i >= 0; --i) {
            PRUnichar ch = aText[i];
            aRenderingContext.GetWidth(ch,cwidth);
            if (twidth + cwidth > width)
              break;
            twidth += cwidth;
          }

          nsAutoString copy;
          aText.Right(copy, length-1-i);
          aText.AssignLiteral(ELLIPSIS);
          aText += copy;
        }
        break;

        case 1:
        {
          // Crop center.
          nsAutoString leftStr, rightStr;
          nscoord cwidth, twidth = 0;
          int length = aText.Length();
          int rightPos = length - 1;
          for (int leftPos = 0; leftPos < rightPos; ++leftPos) {
            PRUnichar ch = aText[leftPos];
            aRenderingContext.GetWidth(ch, cwidth);
            twidth += cwidth;
            if (twidth > width)
              break;
            leftStr.Append(ch);

            ch = aText[rightPos];
            aRenderingContext.GetWidth(ch, cwidth);
            twidth += cwidth;
            if (twidth > width)
              break;
            rightStr.Insert(ch, 0);
            --rightPos;
          }
          aText = leftStr + NS_LITERAL_STRING(ELLIPSIS) + rightStr;
        }
        break;
      }
    }
  }
  else {
    switch (aColumn->GetTextAlignment()) {
      case NS_STYLE_TEXT_ALIGN_RIGHT: {
        aTextRect.x += aTextRect.width - width;
      }
      break;
      case NS_STYLE_TEXT_ALIGN_CENTER: {
        aTextRect.x += (aTextRect.width - width) / 2;
      }
      break;
    }
  }

  aTextRect.width =
    nsLayoutUtils::GetStringWidth(this, &aRenderingContext, aText.get(), aText.Length());
}

nsIAtom*
nsTreeBodyFrame::GetItemWithinCellAt(nscoord aX, const nsRect& aCellRect, 
                                     PRInt32 aRowIndex,
                                     nsTreeColumn* aColumn)
{
  NS_PRECONDITION(aColumn && aColumn->GetFrame(this), "invalid column passed");

  // Obtain the properties for our cell.
  PrefillPropertyArray(aRowIndex, aColumn);
  mView->GetCellProperties(aRowIndex, aColumn, mScratchArray);

  // Resolve style for the cell.
  nsStyleContext* cellContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreecell);

  // Obtain the margins for the cell and then deflate our rect by that 
  // amount.  The cell is assumed to be contained within the deflated rect.
  nsRect cellRect(aCellRect);
  nsMargin cellMargin;
  cellContext->GetStyleMargin()->GetMargin(cellMargin);
  cellRect.Deflate(cellMargin);

  // Adjust the rect for its border and padding.
  AdjustForBorderPadding(cellContext, cellRect);

  if (aX < cellRect.x || aX >= cellRect.x + cellRect.width) {
    // The user clicked within the cell's margins/borders/padding.  This constitutes a click on the cell.
    return nsCSSAnonBoxes::moztreecell;
  }

  nscoord currX = cellRect.x;
  nscoord remainingWidth = cellRect.width;

  // XXX Handle right alignment hit testing.

  if (aColumn->IsPrimary()) {
    // If we're the primary column, we have indentation and a twisty.
    PRInt32 level;
    mView->GetLevel(aRowIndex, &level);

    currX += mIndentation*level;
    remainingWidth -= mIndentation*level;

    if (aX < currX) {
      // The user clicked within the indentation.
      return nsCSSAnonBoxes::moztreecell;
    }

    // Always leave space for the twisty.
    nsRect twistyRect(currX, cellRect.y, remainingWidth, cellRect.height);
    PRBool hasTwisty = PR_FALSE;
    PRBool isContainer = PR_FALSE;
    mView->IsContainer(aRowIndex, &isContainer);
    if (isContainer) {
      PRBool isContainerEmpty = PR_FALSE;
      mView->IsContainerEmpty(aRowIndex, &isContainerEmpty);
      if (!isContainerEmpty)
        hasTwisty = PR_TRUE;
    }

    nsPresContext* presContext = GetPresContext();
    nsCOMPtr<nsIRenderingContext> rc;
    presContext->PresShell()->CreateRenderingContext(this, getter_AddRefs(rc));

    // Resolve style for the twisty.
    nsStyleContext* twistyContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreetwisty);

    nsRect imageSize;
    GetTwistyRect(aRowIndex, aColumn, imageSize, twistyRect, presContext,
                  *rc, twistyContext);

    // We will treat a click as hitting the twisty if it happens on the margins, borders, padding,
    // or content of the twisty object.  By allowing a "slop" into the margin, we make it a little
    // bit easier for a user to hit the twisty.  (We don't want to be too picky here.)
    nsMargin twistyMargin;
    twistyContext->GetStyleMargin()->GetMargin(twistyMargin);
    twistyRect.Inflate(twistyMargin);

    // Now we test to see if aX is actually within the twistyRect.  If it is, and if the item should
    // have a twisty, then we return "twisty".  If it is within the rect but we shouldn't have a twisty,
    // then we return "cell".
    if (aX >= twistyRect.x && aX < twistyRect.x + twistyRect.width) {
      if (hasTwisty)
        return nsCSSAnonBoxes::moztreetwisty;
      else
        return nsCSSAnonBoxes::moztreecell;
    }

    currX += twistyRect.width;
    remainingWidth -= twistyRect.width;    
  }
  
  // Now test to see if the user hit the icon for the cell.
  nsRect iconRect(currX, cellRect.y, remainingWidth, cellRect.height);
  
  // Resolve style for the image.
  nsStyleContext* imageContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreeimage);

  nsRect iconSize = GetImageSize(aRowIndex, aColumn, PR_FALSE, imageContext);
  nsMargin imageMargin;
  imageContext->GetStyleMargin()->GetMargin(imageMargin);
  iconSize.Inflate(imageMargin);
  iconRect.width = iconSize.width;

  if (aX >= iconRect.x && aX < iconRect.x + iconRect.width) {
    // The user clicked on the image.
    return nsCSSAnonBoxes::moztreeimage;
  }

  currX += iconRect.width;
  remainingWidth -= iconRect.width;    

  nsAutoString cellText;
  mView->GetCellText(aRowIndex, aColumn, cellText);
  // We're going to measure this text so we need to ensure bidi is enabled if
  // necessary
  CheckTextForBidi(cellText);

  nsRect textRect(currX, cellRect.y, remainingWidth, cellRect.height);

  nsStyleContext* textContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreecelltext);

  nsMargin textMargin;
  textContext->GetStyleMargin()->GetMargin(textMargin);
  textRect.Deflate(textMargin);

  AdjustForBorderPadding(textContext, textRect);

  nsCOMPtr<nsIRenderingContext> renderingContext;
  GetPresContext()->PresShell()->CreateRenderingContext(this, getter_AddRefs(renderingContext));

  renderingContext->SetFont(textContext->GetStyleFont()->mFont, nsnull);

  AdjustForCellText(cellText, aRowIndex, aColumn, *renderingContext, textRect);

  if (aX >= textRect.x && aX < textRect.x + textRect.width)
    return nsCSSAnonBoxes::moztreecelltext;
  else
    return nsCSSAnonBoxes::moztreecell;
}

void
nsTreeBodyFrame::GetCellAt(nscoord aX, nscoord aY, PRInt32* aRow,
                           nsTreeColumn** aCol, nsIAtom** aChildElt)
{
  *aCol = nsnull;
  *aChildElt = nsnull;

  *aRow = GetRowAt(aX, aY);
  if (*aRow < 0)
    return;

  // Determine the column hit.
  for (nsTreeColumn* currCol = mColumns->GetFirstColumn(); currCol; 
       currCol = currCol->GetNext()) {
    nsRect cellRect;
    nsresult rv = currCol->GetRect(this,
                                   mInnerBox.y +
                                         mRowHeight * (*aRow - mTopRowIndex),
                                   mRowHeight,
                                   &cellRect);
    if (NS_FAILED(rv)) {
      NS_NOTREACHED("column has no frame");
      continue;
    }

    if (!OffsetForHorzScroll(cellRect, PR_TRUE))
      continue;

    PRInt32 overflow = cellRect.x+cellRect.width-(mInnerBox.x+mInnerBox.width);
    if (overflow > 0)
      cellRect.width -= overflow;

    if (aX >= cellRect.x && aX < cellRect.x + cellRect.width) {
      // We know the column hit now.
      if (aCol)
        *aCol = currCol;

      if (currCol->IsCycler())
        // Cyclers contain only images.  Fill this in immediately and return.
        *aChildElt = nsCSSAnonBoxes::moztreeimage;
      else
        *aChildElt = GetItemWithinCellAt(aX, cellRect, *aRow, currCol);
      break;
    }
  }
}

nsresult
nsTreeBodyFrame::GetCellWidth(PRInt32 aRow, nsTreeColumn* aCol,
                              nsIRenderingContext* aRenderingContext,
                              nscoord& aDesiredSize, nscoord& aCurrentSize)
{
  NS_PRECONDITION(aCol, "aCol must not be null");
  NS_PRECONDITION(aRenderingContext, "aRenderingContext must not be null");

  // The rect for the current cell.
  nscoord colWidth;
  nsresult rv = aCol->GetWidthInTwips(this, &colWidth);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRect cellRect(0, 0, colWidth, mRowHeight);

  PRInt32 overflow = cellRect.x+cellRect.width-(mInnerBox.x+mInnerBox.width);
  if (overflow > 0)
    cellRect.width -= overflow;

  // Adjust borders and padding for the cell.
  nsStyleContext* cellContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreecell);
  nsMargin bp(0,0,0,0);
  GetBorderPadding(cellContext, bp);

  aCurrentSize = cellRect.width;
  aDesiredSize = bp.left + bp.right;

  if (aCol->IsPrimary()) {
    // If the current Column is a Primary, then we need to take into account 
    // the indentation and possibly a twisty. 

    // The amount of indentation is the indentation width (|mIndentation|) by the level.
    PRInt32 level;
    mView->GetLevel(aRow, &level);
    aDesiredSize += mIndentation * level;
    
    // Find the twisty rect by computing its size.
    nsStyleContext* twistyContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreetwisty);

    nsRect imageSize;
    nsRect twistyRect(cellRect);
    GetTwistyRect(aRow, aCol, imageSize, twistyRect, GetPresContext(),
                  *aRenderingContext, twistyContext);

    // Add in the margins of the twisty element.
    nsMargin twistyMargin;
    twistyContext->GetStyleMargin()->GetMargin(twistyMargin);
    twistyRect.Inflate(twistyMargin);

    aDesiredSize += twistyRect.width;
  }

  nsStyleContext* imageContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreeimage);

  // Account for the width of the cell image.
  nsRect imageSize = GetImageSize(aRow, aCol, PR_FALSE, imageContext);
  // Add in the margins of the cell image.
  nsMargin imageMargin;
  imageContext->GetStyleMargin()->GetMargin(imageMargin);
  imageSize.Inflate(imageMargin);

  aDesiredSize += imageSize.width;
  
  // Get the cell text.
  nsAutoString cellText;
  mView->GetCellText(aRow, aCol, cellText);
  // We're going to measure this text so we need to ensure bidi is enabled if
  // necessary
  CheckTextForBidi(cellText);

  nsStyleContext* textContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreecelltext);

  // Get the borders and padding for the text.
  GetBorderPadding(textContext, bp);
  
  // Get the font style for the text and pass it to the rendering context.
  aRenderingContext->SetFont(textContext->GetStyleFont()->mFont, nsnull);

  // Get the width of the text itself
  nscoord width =
    nsLayoutUtils::GetStringWidth(this, aRenderingContext, cellText.get(), cellText.Length());
  nscoord totalTextWidth = width + bp.left + bp.right;
  aDesiredSize += totalTextWidth;
  return NS_OK;
}

NS_IMETHODIMP
nsTreeBodyFrame::IsCellCropped(PRInt32 aRow, nsITreeColumn* aCol, PRBool *_retval)
{  
  nscoord currentSize, desiredSize;
  nsresult rv;

  nsRefPtr<nsTreeColumn> col = GetColumnImpl(aCol);
  if (!col)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIRenderingContext> rc;
  rv = GetPresContext()->PresShell()->
    CreateRenderingContext(this, getter_AddRefs(rc));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = GetCellWidth(aRow, col, rc, desiredSize, currentSize);
  NS_ENSURE_SUCCESS(rv, rv);

  *_retval = desiredSize > currentSize;

  return NS_OK;
}

void
nsTreeBodyFrame::MarkDirtyIfSelect()
{
  nsIContent* baseElement = GetBaseElement();

  if (baseElement && baseElement->Tag() == nsGkAtoms::select &&
      baseElement->IsNodeOfType(nsINode::eHTML)) {
    // If we are an intrinsically sized select widget, we may need to
    // resize, if the widest item was removed or a new item was added.
    // XXX optimize this more

    mStringWidth = -1;
    AddStateBits(NS_FRAME_IS_DIRTY);
    GetPresContext()->PresShell()->FrameNeedsReflow(this, nsIPresShell::eTreeChange);
  }
}

nsresult
nsTreeBodyFrame::CreateTimer(const nsILookAndFeel::nsMetricID aID,
                             nsTimerCallbackFunc aFunc, PRInt32 aType,
                             nsITimer** aTimer)
{
  // Get the delay from the look and feel service.
  PRInt32 delay = 0;
  GetPresContext()->LookAndFeel()->GetMetric(aID, delay);

  nsCOMPtr<nsITimer> timer;

  // Create a new timer only if the delay is greater than zero.
  // Zero value means that this feature is completely disabled.
  if (delay > 0) {
    timer = do_CreateInstance("@mozilla.org/timer;1");
    if (timer)
      timer->InitWithFuncCallback(aFunc, this, delay, aType);
  }

  NS_IF_ADDREF(*aTimer = timer);

  return NS_OK;
}

NS_IMETHODIMP nsTreeBodyFrame::RowCountChanged(PRInt32 aIndex, PRInt32 aCount)
{
  if (aCount == 0 || !mView)
    return NS_OK; // Nothing to do.

  // Adjust our selection.
  nsCOMPtr<nsITreeSelection> sel;
  mView->GetSelection(getter_AddRefs(sel));
  if (sel)
    sel->AdjustSelection(aIndex, aCount);

  if (mUpdateBatchNest)
    return NS_OK;

  mRowCount += aCount;
#ifdef DEBUG
  PRInt32 rowCount = mRowCount;
  mView->GetRowCount(&rowCount);
  NS_ASSERTION(rowCount == mRowCount, "row count did not change by the amount suggested, check caller");
#endif

  PRInt32 count = PR_ABS(aCount);
  PRInt32 last = GetLastVisibleRow();
  if (aIndex >= mTopRowIndex && aIndex <= last)
    InvalidateRange(aIndex, last);
    
  ScrollParts parts = GetScrollParts();

  if (mTopRowIndex == 0) {    
    // Just update the scrollbar and return.
    InvalidateScrollbars(parts);
    CheckOverflow(parts);
    MarkDirtyIfSelect();
    return NS_OK;
  }

  // Adjust our top row index.
  if (aCount > 0) {
    if (mTopRowIndex > aIndex) {
      // Rows came in above us.  Augment the top row index.
      mTopRowIndex += aCount;
      UpdateScrollbars(parts);
    }
  }
  else if (aCount < 0) {
    if (mTopRowIndex > aIndex+count-1) {
      // No need to invalidate. The remove happened
      // completely above us (offscreen).
      mTopRowIndex -= count;
      UpdateScrollbars(parts);
    }
    else if (mTopRowIndex >= aIndex) {
      // This is a full-blown invalidate.
      if (mTopRowIndex + mPageLength > mRowCount - 1) {
        mTopRowIndex = PR_MAX(0, mRowCount - 1 - mPageLength);
        UpdateScrollbars(parts);
      }
      Invalidate();
    }
  }

  InvalidateScrollbars(parts);
  CheckOverflow(parts);
  MarkDirtyIfSelect();

  return NS_OK;
}

NS_IMETHODIMP nsTreeBodyFrame::BeginUpdateBatch()
{
  ++mUpdateBatchNest;

  return NS_OK;
}

NS_IMETHODIMP nsTreeBodyFrame::EndUpdateBatch()
{
  NS_ASSERTION(mUpdateBatchNest > 0, "badly nested update batch");

  if (--mUpdateBatchNest == 0) {
    if (mView) {
      Invalidate();
      PRInt32 countBeforeUpdate = mRowCount;
      mView->GetRowCount(&mRowCount);
      if (countBeforeUpdate != mRowCount) {
        ScrollParts parts = GetScrollParts();
        
        if (mTopRowIndex + mPageLength > mRowCount - 1) {
          mTopRowIndex = PR_MAX(0, mRowCount - 1 - mPageLength);
          UpdateScrollbars(parts);
        }
        InvalidateScrollbars(parts);
        CheckOverflow(parts);
      }
    }
  }

  return NS_OK;
}

void
nsTreeBodyFrame::PrefillPropertyArray(PRInt32 aRowIndex, nsTreeColumn* aCol)
{
  NS_PRECONDITION(!aCol || aCol->GetFrame(this), "invalid column passed");
  mScratchArray->Clear();
  
  // focus
  if (mFocused)
    mScratchArray->AppendElement(nsGkAtoms::focus);

  // sort
  PRBool sorted = PR_FALSE;
  mView->IsSorted(&sorted);
  if (sorted)
    mScratchArray->AppendElement(nsGkAtoms::sorted);

  // drag session
  if (mSlots && mSlots->mDragSession)
    mScratchArray->AppendElement(nsGkAtoms::dragSession);

  if (aRowIndex != -1) {
    nsCOMPtr<nsITreeSelection> selection;
    mView->GetSelection(getter_AddRefs(selection));
  
    if (selection) {
      // selected
      PRBool isSelected;
      selection->IsSelected(aRowIndex, &isSelected);
      if (isSelected)
        mScratchArray->AppendElement(nsGkAtoms::selected);

      // current
      PRInt32 currentIndex;
      selection->GetCurrentIndex(&currentIndex);
      if (aRowIndex == currentIndex)
        mScratchArray->AppendElement(nsGkAtoms::current);
  
      // active
      if (aCol) {
        nsCOMPtr<nsITreeColumn> currentColumn;
        selection->GetCurrentColumn(getter_AddRefs(currentColumn));
        if (aCol == currentColumn)
          mScratchArray->AppendElement(nsGkAtoms::active);
      }
    }

    // container or leaf
    PRBool isContainer = PR_FALSE;
    mView->IsContainer(aRowIndex, &isContainer);
    if (isContainer) {
      mScratchArray->AppendElement(nsGkAtoms::container);

      // open or closed
      PRBool isOpen = PR_FALSE;
      mView->IsContainerOpen(aRowIndex, &isOpen);
      if (isOpen)
        mScratchArray->AppendElement(nsGkAtoms::open);
      else
        mScratchArray->AppendElement(nsGkAtoms::closed);
    }
    else {
      mScratchArray->AppendElement(nsGkAtoms::leaf);
    }

    // drop orientation
    if (mSlots && mSlots->mDropAllowed && mSlots->mDropRow == aRowIndex) {
      if (mSlots->mDropOrient == nsITreeView::DROP_BEFORE)
        mScratchArray->AppendElement(nsGkAtoms::dropBefore);
      else if (mSlots->mDropOrient == nsITreeView::DROP_ON)
        mScratchArray->AppendElement(nsGkAtoms::dropOn);
      else if (mSlots->mDropOrient == nsITreeView::DROP_AFTER)
        mScratchArray->AppendElement(nsGkAtoms::dropAfter);
    }

    // odd or even
    if (aRowIndex % 2)
      mScratchArray->AppendElement(nsGkAtoms::odd);
    else
      mScratchArray->AppendElement(nsGkAtoms::even);

    nsIContent* baseContent = GetBaseElement();
    if (baseContent && baseContent->HasAttr(kNameSpaceID_None, nsGkAtoms::editing))
      mScratchArray->AppendElement(nsGkAtoms::editing);
  }

  if (aCol) {
    mScratchArray->AppendElement(aCol->GetAtom());

    if (aCol->IsPrimary())
      mScratchArray->AppendElement(nsGkAtoms::primary);

    if (aCol->GetType() == nsITreeColumn::TYPE_CHECKBOX) {
      mScratchArray->AppendElement(nsGkAtoms::checkbox);

      if (aRowIndex != -1) {
        nsAutoString value;
        mView->GetCellValue(aRowIndex, aCol, value);
        if (value.EqualsLiteral("true"))
          mScratchArray->AppendElement(nsGkAtoms::checked);
      }
    }
    else if (aCol->GetType() == nsITreeColumn::TYPE_PROGRESSMETER) {
      mScratchArray->AppendElement(nsGkAtoms::progressmeter);

      if (aRowIndex != -1) {
        PRInt32 state;
        mView->GetProgressMode(aRowIndex, aCol, &state);
        if (state == nsITreeView::PROGRESS_NORMAL)
          mScratchArray->AppendElement(nsGkAtoms::progressNormal);
        else if (state == nsITreeView::PROGRESS_UNDETERMINED)
          mScratchArray->AppendElement(nsGkAtoms::progressUndetermined);
      }
    }

    // Read special properties from attributes on the column content node
    if (aCol->mContent->AttrValueIs(kNameSpaceID_None,
                                    nsGkAtoms::insertbefore,
                                    nsGkAtoms::_true, eCaseMatters))
      mScratchArray->AppendElement(nsGkAtoms::insertbefore);
    if (aCol->mContent->AttrValueIs(kNameSpaceID_None,
                                    nsGkAtoms::insertafter,
                                    nsGkAtoms::_true, eCaseMatters))
      mScratchArray->AppendElement(nsGkAtoms::insertafter);
  }
}

nsITheme*
nsTreeBodyFrame::GetTwistyRect(PRInt32 aRowIndex,
                               nsTreeColumn* aColumn,
                               nsRect& aImageRect,
                               nsRect& aTwistyRect,
                               nsPresContext* aPresContext,
                               nsIRenderingContext& aRenderingContext,
                               nsStyleContext* aTwistyContext)
{
  // The twisty rect extends all the way to the end of the cell.  This is incorrect.  We need to
  // determine the twisty rect's true width.  This is done by examining the style context for
  // a width first.  If it has one, we use that.  If it doesn't, we use the image's natural width.
  // If the image hasn't loaded and if no width is specified, then we just bail. If there is
  // a -moz-appearance involved, adjust the rect by the minimum widget size provided by
  // the theme implementation.
  aImageRect = GetImageSize(aRowIndex, aColumn, PR_TRUE, aTwistyContext);
  if (aImageRect.height > aTwistyRect.height)
    aImageRect.height = aTwistyRect.height;
  if (aImageRect.width > aTwistyRect.width)
    aImageRect.width = aTwistyRect.width;
  else
    aTwistyRect.width = aImageRect.width;

  PRBool useTheme = PR_FALSE;
  nsITheme *theme = nsnull;
  const nsStyleDisplay* twistyDisplayData = aTwistyContext->GetStyleDisplay();
  if (twistyDisplayData->mAppearance) {
    theme = aPresContext->GetTheme();
    if (theme && theme->ThemeSupportsWidget(aPresContext, nsnull, twistyDisplayData->mAppearance))
      useTheme = PR_TRUE;
  }

  if (useTheme) {
    nsSize minTwistySize(0,0);
    PRBool canOverride = PR_TRUE;
    theme->GetMinimumWidgetSize(&aRenderingContext, this, twistyDisplayData->mAppearance,
                                &minTwistySize, &canOverride);

    // GMWS() returns size in pixels, we need to convert it back to twips
    minTwistySize.width = aPresContext->DevPixelsToAppUnits(minTwistySize.width);
    minTwistySize.height = aPresContext->DevPixelsToAppUnits(minTwistySize.height);

    if (aTwistyRect.width < minTwistySize.width || !canOverride)
      aTwistyRect.width = minTwistySize.width;
  }

  return useTheme ? theme : nsnull;
}

nsresult
nsTreeBodyFrame::GetImage(PRInt32 aRowIndex, nsTreeColumn* aCol, PRBool aUseContext,
                          nsStyleContext* aStyleContext, PRBool& aAllowImageRegions, imgIContainer** aResult)
{
  *aResult = nsnull;

  nsAutoString imageSrc;
  mView->GetImageSrc(aRowIndex, aCol, imageSrc);
  nsCOMPtr<imgIRequest> styleRequest;
  if (!aUseContext && !imageSrc.IsEmpty()) {
    aAllowImageRegions = PR_FALSE;
  }
  else {
    // Obtain the URL from the style context.
    aAllowImageRegions = PR_TRUE;
    styleRequest = aStyleContext->GetStyleList()->mListStyleImage;
    if (!styleRequest)
      return NS_OK;
    nsCOMPtr<nsIURI> uri;
    styleRequest->GetURI(getter_AddRefs(uri));
    nsCAutoString spec;
    uri->GetSpec(spec);
    CopyUTF8toUTF16(spec, imageSrc);
  }

  // Look the image up in our cache.
  nsTreeImageCacheEntry entry;
  if (mImageCache.Get(imageSrc, &entry)) {
    // Find out if the image has loaded.
    PRUint32 status;
    imgIRequest *imgReq = entry.request;
    imgReq->GetImageStatus(&status);
    imgReq->GetImage(aResult); // We hand back the image here.  The GetImage call addrefs *aResult.
    PRUint32 numFrames = 1;
    if (*aResult)
      (*aResult)->GetNumFrames(&numFrames);

    if ((!(status & imgIRequest::STATUS_LOAD_COMPLETE)) || numFrames > 1) {
      // We either aren't done loading, or we're animating. Add our row as a listener for invalidations.
      nsCOMPtr<imgIDecoderObserver> obs;
      imgReq->GetDecoderObserver(getter_AddRefs(obs));
      nsCOMPtr<nsITreeImageListener> listener(do_QueryInterface(obs));
      if (listener)
        listener->AddCell(aRowIndex, aCol);
      return NS_OK;
    }
  }

  if (!*aResult) {
    // Create a new nsTreeImageListener object and pass it our row and column
    // information.
    nsTreeImageListener* listener = new nsTreeImageListener(mTreeBoxObject);
    if (!listener)
      return NS_ERROR_OUT_OF_MEMORY;

    listener->AddCell(aRowIndex, aCol);
    nsCOMPtr<imgIDecoderObserver> imgDecoderObserver = listener;

    nsCOMPtr<imgIRequest> imageRequest;
    if (styleRequest) {
      styleRequest->Clone(imgDecoderObserver, getter_AddRefs(imageRequest));
    } else {
      nsIDocument* doc = mContent->GetDocument();
      if (!doc)
        // The page is currently being torn down.  Why bother.
        return NS_ERROR_FAILURE;

      nsCOMPtr<nsIURI> baseURI = mContent->GetBaseURI();

      nsCOMPtr<nsIURI> srcURI;
      nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(srcURI),
                                                imageSrc,
                                                doc,
                                                baseURI);
      if (!srcURI)
        return NS_ERROR_FAILURE;

      if (nsContentUtils::CanLoadImage(srcURI, mContent, doc)) {
        nsresult rv = nsContentUtils::LoadImage(srcURI,
                                                doc,
                                                doc->GetDocumentURI(),
                                                imgDecoderObserver,
                                                nsIRequest::LOAD_NORMAL,
                                                getter_AddRefs(imageRequest));
        NS_ENSURE_SUCCESS(rv, rv);
                                  
      }
    }
    listener->UnsuppressInvalidation();

    if (!imageRequest)
      return NS_ERROR_FAILURE;

    // In a case it was already cached.
    imageRequest->GetImage(aResult);
    nsTreeImageCacheEntry cacheEntry(imageRequest, imgDecoderObserver);
    mImageCache.Put(imageSrc, cacheEntry);
  }
  return NS_OK;
}

nsRect nsTreeBodyFrame::GetImageSize(PRInt32 aRowIndex, nsTreeColumn* aCol, PRBool aUseContext,
                                     nsStyleContext* aStyleContext)
{
  // XXX We should respond to visibility rules for collapsed vs. hidden.

  // This method returns the width of the twisty INCLUDING borders and padding.
  // It first checks the style context for a width.  If none is found, it tries to
  // use the default image width for the twisty.  If no image is found, it defaults
  // to border+padding.
  nsRect r(0,0,0,0);
  nsMargin bp(0,0,0,0);
  GetBorderPadding(aStyleContext, bp);
  r.Inflate(bp);

  // Now r contains our border+padding info.  We now need to get our width and
  // height.
  PRBool needWidth = PR_FALSE;
  PRBool needHeight = PR_FALSE;

  // We have to load image even though we already have a size.
  // Don't change this, otherwise things start to go crazy.
  PRBool useImageRegion = PR_TRUE;
  nsCOMPtr<imgIContainer> image;
  GetImage(aRowIndex, aCol, aUseContext, aStyleContext, useImageRegion, getter_AddRefs(image));

  const nsStylePosition* myPosition = aStyleContext->GetStylePosition();
  const nsStyleList* myList = aStyleContext->GetStyleList();

  if (useImageRegion) {
    r.x += myList->mImageRegion.x;
    r.y += myList->mImageRegion.y;
  }

  if (myPosition->mWidth.GetUnit() == eStyleUnit_Coord)  {
    PRInt32 val = myPosition->mWidth.GetCoordValue();
    r.width += val;
  }
  else if (useImageRegion && myList->mImageRegion.width > 0)
    r.width += myList->mImageRegion.width;
  else 
    needWidth = PR_TRUE;

  if (myPosition->mHeight.GetUnit() == eStyleUnit_Coord)  {
    PRInt32 val = myPosition->mHeight.GetCoordValue();
    r.height += val;
  }
  else if (useImageRegion && myList->mImageRegion.height > 0)
    r.height += myList->mImageRegion.height;
  else 
    needHeight = PR_TRUE;

  if (image) {
    if (needWidth || needHeight) {
      // Get the natural image size.

      if (needWidth) {
        // Get the size from the image.
        nscoord width;
        image->GetWidth(&width);
        r.width += nsPresContext::CSSPixelsToAppUnits(width); 
      }
    
      if (needHeight) {
        nscoord height;
        image->GetHeight(&height);
        r.height += nsPresContext::CSSPixelsToAppUnits(height); 
      }
    }
  }

  return r;
}

// GetImageDestSize returns the destination size of the image.
// The width and height do not include borders and padding.
// The width and height have not been adjusted to fit in the row height
// or cell width.
// The width and height reflect the destination size specified in CSS,
// or the image region specified in CSS, or the natural size of the
// image.
// If only the destination width has been specified in CSS, the height is
// calculated to maintain the aspect ratio of the image.
// If only the destination height has been specified in CSS, the width is
// calculated to maintain the aspect ratio of the image.
nsSize
nsTreeBodyFrame::GetImageDestSize(nsStyleContext* aStyleContext,
                                  PRBool useImageRegion,
                                  imgIContainer* image)
{
  nsSize size(0,0);

  // We need to get the width and height.
  PRBool needWidth = PR_FALSE;
  PRBool needHeight = PR_FALSE;

  // Get the style position to see if the CSS has specified the
  // destination width/height.
  const nsStylePosition* myPosition = aStyleContext->GetStylePosition();

  if (myPosition->mWidth.GetUnit() == eStyleUnit_Coord) {
    // CSS has specified the destination width.
    size.width = myPosition->mWidth.GetCoordValue();
  }
  else {
    // We'll need to get the width of the image/region.
    needWidth = PR_TRUE;
  }

  if (myPosition->mHeight.GetUnit() == eStyleUnit_Coord)  {
    // CSS has specified the destination height.
    size.height = myPosition->mHeight.GetCoordValue();
  }
  else {
    // We'll need to get the height of the image/region.
    needHeight = PR_TRUE;
  }

  if (needWidth || needHeight) {
    // We need to get the size of the image/region.
    nsSize imageSize(0,0);

    const nsStyleList* myList = aStyleContext->GetStyleList();

    if (useImageRegion && myList->mImageRegion.width > 0) {
      // CSS has specified an image region.
      // Use the width of the image region.
      imageSize.width = myList->mImageRegion.width;
    }
    else if (image) {
      nscoord width;
      image->GetWidth(&width);
      imageSize.width = nsPresContext::CSSPixelsToAppUnits(width);
    }

    if (useImageRegion && myList->mImageRegion.height > 0) {
      // CSS has specified an image region.
      // Use the height of the image region.
      imageSize.height = myList->mImageRegion.height;
    }
    else if (image) {
      nscoord height;
      image->GetHeight(&height);
      imageSize.height = nsPresContext::CSSPixelsToAppUnits(height);
    }

    if (needWidth) {
      if (!needHeight && imageSize.height != 0) {
        // The CSS specified the destination height, but not the destination
        // width. We need to calculate the width so that we maintain the
        // image's aspect ratio.
        size.width = imageSize.width * size.height / imageSize.height;
      }
      else {
        size.width = imageSize.width;
      }
    }

    if (needHeight) {
      if (!needWidth && imageSize.width != 0) {
        // The CSS specified the destination width, but not the destination
        // height. We need to calculate the height so that we maintain the
        // image's aspect ratio.
        size.height = imageSize.height * size.width / imageSize.width;
      }
      else {
        size.height = imageSize.height;
      }
    }
  }

  return size;
}

// GetImageSourceRect returns the source rectangle of the image to be
// displayed.
// The width and height reflect the image region specified in CSS, or
// the natural size of the image.
// The width and height do not include borders and padding.
// The width and height do not reflect the destination size specified
// in CSS.
nsRect
nsTreeBodyFrame::GetImageSourceRect(nsStyleContext* aStyleContext,
                                    PRBool useImageRegion,
                                    imgIContainer* image)
{
  nsRect r(0,0,0,0);

  const nsStyleList* myList = aStyleContext->GetStyleList();

  if (useImageRegion &&
      (myList->mImageRegion.width > 0 || myList->mImageRegion.height > 0)) {
    // CSS has specified an image region.
    r = myList->mImageRegion;
  }
  else if (image) {
    // Use the actual image size.
    nscoord coord;
    image->GetWidth(&coord);
    r.width = nsPresContext::CSSPixelsToAppUnits(coord);
    image->GetHeight(&coord);
    r.height = nsPresContext::CSSPixelsToAppUnits(coord);
  }

  return r;
}

PRInt32 nsTreeBodyFrame::GetRowHeight()
{
  // Look up the correct height.  It is equal to the specified height
  // + the specified margins.
  mScratchArray->Clear();
  nsStyleContext* rowContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreerow);
  if (rowContext) {
    const nsStylePosition* myPosition = rowContext->GetStylePosition();

    nscoord minHeight = 0;
    if (myPosition->mMinHeight.GetUnit() == eStyleUnit_Coord)
      minHeight = myPosition->mMinHeight.GetCoordValue();

    nscoord height = 0;
    if (myPosition->mHeight.GetUnit() == eStyleUnit_Coord)
      height = myPosition->mHeight.GetCoordValue();

    if (height < minHeight)
      height = minHeight;

    if (height > 0) {
      height = nsPresContext::AppUnitsToIntCSSPixels(height);
      height += height % 2;
      height = nsPresContext::CSSPixelsToAppUnits(height);

      // XXX Check box-sizing to determine if border/padding should augment the height
      // Inflate the height by our margins.
      nsRect rowRect(0,0,0,height);
      nsMargin rowMargin;
      rowContext->GetStyleMargin()->GetMargin(rowMargin);
      rowRect.Inflate(rowMargin);
      height = rowRect.height;
      return height;
    }
  }

  return nsPresContext::CSSPixelsToAppUnits(18); // As good a default as any.
}

PRInt32 nsTreeBodyFrame::GetIndentation()
{
  // Look up the correct indentation.  It is equal to the specified indentation width.
  mScratchArray->Clear();
  nsStyleContext* indentContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreeindentation);
  if (indentContext) {
    const nsStylePosition* myPosition = indentContext->GetStylePosition();
    if (myPosition->mWidth.GetUnit() == eStyleUnit_Coord)  {
      nscoord val = myPosition->mWidth.GetCoordValue();
      return val;
    }
  }

  return nsPresContext::CSSPixelsToAppUnits(16); // As good a default as any.
}

void nsTreeBodyFrame::CalcInnerBox()
{
  mInnerBox.SetRect(0, 0, mRect.width, mRect.height);
  AdjustForBorderPadding(mStyleContext, mInnerBox);
}

nscoord
nsTreeBodyFrame::CalcHorzWidth(const ScrollParts& aParts)
{
  nscoord width = 0;
  nscoord height;

  // We calculate this from the scrollable view, so that it 
  // properly covers all contingencies of what could be 
  // scrollable (columns, body, etc...)

  if (aParts.mColumnsScrollableView) {
    if (NS_FAILED (aParts.mColumnsScrollableView->GetContainerSize(&width, &height)))
      width = 0;
  }

  // If no horz scrolling periphery is present, then just
  // return the width of the main box
  if (width == 0) {
    CalcInnerBox();
    width = mInnerBox.width;
  }

  return width;
}

NS_IMETHODIMP
nsTreeBodyFrame::GetCursor(const nsPoint& aPoint,
                           nsIFrame::Cursor& aCursor)
{
  if (mView) {
    PRInt32 row;
    nsTreeColumn* col;
    nsIAtom* child;
    GetCellAt(aPoint.x, aPoint.y, &row, &col, &child);

    if (child) {
      // Our scratch array is already prefilled.
      nsStyleContext* childContext = GetPseudoStyleContext(child);

      FillCursorInformationFromStyle(childContext->GetStyleUserInterface(),
                                     aCursor);
      if (aCursor.mCursor == NS_STYLE_CURSOR_AUTO)
        aCursor.mCursor = NS_STYLE_CURSOR_DEFAULT;

      return NS_OK;
    }
  }

  return nsLeafBoxFrame::GetCursor(aPoint, aCursor);
}

NS_IMETHODIMP
nsTreeBodyFrame::HandleEvent(nsPresContext* aPresContext,
                             nsGUIEvent* aEvent,
                             nsEventStatus* aEventStatus)
{
  if (aEvent->message == NS_DRAGDROP_ENTER) {
    if (!mSlots)
      mSlots = new Slots();

    // Cache several things we'll need throughout the course of our work. These
    // will all get released on a drag exit.

    if (mSlots->mTimer) {
      mSlots->mTimer->Cancel();
      mSlots->mTimer = nsnull;
    }

    // Cache the drag session.
    nsCOMPtr<nsIDragService> dragService = 
             do_GetService("@mozilla.org/widget/dragservice;1");
    dragService->GetCurrentSession(getter_AddRefs(mSlots->mDragSession));
    NS_ASSERTION(mSlots->mDragSession, "can't get drag session");

    if (mSlots->mDragSession)
      mSlots->mDragSession->GetDragAction(&mSlots->mDragAction);
    else
      mSlots->mDragAction = 0;
  }
  else if (aEvent->message == NS_DRAGDROP_OVER) {
    // The mouse is hovering over this tree. If we determine things are
    // different from the last time, invalidate the drop feedback at the old
    // position, query the view to see if the current location is droppable,
    // and then invalidate the drop feedback at the new location if it is.
    // The mouse may or may not have changed position from the last time
    // we were called, so optimize out a lot of the extra notifications by
    // checking if anything changed first. For drop feedback we use drop,
    // dropBefore and dropAfter property.

    if (!mView || !mSlots)
      return NS_OK;

    // Save last values, we will need them.
    PRInt32 lastDropRow = mSlots->mDropRow;
    PRInt16 lastDropOrient = mSlots->mDropOrient;
    PRInt16 lastScrollLines = mSlots->mScrollLines;

    // Find out the current drag action
    PRUint32 lastDragAction = mSlots->mDragAction;
    if (mSlots->mDragSession)
      mSlots->mDragSession->GetDragAction(&mSlots->mDragAction);

    // Compute the row mouse is over and the above/below/on state.
    // Below we'll use this to see if anything changed.
    // Also check if we want to auto-scroll.
    ComputeDropPosition(aEvent, &mSlots->mDropRow, &mSlots->mDropOrient, &mSlots->mScrollLines);

    // While we're here, handle tracking of scrolling during a drag.
    if (mSlots->mScrollLines) {
      if (mSlots->mDropAllowed) {
        // Invalidate primary cell at old location.
        mSlots->mDropAllowed = PR_FALSE;
        InvalidateDropFeedback(lastDropRow, lastDropOrient);
      }
#if !defined(XP_MACOSX)
      if (!lastScrollLines) {
        // Cancel any previously initialized timer.
        if (mSlots->mTimer) {
          mSlots->mTimer->Cancel();
          mSlots->mTimer = nsnull;
        }

        // Set a timer to trigger the tree scrolling.
        CreateTimer(nsILookAndFeel::eMetric_TreeLazyScrollDelay,
                    LazyScrollCallback, nsITimer::TYPE_ONE_SHOT,
                    getter_AddRefs(mSlots->mTimer));
       }
#else
      ScrollByLines(mSlots->mScrollLines);
#endif
      // Bail out to prevent spring loaded timer and feedback line settings.
      return NS_OK;
    }

    // If changed from last time, invalidate primary cell at the old location and if allowed, 
    // invalidate primary cell at the new location. If nothing changed, just bail.
    if (mSlots->mDropRow != lastDropRow ||
        mSlots->mDropOrient != lastDropOrient ||
        mSlots->mDragAction != lastDragAction) {

      // Invalidate row at the old location.
      if (mSlots->mDropAllowed) {
        mSlots->mDropAllowed = PR_FALSE;
        InvalidateDropFeedback(lastDropRow, lastDropOrient);
      }

      if (mSlots->mTimer) {
        // Timer is active but for a different row than the current one, kill it.
        mSlots->mTimer->Cancel();
        mSlots->mTimer = nsnull;
      }

      if (mSlots->mDropRow >= 0) {
        if (!mSlots->mTimer && mSlots->mDropOrient == nsITreeView::DROP_ON) {
          // Either there wasn't a timer running or it was just killed above.
          // If over a folder, start up a timer to open the folder.
          PRBool isContainer = PR_FALSE;
          mView->IsContainer(mSlots->mDropRow, &isContainer);
          if (isContainer) {
            PRBool isOpen = PR_FALSE;
            mView->IsContainerOpen(mSlots->mDropRow, &isOpen);
            if (!isOpen) {
              // This node isn't expanded, set a timer to expand it.
              CreateTimer(nsILookAndFeel::eMetric_TreeOpenDelay,
                          OpenCallback, nsITimer::TYPE_ONE_SHOT,
                          getter_AddRefs(mSlots->mTimer));
            }
          }
        }

        PRBool canDropAtNewLocation = PR_FALSE;
        mView->CanDrop(mSlots->mDropRow, mSlots->mDropOrient, &canDropAtNewLocation);
      
        if (canDropAtNewLocation) {
          // Invalidate row at the new location.
          mSlots->mDropAllowed = canDropAtNewLocation;
          InvalidateDropFeedback(mSlots->mDropRow, mSlots->mDropOrient);
        }
      }
    }

    // Alert the drag session we accept the drop. We have to do this every time
    // since the |canDrop| attribute is reset before we're called.
    if (mSlots->mDropAllowed && mSlots->mDragSession)
      mSlots->mDragSession->SetCanDrop(PR_TRUE);
  }
  else if (aEvent->message == NS_DRAGDROP_DROP) {
     // this event was meant for another frame, so ignore it
     if (!mSlots)
       return NS_OK;

    // Tell the view where the drop happened.

    // Remove the drop folder and all its parents from the array.
    PRInt32 parentIndex;
    nsresult rv = mView->GetParentIndex(mSlots->mDropRow, &parentIndex);
    while (NS_SUCCEEDED(rv) && parentIndex >= 0) {
      mSlots->mValueArray.RemoveValue(parentIndex);
      rv = mView->GetParentIndex(parentIndex, &parentIndex);
    }

    mView->Drop(mSlots->mDropRow, mSlots->mDropOrient);
  }
  else if (aEvent->message == NS_DRAGDROP_EXIT) {
    // this event was meant for another frame, so ignore it
    if (!mSlots)
      return NS_OK;

    // Clear out all our tracking vars.

    if (mSlots->mDropAllowed) {
      mSlots->mDropAllowed = PR_FALSE;
      InvalidateDropFeedback(mSlots->mDropRow, mSlots->mDropOrient);
    }
    else
      mSlots->mDropAllowed = PR_FALSE;
    mSlots->mDropRow = -1;
    mSlots->mDropOrient = -1;
    mSlots->mDragSession = nsnull;
    mSlots->mScrollLines = 0;

    if (mSlots->mTimer) {
      mSlots->mTimer->Cancel();
      mSlots->mTimer = nsnull;
    }

    if (mSlots->mValueArray.Count()) {
      // Close all spring loaded folders except the drop folder.
      CreateTimer(nsILookAndFeel::eMetric_TreeCloseDelay,
                  CloseCallback, nsITimer::TYPE_ONE_SHOT,
                  getter_AddRefs(mSlots->mTimer));
    }
  }

  return NS_OK;
}


nsLineStyle nsTreeBodyFrame::ConvertBorderStyleToLineStyle(PRUint8 aBorderStyle)
{
  switch (aBorderStyle) {
    case NS_STYLE_BORDER_STYLE_DOTTED:
      return nsLineStyle_kDotted;
    case NS_STYLE_BORDER_STYLE_DASHED:
      return nsLineStyle_kDashed;
    default:
      return nsLineStyle_kSolid;
  }
}

static void
PaintTreeBody(nsIFrame* aFrame, nsIRenderingContext* aCtx,
              const nsRect& aDirtyRect, nsPoint aPt)
{
  NS_STATIC_CAST(nsTreeBodyFrame*, aFrame)->PaintTreeBody(*aCtx, aDirtyRect, aPt);
}

// Painting routines
NS_IMETHODIMP
nsTreeBodyFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                  const nsRect&           aDirtyRect,
                                  const nsDisplayListSet& aLists)
{
  // REVIEW: why did we paint if we were collapsed? that makes no sense!
  if (!IsVisibleForPainting(aBuilder))
    return NS_OK; // We're invisible.  Don't paint.

  // Handles painting our background, border, and outline.
  nsresult rv = nsLeafBoxFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mView)
    return NS_OK;

  return aLists.Content()->AppendNewToTop(new (aBuilder)
      nsDisplayGeneric(this, ::PaintTreeBody, "XULTreeBody"));
}

void
nsTreeBodyFrame::PaintTreeBody(nsIRenderingContext& aRenderingContext,
                               const nsRect& aDirtyRect, nsPoint aPt)
{
  // Update our available height and our page count.
  CalcInnerBox();
  aRenderingContext.PushState();
  aRenderingContext.SetClipRect(mInnerBox + aPt, nsClipCombine_kIntersect);
  PRInt32 oldPageCount = mPageLength;
  if (!mHasFixedRowCount)
    mPageLength = mInnerBox.height/mRowHeight;

  if (oldPageCount != mPageLength || mHorzWidth != CalcHorzWidth(GetScrollParts())) {
    // Schedule a ResizeReflow that will update our info properly.
    AddStateBits(NS_FRAME_IS_DIRTY);
    GetPresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eResize);
  }
  #ifdef DEBUG
  PRInt32 rowCount = mRowCount;
  mView->GetRowCount(&mRowCount);
  NS_ASSERTION(mRowCount == rowCount, "row count changed unexpectedly");
  #endif

  // Loop through our columns and paint them (e.g., for sorting).  This is only
  // relevant when painting backgrounds, since columns contain no content.  Content
  // is contained in the rows.
  for (nsTreeColumn* currCol = mColumns->GetFirstColumn(); currCol;
       currCol = currCol->GetNext()) {
    nsRect colRect;
    nsresult rv = currCol->GetRect(this, mInnerBox.y, mInnerBox.height,
                                   &colRect);
    // Don't paint hidden columns.
    if (NS_FAILED(rv) || colRect.width == 0) continue;

    if (OffsetForHorzScroll(colRect, PR_FALSE)) {
      nsRect dirtyRect;
      colRect += aPt;
      if (dirtyRect.IntersectRect(aDirtyRect, colRect)) {
        PaintColumn(currCol, colRect, GetPresContext(), aRenderingContext, aDirtyRect);
      }
    }
  }
  // Loop through our on-screen rows.
  for (PRInt32 i = mTopRowIndex; i < mRowCount && i <= mTopRowIndex+mPageLength; i++) {
    nsRect rowRect(mInnerBox.x, mInnerBox.y+mRowHeight*(i-mTopRowIndex), mInnerBox.width, mRowHeight);
    nsRect dirtyRect;
    if (dirtyRect.IntersectRect(aDirtyRect, rowRect + aPt) &&
        rowRect.y < (mInnerBox.y+mInnerBox.height)) {
      PaintRow(i, rowRect + aPt, GetPresContext(), aRenderingContext, aDirtyRect, aPt);
    }
  }

  if (mSlots && mSlots->mDropAllowed && (mSlots->mDropOrient == nsITreeView::DROP_BEFORE ||
      mSlots->mDropOrient == nsITreeView::DROP_AFTER)) {
    nscoord yPos = mInnerBox.y + mRowHeight * (mSlots->mDropRow - mTopRowIndex) - mRowHeight / 2;
    nsRect feedbackRect(mInnerBox.x, yPos, mInnerBox.width, mRowHeight);
    if (mSlots->mDropOrient == nsITreeView::DROP_AFTER)
      feedbackRect.y += mRowHeight;

    nsRect dirtyRect;
    feedbackRect += aPt;
    if (dirtyRect.IntersectRect(aDirtyRect, feedbackRect)) {
      PaintDropFeedback(feedbackRect, GetPresContext(), aRenderingContext, aDirtyRect);
    }
  }
  aRenderingContext.PopState();
}



void
nsTreeBodyFrame::PaintColumn(nsTreeColumn*        aColumn,
                             const nsRect&        aColumnRect,
                             nsPresContext*      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect)
{
  NS_PRECONDITION(aColumn && aColumn->GetFrame(this), "invalid column passed");

  // Now obtain the properties for our cell.
  PrefillPropertyArray(-1, aColumn);
  mView->GetColumnProperties(aColumn, mScratchArray);

  // Resolve style for the column.  It contains all the info we need to lay ourselves
  // out and to paint.
  nsStyleContext* colContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreecolumn);

  // Obtain the margins for the cell and then deflate our rect by that 
  // amount.  The cell is assumed to be contained within the deflated rect.
  nsRect colRect(aColumnRect);
  nsMargin colMargin;
  colContext->GetStyleMargin()->GetMargin(colMargin);
  colRect.Deflate(colMargin);

  PaintBackgroundLayer(colContext, aPresContext, aRenderingContext, colRect, aDirtyRect);
}

void
nsTreeBodyFrame::PaintRow(PRInt32              aRowIndex,
                          const nsRect&        aRowRect,
                          nsPresContext*       aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect&        aDirtyRect,
                          nsPoint              aPt)
{
  // We have been given a rect for our row.  We treat this row like a full-blown
  // frame, meaning that it can have borders, margins, padding, and a background.
  
  // Without a view, we have no data. Check for this up front.
  if (!mView)
    return;

  nsresult rv;

  // Now obtain the properties for our row.
  // XXX Automatically fill in the following props: open, closed, container, leaf, selected, focused
  PrefillPropertyArray(aRowIndex, nsnull);
  mView->GetRowProperties(aRowIndex, mScratchArray);

  // Resolve style for the row.  It contains all the info we need to lay ourselves
  // out and to paint.
  nsStyleContext* rowContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreerow);

  // Obtain the margins for the row and then deflate our rect by that 
  // amount.  The row is assumed to be contained within the deflated rect.
  nsRect rowRect(aRowRect);
  nsMargin rowMargin;
  rowContext->GetStyleMargin()->GetMargin(rowMargin);
  rowRect.Deflate(rowMargin);

  // Paint our borders and background for our row rect.
  // If a -moz-appearance is provided, use theme drawing only if the current row
  // is not selected (since we draw the selection as part of drawing the background).
  PRBool useTheme = PR_FALSE;
  nsITheme *theme = nsnull;
  const nsStyleDisplay* displayData = rowContext->GetStyleDisplay();
  if (displayData->mAppearance) {
    theme = aPresContext->GetTheme();
    if (theme && theme->ThemeSupportsWidget(aPresContext, nsnull, displayData->mAppearance))
      useTheme = PR_TRUE;
  }
  PRBool isSelected = PR_FALSE;
  nsCOMPtr<nsITreeSelection> selection;
  mView->GetSelection(getter_AddRefs(selection));
  if (selection) 
    selection->IsSelected(aRowIndex, &isSelected);
  if (useTheme && !isSelected) {
    nsRect dirty;
    dirty.IntersectRect(rowRect, aDirtyRect);
    theme->DrawWidgetBackground(&aRenderingContext, this, 
                                displayData->mAppearance, rowRect, dirty);
  } else {
    PaintBackgroundLayer(rowContext, aPresContext, aRenderingContext, rowRect, aDirtyRect);
  }
  
  // Adjust the rect for its border and padding.
  AdjustForBorderPadding(rowContext, rowRect);

  PRBool isSeparator = PR_FALSE;
  mView->IsSeparator(aRowIndex, &isSeparator);
  if (isSeparator) {
    // The row is a separator.

    nscoord primaryX = rowRect.x;
    nsTreeColumn* primaryCol = mColumns->GetPrimaryColumn();
    if (primaryCol) {
      // Paint the primary cell.
      nsRect cellRect;
      rv = primaryCol->GetRect(this, rowRect.y, rowRect.height, &cellRect);
      if (NS_FAILED(rv)) {
        NS_NOTREACHED("primary column is invalid");
        return;
      }

      if (OffsetForHorzScroll(cellRect, PR_FALSE)) {
        cellRect.x += aPt.x;
        nsRect dirtyRect;
        if (dirtyRect.IntersectRect(aDirtyRect, cellRect))
          PaintCell(aRowIndex, primaryCol, cellRect, aPresContext,
                    aRenderingContext, aDirtyRect, primaryX, aPt);
      }

      // Paint the left side of the separator.
      nscoord currX;
      nsTreeColumn* previousCol = primaryCol->GetPrevious();
      if (previousCol) {
        nsRect prevColRect;
        rv = previousCol->GetRect(this, 0, 0, &prevColRect);
        if (NS_SUCCEEDED(rv)) {
          currX = (prevColRect.x - mHorzPosition) + prevColRect.width + aPt.x;
        } else {
          NS_NOTREACHED("The column before the primary column is invalid");
          currX = rowRect.x;
        }
      } else {
        currX = rowRect.x;
      }

      PRInt32 level;
      mView->GetLevel(aRowIndex, &level);
      if (level == 0)
        currX += mIndentation;

      if (currX > rowRect.x) {
        nsRect separatorRect(rowRect);
        separatorRect.width -= rowRect.x + rowRect.width - currX;
        PaintSeparator(aRowIndex, separatorRect, aPresContext, aRenderingContext, aDirtyRect);
      }
    }

    // Paint the right side (whole) separator.
    nsRect separatorRect(rowRect);
    if (primaryX > rowRect.x) {
      separatorRect.width -= primaryX - rowRect.x;
      separatorRect.x += primaryX - rowRect.x;
    }
    PaintSeparator(aRowIndex, separatorRect, aPresContext, aRenderingContext, aDirtyRect);
  }
  else {
    // Now loop over our cells. Only paint a cell if it intersects with our dirty rect.
    for (nsTreeColumn* currCol = mColumns->GetFirstColumn(); currCol;
         currCol = currCol->GetNext()) {
      nsRect cellRect;
      rv = currCol->GetRect(this, rowRect.y, rowRect.height, &cellRect);
      // Don't paint cells in hidden columns.
      if (NS_FAILED(rv) || cellRect.width == 0)
        continue;

      if (OffsetForHorzScroll(cellRect, PR_FALSE)) {
        cellRect.x += aPt.x;
        
        nsRect dirtyRect;
        nscoord dummy;
        if (dirtyRect.IntersectRect(aDirtyRect, cellRect))
          PaintCell(aRowIndex, currCol, cellRect, aPresContext,
                    aRenderingContext, aDirtyRect, dummy, aPt);
      }
    }
  }
}

void
nsTreeBodyFrame::PaintSeparator(PRInt32              aRowIndex,
                                const nsRect&        aSeparatorRect,
                                nsPresContext*      aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect&        aDirtyRect)
{
  // Resolve style for the separator.
  nsStyleContext* separatorContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreeseparator);
  PRBool useTheme = PR_FALSE;
  nsITheme *theme = nsnull;
  const nsStyleDisplay* displayData = separatorContext->GetStyleDisplay();
  if ( displayData->mAppearance ) {
    theme = aPresContext->GetTheme();
    if (theme && theme->ThemeSupportsWidget(aPresContext, nsnull, displayData->mAppearance))
      useTheme = PR_TRUE;
  }

  // use -moz-appearance if provided.
  if (useTheme) {
    nsRect dirty;
    dirty.IntersectRect(aSeparatorRect, aDirtyRect);
    theme->DrawWidgetBackground(&aRenderingContext, this,
                                displayData->mAppearance, aSeparatorRect, dirty); 
  }
  else {
    const nsStylePosition* stylePosition = separatorContext->GetStylePosition();

    // Obtain the height for the separator or use the default value.
    nscoord height;
    if (stylePosition->mHeight.GetUnit() == eStyleUnit_Coord)
      height = stylePosition->mHeight.GetCoordValue();
    else {
      // Use default height 2px.
      height = nsPresContext::CSSPixelsToAppUnits(2);
    }

    // Obtain the margins for the separator and then deflate our rect by that 
    // amount. The separator is assumed to be contained within the deflated rect.
    nsRect separatorRect(aSeparatorRect.x, aSeparatorRect.y, aSeparatorRect.width, height);
    nsMargin separatorMargin;
    separatorContext->GetStyleMargin()->GetMargin(separatorMargin);
    separatorRect.Deflate(separatorMargin);

    // Center the separator.
    separatorRect.y += (aSeparatorRect.height - height) / 2;

    PaintBackgroundLayer(separatorContext, aPresContext, aRenderingContext, separatorRect, aDirtyRect);
  }
}

void
nsTreeBodyFrame::PaintCell(PRInt32              aRowIndex,
                           nsTreeColumn*        aColumn,
                           const nsRect&        aCellRect,
                           nsPresContext*       aPresContext,
                           nsIRenderingContext& aRenderingContext,
                           const nsRect&        aDirtyRect,
                           nscoord&             aCurrX,
                           nsPoint              aPt)
{
  NS_PRECONDITION(aColumn && aColumn->GetFrame(this), "invalid column passed");

  // Now obtain the properties for our cell.
  // XXX Automatically fill in the following props: open, closed, container, leaf, selected, focused, and the col ID.
  PrefillPropertyArray(aRowIndex, aColumn);
  mView->GetCellProperties(aRowIndex, aColumn, mScratchArray);

  // Resolve style for the cell.  It contains all the info we need to lay ourselves
  // out and to paint.
  nsStyleContext* cellContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreecell);

  // Obtain the margins for the cell and then deflate our rect by that 
  // amount.  The cell is assumed to be contained within the deflated rect.
  nsRect cellRect(aCellRect);
  nsMargin cellMargin;
  cellContext->GetStyleMargin()->GetMargin(cellMargin);
  cellRect.Deflate(cellMargin);

  // Paint our borders and background for our row rect.
  PaintBackgroundLayer(cellContext, aPresContext, aRenderingContext, cellRect, aDirtyRect);

  // Adjust the rect for its border and padding.
  AdjustForBorderPadding(cellContext, cellRect);

  nscoord currX = cellRect.x;
  nscoord remainingWidth = cellRect.width;

  // Now we paint the contents of the cells.
  // Text alignment determines the order in which we paint.  
  // LEFT means paint from left to right.
  // RIGHT means paint from right to left.
  // XXX Implement RIGHT alignment!

  if (aColumn->IsPrimary()) {
    // If we're the primary column, we need to indent and paint the twisty and any connecting lines
    // between siblings.

    PRInt32 level;
    mView->GetLevel(aRowIndex, &level);

    currX += mIndentation * level;
    remainingWidth -= mIndentation * level;

    // Resolve the style to use for the connecting lines.
    nsStyleContext* lineContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreeline);
    
    if (mIndentation && level &&
        lineContext->GetStyleVisibility()->IsVisibleOrCollapsed()) {
      // Paint the thread lines.

      // Get the size of the twisty. We don't want to paint the twisty
      // before painting of connecting lines since it would paint lines over
      // the twisty. But we need to leave a place for it.
      nsStyleContext* twistyContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreetwisty);

      nsRect imageSize;
      nsRect twistyRect(aCellRect);
      GetTwistyRect(aRowIndex, aColumn, imageSize, twistyRect, aPresContext,
                    aRenderingContext, twistyContext);

      nsMargin twistyMargin;
      twistyContext->GetStyleMargin()->GetMargin(twistyMargin);
      twistyRect.Inflate(twistyMargin);

      aRenderingContext.PushState();

      const nsStyleBorder* borderStyle = lineContext->GetStyleBorder();
      nscolor color;
      PRBool transparent; PRBool foreground;
      borderStyle->GetBorderColor(NS_SIDE_LEFT, color, transparent, foreground);

      aRenderingContext.SetColor(color);
      PRUint8 style;
      style = borderStyle->GetBorderStyle(NS_SIDE_LEFT);
      aRenderingContext.SetLineStyle(ConvertBorderStyleToLineStyle(style));

      nscoord srcX = currX + twistyRect.width - mIndentation / 2;
      nscoord lineY = (aRowIndex - mTopRowIndex) * mRowHeight + aPt.y;

      // Don't paint off our cell.
      if (srcX <= cellRect.x + cellRect.width) {
        nscoord destX = currX + twistyRect.width;
        if (destX > cellRect.x + cellRect.width)
          destX = cellRect.x + cellRect.width;
        aRenderingContext.DrawLine(srcX, lineY + mRowHeight / 2, destX, lineY + mRowHeight / 2);
      }

      PRInt32 currentParent = aRowIndex;
      for (PRInt32 i = level; i > 0; i--) {
        if (srcX <= cellRect.x + cellRect.width) {
          // Paint full vertical line only if we have next sibling.
          PRBool hasNextSibling;
          mView->HasNextSibling(currentParent, aRowIndex, &hasNextSibling);
          if (hasNextSibling)
            aRenderingContext.DrawLine(srcX, lineY, srcX, lineY + mRowHeight);
          else if (i == level)
            aRenderingContext.DrawLine(srcX, lineY, srcX, lineY + mRowHeight / 2);
        }

        PRInt32 parent;
        if (NS_FAILED(mView->GetParentIndex(currentParent, &parent)) || parent < 0)
          break;
        currentParent = parent;
        srcX -= mIndentation;
      }

      aRenderingContext.PopState();
    }

    // Always leave space for the twisty.
    nsRect twistyRect(currX, cellRect.y, remainingWidth, cellRect.height);
    PaintTwisty(aRowIndex, aColumn, twistyRect, aPresContext, aRenderingContext, aDirtyRect,
                remainingWidth, currX);
  }
  
  // Now paint the icon for our cell.
  nsRect iconRect(currX, cellRect.y, remainingWidth, cellRect.height);
  nsRect dirtyRect;
  if (dirtyRect.IntersectRect(aDirtyRect, iconRect))
    PaintImage(aRowIndex, aColumn, iconRect, aPresContext, aRenderingContext, aDirtyRect,
               remainingWidth, currX);

  // Now paint our element, but only if we aren't a cycler column.
  // XXX until we have the ability to load images, allow the view to 
  // insert text into cycler columns...
  if (!aColumn->IsCycler()) {
    nsRect elementRect(currX, cellRect.y, remainingWidth, cellRect.height);
    nsRect dirtyRect;
    if (dirtyRect.IntersectRect(aDirtyRect, elementRect)) {
      switch (aColumn->GetType()) {
        case nsITreeColumn::TYPE_TEXT:
          PaintText(aRowIndex, aColumn, elementRect, aPresContext, aRenderingContext, aDirtyRect, currX);
          break;
        case nsITreeColumn::TYPE_CHECKBOX:
          PaintCheckbox(aRowIndex, aColumn, elementRect, aPresContext, aRenderingContext, aDirtyRect);
          break;
        case nsITreeColumn::TYPE_PROGRESSMETER:
          PRInt32 state;
          mView->GetProgressMode(aRowIndex, aColumn, &state);
          switch (state) {
            case nsITreeView::PROGRESS_NORMAL:
            case nsITreeView::PROGRESS_UNDETERMINED:
              PaintProgressMeter(aRowIndex, aColumn, elementRect, aPresContext, aRenderingContext, aDirtyRect);
              break;
            case nsITreeView::PROGRESS_NONE:
            default:
              PaintText(aRowIndex, aColumn, elementRect, aPresContext, aRenderingContext, aDirtyRect, currX);
              break;
          }
          break;
      }
    }
  }

  aCurrX = currX;
}

void
nsTreeBodyFrame::PaintTwisty(PRInt32              aRowIndex,
                             nsTreeColumn*        aColumn,
                             const nsRect&        aTwistyRect,
                             nsPresContext*      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect,
                             nscoord&             aRemainingWidth,
                             nscoord&             aCurrX)
{
  NS_PRECONDITION(aColumn && aColumn->GetFrame(this), "invalid column passed");

  // Paint the twisty, but only if we are a non-empty container.
  PRBool shouldPaint = PR_FALSE;
  PRBool isContainer = PR_FALSE;
  mView->IsContainer(aRowIndex, &isContainer);
  if (isContainer) {
    PRBool isContainerEmpty = PR_FALSE;
    mView->IsContainerEmpty(aRowIndex, &isContainerEmpty);
    if (!isContainerEmpty)
      shouldPaint = PR_TRUE;
  }

  // Resolve style for the twisty.
  nsStyleContext* twistyContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreetwisty);

  // Obtain the margins for the twisty and then deflate our rect by that 
  // amount.  The twisty is assumed to be contained within the deflated rect.
  nsRect twistyRect(aTwistyRect);
  nsMargin twistyMargin;
  twistyContext->GetStyleMargin()->GetMargin(twistyMargin);
  twistyRect.Deflate(twistyMargin);

  nsRect imageSize;
  nsITheme* theme = GetTwistyRect(aRowIndex, aColumn, imageSize, twistyRect,
                                  aPresContext, aRenderingContext, twistyContext);

  // Subtract out the remaining width.  This is done even when we don't actually paint a twisty in 
  // this cell, so that cells in different rows still line up.
  nsRect copyRect(twistyRect);
  copyRect.Inflate(twistyMargin);
  aRemainingWidth -= copyRect.width;
  aCurrX += copyRect.width;

  if (shouldPaint) {
    // Paint our borders and background for our image rect.
    PaintBackgroundLayer(twistyContext, aPresContext, aRenderingContext, twistyRect, aDirtyRect);

    if (theme) {
      // yeah, I know it says we're drawing a background, but a twisty is really a fg
      // object since it doesn't have anything that gecko would want to draw over it. Besides,
      // we have to prevent imagelib from drawing it.
      nsRect dirty;
      dirty.IntersectRect(twistyRect, aDirtyRect);
      theme->DrawWidgetBackground(&aRenderingContext, this, 
                                  twistyContext->GetStyleDisplay()->mAppearance, twistyRect, dirty);
    }
    else {
      // Time to paint the twisty.
      // Adjust the rect for its border and padding.
      nsMargin bp(0,0,0,0);
      GetBorderPadding(twistyContext, bp);
      twistyRect.Deflate(bp);
      imageSize.Deflate(bp);

      // Get the image for drawing.
      nsCOMPtr<imgIContainer> image;
      PRBool useImageRegion = PR_TRUE;
      GetImage(aRowIndex, aColumn, PR_TRUE, twistyContext, useImageRegion, getter_AddRefs(image));
      if (image) {
        nsRect r(twistyRect.x, twistyRect.y, imageSize.width, imageSize.height);

        // Center the image. XXX Obey vertical-align style prop?
        if (imageSize.height < twistyRect.height) {
          r.y += (twistyRect.height - imageSize.height)/2;
        }
          
        // Paint the image.
        aRenderingContext.DrawImage(image, imageSize, r);
      }
    }        
  }
}

void
nsTreeBodyFrame::PaintImage(PRInt32              aRowIndex,
                            nsTreeColumn*        aColumn,
                            const nsRect&        aImageRect,
                            nsPresContext*       aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect&        aDirtyRect,
                            nscoord&             aRemainingWidth,
                            nscoord&             aCurrX)
{
  NS_PRECONDITION(aColumn && aColumn->GetFrame(this), "invalid column passed");

  // Resolve style for the image.
  nsStyleContext* imageContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreeimage);

  // Obtain the margins for the image and then deflate our rect by that
  // amount.  The image is assumed to be contained within the deflated rect.
  nsRect imageRect(aImageRect);
  nsMargin imageMargin;
  imageContext->GetStyleMargin()->GetMargin(imageMargin);
  imageRect.Deflate(imageMargin);

  // Get the image.
  PRBool useImageRegion = PR_TRUE;
  nsCOMPtr<imgIContainer> image;
  GetImage(aRowIndex, aColumn, PR_FALSE, imageContext, useImageRegion, getter_AddRefs(image));

  // Get the image destination size.
  nsSize imageDestSize = GetImageDestSize(imageContext, useImageRegion, image);
  if (!imageDestSize.width || !imageDestSize.height)
    return;

  // Get the borders and padding.
  nsMargin bp(0,0,0,0);
  GetBorderPadding(imageContext, bp);

  // destRect will be passed as the aDestRect argument in the DrawImage method.
  // Start with the imageDestSize width and height.
  nsRect destRect(0, 0, imageDestSize.width, imageDestSize.height);
  // Inflate destRect for borders and padding so that we can compare/adjust
  // with respect to imageRect.
  destRect.Inflate(bp);

  // The destRect width and height have not been adjusted to fit within the
  // cell width and height.
  // We must adjust the width even if image is null, because the width is used
  // to update the aRemainingWidth and aCurrX values.
  // Since the height isn't used unless the image is not null, we will adjust
  // the height inside the if (image) block below.

  if (destRect.width > imageRect.width) {
    // The destRect is too wide to fit within the cell width.
    // Adjust destRect width to fit within the cell width.
    destRect.width = imageRect.width;
  }
  else {
    // The cell is wider than the destRect.
    // In a cycler column, the image is centered horizontally.
    if (!aColumn->IsCycler()) {
      // If this column is not a cycler, we won't center the image horizontally.
      // We adjust the imageRect width so that the image is placed at the start
      // of the cell.
      imageRect.width = destRect.width;
    }
  }

  if (image) {
    // Paint our borders and background for our image rect
    PaintBackgroundLayer(imageContext, aPresContext, aRenderingContext, imageRect, aDirtyRect);

    // The destRect x and y have not been set yet. Let's do that now.
    // Initially, we use the imageRect x and y.
    destRect.x = imageRect.x;
    destRect.y = imageRect.y;

    if (destRect.width < imageRect.width) {
      // The destRect width is smaller than the cell width.
      // Center the image horizontally in the cell.
      // Adjust the destRect x accordingly.
      destRect.x += (imageRect.width - destRect.width)/2;
    }

    // Now it's time to adjust the destRect height to fit within the cell height.
    if (destRect.height > imageRect.height) {
      // The destRect height is larger than the cell height.
      // Adjust destRect height to fit within the cell height.
      destRect.height = imageRect.height;
    }
    else if (destRect.height < imageRect.height) {
      // The destRect height is smaller than the cell height.
      // Center the image vertically in the cell.
      // Adjust the destRect y accordingly.
      destRect.y += (imageRect.height - destRect.height)/2;
    }

    // It's almost time to paint the image.
    // Deflate destRect for the border and padding.
    destRect.Deflate(bp);

    // Get the image source rectangle - the rectangle containing the part of
    // the image that we are going to display.
    // sourceRect will be passed as the aSrcRect argument in the DrawImage method.
    nsRect sourceRect = GetImageSourceRect(imageContext, useImageRegion, image);

    // If destRect width/height was adjusted to fit within the cell
    // width/height, we need to make corresponding adjustments to the
    // sourceRect width/height.
    // Here's an explanation. Let's say that the image is 100 pixels tall and
    // that the CSS has specified that the destination height should be 50
    // pixels tall. Let's say that the cell height is only 20 pixels. So, in
    // those 20 visible pixels, we want to see the top 20/50ths of the image.
    // So, the sourceRect.height should be 100 * 20 / 50, which is 40 pixels.
    // Essentially, we are scaling the image as dictated by the CSS destination
    // height and width, and we are then clipping the scaled image by the cell
    // width and height.
    if (destRect.width != imageDestSize.width) {
      sourceRect.width = sourceRect.width * destRect.width / imageDestSize.width;
    }
    if (destRect.height != imageDestSize.height) {
      sourceRect.height = sourceRect.height * destRect.height / imageDestSize.height;
    }

    // Finally we can paint the image.
    aRenderingContext.DrawImage(image, sourceRect, destRect);
  }

  // Update the aRemainingWidth and aCurrX values.
  imageRect.Inflate(imageMargin);
  aRemainingWidth -= imageRect.width;
  aCurrX += imageRect.width;
}

void
nsTreeBodyFrame::PaintText(PRInt32              aRowIndex,
                           nsTreeColumn*        aColumn,
                           const nsRect&        aTextRect,
                           nsPresContext*      aPresContext,
                           nsIRenderingContext& aRenderingContext,
                           const nsRect&        aDirtyRect,
                           nscoord&             aCurrX)
{
  NS_PRECONDITION(aColumn && aColumn->GetFrame(this), "invalid column passed");

  // Now obtain the text for our cell.
  nsAutoString text;
  mView->GetCellText(aRowIndex, aColumn, text);
  // We're going to paint this text so we need to ensure bidi is enabled if
  // necessary
  CheckTextForBidi(text);

  if (text.Length() == 0)
    return; // Don't paint an empty string. XXX What about background/borders? Still paint?

  // Resolve style for the text.  It contains all the info we need to lay ourselves
  // out and to paint.
  nsStyleContext* textContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreecelltext);

  // Obtain the margins for the text and then deflate our rect by that 
  // amount.  The text is assumed to be contained within the deflated rect.
  nsRect textRect(aTextRect);
  nsMargin textMargin;
  textContext->GetStyleMargin()->GetMargin(textMargin);
  textRect.Deflate(textMargin);

  // Adjust the rect for its border and padding.
  nsMargin bp(0,0,0,0);
  GetBorderPadding(textContext, bp);
  textRect.Deflate(bp);

  // Compute our text size.
  nsCOMPtr<nsIFontMetrics> fontMet;
  aPresContext->DeviceContext()->
    GetMetricsFor(textContext->GetStyleFont()->mFont, *getter_AddRefs(fontMet));

  nscoord height, baseline;
  fontMet->GetHeight(height);
  fontMet->GetMaxAscent(baseline);

  // Center the text. XXX Obey vertical-align style prop?
  if (height < textRect.height) {
    textRect.y += (textRect.height - height)/2;
    textRect.height = height;
  }

  // Set our font.
  aRenderingContext.SetFont(fontMet);

  AdjustForCellText(text, aRowIndex, aColumn, aRenderingContext, textRect);

  // Subtract out the remaining width.
  nsRect copyRect(textRect);
  copyRect.Inflate(textMargin);
  aCurrX += copyRect.width;

  textRect.Inflate(bp);
  PaintBackgroundLayer(textContext, aPresContext, aRenderingContext, textRect, aDirtyRect);

  // Time to paint our text.
  textRect.Deflate(bp);

  // Set our color.
  aRenderingContext.SetColor(textContext->GetStyleColor()->mColor);

  // Draw decorations.
  PRUint8 decorations = textContext->GetStyleTextReset()->mTextDecoration;

  nscoord offset;
  nscoord size;
  if (decorations & (NS_FONT_DECORATION_OVERLINE | NS_FONT_DECORATION_UNDERLINE)) {
    fontMet->GetUnderline(offset, size);
    if (decorations & NS_FONT_DECORATION_OVERLINE)
      aRenderingContext.FillRect(textRect.x, textRect.y, textRect.width, size);
    if (decorations & NS_FONT_DECORATION_UNDERLINE)
      aRenderingContext.FillRect(textRect.x, textRect.y + baseline - offset, textRect.width, size);
  }
  if (decorations & NS_FONT_DECORATION_LINE_THROUGH) {
    fontMet->GetStrikeout(offset, size);
    aRenderingContext.FillRect(textRect.x, textRect.y + baseline - offset, textRect.width, size);
  }
#ifdef MOZ_TIMELINE
  NS_TIMELINE_START_TIMER("Render Outline Text");
#endif
  nsLayoutUtils::DrawString(this, &aRenderingContext, text.get(), text.Length(),
                            textRect.TopLeft() + nsPoint(0, baseline));
#ifdef MOZ_TIMELINE
  NS_TIMELINE_STOP_TIMER("Render Outline Text");
  NS_TIMELINE_MARK_TIMER("Render Outline Text");
#endif
}

void
nsTreeBodyFrame::PaintCheckbox(PRInt32              aRowIndex,
                               nsTreeColumn*        aColumn,
                               const nsRect&        aCheckboxRect,
                               nsPresContext*      aPresContext,
                               nsIRenderingContext& aRenderingContext,
                               const nsRect&        aDirtyRect)
{
  NS_PRECONDITION(aColumn && aColumn->GetFrame(this), "invalid column passed");

  // Resolve style for the checkbox.
  nsStyleContext* checkboxContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreecheckbox);

  // Obtain the margins for the checkbox and then deflate our rect by that 
  // amount.  The checkbox is assumed to be contained within the deflated rect.
  nsRect checkboxRect(aCheckboxRect);
  nsMargin checkboxMargin;
  checkboxContext->GetStyleMargin()->GetMargin(checkboxMargin);
  checkboxRect.Deflate(checkboxMargin);
  
  nsRect imageSize = GetImageSize(aRowIndex, aColumn, PR_TRUE, checkboxContext);

  if (imageSize.height > checkboxRect.height)
    imageSize.height = checkboxRect.height;
  if (imageSize.width > checkboxRect.width)
    imageSize.width = checkboxRect.width;

  // Paint our borders and background for our image rect.
  PaintBackgroundLayer(checkboxContext, aPresContext, aRenderingContext, checkboxRect, aDirtyRect);

  // Time to paint the checkbox.
  // Adjust the rect for its border and padding.
  nsMargin bp(0,0,0,0);
  GetBorderPadding(checkboxContext, bp);
  checkboxRect.Deflate(bp);

  // Get the image for drawing.
  nsCOMPtr<imgIContainer> image;
  PRBool useImageRegion = PR_TRUE;
  GetImage(aRowIndex, aColumn, PR_TRUE, checkboxContext, useImageRegion, getter_AddRefs(image));
  if (image) {
    nsRect r(checkboxRect.x, checkboxRect.y, imageSize.width, imageSize.height);
          
    if (imageSize.height < checkboxRect.height) {
      r.y += (checkboxRect.height - imageSize.height)/2;
    }

    if (imageSize.width < checkboxRect.width) {
      r.x += (checkboxRect.width - imageSize.width)/2;
    }

    // Paint the image.
    aRenderingContext.DrawImage(image, imageSize, r);
  }
}

void
nsTreeBodyFrame::PaintProgressMeter(PRInt32              aRowIndex,
                                    nsTreeColumn*        aColumn,
                                    const nsRect&        aProgressMeterRect,
                                    nsPresContext*      aPresContext,
                                    nsIRenderingContext& aRenderingContext,
                                    const nsRect&        aDirtyRect)
{
  NS_PRECONDITION(aColumn && aColumn->GetFrame(this), "invalid column passed");

  // Resolve style for the progress meter.  It contains all the info we need
  // to lay ourselves out and to paint.
  nsStyleContext* meterContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreeprogressmeter);

  // Obtain the margins for the progress meter and then deflate our rect by that 
  // amount. The progress meter is assumed to be contained within the deflated
  // rect.
  nsRect meterRect(aProgressMeterRect);
  nsMargin meterMargin;
  meterContext->GetStyleMargin()->GetMargin(meterMargin);
  meterRect.Deflate(meterMargin);

  // Paint our borders and background for our progress meter rect.
  PaintBackgroundLayer(meterContext, aPresContext, aRenderingContext, meterRect, aDirtyRect);

  // Time to paint our progress. 
  PRInt32 state;
  mView->GetProgressMode(aRowIndex, aColumn, &state);
  if (state == nsITreeView::PROGRESS_NORMAL) {
    // Adjust the rect for its border and padding.
    AdjustForBorderPadding(meterContext, meterRect);

    // Set our color.
    aRenderingContext.SetColor(meterContext->GetStyleColor()->mColor);

    // Now obtain the value for our cell.
    nsAutoString value;
    mView->GetCellValue(aRowIndex, aColumn, value);

    PRInt32 rv;
    PRInt32 intValue = value.ToInteger(&rv);
    if (intValue < 0)
      intValue = 0;
    else if (intValue > 100)
      intValue = 100;

    meterRect.width = NSToCoordRound((float)intValue / 100 * meterRect.width);
    PRBool useImageRegion = PR_TRUE;
    nsCOMPtr<imgIContainer> image;
    GetImage(aRowIndex, aColumn, PR_TRUE, meterContext, useImageRegion, getter_AddRefs(image));
    if (image)
      aRenderingContext.DrawTile(image, 0, 0, &meterRect);
    else
      aRenderingContext.FillRect(meterRect);
  }
  else if (state == nsITreeView::PROGRESS_UNDETERMINED) {
    // Adjust the rect for its border and padding.
    AdjustForBorderPadding(meterContext, meterRect);

    PRBool useImageRegion = PR_TRUE;
    nsCOMPtr<imgIContainer> image;
    GetImage(aRowIndex, aColumn, PR_TRUE, meterContext, useImageRegion, getter_AddRefs(image));
    if (image)
      aRenderingContext.DrawTile(image, 0, 0, &meterRect);
  }
}


void
nsTreeBodyFrame::PaintDropFeedback(const nsRect&        aDropFeedbackRect,
                                   nsPresContext*      aPresContext,
                                   nsIRenderingContext& aRenderingContext,
                                   const nsRect&        aDirtyRect)
{
  // Paint the drop feedback in between rows.

  nscoord currX;

  // Adjust for the primary cell.
  nsTreeColumn* primaryCol = mColumns->GetPrimaryColumn();

  if (primaryCol) {
    nsresult rv = primaryCol->GetXInTwips(this, &currX);
    NS_ASSERTION(NS_SUCCEEDED(rv), "primary column is invalid?");

    currX -= mHorzPosition;
  } else {
    currX = aDropFeedbackRect.x;
  }

  PrefillPropertyArray(mSlots->mDropRow, primaryCol);

  // Resolve the style to use for the drop feedback.
  nsStyleContext* feedbackContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreedropfeedback);

  // Paint only if it is visible.
  if (feedbackContext->GetStyleVisibility()->IsVisibleOrCollapsed()) {
    PRInt32 level;
    mView->GetLevel(mSlots->mDropRow, &level);

    // If our previous or next row has greater level use that for 
    // correct visual indentation.
    if (mSlots->mDropOrient == nsITreeView::DROP_BEFORE) {
      if (mSlots->mDropRow > 0) {
        PRInt32 previousLevel;
        mView->GetLevel(mSlots->mDropRow - 1, &previousLevel);
        if (previousLevel > level)
          level = previousLevel;
      }
    }
    else {
      if (mSlots->mDropRow < mRowCount - 1) {
        PRInt32 nextLevel;
        mView->GetLevel(mSlots->mDropRow + 1, &nextLevel);
        if (nextLevel > level)
          level = nextLevel;
      }
    }

    currX += mIndentation * level;

    if (primaryCol){
      nsStyleContext* twistyContext = GetPseudoStyleContext(nsCSSAnonBoxes::moztreetwisty);
      nsRect imageSize;
      nsRect twistyRect;
      GetTwistyRect(mSlots->mDropRow, primaryCol, imageSize, twistyRect, aPresContext,
                    aRenderingContext, twistyContext);
      nsMargin twistyMargin;
      twistyContext->GetStyleMargin()->GetMargin(twistyMargin);
      twistyRect.Inflate(twistyMargin);
      currX += twistyRect.width;
    }

    const nsStylePosition* stylePosition = feedbackContext->GetStylePosition();

    // Obtain the width for the drop feedback or use default value.
    nscoord width;
    if (stylePosition->mWidth.GetUnit() == eStyleUnit_Coord)
      width = stylePosition->mWidth.GetCoordValue();
    else {
      // Use default width 50px.
      width = nsPresContext::CSSPixelsToAppUnits(50);
    }

    // Obtain the height for the drop feedback or use default value.
    nscoord height;
    if (stylePosition->mHeight.GetUnit() == eStyleUnit_Coord)
      height = stylePosition->mHeight.GetCoordValue();
    else {
      // Use default height 2px.
      height = nsPresContext::CSSPixelsToAppUnits(2);
    }

    // Obtain the margins for the drop feedback and then deflate our rect
    // by that amount.
    nsRect feedbackRect(currX, aDropFeedbackRect.y, width, height);
    nsMargin margin;
    feedbackContext->GetStyleMargin()->GetMargin(margin);
    feedbackRect.Deflate(margin);

    feedbackRect.y += (aDropFeedbackRect.height - height) / 2;

    // Finally paint the drop feedback.
    PaintBackgroundLayer(feedbackContext, aPresContext, aRenderingContext, feedbackRect, aDirtyRect);
  }
}

void
nsTreeBodyFrame::PaintBackgroundLayer(nsStyleContext*      aStyleContext,
                                      nsPresContext*      aPresContext,
                                      nsIRenderingContext& aRenderingContext,
                                      const nsRect&        aRect,
                                      const nsRect&        aDirtyRect)
{
  const nsStyleBackground* myColor = aStyleContext->GetStyleBackground();
  const nsStyleBorder* myBorder = aStyleContext->GetStyleBorder();
  const nsStylePadding* myPadding = aStyleContext->GetStylePadding();
  const nsStyleOutline* myOutline = aStyleContext->GetStyleOutline();
  
  nsCSSRendering::PaintBackgroundWithSC(aPresContext, aRenderingContext,
                                        this, aDirtyRect, aRect,
                                        *myColor, *myBorder, *myPadding,
                                        PR_TRUE);

  nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                              aDirtyRect, aRect, *myBorder, mStyleContext, 0);

  nsCSSRendering::PaintOutline(aPresContext, aRenderingContext, this,
                               aDirtyRect, aRect, *myBorder, *myOutline,
                               aStyleContext, 0);
}

// Scrolling
NS_IMETHODIMP nsTreeBodyFrame::EnsureRowIsVisible(PRInt32 aRow)
{
  return EnsureRowIsVisibleInternal(GetScrollParts(), aRow);
}

nsresult nsTreeBodyFrame::EnsureRowIsVisibleInternal(const ScrollParts& aParts, PRInt32 aRow)
{
  if (!mView)
    return NS_OK;

  if (mTopRowIndex <= aRow && mTopRowIndex+mPageLength > aRow)
    return NS_OK;

  if (aRow < mTopRowIndex)
    ScrollToRowInternal(aParts, aRow);
  else {
    // Bring it just on-screen.
    PRInt32 distance = aRow - (mTopRowIndex+mPageLength)+1;
    ScrollToRowInternal(aParts, mTopRowIndex+distance);
  }

  return NS_OK;
}

NS_IMETHODIMP nsTreeBodyFrame::EnsureCellIsVisible(PRInt32 aRow, nsITreeColumn* aCol)
{
  nsRefPtr<nsTreeColumn> col = GetColumnImpl(aCol);
  if (!col)
    return NS_ERROR_INVALID_ARG;

  ScrollParts parts = GetScrollParts();

  nscoord result = -1;
  nsresult rv;

  nscoord columnPos;
  rv = col->GetXInTwips(this, &columnPos);
  if(NS_FAILED(rv)) return rv;

  nscoord columnWidth;
  rv = col->GetWidthInTwips(this, &columnWidth);
  if(NS_FAILED(rv)) return rv;

  // If the start of the column is before the
  // start of the horizontal view, then scroll
  if (columnPos < mHorzPosition)
    result = columnPos;
  // If the end of the column is past the end of 
  // the horizontal view, then scroll
  else if ((columnPos + columnWidth) > (mHorzPosition + mInnerBox.width))
    result = ((columnPos + columnWidth) - (mHorzPosition + mInnerBox.width)) + mHorzPosition;

  if (result != -1) {
    rv = ScrollHorzInternal(parts, result);
    if(NS_FAILED(rv)) return rv;
  }

  return EnsureRowIsVisibleInternal(parts, aRow);
}

NS_IMETHODIMP nsTreeBodyFrame::ScrollToCell(PRInt32 aRow, nsITreeColumn* aCol)
{
  ScrollParts parts = GetScrollParts();
  nsresult rv = ScrollToRowInternal(parts, aRow);
  if(NS_FAILED(rv)) return rv;

  return ScrollToColumnInternal(parts, aCol);
}

NS_IMETHODIMP nsTreeBodyFrame::ScrollToColumn(nsITreeColumn* aCol)
{
  return ScrollToColumnInternal(GetScrollParts(), aCol);
}

nsresult nsTreeBodyFrame::ScrollToColumnInternal(const ScrollParts& aParts,
                                                 nsITreeColumn* aCol)
{
  nsRefPtr<nsTreeColumn> col = GetColumnImpl(aCol);
  if (!col)
    return NS_ERROR_INVALID_ARG;

  nscoord x;
  nsresult rv = col->GetXInTwips(this, &x);
  if (NS_FAILED(rv))
    return rv;

  return ScrollHorzInternal(aParts, x);
}

NS_IMETHODIMP nsTreeBodyFrame::ScrollToHorizontalPosition(PRInt32 aHorizontalPosition)
{
  ScrollHorzInternal(GetScrollParts(),
                     nsPresContext::CSSPixelsToAppUnits(aHorizontalPosition));
  return NS_OK;
}

NS_IMETHODIMP nsTreeBodyFrame::ScrollToRow(PRInt32 aRow)
{
  return ScrollToRowInternal(GetScrollParts(), aRow);
}

nsresult nsTreeBodyFrame::ScrollToRowInternal(const ScrollParts& aParts, PRInt32 aRow)
{
  ScrollInternal(aParts, aRow);
  UpdateScrollbars(aParts);

#if defined(XP_MAC) || defined(XP_MACOSX)
  // mac can't process the event loop during a drag, so if we're dragging,
  // grab the scroll widget and make it paint synchronously. This is
  // sorta slow (having to paint the entire tree), but it works.
  if (mSlots && mSlots->mDragSession && aParts.mVScrollbar) {
    nsIFrame* frame;
    CallQueryInterface(aParts.mVScrollbar, &frame);
    nsIWidget* scrollWidget = frame->GetWindow();
    if (scrollWidget)
      scrollWidget->Invalidate(PR_TRUE);
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP nsTreeBodyFrame::ScrollByLines(PRInt32 aNumLines)
{
  if (!mView)
    return NS_OK;

  PRInt32 newIndex = mTopRowIndex + aNumLines;
  if (newIndex < 0)
    newIndex = 0;
  else {
    PRInt32 lastPageTopRow = mRowCount - mPageLength;
    if (newIndex > lastPageTopRow)
      newIndex = lastPageTopRow;
  }
  ScrollToRow(newIndex);
  
  return NS_OK;
}

NS_IMETHODIMP nsTreeBodyFrame::ScrollByPages(PRInt32 aNumPages)
{
  if (!mView)
    return NS_OK;

  PRInt32 newIndex = mTopRowIndex + aNumPages * mPageLength;
  if (newIndex < 0)
    newIndex = 0;
  else {
    PRInt32 lastPageTopRow = mRowCount - mPageLength;
    if (newIndex > lastPageTopRow)
      newIndex = lastPageTopRow;
  }
  ScrollToRow(newIndex);
    
  return NS_OK;
}

nsresult
nsTreeBodyFrame::ScrollInternal(const ScrollParts& aParts, PRInt32 aRow)
{
  if (!mView)
    return NS_OK;

  PRInt32 delta = aRow - mTopRowIndex;

  if (delta > 0) {
    if (mTopRowIndex == (mRowCount - mPageLength + 1))
      return NS_OK;
  }
  else {
    if (mTopRowIndex == 0)
      return NS_OK;
  }

  mTopRowIndex += delta;

  nsPresContext* presContext = GetPresContext();

  // See if we have a transparent background or a background image.  
  // If we do, then we cannot blit.
  const nsStyleBackground* background = GetStyleBackground();
  if (background->mBackgroundImage || background->IsTransparent() || 
      PR_ABS(delta)*mRowHeight >= mRect.height) {
    Invalidate();
  } else {
    nsIWidget* widget = nsLeafBoxFrame::GetView()->GetWidget();
    if (widget) {
      nscoord rowHeightAsPixels = presContext->AppUnitsToDevPixels(mRowHeight);
      widget->Scroll(0, -delta*rowHeightAsPixels, nsnull);
    }
  }

  nsScrollbarEvent event(PR_TRUE, NS_SCROLL_EVENT, nsnull);
  // scroll events fired at elements don't bubble (although scroll events
  // fired at documents do, to the window)
  event.flags |= NS_EVENT_FLAG_CANT_BUBBLE;

  nsEventStatus status = nsEventStatus_eIgnore;
  nsEventDispatcher::Dispatch(mContent, presContext, &event, nsnull, &status);

  return NS_OK;
}

nsresult
nsTreeBodyFrame::ScrollHorzInternal(const ScrollParts& aParts, PRInt32 aPosition)
{
  if (!mView || !aParts.mColumnsScrollableView || !aParts.mHScrollbar)
    return NS_OK;

  if (aPosition == mHorzPosition)
    return NS_OK;

  if (aPosition < 0 || aPosition > mHorzWidth)
    return NS_OK;

  nsRect bounds = aParts.mColumnsScrollableView->View()->GetBounds();
  if (aPosition > (mHorzWidth - bounds.width)) 
    aPosition = mHorzWidth - bounds.width;

  PRInt32 delta = aPosition - mHorzPosition;
  mHorzPosition = aPosition;

  // See if we have a background image.  If we do, then we cannot blit.
  const nsStyleBackground* background = GetStyleBackground();
  if (background->mBackgroundImage || background->IsTransparent() || 
      PR_ABS(delta) >= mRect.width) {
    Invalidate();
  } else {
    nsIWidget* widget = nsLeafBoxFrame::GetView()->GetWidget();
    if (widget) {
      widget->Scroll(GetPresContext()->AppUnitsToDevPixels(-delta), 0, nsnull);
    }
  }

  // Reflect the change in the scrollbar 
  nsAutoString curPos;
  curPos.AppendInt(aPosition);
  nsWeakFrame weakFrame(this);
  aParts.mHScrollbarContent->SetAttr(kNameSpaceID_None,
                                     nsGkAtoms::curpos, curPos, PR_TRUE);
  NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);
  // Update the column scroll view
  aParts.mColumnsScrollableView->ScrollTo(mHorzPosition, 0, 0);

  // And fire off an event about it all
  nsScrollbarEvent event(PR_TRUE, NS_SCROLL_EVENT, nsnull);
  // scroll events fired at elements don't bubble (although scroll events
  // fired at documents do, to the window)
  event.flags |= NS_EVENT_FLAG_CANT_BUBBLE;

  nsEventStatus status = nsEventStatus_eIgnore;
  nsEventDispatcher::Dispatch(mContent, GetPresContext(), &event, nsnull,
                              &status);

  return NS_OK;
}

NS_IMETHODIMP
nsTreeBodyFrame::ScrollbarButtonPressed(nsISupports* aScrollbar, PRInt32 aOldIndex, PRInt32 aNewIndex)
{
  // Determine which scrollbar we're talking about 
  nsIScrollbarFrame* sf = nsnull;
  CallQueryInterface(aScrollbar, &sf);
  NS_ASSERTION(sf, "scrollbar has no frame");

  ScrollParts parts = GetScrollParts();

  nsWeakFrame weakFrame(this);
  if (sf == parts.mVScrollbar) {
    if (aNewIndex > aOldIndex)
      ScrollToRowInternal(parts, mTopRowIndex+1);
    else if (aNewIndex < aOldIndex)
      ScrollToRowInternal(parts, mTopRowIndex-1);
  } else {
    ScrollHorzInternal(parts, aNewIndex);
  }

  NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);
  UpdateScrollbars(parts);

  return NS_OK;
}
  
NS_IMETHODIMP
nsTreeBodyFrame::PositionChanged(nsISupports* aScrollbar, PRInt32 aOldIndex, PRInt32& aNewIndex)
{
  ScrollParts parts = GetScrollParts();
  
  if (aOldIndex == aNewIndex)
    return NS_OK;

  // Determine which scrollbar we're talking about 
  nsIScrollbarFrame* sf = nsnull;
  CallQueryInterface(aScrollbar, &sf);
  NS_ASSERTION(sf, "scrollbar has no frame");

  // Vertical Scrollbar 
  if (parts.mVScrollbar == sf) {
    nscoord rh = nsPresContext::AppUnitsToIntCSSPixels(mRowHeight);

    nscoord newrow = aNewIndex/rh;
    nsWeakFrame weakFrame(this);
    ScrollInternal(parts, newrow);
    NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);

    // Go exactly where we're supposed to
    // Update the scrollbar.
    nsAutoString curPos;
    curPos.AppendInt(aNewIndex);
    parts.mVScrollbarContent->SetAttr(kNameSpaceID_None,
                                      nsGkAtoms::curpos, curPos, PR_TRUE);

  // Horizontal Scrollbar
  } else if (parts.mHScrollbar == sf) {
    ScrollHorzInternal(parts, aNewIndex);
  }

  return NS_OK;
}

// The style cache.
nsStyleContext*
nsTreeBodyFrame::GetPseudoStyleContext(nsIAtom* aPseudoElement)
{
  return mStyleCache.GetStyleContext(this, GetPresContext(), mContent,
                                     mStyleContext, aPseudoElement,
                                     mScratchArray);
}

// Our comparator for resolving our complex pseudos
NS_IMETHODIMP
nsTreeBodyFrame::PseudoMatches(nsIAtom* aTag, nsCSSSelector* aSelector, PRBool* aResult)
{
  if (aSelector->mTag == aTag) {
    // Iterate the pseudoclass list.  For each item in the list, see if
    // it is contained in our scratch array.  If we have a miss, then
    // we aren't a match.  If all items in the pseudoclass list are
    // present in the scratch array, then we have a match.
    nsAtomStringList* curr = aSelector->mPseudoClassList;
    while (curr) {
      PRInt32 index;
      mScratchArray->GetIndexOf(curr->mAtom, &index);
      if (index == -1) {
        *aResult = PR_FALSE;
        return NS_OK;
      }
      curr = curr->mNext;
    }
    *aResult = PR_TRUE;
  }
  else 
    *aResult = PR_FALSE;

  return NS_OK;
}

nsIContent*
nsTreeBodyFrame::GetBaseElement()
{
  nsINodeInfo *ni;
  nsIContent* parent = mContent;
  while (parent) {
    ni = parent->NodeInfo();

    if (ni->Equals(nsGkAtoms::tree, kNameSpaceID_XUL) ||
        (ni->Equals(nsGkAtoms::select) &&
         parent->IsNodeOfType(nsINode::eHTML))) {
      break;
    }

    parent = parent->GetParent();
  }

  return parent;
}

NS_IMETHODIMP
nsTreeBodyFrame::ClearStyleAndImageCaches()
{
  mStyleCache.Clear();
  mImageCache.EnumerateRead(CancelImageRequest, nsnull);
  mImageCache.Clear();
  return NS_OK;
}

PRBool 
nsTreeBodyFrame::OffsetForHorzScroll(nsRect& rect, PRBool clip)
{
  rect.x -= mHorzPosition;

  // Scrolled out before
  if (rect.XMost() <= mInnerBox.x)
    return PR_FALSE;

  // Scrolled out after
  if (rect.x > mInnerBox.XMost())
    return PR_FALSE;

  if (clip) {
    nscoord leftEdge = PR_MAX(rect.x, mInnerBox.x);
    nscoord rightEdge = PR_MIN(rect.XMost(), mInnerBox.XMost());
    rect.x = leftEdge;
    rect.width = rightEdge - leftEdge;

    // Should have returned false above
    NS_ASSERTION(rect.width >= 0, "horz scroll code out of sync");
  }

  return PR_TRUE;
}

PRBool
nsTreeBodyFrame::CanAutoScroll(PRInt32 aRowIndex)
{
  // Check first for partially visible last row.
  if (aRowIndex == mRowCount - 1) {
    nscoord y = mInnerBox.y + (aRowIndex - mTopRowIndex) * mRowHeight;
    if (y < mInnerBox.height && y + mRowHeight > mInnerBox.height)
      return PR_TRUE;
  }

  if (aRowIndex > 0 && aRowIndex < mRowCount - 1)
    return PR_TRUE;

  return PR_FALSE;
}

// Given a dom event, figure out which row in the tree the mouse is over,
// if we should drop before/after/on that row or we should auto-scroll.
// Doesn't query the content about if the drag is allowable, that's done elsewhere.
//
// For containers, we break up the vertical space of the row as follows: if in
// the topmost 25%, the drop is _before_ the row the mouse is over; if in the
// last 25%, _after_; in the middle 50%, we consider it a drop _on_ the container.
//
// For non-containers, if the mouse is in the top 50% of the row, the drop is
// _before_ and the bottom 50% _after_
void 
nsTreeBodyFrame::ComputeDropPosition(nsGUIEvent* aEvent, PRInt32* aRow, PRInt16* aOrient,
                                     PRInt16* aScrollLines)
{
  *aOrient = -1;
  *aScrollLines = 0;

  // Convert the event's point to our coordinates.  We want it in
  // the coordinates of our inner box's coordinates.
  nsPoint pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, this);
  PRInt32 xTwips = pt.x - mInnerBox.x;
  PRInt32 yTwips = pt.y - mInnerBox.y;

  *aRow = GetRowAt(xTwips, yTwips);
  if (*aRow >=0) {
    // Compute the top/bottom of the row in question.
    PRInt32 yOffset = yTwips - mRowHeight * (*aRow - mTopRowIndex);
   
    PRBool isContainer = PR_FALSE;
    mView->IsContainer (*aRow, &isContainer);
    if (isContainer) {
      // for a container, use a 25%/50%/25% breakdown
      if (yOffset < mRowHeight / 4)
        *aOrient = nsITreeView::DROP_BEFORE;
      else if (yOffset > mRowHeight - (mRowHeight / 4))
        *aOrient = nsITreeView::DROP_AFTER;
      else
        *aOrient = nsITreeView::DROP_ON;
    }
    else {
      // for a non-container use a 50%/50% breakdown
      if (yOffset < mRowHeight / 2)
        *aOrient = nsITreeView::DROP_BEFORE;
      else
        *aOrient = nsITreeView::DROP_AFTER;
    }
  }

  if (CanAutoScroll(*aRow)) {
    // Get the max value from the look and feel service.
    PRInt32 scrollLinesMax = 0;
    GetPresContext()->LookAndFeel()->
      GetMetric(nsILookAndFeel::eMetric_TreeScrollLinesMax, scrollLinesMax);
    scrollLinesMax--;
    if (scrollLinesMax < 0)
      scrollLinesMax = 0;

    // Determine if we're w/in a margin of the top/bottom of the tree during a drag.
    // This will ultimately cause us to scroll, but that's done elsewhere.
    nscoord height = (3 * mRowHeight) / 4;
    if (yTwips < height) {
      // scroll up
      *aScrollLines = NSToIntRound(-scrollLinesMax * (1 - (float)yTwips / height) - 1);
    }
    else if (yTwips > mRect.height - height) {
      // scroll down
      *aScrollLines = NSToIntRound(scrollLinesMax * (1 - (float)(mRect.height - yTwips) / height) + 1);
    }
  }
} // ComputeDropPosition

void
nsTreeBodyFrame::OpenCallback(nsITimer *aTimer, void *aClosure)
{
  nsTreeBodyFrame* self = NS_STATIC_CAST(nsTreeBodyFrame*, aClosure);
  if (self) {
    aTimer->Cancel();
    self->mSlots->mTimer = nsnull;

    if (self->mSlots->mDropRow >= 0) {
      self->mSlots->mValueArray.AppendValue(self->mSlots->mDropRow);
      self->mView->ToggleOpenState(self->mSlots->mDropRow);
    }
  }
}

void
nsTreeBodyFrame::CloseCallback(nsITimer *aTimer, void *aClosure)
{
  nsTreeBodyFrame* self = NS_STATIC_CAST(nsTreeBodyFrame*, aClosure);
  if (self) {
    aTimer->Cancel();
    self->mSlots->mTimer = nsnull;

    for (PRInt32 i = self->mSlots->mValueArray.Count() - 1; i >= 0; i--) {
      if (self->mView)
        self->mView->ToggleOpenState(self->mSlots->mValueArray[i]);
      self->mSlots->mValueArray.RemoveValueAt(i);
    }
  }
}

void
nsTreeBodyFrame::LazyScrollCallback(nsITimer *aTimer, void *aClosure)
{
  nsTreeBodyFrame* self = NS_STATIC_CAST(nsTreeBodyFrame*, aClosure);
  if (self) {
    aTimer->Cancel();
    self->mSlots->mTimer = nsnull;

    if (self->mView) {
      self->ScrollByLines(self->mSlots->mScrollLines);
      // Set a new timer to scroll the tree repeatedly.
      self->CreateTimer(nsILookAndFeel::eMetric_TreeScrollDelay,
                        ScrollCallback, nsITimer::TYPE_REPEATING_SLACK,
                        getter_AddRefs(self->mSlots->mTimer));
    }
  }
}

void
nsTreeBodyFrame::ScrollCallback(nsITimer *aTimer, void *aClosure)
{
  nsTreeBodyFrame* self = NS_STATIC_CAST(nsTreeBodyFrame*, aClosure);
  if (self) {
    // Don't scroll if we are already at the top or bottom of the view.
    if (self->mView && self->CanAutoScroll(self->mSlots->mDropRow)) {
      self->ScrollByLines(self->mSlots->mScrollLines);
    }
    else {
      aTimer->Cancel();
      self->mSlots->mTimer = nsnull;
    }
  }
}
