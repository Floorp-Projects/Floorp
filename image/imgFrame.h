/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_imgFrame_h
#define mozilla_image_imgFrame_h

#include <functional>
#include <utility>

#include "AnimationParams.h"
#include "MainThreadUtils.h"
#include "gfxDrawable.h"
#include "mozilla/layers/SourceSurfaceSharedData.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Monitor.h"
#include "nsRect.h"

namespace mozilla {
namespace image {

class ImageRegion;
class DrawableFrameRef;
class RawAccessFrameRef;

enum class Opacity : uint8_t { FULLY_OPAQUE, SOME_TRANSPARENCY };

class imgFrame {
  typedef gfx::SourceSurfaceSharedData SourceSurfaceSharedData;
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
  nsresult InitForDecoder(const nsIntSize& aImageSize, SurfaceFormat aFormat,
                          bool aNonPremult,
                          const Maybe<AnimationParams>& aAnimParams,
                          bool aShouldRecycle,
                          uint32_t* aImageDataLength = nullptr);

  /**
   * Reinitialize this imgFrame with the new parameters, but otherwise retain
   * the underlying buffer.
   *
   * This is appropriate for use with animated images, where the decoder was
   * given an IDecoderFrameRecycler object which may yield a recycled imgFrame
   * that was discarded to save memory.
   */
  nsresult InitForDecoderRecycle(const AnimationParams& aAnimParams,
                                 uint32_t* aImageDataLength = nullptr);

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
  nsresult InitWithDrawable(gfxDrawable* aDrawable, const nsIntSize& aSize,
                            const SurfaceFormat aFormat,
                            SamplingFilter aSamplingFilter,
                            uint32_t aImageFlags, gfx::BackendType aBackend);

  DrawableFrameRef DrawableRef();

  /**
   * Create a RawAccessFrameRef for the frame.
   */
  RawAccessFrameRef RawAccessRef(
      gfx::DataSourceSurface::MapType aMapType = gfx::DataSourceSurface::READ);

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
   * @param aFinalize        Finalize the underlying surface (e.g. so that it
   *                         may be marked as read only if possible).
   */
  void Finish(Opacity aFrameOpacity = Opacity::SOME_TRANSPARENCY,
              bool aFinalize = true,
              bool aOrientationSwapsWidthAndHeight = false);

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
   * Returns the number of bytes per pixel this imgFrame requires.
   */
  uint32_t GetBytesPerPixel() const { return 4; }

  const IntSize& GetSize() const { return mImageSize; }
  IntRect GetRect() const { return IntRect(IntPoint(0, 0), mImageSize); }
  const IntRect& GetBlendRect() const { return mBlendRect; }
  IntRect GetBoundedBlendRect() const {
    return mBlendRect.Intersect(GetRect());
  }
  nsIntRect GetDecodedRect() const {
    MonitorAutoLock lock(mMonitor);
    return mDecoded;
  }
  FrameTimeout GetTimeout() const { return mTimeout; }
  BlendMethod GetBlendMethod() const { return mBlendMethod; }
  DisposalMethod GetDisposalMethod() const { return mDisposalMethod; }
  bool FormatHasAlpha() const { return mFormat == SurfaceFormat::OS_RGBA; }

  const IntRect& GetDirtyRect() const { return mDirtyRect; }
  void SetDirtyRect(const IntRect& aDirtyRect) { mDirtyRect = aDirtyRect; }

  void FinalizeSurface();
  already_AddRefed<SourceSurface> GetSourceSurface();

  struct AddSizeOfCbData : public SourceSurface::SizeOfInfo {
    AddSizeOfCbData() : mIndex(0), mFinished(false) {}

    size_t mIndex;
    bool mFinished;
  };

  typedef std::function<void(AddSizeOfCbData& aMetadata)> AddSizeOfCb;

  void AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                              const AddSizeOfCb& aCallback) const;

 private:  // methods
  ~imgFrame();

  bool AreAllPixelsWritten() const MOZ_REQUIRES(mMonitor);
  nsresult ImageUpdatedInternal(const nsIntRect& aUpdateRect);
  uint32_t GetImageBytesPerRow() const;
  uint32_t GetImageDataLength() const;
  void FinalizeSurfaceInternal();
  already_AddRefed<SourceSurface> GetSourceSurfaceInternal();

