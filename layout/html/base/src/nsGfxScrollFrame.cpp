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
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsHTMLParts.h"
#include "nsPresContext.h"
#include "nsHTMLReflowCommand.h"
#include "nsIDeviceContext.h"
#include "nsPageFrame.h"
#include "nsViewsCID.h"
#include "nsIServiceManager.h"
#include "nsIView.h"
#include "nsIScrollableView.h"
#include "nsIScrollable.h"
#include "nsIViewManager.h"
#include "nsHTMLContainerFrame.h"
#include "nsWidgetsCID.h"
#include "nsGfxScrollFrame.h"
#include "nsScrollBoxFrame.h"
#include "nsLayoutAtoms.h"
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsISupportsArray.h"
#include "nsIDocument.h"
#include "nsIFontMetrics.h"
#include "nsIDocumentObserver.h"
#include "nsIDocument.h"
#include "nsBoxLayoutState.h"
#include "nsINodeInfo.h"
#include "nsIScrollbarFrame.h"
#include "nsIScrollbarMediator.h"
#include "nsITextControlFrame.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsNodeInfoManager.h"
#include "nsIURI.h"
#include "nsGUIEvent.h"
#include "nsContentCreatorFunctions.h"
#include "nsISupportsPrimitives.h"
#include "nsIPresShell.h"
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif
//----------------------------------------------------------------------

//----------nsHTMLScrollFrame-------------------------------------------

nsresult
NS_NewHTMLScrollFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRBool aIsRoot)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsHTMLScrollFrame* it = new (aPresShell) nsHTMLScrollFrame(aPresShell, aIsRoot);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsHTMLScrollFrame::nsHTMLScrollFrame(nsIPresShell* aShell, PRBool aIsRoot)
  : nsBoxFrame(aShell, aIsRoot),
    mInner(this)
{
    SetLayoutManager(nsnull);
}

/**
* Get the view that we are scrolling within the scrolling view. 
* @result child view
*/
nsIFrame* nsHTMLScrollFrame::GetScrolledFrame() const
{
  return mInner.GetScrolledFrame();
}

nsIScrollableView* nsHTMLScrollFrame::GetScrollableView()
{
  return mInner.GetScrollableView();
}

nsPoint nsHTMLScrollFrame::GetScrollPosition() const
{
   nsIScrollableView* s = mInner.GetScrollableView();
   nsPoint scrollPosition;
   s->GetScrollPosition(scrollPosition.x, scrollPosition.y);
   return scrollPosition;
}

void nsHTMLScrollFrame::ScrollTo(nsPoint aScrollPosition, PRUint32 aFlags)
{
   nsIScrollableView* s = mInner.GetScrollableView();
   s->ScrollTo(aScrollPosition.x, aScrollPosition.y, aFlags);
}

nsGfxScrollFrameInner::ScrollbarStyles
nsHTMLScrollFrame::GetScrollbarStyles() const {
  return mInner.GetScrollbarStylesFromFrame();
}

nsMargin nsHTMLScrollFrame::GetDesiredScrollbarSizes(nsBoxLayoutState* aState) {
  nsMargin result(0, 0, 0, 0);

  if (mInner.mHScrollbarBox) {
    nsSize size;
    mInner.mHScrollbarBox->GetPrefSize(*aState, size);
#ifdef IBMBIDI
    if (GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL)
      result.left = size.width;
    else
#endif
      result.right = size.width;
  }

  if (mInner.mVScrollbarBox) {
    nsSize size;
    mInner.mVScrollbarBox->GetPrefSize(*aState, size);
    // We don't currently support any scripts that would require a scrollbar
    // at the top. (Are there any?)
    result.bottom = size.height;
  }

  return result;
}

void nsHTMLScrollFrame::SetScrollbarVisibility(PRBool aVerticalVisible, PRBool aHorizontalVisible)
{
  mInner.mNeverHasVerticalScrollbar = !aVerticalVisible;
  mInner.mNeverHasHorizontalScrollbar = !aHorizontalVisible;
}

nsIBox* nsHTMLScrollFrame::GetScrollbarBox(PRBool aVertical)
{
  return aVertical ? mInner.mVScrollbarBox : mInner.mHScrollbarBox;
}

