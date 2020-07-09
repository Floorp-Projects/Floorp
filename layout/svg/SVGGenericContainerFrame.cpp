/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "SVGGenericContainerFrame.h"
#include "nsSVGIntegrationUtils.h"

#include "mozilla/PresShell.h"

//----------------------------------------------------------------------
// SVGGenericContainerFrame Implementation

nsIFrame* NS_NewSVGGenericContainerFrame(mozilla::PresShell* aPresShell,
                                         mozilla::ComputedStyle* aStyle) {
  return new (aPresShell)
      mozilla::SVGGenericContainerFrame(aStyle, aPresShell->GetPresContext());
}

namespace mozilla {

NS_IMPL_FRAMEARENA_HELPERS(SVGGenericContainerFrame)

//----------------------------------------------------------------------
// nsIFrame methods

nsresult SVGGenericContainerFrame::AttributeChanged(int32_t aNameSpaceID,
                                                    nsAtom* aAttribute,
                                                    int32_t aModType) {
#ifdef DEBUG
  nsAutoString str;
  aAttribute->ToString(str);
  printf("** SVGGenericContainerFrame::AttributeChanged(%s)\n",
         NS_LossyConvertUTF16toASCII(str).get());
#endif

  return NS_OK;
}

//----------------------------------------------------------------------
// SVGContainerFrame methods:

gfxMatrix SVGGenericContainerFrame::GetCanvasTM() {
  NS_ASSERTION(GetParent(), "null parent");

  return static_cast<SVGContainerFrame*>(GetParent())->GetCanvasTM();
}

}  // namespace mozilla
