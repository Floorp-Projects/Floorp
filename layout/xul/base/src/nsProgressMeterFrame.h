/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**

  David Hyatt & Eric D Vaughan.

  An XBL-based progress meter. 

  Attributes:

  value: A number between 0% and 100%
  align: horizontal or vertical
  mode: determined, undetermined (one shows progress other shows animated candy cane)

**/

#include "nsBoxFrame.h"

class nsProgressMeterFrame : public nsBoxFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewProgressMeterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState);

  NS_IMETHOD AttributeChanged(PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

protected:
  nsProgressMeterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext) :
    nsBoxFrame(aPresShell, aContext), mNeedsReflowCallback(true) {}
  virtual ~nsProgressMeterFrame();

  bool mNeedsReflowCallback;
}; // class nsProgressMeterFrame