NS_IMETHODIMP
nsHTMLScrollFrame::CreateAnonymousContent(nsPresContext* aPresContext,
                                         nsISupportsArray& aAnonymousChildren)
{
  mInner.CreateAnonymousContent(aAnonymousChildren);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLScrollFrame::Destroy(nsPresContext* aPresContext)
{
  nsIScrollableView *view = mInner.GetScrollableView();
  NS_ASSERTION(view, "unexpected null pointer");
  if (view)
    view->RemoveScrollPositionListener(&mInner);
  return nsBoxFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
nsHTMLScrollFrame::SetInitialChildList(nsPresContext* aPresContext,
                                   nsIAtom*        aListName,
                                   nsIFrame*       aChildList)
{
  nsresult  rv = nsBoxFrame::SetInitialChildList(aPresContext, aListName,
                                                           aChildList);

  mInner.ReloadChildFrames();

  // listen for scroll events.
  mInner.GetScrollableView()->AddScrollPositionListener(&mInner);

  return rv;
}


NS_IMETHODIMP
nsHTMLScrollFrame::AppendFrames(nsPresContext* aPresContext,
                      nsIPresShell&   aPresShell,
                      nsIAtom*        aListName,
                      nsIFrame*       aFrameList)
{
  nsresult rv = nsBoxFrame::AppendFrames(aPresContext,
                                         aPresShell,
                                         aListName,
                                         aFrameList);
  mInner.ReloadChildFrames();
  return rv;
}

NS_IMETHODIMP
nsHTMLScrollFrame::InsertFrames(nsPresContext* aPresContext,
                      nsIPresShell&   aPresShell,
                      nsIAtom*        aListName,
                      nsIFrame*       aPrevFrame,
                      nsIFrame*       aFrameList)
{
  nsresult rv = nsBoxFrame::InsertFrames(aPresContext,
                                         aPresShell,
                                         aListName,
                                         aPrevFrame,
                                         aFrameList);
  mInner.ReloadChildFrames();
  return rv;
}

NS_IMETHODIMP
nsHTMLScrollFrame::RemoveFrame(nsPresContext* aPresContext,
                     nsIPresShell&   aPresShell,
                     nsIAtom*        aListName,
                     nsIFrame*       aOldFrame)
{
  nsresult rv = nsBoxFrame::RemoveFrame(aPresContext,
                                        aPresShell,
                                        aListName,
                                        aOldFrame);
  mInner.ReloadChildFrames();
  return rv;
}


NS_IMETHODIMP
nsHTMLScrollFrame::ReplaceFrame(nsPresContext* aPresContext,
                     nsIPresShell&   aPresShell,
                     nsIAtom*        aListName,
                     nsIFrame*       aOldFrame,
                     nsIFrame*       aNewFrame)
{
  nsresult rv = nsBoxFrame::ReplaceFrame(aPresContext,
                                         aPresShell,
                                         aListName,
                                         aOldFrame,
                                         aNewFrame);
  mInner.ReloadChildFrames();
  return rv;
}

NS_IMETHODIMP
nsHTMLScrollFrame::GetPadding(nsMargin& aMargin)
{
   aMargin.SizeTo(0,0,0,0);
   return NS_OK;
}

NS_IMETHODIMP
nsHTMLScrollFrame::GetContentAndOffsetsFromPoint(nsPresContext* aCX,
                                                const nsPoint&  aPoint,
                                                nsIContent **   aNewContent,
                                                PRInt32&        aContentOffset,
                                                PRInt32&        aContentOffsetEnd,
                                                PRBool&         aBeginFrameContent)
{
  nsPoint point(aPoint);
  //we need to translate the coordinates to the inner
  nsIView *view = GetClosestView();
  if (!view)
    return NS_ERROR_FAILURE;

  nsIView *innerView = GetClosestView();
  while (view != innerView && innerView)
  {
    point -= innerView->GetPosition();
    innerView = innerView->GetParent();
  }

  return mInner.mScrollAreaBox->GetContentAndOffsetsFromPoint(aCX, point, aNewContent, aContentOffset, aContentOffsetEnd, aBeginFrameContent);
}

PRIntn
nsHTMLScrollFrame::GetSkipSides() const
{
  return 0;
}

nsIAtom*
nsHTMLScrollFrame::GetType() const
{
  return nsLayoutAtoms::scrollFrame; 
}

NS_IMETHODIMP
nsHTMLScrollFrame::GetAscent(nsBoxLayoutState& aState, nscoord& aAscent)
{
  aAscent = 0;
  nsresult rv = mInner.mScrollAreaBox->GetAscent(aState, aAscent);
  nsMargin m(0,0,0,0);
  GetBorderAndPadding(m);
  aAscent += m.top;
  GetMargin(m);
  aAscent += m.top;
  GetInset(m);
  aAscent += m.top;

  return rv;
}

NS_IMETHODIMP
nsHTMLScrollFrame::GetPrefSize(nsBoxLayoutState& aState, nsSize& aSize)
{
#ifdef DEBUG_LAYOUT
  PropagateDebug(aState);
#endif

  nsGfxScrollFrameInner::ScrollbarStyles styles = GetScrollbarStyles();

  nsSize vSize(0,0);
  if (mInner.mVScrollbarBox &&
      styles.mVertical == NS_STYLE_OVERFLOW_SCROLL) {
     mInner.mVScrollbarBox->GetPrefSize(aState, vSize);
     nsBox::AddMargin(mInner.mVScrollbarBox, vSize);
  }
   
  nsSize hSize(0,0);
  if (mInner.mHScrollbarBox &&
      styles.mHorizontal == NS_STYLE_OVERFLOW_SCROLL) {
     mInner.mHScrollbarBox->GetPrefSize(aState, hSize);
     nsBox::AddMargin(mInner.mHScrollbarBox, hSize);
  }

  // If one of the width and height is constrained,
  // do smarter preferred size checking in case the scrolled frame is a block.
  // 
  // Details: We're going to pass our width (or height) constraint
  // down to nsBoxToBlockAdaptor.  Then when we call
  // mScrollAreaBox->GetPrefSize below, it will reflow the scrolled
  // block with this width (or height) constraint, and report the resulting
  // height (or width) of the block. So if possible we'll be sized exactly to the
  // height (or width) of the block, which is what we want because 'overflow'
  // should not affect sizing...

  // Push current constraint. We'll restore it when we're done.
  nsSize oldConstrainedSize = aState.ScrolledBlockSizeConstraint();

  const nsHTMLReflowState* HTMLState = aState.GetReflowState();
  // This stores the computed width and height, if available.
  nsSize computedSize(NS_INTRINSICSIZE, NS_INTRINSICSIZE);
  // This stores the maximum width and height we can be, if available.
  // This is what we use to constrain the block reflow.
  nsSize computedMax(NS_INTRINSICSIZE, NS_INTRINSICSIZE);
  if (HTMLState != nsnull) {
    computedSize = nsSize(HTMLState->mComputedWidth, HTMLState->mComputedHeight);
    // If we know the computed size, then that's the effective maximum
    computedMax = computedSize;
    // One could imagine using other constraints in computedMax, but it doesn't
    // really work. See bug 237622.
 
    // Pass a constraint to the block if we have at least one constraint
    // and our size is not fixed
    if (((computedSize.width == NS_INTRINSICSIZE)
         || (computedSize.height == NS_INTRINSICSIZE))
        && (computedMax.width != NS_INTRINSICSIZE
            || computedMax.height != NS_INTRINSICSIZE)) {
      // adjust constraints in case we have scrollbars
      if (computedMax.width != NS_INTRINSICSIZE) {
        computedMax.width = PR_MAX(0, computedMax.width - vSize.width);
      }
      if (computedMax.height != NS_INTRINSICSIZE) {
        computedMax.height = PR_MAX(0, computedMax.height - hSize.height);
      }
      // For now, constrained height seems to just confuse things because
      // various places in the block code assume constrained height => printing.
      // Disable height constraint.
      aState.SetScrolledBlockSizeConstraint(nsSize(computedMax.width, NS_INTRINSICSIZE));
    } else {
      aState.SetScrolledBlockSizeConstraint(nsSize(-1,-1));
    }
  } else {
    aState.SetScrolledBlockSizeConstraint(nsSize(-1,-1));
  }

  nsresult rv = mInner.mScrollAreaBox->GetPrefSize(aState, aSize);

  // Restore old constraint.
  aState.SetScrolledBlockSizeConstraint(oldConstrainedSize);

  // If our height is not constrained, and we will need a horizontal
  // scrollbar, then add space for the scrollbar to our desired height.
  if (computedSize.height == NS_INTRINSICSIZE
      && computedMax.width != NS_INTRINSICSIZE
      && aSize.width > computedMax.width
      && mInner.mHScrollbarBox
      && styles.mHorizontal == NS_STYLE_OVERFLOW_AUTO) {
    // Add height of horizontal scrollbar which will be needed
    mInner.mHScrollbarBox->GetPrefSize(aState, hSize);
    nsBox::AddMargin(mInner.mHScrollbarBox, hSize);
  }

  // If our width is not constrained, and we will need a vertical
  // scrollbar, then add space for the scrollbar to our desired width.
  if (computedSize.width == NS_INTRINSICSIZE
      && computedMax.height != NS_INTRINSICSIZE
      && aSize.height > computedMax.height
      && mInner.mVScrollbarBox
      && styles.mVertical == NS_STYLE_OVERFLOW_AUTO) {
    // Add width of vertical scrollbar which will be needed
    mInner.mVScrollbarBox->GetPrefSize(aState, vSize);
    nsBox::AddMargin(mInner.mVScrollbarBox, vSize);
  }

  nsBox::AddMargin(mInner.mScrollAreaBox, aSize);

  aSize.width += vSize.width;
  aSize.height += hSize.height;

  AddBorderAndPadding(aSize);
  AddInset(aSize);
  nsIBox::AddCSSPrefSize(aState, this, aSize);

  return rv;
}

NS_IMETHODIMP
nsHTMLScrollFrame::GetMinSize(nsBoxLayoutState& aState, nsSize& aSize)
{
#ifdef DEBUG_LAYOUT
  PropagateDebug(aState);
#endif

  nsresult rv = mInner.mScrollAreaBox->GetMinSize(aState, aSize);

  nsGfxScrollFrameInner::ScrollbarStyles styles = GetScrollbarStyles();
     
  if (mInner.mVScrollbarBox &&
      styles.mVertical == NS_STYLE_OVERFLOW_SCROLL) {
    nsSize vSize(0,0);
    mInner.mVScrollbarBox->GetMinSize(aState, vSize);
     AddMargin(mInner.mVScrollbarBox, vSize);
     aSize.width += vSize.width;
     if (aSize.height < vSize.height)
        aSize.height = vSize.height;
  }
        
  if (mInner.mHScrollbarBox &&
      styles.mHorizontal == NS_STYLE_OVERFLOW_SCROLL) {
     nsSize hSize(0,0);
     mInner.mHScrollbarBox->GetMinSize(aState, hSize);
     AddMargin(mInner.mHScrollbarBox, hSize);
     aSize.height += hSize.height;
     if (aSize.width < hSize.width)
        aSize.width = hSize.width;
  }

  AddBorderAndPadding(aSize);
  AddInset(aSize);
  nsIBox::AddCSSMinSize(aState, this, aSize);
  return rv;
}

NS_IMETHODIMP
nsHTMLScrollFrame::GetMaxSize(nsBoxLayoutState& aState, nsSize& aSize)
{
#ifdef DEBUG_LAYOUT
  PropagateDebug(aState);
#endif

  aSize.width = NS_INTRINSICSIZE;
  aSize.height = NS_INTRINSICSIZE;

  AddBorderAndPadding(aSize);
  AddInset(aSize);
  nsIBox::AddCSSMaxSize(aState, this, aSize);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLScrollFrame::Reflow(nsPresContext*      aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsHTMLScrollFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  // if there is a max element request then set it to -1 so we can see if it gets set
  if (aDesiredSize.mComputeMEW)
  {
    aDesiredSize.mMaxElementWidth = -1;
  }

  nsresult rv = nsBoxFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

  if (aDesiredSize.mComputeMEW)
  {
    nsStyleUnit widthUnit = GetStylePosition()->mWidth.GetUnit();
    if (widthUnit == eStyleUnit_Percent || widthUnit == eStyleUnit_Auto) {
      nsMargin border = aReflowState.mComputedBorderPadding;
      aDesiredSize.mMaxElementWidth = border.right + border.left;
      mInner.mMaxElementWidth = aDesiredSize.mMaxElementWidth;
    } else {
      // if not set then use the cached size. If set then set it.
      if (aDesiredSize.mMaxElementWidth == -1)
        aDesiredSize.mMaxElementWidth = mInner.mMaxElementWidth;
      else 
        mInner.mMaxElementWidth = aDesiredSize.mMaxElementWidth;
    }
  }
  
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return rv;
}

NS_IMETHODIMP_(nsrefcnt) 
nsHTMLScrollFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsHTMLScrollFrame::Release(void)
{
    return NS_OK;
}

#ifdef NS_DEBUG
NS_IMETHODIMP
nsHTMLScrollFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("HTMLScroll"), aResult);
}
#endif

#ifdef ACCESSIBILITY
NS_IMETHODIMP nsHTMLScrollFrame::GetAccessible(nsIAccessible** aAccessible)
{
  *aAccessible = nsnull;
  if (!IsFocusable()) {
    return NS_OK;
  }
  // Focusable via CSS, so needs to be in accessibility hierarchy
  nsCOMPtr<nsIAccessibilityService> accService = do_GetService("@mozilla.org/accessibilityService;1");

  if (accService) {
    return accService->CreateHTMLGenericAccessible(NS_STATIC_CAST(nsIFrame*, this), aAccessible);
  }

  return NS_ERROR_FAILURE;
}
#endif

void
nsHTMLScrollFrame::CurPosAttributeChanged(nsIContent* aChild,
                                          PRInt32 aModType)
{
  mInner.CurPosAttributeChanged(aChild, aModType);
}

NS_IMETHODIMP
nsHTMLScrollFrame::DoLayout(nsBoxLayoutState& aState)
{
  PRUint32 flags = aState.LayoutFlags();
  nsresult rv = mInner.Layout(aState);
  aState.SetLayoutFlags(flags);

  nsBox::DoLayout(aState);
  return rv;
}


nsresult 
nsHTMLScrollFrame::GetContentOf(nsIContent** aContent)
{
  *aContent = GetContent();
  NS_IF_ADDREF(*aContent);
  return NS_OK;
}

NS_INTERFACE_MAP_BEGIN(nsHTMLScrollFrame)
  NS_INTERFACE_MAP_ENTRY(nsIAnonymousContentCreator)
#ifdef NS_DEBUG
  NS_INTERFACE_MAP_ENTRY(nsIFrameDebug)
#endif
  NS_INTERFACE_MAP_ENTRY(nsIScrollableFrame)
  NS_INTERFACE_MAP_ENTRY(nsIScrollableViewProvider)
  NS_INTERFACE_MAP_ENTRY(nsIStatefulFrame)
NS_INTERFACE_MAP_END_INHERITING(nsBoxFrame)

//----------nsXULScrollFrame-------------------------------------------

nsresult
NS_NewXULScrollFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRBool aIsRoot)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsXULScrollFrame* it = new (aPresShell) nsXULScrollFrame(aPresShell, aIsRoot);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsXULScrollFrame::nsXULScrollFrame(nsIPresShell* aShell, PRBool aIsRoot)
  : nsBoxFrame(aShell, aIsRoot),
    mInner(this)
{
    SetLayoutManager(nsnull);
}

/**
* Get the view that we are scrolling within the scrolling view. 
* @result child view
*/
nsIFrame* nsXULScrollFrame::GetScrolledFrame() const
{
  return mInner.GetScrolledFrame();
}

nsIScrollableView* nsXULScrollFrame::GetScrollableView()
{
  return mInner.GetScrollableView();
}

nsPoint nsXULScrollFrame::GetScrollPosition() const
{
  nsIScrollableView* s = mInner.GetScrollableView();
  nsPoint scrollPosition;
  s->GetScrollPosition(scrollPosition.x, scrollPosition.y);
  return scrollPosition;
}

void nsXULScrollFrame::ScrollTo(nsPoint aScrollPosition, PRUint32 aFlags)
{
  nsIScrollableView* s = mInner.GetScrollableView();
  s->ScrollTo(aScrollPosition.x, aScrollPosition.y, aFlags);
}

nsGfxScrollFrameInner::ScrollbarStyles
nsXULScrollFrame::GetScrollbarStyles() const {
  return mInner.GetScrollbarStylesFromFrame();
}

