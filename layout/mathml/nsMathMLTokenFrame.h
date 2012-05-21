/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLTokenFrame_h___
#define nsMathMLTokenFrame_h___

#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

//
// Base class to handle token elements
//

class nsMathMLTokenFrame : public nsMathMLContainerFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewMathMLTokenFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_IMETHOD
  TransmitAutomaticData() {
    // The REC defines the following elements to be space-like:
    // * an mtext, mspace, maligngroup, or malignmark element;
    if (mContent->Tag() == nsGkAtoms::mtext_) {
      mPresentationData.flags |= NS_MATHML_SPACE_LIKE;
    }
    return NS_OK;
  }

  NS_IMETHOD
  InheritAutomaticData(nsIFrame* aParent);

  virtual eMathMLFrameType GetMathMLFrameType();

  NS_IMETHOD
  Init(nsIContent*      aContent,
       nsIFrame*        aParent,
       nsIFrame*        aPrevInFlow);

  NS_IMETHOD
  SetInitialChildList(ChildListID     aListID,
                      nsFrameList&    aChildList);

  NS_IMETHOD
  Reflow(nsPresContext*          aPresContext,
         nsHTMLReflowMetrics&     aDesiredSize,
         const nsHTMLReflowState& aReflowState,
         nsReflowStatus&          aStatus);

  virtual nsresult
  Place(nsRenderingContext& aRenderingContext,
        bool                 aPlaceOrigin,
        nsHTMLReflowMetrics& aDesiredSize);

  virtual void MarkIntrinsicWidthsDirty();

  NS_IMETHOD
  AttributeChanged(PRInt32         aNameSpaceID,
                   nsIAtom*        aAttribute,
                   PRInt32         aModType);

  virtual nsresult
  ChildListChanged(PRInt32 aModType)
  {
    ProcessTextData();
    return nsMathMLContainerFrame::ChildListChanged(aModType);
  }

protected:
  nsMathMLTokenFrame(nsStyleContext* aContext) : nsMathMLContainerFrame(aContext) {}
  virtual ~nsMathMLTokenFrame();

  virtual PRIntn GetSkipSides() const { return 0; }

  // hook to perform MathML-specific actions depending on the tag
  virtual void ProcessTextData();

  // helper to set the style of <mi> which has to be italic or normal
  // depending on its textual content
  bool SetTextStyle();

  // helper to set the quotes of <ms>
  void SetQuotes(bool aNotify);
};

#endif /* nsMathMLTokentFrame_h___ */
