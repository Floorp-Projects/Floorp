/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSliderFrame_h__
#define nsSliderFrame_h__

#include "mozilla/Attributes.h"
#include "nsRepeatService.h"
#include "nsBoxFrame.h"
#include "nsIAtom.h"
#include "nsCOMPtr.h"
#include "nsITimer.h"
#include "nsIDOMEventListener.h"

class nsITimer;
class nsSliderFrame;

nsIFrame* NS_NewSliderFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

class nsSliderMediator final : public nsIDOMEventListener
{
public:

  NS_DECL_ISUPPORTS

  nsSliderFrame* mSlider;

  explicit nsSliderMediator(nsSliderFrame* aSlider) {  mSlider = aSlider; }

  virtual void SetSlider(nsSliderFrame* aSlider) { mSlider = aSlider; }

  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) override;

protected:
  virtual ~nsSliderMediator() {}
};

class nsSliderFrame : public nsBoxFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS
  NS_DECL_QUERYFRAME_TARGET(nsSliderFrame)
  NS_DECL_QUERYFRAME

  friend class nsSliderMediator;

  explicit nsSliderFrame(nsStyleContext* aContext);
  virtual ~nsSliderFrame();

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(NS_LITERAL_STRING("SliderFrame"), aResult);
  }
#endif

  virtual nsSize GetXULPrefSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetXULMinSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetXULMaxSize(nsBoxLayoutState& aBoxLayoutState) override;
  NS_IMETHOD DoXULLayout(nsBoxLayoutState& aBoxLayoutState) override;

  // nsIFrame overrides
  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

  virtual void BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                           const nsRect&           aDirtyRect,
                                           const nsDisplayListSet& aLists) override;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;
 
  virtual nsresult AttributeChanged(int32_t aNameSpaceID,
                                    nsIAtom* aAttribute,
                                    int32_t aModType) override;

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         asPrevInFlow) override;


  virtual nsresult HandleEvent(nsPresContext* aPresContext,
                               mozilla::WidgetGUIEvent* aEvent,
                               nsEventStatus* aEventStatus) override;

  virtual nsIAtom* GetType() const override;

  // nsContainerFrame overrides
  virtual void SetInitialChildList(ChildListID     aListID,
                                   nsFrameList&    aChildList) override;
  virtual void AppendFrames(ChildListID     aListID,
                            nsFrameList&    aFrameList) override;
  virtual void InsertFrames(ChildListID     aListID,
                            nsIFrame*       aPrevFrame,
                            nsFrameList&    aFrameList) override;
  virtual void RemoveFrame(ChildListID     aListID,
                           nsIFrame*       aOldFrame) override;

  nsresult StartDrag(nsIDOMEvent* aEvent);
  nsresult StopDrag();

  bool StartAPZDrag(WidgetGUIEvent* aEvent);

  static int32_t GetCurrentPosition(nsIContent* content);
  static int32_t GetMinPosition(nsIContent* content);
  static int32_t GetMaxPosition(nsIContent* content);
  static int32_t GetIncrement(nsIContent* content);
  static int32_t GetPageIncrement(nsIContent* content);
  static int32_t GetIntegerAttribute(nsIContent* content, nsIAtom* atom, int32_t defaultValue);
  void EnsureOrient();

  NS_IMETHOD HandlePress(nsPresContext* aPresContext,
                         mozilla::WidgetGUIEvent* aEvent,
                         nsEventStatus* aEventStatus) override;

  NS_IMETHOD HandleMultiplePress(nsPresContext* aPresContext,
                                 mozilla::WidgetGUIEvent* aEvent,
                                 nsEventStatus* aEventStatus,
                                 bool aControlHeld) override
  {
    return NS_OK;
  }

  NS_IMETHOD HandleDrag(nsPresContext* aPresContext,
                        mozilla::WidgetGUIEvent* aEvent,
                        nsEventStatus* aEventStatus) override
  {
    return NS_OK;
  }

  NS_IMETHOD HandleRelease(nsPresContext* aPresContext,
                           mozilla::WidgetGUIEvent* aEvent,
                           nsEventStatus* aEventStatus) override;

  // Return the ratio the scrollbar thumb should move in proportion to the
  // scrolled frame.
  float GetThumbRatio() const;

private:

  bool GetScrollToClick();
  nsIFrame* GetScrollbar();
  bool ShouldScrollForEvent(mozilla::WidgetGUIEvent* aEvent);
  bool ShouldScrollToClickForEvent(mozilla::WidgetGUIEvent* aEvent);
  bool IsEventOverThumb(mozilla::WidgetGUIEvent* aEvent);

  void PageUpDown(nscoord change);
  void SetCurrentThumbPosition(nsIContent* aScrollbar, nscoord aNewPos, bool aIsSmooth,
                               bool aMaySnap);
  void SetCurrentPosition(nsIContent* aScrollbar, int32_t aNewPos, bool aIsSmooth);
  void SetCurrentPositionInternal(nsIContent* aScrollbar, int32_t pos,
                                  bool aIsSmooth);
  void CurrentPositionChanged();

  void DragThumb(bool aGrabMouseEvents);
  void AddListener();
  void RemoveListener();
  bool isDraggingThumb();

  void StartRepeat() {
    nsRepeatService::GetInstance()->Start(Notify, this);
  }
  void StopRepeat() {
    nsRepeatService::GetInstance()->Stop(Notify, this);
  }
  void Notify();
  static void Notify(void* aData) {
    (static_cast<nsSliderFrame*>(aData))->Notify();
  }
  void PageScroll(nscoord aChange);
 
  nsPoint mDestinationPoint;
  RefPtr<nsSliderMediator> mMediator;

  float mRatio;

  nscoord mDragStart;
  nscoord mThumbStart;

  int32_t mCurPos;

  nscoord mChange;

  bool mDragFinished;

  // true if an attribute change has been caused by the user manipulating the
  // slider. This allows notifications to tell how a slider's current position
  // was changed.
  bool mUserChanged;

  // true if we've handed off the scrolling to APZ. This means that we should
  // ignore scrolling events as the position will be updated by APZ. If we were
  // to process these events then the scroll position update would conflict
  // causing the scroll position to jump.
  bool mScrollingWithAPZ;

  // true if displayport suppression is active, for more performant
  // scrollbar-dragging behaviour.
  bool mSuppressionActive;

  static bool gMiddlePref;
  static int32_t gSnapMultiplier;
}; // class nsSliderFrame

#endif
