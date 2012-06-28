/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */

/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS display: -moz-flex */

#ifndef nsFlexContainerFrame_h___
#define nsFlexContainerFrame_h___

#include "nsContainerFrame.h"
#include "mozilla/Types.h"

nsIFrame* NS_NewFlexContainerFrame(nsIPresShell* aPresShell,
                                   nsStyleContext* aContext);

typedef nsContainerFrame nsFlexContainerFrameSuper;

class nsFlexContainerFrame : public nsFlexContainerFrameSuper {
  NS_DECL_FRAMEARENA_HELPERS
  NS_DECL_QUERYFRAME_TARGET(nsFlexContainerFrame)
  NS_DECL_QUERYFRAME

  // Factory method:
  friend nsIFrame* NS_NewFlexContainerFrame(nsIPresShell* aPresShell,
                                            nsStyleContext* aContext);

  // nsIFrame overrides
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif

protected:
  // Protected constructor & destructor
  nsFlexContainerFrame(nsStyleContext* aContext) : nsFlexContainerFrameSuper(aContext) {}
  virtual ~nsFlexContainerFrame();

  // Protected nsIFrame overrides:
  virtual void DestroyFrom(nsIFrame* aDestructRoot);

};

#endif /* nsFlexContainerFrame_h___ */
