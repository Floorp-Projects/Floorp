/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLTokenFrame_h___
#define nsMathMLTokenFrame_h___

#include "mozilla/Attributes.h"
#include "nsMathMLContainerFrame.h"

//
// Base class to handle token elements
//

class nsMathMLTokenFrame : public nsMathMLContainerFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewMathMLTokenFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_IMETHOD
  TransmitAutomaticData() override {
    // The REC defines the following elements to be space-like:
    // * an mtext, mspace, maligngroup, or malignmark element;
    if (mContent->IsMathMLElement(nsGkAtoms::mtext_)) {
      mPresentationData.flags |= NS_MATHML_SPACE_LIKE;
    }
    return NS_OK;
  }

  NS_IMETHOD
  InheritAutomaticData(nsIFrame* aParent) override;

  virtual eMathMLFrameType GetMathMLFrameType() override;

  virtual void
  SetInitialChildList(ChildListID     aListID,
                      nsFrameList&    aChildList) override;

  virtual void
  AppendFrames(ChildListID            aListID,
               nsFrameList&           aChildList) override;

  virtual void
  InsertFrames(ChildListID            aListID,
               nsIFrame*              aPrevFrame,
               nsFrameList&           aChildList) override;

  virtual void
  Reflow(nsPresContext*          aPresContext,
         nsHTMLReflowMetrics&     aDesiredSize,
         const nsHTMLReflowState& aReflowState,
         nsReflowStatus&          aStatus) override;

  virtual nsresult
  Place(DrawTarget*          aDrawTarget,
        bool                 aPlaceOrigin,
        nsHTMLReflowMetrics& aDesiredSize) override;

protected:
  explicit nsMathMLTokenFrame(nsStyleContext* aContext) : nsMathMLContainerFrame(aContext) {}
  virtual ~nsMathMLTokenFrame();

  void MarkTextFramesAsTokenMathML();
};

#endif /* nsMathMLTokentFrame_h___ */