nsMargin nsXULScrollFrame::GetDesiredScrollbarSizes(nsBoxLayoutState* aState) {
  nsMargin result(0, 0, 0, 0);

  if (mInner.mHScrollbarBox) {
    nsSize size;
    mInner.mHScrollbarBox->GetPrefSize(*aState, size);
#ifdef IBMBIDI
    if (GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL)
      result.left = size.width;
    else
#endif
      result.right = size.width;
  }

  if (mInner.mVScrollbarBox) {
    nsSize size;
    mInner.mVScrollbarBox->GetPrefSize(*aState, size);
    // We don't currently support any scripts that would require a scrollbar
    // at the top. (Are there any?)
    result.bottom = size.height;
  }

  return result;
}

void nsXULScrollFrame::SetScrollbarVisibility(PRBool aVerticalVisible, PRBool aHorizontalVisible)
{
  mInner.mNeverHasVerticalScrollbar = !aVerticalVisible;
  mInner.mNeverHasHorizontalScrollbar = !aHorizontalVisible;
}

nsIBox* nsXULScrollFrame::GetScrollbarBox(PRBool aVertical)
{
  return aVertical ? mInner.mVScrollbarBox : mInner.mHScrollbarBox;
}

NS_IMETHODIMP
nsXULScrollFrame::CreateAnonymousContent(nsPresContext* aPresContext,
                                         nsISupportsArray& aAnonymousChildren)
{
  mInner.CreateAnonymousContent(aAnonymousChildren);
  return NS_OK;
}

NS_IMETHODIMP
nsXULScrollFrame::Destroy(nsPresContext* aPresContext)
{
  nsIScrollableView *view = mInner.GetScrollableView();
  NS_ASSERTION(view, "unexpected null pointer");
  if (view)
    view->RemoveScrollPositionListener(&mInner);
  return nsBoxFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
nsXULScrollFrame::SetInitialChildList(nsPresContext* aPresContext,
                                   nsIAtom*        aListName,
                                   nsIFrame*       aChildList)
{
  nsresult  rv = nsBoxFrame::SetInitialChildList(aPresContext, aListName,
                                                           aChildList);

  mInner.ReloadChildFrames();

  // listen for scroll events.
  mInner.GetScrollableView()->AddScrollPositionListener(&mInner);

  return rv;
}


NS_IMETHODIMP
nsXULScrollFrame::AppendFrames(nsPresContext* aPresContext,
                      nsIPresShell&   aPresShell,
                      nsIAtom*        aListName,
                      nsIFrame*       aFrameList)
{
  nsresult rv = nsBoxFrame::AppendFrames(aPresContext,
                                         aPresShell,
                                         aListName,
                                         aFrameList);
  mInner.ReloadChildFrames();
  return rv;
}

NS_IMETHODIMP
nsXULScrollFrame::InsertFrames(nsPresContext* aPresContext,
                      nsIPresShell&   aPresShell,
                      nsIAtom*        aListName,
                      nsIFrame*       aPrevFrame,
                      nsIFrame*       aFrameList)
{
  nsresult rv = nsBoxFrame::InsertFrames(aPresContext,
                                         aPresShell,
                                         aListName,
                                         aPrevFrame,
                                         aFrameList);
  mInner.ReloadChildFrames();
  return rv;
}

NS_IMETHODIMP
nsXULScrollFrame::RemoveFrame(nsPresContext* aPresContext,
                     nsIPresShell&   aPresShell,
                     nsIAtom*        aListName,
                     nsIFrame*       aOldFrame)
{
  nsresult rv = nsBoxFrame::RemoveFrame(aPresContext,
                                        aPresShell,
                                        aListName,
                                        aOldFrame);
  mInner.ReloadChildFrames();
  return rv;
}


NS_IMETHODIMP
nsXULScrollFrame::ReplaceFrame(nsPresContext* aPresContext,
                     nsIPresShell&   aPresShell,
                     nsIAtom*        aListName,
                     nsIFrame*       aOldFrame,
                     nsIFrame*       aNewFrame)
{
  nsresult rv = nsBoxFrame::ReplaceFrame(aPresContext,
                                         aPresShell,
                                         aListName,
                                         aOldFrame,
                                         aNewFrame);
  mInner.ReloadChildFrames();
  return rv;
}

NS_IMETHODIMP
nsXULScrollFrame::GetPadding(nsMargin& aMargin)
{
   aMargin.SizeTo(0,0,0,0);
   return NS_OK;
}

NS_IMETHODIMP
nsXULScrollFrame::GetContentAndOffsetsFromPoint(nsPresContext* aCX,
                                                const nsPoint&  aPoint,
                                                nsIContent **   aNewContent,
                                                PRInt32&        aContentOffset,
                                                PRInt32&        aContentOffsetEnd,
                                                PRBool&         aBeginFrameContent)
{
  nsPoint point(aPoint);
  //we need to translate the coordinates to the inner
  nsIView *view = GetClosestView();
  if (!view)
    return NS_ERROR_FAILURE;

  nsIView *innerView = GetClosestView();
  while (view != innerView && innerView)
  {
    point -= innerView->GetPosition();
    innerView = innerView->GetParent();
  }

  return mInner.mScrollAreaBox->GetContentAndOffsetsFromPoint(aCX, point, aNewContent, aContentOffset, aContentOffsetEnd, aBeginFrameContent);
}

PRIntn
nsXULScrollFrame::GetSkipSides() const
{
  return 0;
}

nsIAtom*
nsXULScrollFrame::GetType() const
{
  return nsLayoutAtoms::scrollFrame; 
}

NS_IMETHODIMP
nsXULScrollFrame::GetAscent(nsBoxLayoutState& aState, nscoord& aAscent)
{
  aAscent = 0;
  nsresult rv = mInner.mScrollAreaBox->GetAscent(aState, aAscent);
  nsMargin m(0,0,0,0);
  GetBorderAndPadding(m);
  aAscent += m.top;
  GetMargin(m);
  aAscent += m.top;
  GetInset(m);
  aAscent += m.top;

  return rv;
}

NS_IMETHODIMP
nsXULScrollFrame::GetPrefSize(nsBoxLayoutState& aState, nsSize& aSize)
{
#ifdef DEBUG_LAYOUT
  PropagateDebug(aState);
#endif

  nsGfxScrollFrameInner::ScrollbarStyles styles = GetScrollbarStyles();

  nsSize vSize(0,0);
  if (mInner.mVScrollbarBox &&
      styles.mVertical == NS_STYLE_OVERFLOW_SCROLL) {
     mInner.mVScrollbarBox->GetPrefSize(aState, vSize);
     nsBox::AddMargin(mInner.mVScrollbarBox, vSize);
  }
   
  nsSize hSize(0,0);
  if (mInner.mHScrollbarBox &&
      styles.mHorizontal == NS_STYLE_OVERFLOW_SCROLL) {
     mInner.mHScrollbarBox->GetPrefSize(aState, hSize);
     nsBox::AddMargin(mInner.mHScrollbarBox, hSize);
  }

  // If one of the width and height is constrained,
  // do smarter preferred size checking in case the scrolled frame is a block.
  // 
  // Details: We're going to pass our width (or height) constraint
  // down to nsBoxToBlockAdaptor.  Then when we call
  // mScrollAreaBox->GetPrefSize below, it will reflow the scrolled
  // block with this width (or height) constraint, and report the resulting
  // height (or width) of the block. So if possible we'll be sized exactly to the
  // height (or width) of the block, which is what we want because 'overflow'
  // should not affect sizing...

  // Push current constraint. We'll restore it when we're done.
  nsSize oldConstrainedSize = aState.ScrolledBlockSizeConstraint();

  const nsHTMLReflowState* HTMLState = aState.GetReflowState();
  // This stores the computed width and height, if available.
  nsSize computedSize(NS_INTRINSICSIZE, NS_INTRINSICSIZE);
  // This stores the maximum width and height we can be, if available.
  // This is what we use to constrain the block reflow.
  nsSize computedMax(NS_INTRINSICSIZE, NS_INTRINSICSIZE);
  if (HTMLState != nsnull) {
    computedSize = nsSize(HTMLState->mComputedWidth, HTMLState->mComputedHeight);
    // If we know the computed size, then that's the effective maximum
    computedMax = computedSize;
    // One could imagine using other constraints in computedMax, but it doesn't
    // really work. See bug 237622.
 
    // Pass a constraint to the block if we have at least one constraint
    // and our size is not fixed
    if (((computedSize.width == NS_INTRINSICSIZE)
         || (computedSize.height == NS_INTRINSICSIZE))
        && (computedMax.width != NS_INTRINSICSIZE
            || computedMax.height != NS_INTRINSICSIZE)) {
      // adjust constraints in case we have scrollbars
      if (computedMax.width != NS_INTRINSICSIZE) {
        computedMax.width = PR_MAX(0, computedMax.width - vSize.width);
      }
      if (computedMax.height != NS_INTRINSICSIZE) {
        computedMax.height = PR_MAX(0, computedMax.height - hSize.height);
      }
      // For now, constrained height seems to just confuse things because
      // various places in the block code assume constrained height => printing.
      // Disable height constraint.
      aState.SetScrolledBlockSizeConstraint(nsSize(computedMax.width, NS_INTRINSICSIZE));
    } else {
      aState.SetScrolledBlockSizeConstraint(nsSize(-1,-1));
    }
  } else {
    aState.SetScrolledBlockSizeConstraint(nsSize(-1,-1));
  }

  nsresult rv = mInner.mScrollAreaBox->GetPrefSize(aState, aSize);

  // Restore old constraint.
  aState.SetScrolledBlockSizeConstraint(oldConstrainedSize);

  // If our height is not constrained, and we will need a horizontal
  // scrollbar, then add space for the scrollbar to our desired height.
  if (computedSize.height == NS_INTRINSICSIZE
      && computedMax.width != NS_INTRINSICSIZE
      && aSize.width > computedMax.width
      && mInner.mHScrollbarBox
      && styles.mHorizontal == NS_STYLE_OVERFLOW_AUTO) {
    // Add height of horizontal scrollbar which will be needed
    mInner.mHScrollbarBox->GetPrefSize(aState, hSize);
    nsBox::AddMargin(mInner.mHScrollbarBox, hSize);
  }

  // If our width is not constrained, and we will need a vertical
  // scrollbar, then add space for the scrollbar to our desired width.
  if (computedSize.width == NS_INTRINSICSIZE
      && computedMax.height != NS_INTRINSICSIZE
      && aSize.height > computedMax.height
      && mInner.mVScrollbarBox
      && styles.mVertical == NS_STYLE_OVERFLOW_AUTO) {
    // Add width of vertical scrollbar which will be needed
    mInner.mVScrollbarBox->GetPrefSize(aState, vSize);
    nsBox::AddMargin(mInner.mVScrollbarBox, vSize);
  }

  nsBox::AddMargin(mInner.mScrollAreaBox, aSize);

  aSize.width += vSize.width;
  aSize.height += hSize.height;

  AddBorderAndPadding(aSize);
  AddInset(aSize);
  nsIBox::AddCSSPrefSize(aState, this, aSize);

  return rv;
}

NS_IMETHODIMP
nsXULScrollFrame::GetMinSize(nsBoxLayoutState& aState, nsSize& aSize)
{
#ifdef DEBUG_LAYOUT
  PropagateDebug(aState);
#endif

  nsresult rv = mInner.mScrollAreaBox->GetMinSize(aState, aSize);

  nsGfxScrollFrameInner::ScrollbarStyles styles = GetScrollbarStyles();
     
  if (mInner.mVScrollbarBox &&
      styles.mVertical == NS_STYLE_OVERFLOW_SCROLL) {
    nsSize vSize(0,0);
    mInner.mVScrollbarBox->GetMinSize(aState, vSize);
     AddMargin(mInner.mVScrollbarBox, vSize);
     aSize.width += vSize.width;
     if (aSize.height < vSize.height)
        aSize.height = vSize.height;
  }
        
  if (mInner.mHScrollbarBox &&
      styles.mHorizontal == NS_STYLE_OVERFLOW_SCROLL) {
     nsSize hSize(0,0);
     mInner.mHScrollbarBox->GetMinSize(aState, hSize);
     AddMargin(mInner.mHScrollbarBox, hSize);
     aSize.height += hSize.height;
     if (aSize.width < hSize.width)
        aSize.width = hSize.width;
  }

  AddBorderAndPadding(aSize);
  AddInset(aSize);
  nsIBox::AddCSSMinSize(aState, this, aSize);
  return rv;
}

NS_IMETHODIMP
nsXULScrollFrame::GetMaxSize(nsBoxLayoutState& aState, nsSize& aSize)
{
#ifdef DEBUG_LAYOUT
  PropagateDebug(aState);
#endif

  aSize.width = NS_INTRINSICSIZE;
  aSize.height = NS_INTRINSICSIZE;

  AddBorderAndPadding(aSize);
  AddInset(aSize);
  nsIBox::AddCSSMaxSize(aState, this, aSize);
  return NS_OK;
}

NS_IMETHODIMP
nsXULScrollFrame::Reflow(nsPresContext*      aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsXULScrollFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  // if there is a max element request then set it to -1 so we can see if it gets set
  if (aDesiredSize.mComputeMEW)
  {
    aDesiredSize.mMaxElementWidth = -1;
  }

  nsresult rv = nsBoxFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

  if (aDesiredSize.mComputeMEW)
  {
    nsStyleUnit widthUnit = GetStylePosition()->mWidth.GetUnit();
    if (widthUnit == eStyleUnit_Percent || widthUnit == eStyleUnit_Auto) {
      nsMargin border = aReflowState.mComputedBorderPadding;
      aDesiredSize.mMaxElementWidth = border.right + border.left;
      mInner.mMaxElementWidth = aDesiredSize.mMaxElementWidth;
    } else {
      // if not set then use the cached size. If set then set it.
      if (aDesiredSize.mMaxElementWidth == -1)
        aDesiredSize.mMaxElementWidth = mInner.mMaxElementWidth;
      else 
        mInner.mMaxElementWidth = aDesiredSize.mMaxElementWidth;
    }
  }
  
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return rv;
}

NS_IMETHODIMP_(nsrefcnt) 
nsXULScrollFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsXULScrollFrame::Release(void)
{
    return NS_OK;
}

#ifdef NS_DEBUG
NS_IMETHODIMP
nsXULScrollFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("XULScroll"), aResult);
}
#endif

