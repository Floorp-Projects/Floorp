/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsListBoxBodyFrame_h
#define nsListBoxBodyFrame_h

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsBoxFrame.h"
#include "nsIScrollbarMediator.h"
#include "nsIReflowCallback.h"
#include "nsBoxLayoutState.h"
#include "nsThreadUtils.h"
#include "nsPIBoxObject.h"

class nsPresContext;
class nsListScrollSmoother;
nsIFrame* NS_NewListBoxBodyFrame(nsIPresShell* aPresShell,
                                 nsStyleContext* aContext);

class nsListBoxBodyFrame final : public nsBoxFrame,
                                 public nsIScrollbarMediator,
                                 public nsIReflowCallback
{
  nsListBoxBodyFrame(nsStyleContext* aContext,
                     nsBoxLayout* aLayoutManager);
  virtual ~nsListBoxBodyFrame();

public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsListBoxBodyFrame)

  // non-virtual ListBoxObject
  int32_t GetNumberOfVisibleRows();
  int32_t GetIndexOfFirstVisibleRow();
  nsresult EnsureIndexIsVisible(int32_t aRowIndex);
  nsresult ScrollToIndex(int32_t aRowIndex);
  nsresult ScrollByLines(int32_t aNumLines);
  nsresult GetItemAtIndex(int32_t aIndex, nsIDOMElement **aResult);
  nsresult GetIndexOfItem(nsIDOMElement *aItem, int32_t *aResult);

  friend nsIFrame* NS_NewListBoxBodyFrame(nsIPresShell* aPresShell,
                                          nsStyleContext* aContext);

  // nsIFrame
  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;
  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

  virtual nsresult AttributeChanged(int32_t aNameSpaceID, nsIAtom* aAttribute, int32_t aModType) override;

  // nsIScrollbarMediator
  virtual void ScrollByPage(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                            nsIScrollbarMediator::ScrollSnapMode snapMode
                              = nsIScrollbarMediator::DISABLE_SNAP) override;
  virtual void ScrollByWhole(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                             nsIScrollbarMediator::ScrollSnapMode snapMode
                               = nsIScrollbarMediator::DISABLE_SNAP) override;
  virtual void ScrollByLine(nsScrollbarFrame* aScrollbar, int32_t aDirection,
                            nsIScrollbarMediator::ScrollSnapMode snapMode
                              = nsIScrollbarMediator::DISABLE_SNAP) override;
  virtual void RepeatButtonScroll(nsScrollbarFrame* aScrollbar) override;
  virtual void ThumbMoved(nsScrollbarFrame* aScrollbar,
                          int32_t aOldPos,
                          int32_t aNewPos) override;
  virtual void ScrollbarReleased(nsScrollbarFrame* aScrollbar) override {}
  virtual void VisibilityChanged(bool aVisible) override;
  virtual nsIFrame* GetScrollbarBox(bool aVertical) override;
  virtual void ScrollbarActivityStarted() const override {}
  virtual void ScrollbarActivityStopped() const override {}
  virtual bool IsScrollbarOnRight() const override {
    return (StyleVisibility()->mDirection == NS_STYLE_DIRECTION_LTR);
  }
  virtual bool ShouldSuppressScrollbarRepaints() const override {
    return false;
  }


  // nsIReflowCallback
  virtual bool ReflowFinished() override;
  virtual void ReflowCallbackCanceled() override;

  NS_IMETHOD DoXULLayout(nsBoxLayoutState& aBoxLayoutState) override;
  virtual void MarkIntrinsicISizesDirty() override;

  virtual nsSize GetXULMinSizeForScrollArea(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetXULPrefSize(nsBoxLayoutState& aBoxLayoutState) override;

  // size calculation
  int32_t GetRowCount();
  int32_t GetRowHeightAppUnits() { return mRowHeight; }
  int32_t GetRowHeightPixels() const;
  int32_t GetFixedRowSize();
  void SetRowHeight(nscoord aRowHeight);
  nscoord GetYPosition();
  nscoord GetAvailableHeight();
  nscoord ComputeIntrinsicISize(nsBoxLayoutState& aBoxLayoutState);

  // scrolling
  nsresult InternalPositionChangedCallback();
  nsresult InternalPositionChanged(bool aUp, int32_t aDelta);
  // Process pending position changed events, then do the position change.
  // This can wipe out the frametree.
  nsresult DoInternalPositionChangedSync(bool aUp, int32_t aDelta);
  // Actually do the internal position change.  This can wipe out the frametree
  nsresult DoInternalPositionChanged(bool aUp, int32_t aDelta);
  nsListScrollSmoother* GetSmoother();
  void VerticalScroll(int32_t aDelta);
  // Update the scroll index given a position, in CSS pixels
  void UpdateIndex(int32_t aNewPos);

  // frames
  nsIFrame* GetFirstFrame();
  nsIFrame* GetLastFrame();

  // lazy row creation and destruction
  void CreateRows();
  void DestroyRows(int32_t& aRowsToLose);
  void ReverseDestroyRows(int32_t& aRowsToLose);
  nsIFrame* GetFirstItemBox(int32_t aOffset, bool* aCreated);
  nsIFrame* GetNextItemBox(nsIFrame* aBox, int32_t aOffset, bool* aCreated);
  bool ContinueReflow(nscoord height);
  NS_IMETHOD ListBoxAppendFrames(nsFrameList& aFrameList);
  NS_IMETHOD ListBoxInsertFrames(nsIFrame* aPrevFrame, nsFrameList& aFrameList);
  void OnContentInserted(nsIContent* aContent);
  void OnContentRemoved(nsPresContext* aPresContext,  nsIContent* aContainer,
                        nsIFrame* aChildFrame, nsIContent* aOldNextSibling);

  void GetListItemContentAt(int32_t aIndex, nsIContent** aContent);
  void GetListItemNextSibling(nsIContent* aListItem, nsIContent** aContent, int32_t& aSiblingIndex);

  void PostReflowCallback();

  bool SetBoxObject(nsPIBoxObject* aBoxObject)
  {
    NS_ENSURE_TRUE(!mBoxObject, false);
    mBoxObject = aBoxObject;
    return true;
  }

  virtual bool SupportsOrdinalsInChildren() override;

  virtual bool ComputesOwnOverflowArea() override { return true; }

protected:
  class nsPositionChangedEvent;
  friend class nsPositionChangedEvent;

  class nsPositionChangedEvent : public mozilla::Runnable
  {
  public:
    nsPositionChangedEvent(nsListBoxBodyFrame* aFrame, bool aUp, int32_t aDelta)
      : mozilla::Runnable("nsListBoxBodyFrame::nsPositionChangedEvent")
      , mFrame(aFrame)
      , mUp(aUp)
      , mDelta(aDelta)
    {}

    NS_IMETHOD Run() override
    {
      if (!mFrame) {
        return NS_OK;
      }

      mFrame->mPendingPositionChangeEvents.RemoveElement(this);

      return mFrame->DoInternalPositionChanged(mUp, mDelta);
    }

    void Revoke() {
      mFrame = nullptr;
    }

    nsListBoxBodyFrame* mFrame;
    bool mUp;
    int32_t mDelta;
  };

  void ComputeTotalRowCount();
  int32_t ToRowIndex(nscoord aPos) const;
  void RemoveChildFrame(nsBoxLayoutState &aState, nsIFrame *aChild);

  nsTArray< RefPtr<nsPositionChangedEvent> > mPendingPositionChangeEvents;
  nsCOMPtr<nsPIBoxObject> mBoxObject;

  // frame markers
  WeakFrame mTopFrame;
  nsIFrame* mBottomFrame;
  nsIFrame* mLinkupFrame;

  nsListScrollSmoother* mScrollSmoother;

  int32_t mRowsToPrepend;

  // row height
  int32_t mRowCount;
  nscoord mRowHeight;
  nscoord mAvailableHeight;
  nscoord mStringWidth;

  // scrolling
  int32_t mCurrentIndex; // Row-based
  int32_t mOldIndex;
  int32_t mYPosition;
  int32_t mTimePerRow;

  // row height
  bool mRowHeightWasSet;
  // scrolling
  bool mScrolling;
  bool mAdjustScroll;

  bool mReflowCallbackPosted;
};

#endif // nsListBoxBodyFrame_h
