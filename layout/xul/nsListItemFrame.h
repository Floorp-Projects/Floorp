/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "nsGridRowLeafFrame.h"

nsIFrame* NS_NewListItemFrame(nsIPresShell* aPresShell,
                              nsStyleContext *aContext);

class nsListItemFrame final : public nsGridRowLeafFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS(nsListItemFrame)

  friend nsIFrame* NS_NewListItemFrame(nsIPresShell* aPresShell,
                                       nsStyleContext *aContext);

  // overridden so that children of listitems don't handle mouse events,
  // unless allowevents="true" is specified on the listitem
  virtual void BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                           const nsRect&           aDirtyRect,
                                           const nsDisplayListSet& aLists) override;

  virtual nsSize GetXULPrefSize(nsBoxLayoutState& aState) override;

protected:
  explicit nsListItemFrame(nsStyleContext *aContext,
                           bool aIsRoot = false,
                           nsBoxLayout* aLayoutManager = nullptr);
  virtual ~nsListItemFrame();

}; // class nsListItemFrame
