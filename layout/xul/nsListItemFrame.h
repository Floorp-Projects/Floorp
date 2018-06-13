/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "nsGridRowLeafFrame.h"

nsIFrame* NS_NewListItemFrame(nsIPresShell* aPresShell,
                              mozilla::ComputedStyle* aStyle);

class nsListItemFrame final : public nsGridRowLeafFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS(nsListItemFrame)

  friend nsIFrame* NS_NewListItemFrame(nsIPresShell* aPresShell,
                                       ComputedStyle* aStyle);

  // overridden so that children of listitems don't handle mouse events,
  // unless allowevents="true" is specified on the listitem
  virtual void BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                           const nsDisplayListSet& aLists) override;

  virtual nsSize GetXULPrefSize(nsBoxLayoutState& aState) override;

protected:
  explicit nsListItemFrame(ComputedStyle* aStyle,
                           bool aIsRoot = false,
                           nsBoxLayout* aLayoutManager = nullptr);
  virtual ~nsListItemFrame();

}; // class nsListItemFrame
