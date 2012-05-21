/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsListBoxBodyFrame.h"

#include "nsListBoxLayout.h"

#include "nsCOMPtr.h"
#include "nsGridRowGroupLayout.h"
#include "nsIServiceManager.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "nsIDocument.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsCSSFrameConstructor.h"
#include "nsIScrollableFrame.h"
#include "nsScrollbarFrame.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsStyleContext.h"
#include "nsFontMetrics.h"
#include "nsITimer.h"
#include "nsAutoPtr.h"
#include "nsStyleSet.h"
#include "nsPIBoxObject.h"
#include "nsINodeInfo.h"
#include "nsLayoutUtils.h"
#include "nsPIListBoxObject.h"
#include "nsContentUtils.h"
#include "nsChildIterator.h"
#include "nsRenderingContext.h"

#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"
#endif

/////////////// nsListScrollSmoother //////////////////

/* A mediator used to smooth out scrolling. It works by seeing if 
 * we have time to scroll the amount of rows requested. This is determined
 * by measuring how long it takes to scroll a row. If we can scroll the 
 * rows in time we do so. If not we start a timer and skip the request. We
 * do this until the timer finally first because the user has stopped moving
 * the mouse. Then do all the queued requests in on shot.
 */

// the longest amount of time that can go by before the use
// notices it as a delay.
#define USER_TIME_THRESHOLD 150000

// how long it takes to layout a single row initial value.
// we will time this after we scroll a few rows.
#define TIME_PER_ROW_INITAL  50000

// if we decide we can't layout the rows in the amount of time. How long
// do we wait before checking again?
#define SMOOTH_INTERVAL 100

class nsListScrollSmoother : public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS

  nsListScrollSmoother(nsListBoxBodyFrame* aOuter);
  virtual ~nsListScrollSmoother();

  // nsITimerCallback
  NS_DECL_NSITIMERCALLBACK

  void Start();
  void Stop();
  bool IsRunning();

  nsCOMPtr<nsITimer> mRepeatTimer;
  PRInt32 mDelta;
  nsListBoxBodyFrame* mOuter;
}; 

nsListScrollSmoother::nsListScrollSmoother(nsListBoxBodyFrame* aOuter)
{
  mDelta = 0;
  mOuter = aOuter;
}

nsListScrollSmoother::~nsListScrollSmoother()
{
  Stop();
}

NS_IMETHODIMP
nsListScrollSmoother::Notify(nsITimer *timer)
{
  Stop();

  NS_ASSERTION(mOuter, "mOuter is null, see bug #68365");
  if (!mOuter) return NS_OK;

  // actually do some work.
  mOuter->InternalPositionChangedCallback();
  return NS_OK;
}

bool
nsListScrollSmoother::IsRunning()
{
  return mRepeatTimer ? true : false;
}

void
nsListScrollSmoother::Start()
{
  Stop();
  mRepeatTimer = do_CreateInstance("@mozilla.org/timer;1");
  mRepeatTimer->InitWithCallback(this, SMOOTH_INTERVAL, nsITimer::TYPE_ONE_SHOT);
}

void
nsListScrollSmoother::Stop()
{
  if ( mRepeatTimer ) {
    mRepeatTimer->Cancel();
    mRepeatTimer = nsnull;
  }
}

NS_IMPL_ISUPPORTS1(nsListScrollSmoother, nsITimerCallback)

/////////////// nsListBoxBodyFrame //////////////////

nsListBoxBodyFrame::nsListBoxBodyFrame(nsIPresShell* aPresShell,
                                       nsStyleContext* aContext,
                                       nsBoxLayout* aLayoutManager)
  : nsBoxFrame(aPresShell, aContext, false, aLayoutManager),
    mTopFrame(nsnull),
    mBottomFrame(nsnull),
    mLinkupFrame(nsnull),
    mScrollSmoother(nsnull),
    mRowsToPrepend(0),
    mRowCount(-1),
    mRowHeight(0),
    mAvailableHeight(0),
    mStringWidth(-1),
    mCurrentIndex(0),
    mOldIndex(0),
    mYPosition(0),
    mTimePerRow(TIME_PER_ROW_INITAL),
    mRowHeightWasSet(false),
    mScrolling(false),
    mAdjustScroll(false),
    mReflowCallbackPosted(false)
{
}

nsListBoxBodyFrame::~nsListBoxBodyFrame()
{
  NS_IF_RELEASE(mScrollSmoother);

#if USE_TIMER_TO_DELAY_SCROLLING
  StopScrollTracking();
  mAutoScrollTimer = nsnull;
#endif

}

NS_QUERYFRAME_HEAD(nsListBoxBodyFrame)
  NS_QUERYFRAME_ENTRY(nsIScrollbarMediator)
  NS_QUERYFRAME_ENTRY(nsListBoxBodyFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBoxFrame)

////////// nsIFrame /////////////////

NS_IMETHODIMP
nsListBoxBodyFrame::Init(nsIContent*     aContent,
                         nsIFrame*       aParent, 
                         nsIFrame*       aPrevInFlow)
{
  nsresult rv = nsBoxFrame::Init(aContent, aParent, aPrevInFlow);
  NS_ENSURE_SUCCESS(rv, rv);
  nsIScrollableFrame* scrollFrame = nsLayoutUtils::GetScrollableFrameFor(this);
  if (scrollFrame) {
    nsIBox* verticalScrollbar = scrollFrame->GetScrollbarBox(true);
    nsScrollbarFrame* scrollbarFrame = do_QueryFrame(verticalScrollbar);
    if (scrollbarFrame) {
      scrollbarFrame->SetScrollbarMediatorContent(GetContent());
    }
  }
  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fm));
  mRowHeight = fm->MaxHeight();

  return rv;
}

