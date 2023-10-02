/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// nsScrollbarFrame
//

#ifndef nsScrollbarFrame_h__
#define nsScrollbarFrame_h__

#include "mozilla/Attributes.h"
#include "mozilla/ScrollTypes.h"
#include "nsContainerFrame.h"
#include "nsIAnonymousContentCreator.h"

class nsIScrollbarMediator;

namespace mozilla {
class PresShell;
namespace dom {
class Element;
}
}  // namespace mozilla

nsIFrame* NS_NewScrollbarFrame(mozilla::PresShell* aPresShell,
                               mozilla::ComputedStyle* aStyle);

class nsScrollbarFrame final : public nsContainerFrame,
                               public nsIAnonymousContentCreator {
  using Element = mozilla::dom::Element;

 public:
  explicit nsScrollbarFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : nsContainerFrame(aStyle, aPresContext, kClassID),
        mSmoothScroll(false),
        mScrollUnit(mozilla::ScrollUnit::DEVICE_PIXELS),
        mDirection(0),
        mIncrement(0),
        mScrollbarMediator(nullptr),
        mUpTopButton(nullptr),
        mDownTopButton(nullptr),
        mSlider(nullptr),
        mThumb(nullptr),
        mUpBottomButton(nullptr),
        mDownBottomButton(nullptr) {}

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsScrollbarFrame)

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"ScrollbarFrame"_ns, aResult);
  }
#endif

  // nsIFrame overrides
  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;

  NS_IMETHOD HandlePress(nsPresContext* aPresContext,
                         mozilla::WidgetGUIEvent* aEvent,
                         nsEventStatus* aEventStatus) override;

  NS_IMETHOD HandleMultiplePress(nsPresContext* aPresContext,
                                 mozilla::WidgetGUIEvent* aEvent,
                                 nsEventStatus* aEventStatus,
                                 bool aControlHeld) override;

  MOZ_CAN_RUN_SCRIPT
  NS_IMETHOD HandleDrag(nsPresContext* aPresContext,
                        mozilla::WidgetGUIEvent* aEvent,
                        nsEventStatus* aEventStatus) override;

  NS_IMETHOD HandleRelease(nsPresContext* aPresContext,
                           mozilla::WidgetGUIEvent* aEvent,
                           nsEventStatus* aEventStatus) override;

  mozilla::StyleScrollbarWidth ScrollbarWidth() const;
  nscoord ScrollbarTrackSize() const;
  nsSize ScrollbarMinSize() const;
  bool IsHorizontal() const;

  void Destroy(DestroyContext&) override;

  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;

  void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;

  void SetScrollbarMediatorContent(nsIContent* aMediator);
  nsIScrollbarMediator* GetScrollbarMediator();

  /**
   * The following three methods set the value of mIncrement when a
   * scrollbar button is pressed.
   */
  void SetIncrementToLine(int32_t aDirection);
  void SetIncrementToPage(int32_t aDirection);
  void SetIncrementToWhole(int32_t aDirection);

  /**
   * If aImplementsScrollByUnit is Yes then this uses mSmoothScroll,
   * mScrollUnit, and mDirection and calls ScrollByUnit on the
   * nsIScrollbarMediator. The return value is 0. This is better because it is
   * more modern and the scroll frame can perform the scroll via apz for
   * example. The old way below is still supported for xul trees. If
   * aImplementsScrollByUnit is No this adds mIncrement to the current
   * position and updates the curpos attribute obeying mSmoothScroll.
   * @returns The new position after clamping, in CSS Pixels
   * @note This method might destroy the frame, pres shell, and other objects.
   */
  enum class ImplementsScrollByUnit { Yes, No };
  int32_t MoveToNewPosition(ImplementsScrollByUnit aImplementsScrollByUnit);
  int32_t GetIncrement() { return mIncrement; }

  // nsIAnonymousContentCreator
  nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) override;
  void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                uint32_t aFilter) override;

  void UpdateChildrenAttributeValue(nsAtom* aAttribute, bool aNotify);

 protected:
  bool mSmoothScroll;
  mozilla::ScrollUnit mScrollUnit;
  // Direction and multiple to scroll
  int32_t mDirection;

  // Amount to scroll, in CSSPixels
  // Ignored in favour of mScrollUnit/mDirection for regular scroll frames.
  // Trees use this though.
  int32_t mIncrement;

 private:
  nsCOMPtr<nsIContent> mScrollbarMediator;

  nsCOMPtr<Element> mUpTopButton;
  nsCOMPtr<Element> mDownTopButton;
  nsCOMPtr<Element> mSlider;
  nsCOMPtr<Element> mThumb;
  nsCOMPtr<Element> mUpBottomButton;
  nsCOMPtr<Element> mDownBottomButton;
};  // class nsScrollbarFrame

#endif
