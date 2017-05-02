/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* derived class of nsBlockFrame used for xul:label elements */

#ifndef nsXULLabelFrame_h_
#define nsXULLabelFrame_h_

#include "mozilla/Attributes.h"
#include "nsBlockFrame.h"

#ifndef MOZ_XUL
#error "This file should not be included"
#endif

class nsXULLabelFrame final : public nsBlockFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewXULLabelFrame(nsIPresShell* aPresShell,
                                       nsStyleContext *aContext);
  
  // nsIFrame
  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;

  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

  virtual nsresult AttributeChanged(int32_t aNameSpaceID,
                                    nsIAtom* aAttribute,
                                    int32_t aModType) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

protected:
  explicit nsXULLabelFrame(nsStyleContext* aContext)
    : nsBlockFrame(aContext, mozilla::FrameType::XULLabel)
  {}

  nsresult RegUnregAccessKey(bool aDoReg);
};

nsIFrame*
NS_NewXULLabelFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

#endif /* !defined(nsXULLabelFrame_h_) */