void
nsListBoxBodyFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  // make sure we cancel any posted callbacks.
  if (mReflowCallbackPosted)
     PresContext()->PresShell()->CancelReflowCallback(this);

  // Revoke any pending position changed events
  for (PRUint32 i = 0; i < mPendingPositionChangeEvents.Length(); ++i) {
    mPendingPositionChangeEvents[i]->Revoke();
  }

  // Make sure we tell our listbox's box object we're being destroyed.
  if (mBoxObject) {
    mBoxObject->ClearCachedValues();
  }

  nsBoxFrame::DestroyFrom(aDestructRoot);
}

NS_IMETHODIMP
nsListBoxBodyFrame::AttributeChanged(PRInt32 aNameSpaceID,
                                     nsIAtom* aAttribute, 
                                     PRInt32 aModType)
{
  nsresult rv = NS_OK;

  if (aAttribute == nsGkAtoms::rows) {
    PresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eStyleChange, NS_FRAME_IS_DIRTY);
  }
  else
    rv = nsBoxFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);

  return rv;
 
}

/////////// nsIBox ///////////////

/* virtual */ void
nsListBoxBodyFrame::MarkIntrinsicWidthsDirty()
{
  mStringWidth = -1;
  nsBoxFrame::MarkIntrinsicWidthsDirty();
}

/////////// nsBox ///////////////

NS_IMETHODIMP
nsListBoxBodyFrame::DoLayout(nsBoxLayoutState& aBoxLayoutState)
{
  if (mScrolling)
    aBoxLayoutState.SetPaintingDisabled(true);

  nsresult rv = nsBoxFrame::DoLayout(aBoxLayoutState);

  // determine the real height for the scrollable area from the total number
  // of rows, since non-visible rows don't yet have frames
  nsRect rect(nsPoint(0, 0), GetSize());
  nsOverflowAreas overflow(rect, rect);
  if (mLayoutManager) {
    nsIFrame* childFrame = mFrames.FirstChild();
    while (childFrame) {
      ConsiderChildOverflow(overflow, childFrame);
      childFrame = childFrame->GetNextSibling();
    }

    nsSize prefSize = mLayoutManager->GetPrefSize(this, aBoxLayoutState);
    NS_FOR_FRAME_OVERFLOW_TYPES(otype) {
      nsRect& o = overflow.Overflow(otype);
      o.height = NS_MAX(o.height, prefSize.height);
    }
  }
  FinishAndStoreOverflow(overflow, GetSize());

  if (mScrolling)
    aBoxLayoutState.SetPaintingDisabled(false);

  // if we are scrolled and the row height changed
  // make sure we are scrolled to a correct index.
  if (mAdjustScroll)
     PostReflowCallback();

  return rv;
}

nsSize
nsListBoxBodyFrame::GetMinSizeForScrollArea(nsBoxLayoutState& aBoxLayoutState)
{
  nsSize result(0, 0);
  if (nsContentUtils::HasNonEmptyAttr(GetContent(), kNameSpaceID_None,
                                      nsGkAtoms::sizemode)) {
    result = GetPrefSize(aBoxLayoutState);
    result.height = 0;
    nsIScrollableFrame* scrollFrame = nsLayoutUtils::GetScrollableFrameFor(this);
    if (scrollFrame &&
        scrollFrame->GetScrollbarStyles().mVertical == NS_STYLE_OVERFLOW_AUTO) {
      nsMargin scrollbars =
        scrollFrame->GetDesiredScrollbarSizes(&aBoxLayoutState);
      result.width += scrollbars.left + scrollbars.right;
    }
  }
  return result;
}

nsSize
nsListBoxBodyFrame::GetPrefSize(nsBoxLayoutState& aBoxLayoutState)
{  
  nsSize pref = nsBoxFrame::GetPrefSize(aBoxLayoutState);

  PRInt32 size = GetFixedRowSize();
  if (size > -1)
    pref.height = size*GetRowHeightAppUnits();

  nsIScrollableFrame* scrollFrame = nsLayoutUtils::GetScrollableFrameFor(this);
  if (scrollFrame &&
      scrollFrame->GetScrollbarStyles().mVertical == NS_STYLE_OVERFLOW_AUTO) {
    nsMargin scrollbars = scrollFrame->GetDesiredScrollbarSizes(&aBoxLayoutState);
    pref.width += scrollbars.left + scrollbars.right;
  }
  return pref;
}

///////////// nsIScrollbarMediator ///////////////

NS_IMETHODIMP
nsListBoxBodyFrame::PositionChanged(nsScrollbarFrame* aScrollbar, PRInt32 aOldIndex, PRInt32& aNewIndex)
{ 
  if (mScrolling || mRowHeight == 0)
    return NS_OK;

  nscoord oldTwipIndex, newTwipIndex;
  oldTwipIndex = mCurrentIndex*mRowHeight;
  newTwipIndex = nsPresContext::CSSPixelsToAppUnits(aNewIndex);
  PRInt32 twipDelta = newTwipIndex > oldTwipIndex ? newTwipIndex - oldTwipIndex : oldTwipIndex - newTwipIndex;

  PRInt32 rowDelta = twipDelta / mRowHeight;
  PRInt32 remainder = twipDelta % mRowHeight;
  if (remainder > (mRowHeight/2))
    rowDelta++;

  if (rowDelta == 0)
    return NS_OK;

  // update the position to be row based.

  PRInt32 newIndex = newTwipIndex > oldTwipIndex ? mCurrentIndex + rowDelta : mCurrentIndex - rowDelta;
  //aNewIndex = newIndex*mRowHeight/mOnePixel;

  nsListScrollSmoother* smoother = GetSmoother();

  // if we can't scroll the rows in time then start a timer. We will eat
  // events until the user stops moving and the timer stops.
  if (smoother->IsRunning() || rowDelta*mTimePerRow > USER_TIME_THRESHOLD) {

     smoother->Stop();

     smoother->mDelta = newTwipIndex > oldTwipIndex ? rowDelta : -rowDelta;

     smoother->Start();

     return NS_OK;
  }

  smoother->Stop();

  mCurrentIndex = newIndex;
  smoother->mDelta = 0;
  
  if (mCurrentIndex < 0) {
    mCurrentIndex = 0;
    return NS_OK;
  }

  return InternalPositionChanged(newTwipIndex < oldTwipIndex, rowDelta);
}

