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
 * The Original Code is mozilla.org code.
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
#ifndef nsISpaceManager_h___
#define nsISpaceManager_h___

#include "nsISupports.h"
#include "nsCoord.h"
#include "nsRect.h"

class nsIFrame;
class nsVoidArray;
class nsISizeOfHandler;
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

  nscoord   mTopY, mBottomY;            // top and bottom y-coordinates
  nscoord   mTopLeftX, mBottomLeftX;    // left edge x-coordinates
  nscoord   mTopRightX, mBottomRightX;  // right edge x-coordinates
  State     mState;                     // state of the space
  union {
    nsIFrame*          mFrame;  // single frame occupying the space
    const nsVoidArray* mFrames; // list of frames occupying the space
  };

  // Get the height of the trapezoid
  nscoord GetHeight() const {return mBottomY - mTopY;}

  // Get the bouding rect of the trapezoid
  void    GetRect(nsRect& aRect) const;

  // Set the trapezoid from a rectangle
  void operator=(const nsRect& aRect);

  /** does a binary compare of this object with aTrap */
  PRBool Equals(const nsBandTrapezoid aTrap) const;

  /** does a semantic compare only of geometric data in this object and aTrap */
  PRBool EqualGeometry(const nsBandTrapezoid aTrap) const;

  nsBandTrapezoid() {
    mTopY = mBottomY = mTopLeftX = mBottomLeftX = mTopRightX = mBottomRightX = 0;
    mFrame = nsnull;
  }
};

/**
 * Structure used for describing the space within a band.
 * @see #GetBandData()
 */
struct nsBandData {
  PRInt32          mCount;      // [out] actual number of trapezoids in the band data
  PRInt32          mSize;       // [in] the size of the array (number of trapezoids)
  nsBandTrapezoid* mTrapezoids; // [out] array of length 'size'
};

/**
 * Interface for dealing with bands of available space. The space manager defines a coordinate
 * space with an origin at (0, 0) that grows down and to the right.
 *
 * @see nsIRunaround
 */
class nsISpaceManager : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ISPACEMANAGER_IID; return iid; }

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
   * Returns the y-most of the bottommost band or 0 if there are no bands.
   *
   * @return  NS_OK if there are bands and NS_ERROR_ABORT if there are
   *          no bands
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
   * The region is tagged with a frame
   *
   * @param   aFrame the frame used to identify the region. Must not be NULL
   * @param   aUnavailableSpace the bounding rect of the unavailable space
   * @return  NS_OK if successful
   *          NS_ERROR_FAILURE if there is already a region tagged with aFrame
   */
  NS_IMETHOD AddRectRegion(nsIFrame*     aFrame,
                           const nsRect& aUnavailableSpace) = 0;

  /**
   * Resize the rectangular region associated with aFrame by the specified
   * deltas. The height change always applies to the bottom edge or the existing
   * rect. You specify whether the width change applies to the left or right edge
   *
   * Returns NS_OK if successful, NS_ERROR_INVALID_ARG if there is no region
   * tagged with aFrame
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
   * tagged with aFrame
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

#ifdef DEBUG
  /**
   * Dump the state of the spacemanager out to a file
   */
  NS_IMETHOD List(FILE* out) = 0;

  virtual void SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const = 0;
#endif
};

inline void nsBandTrapezoid::GetRect(nsRect& aRect) const
{
  aRect.x = PR_MIN(mTopLeftX, mBottomLeftX);
  aRect.y = mTopY;
  aRect.width = PR_MAX(mTopRightX, mBottomRightX) - aRect.x;
  aRect.height = mBottomY - mTopY;
}

inline void nsBandTrapezoid::operator=(const nsRect& aRect)
{
  mTopLeftX = mBottomLeftX = aRect.x;
  mTopRightX = mBottomRightX = aRect.XMost();
  mTopY = aRect.y;
  mBottomY = aRect.YMost();
}

inline PRBool nsBandTrapezoid::Equals(const nsBandTrapezoid aTrap) const
{
  return (
    mTopLeftX == aTrap.mTopLeftX &&
    mBottomLeftX == aTrap.mBottomLeftX &&
    mTopRightX == aTrap.mTopRightX &&
    mBottomRightX == aTrap.mBottomRightX &&
    mTopY == aTrap.mTopY &&
    mBottomY == aTrap.mBottomY &&
    mState == aTrap.mState &&
    mFrame == aTrap.mFrame    
  );
}

inline PRBool nsBandTrapezoid::EqualGeometry(const nsBandTrapezoid aTrap) const
{
  return (
    mTopLeftX == aTrap.mTopLeftX &&
    mBottomLeftX == aTrap.mBottomLeftX &&
    mTopRightX == aTrap.mTopRightX &&
    mBottomRightX == aTrap.mBottomRightX &&
    mTopY == aTrap.mTopY &&
    mBottomY == aTrap.mBottomY
  );
}

#endif /* nsISpaceManager_h___ */
