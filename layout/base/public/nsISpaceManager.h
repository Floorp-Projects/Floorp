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
#ifndef nsISpaceManager_h___
#define nsISpaceManager_h___

#include "nsISupports.h"
#include "nsCoord.h"

class nsIFrame;
struct nsPoint;
struct nsRect;
struct nsSize;

// IID for the nsISpaceManager interface {17C8FB50-BE96-11d1-80B5-00805F8A274D}
#define NS_ISPACEMANAGER_IID         \
{ 0x17c8fb50, 0xbe96, 0x11d1, \
  {0x80, 0xb5, 0x0, 0x80, 0x5f, 0x8a, 0x27, 0x4d}}

/**
 * Structure used for returning the available space in a band.
 * @see #GetBandData()
 */
struct nsBandData {
  PRInt32 count;  // 'out' parameter. Actual number of rects in the band data
  PRInt32 size;   // 'in' parameter. The size of the array
  nsRect* rects;  // 'out' parameter. Array of length 'size'
};

/**
 * Interface for dealing with bands of available space.
 *
 * @see nsIRunaround
 */
class nsISpaceManager : public nsISupports {
public:
  /**
   * Get the frame that's associated with the space manager. This frame created
   * the space manager, and the world coordinate space is relative to this frame.
   *
   * You can use QueryInterface() on this frame to get additional interfaces such
   * as nsIAnchoredItems
   */
  virtual nsIFrame* GetFrame() const = 0;

  /**
   * Translate the current origin by the specified (dx, dy). This creates a new
   * local coordinate space relative to the current coordinate space.
   */
  virtual void Translate(nscoord aDx, nscoord aDy) = 0;

  /**
   * Returns the current translation from local coordinate space to world
   * coordinate space. This represents the accumulated calls to Translate().
   */
  virtual void GetTranslation(nscoord& aX, nscoord& aY) const = 0;

  /**
   * Returns the y-most of the bottommost band, or 0 if there are no bands.
   */
  virtual nscoord YMost() const = 0;

  /**
   * Returns a band starting at the specified y-offset. The band data indicates
   * which parts of the band are available
   *
   * The band data that is returned is in the coordinate space of the local
   * coordinate system.
   *
   * The local coordinate space origin together with the max size describe a
   * rectangle that's used to clip the underlying band of available space.
   *
   * @param   aYOffset the y-offset of where the band begins. The coordinate is
   *            relative to the upper-left corner of the local coordinate space
   * @param   aMaxSize the size to use to constrain the band data
   * @param   aAvailableSpace <i>out</i> parameter used to return the list of
   *            rects that make up the available space.
   * @returns the number of rects in the band data. If the band data is not
   *            large enough returns the negative of the number of rects needed
   */
  virtual PRInt32 GetBandData(nscoord       aYOffset,
                              const nsSize& aMaxSize,
                              nsBandData&   aAvailableSpace) const = 0;

  /**
   * Add a rectangular region of unavailable space. The space is relative to
   * the local coordinate system.
   */
  virtual void AddRectRegion(const nsRect& aUnavailableSpace) = 0;

  /**
   * Clears the list of regions representing the unavailable space.
   */
  virtual void ClearRegions() = 0;
};

#endif /* nsISpaceManager_h___ */
