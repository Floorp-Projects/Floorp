/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGINNERSVGFRAME_H__
#define __NS_SVGINNERSVGFRAME_H__

#include "SVGViewportFrame.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

nsIFrame* NS_NewSVGInnerSVGFrame(mozilla::PresShell* aPresShell,
                                 mozilla::ComputedStyle* aStyle);

namespace mozilla {

class SVGInnerSVGFrame final : public SVGViewportFrame {
  friend nsIFrame* ::NS_NewSVGInnerSVGFrame(mozilla::PresShell* aPresShell,
                                            ComputedStyle* aStyle);

 protected:
  explicit SVGInnerSVGFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : SVGViewportFrame(aStyle, aPresContext, kClassID) {}

 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(SVGInnerSVGFrame)

#ifdef DEBUG
  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* aPrevInFlow) override;
#endif

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"SVGInnerSVG"_ns, aResult);
  }
#endif
};

}  // namespace mozilla

#endif
