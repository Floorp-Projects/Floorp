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
#include "nsVoidArray.h"

static NS_DEFINE_IID(kISpaceManagerIID, NS_ISPACEMANAGER_IID);

PR_CALLBACK PLHashNumber
NS_HashNumber(const void* key)
{
  return (PLHashNumber)key;
}

PR_CALLBACK PRIntn
NS_RemoveFrameInfoEntries(PLHashEntry* he, PRIntn i, void* arg)
{
  SpaceManager::BandRect* bandRect = (SpaceManager::BandRect*)he->value;

  NS_ASSERTION(nsnull != bandRect, "null band rect");
  delete bandRect;
  return HT_ENUMERATE_REMOVE;
}

/////////////////////////////////////////////////////////////////////////////
// nsSpaceManager

SpaceManager::SpaceManager(nsIFrame* aFrame)
  : mFrame(aFrame)
{
  NS_INIT_REFCNT();
  PR_INIT_CLIST(&mBandList);
  mX = mY = 0;
  mFrameInfoMap = nsnull;
}

void SpaceManager::ClearFrameInfo()
{
  if (nsnull != mFrameInfoMap) {
    PL_HashTableEnumerateEntries(mFrameInfoMap, NS_RemoveFrameInfoEntries, 0);
    PL_HashTableDestroy(mFrameInfoMap);
    mFrameInfoMap = nsnull;
  }
}

void SpaceManager::ClearBandRects()
{
  if (!PR_CLIST_IS_EMPTY(&mBandList)) {
    BandRect* bandRect = (BandRect*)PR_LIST_HEAD(&mBandList);
  
    while (bandRect != &mBandList) {
      BandRect* next = (BandRect*)PR_NEXT_LINK(bandRect);
  
      delete bandRect;
      bandRect = next;
    }
  
    PR_INIT_CLIST(&mBandList);
  }
}

