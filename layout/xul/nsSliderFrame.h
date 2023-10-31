/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSliderFrame_h__
#define nsSliderFrame_h__

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"
#include "nsRepeatService.h"
#include "nsAtom.h"
#include "nsCOMPtr.h"
#include "nsITimer.h"
#include "nsIDOMEventListener.h"

class nsITimer;
class nsScrollbarFrame;
class nsSliderFrame;

namespace mozilla {
class nsDisplaySliderMarks;
class PresShell;
}  // namespace mozilla

nsIFrame* NS_NewSliderFrame(mozilla::PresShell* aPresShell,
                            mozilla::ComputedStyle* aStyle);

class nsSliderMediator final : public nsIDOMEventListener {
 public:
  NS_DECL_ISUPPORTS

  nsSliderFrame* mSlider;

  explicit nsSliderMediator(nsSliderFrame* aSlider) { mSlider = aSlider; }

  void SetSlider(nsSliderFrame* aSlider) { mSlider = aSlider; }

  NS_DECL_NSIDOMEVENTLISTENER

 protected:
  virtual ~nsSliderMediator() = default;
};

class nsSliderFrame final : public nsContainerFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsSliderFrame)
  NS_DECL_QUERYFRAME

  friend class nsSliderMediator;
  friend class mozilla::nsDisplaySliderMarks;

  explicit nsSliderFrame(ComputedStyle* aStyle, nsPresContext* aPresContext);
  virtual ~nsSliderFrame();

  // Get the point associated with this event. Returns true if a single valid
  // point was found. Otherwise false.
  bool GetEventPoint(mozilla::WidgetGUIEvent* aEvent, nsPoint& aPoint);
  // Gets the event coordinates relative to the widget associated with this
  // frame. Return true if a single valid point was found.
  bool GetEventPoint(mozilla::WidgetGUIEvent* aEvent,
                     mozilla::LayoutDeviceIntPoint& aPoint);

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"SliderFrame"_ns, aResult);
  }