NS_IMETHODIMP
nsListBoxBodyFrame::VisibilityChanged(bool aVisible)
{
  if (mRowHeight == 0)
    return NS_OK;

  PRInt32 lastPageTopRow = GetRowCount() - (GetAvailableHeight() / mRowHeight);
  if (lastPageTopRow < 0)
    lastPageTopRow = 0;
  PRInt32 delta = mCurrentIndex - lastPageTopRow;
  if (delta > 0) {
    mCurrentIndex = lastPageTopRow;
    InternalPositionChanged(true, delta);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsListBoxBodyFrame::ScrollbarButtonPressed(nsScrollbarFrame* aScrollbar, PRInt32 aOldIndex, PRInt32 aNewIndex)
{
  if (aOldIndex == aNewIndex)
    return NS_OK;
  if (aNewIndex < aOldIndex)
    mCurrentIndex--;
  else mCurrentIndex++;
  if (mCurrentIndex < 0) {
    mCurrentIndex = 0;
    return NS_OK;
  }
  InternalPositionChanged(aNewIndex < aOldIndex, 1);

  return NS_OK;
}

///////////// nsIReflowCallback ///////////////

bool
nsListBoxBodyFrame::ReflowFinished()
{
  nsAutoScriptBlocker scriptBlocker;
  // now create or destroy any rows as needed
  CreateRows();

  // keep scrollbar in sync
  if (mAdjustScroll) {
     VerticalScroll(mYPosition);
     mAdjustScroll = false;
  }

  // if the row height changed then mark everything as a style change. 
  // That will dirty the entire listbox
  if (mRowHeightWasSet) {
    PresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eStyleChange, NS_FRAME_IS_DIRTY);
     PRInt32 pos = mCurrentIndex * mRowHeight;
     if (mYPosition != pos) 
       mAdjustScroll = true;
    mRowHeightWasSet = false;
  }

  mReflowCallbackPosted = false;
  return true;
}

void
nsListBoxBodyFrame::ReflowCallbackCanceled()
{
  mReflowCallbackPosted = false;
}

///////// nsIListBoxObject ///////////////

nsresult
nsListBoxBodyFrame::GetRowCount(PRInt32* aResult)
{
  *aResult = GetRowCount();
  return NS_OK;
}

nsresult
nsListBoxBodyFrame::GetNumberOfVisibleRows(PRInt32 *aResult)
{
  *aResult= mRowHeight ? GetAvailableHeight() / mRowHeight : 0;
  return NS_OK;
}

nsresult
nsListBoxBodyFrame::GetIndexOfFirstVisibleRow(PRInt32 *aResult)
{
  *aResult = mCurrentIndex;
  return NS_OK;
}

nsresult
nsListBoxBodyFrame::EnsureIndexIsVisible(PRInt32 aRowIndex)
{
  if (aRowIndex < 0)
    return NS_ERROR_ILLEGAL_VALUE;

  PRInt32 rows = 0;
  if (mRowHeight)
    rows = GetAvailableHeight()/mRowHeight;
  if (rows <= 0)
    rows = 1;
  PRInt32 bottomIndex = mCurrentIndex + rows;
  
  // if row is visible, ignore
  if (mCurrentIndex <= aRowIndex && aRowIndex < bottomIndex)
    return NS_OK;

  PRInt32 delta;

  bool up = aRowIndex < mCurrentIndex;
  if (up) {
    delta = mCurrentIndex - aRowIndex;
    mCurrentIndex = aRowIndex;
  }
  else {
    // Check to be sure we're not scrolling off the bottom of the tree
    if (aRowIndex >= GetRowCount())
      return NS_ERROR_ILLEGAL_VALUE;

    // Bring it just into view.
    delta = 1 + (aRowIndex-bottomIndex);
    mCurrentIndex += delta; 
  }

  // Safe to not go off an event here, since this is coming from the
  // box object.
  DoInternalPositionChangedSync(up, delta);
  return NS_OK;
}

nsresult
nsListBoxBodyFrame::ScrollByLines(PRInt32 aNumLines)
{
  PRInt32 scrollIndex, visibleRows;
  GetIndexOfFirstVisibleRow(&scrollIndex);
  GetNumberOfVisibleRows(&visibleRows);

  scrollIndex += aNumLines;
  
  if (scrollIndex < 0)
    scrollIndex = 0;
  else {
    PRInt32 numRows = GetRowCount();
    PRInt32 lastPageTopRow = numRows - visibleRows;
    if (scrollIndex > lastPageTopRow)
      scrollIndex = lastPageTopRow;
  }
  
  ScrollToIndex(scrollIndex);

  return NS_OK;
}

// walks the DOM to get the zero-based row index of the content
nsresult
nsListBoxBodyFrame::GetIndexOfItem(nsIDOMElement* aItem, PRInt32* _retval)
{
  if (aItem) {
    *_retval = 0;
    nsCOMPtr<nsIContent> itemContent(do_QueryInterface(aItem));

    ChildIterator iter, last;
    for (ChildIterator::Init(mContent, &iter, &last);
         iter != last;
         ++iter) {
      nsIContent *child = (*iter);
      // we hit a list row, count it
      if (child->Tag() == nsGkAtoms::listitem) {
        // is this it?
        if (child == itemContent)
          return NS_OK;

        ++(*_retval);
      }
    }
  }

  // not found
  *_retval = -1;
  return NS_OK;
}

nsresult
nsListBoxBodyFrame::GetItemAtIndex(PRInt32 aIndex, nsIDOMElement** aItem)
{
  *aItem = nsnull;
  if (aIndex < 0)
    return NS_OK;
  
  PRInt32 itemCount = 0;
  ChildIterator iter, last;
  for (ChildIterator::Init(mContent, &iter, &last);
       iter != last;
       ++iter) {
    nsIContent *child = (*iter);
    // we hit a list row, check if it is the one we are looking for
    if (child->Tag() == nsGkAtoms::listitem) {
      // is this it?
      if (itemCount == aIndex) {
        return CallQueryInterface(child, aItem);
      }
      ++itemCount;
    }
  }

  // not found
  return NS_OK;
}

/////////// nsListBoxBodyFrame ///////////////

PRInt32
nsListBoxBodyFrame::GetRowCount()
{
  if (mRowCount < 0)
    ComputeTotalRowCount();
  return mRowCount;
}

PRInt32
nsListBoxBodyFrame::GetFixedRowSize()
{
  PRInt32 dummy;

  nsAutoString rows;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::rows, rows);
  if (!rows.IsEmpty())
    return rows.ToInteger(&dummy);
 
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::size, rows);

  if (!rows.IsEmpty())
    return rows.ToInteger(&dummy);

  return -1;
}

