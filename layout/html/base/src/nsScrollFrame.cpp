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
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsIReflowCommand.h"
#include "nsIDeviceContext.h"
#include "nsPageFrame.h"
#include "nsViewsCID.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsBodyFrame.h"
#include "nsHTMLContainerFrame.h"
#include "nsHTMLIIDs.h"
#include "nsCSSRendering.h"
#include "nsIScrollableView.h"
#include "nsWidgetsCID.h"

static NS_DEFINE_IID(kScrollViewIID, NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);

//----------------------------------------------------------------------

class nsScrollViewFrame : public nsHTMLContainerFrame {
public:
  nsScrollViewFrame(nsIContent* aContent, nsIFrame* aParent);

  NS_IMETHOD Init(nsIPresContext& aPresContext, nsIFrame* aChildList);
  
  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD  ListTag(FILE* out = stdout) const;

protected:
  virtual PRIntn GetSkipSides() const;
};

nsScrollViewFrame::nsScrollViewFrame(nsIContent* aContent, nsIFrame* aParent)
  : nsHTMLContainerFrame(aContent, aParent)
{
}

NS_IMETHODIMP
nsScrollViewFrame::Init(nsIPresContext& aPresContext, nsIFrame* aChildList)
{

  // Unless it's already a body frame, child frames that are containers
  // need to be wrapped in a body frame.
  // XXX Check for it a;ready being a body frame...
  nsIFrame* wrapperFrame;
  if (CreateWrapperFrame(aPresContext, aChildList, wrapperFrame)) {
    mFirstChild = wrapperFrame;
  } else {
    mFirstChild = aChildList;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsScrollViewFrame::Reflow(nsIPresContext&          aPresContext,
                          nsHTMLReflowMetrics&     aDesiredSize,
                          const nsHTMLReflowState& aReflowState,
                          nsReflowStatus&          aStatus)
{
  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
                     ("enter nsScrollViewFrame::Reflow: maxSize=%d,%d",
                      aReflowState.maxSize.width,
                      aReflowState.maxSize.height));

  // Create a view
  if (eReflowReason_Initial == aReflowState.reason) {
    // XXX It would be nice if we could do this sort of thing in our Init()
    // member function instead of here. Problem is the other frame code
    // would have to do the same...
    nsHTMLContainerFrame::CreateViewForFrame(aPresContext, this,
                                             mStyleContext, PR_TRUE);
  }

  // Scroll frame handles the border, and we handle the padding and background
  const nsStyleSpacing* spacing = (const nsStyleSpacing*)
    mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin padding;
  spacing->CalcPaddingFor(this, padding);

  // Allow the child frame to be as wide as our max width (minus scrollbar
  // width and padding), and as high as it wants to be.
  nsSize  kidMaxSize(aReflowState.maxSize.width, NS_UNCONSTRAINEDSIZE);

  if (NS_UNCONSTRAINEDSIZE != aReflowState.maxSize.width) {
    nsIDeviceContext* dc = aPresContext.GetDeviceContext();
    float             sbWidth, sbHeight;
  
    dc->GetScrollBarDimensions(sbWidth, sbHeight);
    kidMaxSize.width -= NSToCoordRound(sbWidth) + padding.left + padding.right;
    NS_RELEASE(dc);
  }
  
  // Reflow the child
  nsHTMLReflowMetrics kidMetrics(aDesiredSize.maxElementSize);
  nsHTMLReflowState   kidReflowState(aPresContext, mFirstChild, aReflowState, kidMaxSize);

  ReflowChild(mFirstChild, aPresContext, kidMetrics, kidReflowState, aStatus);
  NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
  
  // Place and size the child
  nsRect  rect(padding.left, padding.top, kidMetrics.width, kidMetrics.height);
  mFirstChild->SetRect(rect);

  // Determine our size
  aDesiredSize.width = kidMetrics.width + padding.left + padding.right;
  aDesiredSize.height = kidMetrics.height + padding.top + padding.bottom;

  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
    ("exit nsScrollViewFrame::Reflow: status=%d width=%d height=%d",
     aStatus, aDesiredSize.width, aDesiredSize.height));
  return NS_OK;
}

