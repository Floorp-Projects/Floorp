/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsBlockBandData.h"
#include "nsIFrame.h"
#include "nsIFrameReflow.h"
#include "nsIStyleContext.h"
#include "nsVoidArray.h"

nsBlockBandData::nsBlockBandData()
  : mSpaceManager(nsnull),
    mSpaceManagerX(0),
    mSpaceManagerY(0),
    mSpace(0, 0)
{
  size = 12;
  trapezoids = mData;
}

nsBlockBandData::~nsBlockBandData()
{
  NS_IF_RELEASE(mSpaceManager);
}

nsresult
nsBlockBandData::Init(nsISpaceManager* aSpaceManager,
                      const nsSize& aSpace)
{
  NS_PRECONDITION(nsnull != aSpaceManager, "null pointer");
  if (nsnull == aSpaceManager) {
    return NS_ERROR_NULL_POINTER;
  }

  NS_IF_RELEASE(mSpaceManager);
  mSpaceManager = aSpaceManager;
  NS_ADDREF(aSpaceManager);
  aSpaceManager->GetTranslation(mSpaceManagerX, mSpaceManagerY);

  mSpace = aSpace;
  return NS_OK;
}

// Get the available reflow space for the current y coordinate. The
// available space is relative to our coordinate system (0,0) is our
// upper left corner.
void
nsBlockBandData::GetAvailableSpace(nscoord aY, nsRect& aResult)
{
  // Get the raw band data for the given Y coordinate
  mSpaceManager->GetBandData(aY, mSpace, *this);

  // Compute the bounding rect of the available space, i.e. space
  // between any left and right floaters.
  ComputeAvailSpaceRect();
  aResult = mAvailSpace;
}

/**
 * Computes the bounding rect of the available space, i.e. space
 * between any left and right floaters. Uses the current trapezoid
 * data, see nsISpaceManager::GetBandData(). Also updates member
 * data "availSpace".
 */
void
nsBlockBandData::ComputeAvailSpaceRect()
{
  if (0 == count) {
    mAvailSpace.x = 0;
    mAvailSpace.y = 0;
    mAvailSpace.width = 0;
    mAvailSpace.height = 0;
    return;
  }

  nsBandTrapezoid* trapezoid = mData;
  nsBandTrapezoid* rightTrapezoid = nsnull;

  PRInt32 leftFloaters = 0;
  PRInt32 rightFloaters = 0;
  if (count > 1) {
    // If there's more than one trapezoid that means there are floaters
    PRInt32 i;

    // Examine each trapezoid in the band, counting up the number of
    // left and right floaters. Use the right-most floater to
    // determine where the right edge of the available space is.
    for (i = 0; i < count; i++) {
      trapezoid = &mData[i];
      if (trapezoid->state != nsBandTrapezoid::Available) {
        const nsStyleDisplay* display;
        if (nsBandTrapezoid::OccupiedMultiple == trapezoid->state) {
          PRInt32 j, numFrames = trapezoid->frames->Count();
          NS_ASSERTION(numFrames > 0, "bad trapezoid frame list");
          for (j = 0; j < numFrames; j++) {
            nsIFrame* f = (nsIFrame*) trapezoid->frames->ElementAt(j);
            f->GetStyleData(eStyleStruct_Display,
                            (const nsStyleStruct*&)display);
            if (NS_STYLE_FLOAT_LEFT == display->mFloats) {
              leftFloaters++;
            }
            else if (NS_STYLE_FLOAT_RIGHT == display->mFloats) {
              rightFloaters++;
              if ((nsnull == rightTrapezoid) && (i > 0)) {
                rightTrapezoid = &mData[i - 1];
              }
            }
          }
        } else {
          trapezoid->frame->GetStyleData(eStyleStruct_Display,
                                    (const nsStyleStruct*&)display);
          if (NS_STYLE_FLOAT_LEFT == display->mFloats) {
            leftFloaters++;
          }
          else if (NS_STYLE_FLOAT_RIGHT == display->mFloats) {
            rightFloaters++;
            if ((nsnull == rightTrapezoid) && (i > 0)) {
              rightTrapezoid = &mData[i - 1];
            }
          }
        }
      }
    }
  }
  mLeftFloaters = leftFloaters;
  mRightFloaters = rightFloaters;
  if (nsnull != rightTrapezoid) {
    trapezoid = rightTrapezoid;
  }
  trapezoid->GetRect(mAvailSpace);

  // When there is no available space, we still need a proper X
  // coordinate to place objects that end up here anyway.
  if (nsBandTrapezoid::Available != trapezoid->state) {
    const nsStyleDisplay* display;
    if (nsBandTrapezoid::OccupiedMultiple == trapezoid->state) {
      // It's not clear what coordinate to use when there is no
      // available space and the space is multiply occupied...So: If
      // any of the floaters that are a part of the trapezoid are left
      // floaters then we move over to the right edge of the
      // unavaliable space.
      PRInt32 j, numFrames = trapezoid->frames->Count();
      NS_ASSERTION(numFrames > 0, "bad trapezoid frame list");
      for (j = 0; j < numFrames; j++) {
        nsIFrame* f = (nsIFrame*) trapezoid->frames->ElementAt(j);
        f->GetStyleData(eStyleStruct_Display,
                        (const nsStyleStruct*&)display);
        if (NS_STYLE_FLOAT_LEFT == display->mFloats) {
          mAvailSpace.x = mAvailSpace.XMost();
          break;
        }
      }
    }
    else {
      trapezoid->frame->GetStyleData(eStyleStruct_Display,
                                     (const nsStyleStruct*&)display);
      if (NS_STYLE_FLOAT_LEFT == display->mFloats) {
        mAvailSpace.x = mAvailSpace.XMost();
      }
    }
    mAvailSpace.width = 0;
  }

  // Fixup width
  if (NS_UNCONSTRAINEDSIZE == mSpace.width) {
    mAvailSpace.width = NS_UNCONSTRAINEDSIZE;
  }
}