void nsXULScrollFrame::CurPosAttributeChanged(nsIContent* aChild, PRInt32 aModType)
{
  mInner.CurPosAttributeChanged(aChild, aModType);
}

NS_IMETHODIMP
nsXULScrollFrame::DoLayout(nsBoxLayoutState& aState)
{
  PRUint32 flags = aState.LayoutFlags();
  nsresult rv = mInner.Layout(aState);
  aState.SetLayoutFlags(flags);

  nsBox::DoLayout(aState);
  return rv;
}


nsresult 
nsXULScrollFrame::GetContentOf(nsIContent** aContent)
{
  *aContent = GetContent();
  NS_IF_ADDREF(*aContent);
  return NS_OK;
}

NS_INTERFACE_MAP_BEGIN(nsXULScrollFrame)
  NS_INTERFACE_MAP_ENTRY(nsIAnonymousContentCreator)
#ifdef NS_DEBUG
  NS_INTERFACE_MAP_ENTRY(nsIFrameDebug)
#endif
  NS_INTERFACE_MAP_ENTRY(nsIScrollableFrame)
  NS_INTERFACE_MAP_ENTRY(nsIScrollableViewProvider)
  NS_INTERFACE_MAP_ENTRY(nsIStatefulFrame)
NS_INTERFACE_MAP_END_INHERITING(nsBoxFrame)



//-------------------- Inner ----------------------

nsGfxScrollFrameInner::nsGfxScrollFrameInner(nsBoxFrame* aOuter)
  : mHScrollbarBox(nsnull),
    mVScrollbarBox(nsnull),
    mScrollAreaBox(nsnull),
    mScrollCornerBox(nsnull),
    mOnePixel(20),
    mOuter(aOuter),
    mMaxElementWidth(0),
    mRestoreRect(-1, -1, -1, -1),
    mLastPos(-1, -1),
    mLastDir(-1),
    mNeverHasVerticalScrollbar(PR_FALSE),
    mNeverHasHorizontalScrollbar(PR_FALSE),
    mHasVerticalScrollbar(PR_FALSE), 
    mHasHorizontalScrollbar(PR_FALSE),
    mViewInitiatedScroll(PR_FALSE),
    mFrameInitiatedScroll(PR_FALSE),
    mDidHistoryRestore(PR_FALSE)
{
}

NS_IMETHODIMP_(nsrefcnt) nsGfxScrollFrameInner::AddRef(void)
{
  return 1;
}

NS_IMETHODIMP_(nsrefcnt) nsGfxScrollFrameInner::Release(void)
{
  return 1;
}

NS_IMPL_QUERY_INTERFACE1(nsGfxScrollFrameInner, nsIScrollPositionListener)

static void HandleScrollPref(nsIScrollable *aScrollable, PRInt32 aOrientation,
                             PRUint8& aValue)
{
  PRInt32 pref;
  aScrollable->GetDefaultScrollbarPreferences(aOrientation, &pref);
  switch (pref) {
    case nsIScrollable::Scrollbar_Auto:
      // leave |aValue| untouched
      break;
    case nsIScrollable::Scrollbar_Never:
      aValue = NS_STYLE_OVERFLOW_HIDDEN;
      break;
    case nsIScrollable::Scrollbar_Always:
      aValue = NS_STYLE_OVERFLOW_SCROLL;
      break;
  }
}

nsGfxScrollFrameInner::ScrollbarStyles
nsGfxScrollFrameInner::GetScrollbarStylesFromFrame() const
{
  ScrollbarStyles result;
  nsIFrame* parent = mOuter->GetParent();
  if (parent && parent->GetType() == nsLayoutAtoms::viewportFrame &&
      // Make sure we're actually the root scrollframe
      parent->GetFirstChild(nsnull) ==
        NS_STATIC_CAST(const nsIFrame*, mOuter)) {
    nsPresContext *presContext = mOuter->GetPresContext();
    result = presContext->GetViewportOverflowOverride();

    nsCOMPtr<nsISupports> container = presContext->GetContainer();
    if (container) {
      nsCOMPtr<nsIScrollable> scrollable = do_QueryInterface(container);
      HandleScrollPref(scrollable, nsIScrollable::ScrollOrientation_X,
                       result.mHorizontal);
      HandleScrollPref(scrollable, nsIScrollable::ScrollOrientation_Y,
                       result.mVertical);
    }
  } else {
    const nsStyleDisplay *disp = mOuter->GetStyleDisplay();
    result.mHorizontal = disp->mOverflowX;
    result.mVertical = disp->mOverflowY;
  }

  NS_ASSERTION(result.mHorizontal != NS_STYLE_OVERFLOW_VISIBLE &&
               result.mHorizontal != NS_STYLE_OVERFLOW_CLIP &&
               result.mVertical != NS_STYLE_OVERFLOW_VISIBLE &&
               result.mVertical != NS_STYLE_OVERFLOW_CLIP,
               "scrollbars should not have been created");
  return result;
}

  /**
   * this code is resposible for restoring the scroll position back to some
   * saved positon. if the user has not moved the scroll position manually
   * we keep scrolling down until we get to our orignally position. keep in
   * mind that content could incrementally be coming in. we only want to stop
   * when we reach our new position.
   */
void
nsGfxScrollFrameInner::ScrollToRestoredPosition()
{
  nsIScrollableView* scrollingView = GetScrollableView();
  if (!scrollingView) {
    return;
  }
  if (mRestoreRect.y == -1 || mLastPos.x == -1 || mLastPos.y == -1) {
    return;
  }
  // make sure our scroll position did not change for where we last put
  // it. if it does then the user must have moved it, and we no longer
  // need to restore.
  nscoord x = 0;
  nscoord y = 0;
  scrollingView->GetScrollPosition(x, y);

  // if we didn't move, we still need to restore
  if (x == mLastPos.x && y == mLastPos.y) {
    nsRect childRect(0, 0, 0, 0);
    nsIView* child = nsnull;
    nsresult rv = scrollingView->GetScrolledView(child);
    if (NS_SUCCEEDED(rv) && child)
      childRect = child->GetBounds();

    PRInt32 cx, cy, x, y;
    scrollingView->GetScrollPosition(cx,cy);

    x = (int)
      (((float)childRect.width / mRestoreRect.width) * mRestoreRect.x);
    y = (int)
      (((float)childRect.height / mRestoreRect.height) * mRestoreRect.y);

    // if our position is greater than the scroll position, scroll.
    // remember that we could be incrementally loading so we may enter
    // and scroll many times.
    if (y > cy || x > cx) {
      scrollingView->ScrollTo(x, y, 0);
      // scrollpostion goes from twips to pixels. this fixes any roundoff
      // problems.
      scrollingView->GetScrollPosition(mLastPos.x, mLastPos.y);
    } else {
      // if we reached the position then stop
      mRestoreRect.y = -1;
      mLastPos.x = -1;
      mLastPos.y = -1;
    }
  } else {
    // user moved the position, so we won't need to restore
    mLastPos.x = -1;
    mLastPos.y = -1;
  }
}

