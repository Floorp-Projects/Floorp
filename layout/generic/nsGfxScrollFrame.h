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

/* rendering object to wrap rendering objects that should be scrollable */

#ifndef nsGfxScrollFrame_h___
#define nsGfxScrollFrame_h___

#include "nsHTMLContainerFrame.h"
#include "nsIAnonymousContentCreator.h"
#include "nsBoxFrame.h"
#include "nsIScrollableFrame.h"
#include "nsIScrollPositionListener.h"
#include "nsIStatefulFrame.h"
#include "nsThreadUtils.h"
#include "nsIReflowCallback.h"
#include "nsBoxLayoutState.h"
#include "nsQueryFrame.h"
#include "nsCOMArray.h"
#ifdef MOZ_SVG
#include "nsSVGIntegrationUtils.h"
#endif
#include "nsExpirationTracker.h"

class nsPresContext;
class nsIPresShell;
class nsIContent;
class nsIAtom;
class nsIDocument;
class nsIScrollFrameInternal;
class nsPresState;
struct ScrollReflowState;

class nsGfxScrollFrameInner : public nsIReflowCallback {
public:
  class AsyncScroll;

  nsGfxScrollFrameInner(nsContainerFrame* aOuter, PRBool aIsRoot,
                        PRBool aIsXUL);
  ~nsGfxScrollFrameInner();

  typedef nsIScrollableFrame::ScrollbarStyles ScrollbarStyles;
  ScrollbarStyles GetScrollbarStylesFromFrame() const;

  // If a child frame was added or removed on the scrollframe,
  // reload our child frame list.
  // We need this if a scrollbar frame is recreated.
  void ReloadChildFrames();

  nsresult CreateAnonymousContent(nsTArray<nsIContent*>& aElements);
  void AppendAnonymousContentTo(nsBaseContentList& aElements);
  nsresult FireScrollPortEvent();
  void PostOverflowEvent();
  void Destroy();