void
nsListBoxBodyFrame::SetRowHeight(nscoord aRowHeight)
{ 
  if (aRowHeight > mRowHeight) { 
    mRowHeight = aRowHeight;

    // signal we need to dirty everything 
    // and we want to be notified after reflow
    // so we can create or destory rows as needed
    mRowHeightWasSet = true;
    PostReflowCallback();
  }
}

nscoord
nsListBoxBodyFrame::GetAvailableHeight()
{
  nsIScrollableFrame* scrollFrame =
    nsLayoutUtils::GetScrollableFrameFor(this);
  if (scrollFrame) {
    return scrollFrame->GetScrollPortRect().height;
  }
  return 0;
}

nscoord
nsListBoxBodyFrame::GetYPosition()
{
  return mYPosition;
}

nscoord
nsListBoxBodyFrame::ComputeIntrinsicWidth(nsBoxLayoutState& aBoxLayoutState)
{
  if (mStringWidth != -1)
    return mStringWidth;

  nscoord largestWidth = 0;

  PRInt32 index = 0;
  nsCOMPtr<nsIDOMElement> firstRowEl;
  GetItemAtIndex(index, getter_AddRefs(firstRowEl));
  nsCOMPtr<nsIContent> firstRowContent(do_QueryInterface(firstRowEl));

  if (firstRowContent) {
    nsRefPtr<nsStyleContext> styleContext;
    nsPresContext *presContext = aBoxLayoutState.PresContext();
    styleContext = presContext->StyleSet()->
      ResolveStyleFor(firstRowContent->AsElement(), nsnull);

    nscoord width = 0;
    nsMargin margin(0,0,0,0);

    if (styleContext->GetStylePadding()->GetPadding(margin))
      width += margin.LeftRight();
    width += styleContext->GetStyleBorder()->GetActualBorder().LeftRight();
    if (styleContext->GetStyleMargin()->GetMargin(margin))
      width += margin.LeftRight();


    ChildIterator iter, last;
    PRUint32 i = 0;
    for (ChildIterator::Init(mContent, &iter, &last);
         iter != last && i < 100;
         ++iter, ++i) {
      nsIContent *child = (*iter);

      if (child->Tag() == nsGkAtoms::listitem) {
        nsRenderingContext* rendContext = aBoxLayoutState.GetRenderingContext();
        if (rendContext) {
          nsAutoString value;
          PRUint32 textCount = child->GetChildCount();
          for (PRUint32 j = 0; j < textCount; ++j) {
            nsIContent* text = child->GetChildAt(j);
            if (text && text->IsNodeOfType(nsINode::eTEXT)) {
              text->AppendTextTo(value);
            }
          }

          nsRefPtr<nsFontMetrics> fm;
          nsLayoutUtils::GetFontMetricsForStyleContext(styleContext,
                                                       getter_AddRefs(fm));
          rendContext->SetFont(fm);

          nscoord textWidth =
            nsLayoutUtils::GetStringWidth(this, rendContext, value.get(), value.Length());
          textWidth += width;

          if (textWidth > largestWidth) 
            largestWidth = textWidth;
        }
      }
    }
  }

  mStringWidth = largestWidth;
  return mStringWidth;
}

void
nsListBoxBodyFrame::ComputeTotalRowCount()
{
  mRowCount = 0;

  ChildIterator iter, last;
  for (ChildIterator::Init(mContent, &iter, &last);
       iter != last;
       ++iter) {
    if ((*iter)->Tag() == nsGkAtoms::listitem)
      ++mRowCount;
  }
}

void
nsListBoxBodyFrame::PostReflowCallback()
{
  if (!mReflowCallbackPosted) {
    mReflowCallbackPosted = true;
    PresContext()->PresShell()->PostReflowCallback(this);
  }
}

////////// scrolling

nsresult
nsListBoxBodyFrame::ScrollToIndex(PRInt32 aRowIndex)
{
  if (( aRowIndex < 0 ) || (mRowHeight == 0))
    return NS_OK;
    
  PRInt32 newIndex = aRowIndex;
  PRInt32 delta = mCurrentIndex > newIndex ? mCurrentIndex - newIndex : newIndex - mCurrentIndex;
  bool up = newIndex < mCurrentIndex;

  // Check to be sure we're not scrolling off the bottom of the tree
  PRInt32 lastPageTopRow = GetRowCount() - (GetAvailableHeight() / mRowHeight);
  if (lastPageTopRow < 0)
    lastPageTopRow = 0;

  if (aRowIndex > lastPageTopRow)
    return NS_OK;

  mCurrentIndex = newIndex;

  nsWeakFrame weak(this);

  // Since we're going to flush anyway, we need to not do this off an event
  DoInternalPositionChangedSync(up, delta);

  if (!weak.IsAlive()) {
    return NS_OK;
  }

  // This change has to happen immediately.
  // Flush any pending reflow commands.
  // XXXbz why, exactly?
  mContent->GetDocument()->FlushPendingNotifications(Flush_Layout);

  return NS_OK;
}