void
nsGfxScrollFrameInner::ReloadChildFrames()
{
  mScrollAreaBox = nsnull;
  mHScrollbarBox = nsnull;
  mVScrollbarBox = nsnull;
  mScrollCornerBox = nsnull;

  nsIFrame* frame = mOuter->GetFirstChild(nsnull);
  while (frame) {
    PRBool understood = PR_FALSE;

    if (frame->IsBoxFrame()) {
      if (frame->GetType() == nsLayoutAtoms::scrollFrame) {
        NS_ASSERTION(!mScrollAreaBox, "Found multiple scroll areas?");
        mScrollAreaBox = frame;
        understood = PR_TRUE;
      } else {
        nsIContent* content = frame->GetContent();
        if (content) {
          nsAutoString value;
          if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None,
                                                            nsXULAtoms::orient, value)) {
            // probably a scrollbar then
            if (value.LowerCaseEqualsLiteral("horizontal")) {
              NS_ASSERTION(!mHScrollbarBox, "Found multiple horizontal scrollbars?");
              mHScrollbarBox = frame;
            } else {
              NS_ASSERTION(!mVScrollbarBox, "Found multiple vertical scrollbars?");
              mVScrollbarBox = frame;
            }
            understood = PR_TRUE;
          } else {
            // probably a scrollcorner
            NS_ASSERTION(!mScrollCornerBox, "Found multiple scrollcorners");
            mScrollCornerBox = frame;
            understood = PR_TRUE;
          }
        }
      }
    }

    NS_ASSERTION(understood, "What is this frame doing here?");

    frame = frame->GetNextSibling();
  }
}
  
void
nsGfxScrollFrameInner::CreateAnonymousContent(nsISupportsArray& aAnonymousChildren)
{
  nsPresContext* presContext = mOuter->GetPresContext();
  nsIFrame* parent = mOuter->GetParent();

  // Don't create scrollbars if we're printing/print previewing
  // Get rid of this code when printing moves to its own presentation
  if (presContext->IsPaginated()) {
    // allow scrollbars if this is the child of the viewport, because
    // we must be the scrollbars for the print preview window
    if (!parent || parent->GetType() != nsLayoutAtoms::viewportFrame) {
      mNeverHasVerticalScrollbar = mNeverHasHorizontalScrollbar = PR_TRUE;
      return;
    }
  }

  nsIScrollableFrame *scrollable;
  CallQueryInterface(mOuter, &scrollable);

  ScrollbarStyles styles = scrollable->GetScrollbarStyles();
  PRBool canHaveHorizontal = styles.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN;
  PRBool canHaveVertical = styles.mVertical != NS_STYLE_OVERFLOW_HIDDEN;
  if (!canHaveHorizontal && !canHaveVertical)
    // Nothing to do.
    return;

  // The anonymous <div> used by <inputs> never gets scrollbars.
  nsCOMPtr<nsITextControlFrame> textFrame(do_QueryInterface(parent));
  if (textFrame) {
    // Make sure we are not a text area.
    nsCOMPtr<nsIDOMHTMLTextAreaElement> textAreaElement(do_QueryInterface(parent->GetContent()));
    if (!textAreaElement) {
      mNeverHasVerticalScrollbar = mNeverHasHorizontalScrollbar = PR_TRUE;
      return;
    }
  }

  nsNodeInfoManager *nodeInfoManager =
    presContext->GetDocument()->NodeInfoManager();
  nsCOMPtr<nsINodeInfo> nodeInfo;
  nodeInfoManager->GetNodeInfo(nsXULAtoms::scrollbar, nsnull,
                               kNameSpaceID_XUL, getter_AddRefs(nodeInfo));

  nsCOMPtr<nsIContent> content;

  if (canHaveHorizontal) {
    NS_NewElement(getter_AddRefs(content), kNameSpaceID_XUL, nodeInfo);
    content->SetAttr(kNameSpaceID_None, nsXULAtoms::orient,
                     NS_LITERAL_STRING("horizontal"), PR_FALSE);
    aAnonymousChildren.AppendElement(content);
  }

  if (canHaveVertical) {
    NS_NewElement(getter_AddRefs(content), kNameSpaceID_XUL, nodeInfo);
    content->SetAttr(kNameSpaceID_None, nsXULAtoms::orient,
                     NS_LITERAL_STRING("vertical"), PR_FALSE);
    aAnonymousChildren.AppendElement(content);
  }

  if (canHaveHorizontal && canHaveVertical) {
    nodeInfoManager->GetNodeInfo(nsXULAtoms::scrollcorner, nsnull,
                                 kNameSpaceID_XUL, getter_AddRefs(nodeInfo));
    NS_NewElement(getter_AddRefs(content), kNameSpaceID_XUL, nodeInfo);
    aAnonymousChildren.AppendElement(content);
  }
}

NS_IMETHODIMP
nsGfxScrollFrameInner::ScrollPositionWillChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY)
{
   // Do nothing.
   return NS_OK;
}

/**
 * Called when someone (external or this frame) moves the scroll area.
 */
void
nsGfxScrollFrameInner::InternalScrollPositionDidChange(nscoord aX, nscoord aY)
{
  if (mVScrollbarBox)
    SetAttribute(mVScrollbarBox, nsXULAtoms::curpos, aY);
  
  if (mHScrollbarBox)
    SetAttribute(mHScrollbarBox, nsXULAtoms::curpos, aX);
}

/**
 * Called if something externally moves the scroll area
 * This can happen if the user pages up down or uses arrow keys
 * So what we need to do up adjust the scrollbars to match.
 */
NS_IMETHODIMP
nsGfxScrollFrameInner::ScrollPositionDidChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY)
{
  NS_ASSERTION(!mViewInitiatedScroll, "Cannot reenter ScrollPositionDidChange");

  mViewInitiatedScroll = PR_TRUE;

  InternalScrollPositionDidChange(aX, aY);

  mViewInitiatedScroll = PR_FALSE;
  
  return NS_OK;
}

void nsGfxScrollFrameInner::CurPosAttributeChanged(nsIContent* aContent, PRInt32 aModType)
{
  NS_ASSERTION(aContent, "aContent must not be null");

  // Attribute changes on the scrollbars happen in one of three ways:
  // 1) The scrollbar changed the attribute in response to some user event
  // 2) We changed the attribute in response to a ScrollPositionDidChange
  // callback from the scrolling view
  // 3) We changed the attribute to adjust the scrollbars for the start
  // of a smooth scroll operation
  //
  // In case 2), we don't need to scroll the view because the scrolling
  // has already happened. In case 3) we don't need to scroll because
  // we're just adjusting the scrollbars back to the correct setting
  // for the view.
  // 
  // We used to detect this case implicitly because we'd compare the
  // scrollbar attributes with the view's current scroll position and
  // bail out if they were equal. But that approach is fragile; it can
  // fail when, for example, the view scrolls horizontally and
  // vertically simultaneously; we'll get here when only the vertical
  // attribute has been set, so the attributes and the view scroll
  // position don't yet agree, and we'd tell the view to scroll to the
  // new vertical position and the old horizontal position! Even worse
  // things could happen when smooth scrolling got involved ... crashes
  // and other terrors.
  if (mViewInitiatedScroll || mFrameInitiatedScroll) return;

  nsIContent* vcontent = mVScrollbarBox ? mVScrollbarBox->GetContent() : nsnull;
  nsIContent* hcontent = mHScrollbarBox ? mHScrollbarBox->GetContent() : nsnull;

  if (hcontent == aContent || vcontent == aContent)
  {
    nscoord x = 0;
    nscoord y = 0;

    nsAutoString value;
    if (hcontent && NS_CONTENT_ATTR_HAS_VALUE == hcontent->GetAttr(kNameSpaceID_None, nsXULAtoms::curpos, value))
    {
      PRInt32 error;

      // convert it to an integer
      x = value.ToInteger(&error);
    }

    if (vcontent && NS_CONTENT_ATTR_HAS_VALUE == vcontent->GetAttr(kNameSpaceID_None, nsXULAtoms::curpos, value))
    {
      PRInt32 error;

      // convert it to an integer
      y = value.ToInteger(&error);
    }

    // Make sure the scrollbars indeed moved before firing the event.
    // I think it is OK to prevent the call to ScrollbarChanged()
    // if we didn't actually move. The following check is the first
    // thing ScrollbarChanged() does anyway, before deciding to move 
    // the scrollbars. 
    nscoord curPosX=0, curPosY=0;
    nsIScrollableView* s = GetScrollableView();
    if (s) {
      s->GetScrollPosition(curPosX, curPosY);
      if ((x*mOnePixel) == curPosX && (y*mOnePixel) == curPosY)
        return;

      PRBool isSmooth = aContent->HasAttr(kNameSpaceID_None, nsXULAtoms::smooth);
        
      if (isSmooth) {
        // Make sure an attribute-setting callback occurs even if the view didn't actually move yet
        // We need to make sure other listeners see that the scroll position is not (yet)
        // what they thought it was.
        s->GetScrollPosition(curPosX, curPosY);

        NS_ASSERTION(!mFrameInitiatedScroll, "Unexpected reentry");
        // Make sure we don't do anything in when the view calls us back for this
        // scroll operation.
        mFrameInitiatedScroll = PR_TRUE;
        InternalScrollPositionDidChange(curPosX, curPosY);
        mFrameInitiatedScroll = PR_FALSE;
      }
      ScrollbarChanged(mOuter->GetPresContext(), x*mOnePixel, y*mOnePixel, isSmooth ? NS_VMREFRESH_SMOOTHSCROLL : 0);

      // Fire the onScroll event now that we have scrolled
      nsIPresShell *presShell = mOuter->GetPresContext()->GetPresShell();
      if (presShell) {
        nsScrollbarEvent event(NS_SCROLL_EVENT);
        nsEventStatus status = nsEventStatus_eIgnore;
        // note if hcontent is non-null then hframe must be non-null.
        // likewise for vcontent and vframe. Thus targetFrame will always
        // be non-null in here.
        nsIFrame* targetFrame =
          hcontent == aContent ? mHScrollbarBox : mVScrollbarBox;
        presShell->HandleEventWithTarget(&event, targetFrame,
                                         aContent,
                                         NS_EVENT_FLAG_INIT, &status);
      }
    }
  }
}

nsIScrollableView*
nsGfxScrollFrameInner::GetScrollableView() const
{
  if (!mScrollAreaBox) {
    return nsnull;
  }

  nsIView* view = mScrollAreaBox->GetView();
  if (!view) return nsnull;

  nsIScrollableView* scrollingView;
#ifdef DEBUG
  nsresult result =
#endif
  CallQueryInterface(view, &scrollingView);
  NS_ASSERTION(NS_SUCCEEDED(result),
               "assertion gfx scrollframe does not contain a scrollframe");
  return scrollingView;
}

