/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef imgFrame_h
#define imgFrame_h

#include "mozilla/MemoryReporting.h"
#include "mozilla/Move.h"
#include "mozilla/Mutex.h"
#include "mozilla/VolatileBuffer.h"
#include "gfxDrawable.h"
#include "imgIContainer.h"

namespace mozilla {
namespace image {

class ImageRegion;
class DrawableFrameRef;
class RawAccessFrameRef;

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
  nsresult InitForDecoder(const nsIntRect& aRect,
                          SurfaceFormat aFormat,
                          uint8_t aPaletteDepth = 0);

  nsresult InitForDecoder(const nsIntSize& aSize,
                          SurfaceFormat aFormat,
                          uint8_t aPaletteDepth = 0)
  {
    return InitForDecoder(nsIntRect(0, 0, aSize.width, aSize.height),
                          aFormat, aPaletteDepth);
  }


  /**
   * Initialize this imgFrame with a new surface and draw the provided
   * gfxDrawable into it.
   *
   * This is appropriate to use when drawing content into an imgFrame, as it
   * uses the same graphics backend as normal content drawing. The downside is
   * that the underlying surface may not be stored in a volatile buffer on all
   * platforms, and raw access to the surface (using RawAccessRef() or
   * LockImageData()) may be much more expensive than in the InitForDecoder()
   * case.
   */
  nsresult InitWithDrawable(gfxDrawable* aDrawable,
                            const nsIntSize& aSize,
                            const SurfaceFormat aFormat,
                            GraphicsFilter aFilter,
                            uint32_t aImageFlags);

  DrawableFrameRef DrawableRef();
  RawAccessFrameRef RawAccessRef();

  bool Draw(gfxContext* aContext, const ImageRegion& aRegion,
            const nsIntMargin& aPadding, GraphicsFilter aFilter,
            uint32_t aImageFlags);

  nsresult ImageUpdated(const nsIntRect &aUpdateRect);

  nsIntRect GetRect() const;
  IntSize GetSize() const { return mSize; }
  bool NeedsPadding() const { return mOffset != nsIntPoint(0, 0); }
  int32_t GetStride() const;
  SurfaceFormat GetFormat() const;
  bool GetNeedsBackground() const;
  uint32_t GetImageBytesPerRow() const;
  uint32_t GetImageDataLength() const;
  bool GetIsPaletted() const;
  bool GetHasAlpha() const;
  void GetImageData(uint8_t **aData, uint32_t *length) const;
  uint8_t* GetImageData() const;
  void GetPaletteData(uint32_t **aPalette, uint32_t *length) const;
  uint32_t* GetPaletteData() const;

  int32_t GetRawTimeout() const;
  void SetRawTimeout(int32_t aTimeout);

  int32_t GetFrameDisposalMethod() const;
  void SetFrameDisposalMethod(int32_t aFrameDisposalMethod);
  int32_t GetBlendMethod() const;
  void SetBlendMethod(int32_t aBlendMethod);
  bool ImageComplete() const;

  void SetHasNoAlpha();
  void SetAsNonPremult(bool aIsNonPremult);

  bool GetCompositingFailed() const;
  void SetCompositingFailed(bool val);

  nsresult LockImageData();
  nsresult UnlockImageData();

  void SetDiscardable();
  void SetOptimizable();

  TemporaryRef<SourceSurface> GetSurface();
  TemporaryRef<DrawTarget> GetDrawTarget();

  Color
  SinglePixelColor()
  {
    return mSinglePixelColor;
  }

  bool IsSinglePixel()
  {
    return mSinglePixel;
  }

  TemporaryRef<SourceSurface> CachedSurface();

  size_t SizeOfExcludingThisWithComputedFallbackIfHeap(
           gfxMemoryLocation aLocation,
           MallocSizeOf aMallocSizeOf) const;

  uint8_t GetPaletteDepth() const { return mPaletteDepth; }
  uint32_t PaletteDataLength() const {
    if (!mPaletteDepth)
      return 0;

    return ((1 << mPaletteDepth) * sizeof(uint32_t));
  }

private: // methods

  ~imgFrame();

  nsresult Optimize();

  struct SurfaceWithFormat {
    nsRefPtr<gfxDrawable> mDrawable;
    SurfaceFormat mFormat;
    SurfaceWithFormat() {}
    SurfaceWithFormat(gfxDrawable* aDrawable, SurfaceFormat aFormat)
     : mDrawable(aDrawable), mFormat(aFormat) {}
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
  RefPtr<DataSourceSurface> mImageSurface;
  RefPtr<SourceSurface> mOptSurface;

  IntSize      mSize;
  nsIntPoint   mOffset;

  nsIntRect    mDecoded;

  mutable Mutex mDecodedMutex;

  // The palette and image data for images that are paletted, since Cairo
  // doesn't support these images.
  // The paletted data comes first, then the image data itself.
  // Total length is PaletteDataLength() + GetImageDataLength().
  uint8_t*     mPalettedImageData;

  // Note that the data stored in gfx::Color is *non-alpha-premultiplied*.
  Color        mSinglePixelColor;

  int32_t      mTimeout; // -1 means display forever
  int32_t      mDisposalMethod;

  /** Indicates how many readers currently have locked this frame */
  int32_t mLockCount;

  RefPtr<VolatileBuffer> mVBuf;
  VolatileBufferPtr<uint8_t> mVBufPtr;

  SurfaceFormat mFormat;
  uint8_t      mPaletteDepth;
  int8_t       mBlendMethod;
  bool mSinglePixel;
  bool mCompositingFailed;
  bool mNonPremult;
  bool mDiscardable;
  bool mOptimizable;

  /** Have we called DiscardTracker::InformAllocation()? */
  bool mInformedDiscardTracker;

  friend class DrawableFrameRef;
  friend class RawAccessFrameRef;
};

/**
 * A reference to an imgFrame that holds the imgFrame's surface in memory,
 * allowing drawing. If you have a DrawableFrameRef |ref| and |if (ref)| returns
 * true, then calls to Draw() and GetSurface() are guaranteed to succeed.
 */
class DrawableFrameRef MOZ_FINAL
{
  // Implementation details for safe boolean conversion.
  typedef void (DrawableFrameRef::* ConvertibleToBool)(float*****, double*****);
  void nonNull(float*****, double*****) {}

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

  operator ConvertibleToBool() const
  {
    return bool(mFrame) ? &DrawableFrameRef::nonNull : 0;
  }

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
class RawAccessFrameRef MOZ_FINAL
{
  // Implementation details for safe boolean conversion.
  typedef void (RawAccessFrameRef::* ConvertibleToBool)(float*****, double*****);
  void nonNull(float*****, double*****) {}

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

  operator ConvertibleToBool() const
  {
    return bool(mFrame) ? &RawAccessFrameRef::nonNull : 0;
  }

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

#endif /* imgFrame_h */