#endif

  void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;

  // nsIFrame overrides
  void Destroy(DestroyContext&) override;

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override;

  void BuildDisplayListForThumb(nsDisplayListBuilder* aBuilder,
                                nsIFrame* aThumb,
                                const nsDisplayListSet& aLists);

  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;

  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;

  nsresult HandleEvent(nsPresContext* aPresContext,
                       mozilla::WidgetGUIEvent* aEvent,
                       nsEventStatus* aEventStatus) override;

  // nsContainerFrame overrides
  void SetInitialChildList(ChildListID aListID,
                           nsFrameList&& aChildList) override;
  void AppendFrames(ChildListID aListID, nsFrameList&& aFrameList) override;
  void InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                    const nsLineList::iterator* aPrevFrameLine,
                    nsFrameList&& aFrameList) override;
  void RemoveFrame(DestroyContext&, ChildListID, nsIFrame*) override;

  nsresult StartDrag(mozilla::dom::Event* aEvent);
  nsresult StopDrag();

  void StartAPZDrag(mozilla::WidgetGUIEvent* aEvent);

  static int32_t GetCurrentPosition(nsIContent* content);
  static int32_t GetMinPosition(nsIContent* content);
  static int32_t GetMaxPosition(nsIContent* content);
  static int32_t GetIncrement(nsIContent* content);
  static int32_t GetPageIncrement(nsIContent* content);
  static int32_t GetIntegerAttribute(nsIContent* content, nsAtom* atom,
                                     int32_t defaultValue);

  NS_IMETHOD HandlePress(nsPresContext* aPresContext,
                         mozilla::WidgetGUIEvent* aEvent,
                         nsEventStatus* aEventStatus) override;

  NS_IMETHOD HandleMultiplePress(nsPresContext* aPresContext,
                                 mozilla::WidgetGUIEvent* aEvent,
                                 nsEventStatus* aEventStatus,
                                 bool aControlHeld) override {
    return NS_OK;
  }

  MOZ_CAN_RUN_SCRIPT
  NS_IMETHOD HandleDrag(nsPresContext* aPresContext,
                        mozilla::WidgetGUIEvent* aEvent,
                        nsEventStatus* aEventStatus) override {
    return NS_OK;
  }

  NS_IMETHOD HandleRelease(nsPresContext* aPresContext,
                           mozilla::WidgetGUIEvent* aEvent,
                           nsEventStatus* aEventStatus) override;

  // Return the ratio the scrollbar thumb should move in proportion to the
  // scrolled frame.
  float GetThumbRatio() const;

  // Notify the slider frame that an async scrollbar drag was started on the
  // APZ side without consulting the main thread. The block id is the APZ
  // input block id of the mousedown that started the drag.
  void AsyncScrollbarDragInitiated(uint64_t aDragBlockId);

  // Notify the slider frame that an async scrollbar drag requested in
  // StartAPZDrag() was rejected by APZ, and the slider frame should
  // fall back to main-thread dragging.
  void AsyncScrollbarDragRejected();

  bool OnlySystemGroupDispatch(mozilla::EventMessage aMessage) const override;

  // Returns the associated scrollframe that contains this slider if any.
  nsIScrollableFrame* GetScrollFrame();

 private:
  bool GetScrollToClick();
  nsScrollbarFrame* Scrollbar();
  bool ShouldScrollForEvent(mozilla::WidgetGUIEvent* aEvent);
  bool ShouldScrollToClickForEvent(mozilla::WidgetGUIEvent* aEvent);
  bool IsEventOverThumb(mozilla::WidgetGUIEvent* aEvent);

  void PageUpDown(nscoord change);
  void SetCurrentThumbPosition(nsIContent* aScrollbar, nscoord aNewPos,
                               bool aIsSmooth, bool aMaySnap);
  void SetCurrentPosition(nsIContent* aScrollbar, int32_t aNewPos,
                          bool aIsSmooth);
  void SetCurrentPositionInternal(nsIContent* aScrollbar, int32_t pos,
                                  bool aIsSmooth);
  void CurrentPositionChanged();

  void DragThumb(bool aGrabMouseEvents);
  void AddListener();
  void RemoveListener();
  bool isDraggingThumb() const;

  void SuppressDisplayport();
  void UnsuppressDisplayport();

  void StartRepeat() {
    nsRepeatService::GetInstance()->Start(Notify, this, mContent->OwnerDoc(),
                                          "nsSliderFrame"_ns);
  }
  void StopRepeat() {
    nsRepeatService::GetInstance()->Stop(Notify, this);
    mCurrentClickHoldDestination = Nothing();
  }
  void Notify();
  static void Notify(void* aData) {
    (static_cast<nsSliderFrame*>(aData))->Notify();
  }
  void PageScroll(bool aClickAndHold);

  void SetupDrag(mozilla::WidgetGUIEvent* aEvent, nsIFrame* aThumbFrame,
                 nscoord aPos, bool aIsHorizontal);

  nsPoint mDestinationPoint;
  // If we are in a scrollbar track click-and-hold, this is populated with
  // the destination of the scroll started at the most recent tick of the
  // repeat timer.
  Maybe<nsPoint> mCurrentClickHoldDestination;
  RefPtr<nsSliderMediator> mMediator;

  float mRatio;

  nscoord mDragStart;
  nscoord mThumbStart;

  int32_t mCurPos;

  nscoord mRepeatDirection;

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

  // If APZ initiated a scrollbar drag without main-thread involvement, it
  // notifies us and this variable stores the input block id of the APZ input
  // block that started the drag. This lets us handle the corresponding
  // mousedown event properly, if it arrives after the scroll position has
  // been shifted due to async scrollbar drag.
  Maybe<uint64_t> mAPZDragInitiated;

  nscoord mThumbMinLength;

  static bool gMiddlePref;
};  // class nsSliderFrame

#endif
