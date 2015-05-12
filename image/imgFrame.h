/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_imgFrame_h
#define mozilla_image_imgFrame_h

#include "mozilla/MemoryReporting.h"
#include "mozilla/Monitor.h"
#include "mozilla/Move.h"
#include "mozilla/VolatileBuffer.h"
#include "gfxDrawable.h"
#include "imgIContainer.h"
#include "MainThreadUtils.h"

namespace mozilla {
namespace image {

class ImageRegion;
class DrawableFrameRef;
class RawAccessFrameRef;

enum class BlendMethod : int8_t {
  // All color components of the frame, including alpha, overwrite the current
  // contents of the frame's output buffer region.
  SOURCE,

  // The frame should be composited onto the output buffer based on its alpha,
  // using a simple OVER operation.
  OVER
};

enum class DisposalMethod : int8_t {
  CLEAR_ALL = -1,  // Clear the whole image, revealing what's underneath.
  NOT_SPECIFIED,   // Leave the frame and let the new frame draw on top.
  KEEP,            // Leave the frame and let the new frame draw on top.
  CLEAR,           // Clear the frame's area, revealing what's underneath.
  RESTORE_PREVIOUS // Restore the previous (composited) frame.
};

enum class Opacity : uint8_t {
  OPAQUE,
  SOME_TRANSPARENCY
};

/**
 * AnimationData contains all of the information necessary for using an imgFrame
 * as part of an animation.
 *
 * It includes pointers to the raw image data of the underlying imgFrame, but
 * does not own that data. A RawAccessFrameRef for the underlying imgFrame must
 * outlive the AnimationData for it to remain valid.
 */
struct AnimationData
{
  AnimationData(uint8_t* aRawData, uint32_t aPaletteDataLength,
                int32_t aRawTimeout, const nsIntRect& aRect,
                BlendMethod aBlendMethod, DisposalMethod aDisposalMethod,
                bool aHasAlpha)
    : mRawData(aRawData)
    , mPaletteDataLength(aPaletteDataLength)
    , mRawTimeout(aRawTimeout)
    , mRect(aRect)
    , mBlendMethod(aBlendMethod)
    , mDisposalMethod(aDisposalMethod)
    , mHasAlpha(aHasAlpha)
  { }

  uint8_t* mRawData;
  uint32_t mPaletteDataLength;
  int32_t mRawTimeout;
  nsIntRect mRect;
  BlendMethod mBlendMethod;
  DisposalMethod mDisposalMethod;
  bool mHasAlpha;
};

/**
 * ScalingData contains all of the information necessary for performing
 * high-quality (CPU-based) scaling an imgFrame.
 *
 * It includes pointers to the raw image data of the underlying imgFrame, but
 * does not own that data. A RawAccessFrameRef for the underlying imgFrame must
 * outlive the ScalingData for it to remain valid.
 */
struct ScalingData
{
  ScalingData(uint8_t* aRawData,
              gfx::IntSize aSize,
              uint32_t aBytesPerRow,
              gfx::SurfaceFormat aFormat)
    : mRawData(aRawData)
    , mSize(aSize)
    , mBytesPerRow(aBytesPerRow)
    , mFormat(aFormat)
  { }

  uint8_t* mRawData;
  gfx::IntSize mSize;
  uint32_t mBytesPerRow;
  gfx::SurfaceFormat mFormat;
};

class imgFrame
{
  typedef gfx::Color Color;
  typedef gfx::DataSourceSurface DataSourceSurface;
  typedef gfx::DrawTarget DrawTarget;
  typedef gfx::IntSize IntSize;
  typedef gfx::SourceSurface SourceSurface;
  typedef gfx::SurfaceFormat SurfaceFormat;

public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(imgFrame)
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(imgFrame)

  imgFrame();

  /**
   * Initialize this imgFrame with an empty surface and prepare it for being
   * written to by a decoder.
   *
   * This is appropriate for use with decoded images, but it should not be used
   * when drawing content into an imgFrame, as it may use a different graphics
   * backend than normal content drawing.
   */
  nsresult InitForDecoder(const nsIntSize& aImageSize,
                          const nsIntRect& aRect,
                          SurfaceFormat aFormat,
                          uint8_t aPaletteDepth = 0,
                          bool aNonPremult = false);

