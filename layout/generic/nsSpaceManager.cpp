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
  nsSpaceManager::FrameInfo* frameInfo = (nsSpaceManager::FrameInfo*)he->value;

  NS_ASSERTION(nsnull != frameInfo, "null frameInfo");
  delete frameInfo;
  return HT_ENUMERATE_REMOVE;
}

/////////////////////////////////////////////////////////////////////////////
// BandList

nsSpaceManager::BandList::BandList()
  : BandRect(-1, -1, -1, -1, (nsIFrame*)nsnull)
{
  PR_INIT_CLIST(this);
  numFrames = 0;
}

void nsSpaceManager::BandList::Clear()
{
  if (!IsEmpty()) {
    BandRect* bandRect = Head();
  
    while (bandRect != this) {
      BandRect* next = bandRect->Next();
  
      delete bandRect;
      bandRect = next;
    }
  
    PR_INIT_CLIST(this);
  }
}


/////////////////////////////////////////////////////////////////////////////
// nsSpaceManager

nsSpaceManager::nsSpaceManager(nsIFrame* aFrame)
  : mFrame(aFrame)
{
  NS_INIT_REFCNT();
  mX = mY = 0;
  mFrameInfoMap = nsnull;
}

void nsSpaceManager::ClearFrameInfo()
{
  if (nsnull != mFrameInfoMap) {
    PL_HashTableEnumerateEntries(mFrameInfoMap, NS_RemoveFrameInfoEntries, 0);
    PL_HashTableDestroy(mFrameInfoMap);
    mFrameInfoMap = nsnull;
  }
}

nsSpaceManager::~nsSpaceManager()
{
  mBandList.Clear();
  ClearFrameInfo();
}

NS_IMPL_ISUPPORTS(nsSpaceManager, kISpaceManagerIID);

nsIFrame* nsSpaceManager::GetFrame() const
{
  return mFrame;
}

void nsSpaceManager::Translate(nscoord aDx, nscoord aDy)
{
  mX += aDx;
  mY += aDy;
}

void nsSpaceManager::GetTranslation(nscoord& aX, nscoord& aY) const
{
  aX = mX;
  aY = mY;
}

