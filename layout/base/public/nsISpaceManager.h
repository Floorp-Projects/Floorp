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
#include "nsRect.h"

class nsIFrame;
class nsVoidArray;
struct nsSize;

// IID for the nsISpaceManager interface {17C8FB50-BE96-11d1-80B5-00805F8A274D}
#define NS_ISPACEMANAGER_IID         \
{ 0x17c8fb50, 0xbe96, 0x11d1, \
  {0x80, 0xb5, 0x0, 0x80, 0x5f, 0x8a, 0x27, 0x4d}}

/**
 * Information about a particular trapezoid within a band. The space described
 * by the trapezoid is in one of three states:
 * <ul>
 * <li>available
 * <li>occupied by one frame
 * <li>occupied by more than one frame
 * </ul>
 */
struct nsBandTrapezoid {
  enum State {Available, Occupied, OccupiedMultiple};

  nscoord   yTop, yBottom;            // top and bottom y-coordinates
  nscoord   xTopLeft, xBottomLeft;    // left edge x-coordinates
  nscoord   xTopRight, xBottomRight;  // right edge x-coordinates
  State     state;                    // state of the space
  union {
    nsIFrame*          frame;  // single frame occupying the space
    const nsVoidArray* frames; // list of frames occupying the space
  };

  // Get the height of the trapezoid
  nscoord GetHeight() const {return yBottom - yTop;}

  // Get the bouding rect of the trapezoid
  void    GetRect(nsRect& aRect) const;

  // Set the trapezoid from a rectangle
  void operator=(const nsRect& aRect);
};

/**
 * Structure used for describing the space within a band.
 * @see #GetBandData()
 */
struct nsBandData {
  PRInt32          count;      // [out] actual number of trapezoids in the band data
  PRInt32          size;       // [in] the size of the array (number of trapezoids)
  nsBandTrapezoid* trapezoids; // [out] array of length 'size'
};

/**
 * Interface for dealing with bands of available space. The space manager defines a coordinate
 * space with an origin at (0, 0) that grows down and to the right.
 *
 * @see nsIRunaround
 */
class nsISpaceManager : public nsISupports {
public:
  /**
   * Get the frame that's associated with the space manager. This frame created
   * the space manager, and the world coordinate space is relative to this frame.
   *
   * You can use QueryInterface() on this frame to get any additional interfaces
   */
  NS_IMETHOD GetFrame(nsIFrame*& aFrame) const = 0;

  /**
   * Translate the current origin by the specified (dx, dy). This creates a new
   * local coordinate space relative to the current coordinate space.
   */
  NS_IMETHOD Translate(nscoord aDx, nscoord aDy) = 0;

  /**
   * Returns the current translation from local coordinate space to world
   * coordinate space. This represents the accumulated calls to Translate().
   */
  NS_IMETHOD GetTranslation(nscoord& aX, nscoord& aY) const = 0;

  /**
   * Returns the y-most of the bottommost band, or 0 if there are no bands.
   */
  NS_IMETHOD YMost(nscoord& aYMost) const = 0;

  /**
   * Returns a band starting at the specified y-offset. The band data indicates
   * which parts of the band are available, and which parts are unavailable
   *
   * The band data that is returned is in the coordinate space of the local
   * coordinate system.
   *
   * The local coordinate space origin, the y-offset, and the max size describe
   * a rectangle that's used to clip the underlying band of available space, i.e.
   * {0, aYOffset, aMaxSize.width, aMaxSize.height} in the local coordinate space
   *
   * @param   aYOffset the y-offset of where the band begins. The coordinate is
   *            relative to the upper-left corner of the local coordinate space
   * @param   aMaxSize the size to use to constrain the band data
   * @param   aBandData [in,out] used to return the list of trapezoids that
   *            describe the available space and the unavailable space
   * @return  NS_OK if successful and NS_ERROR_FAILURE if the band data is not
   *            not large enough. The 'count' member of the band data struct
   *            indicates how large the array of trapezoids needs to be
   */
  NS_IMETHOD GetBandData(nscoord       aYOffset,
                         const nsSize& aMaxSize,
                         nsBandData&   aBandData) const = 0;

  /**
   * Add a rectangular region of unavailable space. The space is relative to
   * the local coordinate system.
   *
   * The region is tagged with a frame. When translated to world coordinates
   * the origin of the rect MUST be within the defined coordinate space, i.e.
   * the x-offset and y-offset must be >= 0
   *
   * @param   aFrame the frame used to identify the region. Must not be NULL
   * @param   aUnavailableSpace the bounding rect of the unavailable space
   * @return  NS_OK if successful
   *          NS_ERROR_FAILURE if there is already a region tagged with aFrame
   *          NS_ERROR_INVALID_ARG if the rect translated to world coordinates
   *            is not within the defined coordinate space
   */
  NS_IMETHOD AddRectRegion(nsIFrame*     aFrame,
                           const nsRect& aUnavailableSpace) = 0;

  /**
   * Resize the rectangular region associated with aFrame by the specified
   * deltas. The height change always applies to the bottom edge or the existing
   * rect. You specify whether the width change applies to the left or right edge
   *
   * Returns NS_OK if successful, NS_ERROR_INVALID_ARG if there is no region
   * tagged with aFrame, and NS_ERROR_FAILURE if the new offset when translated
   * to world coordinates is outside the defined coordinate space
   */
  enum AffectedEdge {LeftEdge, RightEdge};
  NS_IMETHOD ResizeRectRegion(nsIFrame*    aFrame,
                              nscoord      aDeltaWidth,
                              nscoord      aDeltaHeight,
                              AffectedEdge aEdge = RightEdge) = 0;

  /**
   * Offset the region associated with aFrame by the specified amount.
   *
   * Returns NS_OK if successful, NS_ERROR_INVALID_ARG if there is no region
   * tagged with aFrame, and NS_ERROR_FAILURE if the new offset when translated
   * to world coordinates is outside the defined coordinate space
   */
  NS_IMETHOD OffsetRegion(nsIFrame* aFrame, nscoord dx, nscoord dy) = 0;

  /**
   * Remove the region associated with aFrane.
   *
   * Returns NS_OK if successful and NS_ERROR_INVALID_ARG if there is no region
   * tagged with aFrame
   */
  NS_IMETHOD RemoveRegion(nsIFrame* aFrame) = 0;

  /**
   * Clears the list of regions representing the unavailable space.
   */
  NS_IMETHOD ClearRegions() = 0;
};

inline void nsBandTrapezoid::GetRect(nsRect& aRect) const
{
  aRect.x = PR_MIN(xTopLeft, xBottomLeft);
  aRect.y = yTop;
  aRect.width = PR_MAX(xTopRight, xBottomRight) - aRect.x;
  aRect.height = yBottom - yTop;
}

inline void nsBandTrapezoid::operator=(const nsRect& aRect)
{
  xTopLeft = xBottomLeft = aRect.x;
  xTopRight = xBottomRight = aRect.XMost();
  yTop = aRect.y;
  yBottom = aRect.YMost();
}

#endif /* nsISpaceManager_h___ */
