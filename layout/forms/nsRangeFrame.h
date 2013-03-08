/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRangeFrame_h___
#define nsRangeFrame_h___

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"
#include "nsIAnonymousContentCreator.h"
#include "nsCOMPtr.h"

class nsBaseContentList;
class nsGUIEvent;

class nsRangeFrame : public nsContainerFrame,
                     public nsIAnonymousContentCreator
{
  friend nsIFrame*
  NS_NewRangeFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  nsRangeFrame(nsStyleContext* aContext);
  virtual ~nsRangeFrame();

public:
  NS_DECL_QUERYFRAME_TARGET(nsRangeFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // nsIFrame overrides
  virtual void DestroyFrom(nsIFrame* aDestructRoot) MOZ_OVERRIDE;

  void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                        const nsRect&           aDirtyRect,
                        const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  NS_IMETHOD Reflow(nsPresContext*           aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus) MOZ_OVERRIDE;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const MOZ_OVERRIDE {
    return MakeFrameName(NS_LITERAL_STRING("Range"), aResult);
  }
#endif

  virtual bool IsLeaf() const MOZ_OVERRIDE { return true; }

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) MOZ_OVERRIDE;
  virtual void AppendAnonymousContentTo(nsBaseContentList& aElements,
                                        uint32_t aFilter) MOZ_OVERRIDE;

  NS_IMETHOD AttributeChanged(int32_t  aNameSpaceID,
                              nsIAtom* aAttribute,
                              int32_t  aModType) MOZ_OVERRIDE;

  virtual nsSize ComputeAutoSize(nsRenderingContext *aRenderingContext,
                                 nsSize aCBSize, nscoord aAvailableWidth,
                                 nsSize aMargin, nsSize aBorder,
                                 nsSize aPadding, bool aShrinkWrap) MOZ_OVERRIDE;

  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;

  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
  {
    return nsContainerFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

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

  /**
   * Returns whether the frame and its child should use the native style.
   */
  bool ShouldUseNativeStyle() const;

  double GetValueAtEventPoint(nsGUIEvent* aEvent);

private:

  // Helper function which reflows the anonymous div frames.
  nsresult ReflowAnonymousContent(nsPresContext*           aPresContext,
                                  nsHTMLReflowMetrics&     aDesiredSize,
                                  const nsHTMLReflowState& aReflowState);

  /**
   * Returns the input element's value as a fraction of the difference between
   * the input's minimum and its maximum (i.e. returns 0.0 when the value is
   * the same as the minimum, and returns 1.0 when the value is the same as the
   * maximum).
   */
  double GetValueAsFractionOfRange();

  /**
   * The div used to show the track.
   * @see nsRangeFrame::CreateAnonymousContent
   */
  nsCOMPtr<nsIContent> mTrackDiv;

  /**
   * The div used to show the thumb.
   * @see nsRangeFrame::CreateAnonymousContent
   */
  nsCOMPtr<nsIContent> mThumbDiv;
};

#endif
