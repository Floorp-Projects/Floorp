/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**

  Eric D Vaughan
  A frame that can have multiple children. Only one child may be displayed at
one time. So the can be flipped though like a deck of cards.

**/

#ifndef nsStackLayout_h___
#define nsStackLayout_h___

#include "mozilla/Attributes.h"
#include "nsBoxLayout.h"
#include "nsCOMPtr.h"
#include "nsCoord.h"

nsresult NS_NewStackLayout(nsCOMPtr<nsBoxLayout>& aNewLayout);

class nsStackLayout : public nsBoxLayout {
 public:
  friend nsresult NS_NewStackLayout(nsCOMPtr<nsBoxLayout>& aNewLayout);
  static void Shutdown();

  nsStackLayout();

  NS_IMETHOD XULLayout(nsIFrame* aBox, nsBoxLayoutState& aState) override;

  virtual nsSize GetXULPrefSize(nsIFrame* aBox,
                                nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetXULMinSize(nsIFrame* aBox,
                               nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetXULMaxSize(nsIFrame* aBox,
                               nsBoxLayoutState& aBoxLayoutState) override;
  virtual nscoord GetAscent(nsIFrame* aBox,
                            nsBoxLayoutState& aBoxLayoutState) override;

 private:
  static nsBoxLayout* gInstance;

};  // class nsStackLayout

#endif
