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

namespace mozilla {
enum class CSSPseudoElementType : uint8_t;
} // namespace mozilla

class nsProgressFrame : public nsContainerFrame,
                        public nsIAnonymousContentCreator
{
  typedef mozilla::CSSPseudoElementType CSSPseudoElementType;
  typedef mozilla::dom::Element Element;

public:
  NS_DECL_QUERYFRAME_TARGET(nsProgressFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  explicit nsProgressFrame(nsStyleContext* aContext);
  virtual ~nsProgressFrame();

  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  virtual void Reflow(nsPresContext*           aCX,
                      ReflowOutput&     aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus&          aStatus) override;

  virtual nsIAtom* GetType() const override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(NS_LITERAL_STRING("Progress"), aResult);
  }
#endif

  virtual bool IsLeaf() const override { return true; }

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) override;
  virtual void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                        uint32_t aFilter) override;

  virtual nsresult AttributeChanged(int32_t  aNameSpaceID,
                                    nsIAtom* aAttribute,
                                    int32_t  aModType) override;

  virtual mozilla::LogicalSize
  ComputeAutoSize(nsRenderingContext *aRenderingContext,
                  mozilla::WritingMode aWritingMode,
                  const mozilla::LogicalSize& aCBSize,
                  nscoord aAvailableISize,
                  const mozilla::LogicalSize& aMargin,
                  const mozilla::LogicalSize& aBorder,
                  const mozilla::LogicalSize& aPadding,
                  bool aShrinkWrap) override;

  virtual nscoord GetMinISize(nsRenderingContext *aRenderingContext) override;
  virtual nscoord GetPrefISize(nsRenderingContext *aRenderingContext) override;

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsContainerFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

  /**
   * Returns whether the frame and its child should use the native style.
   */
  bool ShouldUseNativeStyle() const;

  virtual Element* GetPseudoElement(CSSPseudoElementType aType) override;

protected:
  // Helper function which reflow the anonymous div frame.
  void ReflowBarFrame(nsIFrame*                aBarFrame,
                      nsPresContext*           aPresContext,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus&          aStatus);

  /**
   * The div used to show the progress bar.
   * @see nsProgressFrame::CreateAnonymousContent
   */
  nsCOMPtr<Element> mBarDiv;
};

#endif

