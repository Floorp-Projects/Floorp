/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsSpaceManager.h"
#include "nsPoint.h"
#include "nsRect.h"
#include "nsSize.h"
#include <stdlib.h>
#include "nsVoidArray.h"
#include "nsIFrame.h"
#include "nsString.h"
#include "nsIPresShell.h"
#include "nsMemory.h"
#include "nsHTMLReflowState.h"
#ifdef DEBUG
#include "nsIFrameDebug.h"
#endif

/////////////////////////////////////////////////////////////////////////////
// BandList

PRInt32 nsSpaceManager::sCachedSpaceManagerCount = 0;
void* nsSpaceManager::sCachedSpaceManagers[NS_SPACE_MANAGER_CACHE_SIZE];

#define NSCOORD_MIN (-2147483647 - 1) /* minimum signed value */

nsSpaceManager::BandList::BandList()
  : nsSpaceManager::BandRect(NSCOORD_MIN, NSCOORD_MIN, NSCOORD_MIN, NSCOORD_MIN, (nsIFrame*)nsnull)
{
  PR_INIT_CLIST(this);
  mNumFrames = 0;
}

void
nsSpaceManager::BandList::Clear()
{
  if (!IsEmpty()) {
    BandRect* bandRect = Head();
  
    while (bandRect != this) {
      BandRect* nxt = bandRect->Next();
  
      delete bandRect;
      bandRect = nxt;
    }
  
    PR_INIT_CLIST(this);
  }
}

/////////////////////////////////////////////////////////////////////////////

// PresShell Arena allocate callback (for nsIntervalSet use below)
PR_STATIC_CALLBACK(void*)
PSArenaAllocCB(size_t aSize, void* aClosure)
{
  return NS_STATIC_CAST(nsIPresShell*, aClosure)->AllocateFrame(aSize);
}

// PresShell Arena free callback (for nsIntervalSet use below)
PR_STATIC_CALLBACK(void)
PSArenaFreeCB(size_t aSize, void* aPtr, void* aClosure)
{
  NS_STATIC_CAST(nsIPresShell*, aClosure)->FreeFrame(aSize, aPtr);
}

/////////////////////////////////////////////////////////////////////////////
// nsSpaceManager

MOZ_DECL_CTOR_COUNTER(nsSpaceManager)

nsSpaceManager::nsSpaceManager(nsIPresShell* aPresShell, nsIFrame* aFrame)
  : mFrame(aFrame),
    mXMost(0),
    mLowestTop(NSCOORD_MIN),
    mFloatDamage(PSArenaAllocCB, PSArenaFreeCB, aPresShell)
{
  MOZ_COUNT_CTOR(nsSpaceManager);
  mX = mY = 0;
  mFrameInfoMap = nsnull;
  mSavedStates = nsnull;
}

void
nsSpaceManager::ClearFrameInfo()
{
  while (mFrameInfoMap) {
    FrameInfo*  next = mFrameInfoMap->mNext;
    delete mFrameInfoMap;
    mFrameInfoMap = next;
  }
}

nsSpaceManager::~nsSpaceManager()
{
  MOZ_COUNT_DTOR(nsSpaceManager);
  mBandList.Clear();
  ClearFrameInfo();

  NS_ASSERTION(!mSavedStates, "states remaining on state stack");

  while (mSavedStates && mSavedStates != &mAutoState){
    SpaceManagerState *state = mSavedStates;
    mSavedStates = state->mNext;
    delete state;
  }
}

// static
void* nsSpaceManager::operator new(size_t aSize) CPP_THROW_NEW
{
  if (sCachedSpaceManagerCount > 0) {
    // We have cached unused instances of this class, return a cached
    // instance in stead of always creating a new one.
    return sCachedSpaceManagers[--sCachedSpaceManagerCount];
  }

  // The cache is empty, this means we haveto create a new instance using
  // the global |operator new|.
  return nsMemory::Alloc(aSize);
}

void
nsSpaceManager::operator delete(void* aPtr, size_t aSize)
{
  if (!aPtr)
    return;
  // This space manager is no longer used, if there's still room in
  // the cache we'll cache this space manager, unless the layout
  // module was already shut down.

  if (sCachedSpaceManagerCount < NS_SPACE_MANAGER_CACHE_SIZE &&
      sCachedSpaceManagerCount >= 0) {
    // There's still space in the cache for more instances, put this
    // instance in the cache in stead of deleting it.

    sCachedSpaceManagers[sCachedSpaceManagerCount++] = aPtr;
    return;
  }

  // The cache is full, or the layout module has been shut down,
  // delete this space manager.
  nsMemory::Free(aPtr);
}


/* static */
void nsSpaceManager::Shutdown()
{
  // The layout module is being shut down, clean up the cache and
  // disable further caching.

  PRInt32 i;

  for (i = 0; i < sCachedSpaceManagerCount; i++) {
    void* spaceManager = sCachedSpaceManagers[i];
    if (spaceManager)
      nsMemory::Free(spaceManager);
  }

  // Disable futher caching.
  sCachedSpaceManagerCount = -1;
}

PRBool
nsSpaceManager::XMost(nscoord& aXMost) const
{
  aXMost = mXMost;
  return !mBandList.IsEmpty();
}

PRBool
nsSpaceManager::YMost(nscoord& aYMost) const
{
  PRBool result;

  if (mBandList.IsEmpty()) {
    aYMost = 0;
    result = PR_FALSE;

  } else {
    BandRect* lastRect = mBandList.Tail();

    aYMost = lastRect->mBottom;
    result = PR_TRUE;
  }

  return result;
}

