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
#include "nsSpaceManager.h"
#include "nsPoint.h"
#include "nsRect.h"
#include "nsSize.h"
#include <stdlib.h>

static NS_DEFINE_IID(kISpaceManagerIID, NS_ISPACEMANAGER_IID);

/////////////////////////////////////////////////////////////////////////////
// nsSpaceManager

SpaceManager::SpaceManager(nsIFrame* aFrame)
  : mFrame(aFrame)
{
  NS_INIT_REFCNT();
  mX = mY = 0;
}

NS_IMPL_ISUPPORTS(SpaceManager, kISpaceManagerIID);

nsIFrame* SpaceManager::GetFrame() const
{
  return mFrame;
}

void SpaceManager::Translate(nscoord aDx, nscoord aDy)
{
  mX += aDx;
  mY += aDy;
}

void SpaceManager::GetTranslation(nscoord& aX, nscoord& aY) const
{
  aX = mX;
  aY = mY;
}

nscoord SpaceManager::YMost() const
{
  if (mRectArray.mCount > 0) {
    return mRectArray.YMost();
  } else {
    return 0;
  }
}

/**
 * Internal function that returns the list of available and unavailable space
 * within the band
 *
 * @param aBand the first rect in the band
 * @param aIndex aBand's index in the rect array
 * @param aY the y-offset in world coordinates
 * @param aMaxSize the size to use to constrain the band data
 * @param aAvailableBand
 */
PRInt32 SpaceManager::GetBandAvailableSpace(const nsBandRect* aBand,
                                            PRInt32           aIndex,
                                            nscoord           aY,
                                            const nsSize&     aMaxSize,
                                            nsBandData&       aBandData) const
{
  PRInt32          numRects = LengthOfBand(aBand, aIndex);
  nscoord          localY = aY - mY;
  nscoord          height = PR_MIN(aBand->YMost() - aY, aMaxSize.height);
  nsBandTrapezoid* trapezoid = aBandData.trapezoids;
  nscoord          rightEdge = mX + aMaxSize.width;

  // Initialize the band data
  aBandData.count = 0;

  // Skip any rectangles that are to the left of the local coordinate space
  while (numRects > 0) {
    if (aBand->XMost() > mX) {
      break;
    }

    // Get the next rect in the band
    aBand++;
    numRects--;
  }

  // This is used to track the current x-location within the band. This is in
  // world coordinates
  nscoord   left = mX;

  // Process the remaining rectangles that are within the clip width
  while ((numRects > 0) && (aBand->x < rightEdge)) {
    // Compare the left edge of the occupied space with the current left
    // coordinate
    if (aBand->x > left) {
      // The rect is to the right of our current left coordinate, so we've
      // found some available space
      trapezoid->state = nsBandTrapezoid::smAvailable;
      trapezoid->frame = nsnull;

      // Assign the trapezoid a rectangular shape. The trapezoid must be in the
      // local coordinate space, so convert the current left coordinate
      *trapezoid = nsRect(left - mX, localY, aBand->x - left, height);

      // Move to the next output rect
      trapezoid++;
      aBandData.count++;
    }

    // The rect represents unavailable space, so add another trapezoid
    trapezoid->state = nsBandTrapezoid::smOccupied;
    trapezoid->frame = aBand->frame;

    nscoord x = aBand->x;
    // The first band can straddle the clip rect
    if (x < mX) {
      // Clip the left edge
      x = mX;
    }

    // Assign the trapezoid a rectangular shape. The trapezoid must be in the
    // local coordinate space, so convert the rects's left coordinate
    *trapezoid = nsRect(x - mX, localY, aBand->XMost() - x, height);

    // Move to the next output rect
    trapezoid++;
    aBandData.count++;

    // Adjust our current x-location within the band
    left = aBand->XMost();

    // Move to the next rect within the band
    numRects--;
    aBand++;
  }

  // No more rects left in the band. If we haven't yet reached the right edge,
  // then all the remaining space is available
  if (left < rightEdge) {
    trapezoid->state = nsBandTrapezoid::smAvailable;
    trapezoid->frame = nsnull;

    // Assign the trapezoid a rectangular shape. The trapezoid must be in the
    // local coordinate space, so convert the current left coordinate
    *trapezoid = nsRect(left - mX, localY, rightEdge - left, height);
    aBandData.count++;
  }

  return aBandData.count;
}

