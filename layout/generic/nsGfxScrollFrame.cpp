/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsCOMPtr.h"
#include "nsHTMLParts.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsIReflowCommand.h"
#include "nsIDeviceContext.h"
#include "nsPageFrame.h"
#include "nsViewsCID.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsHTMLContainerFrame.h"
#include "nsHTMLIIDs.h"
#include "nsCSSRendering.h"
#include "nsIScrollableView.h"
#include "nsWidgetsCID.h"
#include "nsIAreaFrame.h"
#include "nsGfxScrollFrame.h"
#include "nsLayoutAtoms.h"
#include "nsIWebShell.h"
#include "nsIXMLContent.h"
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsISupportsArray.h"
#include "nsIDocument.h"
#include "nsIFontMetrics.h"

static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);

static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);
static NS_DEFINE_IID(kScrollingViewCID, NS_SCROLLING_VIEW_CID);
static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);

static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);
static NS_DEFINE_IID(kScrollViewIID, NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kAreaFrameIID, NS_IAREAFRAME_IID);

static NS_DEFINE_IID(kIAnonymousContentCreatorIID,     NS_IANONYMOUS_CONTENT_CREATOR_IID);

//----------------------------------------------------------------------

nsresult
NS_NewGfxScrollFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsGfxScrollFrame* it = new nsGfxScrollFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsGfxScrollFrame::nsGfxScrollFrame():mHScrollbarFrame(nsnull),
                                     mVScrollbarFrame(nsnull),
                                     mScrollAreaFrame(nsnull),
                                     mHasVerticalScrollbar(PR_FALSE), 
                                     mHasHorizontalScrollbar(PR_FALSE),
                                     mOnePixel(20)
{
}

nsresult NS_CreateAnonymousNode(nsIContent* aParent, nsIAtom* aTag, PRInt32 aNameSpaceId, nsCOMPtr<nsIContent>& aNewNode);

NS_IMETHODIMP
nsGfxScrollFrame::CreateAnonymousContent(nsISupportsArray& aAnonymousChildren)
{
  
  /*
  nsCOMPtr<nsIDocument> idocument;
  mContent->GetDocument(*getter_AddRefs(idocument));

  // create horzontal scrollbar
  nsCOMPtr<nsIXMLContent> content;
  nsresult rv = NS_NewXMLElement(getter_AddRefs(content), nsXULAtoms::titledbutton);
  content->SetDocument(idocument, PR_FALSE);
  content->SetNameSpaceID(nsXULAtoms::nameSpaceID);
  content->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::value, "foo", PR_FALSE);
  aAnonymousChildren.AppendElement(content);

  // create horzontal scrollbar
  rv = NS_NewXMLElement(getter_AddRefs(content), nsXULAtoms::titledbutton);
  content->SetDocument(idocument, PR_FALSE);
  content->SetNameSpaceID(nsXULAtoms::nameSpaceID);
  content->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::value, "foo", PR_FALSE);
  aAnonymousChildren.AppendElement(content);
*/

  /* Comment this in to enable GFX scrollbars */

  // create horzontal scrollbar
  nsCOMPtr<nsIContent> content;
  NS_CreateAnonymousNode(mContent, nsXULAtoms::scrollbar, nsXULAtoms::nameSpaceID, content);
  content->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::align, "horizontal", PR_FALSE);
  aAnonymousChildren.AppendElement(content);

  // create vertical scrollbar
  NS_CreateAnonymousNode(mContent, nsXULAtoms::scrollbar, nsXULAtoms::nameSpaceID, content);
  content->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::align, "vertical", PR_FALSE);
  aAnonymousChildren.AppendElement(content);

      // XXX For GFX never have scrollbars
 // mScrollableView->SetScrollPreference(nsScrollPreference_kNeverScroll);
  
  return NS_OK;
}

NS_IMETHODIMP
nsGfxScrollFrame::Destroy(nsIPresContext& aPresContext)

{
  return nsHTMLContainerFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
nsGfxScrollFrame::Init(nsIPresContext&  aPresContext,
                    nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIStyleContext* aStyleContext,
                    nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsHTMLContainerFrame::Init(aPresContext, aContent,
                                            aParent, aStyleContext,
                                            aPrevInFlow);
  nsCOMPtr<nsIDocument> document;
  mContent->GetDocument(*getter_AddRefs(document));
  document->AddObserver(this);
  return rv;
}
  
NS_IMETHODIMP
nsGfxScrollFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                   nsIAtom*        aListName,
                                   nsIFrame*       aChildList)
{
  nsresult  rv = nsHTMLContainerFrame::SetInitialChildList(aPresContext, aListName,
                                                           aChildList);
  // get scroll area
  mScrollAreaFrame = mFrames.FirstChild();

  // set make sure it never shows scrollbars because we are going to do the
  // the scrollbars.
  GetScrollableView()->SetScrollPreference(nsScrollPreference_kNeverScroll);

  // horizontal scrollbar
  mScrollAreaFrame->GetNextSibling(&mHScrollbarFrame);

  // vertical scrollbar
  mHScrollbarFrame->GetNextSibling(&mVScrollbarFrame);

  return rv;
}

nsIScrollableView*
nsGfxScrollFrame::GetScrollableView()
{
  nsIScrollableView* scrollingView;
  nsIView*           view;
  mScrollAreaFrame->GetView(&view);
  nsresult result = view->QueryInterface(kScrollViewIID, (void**)&scrollingView);
  NS_ASSERTION(NS_SUCCEEDED(result), "assertion gfx scrollframe does not contain a scrollframe");          
  return scrollingView;
}

void
nsGfxScrollFrame::GetScrolledContentSize(nsSize& aSize)
{
    // get the ara frame is the scrollarea
    nsIFrame* child = nsnull;
    mScrollAreaFrame->FirstChild(nsnull, &child);

    nsRect rect(0,0,0,0);
    child->GetRect(rect);
    aSize.width = rect.width;
    aSize.height = rect.height;
}

