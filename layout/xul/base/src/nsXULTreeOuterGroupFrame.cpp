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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *  Mike Pinkerton (pinkerton@netscape.com)
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

#include "nsCOMPtr.h"
#include "nsXULTreeOuterGroupFrame.h"
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "nsIScrollableFrame.h"
#include "nsIScrollbarFrame.h"
#include "nsISupportsArray.h"
#include "nsCSSFrameConstructor.h"
#include "nsIDocument.h"
#include "nsTreeItemDragCapturer.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDragService.h"
#include "nsIServiceManager.h"
#include "nsIScrollableView.h"
#include "nsIMonument.h"
#include "nsTreeLayout.h"
#include "nsITimer.h"
#include "nsIBindingManager.h"
#include "nsScrollPortFrame.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nsIStyleContext.h"
#include "nsIDOMText.h"

//#define MOZ_GRID2 1

#ifdef MOZ_GRID2
  #include "nsGridRowGroupLayout.h"
#else
#include "nsTempleLayout.h"
#endif

#define TICK_FACTOR 50

// the longest amount of time that can go by before the use
// notices it as a delay.
#define USER_TIME_THRESHOLD 150000

// how long it takes to layout a single row inital value.
// we will time this after we scroll a few rows.
#define TIME_PER_ROW_INITAL  50000

// if we decide we can't layout the rows in the amount of time. How long
// do we wait before checking again?
#define SMOOTH_INTERVAL 100


nsresult NS_NewAutoScrollTimer(nsXULTreeOuterGroupFrame* aTree, nsDragAutoScrollTimer **aResult) ;

//
// nsDragOverListener
//
// Just a little class that listens for dragOvers to trigger the auto-scrolling
// code.
//
class nsDragOverListener : public nsIDOMDragListener
{
public:

  nsDragOverListener ( nsXULTreeOuterGroupFrame* inTree )  
    : mTree ( inTree ) 
    {  NS_INIT_REFCNT(); }

  virtual ~nsDragOverListener() { } ;

  NS_DECL_ISUPPORTS

    // nsIDOMDragListener
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; }
  NS_IMETHOD DragEnter(nsIDOMEvent* aDragEvent) { return NS_OK; }
  NS_IMETHOD DragOver(nsIDOMEvent* aDragEvent);
  NS_IMETHOD DragExit(nsIDOMEvent* aDragEvent) { return NS_OK; }
  NS_IMETHOD DragDrop(nsIDOMEvent* aDragEvent) { return NS_OK; }
  NS_IMETHOD DragGesture(nsIDOMEvent* aDragEvent) { return NS_OK; }

protected:

  nsXULTreeOuterGroupFrame* mTree;

}; // nsDragEnterListener


NS_IMPL_ISUPPORTS2(nsDragOverListener, nsIDOMEventListener, nsIDOMDragListener)


//
// DragOver
//
// Kick off our timer/capturing for autoscrolling, the drag has entered us. We
// will continue capturing the mouse until the auto-scroll manager tells us to
// stop (either the drag is over, it left the window, or someone else wants a 
// crack at auto-scrolling).
//
nsresult
nsDragOverListener :: DragOver(nsIDOMEvent* aDragEvent)
{
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent ( do_QueryInterface(aDragEvent) );
  if ( mouseEvent ) {
    PRInt32 x = 0, y = 0;
    mouseEvent->GetClientX ( &x );
    mouseEvent->GetClientY ( &y );
    mTree->HandleAutoScrollTracking ( nsPoint(x,y) );
  }
  return NS_OK;

} // DragOver



#ifdef XP_MAC
#pragma mark -
#endif

/* A mediator used to smooth out scrolling. It works by seeing if 
 * we have time to scroll the amount of rows requested. This is determined
 * by measuring how long it takes to scroll a row. If we can scroll the 
 * rows in time we do so. If not we start a timer and skip the request. We
 * do this until the timer finally first because the user has stopped moving
 * the mouse. Then do all the queued requests in on shot.
 */
class nsScrollSmoother : public nsITimerCallback
{
public:

  NS_IMETHOD_(void) Notify(nsITimer *timer);

  void Start();
  void Stop();
  PRBool IsRunning();

  NS_DECL_ISUPPORTS
  virtual ~nsScrollSmoother();

  nsScrollSmoother(nsXULTreeOuterGroupFrame* aOuter);

  nsCOMPtr<nsITimer>         mRepeatTimer;
  PRBool mDelta;
  nsXULTreeOuterGroupFrame* mOuter;
}; 

nsScrollSmoother::nsScrollSmoother(nsXULTreeOuterGroupFrame* aOuter)
{
  NS_INIT_REFCNT();
  mDelta = 0;
  mOuter = aOuter;
}

nsScrollSmoother::~nsScrollSmoother()
{
  Stop();
}


PRBool nsScrollSmoother::IsRunning()
{
  if (mRepeatTimer)
    return PR_TRUE;
  else 
    return PR_FALSE;
}

void nsScrollSmoother::Start()
{
  Stop();
  mRepeatTimer = do_CreateInstance("@mozilla.org/timer;1");
  mRepeatTimer->Init(this, SMOOTH_INTERVAL);
}

void nsScrollSmoother::Stop()
{
  if ( mRepeatTimer ) {
    mRepeatTimer->Cancel();
    mRepeatTimer = nsnull;
  }
}

NS_IMETHODIMP_(void) nsScrollSmoother::Notify(nsITimer *timer)
{
  //printf("Timer Callback!\n");

  Stop();

  NS_ASSERTION(mOuter, "mOuter is null, see bug #68365");
  if (!mOuter) return;

  // actually do some work.
  mOuter->InternalPositionChangedCallback();
}

NS_IMPL_ISUPPORTS1(nsScrollSmoother, nsITimerCallback)


//
// NS_NewXULTreeOuterGroupFrame
//
// Creates a new TreeOuterGroup frame
//
nsresult
NS_NewXULTreeOuterGroupFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRBool aIsRoot, 
                        nsIBoxLayout* aLayoutManager)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsXULTreeOuterGroupFrame* it = new (aPresShell) nsXULTreeOuterGroupFrame(aPresShell, aIsRoot, aLayoutManager);
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewXULTreeOuterGroupFrame


// Constructor
nsXULTreeOuterGroupFrame::nsXULTreeOuterGroupFrame(nsIPresShell* aPresShell, PRBool aIsRoot, nsIBoxLayout* aLayoutManager)
:nsXULTreeGroupFrame(aPresShell, aIsRoot, aLayoutManager),
 mBatchCount(0), mRowGroupInfo(nsnull), mRowHeight(0), mCurrentIndex(0), mOldIndex(0),
 mTreeIsSorted(PR_FALSE), mDragOverListener(nsnull), mCanDropBetweenRows(PR_TRUE),
 mRowHeightWasSet(PR_FALSE), mReflowCallbackPosted(PR_FALSE), mYPosition(0), mScrolling(PR_FALSE),
 mScrollSmoother(nsnull), mTimePerRow(TIME_PER_ROW_INITAL), mAdjustScroll(PR_FALSE), mTreeItemTag(nsXULAtoms::treeitem),
 mTreeRowTag(nsXULAtoms::treerow), mTreeChildrenTag(nsXULAtoms::treechildren), mStringWidth(-1)
{
}


NS_IMETHODIMP
nsXULTreeOuterGroupFrame::Destroy(nsIPresContext* aPresContext)
{
  
  // make sure we cancel any posted callbacks.
  if (mReflowCallbackPosted) {
     nsCOMPtr<nsIPresShell> shell;
     aPresContext->GetShell(getter_AddRefs(shell));
     shell->CancelReflowCallback(this);
  }
  

  return nsXULTreeGroupFrame::Destroy(aPresContext);
}