  nsresult BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                            const nsRect&           aDirtyRect,
                            const nsDisplayListSet& aLists);

  PRBool GetBorderRadii(nscoord aRadii[8]) const;

  // nsIReflowCallback
  virtual PRBool ReflowFinished();
  virtual void ReflowCallbackCanceled();

  // This gets called when the 'curpos' attribute on one of the scrollbars changes
  void CurPosAttributeChanged(nsIContent* aChild);
  void PostScrollEvent();
  void FireScrollEvent();
  void PostScrolledAreaEvent();
  void FireScrolledAreaEvent();

  class ScrollEvent : public nsRunnable {
  public:
    NS_DECL_NSIRUNNABLE
    ScrollEvent(nsGfxScrollFrameInner *inner) : mInner(inner) {}
    void Revoke() { mInner = nsnull; }
  private:
    nsGfxScrollFrameInner *mInner;
  };

  class AsyncScrollPortEvent : public nsRunnable {
  public:
    NS_DECL_NSIRUNNABLE
    AsyncScrollPortEvent(nsGfxScrollFrameInner *inner) : mInner(inner) {}
    void Revoke() { mInner = nsnull; }
  private:
    nsGfxScrollFrameInner *mInner;
  };

  class ScrolledAreaEvent : public nsRunnable {
  public:
    NS_DECL_NSIRUNNABLE
    ScrolledAreaEvent(nsGfxScrollFrameInner *inner) : mInner(inner) {}
    void Revoke() { mInner = nsnull; }
  private:
    nsGfxScrollFrameInner *mInner;
  };

  static void FinishReflowForScrollbar(nsIContent* aContent, nscoord aMinXY,
                                       nscoord aMaxXY, nscoord aCurPosXY,
                                       nscoord aPageIncrement,
                                       nscoord aIncrement);
  static void SetScrollbarEnabled(nsIContent* aContent, nscoord aMaxPos);
  static void SetCoordAttribute(nsIContent* aContent, nsIAtom* aAtom,
                                nscoord aSize);
  nscoord GetCoordAttribute(nsIBox* aFrame, nsIAtom* atom, nscoord defaultValue);

  // Update scrollbar curpos attributes to reflect current scroll position
  void UpdateScrollbarPosition();

  nsRect GetScrollPortRect() const { return mScrollPort; }
  nsPoint GetScrollPosition() const {
    return mScrollPort.TopLeft() - mScrolledFrame->GetPosition();
  }
  nsRect GetScrollRange() const;

  nsPoint ClampAndRestrictToDevPixels(const nsPoint& aPt, nsIntPoint* aPtDevPx) const;
  nsPoint ClampScrollPosition(const nsPoint& aPt) const;
  static void AsyncScrollCallback(nsITimer *aTimer, void* anInstance);
  void ScrollTo(nsPoint aScrollPosition, nsIScrollableFrame::ScrollMode aMode);
  void ScrollToImpl(nsPoint aScrollPosition);
  void ScrollVisual(nsIntPoint aPixDelta);
  void ScrollBy(nsIntPoint aDelta, nsIScrollableFrame::ScrollUnit aUnit,
                nsIScrollableFrame::ScrollMode aMode, nsIntPoint* aOverflow);
  void ScrollToRestoredPosition();
  nsSize GetLineScrollAmount() const;
  nsSize GetPageScrollAmount() const;

  nsPresState* SaveState(nsIStatefulFrame::SpecialStateID aStateID);
  void RestoreState(nsPresState* aState);

  nsIFrame* GetScrolledFrame() const { return mScrolledFrame; }
  nsIBox* GetScrollbarBox(PRBool aVertical) const {
    return aVertical ? mVScrollbarBox : mHScrollbarBox;
  }

  void AddScrollPositionListener(nsIScrollPositionListener* aListener) {
    mListeners.AppendElement(aListener);
  }
  void RemoveScrollPositionListener(nsIScrollPositionListener* aListener) {
    mListeners.RemoveElement(aListener);
  }

  static void SetScrollbarVisibility(nsIBox* aScrollbar, PRBool aVisible);

  /**
   * GetScrolledRect is designed to encapsulate deciding which
   * directions of overflow should be reachable by scrolling and which
   * should not.  Callers should NOT depend on it having any particular
   * behavior (although nsXULScrollFrame currently does).
   * 
   * This should only be called when the scrolled frame has been
   * reflowed with the scroll port size given in mScrollPort.
   *
   * Currently it allows scrolling down and to the right for
   * nsHTMLScrollFrames with LTR directionality and for all
   * nsXULScrollFrames, and allows scrolling down and to the left for
   * nsHTMLScrollFrames with RTL directionality.
   */
  nsRect GetScrolledRect() const;

  /**
   * GetScrolledRectInternal is designed to encapsulate deciding which
   * directions of overflow should be reachable by scrolling and which
   * should not.  Callers should NOT depend on it having any particular
   * behavior (although nsXULScrollFrame currently does).
   * 
   * Currently it allows scrolling down and to the right for
   * nsHTMLScrollFrames with LTR directionality and for all
   * nsXULScrollFrames, and allows scrolling down and to the left for
   * nsHTMLScrollFrames with RTL directionality.
   */
  nsRect GetScrolledRectInternal(const nsRect& aScrolledOverflowArea,
                                 const nsSize& aScrollPortSize) const;

  PRUint32 GetScrollbarVisibility() const {
    return (mHasVerticalScrollbar ? nsIScrollableFrame::VERTICAL : 0) |
           (mHasHorizontalScrollbar ? nsIScrollableFrame::HORIZONTAL : 0);
  }
  nsMargin GetActualScrollbarSizes() const;
  nsMargin GetDesiredScrollbarSizes(nsBoxLayoutState* aState);
  PRBool IsLTR() const;
  PRBool IsScrollbarOnRight() const;
  PRBool IsScrollingActive() const;
  // adjust the scrollbar rectangle aRect to account for any visible resizer.
  // aHasResizer specifies if there is a content resizer, however this method
  // will also check if a widget resizer is present as well.
  void AdjustScrollbarRectForResizer(nsIFrame* aFrame, nsPresContext* aPresContext,
                                     nsRect& aRect, PRBool aHasResizer, PRBool aVertical);
  // returns true if a resizer should be visible
  PRBool HasResizer() {
      return mScrollCornerContent && mScrollCornerContent->Tag() == nsGkAtoms::resizer;
  }
  void LayoutScrollbars(nsBoxLayoutState& aState,
                        const nsRect& aContentArea,
                        const nsRect& aOldScrollArea);

  PRBool IsAlwaysActive() const;
  void MarkActive();
  nsExpirationState* GetExpirationState() { return &mActivityExpirationState; }

  // owning references to the nsIAnonymousContentCreator-built content
  nsCOMPtr<nsIContent> mHScrollbarContent;
  nsCOMPtr<nsIContent> mVScrollbarContent;
  nsCOMPtr<nsIContent> mScrollCornerContent;

  nsRevocableEventPtr<ScrollEvent> mScrollEvent;
  nsRevocableEventPtr<AsyncScrollPortEvent> mAsyncScrollPortEvent;
  nsRevocableEventPtr<ScrolledAreaEvent> mScrolledAreaEvent;
  nsIBox* mHScrollbarBox;
  nsIBox* mVScrollbarBox;
  nsIFrame* mScrolledFrame;
  nsIBox* mScrollCornerBox;
  nsContainerFrame* mOuter;
  AsyncScroll* mAsyncScroll;
  nsTArray<nsIScrollPositionListener*> mListeners;
  nsRect mScrollPort;
  // Where we're currently scrolling to, if we're scrolling asynchronously.
  // If we're not in the middle of an asynchronous scroll then this is
  // just the current scroll position. ScrollBy will choose its
  // destination based on this value.
  nsPoint mDestination;
  nsPoint mScrollPosAtLastPaint;

  nsPoint mRestorePos;
  nsPoint mLastPos;

  nsExpirationState mActivityExpirationState;

  PRPackedBool mNeverHasVerticalScrollbar:1;
  PRPackedBool mNeverHasHorizontalScrollbar:1;
  PRPackedBool mHasVerticalScrollbar:1;
  PRPackedBool mHasHorizontalScrollbar:1;
  PRPackedBool mFrameIsUpdatingScrollbar:1;
  PRPackedBool mDidHistoryRestore:1;
  // Is this the scrollframe for the document's viewport?
  PRPackedBool mIsRoot:1;
  // Is mOuter an nsXULScrollFrame?
  PRPackedBool mIsXUL:1;
  // If true, don't try to layout the scrollbars in Reflow().  This can be
  // useful if multiple passes are involved, because we don't want to place the
  // scrollbars at the wrong size.
  PRPackedBool mSupppressScrollbarUpdate:1;
  // If true, we skipped a scrollbar layout due to mSupppressScrollbarUpdate
  // being set at some point.  That means we should lay out scrollbars even if
  // it might not strictly be needed next time mSupppressScrollbarUpdate is
  // false.
  PRPackedBool mSkippedScrollbarLayout:1;

  PRPackedBool mHadNonInitialReflow:1;
  // State used only by PostScrollEvents so we know
  // which overflow states have changed.
  PRPackedBool mHorizontalOverflow:1;
  PRPackedBool mVerticalOverflow:1;
  PRPackedBool mPostedReflowCallback:1;
  PRPackedBool mMayHaveDirtyFixedChildren:1;
  // If true, need to actually update our scrollbar attributes in the
  // reflow callback.
  PRPackedBool mUpdateScrollbarAttributes:1;
  // If true, we should be prepared to scroll using this scrollframe
  // by placing descendant content into its own layer(s)
  PRPackedBool mScrollingActive:1;
};

