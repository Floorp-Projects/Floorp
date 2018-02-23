/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_imgFrame_h
#define mozilla_image_imgFrame_h

#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Monitor.h"
#include "mozilla/Move.h"
#include "FrameTimeout.h"
#include "gfxDrawable.h"
#include "imgIContainer.h"
#include "MainThreadUtils.h"
#include "nsAutoPtr.h"

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
  FULLY_OPAQUE,
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
                FrameTimeout aTimeout, const nsIntRect& aRect,
                BlendMethod aBlendMethod, const Maybe<gfx::IntRect>& aBlendRect,
                DisposalMethod aDisposalMethod, bool aHasAlpha)
    : mRawData(aRawData)
    , mPaletteDataLength(aPaletteDataLength)
    , mTimeout(aTimeout)
    , mRect(aRect)
    , mBlendMethod(aBlendMethod)
    , mBlendRect(aBlendRect)
    , mDisposalMethod(aDisposalMethod)
    , mHasAlpha(aHasAlpha)
  { }

  uint8_t* mRawData;
  uint32_t mPaletteDataLength;
  FrameTimeout mTimeout;
  nsIntRect mRect;
  BlendMethod mBlendMethod;
  Maybe<gfx::IntRect> mBlendRect;
  DisposalMethod mDisposalMethod;
  bool mHasAlpha;
};

class imgFrame
{
  typedef gfx::Color Color;
  typedef gfx::DataSourceSurface DataSourceSurface;
  typedef gfx::DrawTarget DrawTarget;
  typedef gfx::SamplingFilter SamplingFilter;
  typedef gfx::IntPoint IntPoint;
  typedef gfx::IntRect IntRect;
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
                          bool aNonPremult = false,
                          bool aIsAnimated = false);

  nsresult InitForAnimator(const nsIntSize& aSize,
                           SurfaceFormat aFormat)
  {
    return InitForDecoder(aSize, nsIntRect(0, 0, aSize.width, aSize.height),
                          aFormat, 0, false, true);
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
   *
   * aBackend specifies the DrawTarget backend type this imgFrame is supposed
   *          to be drawn to.
   */
  nsresult InitWithDrawable(gfxDrawable* aDrawable,
                            const nsIntSize& aSize,
                            const SurfaceFormat aFormat,
                            SamplingFilter aSamplingFilter,
                            uint32_t aImageFlags,
                            gfx::BackendType aBackend);

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
            SamplingFilter aSamplingFilter, uint32_t aImageFlags,
            float aOpacity);

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
   * @param aTimeout         For animation frames, the timeout before the next
   *                         frame is displayed.
   * @param aBlendMethod     For animation frames, a blending method to be used
   *                         when compositing this frame.
   * @param aBlendRect       For animation frames, if present, the subrect in
   *                         which @aBlendMethod applies. Outside of this
   *                         subrect, BlendMethod::OVER is always used.
   * @param aFinalize        Finalize the underlying surface (e.g. so that it
   *                         may be marked as read only if possible).
   */
  void Finish(Opacity aFrameOpacity = Opacity::SOME_TRANSPARENCY,
              DisposalMethod aDisposalMethod = DisposalMethod::KEEP,
              FrameTimeout aTimeout = FrameTimeout::FromRawMilliseconds(0),
              BlendMethod aBlendMethod = BlendMethod::OVER,
              const Maybe<IntRect>& aBlendRect = Nothing(),
              bool aFinalize = true);

  /**
   * Mark this imgFrame as aborted. This informs the imgFrame that if it isn't
   * completely decoded now, it never will be.
   *
   * You must always call either Finish() or Abort() before releasing the last
   * RawAccessFrameRef pointing to an imgFrame.
   */
  void Abort();

  /**
   * Returns true if this imgFrame has been aborted.
   */
  bool IsAborted() const;

  /**
   * Returns true if this imgFrame is completely decoded.
   */
  bool IsFinished() const;

  /**
   * Blocks until this imgFrame is either completely decoded, or is marked as
   * aborted.
   *
   * Note that calling this on the main thread _blocks the main thread_. Be very
   * careful in your use of this method to avoid excessive main thread jank or
   * deadlock.
   */
  void WaitUntilFinished() const;

  /**
   * Returns the number of bytes per pixel this imgFrame requires.  This is a
   * worst-case value that does not take into account the effects of format
   * changes caused by Optimize(), since an imgFrame is not optimized throughout
   * its lifetime.
   */
  uint32_t GetBytesPerPixel() const { return GetIsPaletted() ? 1 : 4; }

  IntSize GetImageSize() const { return mImageSize; }
  IntRect GetRect() const { return mFrameRect; }
  IntSize GetSize() const { return mFrameRect.Size(); }
  void GetImageData(uint8_t** aData, uint32_t* length) const;
  uint8_t* GetImageData() const;

  bool GetIsPaletted() const;
  void GetPaletteData(uint32_t** aPalette, uint32_t* length) const;
  uint32_t* GetPaletteData() const;
  uint8_t GetPaletteDepth() const { return mPaletteDepth; }

  AnimationData GetAnimationData() const;

  bool GetCompositingFailed() const;
  void SetCompositingFailed(bool val);

  void SetOptimizable();

  void FinalizeSurface();
  already_AddRefed<SourceSurface> GetSourceSurface();

  void AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf, size_t& aHeapSizeOut,
                              size_t& aNonHeapSizeOut,
                              size_t& aExtHandlesOut) const;

