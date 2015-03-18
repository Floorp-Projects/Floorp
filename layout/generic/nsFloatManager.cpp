/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* class that manages rules for positioning floats */

#include "nsFloatManager.h"
#include "nsIPresShell.h"
#include "nsMemory.h"
#include "nsHTMLReflowState.h"
#include "nsBlockDebugFlags.h"
#include "nsError.h"
#include <algorithm>

using namespace mozilla;

int32_t nsFloatManager::sCachedFloatManagerCount = 0;
void* nsFloatManager::sCachedFloatManagers[NS_FLOAT_MANAGER_CACHE_SIZE];

/////////////////////////////////////////////////////////////////////////////

// PresShell Arena allocate callback (for nsIntervalSet use below)
static void*
PSArenaAllocCB(size_t aSize, void* aClosure)
{
  return static_cast<nsIPresShell*>(aClosure)->AllocateMisc(aSize);
}

// PresShell Arena free callback (for nsIntervalSet use below)
static void
PSArenaFreeCB(size_t aSize, void* aPtr, void* aClosure)
{
  static_cast<nsIPresShell*>(aClosure)->FreeMisc(aSize, aPtr);
}

/////////////////////////////////////////////////////////////////////////////
// nsFloatManager

nsFloatManager::nsFloatManager(nsIPresShell* aPresShell,
                               mozilla::WritingMode aWM)
  : mWritingMode(aWM),
    mOrigin(aWM),
    mFloatDamage(PSArenaAllocCB, PSArenaFreeCB, aPresShell),
    mPushedLeftFloatPastBreak(false),
    mPushedRightFloatPastBreak(false),
    mSplitLeftFloatAcrossBreak(false),
    mSplitRightFloatAcrossBreak(false)
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

  int32_t i;

  for (i = 0; i < sCachedFloatManagerCount; i++) {
    void* floatManager = sCachedFloatManagers[i];
    if (floatManager)
      nsMemory::Free(floatManager);
  }

  // Disable further caching.
  sCachedFloatManagerCount = -1;
}

