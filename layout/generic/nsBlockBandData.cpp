/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCOMPtr.h"
#include "nsBlockBandData.h"
#include "nsIFrame.h"
#include "nsHTMLReflowState.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIFrameManager.h"
#include "nsLayoutAtoms.h"
#include "nsVoidArray.h"

nsBlockBandData::nsBlockBandData()
  : mSpaceManager(nsnull),
    mSpaceManagerX(0),
    mSpaceManagerY(0),
    mSpace(0, 0)
{
  mSize = NS_BLOCK_BAND_DATA_TRAPS;
  mTrapezoids = mData;
}

nsBlockBandData::~nsBlockBandData()
{
  NS_IF_RELEASE(mSpaceManager);
  if (mTrapezoids != mData) {
    delete [] mTrapezoids;
  }
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
  mLeftFloaters = 0;
  mRightFloaters = 0;
  return NS_OK;
}

// Get the available reflow space for the current y coordinate. The
// available space is relative to our coordinate system (0,0) is our
// upper left corner.
nsresult
nsBlockBandData::GetAvailableSpace(nscoord aY, nsRect& aResult)
{
  // Get the raw band data for the given Y coordinate
  nsresult rv = GetBandData(aY);
  if (NS_FAILED(rv)) { return rv; }

  // Compute the bounding rect of the available space, i.e. space
  // between any left and right floaters.
  ComputeAvailSpaceRect();
  aResult = mAvailSpace;
#ifdef REALLY_NOISY_COMPUTEAVAILSPACERECT
  printf("nsBBD %p GetAvailableSpace(%d) returing (%d, %d, %d, %d)\n",
          this, aY, aResult.x, aResult.y, aResult.width, aResult.height);
#endif
  return NS_OK;
}

// the code below should never loop more than a very few times.
// this is a safety valve to see if we've gone off the deep end
#define ERROR_TOO_MANY_ITERATIONS 1000

/* nsBlockBandData methods should never call mSpaceManager->GetBandData directly.
 * They should always call nsBlockBandData::GetBandData() instead.
 */
