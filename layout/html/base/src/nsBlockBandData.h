/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#ifndef nsBlockBandData_h___
#define nsBlockBandData_h___

#include "nsISpaceManager.h"

/**
 * Class used to manage processing of the space-manager band data.
 * Provides HTML/CSS specific behavior to the raw data.
 */
class nsBlockBandData : public nsBandData {
public:
  nsBlockBandData();
  ~nsBlockBandData();

  // Initialize. This must be called before any of the other methods.
  nsresult Init(nsISpaceManager* aSpaceManager, const nsSize& aSpace);

  // Get some available space. Note that aY is relative to the current
  // space manager translation.
  void GetAvailableSpace(nscoord aY, nsRect& aResult);

  // Clear any current floaters, returning a new Y coordinate
  nscoord ClearFloaters(nscoord aY, PRUint8 aBreakType);

  // Get the raw trapezoid count for this band.
  PRInt32 GetTrapezoidCount() const {
    return count;
  }

  const nsBandTrapezoid* GetTrapezoid(PRInt32 aIndex) const {
    return &mData[aIndex];
  }

  // Get the number of floaters that are impacting the current
  // band. Note that this value is relative to the current translation
  // in the space manager which means that some floaters may be hidden
  // by the translation and therefore won't be in the count.
  PRInt32 GetFloaterCount() const {
    return mFloaters;
  }

  // Return the impact on the max-element-size for this band by
  // computing the maximum width and maximum height of all the
  // floaters.
  void GetMaxElementSize(nscoord* aWidthResult, nscoord* aHeightResult) const;

protected:
  // The spacemanager we are getting space from
  nsISpaceManager* mSpaceManager;
  nscoord mSpaceManagerX, mSpaceManagerY;

  // Limit to the available space (set by Init)
  nsSize mSpace;

  // Trapezoids used during band processing
  // nsBlockBandData what happens if we need more than 12 trapezoids?
  nsBandTrapezoid mData[12];

  // Bounding rect of available space between any left and right floaters
  nsRect mAvailSpace;

  // Number of floaters in the current band. Note that this number may
  // be less than the total number of floaters present in the band, if
  // our translation in the space manager "hides" some floaters.
  PRInt32 mFloaters;

  void ComputeAvailSpaceRect();
  PRBool ShouldClearFrame(nsIFrame* aFrame, PRUint8 aBreakType);
#if 0
  nscoord GetFrameYMost(nsIFrame* aFrame);
#endif
};

#endif /* nsBlockBandData_h___ */
