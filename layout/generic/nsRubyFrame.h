/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: ruby" */

#ifndef nsRubyFrame_h___
#define nsRubyFrame_h___

#include "nsContainerFrame.h"

/**
 * Factory function.
 * @return a newly allocated nsRubyFrame (infallible)
 */
nsContainerFrame* NS_NewRubyFrame(nsIPresShell* aPresShell,
                                  nsStyleContext* aContext);

class nsRubyFrame MOZ_FINAL : public nsContainerFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS
  NS_DECL_QUERYFRAME_TARGET(nsRubyFrame)
  NS_DECL_QUERYFRAME

  // nsIFrame overrides
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;
  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif

protected:
  friend nsContainerFrame* NS_NewRubyFrame(nsIPresShell* aPresShell,
                                           nsStyleContext* aContext);
  nsRubyFrame(nsStyleContext* aContext) : nsContainerFrame(aContext) {}
};

#endif /* nsRubyFrame_h___ */
