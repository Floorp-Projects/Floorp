/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */

/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: grid" */

#ifndef nsGridContainerFrame_h___
#define nsGridContainerFrame_h___

#include "nsContainerFrame.h"

nsIFrame* NS_NewGridContainerFrame(nsIPresShell* aPresShell,
                                   nsStyleContext* aContext);

typedef nsContainerFrame nsGridContainerFrameSuper;

class nsGridContainerFrame : public nsGridContainerFrameSuper {

  NS_DECL_FRAMEARENA_HELPERS
  NS_DECL_QUERYFRAME_TARGET(nsGridContainerFrame)
  NS_DECL_QUERYFRAME

  // Factory method:
  friend nsIFrame* NS_NewGridContainerFrame(nsIPresShell* aPresShell,
                                            nsStyleContext* aContext);

public:
  // nsIFrame overrides
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;
#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif

protected:
  // Protected constructor & destructor
  nsGridContainerFrame(nsStyleContext* aContext) : nsGridContainerFrameSuper(aContext) {}
  virtual ~nsGridContainerFrame();
};

#endif /* nsGridContainerFrame_h___ */