SpaceManager::~SpaceManager()
{
  ClearFrameInfo();
  ClearBandRects();
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
  nscoord yMost;

  if (PR_CLIST_IS_EMPTY(&mBandList)) {
    yMost = 0;
  } else {
    BandRect* lastRect = (BandRect*)PR_LIST_TAIL(&mBandList);

    yMost = lastRect->bottom;
  }

  return yMost;
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
PRInt32 SpaceManager::GetBandAvailableSpace(const BandRect* aBand,
                                            nscoord         aY,
                                            const nsSize&   aMaxSize,
                                            nsBandData&     aBandData) const
{
  nscoord          topOfBand = aBand->top;
  nscoord          localY = aY - mY;
  nscoord          height = PR_MIN(aBand->bottom - aY, aMaxSize.height);
  nsBandTrapezoid* trapezoid = aBandData.trapezoids;
  nscoord          rightEdge = mX + aMaxSize.width;

  // Initialize the band data
  aBandData.count = 0;

  // Skip any rectangles that are to the left of the local coordinate space
  while (aBand->top == topOfBand) {
    if (aBand->right > mX) {
      break;
    }

    // Get the next rect in the band
    aBand = (BandRect*)PR_NEXT_LINK(aBand);
  }

  // This is used to track the current x-location within the band. This is in
  // world coordinates
  nscoord   left = mX;

  // Process the remaining rectangles that are within the clip width
  while ((aBand->top == topOfBand) && (aBand->left < rightEdge)) {
    // Compare the left edge of the occupied space with the current left
    // coordinate
    if (aBand->left > left) {
      // The rect is to the right of our current left coordinate, so we've
      // found some available space
      trapezoid->state = nsBandTrapezoid::smAvailable;
      trapezoid->frame = nsnull;

      // Assign the trapezoid a rectangular shape. The trapezoid must be in the
      // local coordinate space, so convert the current left coordinate
      *trapezoid = nsRect(left - mX, localY, aBand->left - left, height);

      // Move to the next output rect
      trapezoid++;
      aBandData.count++;
    }

    // The rect represents unavailable space, so add another trapezoid
    if (1 == aBand->numFrames) {
      trapezoid->state = nsBandTrapezoid::smOccupied;
      trapezoid->frame = aBand->frame;
    } else {
      NS_ASSERTION(aBand->numFrames > 1, "unexpected frame count");
      trapezoid->state = nsBandTrapezoid::smOccupiedMultiple;
      trapezoid->frames = aBand->frames;
    }

    nscoord x = aBand->left;
    // The first band can straddle the clip rect
    if (x < mX) {
      // Clip the left edge
      x = mX;
    }

    // Assign the trapezoid a rectangular shape. The trapezoid must be in the
    // local coordinate space, so convert the rects's left coordinate
    *trapezoid = nsRect(x - mX, localY, aBand->right - x, height);

    // Move to the next output rect
    trapezoid++;
    aBandData.count++;

    // Adjust our current x-location within the band
    left = aBand->right;

    // Move to the next rect within the band
    aBand = (BandRect*)PR_NEXT_LINK(aBand);
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
  nscoord   y = mY + aYOffset;

  // If there are no unavailable rects or the offset is below the bottommost
  // band, then all the space is available
  if (y >= YMost()) {
    // All the requested space is available
    aBandData.count = 1;
    aBandData.trapezoids[0] = nsRect(0, aYOffset, aMaxSize.width, aMaxSize.height);
    aBandData.trapezoids[0].state = nsBandTrapezoid::smAvailable;
    aBandData.trapezoids[0].frame = nsnull;
  } else {
    // Find the first band that contains the y-offset or is below the y-offset
    BandRect* band = (BandRect*)PR_LIST_HEAD(&mBandList);
    NS_ASSERTION(band != &mBandList, "no bands");

    aBandData.count = 0;
    while (nsnull != band) {
      if (band->top > y) {
        // The band is below the y-offset. The area between the y-offset and
        // the top of the band is available
        aBandData.count = 1;
        aBandData.trapezoids[0] =
          nsRect(0, aYOffset, aMaxSize.width, PR_MIN(band->top - y, aMaxSize.height));
        aBandData.trapezoids[0].state = nsBandTrapezoid::smAvailable;
        aBandData.trapezoids[0].frame = nsnull;
        break;
      } else if (y < band->bottom) {
        // The band contains the y-offset. Return a list of available and
        // unavailable rects within the band
        return GetBandAvailableSpace(band, y, aMaxSize, aBandData);
      } else {
        // Skip to the next band
        band = GetNextBand(band);
      }
    }
  }

  NS_POSTCONDITION(aBandData.count > 0, "unexpected band data count");
  return aBandData.count;
}

/**
 * Skips to the start of the next band.
 *
 * @param aBandRect A rect within the band
 * @returns The start of the next band, or nsnull of this is the last band.
 */
SpaceManager::BandRect* SpaceManager::GetNextBand(const BandRect* aBandRect) const
{
  nscoord topOfBand = aBandRect->top;

  aBandRect = (BandRect*)PR_NEXT_LINK(aBandRect);
  while (aBandRect != &mBandList) {
    // Check whether this rect is part of the same band
    if (aBandRect->top != topOfBand) {
      // We found the start of the next band
      return (BandRect*)aBandRect;
    }

    aBandRect = (BandRect*)PR_NEXT_LINK(aBandRect);
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
void SpaceManager::DivideBand(BandRect* aBandRect, nscoord aBottom)
{
  NS_PRECONDITION(aBottom < aBandRect->bottom, "bad height");
  nscoord   topOfBand = aBandRect->top;
  BandRect* nextBand = GetNextBand(aBandRect);

  if (nsnull == nextBand) {
    nextBand = (BandRect*)&mBandList;
  }

  while (topOfBand == aBandRect->top) {
    // Split the band rect into two vertically
    BandRect* bottomBandRect = aBandRect->SplitVertically(aBottom);

    // Insert the new bottom part
    PR_INSERT_BEFORE(bottomBandRect, nextBand);

    // Move to the next rect in the band
    aBandRect = (BandRect*)PR_NEXT_LINK(aBandRect);
  }
}

/**
 * Tries to join the two adjacent bands. Returns PR_TRUE if successful and
 * PR_FALSE otherwise
 *
 * If the two bands are joined the previous band is the the band that's deleted
 */
PRBool SpaceManager::JoinBands(BandRect* aBandRect, BandRect* aPrevBand)
{
  NS_NOTYETIMPLEMENTED("joining bands");
  return PR_FALSE;
}

/**
 * Adds a new rect to a band.
 *
 * @param aBand the first rect in the band
 * @param aBandRect the band rect to add to the band
 */
void SpaceManager::AddRectToBand(BandRect*  aBand,
                                 BandRect*  aBandRect)
{
  NS_PRECONDITION((aBand->top == aBandRect->top) &&
                  (aBand->bottom == aBandRect->bottom), "bad band");
  NS_PRECONDITION(1 == aBandRect->numFrames, "shared band rect");
  nscoord topOfBand = aBand->top;

  // Figure out where in the band horizontally to insert the rect
  do {
    // Compare the left edge of the new rect with the left edge of the existing
    // rect
    if (aBandRect->left < aBand->left) {
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
      if (aBandRect->right <= aBand->left) {
        // No, the new rect is completely to the left of the existing rect
        // (case #1). Insert a new rect
        PR_INSERT_BEFORE(aBandRect, aBand);
        return;
      }

      // Yes, they overlap. Compare the right edges.
      if (aBandRect->right > aBand->right) {
        // The new rect's right edge is to the right of the existing rect's
        // right edge (case #3). Split the new rect
        BandRect* r1 = aBandRect->SplitHorizontally(aBand->left);

        // Insert the part of the new rect that's to the left of the existing
        // rect as a new band rect
        PR_INSERT_BEFORE(aBandRect, aBand);
        aBandRect = r1;

      } else {
        if (aBand->right > aBandRect->right) {
          // The existing rect extends past the new rect (case #2). Split the
          // existing rect
          BandRect* r1 = aBand->SplitHorizontally(aBandRect->right);

          // Insert the new right half of the existing rect
          PR_INSERT_AFTER(r1, aBand);
        }

        // Insert the part of the new rect that's to the left of the existing
        // rect
        aBandRect->right = aBand->left;
        PR_INSERT_BEFORE(aBandRect, aBand);

        // Mark the part of the existing rect that overlaps as being shared
        aBand->AddFrame(aBandRect->frame);
        return;
      }
    }
      
    if (aBandRect->left > aBand->left) {
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
      if (aBandRect->left >= aBand->right) {
        // The new rect is to the right of the existing rect (case #4), so move
        // to the next rect in the band
        aBand = (BandRect*)PR_NEXT_LINK(aBand);
        continue;
      }

      // The rects overlap, so divide the existing rect into two rects: the
      // part to the left of the new rect, and the part that overlaps
      BandRect* r1 = aBand->SplitHorizontally(aBandRect->left);

      // Insert the new right half of the existing rect, and make it the current
      // rect
      PR_INSERT_AFTER(r1, aBand);
      aBand = r1;
    }

    // At this point the left edge of the new rect is the same as the left edge
    // of the existing rect
    NS_ASSERTION(aBandRect->left == aBand->left, "unexpected rect");

    // Compare which rect is wider, the new rect or the existing rect
    if (aBand->right > aBandRect->right) {
      // The existing rect is wider (case #6). Divide the existing rect into
      // two rects: the part that overlaps, and the part to the right of the
      // new rect
      BandRect* r1 = aBand->SplitHorizontally(aBandRect->right);

      // Insert the new right half of the existing rect
      PR_INSERT_AFTER(r1, aBand);

      // Mark the overlap as being shared
      aBand->AddFrame(aBandRect->frame);
      return;

    } else {
      // Indicate the frames share the existing rect
      aBand->AddFrame(aBandRect->frame);

      if (aBand->right == aBandRect->right) {
        // The new and existing rect have the same right edge. We're all done,
        // and the new band rect is no longer needed
        delete aBandRect;
        return;
      } else {
        // The new rect is wider than the existing rect (cases #5). Set the
        // new rect to be the overhang, and move to the next rect within the band
        aBandRect->left = aBand->right;
        aBand = (BandRect*)PR_NEXT_LINK(aBand);
        continue;
      }
    }
  } while ((aBand != &mBandList) && (aBand->top == topOfBand));

  // Insert a new rect
  PR_INSERT_BEFORE(aBandRect, aBand);
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
void SpaceManager::InsertBandRect(BandRect* aBandRect)
{
  // If there are no existing bands or this rect is below the bottommost
  // band, then add a new band
  if (aBandRect->top >= YMost()) {
    PR_APPEND_LINK(aBandRect, &mBandList);
    return;
  }

  // Examine each band looking for a band that intersects this rect
  BandRect* band = (BandRect*)PR_LIST_HEAD(&mBandList);
  NS_ASSERTION(nsnull != band, "no bands");

  while (nsnull != band) {
    // Compare the top edge of this rect with the top edge of the band
    if (aBandRect->top < band->top) {
      // The top edge of the rect is above the top edge of the band.
      // Is there any overlap?
      if (aBandRect->bottom <= band->top) {
        // Case #1. This rect is completely above the band, so insert a
        // new band before the current band
        PR_INSERT_BEFORE(aBandRect, band);
        break;  // we're all done
      }

      // Case #2 and case #7. Divide this rect, creating a new rect for
      // the part that's above the band
      BandRect* bandRect1 = new BandRect(aBandRect->left, aBandRect->top,
                                         aBandRect->right, band->top,
                                         aBandRect->frame);

      // Insert bandRect1 as a new band
      PR_INSERT_BEFORE(bandRect1, band);

      // Modify this rect to exclude the part above the band
      aBandRect->top = band->top;

    } else if (aBandRect->top > band->top) {
      // The top edge of the rect is below the top edge of the band. Is there
      // any overlap?
      if (aBandRect->top >= band->bottom) {
        // Case #5. This rect is below the current band. Skip to the next band
        band = GetNextBand(band);
        continue;
      }

      // Case #3 and case #4. Divide the current band into two bands with the
      // top band being the part that's above the rect
      DivideBand(band, aBandRect->top);

      // Skip to the bottom band that we just created
      band = GetNextBand(band);
    }

    // At this point the rect and the band should have the same y-offset
    NS_ASSERTION(aBandRect->top == band->top, "unexpected band");

    // Is the band higher than the rect?
    if (band->bottom > aBandRect->bottom) {
      // Divide the band into two bands with the top band the same height
      // as the rect
      DivideBand(band, aBandRect->bottom);
    }

    if (aBandRect->bottom == band->bottom) {
      // Add the rect to the band
      AddRectToBand(band, aBandRect);
      break;

    } else {
      // Case #4 and case #7. The rect contains the band vertically. Divide
      // the rect, creating a new rect for the part that overlaps the band
      BandRect* bandRect1 = new BandRect(aBandRect->left, aBandRect->top,
                                         aBandRect->right, band->bottom,
                                         aBandRect->frame);

      // Add bandRect1 to the band
      AddRectToBand(band, bandRect1);

      // Modify aBandRect to be the part below the band
      aBandRect->top = band->bottom;

      // Continue with the next band
      band = GetNextBand(band);
      if (nsnull == band) {
        // Append a new bottommost band
        PR_APPEND_LINK(aBandRect, &mBandList);
        break;
      }
    }
  }
}

PRBool SpaceManager::AddRectRegion(nsIFrame* aFrame, const nsRect& aUnavailableSpace)
{
  NS_PRECONDITION(nsnull != aFrame, "null frame");

  // See if there is already a region associated with aFrame
  FrameInfo*  frameInfo = GetFrameInfoFor(aFrame);

  if (nsnull != frameInfo) {
    NS_WARNING("aFrame is already associated with a region");
    return PR_FALSE;
  }

  // Convert the frame to world coordinates
  nsRect  rect(aUnavailableSpace.x + mX, aUnavailableSpace.y + mY,
               aUnavailableSpace.width, aUnavailableSpace.height);

  // Create a frame info structure
  frameInfo = CreateFrameInfo(aFrame, rect);

  // Is the rect empty?
  if (aUnavailableSpace.IsEmpty()) {
    // The rect doesn't consume any space, so don't add any band data
    return PR_TRUE;
  }

  // Allocate a band rect
  BandRect* bandRect = new BandRect(rect.x, rect.y, rect.XMost(), rect.YMost(), aFrame);

  // Insert the band rect
  InsertBandRect(bandRect);
  return PR_TRUE;
}

PRBool SpaceManager::ResizeRectRegion(nsIFrame*    aFrame,
                                      nscoord      aDeltaWidth,
                                      nscoord      aDeltaHeight,
                                      AffectedEdge aEdge)
{
  NS_NOTYETIMPLEMENTED("resizing a region");
  return PR_FALSE;
}

PRBool SpaceManager::OffsetRegion(nsIFrame* aFrame, nscoord dx, nscoord dy)
{
  NS_NOTYETIMPLEMENTED("offseting a region");
  return PR_FALSE;
}

PRBool SpaceManager::RemoveRegion(nsIFrame* aFrame)
{
  // Get the frame info associated with with aFrame
  FrameInfo*  frameInfo = GetFrameInfoFor(aFrame);

  if (nsnull == frameInfo) {
    NS_WARNING("no region associated with aFrame");
    return PR_FALSE;
  }

  if (!frameInfo->rect.IsEmpty()) {
    BandRect* band = (BandRect*)PR_LIST_HEAD(&mBandList);
    BandRect* prevBand = nsnull;
    PRBool    prevFoundMatchingRect = PR_FALSE;
    NS_ASSERTION(band != &mBandList, "no band rects");

    // Iterate each band looking for rects tagged with aFrame
    while (nsnull != band) {
      BandRect* rect = band;
      BandRect* prevRect = nsnull;
      nscoord   topOfBand = band->top;
      PRBool    foundMatchingRect = PR_FALSE;
      PRBool    prevIsSharedRect = PR_FALSE;

      // Iterate each rect in the band
      do {
        PRBool  isSharedRect = PR_FALSE;

        if (rect->IsOccupiedBy(aFrame)) {
          if (rect->numFrames > 1) {
            // The band rect is occupied by more than one frame
            rect->RemoveFrame(aFrame);

            // Remember that this rect was being shared by more than one frame
            // including aFrame
            isSharedRect = PR_TRUE;
          } else {
            // The rect isn't shared so just delete it
            PR_REMOVE_LINK(rect);
          }

          // Remember that we found a matching rect in this band
          foundMatchingRect = PR_TRUE;
        }
           
        // We need to try and coalesce adjacent rects iff we find a shared rect
        // occupied by aFrame. If either this rect or the previous rect was
        // shared and occupied by aFrame, then try and coalesce this rect and
        // the previous rect
        if (prevIsSharedRect || (isSharedRect && (nsnull != prevRect))) {
          NS_ASSERTION(nsnull != prevRect, "no previous rect");
          if ((prevRect->right == rect->left) && (prevRect->HasSameFrameList(rect))) {
            // Modify the current rect's left edge, and delete the previous rect
            rect->left = prevRect->left;
            PR_REMOVE_LINK(prevRect);
            delete prevRect;
          }
        }

        // Get the next rect in the band
        prevRect = rect;
        prevIsSharedRect = isSharedRect;
        rect = (BandRect*)PR_NEXT_LINK(rect);

        if (rect == &mBandList) {
          // No bands left
          rect = nsnull;
          break;
        }
      } while (rect->top == topOfBand);

      // If we found a matching rect in this band or the previous band then try
      // join the two bands
      if (prevFoundMatchingRect || (foundMatchingRect && (nsnull != prevBand))) {
        // Try and join this band with the previous band
        NS_ASSERTION(nsnull != prevBand, "no previous band");
        JoinBands(band, prevBand);
      }

      // Move to the next band
      prevFoundMatchingRect = foundMatchingRect;
      band = rect;
    }
  }

  DestroyFrameInfo(frameInfo);
  return PR_TRUE;
}

void SpaceManager::ClearRegions()
{
  ClearFrameInfo();
  ClearBandRects();
}

SpaceManager::FrameInfo* SpaceManager::GetFrameInfoFor(nsIFrame* aFrame)
{
  FrameInfo*  result = nsnull;

  if (nsnull != mFrameInfoMap) {
    result = (FrameInfo*)PL_HashTableLookup(mFrameInfoMap, (const void*)aFrame);
  }

  return result;
}

SpaceManager::FrameInfo* SpaceManager::CreateFrameInfo(nsIFrame*     aFrame,
                                                       const nsRect& aRect)
{
  if (nsnull == mFrameInfoMap) {
    mFrameInfoMap = PL_NewHashTable(17, NS_HashNumber, PL_CompareValues,
                                    PL_CompareValues, nsnull, nsnull);
  }

  FrameInfo*  frameInfo = new FrameInfo(aFrame, aRect);

  PL_HashTableAdd(mFrameInfoMap, (const void*)aFrame, frameInfo);
  return frameInfo;
}

void SpaceManager::DestroyFrameInfo(FrameInfo* aFrameInfo)
{
  PL_HashTableRemove(mFrameInfoMap, (const void*)aFrameInfo);
  delete aFrameInfo;
}

/////////////////////////////////////////////////////////////////////////////
// FrameInfo

SpaceManager::FrameInfo::FrameInfo(nsIFrame* aFrame, const nsRect& aRect)
  : frame(aFrame), rect(aRect)
{
}

/////////////////////////////////////////////////////////////////////////////
// BandRect

SpaceManager::BandRect::BandRect(nscoord    aLeft,
                                 nscoord    aTop,
                                 nscoord    aRight,
                                 nscoord    aBottom,
                                 nsIFrame*  aFrame)
{
  left = aLeft;
  top = aTop;
  right = aRight;
  bottom = aBottom;
  frame = aFrame;
  numFrames = 1;
}

SpaceManager::BandRect::BandRect(nscoord      aLeft,
                                 nscoord      aTop,
                                 nscoord      aRight,
                                 nscoord      aBottom,
                                 nsVoidArray* aFrames)
{
  left = aLeft;
  top = aTop;
  right = aRight;
  bottom = aBottom;
  frames = new nsVoidArray;
  frames->operator=(*aFrames);
  numFrames = frames->Count();
}

SpaceManager::BandRect::~BandRect()
{
  if (numFrames > 1) {
    delete frames;
  }
}

SpaceManager::BandRect* SpaceManager::BandRect::SplitVertically(nscoord aBottom)
{
  NS_PRECONDITION((aBottom > top) && (aBottom < bottom), "bad argument");

  // Create a new band rect for the bottom part
  BandRect* bottomBandRect;
                                            
  if (numFrames > 1) {
    bottomBandRect = new BandRect(left, aBottom, right, bottom, frames);
  } else {
    bottomBandRect = new BandRect(left, aBottom, right, bottom, frame);
  }
                                           
  // This band rect becomes the top part, so adjust the bottom edge
  bottom = aBottom;

  return bottomBandRect;
}

SpaceManager::BandRect* SpaceManager::BandRect::SplitHorizontally(nscoord aRight)
{
  NS_PRECONDITION((aRight > left) && (aRight < right), "bad argument");
  
  // Create a new band rect for the right part
  BandRect* rightBandRect;
                                            
  if (numFrames > 1) {
    rightBandRect = new BandRect(aRight, top, right, bottom, frames);
  } else {
    rightBandRect = new BandRect(aRight, top, right, bottom, frame);
  }
                                           
  // This band rect becomes the left part, so adjust the right edge
  right = aRight;

  return rightBandRect;
}

PRBool SpaceManager::BandRect::IsOccupiedBy(const nsIFrame* aFrame) const
{
  PRBool  result;

  if (1 == numFrames) {
    result = (frame == aFrame);
  } else {
    PRInt32 count = frames->Count();

    result = PR_FALSE;
    for (PRInt32 i = 0; i < count; i++) {
      nsIFrame* f = (nsIFrame*)frames->ElementAt(i);

      if (f == aFrame) {
        result = PR_TRUE;
        break;
      }
    }
  }

  return result;
}

void SpaceManager::BandRect::AddFrame(const nsIFrame* aFrame)
{
  if (1 == numFrames) {
    nsIFrame* f = frame;
    frames = new nsVoidArray;
    frames->AppendElement(f);
  }

  numFrames++;
  frames->AppendElement((void*)aFrame);
  NS_POSTCONDITION(frames->Count() == numFrames, "bad frame count");
}

void SpaceManager::BandRect::RemoveFrame(const nsIFrame* aFrame)
{
  NS_PRECONDITION(numFrames > 1, "only one frame");
  frames->RemoveElement((void*)aFrame);
  numFrames--;

  if (1 == numFrames) {
    nsIFrame* f = (nsIFrame*)frames->ElementAt(0);

    delete frames;
    frame = f;
  }
}

PRBool SpaceManager::BandRect::HasSameFrameList(const BandRect* aBandRect) const
{
  // Check whether they're occupied by the same number of frames
  if (numFrames != aBandRect->numFrames) {
    return PR_FALSE;
  }

  // Check that the list of frames matches
  if (1 == numFrames) {
    return frame == aBandRect->frame;
  } else {
    // For each frame occupying this band rect check whether it also occupies
    // aBandRect
    PRInt32 count = frames->Count();
    for (PRInt32 i = 0; i < count; i++) {
      nsIFrame* f = (nsIFrame*)frames->ElementAt(i);

      if (-1 == aBandRect->frames->IndexOf(f)) {
        return PR_FALSE;
      }
    }

    return PR_TRUE;
  }
}