NS_IMETHODIMP
nsGfxScrollFrame::AppendFrames(nsIPresContext& aPresContext,
                            nsIPresShell&   aPresShell,
                            nsIAtom*        aListName,
                            nsIFrame*       aFrameList)
{
  // Only one child frame allowed
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGfxScrollFrame::InsertFrames(nsIPresContext& aPresContext,
                            nsIPresShell&   aPresShell,
                            nsIAtom*        aListName,
                            nsIFrame*       aPrevFrame,
                            nsIFrame*       aFrameList)
{
  // Only one child frame allowed
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGfxScrollFrame::RemoveFrame(nsIPresContext& aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aOldFrame)
{
  // Scroll frame doesn't support incremental changes
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGfxScrollFrame::DidReflow(nsIPresContext&   aPresContext,
                         nsDidReflowStatus aStatus)
{
    // Let the default nsFrame implementation clear the state flags
    // and size and position our view
  nsresult rv = nsHTMLContainerFrame::DidReflow(aPresContext, aStatus);
    
  return rv;
}


NS_IMETHODIMP
nsGfxScrollFrame::Reflow(nsIPresContext&          aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
                     ("enter nsGfxScrollFrame::Reflow: maxSize=%d,%d",
                      aReflowState.availableWidth,
                      aReflowState.availableHeight));

  // assume we need to reflow nothing
  PRBool hscrollbarNeedsReflow  = PR_FALSE;
  PRBool vscrollbarNeedsReflow  = PR_FALSE;
  PRBool scrollAreaNeedsReflow = PR_FALSE;

  nsIFrame* incrementalChild = nsnull;

  // see what things need reflow
  DetermineReflowNeed(aPresContext,
                      aDesiredSize,
                      aReflowState,
                      aStatus,
                      hscrollbarNeedsReflow, 
                      vscrollbarNeedsReflow, 
                      scrollAreaNeedsReflow,
                      incrementalChild);
 
  // next reflow the scrollbars so we will at least know their size. If they are bigger or smaller than
  // we thought we will have to reflow the scroll area as well.
  ReflowScrollbars(   aPresContext,
                      aReflowState,
                      aStatus,
                      hscrollbarNeedsReflow, 
                      vscrollbarNeedsReflow,
                      scrollAreaNeedsReflow,
                      incrementalChild);
  
  // then reflow the scroll area. If it gets bigger it could signal the scrollbars need
  // to be reflowed.
  ReflowScrollArea(   aPresContext,
                      aDesiredSize,
                      aReflowState,
                      aStatus,
                      hscrollbarNeedsReflow, 
                      vscrollbarNeedsReflow, 
                      scrollAreaNeedsReflow,
                      incrementalChild);

  // reflow the scrollbars again but only the ones that need it.
  ReflowScrollbars(   aPresContext,
                      aReflowState,
                      aStatus,
                      hscrollbarNeedsReflow, 
                      vscrollbarNeedsReflow, 
                      scrollAreaNeedsReflow,
                      incrementalChild);

  // layout all the children.
  LayoutChildren( aPresContext,
                  aDesiredSize,
                  aReflowState);

    // redraw anything that needs it.
  if ( aReflowState.reason == eReflowReason_Incremental ) 
      if (hscrollbarNeedsReflow && vscrollbarNeedsReflow && scrollAreaNeedsReflow)
          Invalidate(nsRect(0,0,mRect.width,mRect.height), PR_FALSE);

  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
    ("exit nsGfxScrollFrame::Reflow: status=%d width=%d height=%d",
     aStatus, aDesiredSize.width, aDesiredSize.height));
  return NS_OK;
}


nsresult
nsGfxScrollFrame::SetFrameSize(   nsIFrame* aFrame,
                               nsSize aSize)
                               
{  
    // get the margin
    const nsStyleSpacing* spacing;
    nsresult rv = aFrame->GetStyleData(eStyleStruct_Spacing,
                   (const nsStyleStruct*&) spacing);

    nsMargin margin;
    if (!spacing->GetMargin(margin)) 
      margin.SizeTo(0,0,0,0);

    // add in the margin
    aSize.width -= margin.left + margin.right;
    aSize.height -= margin.top + margin.bottom;

    aFrame->SizeTo(aSize.width, aSize.height);

    return NS_OK;
}


nsresult
nsGfxScrollFrame::GetFrameSize(   nsIFrame* aFrame,
                               nsSize& aSize)
                               
{  
    // get the child's rect
    aFrame->GetSize(aSize);

    // get the margin
    const nsStyleSpacing* spacing;
    nsresult rv = aFrame->GetStyleData(eStyleStruct_Spacing,
                   (const nsStyleStruct*&) spacing);

    nsMargin margin;
    if (!spacing->GetMargin(margin)) 
      margin.SizeTo(0,0,0,0);

    // add in the margin
    aSize.width += margin.left + margin.right;
    aSize.height += margin.top + margin.bottom;

    return NS_OK;
}


// Returns the width of the vertical scrollbar and the height of
// the horizontal scrollbar in twips
void
nsGfxScrollFrame::GetScrollbarDimensions(nsIPresContext& aPresContext,
                                      nsSize&        aSbSize)
{
                       
  // if we are using 
    nsRect rect(0,0,0,0);
    mHScrollbarFrame->GetRect(rect);
    aSbSize.height = rect.height;

    rect.SetRect(0,0,0,0);
    mVScrollbarFrame->GetRect(rect);
    aSbSize.width = rect.width;
    return;
}


// Calculates the size of the scroll area. This is the area inside of the
// border edge and inside of any vertical and horizontal scrollbar
// Also returns whether space was reserved for the vertical scrollbar.
nsresult
nsGfxScrollFrame::CalculateScrollAreaSize(nsIPresContext&          aPresContext,
                                       const nsHTMLReflowState& aReflowState,
                                       const nsSize&            aSbSize,
                                       nsSize&                  aScrollAreaSize)
{
  aScrollAreaSize.width = aReflowState.mComputedWidth;
  aScrollAreaSize.height = aReflowState.mComputedHeight;

  // get the current state
  PRBool vert = mHasVerticalScrollbar;
  PRBool horiz = mHasHorizontalScrollbar;

  // reset everyting
  mHasVerticalScrollbar = PR_FALSE;
  mHasHorizontalScrollbar = PR_FALSE;

  if (aReflowState.mStyleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLL) 
  {
    vert = PR_TRUE;
    horiz = PR_TRUE;
  } 

  // add in the scrollbars.
  if (horiz)
    AddHorizontalScrollbar(aSbSize, aScrollAreaSize);
  
  if (vert)
    AddVerticalScrollbar(aSbSize, aScrollAreaSize);



  return NS_OK;
}

// Calculate the total amount of space needed for the child frame,
// including its child frames that stick outside its bounds and any
// absolutely positioned child frames.
// Updates the width/height members of the reflow metrics
nsresult
nsGfxScrollFrame::CalculateChildTotalSize(nsIFrame*            aKidFrame,
                                       nsHTMLReflowMetrics& aKidReflowMetrics)
{
  // If the frame has child frames that stick outside its bounds, then take
  // them into account, too
  nsFrameState  kidState;
  aKidFrame->GetFrameState(&kidState);
  if (NS_FRAME_OUTSIDE_CHILDREN & kidState) {
    aKidReflowMetrics.width = aKidReflowMetrics.mCombinedArea.width;
    aKidReflowMetrics.height = aKidReflowMetrics.mCombinedArea.height;
  }

  // If it's an area frame, then get the total size which includes the
  // space taken up by absolutely positioned child elements
  nsIAreaFrame* areaFrame;
  if (NS_SUCCEEDED(aKidFrame->QueryInterface(kAreaFrameIID, (void**)&areaFrame))) {
    nscoord xMost, yMost;

    areaFrame->GetPositionedInfo(xMost, yMost);
    if (xMost > aKidReflowMetrics.width) {
      aKidReflowMetrics.width = xMost;
    }
    if (yMost > aKidReflowMetrics.height) {
      aKidReflowMetrics.height = yMost;
    }
  }

  return NS_OK;
}

void
nsGfxScrollFrame::SetScrollbarVisibility(nsIFrame* aScrollbar, PRBool aVisible)
{
        nsString oldStyle = "";
        nsCOMPtr<nsIContent> child;
        aScrollbar->GetContent(getter_AddRefs(child));
        child->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::value, oldStyle);

        nsString newStyle;
        if (aVisible)
          newStyle = "";
        else
          newStyle = "hidden";

        if (oldStyle != newStyle)
            child->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::value, newStyle, PR_TRUE);
}