nsFlowAreaRect
nsFloatManager::GetFlowArea(WritingMode aWM, nscoord aBOffset,
                            BandInfoType aInfoType, nscoord aBSize,
                            LogicalRect aContentArea, SavedState* aState,
                            nscoord aContainerWidth) const
{
  NS_ASSERTION(aBSize >= 0, "unexpected max block size");
  NS_ASSERTION(aContentArea.ISize(aWM) >= 0,
               "unexpected content area inline size");

  LogicalPoint origin = mOrigin.ConvertTo(aWM, mWritingMode, aContainerWidth);
  nscoord blockStart = aBOffset + origin.B(aWM);
  if (blockStart < nscoord_MIN) {
    NS_WARNING("bad value");
    blockStart = nscoord_MIN;
  }

  // Determine the last float that we should consider.
  uint32_t floatCount;
  if (aState) {
    // Use the provided state.
    floatCount = aState->mFloatInfoCount;
    NS_ABORT_IF_FALSE(floatCount <= mFloats.Length(), "bad state");
  } else {
    // Use our current state.
    floatCount = mFloats.Length();
  }

  // If there are no floats at all, or we're below the last one, return
  // quickly.
  if (floatCount == 0 ||
      (mFloats[floatCount-1].mLeftBEnd <= blockStart &&
       mFloats[floatCount-1].mRightBEnd <= blockStart)) {
    //XXX temporary!
    LogicalRect rect(aWM, aContentArea.IStart(aWM), aBOffset,
                     aContentArea.ISize(aWM), aBSize);
    nsRect phys = rect.GetPhysicalRect(aWM, aContainerWidth);
    return nsFlowAreaRect(phys.x, phys.y, phys.width, phys.height, false);
  }

  nscoord blockEnd;
  if (aBSize == nscoord_MAX) {
    // This warning (and the two below) are possible to hit on pages
    // with really large objects.
    NS_WARN_IF_FALSE(aInfoType == BAND_FROM_POINT,
                     "bad height");
    blockEnd = nscoord_MAX;
  } else {
    blockEnd = blockStart + aBSize;
    if (blockEnd < blockStart || blockEnd > nscoord_MAX) {
      NS_WARNING("bad value");
      blockEnd = nscoord_MAX;
    }
  }
  nscoord inlineStart = origin.I(aWM) + aContentArea.IStart(aWM);
  nscoord inlineEnd = origin.I(aWM) + aContentArea.IEnd(aWM);
  if (inlineEnd < inlineStart) {
    NS_WARNING("bad value");
    inlineEnd = inlineStart;
  }

  // Walk backwards through the floats until we either hit the front of
  // the list or we're above |blockStart|.
  bool haveFloats = false;
  for (uint32_t i = floatCount; i > 0; --i) {
    const FloatInfo &fi = mFloats[i-1];
    if (fi.mLeftBEnd <= blockStart && fi.mRightBEnd <= blockStart) {
      // There aren't any more floats that could intersect this band.
      break;
    }
    if (fi.mRect.IsEmpty()) {
      // For compatibility, ignore floats with empty rects, even though it
      // disagrees with the spec.  (We might want to fix this in the
      // future, though.)
      continue;
    }

    LogicalRect rect = fi.mRect.ConvertTo(aWM, fi.mWritingMode,
                                          aContainerWidth);
    nscoord floatBStart = rect.BStart(aWM);
    nscoord floatBEnd = rect.BEnd(aWM);
    if (blockStart < floatBStart && aInfoType == BAND_FROM_POINT) {
      // This float is below our band.  Shrink our band's height if needed.
      if (floatBStart < blockEnd) {
        blockEnd = floatBStart;
      }
    }
    // If blockStart == blockEnd (which happens only with WIDTH_WITHIN_HEIGHT),
    // we include floats that begin at our 0-height vertical area.  We
    // need to to this to satisfy the invariant that a
    // WIDTH_WITHIN_HEIGHT call is at least as narrow on both sides as a
    // BAND_WITHIN_POINT call beginning at its blockStart.
    else if (blockStart < floatBEnd &&
             (floatBStart < blockEnd ||
              (floatBStart == blockEnd && blockStart == blockEnd))) {
      // This float is in our band.

      // Shrink our band's height if needed.
      if (floatBEnd < blockEnd && aInfoType == BAND_FROM_POINT) {
        blockEnd = floatBEnd;
      }

      // Shrink our band's width if needed.
      if ((fi.mFrame->StyleDisplay()->mFloats == NS_STYLE_FLOAT_LEFT) ==
          aWM.IsBidiLTR()) {
        // A left float in an ltr block or a right float in an rtl block
        nscoord inlineEndEdge = rect.IEnd(aWM);
        if (inlineEndEdge > inlineStart) {
          inlineStart = inlineEndEdge;
          // Only set haveFloats to true if the float is inside our
          // containing block.  This matches the spec for what some
          // callers want and disagrees for other callers, so we should
          // probably provide better information at some point.
          haveFloats = true;
        }
      } else {
        // A left float in an rtl block or a right float in an ltr block
        nscoord inlineStartEdge = rect.IStart(aWM);
        if (inlineStartEdge < inlineEnd) {
          inlineEnd = inlineStartEdge;
          // See above.
          haveFloats = true;
        }
      }
    }
  }

  nscoord height = (blockEnd == nscoord_MAX) ?
                    nscoord_MAX : (blockEnd - blockStart);
  //XXX temporary!
  LogicalRect rect(aWM,
                   inlineStart - origin.I(aWM), blockStart - origin.B(aWM),
                   inlineEnd - inlineStart, height);
  nsRect phys = rect.GetPhysicalRect(aWM, aContainerWidth);
  return nsFlowAreaRect(phys.x, phys.y, phys.width, phys.height, haveFloats);
}

