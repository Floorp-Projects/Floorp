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
  FULLY_OPAQUE,
  SOME_TRANSPARENCY
};

/**
 * FrameTimeout wraps a frame timeout value (measured in milliseconds) after
 * first normalizing it. This normalization is necessary because some tools
 * generate incorrect frame timeout values which we nevertheless have to
 * support. For this reason, code that deals with frame timeouts should always
 * use a FrameTimeout value rather than the raw value from the image header.
 */
struct FrameTimeout
{
  /**
   * @return a FrameTimeout of zero. This should be used only for math
   * involving FrameTimeout values. You can't obtain a zero FrameTimeout from
   * FromRawMilliseconds().
   */
  static FrameTimeout Zero() { return FrameTimeout(0); }

  /// @return an infinite FrameTimeout.
  static FrameTimeout Forever() { return FrameTimeout(-1); }

  /// @return a FrameTimeout obtained by normalizing a raw timeout value.
  static FrameTimeout FromRawMilliseconds(int32_t aRawMilliseconds)
  {
    // Normalize all infinite timeouts to the same value.
    if (aRawMilliseconds < 0) {
      return FrameTimeout::Forever();
    }

    // Very small timeout values are problematic for two reasons: we don't want
    // to burn energy redrawing animated images extremely fast, and broken tools
    // generate these values when they actually want a "default" value, so such
    // images won't play back right without normalization. For some context,
    // see bug 890743, bug 125137, bug 139677, and bug 207059. The historical
    // behavior of IE and Opera was:
    //   IE 6/Win:
    //     10 - 50ms is normalized to 100ms.
    //     >50ms is used unnormalized.
    //   Opera 7 final/Win:
    //     10ms is normalized to 100ms.
    //     >10ms is used unnormalized.
    if (aRawMilliseconds >= 0 && aRawMilliseconds <= 10 ) {
      return FrameTimeout(100);
    }

    // The provided timeout value is OK as-is.
    return FrameTimeout(aRawMilliseconds);
  }

  bool operator==(const FrameTimeout& aOther) const
  {
    return mTimeout == aOther.mTimeout;
  }

  bool operator!=(const FrameTimeout& aOther) const { return !(*this == aOther); }

  FrameTimeout operator+(const FrameTimeout& aOther)
  {
    if (*this == Forever() || aOther == Forever()) {
      return Forever();
    }

    return FrameTimeout(mTimeout + aOther.mTimeout);
  }

  FrameTimeout& operator+=(const FrameTimeout& aOther)
  {
    *this = *this + aOther;
    return *this;
  }

  /**
   * @return this FrameTimeout's value in milliseconds. Illegal to call on a
   * an infinite FrameTimeout value.
   */
  uint32_t AsMilliseconds() const
  {
    if (*this == Forever()) {
      MOZ_ASSERT_UNREACHABLE("Calling AsMilliseconds() on an infinite FrameTimeout");
      return 100;  // Fail to something sane.
    }

    return uint32_t(mTimeout);
  }

  /**
   * @return this FrameTimeout value encoded so that non-negative values
   * represent a timeout in milliseconds, and -1 represents an infinite
   * timeout.
   *
   * XXX(seth): This is a backwards compatibility hack that should be removed.
   */
  int32_t AsEncodedValueDeprecated() const { return mTimeout; }

private:
  explicit FrameTimeout(int32_t aTimeout)
    : mTimeout(aTimeout)
  { }

  int32_t mTimeout;
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
                            SamplingFilter aSamplingFilter,
                            uint32_t aImageFlags);

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
            SamplingFilter aSamplingFilter, uint32_t aImageFlags);

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
   */
  void Finish(Opacity aFrameOpacity = Opacity::SOME_TRANSPARENCY,
              DisposalMethod aDisposalMethod = DisposalMethod::KEEP,
              FrameTimeout aTimeout = FrameTimeout::FromRawMilliseconds(0),
              BlendMethod aBlendMethod = BlendMethod::OVER,
              const Maybe<IntRect>& aBlendRect = Nothing());

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
  bool NeedsPadding() const { return mFrameRect.TopLeft() != IntPoint(0, 0); }
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

  Color SinglePixelColor() const;
  bool IsSinglePixel() const;

  already_AddRefed<SourceSurface> GetSurface();

  void AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf, size_t& aHeapSizeOut,
                              size_t& aNonHeapSizeOut) const;

private: // methods

  ~imgFrame();

  nsresult LockImageData();
  nsresult UnlockImageData();
  bool     CanOptimizeOpaqueImage();
  nsresult Optimize();

  void AssertImageDataLocked() const;

  bool AreAllPixelsWritten() const;
  nsresult ImageUpdatedInternal(const nsIntRect& aUpdateRect);
  void GetImageDataInternal(uint8_t** aData, uint32_t* length) const;
  uint32_t GetImageBytesPerRow() const;
  uint32_t GetImageDataLength() const;
  already_AddRefed<SourceSurface> GetSurfaceInternal();

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

  //! The timeout for this frame.
  FrameTimeout mTimeout;

  DisposalMethod mDisposalMethod;
  BlendMethod    mBlendMethod;
  Maybe<IntRect> mBlendRect;
  SurfaceFormat  mFormat;

  bool mHasNoAlpha;
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

  // Note that the data stored in gfx::Color is *non-alpha-premultiplied*.
  Color        mSinglePixelColor;

  bool mSinglePixel;
  bool mCompositingFailed;
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
  DrawableFrameRef(const DrawableFrameRef& aOther) = delete;

  RefPtr<imgFrame> mFrame;
  VolatileBufferPtr<uint8_t> mRef;
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