nscoord nsSpaceManager::YMost() const
{
  nscoord yMost;

  if (mBandList.IsEmpty()) {
    yMost = 0;
  } else {
    BandRect* lastRect = mBandList.Tail();

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
PRInt32 nsSpaceManager::GetBandAvailableSpace(const BandRect* aBand,
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
    aBand = aBand->Next();
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
      trapezoid->state = nsBandTrapezoid::Available;
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
      trapezoid->state = nsBandTrapezoid::Occupied;
      trapezoid->frame = aBand->frame;
    } else {
      NS_ASSERTION(aBand->numFrames > 1, "unexpected frame count");
      trapezoid->state = nsBandTrapezoid::OccupiedMultiple;
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
    aBand = aBand->Next();
  }

  // No more rects left in the band. If we haven't yet reached the right edge,
  // then all the remaining space is available
  if (left < rightEdge) {
    trapezoid->state = nsBandTrapezoid::Available;
    trapezoid->frame = nsnull;

    // Assign the trapezoid a rectangular shape. The trapezoid must be in the
    // local coordinate space, so convert the current left coordinate
    *trapezoid = nsRect(left - mX, localY, rightEdge - left, height);
    aBandData.count++;
  }

  return aBandData.count;
}

PRInt32 nsSpaceManager::GetBandData(nscoord       aYOffset,
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
    aBandData.trapezoids[0].state = nsBandTrapezoid::Available;
    aBandData.trapezoids[0].frame = nsnull;
  } else {
    // Find the first band that contains the y-offset or is below the y-offset
    NS_ASSERTION(!mBandList.IsEmpty(), "no bands");
    BandRect* band = mBandList.Head();

    aBandData.count = 0;
    while (nsnull != band) {
      if (band->top > y) {
        // The band is below the y-offset. The area between the y-offset and
        // the top of the band is available
        aBandData.count = 1;
        aBandData.trapezoids[0] =
          nsRect(0, aYOffset, aMaxSize.width, PR_MIN(band->top - y, aMaxSize.height));
        aBandData.trapezoids[0].state = nsBandTrapezoid::Available;
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
nsSpaceManager::BandRect* nsSpaceManager::GetNextBand(const BandRect* aBandRect) const
{
  nscoord topOfBand = aBandRect->top;

  aBandRect = aBandRect->Next();
  while (aBandRect != &mBandList) {
    // Check whether this rect is part of the same band
    if (aBandRect->top != topOfBand) {
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
void nsSpaceManager::DivideBand(BandRect* aBandRect, nscoord aBottom)
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
    nextBand->InsertBefore(bottomBandRect);

    // Move to the next rect in the band
    aBandRect = aBandRect->Next();
  }
}

PRBool nsSpaceManager::CanJoinBands(BandRect* aBand, BandRect* aPrevBand)
{
  PRBool  result;
  nscoord topOfBand = aBand->top;
  nscoord topOfPrevBand = aPrevBand->top;

  // The bands can be joined if:
  // - they're adjacent
  // - they have the same number of rects
  // - each rect has the same left and right edge as its corresponding rect, and
  //   the rects are occupied by the same frames
  if (aPrevBand->bottom == aBand->top) {
    // Compare each of the rects in the two bands
    while (PR_TRUE) {
      if ((aBand->left != aPrevBand->left) || (aBand->right != aPrevBand->right)) {
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
      PRBool  endOfBand = aBand->top != topOfBand;
      PRBool  endOfPrevBand = aPrevBand->top != topOfPrevBand;

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
PRBool nsSpaceManager::JoinBands(BandRect* aBand, BandRect* aPrevBand)
{
  if (CanJoinBands(aBand, aPrevBand)) {
    BandRect* startOfNextBand = aBand;

    while (aPrevBand != startOfNextBand) {
      // Adjust the top of the band we're keeping, and then move to the next
      // rect within the band
      aBand->top = aPrevBand->top;
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
void nsSpaceManager::AddRectToBand(BandRect*  aBand,
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
        aBand->InsertBefore(aBandRect);
        return;
      }

      // Yes, they overlap. Compare the right edges.
      if (aBandRect->right > aBand->right) {
        // The new rect's right edge is to the right of the existing rect's
        // right edge (case #3). Split the new rect
        BandRect* r1 = aBandRect->SplitHorizontally(aBand->left);

        // Insert the part of the new rect that's to the left of the existing
        // rect as a new band rect
        aBand->InsertBefore(aBandRect);

        // Continue below with the part that overlaps the existing rect
        aBandRect = r1;

      } else {
        if (aBand->right > aBandRect->right) {
          // The existing rect extends past the new rect (case #2). Split the
          // existing rect
          BandRect* r1 = aBand->SplitHorizontally(aBandRect->right);

          // Insert the new right half of the existing rect
          aBand->InsertAfter(r1);
        }

        // Insert the part of the new rect that's to the left of the existing
        // rect
        aBandRect->right = aBand->left;
        aBand->InsertBefore(aBandRect);

        // Mark the existing rect as shared
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
        aBand = aBand->Next();
        continue;
      }

      // The rects overlap, so divide the existing rect into two rects: the
      // part to the left of the new rect, and the part that overlaps
      BandRect* r1 = aBand->SplitHorizontally(aBandRect->left);

      // Insert the new right half of the existing rect, and make it the current
      // rect
      aBand->InsertAfter(r1);
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
      aBand->InsertAfter(r1);

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
        aBand = aBand->Next();
        continue;
      }
    }
  } while (aBand->top == topOfBand);

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
void nsSpaceManager::InsertBandRect(BandRect* aBandRect)
{
  // If there are no existing bands or this rect is below the bottommost
  // band, then add a new band
  if (aBandRect->top >= YMost()) {
    mBandList.Append(aBandRect);
    return;
  }

  // Examine each band looking for a band that intersects this rect
  NS_ASSERTION(!mBandList.IsEmpty(), "no bands");
  BandRect* band = mBandList.Head();

  while (nsnull != band) {
    // Compare the top edge of this rect with the top edge of the band
    if (aBandRect->top < band->top) {
      // The top edge of the rect is above the top edge of the band.
      // Is there any overlap?
      if (aBandRect->bottom <= band->top) {
        // Case #1. This rect is completely above the band, so insert a
        // new band before the current band
        band->InsertBefore(aBandRect);
        break;  // we're all done
      }

      // Case #2 and case #7. Divide this rect, creating a new rect for
      // the part that's above the band
      BandRect* bandRect1 = new BandRect(aBandRect->left, aBandRect->top,
                                         aBandRect->right, band->top,
                                         aBandRect->frame);

      // Insert bandRect1 as a new band
      band->InsertBefore(bandRect1);

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
        mBandList.Append(aBandRect);
        break;
      }
    }
  }
}

PRBool nsSpaceManager::AddRectRegion(nsIFrame* aFrame, const nsRect& aUnavailableSpace)
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

  // Verify that the offset is within the defined coordinate space
  if ((rect.x < 0) || (rect.y < 0)) {
    NS_WARNING("invalid offset for rect region");
    return PR_FALSE;
  }

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

PRBool nsSpaceManager::ResizeRectRegion(nsIFrame*    aFrame,
                                        nscoord      aDeltaWidth,
                                        nscoord      aDeltaHeight,
                                        AffectedEdge aEdge)
{
  // Get the frame info associated with with aFrame
  FrameInfo*  frameInfo = GetFrameInfoFor(aFrame);

  if (nsnull == frameInfo) {
    NS_WARNING("no region associated with aFrame");
    return PR_FALSE;
  }

  nsRect  rect(frameInfo->rect);
  rect.SizeBy(aDeltaWidth, aDeltaHeight);
  if (aEdge == LeftEdge) {
    rect.x += aDeltaWidth;
  }

  // Verify that the offset is within the defined coordinate space
  if ((rect.x < 0) || (rect.y < 0)) {
    NS_WARNING("invalid offset when resizing rect region");
    return PR_FALSE;
  }

  // For the time being just remove it and add it back in
  RemoveRegion(aFrame);
  return AddRectRegion(aFrame, rect);
}

PRBool nsSpaceManager::OffsetRegion(nsIFrame* aFrame, nscoord aDx, nscoord aDy)
{
  // Get the frame info associated with with aFrame
  FrameInfo*  frameInfo = GetFrameInfoFor(aFrame);

  if (nsnull == frameInfo) {
    NS_WARNING("no region associated with aFrame");
    return PR_FALSE;
  }

  nsRect  rect(frameInfo->rect);
  rect.MoveBy(aDx, aDy);

  // Verify that the offset is within the defined coordinate space
  if ((rect.x < 0) || (rect.y < 0)) {
    NS_WARNING("invalid offset when offseting rect region");
    return PR_FALSE;
  }

  // For the time being just remove it and add it back in
  RemoveRegion(aFrame);
  return AddRectRegion(aFrame, rect);
}

PRBool nsSpaceManager::RemoveRegion(nsIFrame* aFrame)
{
  // Get the frame info associated with aFrame
  FrameInfo*  frameInfo = GetFrameInfoFor(aFrame);

  if (nsnull == frameInfo) {
    NS_WARNING("no region associated with aFrame");
    return PR_FALSE;
  }

  if (!frameInfo->rect.IsEmpty()) {
    NS_ASSERTION(!mBandList.IsEmpty(), "no bands");
    BandRect* band = mBandList.Head();
    BandRect* prevBand = nsnull;
    PRBool    prevFoundMatchingRect = PR_FALSE;

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
          // Remember that we found a matching rect in this band
          foundMatchingRect = PR_TRUE;

          if (rect->numFrames > 1) {
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
              if (topOfBand == next->top) {
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
          if ((prevRect->right == rect->left) && (prevRect->HasSameFrameList(rect))) {
            // Modify the current rect's left edge, and delete the previous rect
            rect->left = prevRect->left;
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
      } while (rect->top == topOfBand);

      if (nsnull != band) {
        // If we found a rect occupied by aFrame in this band or the previous band
        // then try to join the two bands
        if (prevFoundMatchingRect || (foundMatchingRect && (nsnull != prevBand))) {
          // Try and join this band with the previous band
          NS_ASSERTION(nsnull != prevBand, "no previous band");
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
  return PR_TRUE;
}

void nsSpaceManager::ClearRegions()
{
  ClearFrameInfo();
  mBandList.Clear();
}

nsSpaceManager::FrameInfo* nsSpaceManager::GetFrameInfoFor(nsIFrame* aFrame)
{
  FrameInfo*  result = nsnull;

  if (nsnull != mFrameInfoMap) {
    result = (FrameInfo*)PL_HashTableLookup(mFrameInfoMap, (const void*)aFrame);
  }

  return result;
}

nsSpaceManager::FrameInfo* nsSpaceManager::CreateFrameInfo(nsIFrame*     aFrame,
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

void nsSpaceManager::DestroyFrameInfo(FrameInfo* aFrameInfo)
{
  PL_HashTableRemove(mFrameInfoMap, (const void*)aFrameInfo->frame);
  delete aFrameInfo;
}

/////////////////////////////////////////////////////////////////////////////
// FrameInfo

nsSpaceManager::FrameInfo::FrameInfo(nsIFrame* aFrame, const nsRect& aRect)
  : frame(aFrame), rect(aRect)
{
}

/////////////////////////////////////////////////////////////////////////////
// BandRect

nsSpaceManager::BandRect::BandRect(nscoord    aLeft,
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

nsSpaceManager::BandRect::BandRect(nscoord      aLeft,
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

nsSpaceManager::BandRect::~BandRect()
{
  if (numFrames > 1) {
    delete frames;
  }
}

nsSpaceManager::BandRect* nsSpaceManager::BandRect::SplitVertically(nscoord aBottom)
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

nsSpaceManager::BandRect* nsSpaceManager::BandRect::SplitHorizontally(nscoord aRight)
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

PRBool nsSpaceManager::BandRect::IsOccupiedBy(const nsIFrame* aFrame) const
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

void nsSpaceManager::BandRect::AddFrame(const nsIFrame* aFrame)
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

void nsSpaceManager::BandRect::RemoveFrame(const nsIFrame* aFrame)
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

PRBool nsSpaceManager::BandRect::HasSameFrameList(const BandRect* aBandRect) const
{
  PRBool  result;

  // Check whether they're occupied by the same number of frames
  if (numFrames != aBandRect->numFrames) {
    result = PR_FALSE;
  } else if (1 == numFrames) {
    result = frame == aBandRect->frame;
  } else {
    result = PR_TRUE;

    // For each frame occupying this band rect check whether it also occupies
    // aBandRect
    PRInt32 count = frames->Count();
    for (PRInt32 i = 0; i < count; i++) {
      nsIFrame* f = (nsIFrame*)frames->ElementAt(i);

      if (-1 == aBandRect->frames->IndexOf(f)) {
        result = PR_FALSE;
        break;
      }
    }
  }

  return result;
}
