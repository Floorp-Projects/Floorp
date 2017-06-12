/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLSelectedFrame_h___
#define nsMathMLSelectedFrame_h___

#include "nsMathMLContainerFrame.h"

class nsMathMLSelectedFrame : public nsMathMLContainerFrame {
public:
  NS_IMETHOD
  TransmitAutomaticData() override;

  virtual void
  SetInitialChildList(ChildListID     aListID,
                      nsFrameList&    aChildList) override;

  virtual nsresult
  ChildListChanged(int32_t aModType) override;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  virtual nsresult
  Place(DrawTarget*          aDrawTarget,
        bool                 aPlaceOrigin,
        ReflowOutput& aDesiredSize) override;

  virtual mozilla::LogicalSize
  ComputeSize(nsRenderingContext *aRenderingContext,
              mozilla::WritingMode aWritingMode,
              const mozilla::LogicalSize& aCBSize,
              nscoord aAvailableISize,
              const mozilla::LogicalSize& aMargin,
              const mozilla::LogicalSize& aBorder,
              const mozilla::LogicalSize& aPadding,
              ComputeSizeFlags aFlags) override;

  virtual void
  Reflow(nsPresContext*          aPresContext,
         ReflowOutput&     aDesiredSize,
         const ReflowInput& aReflowInput,
         nsReflowStatus&          aStatus) override;

  virtual nsQueryFrame::FrameIID GetFrameId() override = 0;

protected:
  nsMathMLSelectedFrame(nsStyleContext* aContext, ClassID aID) :
    nsMathMLContainerFrame(aContext, aID),
    mSelectedFrame(nullptr),
    mInvalidMarkup(false) {}
  virtual ~nsMathMLSelectedFrame();
  
  virtual nsIFrame* GetSelectedFrame() = 0;
  nsIFrame*       mSelectedFrame;

  bool            mInvalidMarkup;
  
private:
  void* operator new(size_t, nsIPresShell*) MOZ_MUST_OVERRIDE = delete;
};

#endif /* nsMathMLSelectedFrame_h___ */
