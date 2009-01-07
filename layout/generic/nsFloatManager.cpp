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

#include "nsFloatManager.h"
#include "nsIPresShell.h"
#include "nsMemory.h"
#include "nsHTMLReflowState.h"
#include "nsHashSets.h"
#include "nsBlockDebugFlags.h"

PRInt32 nsFloatManager::sCachedFloatManagerCount = 0;
void* nsFloatManager::sCachedFloatManagers[NS_FLOAT_MANAGER_CACHE_SIZE];

/////////////////////////////////////////////////////////////////////////////

// PresShell Arena allocate callback (for nsIntervalSet use below)
static void*
PSArenaAllocCB(size_t aSize, void* aClosure)
{
  return static_cast<nsIPresShell*>(aClosure)->AllocateFrame(aSize);
}

// PresShell Arena free callback (for nsIntervalSet use below)
static void
PSArenaFreeCB(size_t aSize, void* aPtr, void* aClosure)
{
  static_cast<nsIPresShell*>(aClosure)->FreeFrame(aSize, aPtr);
}

/////////////////////////////////////////////////////////////////////////////
// nsFloatManager

nsFloatManager::nsFloatManager(nsIPresShell* aPresShell)
  : mX(0), mY(0),
    mFloatDamage(PSArenaAllocCB, PSArenaFreeCB, aPresShell)
{
  MOZ_COUNT_CTOR(nsFloatManager);
}

nsFloatManager::~nsFloatManager()
{
  MOZ_COUNT_DTOR(nsFloatManager);
}

// static
void* nsFloatManager::operator new(size_t aSize) CPP_THROW_NEW
{
  if (sCachedFloatManagerCount > 0) {
    // We have cached unused instances of this class, return a cached
    // instance in stead of always creating a new one.
    return sCachedFloatManagers[--sCachedFloatManagerCount];
  }

  // The cache is empty, this means we haveto create a new instance using
  // the global |operator new|.
  return nsMemory::Alloc(aSize);
}

void
nsFloatManager::operator delete(void* aPtr, size_t aSize)
{
  if (!aPtr)
    return;
  // This float manager is no longer used, if there's still room in
  // the cache we'll cache this float manager, unless the layout
  // module was already shut down.

  if (sCachedFloatManagerCount < NS_FLOAT_MANAGER_CACHE_SIZE &&
      sCachedFloatManagerCount >= 0) {
    // There's still space in the cache for more instances, put this
    // instance in the cache in stead of deleting it.

    sCachedFloatManagers[sCachedFloatManagerCount++] = aPtr;
    return;
  }

  // The cache is full, or the layout module has been shut down,
  // delete this float manager.
  nsMemory::Free(aPtr);
}


/* static */
void nsFloatManager::Shutdown()
{
  // The layout module is being shut down, clean up the cache and
  // disable further caching.

  PRInt32 i;

  for (i = 0; i < sCachedFloatManagerCount; i++) {
    void* floatManager = sCachedFloatManagers[i];
    if (floatManager)
      nsMemory::Free(floatManager);
  }

  // Disable further caching.
  sCachedFloatManagerCount = -1;
}

