/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLmpaddedFrame_h___
#define nsMathMLmpaddedFrame_h___

#include "mozilla/Attributes.h"
#include "nsMathMLContainerFrame.h"

//
// <mpadded> -- adjust space around content  
//

class nsMathMLmpaddedFrame : public nsMathMLContainerFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewMathMLmpaddedFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_IMETHOD
  InheritAutomaticData(nsIFrame* aParent) override;

  NS_IMETHOD
  TransmitAutomaticData() override {
    return TransmitAutomaticDataForMrowLikeElement();
  }

  virtual void
  Reflow(nsPresContext*          aPresContext,
         nsHTMLReflowMetrics&     aDesiredSize,
         const nsHTMLReflowState& aReflowState,
         nsReflowStatus&          aStatus) override;
  
  virtual nsresult
  Place(DrawTarget*          aDrawTarget,
        bool                 aPlaceOrigin,
        nsHTMLReflowMetrics& aDesiredSize) override;

  bool
  IsMrowLike() override {
    return mFrames.FirstChild() != mFrames.LastChild() ||
           !mFrames.FirstChild();
  }

protected:
  explicit nsMathMLmpaddedFrame(nsStyleContext* aContext) : nsMathMLContainerFrame(aContext) {}
  virtual ~nsMathMLmpaddedFrame();

  virtual nsresult
  MeasureForWidth(DrawTarget* aDrawTarget,
                  nsHTMLReflowMetrics& aDesiredSize) override;

private:
  nsCSSValue mWidth;
  nsCSSValue mHeight;
  nsCSSValue mDepth;
  nsCSSValue mLeadingSpace;
  nsCSSValue mVerticalOffset;

  int32_t    mWidthSign;
  int32_t    mHeightSign;
  int32_t    mDepthSign;
  int32_t    mLeadingSpaceSign;
  int32_t    mVerticalOffsetSign;

  int32_t    mWidthPseudoUnit;
  int32_t    mHeightPseudoUnit;
  int32_t    mDepthPseudoUnit;
  int32_t    mLeadingSpacePseudoUnit;
  int32_t    mVerticalOffsetPseudoUnit;

  // helpers to process the attributes
  void
  ProcessAttributes();

  static bool
  ParseAttribute(nsString&   aString,
                 int32_t&    aSign,
                 nsCSSValue& aCSSValue,
                 int32_t&    aPseudoUnit);

  void
  UpdateValue(int32_t                  aSign,
              int32_t                  aPseudoUnit,
              const nsCSSValue&        aCSSValue,
              const nsHTMLReflowMetrics& aDesiredSize,
              nscoord&                 aValueToUpdate,
              float                    aFontSizeInflation) const;
};

#endif /* nsMathMLmpaddedFrame_h___ */
