/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsSpaceManager_h___
#define nsSpaceManager_h___

#include "prclist.h"
#include "nsIntervalSet.h"
#include "nsISupports.h"
#include "nsCoord.h"
#include "nsRect.h"

class nsIPresShell;
class nsIFrame;
class nsVoidArray;
struct nsSize;
struct nsHTMLReflowState;
class nsPresContext;

#define NS_SPACE_MANAGER_CACHE_SIZE 4

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

  // Get the bounding rect of the trapezoid
  inline void GetRect(nsRect& aRect) const;

  // Set the trapezoid from a rectangle
  inline void operator=(const nsRect& aRect);

  // Do these trapezoids have the same geometry, frame, and state?
  inline PRBool Equals(const nsBandTrapezoid& aTrap) const;

  // Do these trapezoids have the same geometry?
  inline PRBool EqualGeometry(const nsBandTrapezoid& aTrap) const;

  nsBandTrapezoid()
    : mTopY(0),
      mBottomY(0),
      mTopLeftX(0),
      mBottomLeftX(0),
      mTopRightX(0),
      mBottomRightX(0),
      mFrame(nsnull)
  {
  }
};

inline void nsBandTrapezoid::GetRect(nsRect& aRect) const
{
  aRect.x = PR_MIN(mTopLeftX, mBottomLeftX);
  aRect.y = mTopY;
  aRect.width = PR_MAX(mTopRightX, mBottomRightX);
  if (NS_MAXSIZE != aRect.width) {
    aRect.width -= aRect.x;
  }
  aRect.height = (NS_MAXSIZE == mBottomY) ? NS_MAXSIZE : mBottomY - mTopY;
}

inline void nsBandTrapezoid::operator=(const nsRect& aRect)
{
  mTopLeftX = mBottomLeftX = aRect.x;
  mTopRightX = mBottomRightX = aRect.XMost();
  mTopY = aRect.y;
  mBottomY = aRect.YMost();
}

inline PRBool nsBandTrapezoid::Equals(const nsBandTrapezoid& aTrap) const
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

inline PRBool nsBandTrapezoid::EqualGeometry(const nsBandTrapezoid& aTrap) const
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

/**
 * Structure used for describing the space within a band.
 * @see #GetBandData()
 */
struct nsBandData {
  PRInt32 mCount; // [out] actual number of trapezoids in the band data
  PRInt32 mSize; // [in] the size of the array (number of trapezoids)
  nsBandTrapezoid* mTrapezoids; // [out] array of length 'size'
};

/**
 * Class for dealing with bands of available space. The space manager
 * defines a coordinate space with an origin at (0, 0) that grows down
 * and to the right.
 */
class nsSpaceManager {
public:
  nsSpaceManager(nsIPresShell* aPresShell, nsIFrame* aFrame);
  ~nsSpaceManager();

  void* operator new(size_t aSize) CPP_THROW_NEW;
  void operator delete(void* aPtr, size_t aSize);

  static void Shutdown();

  /*
   * Get the frame that's associated with the space manager. This frame
   * created the space manager, and the world coordinate space is
   * relative to this frame.
   *
   * You can use QueryInterface() on this frame to get any additional
   * interfaces.
   */
  nsIFrame* GetFrame() const { return mFrame; }

  /**
   * Translate the current origin by the specified (dx, dy). This
   * creates a new local coordinate space relative to the current
   * coordinate space.
   */
  void Translate(nscoord aDx, nscoord aDy) { mX += aDx; mY += aDy; }

  /**
   * Returns the current translation from local coordinate space to
   * world coordinate space. This represents the accumulated calls to
   * Translate().
   */
  void GetTranslation(nscoord& aX, nscoord& aY) const { aX = mX; aY = mY; }

  /**
   * Returns the x-most rect in the space manager, or 0 if there are no
   * rects.
   *
   * @return  PR_TRUE if there are bands and PR_FALSE if there are no bands
   */
  PRBool XMost(nscoord& aXMost) const;

  /**
   * Returns the y-most of the bottommost band or 0 if there are no bands.
   *
   * @return  PR_TRUE if there are bands and PR_FALSE if there are no bands
   */
  PRBool YMost(nscoord& aYMost) const;

