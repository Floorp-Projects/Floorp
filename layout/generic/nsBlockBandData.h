/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsBlockBandData_h___
#define nsBlockBandData_h___

#include "nsSpaceManager.h"

class nsIPresContext;

// Number of builtin nsBandTrapezoid's
#define NS_BLOCK_BAND_DATA_TRAPS 6

/**
 * Class used to manage processing of the space-manager band data.
 * Provides HTML/CSS specific behavior to the raw data.
 */
class nsBlockBandData : public nsBandData {
public:
  nsBlockBandData();
  ~nsBlockBandData();

  // Initialize. This must be called before any of the other methods.
  nsresult Init(nsSpaceManager* aSpaceManager, const nsSize& aSpace);

  // Get some available space. Note that aY is relative to the current
  // space manager translation.
  nsresult GetAvailableSpace(nscoord aY, nsRect& aResult);

  // Clear any current floaters, returning a new Y coordinate
  nscoord ClearFloaters(nscoord aY, PRUint8 aBreakType);

  // Get the raw trapezoid count for this band.
  PRInt32 GetTrapezoidCount() const {
    return mCount;
  }

  const nsBandTrapezoid* GetTrapezoid(PRInt32 aIndex) const {
    return &mTrapezoids[aIndex];
  }

  // Get the number of floaters that are impacting the current
  // band. Note that this value is relative to the current translation
  // in the space manager which means that some floaters may be hidden
  // by the translation and therefore won't be in the count.
  PRInt32 GetFloaterCount() const {
    return mLeftFloaters + mRightFloaters;
  }
  PRInt32 GetLeftFloaterCount() const {
    return mLeftFloaters;
  }
  PRInt32 GetRightFloaterCount() const {
    return mRightFloaters;
  }

  // Return the impact on the max-element-size for this band by
  // computing the maximum width and maximum height of all the
  // floaters.
  void GetMaxElementSize(nsIPresContext* aPresContext,
                         nscoord* aWidthResult, nscoord* aHeightResult) const;

  // Utility method to save away the max-element-size associated with
  // a floating frame.
  static void StoreMaxElementSize(nsIPresContext* aPresContext,
                                  nsIFrame* aFrame,
                                  const nsSize& aMaxElementSize);

  // Utility method to recover a stored max-element-size value
  // associated with a floating frame.
  static void RecoverMaxElementSize(nsIPresContext* aPresContext,
                                    nsIFrame* aFrame,
                                    nsSize* aResult);

#ifdef DEBUG
  void List();
#endif

protected:

  /** utility method to calculate the band data at aY.
    * nsBlockBandData methods should never call 
    * mSpaceManager->GetBandData directly.
    * They should always call this method instead so data members
    * mTrapezoid, mCount, and mSize all get managed properly.
    */
  nsresult GetBandData(nscoord aY);

  // The spacemanager we are getting space from
  nsSpaceManager* mSpaceManager;
  nscoord mSpaceManagerX, mSpaceManagerY;

  // Limit to the available space (set by Init)
  nsSize mSpace;

  // Trapezoids used during band processing
  nsBandTrapezoid mData[NS_BLOCK_BAND_DATA_TRAPS];

  // Bounding rect of available space between any left and right floaters
  nsRect mAvailSpace;

  // Number of left/right floaters in the current band. Note that this
  // number may be less than the total number of floaters present in
  // the band, if our translation in the space manager "hides" some
  // floaters.
  PRInt32 mLeftFloaters, mRightFloaters;

  void ComputeAvailSpaceRect();
  PRBool ShouldClearFrame(nsIFrame* aFrame, PRUint8 aBreakType);

};

#endif /* nsBlockBandData_h___ */