private: // methods

  ~imgFrame();

  nsresult LockImageData();
  nsresult UnlockImageData();
  nsresult Optimize(gfx::DrawTarget* aTarget);

  void AssertImageDataLocked() const;

  bool AreAllPixelsWritten() const;
  nsresult ImageUpdatedInternal(const nsIntRect& aUpdateRect);
  void GetImageDataInternal(uint8_t** aData, uint32_t* length) const;
  uint32_t GetImageBytesPerRow() const;
  uint32_t GetImageDataLength() const;
  void FinalizeSurfaceInternal();
  already_AddRefed<SourceSurface> GetSourceSurfaceInternal();

  uint32_t PaletteDataLength() const
  {
    return mPaletteDepth ? (size_t(1) << mPaletteDepth) * sizeof(uint32_t)
                         : 0;
  }

  struct SurfaceWithFormat {
    RefPtr<gfxDrawable> mDrawable;
    SurfaceFormat mFormat;
    SurfaceWithFormat() { }
    SurfaceWithFormat(gfxDrawable* aDrawable, SurfaceFormat aFormat)
      : mDrawable(aDrawable), mFormat(aFormat)
    { }
    bool IsValid() { return !!mDrawable; }
  };

  SurfaceWithFormat SurfaceForDrawing(bool               aDoPartialDecode,
                                      bool               aDoTile,
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

  /**
   * Surface which contains either a weak or a strong reference to its
   * underlying data buffer. If it is a weak reference, and there are no strong
   * references, the buffer may be released due to events such as low memory.
   */
  RefPtr<DataSourceSurface> mRawSurface;

  /**
   * Refers to the same data as mRawSurface, but when set, it guarantees that
   * we hold a strong reference to the underlying data buffer.
   */
  RefPtr<DataSourceSurface> mLockedSurface;

  /**
   * Optimized copy of mRawSurface for the DrawTarget that will render it. This
   * is unused if the DrawTarget is able to render DataSourceSurface buffers
   * directly.
   */
  RefPtr<SourceSurface> mOptSurface;

  nsIntRect mDecoded;

  //! Number of RawAccessFrameRefs currently alive for this imgFrame.
  int32_t mLockCount;

  //! The timeout for this frame.
  FrameTimeout mTimeout;

  DisposalMethod mDisposalMethod;
  BlendMethod    mBlendMethod;
  Maybe<IntRect> mBlendRect;
  SurfaceFormat  mFormat;

  bool mAborted;
  bool mFinished;
  bool mOptimizable;


  //////////////////////////////////////////////////////////////////////////////
  // Effectively const data, only mutated in the Init methods.
  //////////////////////////////////////////////////////////////////////////////

  IntSize      mImageSize;
  IntRect      mFrameRect;

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

  bool mCompositingFailed;
};

/**
 * A reference to an imgFrame that holds the imgFrame's surface in memory,
 * allowing drawing. If you have a DrawableFrameRef |ref| and |if (ref)| returns
 * true, then calls to Draw() and GetSourceSurface() are guaranteed to succeed.
 */
class DrawableFrameRef final
{
  typedef gfx::DataSourceSurface DataSourceSurface;

public:
  DrawableFrameRef() { }

  explicit DrawableFrameRef(imgFrame* aFrame)
    : mFrame(aFrame)
  {
    MOZ_ASSERT(aFrame);
    MonitorAutoLock lock(aFrame->mMonitor);

    // Paletted images won't have a surface so there is no strong reference
    // to hold on to. Since Draw() and GetSourceSurface() calls will not work
    // in that case, we should be using RawAccessFrameRef exclusively instead.
    // See FrameAnimator::GetRawFrame for an example of this behaviour.
    if (aFrame->mRawSurface) {
      mRef = new DataSourceSurface::ScopedMap(aFrame->mRawSurface,
                                              DataSourceSurface::READ_WRITE);
      if (!mRef->IsMapped()) {
        mFrame = nullptr;
        mRef = nullptr;
      }
    } else {
      MOZ_ASSERT(aFrame->mOptSurface || aFrame->GetIsPaletted());
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
  DrawableFrameRef(const DrawableFrameRef& aOther) = delete;

  RefPtr<imgFrame> mFrame;
  nsAutoPtr<DataSourceSurface::ScopedMap> mRef;
};

/**
 * A reference to an imgFrame that holds the imgFrame's surface in memory in a
 * format appropriate for access as raw data. If you have a RawAccessFrameRef
 * |ref| and |if (ref)| is true, then calls to GetImageData() and
 * GetPaletteData() are guaranteed to succeed. This guarantee is stronger than
 * DrawableFrameRef, so everything that a valid DrawableFrameRef guarantees is
 * also guaranteed by a valid RawAccessFrameRef.
 *
 * This may be considerably more expensive than is necessary just for drawing,
 * so only use this when you need to read or write the raw underlying image data
 * that the imgFrame holds.
 *
 * Once all an imgFrame's RawAccessFrameRefs go out of scope, new
 * RawAccessFrameRefs cannot be created.
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
  RawAccessFrameRef(const RawAccessFrameRef& aOther) = delete;

  RefPtr<imgFrame> mFrame;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_imgFrame_h