  /**
   * Returns a band starting at the specified y-offset. The band data
   * indicates which parts of the band are available, and which parts
   * are unavailable
   *
   * The band data that is returned is in the coordinate space of the
   * local coordinate system.
   *
   * The local coordinate space origin, the y-offset, and the max size
   * describe a rectangle that's used to clip the underlying band of
   * available space, i.e.
   * {0, aYOffset, aMaxSize.width, aMaxSize.height} in the local
   * coordinate space
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
  nsresult GetBandData(nscoord       aYOffset,
                       const nsSize& aMaxSize,
                       nsBandData&   aBandData) const;

  /**
   * Add a rectangular region of unavailable space. The space is
   * relative to the local coordinate system.
   *
   * The region is tagged with a frame
   *
   * @param   aFrame the frame used to identify the region. Must not be NULL
   * @param   aUnavailableSpace the bounding rect of the unavailable space
   * @return  NS_OK if successful
   *          NS_ERROR_FAILURE if there is already a region tagged with aFrame
   */
  nsresult AddRectRegion(nsIFrame*     aFrame,
                         const nsRect& aUnavailableSpace);

  /**
   * Remove the region associated with aFrane.
   *
   * Returns NS_OK if successful and NS_ERROR_INVALID_ARG if there is no region
   * tagged with aFrame
   */
protected: /* doesn't work in the general case */
  nsresult RemoveRegion(nsIFrame* aFrame);

public:
  /**
   * Clears the list of regions representing the unavailable space.
   */
  void ClearRegions();

  /**
   * Methods for dealing with the propagation of float damage during
   * reflow.
   */
  PRBool HasFloatDamage()
  {
    return !mFloatDamage.IsEmpty();
  }

  void IncludeInDamage(nscoord aIntervalBegin, nscoord aIntervalEnd)
  {
    mFloatDamage.IncludeInterval(aIntervalBegin + mY, aIntervalEnd + mY);
  }

  PRBool IntersectsDamage(nscoord aIntervalBegin, nscoord aIntervalEnd)
  {
    return mFloatDamage.Intersects(aIntervalBegin + mY, aIntervalEnd + mY);
  }

  /**
   * Pushes the current state of the space manager onto a state stack.
   */
  void PushState();

  /**
   * Restores the space manager to the state at the top of the state stack,
   * then pops this state off the stack.
   */
  void PopState();

  /**
   * Get the top of the last region placed into the space manager, to
   * enforce the rule that a float can't be above an earlier float.
   * Returns the minimum nscoord value if there are no regions.
   */
  nscoord GetLowestRegionTop();

  /**
   * Return the coordinate of the lowest float matching aBreakType in this
   * space manager. Returns aY if there are no matching floats.
   */
  nscoord ClearFloats(nscoord aY, PRUint8 aBreakType);

#ifdef DEBUG
  /**
   * Dump the state of the spacemanager out to a file
   */
  nsresult List(FILE* out);
#endif

protected:
  // Structure that maintains information about the region associated
  // with a particular frame
  struct FrameInfo {
    nsIFrame* const mFrame;
    nsRect          mRect;       // rectangular region
    FrameInfo*      mNext;

    FrameInfo(nsIFrame* aFrame, const nsRect& aRect);
#ifdef NS_BUILD_REFCNT_LOGGING
    ~FrameInfo();
#endif
  };

  // Structure that stores the current state of a frame manager for
  // Save/Restore purposes.
  struct SpaceManagerState {
    nscoord mX, mY;
    nsIFrame *mLastFrame;
    nscoord mXMost;
    nscoord mLowestTop;
    SpaceManagerState *mNext;
  };

public:
  // Doubly linked list of band rects
  struct BandRect : PRCListStr {
    nscoord   mLeft, mTop;
    nscoord   mRight, mBottom;
    PRInt32   mNumFrames;    // number of frames occupying this rect
    union {
      nsIFrame*    mFrame;   // single frame occupying the space
      nsVoidArray* mFrames;  // list of frames occupying the space
    };

    BandRect(nscoord aLeft, nscoord aTop,
             nscoord aRight, nscoord aBottom,
             nsIFrame*);
    BandRect(nscoord aLeft, nscoord aTop,
             nscoord aRight, nscoord aBottom,
             nsVoidArray*);
    ~BandRect();

    // List operations
    BandRect* Next() const {return (BandRect*)PR_NEXT_LINK(this);}
    BandRect* Prev() const {return (BandRect*)PR_PREV_LINK(this);}
    void      InsertBefore(BandRect* aBandRect) {PR_INSERT_BEFORE(aBandRect, this);}
    void      InsertAfter(BandRect* aBandRect) {PR_INSERT_AFTER(aBandRect, this);}
    void      Remove() {PR_REMOVE_LINK(this);}