  nsresult InitForDecoder(const nsIntSize& aSize,
                          SurfaceFormat aFormat,
                          uint8_t aPaletteDepth = 0)
  {
    return InitForDecoder(aSize, nsIntRect(0, 0, aSize.width, aSize.height),
                          aFormat, aPaletteDepth);
  }


  /**
   * Initialize this imgFrame with a new surface and draw the provided
   * gfxDrawable into it.
   *
   * This is appropriate to use when drawing content into an imgFrame, as it
   * uses the same graphics backend as normal content drawing. The downside is
   * that the underlying surface may not be stored in a volatile buffer on all
   * platforms, and raw access to the surface (using RawAccessRef()) may be much
   * more expensive than in the InitForDecoder() case.
   */
  nsresult InitWithDrawable(gfxDrawable* aDrawable,
                            const nsIntSize& aSize,
                            const SurfaceFormat aFormat,
                            GraphicsFilter aFilter,
                            uint32_t aImageFlags);

  /**
   * Reinitializes an existing imgFrame with new parameters. You must be holding
   * a RawAccessFrameRef to the imgFrame, and it must never have been written
   * to, marked finished, or aborted.
   *
   * XXX(seth): We will remove this in bug 1117607.
   */
  nsresult ReinitForDecoder(const nsIntSize& aImageSize,
                            const nsIntRect& aRect,
                            SurfaceFormat aFormat,
                            uint8_t aPaletteDepth = 0,
                            bool aNonPremult = false);

  DrawableFrameRef DrawableRef();
  RawAccessFrameRef RawAccessRef();

  /**
   * Make this imgFrame permanently available for raw access.
   *
   * This is irrevocable, and should be avoided whenever possible, since it
   * prevents this imgFrame from being optimized and makes it impossible for its
   * volatile buffer to be freed.
   *
   * It is an error to call this without already holding a RawAccessFrameRef to
   * this imgFrame.
   */
  void SetRawAccessOnly();

  bool Draw(gfxContext* aContext, const ImageRegion& aRegion,
            GraphicsFilter aFilter, uint32_t aImageFlags);

  nsresult ImageUpdated(const nsIntRect& aUpdateRect);

  /**
   * Mark this imgFrame as completely decoded, and set final options.
   *
   * You must always call either Finish() or Abort() before releasing the last
   * RawAccessFrameRef pointing to an imgFrame.
   *
   * @param aFrameOpacity    Whether this imgFrame is opaque.
   * @param aDisposalMethod  For animation frames, how this imgFrame is cleared
   *                         from the compositing frame before the next frame is
   *                         displayed.
   * @param aRawTimeout      For animation frames, the timeout in milliseconds
   *                         before the next frame is displayed. This timeout is
   *                         not necessarily the timeout that will actually be
   *                         used; see FrameAnimator::GetTimeoutForFrame.
   * @param aBlendMethod     For animation frames, a blending method to be used
   *                         when compositing this frame.
   */
  void Finish(Opacity aFrameOpacity = Opacity::SOME_TRANSPARENCY,
              DisposalMethod aDisposalMethod = DisposalMethod::KEEP,
              int32_t aRawTimeout = 0,
              BlendMethod aBlendMethod = BlendMethod::OVER);

  /**
   * Mark this imgFrame as aborted. This informs the imgFrame that if it isn't
   * completely decoded now, it never will be.
   *
   * You must always call either Finish() or Abort() before releasing the last
   * RawAccessFrameRef pointing to an imgFrame.
   */
  void Abort();

  /**
   * Returns true if this imgFrame is completely decoded.
   */
  bool IsImageComplete() const;

  /**
   * Blocks until this imgFrame is either completely decoded, or is marked as
   * aborted.
   *
   * Note that calling this on the main thread _blocks the main thread_. Be very
   * careful in your use of this method to avoid excessive main thread jank or
   * deadlock.
   */
  void WaitUntilComplete() const;

  /**
   * Returns the number of bytes per pixel this imgFrame requires.  This is a
   * worst-case value that does not take into account the effects of format
   * changes caused by Optimize(), since an imgFrame is not optimized throughout
   * its lifetime.
   */
  uint32_t GetBytesPerPixel() const { return GetIsPaletted() ? 1 : 4; }

