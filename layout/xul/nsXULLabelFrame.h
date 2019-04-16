/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* derived class of nsBlockFrame used for xul:label elements */

#ifndef nsXULLabelFrame_h_
#define nsXULLabelFrame_h_

#include "mozilla/Attributes.h"
#include "nsBlockFrame.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

class nsXULLabelFrame final : public nsBlockFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsXULLabelFrame)

  friend nsIFrame* NS_NewXULLabelFrame(mozilla::PresShell* aPresShell,
                                       ComputedStyle* aStyle);

  // nsIFrame
  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* aPrevInFlow) override;

  virtual void DestroyFrom(nsIFrame* aDestructRoot,
                           PostDestroyData& aPostDestroyData) override;

  virtual nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                    int32_t aModType) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

 protected:
  explicit nsXULLabelFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : nsBlockFrame(aStyle, aPresContext, kClassID) {}

  nsresult RegUnregAccessKey(bool aDoReg);
};

nsIFrame* NS_NewXULLabelFrame(mozilla::PresShell* aPresShell,
                              mozilla::ComputedStyle* aStyle);

#endif /* !defined(nsXULLabelFrame_h_) */