PRBool
nsGfxScrollFrameInner::AddHorizontalScrollbar(nsBoxLayoutState& aState, nsRect& aScrollAreaSize, PRBool aOnTop)
{
  if (!mHScrollbarBox)
    return PR_TRUE;

#ifdef IBMBIDI
  const nsStyleVisibility* vis = mOuter->GetStyleVisibility();

  // Scroll the view horizontally if:
  // 1)  We are creating the scrollbar for the first time and the
  //     horizontal scroll position of the view is 0 or
  // 2)  The display direction is changed
  PRBool needScroll;
  if (mLastDir == -1) {
    // Creating the scrollbar the first time
    nscoord curPosX = 0, curPosY = 0;      
    nsIScrollableView* s = GetScrollableView();
    if (s) {
      s->GetScrollPosition(curPosX, curPosY);
    }
    needScroll = (curPosX == 0);
  } else {
    needScroll = (mLastDir != vis->mDirection);
  }
  if (needScroll) {
    SetAttribute(mHScrollbarBox, nsXULAtoms::curpos,
                 (NS_STYLE_DIRECTION_LTR == vis->mDirection) ? 0 : 0x7FFFFFFF);
  }
  mLastDir = vis->mDirection;
#endif // IBMBIDI
  
  return AddRemoveScrollbar(aState, aScrollAreaSize, aOnTop, PR_TRUE, PR_TRUE);
}

PRBool
nsGfxScrollFrameInner::AddVerticalScrollbar(nsBoxLayoutState& aState, nsRect& aScrollAreaSize, PRBool aOnRight)
{
  if (!mVScrollbarBox)
    return PR_TRUE;

  return AddRemoveScrollbar(aState, aScrollAreaSize, aOnRight, PR_FALSE, PR_TRUE);
}

void
nsGfxScrollFrameInner::RemoveHorizontalScrollbar(nsBoxLayoutState& aState, nsRect& aScrollAreaSize, PRBool aOnTop)
{
  // removing a scrollbar should always fit
#ifdef DEBUG
  PRBool result =
#endif
  AddRemoveScrollbar(aState, aScrollAreaSize, aOnTop, PR_TRUE, PR_FALSE);
  NS_ASSERTION(result, "Removing horizontal scrollbar failed to fit??");
}

void
nsGfxScrollFrameInner::RemoveVerticalScrollbar(nsBoxLayoutState& aState, nsRect& aScrollAreaSize,  PRBool aOnRight)
{
  // removing a scrollbar should always fit
#ifdef DEBUG
  PRBool result =
#endif
  AddRemoveScrollbar(aState, aScrollAreaSize, aOnRight, PR_FALSE, PR_FALSE);
  NS_ASSERTION(result, "Removing vertical scrollbar failed to fit??");
}

PRBool
nsGfxScrollFrameInner::AddRemoveScrollbar(nsBoxLayoutState& aState, nsRect& aScrollAreaSize, PRBool aOnTop, PRBool aHorizontal, PRBool aAdd)
{
  if (aHorizontal) {
     if (mNeverHasHorizontalScrollbar || !mHScrollbarBox)
       return PR_FALSE;

     nsSize hSize;
     mHScrollbarBox->GetPrefSize(aState, hSize);
     nsBox::AddMargin(mHScrollbarBox, hSize);

     SetScrollbarVisibility(mHScrollbarBox, aAdd);

     PRBool hasHorizontalScrollbar;
     PRBool fit = AddRemoveScrollbar(hasHorizontalScrollbar, aScrollAreaSize.y, aScrollAreaSize.height, hSize.height, aOnTop, aAdd);
     mHasHorizontalScrollbar = hasHorizontalScrollbar;    // because mHasHorizontalScrollbar is a PRPackedBool
     if (!fit)
        SetScrollbarVisibility(mHScrollbarBox, !aAdd);

     return fit;
  } else {
     if (mNeverHasVerticalScrollbar || !mVScrollbarBox)
       return PR_FALSE;

     nsSize vSize;
     mVScrollbarBox->GetPrefSize(aState, vSize);
     nsBox::AddMargin(mVScrollbarBox, vSize);

     SetScrollbarVisibility(mVScrollbarBox, aAdd);

     PRBool hasVerticalScrollbar;
     PRBool fit = AddRemoveScrollbar(hasVerticalScrollbar, aScrollAreaSize.x, aScrollAreaSize.width, vSize.width, aOnTop, aAdd);
     mHasVerticalScrollbar = hasVerticalScrollbar;    // because mHasVerticalScrollbar is a PRPackedBool
     if (!fit)
        SetScrollbarVisibility(mVScrollbarBox, !aAdd);

     return fit;
  }
}

PRBool
nsGfxScrollFrameInner::AddRemoveScrollbar(PRBool& aHasScrollbar, nscoord& aXY, nscoord& aSize, nscoord aSbSize, PRBool aRightOrBottom, PRBool aAdd)
{ 
   nscoord size = aSize;
   nscoord xy = aXY;

   if (size != NS_INTRINSICSIZE) {
     if (aAdd) {
        size -= aSbSize;
        if (!aRightOrBottom && size >= 0)
          xy += aSbSize;
     } else {
        size += aSbSize;
        if (!aRightOrBottom)
          xy -= aSbSize;
     }
   }

   // not enough room? Yes? Return true.
   if (size >= 0) {
       aHasScrollbar = aAdd;
       aSize = size;
       aXY = xy;
       return PR_TRUE;
   }

   aHasScrollbar = PR_FALSE;
   return PR_FALSE;
}

nsresult
nsGfxScrollFrameInner::LayoutBox(nsBoxLayoutState& aState, nsIBox* aBox, const nsRect& aRect)
{
  return mOuter->LayoutChildAt(aState, aBox, aRect);
}

/**
 * When reflowing a HTML document where the content model is being created
 * The nsGfxScrollFrame will get an Initial reflow when the body is opened by the content sink.
 * But there isn't enough content to really reflow very much of the document
 * so it never needs to do layout for the scrollbars
 *
 * So later other reflows happen and these are Incremental reflows, and then the scrollbars
 * get reflowed. The important point here is that when they reflowed the ReflowState inside the 
 * BoxLayoutState contains an "Incremental" reason and never a "Initial" reason.
 *
 * When it reflows for Print Preview, the content model is already full constructed and it lays
 * out the entire document at that time. When it returns back here it discovers it needs scrollbars
 * and this is a problem because the ReflowState inside the BoxLayoutState still has a "Initial"
 * reason and if it does a Layout it is essentially asking everything to reflow yet again with
 * an "Initial" reason. This causes a lot of problems especially for tables.
 * 
 * The solution for this is to change the ReflowState's reason from Initial to Resize and let 
 * all the frames do what is necessary for a resize refow. Now, we only need to do this when 
 * it is doing PrintPreview and we need only do it for HTML documents and NOT chrome.
 *
 */
void
nsGfxScrollFrameInner::AdjustReflowStateForPrintPreview(nsBoxLayoutState& aState, PRBool& aSetBack)
{
  aSetBack = PR_FALSE;
  PRBool isChrome;
  PRBool isInitialPP = nsBoxFrame::IsInitialReflowForPrintPreview(aState, isChrome);
  if (isInitialPP && !isChrome) {
    // I know you shouldn't, but we cast away the "const" here
    nsHTMLReflowState* reflowState = (nsHTMLReflowState*)aState.GetReflowState();
    reflowState->reason = eReflowReason_Resize;
    aSetBack = PR_TRUE;
  }
}

/**
 * Sets reflow state back to Initial when we are done.
 */
void
nsGfxScrollFrameInner::AdjustReflowStateBack(nsBoxLayoutState& aState, PRBool aSetBack)
{
  // I know you shouldn't, but we cast away the "const" here
  nsHTMLReflowState* reflowState = (nsHTMLReflowState*)aState.GetReflowState();
  if (aSetBack && reflowState->reason == eReflowReason_Resize) {
    reflowState->reason = eReflowReason_Initial;
  }
}

/**
 * Reflow the scroll area if it needs it and return its size. Also determine if the reflow will
 * cause any of the scrollbars to need to be reflowed.
 */
