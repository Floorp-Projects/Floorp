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
    mSpace(0, 0),
    mSpaceManagerX(0),
    mSpaceManagerY(0)
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

  // Fixup width
  if (NS_UNCONSTRAINEDSIZE == mSpace.width) {
    mAvailSpace.width = NS_UNCONSTRAINEDSIZE;
  }
  aResult = mAvailSpace;
}

// Get the available reflow space for the current y coordinate. The
// available space is relative to our coordinate system (0,0) is our
// upper left corner.
void
nsBlockBandData::GetAvailableSpace(nscoord aY)
{
  // Get the raw band data for the given Y coordinate
  mSpaceManager->GetBandData(aY, mSpace, *this);

  // Compute the bounding rect of the available space, i.e. space
  // between any left and right floaters.
  ComputeAvailSpaceRect();

  // Fixup width
  if (NS_UNCONSTRAINEDSIZE == mSpace.width) {
    mAvailSpace.width = NS_UNCONSTRAINEDSIZE;
  }
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
  nsBandTrapezoid* trapezoid = mData;

  if (count > 1) {
    // If there's more than one trapezoid that means there are floaters
    PRInt32 i;

    // Stop when we get to space occupied by a right floater, or when we've
    // looked at every trapezoid and none are right floaters
    for (i = 0; i < count; i++) {
      trapezoid = &mData[i];
      if (trapezoid->state != nsBandTrapezoid::Available) {
        const nsStyleDisplay* display;
        if (nsBandTrapezoid::OccupiedMultiple == trapezoid->state) {
          PRInt32 j, numFrames = trapezoid->frames->Count();
          NS_ASSERTION(numFrames > 0, "bad trapezoid frame list");
          for (j = 0; j < numFrames; j++) {
            nsIFrame* f = (nsIFrame*)trapezoid->frames->ElementAt(j);
            f->GetStyleData(eStyleStruct_Display,
                            (const nsStyleStruct*&)display);
            if (NS_STYLE_FLOAT_RIGHT == display->mFloats) {
              goto foundRightFloater;
            }
          }
        } else {
          trapezoid->frame->GetStyleData(eStyleStruct_Display,
                                         (const nsStyleStruct*&)display);
          if (NS_STYLE_FLOAT_RIGHT == display->mFloats) {
            break;
          }
        }
      }
    }
  foundRightFloater:

    if (i > 0) {
      trapezoid = &mData[i - 1];
    }
  }

  if (nsBandTrapezoid::Available == trapezoid->state) {
    // The trapezoid is available
    trapezoid->GetRect(mAvailSpace);
  } else {
    const nsStyleDisplay* display;

    // The trapezoid is occupied. That means there's no available space
    trapezoid->GetRect(mAvailSpace);

    // XXX Better handle the case of multiple frames
    if (nsBandTrapezoid::Occupied == trapezoid->state) {
      trapezoid->frame->GetStyleData(eStyleStruct_Display,
                                     (const nsStyleStruct*&)display);
      if (NS_STYLE_FLOAT_LEFT == display->mFloats) {
        mAvailSpace.x = mAvailSpace.XMost();
      }
    }
    mAvailSpace.width = 0;
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
  spaceFrame = mSpaceManager->GetFrame();

  nsRect r;
  nsPoint p;
  aFrame->GetRect(r);
  nscoord y = r.y;
  nsIFrame* parent;
  aFrame->GetGeometricParent(parent);
  PRBool done = PR_FALSE;
  while (!done && (parent != spaceFrame)) {
    // If parent has a prev-in-flow, check there for equality first
    // before looking upward.
    nsIFrame* parentPrevInFlow;
    parent->GetPrevInFlow(parentPrevInFlow);
    while (nsnull != parentPrevInFlow) {
      if (parentPrevInFlow == spaceFrame) {
        done = PR_TRUE;
        break;
      }
      parentPrevInFlow->GetPrevInFlow(parentPrevInFlow);
    }

    if (!done) {
      parent->GetOrigin(p);
      y += p.y;
      parent->GetGeometricParent(parent);
    }
  }

  return y + r.height;
}

nscoord
nsBlockBandData::ClearFloaters(nscoord aY, PRUint8 aBreakType)
{
  for (;;) {
    // Update band information based on target Y before clearing.
    nscoord oldY = aY;
    GetAvailableSpace(aY);

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
