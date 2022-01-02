/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: ruby-text" */

#ifndef nsRubyTextFrame_h___
#define nsRubyTextFrame_h___

#include "nsRubyContentFrame.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

/**
 * Factory function.
 * @return a newly allocated nsRubyTextFrame (infallible)
 */
nsContainerFrame* NS_NewRubyTextFrame(mozilla::PresShell* aPresShell,
                                      mozilla::ComputedStyle* aStyle);

class nsRubyTextFrame final : public nsRubyContentFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsRubyTextFrame)
  NS_DECL_QUERYFRAME

  // nsIFrame overrides
  virtual bool CanContinueTextRun() const override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsDisplayListSet& aLists) override;

  virtual void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;

  bool IsCollapsed() const {
    return HasAnyStateBits(NS_RUBY_TEXT_FRAME_COLLAPSED);
  }

 protected:
  friend nsContainerFrame* NS_NewRubyTextFrame(mozilla::PresShell* aPresShell,
                                               ComputedStyle* aStyle);
  explicit nsRubyTextFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : nsRubyContentFrame(aStyle, aPresContext, kClassID) {}
};

#endif /* nsRubyTextFrame_h___ */