/**
 * See if the given frame should be cleared
 */
PRBool
nsBlockBandData::ShouldClearFrame(nsIFrame* aFrame, PRUint8 aBreakType)
{
  PRBool result = PR_FALSE;
  const nsStyleDisplay* display;
  nsresult rv = aFrame->GetStyleData(eStyleStruct_Display,
                                     (const nsStyleStruct*&)display);
  if (NS_SUCCEEDED(rv) && (nsnull != display)) {
    if (NS_STYLE_CLEAR_LEFT_AND_RIGHT == aBreakType) {
      result = PR_TRUE;
    }
    else if (NS_STYLE_FLOAT_LEFT == display->mFloats) {
      if (NS_STYLE_CLEAR_LEFT == aBreakType) {
        result = PR_TRUE;
      }
    }
    else if (NS_STYLE_FLOAT_RIGHT == display->mFloats) {
      if (NS_STYLE_CLEAR_RIGHT == aBreakType) {
        result = PR_TRUE;
      }
    }
  }
  return result;
}

/**
 * Get the frames YMost, in the space managers "root" coordinate
 * system. Because the floating frame may have been placed in a
 * parent/child frame, we map the coordinates into the space-managers
 * untranslated coordinate system.
 */
nscoord
nsBlockBandData::GetFrameYMost(nsIFrame* aFrame)
{
  nsIFrame* spaceFrame;
  mSpaceManager->GetFrame(spaceFrame);

  nsRect r;
  nsPoint p;
  aFrame->GetRect(r);
  nscoord y = r.y;
  nsIFrame* parent;
  aFrame->GetParent(&parent);
  PRBool done = PR_FALSE;
  while (!done && (parent != spaceFrame)) {
    // If parent has a prev-in-flow, check there for equality first
    // before looking upward.
    nsIFrame* parentPrevInFlow;
    parent->GetPrevInFlow(&parentPrevInFlow);
    while (nsnull != parentPrevInFlow) {
      if (parentPrevInFlow == spaceFrame) {
        done = PR_TRUE;
        break;
      }
      parentPrevInFlow->GetPrevInFlow(&parentPrevInFlow);
    }

    if (!done) {
      parent->GetOrigin(p);
      y += p.y;
      parent->GetParent(&parent);
    }
  }

  return y + r.height;
}

// XXX optimization? use mLeftFloaters && mRightFloaters to avoid
// doing anything
nscoord
nsBlockBandData::ClearFloaters(nscoord aY, PRUint8 aBreakType)
{
  for (;;) {
    // Update band information based on target Y before clearing.
    mSpaceManager->GetBandData(aY, mSpace, *this);
    ComputeAvailSpaceRect();

    // Compute aYS as aY in space-manager "root" coordinates.
    nscoord aYS = aY + mSpaceManagerY;

    // Find the largest frame YMost for the appropriate floaters in
    // this band.
    nscoord yMost = aYS;
    PRInt32 i;
    for (i = 0; i < count; i++) {
      nsBandTrapezoid* trapezoid = &mData[i];
      if (nsBandTrapezoid::Available != trapezoid->state) {
        if (nsBandTrapezoid::OccupiedMultiple == trapezoid->state) {
          PRInt32 fn, numFrames = trapezoid->frames->Count();
          NS_ASSERTION(numFrames > 0, "bad trapezoid frame list");
          for (fn = 0; fn < numFrames; fn++) {
            nsIFrame* frame = (nsIFrame*) trapezoid->frames->ElementAt(fn);
            if (ShouldClearFrame(frame, aBreakType)) {
              nscoord ym = GetFrameYMost(frame);
              if (ym > yMost) yMost = ym;
            }
          }
        }
        else if (ShouldClearFrame(trapezoid->frame, aBreakType)) {
          nscoord ym = GetFrameYMost(trapezoid->frame);
          if (ym > yMost) yMost = ym;
        }
      }
    }

    // If yMost is unchanged (aYS) then there were no appropriate
    // floaters in the band. Time to stop clearing.
    if (yMost == aYS) {
      break;
    }
    aY = aY + (yMost - aYS);
  }
  return aY;
}

void
nsBlockBandData::GetMaxElementSize(nscoord* aWidthResult,
                                   nscoord* aHeightResult) const
{
  nsRect r;
  nscoord maxWidth = 0;
  nscoord maxHeight = 0;
  for (PRInt32 i = 0; i < count; i++) {
    const nsBandTrapezoid* trap = &mData[i];
    if (trap->state != nsBandTrapezoid::Available) {
      // Get the width of the impacted area and update the maxWidth
      trap->GetRect(r);
      if (r.width > maxWidth) maxWidth = r.width;

      // Get the total height of the frame to compute the maxHeight,
      // not just the height that is part of this band.
      // XXX vertical margins!
      if (nsBandTrapezoid::OccupiedMultiple == trap->state) {
        PRInt32 j, numFrames = trap->frames->Count();
        NS_ASSERTION(numFrames > 0, "bad trapezoid frame list");
        for (j = 0; j < numFrames; j++) {
          nsIFrame* f = (nsIFrame*) trap->frames->ElementAt(j);
          f->GetRect(r);
          if (r.height > maxHeight) maxHeight = r.height;
        }
      } else {
        trap->frame->GetRect(r);
        if (r.height > maxHeight) maxHeight = r.height;
      }
    }
  }
  *aWidthResult = maxWidth;
  *aHeightResult = maxHeight;
}
