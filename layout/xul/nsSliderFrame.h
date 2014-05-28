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

class nsString;
class nsITimer;
class nsSliderFrame;

nsIFrame* NS_NewSliderFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

class nsSliderMediator : public nsIDOMEventListener
{
public:

  NS_DECL_ISUPPORTS

  nsSliderFrame* mSlider;

  nsSliderMediator(nsSliderFrame* aSlider) {  mSlider = aSlider; }
  virtual ~nsSliderMediator() {}

  virtual void SetSlider(nsSliderFrame* aSlider) { mSlider = aSlider; }

  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) MOZ_OVERRIDE;
};

class nsSliderFrame : public nsBoxFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend class nsSliderMediator;

  nsSliderFrame(nsIPresShell* aShell, nsStyleContext* aContext);
  virtual ~nsSliderFrame();

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE {
    return MakeFrameName(NS_LITERAL_STRING("SliderFrame"), aResult);
  }
#endif

  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;
  virtual nsSize GetMinSize(nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;
  virtual nsSize GetMaxSize(nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;
  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;

  // nsIFrame overrides
  virtual void  AppendFrames(ChildListID     aListID,
                                 nsFrameList&    aFrameList) MOZ_OVERRIDE;

  virtual void  InsertFrames(ChildListID     aListID,
                                 nsIFrame*       aPrevFrame,
                                 nsFrameList&    aFrameList) MOZ_OVERRIDE;

  virtual void  RemoveFrame(ChildListID     aListID,
                                nsIFrame*       aOldFrame) MOZ_OVERRIDE;

  virtual void DestroyFrom(nsIFrame* aDestructRoot) MOZ_OVERRIDE;

  virtual void BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                           const nsRect&           aDirtyRect,
                                           const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;
 
  virtual nsresult AttributeChanged(int32_t aNameSpaceID,
                                    nsIAtom* aAttribute,
                                    int32_t aModType) MOZ_OVERRIDE;

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         asPrevInFlow) MOZ_OVERRIDE;


  virtual nsresult HandleEvent(nsPresContext* aPresContext,
                               mozilla::WidgetGUIEvent* aEvent,
                               nsEventStatus* aEventStatus) MOZ_OVERRIDE;

  virtual void SetInitialChildList(ChildListID     aListID,
                                       nsFrameList&    aChildList) MOZ_OVERRIDE;

  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

  nsresult StartDrag(nsIDOMEvent* aEvent);

  static int32_t GetCurrentPosition(nsIContent* content);
  static int32_t GetMinPosition(nsIContent* content);
  static int32_t GetMaxPosition(nsIContent* content);
  static int32_t GetIncrement(nsIContent* content);
  static int32_t GetPageIncrement(nsIContent* content);
  static int32_t GetIntegerAttribute(nsIContent* content, nsIAtom* atom, int32_t defaultValue);
  void EnsureOrient();

  NS_IMETHOD HandlePress(nsPresContext* aPresContext,
                         mozilla::WidgetGUIEvent* aEvent,
                         nsEventStatus* aEventStatus) MOZ_OVERRIDE;

  NS_IMETHOD HandleMultiplePress(nsPresContext* aPresContext,
                                 mozilla::WidgetGUIEvent* aEvent,
                                 nsEventStatus* aEventStatus,
                                 bool aControlHeld) MOZ_OVERRIDE
  {
    return NS_OK;
  }

  NS_IMETHOD HandleDrag(nsPresContext* aPresContext,
                        mozilla::WidgetGUIEvent* aEvent,
                        nsEventStatus* aEventStatus) MOZ_OVERRIDE
  {
    return NS_OK;
  }

  NS_IMETHOD HandleRelease(nsPresContext* aPresContext,
                           mozilla::WidgetGUIEvent* aEvent,
                           nsEventStatus* aEventStatus) MOZ_OVERRIDE;

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
 
  nsPoint mDestinationPoint;
  nsRefPtr<nsSliderMediator> mMediator;

  float mRatio;

  nscoord mDragStart;
  nscoord mThumbStart;

  int32_t mCurPos;

  nscoord mChange;

  // true if an attribute change has been caused by the user manipulating the
  // slider. This allows notifications to tell how a slider's current position
  // was changed.
  bool mUserChanged;

  static bool gMiddlePref;
  static int32_t gSnapMultiplier;
}; // class nsSliderFrame

#endif