    // Split the band rect into two vertically, with this band rect becoming
    // the top part, and a new band rect being allocated and returned for the
    // bottom part
    //
    // Does not insert the new band rect into the linked list
    BandRect* SplitVertically(nscoord aBottom);

    // Split the band rect into two horizontally, with this band rect becoming
    // the left part, and a new band rect being allocated and returned for the
    // right part
    //
    // Does not insert the new band rect into the linked list
    BandRect* SplitHorizontally(nscoord aRight);

    // Accessor functions
    PRBool  IsOccupiedBy(const nsIFrame*) const;
    void    AddFrame(const nsIFrame*);
    void    RemoveFrame(const nsIFrame*);
    PRBool  HasSameFrameList(const BandRect* aBandRect) const;
    PRInt32 Length() const;
  };

  // Circular linked list of band rects
  struct BandList : BandRect {
    BandList();

    // Accessors
    PRBool    IsEmpty() const {return PR_CLIST_IS_EMPTY((PRCListStr*)this);}
    BandRect* Head() const {return (BandRect*)PR_LIST_HEAD(this);}
    BandRect* Tail() const {return (BandRect*)PR_LIST_TAIL(this);}

    // Operations
    void      Append(BandRect* aBandRect) {PR_APPEND_LINK(aBandRect, this);}

    // Remove and delete all the band rects in the list
    void      Clear();
  };

protected:
  nsIFrame* const mFrame;     // frame associated with the space manager
  nscoord         mX, mY;     // translation from local to global coordinate space
  BandList        mBandList;  // header/sentinel for circular linked list of band rects
  nscoord         mXMost;
  nscoord         mLowestTop;  // the lowest *top*
  FrameInfo*      mFrameInfoMap;
  nsIntervalSet   mFloatDamage;

  SpaceManagerState *mSavedStates;
  SpaceManagerState mAutoState;

protected:
  FrameInfo* GetFrameInfoFor(nsIFrame* aFrame);
  FrameInfo* CreateFrameInfo(nsIFrame* aFrame, const nsRect& aRect);
  void       DestroyFrameInfo(FrameInfo*);

  void       ClearFrameInfo();
  void       ClearBandRects();

  BandRect*  GetNextBand(const BandRect* aBandRect) const;
  void       DivideBand(BandRect* aBand, nscoord aBottom);
  PRBool     CanJoinBands(BandRect* aBand, BandRect* aPrevBand);
  PRBool     JoinBands(BandRect* aBand, BandRect* aPrevBand);
  void       AddRectToBand(BandRect* aBand, BandRect* aBandRect);
  void       InsertBandRect(BandRect* aBandRect);

  nsresult   GetBandAvailableSpace(const BandRect* aBand,
                                   nscoord         aY,
                                   const nsSize&   aMaxSize,
                                   nsBandData&     aAvailableSpace) const;

private:
  static PRInt32 sCachedSpaceManagerCount;
  static void* sCachedSpaceManagers[NS_SPACE_MANAGER_CACHE_SIZE];

  nsSpaceManager(const nsSpaceManager&);  // no implementation
  void operator=(const nsSpaceManager&);  // no implementation
};

/**
 * A helper class to manage maintenance of the space manager during
 * nsBlockFrame::Reflow. It automatically restores the old space
 * manager in the reflow state when the object goes out of scope.
 */
class nsAutoSpaceManager {
public:
  nsAutoSpaceManager(nsHTMLReflowState& aReflowState)
    : mReflowState(aReflowState),
#ifdef DEBUG
      mOwns(PR_TRUE),
#endif
      mNew(nsnull),
      mOld(nsnull) {}

  ~nsAutoSpaceManager();

  /**
   * Create a new space manager for the specified frame. This will
   * `remember' the old space manager, and install the new space
   * manager in the reflow state.
   */
  nsresult
  CreateSpaceManagerFor(nsPresContext *aPresContext,
                        nsIFrame *aFrame);

#ifdef DEBUG
  /**
   * `Orphan' any space manager that the nsAutoSpaceManager created;
   * i.e., make it so that we don't destroy the space manager when we
   * go out of scope.
   */
  void DebugOrphanSpaceManager() { mOwns = PR_FALSE; }
#endif

protected:
  nsHTMLReflowState &mReflowState;
#ifdef DEBUG
  PRBool mOwns;
#endif
  nsSpaceManager *mNew;
  nsSpaceManager *mOld;
};

#endif /* nsSpaceManager_h___ */

