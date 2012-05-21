/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIPercentHeightObserver_h___
#define nsIPercentHeightObserver_h___

#include "nsQueryFrame.h"

struct nsHTMLReflowState;

/**
 * This interface is supported by frames that need to provide computed height
 * values to children during reflow which would otherwise not happen. Currently only
 * table cells support this.
 */
class nsIPercentHeightObserver
{
public:
  NS_DECL_QUERYFRAME_TARGET(nsIPercentHeightObserver)

  // Notify the observer that aReflowState has no computed height, but it has a percent height
  virtual void NotifyPercentHeight(const nsHTMLReflowState& aReflowState) = 0;

  // Ask the observer if it should observe aReflowState.frame
  virtual bool NeedsToObserve(const nsHTMLReflowState& aReflowState) = 0;
};

#endif // nsIPercentHeightObserver_h___ 
