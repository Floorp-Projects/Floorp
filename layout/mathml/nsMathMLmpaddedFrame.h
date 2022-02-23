/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLmpaddedFrame_h___
#define nsMathMLmpaddedFrame_h___

#include "mozilla/Attributes.h"
#include "nsCSSValue.h"
#include "nsMathMLContainerFrame.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

//
// <mpadded> -- adjust space around content
//

class nsMathMLmpaddedFrame final : public nsMathMLContainerFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsMathMLmpaddedFrame)

  friend nsIFrame* NS_NewMathMLmpaddedFrame(mozilla::PresShell* aPresShell,
                                            ComputedStyle* aStyle);

  NS_IMETHOD
  InheritAutomaticData(nsIFrame* aParent) override;

  NS_IMETHOD
  TransmitAutomaticData() override {
    return TransmitAutomaticDataForMrowLikeElement();
  }

  virtual void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;

  virtual nsresult Place(DrawTarget* aDrawTarget, bool aPlaceOrigin,
                         ReflowOutput& aDesiredSize) override;

  bool IsMrowLike() override {
    return mFrames.FirstChild() != mFrames.LastChild() || !mFrames.FirstChild();
  }

 protected:
  explicit nsMathMLmpaddedFrame(ComputedStyle* aStyle,
                                nsPresContext* aPresContext)
      : nsMathMLContainerFrame(aStyle, aPresContext, kClassID),
        mWidthSign(0),
        mHeightSign(0),
        mDepthSign(0),
        mLeadingSpaceSign(0),
        mVerticalOffsetSign(0),
        mWidthPseudoUnit(0),
        mHeightPseudoUnit(0),
        mDepthPseudoUnit(0),
        mLeadingSpacePseudoUnit(0),
        mVerticalOffsetPseudoUnit(0) {}

  virtual ~nsMathMLmpaddedFrame();

  virtual nsresult MeasureForWidth(DrawTarget* aDrawTarget,
                                   ReflowOutput& aDesiredSize) override;

 private:
  nsCSSValue mWidth;
  nsCSSValue mHeight;
  nsCSSValue mDepth;
  nsCSSValue mLeadingSpace;
  nsCSSValue mVerticalOffset;

  int32_t mWidthSign;
  int32_t mHeightSign;
  int32_t mDepthSign;
  int32_t mLeadingSpaceSign;
  int32_t mVerticalOffsetSign;

  int32_t mWidthPseudoUnit;
  int32_t mHeightPseudoUnit;
  int32_t mDepthPseudoUnit;
  int32_t mLeadingSpacePseudoUnit;
  int32_t mVerticalOffsetPseudoUnit;

  // helpers to process the attributes
  void ProcessAttributes();

  bool ParseAttribute(nsString& aString, int32_t& aSign, nsCSSValue& aCSSValue,
                      int32_t& aPseudoUnit);

  void UpdateValue(int32_t aSign, int32_t aPseudoUnit,
                   const nsCSSValue& aCSSValue,
                   const ReflowOutput& aDesiredSize, nscoord& aValueToUpdate,
                   float aFontSizeInflation) const;
};

#endif /* nsMathMLmpaddedFrame_h___ */