PRIntn
nsScrollViewFrame::GetSkipSides() const
{
  // Scroll frame handles the border...
  return 0xF;
}

NS_IMETHODIMP
nsScrollViewFrame::ListTag(FILE* out) const
{
  fputs("*scrollviewframe<", out);
  nsIAtom* atom;
  mContent->GetTag(atom);
  if (nsnull != atom) {
    nsAutoString tmp;
    atom->ToString(tmp);
    fputs(tmp, out);
    NS_RELEASE(atom);
  }
  fprintf(out, ">(%d)@%p", ContentIndexInContainer(this), this);
  return NS_OK;
}

//----------------------------------------------------------------------

/**
 * The scrolling view frame creates and manages the scrolling view.
 * It creates a nsScrollViewFrame which handles padding and rendering
 * of the background.
 */
class nsScrollingViewFrame : public nsHTMLContainerFrame {
public:
  nsScrollingViewFrame(nsIContent* aContent, nsIFrame* aParent);

  NS_IMETHOD Init(nsIPresContext& aPresContext, nsIFrame* aChildList);

  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD Paint(nsIPresContext&      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect);

  NS_IMETHOD  ListTag(FILE* out = stdout) const;

protected:
  virtual PRIntn GetSkipSides() const;
};

nsScrollingViewFrame::nsScrollingViewFrame(nsIContent* aContent, nsIFrame* aParent)
  : nsHTMLContainerFrame(aContent, aParent)
{
}

NS_IMETHODIMP
nsScrollingViewFrame::Init(nsIPresContext& aPresContext, nsIFrame* aChildList)
{
  // Create a scroll view frame
  mFirstChild = new nsScrollViewFrame(mContent, this);
  if (nsnull == mFirstChild) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  // Have it use our style context
  mFirstChild->SetStyleContext(&aPresContext, mStyleContext);

  // Reset the child frame's geometric and content parent to be
  // the scroll view frame
  aChildList->SetGeometricParent(mFirstChild);
  aChildList->SetContentParent(mFirstChild);

  // Init the scroll view frame passing it the child list
  return mFirstChild->Init(aPresContext, aChildList);
}