nsresult
nsGfxScrollFrameInner::Layout(nsBoxLayoutState& aState)
{
  //TODO make bidi code set these from preferences

  // if true places the vertical scrollbar on the right false puts it on the left.
  PRBool scrollBarRight = PR_TRUE;

  // if true places the horizontal scrollbar on the bottom false puts it on the top.
  PRBool scrollBarBottom = PR_TRUE;

#ifdef IBMBIDI
  const nsStyleVisibility* vis = mOuter->GetStyleVisibility();

  //
  // Direction Style from this->GetStyleData()
  // now in (vis->mDirection)
  // ------------------
  // NS_STYLE_DIRECTION_LTR : LTR or Default
  // NS_STYLE_DIRECTION_RTL
  // NS_STYLE_DIRECTION_INHERIT
  //

  if (vis->mDirection == NS_STYLE_DIRECTION_RTL){
    // if true places the vertical scrollbar on the right false puts it on the left.
    scrollBarRight = PR_FALSE;

    // if true places the horizontal scrollbar on the bottom false puts it on the top.
    scrollBarBottom = PR_TRUE;
  }
  nsHTMLReflowState* reflowState = (nsHTMLReflowState*)aState.GetReflowState();
#endif // IBMBIDI

  // get the content rect
  nsRect clientRect(0,0,0,0);
  mOuter->GetClientRect(clientRect);

  // the scroll area size starts off as big as our content area
  nsRect scrollAreaRect(clientRect);

  /**************
   Our basic strategy here is to first try laying out the content with
   the scrollbars in their current state. We're hoping that that will
   just "work"; the content will overflow wherever there's a scrollbar
   already visible. If that does work, then there's no need to lay out
   the scrollarea. Otherwise we fix up the scrollbars; first we add a
   vertical one to scroll the content if necessary, or remove it if
   it's not needed. Then we reflow the content if the scrollbar
   changed.  Then we add a horizontal scrollbar if necessary (or
   remove if not needed), and if that changed, we reflow the content
   again. At this point, any scrollbars that are needed to scroll the
   content have been added.

   In the second phase we check to see if any scrollbars are too small
   to display, and if so, we remove them. We check the horizontal
   scrollbar first; removing it might make room for the vertical
   scrollbar, and if we have room for just one scrollbar we'll save
   the vertical one.

   Finally we position and size the scrollbars and scrollcorner (the
   square that is needed in the corner of the window when two
   scrollbars are visible), and reflow any fixed position views
   (if we're the viewport and we added or removed a scrollbar).
   **************/

  nsIScrollableFrame *scrollableFrame;
  CallQueryInterface(mOuter, &scrollableFrame);
  ScrollbarStyles styles = scrollableFrame->GetScrollbarStyles();

  // Look at our style do we always have vertical or horizontal scrollbars?
  if (styles.mHorizontal == NS_STYLE_OVERFLOW_SCROLL)
     mHasHorizontalScrollbar = PR_TRUE;
  if (styles.mVertical == NS_STYLE_OVERFLOW_SCROLL)
     mHasVerticalScrollbar = PR_TRUE;

  if (mHasHorizontalScrollbar)
     AddHorizontalScrollbar(aState, scrollAreaRect, scrollBarBottom);

  if (mHasVerticalScrollbar)
     AddVerticalScrollbar(aState, scrollAreaRect, scrollBarRight);
     
  nsRect oldScrollAreaBounds;
  mScrollAreaBox->GetClientRect(oldScrollAreaBounds);

  // layout our the scroll area
  LayoutBox(aState, mScrollAreaBox, scrollAreaRect);
  
  // now look at the content area and see if we need scrollbars or not
  PRBool needsLayout = PR_FALSE;
  nsSize scrolledContentSize(0,0);

  // if we have 'auto' scrollbars look at the vertical case
  if (styles.mVertical != NS_STYLE_OVERFLOW_SCROLL) {
      // get the area frame is the scrollarea
      scrolledContentSize = GetScrolledSize();

    // There are two cases to consider
      if (scrolledContentSize.height <= scrollAreaRect.height
          || styles.mVertical != NS_STYLE_OVERFLOW_AUTO) {
        if (mHasVerticalScrollbar) {
          // We left room for the vertical scrollbar, but it's not needed;
          // remove it.
          RemoveVerticalScrollbar(aState, scrollAreaRect, scrollBarRight);
          needsLayout = PR_TRUE;
          SetAttribute(mVScrollbarBox, nsXULAtoms::curpos, 0);
        }
      } else {
        if (!mHasVerticalScrollbar) {
          // We didn't leave room for the vertical scrollbar, but it turns
          // out we needed it
          if (AddVerticalScrollbar(aState, scrollAreaRect, scrollBarRight))
            needsLayout = PR_TRUE;
        }
    }

    // ok layout at the right size
    if (needsLayout) {
       nsBoxLayoutState resizeState(aState);
       resizeState.SetLayoutReason(nsBoxLayoutState::Resize);
       PRBool setBack;
       AdjustReflowStateForPrintPreview(aState, setBack);
       LayoutBox(resizeState, mScrollAreaBox, scrollAreaRect);
       AdjustReflowStateBack(aState, setBack);
       needsLayout = PR_FALSE;
    }
  }


  // if scrollbars are auto look at the horizontal case
  if (styles.mHorizontal != NS_STYLE_OVERFLOW_SCROLL)
  {
    // get the area frame is the scrollarea
    scrolledContentSize = GetScrolledSize();

    // if the child is wider that the scroll area
    // and we don't have a scrollbar add one.
    if (scrolledContentSize.width > scrollAreaRect.width
        && styles.mHorizontal == NS_STYLE_OVERFLOW_AUTO) {

      if (!mHasHorizontalScrollbar) {
           // no scrollbar? 
          if (AddHorizontalScrollbar(aState, scrollAreaRect, scrollBarBottom))
             needsLayout = PR_TRUE;

           // if we added a horizonal scrollbar and we did not have a vertical
           // there is a chance that by adding the horizonal scrollbar we will
           // suddenly need a vertical scrollbar. Is a special case but its 
           // important.
           //if (!mHasVerticalScrollbar && scrolledContentSize.height > scrollAreaRect.height - sbSize.height)
           //  printf("****Gfx Scrollbar Special case hit!!*****\n");
           
      }
#ifdef IBMBIDI
      const nsStyleVisibility* ourVis = mOuter->GetStyleVisibility();

      if (NS_STYLE_DIRECTION_RTL == ourVis->mDirection) {
        nsCOMPtr<nsITextControlFrame> textControl =
          do_QueryInterface(mOuter->GetParent());
        if (textControl) {
          needsLayout = PR_TRUE;
          reflowState->mRightEdge = scrolledContentSize.width;
          mScrollAreaBox->MarkDirty(aState);
        }
      }
#endif // IBMBIDI
    } else {
        // if the area is smaller or equal to and we have a scrollbar then
        // remove it.
      if (mHasHorizontalScrollbar) {
        RemoveHorizontalScrollbar(aState, scrollAreaRect, scrollBarBottom);
        needsLayout = PR_TRUE;
        SetAttribute(mHScrollbarBox, nsXULAtoms::curpos, 0);
      }
    }
  }

  // we only need to set the rect. The inner child stays the same size.
  if (needsLayout) {
     nsBoxLayoutState resizeState(aState);
     resizeState.SetLayoutReason(nsBoxLayoutState::Resize);
     PRBool setBack;
     AdjustReflowStateForPrintPreview(aState, setBack);
     LayoutBox(resizeState, mScrollAreaBox, scrollAreaRect); 
     AdjustReflowStateBack(aState, setBack);
     needsLayout = PR_FALSE;
#ifdef IBMBIDI
     reflowState->mRightEdge = NS_UNCONSTRAINEDSIZE;
#endif // IBMBIDI
  }
    
  scrolledContentSize = GetScrolledSize();

  nsPresContext* presContext = aState.PresContext();
  mOnePixel = presContext->IntScaledPixelsToTwips(1);
  const nsStyleFont* font = mOuter->GetStyleFont();
  const nsFont& f = font->mFont;
  nsCOMPtr<nsIFontMetrics> fm = presContext->GetMetricsFor(f);
  nscoord fontHeight = 1;
  NS_ASSERTION(fm,"FontMetrics is null assuming fontHeight == 1");
  if (fm)
    fm->GetHeight(fontHeight);

  // get the preferred size of the scrollbars
  nsSize hSize(0,0);
  nsSize vSize(0,0);
  nsSize hMinSize(0,0);
  nsSize vMinSize(0,0);

  if (mHScrollbarBox && mHasHorizontalScrollbar) {
    mHScrollbarBox->GetPrefSize(aState, hSize);
    mHScrollbarBox->GetMinSize(aState, hMinSize);
    nsBox::AddMargin(mHScrollbarBox, hSize);
    nsBox::AddMargin(mHScrollbarBox, hMinSize);
  }
  if (mVScrollbarBox && mHasVerticalScrollbar) {
    mVScrollbarBox->GetPrefSize(aState, vSize);
    mVScrollbarBox->GetMinSize(aState, vMinSize);
    nsBox::AddMargin(mVScrollbarBox, vSize);
    nsBox::AddMargin(mVScrollbarBox, vMinSize);
  }

  // Disable scrollbars that are too small
  // Disable horizontal scrollbar first. If we have to disable only one
  // scrollbar, we'd rather keep the vertical scrollbar.
  // Note that we always give horizontal scrollbars their preferred height,
  // never their min-height. So check that there's room for the preferred height.
  if (mHasHorizontalScrollbar &&
      (hMinSize.width > clientRect.width - vSize.width
       || hSize.height > clientRect.height)) {
    RemoveHorizontalScrollbar(aState, scrollAreaRect, scrollBarBottom);
    needsLayout = PR_TRUE;
    SetAttribute(mHScrollbarBox, nsXULAtoms::curpos, 0);
    // set the scrollbar to zero height to make it go away
    hSize.height = 0;
    hMinSize.height = 0;
  }
  // Now disable vertical scrollbar if necessary
  if (mHasVerticalScrollbar &&
      (vMinSize.height > clientRect.height - hSize.height
       || vSize.width > clientRect.width)) {
    RemoveVerticalScrollbar(aState, scrollAreaRect, scrollBarRight);
    needsLayout = PR_TRUE;
    SetAttribute(mVScrollbarBox, nsXULAtoms::curpos, 0);
    // set the scrollbar to zero width to make it go away
    vSize.width = 0;
    vMinSize.width = 0;
  }

  nscoord maxX = scrolledContentSize.width - scrollAreaRect.width;
  nscoord maxY = scrolledContentSize.height - scrollAreaRect.height;

  nsIScrollableView* scrollable = GetScrollableView();
  scrollable->SetLineHeight(fontHeight);

  if (mVScrollbarBox) {
    SetScrollbarEnabled(mVScrollbarBox, maxY);
    SetAttribute(mVScrollbarBox, nsXULAtoms::maxpos, maxY);
    SetAttribute(mVScrollbarBox, nsXULAtoms::pageincrement, nscoord(scrollAreaRect.height - fontHeight));
    SetAttribute(mVScrollbarBox, nsXULAtoms::increment, fontHeight);

    nsRect vRect(scrollAreaRect);
    vRect.width = vSize.width;
    vRect.x = scrollBarRight ? scrollAreaRect.XMost() : clientRect.x;
    nsMargin margin;
    mVScrollbarBox->GetMargin(margin);
    vRect.Deflate(margin);
    LayoutBox(aState, mVScrollbarBox, vRect);
  }
    
  if (mHScrollbarBox) {
    SetScrollbarEnabled(mHScrollbarBox, maxX);
    SetAttribute(mHScrollbarBox, nsXULAtoms::maxpos, maxX);
    SetAttribute(mHScrollbarBox, nsXULAtoms::pageincrement, nscoord(float(scrollAreaRect.width)*0.8));
    SetAttribute(mHScrollbarBox, nsXULAtoms::increment, 10*mOnePixel);

    nsRect hRect(scrollAreaRect);
    hRect.height = hSize.height;
    hRect.y = scrollBarBottom ? scrollAreaRect.YMost() : clientRect.y;
    nsMargin margin;
    mHScrollbarBox->GetMargin(margin);
    hRect.Deflate(margin);
    LayoutBox(aState, mHScrollbarBox, hRect);
  } 

  // we only need to set the rect. The inner child stays the same size.
  if (needsLayout) {
     nsBoxLayoutState resizeState(aState);
     resizeState.SetLayoutReason(nsBoxLayoutState::Resize);
     LayoutBox(resizeState, mScrollAreaBox, scrollAreaRect); 
     needsLayout = PR_FALSE;
  }

  // place the scrollcorner
  if (mScrollCornerBox) {
    nsRect r(0, 0, 0, 0);
    if (clientRect.x != scrollAreaRect.x) {
      // scrollbar (if any) on left
      r.x = clientRect.x;
      r.width = scrollAreaRect.x - clientRect.x;
      NS_ASSERTION(r.width >= 0, "Scroll area should be inside client rect");
    } else {
      // scrollbar (if any) on right
      r.x = scrollAreaRect.XMost();
      r.width = clientRect.XMost() - scrollAreaRect.XMost();
      NS_ASSERTION(r.width >= 0, "Scroll area should be inside client rect");
    }
    if (clientRect.y != scrollAreaRect.y) {
      // scrollbar (if any) on top
      r.y = clientRect.y;
      r.height = scrollAreaRect.y - clientRect.y;
      NS_ASSERTION(r.height >= 0, "Scroll area should be inside client rect");
    } else {
      // scrollbar (if any) on bottom
      r.y = scrollAreaRect.YMost();
      r.height = clientRect.YMost() - scrollAreaRect.YMost();
      NS_ASSERTION(r.height >= 0, "Scroll area should be inside client rect");
    }
    LayoutBox(aState, mScrollCornerBox, r); 
  }

  // may need to update fixed position children of the viewport,
  // if the client area changed size because of some dirty reflow
  // (if the reflow is initial or resize, the fixed children will
  // be re-laid out anyway)
  if ((oldScrollAreaBounds.width != scrollAreaRect.width
      || oldScrollAreaBounds.height != scrollAreaRect.height)
      && nsBoxLayoutState::Dirty == aState.LayoutReason()) {
    nsIFrame* parentFrame = mOuter->GetParent();
    if (parentFrame) {
      if (parentFrame->GetType() == nsLayoutAtoms::viewportFrame) {
        // Usually there are no fixed children, so don't do anything unless there's
        // at least one fixed child
        if (parentFrame->GetFirstChild(nsLayoutAtoms::fixedList)) {
          // force a reflow of the fixed children
          nsFrame::CreateAndPostReflowCommand(mOuter->GetPresContext()->PresShell(),
            parentFrame,
            eReflowType_UserDefined, nsnull, nsnull, nsLayoutAtoms::fixedList);
        }
      }
    }
  }
  
  ScrollToRestoredPosition();

  return NS_OK;
}

