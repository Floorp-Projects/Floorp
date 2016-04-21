/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsListBoxLayout_h___
#define nsListBoxLayout_h___

#include "mozilla/Attributes.h"
#include "nsGridRowGroupLayout.h"

class nsIFrame;
typedef class nsIFrame nsIFrame;
class nsBoxLayoutState;

class nsListBoxLayout : public nsGridRowGroupLayout
{
public:
  nsListBoxLayout();

  // nsBoxLayout
  NS_IMETHOD XULLayout(nsIFrame* aBox, nsBoxLayoutState& aState) override;
  virtual nsSize GetXULPrefSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetXULMinSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetXULMaxSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState) override;

protected:
  NS_IMETHOD LayoutInternal(nsIFrame* aBox, nsBoxLayoutState& aState);
};

#endif

