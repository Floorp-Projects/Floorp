/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: ruby-base-container" */

#ifndef nsRubyBaseContainerFrame_h___
#define nsRubyBaseContainerFrame_h___

#include "nsContainerFrame.h"

/**
 * Factory function.
 * @return a newly allocated nsRubyBaseContainerFrame (infallible)
 */
nsContainerFrame* NS_NewRubyBaseContainerFrame(nsIPresShell* aPresShell,
                                               nsStyleContext* aContext);

class nsRubyBaseContainerFrame MOZ_FINAL : public nsContainerFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS
  NS_DECL_QUERYFRAME_TARGET(nsRubyBaseContainerFrame)
  NS_DECL_QUERYFRAME

  // nsIFrame overrides
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif

protected:
  friend nsContainerFrame*
    NS_NewRubyBaseContainerFrame(nsIPresShell* aPresShell,
                                 nsStyleContext* aContext);
  nsRubyBaseContainerFrame(nsStyleContext* aContext) : nsContainerFrame(aContext) {}
};

#endif /* nsRubyBaseContainerFrame_h___ */