//XXX incremental reflow pass through
NS_IMETHODIMP
nsScrollingViewFrame::Reflow(nsIPresContext&          aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus)
{
  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
                     ("enter nsScrollingViewFrame::Reflow: maxSize=%d,%d",
                      aReflowState.maxSize.width,
                      aReflowState.maxSize.height));

  // Make sure we have a scrolling view
  nsIView* view;
  GetView(view);
  if (nsnull == view) {
    static NS_DEFINE_IID(kScrollingViewCID, NS_SCROLLING_VIEW_CID);
    static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);

    // Get parent view
    nsIFrame* parent;
    GetParentWithView(parent);
    NS_ASSERTION(parent, "GetParentWithView failed");
    nsIView* parentView;
    parent->GetView(parentView);
    NS_ASSERTION(parentView, "GetParentWithView failed");

    nsIViewManager* viewManager;
    parentView->GetViewManager(viewManager);

    nsresult rv = nsRepository::CreateInstance(kScrollingViewCID, 
                                               nsnull, 
                                               kIViewIID, 
                                               (void **)&view);
    // XXX We want the scrolling view to have a widget to clip any child
    // widgets that aren't visible, e.g. form elements, but there's currently
    // a bug which is why it's commented out
    if ((NS_OK != rv) || (NS_OK != view->Init(viewManager, 
                                              mRect,
                                              parentView,
                                              nsnull))) {
                                              // &kWidgetCID))) {
      NS_RELEASE(viewManager);
      return rv;
    }

    // Insert new view as a child of the parent view
    viewManager->InsertChild(parentView, view, 0);

    // If the background is transparent then inform the view manager
    const nsStyleColor* color = (const nsStyleColor*)
      mStyleContext->GetStyleData(eStyleStruct_Color);
    if (NS_STYLE_BG_COLOR_TRANSPARENT & color->mBackgroundFlags) {
      viewManager->SetViewContentTransparency(view, PR_TRUE);
    }
    SetView(view);
    NS_RELEASE(viewManager);
  }
  if (nsnull == view) {
    return NS_OK;
  }

  // Reflow the child and get its desired size. Let the child's height be
  // whatever it wants
  nsHTMLReflowState  kidReflowState(aPresContext, mFirstChild, aReflowState,
                                    nsSize(aReflowState.maxSize.width, NS_UNCONSTRAINEDSIZE));

  ReflowChild(mFirstChild, aPresContext, aDesiredSize, kidReflowState, aStatus);
  NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
  
  // If our height is constrained then make sure the scroll view is at least as
  // high as we're going to be
  if (NS_UNCONSTRAINEDSIZE != aReflowState.maxSize.height) {
    // Make sure we're at least as tall as the max height we were given
    if (aDesiredSize.height < aReflowState.maxSize.height) {
      aDesiredSize.height = aReflowState.maxSize.height;
    }
  }

  // Place and size the child
  nsRect rect(0, 0, aDesiredSize.width, aDesiredSize.height);
  mFirstChild->SetRect(rect);

  // The scroll view frame either shrink wraps around it's single
  // child OR uses the style width/height.
  if (aReflowState.HaveConstrainedWidth()) {
    aDesiredSize.width = aReflowState.minWidth;
  }
  if (aReflowState.HaveConstrainedHeight()) {
    aDesiredSize.height = aReflowState.minHeight;
  }
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;

  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
    ("exit nsScrollingViewFrame::Reflow: status=%d width=%d height=%d",
     aStatus, aDesiredSize.width, aDesiredSize.height));
  return NS_OK;
}

NS_IMETHODIMP
nsScrollingViewFrame::Paint(nsIPresContext&      aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect&        aDirtyRect)
{
  // Paint our children
  return nsContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect);
}

PRIntn
nsScrollingViewFrame::GetSkipSides() const
{
  return 0;
}

NS_IMETHODIMP
nsScrollingViewFrame::ListTag(FILE* out) const
{
  fputs("*scrollingviewframe<", out);
  nsIAtom* atom;
  mContent->GetTag(atom);
  if (nsnull != atom) {
    nsAutoString tmp;
    atom->ToString(tmp);
    fputs(tmp, out);
    NS_RELEASE(atom);
  }
  fprintf(out, ">(%d)@%p", ContentIndexInContainer(this), this);
  return NS_OK;
}

//----------------------------------------------------------------------

/**
 * The scroll frame basically acts as a border frame. It leaves room for and
 * renders the border. It creates a nsScrollingViewFrame as its only child
 * frame
 */
class nsScrollFrame : public nsHTMLContainerFrame {
public:
  nsScrollFrame(nsIContent* aContent, nsIFrame* aParent);

  NS_IMETHOD Init(nsIPresContext& aPresContext, nsIFrame* aChildList);

  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD Paint(nsIPresContext&      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect);

  NS_IMETHOD  ListTag(FILE* out = stdout) const;

protected:
  virtual PRIntn GetSkipSides() const;
};

nsScrollFrame::nsScrollFrame(nsIContent* aContent, nsIFrame* aParent)
  : nsHTMLContainerFrame(aContent, aParent)
{
}

NS_IMETHODIMP
nsScrollFrame::Init(nsIPresContext& aPresContext, nsIFrame* aChildList)
{
  NS_PRECONDITION(nsnull != aChildList, "no child frame");

  // Create a scrolling view frame
  mFirstChild = new nsScrollingViewFrame(mContent, this);
  if (nsnull == mFirstChild) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  // Have it use our style context
  mFirstChild->SetStyleContext(&aPresContext, mStyleContext);

  // Reset the child frame's geometric and content parent to be
  // the scrolling view frame
#ifdef NS_DEBUG
  // Verify that there's only one child frame
  nsIFrame* nextSibling;
  aChildList->GetNextSibling(nextSibling);
  NS_ASSERTION(nsnull == nextSibling, "expected only one child");
#endif
  aChildList->SetGeometricParent(mFirstChild);
  aChildList->SetContentParent(mFirstChild);

  // Init the scrollable view frame passing it the child list
  return mFirstChild->Init(aPresContext, aChildList);
}

