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
  TransmitAutomaticData() MOZ_OVERRIDE {
    // The REC defines the following elements to be space-like:
    // * an mtext, mspace, maligngroup, or malignmark element;
    if (mContent->Tag() == nsGkAtoms::mtext_) {
      mPresentationData.flags |= NS_MATHML_SPACE_LIKE;
    }
    return NS_OK;
  }

  NS_IMETHOD
  InheritAutomaticData(nsIFrame* aParent) MOZ_OVERRIDE;

  virtual eMathMLFrameType GetMathMLFrameType() MOZ_OVERRIDE;

  virtual void
  SetInitialChildList(ChildListID     aListID,
                      nsFrameList&    aChildList) MOZ_OVERRIDE;

  virtual void
  AppendFrames(ChildListID            aListID,
               nsFrameList&           aChildList) MOZ_OVERRIDE;

  virtual void
  InsertFrames(ChildListID            aListID,
               nsIFrame*              aPrevFrame,
               nsFrameList&           aChildList) MOZ_OVERRIDE;

  virtual void
  Reflow(nsPresContext*          aPresContext,
         nsHTMLReflowMetrics&     aDesiredSize,
         const nsHTMLReflowState& aReflowState,
         nsReflowStatus&          aStatus) MOZ_OVERRIDE;

  virtual nsresult
  Place(nsRenderingContext& aRenderingContext,
        bool                 aPlaceOrigin,
        nsHTMLReflowMetrics& aDesiredSize) MOZ_OVERRIDE;

protected:
  nsMathMLTokenFrame(nsStyleContext* aContext) : nsMathMLContainerFrame(aContext) {}
  virtual ~nsMathMLTokenFrame();

  void MarkTextFramesAsTokenMathML();
};

#endif /* nsMathMLTokentFrame_h___ */