  struct SurfaceWithFormat {
    RefPtr<gfxDrawable> mDrawable;
    SurfaceFormat mFormat;
    SurfaceWithFormat() : mFormat(SurfaceFormat::UNKNOWN) {}
    SurfaceWithFormat(gfxDrawable* aDrawable, SurfaceFormat aFormat)
        : mDrawable(aDrawable), mFormat(aFormat) {}
    SurfaceWithFormat(SurfaceWithFormat&& aOther)
        : mDrawable(std::move(aOther.mDrawable)), mFormat(aOther.mFormat) {}
    SurfaceWithFormat& operator=(SurfaceWithFormat&& aOther) {
      mDrawable = std::move(aOther.mDrawable);
      mFormat = aOther.mFormat;
      return *this;
    }
    SurfaceWithFormat& operator=(const SurfaceWithFormat& aOther) = delete;
    SurfaceWithFormat(const SurfaceWithFormat& aOther) = delete;
    bool IsValid() { return !!mDrawable; }
  };

  SurfaceWithFormat SurfaceForDrawing(bool aDoPartialDecode, bool aDoTile,
                                      ImageRegion& aRegion,
                                      SourceSurface* aSurface);

 private:  // data
  friend class DrawableFrameRef;
  friend class RawAccessFrameRef;
  friend class UnlockImageDataRunnable;

  //////////////////////////////////////////////////////////////////////////////
  // Thread-safe mutable data, protected by mMonitor.
  //////////////////////////////////////////////////////////////////////////////

  mutable Monitor mMonitor;

  /**
   * Used for rasterized images, this contains the raw pixel data.
   */
  RefPtr<SourceSurfaceSharedData> mRawSurface MOZ_GUARDED_BY(mMonitor);
  RefPtr<SourceSurfaceSharedData> mBlankRawSurface MOZ_GUARDED_BY(mMonitor);

  /**
   * Used for vector images that were not rasterized directly. This might be a
   * blob recording or native surface.
   */
  RefPtr<SourceSurface> mOptSurface MOZ_GUARDED_BY(mMonitor);

  nsIntRect mDecoded MOZ_GUARDED_BY(mMonitor);

  bool mAborted MOZ_GUARDED_BY(mMonitor);
  bool mFinished MOZ_GUARDED_BY(mMonitor);
  bool mShouldRecycle MOZ_GUARDED_BY(mMonitor);

  //////////////////////////////////////////////////////////////////////////////
  // Effectively const data, only mutated in the Init methods.
  //////////////////////////////////////////////////////////////////////////////

  //! The size of the buffer we are decoding to.
  IntSize mImageSize;

  //! The contents for the frame, as represented in the encoded image. This may
  //! differ from mImageSize because it may be a partial frame. For the first
  //! frame, this means we need to shift the data in place, and for animated
  //! frames, it likely need to combine with a previous frame to get the full
  //! contents.
  IntRect mBlendRect;

  //! This is the region that has changed between this frame and the previous
  //! frame of an animation. For the first frame, this will be the same as
  //! mFrameRect.
  IntRect mDirtyRect;

  //! The timeout for this frame.
  FrameTimeout mTimeout;

  DisposalMethod mDisposalMethod;
  BlendMethod mBlendMethod;
  SurfaceFormat mFormat;

  bool mNonPremult;
};

/**
 * A reference to an imgFrame that holds the imgFrame's surface in memory,
 * allowing drawing. If you have a DrawableFrameRef |ref| and |if (ref)| returns
 * true, then calls to Draw() and GetSourceSurface() are guaranteed to succeed.
 */
class DrawableFrameRef final {
  typedef gfx::DataSourceSurface DataSourceSurface;

 public:
  DrawableFrameRef() = default;

  explicit DrawableFrameRef(imgFrame* aFrame) : mFrame(aFrame) {
    MOZ_ASSERT(aFrame);
    MonitorAutoLock lock(aFrame->mMonitor);

    if (aFrame->mRawSurface) {
      mRef.emplace(aFrame->mRawSurface, DataSourceSurface::READ);
      if (!mRef->IsMapped()) {
        mFrame = nullptr;
        mRef.reset();
      }
    } else if (!aFrame->mOptSurface || !aFrame->mOptSurface->IsValid()) {
      // The optimized surface has become invalid, so we need to redecode.
      // For example, on Windows, there may have been a device reset, and
      // all D2D surfaces now need to be recreated.
      mFrame = nullptr;
    }
  }