nsRect
nsFloatManager::GetBand(nscoord aYOffset,
                        nscoord aMaxHeight,
                        nscoord aContentAreaWidth,
                        PRBool* aHasFloats) const
{
  NS_ASSERTION(aMaxHeight >= 0, "unexpected max height");
  NS_ASSERTION(aContentAreaWidth >= 0, "unexpected content area width");

  nscoord top = aYOffset + mY;
  if (top < nscoord_MIN) {
    NS_WARNING("bad value");
    top = nscoord_MIN;
  }

  // If there are no floats at all, or we're below the last one, return
  // quickly.
  PRUint32 floatCount = mFloats.Length();
  if (floatCount == 0 ||
      (mFloats[floatCount-1].mLeftYMost <= top &&
       mFloats[floatCount-1].mRightYMost <= top)) {
    *aHasFloats = PR_FALSE;
    return nsRect(0, aYOffset, aContentAreaWidth, aMaxHeight);
  }

  nscoord bottom;
  if (aMaxHeight == nscoord_MAX) {
    bottom = nscoord_MAX;
  } else {
    bottom = top + aMaxHeight;
    if (bottom < top || bottom > nscoord_MAX) {
      NS_WARNING("bad value");
      bottom = nscoord_MAX;
    }
  }
  nscoord left = mX;
  nscoord right = aContentAreaWidth + mX;
  if (right < left) {
    NS_WARNING("bad value");
    right = left;
  }

  // Walk backwards through the floats until we either hit the front of
  // the list or we're above |top|.
  PRBool haveFloats = PR_FALSE;
  for (PRUint32 i = mFloats.Length(); i > 0; --i) {
    const FloatInfo &fi = mFloats[i-1];
    if (fi.mLeftYMost <= top && fi.mRightYMost <= top) {
      // There aren't any more floats that could intersect this band.
      break;
    }
    if (fi.mRect.IsEmpty()) {
      // For compatibility, ignore floats with empty rects, even though it
      // disagrees with the spec.  (We might want to fix this in the
      // future, though.)
      continue;
    }
    nscoord floatTop = fi.mRect.y, floatBottom = fi.mRect.YMost();
    if (floatTop > top) {
      // This float is below our band.  Shrink our band's height if needed.
      if (floatTop < bottom) {
        bottom = floatTop;
      }
    } else if (floatBottom > top) {
      // This float is in our band.

      // Shrink our band's height if needed.
      if (floatBottom < bottom) {
        bottom = floatBottom;
      }

      // Shrink our band's width if needed.
      if (fi.mFrame->GetStyleDisplay()->mFloats == NS_STYLE_FLOAT_LEFT) {
        // A left float.
        nscoord rightEdge = fi.mRect.XMost();
        if (rightEdge > left) {
          left = rightEdge;
          // Only set haveFloats to true if the float is inside our
          // containing block.  This matches the spec for what some
          // callers want and disagrees for other callers, so we should
          // probably provide better information at some point.
          haveFloats = PR_TRUE;
        }
      } else {
        // A right float.
        nscoord leftEdge = fi.mRect.x;
        if (leftEdge < right) {
          right = leftEdge;
          // See above.
          haveFloats = PR_TRUE;
        }
      }
    }
  }

  *aHasFloats = haveFloats;
  nscoord height = (bottom == nscoord_MAX) ? nscoord_MAX : (bottom - top);
  return nsRect(left - mX, top - mY, right - left, height);
}

