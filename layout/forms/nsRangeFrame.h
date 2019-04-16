/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRangeFrame_h___
#define nsRangeFrame_h___

#include "mozilla/Attributes.h"
#include "mozilla/Decimal.h"
#include "mozilla/EventForwards.h"
#include "nsContainerFrame.h"
#include "nsIAnonymousContentCreator.h"
#include "nsIDOMEventListener.h"
#include "nsCOMPtr.h"

class nsDisplayRangeFocusRing;

namespace mozilla {
class PresShell;
namespace dom {
class Event;
}  // namespace dom
}  // namespace mozilla

class nsRangeFrame final : public nsContainerFrame,
                           public nsIAnonymousContentCreator {
  friend nsIFrame* NS_NewRangeFrame(mozilla::PresShell* aPresShell,
                                    ComputedStyle* aStyle);

  friend class nsDisplayRangeFocusRing;

  explicit nsRangeFrame(ComputedStyle* aStyle, nsPresContext* aPresContext);
  virtual ~nsRangeFrame();

  typedef mozilla::PseudoStyleType PseudoStyleType;
  typedef mozilla::dom::Element Element;

 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsRangeFrame)

  // nsIFrame overrides
  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* aPrevInFlow) override;

  virtual void DestroyFrom(nsIFrame* aDestructRoot,
                           PostDestroyData& aPostDestroyData) override;

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override;

  virtual void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(NS_LITERAL_STRING("Range"), aResult);
  }
#endif

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() override;
#endif

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(
      nsTArray<ContentInfo>& aElements) override;
  virtual void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                        uint32_t aFilter) override;

  virtual nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                    int32_t aModType) override;

  virtual mozilla::LogicalSize ComputeAutoSize(
      gfxContext* aRenderingContext, mozilla::WritingMode aWM,
      const mozilla::LogicalSize& aCBSize, nscoord aAvailableISize,
      const mozilla::LogicalSize& aMargin, const mozilla::LogicalSize& aBorder,
      const mozilla::LogicalSize& aPadding, ComputeSizeFlags aFlags) override;

  virtual nscoord GetMinISize(gfxContext* aRenderingContext) override;
  virtual nscoord GetPrefISize(gfxContext* aRenderingContext) override;

  virtual bool IsFrameOfType(uint32_t aFlags) const override {
    return nsContainerFrame::IsFrameOfType(
        aFlags & ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

  ComputedStyle* GetAdditionalComputedStyle(int32_t aIndex) const override;
  void SetAdditionalComputedStyle(int32_t aIndex,
                                  ComputedStyle* aComputedStyle) override;

  /**
   * Returns true if the slider's thumb moves horizontally, or else false if it
   * moves vertically.
   */
  bool IsHorizontal() const;

  /**
   * Returns true if the slider is oriented along the inline axis.
   */
  bool IsInlineOriented() const {
    return IsHorizontal() != GetWritingMode().IsVertical();
  }

  /**
   * Returns true if the slider's thumb moves right-to-left for increasing
   * values; only relevant when IsHorizontal() is true.
   */
  bool IsRightToLeft() const {
    MOZ_ASSERT(IsHorizontal());
    mozilla::WritingMode wm = GetWritingMode();
    return wm.IsVertical() ? wm.IsVerticalRL() : !wm.IsBidiLTR();
  }

  double GetMin() const;
  double GetMax() const;
  double GetValue() const;

  /**
   * Returns the input element's value as a fraction of the difference between
   * the input's minimum and its maximum (i.e. returns 0.0 when the value is
   * the same as the minimum, and returns 1.0 when the value is the same as the
   * maximum).
   */
  double GetValueAsFractionOfRange();

  /**
   * Returns whether the frame and its child should use the native style.
   */
  bool ShouldUseNativeStyle() const;

  mozilla::Decimal GetValueAtEventPoint(mozilla::WidgetGUIEvent* aEvent);

  /**
   * Helper that's used when the value of the range changes to reposition the
   * thumb, resize the range-progress element, and schedule a repaint. (This
   * does not reflow, since the position and size of the thumb and
   * range-progress element do not affect the position or size of any other
   * frames.)
   */
  void UpdateForValueChange();

 private:
  // Return our preferred size in the cross-axis (the axis perpendicular
  // to the direction of movement of the thumb).
  nscoord AutoCrossSize(nscoord aEm);

  nsresult MakeAnonymousDiv(Element** aResult, PseudoStyleType aPseudoType,
                            nsTArray<ContentInfo>& aElements);

  // Helper function which reflows the anonymous div frames.
  void ReflowAnonymousContent(nsPresContext* aPresContext,
                              ReflowOutput& aDesiredSize,
                              const ReflowInput& aReflowInput);

  void DoUpdateThumbPosition(nsIFrame* aThumbFrame, const nsSize& aRangeSize);

  void DoUpdateRangeProgressFrame(nsIFrame* aProgressFrame,
                                  const nsSize& aRangeSize);

  /**
   * The div used to show the ::-moz-range-track pseudo-element.
   * @see nsRangeFrame::CreateAnonymousContent
   */
  nsCOMPtr<Element> mTrackDiv;

  /**
   * The div used to show the ::-moz-range-progress pseudo-element, which is
   * used to (optionally) style the specific chunk of track leading up to the
   * thumb's current position.
   * @see nsRangeFrame::CreateAnonymousContent
   */
  nsCOMPtr<Element> mProgressDiv;

  /**
   * The div used to show the ::-moz-range-thumb pseudo-element.
   * @see nsRangeFrame::CreateAnonymousContent
   */
  nsCOMPtr<Element> mThumbDiv;

  /**
   * Cached ComputedStyle for -moz-focus-outer CSS pseudo-element style.
   */
  RefPtr<ComputedStyle> mOuterFocusStyle;

  class DummyTouchListener final : public nsIDOMEventListener {
   private:
    ~DummyTouchListener() {}

   public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD HandleEvent(mozilla::dom::Event* aEvent) override {
      return NS_OK;
    }
  };

  /**
   * A no-op touch-listener used for APZ purposes (see nsRangeFrame::Init).
   */
  RefPtr<DummyTouchListener> mDummyTouchListener;
};

#endif
