/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsCOMPtr.h"

class nsBaseContentList;
class nsDisplayRangeFocusRing;

class nsRangeFrame : public nsContainerFrame,
                     public nsIAnonymousContentCreator
{
  friend nsIFrame*
  NS_NewRangeFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  friend class nsDisplayRangeFocusRing;

  nsRangeFrame(nsStyleContext* aContext);
  virtual ~nsRangeFrame();

  typedef mozilla::dom::Element Element;

public:
  NS_DECL_QUERYFRAME_TARGET(nsRangeFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // nsIFrame overrides
  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) MOZ_OVERRIDE;

  virtual void DestroyFrom(nsIFrame* aDestructRoot) MOZ_OVERRIDE;

  void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                        const nsRect&           aDirtyRect,
                        const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  virtual void Reflow(nsPresContext*           aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus) MOZ_OVERRIDE;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE {
    return MakeFrameName(NS_LITERAL_STRING("Range"), aResult);
  }
#endif

  virtual bool IsLeaf() const MOZ_OVERRIDE { return true; }

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() MOZ_OVERRIDE;
#endif

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) MOZ_OVERRIDE;
  virtual void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                        uint32_t aFilter) MOZ_OVERRIDE;

  virtual nsresult AttributeChanged(int32_t  aNameSpaceID,
                                    nsIAtom* aAttribute,
                                    int32_t  aModType) MOZ_OVERRIDE;

  virtual mozilla::LogicalSize
  ComputeAutoSize(nsRenderingContext *aRenderingContext,
                  mozilla::WritingMode aWritingMode,
                  const mozilla::LogicalSize& aCBSize,
                  nscoord aAvailableISize,
                  const mozilla::LogicalSize& aMargin,
                  const mozilla::LogicalSize& aBorder,
                  const mozilla::LogicalSize& aPadding,
                  bool aShrinkWrap) MOZ_OVERRIDE;

  virtual nscoord GetMinISize(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual nscoord GetPrefISize(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;

  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
  {
    return nsContainerFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

  nsStyleContext* GetAdditionalStyleContext(int32_t aIndex) const MOZ_OVERRIDE;
  void SetAdditionalStyleContext(int32_t aIndex,
                                 nsStyleContext* aStyleContext) MOZ_OVERRIDE;

  /**
   * Returns true if the slider's thumb moves horizontally, or else false if it
   * moves vertically.
   *
   * aOverrideFrameSize If specified, this will be used instead of the size of
   *   the frame's rect (i.e. the frame's border-box size) if the frame's
   *   rect would have otherwise been examined. This should only be specified
   *   during reflow when the frame's [new] border-box size has not yet been
   *   stored in its mRect.
   */
  bool IsHorizontal(const nsSize *aFrameSizeOverride = nullptr) const;

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

  virtual Element* GetPseudoElement(nsCSSPseudoElements::Type aType) MOZ_OVERRIDE;

private:

  nsresult MakeAnonymousDiv(Element** aResult,
                            nsCSSPseudoElements::Type aPseudoType,
                            nsTArray<ContentInfo>& aElements);

  // Helper function which reflows the anonymous div frames.
  void ReflowAnonymousContent(nsPresContext*           aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState);

  void DoUpdateThumbPosition(nsIFrame* aThumbFrame,
                             const nsSize& aRangeSize);

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
   * Cached style context for -moz-focus-outer CSS pseudo-element style.
   */
  nsRefPtr<nsStyleContext> mOuterFocusStyle;
};

#endif
