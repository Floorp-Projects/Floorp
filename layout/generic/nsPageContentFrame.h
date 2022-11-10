/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsPageContentFrame_h___
#define nsPageContentFrame_h___

#include "mozilla/Attributes.h"
#include "mozilla/ViewportFrame.h"

class nsPageFrame;
class nsSharedPageData;

namespace mozilla {
class PresShell;
}  // namespace mozilla

// Page content frame class. Represents a page's content, in paginated mode.
class nsPageContentFrame final : public mozilla::ViewportFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsPageContentFrame)

  friend nsPageContentFrame* NS_NewPageContentFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle);
  friend class nsPageFrame;

  // nsIFrame
  void Reflow(nsPresContext* aPresContext, ReflowOutput& aReflowOutput,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;

  bool IsFrameOfType(uint32_t aFlags) const override {
    return ViewportFrame::IsFrameOfType(
        aFlags & ~(nsIFrame::eCanContainOverflowContainers));
  }

  void SetSharedPageData(nsSharedPageData* aPD) { mPD = aPD; }

  ComputeTransformFunction GetTransformGetter() const override;
  void BuildDisplayList(nsDisplayListBuilder*,
                        const nsDisplayListSet&) override;

  /**
   * Return our canvas frame.
   */
  void AppendDirectlyOwnedAnonBoxes(nsTArray<OwnedAnonBox>& aResult) override;

#ifdef DEBUG_FRAME_DUMP
  // Debugging
  nsresult GetFrameName(nsAString& aResult) const override;
#endif

 protected:
  explicit nsPageContentFrame(ComputedStyle* aStyle,
                              nsPresContext* aPresContext)
      : ViewportFrame(aStyle, aPresContext, kClassID) {}

  // Note: this will be set before reflow, and it's strongly owned by our
  // nsPageSequenceFrame, which outlives us.
  nsSharedPageData* mPD = nullptr;

  // The combined InkOverflow from the previous and current page that does not
  // yet have space allocated for it.
  nscoord mRemainingOverflow = 0;
};

#endif /* nsPageContentFrame_h___ */
