/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLSelectedFrame_h___
#define nsMathMLSelectedFrame_h___

#include "nsMathMLContainerFrame.h"

class nsMathMLSelectedFrame : public nsMathMLContainerFrame {
 public:
  NS_DECL_ABSTRACT_FRAME(nsMathMLSelectedFrame)

  NS_IMETHOD
  TransmitAutomaticData() override;

  void SetInitialChildList(ChildListID aListID,
                           nsFrameList&& aChildList) override;

  virtual nsresult ChildListChanged(int32_t aModType) override;

  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsDisplayListSet& aLists) override;

  virtual nsresult Place(DrawTarget* aDrawTarget, bool aPlaceOrigin,
                         ReflowOutput& aDesiredSize) override;

  SizeComputationResult ComputeSize(
      gfxContext* aRenderingContext, mozilla::WritingMode aWM,
      const mozilla::LogicalSize& aCBSize, nscoord aAvailableISize,
      const mozilla::LogicalSize& aMargin,
      const mozilla::LogicalSize& aBorderPadding,
      const mozilla::StyleSizeOverrides& aSizeOverrides,
      mozilla::ComputeSizeFlags aFlags) override;

  virtual void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;

 protected:
  nsMathMLSelectedFrame(ComputedStyle* aStyle, nsPresContext* aPresContext,
                        ClassID aID)
      : nsMathMLContainerFrame(aStyle, aPresContext, aID),
        mSelectedFrame(nullptr),
        mInvalidMarkup(false) {}
  virtual ~nsMathMLSelectedFrame();

  virtual nsIFrame* GetSelectedFrame() = 0;
  nsIFrame* mSelectedFrame;

  bool mInvalidMarkup;
};

#endif /* nsMathMLSelectedFrame_h___ */
