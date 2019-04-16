/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGGenericContainerFrame.h"
#include "nsSVGIntegrationUtils.h"

#include "mozilla/PresShell.h"

using namespace mozilla;

//----------------------------------------------------------------------
// nsSVGGenericContainerFrame Implementation

nsIFrame* NS_NewSVGGenericContainerFrame(PresShell* aPresShell,
                                         ComputedStyle* aStyle) {
  return new (aPresShell)
      nsSVGGenericContainerFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGGenericContainerFrame)

//----------------------------------------------------------------------
// nsIFrame methods

nsresult nsSVGGenericContainerFrame::AttributeChanged(int32_t aNameSpaceID,
                                                      nsAtom* aAttribute,
                                                      int32_t aModType) {
#ifdef DEBUG
  nsAutoString str;
  aAttribute->ToString(str);
  printf("** nsSVGGenericContainerFrame::AttributeChanged(%s)\n",
         NS_LossyConvertUTF16toASCII(str).get());
#endif

  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGContainerFrame methods:

gfxMatrix nsSVGGenericContainerFrame::GetCanvasTM() {
  NS_ASSERTION(GetParent(), "null parent");

  return static_cast<nsSVGContainerFrame*>(GetParent())->GetCanvasTM();
}