/**
 * Internal function that returns the list of available and unavailable space
 * within the band
 *
 * @param aBand the first rect in the band
 * @param aY the y-offset in world coordinates
 * @param aMaxSize the size to use to constrain the band data
 * @param aAvailableBand
 */
nsresult
nsSpaceManager::GetBandAvailableSpace(const BandRect* aBand,
                                      nscoord         aY,
                                      const nsSize&   aMaxSize,
                                      nsBandData&     aBandData) const
{
  nscoord          topOfBand = aBand->mTop;
  nscoord          localY = aY - mY;
  nscoord          height = PR_MIN(aBand->mBottom - aY, aMaxSize.height);
  nsBandTrapezoid* trapezoid = aBandData.mTrapezoids;
  nscoord          rightEdge = mX + aMaxSize.width;

  // Initialize the band data
  aBandData.mCount = 0;

  // Skip any rectangles that are to the left of the local coordinate space
  while (aBand->mTop == topOfBand) {
    if (aBand->mRight > mX) {
      break;
    }

    // Get the next rect in the band
    aBand = aBand->Next();
  }

  // This is used to track the current x-location within the band. This is in
  // world coordinates
  nscoord   left = mX;

  // Process the remaining rectangles that are within the clip width
  while ((aBand->mTop == topOfBand) && (aBand->mLeft < rightEdge)) {
    // Compare the left edge of the occupied space with the current left
    // coordinate
    if (aBand->mLeft > left) {
      // The rect is to the right of our current left coordinate, so we've
      // found some available space
      if (aBandData.mCount >= aBandData.mSize) {
        // Not enough space in the array of trapezoids
        aBandData.mCount += 2 * aBand->Length() + 2;  // estimate the number needed
        return NS_ERROR_FAILURE;
      }
      trapezoid->mState = nsBandTrapezoid::Available;
      trapezoid->mFrame = nsnull;

      // Assign the trapezoid a rectangular shape. The trapezoid must be in the
      // local coordinate space, so convert the current left coordinate
      *trapezoid = nsRect(left - mX, localY, aBand->mLeft - left, height);

      // Move to the next output rect
      trapezoid++;
      aBandData.mCount++;
    }

    // The rect represents unavailable space, so add another trapezoid
    if (aBandData.mCount >= aBandData.mSize) {
      // Not enough space in the array of trapezoids
      aBandData.mCount += 2 * aBand->Length() + 1;  // estimate the number needed
      return NS_ERROR_FAILURE;
    }
    if (1 == aBand->mNumFrames) {
      trapezoid->mState = nsBandTrapezoid::Occupied;
      trapezoid->mFrame = aBand->mFrame;
    } else {
      NS_ASSERTION(aBand->mNumFrames > 1, "unexpected frame count");
      trapezoid->mState = nsBandTrapezoid::OccupiedMultiple;
      trapezoid->mFrames = aBand->mFrames;
    }

    nscoord x = aBand->mLeft;
    // The first band can straddle the clip rect
    if (x < mX) {
      // Clip the left edge
      x = mX;
    }

    // Assign the trapezoid a rectangular shape. The trapezoid must be in the
    // local coordinate space, so convert the rects's left coordinate
    *trapezoid = nsRect(x - mX, localY, aBand->mRight - x, height);

    // Move to the next output rect
    trapezoid++;
    aBandData.mCount++;

    // Adjust our current x-location within the band
    left = aBand->mRight;

    // Move to the next rect within the band
    aBand = aBand->Next();
  }

  // No more rects left in the band. If we haven't yet reached the right edge,
  // then all the remaining space is available
  if (left < rightEdge) {
    if (aBandData.mCount >= aBandData.mSize) {
      // Not enough space in the array of trapezoids
      aBandData.mCount++;
      return NS_ERROR_FAILURE;
    }
    trapezoid->mState = nsBandTrapezoid::Available;
    trapezoid->mFrame = nsnull;

    // Assign the trapezoid a rectangular shape. The trapezoid must be in the
    // local coordinate space, so convert the current left coordinate
    *trapezoid = nsRect(left - mX, localY, rightEdge - left, height);
    aBandData.mCount++;
  }

  return NS_OK;
}

nsresult
nsSpaceManager::GetBandData(nscoord       aYOffset,
                            const nsSize& aMaxSize,
                            nsBandData&   aBandData) const
{
  NS_PRECONDITION(aBandData.mSize >= 1, "bad band data");
  nsresult  result = NS_OK;

  // Convert the y-offset to world coordinates
  nscoord   y = mY + aYOffset;

  // If there are no unavailable rects or the offset is below the bottommost
  // band, then all the space is available
  nscoord yMost;
  
  if (!YMost(yMost) || (y >= yMost)) {
    // All the requested space is available
    aBandData.mCount = 1;
    aBandData.mTrapezoids[0] = nsRect(0, aYOffset, aMaxSize.width, aMaxSize.height);
    aBandData.mTrapezoids[0].mState = nsBandTrapezoid::Available;
    aBandData.mTrapezoids[0].mFrame = nsnull;
  } else {
    // Find the first band that contains the y-offset or is below the y-offset
    NS_ASSERTION(!mBandList.IsEmpty(), "no bands");
    BandRect* band = mBandList.Head();

    aBandData.mCount = 0;
    while (nsnull != band) {
      if (band->mTop > y) {
        // The band is below the y-offset. The area between the y-offset and
        // the top of the band is available
        aBandData.mCount = 1;
        aBandData.mTrapezoids[0] =
          nsRect(0, aYOffset, aMaxSize.width, PR_MIN(band->mTop - y, aMaxSize.height));
        aBandData.mTrapezoids[0].mState = nsBandTrapezoid::Available;
        aBandData.mTrapezoids[0].mFrame = nsnull;
        break;
      } else if (y < band->mBottom) {
        // The band contains the y-offset. Return a list of available and
        // unavailable rects within the band
        return GetBandAvailableSpace(band, y, aMaxSize, aBandData);
      } else {
        // Skip to the next band
        band = GetNextBand(band);
      }
    }
  }

  NS_POSTCONDITION(aBandData.mCount > 0, "unexpected band data count");
  return result;
}

