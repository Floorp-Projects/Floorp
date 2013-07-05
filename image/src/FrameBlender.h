/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_imagelib_FrameBlender_h_
#define mozilla_imagelib_FrameBlender_h_

#include "nsTArray.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/TimeStamp.h"
#include "gfxASurface.h"
#include "imgFrame.h"

namespace mozilla {
namespace image {

/**
 * FrameDataPair is a slightly-smart tuple of (frame, raw frame data) where the
 * raw frame data is allowed to be (and is, initially) null.
 *
 * If you call LockAndGetData, you will be able to call GetFrameData() on that
 * instance, and when the FrameDataPair is destructed, the imgFrame lock will
 * be unlocked.
 */
class FrameDataPair
{
public:
  explicit FrameDataPair(imgFrame* frame)
    : mFrame(frame)
    , mFrameData(nullptr)
  {}

  FrameDataPair()
    : mFrame(nullptr)
    , mFrameData(nullptr)
  {}

  FrameDataPair(FrameDataPair& other)
  {
    mFrame = other.mFrame;
    mFrameData = other.mFrameData;

    // since mFrame is an nsAutoPtr, the assignment operator above actually
    // nulls out other.mFrame. In order to fully assume ownership over the
    // frame, we also null out the other's mFrameData.
    other.mFrameData = nullptr;
  }

  ~FrameDataPair()
  {
    if (mFrameData) {
      mFrame->UnlockImageData();
    }
  }

  // Lock the frame and store its mFrameData. The frame will be unlocked (and
  // deleted) when this FrameDataPair is deleted.
  void LockAndGetData()
  {
    if (mFrame) {
      if (NS_SUCCEEDED(mFrame->LockImageData())) {
        if (mFrame->GetIsPaletted()) {
          mFrameData = reinterpret_cast<uint8_t*>(mFrame->GetPaletteData());
        } else {
          mFrameData = mFrame->GetImageData();
        }
      }
    }
  }

  // Null out this FrameDataPair and return its frame. You must ensure the
  // frame will be deleted separately.
  imgFrame* Forget()
  {
    if (mFrameData) {
      mFrame->UnlockImageData();
    }

    imgFrame* frame = mFrame.forget();
    mFrameData = nullptr;
    return frame;
  }

  bool HasFrameData() const
  {
    if (mFrameData) {
      MOZ_ASSERT(!!mFrame);
    }
    return !!mFrameData;
  }

  uint8_t* GetFrameData() const
  {
    return mFrameData;
  }

  imgFrame* GetFrame() const
  {
    return mFrame;
  }

  // Resets this FrameDataPair to work with a different frame. Takes ownership
  // of the frame, deleting the old frame (if any).
  void SetFrame(imgFrame* frame)
  {
    if (mFrameData) {
      mFrame->UnlockImageData();
    }

    mFrame = frame;
    mFrameData = nullptr;
  }

  operator imgFrame*() const
  {
    return GetFrame();
  }

  imgFrame* operator->() const
  {
    return GetFrame();
  }

  bool operator==(imgFrame* other) const
  {
    return mFrame == other;
  }

private:
  nsAutoPtr<imgFrame> mFrame;
  uint8_t* mFrameData;
};

/**
 * FrameBlender stores and gives access to imgFrames. It also knows how to
 * blend frames from previous to next, looping if necessary.
 *
 * All logic about when and whether to blend are external to FrameBlender.
 */
class FrameBlender
{
public:

  FrameBlender();
  ~FrameBlender();

  bool DoBlend(nsIntRect* aDirtyRect, uint32_t aPrevFrameIndex,
               uint32_t aNextFrameIndex);

  /**
   * Get the @aIndex-th frame, including (if applicable) any results of
   * blending.
   */
  imgFrame* GetFrame(uint32_t aIndex) const;

  /**
   * Get the @aIndex-th frame in the frame index, ignoring results of blending.
   */
  imgFrame* RawGetFrame(uint32_t aIndex) const;

  void InsertFrame(uint32_t framenum, imgFrame* aFrame);
  void RemoveFrame(uint32_t framenum);
  imgFrame* SwapFrame(uint32_t framenum, imgFrame* aFrame);
  void ClearFrames();