nsresult
nsFloatManager::AddFloat(nsIFrame* aFloatFrame, const nsRect& aMarginRect)
{
  NS_ASSERTION(aMarginRect.width >= 0, "negative width!");
  NS_ASSERTION(aMarginRect.height >= 0, "negative height!");

  FloatInfo info(aFloatFrame, aMarginRect + nsPoint(mX, mY));

  // Set mLeftYMost and mRightYMost.
  if (HasAnyFloats()) {
    FloatInfo &tail = mFloats[mFloats.Length() - 1];
    info.mLeftYMost = tail.mLeftYMost;
    info.mRightYMost = tail.mRightYMost;
  } else {
    info.mLeftYMost = nscoord_MIN;
    info.mRightYMost = nscoord_MIN;
  }
  PRUint8 floatStyle = aFloatFrame->GetStyleDisplay()->mFloats;
  NS_ASSERTION(floatStyle == NS_STYLE_FLOAT_LEFT ||
               floatStyle == NS_STYLE_FLOAT_RIGHT, "unexpected float");
  nscoord& sideYMost = (floatStyle == NS_STYLE_FLOAT_LEFT) ? info.mLeftYMost
                                                           : info.mRightYMost;
  nscoord thisYMost = info.mRect.YMost();
  if (thisYMost > sideYMost)
    sideYMost = thisYMost;

  if (!mFloats.AppendElement(info))
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

nsresult
nsFloatManager::RemoveTrailingRegions(nsIFrame* aFrameList)
{
  if (!aFrameList) {
    return NS_OK;
  }
  // This could be a good bit simpler if we could guarantee that the
  // floats given were at the end of our list, so we could just search
  // for the head of aFrameList.  (But we can't;
  // layout/reftests/bugs/421710-1.html crashes.)
  nsVoidHashSet frameSet;

  frameSet.Init(1);
  for (nsIFrame* f = aFrameList; f; f = f->GetNextSibling()) {
    frameSet.Put(f);
  }

  PRUint32 newLength = mFloats.Length();
  while (newLength > 0) {
    if (!frameSet.Contains(mFloats[newLength - 1].mFrame)) {
      break;
    }
    --newLength;
  }
  mFloats.TruncateLength(newLength);

#ifdef DEBUG
  for (PRUint32 i = 0; i < mFloats.Length(); ++i) {
    NS_ASSERTION(!frameSet.Contains(mFloats[i].mFrame),
                 "Frame region deletion was requested but we couldn't delete it");
  }
#endif

  return NS_OK;
}

void
nsFloatManager::PushState(SavedState* aState)
{
  NS_PRECONDITION(aState, "Need a place to save state");

  // This is a cheap push implementation, which
  // only saves the (x,y) and last frame in the mFrameInfoMap
  // which is enough info to get us back to where we should be
  // when pop is called.
  //
  // This push/pop mechanism is used to undo any
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
  aState->mX = mX;
  aState->mY = mY;
  aState->mFloatInfoCount = mFloats.Length();
}

void
nsFloatManager::PopState(SavedState* aState)
{
  NS_PRECONDITION(aState, "No state to restore?");

  mX = aState->mX;
  mY = aState->mY;

  NS_ASSERTION(aState->mFloatInfoCount <= mFloats.Length(),
               "somebody misused PushState/PopState");
  mFloats.TruncateLength(aState->mFloatInfoCount);
}

nscoord
nsFloatManager::GetLowestFloatTop() const
{
  if (!HasAnyFloats()) {
    return nscoord_MIN;
  }
  return mFloats[mFloats.Length() - 1].mRect.y - mY;
}

#ifdef DEBUG
void
DebugListFloatManager(const nsFloatManager *aFloatManager)
{
  aFloatManager->List(stdout);
}

nsresult
nsFloatManager::List(FILE* out) const
{
  if (!HasAnyFloats())
    return NS_OK;

  for (PRUint32 i = 0; i < mFloats.Length(); ++i) {
    const FloatInfo &fi = mFloats[i];
    printf("Float %u: frame=%p rect={%d,%d,%d,%d} ymost={l:%d, r:%d}\n",
           i, static_cast<void*>(fi.mFrame),
           fi.mRect.x, fi.mRect.y, fi.mRect.width, fi.mRect.height,
           fi.mLeftYMost, fi.mRightYMost);
  }

  return NS_OK;
}
#endif

nscoord
nsFloatManager::ClearFloats(nscoord aY, PRUint8 aBreakType) const
{
  if (!HasAnyFloats()) {
    return aY;
  }

  nscoord bottom = aY + mY;

  const FloatInfo &tail = mFloats[mFloats.Length() - 1];
  switch (aBreakType) {
    case NS_STYLE_CLEAR_LEFT_AND_RIGHT:
      bottom = PR_MAX(bottom, tail.mLeftYMost);
      bottom = PR_MAX(bottom, tail.mRightYMost);
      break;
    case NS_STYLE_CLEAR_LEFT:
      bottom = PR_MAX(bottom, tail.mLeftYMost);
      break;
    case NS_STYLE_CLEAR_RIGHT:
      bottom = PR_MAX(bottom, tail.mRightYMost);
      break;
    default:
      // Do nothing
      break;
  }

  bottom -= mY;

  return bottom;
}

/////////////////////////////////////////////////////////////////////////////
// FloatInfo

nsFloatManager::FloatInfo::FloatInfo(nsIFrame* aFrame, const nsRect& aRect)
  : mFrame(aFrame), mRect(aRect)
{
  MOZ_COUNT_CTOR(nsFloatManager::FloatInfo);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsFloatManager::FloatInfo::FloatInfo(const FloatInfo& aOther)
  : mFrame(aOther.mFrame),
    mRect(aOther.mRect),
    mLeftYMost(aOther.mLeftYMost),
    mRightYMost(aOther.mRightYMost)
{
  MOZ_COUNT_CTOR(nsFloatManager::FloatInfo);
}

nsFloatManager::FloatInfo::~FloatInfo()
{
  MOZ_COUNT_DTOR(nsFloatManager::FloatInfo);
}
#endif

//----------------------------------------------------------------------

nsAutoFloatManager::~nsAutoFloatManager()
{
  // Restore the old float manager in the reflow state if necessary.
  if (mNew) {
#ifdef NOISY_FLOATMANAGER
    printf("restoring old float manager %p\n", mOld);
#endif

    mReflowState.mFloatManager = mOld;

#ifdef NOISY_FLOATMANAGER
    if (mOld) {
      static_cast<nsFrame *>(mReflowState.frame)->ListTag(stdout);
      printf(": space-manager %p after reflow\n", mOld);
      mOld->List(stdout);
    }
#endif

    delete mNew;
  }
}

nsresult
nsAutoFloatManager::CreateFloatManager(nsPresContext *aPresContext)
{
  // Create a new float manager and install it in the reflow
  // state. `Remember' the old float manager so we can restore it
  // later.
  mNew = new nsFloatManager(aPresContext->PresShell());
  if (! mNew)
    return NS_ERROR_OUT_OF_MEMORY;

#ifdef NOISY_FLOATMANAGER
  printf("constructed new float manager %p (replacing %p)\n",
         mNew, mReflowState.mFloatManager);
#endif

  // Set the float manager in the existing reflow state
  mOld = mReflowState.mFloatManager;
  mReflowState.mFloatManager = mNew;
  return NS_OK;
}