/**
 * Determine which of our children need reflow. It also return the child that is being 
 * incrementally reflowed if there is one.
 */
void
nsGfxScrollFrame::DetermineReflowNeed(nsIPresContext&          aPresContext,
                                   nsHTMLReflowMetrics&     aDesiredSize,
                                   const nsHTMLReflowState& aReflowState,
                                   nsReflowStatus&          aStatus,
                                   PRBool&                  aHscrollbarNeedsReflow, 
                                   PRBool&                  aVscrollbarNeedsReflow, 
                                   PRBool&                  aScrollAreaNeedsReflow, 
                                   nsIFrame*&               aIncrementalChild) 
{
  nsIFrame* targetFrame;

  // Special handling for incremental reflow
  if (eReflowReason_Incremental == aReflowState.reason) {
    // See whether we're the target of the reflow command
    aReflowState.reflowCommand->GetTarget(targetFrame);
    if (this == targetFrame) {
      nsIReflowCommand::ReflowType  type;

      // The only type of reflow command we expect to get is a style
      // change reflow command
      aReflowState.reflowCommand->GetType(type);
      NS_ASSERTION(nsIReflowCommand::StyleChanged == type, "unexpected reflow type");

      // Make a copy of the reflow state (with a different reflow reason) and
      // then recurse
      nsHTMLReflowState reflowState(aReflowState);
      reflowState.reason = eReflowReason_StyleChange;
      reflowState.reflowCommand = nsnull;
      Reflow(aPresContext, aDesiredSize, reflowState, aStatus);
    }

    // Get the next frame in the reflow chain this will be one of the scrollbars.
    aReflowState.reflowCommand->GetNext(aIncrementalChild);
    
    // see which scrollbar it is and signal that is needs reflow
    if (aIncrementalChild == mHScrollbarFrame)
      aHscrollbarNeedsReflow = PR_TRUE;
    else if (aIncrementalChild == mVScrollbarFrame)
      aVscrollbarNeedsReflow = PR_TRUE;
    else if (aIncrementalChild == mScrollAreaFrame)
      aScrollAreaNeedsReflow = PR_TRUE;

  } else {
    // if not incremental reflow then reflow everyone one.
    aHscrollbarNeedsReflow = PR_TRUE;
    aVscrollbarNeedsReflow = PR_TRUE;
    aScrollAreaNeedsReflow = PR_TRUE;
  }
}


/**
 * Reflows the visible scrollbars to fit in our content area. If the scrollbars change size
 * Signal that the content area needs to be reflowed to accomidate the new size. This can
 * happend if a css rule like :hover causes the scrollbar to get bigger. 
 */
void
nsGfxScrollFrame::ReflowScrollbars(   nsIPresContext&         aPresContext,
                                   const nsHTMLReflowState& aReflowState,
                                   nsReflowStatus&          aStatus,
                                   PRBool&                  aHscrollbarNeedsReflow, 
                                   PRBool&                  aVscrollbarNeedsReflow, 
                                   PRBool&                  aScrollBarResized, 
                                   nsIFrame*&               aIncrementalChild)
{
  // if we have gfx scrollbars reflow them. If they changed size then signal that 
  // we need to reflow the content area. This could happen if there is a style rule set
  // up on them that changed there size. Like an image on the thumb gets big.
    if (aHscrollbarNeedsReflow)
    {
      ReflowScrollbar(aPresContext, 
                      aReflowState, 
                      aStatus, 
                      aScrollBarResized,
                      mHScrollbarFrame, 
                      aIncrementalChild);

      if (aScrollBarResized)
          aVscrollbarNeedsReflow = PR_TRUE;

    } 
    
    if (aVscrollbarNeedsReflow)
    {
      ReflowScrollbar(aPresContext, 
                      aReflowState, 
                      aStatus, 
                      aScrollBarResized,
                      mVScrollbarFrame, 
                      aIncrementalChild);
      if (aScrollBarResized)
          aHscrollbarNeedsReflow = PR_TRUE;

     }

    SetScrollbarVisibility(mHScrollbarFrame, mHasHorizontalScrollbar);
    SetScrollbarVisibility(mVScrollbarFrame, mHasVerticalScrollbar);
}