/**
 * The scroll frame creates and manages the scrolling view
 *
 * It only supports having a single child frame that typically is an area
 * frame, but doesn't have to be. The child frame must have a view, though
 *
 * Scroll frames don't support incremental changes, i.e. you can't replace
 * or remove the scrolled frame
 */
class nsHTMLScrollFrame : public nsHTMLContainerFrame,
                          public nsIScrollableFrame,
                          public nsIAnonymousContentCreator,
                          public nsIStatefulFrame {
public:
  friend nsIFrame* NS_NewHTMLScrollFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, PRBool aIsRoot);

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // Called to set the child frames. We typically have three: the scroll area,
  // the vertical scrollbar, and the horizontal scrollbar.
  NS_IMETHOD SetInitialChildList(nsIAtom*        aListName,
                                 nsFrameList&    aChildList);

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists) {
    return mInner.BuildDisplayList(aBuilder, aDirtyRect, aLists);
  }

  PRBool TryLayout(ScrollReflowState* aState,
                   nsHTMLReflowMetrics* aKidMetrics,
                   PRBool aAssumeVScroll, PRBool aAssumeHScroll,
                   PRBool aForce, nsresult* aResult);
  PRBool ScrolledContentDependsOnHeight(ScrollReflowState* aState);
  nsresult ReflowScrolledFrame(ScrollReflowState* aState,
                               PRBool aAssumeHScroll,
                               PRBool aAssumeVScroll,
                               nsHTMLReflowMetrics* aMetrics,
                               PRBool aFirstPass);
  nsresult ReflowContents(ScrollReflowState* aState,
                          const nsHTMLReflowMetrics& aDesiredSize);
  void PlaceScrollArea(const ScrollReflowState& aState,
                       const nsPoint& aScrollPosition);
  nscoord GetIntrinsicVScrollbarWidth(nsIRenderingContext *aRenderingContext);

  virtual PRBool GetBorderRadii(nscoord aRadii[8]) const {
    return mInner.GetBorderRadii(aRadii);
  }

  virtual nscoord GetMinWidth(nsIRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsIRenderingContext *aRenderingContext);
  NS_IMETHOD GetPadding(nsMargin& aPadding);
  virtual PRBool IsCollapsed(nsBoxLayoutState& aBoxLayoutState);
  
  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  // Because there can be only one child frame, these two function return
  // NS_ERROR_FAILURE
  NS_IMETHOD AppendFrames(nsIAtom*        aListName,
                          nsFrameList&    aFrameList);
  NS_IMETHOD InsertFrames(nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsFrameList&    aFrameList);

  virtual void DestroyFrom(nsIFrame* aDestructRoot);


  NS_IMETHOD RemoveFrame(nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);

  virtual nsIScrollableFrame* GetScrollTargetFrame() {
    return this;
  }

  virtual nsIFrame* GetContentInsertionFrame() {
    return mInner.GetScrolledFrame()->GetContentInsertionFrame();
  }

  virtual void InvalidateInternal(const nsRect& aDamageRect,
                                  nscoord aX, nscoord aY, nsIFrame* aForChild,
                                  PRUint32 aFlags);

  virtual PRBool DoesClipChildren() { return PR_TRUE; }
  virtual nsSplittableType GetSplittableType() const;

  virtual nsPoint GetPositionOfChildIgnoringScrolling(nsIFrame* aChild)
  { nsPoint pt = aChild->GetPosition();
    if (aChild == mInner.GetScrolledFrame()) pt += GetScrollPosition();
    return pt;
  }

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<nsIContent*>& aElements);
  virtual void AppendAnonymousContentTo(nsBaseContentList& aElements);

  // nsIScrollableFrame
  virtual nsIFrame* GetScrolledFrame() const {
    return mInner.GetScrolledFrame();
  }
  virtual nsGfxScrollFrameInner::ScrollbarStyles GetScrollbarStyles() const {
    return mInner.GetScrollbarStylesFromFrame();
  }
  virtual PRUint32 GetScrollbarVisibility() const {
    return mInner.GetScrollbarVisibility();
  }
  virtual nsMargin GetActualScrollbarSizes() const {
    return mInner.GetActualScrollbarSizes();
  }
  virtual nsMargin GetDesiredScrollbarSizes(nsBoxLayoutState* aState) {
    return mInner.GetDesiredScrollbarSizes(aState);
  }
  virtual nsMargin GetDesiredScrollbarSizes(nsPresContext* aPresContext,
          nsIRenderingContext* aRC) {
    nsBoxLayoutState bls(aPresContext, aRC, 0);
    return GetDesiredScrollbarSizes(&bls);
  }
  virtual nsRect GetScrollPortRect() const {
    return mInner.GetScrollPortRect();
  }
  virtual nsPoint GetScrollPosition() const {
    return mInner.GetScrollPosition();
  }
  virtual nsRect GetScrollRange() const {
    return mInner.GetScrollRange();
  }
  virtual nsSize GetLineScrollAmount() const {
    return mInner.GetLineScrollAmount();
  }
  virtual nsSize GetPageScrollAmount() const {
    return mInner.GetPageScrollAmount();
  }
  virtual void ScrollTo(nsPoint aScrollPosition, ScrollMode aMode) {
    mInner.ScrollTo(aScrollPosition, aMode);
  }
  virtual void ScrollBy(nsIntPoint aDelta, ScrollUnit aUnit, ScrollMode aMode,
                        nsIntPoint* aOverflow) {
    mInner.ScrollBy(aDelta, aUnit, aMode, aOverflow);
  }
  virtual void ScrollToRestoredPosition() {
    mInner.ScrollToRestoredPosition();
  }
  virtual void AddScrollPositionListener(nsIScrollPositionListener* aListener) {
    mInner.AddScrollPositionListener(aListener);
  }
  virtual void RemoveScrollPositionListener(nsIScrollPositionListener* aListener) {
    mInner.RemoveScrollPositionListener(aListener);
  }
  virtual nsIBox* GetScrollbarBox(PRBool aVertical) {
    return mInner.GetScrollbarBox(aVertical);
  }
  virtual void CurPosAttributeChanged(nsIContent* aChild) {
    mInner.CurPosAttributeChanged(aChild);
  }
  NS_IMETHOD PostScrolledAreaEventForCurrentArea() {
    mInner.PostScrolledAreaEvent();
    return NS_OK;
  }
  virtual PRBool IsScrollingActive() {
    return mInner.IsScrollingActive();
  }

  // nsIStatefulFrame
  NS_IMETHOD SaveState(SpecialStateID aStateID, nsPresState** aState) {
    NS_ENSURE_ARG_POINTER(aState);
    *aState = mInner.SaveState(aStateID);
    return NS_OK;
  }
  NS_IMETHOD RestoreState(nsPresState* aState) {
    NS_ENSURE_ARG_POINTER(aState);
    mInner.RestoreState(aState);
    return NS_OK;
  }

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::scrollFrame
   */
  virtual nsIAtom* GetType() const;
  
