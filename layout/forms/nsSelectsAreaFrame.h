/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsSelectsAreaFrame_h___
#define nsSelectsAreaFrame_h___

#include "mozilla/Attributes.h"
#include "nsBlockFrame.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

class nsSelectsAreaFrame final : public nsBlockFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsSelectsAreaFrame)

  friend nsContainerFrame* NS_NewSelectsAreaFrame(mozilla::PresShell* aShell,
                                                  ComputedStyle* aStyle,
                                                  nsFrameState aFlags);

  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsDisplayListSet& aLists) override;

  void BuildDisplayListInternal(nsDisplayListBuilder* aBuilder,
                                const nsDisplayListSet& aLists);

  virtual void Reflow(nsPresContext* aCX, ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;

  nscoord BSizeOfARow() const { return mBSizeOfARow; }

 protected:
  explicit nsSelectsAreaFrame(ComputedStyle* aStyle,
                              nsPresContext* aPresContext)
      : nsBlockFrame(aStyle, aPresContext, kClassID),
        // initialize to wacky value so first call of
        // nsSelectsAreaFrame::Reflow will always invalidate
        mBSizeOfARow(nscoord_MIN) {}

  // We cache the block size of a single row so that changes to the
  // "size" attribute, padding, etc. can all be handled with only one
  // reflow.  We'll have to reflow twice if someone changes our font
  // size or something like that, so that the block size of our options
  // will change.
  nscoord mBSizeOfARow;
};

#endif /* nsSelectsAreaFrame_h___ */