void
nsGfxScrollFrame::ReflowScrollbar(    nsIPresContext&          aPresContext,
                                   const nsHTMLReflowState& aReflowState,
                                   nsReflowStatus&          aStatus,
                                   PRBool&                  aScrollbarResized, 
                                   nsIFrame*                aScrollbarFrame,
                                   nsIFrame*&               aIncrementalChild)
{
  nsSize scrollArea;
  GetFrameSize(mScrollAreaFrame, scrollArea);
 
  // is it the horizontal scrollbar?
  if (aScrollbarFrame == mHScrollbarFrame) {

      // get the width to layout in
      nscoord width = scrollArea.width;

      nsHTMLReflowMetrics desiredSize(nsnull);

      // reflow the frame
      ReflowFrame(aPresContext, desiredSize, aReflowState, aStatus, mHScrollbarFrame, 
                      nsSize(NS_INTRINSICSIZE, NS_INTRINSICSIZE),
                      nsSize(width, NS_INTRINSICSIZE),
                      aScrollbarResized,
                      aIncrementalChild);

  } else if (aScrollbarFrame == mVScrollbarFrame) {
      // is it the vertical scrollbar?

      // get the height to layout in
      nscoord height = scrollArea.height;

      nsHTMLReflowMetrics desiredSize(nsnull);

      // reflow the frame
      ReflowFrame(aPresContext, desiredSize, aReflowState, aStatus, mVScrollbarFrame, 
                      nsSize(NS_INTRINSICSIZE, NS_INTRINSICSIZE),
                      nsSize(NS_INTRINSICSIZE, height),
                      aScrollbarResized,
                      aIncrementalChild);
      
  }

}

void
nsGfxScrollFrame::AddRemoveScrollbar(PRBool& aHasScrollbar, nscoord& aSize, nscoord aSbSize, PRBool aAdd)
{
   if ((aAdd && aHasScrollbar) || (!aAdd && !aHasScrollbar))
      return;
   
   if (aSize != NS_INTRINSICSIZE) {
     if (aAdd)
        aSize -= aSbSize;
     else
        aSize += aSbSize;
   }
   
   aHasScrollbar = aAdd;
}

void
nsGfxScrollFrame::AddHorizontalScrollbar(const nsSize& aSbSize, nsSize& aScrollAreaSize)
{
   AddRemoveScrollbar(mHasHorizontalScrollbar, aScrollAreaSize.height, aSbSize.height, PR_TRUE);
}

void
nsGfxScrollFrame::AddVerticalScrollbar(const nsSize& aSbSize, nsSize& aScrollAreaSize)
{
   AddRemoveScrollbar(mHasVerticalScrollbar, aScrollAreaSize.width, aSbSize.width, PR_TRUE);
}

void
nsGfxScrollFrame::RemoveHorizontalScrollbar(const nsSize& aSbSize, nsSize& aScrollAreaSize)
{
   AddRemoveScrollbar(mHasHorizontalScrollbar, aScrollAreaSize.height, aSbSize.height, PR_FALSE);
}

void
nsGfxScrollFrame::RemoveVerticalScrollbar(const nsSize& aSbSize, nsSize& aScrollAreaSize)
{
   AddRemoveScrollbar(mHasVerticalScrollbar, aScrollAreaSize.width, aSbSize.width, PR_FALSE);
}

/**
 * Give a computed and available sizes flows the child at that size. 
 * This is special because it takes into account margin and borderpadding.
 * It also tells us if the child changed size or not.
 * And finally it will handle the incremental reflow for us. If it reflowed the 
 * incrementalChild it will set it to nsnull
 */
nsresult
nsGfxScrollFrame::ReflowFrame(    nsIPresContext&          aPresContext,
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus&          aStatus,
                               nsIFrame*                aFrame,
                               const nsSize&            aAvailable,
                               const nsSize&            aComputed,
                               PRBool&                  aResized,
                               nsIFrame*&               aIncrementalChild)
{
    // get the current size
    nsSize oldSize;
    GetFrameSize(aFrame, oldSize);

    aDesiredSize.width = 0;
    aDesiredSize.height = 0;

    nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                           aFrame, aAvailable);

    // handle incremental reflow
    if (aReflowState.reason == eReflowReason_Incremental) {
        if (aFrame == aIncrementalChild)
           aIncrementalChild = nsnull;
        else 
           childReflowState.reason = eReflowReason_Resize;
    }

    childReflowState.mComputedWidth =  aComputed.width;
    childReflowState.mComputedHeight = aComputed.height;

    // subtract out the childs margin and border if computed
    const nsStyleSpacing* spacing;
    nsresult rv = aFrame->GetStyleData(eStyleStruct_Spacing,
                   (const nsStyleStruct*&) spacing);

    nsMargin margin;
    if (!spacing->GetMargin(margin)) 
      margin.SizeTo(0,0,0,0);
  
    // this basically sucks. Sometime it fails to get the border and padding
    // because one or the other is not set. So we are forced to go out and get
    // the border and padding separately and add them together.
    nsMargin border;
    if (!spacing->GetBorderPadding(border)) {
      if (!spacing->GetBorder(border)) 
        border.SizeTo(0,0,0,0);

      nsMargin padding;
      if (!spacing->GetPadding(padding)) 
        padding.SizeTo(0,0,0,0);
      
      border += padding;
    }

    nsMargin total = margin + border;

    if (childReflowState.mComputedWidth != NS_INTRINSICSIZE)
       childReflowState.mComputedWidth -= total.left + total.right;

    if (childReflowState.mComputedHeight != NS_INTRINSICSIZE)
       childReflowState.mComputedHeight -= total.top + total.bottom;

    ReflowChild(aFrame, aPresContext, aDesiredSize, childReflowState, aStatus);
    NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");

    // if the frame size change then mark the flag
    if (oldSize.width != aDesiredSize.width || oldSize.height != aDesiredSize.height) {
       aFrame->SizeTo(aDesiredSize.width, aDesiredSize.height);
       aResized = PR_TRUE;
    }

    // add the margin back in
    aDesiredSize.width += margin.left + margin.right;
    aDesiredSize.height += margin.top + margin.bottom;

    return NS_OK;
}
/**
 * Reflow the scroll area if it needs it and return its size. Also determine if the reflow will
 * cause any of the scrollbars to need to be reflowed.
 */
