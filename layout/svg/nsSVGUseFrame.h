/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGUSEFRAME_H__
#define __NS_SVGUSEFRAME_H__

// Keep in (case-insensitive) order:
#include "nsSVGGFrame.h"

class nsSVGUseFrame final
  : public nsSVGGFrame
{
  friend nsIFrame* NS_NewSVGUseFrame(nsIPresShell* aPresShell,
                                     ComputedStyle* aStyle);

protected:
  explicit nsSVGUseFrame(ComputedStyle* aStyle)
    : nsSVGGFrame(aStyle, kClassID)
    , mHasValidDimensions(true)
  {
  }

public:
  NS_DECL_FRAMEARENA_HELPERS(nsSVGUseFrame)

  // nsIFrame interface:
  void Init(nsIContent* aContent,
            nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;

  nsresult AttributeChanged(int32_t aNameSpaceID,
                            nsAtom* aAttribute,
                            int32_t aModType) override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGUse"), aResult);
  }
#endif

  // nsSVGDisplayableFrame interface:
  void ReflowSVG() override;
  void NotifySVGChanged(uint32_t aFlags) override;

private:
  bool mHasValidDimensions;
};

#endif // __NS_SVGUSEFRAME_H__
