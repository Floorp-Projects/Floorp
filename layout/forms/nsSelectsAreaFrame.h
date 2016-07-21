/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsSelectsAreaFrame_h___
#define nsSelectsAreaFrame_h___

#include "mozilla/Attributes.h"
#include "nsBlockFrame.h"

class nsSelectsAreaFrame : public nsBlockFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsContainerFrame* NS_NewSelectsAreaFrame(nsIPresShell* aShell,
                                                  nsStyleContext* aContext,
                                                  nsFrameState aFlags);

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  void BuildDisplayListInternal(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists);

  virtual void Reflow(nsPresContext*           aCX,
                      ReflowOutput&     aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus&          aStatus) override;

  nscoord BSizeOfARow() const { return mBSizeOfARow; }
  
protected:
  explicit nsSelectsAreaFrame(nsStyleContext* aContext) :
    nsBlockFrame(aContext),
    mBSizeOfARow(0)
  {}

  // We cache the block size of a single row so that changes to the
  // "size" attribute, padding, etc. can all be handled with only one
  // reflow.  We'll have to reflow twice if someone changes our font
  // size or something like that, so that the block size of our options
  // will change.
  nscoord mBSizeOfARow;
};

#endif /* nsSelectsAreaFrame_h___ */
