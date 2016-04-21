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

#include "mozilla/Attributes.h"
#include "nsBoxFrame.h"

class nsProgressMeterFrame : public nsBoxFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewProgressMeterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_IMETHOD DoXULLayout(nsBoxLayoutState& aBoxLayoutState) override;

  virtual nsresult AttributeChanged(int32_t aNameSpaceID,
                                    nsIAtom* aAttribute,
                                    int32_t aModType) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

protected:
  explicit nsProgressMeterFrame(nsStyleContext* aContext) :
    nsBoxFrame(aContext), mNeedsReflowCallback(true) {}
  virtual ~nsProgressMeterFrame();

  bool mNeedsReflowCallback;
}; // class nsProgressMeterFrame
