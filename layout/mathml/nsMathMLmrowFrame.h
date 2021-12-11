/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLmrowFrame_h___
#define nsMathMLmrowFrame_h___

#include "mozilla/Attributes.h"
#include "nsMathMLContainerFrame.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

//
// <mrow> -- horizontally group any number of subexpressions
// <mphantom> -- make content invisible but preserve its size
// <mstyle> -- make style changes that affect the rendering of its contents
//

class nsMathMLmrowFrame final : public nsMathMLContainerFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsMathMLmrowFrame)

  friend nsIFrame* NS_NewMathMLmrowFrame(mozilla::PresShell* aPresShell,
                                         ComputedStyle* aStyle);

  virtual nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                    int32_t aModType) override;

  NS_IMETHOD
  InheritAutomaticData(nsIFrame* aParent) override;

  NS_IMETHOD
  TransmitAutomaticData() override {
    return TransmitAutomaticDataForMrowLikeElement();
  }

  virtual eMathMLFrameType GetMathMLFrameType() override;

  bool IsMrowLike() override {
    // <mrow> elements with a single child are treated identically to the case
    // where the child wasn't within an mrow, so we pretend the mrow isn't an
    // mrow in that situation.
    return mFrames.FirstChild() != mFrames.LastChild() || !mFrames.FirstChild();
  }

 protected:
  explicit nsMathMLmrowFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : nsMathMLContainerFrame(aStyle, aPresContext, kClassID) {}
  virtual ~nsMathMLmrowFrame();
};

#endif /* nsMathMLmrowFrame_h___ */