  IntSize GetImageSize() const { return mImageSize; }
  nsIntRect GetRect() const;
  IntSize GetSize() const { return mSize; }
  bool NeedsPadding() const { return mOffset != nsIntPoint(0, 0); }
  void GetImageData(uint8_t** aData, uint32_t* length) const;
  uint8_t* GetImageData() const;

  bool GetIsPaletted() const;
  void GetPaletteData(uint32_t** aPalette, uint32_t* length) const;
  uint32_t* GetPaletteData() const;
  uint8_t GetPaletteDepth() const { return mPaletteDepth; }

  /**
   * Get the SurfaceFormat for this imgFrame.
   *
   * This should only be used for assertions.
   */
  SurfaceFormat GetFormat() const;

  AnimationData GetAnimationData() const;
  ScalingData GetScalingData() const;

  bool GetCompositingFailed() const;
  void SetCompositingFailed(bool val);

  void SetOptimizable();

  Color SinglePixelColor() const;
  bool IsSinglePixel() const;

  TemporaryRef<SourceSurface> GetSurface();
  TemporaryRef<DrawTarget> GetDrawTarget();

  size_t SizeOfExcludingThis(gfxMemoryLocation aLocation,
                             MallocSizeOf aMallocSizeOf) const;

private: // methods

  ~imgFrame();

  nsresult LockImageData();
  nsresult UnlockImageData();
  nsresult Optimize();
  nsresult Deoptimize();

  void AssertImageDataLocked() const;

  bool IsImageCompleteInternal() const;
  nsresult ImageUpdatedInternal(const nsIntRect& aUpdateRect);
  void GetImageDataInternal(uint8_t** aData, uint32_t* length) const;
  uint32_t GetImageBytesPerRow() const;
  uint32_t GetImageDataLength() const;
  int32_t GetStride() const;
  TemporaryRef<SourceSurface> GetSurfaceInternal();

  uint32_t PaletteDataLength() const
  {
    return mPaletteDepth ? (size_t(1) << mPaletteDepth) * sizeof(uint32_t)
                         : 0;
  }

  struct SurfaceWithFormat {
    nsRefPtr<gfxDrawable> mDrawable;
    SurfaceFormat mFormat;
    SurfaceWithFormat() { }
    SurfaceWithFormat(gfxDrawable* aDrawable, SurfaceFormat aFormat)
      : mDrawable(aDrawable), mFormat(aFormat)
    { }
    bool IsValid() { return !!mDrawable; }
  };

  SurfaceWithFormat SurfaceForDrawing(bool               aDoPadding,
                                      bool               aDoPartialDecode,
                                      bool               aDoTile,
                                      gfxContext*        aContext,
                                      const nsIntMargin& aPadding,
                                      gfxRect&           aImageRect,
                                      ImageRegion&       aRegion,
                                      SourceSurface*     aSurface);

private: // data
  friend class DrawableFrameRef;
  friend class RawAccessFrameRef;
  friend class UnlockImageDataRunnable;

  //////////////////////////////////////////////////////////////////////////////
  // Thread-safe mutable data, protected by mMonitor.
  //////////////////////////////////////////////////////////////////////////////

  mutable Monitor mMonitor;

  RefPtr<DataSourceSurface> mImageSurface;
  RefPtr<SourceSurface> mOptSurface;

  RefPtr<VolatileBuffer> mVBuf;
  VolatileBufferPtr<uint8_t> mVBufPtr;

  nsIntRect mDecoded;

  //! Number of RawAccessFrameRefs currently alive for this imgFrame.
  int32_t mLockCount;

  //! Raw timeout for this frame. (See FrameAnimator::GetTimeoutForFrame.)
  int32_t mTimeout; // -1 means display forever.

  DisposalMethod mDisposalMethod;
  BlendMethod    mBlendMethod;
  SurfaceFormat  mFormat;

  bool mHasNoAlpha;
  bool mAborted;


  //////////////////////////////////////////////////////////////////////////////
  // Effectively const data, only mutated in the Init methods.
  //////////////////////////////////////////////////////////////////////////////

  IntSize      mImageSize;
  IntSize      mSize;
  nsIntPoint   mOffset;

  // The palette and image data for images that are paletted, since Cairo
  // doesn't support these images.
  // The paletted data comes first, then the image data itself.
  // Total length is PaletteDataLength() + GetImageDataLength().
  uint8_t*     mPalettedImageData;
  uint8_t      mPaletteDepth;

