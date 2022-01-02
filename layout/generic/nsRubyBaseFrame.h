/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: ruby-base" */

#ifndef nsRubyBaseFrame_h___
#define nsRubyBaseFrame_h___

#include "nsRubyContentFrame.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

/**
 * Factory function.
 * @return a newly allocated nsRubyBaseFrame (infallible)
 */
nsContainerFrame* NS_NewRubyBaseFrame(mozilla::PresShell* aPresShell,
                                      mozilla::ComputedStyle* aStyle);

class nsRubyBaseFrame final : public nsRubyContentFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsRubyBaseFrame)
  NS_DECL_QUERYFRAME

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

 protected:
  friend nsContainerFrame* NS_NewRubyBaseFrame(mozilla::PresShell* aPresShell,
                                               ComputedStyle* aStyle);
  explicit nsRubyBaseFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : nsRubyContentFrame(aStyle, aPresContext, kClassID) {}
};

#endif /* nsRubyBaseFrame_h___ */