nsresult
nsListBoxBodyFrame::InternalPositionChangedCallback()
{
  nsListScrollSmoother* smoother = GetSmoother();

  if (smoother->mDelta == 0)
    return NS_OK;

  mCurrentIndex += smoother->mDelta;

  if (mCurrentIndex < 0)
    mCurrentIndex = 0;

  return DoInternalPositionChangedSync(smoother->mDelta < 0,
                                       smoother->mDelta < 0 ?
                                         -smoother->mDelta : smoother->mDelta);
}

nsresult
nsListBoxBodyFrame::InternalPositionChanged(bool aUp, PRInt32 aDelta)
{
  nsRefPtr<nsPositionChangedEvent> ev =
    new nsPositionChangedEvent(this, aUp, aDelta);
  nsresult rv = NS_DispatchToCurrentThread(ev);
  if (NS_SUCCEEDED(rv)) {
    if (!mPendingPositionChangeEvents.AppendElement(ev)) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      ev->Revoke();
    }
  }
  return rv;
}

nsresult
nsListBoxBodyFrame::DoInternalPositionChangedSync(bool aUp, PRInt32 aDelta)
{
  nsWeakFrame weak(this);
  
  // Process all the pending position changes first
  nsTArray< nsRefPtr<nsPositionChangedEvent> > temp;
  temp.SwapElements(mPendingPositionChangeEvents);
  for (PRUint32 i = 0; i < temp.Length(); ++i) {
    if (weak.IsAlive()) {
      temp[i]->Run();
    }
    temp[i]->Revoke();
  }

  if (!weak.IsAlive()) {
    return NS_OK;
  }

  return DoInternalPositionChanged(aUp, aDelta);
}

nsresult
nsListBoxBodyFrame::DoInternalPositionChanged(bool aUp, PRInt32 aDelta)
{
  if (aDelta == 0)
    return NS_OK;

  nsRefPtr<nsPresContext> presContext(PresContext());
  nsBoxLayoutState state(presContext);

  // begin timing how long it takes to scroll a row
  PRTime start = PR_Now();

  nsWeakFrame weakThis(this);
  mContent->GetDocument()->FlushPendingNotifications(Flush_Layout);
  if (!weakThis.IsAlive()) {
    return NS_OK;
  }

  {
    nsAutoScriptBlocker scriptBlocker;

    PRInt32 visibleRows = 0;
    if (mRowHeight)
      visibleRows = GetAvailableHeight()/mRowHeight;
  
    if (aDelta < visibleRows) {
      PRInt32 loseRows = aDelta;
      if (aUp) {
        // scrolling up, destroy rows from the bottom downwards
        ReverseDestroyRows(loseRows);
        mRowsToPrepend += aDelta;
        mLinkupFrame = nsnull;
      }
      else {
        // scrolling down, destroy rows from the top upwards
        DestroyRows(loseRows);
        mRowsToPrepend = 0;
      }
    }
    else {
      // We have scrolled so much that all of our current frames will
      // go off screen, so blow them all away. Weeee!
      nsIFrame *currBox = mFrames.FirstChild();
      nsCSSFrameConstructor* fc = presContext->PresShell()->FrameConstructor();
      fc->BeginUpdate();
      while (currBox) {
        nsIFrame *nextBox = currBox->GetNextSibling();
        RemoveChildFrame(state, currBox);
        currBox = nextBox;
      }
      fc->EndUpdate();
    }

    // clear frame markers so that CreateRows will re-create
    mTopFrame = mBottomFrame = nsnull; 
  
    mYPosition = mCurrentIndex*mRowHeight;
    mScrolling = true;
    presContext->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eResize, NS_FRAME_HAS_DIRTY_CHILDREN);
  }
  if (!weakThis.IsAlive()) {
    return NS_OK;
  }
  // Flush calls CreateRows
  // XXXbz there has to be a better way to do this than flushing!
  presContext->PresShell()->FlushPendingNotifications(Flush_Layout);
  if (!weakThis.IsAlive()) {
    return NS_OK;
  }

  mScrolling = false;
  
  VerticalScroll(mYPosition);

  PRTime end = PR_Now();

  PRTime difTime;
  LL_SUB(difTime, end, start);

  PRInt32 newTime;
  LL_L2I(newTime, difTime);
  newTime /= aDelta;

  // average old and new
  mTimePerRow = (newTime + mTimePerRow)/2;
  
  return NS_OK;
}

nsListScrollSmoother* 
nsListBoxBodyFrame::GetSmoother()
{
  if (!mScrollSmoother) {
    mScrollSmoother = new nsListScrollSmoother(this);
    NS_ASSERTION(mScrollSmoother, "out of memory");
    NS_IF_ADDREF(mScrollSmoother);
  }

  return mScrollSmoother;
}

void
nsListBoxBodyFrame::VerticalScroll(PRInt32 aPosition)
{
  nsIScrollableFrame* scrollFrame
    = nsLayoutUtils::GetScrollableFrameFor(this);
  if (!scrollFrame) {
    return;
  }

  nsPoint scrollPosition = scrollFrame->GetScrollPosition();
 
  scrollFrame->ScrollTo(nsPoint(scrollPosition.x, aPosition),
                        nsIScrollableFrame::INSTANT);

  mYPosition = aPosition;
}

////////// frame and box retrieval

nsIFrame*
nsListBoxBodyFrame::GetFirstFrame()
{
  mTopFrame = mFrames.FirstChild();
  return mTopFrame;
}

nsIFrame*
nsListBoxBodyFrame::GetLastFrame()
{
  return mFrames.LastChild();
}

bool
nsListBoxBodyFrame::SupportsOrdinalsInChildren()
{
  return false;
}

////////// lazy row creation and destruction