/**
 * Skips to the start of the next band.
 *
 * @param aBandRect A rect within the band
 * @returns The start of the next band, or nsnull of this is the last band.
 */
nsSpaceManager::BandRect*
nsSpaceManager::GetNextBand(const BandRect* aBandRect) const
{
  nscoord topOfBand = aBandRect->mTop;

  aBandRect = aBandRect->Next();
  while (aBandRect != &mBandList) {
    // Check whether this rect is part of the same band
    if (aBandRect->mTop != topOfBand) {
      // We found the start of the next band
      return (BandRect*)aBandRect;
    }

    aBandRect = aBandRect->Next();
  }

  // No bands left
  return nsnull;
}

/**
 * Divides the current band into two vertically
 *
 * @param aBandRect the first rect in the band
 * @param aBottom where to split the band. This becomes the bottom of the top
 *          part
 */
void
nsSpaceManager::DivideBand(BandRect* aBandRect, nscoord aBottom)
{
  NS_PRECONDITION(aBottom < aBandRect->mBottom, "bad height");
  nscoord   topOfBand = aBandRect->mTop;
  BandRect* nextBand = GetNextBand(aBandRect);

  if (nsnull == nextBand) {
    nextBand = (BandRect*)&mBandList;
  }

  while (topOfBand == aBandRect->mTop) {
    // Split the band rect into two vertically
    BandRect* bottomBandRect = aBandRect->SplitVertically(aBottom);

    // Insert the new bottom part
    nextBand->InsertBefore(bottomBandRect);

    // Move to the next rect in the band
    aBandRect = aBandRect->Next();
  }
}

PRBool
nsSpaceManager::CanJoinBands(BandRect* aBand, BandRect* aPrevBand)
{
  PRBool  result;
  nscoord topOfBand = aBand->mTop;
  nscoord topOfPrevBand = aPrevBand->mTop;

  // The bands can be joined if:
  // - they're adjacent
  // - they have the same number of rects
  // - each rect has the same left and right edge as its corresponding rect, and
  //   the rects are occupied by the same frames
  if (aPrevBand->mBottom == aBand->mTop) {
    // Compare each of the rects in the two bands
    while (PR_TRUE) {
      if ((aBand->mLeft != aPrevBand->mLeft) || (aBand->mRight != aPrevBand->mRight)) {
        // The rects have different edges
        result = PR_FALSE;
        break;
      }

      if (!aBand->HasSameFrameList(aPrevBand)) {
        // The rects are occupied by different frames
        result = PR_FALSE;
        break;
      }

      // Move to the next rects within the bands
      aBand = aBand->Next();
      aPrevBand = aPrevBand->Next();

      // Have we reached the end of either band?
      PRBool  endOfBand = aBand->mTop != topOfBand;
      PRBool  endOfPrevBand = aPrevBand->mTop != topOfPrevBand;

      if (endOfBand || endOfPrevBand) {
        result = endOfBand & endOfPrevBand;
        break;  // all done
      }
    }

  } else {
    // The bands aren't adjacent
    result = PR_FALSE;
  }

  return result;
}

/**
 * Tries to join the two adjacent bands. Returns PR_TRUE if successful and
 * PR_FALSE otherwise
 *
 * If the two bands are joined, the previous band is the the band that's deleted
 */
PRBool
nsSpaceManager::JoinBands(BandRect* aBand, BandRect* aPrevBand)
{
  if (CanJoinBands(aBand, aPrevBand)) {
    BandRect* startOfNextBand = aBand;

    while (aPrevBand != startOfNextBand) {
      // Adjust the top of the band we're keeping, and then move to the next
      // rect within the band
      aBand->mTop = aPrevBand->mTop;
      aBand = aBand->Next();

      // Delete the rect from the previous band
      BandRect* next = aPrevBand->Next();

      aPrevBand->Remove();
      delete aPrevBand;
      aPrevBand = next;
    }

    return PR_TRUE;
  }

  return PR_FALSE;
}

/**
 * Adds a new rect to a band.
 *
 * @param aBand the first rect in the band
 * @param aBandRect the band rect to add to the band
 */
