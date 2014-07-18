/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_imagelib_FrameSequence_h_
#define mozilla_imagelib_FrameSequence_h_

#include "nsTArray.h"
#include "mozilla/MemoryReporting.h"
#include "gfxTypes.h"
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
 * FrameSequence stores image frames (and their associated raw data pointers).
 * It is little more than a smart array.
 */
class FrameSequence
{
  ~FrameSequence();

public:

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FrameSequence)

  /**
   * Get the read-only (frame, data) pair at index aIndex.
   */
  const FrameDataPair& GetFrame(uint32_t aIndex) const;

  /**
   * Insert a frame into the array. FrameSequence takes ownership of the frame.
   */
  void InsertFrame(uint32_t framenum, imgFrame* aFrame);

  /**
   * Remove (and delete) the frame at index framenum.
   */
  void RemoveFrame(uint32_t framenum);

  /**
   * Swap aFrame with the frame at sequence framenum, and return that frame.
   * You take ownership over the frame returned.
   */
  imgFrame* SwapFrame(uint32_t framenum, imgFrame* aFrame);

  /**
   * Remove (and delete) all frames.
   */
  void ClearFrames();

  /* The total number of frames in this image. */
  uint32_t GetNumFrames() const;

  size_t SizeOfDecodedWithComputedFallbackIfHeap(gfxMemoryLocation aLocation,
                                                 MallocSizeOf aMallocSizeOf) const;

private: // data
  //! All the frames of the image
  nsTArray<FrameDataPair> mFrames;
};

} // namespace image
} // namespace mozilla

#endif /* mozilla_imagelib_FrameSequence_h_ */