// Destructor
nsXULTreeOuterGroupFrame::~nsXULTreeOuterGroupFrame()
{
  NS_IF_RELEASE(mScrollSmoother);

  // TODO cancel posted events.

  nsCOMPtr<nsIContent> content;
  GetContent(getter_AddRefs(content));
  nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(content));

  // NOTE: the last Remove will delete the drag capturer
  if ( receiver && mDragOverListener ) 
    receiver->RemoveEventListener(NS_ConvertASCIItoUCS2("dragover"), mDragOverListener, PR_TRUE);
  
  delete mRowGroupInfo;

#if USE_TIMER_TO_DELAY_SCROLLING
  StopScrollTracking();
  mAutoScrollTimer = nsnull;
#endif

}

NS_IMETHODIMP_(nsrefcnt) 
nsXULTreeOuterGroupFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsXULTreeOuterGroupFrame::Release(void)
{
  return NS_OK;
}

//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsXULTreeOuterGroupFrame)
  NS_INTERFACE_MAP_ENTRY(nsIScrollbarMediator)
  NS_INTERFACE_MAP_ENTRY(nsIReflowCallback)
NS_INTERFACE_MAP_END_INHERITING(nsXULTreeGroupFrame)


//
// Init
//
// Setup scrolling and event listeners for drag auto-scrolling
//
NS_IMETHODIMP
nsXULTreeOuterGroupFrame::Init(nsIPresContext* aPresContext, nsIContent* aContent,
                               nsIFrame* aParent, nsIStyleContext* aContext, nsIFrame* aPrevInFlow)
{
  nsresult rv = nsXULTreeGroupFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  nsAutoString value;
  mContent->GetAttr(kNameSpaceID_None, nsXULAtoms::treeitem, value);
  if (!value.IsEmpty())
    mTreeItemTag = getter_AddRefs(NS_NewAtom(value));
  mContent->GetAttr(kNameSpaceID_None, nsXULAtoms::treerow, value);
  if (!value.IsEmpty())
    mTreeRowTag = getter_AddRefs(NS_NewAtom(value));
  mContent->GetAttr(kNameSpaceID_None, nsXULAtoms::treechildren, value);
  if (!value.IsEmpty())
    mTreeChildrenTag = getter_AddRefs(NS_NewAtom(value));

 // mLayingOut = PR_FALSE;

  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  mOnePixel = NSIntPixelsToTwips(1, p2t);
  
  nsIFrame* box;
  aParent->GetParent(&box);
  if (!box)
    return rv;

  nsCOMPtr<nsIScrollableFrame> scrollFrame(do_QueryInterface(box));
  if (!scrollFrame)
    return rv;

  nsIScrollableView* scrollableView;
  scrollFrame->GetScrollableView(aPresContext, &scrollableView);
  scrollableView->SetScrollProperties(NS_SCROLL_PROPERTY_ALWAYS_BLIT);

  nsIBox* verticalScrollbar;
  scrollFrame->GetScrollbarBox(PR_TRUE, &verticalScrollbar);
  if (!verticalScrollbar) {
    NS_ERROR("Unable to install the scrollbar mediator on the tree widget. You must be using GFX scrollbars.");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIScrollbarFrame> scrollbarFrame(do_QueryInterface(verticalScrollbar));
  scrollbarFrame->SetScrollbarMediator(this);

  nsBoxLayoutState boxLayoutState(aPresContext);

  const nsStyleFont* font = (const nsStyleFont*)aContext->GetStyleData(eStyleStruct_Font);
  nsCOMPtr<nsIDeviceContext> dc;
  aPresContext->GetDeviceContext(getter_AddRefs(dc));
  nsCOMPtr<nsIFontMetrics> fm;
  dc->GetMetricsFor(font->mFont, *getter_AddRefs(fm));
  fm->GetHeight(mRowHeight);

  // Our frame's lifetime is bounded by the lifetime of the content model, so we're guaranteed
  // that the content node won't go away on us. As a result, our listener can't go away before the
  // frame is deleted. Since the content node holds owning references to our drag capturer, which
  // we tear down in the dtor, there is no need to hold an owning ref to it ourselves.  
  nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(aContent));
  if ( receiver ) {
    mDragOverListener = new nsDragOverListener(this);
    receiver->AddEventListener(NS_ConvertASCIItoUCS2("dragover"), mDragOverListener, PR_FALSE);
  }
  
  // our parent is the <tree> tag. check if it has an attribute denying the ability to
  // drop between rows and cache it here for the benefit of the rows inside us.
  nsCOMPtr<nsIContent> parent;
  GetTreeContent(getter_AddRefs(parent));

  if ( parent ) {
    nsAutoString attr;
    parent->GetAttr ( kNameSpaceID_None, nsXULAtoms::ddNoDropBetweenRows, attr ); 
    if ( attr.Equals(NS_ConvertASCIItoUCS2("true")) )
      mCanDropBetweenRows = PR_FALSE;
  }
  
  return rv;

} // Init

NS_IMETHODIMP
nsXULTreeOuterGroupFrame::NeedsRecalc()
{
  mStringWidth = -1;
  return nsXULTreeGroupFrame::NeedsRecalc();
}

NS_IMETHODIMP
nsXULTreeOuterGroupFrame::GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
  // NeedsRecalc();  // Don't think this is needed any more.
  return nsXULTreeGroupFrame::GetPrefSize(aBoxLayoutState, aSize);
}

NS_IMETHODIMP
nsXULTreeOuterGroupFrame::DoLayout(nsBoxLayoutState& aBoxLayoutState)
{
  if (mScrolling)
    aBoxLayoutState.SetDisablePainting(PR_TRUE);

  nsresult rv = nsXULTreeGroupFrame::DoLayout(aBoxLayoutState);

  if (mScrolling)
    aBoxLayoutState.SetDisablePainting(PR_FALSE);

  // if we are scrolled and the row height changed
  // make sure we are scrolled to a correct index.
  if (mAdjustScroll) 
     PostReflowCallback();

  return rv;
}

PRInt32
nsXULTreeOuterGroupFrame::GetFixedRowSize()
{
  PRInt32 dummy;

  nsCOMPtr<nsIContent> parent;
  GetTreeContent(getter_AddRefs(parent));
  nsAutoString rows;
  parent->GetAttr(kNameSpaceID_None, nsXULAtoms::rows, rows);
  if (!rows.IsEmpty())
    return rows.ToInteger(&dummy);
 
  parent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::size, rows);

  if (!rows.IsEmpty())
    return rows.ToInteger(&dummy);

  return -1;
}

void
nsXULTreeOuterGroupFrame::SetRowHeight(nscoord aRowHeight)
{ 
  if (aRowHeight > mRowHeight) { 
    mRowHeight = aRowHeight;
    
    nsCOMPtr<nsIContent> parent;
    GetTreeContent(getter_AddRefs(parent));
    nsAutoString rows;
    parent->GetAttr(kNameSpaceID_None, nsXULAtoms::rows, rows);
    if (rows.IsEmpty())
      parent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::size, rows);
    
    if (!rows.IsEmpty()) {
      PRInt32 dummy;
      PRInt32 count = rows.ToInteger(&dummy);
      float t2p;
      mPresContext->GetTwipsToPixels(&t2p);
      PRInt32 rowHeight = NSTwipsToIntPixels(aRowHeight, t2p);
      nsAutoString value;
      value.AppendInt(rowHeight*count);
      mContent->SetAttr(kNameSpaceID_None, nsXULAtoms::minheight, value, PR_FALSE);
    }

    // signal we need to dirty everything 
    // and we want to be notified after reflow
    // so we can create or destory rows as needed
    mRowHeightWasSet = PR_TRUE;
    PostReflowCallback();
  } 
}

