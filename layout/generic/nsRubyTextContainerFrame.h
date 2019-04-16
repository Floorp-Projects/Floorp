/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: ruby-text-container" */

#ifndef nsRubyTextContainerFrame_h___
#define nsRubyTextContainerFrame_h___

#include "nsBlockFrame.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

/**
 * Factory function.
 * @return a newly allocated nsRubyTextContainerFrame (infallible)
 */
nsContainerFrame* NS_NewRubyTextContainerFrame(mozilla::PresShell* aPresShell,
                                               mozilla::ComputedStyle* aStyle);

class nsRubyTextContainerFrame final : public nsContainerFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsRubyTextContainerFrame)
  NS_DECL_QUERYFRAME

  // nsIFrame overrides
  virtual bool IsFrameOfType(uint32_t aFlags) const override;
  virtual void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  // nsContainerFrame overrides
  virtual void SetInitialChildList(ChildListID aListID,
                                   nsFrameList& aChildList) override;
  virtual void AppendFrames(ChildListID aListID,
                            nsFrameList& aFrameList) override;
  virtual void InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                            nsFrameList& aFrameList) override;
  virtual void RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame) override;

  bool IsSpanContainer() const {
    return GetStateBits() & NS_RUBY_TEXT_CONTAINER_IS_SPAN;
  }

 protected:
  friend nsContainerFrame* NS_NewRubyTextContainerFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle);

  explicit nsRubyTextContainerFrame(ComputedStyle* aStyle,
                                    nsPresContext* aPresContext)
      : nsContainerFrame(aStyle, aPresContext, kClassID), mISize(0) {}

  void UpdateSpanFlag();

  friend class nsRubyBaseContainerFrame;
  void SetISize(nscoord aISize) { mISize = aISize; }

  // The intended inline size of the ruby text container. It is set by
  // the corresponding ruby base container when the segment is reflowed,
  // and used when the ruby text container is reflowed by its parent.
  nscoord mISize;
};

#endif /* nsRubyTextContainerFrame_h___ */
