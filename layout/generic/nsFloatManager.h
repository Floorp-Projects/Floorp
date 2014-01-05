/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* class that manages rules for positioning floats */

#ifndef nsFloatManager_h_
#define nsFloatManager_h_

#include "mozilla/Attributes.h"

#include "nsIntervalSet.h"
#include "nsCoord.h"
#include "nsRect.h"
#include "nsTArray.h"
#include "nsFrameList.h" // for DEBUG_FRAME_DUMP

class nsIPresShell;
class nsIFrame;
struct nsHTMLReflowState;
class nsPresContext;

/**
 * The available space for content not occupied by floats is divided
 * into a (vertical) sequence of rectangles.  However, we need to know
 * not only the rectangle, but also whether it was reduced (from the
 * content rectangle) by floats that actually intruded into the content
 * rectangle.
 */
struct nsFlowAreaRect {
  nsRect mRect;
  bool mHasFloats;

  nsFlowAreaRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                 bool aHasFloats)
    : mRect(aX, aY, aWidth, aHeight), mHasFloats(aHasFloats) {}
};

#define NS_FLOAT_MANAGER_CACHE_SIZE 4

class nsFloatManager {
public:
  nsFloatManager(nsIPresShell* aPresShell);
  ~nsFloatManager();

  void* operator new(size_t aSize) CPP_THROW_NEW;
  void operator delete(void* aPtr, size_t aSize);

  static void Shutdown();

  /**
   * Get float region stored on the frame. (Defaults to mRect if it's
   * not there.) The float region is the area impacted by this float;
   * the coordinates are relative to the containing block frame.
   */
  static nsRect GetRegionFor(nsIFrame* aFloatFrame);
  /**
   * Calculate the float region for this frame using aMargin and the
   * frame's mRect. The region includes the margins around the float,
   * but doesn't include the relative offsets.
   * Note that if the frame is or has a continuation, aMargin's top
   * and/or bottom must be zeroed by the caller.
   */
  static nsRect CalculateRegionFor(nsIFrame* aFloatFrame,
                                   const nsMargin& aMargin);
  /**
   * Store the float region on the frame. The region is stored
   * as a delta against the mRect, so repositioning the frame will
   * also reposition the float region.
   */
  static void StoreRegionFor(nsIFrame* aFloat, nsRect& aRegion);

  // Structure that stores the current state of a frame manager for
  // Save/Restore purposes.
  struct SavedState;
  friend struct SavedState;
  struct SavedState {
  private:
    uint32_t mFloatInfoCount;
    nscoord mX, mY;
    bool mPushedLeftFloatPastBreak;
    bool mPushedRightFloatPastBreak;
    bool mSplitLeftFloatAcrossBreak;
    bool mSplitRightFloatAcrossBreak;

    friend class nsFloatManager;
  };

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
   * Get information about the area available to content that flows
   * around floats.  Two different types of space can be requested:
   *   BAND_FROM_POINT: returns the band containing vertical coordinate
   *     |aY| (though actually with the top truncated to begin at aY),
   *     but up to at most |aHeight| (which may be nscoord_MAX).
   *     This will return the tallest rectangle whose top is |aY| and in
   *     which there are no changes in what floats are on the sides of
   *     that rectangle, but will limit the height of the rectangle to
   *     |aHeight|.  The left and right edges of the rectangle give the
   *     area available for line boxes in that space.  The width of this
   *     resulting rectangle will not be negative.
   *   WIDTH_WITHIN_HEIGHT: This returns a rectangle whose top is aY and
   *     whose height is exactly aHeight.  Its left and right edges give
   *     the left and right edges of the space that can be used for line
   *     boxes *throughout* that space.  (It is possible that more
   *     horizontal space could be used in part of the space if a float
   *     begins or ends in it.)  The width of the resulting rectangle
   *     can be negative.
   *
   * @param aY [in] vertical coordinate for top of available space
   *           desired
   * @param aHeight [in] see above
   * @param aContentArea [in] an nsRect representing the content area
   * @param aState [in] If null, use the current state, otherwise, do
   *                    computation based only on floats present in the given
   *                    saved state.
   * @return An nsFlowAreaRect whose:
   *           mRect is the resulting rectangle for line boxes.  It will not
   *             extend beyond aContentArea's horizontal bounds, but may be
   *             narrower when floats are present.
   *          mBandHasFloats is whether there are floats at the sides of the
   *            return value including those that do not reduce the line box
   *            width at all (because they are entirely in the margins)
   *
   * aY and aAvailSpace are positioned relative to the current translation
   */
  enum BandInfoType { BAND_FROM_POINT, WIDTH_WITHIN_HEIGHT };
  nsFlowAreaRect GetFlowArea(nscoord aY, BandInfoType aInfoType,
                             nscoord aHeight, nsRect aContentArea,
                             SavedState* aState) const;