nscoord
nsXULTreeOuterGroupFrame::GetYPosition()
{
  return mYPosition;
}

void
nsXULTreeOuterGroupFrame::VerticalScroll(PRInt32 aPosition)
{
  nsIBox* box;
  GetParentBox(&box);
  if (!box)
    return;

  box->GetParentBox(&box);
  if (!box)
    return;

  nsCOMPtr<nsIScrollableFrame> scrollFrame(do_QueryInterface(box));
  if (!scrollFrame)
    return;

  nscoord x, y;
  scrollFrame->GetScrollPosition(mPresContext, x, y);
 
  scrollFrame->ScrollTo(mPresContext, x, aPosition, NS_SCROLL_PROPERTY_ALWAYS_BLIT);

  mYPosition = aPosition;
}

nscoord
nsXULTreeOuterGroupFrame::GetAvailableHeight()
{
  nsIBox* box;
  GetParentBox(&box);
  if (!box)
    return 0;

  nsRect contentRect;
  box->GetContentRect(contentRect);
  return contentRect.height;
}

void
nsXULTreeOuterGroupFrame::ComputeTotalRowCount(PRInt32& aCount, nsIContent* aParent)
{
  if (!mRowGroupInfo) {
    mRowGroupInfo = new nsXULTreeRowGroupInfo();
  }

  nsCOMPtr<nsIContent> parent = aParent;
  if (aParent == mContent) {
    nsCOMPtr<nsIContent> content;
    mContent->GetBindingParent(getter_AddRefs(content));
    if (content)
      GetTreeContent(getter_AddRefs(parent));
  }

  PRInt32 childCount;
  parent->ChildCount(childCount);

  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> childContent;
    parent->ChildAt(i, *getter_AddRefs(childContent));
    nsCOMPtr<nsIAtom> tag;
    childContent->GetTag(*getter_AddRefs(tag));
    if (tag == mTreeRowTag) {
      if ((aCount%TICK_FACTOR) == 0)
        mRowGroupInfo->Add(childContent);

      mRowGroupInfo->mLastChild = childContent;

      aCount++;
    }
    else if (tag == mTreeItemTag) {
      // Descend into this row group and try to find the next row.
      ComputeTotalRowCount(aCount, childContent);
    }
    else if (tag == mTreeChildrenTag) {
      // If it's open, descend into its treechildren.
      nsCOMPtr<nsIAtom> openAtom = dont_AddRef(NS_NewAtom("open"));
      nsAutoString isOpen;
      nsCOMPtr<nsIContent> parent;
      childContent->GetParent(*getter_AddRefs(parent));
      parent->GetAttr(kNameSpaceID_None, openAtom, isOpen);
      if (isOpen.EqualsWithConversion("true"))
        ComputeTotalRowCount(aCount, childContent);
    }
  }
}

void
nsXULTreeOuterGroupFrame::PostReflowCallback()
{
  if (!mReflowCallbackPosted) {
    mReflowCallbackPosted = PR_TRUE;
    nsCOMPtr<nsIPresShell> shell;
    mPresContext->GetShell(getter_AddRefs(shell));
    shell->PostReflowCallback(this);
  }
}