void
nsListBoxBodyFrame::CreateRows()
{
  // Get our client rect.
  nsRect clientRect;
  GetClientRect(clientRect);

  // Get the starting y position and the remaining available
  // height.
  nscoord availableHeight = GetAvailableHeight();
  
  if (availableHeight <= 0) {
    bool fixed = (GetFixedRowSize() != -1);
    if (fixed)
      availableHeight = 10;
    else
      return;
  }
  
  // get the first tree box. If there isn't one create one.
  bool created = false;
  nsIBox* box = GetFirstItemBox(0, &created);
  nscoord rowHeight = GetRowHeightAppUnits();
  while (box) {  
    if (created && mRowsToPrepend > 0)
      --mRowsToPrepend;

    // if the row height is 0 then fail. Wait until someone 
    // laid out and sets the row height.
    if (rowHeight == 0)
        return;
     
    availableHeight -= rowHeight;
    
    // should we continue? Is the enought height?
    if (!ContinueReflow(availableHeight))
      break;

    // get the next tree box. Create one if needed.
    box = GetNextItemBox(box, 0, &created);
  }

  mRowsToPrepend = 0;
  mLinkupFrame = nsnull;
}

void
nsListBoxBodyFrame::DestroyRows(PRInt32& aRowsToLose) 
{
  // We need to destroy frames until our row count has been properly
  // reduced.  A reflow will then pick up and create the new frames.
  nsIFrame* childFrame = GetFirstFrame();
  nsBoxLayoutState state(PresContext());

  nsCSSFrameConstructor* fc = PresContext()->PresShell()->FrameConstructor();
  fc->BeginUpdate();
  while (childFrame && aRowsToLose > 0) {
    --aRowsToLose;

    nsIFrame* nextFrame = childFrame->GetNextSibling();
    RemoveChildFrame(state, childFrame);

    mTopFrame = childFrame = nextFrame;
  }
  fc->EndUpdate();

  PresContext()->PresShell()->
    FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                     NS_FRAME_HAS_DIRTY_CHILDREN);
}

void
nsListBoxBodyFrame::ReverseDestroyRows(PRInt32& aRowsToLose) 
{
  // We need to destroy frames until our row count has been properly
  // reduced.  A reflow will then pick up and create the new frames.
  nsIFrame* childFrame = GetLastFrame();
  nsBoxLayoutState state(PresContext());

  nsCSSFrameConstructor* fc = PresContext()->PresShell()->FrameConstructor();
  fc->BeginUpdate();
  while (childFrame && aRowsToLose > 0) {
    --aRowsToLose;
    
    nsIFrame* prevFrame;
    prevFrame = childFrame->GetPrevSibling();
    RemoveChildFrame(state, childFrame);

    mBottomFrame = childFrame = prevFrame;
  }
  fc->EndUpdate();

  PresContext()->PresShell()->
    FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                     NS_FRAME_HAS_DIRTY_CHILDREN);
}

static bool
IsListItemChild(nsListBoxBodyFrame* aParent, nsIContent* aChild,
                nsIFrame** aChildFrame)
{
  *aChildFrame = nsnull;
  if (!aChild->IsXUL() || aChild->Tag() != nsGkAtoms::listitem) {
    return false;
  }
  nsIFrame* existingFrame = aChild->GetPrimaryFrame();
  if (existingFrame && existingFrame->GetParent() != aParent) {
    return false;
  }
  *aChildFrame = existingFrame;
  return true;
}

//
// Get the nsIBox for the first visible listitem, and if none exists,
// create one.
//
nsIBox* 
nsListBoxBodyFrame::GetFirstItemBox(PRInt32 aOffset, bool* aCreated)
{
  if (aCreated)
   *aCreated = false;

  // Clear ourselves out.
  mBottomFrame = mTopFrame;

  if (mTopFrame) {
    return mTopFrame->IsBoxFrame() ? static_cast<nsIBox*>(mTopFrame) : nsnull;
  }

  // top frame was cleared out
  mTopFrame = GetFirstFrame();
  mBottomFrame = mTopFrame;

  if (mTopFrame && mRowsToPrepend <= 0) {
    return mTopFrame->IsBoxFrame() ? static_cast<nsIBox*>(mTopFrame) : nsnull;
  }

  // At this point, we either have no frames at all, 
  // or the user has scrolled upwards, leaving frames
  // to be created at the top.  Let's determine which
  // content needs a new frame first.

  nsCOMPtr<nsIContent> startContent;
  if (mTopFrame && mRowsToPrepend > 0) {
    // We need to insert rows before the top frame
    nsIContent* topContent = mTopFrame->GetContent();
    nsIContent* topParent = topContent->GetParent();
    PRInt32 contentIndex = topParent->IndexOf(topContent);
    contentIndex -= aOffset;
    if (contentIndex < 0)
      return nsnull;
    startContent = topParent->GetChildAt(contentIndex - mRowsToPrepend);
  } else {
    // This will be the first item frame we create.  Use the content
    // at the current index, which is the first index scrolled into view
    GetListItemContentAt(mCurrentIndex+aOffset, getter_AddRefs(startContent));
  }

  if (startContent) {  
    nsIFrame* existingFrame;
    if (!IsListItemChild(this, startContent, &existingFrame)) {
      return GetFirstItemBox(++aOffset, aCreated);
    }
    if (existingFrame) {
      return existingFrame->IsBoxFrame() ? existingFrame : nsnull;
    }

    // Either append the new frame, or prepend it (at index 0)
    // XXX check here if frame was even created, it may not have been if
    //     display: none was on listitem content
    bool isAppend = mRowsToPrepend <= 0;
    
    nsPresContext* presContext = PresContext();
    nsCSSFrameConstructor* fc = presContext->PresShell()->FrameConstructor();
    nsIFrame* topFrame = nsnull;
    fc->CreateListBoxContent(presContext, this, nsnull, startContent,
                             &topFrame, isAppend, false, nsnull);
    mTopFrame = topFrame;
    if (mTopFrame) {
      if (aCreated)
        *aCreated = true;

      mBottomFrame = mTopFrame;

      return mTopFrame->IsBoxFrame() ? static_cast<nsIBox*>(mTopFrame) : nsnull;
    } else
      return GetFirstItemBox(++aOffset, 0);
  }

  return nsnull;
}

