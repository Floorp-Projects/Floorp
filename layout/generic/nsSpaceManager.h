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
#include "prclist.h"
#include "plhash.h"

/**
 * Implementation of nsISpaceManager that maintains a region data structure of
 * unavailable space
 */
class SpaceManager : public nsISpaceManager {
public:
  SpaceManager(nsIFrame* aFrame);
  ~SpaceManager();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsISpaceManager
  virtual nsIFrame* GetFrame() const;

  virtual void    Translate(nscoord aDx, nscoord aDy);
  virtual void    GetTranslation(nscoord& aX, nscoord& aY) const;
  virtual nscoord YMost() const;

  virtual PRInt32 GetBandData(nscoord       aYOffset,
                              const nsSize& aMaxSize,
                              nsBandData&   aBandData) const;

  virtual PRBool AddRectRegion(nsIFrame* aFrame, const nsRect& aUnavailableSpace);
  virtual PRBool ResizeRectRegion(nsIFrame*    aFrame,
                                  nscoord      aDeltaWidth,
                                  nscoord      aDeltaHeight,
                                  AffectedEdge aEdge);
  virtual PRBool OffsetRegion(nsIFrame* aFrame, nscoord dx, nscoord dy);
  virtual PRBool RemoveRegion(nsIFrame* aFrame);

  virtual void   ClearRegions();

protected:
  // Structure that maintains information about the region associated
  // with a particular frame
  struct FrameInfo {
    nsIFrame* const frame;
    nsRect          rect;       // rectangular region

    FrameInfo(nsIFrame* aFrame, const nsRect& aRect);
  };

  // Doubly linked list of band rects
  struct BandRect : PRCListStr {
    nscoord   left, top;
    nscoord   right, bottom;
    PRIntn    numFrames;    // number of frames occupying this rect
    union {
      nsIFrame*    frame;   // single frame occupying the space
      nsVoidArray* frames;  // list of frames occupying the space
    };

    BandRect(nscoord aLeft, nscoord aTop,
             nscoord aRight, nscoord aBottom,
             nsIFrame*);
    BandRect(nscoord aLeft, nscoord aTop,
             nscoord aRight, nscoord aBottom,
             nsVoidArray*);
    ~BandRect();

    // Split the band rect into two vertically, with this band rect becoming
    // the top part, and a new band rect being allocated and returned for the
    // bottom part
    BandRect* SplitVertically(nscoord aBottom);

    // Split the band rect into two horizontally, with this band rect becoming
    // the left part, and a new band rect being allocated and returned for the
    // right part
    BandRect* SplitHorizontally(nscoord aRight);

    // Accessor functions
    PRBool  IsOccupiedBy(nsIFrame*);
    void    AddFrame(nsIFrame*);
    void    RemoveFrame(nsIFrame*);
  };

  nsIFrame* const mFrame;     // frame associated with the space manager
  nscoord         mX, mY;     // translation from local to global coordinate space
  PRCListStr      mBandList;
  PLHashTable*    mFrameInfoMap;

protected:
  FrameInfo* GetFrameInfoFor(nsIFrame* aFrame);
  FrameInfo* CreateFrameInfo(nsIFrame* aFrame, const nsRect& aRect);
  void       DestroyFrameInfo(FrameInfo*);

  void       ClearFrameInfo();
  void       ClearBandRects();

  BandRect*  GetNextBand(const BandRect* aBandRect) const;
  void       DivideBand(BandRect* aBand, nscoord aBottom);
  void       AddRectToBand(BandRect* aBand, BandRect* aBandRect);
  void       InsertBandRect(BandRect* aBandRect);

  PRInt32    GetBandAvailableSpace(const BandRect* aBand,
                                   nscoord         aY,
                                   const nsSize&   aMaxSize,
                                   nsBandData&     aAvailableSpace) const;

private:
	SpaceManager(const SpaceManager&);    // no implementation
	void operator=(const SpaceManager&);  // no implementation
  friend PR_CALLBACK PRIntn NS_RemoveFrameInfoEntries(PLHashEntry*, PRIntn, void*);
};

#endif /* nsSpaceManager_h___ */