NS_IMETHODIMP
nsXULTreeOuterGroupFrame::VisibilityChanged(PRBool aVisible)
{
  if (!aVisible && mCurrentIndex > 0)
    EnsureRowIsVisible(0);

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeOuterGroupFrame::ScrollbarButtonPressed(PRInt32 aOldIndex, PRInt32 aNewIndex)
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

NS_IMETHODIMP
nsXULTreeOuterGroupFrame::PositionChanged(PRInt32 aOldIndex, PRInt32& aNewIndex)
{ 
  if (mScrolling)
    return NS_OK;

  PRInt32 oldTwipIndex, newTwipIndex;
  oldTwipIndex = mCurrentIndex*mRowHeight;
  newTwipIndex = (aNewIndex*mOnePixel);
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

  nsScrollSmoother* smoother = GetSmoother();

  //printf("%d rows, %d per row, Estimated time needed %d, Threshhold %d (%s)\n", rowDelta, mTimePerRow, mTimePerRow * rowDelta, USER_TIME_THRESHOLD, (mTimePerRow * rowDelta > USER_TIME_THRESHOLD) ? "Nope" : "Yep");

  // if we can't scroll the rows in time then start a timer. We will eat
  // events until the user stops moving and the timer stops.
  if (smoother->IsRunning() || rowDelta*mTimePerRow > USER_TIME_THRESHOLD) {

     smoother->Stop();

     nsCOMPtr<nsIPresShell> shell;
     mPresContext->GetShell(getter_AddRefs(shell));
     shell->FlushPendingNotifications(PR_FALSE);

     smoother->mDelta = newTwipIndex > oldTwipIndex ? rowDelta : -rowDelta;

     //printf("Eating scroll!\n");

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

nsScrollSmoother* 
nsXULTreeOuterGroupFrame::GetSmoother()
{
  if (!mScrollSmoother) {
    mScrollSmoother = new nsScrollSmoother(this);
    NS_ASSERTION(mScrollSmoother, "out of memory");
    NS_IF_ADDREF(mScrollSmoother);
  }

  return mScrollSmoother;
}

NS_IMETHODIMP
nsXULTreeOuterGroupFrame::InternalPositionChangedCallback()
{
   nsScrollSmoother* smoother = GetSmoother();
   
   if (smoother->mDelta == 0)
     return NS_OK;

   mCurrentIndex += smoother->mDelta;

   if (mCurrentIndex < 0)
     mCurrentIndex = 0;

   return InternalPositionChanged(smoother->mDelta < 0, smoother->mDelta < 0 ? -smoother->mDelta : smoother->mDelta);
}

NS_IMETHODIMP
nsXULTreeOuterGroupFrame::InternalPositionChanged(PRBool aUp, PRInt32 aDelta, PRBool aForceDestruct)
{  
  if (aDelta == 0)
    return NS_OK;

  // begin timing how long it takes to scroll a row
  PRTime start = PR_Now();

  //printf("Actually doing scroll mCurrentIndex=%d, delta=%d!\n", mCurrentIndex, aDelta);

  nsCOMPtr<nsIPresShell> shell;
  mPresContext->GetShell(getter_AddRefs(shell));
  shell->FlushPendingNotifications(PR_FALSE);

  PRInt32 visibleRows = 0;
  if (mRowHeight)
	  visibleRows = GetAvailableHeight()/mRowHeight;
  
  // Get our presentation context.
  if (aDelta < visibleRows && !aForceDestruct) {
    PRInt32 loseRows = aDelta;

    // scrolling down
    if (!aUp) {
      // Figure out how many rows we have to lose off the top.
      DestroyRows(loseRows);
    }
    // scrolling up
    else {
      // Get our first row content.
      nsCOMPtr<nsIContent> rowContent;
      GetFirstRowContent(getter_AddRefs(rowContent));

      // Figure out how many rows we have to lose off the bottom.
      ReverseDestroyRows(loseRows);
    
      // Now that we've lost some rows, we need to create a
      // content chain that provides a hint for moving forward.
      nsCOMPtr<nsIContent> topRowContent;
      PRInt32 findContent = aDelta;
      FindPreviousRowContent(findContent, rowContent, nsnull, getter_AddRefs(topRowContent));
      ConstructContentChain(topRowContent);
	    //Now construct the chain for the old top row so its content chain gets
	    //set up correctly.
	    ConstructOldContentChain(rowContent);
    }
  }
  else {
    // Just blow away all our frames, but keep a content chain
    // as a hint to figure out how to build the frames.
    // Remove the scrollbar first.
    // get the starting row index and row count
    nsIBox* currBox;
    GetChildBox(&currBox);
    while (currBox) {
      nsIBox* nextBox;
      currBox->GetNextBox(&nextBox);
      nsIFrame* frame;
      currBox->QueryInterface(NS_GET_IID(nsIFrame), (void**)&frame); 
      mFrameConstructor->RemoveMappingsForFrameSubtree(mPresContext, frame, nsnull);
      currBox = nextBox;
    }

    nsBoxLayoutState state(mPresContext);
    ClearChildren(state);
    mFrames.DestroyFrames(mPresContext);

    nsCOMPtr<nsIContent> topRowContent;
    FindRowContentAtIndex(mCurrentIndex, mContent, getter_AddRefs(topRowContent));

    if (topRowContent)
      ConstructContentChain(topRowContent);
  }

  mTopFrame = mBottomFrame = nsnull; // Make sure everything is cleared out.
  
  mYPosition = mCurrentIndex*mRowHeight;
  nsBoxLayoutState state(mPresContext);
  mScrolling = PR_TRUE;
  MarkDirtyChildren(state);
  shell->FlushPendingNotifications(PR_FALSE);
  mScrolling = PR_FALSE;
  
  VerticalScroll(mYPosition);

  if (aForceDestruct)
    Redraw(state, nsnull, PR_FALSE);
  
  PRTime end = PR_Now();

  PRTime difTime;
  LL_SUB(difTime, end, start);

  PRInt32 newTime;
  LL_L2I(newTime, difTime);
  newTime /= aDelta;

  // average old and new
  mTimePerRow = (newTime + mTimePerRow)/2;
  
  //printf("time per row=%d\n", mTimePerRow);

  return NS_OK;
}


void 
nsXULTreeOuterGroupFrame::ConstructContentChain(nsIContent* aRowContent)
{
  // Create the content chain array.
  NS_IF_RELEASE(mContentChain);
  NS_NewISupportsArray(&mContentChain);

  nsCOMPtr<nsIContent> treeContent;
  GetTreeContent(getter_AddRefs(treeContent));

  // Move up the chain until we hit our content node.
  nsCOMPtr<nsIContent> currContent = dont_QueryInterface(aRowContent);
  while (currContent && (currContent.get() != mContent) &&
         currContent != treeContent) {
    mContentChain->InsertElementAt(currContent, 0);
    nsCOMPtr<nsIContent> otherContent = currContent;
    otherContent->GetParent(*getter_AddRefs(currContent));
  }

  NS_ASSERTION(currContent.get() == mContent || currContent == treeContent, 
               "Disaster! Content not contained in our tree!\n");
}

void 
nsXULTreeOuterGroupFrame::ConstructOldContentChain(nsIContent* aOldRowContent)
{
	nsCOMPtr<nsIContent> childOfCommonAncestor;

	//Find the first child of the common ancestor between the new top row's content chain
	//and the old top row.  Everything between this child and the old top row potentially need
	//to have their content chains reset.
	FindChildOfCommonContentChainAncestor(aOldRowContent, getter_AddRefs(childOfCommonAncestor));

	if (childOfCommonAncestor) {
      //Set up the old top rows content chian.
	  CreateOldContentChain(aOldRowContent, childOfCommonAncestor);
	}
}

void
nsXULTreeOuterGroupFrame::FindChildOfCommonContentChainAncestor(nsIContent *startContent, nsIContent **child)
{
	PRUint32 count;
	if (mContentChain)
	{
	  nsresult rv = mContentChain->Count(&count);

	  if (NS_SUCCEEDED(rv) && (count >0)) {
	    for (PRInt32 curItem = count - 1; curItem >= 0; curItem--) {
		    nsCOMPtr<nsISupports> supports;
        mContentChain->GetElementAt(curItem, getter_AddRefs(supports));
        nsCOMPtr<nsIContent> curContent = do_QueryInterface(supports);

		    //See if curContent is an ancestor of startContent.
		    if (IsAncestor(curContent, startContent, child))
		      return;
		  }
	  }
	}

	//mContent isn't actually put in the content chain, so we need to
	//check it separately.
	if (IsAncestor(mContent, startContent, child))
		return;

	*child = nsnull;
}


// if oldRowContent is an ancestor of rowContent, return true,
// and return the previous ancestor if requested
PRBool
nsXULTreeOuterGroupFrame::IsAncestor(nsIContent *aRowContent, nsIContent *aOldRowContent, nsIContent** firstDescendant)
{
  nsCOMPtr<nsIContent> prevContent;	
  nsCOMPtr<nsIContent> currContent = dont_QueryInterface(aOldRowContent);
  
  while (currContent) {
	  if (aRowContent == currContent.get()) {
		  if (firstDescendant) {
		    *firstDescendant = prevContent;
		    NS_IF_ADDREF(*firstDescendant);
		  }
		  return PR_TRUE;
	  }
    
	  prevContent = currContent;
	  prevContent->GetParent(*getter_AddRefs(currContent));
  }

  return PR_FALSE;
}

void
nsXULTreeOuterGroupFrame::CreateOldContentChain(nsIContent* aOldRowContent, nsIContent* topOfChain)
{
  nsCOMPtr<nsIContent> currContent = dont_QueryInterface(aOldRowContent);
  nsCOMPtr<nsIContent> prevContent;

  nsCOMPtr<nsIPresShell> shell;
  mPresContext->GetShell(getter_AddRefs(shell));

  //For each item between the (oldtoprow) and
  // (the new first child of common ancestry between new top row and old top row)
  // we need to see if the content chain has to be reset.
  while (currContent.get() != topOfChain) {
    nsIFrame* primaryFrame = nsnull;
    shell->GetPrimaryFrameFor(currContent, &primaryFrame);
      
    if (primaryFrame) {
      nsCOMPtr<nsIXULTreeSlice> slice(do_QueryInterface(primaryFrame));
      PRBool isRowGroup = PR_FALSE;
      if (slice)
        slice->IsGroupFrame(&isRowGroup);
    
	    if (isRowGroup) {
		    //Get the current content's parent's first child
		    nsCOMPtr<nsIContent> parent;
		    currContent->GetParent(*getter_AddRefs(parent));
		    
		    nsCOMPtr<nsIContent> firstChild;
		    parent->ChildAt(0, *getter_AddRefs(firstChild));

        nsIFrame* parentFrame;
        primaryFrame->GetParent(&parentFrame);
        PRBool isParentRowGroup = PR_FALSE;
        nsCOMPtr<nsIXULTreeSlice> slice(do_QueryInterface(parentFrame));
        if (slice)
          slice->IsGroupFrame(&isParentRowGroup);
    
	      if (isParentRowGroup) {
           //Get the current content's parent's first frame.
           nsXULTreeGroupFrame *parentRowGroupFrame =
               (nsXULTreeGroupFrame*)parentFrame;
		       nsIFrame *currentTopFrame = parentRowGroupFrame->GetFirstFrame();

			     nsCOMPtr<nsIContent> topContent;
			     currentTopFrame->GetContent(getter_AddRefs(topContent));

			     // If the current content's parent's first child is different
           // than the current frame's parent's first child then we know
           // they are out of synch and we need to set the content
           // chain correctly.
			     if(topContent.get() != firstChild.get()) {
				     nsCOMPtr<nsISupportsArray> contentChain;
				     NS_NewISupportsArray(getter_AddRefs(contentChain));
             contentChain->InsertElementAt(firstChild, 0);
             parentRowGroupFrame->SetContentChain(contentChain);
           }
        }
      }
    }

	  prevContent = currContent;
	  prevContent->GetParent(*getter_AddRefs(currContent));
  }
}

void
nsXULTreeOuterGroupFrame::FindRowContentAtIndex(PRInt32& aIndex,
                                                nsIContent* aParent,
                                                nsIContent** aResult)
{
  // Init to nsnull.
  *aResult = nsnull;

  // Walk over the tick array.
  if (mRowGroupInfo == nsnull)
    return;

  PRUint32 index = 0;
  PRUint32 arrayCount;
  mRowGroupInfo->mTickArray->Count(&arrayCount);
  nsCOMPtr<nsIContent> startContent;
  PRUint32 location = aIndex/TICK_FACTOR + 1;
  PRUint32 point = location*TICK_FACTOR;
  if (location >= arrayCount) {
    startContent = mRowGroupInfo->mLastChild;
    point = mRowGroupInfo->mRowCount-1;
  }
  else {
    nsCOMPtr<nsISupports> supp = getter_AddRefs(mRowGroupInfo->mTickArray->ElementAt(location));
    startContent = do_QueryInterface(supp);
  }

  if (!startContent) {
    NS_ERROR("The tree's tick array is confused!");
    return;
  }

  PRInt32 delta = (PRInt32)(point-aIndex);
  if (delta == 0) {
    *aResult = startContent;
    NS_IF_ADDREF(*aResult);
  }
  else FindPreviousRowContent(delta, startContent, nsnull, aResult);
}

void 
nsXULTreeOuterGroupFrame::FindPreviousRowContent(PRInt32& aDelta, nsIContent* aUpwardHint, 
                                                 nsIContent* aDownwardHint,
                                                 nsIContent** aResult)
{
  // Init to nsnull.
  *aResult = nsnull;

  // It disappoints me that this function is completely tied to the content nodes,
  // but I can't see any other way to handle this.  I don't have the frames, so I have nothing
  // else to fall back on but the content nodes.
  PRInt32 index = 0;
  nsCOMPtr<nsIContent> parentContent;
  if (aUpwardHint) {
    aUpwardHint->GetParent(*getter_AddRefs(parentContent));
    NS_ASSERTION(parentContent, "Parent content null in the upward hint of FPRC\n");
    if (!parentContent)
      return;
    parentContent->IndexOf(aUpwardHint, index);
  }
  else if (aDownwardHint) {
    parentContent = dont_QueryInterface(aDownwardHint);
    parentContent->ChildCount(index);
  }

  for (PRInt32 i = index-1; i >= 0; i--) {
    nsCOMPtr<nsIContent> childContent;
    parentContent->ChildAt(i, *getter_AddRefs(childContent));
    nsCOMPtr<nsIAtom> tag;
    childContent->GetTag(*getter_AddRefs(tag));
    if (tag == mTreeRowTag) {
      aDelta--;
      if (aDelta == 0) {
        *aResult = childContent;
        NS_IF_ADDREF(*aResult);
        return;
      }
    }
    else if (tag == mTreeItemTag) {
      // If it's open, descend into its treechildren node first.
      nsCOMPtr<nsIAtom> openAtom = dont_AddRef(NS_NewAtom("open"));
      nsAutoString isOpen;
      childContent->GetAttr(kNameSpaceID_None, openAtom, isOpen);
      if (isOpen.EqualsWithConversion("true")) {
        // Find the <treechildren> node.
        PRInt32 childContentCount;
        nsCOMPtr<nsIContent> grandChild;
        childContent->ChildCount(childContentCount);

        PRInt32 j;
        for (j = childContentCount-1; j >= 0; j--) {
          
          childContent->ChildAt(j, *getter_AddRefs(grandChild));
          nsCOMPtr<nsIAtom> grandChildTag;
          grandChild->GetTag(*getter_AddRefs(grandChildTag));
          if (grandChildTag == mTreeChildrenTag)
            break;
        }
        if (j >= 0 && grandChild)
          FindPreviousRowContent(aDelta, nsnull, grandChild, aResult);
      
        if (aDelta == 0)
          return;
      }

      // Descend into this row group and try to find a previous row.
      FindPreviousRowContent(aDelta, nsnull, childContent, aResult);
      if (aDelta == 0)
        return;
    }
  }

  NS_ASSERTION(parentContent, "Parent content null at the end of FPRC\n");
  if (!parentContent)
    return;

  nsCOMPtr<nsIAtom> tag;
  parentContent->GetTag(*getter_AddRefs(tag));
  if (tag && tag.get() == nsXULAtoms::tree) {
    // Hopeless. It ain't in there.
    return;
  }
  else if (!aDownwardHint) // We didn't find it here. We need to go up to our parent, using ourselves as a hint.
    FindPreviousRowContent(aDelta, parentContent, nsnull, aResult);

  // Bail. There's nothing else we can do.
}

void 
nsXULTreeOuterGroupFrame::FindNextRowContent(PRInt32& aDelta, nsIContent* aUpwardHint, 
                                             nsIContent* aDownwardHint,
                                             nsIContent** aResult)
{
  // Init to nsnull.
  *aResult = nsnull;

  // It disappoints me that this function is completely tied to the content nodes,
  // but I can't see any other way to handle this.  I don't have the frames, so I have nothing
  // else to fall back on but the content nodes.
  PRInt32 index = -1;
  nsCOMPtr<nsIContent> parentContent;
  if (aUpwardHint) {
    aUpwardHint->GetParent(*getter_AddRefs(parentContent));
    if (!parentContent) {
      NS_ERROR("Parent content should not be NULL!");
      return;
    }
    parentContent->IndexOf(aUpwardHint, index);
  }
  else if (aDownwardHint) {
    parentContent = dont_QueryInterface(aDownwardHint);
  }

  PRInt32 childCount;
  parentContent->ChildCount(childCount);
  for (PRInt32 i = index+1; i < childCount; i++) {
    nsCOMPtr<nsIContent> childContent;
    parentContent->ChildAt(i, *getter_AddRefs(childContent));
    nsCOMPtr<nsIAtom> tag;
    childContent->GetTag(*getter_AddRefs(tag));
    if (tag == mTreeRowTag) {
      aDelta--;
      if (aDelta == 0) {
        *aResult = childContent;
        NS_IF_ADDREF(*aResult);
        return;
      }
    }
    else if (tag == mTreeItemTag) {
      // Descend into this row group and try to find a next row.
      FindNextRowContent(aDelta, nsnull, childContent, aResult);
      if (aDelta == 0)
        return;
    }
    else if (tag == mTreeChildrenTag) {
      // If it's open, descend into its treechildren node first.
      nsCOMPtr<nsIAtom> openAtom = dont_AddRef(NS_NewAtom("open"));
      nsAutoString isOpen;
      parentContent->GetAttr(kNameSpaceID_None, openAtom, isOpen);
      if (isOpen.EqualsWithConversion("true")) {
        FindNextRowContent(aDelta, nsnull, childContent, aResult);
        if (aDelta == 0)
          return;
      }
    }
  }

  nsCOMPtr<nsIAtom> tag;
  parentContent->GetTag(*getter_AddRefs(tag));
  if (tag && tag.get() == nsXULAtoms::tree) {
    // Hopeless. It ain't in there.
    return;
  }
  else if (!aDownwardHint) // We didn't find it here. We need to go up to our parent, using ourselves as a hint.
    FindNextRowContent(aDelta, parentContent, nsnull, aResult);

  // Bail. There's nothing else we can do.
}

void
nsXULTreeOuterGroupFrame::EnsureRowIsVisible(PRInt32 aRowIndex)
{
  NS_ASSERTION(aRowIndex >= 0, "Ensure row is visible called with a negative number!");
  if (aRowIndex < 0)
    return;

  PRInt32 rows = 0;
  if (mRowHeight)
    rows = GetAvailableHeight()/mRowHeight;
  PRInt32 bottomIndex = mCurrentIndex + rows;
  
  // if row is visible, ignore
  if (mCurrentIndex <= aRowIndex && aRowIndex < bottomIndex)
    return;

  // Check to be sure we're not scrolling off the bottom of the tree
  PRInt32 delta;

  PRBool up = aRowIndex < mCurrentIndex;
  if (up) {
    delta = mCurrentIndex - aRowIndex;
    mCurrentIndex = aRowIndex;
  }
  else {
    // Bring it just into view.
    delta = 1 + (aRowIndex-bottomIndex);
    mCurrentIndex += delta; 
  }

  InternalPositionChanged(up, delta);
}

void
nsXULTreeOuterGroupFrame::ScrollToIndex(PRInt32 aRowIndex, PRBool aForceDestruct)
{
  if (( aRowIndex < 0 ) || (mRowHeight == 0))
    return;
    
  PRInt32 newIndex = aRowIndex;
  PRInt32 delta = mCurrentIndex > newIndex ? mCurrentIndex - newIndex : newIndex - mCurrentIndex;
  PRBool up = newIndex < mCurrentIndex;

  // Check to be sure we're not scrolling off the bottom of the tree
  PRInt32 lastPageTopRow = GetRowCount() - (GetAvailableHeight() / mRowHeight);
  if (lastPageTopRow < 0)
    lastPageTopRow = 0;

  if (aRowIndex > lastPageTopRow)
    return;

  mCurrentIndex = newIndex;
  InternalPositionChanged(up, delta, aForceDestruct);

  // This change has to happen immediately.
  // Flush any pending reflow commands.
  nsCOMPtr<nsIDocument> doc;
  mContent->GetDocument(*getter_AddRefs(doc));
  doc->FlushPendingNotifications();
}

// walks the DOM to get the zero-based row index of the current content
// note that aContent can be any element, this will get the index of the
// element's parent
NS_IMETHODIMP
nsXULTreeOuterGroupFrame::IndexOfItem(nsIContent* aRoot, nsIContent* aContent,
                                      PRBool aDescendIntoRows, // Invariant
                                      PRBool aParentIsOpen,
                                      PRInt32 *aResult)
{
  PRInt32 childCount=0;
  aRoot->ChildCount(childCount);

  nsresult rv;
  
  PRInt32 childIndex;
  for (childIndex=0; childIndex<childCount; childIndex++) {
    nsCOMPtr<nsIContent> child;
    aRoot->ChildAt(childIndex, *getter_AddRefs(child));
    
    nsCOMPtr<nsIAtom> childTag;
    child->GetTag(*getter_AddRefs(childTag));

    // is this it?
    if (child.get() == aContent)
      return NS_OK;

    // we hit a treerow, count it
    if (childTag == mTreeItemTag)
      (*aResult)++;
  
    PRBool descend = PR_TRUE;
    PRBool parentIsOpen = aParentIsOpen;

    // don't descend into closed children
    if (childTag == mTreeChildrenTag && !parentIsOpen)
      descend = PR_FALSE;

    // speed optimization - descend into rows only when told
    else if (childTag == mTreeRowTag && !aDescendIntoRows)
      descend = PR_FALSE;

    // descend as normally, but remember that the parent is closed!
    else if (childTag == mTreeItemTag) {
      nsAutoString isOpen;
      rv = child->GetAttr(kNameSpaceID_None, nsXULAtoms::open, isOpen);

      if (!isOpen.EqualsWithConversion("true"))
        parentIsOpen=PR_FALSE;
    }

    // now that we've analyzed the tags, recurse
    if (descend) {
      rv = IndexOfItem(child, aContent,
                       aDescendIntoRows, parentIsOpen, aResult);
      if (NS_SUCCEEDED(rv))
        return NS_OK;
    }
  }

  // not found
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULTreeOuterGroupFrame::EndBatch()
{
  NS_ASSERTION(mBatchCount, "EndBatch called on a tree that isn't batching!\n");
  if (mBatchCount == 0)
    return NS_OK;

  mBatchCount--;
  if (mBatchCount == 0) {
    if (mCurrentIndex == mOldIndex) {
      nsBoxLayoutState state(mPresContext);
      MarkDirtyChildren(state);
    }
    else ScrollToIndex(mCurrentIndex, PR_TRUE);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeOuterGroupFrame::ReflowFinished(nsIPresShell* aPresShell, PRBool* aFlushFlag)
{
  // Now dirty the world.
  nsCOMPtr<nsIContent> tree;
  GetTreeContent(getter_AddRefs(tree));
  
  nsIFrame* treeFrame;
  aPresShell->GetPrimaryFrameFor(tree, &treeFrame);

  nsCOMPtr<nsIBox> treeBox(do_QueryInterface(treeFrame));

  nsBoxLayoutState state(mPresContext);

  // now build or destroy any needed rows.
  nsCOMPtr<nsIBoxLayout> layout;
  GetLayoutManager(getter_AddRefs(layout));
  nsTreeLayout* treeLayout = (nsTreeLayout*)layout.get();
  treeLayout->LazyRowCreator(state, this);

  if (mAdjustScroll) {
     VerticalScroll(mYPosition);
     mAdjustScroll = PR_FALSE;
  }

  // if the row height changed
  // then mark everything as a style change. That
  // will dirty the tree all the way to its leaves.
  if (mRowHeightWasSet) {
     if (!treeBox)
        return NS_ERROR_NULL_POINTER;
	 treeBox->MarkStyleChange(state);
     PRInt32 pos = mCurrentIndex*mRowHeight;
     if (mYPosition != pos) 
       mAdjustScroll = PR_TRUE;

    mRowHeightWasSet = PR_FALSE;
  }

  mReflowCallbackPosted = PR_FALSE;

  *aFlushFlag = PR_TRUE;
  
  return NS_OK;
}

void
nsXULTreeOuterGroupFrame::RegenerateRowGroupInfo(PRBool aOnScreenCount)
{ 
  NeedsRecalc(); 
  
  PRInt32 oldRowCount = GetRowCount();
  if (mRowGroupInfo) 
    mRowGroupInfo->Clear(); 
  PRInt32 newRowCount = GetRowCount();

  if (mRowHeight <= 0)
      return;

  // For removal only, we need to know how many rows are onscreen.  These subtract
  // from the amount that we need to adjust.  For example, if the tree widget is
  // scrolled to index 40, and if a folder that is offscreen at index 0
  // is deleted, and it contains 9 kids, then a total of 10 rows are vanishing.  
  // newRowCount - oldRowCount will be -10 following the removal.  However, if 4 of those
  // 10 rows were onscreen, then the tree widget's index only needs to be adjusted by
  // -6 (-10 + 4).  
  PRInt32 delta = newRowCount-oldRowCount+aOnScreenCount;
  PRInt32 newIndex = mCurrentIndex + delta;
  PRBool adjust = PR_FALSE;

  // Check to be sure we're not scrolling off the bottom of the tree
  PRInt32 lastPageTopRow = newRowCount - (GetAvailableHeight() / mRowHeight);
  if (lastPageTopRow < 0) {
    if (aOnScreenCount > 0)
      adjust = PR_TRUE;
    lastPageTopRow = 0;
  }
  
  if (newIndex > lastPageTopRow) { 
    newIndex = lastPageTopRow;
    if (aOnScreenCount > 0)
      adjust = PR_TRUE;
  }

  if (newIndex < 0)
    newIndex = 0;

  if (!adjust) {
    if (mCurrentIndex == 0 || delta == 0)
      return; // Just a simple update or we aren't scrolled, so bail.

    nsCOMPtr<nsIContent> row;
    GetFirstRowContent(getter_AddRefs(row));
    NS_ASSERTION(row, "No row in regen check!");
    if (!row)
      return;

    // An element was passed in that was either removed or added.
    // We need to adjust our scroll position if this element's index
    // is < our current scrolled index.
    PRInt32 index = 0;
    nsCOMPtr<nsIContent> item;
    row->GetParent(*getter_AddRefs(item));
    IndexOfItem(mContent, item, PR_FALSE, PR_TRUE, &index);
    if (index == -1 || index == mCurrentIndex)
      return;
  }
  
  // We mark the outer row group dirty and force a comprehensive
  // rebuild of all tree widget frames.  This ensures that we
  // stay precisely in sync whenever we lose content from above.
  if (IsBatching())
    mCurrentIndex = newIndex;
  else ScrollToIndex(newIndex, !adjust);

  if (adjust) {
    // Force a full redraw.
    nsBoxLayoutState state(mPresContext);
    Redraw(state, nsnull, PR_FALSE);
  }
}

void
nsXULTreeOuterGroupFrame::GetTreeContent(nsIContent** aResult)
{
  nsCOMPtr<nsIContent> content(mContent);
  nsCOMPtr<nsIContent> bindingParent;
  mContent->GetBindingParent(getter_AddRefs(bindingParent));
  if (bindingParent) {
    nsCOMPtr<nsIDocument> doc;
    bindingParent->GetDocument(*getter_AddRefs(doc));
    nsCOMPtr<nsIBindingManager> bindingManager;
    doc->GetBindingManager(getter_AddRefs(bindingManager));
    nsCOMPtr<nsIAtom> tag;
    PRInt32 namespaceID;
    bindingManager->ResolveTag(bindingParent, &namespaceID, getter_AddRefs(tag));
    if (tag.get() == nsXULAtoms::tree) {
      *aResult = bindingParent;
      NS_ADDREF(*aResult);
    }
  }
  else 
    mContent->GetParent(*aResult); // method does the addref
}


//
// Paint
//
// Overridden to handle the case where we should be drawing the tree as sorted.
//
NS_IMETHODIMP
nsXULTreeOuterGroupFrame :: Paint ( nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext,
                                      const nsRect& aDirtyRect, nsFramePaintLayer aWhichLayer)
{
  nsresult res = NS_OK;
  
  res = nsBoxFrame::Paint ( aPresContext, aRenderingContext, aDirtyRect, aWhichLayer );
  
  if ( (aWhichLayer == eFramePaintLayer_Content) &&
        (mYDropLoc != nsTreeItemDragCapturer::kNoDropLoc || mDropOnContainer || mTreeIsSorted) )
    PaintDropFeedback ( aPresContext, aRenderingContext, PR_TRUE );

  return res;

} // Paint


//
// AttributeChanged
//
// Track several attributes set by the d&d drop feedback tracking mechanism, notably
// telling us to paint as sorted
//
NS_IMETHODIMP
nsXULTreeOuterGroupFrame :: AttributeChanged ( nsIPresContext* aPresContext, nsIContent* aChild,
                                                 PRInt32 aNameSpaceID, nsIAtom* aAttribute, 
                                                 PRInt32 aModType, PRInt32 aHint)
{
  nsresult rv = NS_OK;
   
  if ( aAttribute == nsXULAtoms::ddTriggerRepaintSorted ) {
    // Set a flag so that children won't draw the drop feedback but the parent
    // will.
    mTreeIsSorted = PR_TRUE;
    ForceDrawFrame ( aPresContext, this );
    mTreeIsSorted = PR_FALSE;
  }
  else
    rv = nsXULTreeGroupFrame::AttributeChanged ( aPresContext, aChild, aNameSpaceID, aAttribute, aModType, aHint );

  return rv;
 
} // AttributeChanged



#ifdef XP_MAC
#pragma mark -
#endif



//
// HandleAutoScrollTracking
//
//
nsresult
nsXULTreeOuterGroupFrame :: HandleAutoScrollTracking ( const nsPoint & aPoint )
{
#if USE_TIMER_TO_DELAY_SCROLLING
// if we're in the scroll region and there is no timer, start one and return
// if the timer has started but not yet fired, do nothing
// if the timer has fired, scroll the direction it tells us.
  PRBool scrollUp = PR_FALSE;
  if ( IsInDragScrollRegion(aPoint, &scrollUp) ) {
    if ( mAutoScrollTimerStarted ) {
printf("waiting...\n");
      if ( mAutoScrollTimerHasFired ) {
        // we're in the right area and the timer has fired, so scroll
        ScrollToIndex ( scrollUp ? mCurrentIndex - 1 : mCurrentIndex + 1);
      }
    }
    else {
printf("starting timer\n");
      // timer hasn't started yet, kick it off. Remember, this timer is a one-shot
      mAutoScrollTimer = do_CreateInstance("@mozilla.org/timer;1");
      if ( mAutoScrollTimer ) {
        mAutoScrollTimer->Init(this, 100);
        mAutoScrollTimerStarted = PR_TRUE;
      }
      else
          return NS_ERROR_FAILURE;
    }
  } // we're in an area we care about
  else
    StopScrollTracking();
    
#else

  PRBool scrollUp = PR_FALSE;
  if ( IsInDragScrollRegion(aPoint, &scrollUp) )
    ScrollToIndex ( scrollUp ? mCurrentIndex - 1 : mCurrentIndex + 1);
    
#endif

  return NS_OK;

} // HandleAutoScrollTracking


//
// IsInDragScrollRegion
//
// Determine if the current point is inside one of the scroll regions near the top
// or bottom of the tree and will return PR_TRUE if it is, PR_FALSE if it isn't. 
// If it is, this will set |outScrollUp| to PR_TRUE if the tree should scroll up,
// PR_FALSE if it should scroll down.
//
PRBool
nsXULTreeOuterGroupFrame :: IsInDragScrollRegion ( const nsPoint& inPoint, PRBool* outScrollUp )
{
  PRBool isInRegion = PR_FALSE;

  float pixelsToTwips = 0.0;
  mPresContext->GetPixelsToTwips ( &pixelsToTwips );
  nsPoint mouseInTwips ( NSToIntRound(inPoint.x * pixelsToTwips), NSToIntRound(inPoint.y * pixelsToTwips) );
  
  // compute the offset to top level in twips and subtract the offset from
  // the mouse coord to put it into our coordinates.
  nscoord frameOffsetX = 0, frameOffsetY = 0;
  nsIFrame* curr = this;
  curr->GetParent(&curr);
  while ( curr ) {
    nsPoint origin;
    curr->GetOrigin(origin);      // in twips    
    frameOffsetX += origin.x;     // build the offset incrementally
    frameOffsetY += origin.y;    
    curr->GetParent(&curr);       // moving up the chain
  } // until we reach the top  
  mouseInTwips.MoveBy ( -frameOffsetX, -frameOffsetY );

  const int kMarginHeight = NSToIntRound ( 12 * pixelsToTwips );

  // scroll if we're inside the bounds of the tree and w/in |kMarginHeight|
  // from the top or bottom of the tree.
  if ( mRect.Contains(mouseInTwips) ) {
    if ( mouseInTwips.y <= kMarginHeight ) {
      isInRegion = PR_TRUE;
      if ( outScrollUp )
        *outScrollUp = PR_TRUE;      // scroll up
    }
    else if ( mouseInTwips.y > GetAvailableHeight() - kMarginHeight ) {
      isInRegion = PR_TRUE;
      if ( outScrollUp )
        *outScrollUp = PR_FALSE;     // scroll down
    }
  }
  
  return isInRegion;
  
} // IsInDragScrollRegion


#if USE_TIMER_TO_DELAY_SCROLLING

//
// Notify
//
// Called when the timer fires. Tells the tree that we're ready to scroll if the 
// mouse is in the right position.
//
void
nsXULTreeOuterGroupFrame :: Notify ( nsITimer* timer )
{
printf("000 FIRE\n");
  mAutoScrollTimerHasFired = PR_TRUE;
}

nsresult
nsXULTreeOuterGroupFrame :: StopScrollTracking()
{
printf("--stop\n");
  if ( mAutoScrollTimer && mAutoScrollTimerStarted )
    mAutoScrollTimer->Cancel();
  mAutoScrollTimerHasFired = PR_FALSE;
  mAutoScrollTimerStarted = PR_FALSE;
  
  return NS_OK;
}

#endif

NS_IMETHODIMP 
nsXULTreeOuterGroupFrame::SizeTo(nsIPresContext* aPresContext, nscoord aWidth, nscoord aHeight)
{
  return nsXULTreeGroupFrame::SizeTo(aPresContext, aWidth, aHeight);
}

class nsTreeScrollPortFrame : public nsScrollPortFrame
{
public:
  nsTreeScrollPortFrame(nsIPresShell* aShell);
  friend nsresult NS_NewScrollBoxFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
  NS_IMETHOD GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetMinSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
};

// Scrollport Subclass
nsresult
NS_NewTreeScrollPortFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTreeScrollPortFrame* it = new (aPresShell) nsTreeScrollPortFrame (aPresShell);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsTreeScrollPortFrame::nsTreeScrollPortFrame(nsIPresShell* aShell):nsScrollPortFrame(aShell)
{
}

NS_IMETHODIMP
nsTreeScrollPortFrame::GetMinSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{  
  nsIBox* child = nsnull;
  GetChildBox(&child);
 
  nsresult rv = child->GetPrefSize(aBoxLayoutState, aSize);
  nsXULTreeOuterGroupFrame* outer = NS_STATIC_CAST(nsXULTreeOuterGroupFrame*,child);

  nsAutoString sizeMode;
  nsCOMPtr<nsIContent> content;
  outer->GetContent(getter_AddRefs(content));
  content->GetAttr(kNameSpaceID_None, nsXULAtoms::sizemode, sizeMode);
  if (!sizeMode.IsEmpty()) {  
    nsCOMPtr<nsIScrollableFrame> scrollFrame(do_QueryInterface(mParent));
    if (scrollFrame) {
      nsIScrollableFrame::nsScrollPref scrollPref;
      scrollFrame->GetScrollPreference(aBoxLayoutState.GetPresContext(), &scrollPref);

      if (scrollPref == nsIScrollableFrame::Auto) {
        nscoord vbarwidth, hbarheight;
        scrollFrame->GetScrollbarSizes(aBoxLayoutState.GetPresContext(),
                                       &vbarwidth, &hbarheight);
        aSize.width += vbarwidth;
      }
    }
  }
  else aSize.width = 0;

  aSize.height = 0;
  
  AddMargin(child, aSize);
  AddBorderAndPadding(aSize);
  AddInset(aSize);
  nsIBox::AddCSSMinSize(aBoxLayoutState, this, aSize);
  return rv;

}

NS_IMETHODIMP
nsTreeScrollPortFrame::GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{  
  nsIBox* child = nsnull;
  GetChildBox(&child);
 
  nsresult rv = child->GetPrefSize(aBoxLayoutState, aSize);
  nsXULTreeOuterGroupFrame* outer = NS_STATIC_CAST(nsXULTreeOuterGroupFrame*,child);

  PRInt32 size = outer->GetFixedRowSize();

  if (size > -1)
    aSize.height = size*outer->GetRowHeightTwips();
   
  nsCOMPtr<nsIScrollableFrame> scrollFrame(do_QueryInterface(mParent));
  if (scrollFrame) {
    nsIScrollableFrame::nsScrollPref scrollPref;
    scrollFrame->GetScrollPreference(aBoxLayoutState.GetPresContext(), &scrollPref);

    if (scrollPref == nsIScrollableFrame::Auto) {
      nscoord vbarwidth, hbarheight;
      scrollFrame->GetScrollbarSizes(aBoxLayoutState.GetPresContext(),
                                     &vbarwidth, &hbarheight);
      aSize.width += vbarwidth;
    }
  }

  AddMargin(child, aSize);
  AddBorderAndPadding(aSize);
  AddInset(aSize);
  nsIBox::AddCSSPrefSize(aBoxLayoutState, this, aSize);
  return rv;

}

nscoord
nsXULTreeOuterGroupFrame::ComputeIntrinsicWidth(nsBoxLayoutState& aBoxLayoutState)
{
  if (mStringWidth != -1)
    return mStringWidth;

  nscoord largestWidth = 0;

  nsCOMPtr<nsIContent> firstRowContent;
  nsCOMPtr<nsIContent> content;
  GetTreeContent(getter_AddRefs(content));
  PRInt32 index = 0;
  FindRowContentAtIndex(index, content, getter_AddRefs(firstRowContent));

  if (firstRowContent) {
    nsCOMPtr<nsIStyleContext> styleContext;
    aBoxLayoutState.GetPresContext()->ResolveStyleContextFor(firstRowContent, nsnull,
                                                             PR_FALSE, 
                                                             getter_AddRefs(styleContext));

    nscoord width = 0;
    nsMargin margin(0,0,0,0);

    nsStyleBorderPadding  bPad;
    styleContext->GetBorderPaddingFor(bPad);
    bPad.GetBorderPadding(margin);

    width += (margin.left + margin.right);

    const nsStyleMargin* styleMargin = (const nsStyleMargin*)styleContext->GetStyleData(eStyleStruct_Margin);
    styleMargin->GetMargin(margin);
    width += (margin.left + margin.right);

    nsCOMPtr<nsIContent> content;
    GetTreeContent(getter_AddRefs(content));

    PRInt32 childCount;
    content->ChildCount(childCount);

    nsCOMPtr<nsIContent> child;
    for (PRInt32 i = 0; i < childCount && i < 100; ++i) {
      content->ChildAt(i, *getter_AddRefs(child));

      nsCOMPtr<nsIAtom> tag;
      child->GetTag(*getter_AddRefs(tag));
      if (tag == mTreeRowTag) {
        nsIPresContext* presContext = aBoxLayoutState.GetPresContext();
        nsIRenderingContext* rendContext = aBoxLayoutState.GetReflowState()->rendContext;
        if (rendContext) {
          nsAutoString value;
          nsCOMPtr<nsIContent> textChild;
          PRInt32 textCount;
          child->ChildCount(textCount);
          for (PRInt32 j = 0; j < textCount; ++j) {
            child->ChildAt(j, *getter_AddRefs(textChild));
            nsCOMPtr<nsIDOMText> text(do_QueryInterface(textChild));
            if (text) {
              nsAutoString data;
              text->GetData(data);
              value += data;
            }
          }
          const nsStyleFont* font = (const nsStyleFont*)styleContext->GetStyleData(eStyleStruct_Font);
          nsCOMPtr<nsIDeviceContext> dc;
          presContext->GetDeviceContext(getter_AddRefs(dc));
          nsCOMPtr<nsIFontMetrics> fm;
          dc->GetMetricsFor(font->mFont, *getter_AddRefs(fm));
          rendContext->SetFont(fm);

          nscoord textWidth;
          rendContext->GetWidth(value, textWidth);
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