  bool mNonPremult;


  //////////////////////////////////////////////////////////////////////////////
  // Main-thread-only mutable data.
  //////////////////////////////////////////////////////////////////////////////

  // Note that the data stored in gfx::Color is *non-alpha-premultiplied*.
  Color        mSinglePixelColor;

  bool mSinglePixel;
  bool mCompositingFailed;
  bool mOptimizable;
};

/**
 * A reference to an imgFrame that holds the imgFrame's surface in memory,
 * allowing drawing. If you have a DrawableFrameRef |ref| and |if (ref)| returns
 * true, then calls to Draw() and GetSurface() are guaranteed to succeed.
 */
class DrawableFrameRef final
{
public:
  DrawableFrameRef() { }

  explicit DrawableFrameRef(imgFrame* aFrame)
    : mFrame(aFrame)
    , mRef(aFrame->mVBuf)
  {
    if (mRef.WasBufferPurged()) {
      mFrame = nullptr;
      mRef = nullptr;
    }
  }

  DrawableFrameRef(DrawableFrameRef&& aOther)
    : mFrame(aOther.mFrame.forget())
    , mRef(Move(aOther.mRef))
  { }

  DrawableFrameRef& operator=(DrawableFrameRef&& aOther)
  {
    MOZ_ASSERT(this != &aOther, "Self-moves are prohibited");
    mFrame = aOther.mFrame.forget();
    mRef = Move(aOther.mRef);
    return *this;
  }

  explicit operator bool() const { return bool(mFrame); }

  imgFrame* operator->()
  {
    MOZ_ASSERT(mFrame);
    return mFrame;
  }

  const imgFrame* operator->() const
  {
    MOZ_ASSERT(mFrame);
    return mFrame;
  }

  imgFrame* get() { return mFrame; }
  const imgFrame* get() const { return mFrame; }

  void reset()
  {
    mFrame = nullptr;
    mRef = nullptr;
  }

private:
  nsRefPtr<imgFrame> mFrame;
  VolatileBufferPtr<uint8_t> mRef;
};

/**
 * A reference to an imgFrame that holds the imgFrame's surface in memory in a
 * format appropriate for access as raw data. If you have a RawAccessFrameRef
 * |ref| and |if (ref)| is true, then calls to GetImageData(), GetPaletteData(),
 * and GetDrawTarget() are guaranteed to succeed. This guarantee is stronger
 * than DrawableFrameRef, so everything that a valid DrawableFrameRef guarantees
 * is also guaranteed by a valid RawAccessFrameRef.
 *
 * This may be considerably more expensive than is necessary just for drawing,
 * so only use this when you need to read or write the raw underlying image data
 * that the imgFrame holds.
 */
class RawAccessFrameRef final
{
public:
  RawAccessFrameRef() { }

  explicit RawAccessFrameRef(imgFrame* aFrame)
    : mFrame(aFrame)
  {
    MOZ_ASSERT(mFrame, "Need a frame");

    if (NS_FAILED(mFrame->LockImageData())) {
      mFrame->UnlockImageData();
      mFrame = nullptr;
    }
  }

  RawAccessFrameRef(RawAccessFrameRef&& aOther)
    : mFrame(aOther.mFrame.forget())
  { }

  ~RawAccessFrameRef()
  {
    if (mFrame) {
      mFrame->UnlockImageData();
    }
  }

  RawAccessFrameRef& operator=(RawAccessFrameRef&& aOther)
  {
    MOZ_ASSERT(this != &aOther, "Self-moves are prohibited");

    if (mFrame) {
      mFrame->UnlockImageData();
    }

    mFrame = aOther.mFrame.forget();

    return *this;
  }

  explicit operator bool() const { return bool(mFrame); }

  imgFrame* operator->()
  {
    MOZ_ASSERT(mFrame);
    return mFrame.get();
  }

  const imgFrame* operator->() const
  {
    MOZ_ASSERT(mFrame);
    return mFrame;
  }

  imgFrame* get() { return mFrame; }
  const imgFrame* get() const { return mFrame; }

  void reset()
  {
    if (mFrame) {
      mFrame->UnlockImageData();
    }
    mFrame = nullptr;
  }

private:
  nsRefPtr<imgFrame> mFrame;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_imgFrame_h