void
nsGfxScrollFrame::ReflowScrollArea(   nsIPresContext&          aPresContext,
                                   nsHTMLReflowMetrics&     aDesiredSize,
                                   const nsHTMLReflowState& aReflowState,
                                   nsReflowStatus&          aStatus,
                                   PRBool&                  aHscrollbarNeedsReflow, 
                                   PRBool&                  aVscrollbarNeedsReflow, 
                                   PRBool&                  aScrollAreaNeedsReflow, 
                                   nsIFrame*&               aIncrementalChild)
{

    aVscrollbarNeedsReflow = PR_FALSE;
    aHscrollbarNeedsReflow = PR_FALSE;

    nsSize scrollAreaSize;

    if (!aScrollAreaNeedsReflow) {
       // if we don't need to reflow the content area then just use its old size.
       GetFrameSize(mScrollAreaFrame, scrollAreaSize);
    } else {

      // get the current scrollarea size
      nsSize oldScrollAreaSize;
      GetFrameSize(mScrollAreaFrame, oldScrollAreaSize);

      // get the width of the vertical scrollbar and the height of the horizontal scrollbar
      nsSize sbSize;
      GetScrollbarDimensions(aPresContext, sbSize);

      // Compute the scroll area size (area inside of the border edge and inside
      // of any vertical and horizontal scrollbars)
      nsSize scrollAreaSize;
      nsHTMLReflowMetrics scrollAreaDesiredSize(aDesiredSize.maxElementSize);

      CalculateScrollAreaSize(aPresContext, aReflowState, sbSize,
                              scrollAreaSize);


      // -------- flow the scroll area -----------
      nsSize      scrollAreaReflowSize(scrollAreaSize.width, scrollAreaSize.height);

      PRBool resized = PR_FALSE;
      ReflowFrame(aPresContext, scrollAreaDesiredSize, aReflowState, aStatus, mScrollAreaFrame, 
                  scrollAreaReflowSize,
                  scrollAreaReflowSize,
                  resized,
                  aIncrementalChild);

      // make sure we take into account any children outside our bounds
      CalculateChildTotalSize(mScrollAreaFrame, scrollAreaDesiredSize);


      // its possible that we will have to do this twice. Its a special case
      // when a the scrollarea is exactly the same height as its content but
      // the contents width is greater. We will need a horizontal scrollbar
      // but the size of the scrollbar will cause the scrollarea to be shorter
      // than the content. So we will need a vertical scrollbar. But that could
      // require us to squeeze the content. So we would have to reflow. Basically
      // we have to start over. Thats why this loop is here.
      for (int i=0; i < 1; i++) {

        // if we are shrink wrapping the height. 
        if (NS_INTRINSICSIZE == scrollAreaSize.height) {
              // the view port become the size that child requested.
              scrollAreaSize.height = scrollAreaDesiredSize.height;

              // if we were auto and have a vertical scrollbar remove it because we
              // can have whatever size we wanted.
              if (aReflowState.mStyleDisplay->mOverflow != NS_STYLE_OVERFLOW_SCROLL) {
                RemoveVerticalScrollbar(sbSize, scrollAreaSize);
              }
        } else {
          // if we are not shrink wrapping

          // if we have 'auto' scrollbars
          if (aReflowState.mStyleDisplay->mOverflow != NS_STYLE_OVERFLOW_SCROLL) {
            // get the ara frame is the scrollarea
            nsSize size;
            GetScrolledContentSize(size);

            PRBool  mustReflow = PR_FALSE;

            // There are two cases to consider
              if (size.height <= scrollAreaSize.height) {
                if (mHasVerticalScrollbar) {
                  // We left room for the vertical scrollbar, but it's not needed;
                  // remove it.
                  RemoveVerticalScrollbar(sbSize, scrollAreaSize);
                  mustReflow = PR_TRUE;
                }
              } else {
                if (!mHasVerticalScrollbar) {
                  // We didn't leave room for the vertical scrollbar, but it turns
                  // out we needed it
                  AddVerticalScrollbar(sbSize, scrollAreaSize);
                  mustReflow = PR_TRUE;
                  aVscrollbarNeedsReflow = PR_TRUE;
                }
            }

            // Go ahead and reflow the child a second time if we added or removed the scrollbar
            if (mustReflow) {
              nsSize          scrollAreaReflowSize(scrollAreaSize.width, scrollAreaSize.height);

              PRBool resized = PR_FALSE;

              ReflowFrame(aPresContext, scrollAreaDesiredSize, aReflowState, aStatus, mScrollAreaFrame, 
                              scrollAreaReflowSize,
                              scrollAreaReflowSize,
                              resized,
                              aIncrementalChild);
          
              CalculateChildTotalSize(mScrollAreaFrame, scrollAreaDesiredSize);

              //scrollAreaSize.height = scrollAreaDesiredSize.height;
            }
          }
        }
      

        // if we are shrink wrapping the scroll area height becomes whatever the child wanted
          if (NS_INTRINSICSIZE == scrollAreaSize.width) {
             scrollAreaSize.width = scrollAreaDesiredSize.width;

             // if we have auto scrollbars the remove the horizontal scrollbar
             if (aReflowState.mStyleDisplay->mOverflow != NS_STYLE_OVERFLOW_SCROLL) {
                 RemoveHorizontalScrollbar(sbSize, scrollAreaSize);
             }
          } else {
            // if scrollbars are auto
            if ((NS_STYLE_OVERFLOW_SCROLL != aReflowState.mStyleDisplay->mOverflow))
            {
              // get the ara frame is the scrollarea
              nsSize size;
              GetScrolledContentSize(size);

              // if the child is wider that the scroll area
              // and we don't have a scrollbar add one.
              if (size.width > scrollAreaSize.width) { 

                if (!mHasHorizontalScrollbar) {
                     AddHorizontalScrollbar(sbSize, scrollAreaSize);

                     // if we added a horizonal scrollbar and we did not have a vertical
                     // there is a chance that by adding the horizonal scrollbar we will
                     // suddenly need a vertical scrollbar. Is a special case but its 
                     // important.
                     if (!mHasVerticalScrollbar && size.height > scrollAreaSize.height - sbSize.height)
                        continue;
                }
              } else {
                  // if the area is smaller or equal to and we have a scrollbar then
                  // remove it.
                  RemoveHorizontalScrollbar(sbSize, scrollAreaSize);
              }
            }
        }

        // always break out of the loop. Only continue if another scrollbar was added.
        break;
      }


      // if the scroll area changed size
      if (oldScrollAreaSize != scrollAreaSize)
      {
        // if the scroll area changed height then signal for reflow of the vertical scrollbar
        if (oldScrollAreaSize.height != scrollAreaSize.height)
           aVscrollbarNeedsReflow = PR_TRUE;

        // if the scroll area changed width then signal for reflow of the horizontal scrollbar
        if (oldScrollAreaSize.width != scrollAreaSize.width)
           aHscrollbarNeedsReflow = PR_TRUE;
      }

      nsSize size;
      GetScrolledContentSize(size);

      float p2t;
      aPresContext.GetScaledPixelsToTwips(&p2t);
      mOnePixel = NSIntPixelsToTwips(1, p2t);
      const nsStyleFont* font;
      GetStyleData(eStyleStruct_Font, (const nsStyleStruct*&) font);
      const nsFont& f = font->mFont;
      nsCOMPtr<nsIFontMetrics> fm;
      aPresContext.GetMetricsFor(f, getter_AddRefs(fm));
      nscoord fontHeight = 1;
      fm->GetHeight(fontHeight);

      SetAttribute(mVScrollbarFrame, nsXULAtoms::maxpos, size.height - scrollAreaSize.height);
      SetAttribute(mHScrollbarFrame, nsXULAtoms::maxpos, size.width - scrollAreaSize.width);
      SetAttribute(mVScrollbarFrame, nsXULAtoms::pageincrement, nscoord(float(scrollAreaSize.height)*0.8));
      SetAttribute(mHScrollbarFrame, nsXULAtoms::pageincrement, nscoord(float(scrollAreaSize.width)*0.8));

      SetAttribute(mVScrollbarFrame, nsXULAtoms::increment, fontHeight);
      SetAttribute(mHScrollbarFrame, nsXULAtoms::increment, 10*mOnePixel);

      // size the scroll area
      // even if it is different from the old size. This is because ReflowFrame set the child's
      // frame so we have to make sure our final size is scrollAreaSize

      SetFrameSize(mScrollAreaFrame, scrollAreaSize);  

  } 
}  



