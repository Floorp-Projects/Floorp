/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FrameBlender.h"

#include "mozilla/MemoryReporting.h"
#include "RasterImage.h"

#include "pixman.h"

using namespace mozilla;
using namespace mozilla::image;

namespace mozilla {
namespace image {

FrameBlender::FrameBlender(FrameSequence* aSequenceToUse /* = nullptr */)
 : mFrames(aSequenceToUse)
 , mAnim(nullptr)
{
  if (!mFrames) {
    mFrames = new FrameSequence();
  }
}

FrameBlender::~FrameBlender()
{
  delete mAnim;
}

already_AddRefed<FrameSequence>
FrameBlender::GetFrameSequence()
{
  nsRefPtr<FrameSequence> seq(mFrames);
  return seq.forget();
}

imgFrame*
FrameBlender::GetFrame(uint32_t framenum) const
{
  if (!mAnim) {
    NS_ASSERTION(framenum == 0, "Don't ask for a frame > 0 if we're not animated!");
    return mFrames->GetFrame(0);
  }
  if (mAnim->lastCompositedFrameIndex == int32_t(framenum))
    return mAnim->compositingFrame;
  return mFrames->GetFrame(framenum);
}

imgFrame*
FrameBlender::RawGetFrame(uint32_t framenum) const
{
  if (!mAnim) {
    NS_ASSERTION(framenum == 0, "Don't ask for a frame > 0 if we're not animated!");
    return mFrames->GetFrame(0);
  }

  return mFrames->GetFrame(framenum);
}

uint32_t
FrameBlender::GetNumFrames() const
{
  return mFrames->GetNumFrames();
}

void
FrameBlender::RemoveFrame(uint32_t framenum)
{
  NS_ABORT_IF_FALSE(framenum < GetNumFrames(), "Deleting invalid frame!");

  mFrames->RemoveFrame(framenum);
}

void
FrameBlender::ClearFrames()
{
  // Forget our old frame sequence, letting whoever else has it deal with it.
  mFrames = new FrameSequence();
}

void
FrameBlender::InsertFrame(uint32_t framenum, imgFrame* aFrame)
{
  NS_ABORT_IF_FALSE(framenum <= GetNumFrames(), "Inserting invalid frame!");
  mFrames->InsertFrame(framenum, aFrame);
  if (GetNumFrames() > 1) {
    EnsureAnimExists();
  }
}

imgFrame*
FrameBlender::SwapFrame(uint32_t framenum, imgFrame* aFrame)
{
  NS_ABORT_IF_FALSE(framenum < GetNumFrames(), "Swapping invalid frame!");

  imgFrame* ret;

  // Steal the imgFrame from wherever it's currently stored
  if (mAnim && mAnim->lastCompositedFrameIndex == int32_t(framenum)) {
    ret = mAnim->compositingFrame.Forget();
    mAnim->lastCompositedFrameIndex = -1;
    nsAutoPtr<imgFrame> toDelete(mFrames->SwapFrame(framenum, aFrame));
  } else {
    ret = mFrames->SwapFrame(framenum, aFrame);
  }

  return ret;
}

void
FrameBlender::EnsureAnimExists()
{
  if (!mAnim) {
    // Create the animation context
    mAnim = new Anim();

    // We should only get into this code path directly after we've created our
    // second frame (hence we know we're animated).
    MOZ_ASSERT(GetNumFrames() == 2);
  }
}

//******************************************************************************
// DoBlend gets called when the timer for animation get fired and we have to
// update the composited frame of the animation.
bool
FrameBlender::DoBlend(nsIntRect* aDirtyRect,
                      uint32_t aPrevFrameIndex,
                      uint32_t aNextFrameIndex)
{
  if (!aDirtyRect) {
    return false;
  }

  const FrameDataPair& prevFrame = mFrames->GetFrame(aPrevFrameIndex);
  const FrameDataPair& nextFrame = mFrames->GetFrame(aNextFrameIndex);
  if (!prevFrame.HasFrameData() || !nextFrame.HasFrameData()) {
    return false;
  }

  int32_t prevFrameDisposalMethod = prevFrame->GetFrameDisposalMethod();
  if (prevFrameDisposalMethod == FrameBlender::kDisposeRestorePrevious &&
      !mAnim->compositingPrevFrame)
    prevFrameDisposalMethod = FrameBlender::kDisposeClear;

  nsIntRect prevFrameRect = prevFrame->GetRect();
  bool isFullPrevFrame = (prevFrameRect.x == 0 && prevFrameRect.y == 0 &&
                          prevFrameRect.width == mSize.width &&
                          prevFrameRect.height == mSize.height);

  // Optimization: DisposeClearAll if the previous frame is the same size as
  //               container and it's clearing itself
  if (isFullPrevFrame &&
      (prevFrameDisposalMethod == FrameBlender::kDisposeClear))
    prevFrameDisposalMethod = FrameBlender::kDisposeClearAll;

  int32_t nextFrameDisposalMethod = nextFrame->GetFrameDisposalMethod();
  nsIntRect nextFrameRect = nextFrame->GetRect();
  bool isFullNextFrame = (nextFrameRect.x == 0 && nextFrameRect.y == 0 &&
                          nextFrameRect.width == mSize.width &&
                          nextFrameRect.height == mSize.height);

  if (!nextFrame->GetIsPaletted()) {
    // Optimization: Skip compositing if the previous frame wants to clear the
    //               whole image
    if (prevFrameDisposalMethod == FrameBlender::kDisposeClearAll) {
      aDirtyRect->SetRect(0, 0, mSize.width, mSize.height);
      return true;
    }

    // Optimization: Skip compositing if this frame is the same size as the
    //               container and it's fully drawing over prev frame (no alpha)
    if (isFullNextFrame &&
        (nextFrameDisposalMethod != FrameBlender::kDisposeRestorePrevious) &&
        !nextFrame->GetHasAlpha()) {
      aDirtyRect->SetRect(0, 0, mSize.width, mSize.height);
      return true;
    }
  }

  // Calculate area that needs updating
  switch (prevFrameDisposalMethod) {
    default:
    case FrameBlender::kDisposeNotSpecified:
    case FrameBlender::kDisposeKeep:
      *aDirtyRect = nextFrameRect;
      break;

    case FrameBlender::kDisposeClearAll:
      // Whole image container is cleared
      aDirtyRect->SetRect(0, 0, mSize.width, mSize.height);
      break;

    case FrameBlender::kDisposeClear:
      // Calc area that needs to be redrawn (the combination of previous and
      // this frame)
      // XXX - This could be done with multiple framechanged calls
      //       Having prevFrame way at the top of the image, and nextFrame
      //       way at the bottom, and both frames being small, we'd be
      //       telling framechanged to refresh the whole image when only two
      //       small areas are needed.
      aDirtyRect->UnionRect(nextFrameRect, prevFrameRect);
      break;

    case FrameBlender::kDisposeRestorePrevious:
      aDirtyRect->SetRect(0, 0, mSize.width, mSize.height);
      break;
  }

  // Optimization:
  //   Skip compositing if the last composited frame is this frame
  //   (Only one composited frame was made for this animation.  Example:
  //    Only Frame 3 of a 10 frame image required us to build a composite frame
  //    On the second loop, we do not need to rebuild the frame
  //    since it's still sitting in compositingFrame)
  if (mAnim->lastCompositedFrameIndex == int32_t(aNextFrameIndex)) {
    return true;
  }

  bool needToBlankComposite = false;

  // Create the Compositing Frame
  if (!mAnim->compositingFrame) {
    mAnim->compositingFrame.SetFrame(new imgFrame());
    nsresult rv = mAnim->compositingFrame->Init(0, 0, mSize.width, mSize.height,
                                                gfxASurface::ImageFormatARGB32);
    if (NS_FAILED(rv)) {
      mAnim->compositingFrame.SetFrame(nullptr);
      return false;
    }
    mAnim->compositingFrame.LockAndGetData();
    needToBlankComposite = true;
  } else if (int32_t(aNextFrameIndex) != mAnim->lastCompositedFrameIndex+1) {

    // If we are not drawing on top of last composited frame,
    // then we are building a new composite frame, so let's clear it first.
    needToBlankComposite = true;
  }

  // More optimizations possible when next frame is not transparent
  // But if the next frame has FrameBlender::kDisposeRestorePrevious,
  // this "no disposal" optimization is not possible,
  // because the frame in "after disposal operation" state
  // needs to be stored in compositingFrame, so it can be
  // copied into compositingPrevFrame later.
  bool doDisposal = true;
  if (!nextFrame->GetHasAlpha() &&
      nextFrameDisposalMethod != FrameBlender::kDisposeRestorePrevious) {
    if (isFullNextFrame) {
      // Optimization: No need to dispose prev.frame when
      // next frame is full frame and not transparent.
      doDisposal = false;
      // No need to blank the composite frame
      needToBlankComposite = false;
    } else {
      if ((prevFrameRect.x >= nextFrameRect.x) &&
          (prevFrameRect.y >= nextFrameRect.y) &&
          (prevFrameRect.x + prevFrameRect.width <= nextFrameRect.x + nextFrameRect.width) &&
          (prevFrameRect.y + prevFrameRect.height <= nextFrameRect.y + nextFrameRect.height)) {
        // Optimization: No need to dispose prev.frame when
        // next frame fully overlaps previous frame.
        doDisposal = false;
      }
    }
  }

  if (doDisposal) {
    // Dispose of previous: clear, restore, or keep (copy)
    switch (prevFrameDisposalMethod) {
      case FrameBlender::kDisposeClear:
        if (needToBlankComposite) {
          // If we just created the composite, it could have anything in its
          // buffer. Clear whole frame
          ClearFrame(mAnim->compositingFrame.GetFrameData(),
                     mAnim->compositingFrame.GetFrame()->GetRect());
        } else {
          // Only blank out previous frame area (both color & Mask/Alpha)
          ClearFrame(mAnim->compositingFrame.GetFrameData(),
                     mAnim->compositingFrame.GetFrame()->GetRect(),
                     prevFrameRect);
        }
        break;

      case FrameBlender::kDisposeClearAll:
        ClearFrame(mAnim->compositingFrame.GetFrameData(),
                   mAnim->compositingFrame.GetFrame()->GetRect());
        break;

      case FrameBlender::kDisposeRestorePrevious:
        // It would be better to copy only the area changed back to
        // compositingFrame.
        if (mAnim->compositingPrevFrame) {
          CopyFrameImage(mAnim->compositingPrevFrame.GetFrameData(),
                         mAnim->compositingPrevFrame.GetFrame()->GetRect(),
                         mAnim->compositingFrame.GetFrameData(),
                         mAnim->compositingFrame.GetFrame()->GetRect());

          // destroy only if we don't need it for this frame's disposal
          if (nextFrameDisposalMethod != FrameBlender::kDisposeRestorePrevious)
            mAnim->compositingPrevFrame.SetFrame(nullptr);
        } else {
          ClearFrame(mAnim->compositingFrame.GetFrameData(),
                     mAnim->compositingFrame.GetFrame()->GetRect());
        }
        break;

      default:
        // Copy previous frame into compositingFrame before we put the new frame on top
        // Assumes that the previous frame represents a full frame (it could be
        // smaller in size than the container, as long as the frame before it erased
        // itself)
        // Note: Frame 1 never gets into DoBlend(), so (aNextFrameIndex - 1) will
        // always be a valid frame number.
        if (mAnim->lastCompositedFrameIndex != int32_t(aNextFrameIndex - 1)) {
          if (isFullPrevFrame && !prevFrame->GetIsPaletted()) {
            // Just copy the bits
            CopyFrameImage(prevFrame.GetFrameData(),
                           prevFrame.GetFrame()->GetRect(),
                           mAnim->compositingFrame.GetFrameData(),
                           mAnim->compositingFrame.GetFrame()->GetRect());
          } else {
            if (needToBlankComposite) {
              // Only blank composite when prev is transparent or not full.
              if (prevFrame->GetHasAlpha() || !isFullPrevFrame) {
                ClearFrame(mAnim->compositingFrame.GetFrameData(),
                           mAnim->compositingFrame.GetFrame()->GetRect());
              }
            }
            DrawFrameTo(prevFrame.GetFrameData(), prevFrameRect,
                        prevFrame.GetFrame()->PaletteDataLength(),
                        prevFrame.GetFrame()->GetHasAlpha(),
                        mAnim->compositingFrame.GetFrameData(),
                        mAnim->compositingFrame.GetFrame()->GetRect(),
                        FrameBlendMethod(prevFrame.GetFrame()->GetBlendMethod()));
          }
        }
    }
  } else if (needToBlankComposite) {
    // If we just created the composite, it could have anything in it's
    // buffers. Clear them
    ClearFrame(mAnim->compositingFrame.GetFrameData(),
               mAnim->compositingFrame.GetFrame()->GetRect());
  }

  // Check if the frame we are composing wants the previous image restored afer
  // it is done. Don't store it (again) if last frame wanted its image restored
  // too
  if ((nextFrameDisposalMethod == FrameBlender::kDisposeRestorePrevious) &&
      (prevFrameDisposalMethod != FrameBlender::kDisposeRestorePrevious)) {
    // We are storing the whole image.
    // It would be better if we just stored the area that nextFrame is going to
    // overwrite.
    if (!mAnim->compositingPrevFrame) {
      mAnim->compositingPrevFrame.SetFrame(new imgFrame());
      nsresult rv = mAnim->compositingPrevFrame->Init(0, 0, mSize.width, mSize.height,
                                                      gfxASurface::ImageFormatARGB32);
      if (NS_FAILED(rv)) {
        mAnim->compositingPrevFrame.SetFrame(nullptr);
        return false;
      }

      mAnim->compositingPrevFrame.LockAndGetData();
    }

    CopyFrameImage(mAnim->compositingFrame.GetFrameData(),
                   mAnim->compositingFrame.GetFrame()->GetRect(),
                   mAnim->compositingPrevFrame.GetFrameData(),
                   mAnim->compositingPrevFrame.GetFrame()->GetRect());
  }

  // blit next frame into it's correct spot
  DrawFrameTo(nextFrame.GetFrameData(), nextFrameRect,
              nextFrame.GetFrame()->PaletteDataLength(),
              nextFrame.GetFrame()->GetHasAlpha(),
              mAnim->compositingFrame.GetFrameData(),
              mAnim->compositingFrame.GetFrame()->GetRect(),
              FrameBlendMethod(nextFrame.GetFrame()->GetBlendMethod()));

  // Set timeout of CompositeFrame to timeout of frame we just composed
  // Bug 177948
  int32_t timeout = nextFrame->GetTimeout();
  mAnim->compositingFrame->SetTimeout(timeout);

  // Tell the image that it is fully 'downloaded'.
  nsresult rv = mAnim->compositingFrame->ImageUpdated(mAnim->compositingFrame->GetRect());
  if (NS_FAILED(rv)) {
    return false;
  }

  mAnim->lastCompositedFrameIndex = int32_t(aNextFrameIndex);

  return true;
}

//******************************************************************************
// Fill aFrame with black. Does also clears the mask.
void
FrameBlender::ClearFrame(uint8_t* aFrameData, const nsIntRect& aFrameRect)
{
  if (!aFrameData)
    return;

  memset(aFrameData, 0, aFrameRect.width * aFrameRect.height * 4);
}

//******************************************************************************
void
FrameBlender::ClearFrame(uint8_t* aFrameData, const nsIntRect& aFrameRect, const nsIntRect& aRectToClear)
{
  if (!aFrameData || aFrameRect.width <= 0 || aFrameRect.height <= 0 ||
      aRectToClear.width <= 0 || aRectToClear.height <= 0) {
    return;
  }

  nsIntRect toClear = aFrameRect.Intersect(aRectToClear);
  if (toClear.IsEmpty()) {
    return;
  }

  uint32_t bytesPerRow = aFrameRect.width * 4;
  for (int row = toClear.y; row < toClear.y + toClear.height; ++row) {
    memset(aFrameData + toClear.x * 4 + row * bytesPerRow, 0, toClear.width * 4);
  }
}

//******************************************************************************
// Whether we succeed or fail will not cause a crash, and there's not much
// we can do about a failure, so there we don't return a nsresult
bool
FrameBlender::CopyFrameImage(const uint8_t *aDataSrc, const nsIntRect& aRectSrc,
                             uint8_t *aDataDest, const nsIntRect& aRectDest)
{
  uint32_t dataLengthSrc = aRectSrc.width * aRectSrc.height * 4;
  uint32_t dataLengthDest = aRectDest.width * aRectDest.height * 4;

  if (!aDataDest || !aDataSrc || dataLengthSrc != dataLengthDest) {
    return false;
  }

  memcpy(aDataDest, aDataSrc, dataLengthDest);

  return true;
}

nsresult
FrameBlender::DrawFrameTo(const uint8_t *aSrcData, const nsIntRect& aSrcRect,
                          uint32_t aSrcPaletteLength, bool aSrcHasAlpha,
                          uint8_t *aDstPixels, const nsIntRect& aDstRect,
                          FrameBlender::FrameBlendMethod aBlendMethod)
{
  NS_ENSURE_ARG_POINTER(aSrcData);
  NS_ENSURE_ARG_POINTER(aDstPixels);

  // According to both AGIF and APNG specs, offsets are unsigned
  if (aSrcRect.x < 0 || aSrcRect.y < 0) {
    NS_WARNING("FrameBlender::DrawFrameTo: negative offsets not allowed");
    return NS_ERROR_FAILURE;
  }
  // Outside the destination frame, skip it
  if ((aSrcRect.x > aDstRect.width) || (aSrcRect.y > aDstRect.height)) {
    return NS_OK;
  }

  if (aSrcPaletteLength) {
    // Larger than the destination frame, clip it
    int32_t width = std::min(aSrcRect.width, aDstRect.width - aSrcRect.x);
    int32_t height = std::min(aSrcRect.height, aDstRect.height - aSrcRect.y);

    // The clipped image must now fully fit within destination image frame
    NS_ASSERTION((aSrcRect.x >= 0) && (aSrcRect.y >= 0) &&
                 (aSrcRect.x + width <= aDstRect.width) &&
                 (aSrcRect.y + height <= aDstRect.height),
                "FrameBlender::DrawFrameTo: Invalid aSrcRect");

    // clipped image size may be smaller than source, but not larger
    NS_ASSERTION((width <= aSrcRect.width) && (height <= aSrcRect.height),
                 "FrameBlender::DrawFrameTo: source must be smaller than dest");

    // Get pointers to image data
    const uint8_t *srcPixels = aSrcData + aSrcPaletteLength;
    uint32_t *dstPixels = reinterpret_cast<uint32_t*>(aDstPixels);
    const uint32_t *colormap = reinterpret_cast<const uint32_t*>(aSrcData);

    // Skip to the right offset
    dstPixels += aSrcRect.x + (aSrcRect.y * aDstRect.width);
    if (!aSrcHasAlpha) {
      for (int32_t r = height; r > 0; --r) {
        for (int32_t c = 0; c < width; c++) {
          dstPixels[c] = colormap[srcPixels[c]];
        }
        // Go to the next row in the source resp. destination image
        srcPixels += aSrcRect.width;
        dstPixels += aDstRect.width;
      }
    } else {
      for (int32_t r = height; r > 0; --r) {
        for (int32_t c = 0; c < width; c++) {
          const uint32_t color = colormap[srcPixels[c]];
          if (color)
            dstPixels[c] = color;
        }
        // Go to the next row in the source resp. destination image
        srcPixels += aSrcRect.width;
        dstPixels += aDstRect.width;
      }
    }
  } else {
    pixman_image_t* src = pixman_image_create_bits(aSrcHasAlpha ? PIXMAN_a8r8g8b8 : PIXMAN_x8r8g8b8,
                                                   aSrcRect.width,
                                                   aSrcRect.height,
                                                   reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(aSrcData)),
                                                   aSrcRect.width * 4);
    pixman_image_t* dst = pixman_image_create_bits(PIXMAN_a8r8g8b8,
                                                   aDstRect.width,
                                                   aDstRect.height,
                                                   reinterpret_cast<uint32_t*>(aDstPixels),
                                                   aDstRect.width * 4);

    pixman_image_composite32(aBlendMethod == FrameBlender::kBlendSource ? PIXMAN_OP_SRC : PIXMAN_OP_OVER,
                             src,
                             nullptr,
                             dst,
                             0, 0,
                             0, 0,
                             aSrcRect.x, aSrcRect.y,
                             aSrcRect.width, aSrcRect.height);

    pixman_image_unref(src);
    pixman_image_unref(dst);
  }

