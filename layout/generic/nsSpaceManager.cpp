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
 * Internal function that returns the available space within the band
 *
 * @param aBand the first rect in the band
 * @param aIndex aBand's index in the rect array
 * @param aY the y-offset in world coordinates
 * @param aMaxSize the size to use to constrain the band data
 * @param aAvailableBand
 */
PRInt32 SpaceManager::GetBandAvailableSpace(const nsRect* aBand,
                                            PRInt32       aIndex,
                                            nscoord       aY,
                                            const nsSize& aMaxSize,
                                            nsBandData&   aAvailableSpace) const
{
  PRInt32 numRects = LengthOfBand(aBand, aIndex);
  nscoord localY = aY - mY;
  nscoord height = PR_MIN(aBand->YMost() - aY, aMaxSize.height);
  nsRect* rect = aAvailableSpace.rects;
  nscoord rightEdge = mX + aMaxSize.width;

  // Initialize the band data
  aAvailableSpace.count = 0;
  rect->x = mX;

  // Skip any rectangles that are to the left of the local coordinate space
  while (numRects > 0) {
    if (aBand->XMost() >= mX) {
      break;
    }

    // Get the next rect in the band
    aBand++;
    numRects--;
  }

  // Process all the remaining rectangles that are within the clip width
  while ((numRects > 0) && (aBand->x < rightEdge)) {
    if (aBand->x > rect->x) {
      // We found some available space
      rect->width = aBand->x - rect->x;
      rect->x -= mX;  // convert from world to local coordinates
      rect->y = localY;
      rect->height = height;
  
      // Move to the next output rect
      rect++;
      aAvailableSpace.count++;
    }

    rect->x = aBand->XMost();

    // Move to the next rect in the band
    numRects--;
    aBand++;
  }

  // No more rects left in the band. If we haven't yet reached the right edge
  // then all the remaining space is available
  if (rect->x < rightEdge) {
    rect->width = rightEdge - rect->x;
    rect->x -= mX;  // convert from world to local coordinates
    rect->y = localY;
    rect->height = height;
    aAvailableSpace.count++;
  }

  return aAvailableSpace.count;
}