  /**
   * Add a float that comes after all floats previously added.  Its top
   * must be even with or below the top of all previous floats.
   *
   * aMarginRect is relative to the current translation.  The caller
   * must ensure aMarginRect.height >= 0 and aMarginRect.width >= 0.
   */
  nsresult AddFloat(nsIFrame* aFloatFrame, const nsRect& aMarginRect);

  /**
   * Notify that we tried to place a float that could not fit at all and
   * had to be pushed to the next page/column?  (If so, we can't place
   * any more floats in this page/column because of the rule that the
   * top of a float cannot be above the top of an earlier float.  It
   * also means that any clear needs to continue to the next column.)
   */
  void SetPushedLeftFloatPastBreak()
    { mPushedLeftFloatPastBreak = true; }
  void SetPushedRightFloatPastBreak()
    { mPushedRightFloatPastBreak = true; }

  /**
   * Notify that we split a float, with part of it needing to be pushed
   * to the next page/column.  (This means that any 'clear' needs to
   * continue to the next page/column.)
   */
  void SetSplitLeftFloatAcrossBreak()
    { mSplitLeftFloatAcrossBreak = true; }
  void SetSplitRightFloatAcrossBreak()
    { mSplitRightFloatAcrossBreak = true; }

  /**
   * Remove the regions associated with this floating frame and its
   * next-sibling list.  Some of the frames may never have been added;
   * we just skip those. This is not fully general; it only works as
   * long as the N frames to be removed are the last N frames to have
   * been added; if there's a frame in the middle of them that should
   * not be removed, YOU LOSE.
   */
  nsresult RemoveTrailingRegions(nsIFrame* aFrameList);

private:
  struct FloatInfo;
public:

  bool HasAnyFloats() const { return !mFloats.IsEmpty(); }

  /**
   * Methods for dealing with the propagation of float damage during
   * reflow.
   */
  bool HasFloatDamage() const
  {
    return !mFloatDamage.IsEmpty();
  }

  void IncludeInDamage(nscoord aIntervalBegin, nscoord aIntervalEnd)
  {
    mFloatDamage.IncludeInterval(aIntervalBegin + mY, aIntervalEnd + mY);
  }

  bool IntersectsDamage(nscoord aIntervalBegin, nscoord aIntervalEnd) const
  {
    return mFloatDamage.Intersects(aIntervalBegin + mY, aIntervalEnd + mY);
  }

  /**
   * Saves the current state of the float manager into aState.
   */
  void PushState(SavedState* aState);

  /**
   * Restores the float manager to the saved state.
   * 
   * These states must be managed using stack discipline. PopState can only
   * be used after PushState has been used to save the state, and it can only
   * be used once --- although it can be omitted; saved states can be ignored.
   * States must be popped in the reverse order they were pushed.  A
   * call to PopState invalidates any saved states Pushed after the
   * state passed to PopState was pushed.
   */
  void PopState(SavedState* aState);

  /**
   * Get the top of the last float placed into the float manager, to
   * enforce the rule that a float can't be above an earlier float.
   * Returns the minimum nscoord value if there are no floats.
   *
   * The result is relative to the current translation.
   */
  nscoord GetLowestFloatTop() const;