//XXX incremental reflow pass through
NS_IMETHODIMP
nsScrollFrame::Reflow(nsIPresContext&          aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
                     ("enter nsScrollFrame::Reflow: maxSize=%d,%d",
                      aReflowState.maxSize.width,
                      aReflowState.maxSize.height));

  // We handle the border only
  const nsStyleSpacing* spacing = (const nsStyleSpacing*)
    mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin border;
  spacing->CalcBorderFor(this, border);
  nscoord lr = border.left + border.right;
  nscoord tb = border.top + border.bottom;

  // Compute the scrolling view frame's max size taking into account our
  // borders
  nsSize kidMaxSize;
  if (aReflowState.HaveConstrainedWidth()) {
    // This value reflects space for the content area only, so don't
    // subtract for borders...
    kidMaxSize.width = aReflowState.minWidth;
  }
  else {
    kidMaxSize.width = aReflowState.maxSize.width;
    if (NS_UNCONSTRAINEDSIZE != kidMaxSize.width) {
      kidMaxSize.width -= lr;
    }
  }
  if (aReflowState.HaveConstrainedHeight()) {
    // This value reflects space for the content area only, so don't
    // subtract for borders...
    kidMaxSize.height = aReflowState.minHeight;
  }
  else {
    kidMaxSize.height = aReflowState.maxSize.height;
    if (NS_UNCONSTRAINEDSIZE != kidMaxSize.height) {
      kidMaxSize.height -= tb;
    }
  }

  // Reflow the child and get its desired size
  nsHTMLReflowState  kidReflowState(aPresContext, mFirstChild, aReflowState, kidMaxSize);

  ReflowChild(mFirstChild, aPresContext, aDesiredSize, kidReflowState, aStatus);
  NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
  
  // Place and size the child
  nsRect rect(border.left, border.top, aDesiredSize.width, aDesiredSize.height);
  mFirstChild->SetRect(rect);

  // Compute our desired size by adding in space for the borders
  aDesiredSize.width += lr;
  aDesiredSize.height += tb;
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;

  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
    ("exit nsScrollFrame::Reflow: status=%d width=%d height=%d",
     aStatus, aDesiredSize.width, aDesiredSize.height));
  return NS_OK;
}

NS_IMETHODIMP
nsScrollFrame::Paint(nsIPresContext&      aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect&        aDirtyRect)
{
  // Paint our border only (no background)
  const nsStyleSpacing* spacing =
    (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);

  nsRect  rect(0, 0, mRect.width, mRect.height);
  nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                              aDirtyRect, rect, *spacing, 0);

  // Paint our children
  return nsContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect);
}

PRIntn
nsScrollFrame::GetSkipSides() const
{
  return 0;
}

NS_IMETHODIMP
nsScrollFrame::ListTag(FILE* out) const
{
  fputs("*scrollframe<", out);
  nsIAtom* atom;
  mContent->GetTag(atom);
  if (nsnull != atom) {
    nsAutoString tmp;
    atom->ToString(tmp);
    fputs(tmp, out);
    NS_RELEASE(atom);
  }
  fprintf(out, ">(%d)@%p", ContentIndexInContainer(this), this);
  return NS_OK;
}

//----------------------------------------------------------------------

nsresult
NS_NewScrollFrame(nsIContent* aContent,
                  nsIFrame*   aParentFrame,
                  nsIFrame*&  aResult)
{
  aResult = new nsScrollFrame(aContent, aParentFrame);
  if (nsnull == aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}