  /* The total number of frames in this image. */
  uint32_t GetNumFrames() const;

  void Discard();

  void SetSize(nsIntSize aSize) { mSize = aSize; }

  size_t SizeOfDecodedWithComputedFallbackIfHeap(gfxASurface::MemoryLocation aLocation,
                                                 mozilla::MallocSizeOf aMallocSizeOf) const;

  void ResetAnimation();

  // "Blend" method indicates how the current image is combined with the
  // previous image.
  enum FrameBlendMethod {
    // All color components of the frame, including alpha, overwrite the current
    // contents of the frame's output buffer region
    kBlendSource =  0,

    // The frame should be composited onto the output buffer based on its alpha,
    // using a simple OVER operation
    kBlendOver
  };

  enum FrameDisposalMethod {
    kDisposeClearAll         = -1, // Clear the whole image, revealing
                                   // what was there before the gif displayed
    kDisposeNotSpecified,   // Leave frame, let new frame draw on top
    kDisposeKeep,           // Leave frame, let new frame draw on top
    kDisposeClear,          // Clear the frame's area, revealing bg
    kDisposeRestorePrevious // Restore the previous (composited) frame
  };

  // A hint as to whether an individual frame is entirely opaque, or requires
  // alpha blending.
  enum FrameAlpha {
    kFrameHasAlpha,
    kFrameOpaque
  };

private:

  struct Anim
  {
    //! Track the last composited frame for Optimizations (See DoComposite code)
    int32_t lastCompositedFrameIndex;

    /** For managing blending of frames
     *
     * Some animations will use the compositingFrame to composite images
     * and just hand this back to the caller when it is time to draw the frame.
     * NOTE: When clearing compositingFrame, remember to set
     *       lastCompositedFrameIndex to -1.  Code assume that if
     *       lastCompositedFrameIndex >= 0 then compositingFrame exists.
     */
    FrameDataPair compositingFrame;

    /** the previous composited frame, for DISPOSE_RESTORE_PREVIOUS
     *
     * The Previous Frame (all frames composited up to the current) needs to be
     * stored in cases where the image specifies it wants the last frame back
     * when it's done with the current frame.
     */
    FrameDataPair compositingPrevFrame;

    Anim() :
      lastCompositedFrameIndex(-1)
    {}
  };

  void EnsureAnimExists();

  /** Clears an area of <aFrame> with transparent black.
   *
   * @param aFrameData Target Frame data
   * @param aFrameRect The rectangle of the data pointed ot by aFrameData
   *
   * @note Does also clears the transparancy mask
   */
  static void ClearFrame(uint8_t* aFrameData, const nsIntRect& aFrameRect);

  //! @overload
  static void ClearFrame(uint8_t* aFrameData, const nsIntRect& aFrameRect, const nsIntRect &aRectToClear);

  //! Copy one frames's image and mask into another
  static bool CopyFrameImage(const uint8_t *aDataSrc, const nsIntRect& aRectSrc,
                             uint8_t *aDataDest, const nsIntRect& aRectDest);

  /**
   * Draws one frames's image to into another, at the position specified by
   * aSrcRect.
   *
   * @aSrcData the raw data of the current frame being drawn
   * @aSrcRect the size of the source frame, and the position of that frame in
   *           the composition frame
   * @aSrcPaletteLength the length (in bytes) of the palette at the beginning
   *                    of the source data (0 if image is not paletted)
   * @aSrcHasAlpha whether the source data represents an image with alpha
   * @aDstPixels the raw data of the composition frame where the current frame
   *             is drawn into (32-bit ARGB)
   * @aDstRect the size of the composition frame
   * @aBlendMethod the blend method for how to blend src on the composition frame.
   */
  static nsresult DrawFrameTo(const uint8_t *aSrcData, const nsIntRect& aSrcRect,
                              uint32_t aSrcPaletteLength, bool aSrcHasAlpha,
                              uint8_t *aDstPixels, const nsIntRect& aDstRect,
                              FrameBlendMethod aBlendMethod);

private: // data
  //! All the frames of the image
  nsTArray<FrameDataPair> mFrames;
  nsIntSize mSize;
  Anim* mAnim;
};

} // namespace image
} // namespace mozilla

#endif /* mozilla_imagelib_FrameBlender_h_ */
