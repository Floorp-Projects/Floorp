/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 *  Ben Goodger <ben@netscape.com>
 *  Joe Hewitt <hewitt@netscape.com>
 */

#include "nsCOMPtr.h"
#include "nsISupportsArray.h"
#include "nsIPresContext.h"
#include "nsINameSpaceManager.h"
#include "nsIScrollbarFrame.h"

#include "nsOutlinerBodyFrame.h"
#include "nsOutlinerSelection.h"

#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsCSSAtoms.h"

#include "nsIContent.h"
#include "nsIStyleContext.h"
#include "nsIBoxObject.h"
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
#include "nsWidgetsCID.h"
#include "nsBoxFrame.h"
#include "nsBoxObject.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsBoxLayoutState.h"
#include "nsIDragService.h"

#ifdef USE_IMG2
#include "imgIRequest.h"
#include "imgIContainer.h"
#include "imgIContainerObserver.h"
#include "imgILoader.h"
#endif

#define ELLIPSIS "..."

// The style context cache impl
nsresult 
nsOutlinerStyleCache::GetStyleContext(nsICSSPseudoComparator* aComparator,
                                      nsIPresContext* aPresContext, nsIContent* aContent, 
                                      nsIStyleContext* aContext, nsIAtom* aPseudoElement,
                                      nsISupportsArray* aInputWord,
                                      nsIStyleContext** aResult)
{
  *aResult = nsnull;
  
  PRUint32 count;
  aInputWord->Count(&count);
  nsDFAState startState(0);
  nsDFAState* currState = &startState;

  // Go ahead and init the transition table.
  if (!mTransitionTable) {
    // Automatic miss. Build the table
    mTransitionTable =
      new nsObjectHashtable(nsnull, nsnull, DeleteDFAState, nsnull);
  }

  // The first transition is always made off the supplied pseudo-element.
  nsTransitionKey key(currState->GetStateID(), aPseudoElement);
  currState = NS_STATIC_CAST(nsDFAState*, mTransitionTable->Get(&key));

  if (!currState) {
    // We had a miss. Make a new state and add it to our hash.
    currState = new nsDFAState(mNextState);
    mNextState++;
    mTransitionTable->Put(&key, currState);
  }

  for (PRUint32 i = 0; i < count; i++)
  {
    nsCOMPtr<nsIAtom> pseudo = getter_AddRefs(NS_STATIC_CAST(nsIAtom*, aInputWord->ElementAt(i)));
    nsTransitionKey key(currState->GetStateID(), pseudo);
    currState = NS_STATIC_CAST(nsDFAState*, mTransitionTable->Get(&key));

    if (!currState) {
      // We had a miss. Make a new state and add it to our hash.
      currState = new nsDFAState(mNextState);
      mNextState++;
      mTransitionTable->Put(&key, currState);
    }
  }

  // We're in a final state.
  // Look up our style context for this state.
   if (mCache)
    *aResult = NS_STATIC_CAST(nsIStyleContext*, mCache->Get(currState)); // Addref occurs on *aResult.
  if (!*aResult) {
    // We missed the cache. Resolve this pseudo-style.
    aPresContext->ResolvePseudoStyleWithComparator(aContent, aPseudoElement,
                                                   aContext, PR_FALSE,
                                                   aComparator,
                                                   aResult); // Addref occurs on *aResult.
    // Put it in our table.
    if (!mCache)
      mCache = new nsSupportsHashtable;
    mCache->Put(currState, *aResult);
  }

  return NS_OK;
}

/* static */ PRBool PR_CALLBACK
nsOutlinerStyleCache::DeleteDFAState(nsHashKey *aKey,
                                     void *aData,
                                     void *closure)
{
  nsDFAState* entry = NS_STATIC_CAST(nsDFAState*, aData);
  delete entry;
  return PR_TRUE;
}

// Column class that caches all the info about our column.
nsOutlinerColumn::nsOutlinerColumn(nsIContent* aColElement, nsIFrame* aFrame)
:mNext(nsnull)
{
  mColFrame = aFrame;
  mColElement = aColElement; 

  // Fetch the ID.
  mColElement->GetAttr(kNameSpaceID_None, nsHTMLAtoms::id, mID);

  // Cache the ID as an atom.
  mIDAtom = getter_AddRefs(NS_NewAtom(mID));

  nsCOMPtr<nsIStyleContext> styleContext;
  aFrame->GetStyleContext(getter_AddRefs(styleContext));

  // Fetch the crop style.
  mCropStyle = 0;
  nsAutoString crop;
  mColElement->GetAttr(kNameSpaceID_None, nsXULAtoms::crop, crop);
  if (crop.EqualsIgnoreCase("center"))
    mCropStyle = 1;
  else if (crop.EqualsIgnoreCase("left") || crop.EqualsIgnoreCase("start"))
    mCropStyle = 2;

  if (mCropStyle == 0 || mCropStyle == 2) { // Left or Right
    const nsStyleVisibility* vis = 
      (const nsStyleVisibility*)styleContext->GetStyleData(eStyleStruct_Visibility);
    if (vis->mDirection == NS_STYLE_DIRECTION_RTL)
      mCropStyle = 2 - mCropStyle; // Right becomes left, left becomes right.
  }

  // Cache our text alignment policy.
  const nsStyleText* textStyle =
        (const nsStyleText*)styleContext->GetStyleData(eStyleStruct_Text);

  mTextAlignment = textStyle->mTextAlign;

  // Figure out if we're the primary column (that has to have indentation
  // and twisties drawn.
  mIsPrimaryCol = PR_FALSE;
  nsAutoString primary;
  mColElement->GetAttr(kNameSpaceID_None, nsXULAtoms::primary, primary);
  if (primary.EqualsIgnoreCase("true"))
    mIsPrimaryCol = PR_TRUE;

  // Figure out if we're a cycling column (one that doesn't cause a selection
  // to happen).
  mIsCyclerCol = PR_FALSE;
  nsAutoString cycler;
  mColElement->GetAttr(kNameSpaceID_None, nsXULAtoms::cycler, cycler);
  if (cycler.EqualsIgnoreCase("true"))
    mIsCyclerCol = PR_TRUE;
}

inline nscoord nsOutlinerColumn::GetWidth()
{
  if (mColFrame) {
    nsRect rect;
    mColFrame->GetRect(rect);
    return rect.width;
  }
  return 0;
}

//
// NS_NewOutlinerFrame
//
// Creates a new outliner frame
//
nsresult
NS_NewOutlinerBodyFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsOutlinerBodyFrame* it = new (aPresShell) nsOutlinerBodyFrame(aPresShell);
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewOutlinerFrame


//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsOutlinerBodyFrame)
  NS_INTERFACE_MAP_ENTRY(nsIOutlinerBoxObject)
  NS_INTERFACE_MAP_ENTRY(nsICSSPseudoComparator)
  NS_INTERFACE_MAP_ENTRY(nsIScrollbarMediator)
NS_INTERFACE_MAP_END_INHERITING(nsLeafFrame)



// Constructor
nsOutlinerBodyFrame::nsOutlinerBodyFrame(nsIPresShell* aPresShell)
:nsLeafBoxFrame(aPresShell), mPresContext(nsnull), mOutlinerBoxObject(nsnull), mFocused(PR_FALSE), mImageCache(nsnull),
 mColumns(nsnull), mScrollbar(nsnull), mTopRowIndex(0), mRowHeight(0), mIndentation(0),
 mDropRow(kIllegalRow), mDropOrient(kNoOrientation), mDropAllowed(PR_FALSE), mIsSortRectDrawn(PR_FALSE),
 mAlreadyUndrewDueToScroll(PR_FALSE)
{
  NS_NewISupportsArray(getter_AddRefs(mScratchArray));
  mColumnsDirty = PR_TRUE;
}

// Destructor
nsOutlinerBodyFrame::~nsOutlinerBodyFrame()
{
  delete mImageCache;
}

NS_IMETHODIMP_(nsrefcnt) 
nsOutlinerBodyFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsOutlinerBodyFrame::Release(void)
{
  return NS_OK;
}

static nsIFrame* InitScrollbarFrame(nsIPresContext* aPresContext, nsIFrame* aCurrFrame, nsIScrollbarMediator* aSM)
{
  // Check ourselves
  nsCOMPtr<nsIScrollbarFrame> sf(do_QueryInterface(aCurrFrame));
  if (sf) {
    sf->SetScrollbarMediator(aSM);
    return aCurrFrame;
  }

  nsIFrame* child;
  aCurrFrame->FirstChild(aPresContext, nsnull, &child);
  while (child) {
    nsIFrame* result = InitScrollbarFrame(aPresContext, child, aSM);
    if (result)
      return result;
    child->GetNextSibling(&child);
  }

  return nsnull;
}

NS_IMETHODIMP
nsOutlinerBodyFrame::Init(nsIPresContext* aPresContext, nsIContent* aContent,
                          nsIFrame* aParent, nsIStyleContext* aContext, nsIFrame* aPrevInFlow)
{
  mPresContext = aPresContext;
  nsresult rv = nsLeafBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  nsBoxFrame::CreateViewForFrame(aPresContext, this, aContext, PR_TRUE);
  nsIView* ourView;
  nsLeafBoxFrame::GetView(aPresContext, &ourView);

static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);

  ourView->CreateWidget(kWidgetCID);
  ourView->GetWidget(*getter_AddRefs(mOutlinerWidget));
  return rv;
}

NS_IMETHODIMP
nsOutlinerBodyFrame::Destroy(nsIPresContext* aPresContext)
{
  // Delete our column structures.
  delete mColumns;
  mColumns = nsnull;
  
  // Save off our info into the box object.
  if (mOutlinerBoxObject) {
    nsCOMPtr<nsIBoxObject> box(do_QueryInterface(mOutlinerBoxObject));
    if (mTopRowIndex > 0) {
      nsAutoString topRowStr; topRowStr.AssignWithConversion("topRow");
      nsAutoString topRow;
      topRow.AppendInt(mTopRowIndex);
      box->SetProperty(topRowStr.get(), topRow.get());
    }

    // Always null out the cached outliner body frame.
    nsAutoString outlinerBody; outlinerBody.AssignWithConversion("outlinerbody");
    box->RemoveProperty(outlinerBody.get());

    mOutlinerBoxObject = nsnull; // Drop our ref here.
  }

  mView = nsnull;

  return nsLeafBoxFrame::Destroy(aPresContext);
}

NS_IMETHODIMP nsOutlinerBodyFrame::Reflow(nsIPresContext* aPresContext,
                                          nsHTMLReflowMetrics& aReflowMetrics,
                                          const nsHTMLReflowState& aReflowState,
                                          nsReflowStatus& aStatus)
{
  if (aReflowState.reason == eReflowReason_Initial) {
    // We might have a box object with some properties already cached.  If so,
    // pull them out of the box object and restore them here.
    mRowHeight = GetRowHeight();
    nsCOMPtr<nsIContent> parent;
    mContent->GetParent(*getter_AddRefs(parent));
    nsCOMPtr<nsIDOMXULElement> parentXUL(do_QueryInterface(parent));
    if (parentXUL) {
      nsCOMPtr<nsIBoxObject> box;
      parentXUL->GetBoxObject(getter_AddRefs(box));
      if (box) {
        nsCOMPtr<nsIOutlinerBoxObject> outlinerBox(do_QueryInterface(box));
        SetBoxObject(outlinerBox);

        nsAutoString view; view.AssignWithConversion("view");
        nsCOMPtr<nsISupports> suppView;
        box->GetPropertyAsSupports(view.get(), getter_AddRefs(suppView));
        nsCOMPtr<nsIOutlinerView> outlinerView(do_QueryInterface(suppView));

        if (outlinerView) {
          nsAutoString topRow; topRow.AssignWithConversion("topRow");
          nsXPIDLString rowStr;
          box->GetProperty(topRow.get(), getter_Copies(rowStr));
          nsAutoString rowStr2(rowStr);
          PRInt32 error;
          PRInt32 rowIndex = rowStr2.ToInteger(&error);
      
          // Set our view.
          SetView(outlinerView);

          // Scroll to the given row.
          ScrollToRow(rowIndex);

          // Clear out the property info for the top row, but we always keep the
          // view current.
          box->RemoveProperty(topRow.get());

          return nsLeafBoxFrame::Reflow(aPresContext, aReflowMetrics, aReflowState, aStatus);
        }
      }
    }

    // See if there is a XUL outliner builder associated with the
    // element. If so, try to make *it* be the view.
    nsCOMPtr<nsIDOMXULElement> xulele = do_QueryInterface(mContent);
    if (xulele) {
      nsCOMPtr<nsIXULTemplateBuilder> builder;
      xulele->GetBuilder(getter_AddRefs(builder));
      if (builder) {
        nsCOMPtr<nsIOutlinerView> view = do_QueryInterface(builder);
        if (view)
          SetView(view);
      }
    }
  }

  if (mView && mRowHeight && aReflowState.reason == eReflowReason_Resize) {
    mInnerBox = GetInnerBox();
    mPageCount = mInnerBox.height / mRowHeight;

    PRInt32 rowCount;
    mView->GetRowCount(&rowCount);
    PRInt32 lastPageTopRow = rowCount - mPageCount;
    if (mTopRowIndex >= lastPageTopRow)
      ScrollToRow(lastPageTopRow);

    InvalidateScrollbar();
    SetVisibleScrollbar((rowCount >= mPageCount));
  }

  return nsLeafBoxFrame::Reflow(aPresContext, aReflowMetrics, aReflowState, aStatus);
}