nsresult
nsFloatManager::AddFloat(nsIFrame* aFloatFrame, const LogicalRect& aMarginRect,
                         WritingMode aWM, nscoord aContainerWidth)
{
  NS_ASSERTION(aMarginRect.ISize(aWM) >= 0, "negative inline size!");
  NS_ASSERTION(aMarginRect.BSize(aWM) >= 0, "negative block size!");

  FloatInfo info(aFloatFrame, aWM, aMarginRect + mOrigin);

  // Set mLeftBEnd and mRightBEnd.
  if (HasAnyFloats()) {
    FloatInfo &tail = mFloats[mFloats.Length() - 1];
    info.mLeftBEnd = tail.mLeftBEnd;
    info.mRightBEnd = tail.mRightBEnd;
  } else {
    info.mLeftBEnd = nscoord_MIN;
    info.mRightBEnd = nscoord_MIN;
  }
  uint8_t floatStyle = aFloatFrame->StyleDisplay()->mFloats;
  NS_ASSERTION(floatStyle == NS_STYLE_FLOAT_LEFT ||
               floatStyle == NS_STYLE_FLOAT_RIGHT, "unexpected float");
  nscoord& sideBEnd =
    ((floatStyle == NS_STYLE_FLOAT_LEFT) == aWM.IsBidiLTR()) ? info.mLeftBEnd
                                                             : info.mRightBEnd;
  nscoord thisBEnd = info.mRect.BEnd(aWM);
  if (thisBEnd > sideBEnd)
    sideBEnd = thisBEnd;

  if (!mFloats.AppendElement(info))
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

LogicalRect
nsFloatManager::CalculateRegionFor(WritingMode          aWM,
                                   nsIFrame*            aFloat,
                                   const LogicalMargin& aMargin,
                                   nscoord              aContainerWidth)
{
  // We consider relatively positioned frames at their original position.
  LogicalRect region(aWM, nsRect(aFloat->GetNormalPosition(),
                                 aFloat->GetSize()),
                     aContainerWidth);

  // Float region includes its margin
  region.Inflate(aWM, aMargin);

  // Don't store rectangles with negative margin-box width or height in
  // the float manager; it can't deal with them.
  if (region.ISize(aWM) < 0) {
    // Preserve the right margin-edge for left floats and the left
    // margin-edge for right floats
    const nsStyleDisplay* display = aFloat->StyleDisplay();
    if ((NS_STYLE_FLOAT_LEFT == display->mFloats) == aWM.IsBidiLTR()) {
      region.IStart(aWM) = region.IEnd(aWM);
    }
    region.ISize(aWM) = 0;
  }
  if (region.BSize(aWM) < 0) {
    region.BSize(aWM) = 0;
  }
  return region;
}

NS_DECLARE_FRAME_PROPERTY(FloatRegionProperty, nsIFrame::DestroyMargin)

LogicalRect
nsFloatManager::GetRegionFor(WritingMode aWM, nsIFrame* aFloat,
                             nscoord aContainerWidth)
{
  LogicalRect region = aFloat->GetLogicalRect(aWM, aContainerWidth);
  void* storedRegion = aFloat->Properties().Get(FloatRegionProperty());
  if (storedRegion) {
    nsMargin margin = *static_cast<nsMargin*>(storedRegion);
    region.Inflate(aWM, LogicalMargin(aWM, margin));
  }
  return region;
}

void
nsFloatManager::StoreRegionFor(WritingMode aWM, nsIFrame* aFloat,
                               const LogicalRect& aRegion,
                               nscoord aContainerWidth)
{
  nsRect region = aRegion.GetPhysicalRect(aWM, aContainerWidth);
  nsRect rect = aFloat->GetRect();
  FrameProperties props = aFloat->Properties();
  if (region.IsEqualEdges(rect)) {
    props.Delete(FloatRegionProperty());
  }
  else {
    nsMargin* storedMargin = static_cast<nsMargin*>
      (props.Get(FloatRegionProperty()));
    if (!storedMargin) {
      storedMargin = new nsMargin();
      props.Set(FloatRegionProperty(), storedMargin);
    }
    *storedMargin = region - rect;
  }
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
  nsTHashtable<nsPtrHashKey<nsIFrame> > frameSet(1);

  for (nsIFrame* f = aFrameList; f; f = f->GetNextSibling()) {
    frameSet.PutEntry(f);
  }

  uint32_t newLength = mFloats.Length();
  while (newLength > 0) {
    if (!frameSet.Contains(mFloats[newLength - 1].mFrame)) {
      break;
    }
    --newLength;
  }
  mFloats.TruncateLength(newLength);

#ifdef DEBUG
  for (uint32_t i = 0; i < mFloats.Length(); ++i) {
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
  aState->mWritingMode = mWritingMode;
  aState->mOrigin = mOrigin;
  aState->mPushedLeftFloatPastBreak = mPushedLeftFloatPastBreak;
  aState->mPushedRightFloatPastBreak = mPushedRightFloatPastBreak;
  aState->mSplitLeftFloatAcrossBreak = mSplitLeftFloatAcrossBreak;
  aState->mSplitRightFloatAcrossBreak = mSplitRightFloatAcrossBreak;
  aState->mFloatInfoCount = mFloats.Length();
}

void
nsFloatManager::PopState(SavedState* aState)
{
  NS_PRECONDITION(aState, "No state to restore?");

  mWritingMode = aState->mWritingMode;
  mOrigin = aState->mOrigin;
  mPushedLeftFloatPastBreak = aState->mPushedLeftFloatPastBreak;
  mPushedRightFloatPastBreak = aState->mPushedRightFloatPastBreak;
  mSplitLeftFloatAcrossBreak = aState->mSplitLeftFloatAcrossBreak;
  mSplitRightFloatAcrossBreak = aState->mSplitRightFloatAcrossBreak;

  NS_ASSERTION(aState->mFloatInfoCount <= mFloats.Length(),
               "somebody misused PushState/PopState");
  mFloats.TruncateLength(aState->mFloatInfoCount);
}

nscoord
nsFloatManager::GetLowestFloatTop(WritingMode aWM,
                                  nscoord aContainerWidth) const
{
  if (mPushedLeftFloatPastBreak || mPushedRightFloatPastBreak) {
    return nscoord_MAX;
  }
  if (!HasAnyFloats()) {
    return nscoord_MIN;
  }
  FloatInfo fi = mFloats[mFloats.Length() - 1];
  LogicalRect rect = fi.mRect.ConvertTo(aWM, fi.mWritingMode, aContainerWidth);
  LogicalPoint origin = mOrigin.ConvertTo(aWM, mWritingMode, aContainerWidth);

  return rect.BStart(aWM) - origin.B(aWM);
}

#ifdef DEBUG_FRAME_DUMP
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

  for (uint32_t i = 0; i < mFloats.Length(); ++i) {
    const FloatInfo &fi = mFloats[i];
    fprintf_stderr(out, "Float %u: frame=%p rect={%d,%d,%d,%d} ymost={l:%d, r:%d}\n",
                   i, static_cast<void*>(fi.mFrame),
                   fi.mRect.IStart(fi.mWritingMode),
                   fi.mRect.BStart(fi.mWritingMode),
                   fi.mRect.ISize(fi.mWritingMode),
                   fi.mRect.BSize(fi.mWritingMode),
                   fi.mLeftBEnd, fi.mRightBEnd);
  }
  return NS_OK;
}
#endif

nscoord
nsFloatManager::ClearFloats(WritingMode aWM, nscoord aBCoord,
                            uint8_t aBreakType, nscoord aContainerWidth,
                            uint32_t aFlags) const
{
  if (!(aFlags & DONT_CLEAR_PUSHED_FLOATS) && ClearContinues(aBreakType)) {
    return nscoord_MAX;
  }
  if (!HasAnyFloats()) {
    return aBCoord;
  }

  LogicalPoint origin = mOrigin.ConvertTo(aWM, mWritingMode, aContainerWidth);
  nscoord blockEnd = aBCoord + origin.B(aWM);

  const FloatInfo &tail = mFloats[mFloats.Length() - 1];
  switch (aBreakType) {
    case NS_STYLE_CLEAR_BOTH:
      blockEnd = std::max(blockEnd, tail.mLeftBEnd);
      blockEnd = std::max(blockEnd, tail.mRightBEnd);
      break;
    case NS_STYLE_CLEAR_LEFT:
      blockEnd = std::max(blockEnd, aWM.IsBidiLTR() ? tail.mLeftBEnd
                                                    : tail.mRightBEnd);
      break;
    case NS_STYLE_CLEAR_RIGHT:
      blockEnd = std::max(blockEnd, aWM.IsBidiLTR() ? tail.mRightBEnd
                                                    : tail.mLeftBEnd);
      break;
    default:
      // Do nothing
      break;
  }

  blockEnd -= origin.B(aWM);

  return blockEnd;
}

bool
nsFloatManager::ClearContinues(uint8_t aBreakType) const
{
  return ((mPushedLeftFloatPastBreak || mSplitLeftFloatAcrossBreak) &&
          (aBreakType == NS_STYLE_CLEAR_BOTH ||
           aBreakType == NS_STYLE_CLEAR_LEFT)) ||
         ((mPushedRightFloatPastBreak || mSplitRightFloatAcrossBreak) &&
          (aBreakType == NS_STYLE_CLEAR_BOTH ||
           aBreakType == NS_STYLE_CLEAR_RIGHT));
}

/////////////////////////////////////////////////////////////////////////////
// FloatInfo

nsFloatManager::FloatInfo::FloatInfo(nsIFrame* aFrame, WritingMode aWM,
                                     const LogicalRect& aRect)
  : mFrame(aFrame), mRect(aRect), mWritingMode(aWM)
{
  MOZ_COUNT_CTOR(nsFloatManager::FloatInfo);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsFloatManager::FloatInfo::FloatInfo(const FloatInfo& aOther)
  : mFrame(aOther.mFrame),
    mRect(aOther.mRect),
    mWritingMode(aOther.mWritingMode),
    mLeftBEnd(aOther.mLeftBEnd),
    mRightBEnd(aOther.mRightBEnd)
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
  mNew = new nsFloatManager(aPresContext->PresShell(),
                            mReflowState.GetWritingMode());
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