  /**
   * Return the coordinate of the lowest float matching aBreakType in this
   * float manager. Returns aY if there are no matching floats.
   *
   * Both aY and the result are relative to the current translation.
   */
  enum {
    // Tell ClearFloats not to push to nscoord_MAX when floats have been
    // pushed to the next page/column.
    DONT_CLEAR_PUSHED_FLOATS = (1<<0)
  };
  nscoord ClearFloats(nscoord aY, uint8_t aBreakType, uint32_t aFlags = 0) const;

  /**
   * Checks if clear would pass into the floats' BFC's next-in-flow,
   * i.e. whether floats affecting this clear have continuations.
   */
  bool ClearContinues(uint8_t aBreakType) const;

  void AssertStateMatches(SavedState *aState) const
  {
    NS_ASSERTION(aState->mX == mX && aState->mY == mY &&
                 aState->mPushedLeftFloatPastBreak ==
                   mPushedLeftFloatPastBreak &&
                 aState->mPushedRightFloatPastBreak ==
                   mPushedRightFloatPastBreak &&
                 aState->mSplitLeftFloatAcrossBreak ==
                   mSplitLeftFloatAcrossBreak &&
                 aState->mSplitRightFloatAcrossBreak ==
                   mSplitRightFloatAcrossBreak &&
                 aState->mFloatInfoCount == mFloats.Length(),
                 "float manager state should match saved state");
  }

#ifdef DEBUG_FRAME_DUMP
  /**
   * Dump the state of the float manager out to a file.
   */
  nsresult List(FILE* out) const;
#endif

private:

  struct FloatInfo {
    nsIFrame *const mFrame;
    nsRect mRect;
    // The lowest bottoms of left/right floats up to and including this one.
    nscoord mLeftYMost, mRightYMost;

    FloatInfo(nsIFrame* aFrame, const nsRect& aRect);
#ifdef NS_BUILD_REFCNT_LOGGING
    FloatInfo(const FloatInfo& aOther);
    ~FloatInfo();
#endif
  };

  nscoord         mX, mY;     // translation from local to global coordinate space
  nsTArray<FloatInfo> mFloats;
  nsIntervalSet   mFloatDamage;

  // Did we try to place a float that could not fit at all and had to be
  // pushed to the next page/column?  If so, we can't place any more
  // floats in this page/column because of the rule that the top of a
  // float cannot be above the top of an earlier float.  And we also
  // need to apply this information to 'clear', and thus need to
  // separate left and right floats.
  bool mPushedLeftFloatPastBreak;
  bool mPushedRightFloatPastBreak;

  // Did we split a float, with part of it needing to be pushed to the
  // next page/column.  This means that any 'clear' needs to continue to
  // the next page/column.
  bool mSplitLeftFloatAcrossBreak;
  bool mSplitRightFloatAcrossBreak;

  static int32_t sCachedFloatManagerCount;
  static void* sCachedFloatManagers[NS_FLOAT_MANAGER_CACHE_SIZE];

  nsFloatManager(const nsFloatManager&) MOZ_DELETE;
  void operator=(const nsFloatManager&) MOZ_DELETE;
};

/**
 * A helper class to manage maintenance of the float manager during
 * nsBlockFrame::Reflow. It automatically restores the old float
 * manager in the reflow state when the object goes out of scope.
 */
class nsAutoFloatManager {
public:
  nsAutoFloatManager(nsHTMLReflowState& aReflowState)
    : mReflowState(aReflowState),
      mNew(nullptr),
      mOld(nullptr) {}

  ~nsAutoFloatManager();

  /**
   * Create a new float manager for the specified frame. This will
   * `remember' the old float manager, and install the new float
   * manager in the reflow state.
   */
  nsresult
  CreateFloatManager(nsPresContext *aPresContext);

protected:
  nsHTMLReflowState &mReflowState;
  nsFloatManager *mNew;
  nsFloatManager *mOld;
};

#endif /* !defined(nsFloatManager_h_) */
