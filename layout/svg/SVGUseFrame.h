/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_SVG_SVGUSEFRAME_H_
#define LAYOUT_SVG_SVGUSEFRAME_H_

// Keep in (case-insensitive) order:
#include "SVGGFrame.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

nsIFrame* NS_NewSVGUseFrame(mozilla::PresShell* aPresShell,
                            mozilla::ComputedStyle* aStyle);

namespace mozilla {

class SVGUseFrame final : public SVGGFrame {
  friend nsIFrame* ::NS_NewSVGUseFrame(mozilla::PresShell* aPresShell,
                                       ComputedStyle* aStyle);

 protected:
  explicit SVGUseFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : SVGGFrame(aStyle, aPresContext, kClassID), mHasValidDimensions(true) {}

 public:
  NS_DECL_FRAMEARENA_HELPERS(SVGUseFrame)

  // nsIFrame interface:
  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;

  // Called when the href attributes changed.
  void HrefChanged();

  // Called when the width or height attributes changed.
  void DimensionAttributeChanged(bool aHadValidDimensions,
                                 bool aAttributeIsUsed);

  nsresult AttributeChanged(int32_t aNamespaceID, nsAtom* aAttribute,
                            int32_t aModType) override;
  void DidSetComputedStyle(ComputedStyle* aOldComputedStyle) override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"SVGUse"_ns, aResult);
  }
#endif

  // ISVGDisplayableFrame interface:
  void ReflowSVG() override;
  void NotifySVGChanged(uint32_t aFlags) override;

 private:
  bool mHasValidDimensions;
};

}  // namespace mozilla

#endif  // LAYOUT_SVG_SVGUSEFRAME_H_