nsresult
nsBlockBandData::GetBandData(nscoord aY)
{
  NS_ASSERTION(mSpaceManager, "bad state, no space manager");
  if (!mSpaceManager) {
    return NS_ERROR_FAILURE;
  }
  PRInt32 iterations =0;
  nsresult rv = mSpaceManager->GetBandData(aY, mSpace, *this);
  while (NS_FAILED(rv)) {
    iterations++;
    if (iterations>ERROR_TOO_MANY_ITERATIONS)
    {
      NS_ASSERTION(PR_FALSE, "too many iterations in nsBlockBandData::GetBandData");
      return NS_ERROR_FAILURE;
    }
    // We need more space for our bands
    NS_ASSERTION(mTrapezoids, "bad state, no mTrapezoids");
    if (mTrapezoids && (mTrapezoids != mData)) {
      delete [] mTrapezoids;
    }
    PRInt32 newSize = mSize * 2;
    if (newSize<mCount) {
      newSize = mCount;
    }
    mTrapezoids = new nsBandTrapezoid[newSize];
    NS_POSTCONDITION(mTrapezoids, "failure allocating mTrapezoids");
    if (!mTrapezoids) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mSize = newSize;
    rv = mSpaceManager->GetBandData(aY, mSpace, *this);
  }
  NS_POSTCONDITION(mCount<=mSize, "bad state, count > size");
  return NS_OK;
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
#ifdef REALLY_NOISY_COMPUTEAVAILSPACERECT
  printf("nsBlockBandData::ComputeAvailSpaceRect %p with count %d\n", this, mCount);
#endif
  if (0 == mCount) {
    mAvailSpace.x = 0;
    mAvailSpace.y = 0;
    mAvailSpace.width = 0;
    mAvailSpace.height = 0;
    mLeftFloaters = 0;
    mRightFloaters = 0;
    return;
  }

  nsBandTrapezoid* trapezoid = mTrapezoids;
  nsBandTrapezoid* rightTrapezoid = nsnull;

  PRInt32 leftFloaters = 0;
  PRInt32 rightFloaters = 0;
  if (mCount > 1) {
    // If there's more than one trapezoid that means there are floaters
    PRInt32 i;

    // Examine each trapezoid in the band, counting up the number of
    // left and right floaters. Use the right-most floater to
    // determine where the right edge of the available space is.
    NS_PRECONDITION(mCount<=mSize, "bad state, count > size");
    for (i = 0; i < mCount; i++) {
      trapezoid = &mTrapezoids[i];
      if (trapezoid->mState != nsBandTrapezoid::Available) {
#ifdef REALLY_NOISY_COMPUTEAVAILSPACERECT
        printf("band %p checking !Avail trap %p with frame %p\n", this, trapezoid, trapezoid->mFrame);
#endif
        const nsStyleDisplay* display;
        if (nsBandTrapezoid::OccupiedMultiple == trapezoid->mState) {
          PRInt32 j, numFrames = trapezoid->mFrames->Count();
          NS_ASSERTION(numFrames > 0, "bad trapezoid frame list");
          for (j = 0; j < numFrames; j++) {
            nsIFrame* f = (nsIFrame*) trapezoid->mFrames->ElementAt(j);
            f->GetStyleData(eStyleStruct_Display,
                            (const nsStyleStruct*&)display);
            if (NS_STYLE_FLOAT_LEFT == display->mFloats) {
              leftFloaters++;
            }
            else if (NS_STYLE_FLOAT_RIGHT == display->mFloats) {
              rightFloaters++;
              if ((nsnull == rightTrapezoid) && (i > 0)) {
                rightTrapezoid = &mTrapezoids[i - 1];
              }
            }
          }
        } else {
          trapezoid->mFrame->GetStyleData(eStyleStruct_Display,
                                    (const nsStyleStruct*&)display);
          if (NS_STYLE_FLOAT_LEFT == display->mFloats) {
            leftFloaters++;
          }
          else if (NS_STYLE_FLOAT_RIGHT == display->mFloats) {
            rightFloaters++;
            if ((nsnull == rightTrapezoid) && (i > 0)) {
              rightTrapezoid = &mTrapezoids[i - 1];
            }
          }
        }
      }
    }
  }
  else if (mTrapezoids[0].mState != nsBandTrapezoid::Available) {
    // We have a floater using up all the available space
    leftFloaters = 1;
  }
#ifdef REALLY_NOISY_COMPUTEAVAILSPACERECT
  printf("band %p has floaters %d, %d\n", this, leftFloaters, rightFloaters);
#endif
  mLeftFloaters = leftFloaters;
  mRightFloaters = rightFloaters;

  if (nsnull != rightTrapezoid) {
    trapezoid = rightTrapezoid;
  }
  trapezoid->GetRect(mAvailSpace);

  // When there is no available space, we still need a proper X
  // coordinate to place objects that end up here anyway.
  if (nsBandTrapezoid::Available != trapezoid->mState) {
    const nsStyleDisplay* display;
    if (nsBandTrapezoid::OccupiedMultiple == trapezoid->mState) {
      // It's not clear what coordinate to use when there is no
      // available space and the space is multiply occupied...So: If
      // any of the floaters that are a part of the trapezoid are left
      // floaters then we move over to the right edge of the
      // unavaliable space.
      PRInt32 j, numFrames = trapezoid->mFrames->Count();
      NS_ASSERTION(numFrames > 0, "bad trapezoid frame list");
      for (j = 0; j < numFrames; j++) {
        nsIFrame* f = (nsIFrame*) trapezoid->mFrames->ElementAt(j);
        f->GetStyleData(eStyleStruct_Display,
                        (const nsStyleStruct*&)display);
        if (NS_STYLE_FLOAT_LEFT == display->mFloats) {
          mAvailSpace.x = mAvailSpace.XMost();
          break;
        }
      }
    }
    else {
      trapezoid->mFrame->GetStyleData(eStyleStruct_Display,
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
#ifdef REALLY_NOISY_COMPUTEAVAILSPACERECT
  printf("  ComputeAvailSpaceRect settting state mAvailSpace (%d,%d,%d,%d)\n", 
         mAvailSpace.x, mAvailSpace.y, mAvailSpace.width, mAvailSpace.height);
#endif

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

// XXX optimization? use mFloaters to avoid doing anything
nscoord
nsBlockBandData::ClearFloaters(nscoord aY, PRUint8 aBreakType)
{
  for (;;) {
    nsresult rv = GetBandData(aY);
    if (NS_FAILED(rv)) {
      break;  // something is seriously wrong, bail
    }
    ComputeAvailSpaceRect();

    // Compute aYS as aY in space-manager "root" coordinates.
    nscoord aYS = aY + mSpaceManagerY;

    // Find the largest frame YMost for the appropriate floaters in
    // this band.
    nscoord yMost = aYS;
    PRInt32 i;
    NS_PRECONDITION(mCount<=mSize, "bad state, count > size");
    for (i = 0; i < mCount; i++) {
      nsBandTrapezoid* trapezoid = &mTrapezoids[i];
      if (nsBandTrapezoid::Available != trapezoid->mState) {
        if (nsBandTrapezoid::OccupiedMultiple == trapezoid->mState) {
          PRInt32 fn, numFrames = trapezoid->mFrames->Count();
          NS_ASSERTION(numFrames > 0, "bad trapezoid frame list");
          for (fn = 0; fn < numFrames; fn++) {
            nsIFrame* frame = (nsIFrame*) trapezoid->mFrames->ElementAt(fn);
            if (ShouldClearFrame(frame, aBreakType)) {
              nscoord ym = trapezoid->mBottomY + mSpaceManagerY;
              if (ym > yMost) yMost = ym;
            }
          }
        }
        else if (ShouldClearFrame(trapezoid->mFrame, aBreakType)) {
          nscoord ym = trapezoid->mBottomY + mSpaceManagerY;
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

//----------------------------------------------------------------------

static void
MaxElementSizePropertyDtor(nsIPresContext* aPresContext,
                           nsIFrame*       aFrame,
                           nsIAtom*        aPropertyName,
                           void*           aPropertyValue)
{
  nsSize* size = (nsSize*) aPropertyValue;
  delete size;
}

void
nsBlockBandData::StoreMaxElementSize(nsIPresContext* aPresContext,
                                     nsIFrame* aFrame,
                                     const nsSize& aMaxElementSize)
{
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  if (shell) {
    nsCOMPtr<nsIFrameManager> mgr;
    shell->GetFrameManager(getter_AddRefs(mgr));
    if (mgr) {
      nsSize* size = new nsSize(aMaxElementSize);
      if (size) {
        mgr->SetFrameProperty(aFrame, nsLayoutAtoms::maxElementSizeProperty,
                              size, MaxElementSizePropertyDtor);
      }
    }
  }
}

void
nsBlockBandData::RecoverMaxElementSize(nsIPresContext* aPresContext,
                                       nsIFrame* aFrame,
                                       nsSize* aResult)
{
  if (!aResult) return;

  nsSize answer(0, 0);
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  if (shell) {
    nsCOMPtr<nsIFrameManager> mgr;
    shell->GetFrameManager(getter_AddRefs(mgr));
    if (mgr) {
      nsSize* size = nsnull;
      mgr->GetFrameProperty(aFrame, nsLayoutAtoms::maxElementSizeProperty,
                            0, (void**) &size);
      if (size) {
        answer = *size;
      }
    }
  }

  *aResult = answer;
}

void
nsBlockBandData::GetMaxElementSize(nsIPresContext* aPresContext,
                                   nscoord* aWidthResult,
                                   nscoord* aHeightResult) const
{
  nsCOMPtr<nsIFrameManager> mgr;
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  if (shell) {
    shell->GetFrameManager(getter_AddRefs(mgr));
  }

  nsRect r;
  nscoord maxWidth = 0;
  nscoord maxHeight = 0;
  for (PRInt32 i = 0; i < mCount; i++) {
    const nsBandTrapezoid* trap = &mTrapezoids[i];
    if (trap->mState != nsBandTrapezoid::Available) {

      // Note: get the total height of the frame to compute the maxHeight,
      // not just the height that is part of this band.

      if (nsBandTrapezoid::OccupiedMultiple == trap->mState) {
        PRBool usedBackupValue = PR_FALSE;
        PRInt32 j, numFrames = trap->mFrames->Count();
        NS_ASSERTION(numFrames > 0, "bad trapezoid frame list");
        for (j = 0; j < numFrames; j++) {
          PRBool useBackupValue = PR_TRUE;

          nsIFrame* f = (nsIFrame*) trap->mFrames->ElementAt(j);
          if (mgr) {
            nsSize* maxElementSize = nsnull;
            mgr->GetFrameProperty(f, nsLayoutAtoms::maxElementSizeProperty,
                                  0, (void**) &maxElementSize);
            if (maxElementSize) {
              useBackupValue = PR_FALSE;
              if (maxElementSize->width > maxWidth) {
                maxWidth = maxElementSize->width;
              }
              if (maxElementSize->height > maxHeight) {
                maxHeight = maxElementSize->height;
              }
            }
          }
          if (useBackupValue) {
            usedBackupValue = PR_TRUE;
            f->GetRect(r);
            if (r.height > maxHeight) maxHeight = r.height;
          }
        }

        // Get the width of the impacted area and update the maxWidth
        if (usedBackupValue) {
          trap->GetRect(r);
          if (r.width > maxWidth) maxWidth = r.width;
        }
      } else {
        PRBool useBackupValue = PR_TRUE;
        if (mgr) {
          nsSize* maxElementSize = nsnull;
          mgr->GetFrameProperty(trap->mFrame,
                                nsLayoutAtoms::maxElementSizeProperty,
                                0, (void**) &maxElementSize);
          if (maxElementSize) {
            useBackupValue = PR_FALSE;
            if (maxElementSize->width > maxWidth) {
              maxWidth = maxElementSize->width;
            }
            if (maxElementSize->height > maxHeight) {
              maxHeight = maxElementSize->height;
            }
          }
        }
        if (useBackupValue) {
          trap->GetRect(r);
          if (r.width > maxWidth) maxWidth = r.width;
          trap->mFrame->GetRect(r);
          if (r.height > maxHeight) maxHeight = r.height;
        }
      }
    }
  }
  *aWidthResult = maxWidth;
  *aHeightResult = maxHeight;
}

#ifdef DEBUG
void nsBlockBandData::List()
{
  printf("nsBlockBandData %p sm=%p, sm coord = (%d,%d), mSpace = (%d,%d)\n",
          this, mSpaceManager, mSpaceManagerX, mSpaceManagerY,
          mSpace.width, mSpace.height);
  printf("  availSpace=(%d, %d, %d, %d), floaters l=%d r=%d\n",
          mAvailSpace.x, mAvailSpace.y, mAvailSpace.width, mAvailSpace.height,
          mLeftFloaters, mRightFloaters);
}
#endif