//
// Get the nsIBox for the next visible listitem after aBox, and if none
// exists, create one.
//
nsIBox* 
nsListBoxBodyFrame::GetNextItemBox(nsIBox* aBox, PRInt32 aOffset,
                                   bool* aCreated)
{
  if (aCreated)
    *aCreated = false;

  nsIFrame* result = aBox->GetNextSibling();

  if (!result || result == mLinkupFrame || mRowsToPrepend > 0) {
    // No result found. See if there's a content node that wants a frame.
    nsIContent* prevContent = aBox->GetContent();
    nsIContent* parentContent = prevContent->GetParent();

    PRInt32 i = parentContent->IndexOf(prevContent);

    PRUint32 childCount = parentContent->GetChildCount();
    if (((PRUint32)i + aOffset + 1) < childCount) {
      // There is a content node that wants a frame.
      nsIContent *nextContent = parentContent->GetChildAt(i + aOffset + 1);

      nsIFrame* existingFrame;
      if (!IsListItemChild(this, nextContent, &existingFrame)) {
        return GetNextItemBox(aBox, ++aOffset, aCreated);
      }
      if (!existingFrame) {
        // Either append the new frame, or insert it after the current frame
        bool isAppend = result != mLinkupFrame && mRowsToPrepend <= 0;
        nsIFrame* prevFrame = isAppend ? nsnull : aBox;
      
        nsPresContext* presContext = PresContext();
        nsCSSFrameConstructor* fc = presContext->PresShell()->FrameConstructor();
        fc->CreateListBoxContent(presContext, this, prevFrame, nextContent,
                                 &result, isAppend, false, nsnull);

        if (result) {
          if (aCreated)
            *aCreated = true;
        } else
          return GetNextItemBox(aBox, ++aOffset, aCreated);
      } else {
        result = existingFrame;
      }
            
      mLinkupFrame = nsnull;
    }
  }

  if (!result)
    return nsnull;

  mBottomFrame = result;

  NS_ASSERTION(!result->IsBoxFrame() || result->GetParent() == this,
               "returning frame that is not in childlist");

  return result->IsBoxFrame() ? result : nsnull;
}

bool
nsListBoxBodyFrame::ContinueReflow(nscoord height) 
{
#ifdef ACCESSIBILITY
  if (nsIPresShell::IsAccessibilityActive()) {
    // Create all the frames at once so screen readers and
    // onscreen keyboards can see the full list right away
    return true;
  }
#endif

  if (height <= 0) {
    nsIFrame* lastChild = GetLastFrame();
    nsIFrame* startingPoint = mBottomFrame;
    if (startingPoint == nsnull) {
      // We just want to delete everything but the first item.
      startingPoint = GetFirstFrame();
    }

    if (lastChild != startingPoint) {
      // We have some hangers on (probably caused by shrinking the size of the window).
      // Nuke them.
      nsIFrame* currFrame = startingPoint->GetNextSibling();
      nsBoxLayoutState state(PresContext());

      nsCSSFrameConstructor* fc =
        PresContext()->PresShell()->FrameConstructor();
      fc->BeginUpdate();
      while (currFrame) {
        nsIFrame* nextFrame = currFrame->GetNextSibling();
        RemoveChildFrame(state, currFrame);
        currFrame = nextFrame;
      }
      fc->EndUpdate();

      PresContext()->PresShell()->
        FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                         NS_FRAME_HAS_DIRTY_CHILDREN);
    }
    return false;
  }
  else
    return true;
}

NS_IMETHODIMP
nsListBoxBodyFrame::ListBoxAppendFrames(nsFrameList& aFrameList)
{
  // append them after
  nsBoxLayoutState state(PresContext());
  const nsFrameList::Slice& newFrames = mFrames.AppendFrames(nsnull, aFrameList);
  if (mLayoutManager)
    mLayoutManager->ChildrenAppended(this, state, newFrames);
  PresContext()->PresShell()->
    FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                     NS_FRAME_HAS_DIRTY_CHILDREN);
  
  return NS_OK;
}

NS_IMETHODIMP
nsListBoxBodyFrame::ListBoxInsertFrames(nsIFrame* aPrevFrame,
                                        nsFrameList& aFrameList)
{
  // insert the frames to our info list
  nsBoxLayoutState state(PresContext());
  const nsFrameList::Slice& newFrames =
    mFrames.InsertFrames(nsnull, aPrevFrame, aFrameList);
  if (mLayoutManager)
    mLayoutManager->ChildrenInserted(this, state, aPrevFrame, newFrames);
  PresContext()->PresShell()->
    FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                     NS_FRAME_HAS_DIRTY_CHILDREN);

  return NS_OK;
}

// 
// Called by nsCSSFrameConstructor when a new listitem content is inserted.
//
void 
nsListBoxBodyFrame::OnContentInserted(nsPresContext* aPresContext, nsIContent* aChildContent)
{
  if (mRowCount >= 0)
    ++mRowCount;

  // The RDF content builder will build content nodes such that they are all 
  // ready when OnContentInserted is first called, meaning the first call
  // to CreateRows will create all the frames, but OnContentInserted will
  // still be called again for each content node - so we need to make sure
  // that the frame for each content node hasn't already been created.
  nsIFrame* childFrame = aChildContent->GetPrimaryFrame();
  if (childFrame)
    return;

  PRInt32 siblingIndex;
  nsCOMPtr<nsIContent> nextSiblingContent;
  GetListItemNextSibling(aChildContent, getter_AddRefs(nextSiblingContent), siblingIndex);
  
  // if we're inserting our item before the first visible content,
  // then we need to shift all rows down by one
  if (siblingIndex >= 0 &&  siblingIndex-1 <= mCurrentIndex) {
    mTopFrame = nsnull;
    mRowsToPrepend = 1;
  } else if (nextSiblingContent) {
    // we may be inserting before a frame that is on screen
    nsIFrame* nextSiblingFrame = nextSiblingContent->GetPrimaryFrame();
    mLinkupFrame = nextSiblingFrame;
  }
  
  CreateRows();
  PresContext()->PresShell()->
    FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                     NS_FRAME_HAS_DIRTY_CHILDREN);
}

