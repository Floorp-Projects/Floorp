/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGUSEFRAME_H__
#define __NS_SVGUSEFRAME_H__

// Keep in (case-insensitive) order:
#include "nsSVGGFrame.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

class nsSVGUseFrame final : public nsSVGGFrame {
  friend nsIFrame* NS_NewSVGUseFrame(mozilla::PresShell* aPresShell,
                                     ComputedStyle* aStyle);

 protected:
  explicit nsSVGUseFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : nsSVGGFrame(aStyle, aPresContext, kClassID),
        mHasValidDimensions(true) {}

 public:
  NS_DECL_FRAMEARENA_HELPERS(nsSVGUseFrame)

  // nsIFrame interface:
  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;

  // Called when the x or y attributes changed.
  void PositionAttributeChanged();

  // Called when the href attributes changed.
  void HrefChanged();

  // Called when the width or height attributes changed.
  void DimensionAttributeChanged(bool aHadValidDimensions,
                                 bool aAttributeIsUsed);

  nsresult AttributeChanged(int32_t aNamespaceID, nsAtom* aAttribute,
                            int32_t aModType) final;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(NS_LITERAL_STRING("SVGUse"), aResult);
  }
#endif

  // nsSVGDisplayableFrame interface:
  void ReflowSVG() override;
  void NotifySVGChanged(uint32_t aFlags) override;

 private:
  bool mHasValidDimensions;
};

#endif  // __NS_SVGUSEFRAME_H__