void
nsGfxScrollFrameInner::ScrollbarChanged(nsPresContext* aPresContext, nscoord aX, nscoord aY, PRUint32 aFlags)
{
  nsIScrollableView* scrollable = GetScrollableView();
  scrollable->ScrollTo(aX, aY, aFlags);
 // printf("scrolling to: %d, %d\n", aX, aY);
}

void
nsGfxScrollFrameInner::SetScrollbarEnabled(nsIBox* aBox, nscoord aMaxPos, PRBool aReflow)
{
  mOuter->GetPresContext()->PresShell()->PostAttributeChange(
    aBox->GetContent(),
    kNameSpaceID_None,
    nsHTMLAtoms::disabled,
    NS_LITERAL_STRING("true"),
    aReflow,
    aMaxPos ? eChangeType_Remove : eChangeType_Set);
}

/**
 * Returns whether it actually needed to change the attribute
 */
PRBool
nsGfxScrollFrameInner::SetAttribute(nsIBox* aBox, nsIAtom* aAtom, nscoord aSize, PRBool aReflow)
{
  // convert to pixels
  aSize /= mOnePixel;

  // only set the attribute if it changed.

  PRInt32 current = GetIntegerAttribute(aBox, aAtom, -1);
  if (current != aSize)
  {
      nsAutoString newValue;
      newValue.AppendInt(aSize);
      aBox->GetContent()->SetAttr(kNameSpaceID_None, aAtom, newValue, aReflow);
      return PR_TRUE;
  }

  return PR_FALSE;
}

/**
 * Gets the size of the area that lies inside the scrollbars but clips the scrolled frame
 */
nsSize
nsGfxScrollFrameInner::GetScrolledSize() const
{
  // our scrolled size is the size of our scrolled view.
  nsIBox* child = nsnull;
  mScrollAreaBox->GetChildBox(&child);
  nsIView* view = child->GetView();
  NS_ASSERTION(view,"Scrolled frame must have a view!!!");
  
  nsRect rect = view->GetBounds();
  nsSize size(rect.width, rect.height);
 
  nsBox::AddMargin(child, size);
  nsBox::AddBorderAndPadding(mScrollAreaBox, size);
  nsBox::AddInset(mScrollAreaBox, size);

  return size;
}

nsMargin
nsGfxScrollFrameInner::GetActualScrollbarSizes() const {
  nsRect contentRect;
  mOuter->GetClientRect(contentRect);
  nsRect scrollArea = mScrollAreaBox->GetRect();

  return nsMargin(scrollArea.x - contentRect.x, scrollArea.y - contentRect.y,
                  contentRect.XMost() - scrollArea.XMost(),
                  contentRect.YMost() - scrollArea.YMost());
}

void
nsGfxScrollFrameInner::SetScrollbarVisibility(nsIBox* aScrollbar, PRBool aVisible)
{
  if (!aScrollbar)
    return;

  nsCOMPtr<nsIScrollbarFrame> scrollbar(do_QueryInterface(aScrollbar));
  if (scrollbar) {
    // See if we have a mediator.
    nsCOMPtr<nsIScrollbarMediator> mediator;
    scrollbar->GetScrollbarMediator(getter_AddRefs(mediator));
    if (mediator) {
      // Inform the mediator of the visibility change.
      mediator->VisibilityChanged(scrollbar, aVisible);
    }
  }
}

PRInt32
nsGfxScrollFrameInner::GetIntegerAttribute(nsIBox* aBox, nsIAtom* atom, PRInt32 defaultValue)
{
    nsIContent* content = aBox->GetContent();

    nsAutoString value;
    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, atom, value))
    {
      PRInt32 error;

      // convert it to an integer
      defaultValue = value.ToInteger(&error);
    }

    return defaultValue;
}

already_AddRefed<nsIPresState>
nsGfxScrollFrameInner::SaveState()
{
  nsCOMPtr<nsIScrollbarMediator> mediator;
  nsIFrame* first = GetScrolledFrame();
  mediator = do_QueryInterface(first);
  if (mediator) {
    // Child manages its own scrolling. Bail.
    return nsnull;
  }

  nsIScrollableView* scrollingView = GetScrollableView();
  PRInt32 x,y;
  scrollingView->GetScrollPosition(x,y);
  // Don't save scroll position if we are at (0,0)
  if (!x && !y) {
    return nsnull;
  }

  nsIView* child = nsnull;
  scrollingView->GetScrolledView(child);
  if (!child) {
    return nsnull;
  }

  nsRect childRect = child->GetBounds();
  nsCOMPtr<nsIPresState> state;
  nsresult rv = NS_NewPresState(getter_AddRefs(state));
  NS_ENSURE_SUCCESS(rv, nsnull);

  nsCOMPtr<nsISupportsPRInt32> xoffset;
  nsComponentManager::CreateInstance(NS_SUPPORTS_PRINT32_CONTRACTID,
    nsnull, NS_GET_IID(nsISupportsPRInt32), (void**)getter_AddRefs(xoffset));
  if (xoffset) {
    rv = xoffset->SetData(x);
    NS_ENSURE_SUCCESS(rv, nsnull);
    state->SetStatePropertyAsSupports(NS_LITERAL_STRING("x-offset"), xoffset);
  }

  nsCOMPtr<nsISupportsPRInt32> yoffset;
  nsComponentManager::CreateInstance(NS_SUPPORTS_PRINT32_CONTRACTID,
    nsnull, NS_GET_IID(nsISupportsPRInt32), (void**)getter_AddRefs(yoffset));
  if (yoffset) {
    rv = yoffset->SetData(y);
    NS_ENSURE_SUCCESS(rv, nsnull);
    state->SetStatePropertyAsSupports(NS_LITERAL_STRING("y-offset"), yoffset);
  }

  nsCOMPtr<nsISupportsPRInt32> width;
  nsComponentManager::CreateInstance(NS_SUPPORTS_PRINT32_CONTRACTID,
    nsnull, NS_GET_IID(nsISupportsPRInt32), (void**)getter_AddRefs(width));
  if (width) {
    rv = width->SetData(childRect.width);
    NS_ENSURE_SUCCESS(rv, nsnull);
    state->SetStatePropertyAsSupports(NS_LITERAL_STRING("width"), width);
  }

  nsCOMPtr<nsISupportsPRInt32> height;
  nsComponentManager::CreateInstance(NS_SUPPORTS_PRINT32_CONTRACTID,
    nsnull, NS_GET_IID(nsISupportsPRInt32), (void**)getter_AddRefs(height));
  if (height) {
    rv = height->SetData(childRect.height);
    NS_ENSURE_SUCCESS(rv, nsnull);
    state->SetStatePropertyAsSupports(NS_LITERAL_STRING("height"), height);
  }
  nsIPresState* result = state;
  NS_ADDREF(result);
  return result;
}

void
nsGfxScrollFrameInner::RestoreState(nsIPresState* aState)
{
  nsCOMPtr<nsISupportsPRInt32> xoffset;
  nsCOMPtr<nsISupportsPRInt32> yoffset;
  nsCOMPtr<nsISupportsPRInt32> width;
  nsCOMPtr<nsISupportsPRInt32> height;
  aState->GetStatePropertyAsSupports(NS_LITERAL_STRING("x-offset"), getter_AddRefs(xoffset));
  aState->GetStatePropertyAsSupports(NS_LITERAL_STRING("y-offset"), getter_AddRefs(yoffset));
  aState->GetStatePropertyAsSupports(NS_LITERAL_STRING("width"), getter_AddRefs(width));
  aState->GetStatePropertyAsSupports(NS_LITERAL_STRING("height"), getter_AddRefs(height));

  if (xoffset && yoffset) {
    PRInt32 x,y,w,h;
    nsresult rv = xoffset->GetData(&x);
    if (NS_SUCCEEDED(rv))
      rv = yoffset->GetData(&y);
    if (NS_SUCCEEDED(rv))
      rv = width->GetData(&w);
    if (NS_SUCCEEDED(rv))
      rv = height->GetData(&h);

    mLastPos.x = -1;
    mLastPos.y = -1;
    mRestoreRect.SetRect(-1, -1, -1, -1);

    // don't do it now, store it later and do it in layout.
    if (NS_SUCCEEDED(rv)) {
      mRestoreRect.SetRect(x, y, w, h);
      mDidHistoryRestore = PR_TRUE;
      nsIScrollableView* scrollingView = GetScrollableView();
      if (scrollingView) {
        scrollingView->GetScrollPosition(mLastPos.x, mLastPos.y);
      } else {
        // Our scrollboxframe probably hasn't been constructed yet.
        mLastPos = nsPoint(0, 0);
      }
    }
  }
}