// 
// Called by nsCSSFrameConstructor when listitem content is removed.
//
void
nsListBoxBodyFrame::OnContentRemoved(nsPresContext* aPresContext,
                                     nsIContent* aContainer,
                                     nsIFrame* aChildFrame,
                                     nsIContent* aOldNextSibling)
{
  NS_ASSERTION(!aChildFrame || aChildFrame->GetParent() == this,
               "Removing frame that's not our child... Not good");
  
  if (mRowCount >= 0)
    --mRowCount;

  if (aContainer) {
    if (!aChildFrame) {
      // The row we are removing is out of view, so we need to try to
      // determine the index of its next sibling.
      PRInt32 siblingIndex = -1;
      if (aOldNextSibling) {
        nsCOMPtr<nsIContent> nextSiblingContent;
        GetListItemNextSibling(aOldNextSibling,
                               getter_AddRefs(nextSiblingContent),
                               siblingIndex);
      }
    
      // if the row being removed is off-screen and above the top frame, we need to
      // adjust our top index and tell the scrollbar to shift up one row.
      if (siblingIndex >= 0 && siblingIndex-1 < mCurrentIndex) {
        NS_PRECONDITION(mCurrentIndex > 0, "mCurrentIndex > 0");
        --mCurrentIndex;
        mYPosition = mCurrentIndex*mRowHeight;
        VerticalScroll(mYPosition);
      }
    } else if (mCurrentIndex > 0) {
      // At this point, we know we have a scrollbar, and we need to know 
      // if we are scrolled to the last row.  In this case, the behavior
      // of the scrollbar is to stay locked to the bottom.  Since we are
      // removing visible content, the first visible row will have to move
      // down by one, and we will have to insert a new frame at the top.
      
      // if the last content node has a frame, we are scrolled to the bottom
      ChildIterator iter, last;
      ChildIterator::Init(mContent, &iter, &last);
      if (iter != last) {
        iter = last;
        --iter;
        nsIContent *lastChild = *iter;
        nsIFrame* lastChildFrame = lastChild->GetPrimaryFrame();
      
        if (lastChildFrame) {
          mTopFrame = nsnull;
          mRowsToPrepend = 1;
          --mCurrentIndex;
          mYPosition = mCurrentIndex*mRowHeight;
          VerticalScroll(mYPosition);
        }
      }
    }
  }

  // if we're removing the top row, the new top row is the next row
  if (mTopFrame && mTopFrame == aChildFrame)
    mTopFrame = mTopFrame->GetNextSibling();

  // Go ahead and delete the frame.
  nsBoxLayoutState state(aPresContext);
  if (aChildFrame) {
    RemoveChildFrame(state, aChildFrame);
  }

  PresContext()->PresShell()->
    FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                     NS_FRAME_HAS_DIRTY_CHILDREN);
}

void
nsListBoxBodyFrame::GetListItemContentAt(PRInt32 aIndex, nsIContent** aContent)
{
  *aContent = nsnull;

  PRInt32 itemsFound = 0;
  ChildIterator iter, last;
  for (ChildIterator::Init(mContent, &iter, &last);
       iter != last;
       ++iter) {
    nsIContent *kid = (*iter);
    if (kid->Tag() == nsGkAtoms::listitem) {
      ++itemsFound;
      if (itemsFound-1 == aIndex) {
        *aContent = kid;
        NS_IF_ADDREF(*aContent);
        return;
      }
    }
  }
}

void
nsListBoxBodyFrame::GetListItemNextSibling(nsIContent* aListItem, nsIContent** aContent, PRInt32& aSiblingIndex)
{
  *aContent = nsnull;
  aSiblingIndex = -1;
  nsIContent *prevKid = nsnull;
  ChildIterator iter, last;
  for (ChildIterator::Init(mContent, &iter, &last);
       iter != last;
       ++iter) {
    nsIContent *kid = (*iter);

    if (kid->Tag() == nsGkAtoms::listitem) {
      ++aSiblingIndex;
      if (prevKid == aListItem) {
        *aContent = kid;
        NS_IF_ADDREF(*aContent);
        return;
      }
    }
    prevKid = kid;
  }

  aSiblingIndex = -1; // no match, so there is no next sibling
}

void
nsListBoxBodyFrame::RemoveChildFrame(nsBoxLayoutState &aState,
                                     nsIFrame         *aFrame)
{
  if (!mFrames.ContainsFrame(aFrame)) {
    NS_ERROR("tried to remove a child frame which isn't our child");
    return;
  }

  if (aFrame == GetContentInsertionFrame()) {
    // Don't touch that one
    return;
  }

#ifdef ACCESSIBILITY
  nsAccessibilityService* accService = nsIPresShell::AccService();
  if (accService) {
    nsIContent* content = aFrame->GetContent();
    accService->ContentRemoved(PresContext()->PresShell(), content->GetParent(),
                               content);
  }
#endif

  mFrames.RemoveFrame(aFrame);
  if (mLayoutManager)
    mLayoutManager->ChildrenRemoved(this, aState, aFrame);
  aFrame->Destroy();
}

// Creation Routines ///////////////////////////////////////////////////////////////////////

already_AddRefed<nsBoxLayout> NS_NewListBoxLayout();

nsIFrame*
NS_NewListBoxBodyFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  nsCOMPtr<nsBoxLayout> layout = NS_NewListBoxLayout();
  if (!layout) {
    return nsnull;
  }

  return new (aPresShell) nsListBoxBodyFrame(aPresShell, aContext, layout);
}

NS_IMPL_FRAMEARENA_HELPERS(nsListBoxBodyFrame)
