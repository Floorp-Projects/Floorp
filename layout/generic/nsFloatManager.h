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
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation
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

/* class that manages rules for positioning floats */

#ifndef nsFloatManager_h_
#define nsFloatManager_h_

#include "nsIntervalSet.h"
#include "nsCoord.h"
#include "nsRect.h"
#include "nsTArray.h"

class nsIPresShell;
class nsIFrame;
struct nsHTMLReflowState;
class nsPresContext;

#define NS_FLOAT_MANAGER_CACHE_SIZE 4

class nsFloatManager {
public:
  nsFloatManager(nsIPresShell* aPresShell);
  ~nsFloatManager();

  void* operator new(size_t aSize) CPP_THROW_NEW;
  void operator delete(void* aPtr, size_t aSize);

  static void Shutdown();

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
   * Get information about the band containing vertical coordinate |aY|,
   * but up to at most |aMaxHeight| (which may be nscoord_MAX).  This
   * will return the tallest rectangle whose top is |aY| and in which
   * there are no changes in what floats are on the sides of that
   * rectangle, but will limit the height of the rectangle to
   * |aMaxHeight|.  The left and right edges of the rectangle give the
   * area available for line boxes in that space.
   *
   * @param aY [in] vertical coordinate for top of available space
   *           desired
   * @param aMaxHeight [in] maximum height of available space desired
   * @param aContentAreaWidth [in] the width of the content area (whose left
   *                          edge must be zero in the current translation)
   * @param aHasFloats [out] whether there are floats at the sides of
   *                    the return value including those that do not
   *                    reduce the line box width at all (because they
   *                    are entirely in the margins)
   * @return the resulting rectangle for line boxes.  It will not go
   *         left of 0, nor right of aContentAreaWidth, but will be
   *         narrower when floats are present.
   *
   * aY and aAvailSpace are positioned relative to the current translation
   */
  nsRect GetBand(nscoord aY, nscoord aMaxHeight, nscoord aContentAreaWidth,
                 PRBool* aHasFloats) const;

  /**
   * Add a float that comes after all floats previously added.  Its top
   * must be even with or below the top of all previous floats.
   *
   * aMarginRect is relative to the current translation.  The caller
   * must ensure aMarginRect.height >= 0 and aMarginRect.width >= 0.
   */
  nsresult AddFloat(nsIFrame* aFloatFrame, const nsRect& aMarginRect);

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

  // Structure that stores the current state of a frame manager for
  // Save/Restore purposes.
  struct SavedState;
  friend struct SavedState;
  struct SavedState {
  private:
    PRUint32 mFloatInfoCount;
    nscoord mX, mY;
    
    friend class nsFloatManager;
  };

  PRBool HasAnyFloats() const { return !mFloats.IsEmpty(); }

  /**
   * Methods for dealing with the propagation of float damage during
   * reflow.
   */
  PRBool HasFloatDamage() const
  {
    return !mFloatDamage.IsEmpty();
  }

  void IncludeInDamage(nscoord aIntervalBegin, nscoord aIntervalEnd)
  {
    mFloatDamage.IncludeInterval(aIntervalBegin + mY, aIntervalEnd + mY);
  }

  PRBool IntersectsDamage(nscoord aIntervalBegin, nscoord aIntervalEnd) const
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
   * States must be popped in the reverse order they were pushed. 
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
  nscoord ClearFloats(nscoord aY, PRUint8 aBreakType) const;

#ifdef DEBUG
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

  static PRInt32 sCachedFloatManagerCount;
  static void* sCachedFloatManagers[NS_FLOAT_MANAGER_CACHE_SIZE];

  nsFloatManager(const nsFloatManager&);  // no implementation
  void operator=(const nsFloatManager&);  // no implementation
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
      mNew(nsnull),
      mOld(nsnull) {}

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