PRInt32 SpaceManager::GetBandData(nscoord       aYOffset,
                                  const nsSize& aMaxSize,
                                  nsBandData&   aBandData) const
{
  // Convert the y-offset to world coordinates
  nscoord y = mY + aYOffset;

  // If there are no unavailable rects or the offset is below the bottommost
  // band then all the space is available
  if ((0 == mRectArray.mCount) || (y >= mRectArray.YMost())) {
    // All the requested space is available
    aBandData.count = 1;
    aBandData.trapezoids[0] = nsRect(0, aYOffset, aMaxSize.width, aMaxSize.height);
    aBandData.trapezoids[0].state = nsBandTrapezoid::smAvailable;
    aBandData.trapezoids[0].frame = nsnull;
  } else {
    // Find the first band that contains the y-offset or is below the y-offset
    nsBandRect* band = mRectArray.mRects;

    for (PRInt32 i = 0; i < mRectArray.mCount; ) {
      if (band->y > y) {
        // The band is below the y-offset. The area between the y-offset and
        // the top of the band is available
        aBandData.count = 1;
        aBandData.trapezoids[0] =
          nsRect(0, aYOffset, aMaxSize.width, PR_MIN(band->y - y, aMaxSize.height));
        aBandData.trapezoids[0].state = nsBandTrapezoid::smAvailable;
        aBandData.trapezoids[0].frame = nsnull;
        break;
      } else if (y < band->YMost()) {
        // The band contains the y-offset. Return a list of available and
        // unavailable rects within the band
        return GetBandAvailableSpace(band, i, y, aMaxSize, aBandData);
      } else {
        // Skip to the next band
        GetNextBand(band, i);
      }
    }
  }

  return aBandData.count;
}

/**
 * Skips to the start of the next band.
 *
 * @param aRect <i>in out</i> paremeter. A rect in the band
 * @param aIndex <i>in out</i> parameter. aRect's index in the rect array
 * @returns PR_TRUE if successful and PR_FALSE if this is the last band.
 *          If successful aRect and aIndex are updated to point to the
 *          next band. If there is no next band then aRect is undefined
 *          and aIndex is set to the number of rects in the rect array
 */
PRBool SpaceManager::GetNextBand(nsBandRect*& aRect, PRInt32& aIndex) const
{
  nscoord topOfBand = aRect->y;

  while (++aIndex < mRectArray.mCount) {
    // Get the next rect and check whether it's part of the same band
    aRect++;

    if (aRect->y != topOfBand) {
      // We found the start of the next band
      return PR_TRUE;
    }
  }

  // No bands left
  return PR_FALSE;
}

/**
 * Returns the number of rectangles in the band
 *
 * @param aBand the first rect in the band
 * @param aIndex aBand's index in the rect array
 */
PRInt32 SpaceManager::LengthOfBand(const nsBandRect* aBand, PRInt32 aIndex) const
{
  nscoord topOfBand = aBand->y;
  PRInt32 result = 1;

  while (++aIndex < mRectArray.mCount) {
    // Get the next rect and check whether it's part of the same band
    aBand++;

    if (aBand->y != topOfBand) {
      // We found the start of the next band
      break;
    }

    result++;
  }

  return result;
}

/**
 * Tries to coalesce adjoining rectangles within a band. Returns PR_TRUE if any
 * of the rects are coalesced and PR_FALSE otherwise
 *
 * @param aRect the first rect in the band
 * @param aIndex aBand's index in the rect array
 */