/**
 * Places the scrollbars and scrollarea and returns the overall size
 */
void
nsGfxScrollFrame::LayoutChildren(nsIPresContext&          aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState)
{
  nsSize scrollAreaSize;
  GetFrameSize(mScrollAreaFrame, scrollAreaSize);

  nsSize sbSize;
  GetScrollbarDimensions(aPresContext, sbSize);


  // Place scroll area
  mScrollAreaFrame->MoveTo(aReflowState.mComputedBorderPadding.left, aReflowState.mComputedBorderPadding.top);

  // place vertical scrollbar
  mVScrollbarFrame->MoveTo(aReflowState.mComputedBorderPadding.left + scrollAreaSize.width, aReflowState.mComputedBorderPadding.top);

  // place horizontal scrollbar
  mHScrollbarFrame->MoveTo(aReflowState.mComputedBorderPadding.left, aReflowState.mComputedBorderPadding.top + scrollAreaSize.height);

  // Compute our desired size
  // ---------- compute width ----------- 
  aDesiredSize.width = scrollAreaSize.width;

  // add border
  aDesiredSize.width += aReflowState.mComputedBorderPadding.left + aReflowState.mComputedBorderPadding.right;

  // add scrollbar
  if (mHasVerticalScrollbar) 
     aDesiredSize.width += sbSize.width;
  
  // ---------- compute height ----------- 

  /*
  // For the height if we're shrink wrapping then use whatever is smaller between
  // the available height and the child's desired size; otherwise, use the scroll
  // area size
  if (NS_AUTOHEIGHT == aReflowState.mComputedHeight) {
    aDesiredSize.height = PR_MIN(aReflowState.availableHeight, scrollAreaSize.height);
  } else {
  */

    aDesiredSize.height = scrollAreaSize.height;
  //}

  // add the scrollbar in
  if (mHasHorizontalScrollbar)
     aDesiredSize.height += sbSize.height;

  // add in our border
  aDesiredSize.height += aReflowState.mComputedBorderPadding.top + aReflowState.mComputedBorderPadding.bottom;
  

  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;

  // ------- set up max size -------
  if (nsnull != aDesiredSize.maxElementSize) {
    nscoord maxWidth = aDesiredSize.maxElementSize->width;
    maxWidth += aReflowState.mComputedBorderPadding.left + aReflowState.mComputedBorderPadding.right + sbSize.width;
    nscoord maxHeight = aDesiredSize.maxElementSize->height;
    maxHeight += aReflowState.mComputedBorderPadding.top + aReflowState.mComputedBorderPadding.bottom;
    aDesiredSize.maxElementSize->width = maxWidth;
    aDesiredSize.maxElementSize->height = maxHeight;
  }
}


NS_IMETHODIMP
nsGfxScrollFrame::Paint(nsIPresContext&      aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect&        aDirtyRect,
                     nsFramePaintLayer    aWhichLayer)
{
  /*
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    // Only paint the border and background if we're visible
    const nsStyleDisplay* display = (const nsStyleDisplay*)
      mStyleContext->GetStyleData(eStyleStruct_Display);

    if (display->mVisible) {
      // Paint our border only (no background)
      const nsStyleSpacing* spacing = (const nsStyleSpacing*)
        mStyleContext->GetStyleData(eStyleStruct_Spacing);

      nsRect  rect(0, 0, mRect.width, mRect.height);
      nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                  aDirtyRect, rect, *spacing, mStyleContext, 0);

      
      // paint the little square between the scrollbars
      nsRect vbar;
      nsRect hbar;

      // get the child's rect
      mVScrollbarFrame->GetRect(vbar);
      mVScrollbarFrame->GetRect(hbar);

      // get the margin
      const nsStyleSpacing* s;
      nsresult rv = mVScrollbarFrame->GetStyleData(eStyleStruct_Spacing,
                     (const nsStyleStruct*&) s);

      nsMargin margin;
      if (!s->GetMargin(margin)) 
        margin.SizeTo(0,0,0,0);

      vbar.Inflate(margin);

      
      // get the margin
      rv = mHScrollbarFrame->GetStyleData(eStyleStruct_Spacing,
                     (const nsStyleStruct*&) s);

      if (!s->GetMargin(margin)) 
        margin.SizeTo(0,0,0,0);

      hbar.Inflate(margin);

      rect.SetRect(hbar.x + hbar.width, vbar.y + vbar.height, vbar.width, hbar.height);
      
      nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                      aDirtyRect, rect, *myColor, *mySpacing, 0, 0);

    }
  }
*/
  // Paint our children
  nsresult rv = nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                                 aWhichLayer);
        return rv;
        
}


PRIntn
nsGfxScrollFrame::GetSkipSides() const
{
  return 0;
}

NS_IMETHODIMP
nsGfxScrollFrame::GetFrameType(nsIAtom** aType) const
{
  return nsHTMLContainerFrame::GetFrameType(aType);
}

NS_IMETHODIMP
nsGfxScrollFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("GfxScroll", aResult);
}