void
nsSpaceManager::AddRectToBand(BandRect* aBand, BandRect* aBandRect)
{
  NS_PRECONDITION((aBand->mTop == aBandRect->mTop) &&
                  (aBand->mBottom == aBandRect->mBottom), "bad band");
  NS_PRECONDITION(1 == aBandRect->mNumFrames, "shared band rect");
  nscoord topOfBand = aBand->mTop;

  // Figure out where in the band horizontally to insert the rect
  do {
    // Compare the left edge of the new rect with the left edge of the existing
    // rect
    if (aBandRect->mLeft < aBand->mLeft) {
      // The new rect's left edge is to the left of the existing rect's left edge.
      // Could be any of these cases (N is new rect, E is existing rect):
      //
      //   Case 1: left-of      Case 2: overlaps     Case 3: N.contains(E)
      //   ---------------      ----------------     ---------------------
      //   +-----+ +-----+      +-----+              +---------+
      //   |  N  | |  E  |      |  N  |              |    N    |
      //   +-----+ +-----+      +-----+              +---------+
      //                           +-----+              +---+
      //                           |  E  |              | E |
      //                           +-----+              +---+
      //
      // Do the two rectangles overlap?
      if (aBandRect->mRight <= aBand->mLeft) {
        // No, the new rect is completely to the left of the existing rect
        // (case #1). Insert a new rect
        aBand->InsertBefore(aBandRect);
        return;
      }

      // Yes, they overlap. Compare the right edges.
      if (aBandRect->mRight > aBand->mRight) {
        // The new rect's right edge is to the right of the existing rect's
        // right edge (case #3). Split the new rect
        BandRect* r1 = aBandRect->SplitHorizontally(aBand->mLeft);

        // Insert the part of the new rect that's to the left of the existing
        // rect as a new band rect
        aBand->InsertBefore(aBandRect);

        // Continue below with the part that overlaps the existing rect
        aBandRect = r1;

      } else {
        if (aBand->mRight > aBandRect->mRight) {
          // The existing rect extends past the new rect (case #2). Split the
          // existing rect
          BandRect* r1 = aBand->SplitHorizontally(aBandRect->mRight);

          // Insert the new right half of the existing rect
          aBand->InsertAfter(r1);
        }

        // Insert the part of the new rect that's to the left of the existing
        // rect
        aBandRect->mRight = aBand->mLeft;
        aBand->InsertBefore(aBandRect);

        // Mark the existing rect as shared
        aBand->AddFrame(aBandRect->mFrame);
        return;
      }
    }
      
    if (aBandRect->mLeft > aBand->mLeft) {
      // The new rect's left edge is to the right of the existing rect's left
      // edge. Could be any one of these cases:
      //
      //   Case 4: right-of    Case 5: overlaps     Case 6: E.Contains(N)
      //   ---------------    ----------------     ---------------------
      //   +-----+ +-----+    +-----+              +------------+
      //   |  E  | |  N  |    |  E  |              |      E     |
      //   +-----+ +-----+    +-----+              +------------+
      //                         +-----+              +-----+
      //                         |  N  |              |  N  |
      //                         +-----+              +-----+
      //
      if (aBandRect->mLeft >= aBand->mRight) {
        // The new rect is to the right of the existing rect (case #4), so move
        // to the next rect in the band
        aBand = aBand->Next();
        continue;
      }

      // The rects overlap, so divide the existing rect into two rects: the
      // part to the left of the new rect, and the part that overlaps
      BandRect* r1 = aBand->SplitHorizontally(aBandRect->mLeft);

      // Insert the new right half of the existing rect, and make it the current
      // rect
      aBand->InsertAfter(r1);
      aBand = r1;
    }

    // At this point the left edge of the new rect is the same as the left edge
    // of the existing rect
    NS_ASSERTION(aBandRect->mLeft == aBand->mLeft, "unexpected rect");

    // Compare which rect is wider, the new rect or the existing rect
    if (aBand->mRight > aBandRect->mRight) {
      // The existing rect is wider (case #6). Divide the existing rect into
      // two rects: the part that overlaps, and the part to the right of the
      // new rect
      BandRect* r1 = aBand->SplitHorizontally(aBandRect->mRight);

      // Insert the new right half of the existing rect
      aBand->InsertAfter(r1);

      // Mark the overlap as being shared
      aBand->AddFrame(aBandRect->mFrame);
      return;

    } else {
      // Indicate the frames share the existing rect
      aBand->AddFrame(aBandRect->mFrame);

      if (aBand->mRight == aBandRect->mRight) {
        // The new and existing rect have the same right edge. We're all done,
        // and the new band rect is no longer needed
        delete aBandRect;
        return;
      } else {
        // The new rect is wider than the existing rect (cases #5). Set the
        // new rect to be the overhang, and move to the next rect within the band
        aBandRect->mLeft = aBand->mRight;
        aBand = aBand->Next();
        continue;
      }
    }
  } while (aBand->mTop == topOfBand);

  // Insert a new rect
  aBand->InsertBefore(aBandRect);
}

