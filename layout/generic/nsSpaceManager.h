/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsSpaceManager_h___
#define nsSpaceManager_h___

#include "nsISpaceManager.h"

/**
 * Implementation of nsISpaceManager that maintains a region data structure of
 * unavailable space
 */
class SpaceManager : public nsISpaceManager {
public:
  SpaceManager(nsIFrame* aFrame);

  // nsISupports
  NS_DECL_ISUPPORTS

  virtual nsIFrame* GetFrame() const;

  virtual void    Translate(nscoord aDx, nscoord aDy);
  virtual void    GetTranslation(nscoord& aX, nscoord& aY) const;
  virtual nscoord YMost() const;

  virtual PRInt32 GetBandData(nscoord       aYOffset,
                              const nsSize& aMaxSize,
                              nsBandData&   aBandData) const;

  virtual PRBool AddRectRegion(nsIFrame* aFrame, const nsRect& aUnavailableSpace);
  virtual PRBool ReshapeRectRegion(nsIFrame* aFrame, const nsRect& aUnavailableSpace);
  virtual PRBool OffsetRegion(nsIFrame* aFrame, nscoord dx, nscoord dy);
  virtual PRBool RemoveRegion(nsIFrame* aFrame);

  virtual void   ClearRegions();

protected:
  struct nsBandRect : nsRect {
    nsIFrame* frame;
  };

  class RectArray {
  public:
    RectArray();
    ~RectArray();

    // Functions to add rects
    void Append(const nsRect& aRect, nsIFrame* aFrame);
    void InsertAt(const nsRect& aRect, PRInt32 aIndex, nsIFrame* aFrame);
    void RemoveAt(PRInt32 aIndex);

    // Clear the list of rectangles
    void Clear();

    // Returns the y-most of the bottom band
    nscoord YMost() const;

  public:
    // Access to the underlying storage
    nsBandRect* mRects;  // y-x banded array of rectangles of unavailable space
    PRInt32     mCount;  // current number of rects
    PRInt32     mMax;    // capacity of rect array
  };

  nsIFrame* const mFrame;      // frame associated with the space manager
  nscoord         mX, mY;      // translation from local to global coordinate space
  RectArray       mRectArray;  // y-x banded array of rectangles of unavailable space
  RectArray       mEmptyRects; // list of empty height rects

protected:
  PRBool  GetNextBand(nsBandRect*& aRect, PRInt32& aIndex) const;
  PRInt32 LengthOfBand(const nsBandRect* aBand, PRInt32 aIndex) const;
  PRBool  CoalesceBand(nsBandRect* aBand, PRInt32 aIndex);
  void    DivideBand(nsBandRect* aBand, PRInt32 aIndex, nscoord aB1Height);
  void    AddRectToBand(nsBandRect* aBand, PRInt32 aIndex,
                        const nsRect& aRect, nsIFrame* aFrame);
  PRInt32 GetBandAvailableSpace(const nsBandRect* aBand,
                                PRInt32           aIndex,
                                nscoord           aY,
                                const nsSize&     aMaxSize,
                                nsBandData&       aAvailableSpace) const;

private:
	SpaceManager(const SpaceManager&);    // no implementation
	void operator=(const SpaceManager&);  // no implementation
};

#endif /* nsSpaceManager_h___ */

