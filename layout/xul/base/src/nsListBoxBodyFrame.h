/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsListBoxBodyFrame_h
#define nsListBoxBodyFrame_h

#include "nsCOMPtr.h"
#include "nsBoxFrame.h"
#include "nsIListBoxObject.h"
#include "nsIScrollbarMediator.h"
#include "nsIReflowCallback.h"
#include "nsBoxLayoutState.h"
#include "nsThreadUtils.h"
#include "nsPIBoxObject.h"

class nsPresContext;
class nsListScrollSmoother;
nsIFrame* NS_NewListBoxBodyFrame(nsIPresShell* aPresShell,
                                 nsStyleContext* aContext);

class nsListBoxBodyFrame : public nsBoxFrame,
                           public nsIScrollbarMediator,
                           public nsIReflowCallback
{
  nsListBoxBodyFrame(nsIPresShell* aPresShell, nsStyleContext* aContext,
                     nsBoxLayout* aLayoutManager);
  virtual ~nsListBoxBodyFrame();

public:
  NS_DECL_QUERYFRAME_TARGET(nsListBoxBodyFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // non-virtual nsIListBoxObject
  nsresult GetRowCount(PRInt32 *aResult);
  nsresult GetNumberOfVisibleRows(PRInt32 *aResult);
  nsresult GetIndexOfFirstVisibleRow(PRInt32 *aResult);
  nsresult EnsureIndexIsVisible(PRInt32 aRowIndex);
  nsresult ScrollToIndex(PRInt32 aRowIndex);
  nsresult ScrollByLines(PRInt32 aNumLines);
  nsresult GetItemAtIndex(PRInt32 aIndex, nsIDOMElement **aResult);
  nsresult GetIndexOfItem(nsIDOMElement *aItem, PRInt32 *aResult);

  friend nsIFrame* NS_NewListBoxBodyFrame(nsIPresShell* aPresShell,
                                          nsStyleContext* aContext);
  
  // nsIFrame
  NS_IMETHOD Init(nsIContent*     aContent,
                  nsIFrame*       aParent, 
                  nsIFrame*       aPrevInFlow);
  virtual void DestroyFrom(nsIFrame* aDestructRoot);

  NS_IMETHOD AttributeChanged(PRInt32 aNameSpaceID, nsIAtom* aAttribute, PRInt32 aModType);

  // nsIScrollbarMediator
  NS_IMETHOD PositionChanged(nsScrollbarFrame* aScrollbar, PRInt32 aOldIndex, PRInt32& aNewIndex);
  NS_IMETHOD ScrollbarButtonPressed(nsScrollbarFrame* aScrollbar, PRInt32 aOldIndex, PRInt32 aNewIndex);
  NS_IMETHOD VisibilityChanged(bool aVisible);

  // nsIReflowCallback
  virtual bool ReflowFinished();
  virtual void ReflowCallbackCanceled();

  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState);
  virtual void MarkIntrinsicWidthsDirty();

  virtual nsSize GetMinSizeForScrollArea(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState);

  // size calculation 
  PRInt32 GetRowCount();
  PRInt32 GetRowHeightAppUnits() { return mRowHeight; }
  PRInt32 GetFixedRowSize();
  void SetRowHeight(nscoord aRowHeight);
  nscoord GetYPosition();
  nscoord GetAvailableHeight();
  nscoord ComputeIntrinsicWidth(nsBoxLayoutState& aBoxLayoutState);

  // scrolling
  nsresult InternalPositionChangedCallback();
  nsresult InternalPositionChanged(bool aUp, PRInt32 aDelta);
  // Process pending position changed events, then do the position change.
  // This can wipe out the frametree.
  nsresult DoInternalPositionChangedSync(bool aUp, PRInt32 aDelta);
  // Actually do the internal position change.  This can wipe out the frametree
  nsresult DoInternalPositionChanged(bool aUp, PRInt32 aDelta);
  nsListScrollSmoother* GetSmoother();
  void VerticalScroll(PRInt32 aDelta);

  // frames
  nsIFrame* GetFirstFrame();
  nsIFrame* GetLastFrame();

  // lazy row creation and destruction
  void CreateRows();
  void DestroyRows(PRInt32& aRowsToLose);
  void ReverseDestroyRows(PRInt32& aRowsToLose);
  nsIFrame* GetFirstItemBox(PRInt32 aOffset, bool* aCreated);
  nsIFrame* GetNextItemBox(nsIFrame* aBox, PRInt32 aOffset, bool* aCreated);
  bool ContinueReflow(nscoord height);
  NS_IMETHOD ListBoxAppendFrames(nsFrameList& aFrameList);
  NS_IMETHOD ListBoxInsertFrames(nsIFrame* aPrevFrame, nsFrameList& aFrameList);
  void OnContentInserted(nsPresContext* aPresContext, nsIContent* aContent);
  void OnContentRemoved(nsPresContext* aPresContext,  nsIContent* aContainer,
                        nsIFrame* aChildFrame, nsIContent* aOldNextSibling);

  void GetListItemContentAt(PRInt32 aIndex, nsIContent** aContent);
  void GetListItemNextSibling(nsIContent* aListItem, nsIContent** aContent, PRInt32& aSiblingIndex);

  void PostReflowCallback();

  bool SetBoxObject(nsPIBoxObject* aBoxObject)
  {
    NS_ENSURE_TRUE(!mBoxObject, false);
    mBoxObject = aBoxObject;
    return true;
  }

  virtual bool SupportsOrdinalsInChildren();

  virtual bool ComputesOwnOverflowArea() { return true; }

protected:
  class nsPositionChangedEvent;
  friend class nsPositionChangedEvent;

  class nsPositionChangedEvent : public nsRunnable
  {
  public:
    nsPositionChangedEvent(nsListBoxBodyFrame* aFrame,
                           bool aUp, PRInt32 aDelta) :
      mFrame(aFrame), mUp(aUp), mDelta(aDelta)
    {}
  
    NS_IMETHOD Run()
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
    PRInt32 mDelta;
  };

  void ComputeTotalRowCount();
  void RemoveChildFrame(nsBoxLayoutState &aState, nsIFrame *aChild);

  nsTArray< nsRefPtr<nsPositionChangedEvent> > mPendingPositionChangeEvents;
  nsCOMPtr<nsPIBoxObject> mBoxObject;

  // frame markers
  nsWeakFrame mTopFrame;
  nsIFrame* mBottomFrame;
  nsIFrame* mLinkupFrame;

  nsListScrollSmoother* mScrollSmoother;

  PRInt32 mRowsToPrepend;

  // row height
  PRInt32 mRowCount;
  nscoord mRowHeight;
  nscoord mAvailableHeight;
  nscoord mStringWidth;

  // scrolling
  PRInt32 mCurrentIndex; // Row-based
  PRInt32 mOldIndex; 
  PRInt32 mYPosition;
  PRInt32 mTimePerRow;

  // row height
  bool mRowHeightWasSet;
  // scrolling
  bool mScrolling;
  bool mAdjustScroll;

  bool mReflowCallbackPosted;
};

#endif // nsListBoxBodyFrame_h
