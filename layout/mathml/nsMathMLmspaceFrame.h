/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLmspaceFrame_h___
#define nsMathMLmspaceFrame_h___

#include "mozilla/Attributes.h"
#include "nsMathMLContainerFrame.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

//
// <mspace> -- space
//

class nsMathMLmspaceFrame final : public nsMathMLContainerFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsMathMLmspaceFrame)

  friend nsIFrame* NS_NewMathMLmspaceFrame(mozilla::PresShell* aPresShell,
                                           ComputedStyle* aStyle);

  NS_IMETHOD
  TransmitAutomaticData() override {
    // The REC defines the following elements to be space-like:
    // * an mtext, mspace, maligngroup, or malignmark element;
    mPresentationData.flags |= NS_MATHML_SPACE_LIKE;
    return NS_OK;
  }

  virtual void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;

 protected:
  explicit nsMathMLmspaceFrame(ComputedStyle* aStyle,
                               nsPresContext* aPresContext)
      : nsMathMLContainerFrame(aStyle, aPresContext, kClassID),
        mWidth(0),
        mHeight(0),
        mDepth(0) {}
  virtual ~nsMathMLmspaceFrame();

  virtual nsresult MeasureForWidth(DrawTarget* aDrawTarget,
                                   ReflowOutput& aDesiredSize) override;

 private:
  nscoord mWidth;
  nscoord mHeight;
  nscoord mDepth;

  // helper method to initialize our member data
  void ProcessAttributes(nsPresContext* aPresContext);
};

#endif /* nsMathMLmspaceFrame_h___ */