PRBool SpaceManager::CoalesceBand(nsBandRect* aBand, PRInt32 aIndex)
{
  PRBool  result = PR_FALSE;

  while ((aIndex + 1) < mRectArray.mCount) {
    // Is there another rect in this band?
    nsBandRect* nextRect = aBand + 1;

    if (nextRect->y == aBand->y) {
      // The rects must not be overlapping
      NS_ASSERTION(nextRect->x >= aBand->XMost(), "overlapping rects");

      // Are the two rects adjoining and have the same tagged frame?
      if ((aBand->XMost() == nextRect->x) && (aBand->frame == nextRect->frame)) {
        // Yes. Extend the right edge of the current rect
        aBand->width = nextRect->XMost() - aBand->x;

        // Remove the next rect
        mRectArray.RemoveAt(aIndex + 1);
        aBand = mRectArray.mRects + aIndex;  // memory may have changed...

        // Continue through the loop trying to coalesce this rect
        result = PR_TRUE;

      } else {
        // No. Move to the next rect within the band
        aBand++;
        aIndex++;
      }
    } else {
      // The next rect is part of a different band, so we're all done
      break;
    }
  }

  return result;
}

/**
 * Divides the current band into two vertically
 *
 * @param aBand the first rect in the band
 * @param aIndex aBand's index in the rect array
 * @param aB1Height the height of the new band to create
 */
void SpaceManager::DivideBand(nsBandRect* aBand, PRInt32 aIndex, nscoord aB1Height)
{
  NS_PRECONDITION(aB1Height < aBand->height, "bad height");

  PRInt32 numRects = LengthOfBand(aBand, aIndex);
  nscoord aB2Height = aBand->height - aB1Height;

  // Index where we'll insert the new band
  PRInt32 insertAt = aIndex + numRects;

  while (numRects-- > 0) {
    // Insert a new bottom band
    nsRect  r(aBand->x, aBand->y + aB1Height, aBand->width, aB2Height);

    mRectArray.InsertAt(r, insertAt, aBand->frame);
    aBand = mRectArray.mRects + aIndex;  // memory may have changed...

    // Adjust the height of the top band
    aBand->height = aB1Height;

    // Move to the next rect in the band
    aBand++;
    insertAt++;
  }
}

/**
 * Adds a new rect to a band.
 *
 * @param aBand the first rect in the band
 * @param aIndex aBand's index in the rect array
 * @param aRect the rect to add to the band. It's in world coordinates
 * @returns PR_TRUE if successful and PR_FALSE if this is the last band
 */