// When comparing a rect to a band there are seven cases to consider.
// 'R' is the rect and 'B' is the band.
//
//      Case 1              Case 2              Case 3              Case 4
//      ------              ------              ------              ------
// +-----+             +-----+                      +-----+             +-----+
// |  R  |             |  R  |  +-----+    +-----+  |     |             |     |
// +-----+             +-----+  |     |    |  R  |  |  B  |             |  B  |
//          +-----+             |  B  |    +-----+  |     |    +-----+  |     |
//          |     |             |     |             +-----+    |  R  |  +-----+
//          |  B  |             +-----+                        +-----+
//          |     |
//          +-----+
//
//
//
//      Case 5              Case 6              Case 7
//      ------              ------              ------
//          +-----+    +-----+  +-----+    +-----+
//          |     |    |  R  |  |  B  |    |     |  +-----+
//          |  B  |    +-----+  +-----+    |  R  |  |  B  |
//          |     |                        |     |  +-----+
//          +-----+                        +-----+
// +-----+
// |  R  |
// +-----+
//
void
nsSpaceManager::InsertBandRect(BandRect* aBandRect)
{
  // If there are no existing bands or this rect is below the bottommost
  // band, then add a new band
  nscoord yMost;
  if (!YMost(yMost) || (aBandRect->mTop >= yMost)) {
    mBandList.Append(aBandRect);
    return;
  }

  // Examine each band looking for a band that intersects this rect
  NS_ASSERTION(!mBandList.IsEmpty(), "no bands");
  BandRect* band = mBandList.Head();

  while (nsnull != band) {
    // Compare the top edge of this rect with the top edge of the band
    if (aBandRect->mTop < band->mTop) {
      // The top edge of the rect is above the top edge of the band.
      // Is there any overlap?
      if (aBandRect->mBottom <= band->mTop) {
        // Case #1. This rect is completely above the band, so insert a
        // new band before the current band
        band->InsertBefore(aBandRect);
        break;  // we're all done
      }

      // Case #2 and case #7. Divide this rect, creating a new rect for
      // the part that's above the band
      BandRect* bandRect1 = new BandRect(aBandRect->mLeft, aBandRect->mTop,
                                         aBandRect->mRight, band->mTop,
                                         aBandRect->mFrame);

      // Insert bandRect1 as a new band
      band->InsertBefore(bandRect1);

      // Modify this rect to exclude the part above the band
      aBandRect->mTop = band->mTop;

    } else if (aBandRect->mTop > band->mTop) {
      // The top edge of the rect is below the top edge of the band. Is there
      // any overlap?
      if (aBandRect->mTop >= band->mBottom) {
        // Case #5. This rect is below the current band. Skip to the next band
        band = GetNextBand(band);
        continue;
      }

      // Case #3 and case #4. Divide the current band into two bands with the
      // top band being the part that's above the rect
      DivideBand(band, aBandRect->mTop);

      // Skip to the bottom band that we just created
      band = GetNextBand(band);
    }

    // At this point the rect and the band should have the same y-offset
    NS_ASSERTION(aBandRect->mTop == band->mTop, "unexpected band");

    // Is the band higher than the rect?
    if (band->mBottom > aBandRect->mBottom) {
      // Divide the band into two bands with the top band the same height
      // as the rect
      DivideBand(band, aBandRect->mBottom);
    }

    if (aBandRect->mBottom == band->mBottom) {
      // Add the rect to the band
      AddRectToBand(band, aBandRect);
      break;

    } else {
      // Case #4 and case #7. The rect contains the band vertically. Divide
      // the rect, creating a new rect for the part that overlaps the band
      BandRect* bandRect1 = new BandRect(aBandRect->mLeft, aBandRect->mTop,
                                         aBandRect->mRight, band->mBottom,
                                         aBandRect->mFrame);

      // Add bandRect1 to the band
      AddRectToBand(band, bandRect1);

      // Modify aBandRect to be the part below the band
      aBandRect->mTop = band->mBottom;

      // Continue with the next band
      band = GetNextBand(band);
      if (nsnull == band) {
        // Append a new bottommost band
        mBandList.Append(aBandRect);
        break;
      }
    }
  }
}

nsresult
nsSpaceManager::AddRectRegion(nsIFrame* aFrame, const nsRect& aUnavailableSpace)
{
  NS_PRECONDITION(nsnull != aFrame, "null frame");

  // See if there is already a region associated with aFrame
  FrameInfo*  frameInfo = GetFrameInfoFor(aFrame);

  if (nsnull != frameInfo) {
    NS_WARNING("aFrame is already associated with a region");
    return NS_ERROR_FAILURE;
  }

  // Convert the frame to world coordinates
  nsRect  rect(aUnavailableSpace.x + mX, aUnavailableSpace.y + mY,
               aUnavailableSpace.width, aUnavailableSpace.height);

  nscoord xmost = rect.XMost();
  if (xmost > mXMost)
    mXMost = xmost;

  if (rect.y > mLowestTop)
    mLowestTop = rect.y;

  // Create a frame info structure
  frameInfo = CreateFrameInfo(aFrame, rect);
  if (nsnull == frameInfo) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Is the rect empty?
  if (aUnavailableSpace.IsEmpty()) {
    // The rect doesn't consume any space, so don't add any band data
    return NS_OK;
  }

  // Allocate a band rect
  BandRect* bandRect = new BandRect(rect.x, rect.y, rect.XMost(), rect.YMost(), aFrame);
  if (nsnull == bandRect) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Insert the band rect
  InsertBandRect(bandRect);
  return NS_OK;
}

nsresult
nsSpaceManager::RemoveRegion(nsIFrame* aFrame)
{
  // Get the frame info associated with aFrame
  FrameInfo*  frameInfo = GetFrameInfoFor(aFrame);

  if (nsnull == frameInfo) {
    NS_WARNING("no region associated with aFrame");
    return NS_ERROR_INVALID_ARG;
  }

  if (!frameInfo->mRect.IsEmpty()) {
    NS_ASSERTION(!mBandList.IsEmpty(), "no bands");
    BandRect* band = mBandList.Head();
    BandRect* prevBand = nsnull;
    PRBool    prevFoundMatchingRect = PR_FALSE;

    // Iterate each band looking for rects tagged with aFrame
    while (nsnull != band) {
      BandRect* rect = band;
      BandRect* prevRect = nsnull;
      nscoord   topOfBand = band->mTop;
      PRBool    foundMatchingRect = PR_FALSE;
      PRBool    prevIsSharedRect = PR_FALSE;

      // Iterate each rect in the band
      do {
        PRBool  isSharedRect = PR_FALSE;

        if (rect->IsOccupiedBy(aFrame)) {
          // Remember that we found a matching rect in this band
          foundMatchingRect = PR_TRUE;

          if (rect->mNumFrames > 1) {
            // The band rect is occupied by more than one frame
            rect->RemoveFrame(aFrame);

            // Remember that this rect was being shared by more than one frame
            // including aFrame
            isSharedRect = PR_TRUE;
          } else {
            // The rect isn't shared so just delete it
            BandRect* next = rect->Next();
            rect->Remove();
            if (rect == band) {
              // The rect we're deleting is the start of the band
              if (topOfBand == next->mTop) {
                band = next;
              } else {
                band = nsnull;
              }
            }
            delete rect;
            rect = next;

            // We don't need to try and coalesce adjacent rects in this case
            prevRect = nsnull;
            prevIsSharedRect = PR_FALSE;
            continue;
          }
        }
           
        // If we found a shared rect occupied by aFrame, then we need to try
        // and coalesce adjacent rects
        if (prevIsSharedRect || (isSharedRect && (nsnull != prevRect))) {
          NS_ASSERTION(nsnull != prevRect, "no previous rect");
          if ((prevRect->mRight == rect->mLeft) && (prevRect->HasSameFrameList(rect))) {
            // Modify the current rect's left edge, and delete the previous rect
            rect->mLeft = prevRect->mLeft;
            prevRect->Remove();
            if (prevRect == band) {
              // the rect we're deleting is the start of the band
              band = rect;
            }
            delete prevRect;
          }
        }

        // Get the next rect in the band
        prevRect = rect;
        prevIsSharedRect = isSharedRect;
        rect = rect->Next();
      } while (rect->mTop == topOfBand);

      if (nsnull != band) {
        // If we found a rect occupied by aFrame in this band or the previous band
        // then try to join the two bands
        if ((nsnull != prevBand) && (foundMatchingRect || prevFoundMatchingRect)) {
          // Try and join this band with the previous band
          JoinBands(band, prevBand);
        }
      }

      // Move to the next band
      prevFoundMatchingRect = foundMatchingRect;
      prevBand = band;
      band = (rect == &mBandList) ? nsnull : rect;
    }
  }

  DestroyFrameInfo(frameInfo);
  return NS_OK;
}