#ifdef NS_DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  PRBool DidHistoryRestore() { return mInner.mDidHistoryRestore; }

#ifdef ACCESSIBILITY
  virtual already_AddRefed<nsAccessible> CreateAccessible();
#endif

protected:
  nsHTMLScrollFrame(nsIPresShell* aShell, nsStyleContext* aContext, PRBool aIsRoot);
  virtual PRIntn GetSkipSides() const;
  
  void SetSuppressScrollbarUpdate(PRBool aSuppress) {
    mInner.mSupppressScrollbarUpdate = aSuppress;
  }
  PRBool GuessHScrollbarNeeded(const ScrollReflowState& aState);
  PRBool GuessVScrollbarNeeded(const ScrollReflowState& aState);

  PRBool IsScrollbarUpdateSuppressed() const {
    return mInner.mSupppressScrollbarUpdate;
  }

  // Return whether we're in an "initial" reflow.  Some reflows with
  // NS_FRAME_FIRST_REFLOW set are NOT "initial" as far as we're concerned.
  PRBool InInitialReflow() const;
  
  /**
   * Override this to return false if computed height/min-height/max-height
   * should NOT be propagated to child content.
   * nsListControlFrame uses this.
   */
  virtual PRBool ShouldPropagateComputedHeightToScrolledContent() const { return PR_TRUE; }

