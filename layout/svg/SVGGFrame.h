/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_SVG_SVGGFRAME_H_
#define LAYOUT_SVG_SVGGFRAME_H_

#include "mozilla/Attributes.h"
#include "mozilla/SVGContainerFrame.h"
#include "gfxMatrix.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

nsIFrame* NS_NewSVGGFrame(mozilla::PresShell* aPresShell,
                          mozilla::ComputedStyle* aStyle);

namespace mozilla {

class SVGGFrame : public SVGDisplayContainerFrame {
  friend nsIFrame* ::NS_NewSVGGFrame(mozilla::PresShell* aPresShell,
                                     mozilla::ComputedStyle* aStyle);
  explicit SVGGFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : SVGGFrame(aStyle, aPresContext, kClassID) {}

 protected:
  SVGGFrame(ComputedStyle* aStyle, nsPresContext* aPresContext,
            nsIFrame::ClassID aID)
      : SVGDisplayContainerFrame(aStyle, aPresContext, aID) {}

 public:
  NS_DECL_FRAMEARENA_HELPERS(SVGGFrame)

#ifdef DEBUG
  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;
#endif

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"SVGG"_ns, aResult);
  }
#endif

  // nsIFrame interface:
  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;
};

}  // namespace mozilla

#endif  // LAYOUT_SVG_SVGGFRAME_H_