static void 
AdjustForBorderPadding(nsIStyleContext* aContext, nsRect& aRect)
{
  nsMargin m(0,0,0,0);
  nsStyleBorderPadding  bPad;
  aContext->GetBorderPaddingFor(bPad);
  bPad.GetBorderPadding(m);
  aRect.Deflate(m);
}

NS_IMETHODIMP nsOutlinerBodyFrame::GetView(nsIOutlinerView * *aView)
{
  *aView = mView;
  NS_IF_ADDREF(*aView);
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::SetView(nsIOutlinerView * aView)
{
  // First clear out the old view.
  nsCOMPtr<nsIBoxObject> box(do_QueryInterface(mOutlinerBoxObject));
  if (!box)
    return NS_OK; // Just ignore the call.  An initial reflow when it comes in
                  // will retrieve the view from the box object.
  
  nsAutoString view; view.AssignWithConversion("view");
  
  if (mView) {
    mView->SetOutliner(nsnull);
    mView = nsnull;
    box->RemoveProperty(view.get());

    // Only reset the top row index and delete the columns if we had an old non-null view.
    mTopRowIndex = 0;
    delete mColumns;
    mColumns = nsnull;
  }

  // Outliner, meet the view.
  mView = aView;
 
  // Changing the view causes us to refetch our data.  This will
  // necessarily entail a full invalidation of the outliner.
  Invalidate();
 
  if (mView) {
    // View, meet the outliner.
    mView->SetOutliner(mOutlinerBoxObject);
 
    box->SetPropertyAsSupports(view.get(), mView);

    // Give the view a new empty selection object to play with, but only if it
    // doesn't have one already.
    nsCOMPtr<nsIOutlinerSelection> sel;
    mView->GetSelection(getter_AddRefs(sel));
    if (!sel) {
      NS_NewOutlinerSelection(this, getter_AddRefs(sel));
      mView->SetSelection(sel);
    }

    // The scrollbar will need to be updated.
    InvalidateScrollbar();

    PRInt32 rowCount;
    mView->GetRowCount(&rowCount);
    SetVisibleScrollbar((rowCount >= mPageCount));
  }
 
  return NS_OK;
}

NS_IMETHODIMP 
nsOutlinerBodyFrame::GetFocused(PRBool* aFocused)
{
  *aFocused = mFocused;
  return NS_OK;
}

NS_IMETHODIMP 
nsOutlinerBodyFrame::SetFocused(PRBool aFocused)
{
  mFocused = aFocused;
  if (mView) {
    nsCOMPtr<nsIOutlinerSelection> sel;
    mView->GetSelection(getter_AddRefs(sel));
    if (sel)
      sel->InvalidateSelection();
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsOutlinerBodyFrame::GetOutlinerBody(nsIDOMElement** aElement)
{
  return mContent->QueryInterface(NS_GET_IID(nsIDOMElement), (void**)aElement);
}

NS_IMETHODIMP nsOutlinerBodyFrame::GetSelection(nsIOutlinerSelection** aSelection)
{
  if (mView)
    return mView->GetSelection(aSelection);

  *aSelection = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::GetRowHeight(PRInt32* _retval)
{
  PRInt32 height = mRowHeight == 0 ? GetRowHeight() : mRowHeight;

  float t2p;
  mPresContext->GetTwipsToPixels(&t2p);
  *_retval = NSToCoordRound((float) height * t2p);

  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::GetFirstVisibleRow(PRInt32 *_retval)
{
  *_retval = mTopRowIndex;
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::GetLastVisibleRow(PRInt32 *_retval)
{
  *_retval = mTopRowIndex + mPageCount - 1;
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::GetPageCount(PRInt32 *_retval)
{
  *_retval = mPageCount;
  return NS_OK;
}


NS_IMETHODIMP nsOutlinerBodyFrame::Invalidate()
{
  nsLeafBoxFrame::Invalidate(mPresContext, mRect, PR_FALSE);
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::InvalidateRow(PRInt32 aIndex)
{
  if (aIndex < mTopRowIndex || aIndex > mTopRowIndex + mPageCount + 1)
    return NS_OK;

  nsRect rowRect(mInnerBox.x, mInnerBox.y+mRowHeight*(aIndex-mTopRowIndex), mInnerBox.width, mRowHeight);
  nsLeafBoxFrame::Invalidate(mPresContext, rowRect, PR_FALSE);
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::InvalidateCell(PRInt32 aIndex, const PRUnichar *aColID)
{
  if (aIndex < mTopRowIndex || aIndex > mTopRowIndex + mPageCount + 1)
    return NS_OK;

  nscoord currX = mInnerBox.x;
  for (nsOutlinerColumn* currCol = mColumns; currCol && currX < mInnerBox.x+mInnerBox.width; 
       currCol = currCol->GetNext()) {
    nsRect colRect(currX, mInnerBox.y, currCol->GetWidth(), mInnerBox.height);
    PRInt32 overflow = colRect.x+colRect.width-(mInnerBox.x+mInnerBox.width);
    if (overflow > 0)
      colRect.width -= overflow;

    if (nsCRT::strcmp(currCol->GetID(), aColID) == 0) {
      nsLeafBoxFrame::Invalidate(mPresContext, colRect, PR_FALSE);
      break;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::InvalidateRange(PRInt32 aStart, PRInt32 aEnd)
{
  if (aStart == aEnd)
    return InvalidateRow(aStart);

  PRInt32 last;
  GetLastVisibleRow(&last);
  if (aEnd < mTopRowIndex || aStart > last)
    return NS_OK;

  if (aStart < mTopRowIndex)
    aStart = mTopRowIndex;

  if (aEnd > mTopRowIndex + mPageCount + 1)
    aEnd = mTopRowIndex + mPageCount + 1;

  nsRect rangeRect(mInnerBox.x, mInnerBox.y+mRowHeight*(aStart-mTopRowIndex), mInnerBox.width, mRowHeight*(aEnd-aStart+1));
  nsLeafBoxFrame::Invalidate(mPresContext, rangeRect, PR_FALSE);

  return NS_OK;
}

void
nsOutlinerBodyFrame::UpdateScrollbar()
{
  // Update the scrollbar.
  nsCOMPtr<nsIContent> scrollbarContent;
  NS_ASSERTION(mScrollbar, "no scroll bar");
  if (!mScrollbar)
    return;
  mScrollbar->GetContent(getter_AddRefs(scrollbarContent));
  float t2p;
  mPresContext->GetTwipsToPixels(&t2p);
  nscoord rowHeightAsPixels = NSToCoordRound((float)mRowHeight*t2p);

  nsAutoString curPos;
  curPos.AppendInt(mTopRowIndex*rowHeightAsPixels);
  scrollbarContent->SetAttr(kNameSpaceID_None, nsXULAtoms::curpos, curPos, PR_TRUE);
}

nsresult nsOutlinerBodyFrame::SetVisibleScrollbar(PRBool aSetVisible)
{
  NS_ASSERTION(mScrollbar, "no scroll bar");
  if (!mScrollbar)
    return NS_OK;

  nsCOMPtr<nsIContent> scrollbarContent;
  mScrollbar->GetContent(getter_AddRefs(scrollbarContent));

  nsAutoString isCollapsed;
  scrollbarContent->GetAttr(kNameSpaceID_None, nsXULAtoms::collapsed,
                            isCollapsed);
  
  if (!isCollapsed.IsEmpty() && aSetVisible)
    scrollbarContent->UnsetAttr(kNameSpaceID_None, nsXULAtoms::collapsed, 
                                PR_TRUE);
  else if (isCollapsed.IsEmpty() && !aSetVisible)
    scrollbarContent->SetAttr(kNameSpaceID_None, nsXULAtoms::collapsed,
                              NS_LITERAL_STRING("true"), PR_TRUE);

  Invalidate();

  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::InvalidateScrollbar()
{
  if (!mScrollbar) {
    // Try to find it.
    nsCOMPtr<nsIContent> parContent;
    mContent->GetParent(*getter_AddRefs(parContent));
    nsCOMPtr<nsIPresShell> shell;
    mPresContext->GetShell(getter_AddRefs(shell));
    nsIFrame* outlinerFrame;
    shell->GetPrimaryFrameFor(parContent, &outlinerFrame);
    if (outlinerFrame)
      mScrollbar = InitScrollbarFrame(mPresContext, outlinerFrame, this);
  }

  NS_ASSERTION(mScrollbar, "no scroll bar");
  if (!mScrollbar || !mView)
    return NS_OK;

  PRInt32 rowCount = 0;
  mView->GetRowCount(&rowCount);
  
  nsCOMPtr<nsIContent> scrollbar;
  mScrollbar->GetContent(getter_AddRefs(scrollbar));

  nsAutoString maxposStr;

  float t2p;
  mPresContext->GetTwipsToPixels(&t2p);
  nscoord rowHeightAsPixels = NSToCoordRound((float)mRowHeight*t2p);

  PRInt32 size = rowHeightAsPixels*(rowCount-mPageCount);
  maxposStr.AppendInt(size);
  scrollbar->SetAttr(kNameSpaceID_None, nsXULAtoms::maxpos, maxposStr, PR_TRUE);

  // Also set our page increment and decrement.
  nscoord pageincrement = mPageCount*rowHeightAsPixels;
  nsAutoString pageStr;
  pageStr.AppendInt(pageincrement);
  scrollbar->SetAttr(kNameSpaceID_None, nsXULAtoms::pageincrement, pageStr, PR_TRUE);

  return NS_OK;
}


//
// AdjustEventCoordsToBoxCoordSpace
//
// Takes client x/y in pixels, converts them to twips, and massages them to be
// in our coordinate system.
//
void
nsOutlinerBodyFrame :: AdjustEventCoordsToBoxCoordSpace ( PRInt32 inX, PRInt32 inY, PRInt32* outX, PRInt32* outY )
{
  // Convert our x and y coords to twips.
  float pixelsToTwips = 0.0;
  mPresContext->GetPixelsToTwips(&pixelsToTwips);
  inX = NSToIntRound(inX * pixelsToTwips);
  inY = NSToIntRound(inY * pixelsToTwips);
  
  // Get our box object.
  nsCOMPtr<nsIDocument> doc;
  mContent->GetDocument(*getter_AddRefs(doc));
  nsCOMPtr<nsIDOMNSDocument> nsDoc(do_QueryInterface(doc));
  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(mContent));
  
  nsCOMPtr<nsIBoxObject> boxObject;
  nsDoc->GetBoxObjectFor(elt, getter_AddRefs(boxObject));
  
  PRInt32 x;
  PRInt32 y;
  boxObject->GetX(&x);
  boxObject->GetY(&y);

  x = NSToIntRound(x * pixelsToTwips);
  y = NSToIntRound(y * pixelsToTwips);

  // Adjust into our coordinate space.
  x = inX-x;
  y = inY-y;

  // Adjust y by the inner box y, so that we're in the inner box's
  // coordinate space.
  y += mInnerBox.y;
  
  *outX = x;
  *outY = y;

} // AdjustEventCoordsToBoxCoordSpace


NS_IMETHODIMP nsOutlinerBodyFrame::GetCellAt(PRInt32 aX, PRInt32 aY, PRInt32* aRow, PRUnichar** aColID,
                                             PRUnichar** aChildElt)
{
  // Ensure we have a row height.
  if (mRowHeight == 0)
    mRowHeight = GetRowHeight();

  PRInt32 x, y;
  AdjustEventCoordsToBoxCoordSpace ( aX, aY, &x, &y );
  
#if 0
  // Convert our x and y coords to twips.
  float pixelsToTwips = 0.0;
  mPresContext->GetPixelsToTwips(&pixelsToTwips);
  aX = NSToIntRound(aX * pixelsToTwips);
  aY = NSToIntRound(aY * pixelsToTwips);
  
  // Get our box object.
  nsCOMPtr<nsIDocument> doc;
  mContent->GetDocument(*getter_AddRefs(doc));
  nsCOMPtr<nsIDOMNSDocument> nsDoc(do_QueryInterface(doc));
  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(mContent));
  
  nsCOMPtr<nsIBoxObject> boxObject;
  nsDoc->GetBoxObjectFor(elt, getter_AddRefs(boxObject));
  
  PRInt32 x;
  PRInt32 y;
  boxObject->GetX(&x);
  boxObject->GetY(&y);

  x = NSToIntRound(x * pixelsToTwips);
  y = NSToIntRound(y * pixelsToTwips);

  // Adjust into our coordinate space.
  x = aX-x;
  y = aY-y;

  // Adjust y by the inner box y, so that we're in the inner box's
  // coordinate space.
  y += mInnerBox.y;
#endif

  // Now just mod by our total inner box height and add to our top row index.
  *aRow = (y/mRowHeight)+mTopRowIndex;

  // Determine the column hit.
  nscoord currX = mInnerBox.x;
  for (nsOutlinerColumn* currCol = mColumns; currCol && currX < mInnerBox.x+mInnerBox.width; 
       currCol = currCol->GetNext()) {
    nsRect cellRect(currX, mInnerBox.y+mRowHeight*(*aRow-mTopRowIndex), currCol->GetWidth(), mRowHeight);
    PRInt32 overflow = cellRect.x+cellRect.width-(mInnerBox.x+mInnerBox.width);
    if (overflow > 0)
      cellRect.width -= overflow;

    if (x >= cellRect.x && x < cellRect.x + cellRect.width) {
      // We know the column hit now.
      *aColID = nsCRT::strdup(currCol->GetID());

      if (currCol->IsCycler())
        // Cyclers contain only images.  Fill this in immediately and return.
        *aChildElt = ToNewUnicode(NS_LITERAL_STRING("image"));
      else
        GetItemWithinCellAt(x, cellRect, *aRow, currCol, aChildElt);
      break;
    }

    currX += cellRect.width;
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
nsOutlinerBodyFrame::GetCoordsForCellItem(PRInt32 aRow, const PRUnichar *aColID, const PRUnichar *aCellItem, 
                                          PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight)
{
  *aX = 0;
  *aY = 0;
  *aWidth = 0;
  *aHeight = 0;

  nscoord currX = mInnerBox.x;

  // The Rect for the requested item. 
  nsRect theRect;

  for (nsOutlinerColumn* currCol = mColumns; currCol && currX < mInnerBox.x + mInnerBox.width;
       currCol = currCol->GetNext()) {

    // The Rect for the current cell. 
    nsRect cellRect(currX, mInnerBox.y + mRowHeight * (aRow - mTopRowIndex), currCol->GetWidth(), mRowHeight);

    nsAutoString colID;
    currCol->GetID(colID);
    // Check the ID of the current column to see if it matches. If it doesn't 
    // increment the current X value and continue to the next column.
    if (!colID.EqualsWithConversion(aColID)) {
      currX += cellRect.width;
      continue;
    }

    // Now obtain the properties for our cell.
    PrefillPropertyArray(aRow, currCol);
    mView->GetCellProperties(aRow, currCol->GetID(), mScratchArray);

    nsCOMPtr<nsIStyleContext> rowContext;
    GetPseudoStyleContext(nsXULAtoms::mozoutlinerrow, getter_AddRefs(rowContext));

    // We don't want to consider any of the decorations that may be present
    // on the current row, so we have to deflate the rect by the border and 
    // padding and offset its left and top coordinates appropriately. 
    AdjustForBorderPadding(rowContext, cellRect);

    nsCOMPtr<nsIStyleContext> cellContext;
    GetPseudoStyleContext(nsXULAtoms::mozoutlinercell, getter_AddRefs(cellContext));

    nsAutoString cell; cell.AssignWithConversion("cell");
    if (currCol->IsCycler() || cell.EqualsWithConversion(aCellItem)) {
      // If the current Column is a Cycler, then the Rect is just the cell - the margins. 
      // Similarly, if we're just being asked for the cell rect, provide it. 

      theRect = cellRect;
      const nsStyleMargin* cellMarginData = (const nsStyleMargin*) cellContext->GetStyleData(eStyleStruct_Margin);
      nsMargin cellMargin;
      cellMarginData->GetMargin(cellMargin);
      theRect.Deflate(cellMargin);
      break;
    }

    // Since we're not looking for the cell, and since the cell isn't a cycler,
    // we're looking for some subcomponent, and now we need to subtract the 
    // borders and padding of the cell from cellRect so this does not 
    // interfere with our computations.
    AdjustForBorderPadding(cellContext, cellRect);

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

      // Start the Twisty Rect as the full width of the cell, and gradually decrement its width
      // as we figure out the size of other elements. 
      nsRect twistyRect(cellX, cellRect.y, remainWidth, cellRect.height);

      PRBool hasTwisty = PR_FALSE;
      PRBool isContainer = PR_FALSE;
      mView->IsContainer(aRow, &isContainer);
      if (isContainer) {
        PRBool isContainerEmpty = PR_FALSE;
        mView->IsContainerEmpty(aRow, &isContainerEmpty);
        if (!isContainerEmpty)
          hasTwisty = PR_TRUE;
      }

      // Find the twisty rect by computing its size. 
      nsCOMPtr<nsIStyleContext> twistyContext;
      GetPseudoStyleContext(nsXULAtoms::mozoutlinertwisty, getter_AddRefs(twistyContext));

      // |GetImageSize| returns the rect of the twisty image, including the 
      // borders and padding. 
      nsRect twistyImageRect = GetImageSize(aRow, currCol->GetID(), twistyContext);
      if (NS_LITERAL_STRING("twisty").Equals(aCellItem)) {
        // If we're looking for the twisty Rect, just return the result of |GetImageSize|
        theRect = twistyImageRect;
        break;
      }
      
      // Now we need to add in the margins of the twisty element, so that we 
      // can find the offset of the next element in the cell. 
      const nsStyleMargin* twistyMarginData = (const nsStyleMargin*) twistyContext->GetStyleData(eStyleStruct_Margin);
      nsMargin twistyMargin;
      twistyMarginData->GetMargin(twistyMargin);
      twistyImageRect.Inflate(twistyMargin);

      // Adjust our working X value with the twisty width (image size, margins,
      // borders, padding. 
      cellX += twistyImageRect.width;
    }

    // Cell Image
    nsCOMPtr<nsIStyleContext> imageContext;
    GetPseudoStyleContext(nsXULAtoms::mozoutlinerimage, getter_AddRefs(imageContext));

    nsRect imageSize = GetImageSize(aRow, currCol->GetID(), imageContext);
    if (NS_LITERAL_STRING("image").Equals(aCellItem)) {
      theRect = imageSize;
      theRect.x = cellX;
      theRect.y = cellRect.y;
      break;
    }

    // Increment cellX by the image width
    cellX += imageSize.width;
    
    // Cell Text 
    nsXPIDLString text;
    mView->GetCellText(aRow, currCol->GetID(), getter_Copies(text));
    nsAutoString cellText(text);

    // Create a scratch rect to represent the text rectangle, with the current 
    // X and Y coords, and a guess at the width and height. The width is the 
    // remaining width we have left to traverse in the cell, which will be the
    // widest possible value for the text rect, and the row height. 
    nsRect textRect(cellX, cellRect.y, remainWidth, mRowHeight);

    // Measure the width of the text. If the width of the text is greater than 
    // the remaining width available, then we just assume that the text has 
    // been cropped and use the remaining rect as the text Rect. Otherwise,
    // we add in borders and padding to the text dimension and give that back. 
    nsCOMPtr<nsIStyleContext> textContext;
    GetPseudoStyleContext(nsXULAtoms::mozoutlinercelltext, getter_AddRefs(textContext));

    const nsStyleFont* fontStyle = (const nsStyleFont*) textContext->GetStyleData(eStyleStruct_Font);
    nsCOMPtr<nsIDeviceContext> dc;

    mPresContext->GetDeviceContext(getter_AddRefs(dc));
    nsCOMPtr<nsIFontMetrics> fm;
    dc->GetMetricsFor(fontStyle->mFont, *getter_AddRefs(fm));
    nscoord height;
    fm->GetHeight(height);

    nsStyleBorderPadding borderPadding;
    textContext->GetBorderPaddingFor(borderPadding);
    nsMargin bp(0,0,0,0);
    borderPadding.GetBorderPadding(bp);
    
    textRect.height = height + bp.top + bp.bottom;

    nsCOMPtr<nsIPresShell> shell;
    mPresContext->GetShell(getter_AddRefs(shell));
    nsCOMPtr<nsIRenderingContext> rc;
    shell->CreateRenderingContext(this, getter_AddRefs(rc));
    rc->SetFont(fm);
    nscoord width;
    rc->GetWidth(cellText, width);

    nscoord totalTextWidth = width + bp.left + bp.right;
    if (totalTextWidth < remainWidth) {
      // If the text is not cropped, the text is smaller than the available 
      // space and we set the text rect to be that width. 
      textRect.width = totalTextWidth;
    }

    theRect = textRect;
  }
  
  float t2p = 0.0;
  mPresContext->GetTwipsToPixels(&t2p);
  
  *aX = NSToIntRound(theRect.x * t2p);
  *aY = NSToIntRound(theRect.y * t2p);
  *aWidth = NSToIntRound(theRect.width * t2p);
  *aHeight = NSToIntRound(theRect.height * t2p);
 
  return NS_OK;
}

nsresult
nsOutlinerBodyFrame::GetItemWithinCellAt(PRInt32 aX, const nsRect& aCellRect, 
                                         PRInt32 aRowIndex,
                                         nsOutlinerColumn* aColumn, PRUnichar** aChildElt)
{
  // Obtain the properties for our cell.
  PrefillPropertyArray(aRowIndex, aColumn);
  mView->GetCellProperties(aRowIndex, aColumn->GetID(), mScratchArray);

  // Resolve style for the cell.
  nsCOMPtr<nsIStyleContext> cellContext;
  GetPseudoStyleContext(nsXULAtoms::mozoutlinercell, getter_AddRefs(cellContext));

  // Obtain the margins for the cell and then deflate our rect by that 
  // amount.  The cell is assumed to be contained within the deflated rect.
  nsRect cellRect(aCellRect);
  const nsStyleMargin* cellMarginData = (const nsStyleMargin*)cellContext->GetStyleData(eStyleStruct_Margin);
  nsMargin cellMargin;
  cellMarginData->GetMargin(cellMargin);
  cellRect.Deflate(cellMargin);

  // Adjust the rect for its border and padding.
  AdjustForBorderPadding(cellContext, cellRect);

  if (aX < cellRect.x || aX >= cellRect.x + cellRect.width) {
    // The user clicked within the cell's margins/borders/padding.  This constitutes a click on the cell.
    *aChildElt = ToNewUnicode(NS_LITERAL_STRING("cell"));
    return NS_OK;
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
      *aChildElt = ToNewUnicode(NS_LITERAL_STRING("cell"));
      return NS_OK;
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

    // Resolve style for the twisty.
    nsCOMPtr<nsIStyleContext> twistyContext;
    GetPseudoStyleContext(nsXULAtoms::mozoutlinertwisty, getter_AddRefs(twistyContext));

    // We will treat a click as hitting the twisty if it happens on the margins, borders, padding,
    // or content of the twisty object.  By allowing a "slop" into the margin, we make it a little
    // bit easier for a user to hit the twisty.  (We don't want to be too picky here.)
    nsRect imageSize = GetImageSize(aRowIndex, aColumn->GetID(), twistyContext);
    const nsStyleMargin* twistyMarginData = (const nsStyleMargin*)twistyContext->GetStyleData(eStyleStruct_Margin);
    nsMargin twistyMargin;
    twistyMarginData->GetMargin(twistyMargin);
    imageSize.Inflate(twistyMargin);
    twistyRect.width = imageSize.width;

    // Now we test to see if aX is actually within the twistyRect.  If it is, and if the item should
    // have a twisty, then we return "twisty".  If it is within the rect but we shouldn't have a twisty,
    // then we return "cell".
    if (aX >= twistyRect.x && aX < twistyRect.x + twistyRect.width) {
      if (hasTwisty)
        *aChildElt = ToNewUnicode(NS_LITERAL_STRING("twisty"));
      else
        *aChildElt = ToNewUnicode(NS_LITERAL_STRING("cell"));
      return NS_OK;
    }

    currX += twistyRect.width;
    remainingWidth -= twistyRect.width;    
  }
  
  // Now test to see if the user hit the icon for the cell.
  nsRect iconRect(currX, cellRect.y, remainingWidth, cellRect.height);
  
  // Resolve style for the image.
  nsCOMPtr<nsIStyleContext> imageContext;
  GetPseudoStyleContext(nsXULAtoms::mozoutlinerimage, getter_AddRefs(imageContext));

  nsRect iconSize = GetImageSize(aRowIndex, aColumn->GetID(), imageContext);
  const nsStyleMargin* imageMarginData = (const nsStyleMargin*)imageContext->GetStyleData(eStyleStruct_Margin);
  nsMargin imageMargin;
  imageMarginData->GetMargin(imageMargin);
  iconSize.Inflate(imageMargin);
  iconRect.width = iconSize.width;

  if (aX >= iconRect.x && aX < iconRect.x + iconRect.width) {
    // The user clicked on the image.
    *aChildElt = ToNewUnicode(NS_LITERAL_STRING("image"));
    return NS_OK;
  }

  // Just assume "text".
  // XXX For marquee selection, we'll have to make this more precise and do text measurement.
  *aChildElt = ToNewUnicode(NS_LITERAL_STRING("text"));
  return NS_OK;
}


NS_IMETHODIMP nsOutlinerBodyFrame::RowCountChanged(PRInt32 aIndex, PRInt32 aCount)
{
  if (aCount == 0 || !mView)
    return NS_OK; // Nothing to do.

  PRInt32 count = aCount > 0 ? aCount : -aCount;
  PRInt32 rowCount;
  mView->GetRowCount(&rowCount);

  // Adjust our selection.
  nsCOMPtr<nsIOutlinerSelection> sel;
  mView->GetSelection(getter_AddRefs(sel));
  if (sel)
    sel->AdjustSelection(aIndex, aCount);

  PRInt32 last;
  GetLastVisibleRow(&last);
  if (aIndex >= mTopRowIndex && aIndex <= last)
    InvalidateRange(aIndex, last);

  if (mTopRowIndex == 0) {    
    // Just update the scrollbar and return.
    InvalidateScrollbar();
    SetVisibleScrollbar((rowCount >= mPageCount));
    return NS_OK;
  }

  // Adjust our top row index.
  if (aCount > 0) {
    if (mTopRowIndex > aIndex) {
      // Rows came in above us.  Augment the top row index.
      mTopRowIndex += aCount;
      UpdateScrollbar();
    }
  }
  else if (aCount < 0) {
    if (mTopRowIndex > aIndex+count-1) {
      // No need to invalidate. The remove happened
      // completely above us (offscreen).
      mTopRowIndex -= count;
      UpdateScrollbar();
    }
    else if (mTopRowIndex >= aIndex) {
      // This is a full-blown invalidate.
      if (mTopRowIndex + mPageCount > rowCount - 1)
        mTopRowIndex = PR_MAX(0, rowCount - 1 - mPageCount);
      UpdateScrollbar();
      Invalidate();
    }
  }

  InvalidateScrollbar();
  SetVisibleScrollbar((rowCount >= mPageCount));
  return NS_OK;
}

void
nsOutlinerBodyFrame::PrefillPropertyArray(PRInt32 aRowIndex, nsOutlinerColumn* aCol)
{
  mScratchArray->Clear();
  
  // focus
  if (mFocused)
    mScratchArray->AppendElement(nsXULAtoms::focus);

  if (aRowIndex != -1) {
    nsCOMPtr<nsIOutlinerSelection> selection;
    mView->GetSelection(getter_AddRefs(selection));
  
    if (selection) {
      // selected
      PRBool isSelected;
      selection->IsSelected(aRowIndex, &isSelected);
      if (isSelected)
        mScratchArray->AppendElement(nsHTMLAtoms::selected);

      // current
      PRInt32 currentIndex;
      selection->GetCurrentIndex(&currentIndex);
      if (aRowIndex == currentIndex)
        mScratchArray->AppendElement(nsXULAtoms::current);
    }

    // container or leaf
    PRBool isContainer = PR_FALSE;
    mView->IsContainer(aRowIndex, &isContainer);
    if (isContainer) {
      mScratchArray->AppendElement(nsXULAtoms::container);

      // open or closed
      PRBool isOpen = PR_FALSE;
      mView->IsContainerOpen(aRowIndex, &isOpen);
      if (isOpen)
        mScratchArray->AppendElement(nsXULAtoms::open);
      else
        mScratchArray->AppendElement(nsXULAtoms::closed);
    }
    else {
      mScratchArray->AppendElement(nsXULAtoms::leaf);
    }
  }

  if (aCol) {
    nsCOMPtr<nsIAtom> colID;
    aCol->GetIDAtom(getter_AddRefs(colID));
    mScratchArray->AppendElement(colID);
  }
}

#ifdef USE_IMG2
nsresult
nsOutlinerBodyFrame::GetImage(PRInt32 aRowIndex, const PRUnichar* aColID, 
                              nsIStyleContext* aStyleContext, imgIContainer** aResult)
{
  *aResult = nsnull;
  if (mImageCache) {
    // Look the image up in our cache.
    nsISupportsKey key(aStyleContext);
    nsCOMPtr<imgIRequest> imgReq = getter_AddRefs(NS_STATIC_CAST(imgIRequest*, mImageCache->Get(&key)));
    if (imgReq) {
      // Find out if the image has loaded.
      PRUint32 status;
      imgReq->GetImageStatus(&status);
      imgReq->GetImage(aResult); // We hand back the image here.  The GetImage call addrefs *aResult.
      PRUint32 numFrames = 1;
      if (*aResult)
        (*aResult)->GetNumFrames(&numFrames);

      if ((!(status & imgIRequest::STATUS_LOAD_COMPLETE)) || numFrames > 1) {
        // We either aren't done loading, or we're animating. Add our row as a listener for invalidations.
        nsCOMPtr<imgIDecoderObserver> obs;
        imgReq->GetDecoderObserver(getter_AddRefs(obs));
        nsCOMPtr<nsIOutlinerImageListener> listener(do_QueryInterface(obs));
        if (listener)
          listener->AddRow(aRowIndex);
      }
    }
  }

  if (!*aResult) {
    // We missed. Kick off the image load.
    // Obtain the URL from the style context.
    const nsStyleList* myList =
      (const nsStyleList*)aStyleContext->GetStyleData(eStyleStruct_List);
  
    if (myList->mListStyleImage.Length() > 0) {
      // Create a new nsOutlinerImageListener object and pass it our row and column
      // information.
      nsOutlinerImageListener* listener = new nsOutlinerImageListener(mOutlinerBoxObject, aColID);
      if (!listener)
        return NS_ERROR_OUT_OF_MEMORY;

      listener->AddRow(aRowIndex);

      nsCOMPtr<nsIURI> baseURI;
      nsCOMPtr<nsIDocument> doc;
      mContent->GetDocument(*getter_AddRefs(doc));
      doc->GetBaseURL(*getter_AddRefs(baseURI));
     
      nsCOMPtr<nsIURI> srcURI;
      NS_NewURI(getter_AddRefs(srcURI), myList->mListStyleImage, baseURI);

      nsCOMPtr<imgIRequest> imageRequest;

      nsresult rv;
      nsCOMPtr<imgILoader> il(do_GetService("@mozilla.org/image/loader;1", &rv));
      il->LoadImage(srcURI, nsnull, listener, mPresContext, nsIRequest::LOAD_NORMAL, getter_AddRefs(imageRequest));

      if (!mImageCache) {
        mImageCache = new nsSupportsHashtable(32);
        if (!mImageCache)
          return NS_ERROR_OUT_OF_MEMORY;
      }

      nsISupportsKey key(aStyleContext);
      mImageCache->Put(&key, imageRequest);
    }
  }
  return NS_OK;
}
#endif

nsRect nsOutlinerBodyFrame::GetImageSize(PRInt32 aRowIndex, const PRUnichar* aColID, 
                                         nsIStyleContext* aStyleContext)
{
  // XXX We should respond to visibility rules for collapsed vs. hidden.

  // This method returns the width of the twisty INCLUDING borders and padding.
  // It first checks the style context for a width.  If none is found, it tries to
  // use the default image width for the twisty.  If no image is found, it defaults
  // to border+padding.
  nsRect r(0,0,0,0);
  nsMargin m(0,0,0,0);
  nsStyleBorderPadding  bPad;
  aStyleContext->GetBorderPaddingFor(bPad);
  bPad.GetBorderPadding(m);
  r.Inflate(m);

  // Now r contains our border+padding info.  We now need to get our width and
  // height.
  PRBool needWidth = PR_FALSE;
  PRBool needHeight = PR_FALSE;

  const nsStylePosition* myPosition = (const nsStylePosition*)
        aStyleContext->GetStyleData(eStyleStruct_Position);
  if (myPosition->mWidth.GetUnit() == eStyleUnit_Coord)  {
    PRInt32 val = myPosition->mWidth.GetCoordValue();
    r.width += val;
  }
  else 
    needWidth = PR_TRUE;

  if (myPosition->mHeight.GetUnit() == eStyleUnit_Coord)  {
    PRInt32 val = myPosition->mHeight.GetCoordValue();
    r.height += val;
  }
  else 
    needHeight = PR_TRUE;

  if (needWidth || needHeight) {
#ifdef USE_IMG2
    nsCOMPtr<imgIContainer> image;
    GetImage(aRowIndex, aColID, aStyleContext, getter_AddRefs(image));
    // Get the natural image size.
    if (image) {
      float p2t;
      mPresContext->GetPixelsToTwips(&p2t);

      if (needWidth) {
        // Get the size from the image.
        nscoord width;
        image->GetWidth(&width);
        r.width += NSIntPixelsToTwips(width, p2t); 
      }
    
      if (needHeight) {
        nscoord height;
        image->GetHeight(&height);
        r.height += NSIntPixelsToTwips(height, p2t); 
      }
    }
#endif
  }

  return r;
}

PRInt32 nsOutlinerBodyFrame::GetRowHeight()
{
  // Look up the correct height.  It is equal to the specified height
  // + the specified margins.
  nsCOMPtr<nsIStyleContext> rowContext;
  mScratchArray->Clear();
  GetPseudoStyleContext(nsXULAtoms::mozoutlinerrow, getter_AddRefs(rowContext));
  if (rowContext) {
    const nsStylePosition* myPosition = (const nsStylePosition*)
          rowContext->GetStyleData(eStyleStruct_Position);
    if (myPosition->mHeight.GetUnit() == eStyleUnit_Coord)  {
      PRInt32 val = myPosition->mHeight.GetCoordValue();
      if (val > 0) {
        // XXX Check box-sizing to determine if border/padding should augment the height
        // Inflate the height by our margins.
        nsRect rowRect(0,0,0,val);
        const nsStyleMargin* rowMarginData = (const nsStyleMargin*)rowContext->GetStyleData(eStyleStruct_Margin);
        nsMargin rowMargin;
        rowMarginData->GetMargin(rowMargin);
        rowRect.Inflate(rowMargin);
        val = rowRect.height;
      }
      return val;
    }
  }
  return 19*15; // As good a default as any.
}

PRInt32 nsOutlinerBodyFrame::GetIndentation()
{
  // Look up the correct indentation.  It is equal to the specified indentation width.
  nsCOMPtr<nsIStyleContext> indentContext;
  mScratchArray->Clear();
  GetPseudoStyleContext(nsXULAtoms::mozoutlinerindentation, getter_AddRefs(indentContext));
  if (indentContext) {
    const nsStylePosition* myPosition = (const nsStylePosition*)
          indentContext->GetStyleData(eStyleStruct_Position);
    if (myPosition->mWidth.GetUnit() == eStyleUnit_Coord)  {
      PRInt32 val = myPosition->mWidth.GetCoordValue();
      return val;
    }
  }
  return 16*15; // As good a default as any.
}

nsRect nsOutlinerBodyFrame::GetInnerBox()
{
  nsRect r(0,0,mRect.width, mRect.height);
  nsMargin m(0,0,0,0);
  nsStyleBorderPadding  bPad;
  mStyleContext->GetBorderPaddingFor(bPad);
  bPad.GetBorderPadding(m);
  r.Deflate(m);
  return r;
}

// Painting routines
NS_IMETHODIMP nsOutlinerBodyFrame::Paint(nsIPresContext*      aPresContext,
                                         nsIRenderingContext& aRenderingContext,
                                         const nsRect&        aDirtyRect,
                                         nsFramePaintLayer    aWhichLayer,
                                         PRUint32             aFlags)
{
  // XXX This trap handles an odd bogus 1 pixel invalidation that we keep getting 
  // when scrolling.
  if (aDirtyRect.width == 1)
    return NS_OK;

  if (aWhichLayer != NS_FRAME_PAINT_LAYER_BACKGROUND &&
      aWhichLayer != NS_FRAME_PAINT_LAYER_FOREGROUND)
    return NS_OK;

  const nsStyleVisibility* vis = 
      (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);
  if (!vis->IsVisibleOrCollapsed())
    return NS_OK; // We're invisible.  Don't paint.

  // Handles painting our background, border, and outline.
  nsresult rv = nsLeafFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  if (NS_FAILED(rv)) return rv;

  if (!mView)
    return NS_OK;

  PRBool clipState = PR_FALSE;
  
  // Update our page count, our available height and our row height.
  PRInt32 oldRowHeight = mRowHeight;
  PRInt32 oldPageCount = mPageCount;
  mRowHeight = GetRowHeight();
  mIndentation = GetIndentation();
  mInnerBox = GetInnerBox();
  mPageCount = mInnerBox.height/mRowHeight;

  if (mRowHeight != oldRowHeight || oldPageCount != mPageCount) {
    // Schedule a ResizeReflow that will update our page count properly.
    nsBoxLayoutState state(mPresContext);
    MarkDirty(state);
  }

  PRInt32 rowCount = 0;
  mView->GetRowCount(&rowCount);
  
  // Ensure our column info is built.
  EnsureColumns();

  // Loop through our columns and paint them (e.g., for sorting).  This is only
  // relevant when painting backgrounds, since columns contain no content.  Content
  // is contained in the rows.
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    nscoord currX = mInnerBox.x;
    for (nsOutlinerColumn* currCol = mColumns; currCol && currX < mInnerBox.x+mInnerBox.width; 
         currCol = currCol->GetNext()) {
      nsRect colRect(currX, mInnerBox.y, currCol->GetWidth(), mInnerBox.height);
      PRInt32 overflow = colRect.x+colRect.width-(mInnerBox.x+mInnerBox.width);
      if (overflow > 0)
        colRect.width -= overflow;
      nsRect dirtyRect;
      if (dirtyRect.IntersectRect(aDirtyRect, colRect)) {
        PaintColumn(currCol, colRect, aPresContext, aRenderingContext, aDirtyRect, aWhichLayer); 
      }
      currX += currCol->GetWidth();
    }
  }

  // Loop through our on-screen rows.
  for (PRInt32 i = mTopRowIndex; i < rowCount && i < mTopRowIndex+mPageCount+1; i++) {
    nsRect rowRect(mInnerBox.x, mInnerBox.y+mRowHeight*(i-mTopRowIndex), mInnerBox.width, mRowHeight);
    nsRect dirtyRect;
    if (dirtyRect.IntersectRect(aDirtyRect, rowRect) && rowRect.y < (mInnerBox.y+mInnerBox.height)) {
      PRBool clip = (rowRect.y + rowRect.height > mInnerBox.y + mInnerBox.height);
      if (clip) {
        // We need to clip the last row, since it extends outside our inner box.  Push
        // a clip rect down.
        PRInt32 overflow = (rowRect.y+rowRect.height) - (mInnerBox.y+mInnerBox.height);
        nsRect clipRect(rowRect.x, rowRect.y, mInnerBox.width, mRowHeight-overflow);
        aRenderingContext.PushState();
        aRenderingContext.SetClipRect(clipRect, nsClipCombine_kReplace, clipState);
      }

      PaintRow(i, rowRect, aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);

      if (clip)
        aRenderingContext.PopState(clipState);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::PaintColumn(nsOutlinerColumn*    aColumn,
                                               const nsRect& aColRect,
                                               nsIPresContext*      aPresContext,
                                               nsIRenderingContext& aRenderingContext,
                                               const nsRect&        aDirtyRect,
                                               nsFramePaintLayer    aWhichLayer)
{
  if (aColRect.width == 0)
    return NS_OK; // Don't paint hidden columns.

  // Now obtain the properties for our cell.
  // XXX Automatically fill in the following props: open, closed, container, leaf, selected, focused, and the col ID.
  PrefillPropertyArray(-1, aColumn);
  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(aColumn->GetElement()));
  mView->GetColumnProperties(aColumn->GetID(), elt, mScratchArray);

  // Read special properties from attributes on the column content node
  nsAutoString attr;
  aColumn->GetElement()->GetAttr(kNameSpaceID_None, nsXULAtoms::insertbefore, attr);
  if (attr.EqualsWithConversion("true"))
    mScratchArray->AppendElement(nsXULAtoms::insertbefore);
  attr.AssignWithConversion("");
  aColumn->GetElement()->GetAttr(kNameSpaceID_None, nsXULAtoms::insertafter, attr);
  if (attr.EqualsWithConversion("true"))
    mScratchArray->AppendElement(nsXULAtoms::insertafter);
  
  // Resolve style for the column.  It contains all the info we need to lay ourselves
  // out and to paint.
  nsCOMPtr<nsIStyleContext> colContext;
  GetPseudoStyleContext(nsXULAtoms::mozoutlinercolumn, getter_AddRefs(colContext));

  // Obtain the margins for the cell and then deflate our rect by that 
  // amount.  The cell is assumed to be contained within the deflated rect.
  nsRect colRect(aColRect);
  const nsStyleMargin* colMarginData = (const nsStyleMargin*)colContext->GetStyleData(eStyleStruct_Margin);
  nsMargin colMargin;
  colMarginData->GetMargin(colMargin);
  colRect.Deflate(colMargin);

  PaintBackgroundLayer(colContext, aPresContext, aRenderingContext, colRect, aDirtyRect);

  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::PaintRow(int aRowIndex, const nsRect& aRowRect,
                                            nsIPresContext*      aPresContext,
                                            nsIRenderingContext& aRenderingContext,
                                            const nsRect&        aDirtyRect,
                                            nsFramePaintLayer    aWhichLayer)
{
  // We have been given a rect for our row.  We treat this row like a full-blown
  // frame, meaning that it can have borders, margins, padding, and a background.
  
  // Without a view, we have no data. Check for this up front.
  if (!mView)
    return NS_OK;

  // Now obtain the properties for our row.
  // XXX Automatically fill in the following props: open, closed, container, leaf, selected, focused
  PrefillPropertyArray(aRowIndex, nsnull);
  mView->GetRowProperties(aRowIndex, mScratchArray);

  // Resolve style for the row.  It contains all the info we need to lay ourselves
  // out and to paint.
  nsCOMPtr<nsIStyleContext> rowContext;
  GetPseudoStyleContext(nsXULAtoms::mozoutlinerrow, getter_AddRefs(rowContext));

  // Obtain the margins for the row and then deflate our rect by that 
  // amount.  The row is assumed to be contained within the deflated rect.
  nsRect rowRect(aRowRect);
  const nsStyleMargin* rowMarginData = (const nsStyleMargin*)rowContext->GetStyleData(eStyleStruct_Margin);
  nsMargin rowMargin;
  rowMarginData->GetMargin(rowMargin);
  rowRect.Deflate(rowMargin);

  // If the layer is the background layer, we must paint our borders and background for our
  // row rect.
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer)
    PaintBackgroundLayer(rowContext, aPresContext, aRenderingContext, rowRect, aDirtyRect);

  // Adjust the rect for its border and padding.
  AdjustForBorderPadding(rowContext, rowRect);

  // Now loop over our cells. Only paint a cell if it intersects with our dirty rect.
  nscoord currX = rowRect.x;
  for (nsOutlinerColumn* currCol = mColumns; currCol && currX < mInnerBox.x+mInnerBox.width; 
       currCol = currCol->GetNext()) {
    nsRect cellRect(currX, rowRect.y, currCol->GetWidth(), rowRect.height);
    PRInt32 overflow = cellRect.x+cellRect.width-(mInnerBox.x+mInnerBox.width);
    if (overflow > 0)
      cellRect.width -= overflow;
    nsRect dirtyRect;
    if (dirtyRect.IntersectRect(aDirtyRect, cellRect)) {
      PaintCell(aRowIndex, currCol, cellRect, aPresContext, aRenderingContext, aDirtyRect, aWhichLayer); 
    }
    currX += currCol->GetWidth();
  }

  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::PaintCell(int                  aRowIndex, 
                                             nsOutlinerColumn*    aColumn,
                                             const nsRect&        aCellRect,
                                             nsIPresContext*      aPresContext,
                                             nsIRenderingContext& aRenderingContext,
                                             const nsRect&        aDirtyRect,
                                             nsFramePaintLayer    aWhichLayer)
{
  if (aCellRect.width == 0)
    return NS_OK; // Don't paint cells in hidden columns.

  // Now obtain the properties for our cell.
  // XXX Automatically fill in the following props: open, closed, container, leaf, selected, focused, and the col ID.
  PrefillPropertyArray(aRowIndex, aColumn);
  mView->GetCellProperties(aRowIndex, aColumn->GetID(), mScratchArray);

  // Resolve style for the cell.  It contains all the info we need to lay ourselves
  // out and to paint.
  nsCOMPtr<nsIStyleContext> cellContext;
  GetPseudoStyleContext(nsXULAtoms::mozoutlinercell, getter_AddRefs(cellContext));

  // Obtain the margins for the cell and then deflate our rect by that 
  // amount.  The cell is assumed to be contained within the deflated rect.
  nsRect cellRect(aCellRect);
  const nsStyleMargin* cellMarginData = (const nsStyleMargin*)cellContext->GetStyleData(eStyleStruct_Margin);
  nsMargin cellMargin;
  cellMarginData->GetMargin(cellMargin);
  cellRect.Deflate(cellMargin);

  // If the layer is the background layer, we must paint our borders and background for our
  // row rect.
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer)
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

    // Always leave space for the twisty.
    nsRect twistyRect(currX, cellRect.y, remainingWidth, cellRect.height);
      PaintTwisty(aRowIndex, aColumn, twistyRect, aPresContext, aRenderingContext, aDirtyRect, aWhichLayer,
                  remainingWidth, currX);

    // Resolve the style to use for the connecting lines.
    nsCOMPtr<nsIStyleContext> lineContext;
    GetPseudoStyleContext(nsXULAtoms::mozoutlinerline, getter_AddRefs(lineContext));
    const nsStyleVisibility* vis = 
      (const nsStyleVisibility*)lineContext->GetStyleData(eStyleStruct_Visibility);
    
    if (vis->IsVisibleOrCollapsed() && level && NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
      // Paint the connecting lines.
      aRenderingContext.PushState();

      const nsStyleBorder* borderStyle = (const nsStyleBorder*)lineContext->GetStyleData(eStyleStruct_Border);
      nscolor color;
      PRBool transparent; PRBool foreground;
      borderStyle->GetBorderColor(NS_SIDE_LEFT, color, transparent, foreground);

      aRenderingContext.SetColor(color);
      PRUint8 style;
      style = borderStyle->GetBorderStyle(NS_SIDE_LEFT);
      if (style == NS_STYLE_BORDER_STYLE_DOTTED)
        aRenderingContext.SetLineStyle(nsLineStyle_kDotted);
      else if (style == NS_STYLE_BORDER_STYLE_DASHED)
        aRenderingContext.SetLineStyle(nsLineStyle_kDashed);
      else
        aRenderingContext.SetLineStyle(nsLineStyle_kSolid);

      PRInt32 x;
      PRInt32 y = (aRowIndex - mTopRowIndex) * mRowHeight;

      // Compute the maximal level to paint.
      PRInt32 maxLevel = level;
      if (maxLevel > cellRect.width / mIndentation)
        maxLevel = cellRect.width / mIndentation;

      PRInt32 currentParent = aRowIndex;
      for (PRInt32 i = level; i > 0; i--) {
        if (i <= maxLevel) {
          // Get size of parent image to line up.
          PrefillPropertyArray(currentParent, aColumn);
          mView->GetCellProperties(currentParent, aColumn->GetID(), mScratchArray);

          nsCOMPtr<nsIStyleContext> imageContext;
          GetPseudoStyleContext(nsXULAtoms::mozoutlinerimage, getter_AddRefs(imageContext));

          nsRect imageSize = GetImageSize(currentParent, aColumn->GetID(), imageContext);

          const nsStyleMargin* imageMarginData = (const nsStyleMargin*)imageContext->GetStyleData(eStyleStruct_Margin);
          nsMargin imageMargin;
          imageMarginData->GetMargin(imageMargin);
          imageSize.Inflate(imageMargin);

          // Line up line with the parent image.
          x = currX + imageSize.width / 2;

          // Paint full vertical line only if we have next sibling.
          PRBool hasNextSibling;
          mView->HasNextSibling(currentParent, aRowIndex, &hasNextSibling);
          if (hasNextSibling)
            aRenderingContext.DrawLine(x - (level - i + 1) * mIndentation, y, x - (level - i + 1) * mIndentation, y + mRowHeight);
          else if (i == level)
            aRenderingContext.DrawLine(x - (level - i + 1) * mIndentation, y, x - (level - i + 1) * mIndentation, y + mRowHeight / 2);
        }

        PRInt32 parent;
        mView->GetParentIndex(currentParent, &parent);
        if (parent == -1)
          break;
        currentParent = parent;
      }

      // Don't paint off our cell.
      if (level == maxLevel)
        aRenderingContext.DrawLine(x - mIndentation + 16, y + mRowHeight / 2, x, y + mRowHeight /2);

      PRBool clipState;
      aRenderingContext.PopState(clipState);

      PrefillPropertyArray(aRowIndex, aColumn);
      mView->GetCellProperties(aRowIndex, aColumn->GetID(), mScratchArray);
    }
  }
  
  // Now paint the icon for our cell.
  nsRect iconRect(currX, cellRect.y, remainingWidth, cellRect.height);
  nsRect dirtyRect;
  if (dirtyRect.IntersectRect(aDirtyRect, iconRect))
    PaintImage(aRowIndex, aColumn, iconRect, aPresContext, aRenderingContext, aDirtyRect, aWhichLayer,
               remainingWidth, currX);

  // Now paint our text, but only if we aren't a cycler column.
  // XXX until we have the ability to load images, allow the view to 
  // insert text into cycler columns...
  if (!aColumn->IsCycler()) {
    nsRect textRect(currX, cellRect.y, remainingWidth, cellRect.height);
    nsRect dirtyRect;
    if (dirtyRect.IntersectRect(aDirtyRect, textRect))
      PaintText(aRowIndex, aColumn, textRect, aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerBodyFrame::PaintTwisty(int                  aRowIndex,
                                 nsOutlinerColumn*    aColumn,
                                 const nsRect&        aTwistyRect,
                                 nsIPresContext*      aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 const nsRect&        aDirtyRect,
                                 nsFramePaintLayer    aWhichLayer,
                                 nscoord&             aRemainingWidth,
                                 nscoord&             aCurrX)
{
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
  nsCOMPtr<nsIStyleContext> twistyContext;
  GetPseudoStyleContext(nsXULAtoms::mozoutlinertwisty, getter_AddRefs(twistyContext));

  // Obtain the margins for the twisty and then deflate our rect by that 
  // amount.  The twisty is assumed to be contained within the deflated rect.
  nsRect twistyRect(aTwistyRect);
  const nsStyleMargin* twistyMarginData = (const nsStyleMargin*)twistyContext->GetStyleData(eStyleStruct_Margin);
  nsMargin twistyMargin;
  twistyMarginData->GetMargin(twistyMargin);
  twistyRect.Deflate(twistyMargin);

  // The twisty rect extends all the way to the end of the cell.  This is incorrect.  We need to
  // determine the twisty rect's true width.  This is done by examining the style context for
  // a width first.  If it has one, we use that.  If it doesn't, we use the image's natural width.
  // If the image hasn't loaded and if no width is specified, then we just bail.
  nsRect imageSize = GetImageSize(aRowIndex, aColumn->GetID(), twistyContext);
  twistyRect.width = imageSize.width;

  // Subtract out the remaining width.  This is done even when we don't actually paint a twisty in 
  // this cell, so that cells in different rows still line up.
  nsRect copyRect(twistyRect);
  copyRect.Inflate(twistyMargin);
  aRemainingWidth -= copyRect.width;
  aCurrX += copyRect.width;

  if (shouldPaint) {
    // If the layer is the background layer, we must paint our borders and background for our
    // image rect.
    if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer)
      PaintBackgroundLayer(twistyContext, aPresContext, aRenderingContext, twistyRect, aDirtyRect);
    else if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
      // Time to paint the twisty.
      // Adjust the rect for its border and padding.
      AdjustForBorderPadding(twistyContext, twistyRect);
      AdjustForBorderPadding(twistyContext, imageSize);

#ifdef USE_IMG2
      // Get the image for drawing.
      nsCOMPtr<imgIContainer> image; 
      GetImage(aRowIndex, aColumn->GetID(), twistyContext, getter_AddRefs(image));
      if (image) {
        nsPoint p(twistyRect.x, twistyRect.y);
        
        // Center the image. XXX Obey vertical-align style prop?
        if (imageSize.height < twistyRect.height) {
          p.y += (twistyRect.height - imageSize.height)/2;
          if (((twistyRect.height - imageSize.height)/15)%2 != 0)
            p.y -= 15;
        }
        
        // Paint the image.
        aRenderingContext.DrawImage(image, &imageSize, &p);
      }
#endif
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerBodyFrame::PaintImage(int                  aRowIndex,
                                nsOutlinerColumn*    aColumn,
                                const nsRect&        aImageRect,
                                nsIPresContext*      aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect&        aDirtyRect,
                                nsFramePaintLayer    aWhichLayer,
                                nscoord&             aRemainingWidth,
                                nscoord&             aCurrX)
{
  // Resolve style for the image.
  nsCOMPtr<nsIStyleContext> imageContext;
  GetPseudoStyleContext(nsXULAtoms::mozoutlinerimage, getter_AddRefs(imageContext));

  // Obtain the margins for the twisty and then deflate our rect by that 
  // amount.  The twisty is assumed to be contained within the deflated rect.
  nsRect imageRect(aImageRect);
  const nsStyleMargin* imageMarginData = (const nsStyleMargin*)imageContext->GetStyleData(eStyleStruct_Margin);
  nsMargin imageMargin;
  imageMarginData->GetMargin(imageMargin);
  imageRect.Deflate(imageMargin);

  // If the column isn't a cycler, the image rect extends all the way to the end of the cell.  
  // This is incorrect.  We need to determine the image rect's true width.  This is done by 
  // examining the style context for a width first.  If it has one, we use that.  If it doesn't, 
  // we use the image's natural width.
  // If the image hasn't loaded and if no width is specified, then we just bail.
  nsRect imageSize = GetImageSize(aRowIndex, aColumn->GetID(), imageContext);
  if (!aColumn->IsCycler())
   imageRect.width = imageSize.width;

  // Subtract out the remaining width.
  nsRect copyRect(imageRect);
  copyRect.Inflate(imageMargin);
  aRemainingWidth -= copyRect.width;
  aCurrX += copyRect.width;

  // If the layer is the background layer, we must paint our borders and background for our
  // image rect.
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer)
    PaintBackgroundLayer(imageContext, aPresContext, aRenderingContext, imageRect, aDirtyRect);
  else if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
    // Time to paint the twisty.
    // Adjust the rect for its border and padding.
    AdjustForBorderPadding(imageContext, imageRect);
    AdjustForBorderPadding(imageContext, imageSize);

#ifdef USE_IMG2
    // Get the image for drawing.
    nsCOMPtr<imgIContainer> image; 
    GetImage(aRowIndex, aColumn->GetID(), imageContext, getter_AddRefs(image));
    if (image) {
      nsPoint p(imageRect.x, imageRect.y);
      
      // Center the image. XXX Obey vertical-align style prop?
      if (imageSize.height < imageRect.height) {
        p.y += (imageRect.height - imageSize.height)/2;
        if (((imageRect.height - imageSize.height)/15)%2 != 0)
          p.y -= 15; // One pixel in twips
      }

      // For cyclers, we also want to center the image in the column.
      if (aColumn->IsCycler() && imageSize.width < imageRect.width) {
        p.x += (imageRect.width - imageSize.width)/2;
        if (((imageRect.width - imageSize.width)/15)%2 != 0)
          p.x -= 15; // One pixel in twips
      }

      // Paint the image.
      aRenderingContext.DrawImage(image, &imageSize, &p);
    }
#endif
  }

  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::PaintText(int aRowIndex, 
                                             nsOutlinerColumn*    aColumn,
                                             const nsRect& aTextRect,
                                             nsIPresContext*      aPresContext,
                                             nsIRenderingContext& aRenderingContext,
                                             const nsRect&        aDirtyRect,
                                             nsFramePaintLayer    aWhichLayer)
{
  // Now obtain the text for our cell.
  nsXPIDLString text;
  mView->GetCellText(aRowIndex, aColumn->GetID(), getter_Copies(text));

  nsAutoString realText(text);

  if (realText.Length() == 0)
    return NS_OK; // Don't paint an empty string. XXX What about background/borders? Still paint?

  // Resolve style for the text.  It contains all the info we need to lay ourselves
  // out and to paint.
  nsCOMPtr<nsIStyleContext> textContext;
  GetPseudoStyleContext(nsXULAtoms::mozoutlinercelltext, getter_AddRefs(textContext));

  // Obtain the margins for the text and then deflate our rect by that 
  // amount.  The text is assumed to be contained within the deflated rect.
  nsRect textRect(aTextRect);
  const nsStyleMargin* textMarginData = (const nsStyleMargin*)textContext->GetStyleData(eStyleStruct_Margin);
  nsMargin textMargin;
  textMarginData->GetMargin(textMargin);
  textRect.Deflate(textMargin);

  // If the layer is the background layer, we must paint our borders and background for our
  // text rect.
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer)
    PaintBackgroundLayer(textContext, aPresContext, aRenderingContext, textRect, aDirtyRect);
  else if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
    // Time to paint our text. 
    // Adjust the rect for its border and padding.
    AdjustForBorderPadding(textContext, textRect);
    
    // Compute our text size.
    const nsStyleFont* fontStyle = (const nsStyleFont*)textContext->GetStyleData(eStyleStruct_Font);

    nsCOMPtr<nsIDeviceContext> deviceContext;
    aPresContext->GetDeviceContext(getter_AddRefs(deviceContext));

    nsCOMPtr<nsIFontMetrics> fontMet;
    deviceContext->GetMetricsFor(fontStyle->mFont, *getter_AddRefs(fontMet));
    nscoord height;
    fontMet->GetHeight(height);

    // Center the text. XXX Obey vertical-align style prop?
    if (height < textRect.height) {
      textRect.y += (textRect.height - height)/2;
      textRect.height = height;
    }

    // Set our font.
    aRenderingContext.SetFont(fontMet);

    nscoord width;
    aRenderingContext.GetWidth(realText, width);

    if (width > textRect.width) {
      // See if the width is even smaller than the ellipsis
      // If so, clear the text completely.
      nscoord ellipsisWidth;
      aRenderingContext.GetWidth(ELLIPSIS, ellipsisWidth);

      nsAutoString ellipsis; ellipsis.AssignWithConversion(ELLIPSIS);

      nscoord width = textRect.width;
      if (ellipsisWidth > width)
        realText.SetLength(0);
      else if (ellipsisWidth == width)
        realText = ellipsis;
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
            int length = realText.Length();
            int i;
            for (i = 0; i < length; ++i) {
              PRUnichar ch = realText.CharAt(i);
              aRenderingContext.GetWidth(ch,cwidth);
              if (twidth + cwidth > width)
                break;
              twidth += cwidth;
            }

            realText.Truncate(i);
            realText += ellipsis;
          }
          break;

          case 2: {
            // Crop left.
            nscoord cwidth;
            nscoord twidth = 0;
            int length = realText.Length();
            int i;
            for (i=length-1; i >= 0; --i) {
              PRUnichar ch = realText.CharAt(i);
              aRenderingContext.GetWidth(ch,cwidth);
              if (twidth + cwidth > width)
                  break;

              twidth += cwidth;
            }

            nsAutoString copy;
            realText.Right(copy, length-1-i);
            realText = ellipsis;
            realText += copy;
          }
          break;

          case 1:
          {
            // XXX Not yet implemented.
          }
          break;
        }
      }
    }

    // Set our color.
    const nsStyleColor* colorStyle = (const nsStyleColor*)textContext->GetStyleData(eStyleStruct_Color);
    aRenderingContext.SetColor(colorStyle->mColor);

    aRenderingContext.DrawString(realText, textRect.x, textRect.y);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerBodyFrame::PaintBackgroundLayer(nsIStyleContext* aStyleContext, nsIPresContext* aPresContext, 
                                          nsIRenderingContext& aRenderingContext, 
                                          const nsRect& aRect, const nsRect& aDirtyRect)
{

  const nsStyleBackground* myColor = (const nsStyleBackground*)
      aStyleContext->GetStyleData(eStyleStruct_Background);
  const nsStyleBorder* myBorder = (const nsStyleBorder*)
      aStyleContext->GetStyleData(eStyleStruct_Border);
  const nsStyleOutline* myOutline = (const nsStyleOutline*)
      aStyleContext->GetStyleData(eStyleStruct_Outline);
  
  nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                  aDirtyRect, aRect, *myColor, *myBorder, 0, 0);

  nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                              aDirtyRect, aRect, *myBorder, mStyleContext, 0);

  nsCSSRendering::PaintOutline(aPresContext, aRenderingContext, this,
                              aDirtyRect, aRect, *myBorder, *myOutline, aStyleContext, 0);

  return NS_OK;
}

// Scrolling
NS_IMETHODIMP nsOutlinerBodyFrame::EnsureRowIsVisible(PRInt32 aRow)
{
  if (!mView)
    return NS_OK;

  if (mTopRowIndex <= aRow && mTopRowIndex+mPageCount > aRow)
    return NS_OK;

  if (aRow < mTopRowIndex)
    ScrollToRow(aRow);
  else {
    // Bring it just on-screen.
    PRInt32 distance = aRow - (mTopRowIndex+mPageCount)+1;
    ScrollToRow(mTopRowIndex+distance);
  }

  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::ScrollToRow(PRInt32 aRow)
{
  ScrollInternal(aRow);
  UpdateScrollbar();

#ifdef XP_MAC
  // mac can't process the event loop during a drag, so if we're dragging,
  // grab the scroll widget and make it paint synchronously. This is
  // sorta slow (having to paint the entire tree), but it works.
  if ( mDragSession ) {
    nsCOMPtr<nsIWidget> scrollWidget;
    mScrollbar->GetWindow(mPresContext, getter_AddRefs(scrollWidget));
    if ( scrollWidget )
      scrollWidget->Invalidate(PR_TRUE);
  }
#endif
  
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::ScrollByLines(PRInt32 aNumLines)
{
  if (!mView)
    return NS_OK;

  PRInt32 newIndex = mTopRowIndex + aNumLines;
  if (newIndex < 0)
    newIndex = 0;
  else {
    PRInt32 rowCount;
    mView->GetRowCount(&rowCount);
    PRInt32 lastPageTopRow = rowCount - mPageCount;
    if (newIndex > lastPageTopRow)
      newIndex = lastPageTopRow;
  }
  ScrollToRow(newIndex);
  
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::ScrollByPages(PRInt32 aNumPages)
{
  if (!mView)
    return NS_OK;

  PRInt32 newIndex = mTopRowIndex + aNumPages * mPageCount;
  if (newIndex < 0)
    newIndex = 0;
  else {
    PRInt32 rowCount;
    mView->GetRowCount(&rowCount);
    PRInt32 lastPageTopRow = rowCount - mPageCount;
    if (newIndex > lastPageTopRow)
      newIndex = lastPageTopRow;
  }
  ScrollToRow(newIndex);
    
  return NS_OK;
}

nsresult
nsOutlinerBodyFrame::ScrollInternal(PRInt32 aRow)
{
  if (!mView)
    return NS_OK;

  PRInt32 rowCount;
  mView->GetRowCount(&rowCount);

  PRInt32 delta = aRow - mTopRowIndex;

  if (delta > 0) {
    if (mTopRowIndex == (rowCount - mPageCount + 1))
      return NS_OK;
  }
  else {
    if (mTopRowIndex == 0)
      return NS_OK;
  }

  mTopRowIndex += delta;

  float t2p;
  mPresContext->GetTwipsToPixels(&t2p);
  nscoord rowHeightAsPixels = NSToCoordRound((float)mRowHeight*t2p);

  // See if we have a background image.  If we do, then we cannot blit.
  const nsStyleBackground* myColor = (const nsStyleBackground*)
      mStyleContext->GetStyleData(eStyleStruct_Background);
  PRBool hasBackground = myColor->mBackgroundImage.Length() > 0;

  PRInt32 absDelta = delta > 0 ? delta : -delta;
  if (hasBackground || absDelta*mRowHeight >= mRect.height)
    Invalidate();
  else if (mOutlinerWidget)
    mOutlinerWidget->Scroll(0, -delta*rowHeightAsPixels, nsnull);
 
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerBodyFrame::ScrollbarButtonPressed(PRInt32 aOldIndex, PRInt32 aNewIndex)
{
  if (aNewIndex > aOldIndex)
    ScrollToRow(mTopRowIndex+1);
  else if (aNewIndex < aOldIndex)
    ScrollToRow(mTopRowIndex-1);
  return NS_OK;
}
  
NS_IMETHODIMP
nsOutlinerBodyFrame::PositionChanged(PRInt32 aOldIndex, PRInt32& aNewIndex)
{
  float t2p;
  if (!mRowHeight) return NS_ERROR_UNEXPECTED;
  mPresContext->GetTwipsToPixels(&t2p);
  nscoord rh = NSToCoordRound((float)mRowHeight*t2p);

  nscoord oldrow = aOldIndex/rh;
  nscoord newrow = aNewIndex/rh;

  if (oldrow != newrow)
    ScrollInternal(newrow);

  // Go exactly where we're supposed to
  // Update the scrollbar.
  nsCOMPtr<nsIContent> scrollbarContent;
  mScrollbar->GetContent(getter_AddRefs(scrollbarContent));
  nsAutoString curPos;
  curPos.AppendInt(aNewIndex);
  scrollbarContent->SetAttr(kNameSpaceID_None, nsXULAtoms::curpos, curPos, PR_TRUE);

  return NS_OK;
}

// The style cache.
nsresult 
nsOutlinerBodyFrame::GetPseudoStyleContext(nsIAtom* aPseudoElement, 
                                           nsIStyleContext** aResult)
{
  return mStyleCache.GetStyleContext(this, mPresContext, mContent, mStyleContext, aPseudoElement,
                                     mScratchArray, aResult);
}

// Our comparator for resolving our complex pseudos
NS_IMETHODIMP
nsOutlinerBodyFrame::PseudoMatches(nsIAtom* aTag, nsCSSSelector* aSelector, PRBool* aResult)
{
  if (aSelector->mTag == aTag) {
    // Iterate the pseudoclass list.  For each item in the list, see if
    // it is contained in our scratch array.  If we have a miss, then
    // we aren't a match.  If all items in the pseudoclass list are
    // present in the scratch array, then we have a match.
    nsAtomList* curr = aSelector->mPseudoClassList;
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

void
nsOutlinerBodyFrame::InvalidateColumnCache()
{
  mColumnsDirty = PR_TRUE;
}

void
nsOutlinerBodyFrame::EnsureColumns()
{
  if (!mColumns || mColumnsDirty) {
    delete mColumns;
    mColumnsDirty = PR_FALSE;

    nsCOMPtr<nsIContent> parent;
    mContent->GetParent(*getter_AddRefs(parent));
    nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(parent));

    nsCOMPtr<nsIDOMNodeList> cols;
    elt->GetElementsByTagNameNS(NS_LITERAL_STRING("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"),
                                NS_LITERAL_STRING("outlinercol"),
                                getter_AddRefs(cols));

    nsCOMPtr<nsIPresShell> shell; 
    mPresContext->GetShell(getter_AddRefs(shell));

    PRUint32 count;
    cols->GetLength(&count);

    if (count == 0)
      return; // Nothing to do.

    // Get a column, and get the parent frame.  We need to use its box direction
    // to find out the order in which we should iterate the columns.
    nsCOMPtr<nsIDOMNode> node;
    cols->Item(0, getter_AddRefs(node));
    nsCOMPtr<nsIContent> child(do_QueryInterface(node));
        
    // Get the frame for this column.
    nsIFrame* frame;
    shell->GetPrimaryFrameFor(child, &frame);
    if (!frame)
      return; // This is disastrous.

    nsIFrame* colContainer;
    frame->GetParent(&colContainer);
      
    nsCOMPtr<nsIBox> colContainerBox(do_QueryInterface(colContainer));
    nsIBox* colBox;
    colContainerBox->GetChildBox(&colBox);
    
    nsOutlinerColumn* currCol = nsnull;
    while (colBox) {
      nsIFrame* frame;
      colBox->GetFrame(&frame);
      nsCOMPtr<nsIContent> content;
      frame->GetContent(getter_AddRefs(content));
      nsCOMPtr<nsIDOMElement> colElt(do_QueryInterface(content));
      nsCOMPtr<nsIDOMNode> colParentElt;
      colElt->GetParentNode(getter_AddRefs(colParentElt));
      if (colParentElt == elt) {
        // Create a new column structure.
        nsOutlinerColumn* col = new nsOutlinerColumn(content, frame);
        if (currCol)
          currCol->SetNext(col);
        else mColumns = col;
        currCol = col;
      }
      
      colBox->GetNextBox(&colBox);
    }
  }
}


#ifdef XP_MAC
#pragma mark - 
#endif


//
// OnDragDrop
//
// Tell the view where the drop happened
//
NS_IMETHODIMP
nsOutlinerBodyFrame :: OnDragDrop ( nsIDOMEvent* inEvent )
{
  mView->Drop ( mDropRow, mDropOrient );
  return NS_OK;

} // OnDragDrop


//
// OnDragExit
//
// Clear out all our tracking vars. If we were drawing feedback, undraw it
//
NS_IMETHODIMP
nsOutlinerBodyFrame :: OnDragExit ( nsIDOMEvent* inEvent )
{
  if ( mDropAllowed && !mAlreadyUndrewDueToScroll )
    DrawDropFeedback ( mDropRow, mDropOrient, kUndrawFeedback ) ;
  
  mDropRow = kIllegalRow;
  mDropOrient = kNoOrientation;
  mDropAllowed = PR_FALSE;
  mIsSortRectDrawn = PR_FALSE;
  mAlreadyUndrewDueToScroll = PR_FALSE;
   
  mDragSession = nsnull;
  mRenderingContext = nsnull;
  return NS_OK;

} // OnDragExit


//
// OnDragOver
//
// The mouse is hovering over this outliner. If we determine things are different from the
// last time, undraw feedback at the old position, query the view to see if the current location is 
// droppable, and then draw feedback at the new location if it is. The mouse may or may
// not have changed position from the last time we were called, so optimize out a lot of
// the extra notifications by checking if anything changed first.
//
NS_IMETHODIMP
nsOutlinerBodyFrame :: OnDragOver ( nsIDOMEvent* inEvent )
{
  // while we're here, handle tracking of scrolling during a drag. There is a little craziness
  // here as we turn off tracking of feedback during the scroll. When we first start scrolling,
  // we explicitly undraw the previous feedback, then set |mAlreadyUndrewDueToScroll| to
  // alert other places not to undraw again later (we're using XOR, so undrawing twice
  // is bad). Below, we'll clear this member the next time we try to undraw the regular feedback.
  PRBool scrollUp = PR_FALSE;
  if ( IsInDragScrollRegion(inEvent, &scrollUp) ) {
    if ( mDropAllowed && !mAlreadyUndrewDueToScroll )
      DrawDropFeedback ( mDropRow, mDropOrient, kUndrawFeedback );    // undraw it at old loc, if we were drawing
    mAlreadyUndrewDueToScroll = PR_TRUE;
    ScrollByLines ( scrollUp ? -1 : 1);
    return NS_OK;
  }

  // compute the row mouse is over and the above/below/on state. Below we'll use this
  // to see if anything changed.
  PRInt32 newRow = kIllegalRow;
  DropOrientation newOrient = kNoOrientation;
  ComputeDropPosition ( inEvent, &newRow, &newOrient );
  
  // if changed from last time, undraw it at the old location and if allowed, 
  // draw it at the new location. If nothing changed, just bail.
  if ( newRow != mDropRow || newOrient != mDropOrient ) {

    // undraw feedback at old loc. If we are coming off a scroll, 
    // don't undraw the old (we already did that), but reset us so that
    // we're back in the normal case.
    if ( !mAlreadyUndrewDueToScroll ) {
      if ( mDropAllowed )
        DrawDropFeedback ( mDropRow, mDropOrient, kUndrawFeedback );
    }
    else
      mAlreadyUndrewDueToScroll = PR_FALSE;

    // cache the new row and orientation regardless so we can check if it changed
    // for next time.
    mDropRow = newRow;
    mDropOrient = newOrient;

    PRBool canDropAtNewLocation = PR_FALSE;
    if ( newOrient == kOnRow )
      mView->CanDropOn ( newRow, &canDropAtNewLocation );
    else
      mView->CanDropBeforeAfter ( newRow, newOrient == kBeforeRow ? PR_TRUE : PR_FALSE, &canDropAtNewLocation );
      
    if ( canDropAtNewLocation )
      DrawDropFeedback ( newRow, newOrient, kDrawFeedback );         // draw it at old loc, if we are allowed

    mDropAllowed = canDropAtNewLocation;
  }
  
  // alert the drag session we accept the drop. We have to do this every time
  // since the |canDrop| attribute is reset before we're called.
  if ( mDropAllowed && mDragSession )
    mDragSession->SetCanDrop(PR_TRUE);

  return NS_OK;

} // OnDragOver


//
// DrawDropFeedback
//
// Takes care of actually drawing the correct feedback. |inDrawFeedback| tells us whether
// we're drawing or undrawing (removing/clearing) the feedback for the given row.
//
// XXX Need to be able to make line color respect style
//
void
nsOutlinerBodyFrame :: DrawDropFeedback ( PRInt32 inDropRow, DropOrientation inDropOrient, PRBool inDrawFeedback ) 
{
  // call appropriate routine (insert, container, etc) based on |inDropOrient| and pass in |inDrawFeedback|
  float pixelsToTwips = 0.0;
  mPresContext->GetPixelsToTwips ( &pixelsToTwips );

#if NOT_YET_WORKING
  // feedback will differ depending on if we're sorted or not -- leaving this around
  // for later.
  if ( viewSorted ) {    
    PRInt32 penSize = NSToIntRound(1*pixelsToTwips);      // use a 1 pixel wide pen
    
    // draw outline rectangle made of 4 rects (gfx can't just frame a rectangle). The
    // rects can't overlap because we're XORing.
    mRenderingContext->InvertRect ( 0, 0, mRect.width, penSize );           // top
    mRenderingContext->InvertRect ( 0, penSize, penSize, mRect.height );    // left
    mRenderingContext->InvertRect ( mRect.width - penSize, penSize, penSize, mRect.height );    // right
    mRenderingContext->InvertRect ( penSize, mRect.height - penSize, 
                                      mRect.width - 2*penSize, penSize );    // bottom
  }
#endif
  
  nsOutlinerColumn* primaryCol = nsnull;
  for (nsOutlinerColumn* currCol = mColumns; currCol; currCol = currCol->GetNext()) {
    if ( currCol->IsPrimary() ) {
      primaryCol = currCol;
      break;
    }
  }
  if ( !primaryCol )
    return;
  
  if ( inDropOrient == kOnRow ) {
    // drawing "on" a row. Invert the image and text in the primary column.
    PRInt32 x, y, width, height;
    GetCoordsForCellItem ( inDropRow, primaryCol->GetID(), NS_LITERAL_STRING("image").get(), 
                            &x, &y, &width, &height );     
    mRenderingContext->InvertRect ( NSToIntRound(x*pixelsToTwips), NSToIntRound(y*pixelsToTwips),
                                      NSToIntRound(width*pixelsToTwips), NSToIntRound(height*pixelsToTwips) );
    GetCoordsForCellItem ( inDropRow, primaryCol->GetID(), NS_LITERAL_STRING("text").get(), 
                            &x, &y, &width, &height );     
    mRenderingContext->InvertRect ( NSToIntRound(x*pixelsToTwips), NSToIntRound(y*pixelsToTwips),
                                      NSToIntRound(width*pixelsToTwips), NSToIntRound(height*pixelsToTwips) );
  }
  else {
    // drawing between rows, find the X/Y to draw a 2 pixel line indented 5 pixels
    // from the left of the image in the primary column.
    PRInt32 whereToDrawY = mRowHeight * (inDropRow - mTopRowIndex);
    if ( inDropOrient == kAfterRow )
      whereToDrawY += mRowHeight;

    PRInt32 whereToDrawX = 0;    
    PRInt32 y, width, height;
    GetCoordsForCellItem ( inDropRow, primaryCol->GetID(), NS_LITERAL_STRING("image").get(), 
                            &whereToDrawX, &y, &width, &height );     
    whereToDrawX += 5;                                // indent 5 pixels from left of image
    
    mRenderingContext->InvertRect ( NSToIntRound(whereToDrawX*pixelsToTwips),
                                      whereToDrawY, NSToIntRound(25*pixelsToTwips), NSToIntRound(2*pixelsToTwips) );
  }
   
} // DrawDropFeedback


//
// ComputeDropPosition
//
// Given a dom event, figure out which row in the tree the mouse is over
// and if we should drop before/after/on that row. Doesn't query the content
// about if the drag is allowable, that's done elsewhere.
//
// For containers, we break up the vertical space of the row as follows: if in
// the topmost 25%, the drop is _before_ the row the mouse is over; if in the
// last 25%, _after_; in the middle 50%, we consider it a drop _on_ the container.
//
// For non-containers, if the mouse is in the top 50% of the row, the drop is
// _before_ and the bottom 50% _after_
//
void 
nsOutlinerBodyFrame :: ComputeDropPosition ( nsIDOMEvent* inEvent, PRInt32* outRow, DropOrientation* outOrient )
{
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent ( do_QueryInterface(inEvent) );
  if ( mouseEvent ) {
    PRInt32 x = 0, y = 0;
    mouseEvent->GetClientX(&x); mouseEvent->GetClientY(&y);
    
    PRInt32 row = kIllegalRow;
    nsXPIDLString colID, child;
    GetCellAt ( x, y, &row, getter_Copies(colID), getter_Copies(child) );
    
    // GetCellAt() will just blindly report a row even if there is no content there
    // (ie, dragging below the end of the tree). If that's the case, set the reported row
    // to after the final row. If we're not off the end, then check the coords w/in the cell
    // to see if we are dropping before/on/after.
    PRInt32 totalNumRows = 0;
    mView->GetRowCount ( &totalNumRows );
    if ( row > totalNumRows - 1 ) {             // doh, we're off the end of the tree
      row = (totalNumRows-1) - mTopRowIndex;
      *outOrient = kAfterRow;
    }
    else {                                              // w/in a cell, check above/below/on
      // Compute the top/bottom of the row in question. We need to convert
      // our y coord to twips since |mRowHeight| is in twips.
      PRInt32 yTwips, xTwips;
      AdjustEventCoordsToBoxCoordSpace ( x, y, &xTwips, &yTwips );
      PRInt32 rowTop = mRowHeight * (row - mTopRowIndex);
      PRInt32 rowBottom = rowTop + mRowHeight;
      PRInt32 yOffset = yTwips - rowTop;
   
      PRBool isContainer = PR_FALSE;
      mView->IsContainer ( row, &isContainer );
      if ( isContainer ) {
        // for a container, use a 25%/50%/25% breakdown
        if ( yOffset < mRowHeight / 4 )
          *outOrient = kBeforeRow;
        else if ( yOffset > mRowHeight - (mRowHeight / 4) )
          *outOrient = kAfterRow;
        else
          *outOrient = kOnRow;
      }
      else {
        // for a non-container use a 50%/50% breakdown
        if ( yOffset < mRowHeight / 2 )
          *outOrient = kBeforeRow;
        else
          *outOrient = kAfterRow;        
      }
    }
    
    *outRow = row;
  }
  
} // ComputeDropPosition


//
// IsInDragScrollRegion
//
// Determine if we're w/in a margin of the top/bottom of the outliner during a drag.
// This will ultimately cause us to scroll, but that's done elsewhere.
//
PRBool
nsOutlinerBodyFrame :: IsInDragScrollRegion ( nsIDOMEvent* inEvent, PRBool* outScrollUp )
{
  PRBool isInRegion = PR_FALSE;

  float pixelsToTwips = 0.0;
  mPresContext->GetPixelsToTwips ( &pixelsToTwips );
  const int kMarginHeight = NSToIntRound ( 12 * pixelsToTwips );
  
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent ( do_QueryInterface(inEvent) );
  if ( mouseEvent ) {
    PRInt32 x = 0, y = 0;
    mouseEvent->GetClientX(&x); mouseEvent->GetClientY(&y);
  
    PRInt32 yTwips, xTwips;
    AdjustEventCoordsToBoxCoordSpace ( x, y, &xTwips, &yTwips );
    
    if ( yTwips < kMarginHeight ) {
      isInRegion = PR_TRUE;
      if ( outScrollUp )
        *outScrollUp = PR_TRUE;       // scroll up
    }
    else if ( yTwips > mRect.height - kMarginHeight ) {
      isInRegion = PR_TRUE;
      if ( outScrollUp )
        *outScrollUp = PR_FALSE;     // scroll down
    }    
  }

  return isInRegion;
  
} // IsInDragScrollRegion


//
// OnDragEnter
//
// Cache several things we'll need throughout the course of our work. These
// will all get released on a drag exit
//
NS_IMETHODIMP
nsOutlinerBodyFrame :: OnDragEnter ( nsIDOMEvent* inEvent )
{
  // create a rendering context for our drawing needs
  nsCOMPtr<nsIPresShell> presShell;
  mPresContext->GetShell(getter_AddRefs(presShell));
  nsCOMPtr<nsIRenderingContext> rendContext;
  presShell->CreateRenderingContext ( this, getter_AddRefs(mRenderingContext) );

  // cache the drag session
  nsresult rv;
  nsCOMPtr<nsIDragService> dragService = 
           do_GetService("@mozilla.org/widget/dragservice;1", &rv);
  nsCOMPtr<nsIDragSession> dragSession;
  dragService->GetCurrentSession(getter_AddRefs(mDragSession));
  NS_ASSERTION ( mDragSession, "can't get drag session" );

  return NS_OK;

} // OnDragEnter



#ifdef XP_MAC
#pragma mark - 
#endif


// ==============================================================================
// The ImageListener implementation
// ==============================================================================

#ifdef USE_IMG2
NS_IMPL_ISUPPORTS3(nsOutlinerImageListener, imgIDecoderObserver, imgIContainerObserver, nsIOutlinerImageListener)

nsOutlinerImageListener::nsOutlinerImageListener(nsIOutlinerBoxObject* aOutliner, const PRUnichar* aID)
{
  NS_INIT_ISUPPORTS();
  mOutliner = aOutliner;
  mColID = aID;
  mMin = mMax = 0;
}

nsOutlinerImageListener::~nsOutlinerImageListener()
{
}

NS_IMETHODIMP nsOutlinerImageListener::OnStartDecode(imgIRequest *aRequest, nsISupports *aContext)
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerImageListener::OnStartContainer(imgIRequest *aRequest, nsISupports *aContext, imgIContainer *aImage)
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerImageListener::OnStartFrame(imgIRequest *aRequest, nsISupports *aContext, gfxIImageFrame *aFrame)
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerImageListener::OnDataAvailable(imgIRequest *aRequest, nsISupports *aContext, gfxIImageFrame *aFrame, const nsRect *aRect)
{
  Invalidate();
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerImageListener::OnStopFrame(imgIRequest *aRequest, nsISupports *aContext, gfxIImageFrame *aFrame)
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerImageListener::OnStopContainer(imgIRequest *aRequest, nsISupports *aContext, imgIContainer *aImage)
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerImageListener::OnStopDecode(imgIRequest *aRequest, nsISupports *aContext, nsresult status, const PRUnichar *statusArg)
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerImageListener::FrameChanged(imgIContainer *aContainer, nsISupports *aContext, gfxIImageFrame *newframe, nsRect * dirtyRect)
{
  Invalidate();
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerImageListener::AddRow(int aIndex)
{
  if (aIndex < mMin)
    mMin = aIndex;
  else if (aIndex > mMax)
    mMax = aIndex;

  return NS_OK;
}

NS_IMETHODIMP 
nsOutlinerImageListener::Invalidate()
{
  // Loop from min to max, invalidating each cell that was listening for this image.
  for (PRInt32 i = mMin; i <= mMax; i++) {
    mOutliner->InvalidateCell(i, mColID.get());
  }

  return NS_OK;
}
#endif