NS_IMETHODIMP 
nsGfxScrollFrame::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{           
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                                        
  if (aIID.Equals(kIAnonymousContentCreatorIID)) {                                         
    *aInstancePtr = (void*)(nsIAnonymousContentCreator*) this;                                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  } 

  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);                                  
}

NS_IMETHODIMP
nsGfxScrollFrame::AttributeChanged(nsIDocument *aDocument,
                              nsIContent*  aContent,
                              nsIAtom*     aAttribute,
                              PRInt32      aHint) 
{
   if (mHScrollbarFrame && mVScrollbarFrame)
   {
     nsCOMPtr<nsIContent> vcontent;
     nsCOMPtr<nsIContent> hcontent;

     mHScrollbarFrame->GetContent(getter_AddRefs(hcontent));
     mVScrollbarFrame->GetContent(getter_AddRefs(vcontent));

     if (hcontent.get() == aContent  || vcontent.get() == aContent)
     {
        nscoord x = 0;
        nscoord y = 0;

        nsString value;
        if (NS_CONTENT_ATTR_HAS_VALUE == hcontent->GetAttribute(kNameSpaceID_None, nsXULAtoms::curpos, value))
        {
           PRInt32 error;

           // convert it to an integer
           x = value.ToInteger(&error);
        }

        if (NS_CONTENT_ATTR_HAS_VALUE == vcontent->GetAttribute(kNameSpaceID_None, nsXULAtoms::curpos, value))
        {
           PRInt32 error;

           // convert it to an integer
           y = value.ToInteger(&error);
        }

        ScrollbarChanged(x*mOnePixel, y*mOnePixel);
     }
   }

   return NS_OK;
}


void
nsGfxScrollFrame::ScrollbarChanged(nscoord aX, nscoord aY)
{
  nsIScrollableView* scrollable = GetScrollableView();
  scrollable->ScrollTo(aX,aY, NS_SCROLL_PROPERTY_ALWAYS_BLIT);
 // printf("scrolling to: %d, %d\n", aX, aY);
}

NS_IMETHODIMP
nsGfxScrollFrame::ContentReplaced(nsIDocument *aDocument,
			                       nsIContent* aContainer,
                             nsIContent* aOldChild,
                             nsIContent* aNewChild,
                             PRInt32 aIndexInContainer) 
{ 
  if (aContainer == mContent) {
    nsCOMPtr<nsIDocument> document;
    mContent->GetDocument(*getter_AddRefs(document));
    document->RemoveObserver(this);
  }
  return NS_OK; 
}  

NS_IMETHODIMP
nsGfxScrollFrame::ContentRemoved(nsIDocument *aDocument,
                            nsIContent* aContainer,
                            nsIContent* aChild,
                            PRInt32 aIndexInContainer) 
{ 
  if (aContainer == mContent) {
    nsCOMPtr<nsIDocument> document;
    mContent->GetDocument(*getter_AddRefs(document));
    document->RemoveObserver(this);
  }
  return NS_OK; 
}

void
nsGfxScrollFrame::SetAttribute(nsIFrame* aFrame, nsIAtom* aAtom, nscoord aSize)
{
  // convert to pixels
  aSize /= mOnePixel;

  // only set the attribute if it changed.
  nsString currentValue;
  nsCOMPtr<nsIContent> content;
  aFrame->GetContent(getter_AddRefs(content));
  content->GetAttribute(kNameSpaceID_None, aAtom, currentValue);

  char ch[100];
  sprintf(ch,"%d", aSize);
  nsString newValue(ch);

  if (currentValue != newValue)
      content->SetAttribute(kNameSpaceID_None, aAtom, newValue, PR_TRUE);
}

