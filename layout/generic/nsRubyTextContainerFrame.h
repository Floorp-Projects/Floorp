/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: ruby-text-container" */

#ifndef nsRubyTextContainerFrame_h___
#define nsRubyTextContainerFrame_h___

#include "nsBlockFrame.h"

/**
 * Factory function.
 * @return a newly allocated nsRubyTextContainerFrame (infallible)
 */
nsContainerFrame* NS_NewRubyTextContainerFrame(nsIPresShell* aPresShell,
                                               nsStyleContext* aContext);

class nsRubyTextContainerFrame final : public nsContainerFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS(nsRubyTextContainerFrame)
  NS_DECL_QUERYFRAME

  // nsIFrame overrides
  virtual bool IsFrameOfType(uint32_t aFlags) const override;
  virtual void Reflow(nsPresContext* aPresContext,
                      ReflowOutput& aDesiredSize,
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
  virtual void RemoveFrame(ChildListID aListID,
                           nsIFrame* aOldFrame) override;

  bool IsSpanContainer() const
  {
    return GetStateBits() & NS_RUBY_TEXT_CONTAINER_IS_SPAN;
  }

protected:
  friend nsContainerFrame*
    NS_NewRubyTextContainerFrame(nsIPresShell* aPresShell,
                                 nsStyleContext* aContext);

  explicit nsRubyTextContainerFrame(nsStyleContext* aContext)
    : nsContainerFrame(aContext, kClassID)
    , mISize(0)
  {}

  void UpdateSpanFlag();

  friend class nsRubyBaseContainerFrame;
  void SetISize(nscoord aISize) { mISize = aISize; }

  // The intended inline size of the ruby text container. It is set by
  // the corresponding ruby base container when the segment is reflowed,
  // and used when the ruby text container is reflowed by its parent.
  nscoord mISize;
};

#endif /* nsRubyTextContainerFrame_h___ */