PRInt32 SpaceManager::GetBandData(nscoord       aYOffset,
                                  const nsSize& aMaxSize,
                                  nsBandData&   aAvailableSpace) const
{
  // Convert the y-offset to world coordinates
  nscoord y = mY + aYOffset;

  // If there are no unavailable rects or the offset is below the bottommost
  // band then all the space is available
  if ((0 == mRectArray.mCount) || (y >= mRectArray.YMost())) {
    aAvailableSpace.count = 1;
    aAvailableSpace.rects[0].x = 0;
    aAvailableSpace.rects[0].y = aYOffset;
    aAvailableSpace.rects[0].width = aMaxSize.width;
    aAvailableSpace.rects[0].height = aMaxSize.height;
  } else {
    // Find the first band that contains the y-offset or is below the y-offset
    nsRect* band = mRectArray.mRects;

    for (PRInt32 i = 0; i < mRectArray.mCount; ) {
      if (band->y > y) {
        // The band is below the y-offset
        aAvailableSpace.count = 1;
        aAvailableSpace.rects[0].x = 0;
        aAvailableSpace.rects[0].y = aYOffset;
        aAvailableSpace.rects[0].width = aMaxSize.width;
        aAvailableSpace.rects[0].height = PR_MIN(band->y - y, aMaxSize.height);
        break;
      } else if (y < band->YMost()) {
        // The band contains the y-offset
        return GetBandAvailableSpace(band, i, y, aMaxSize, aAvailableSpace);
      } else {
        // Skip to the next band
        GetNextBand(band, i);
      }
    }
  }

  return aAvailableSpace.count;
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
PRBool SpaceManager::GetNextBand(nsRect*& aRect, PRInt32& aIndex) const
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
PRInt32 SpaceManager::LengthOfBand(const nsRect* aBand, PRInt32 aIndex) const
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
 * Tries to coalesce horizontally overlapping rectangles within a band
 *
 * @param aRect a rect in the band
 * @param aIndex aRect's index in the rect array
 */
void SpaceManager::CoalesceBand(nsRect* aRect, PRInt32 aIndex)
{
  while ((aIndex + 1) < mRectArray.mCount) {
    // Is there another rect in this band?
    nsRect* nextRect = aRect + 1;

    if (nextRect->y == aRect->y) {
      // Yes. Do the two rects overlap horizontally?
      if (aRect->XMost() > nextRect->x) {
        // Yes. Extend the right edge of aRect
        aRect->width = nextRect->XMost() - aRect->x;

        // Remove the next rect
        mRectArray.RemoveAt(aIndex + 1);
        aRect = mRectArray.mRects + aIndex;  // memory may have changed...

      } else {
        // No. We're all done
        break;
      }
    } else {
      // The next rect is part of a different band, so we're all done
      break;
    }
  }
}

/**
 * Divides the current band into two vertically
 *
 * @param aBand the first rect in the band
 * @param aIndex aBand's index in the rect array
 * @param aB1Height the height of the new band to create
 */
void SpaceManager::DivideBand(nsRect* aBand, PRInt32 aIndex, nscoord aB1Height)
{
  NS_PRECONDITION(aB1Height < aBand->height, "bad height");

  PRInt32 numRects = LengthOfBand(aBand, aIndex);
  nscoord aB2Height = aBand->height - aB1Height;

  // Index where we'll insert the new band
  PRInt32 insertAt = aIndex + numRects;

  while (numRects-- > 0) {
    // Insert a new bottom band
    nsRect  r(aBand->x, aBand->y + aB1Height, aBand->width, aB2Height);

    mRectArray.InsertAt(r, insertAt);
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
 * @param aRect the rect to add to the band
 * @returns PR_TRUE if successful and PR_FALSE if this is the last band
 */
void SpaceManager::AddRectToBand(nsRect*       aBand,
                                 PRInt32       aIndex,
                                 const nsRect& aRect)
{
  NS_PRECONDITION((aBand->y == aRect.y) && (aBand->height == aRect.height), "bad band");

  nscoord topOfBand = aBand->y;

  // Figure out where in the band to horizontally insert the rect. Try and
  // coalesce it with an existing rect if possible.
  do {
    // Compare the left edge of the new rect with the left edge of the existing
    // rect
    if (aRect.x <= aBand->x) {
      // The new rect's left edge is to the left of the existing rect's left
      // edge. Does the new rect overlap the existing rect?
      if (aRect.XMost() >= aBand->x) {
        // Yes. Extend the existing rect
        if (aRect.XMost() > aBand->XMost()) {
          aBand->x = aRect.x;
          aBand->width = aRect.width;

          // We're extending the right edge of the existing rect which may
          // cause the rect to overlap the remaining rects in the band
          CoalesceBand(aBand, aIndex);
        } else {
          aBand->width = aBand->XMost() - aRect.x;
          aBand->x = aRect.x;
        }
      } else {
        // No, it's completely to the left of the existing rect. Insert a new
        // rect
        mRectArray.InsertAt(aRect, aIndex);
      }
      return;

    } else if (aRect.x <= aBand->XMost()) {
      // The two rects are overlapping or adjoining. Extend the right edge of
      // the existing rect
      aBand->width = aRect.XMost() - aBand->x;

      // Since we extended the right edge of the existing rect it may now
      // overlap the remaining rects in the band
      CoalesceBand(aBand, aIndex);
      return;
    }

    // Move to the next rect in the band
    aBand++; aIndex++;
  } while ((aIndex < mRectArray.mCount) && (aBand->y == topOfBand));

  // Insert a new rect
  mRectArray.InsertAt(aRect, aIndex);
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
void SpaceManager::AddRectRegion(const nsRect& aUnavailableSpace)
{
  // Convert from local to world coordinates
  nsRect  rect(aUnavailableSpace.x + mX, aUnavailableSpace.y + mY,
               aUnavailableSpace.width, aUnavailableSpace.height);

  // If there are no existing bands or this rect is below the bottommost band,
  // then add a new band
  if ((0 == mRectArray.mCount) || (rect.y >= mRectArray.YMost())) {
      // Append a new bottommost band
      mRectArray.Append(rect);
      return;
  }

  // Examine each band looking for a band that intersects this rect
  nsRect* band = mRectArray.mRects;

  for (PRInt32 i = 0; i < mRectArray.mCount; ) {
    // Compare the top edge of this rect with the top edge of the band
    if (rect.y < band->y) {
      // The top edge of the rect is above the top edge of the band.
      // Is there any overlap?
      if (rect.YMost() <= band->y) {
        // Case #1. This rect is completely above the band, so insert a
        // new band
        mRectArray.InsertAt(rect, i);
        break;  // we're all done
      }

      // Case #2 and case #7. Divide this rect, creating a new rect for the
      // part that's above the band
      nsRect  r1(rect.x, rect.y, rect.width, band->y - rect.y);

      // Insert r1 as a new band
      mRectArray.InsertAt(r1, i);
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
      AddRectToBand(band, i, rect);
      break;

    } else {
      // Case #4 and case #7. The rect contains the band vertically. Divide
      // the rect, creating a new rect for the part that overlaps the band
      nsRect  r1(rect.x, rect.y, rect.width, band->YMost() - rect.y);

      // Add r1 to the band
      AddRectToBand(band, i, r1);
      band = mRectArray.mRects + i;  // memory may have changed...

      // Modify rect to be the part below the band
      rect.height = rect.YMost() - band->YMost();
      rect.y = band->YMost();

      // Continue with the next band
      if (!GetNextBand(band, i)) {
        // Append a new bottommost band
        mRectArray.Append(rect);
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

void SpaceManager::RectArray::Append(const nsRect& aRect)
{
  // Ensure there's enough capacity
  if (mCount >= mMax) {
    mMax += 8;
    mRects = (nsRect*)realloc(mRects, mMax * sizeof(nsRect));
  }

  mRects[mCount++] = aRect;
}

void SpaceManager::RectArray::InsertAt(const nsRect& aRect, PRInt32 aIndex)
{
  NS_PRECONDITION(aIndex <= mCount, "bad index");  // no holes in the array

  // Ensure there's enough capacity
  if (mCount >= mMax) {
    mMax += 8;
    mRects = (nsRect*)realloc(mRects, mMax * sizeof(nsRect));
  }

  memmove(&mRects[aIndex + 1], &mRects[aIndex], (mCount - aIndex) * sizeof(nsRect));
  mRects[aIndex] = aRect;
  mCount++;
}

void SpaceManager::RectArray::RemoveAt(PRInt32 aIndex)
{
  NS_PRECONDITION(aIndex < mCount, "bad index");

  mCount--;
  if (aIndex < mCount) {
    memmove(&mRects[aIndex], &mRects[aIndex + 1], (mCount - aIndex) * sizeof(nsRect));
  }
}

void SpaceManager::RectArray::Clear()
{
  mCount = 0;
}
