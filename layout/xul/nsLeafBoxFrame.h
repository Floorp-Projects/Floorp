/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsLeafBoxFrame_h___
#define nsLeafBoxFrame_h___

#include "mozilla/Attributes.h"
#include "nsLeafFrame.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

class nsLeafBoxFrame : public nsLeafFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsLeafBoxFrame)

  friend nsIFrame* NS_NewLeafBoxFrame(mozilla::PresShell* aPresShell,
                                      ComputedStyle* aStyle);

  virtual nsSize GetXULPrefSize(nsBoxLayoutState& aState) override;
  virtual nsSize GetXULMinSize(nsBoxLayoutState& aState) override;
  virtual nsSize GetXULMaxSize(nsBoxLayoutState& aState) override;
  virtual nscoord GetXULFlex() override;
  virtual nscoord GetXULBoxAscent(nsBoxLayoutState& aState) override;

  virtual bool IsFrameOfType(uint32_t aFlags) const override {
    // This is bogus, but it's what we've always done.
    // Note that nsLeafFrame is also eReplacedContainsBlock.
    return nsLeafFrame::IsFrameOfType(
        aFlags & ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock |
                   nsIFrame::eXULBox));
  }

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  // nsIHTMLReflow overrides

  virtual nscoord GetMinISize(gfxContext* aRenderingContext) override;
  virtual nscoord GetPrefISize(gfxContext* aRenderingContext) override;

  // Our auto size is that provided by nsFrame, not nsLeafFrame
  mozilla::LogicalSize ComputeAutoSize(
      gfxContext* aRenderingContext, mozilla::WritingMode aWM,
      const mozilla::LogicalSize& aCBSize, nscoord aAvailableISize,
      const mozilla::LogicalSize& aMargin,
      const mozilla::LogicalSize& aBorderPadding,
      const mozilla::StyleSizeOverrides& aSizeOverrides,
      mozilla::ComputeSizeFlags aFlags) override;

  virtual void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;

  virtual nsresult CharacterDataChanged(
      const CharacterDataChangeInfo&) override;

  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* asPrevInFlow) override;

  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsDisplayListSet& aLists) override;

  virtual bool XULComputesOwnOverflowArea() override { return false; }

 protected:
  NS_IMETHOD DoXULLayout(nsBoxLayoutState& aState) override;

  virtual nscoord GetIntrinsicISize() override;

  explicit nsLeafBoxFrame(ComputedStyle* aStyle, nsPresContext* aPresContext,
                          ClassID aID = kClassID)
      : nsLeafFrame(aStyle, aPresContext, aID) {}

};  // class nsLeafBoxFrame

#endif /* nsLeafBoxFrame_h___ */