private:
  friend class nsGfxScrollFrameInner;
  nsGfxScrollFrameInner mInner;
};

/**
 * The scroll frame creates and manages the scrolling view
 *
 * It only supports having a single child frame that typically is an area
 * frame, but doesn't have to be. The child frame must have a view, though
 *
 * Scroll frames don't support incremental changes, i.e. you can't replace
 * or remove the scrolled frame
 */
class nsXULScrollFrame : public nsBoxFrame,
                         public nsIScrollableFrame,
                         public nsIAnonymousContentCreator,
                         public nsIStatefulFrame {
public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewXULScrollFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, PRBool aIsRoot);

  // Called to set the child frames. We typically have three: the scroll area,
  // the vertical scrollbar, and the horizontal scrollbar.
  NS_IMETHOD SetInitialChildList(nsIAtom*        aListName,
                                 nsFrameList&    aChildList);

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists) {
    return mInner.BuildDisplayList(aBuilder, aDirtyRect, aLists);
  }

  // XXXldb Is this actually used?
#if 0
  virtual nscoord GetMinWidth(nsIRenderingContext *aRenderingContext);
#endif

  // Because there can be only one child frame, these two function return
  // NS_ERROR_FAILURE
  NS_IMETHOD AppendFrames(nsIAtom*        aListName,
                          nsFrameList&    aFrameList);
  NS_IMETHOD InsertFrames(nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsFrameList&    aFrameList);

  virtual void DestroyFrom(nsIFrame* aDestructRoot);

  NS_IMETHOD RemoveFrame(nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);

  virtual nsIScrollableFrame* GetScrollTargetFrame() {
    return this;
  }

  virtual nsIFrame* GetContentInsertionFrame() {
    return mInner.GetScrolledFrame()->GetContentInsertionFrame();
  }

  virtual void InvalidateInternal(const nsRect& aDamageRect,
                                  nscoord aX, nscoord aY, nsIFrame* aForChild,
                                  PRUint32 aFlags);

  virtual PRBool DoesClipChildren() { return PR_TRUE; }
  virtual nsSplittableType GetSplittableType() const;

  virtual nsPoint GetPositionOfChildIgnoringScrolling(nsIFrame* aChild)
  { nsPoint pt = aChild->GetPosition();
    if (aChild == mInner.GetScrolledFrame()) pt += GetScrollPosition();
    return pt;
  }

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<nsIContent*>& aElements);
  virtual void AppendAnonymousContentTo(nsBaseContentList& aElements);

  virtual nsSize GetMinSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMaxSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nscoord GetBoxAscent(nsBoxLayoutState& aBoxLayoutState);

  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState);
  NS_IMETHOD GetPadding(nsMargin& aPadding);

  virtual PRBool GetBorderRadii(nscoord aRadii[8]) const {
    return mInner.GetBorderRadii(aRadii);
  }

  nsresult Layout(nsBoxLayoutState& aState);
  void LayoutScrollArea(nsBoxLayoutState& aState, const nsPoint& aScrollPosition);

  static PRBool AddRemoveScrollbar(PRBool& aHasScrollbar, 
                                   nscoord& aXY, 
                                   nscoord& aSize, 
                                   nscoord aSbSize, 
                                   PRBool aOnRightOrBottom, 
                                   PRBool aAdd);
  
  PRBool AddRemoveScrollbar(nsBoxLayoutState& aState, 
                            PRBool aOnTop, 
                            PRBool aHorizontal, 
                            PRBool aAdd);
  
  PRBool AddHorizontalScrollbar (nsBoxLayoutState& aState, PRBool aOnBottom);
  PRBool AddVerticalScrollbar   (nsBoxLayoutState& aState, PRBool aOnRight);
  void RemoveHorizontalScrollbar(nsBoxLayoutState& aState, PRBool aOnBottom);
  void RemoveVerticalScrollbar  (nsBoxLayoutState& aState, PRBool aOnRight);

  static void AdjustReflowStateForPrintPreview(nsBoxLayoutState& aState, PRBool& aSetBack);
  static void AdjustReflowStateBack(nsBoxLayoutState& aState, PRBool aSetBack);

  // nsIScrollableFrame
  virtual nsIFrame* GetScrolledFrame() const {
    return mInner.GetScrolledFrame();
  }
  virtual nsGfxScrollFrameInner::ScrollbarStyles GetScrollbarStyles() const {
    return mInner.GetScrollbarStylesFromFrame();
  }
  virtual PRUint32 GetScrollbarVisibility() const {
    return mInner.GetScrollbarVisibility();
  }
  virtual nsMargin GetActualScrollbarSizes() const {
    return mInner.GetActualScrollbarSizes();
  }
  virtual nsMargin GetDesiredScrollbarSizes(nsBoxLayoutState* aState) {
    return mInner.GetDesiredScrollbarSizes(aState);
  }
  virtual nsMargin GetDesiredScrollbarSizes(nsPresContext* aPresContext,
          nsIRenderingContext* aRC) {
    nsBoxLayoutState bls(aPresContext, aRC, 0);
    return GetDesiredScrollbarSizes(&bls);
  }
  virtual nsRect GetScrollPortRect() const {
    return mInner.GetScrollPortRect();
  }
  virtual nsPoint GetScrollPosition() const {
    return mInner.GetScrollPosition();
  }
  virtual nsRect GetScrollRange() const {
    return mInner.GetScrollRange();
  }
  virtual nsSize GetLineScrollAmount() const {
    return mInner.GetLineScrollAmount();
  }
  virtual nsSize GetPageScrollAmount() const {
    return mInner.GetPageScrollAmount();
  }
  virtual void ScrollTo(nsPoint aScrollPosition, ScrollMode aMode) {
    mInner.ScrollTo(aScrollPosition, aMode);
  }
  virtual void ScrollBy(nsIntPoint aDelta, ScrollUnit aUnit, ScrollMode aMode,
                        nsIntPoint* aOverflow) {
    mInner.ScrollBy(aDelta, aUnit, aMode, aOverflow);
  }
  virtual void ScrollToRestoredPosition() {
    mInner.ScrollToRestoredPosition();
  }
  virtual void AddScrollPositionListener(nsIScrollPositionListener* aListener) {
    mInner.AddScrollPositionListener(aListener);
  }
  virtual void RemoveScrollPositionListener(nsIScrollPositionListener* aListener) {
    mInner.RemoveScrollPositionListener(aListener);
  }
  virtual nsIBox* GetScrollbarBox(PRBool aVertical) {
    return mInner.GetScrollbarBox(aVertical);
  }
  virtual void CurPosAttributeChanged(nsIContent* aChild) {
    mInner.CurPosAttributeChanged(aChild);
  }
  NS_IMETHOD PostScrolledAreaEventForCurrentArea() {
    mInner.PostScrolledAreaEvent();
    return NS_OK;
  }
  virtual PRBool IsScrollingActive() {
    return mInner.IsScrollingActive();
  }

  // nsIStatefulFrame
  NS_IMETHOD SaveState(SpecialStateID aStateID, nsPresState** aState) {
    NS_ENSURE_ARG_POINTER(aState);
    *aState = mInner.SaveState(aStateID);
    return NS_OK;
  }
  NS_IMETHOD RestoreState(nsPresState* aState) {
    NS_ENSURE_ARG_POINTER(aState);
    mInner.RestoreState(aState);
    return NS_OK;
  }

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::scrollFrame
   */
  virtual nsIAtom* GetType() const;
  
  virtual PRBool IsFrameOfType(PRUint32 aFlags) const
  {
    // Override bogus IsFrameOfType in nsBoxFrame.
    if (aFlags & (nsIFrame::eReplacedContainsBlock | nsIFrame::eReplaced))
      return PR_FALSE;
    return nsBoxFrame::IsFrameOfType(aFlags);
  }

#ifdef NS_DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

protected:
  nsXULScrollFrame(nsIPresShell* aShell, nsStyleContext* aContext, PRBool aIsRoot);
  virtual PRIntn GetSkipSides() const;

private:
  friend class nsGfxScrollFrameInner;
  nsGfxScrollFrameInner mInner;
};

#endif /* nsGfxScrollFrame_h___ */