void SpaceManager::AddRectToBand(nsBandRect*   aBand,
                                 PRInt32       aIndex,
                                 const nsRect& aRect,
                                 nsIFrame*     aFrame)
{
  NS_PRECONDITION((aBand->y == aRect.y) && (aBand->height == aRect.height), "bad band");

  nscoord topOfBand = aBand->y;
  nsRect  rect(aRect);

  // Figure out where in the band horizontally to insert the rect. Try and
  // coalesce it with an existing rect if possible. We can only do this if
  // they're tagged with the same frame
  do {
    // Compare the left edge of the new rect with the left edge of the existing
    // rect
    if (rect.x < aBand->x) {
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
      if (rect.XMost() <= aBand->x) {
        // No, the new rect is completely to the left of the existing rect
        // (case #1). Insert a new rect
        mRectArray.InsertAt(rect, aIndex, aFrame);
        return;
      }

      // Yes, they overlap. Insert a new rect for the part that's to the left
      // of the existing rect
      nsRect  r1(rect.x, rect.y, aBand->x - rect.x, rect.height);

      mRectArray.InsertAt(r1, aIndex, aFrame);
      aIndex++;
      aBand = mRectArray.mRects + aIndex;  // memory may have changed...

      // Adjust rect to reflect the overlap
      rect.width = rect.XMost() - aBand->x;
      rect.x = aBand->x;
      
    } else if (rect.x > aBand->x) {
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
      if (rect.x >= aBand->XMost()) {
        // The new rect is to the right of the existing rect (case #4), so move
        // to the next rect in the band
        aBand++; aIndex++;
        continue;
      }

      // The rects overlap, so divide the existing rect into two rects: the
      // part to the left of the new rect, and the part that overlaps
      nsRect  r1(rect.x, aBand->y, aBand->XMost() - rect.x, aBand->height);

      // Modify the left half of the existing rect
      aBand->width = rect.x - aBand->x;

      // Insert the new right half of the existing rect, and make it the current
      // rect
      aIndex++;
      mRectArray.InsertAt(r1, aIndex, aBand->frame);
      aBand = mRectArray.mRects + aIndex;  // memory may have changed...
    }

    // At this point the left edge of the new rect is the same as the left edge
    // of the existing rect
    NS_ASSERTION(rect.x == aBand->x, "unexpected rect");

    // Compare which rect is wider, the new rect or the existing rect
    if (aBand->width > rect.width) {
      // The existing rect is wider (cases 2 and 6). Divide the existing rect
      // into two rects: the part that overlaps, and the part to the right of
      // the new rect
      nsRect  r1(rect.XMost(), aBand->y, aBand->XMost() - rect.XMost(), aBand->height);

      // Modify the left half of the existing rect
      aBand->width = r1.x - aBand->x;

      // Insert the new right half of the existing rect
      mRectArray.InsertAt(r1, aIndex, aBand->frame);
      aBand = mRectArray.mRects + aIndex;  // memory may have changed...
    }

    // XXX Mark the existing rect as being shared by the two frames...

    if (rect.width == aBand->width) {
      // We're all done
      return;
    }

    // The new rect is wider than the existing rect (cases 3 and 5). Set rect
    // to be the overhang and move to the next rect within the band
    rect.width = rect.XMost() - aBand->XMost();
    rect.x = aBand->XMost();
    aBand++; aIndex++;
  } while ((aIndex < mRectArray.mCount) && (aBand->y == topOfBand));

  // Insert a new rect
  mRectArray.InsertAt(aRect, aIndex, aFrame);
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
void SpaceManager::AddRectRegion(const nsRect& aUnavailableSpace, nsIFrame* aFrame)
{
  // Convert from local to world coordinates
  nsRect  rect(aUnavailableSpace.x + mX, aUnavailableSpace.y + mY,
               aUnavailableSpace.width, aUnavailableSpace.height);

  // If there are no existing bands or this rect is below the bottommost band,
  // then add a new band
  if ((0 == mRectArray.mCount) || (rect.y >= mRectArray.YMost())) {
      // Append a new bottommost band
      mRectArray.Append(rect, aFrame);
      return;
  }

  // Examine each band looking for a band that intersects this rect
  nsBandRect* band = mRectArray.mRects;

  for (PRInt32 i = 0; i < mRectArray.mCount; ) {
    // Compare the top edge of this rect with the top edge of the band
    if (rect.y < band->y) {
      // The top edge of the rect is above the top edge of the band.
      // Is there any overlap?
      if (rect.YMost() <= band->y) {
        // Case #1. This rect is completely above the band, so insert a
        // new band
        mRectArray.InsertAt(rect, i, aFrame);
        break;  // we're all done
      }

      // Case #2 and case #7. Divide this rect, creating a new rect for the
      // part that's above the band
      nsRect  r1(rect.x, rect.y, rect.width, band->y - rect.y);

      // Insert r1 as a new band
      mRectArray.InsertAt(r1, i, aFrame);
      i++;
      band = mRectArray.mRects + i;  // memory may have changed...

      // Modify rect to exclude the part above the band
      rect.height = rect.YMost() - band->y;
      rect.y = band->y;

    } else if (rect.y > band->y) {
      // The top edge of the rect is below the top edge of the band. Is there
      // any overlap?
      if (rect.y >= band->YMost()) {
        // Case #5. This rect is below the current band. Skip to the next band
        GetNextBand(band, i);
        continue;
      }

      // Case #3 and case #4. Divide the current band into two bands with the
      // top band being the part that's above the rect
      DivideBand(band, i, rect.y - band->y);
      band = mRectArray.mRects + i;  // memory may have changed...

      // Skip to the bottom band that we just created
      GetNextBand(band, i);
    }

    // At this point the rect and the band should have the same y-offset
    NS_ASSERTION(rect.y == band->y, "unexpected band");

    // Is the band higher than the rect?
    if (band->height > rect.height) {
      // Divide the band into two bands with the top band the same height
      // as the rect
      DivideBand(band, i, rect.height);
      band = mRectArray.mRects + i;  // memory may have changed...
    }

    if (rect.height == band->height) {
      // Add the rect to the band
      AddRectToBand(band, i, rect, aFrame);
      break;

    } else {
      // Case #4 and case #7. The rect contains the band vertically. Divide
      // the rect, creating a new rect for the part that overlaps the band
      nsRect  r1(rect.x, rect.y, rect.width, band->YMost() - rect.y);

      // Add r1 to the band
      AddRectToBand(band, i, r1, aFrame);
      band = mRectArray.mRects + i;  // memory may have changed...

      // Modify rect to be the part below the band
      rect.height = rect.YMost() - band->YMost();
      rect.y = band->YMost();

      // Continue with the next band
      if (!GetNextBand(band, i)) {
        // Append a new bottommost band
        mRectArray.Append(rect, aFrame);
        break;
      }
    }
  }
}

void SpaceManager::ClearRegions()
{
  mRectArray.Clear();
}

/////////////////////////////////////////////////////////////////////////////
// RectArray

SpaceManager::RectArray::RectArray()
{
  mRects = nsnull;
  mCount = mMax = 0;
}

SpaceManager::RectArray::~RectArray()
{
  if (nsnull != mRects) {
    free(mRects);
  }
}

nscoord SpaceManager::RectArray::YMost() const
{
  return mRects[mCount - 1].YMost();
}

void SpaceManager::RectArray::Append(const nsRect& aRect, nsIFrame* aFrame)
{
  // Ensure there's enough capacity
  if (mCount >= mMax) {
    mMax += 8;
    mRects = (nsBandRect*)realloc(mRects, mMax * sizeof(nsBandRect));
  }

  mRects[mCount].x = aRect.x;
  mRects[mCount].y = aRect.y;
  mRects[mCount].width = aRect.width;
  mRects[mCount].height = aRect.height;
  mRects[mCount].frame = aFrame;
  mCount++;
}

void SpaceManager::RectArray::InsertAt(const nsRect& aRect,
                                       PRInt32       aIndex,
                                       nsIFrame*     aFrame)
{
  NS_PRECONDITION(aIndex <= mCount, "bad index");  // no holes in the array

  // Ensure there's enough capacity
  if (mCount >= mMax) {
    mMax += 8;
    mRects = (nsBandRect*)realloc(mRects, mMax * sizeof(nsBandRect));
  }

  memmove(&mRects[aIndex + 1], &mRects[aIndex], (mCount - aIndex) * sizeof(nsBandRect));
  mRects[aIndex].x = aRect.x;
  mRects[aIndex].y = aRect.y;
  mRects[aIndex].width = aRect.width;
  mRects[aIndex].height = aRect.height;
  mRects[aIndex].frame = aFrame;
  mCount++;
}

void SpaceManager::RectArray::RemoveAt(PRInt32 aIndex)
{
  NS_PRECONDITION(aIndex < mCount, "bad index");

  mCount--;
  if (aIndex < mCount) {
    memmove(&mRects[aIndex], &mRects[aIndex + 1], (mCount - aIndex) * sizeof(nsBandRect));
  }
}

void SpaceManager::RectArray::Clear()
{
  mCount = 0;
}
