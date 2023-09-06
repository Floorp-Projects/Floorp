/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
enum class PseudoStyleType : uint8_t;
}  // namespace mozilla

class nsProgressFrame final : public nsContainerFrame,
                              public nsIAnonymousContentCreator {
  typedef mozilla::PseudoStyleType PseudoStyleType;
  typedef mozilla::dom::Element Element;

 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsProgressFrame)

  explicit nsProgressFrame(ComputedStyle* aStyle, nsPresContext* aPresContext);
  virtual ~nsProgressFrame();

  void Destroy(DestroyContext&) override;

  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsDisplayListSet& aLists) override;

  virtual void Reflow(nsPresContext* aCX, ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"Progress"_ns, aResult);
  }
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
      const mozilla::LogicalSize& aMargin,
      const mozilla::LogicalSize& aBorderPadding,
      const mozilla::StyleSizeOverrides& aSizeOverrides,
      mozilla::ComputeSizeFlags aFlags) override;

  virtual nscoord GetMinISize(gfxContext* aRenderingContext) override;
  virtual nscoord GetPrefISize(gfxContext* aRenderingContext) override;

  virtual bool IsFrameOfType(uint32_t aFlags) const override {
    return nsContainerFrame::IsFrameOfType(
        aFlags & ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

  /**
   * Returns whether the frame and its child should use the native style.
   */
  bool ShouldUseNativeStyle() const;

 protected:
  // Helper function to reflow a child frame.
  void ReflowChildFrame(nsIFrame* aChild, nsPresContext* aPresContext,
                        const ReflowInput& aReflowInput,
                        nsReflowStatus& aStatus);

  /**
   * The div used to show the progress bar.
   * @see nsProgressFrame::CreateAnonymousContent
   */
  nsCOMPtr<Element> mBarDiv;
};

#endif