/*

class OurDocObserver: public nsIDocumentObserver
{
  OurDocObserver();
  virtual ~OurDocObserver();

  NS_DECL_ISUPPORTS

  NS_IMETHOD BeginUpdate(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD EndUpdate(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD BeginLoad(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD EndLoad(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD BeginReflow(nsIDocument *aDocument,
			                   nsIPresShell* aShell) { return NS_OK; }
  NS_IMETHOD EndReflow(nsIDocument *aDocument,
		                   nsIPresShell* aShell) { return NS_OK; } 
  NS_IMETHOD ContentChanged(nsIDocument* aDoc, , 
                            nsIContent* aContent,
                            nsISupports* aSubContent) { return NS_OK; }
  NS_IMETHOD ContentStatesChanged(nsIDocument* aDocument,
                                  nsIContent* aContent1,
                                  nsIContent* aContent2) { return NS_OK; }
  NS_IMETHOD AttributeChanged(nsIDocument *aDocument,
                              nsIContent*  aContent,
                              nsIAtom*     aAttribute,
                              PRInt32      aHint);
  NS_IMETHOD ContentAppended(nsIDocument *aDocument,
			                       nsIContent* aContainer,
                             PRInt32     aNewIndexInContainer) { return NS_OK; } 
  NS_IMETHOD ContentInserted(nsIDocument *aDocument,
			                       nsIContent* aContainer,
                             nsIContent* aChild,
                             PRInt32 aIndexInContainer) { return NS_OK; } 
  NS_IMETHOD ContentReplaced(nsIDocument *aDocument,
			                       nsIContent* aContainer,
                             nsIContent* aOldChild,
                             nsIContent* aNewChild,
                             PRInt32 aIndexInContainer);
  NS_IMETHOD ContentRemoved(nsIDocument *aDocument,
                            nsIContent* aContainer,
                            nsIContent* aChild,
                            PRInt32 aIndexInContainer);
  NS_IMETHOD StyleSheetAdded(nsIDocument *aDocument,
                             nsIStyleSheet* aStyleSheet) { return NS_OK; }
  NS_IMETHOD StyleSheetRemoved(nsIDocument *aDocument,
                               nsIStyleSheet* aStyleSheet) { return NS_OK; }
  NS_IMETHOD StyleSheetDisabledStateChanged(nsIDocument *aDocument,
                                            nsIStyleSheet* aStyleSheet,
                                            PRBool aDisabled) { return NS_OK; }
  NS_IMETHOD StyleRuleChanged(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule,
                              PRInt32 aHint) { return NS_OK; }
  NS_IMETHOD StyleRuleAdded(nsIDocument *aDocument,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule) { return NS_OK; }
  NS_IMETHOD StyleRuleRemoved(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule) { return NS_OK; }
  NS_IMETHOD DocumentWillBeDestroyed(nsIDocument *aDocument) { return NS_OK; }

  nsGfxScrollFrame* mTarget;
}



static NS_DEFINE_IID(kIDocumentObserverIID, NS_IDOCUMENT_OBSERVER_IID);
NS_IMPL_ISUPPORTS(OurDocObserver, kIDocumentObserverIID);


NS_IMETHODIMP
nsGfxScrollFrame::GetBoxInfo(nsIPresContext& aPresContext, const nsHTMLReflowState& aReflowState, nsBoxInfo& aSize)
{
  nsresult rv;
  nsBoxInfo scrollArea, vbar, hbar;
  nsIBox ibox = nsnull;
  rv = mScrollAreaFrame->QueryInterface(nsIBox::GetIID(), (void**)&ibox))
  ibox->GetBoxInfo(aPresContext, aReflowState, scrollArea);
  NS_ASSERTION(NS_SUCCEEDED(rv),"Scrollarea must implement box!!");

  if (mHasVerticalScrollbar) {
    ibox = nsnull;
    mVScrollFrame->QueryInterface(nsIBox::GetIID(), (void**)&ibox))
    ibox->GetBoxInfo(aPresContext, aReflowState, vbox);
    NS_ASSERTION(NS_SUCCEEDED(rv),"Scrollarea must implement box!!");
  }

  if (mHasHorizontalScrollbar) {
    ibox = nsnull;
    mHScrollFrame->QueryInterface(nsIBox::GetIID(), (void**)&ibox))
    ibox->GetBoxInfo(aPresContext, aReflowState, hbar);
    NS_ASSERTION(NS_SUCCEEDED(rv),"Scrollarea must implement box!!");
  }

  aSize.maxSize = NS_INTRINSICSIZE;
  aSize.minSize = 0;
  aSize.prefSize = 0;

  aSize.prefSize.width += scrollAreaInfo.prefSize.width + vboxInfo.prefSize.width;
  aSize.prefSize.height += scrollAreaInfo.prefSize.height + hboxInfo.prefSize.height;

  aSize.minSize.width += scrollAreaInfo.minSize.width + vboxInfo.minSize.width;
  aSize.minSize.height += scrollAreaInfo.minSize.height + hboxInfo.minSize.height;

  if (scrollAreaInfo.maxSize != NS_INTRINSICSIZE && 
      mVScrollFrame.maxSize != NS_INTRINSICSIZE &&
      mHScrollFrame.maxSize != NS_INTRINSICSIZE) 
  {
     aSize.maxSize += scrollAreaInfo.maxSize + mVScrollFrame.maxSize + mHScrollFrame.maxSize;
  }

  GetRedefinedMinPrefMax(this, aSize);

  return NS_OK;
}

void 
nsBoxFrame::GetRedefinedMinPrefMax(nsIFrame* aFrame, nsBoxInfo& aSize)
{
  // add in the css min, max, pref
    const nsStylePosition* position;
    nsresult rv = aFrame->GetStyleData(eStyleStruct_Position,
                  (const nsStyleStruct*&) position);

    // see if the width or height was specifically set
    if (position->mWidth.GetUnit() == eStyleUnit_Coord)  {
        aSize.prefSize.width = position->mWidth.GetCoordValue();
    }

    if (position->mHeight.GetUnit() == eStyleUnit_Coord) {
        aSize.prefSize.height = position->mHeight.GetCoordValue();     
    }
    
    // same for min size. Unfortunately min size is always set to 0. So for now
    // we will assume 0 means not set.
    if (position->mMinWidth.GetUnit() == eStyleUnit_Coord) {
        nscoord min = position->mMinWidth.GetCoordValue();
        if (min != 0)
           aSize.minSize.width = min;
    }

    if (position->mMinHeight.GetUnit() == eStyleUnit_Coord) {
        nscoord min = position->mMinHeight.GetCoordValue();
        if (min != 0)
           aSize.minSize.height = min;
    }

    // and max
    if (position->mMaxWidth.GetUnit() == eStyleUnit_Coord) {
        nscoord max = position->mMaxWidth.GetCoordValue();
        aSize.maxSize.width = max;
    }

    if (position->mMaxHeight.GetUnit() == eStyleUnit_Coord) {
        nscoord max = position->mMaxHeight.GetCoordValue();
        aSize.maxSize.height = max;
    }

    // get the flexibility
    nsCOMPtr<nsIContent> content;
    aFrame->GetContent(getter_AddRefs(content));

    PRInt32 error;
    nsString value;

    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsXULAtoms::flex, value))
    {
        value.Trim("%");
        // convert to a percent.
        aSize.flex = value.ToFloat(&error)/float(100.0);
    }
}



NS_IMETHODIMP 
nsGfxScrollFrame::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{           
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                                        
  if (aIID.Equals(kIBoxIID)) {                                         
    *aInstancePtr = (void*)(nsIBox*) this;                                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }   

  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);                                     
}

NS_IMETHODIMP
nsGfxScrollFrame::Dirty(const nsHTMLReflowState& aReflowState, nsIFrame*& incrementalChild)
{
  mIncrementalChild = nsnull;
  incrementalChild = nsnull;
  nsresult rv = NS_OK;

  // Dirty any children that need it.
  nsIFrame* frame;
  aReflowState.reflowCommand->GetNext(frame);

  nscoord count = 0;
  nsIFrame* childFrame = mFrames.FirstChild(); 
  while (nsnull != childFrame) 
  {
    if (childFrame == frame) {
        if (frame == mVScrollbarFrame)
           mVscrollbarNeedsReflow = PR_TRUE;
        else if (frame == mHScrollbarFrame)
           mHscrollbarNeedsReflow = PR_TRUE;
        else if (frame == mScrollAreaFrame)
           mHscrollAreaNeedsReflow = PR_TRUE;
 
        nsIBox* ibox;
        if (NS_SUCCEEDED(childFrame->QueryInterface(nsIBox::GetIID(), (void**)&ibox)) && ibox)
            ibox->Dirty(aReflowState, incrementalChild);
        else
            incrementalChild = frame;

        // if we found a leaf. Then mark it as being incremental. So when we
        // flow it we will flow it incrementally
        if (incrementalChild == childFrame)
            mIncrementalChild = frame;

        break;
    }

    rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(rv == NS_OK,"failed to get next child");
    if (NS_FAILED(rv))
      return rv;

    count++;
  }

  return NS_OK;
}


NS_IMETHODIMP_(nsrefcnt) 
nsGfxScrollFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) 
nsGfxScrollFrame::Release(void)
{
    return NS_OK;
}
*/