void
nsSpaceManager::ClearRegions()
{
  ClearFrameInfo();
  mBandList.Clear();
  mLowestTop = NSCOORD_MIN;
}

void
nsSpaceManager::PushState()
{
  // This is a quick and dirty push implementation, which
  // only saves the (x,y) and last frame in the mFrameInfoMap
  // which is enough info to get us back to where we should be
  // when pop is called.
  //
  // The alternative would be to make full copies of the contents
  // of mBandList and mFrameInfoMap and restore them when pop is
  // called, but I'm not sure it's worth the effort/bloat at this
  // point, since this push/pop mechanism is only used to undo any
  // floats that were added during the unconstrained reflow
  // in nsBlockReflowContext::DoReflowBlock(). (See bug 96736)
  //
  // It should also be noted that the state for mFloatDamage is
  // intentionally not saved or restored in PushState() and PopState(),
  // since that could lead to bugs where damage is missed/dropped when
  // we move from position A to B (during the intermediate incremental
  // reflow mentioned above) and then from B to C during the subsequent
  // reflow. In the typical case A and C will be the same, but not always.
  // Allowing mFloatDamage to accumulate the damage incurred during both
  // reflows ensures that nothing gets missed.

  SpaceManagerState *state;

  if(mSavedStates) {
    state = new SpaceManagerState;
  } else {
    state = &mAutoState;
  }

  NS_ASSERTION(state, "PushState() failed!");

  if (!state) {
    return;
  }

  state->mX = mX;
  state->mY = mY;
  state->mXMost = mXMost;
  state->mLowestTop = mLowestTop;

  if (mFrameInfoMap) {
    state->mLastFrame = mFrameInfoMap->mFrame;
  } else {
    state->mLastFrame = nsnull;
  }

  // Now that we've saved our state, add it to mSavedStates.

  state->mNext = mSavedStates;
  mSavedStates = state;
}

void
nsSpaceManager::PopState()
{
  // This is a quick and dirty pop implementation, to
  // match the current implementation of PushState(). The
  // idea here is to remove any frames that have been added
  // to the mFrameInfoMap since the last call to PushState().

  NS_ASSERTION(mSavedStates, "Invalid call to PopState()!");

  if (!mSavedStates) {
    return;
  }

  // mFrameInfoMap is LIFO so keep removing what it points
  // to until we hit mLastFrame.

  while (mFrameInfoMap && mFrameInfoMap->mFrame != mSavedStates->mLastFrame) {
    RemoveRegion(mFrameInfoMap->mFrame);
  }

  // If we trip this assertion it means that someone added
  // PushState()/PopState() calls around code that actually
  // removed mLastFrame from mFrameInfoMap, which means our
  // state is now out of sync with what we thought it should be.

  NS_ASSERTION(((mSavedStates->mLastFrame && mFrameInfoMap) ||
               (!mSavedStates->mLastFrame && !mFrameInfoMap)),
               "Unexpected outcome!");

  mX = mSavedStates->mX;
  mY = mSavedStates->mY;
  mXMost = mSavedStates->mXMost;
  mLowestTop = mSavedStates->mLowestTop;

  // Now that we've restored our state, pop the topmost
  // state and delete it.  Make sure not to delete the mAutoState element
  // as it is embeded in this class

  SpaceManagerState *state = mSavedStates;
  mSavedStates = mSavedStates->mNext;
  if(state != &mAutoState) {
    delete state;
  }
}

nscoord
nsSpaceManager::GetLowestRegionTop()
{
  if (mLowestTop == NSCOORD_MIN)
    return mLowestTop;
  return mLowestTop - mY;
}

#ifdef DEBUG
void
DebugListSpaceManager(nsSpaceManager *aSpaceManager)
{
  aSpaceManager->List(stdout);
}

