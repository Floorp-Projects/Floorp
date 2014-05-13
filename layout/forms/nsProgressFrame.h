/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsProgressFrame_h___
#define nsProgressFrame_h___

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"
#include "nsIAnonymousContentCreator.h"
#include "nsCOMPtr.h"

class nsBaseContentList;

class nsProgressFrame : public nsContainerFrame,
                        public nsIAnonymousContentCreator
{
  typedef mozilla::dom::Element Element;

public:
  NS_DECL_QUERYFRAME_TARGET(nsProgressFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  nsProgressFrame(nsStyleContext* aContext);
  virtual ~nsProgressFrame();

  virtual void DestroyFrom(nsIFrame* aDestructRoot) MOZ_OVERRIDE;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  virtual void Reflow(nsPresContext*           aCX,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus) MOZ_OVERRIDE;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE {
    return MakeFrameName(NS_LITERAL_STRING("Progress"), aResult);
  }
#endif

  virtual bool IsLeaf() const MOZ_OVERRIDE { return true; }

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) MOZ_OVERRIDE;
  virtual void AppendAnonymousContentTo(nsBaseContentList& aElements,
                                        uint32_t aFilter) MOZ_OVERRIDE;

  virtual nsresult AttributeChanged(int32_t  aNameSpaceID,
                                    nsIAtom* aAttribute,
                                    int32_t  aModType) MOZ_OVERRIDE;

  virtual nsSize ComputeAutoSize(nsRenderingContext *aRenderingContext,
                                 nsSize aCBSize, nscoord aAvailableWidth,
                                 nsSize aMargin, nsSize aBorder,
                                 nsSize aPadding, bool aShrinkWrap) MOZ_OVERRIDE;

  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;

  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
  {
    return nsContainerFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

  /**
   * Returns whether the frame and its child should use the native style.
   */
  bool ShouldUseNativeStyle() const;

  virtual Element* GetPseudoElement(nsCSSPseudoElements::Type aType) MOZ_OVERRIDE;

protected:
  // Helper function which reflow the anonymous div frame.
  void ReflowBarFrame(nsIFrame*                aBarFrame,
                      nsPresContext*           aPresContext,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus);

  /**
   * The div used to show the progress bar.
   * @see nsProgressFrame::CreateAnonymousContent
   */
  nsCOMPtr<Element> mBarDiv;
};

#endif