  return NS_OK;
}

void
FrameBlender::Discard()
{
  MOZ_ASSERT(NS_IsMainThread());

  // As soon as an image becomes animated, it becomes non-discardable and any
  // timers are cancelled.
  NS_ABORT_IF_FALSE(!mAnim, "Asked to discard for animated image!");

  // Delete all the decoded frames, then clear the array.
  ClearFrames();
}

size_t
FrameBlender::SizeOfDecodedWithComputedFallbackIfHeap(gfxASurface::MemoryLocation aLocation,
                                                      MallocSizeOf aMallocSizeOf) const
{
  size_t n = mFrames->SizeOfDecodedWithComputedFallbackIfHeap(aLocation, aMallocSizeOf);

  if (mAnim) {
    if (mAnim->compositingFrame) {
      n += mAnim->compositingFrame->SizeOfExcludingThisWithComputedFallbackIfHeap(aLocation, aMallocSizeOf);
    }
    if (mAnim->compositingPrevFrame) {
      n += mAnim->compositingPrevFrame->SizeOfExcludingThisWithComputedFallbackIfHeap(aLocation, aMallocSizeOf);
    }
  }

  return n;
}

void
FrameBlender::ResetAnimation()
{
  if (mAnim) {
    mAnim->lastCompositedFrameIndex = -1;
  }
}

} // namespace image
} // namespace mozilla