nsresult
nsSpaceManager::List(FILE* out)
{
  nsAutoString tmp;

  fprintf(out, "SpaceManager@%p", this);
  if (mFrame) {
    nsIFrameDebug*  frameDebug;

    if (NS_SUCCEEDED(mFrame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
      frameDebug->GetFrameName(tmp);
      fprintf(out, " frame=");
      fputs(NS_LossyConvertUCS2toASCII(tmp).get(), out);
      fprintf(out, "@%p", mFrame);
    }
  }
  fprintf(out, " xy=%d,%d <\n", mX, mY);
  if (mBandList.IsEmpty()) {
    fprintf(out, "  no bands\n");
  }
  else {
    BandRect* band = mBandList.Head();
    do {
      fprintf(out, "  left=%d top=%d right=%d bottom=%d numFrames=%d",
              band->mLeft, band->mTop, band->mRight, band->mBottom,
              band->mNumFrames);
      if (1 == band->mNumFrames) {
        nsIFrameDebug*  frameDebug;

        if (NS_SUCCEEDED(band->mFrame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
          frameDebug->GetFrameName(tmp);
          fprintf(out, " frame=");
          fputs(NS_LossyConvertUCS2toASCII(tmp).get(), out);
          fprintf(out, "@%p", band->mFrame);
        }
      }
      else if (1 < band->mNumFrames) {
        fprintf(out, "\n    ");
        nsVoidArray* a = band->mFrames;
        PRInt32 i, n = a->Count();
        for (i = 0; i < n; i++) {
          nsIFrame* frame = (nsIFrame*) a->ElementAt(i);
          if (frame) {
            nsIFrameDebug*  frameDebug;

            if (NS_SUCCEEDED(frame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
              frameDebug->GetFrameName(tmp);
              fputs(NS_LossyConvertUCS2toASCII(tmp).get(), out);
              fprintf(out, "@%p ", frame);
            }
          }
        }
      }
      fprintf(out, "\n");
      band = band->Next();
    } while (band != mBandList.Head());
  }
  fprintf(out, ">\n");
  return NS_OK;
}
#endif

nsSpaceManager::FrameInfo*
nsSpaceManager::GetFrameInfoFor(nsIFrame* aFrame)
{
  FrameInfo*  result = nsnull;

  for (result = mFrameInfoMap; result; result = result->mNext) {
    if (result->mFrame == aFrame) {
      break;
    }
  }

  return result;
}

nsSpaceManager::FrameInfo*
nsSpaceManager::CreateFrameInfo(nsIFrame* aFrame, const nsRect& aRect)
{
  FrameInfo*  frameInfo = new FrameInfo(aFrame, aRect);

  if (frameInfo) {
    // Link it into the list
    frameInfo->mNext = mFrameInfoMap;
    mFrameInfoMap = frameInfo;
  }
  return frameInfo;
}

void
nsSpaceManager::DestroyFrameInfo(FrameInfo* aFrameInfo)
{
  // See if it's at the head of the list
  if (mFrameInfoMap == aFrameInfo) {
    mFrameInfoMap = aFrameInfo->mNext;

  } else {
    FrameInfo*  prev;
    
    // Find the previous node in the list
    for (prev = mFrameInfoMap; prev && (prev->mNext != aFrameInfo); prev = prev->mNext) {
      ;
    }

    // Disconnect it from the list
    NS_ASSERTION(prev, "element not in list");
    if (prev) {
      prev->mNext = aFrameInfo->mNext;
    }
  }

  delete aFrameInfo;
}

static PRBool
ShouldClearFrame(nsIFrame* aFrame, PRUint8 aBreakType)
{
  PRUint8 floatType = aFrame->GetStyleDisplay()->mFloats;
  PRBool result;
  switch (aBreakType) {
    case NS_STYLE_CLEAR_LEFT_AND_RIGHT:
      result = PR_TRUE;
      break;
    case NS_STYLE_CLEAR_LEFT:
      result = floatType == NS_STYLE_FLOAT_LEFT;
      break;
    case NS_STYLE_CLEAR_RIGHT:
      result = floatType == NS_STYLE_FLOAT_RIGHT;
      break;
    default:
      result = PR_FALSE;
  }
  return result;
}

nscoord
nsSpaceManager::ClearFloats(nscoord aY, PRUint8 aBreakType)
{
  nscoord bottom = aY + mY;

  for (FrameInfo *frame = mFrameInfoMap; frame; frame = frame->mNext) {
    if (ShouldClearFrame(frame->mFrame, aBreakType)) {
      if (frame->mRect.YMost() > bottom) {
        bottom = frame->mRect.YMost();
      }
    }
  }

  bottom -= mY;

  return bottom;
}

/////////////////////////////////////////////////////////////////////////////
// FrameInfo

MOZ_DECL_CTOR_COUNTER(nsSpaceManager::FrameInfo)

nsSpaceManager::FrameInfo::FrameInfo(nsIFrame* aFrame, const nsRect& aRect)
  : mFrame(aFrame), mRect(aRect), mNext(0)
{
  MOZ_COUNT_CTOR(nsSpaceManager::FrameInfo);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsSpaceManager::FrameInfo::~FrameInfo()
{
  MOZ_COUNT_DTOR(nsSpaceManager::FrameInfo);
}
#endif

/////////////////////////////////////////////////////////////////////////////
// BandRect

MOZ_DECL_CTOR_COUNTER(BandRect)

nsSpaceManager::BandRect::BandRect(nscoord    aLeft,
                                   nscoord    aTop,
                                   nscoord    aRight,
                                   nscoord    aBottom,
                                   nsIFrame*  aFrame)
{
  MOZ_COUNT_CTOR(BandRect);
  mLeft = aLeft;
  mTop = aTop;
  mRight = aRight;
  mBottom = aBottom;
  mFrame = aFrame;
  mNumFrames = 1;
}

nsSpaceManager::BandRect::BandRect(nscoord      aLeft,
                                   nscoord      aTop,
                                   nscoord      aRight,
                                   nscoord      aBottom,
                                   nsVoidArray* aFrames)
{
  MOZ_COUNT_CTOR(BandRect);
  mLeft = aLeft;
  mTop = aTop;
  mRight = aRight;
  mBottom = aBottom;
  mFrames = new nsVoidArray;
  mFrames->operator=(*aFrames);
  mNumFrames = mFrames->Count();
}

nsSpaceManager::BandRect::~BandRect()
{
  MOZ_COUNT_DTOR(BandRect);
  if (mNumFrames > 1) {
    delete mFrames;
  }
}

nsSpaceManager::BandRect*
nsSpaceManager::BandRect::SplitVertically(nscoord aBottom)
{
  NS_PRECONDITION((aBottom > mTop) && (aBottom < mBottom), "bad argument");

  // Create a new band rect for the bottom part
  BandRect* bottomBandRect;
                                            
  if (mNumFrames > 1) {
    bottomBandRect = new BandRect(mLeft, aBottom, mRight, mBottom, mFrames);
  } else {
    bottomBandRect = new BandRect(mLeft, aBottom, mRight, mBottom, mFrame);
  }
                                           
  // This band rect becomes the top part, so adjust the bottom edge
  mBottom = aBottom;
  return bottomBandRect;
}

nsSpaceManager::BandRect*
nsSpaceManager::BandRect::SplitHorizontally(nscoord aRight)
{
  NS_PRECONDITION((aRight > mLeft) && (aRight < mRight), "bad argument");
  
  // Create a new band rect for the right part
  BandRect* rightBandRect;
                                            
  if (mNumFrames > 1) {
    rightBandRect = new BandRect(aRight, mTop, mRight, mBottom, mFrames);
  } else {
    rightBandRect = new BandRect(aRight, mTop, mRight, mBottom, mFrame);
  }
                                           
  // This band rect becomes the left part, so adjust the right edge
  mRight = aRight;
  return rightBandRect;
}

PRBool
nsSpaceManager::BandRect::IsOccupiedBy(const nsIFrame* aFrame) const
{
  PRBool  result;

  if (1 == mNumFrames) {
    result = (mFrame == aFrame);
  } else {
    PRInt32 count = mFrames->Count();

    result = PR_FALSE;
    for (PRInt32 i = 0; i < count; i++) {
      nsIFrame* f = (nsIFrame*)mFrames->ElementAt(i);

      if (f == aFrame) {
        result = PR_TRUE;
        break;
      }
    }
  }

  return result;
}

void
nsSpaceManager::BandRect::AddFrame(const nsIFrame* aFrame)
{
  if (1 == mNumFrames) {
    nsIFrame* f = mFrame;
    mFrames = new nsVoidArray;
    mFrames->AppendElement(f);
  }

  mNumFrames++;
  mFrames->AppendElement((void*)aFrame);
  NS_POSTCONDITION(mFrames->Count() == mNumFrames, "bad frame count");
}

void
nsSpaceManager::BandRect::RemoveFrame(const nsIFrame* aFrame)
{
  NS_PRECONDITION(mNumFrames > 1, "only one frame");
  mFrames->RemoveElement((void*)aFrame);
  mNumFrames--;

  if (1 == mNumFrames) {
    nsIFrame* f = (nsIFrame*)mFrames->ElementAt(0);

    delete mFrames;
    mFrame = f;
  }
}

PRBool
nsSpaceManager::BandRect::HasSameFrameList(const BandRect* aBandRect) const
{
  PRBool  result;

  // Check whether they're occupied by the same number of frames
  if (mNumFrames != aBandRect->mNumFrames) {
    result = PR_FALSE;
  } else if (1 == mNumFrames) {
    result = (mFrame == aBandRect->mFrame);
  } else {
    result = PR_TRUE;

    // For each frame occupying this band rect check whether it also occupies
    // aBandRect
    PRInt32 count = mFrames->Count();
    for (PRInt32 i = 0; i < count; i++) {
      nsIFrame* f = (nsIFrame*)mFrames->ElementAt(i);

      if (-1 == aBandRect->mFrames->IndexOf(f)) {
        result = PR_FALSE;
        break;
      }
    }
  }

  return result;
}

/**
 * Internal helper function that counts the number of rects in this band
 * including the current band rect
 */
PRInt32
nsSpaceManager::BandRect::Length() const
{
  PRInt32   len = 1;
  BandRect* bandRect = Next();

  // Because there's a header cell we know we'll either find the next band
  // (which has a different y-offset) or the header cell which has an invalid
  // y-offset
  while (bandRect->mTop == mTop) {
    len++;
    bandRect = bandRect->Next();
  }

  return len;
}


//----------------------------------------------------------------------

nsAutoSpaceManager::~nsAutoSpaceManager()
{
  // Restore the old space manager in the reflow state if necessary.
  if (mNew) {
#ifdef NOISY_SPACEMANAGER
    printf("restoring old space manager %p\n", mOld);
#endif

    mReflowState.mSpaceManager = mOld;

#ifdef NOISY_SPACEMANAGER
    if (mOld) {
      NS_STATIC_CAST(nsFrame *, mReflowState.frame)->ListTag(stdout);
      printf(": space-manager %p after reflow\n", mOld);
      mOld->List(stdout);
    }
#endif

#ifdef DEBUG
    if (mOwns)
#endif
      delete mNew;
  }
}

nsresult
nsAutoSpaceManager::CreateSpaceManagerFor(nsPresContext *aPresContext, nsIFrame *aFrame)
{
  // Create a new space manager and install it in the reflow
  // state. `Remember' the old space manager so we can restore it
  // later.
  mNew = new nsSpaceManager(aPresContext->PresShell(), aFrame);
  if (! mNew)
    return NS_ERROR_OUT_OF_MEMORY;

#ifdef NOISY_SPACEMANAGER
  printf("constructed new space manager %p (replacing %p)\n",
         mNew, mReflowState.mSpaceManager);
#endif

  // Set the space manager in the existing reflow state
  mOld = mReflowState.mSpaceManager;
  mReflowState.mSpaceManager = mNew;
  return NS_OK;
}