  DrawableFrameRef(DrawableFrameRef&& aOther)
      : mFrame(std::move(aOther.mFrame)), mRef(std::move(aOther.mRef)) {}

  DrawableFrameRef& operator=(DrawableFrameRef&& aOther) {
    MOZ_ASSERT(this != &aOther, "Self-moves are prohibited");
    mFrame = std::move(aOther.mFrame);
    mRef = std::move(aOther.mRef);
    return *this;
  }

  explicit operator bool() const { return bool(mFrame); }

  imgFrame* operator->() {
    MOZ_ASSERT(mFrame);
    return mFrame;
  }

  const imgFrame* operator->() const {
    MOZ_ASSERT(mFrame);
    return mFrame;
  }

  imgFrame* get() { return mFrame; }
  const imgFrame* get() const { return mFrame; }

  void reset() {
    mFrame = nullptr;
    mRef.reset();
  }

 private:
  DrawableFrameRef(const DrawableFrameRef& aOther) = delete;
  DrawableFrameRef& operator=(const DrawableFrameRef& aOther) = delete;

  RefPtr<imgFrame> mFrame;
  Maybe<DataSourceSurface::ScopedMap> mRef;
};

/**
 * A reference to an imgFrame that holds the imgFrame's surface in memory in a
 * format appropriate for access as raw data. If you have a RawAccessFrameRef
 * |ref| and |if (ref)| is true, then calls to GetImageData() is guaranteed to
 * succeed. This guarantee is stronger than DrawableFrameRef, so everything that
 * a valid DrawableFrameRef guarantees is also guaranteed by a valid
 * RawAccessFrameRef.
 *
 * This may be considerably more expensive than is necessary just for drawing,
 * so only use this when you need to read or write the raw underlying image data
 * that the imgFrame holds.
 *
 * Once all an imgFrame's RawAccessFrameRefs go out of scope, new
 * RawAccessFrameRefs cannot be created.
 */
class RawAccessFrameRef final {
 public:
  RawAccessFrameRef() = default;

  explicit RawAccessFrameRef(imgFrame* aFrame,
                             gfx::DataSourceSurface::MapType aMapType)
      : mFrame(aFrame) {
    MOZ_ASSERT(mFrame, "Need a frame");

    // Note that we do not use ScopedMap here because it holds a strong
    // reference to the underlying surface. This affects the reuse logic for
    // recycling in imgFrame::InitForDecoderRecycle.
    {
      MonitorAutoLock lock(mFrame->mMonitor);
      gfx::DataSourceSurface::MappedSurface map;
      if (mFrame->mRawSurface && mFrame->mRawSurface->Map(aMapType, &map)) {
        MOZ_ASSERT(map.mData);
        mData = map.mData;
      }
    }

    if (!mData) {
      mFrame = nullptr;
    }
  }

  RawAccessFrameRef(RawAccessFrameRef&& aOther)
      : mFrame(std::move(aOther.mFrame)), mData(aOther.mData) {
    aOther.mData = nullptr;
  }

  ~RawAccessFrameRef() { reset(); }

  RawAccessFrameRef& operator=(RawAccessFrameRef&& aOther) {
    MOZ_ASSERT(this != &aOther, "Self-moves are prohibited");

    if (mFrame) {
      MonitorAutoLock lock(mFrame->mMonitor);
      mFrame->mRawSurface->Unmap();
    }
    mFrame = std::move(aOther.mFrame);
    mData = aOther.mData;
    aOther.mData = nullptr;

    return *this;
  }

  explicit operator bool() const { return bool(mFrame); }

  imgFrame* operator->() {
    MOZ_ASSERT(mFrame);
    return mFrame.get();
  }

  const imgFrame* operator->() const {
    MOZ_ASSERT(mFrame);
    return mFrame;
  }

  imgFrame* get() { return mFrame; }
  const imgFrame* get() const { return mFrame; }

  void reset() {
    if (mFrame) {
      MonitorAutoLock lock(mFrame->mMonitor);
      mFrame->mRawSurface->Unmap();
    }
    mFrame = nullptr;
    mData = nullptr;
  }

  uint8_t* Data() const { return mData; }

 private:
  RawAccessFrameRef(const RawAccessFrameRef& aOther) = delete;
  RawAccessFrameRef& operator=(const RawAccessFrameRef& aOther) = delete;

  RefPtr<imgFrame> mFrame;
  uint8_t* mData = nullptr;
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_imgFrame_h
