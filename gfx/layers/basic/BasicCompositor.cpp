/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BasicCompositor.h"
#include "BasicLayersImpl.h"  // for FillRectWithMask
#include "GeckoProfiler.h"
#include "TextureHostBasic.h"
#include "mozilla/layers/Effects.h"
#include "nsIWidget.h"
#include "gfx2DGlue.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/Helpers.h"
#include "mozilla/gfx/Swizzle.h"
#include "mozilla/gfx/Tools.h"
#include "mozilla/gfx/ssse3-scaler.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/SSE.h"
#include "mozilla/StaticPrefs_layers.h"
#include "mozilla/StaticPrefs_nglayout.h"
#include "gfxPlatform.h"
#include "gfxUtils.h"
#include "YCbCrUtils.h"
#include <algorithm>
#include "ImageContainer.h"

#ifdef XP_MACOSX
#  include "mozilla/gfx/MacIOSurface.h"
#  include "mozilla/layers/NativeLayerCA.h"
#endif

namespace mozilla {
using namespace mozilla::gfx;

namespace layers {

class DataTextureSourceBasic : public DataTextureSource,
                               public TextureSourceBasic {
 public:
  const char* Name() const override { return "DataTextureSourceBasic"; }

  explicit DataTextureSourceBasic(DataSourceSurface* aSurface)
      : mSurface(aSurface), mWrappingExistingData(!!aSurface) {}

  DataTextureSource* AsDataTextureSource() override {
    // If the texture wraps someone else's memory we'd rather not use it as
    // a DataTextureSource per say (that is call Update on it).
    return mWrappingExistingData ? nullptr : this;
  }

  TextureSourceBasic* AsSourceBasic() override { return this; }

  gfx::SourceSurface* GetSurface(DrawTarget* aTarget) override {
    return mSurface;
  }

  SurfaceFormat GetFormat() const override {
    return mSurface ? mSurface->GetFormat() : gfx::SurfaceFormat::UNKNOWN;
  }

  IntSize GetSize() const override {
    return mSurface ? mSurface->GetSize() : gfx::IntSize(0, 0);
  }

  bool Update(gfx::DataSourceSurface* aSurface,
              nsIntRegion* aDestRegion = nullptr,
              gfx::IntPoint* aSrcOffset = nullptr) override {
    MOZ_ASSERT(!mWrappingExistingData);
    if (mWrappingExistingData) {
      return false;
    }
    mSurface = aSurface;
    return true;
  }

  void DeallocateDeviceData() override {
    mSurface = nullptr;
    SetUpdateSerial(0);
  }

 public:
  RefPtr<gfx::DataSourceSurface> mSurface;
  bool mWrappingExistingData;
};

/**
 * WrappingTextureSourceYCbCrBasic wraps YUV format BufferTextureHost to defer
 * yuv->rgb conversion. The conversion happens when GetSurface is called.
 */
class WrappingTextureSourceYCbCrBasic : public DataTextureSource,
                                        public TextureSourceBasic {
 public:
  const char* Name() const override {
    return "WrappingTextureSourceYCbCrBasic";
  }

  explicit WrappingTextureSourceYCbCrBasic(BufferTextureHost* aTexture)
      : mTexture(aTexture), mSize(aTexture->GetSize()), mNeedsUpdate(true) {
    mFromYCBCR = true;
  }

  DataTextureSource* AsDataTextureSource() override { return this; }

  TextureSourceBasic* AsSourceBasic() override { return this; }

  WrappingTextureSourceYCbCrBasic* AsWrappingTextureSourceYCbCrBasic()
      override {
    return this;
  }

  gfx::SourceSurface* GetSurface(DrawTarget* aTarget) override {
    if (mSurface && !mNeedsUpdate) {
      return mSurface;
    }
    MOZ_ASSERT(mTexture);
    if (!mTexture) {
      return nullptr;
    }

    if (!mSurface) {
      mSurface =
          Factory::CreateDataSourceSurface(mSize, gfx::SurfaceFormat::B8G8R8X8);
    }
    if (!mSurface) {
      return nullptr;
    }
    MOZ_ASSERT(mTexture->GetBufferDescriptor().type() ==
               BufferDescriptor::TYCbCrDescriptor);
    MOZ_ASSERT(mTexture->GetSize() == mSize);

    mSurface = ImageDataSerializer::DataSourceSurfaceFromYCbCrDescriptor(
        mTexture->GetBuffer(),
        mTexture->GetBufferDescriptor().get_YCbCrDescriptor(), mSurface);
    mNeedsUpdate = false;
    return mSurface;
  }

  SurfaceFormat GetFormat() const override {
    return gfx::SurfaceFormat::B8G8R8X8;
  }

  IntSize GetSize() const override { return mSize; }

  virtual bool Update(gfx::DataSourceSurface* aSurface,
                      nsIntRegion* aDestRegion = nullptr,
                      gfx::IntPoint* aSrcOffset = nullptr) override {
    return false;
  }

  void DeallocateDeviceData() override {
    mTexture = nullptr;
    mSurface = nullptr;
    SetUpdateSerial(0);
  }

  void Unbind() override { mNeedsUpdate = true; }

  void SetBufferTextureHost(BufferTextureHost* aTexture) override {
    mTexture = aTexture;
    mNeedsUpdate = true;
  }

  void ConvertAndScale(const SurfaceFormat& aDestFormat,
                       const IntSize& aDestSize, unsigned char* aDestBuffer,
                       int32_t aStride) {
    MOZ_ASSERT(mTexture);
    if (!mTexture) {
      return;
    }
    MOZ_ASSERT(mTexture->GetBufferDescriptor().type() ==
               BufferDescriptor::TYCbCrDescriptor);
    MOZ_ASSERT(mTexture->GetSize() == mSize);
    ImageDataSerializer::ConvertAndScaleFromYCbCrDescriptor(
        mTexture->GetBuffer(),
        mTexture->GetBufferDescriptor().get_YCbCrDescriptor(), aDestFormat,
        aDestSize, aDestBuffer, aStride);
  }

 public:
  BufferTextureHost* mTexture;
  const gfx::IntSize mSize;
  RefPtr<gfx::DataSourceSurface> mSurface;
  bool mNeedsUpdate;
};

class BasicAsyncReadbackBuffer final : public AsyncReadbackBuffer {
 public:
  explicit BasicAsyncReadbackBuffer(const IntSize& aSize)
      : AsyncReadbackBuffer(aSize) {}

  bool MapAndCopyInto(DataSourceSurface* aSurface,
                      const IntSize& aReadSize) const override;

  void TakeSurface(SourceSurface* aSurface) { mSurface = aSurface; }

 private:
  RefPtr<SourceSurface> mSurface;
};

bool BasicAsyncReadbackBuffer::MapAndCopyInto(DataSourceSurface* aSurface,
                                              const IntSize& aReadSize) const {
  if (!mSurface) {
    return false;
  }

  MOZ_RELEASE_ASSERT(aReadSize <= aSurface->GetSize());
  RefPtr<DataSourceSurface> source = mSurface->GetDataSurface();

  DataSourceSurface::ScopedMap sourceMap(source, DataSourceSurface::READ);
  DataSourceSurface::ScopedMap destMap(aSurface, DataSourceSurface::WRITE);

  return SwizzleData(sourceMap.GetData(), sourceMap.GetStride(),
                     mSurface->GetFormat(), destMap.GetData(),
                     destMap.GetStride(), aSurface->GetFormat(), aReadSize);
}

BasicCompositor::BasicCompositor(CompositorBridgeParent* aParent,
                                 widget::CompositorWidget* aWidget)
    : Compositor(aWidget, aParent),
      mIsPendingEndRemoteDrawing(false),
      mFullWindowRenderTarget(nullptr) {
  MOZ_COUNT_CTOR(BasicCompositor);

  // The widget backends may create intermediate Cairo surfaces to deal with
  // various window buffers, regardless of actual content backend type, when
  // using the basic compositor. Ensure that the buffers will be able to fit
  // in or blit with a Cairo surface.
  mMaxTextureSize =
      std::min(Factory::GetMaxSurfaceSize(gfxVars::ContentBackend()),
               Factory::GetMaxSurfaceSize(BackendType::CAIRO));
}

BasicCompositor::~BasicCompositor() { MOZ_COUNT_DTOR(BasicCompositor); }

bool BasicCompositor::Initialize(nsCString* const out_failureReason) {
  return mWidget ? mWidget->InitCompositor(this) : false;
};

int32_t BasicCompositor::GetMaxTextureSize() const { return mMaxTextureSize; }

void BasicCompositingRenderTarget::BindRenderTarget() {
  if (mClearOnBind) {
    mDrawTarget->ClearRect(Rect(GetRect()));
    mClearOnBind = false;
  }
}

void BasicCompositor::Destroy() {
  if (mIsPendingEndRemoteDrawing) {
    // Force to end previous remote drawing.
    EndRemoteDrawing();
    MOZ_ASSERT(!mIsPendingEndRemoteDrawing);
  }
  mWidget->CleanupRemoteDrawing();

  Compositor::Destroy();
}

TextureFactoryIdentifier BasicCompositor::GetTextureFactoryIdentifier() {
  TextureFactoryIdentifier ident(LayersBackend::LAYERS_BASIC,
                                 XRE_GetProcessType(), GetMaxTextureSize());
  return ident;
}

already_AddRefed<CompositingRenderTarget> BasicCompositor::CreateRenderTarget(
    const IntRect& aRect, SurfaceInitMode aInit) {
  MOZ_ASSERT(!aRect.IsZeroArea(),
             "Trying to create a render target of invalid size");

  if (aRect.IsZeroArea()) {
    return nullptr;
  }

  RefPtr<DrawTarget> target =
      mRenderTarget->mDrawTarget->CreateSimilarDrawTarget(
          aRect.Size(), SurfaceFormat::B8G8R8A8);

  if (!target) {
    return nullptr;
  }

  RefPtr<BasicCompositingRenderTarget> rt =
      new BasicCompositingRenderTarget(target, aRect, aRect.TopLeft());

  rt->mDrawTarget->SetTransform(Matrix::Translation(-rt->GetOrigin()));

  return rt.forget();
}

already_AddRefed<CompositingRenderTarget>
BasicCompositor::CreateRenderTargetFromSource(
    const IntRect& aRect, const CompositingRenderTarget* aSource,
    const IntPoint& aSourcePoint) {
  MOZ_CRASH("GFX: Shouldn't be called!");
  return nullptr;
}

already_AddRefed<CompositingRenderTarget>
BasicCompositor::CreateRootRenderTarget(DrawTarget* aDrawTarget,
                                        const IntRect& aDrawTargetRect,
                                        const IntRegion& aClearRegion) {
  RefPtr<BasicCompositingRenderTarget> rt = new BasicCompositingRenderTarget(
      aDrawTarget, aDrawTargetRect, IntPoint());

  rt->mDrawTarget->SetTransform(Matrix::Translation(-rt->GetOrigin()));

  if (!aClearRegion.IsEmpty()) {
    gfx::IntRect clearRect = aClearRegion.GetBounds();
    gfxUtils::ClipToRegion(rt->mDrawTarget, aClearRegion);
    rt->mDrawTarget->ClearRect(gfx::Rect(clearRect));
    rt->mDrawTarget->PopClip();
  }

  return rt.forget();
}

already_AddRefed<DataTextureSource> BasicCompositor::CreateDataTextureSource(
    TextureFlags aFlags) {
  RefPtr<DataTextureSourceBasic> result = new DataTextureSourceBasic(nullptr);
  if (aFlags & TextureFlags::RGB_FROM_YCBCR) {
    result->mFromYCBCR = true;
  }
  return result.forget();
}

already_AddRefed<DataTextureSource>
BasicCompositor::CreateDataTextureSourceAround(DataSourceSurface* aSurface) {
  RefPtr<DataTextureSource> result = new DataTextureSourceBasic(aSurface);
  return result.forget();
}

already_AddRefed<DataTextureSource>
BasicCompositor::CreateDataTextureSourceAroundYCbCr(TextureHost* aTexture) {
  BufferTextureHost* bufferTexture = aTexture->AsBufferTextureHost();
  MOZ_ASSERT(bufferTexture);

  if (!bufferTexture) {
    return nullptr;
  }
  RefPtr<DataTextureSource> result =
      new WrappingTextureSourceYCbCrBasic(bufferTexture);
  return result.forget();
}

bool BasicCompositor::SupportsEffect(EffectTypes aEffect) {
  return aEffect != EffectTypes::YCBCR &&
         aEffect != EffectTypes::COMPONENT_ALPHA;
}

bool BasicCompositor::SupportsLayerGeometry() const {
  return StaticPrefs::layers_geometry_basic_enabled();
}

static RefPtr<gfx::Path> BuildPathFromPolygon(const RefPtr<DrawTarget>& aDT,
                                              const gfx::Polygon& aPolygon) {
  MOZ_ASSERT(!aPolygon.IsEmpty());

  RefPtr<PathBuilder> pathBuilder = aDT->CreatePathBuilder();
  const nsTArray<Point4D>& points = aPolygon.GetPoints();

  pathBuilder->MoveTo(points[0].As2DPoint());

  for (size_t i = 1; i < points.Length(); ++i) {
    pathBuilder->LineTo(points[i].As2DPoint());
  }

  pathBuilder->Close();
  return pathBuilder->Finish();
}

static void DrawSurface(gfx::DrawTarget* aDest, const gfx::Rect& aDestRect,
                        const gfx::Rect& /* aClipRect */,
                        const gfx::Color& aColor,
                        const gfx::DrawOptions& aOptions,
                        gfx::SourceSurface* aMask,
                        const gfx::Matrix* aMaskTransform) {
  FillRectWithMask(aDest, aDestRect, aColor, aOptions, aMask, aMaskTransform);
}

static void DrawSurface(gfx::DrawTarget* aDest, const gfx::Polygon& aPolygon,
                        const gfx::Rect& aClipRect, const gfx::Color& aColor,
                        const gfx::DrawOptions& aOptions,
                        gfx::SourceSurface* aMask,
                        const gfx::Matrix* aMaskTransform) {
  RefPtr<Path> path = BuildPathFromPolygon(aDest, aPolygon);
  FillPathWithMask(aDest, path, aClipRect, aColor, aOptions, aMask,
                   aMaskTransform);
}

static void DrawTextureSurface(
    gfx::DrawTarget* aDest, const gfx::Rect& aDestRect,
    const gfx::Rect& /* aClipRect */, gfx::SourceSurface* aSource,
    gfx::SamplingFilter aSamplingFilter, const gfx::DrawOptions& aOptions,
    ExtendMode aExtendMode, gfx::SourceSurface* aMask,
    const gfx::Matrix* aMaskTransform, const Matrix* aSurfaceTransform) {
  FillRectWithMask(aDest, aDestRect, aSource, aSamplingFilter, aOptions,
                   aExtendMode, aMask, aMaskTransform, aSurfaceTransform);
}

static void DrawTextureSurface(
    gfx::DrawTarget* aDest, const gfx::Polygon& aPolygon,
    const gfx::Rect& aClipRect, gfx::SourceSurface* aSource,
    gfx::SamplingFilter aSamplingFilter, const gfx::DrawOptions& aOptions,
    ExtendMode aExtendMode, gfx::SourceSurface* aMask,
    const gfx::Matrix* aMaskTransform, const Matrix* aSurfaceTransform) {
  RefPtr<Path> path = BuildPathFromPolygon(aDest, aPolygon);
  FillPathWithMask(aDest, path, aClipRect, aSource, aSamplingFilter, aOptions,
                   aExtendMode, aMask, aMaskTransform, aSurfaceTransform);
}

template <typename Geometry>
static void DrawSurfaceWithTextureCoords(
    gfx::DrawTarget* aDest, const Geometry& aGeometry,
    const gfx::Rect& aDestRect, gfx::SourceSurface* aSource,
    const gfx::Rect& aTextureCoords, gfx::SamplingFilter aSamplingFilter,
    const gfx::DrawOptions& aOptions, gfx::SourceSurface* aMask,
    const gfx::Matrix* aMaskTransform) {
  if (!aSource) {
    gfxWarning() << "DrawSurfaceWithTextureCoords problem "
                 << gfx::hexa(aSource) << " and " << gfx::hexa(aMask);
    return;
  }

  // Convert aTextureCoords into aSource's coordinate space
  gfxRect sourceRect(aTextureCoords.X() * aSource->GetSize().width,
                     aTextureCoords.Y() * aSource->GetSize().height,
                     aTextureCoords.Width() * aSource->GetSize().width,
                     aTextureCoords.Height() * aSource->GetSize().height);

  // Floating point error can accumulate above and we know our visible region
  // is integer-aligned, so round it out.
  sourceRect.Round();

  // Compute a transform that maps sourceRect to aDestRect.
  Matrix matrix = gfxUtils::TransformRectToRect(
      sourceRect, gfx::IntPoint::Truncate(aDestRect.X(), aDestRect.Y()),
      gfx::IntPoint::Truncate(aDestRect.XMost(), aDestRect.Y()),
      gfx::IntPoint::Truncate(aDestRect.XMost(), aDestRect.YMost()));

  // Only use REPEAT if aTextureCoords is outside (0, 0, 1, 1).
  gfx::Rect unitRect(0, 0, 1, 1);
  ExtendMode mode = unitRect.Contains(aTextureCoords) ? ExtendMode::CLAMP
                                                      : ExtendMode::REPEAT;

  DrawTextureSurface(aDest, aGeometry, aDestRect, aSource, aSamplingFilter,
                     aOptions, mode, aMask, aMaskTransform, &matrix);
}

static void SetupMask(const EffectChain& aEffectChain, DrawTarget* aDest,
                      const IntPoint& aOffset,
                      RefPtr<SourceSurface>& aMaskSurface,
                      Matrix& aMaskTransform) {
  if (aEffectChain.mSecondaryEffects[EffectTypes::MASK]) {
    EffectMask* effectMask = static_cast<EffectMask*>(
        aEffectChain.mSecondaryEffects[EffectTypes::MASK].get());
    aMaskSurface = effectMask->mMaskTexture->AsSourceBasic()->GetSurface(aDest);
    if (!aMaskSurface) {
      gfxWarning() << "Invalid sourceMask effect";
    }
    MOZ_ASSERT(effectMask->mMaskTransform.Is2D(),
               "How did we end up with a 3D transform here?!");
    aMaskTransform = effectMask->mMaskTransform.As2D();
    aMaskTransform.PostTranslate(-aOffset.x, -aOffset.y);
  }
}

static bool AttemptVideoScale(TextureSourceBasic* aSource,
                              const SourceSurface* aSourceMask,
                              gfx::Float aOpacity, CompositionOp aBlendMode,
                              const TexturedEffect* aTexturedEffect,
                              const Matrix& aNewTransform,
                              const gfx::Rect& aRect,
                              const gfx::Rect& aClipRect, DrawTarget* aDest,
                              const DrawTarget* aBuffer) {
#ifdef MOZILLA_SSE_HAVE_CPUID_DETECTION
  if (!mozilla::supports_ssse3()) return false;
  if (aNewTransform
          .IsTranslation())  // unscaled painting should take the regular path
    return false;
  if (aNewTransform.HasNonAxisAlignedTransform() ||
      aNewTransform.HasNegativeScaling())
    return false;
  if (aSourceMask || aOpacity != 1.0f) return false;
  if (aBlendMode != CompositionOp::OP_OVER &&
      aBlendMode != CompositionOp::OP_SOURCE)
    return false;

  IntRect dstRect;
  // the compiler should know a lot about aNewTransform at this point
  // maybe it can do some sophisticated optimization of the following
  if (!aNewTransform.TransformBounds(aRect).ToIntRect(&dstRect)) return false;

  IntRect clipRect;
  if (!aClipRect.ToIntRect(&clipRect)) return false;

  if (!(aTexturedEffect->mTextureCoords == Rect(0.0f, 0.0f, 1.0f, 1.0f)))
    return false;
  if (aDest->GetFormat() == SurfaceFormat::R5G6B5_UINT16) return false;

  uint8_t* dstData;
  IntSize dstSize;
  int32_t dstStride;
  SurfaceFormat dstFormat;
  if (aDest->LockBits(&dstData, &dstSize, &dstStride, &dstFormat)) {
    // If we're not painting to aBuffer the clip will
    // be applied later
    IntRect fillRect = dstRect;
    if (aDest == aBuffer) {
      // we need to clip fillRect because LockBits ignores the clip on the aDest
      fillRect = fillRect.Intersect(clipRect);
    }

    fillRect = fillRect.Intersect(IntRect(IntPoint(0, 0), aDest->GetSize()));
    IntPoint offset = fillRect.TopLeft() - dstRect.TopLeft();

    RefPtr<DataSourceSurface> srcSource =
        aSource->GetSurface(aDest)->GetDataSurface();
    DataSourceSurface::ScopedMap mapSrc(srcSource, DataSourceSurface::READ);

    bool success = ssse3_scale_data(
        (uint32_t*)mapSrc.GetData(), srcSource->GetSize().width,
        srcSource->GetSize().height, mapSrc.GetStride() / 4,
        ((uint32_t*)dstData) + fillRect.X() + (dstStride / 4) * fillRect.Y(),
        dstRect.Width(), dstRect.Height(), dstStride / 4, offset.x, offset.y,
        fillRect.Width(), fillRect.Height());

    aDest->ReleaseBits(dstData);
    return success;
  } else
#endif  // MOZILLA_SSE_HAVE_CPUID_DETECTION
    return false;
}

static bool AttemptVideoConvertAndScale(
    TextureSource* aSource, const SourceSurface* aSourceMask,
    gfx::Float aOpacity, CompositionOp aBlendMode,
    const TexturedEffect* aTexturedEffect, const Matrix& aNewTransform,
    const gfx::Rect& aRect, const gfx::Rect& aClipRect, DrawTarget* aDest,
    const DrawTarget* aBuffer) {
#if defined(XP_WIN) && defined(_M_X64)
  // libyuv does not support SIMD scaling on win 64bit. See Bug 1295927.
  return false;
#endif

  WrappingTextureSourceYCbCrBasic* wrappingSource =
      aSource->AsWrappingTextureSourceYCbCrBasic();
  if (!wrappingSource) return false;
#ifdef MOZILLA_SSE_HAVE_CPUID_DETECTION
  if (!mozilla::supports_ssse3())  // libyuv requests SSSE3 for fast YUV
                                   // conversion.
    return false;
  if (aNewTransform.HasNonAxisAlignedTransform() ||
      aNewTransform.HasNegativeScaling())
    return false;
  if (aSourceMask || aOpacity != 1.0f) return false;
  if (aBlendMode != CompositionOp::OP_OVER &&
      aBlendMode != CompositionOp::OP_SOURCE)
    return false;

  IntRect dstRect;
  // the compiler should know a lot about aNewTransform at this point
  // maybe it can do some sophisticated optimization of the following
  if (!aNewTransform.TransformBounds(aRect).ToIntRect(&dstRect)) return false;

  IntRect clipRect;
  if (!aClipRect.ToIntRect(&clipRect)) return false;

  if (!(aTexturedEffect->mTextureCoords == Rect(0.0f, 0.0f, 1.0f, 1.0f)))
    return false;
  if (aDest->GetFormat() == SurfaceFormat::R5G6B5_UINT16) return false;

  if (aDest == aBuffer && !clipRect.Contains(dstRect)) return false;
  if (!IntRect(IntPoint(0, 0), aDest->GetSize()).Contains(dstRect))
    return false;

  uint8_t* dstData;
  IntSize dstSize;
  int32_t dstStride;
  SurfaceFormat dstFormat;
  if (aDest->LockBits(&dstData, &dstSize, &dstStride, &dstFormat)) {
    wrappingSource->ConvertAndScale(
        dstFormat, dstRect.Size(),
        dstData + ptrdiff_t(dstRect.X()) * BytesPerPixel(dstFormat) +
            ptrdiff_t(dstRect.Y()) * dstStride,
        dstStride);
    aDest->ReleaseBits(dstData);
    return true;
  } else
#endif  // MOZILLA_SSE_HAVE_CPUID_DETECTION
    return false;
}

void BasicCompositor::DrawQuad(const gfx::Rect& aRect,
                               const gfx::IntRect& aClipRect,
                               const EffectChain& aEffectChain,
                               gfx::Float aOpacity,
                               const gfx::Matrix4x4& aTransform,
                               const gfx::Rect& aVisibleRect) {
  DrawGeometry(aRect, aRect, aClipRect, aEffectChain, aOpacity, aTransform,
               aVisibleRect, true);
}

void BasicCompositor::DrawPolygon(const gfx::Polygon& aPolygon,
                                  const gfx::Rect& aRect,
                                  const gfx::IntRect& aClipRect,
                                  const EffectChain& aEffectChain,
                                  gfx::Float aOpacity,
                                  const gfx::Matrix4x4& aTransform,
                                  const gfx::Rect& aVisibleRect) {
  DrawGeometry(aPolygon, aRect, aClipRect, aEffectChain, aOpacity, aTransform,
               aVisibleRect, false);
}

template <typename Geometry>
void BasicCompositor::DrawGeometry(
    const Geometry& aGeometry, const gfx::Rect& aRect,
    const gfx::IntRect& aClipRect, const EffectChain& aEffectChain,
    gfx::Float aOpacity, const gfx::Matrix4x4& aTransform,
    const gfx::Rect& aVisibleRect, const bool aEnableAA) {
  RefPtr<DrawTarget> buffer = mRenderTarget->mDrawTarget;
  AutoRestoreTransform autoRestoreTransform(buffer);

  Matrix newTransform;
  Rect transformBounds;
  Matrix4x4 new3DTransform;
  IntPoint offset = mRenderTarget->GetOrigin();

  // For 2D drawing, |dest| and |buffer| are the same surface. For 3D drawing,
  // |dest| is a temporary surface.
  RefPtr<DrawTarget> dest;

  if (aTransform.Is2D()) {
    dest = buffer;
    newTransform = aTransform.As2D();
  } else {
    // Create a temporary surface for the transform.
    dest = Factory::CreateDrawTarget(gfxVars::ContentBackend(),
                                     RoundedOut(aRect).Size(),
                                     SurfaceFormat::B8G8R8A8);
    if (!dest) {
      return;
    }

    dest->SetTransform(Matrix::Translation(-aRect.X(), -aRect.Y()));

    // Get the bounds post-transform.
    transformBounds = aTransform.TransformAndClipBounds(
        aRect, Rect(mRenderTarget->GetRect()));
    transformBounds.RoundOut();

    if (transformBounds.IsEmpty()) {
      return;
    }

    newTransform = Matrix();

    // When we apply the 3D transformation, we do it against a temporary
    // surface, so undo the coordinate offset.
    new3DTransform = aTransform;
    new3DTransform.PreTranslate(aRect.X(), aRect.Y(), 0);
  }

  // The current transform on buffer is always only a translation by `-offset`.
  // aClipRect is relative to mRenderTarget->GetClipSpaceOrigin().
  // For non-root render targets, the clip space origin is equal to `offset`.
  // For the root render target, the clip space origin is at (0, 0) and the
  // offset can be anywhere.
  IntRect clipRectInRenderTargetSpace =
      aClipRect + mRenderTarget->GetClipSpaceOrigin();
  buffer->PushClipRect(Rect(clipRectInRenderTargetSpace));
  Rect deviceSpaceClipRect(clipRectInRenderTargetSpace - offset);

  newTransform.PostTranslate(-offset.x, -offset.y);
  buffer->SetTransform(newTransform);

  RefPtr<SourceSurface> sourceMask;
  Matrix maskTransform;
  if (aTransform.Is2D()) {
    SetupMask(aEffectChain, dest, offset, sourceMask, maskTransform);
  }

  CompositionOp blendMode = CompositionOp::OP_OVER;
  if (Effect* effect =
          aEffectChain.mSecondaryEffects[EffectTypes::BLEND_MODE].get()) {
    blendMode = static_cast<EffectBlendMode*>(effect)->mBlendMode;
  }

  const AntialiasMode aaMode =
      aEnableAA ? AntialiasMode::DEFAULT : AntialiasMode::NONE;

  DrawOptions drawOptions(aOpacity, blendMode, aaMode);

  switch (aEffectChain.mPrimaryEffect->mType) {
    case EffectTypes::SOLID_COLOR: {
      EffectSolidColor* effectSolidColor =
          static_cast<EffectSolidColor*>(aEffectChain.mPrimaryEffect.get());

      bool unboundedOp = !IsOperatorBoundByMask(blendMode);
      if (unboundedOp) {
        dest->PushClipRect(aRect);
      }

      DrawSurface(dest, aGeometry, aRect, effectSolidColor->mColor, drawOptions,
                  sourceMask, &maskTransform);

      if (unboundedOp) {
        dest->PopClip();
      }
      break;
    }
    case EffectTypes::RGB: {
      TexturedEffect* texturedEffect =
          static_cast<TexturedEffect*>(aEffectChain.mPrimaryEffect.get());
      TextureSourceBasic* source = texturedEffect->mTexture->AsSourceBasic();

      if (source && texturedEffect->mPremultiplied) {
        // we have a fast path for video here
        if (source->mFromYCBCR &&
            AttemptVideoConvertAndScale(texturedEffect->mTexture, sourceMask,
                                        aOpacity, blendMode, texturedEffect,
                                        newTransform, aRect,
                                        deviceSpaceClipRect, dest, buffer)) {
          // we succeeded in convert and scaling
        } else if (source->mFromYCBCR && !source->GetSurface(dest)) {
          gfxWarning() << "Failed to get YCbCr to rgb surface.";
        } else if (source->mFromYCBCR &&
                   AttemptVideoScale(source, sourceMask, aOpacity, blendMode,
                                     texturedEffect, newTransform, aRect,
                                     deviceSpaceClipRect, dest, buffer)) {
          // we succeeded in scaling
        } else {
          DrawSurfaceWithTextureCoords(
              dest, aGeometry, aRect, source->GetSurface(dest),
              texturedEffect->mTextureCoords, texturedEffect->mSamplingFilter,
              drawOptions, sourceMask, &maskTransform);
        }
      } else if (source) {
        SourceSurface* srcSurf = source->GetSurface(dest);
        if (srcSurf) {
          RefPtr<DataSourceSurface> srcData = srcSurf->GetDataSurface();

          // Yes, we re-create the premultiplied data every time.
          // This might be better with a cache, eventually.
          RefPtr<DataSourceSurface> premultData =
              gfxUtils::CreatePremultipliedDataSurface(srcData);

          DrawSurfaceWithTextureCoords(dest, aGeometry, aRect, premultData,
                                       texturedEffect->mTextureCoords,
                                       texturedEffect->mSamplingFilter,
                                       drawOptions, sourceMask, &maskTransform);
        }
      } else {
        gfxDevCrash(LogReason::IncompatibleBasicTexturedEffect)
            << "Bad for basic with " << texturedEffect->mTexture->Name()
            << " and " << gfx::hexa(sourceMask);
      }

      break;
    }
    case EffectTypes::YCBCR: {
      MOZ_CRASH("Can't (easily) support component alpha with BasicCompositor!");
      break;
    }
    case EffectTypes::RENDER_TARGET: {
      EffectRenderTarget* effectRenderTarget =
          static_cast<EffectRenderTarget*>(aEffectChain.mPrimaryEffect.get());
      RefPtr<BasicCompositingRenderTarget> surface =
          static_cast<BasicCompositingRenderTarget*>(
              effectRenderTarget->mRenderTarget.get());
      RefPtr<SourceSurface> sourceSurf = surface->mDrawTarget->Snapshot();

      DrawSurfaceWithTextureCoords(dest, aGeometry, aRect, sourceSurf,
                                   effectRenderTarget->mTextureCoords,
                                   effectRenderTarget->mSamplingFilter,
                                   drawOptions, sourceMask, &maskTransform);
      break;
    }
    case EffectTypes::COMPONENT_ALPHA: {
      MOZ_CRASH("Can't (easily) support component alpha with BasicCompositor!");
      break;
    }
    default: {
      MOZ_CRASH("Invalid effect type!");
      break;
    }
  }

  if (!aTransform.Is2D()) {
    dest->Flush();

    RefPtr<SourceSurface> destSnapshot = dest->Snapshot();

    SetupMask(aEffectChain, buffer, offset, sourceMask, maskTransform);

    if (sourceMask) {
      RefPtr<DrawTarget> transformDT = dest->CreateSimilarDrawTarget(
          IntSize::Truncate(transformBounds.Width(), transformBounds.Height()),
          SurfaceFormat::B8G8R8A8);
      new3DTransform.PostTranslate(-transformBounds.X(), -transformBounds.Y(),
                                   0);
      if (transformDT &&
          transformDT->Draw3DTransformedSurface(destSnapshot, new3DTransform)) {
        RefPtr<SourceSurface> transformSnapshot = transformDT->Snapshot();

        // Transform the source by it's normal transform, and then the inverse
        // of the mask transform so that it's in the mask's untransformed
        // coordinate space.
        Matrix sourceTransform = newTransform;
        sourceTransform.PostTranslate(transformBounds.TopLeft());

        Matrix inverseMask = maskTransform;
        inverseMask.Invert();

        sourceTransform *= inverseMask;

        SurfacePattern source(transformSnapshot, ExtendMode::CLAMP,
                              sourceTransform);

        buffer->PushClipRect(transformBounds);

        // Mask in the untransformed coordinate space, and then transform
        // by the mask transform to put the result back into destination
        // coords.
        buffer->SetTransform(maskTransform);
        buffer->MaskSurface(source, sourceMask, Point(0, 0));

        buffer->PopClip();
      }
    } else {
      buffer->Draw3DTransformedSurface(destSnapshot, new3DTransform);
    }
  }

  buffer->PopClip();
}

void BasicCompositor::ClearRect(const gfx::Rect& aRect) {
  mRenderTarget->mDrawTarget->ClearRect(aRect);

  if (mFullWindowRenderTarget) {
    mFullWindowRenderTarget->mDrawTarget->ClearRect(aRect);
  }
}

bool BasicCompositor::ReadbackRenderTarget(CompositingRenderTarget* aSource,
                                           AsyncReadbackBuffer* aDest) {
  RefPtr<SourceSurface> snapshot =
      static_cast<BasicCompositingRenderTarget*>(aSource)
          ->mDrawTarget->Snapshot();
  static_cast<BasicAsyncReadbackBuffer*>(aDest)->TakeSurface(snapshot);
  return true;
}

already_AddRefed<AsyncReadbackBuffer>
BasicCompositor::CreateAsyncReadbackBuffer(const gfx::IntSize& aSize) {
  return MakeAndAddRef<BasicAsyncReadbackBuffer>(aSize);
}

bool BasicCompositor::BlitRenderTarget(CompositingRenderTarget* aSource,
                                       const gfx::IntSize& aSourceSize,
                                       const gfx::IntSize& aDestSize) {
  RefPtr<SourceSurface> surface =
      static_cast<BasicCompositingRenderTarget*>(aSource)
          ->mDrawTarget->Snapshot();
  mRenderTarget->mDrawTarget->DrawSurface(
      surface, Rect(Point(), Size(aDestSize)), Rect(Point(), Size(aSourceSize)),
      DrawSurfaceOptions(), DrawOptions(1.0f, CompositionOp::OP_SOURCE));
  return true;
}

Maybe<gfx::IntRect> BasicCompositor::BeginFrameForWindow(
    const nsIntRegion& aInvalidRegion, const Maybe<IntRect>& aClipRect,
    const IntRect& aRenderBounds, const nsIntRegion& aOpaqueRegion) {
  if (mIsPendingEndRemoteDrawing) {
    // Force to end previous remote drawing.
    EndRemoteDrawing();
    MOZ_ASSERT(!mIsPendingEndRemoteDrawing);
  }

  MOZ_RELEASE_ASSERT(mCurrentFrameDest == FrameDestination::NO_CURRENT_FRAME,
                     "mCurrentFrameDest not restored properly");

  IntRect rect(IntPoint(), mWidget->GetClientSize().ToUnknownSize());

  mShouldInvalidateWindow = NeedToRecreateFullWindowRenderTarget();

  if (mShouldInvalidateWindow) {
    mInvalidRegion = rect;
  } else {
    IntRegion invalidRegionSafe;
    // Sometimes the invalid region is larger than we want to draw.
    invalidRegionSafe.And(aInvalidRegion, rect);

    mInvalidRegion = invalidRegionSafe;
  }

  LayoutDeviceIntRegion invalidRegion =
      LayoutDeviceIntRegion::FromUnknownRegion(mInvalidRegion);
  BufferMode bufferMode = BufferMode::BUFFERED;
  // StartRemoteDrawingInRegion can mutate invalidRegion.
  RefPtr<DrawTarget> dt =
      mWidget->StartRemoteDrawingInRegion(invalidRegion, &bufferMode);
  if (!dt) {
    return Nothing();
  }
  if (invalidRegion.IsEmpty()) {
    mWidget->EndRemoteDrawingInRegion(dt, invalidRegion);
    return Nothing();
  }

  mInvalidRegion = invalidRegion.ToUnknownRegion();
  IntRegion clearRegion;
  clearRegion.Sub(mInvalidRegion, aOpaqueRegion);

  RefPtr<CompositingRenderTarget> target;
  if (bufferMode == BufferMode::BUFFERED) {
    // Buffer drawing via a back buffer.
    IntRect backBufferRect = mInvalidRegion.GetBounds();
    bool isCleared = false;
    RefPtr<DrawTarget> backBuffer =
        mWidget->GetBackBufferDrawTarget(dt, backBufferRect, &isCleared);
    if (!backBuffer) {
      mWidget->EndRemoteDrawingInRegion(dt, invalidRegion);
      return Nothing();
    }
    // Set up a render target for drawirg to the back buffer.
    target = CreateRootRenderTarget(backBuffer, backBufferRect,
                                    isCleared ? IntRegion() : clearRegion);
    mFrontBuffer = dt;
    // We will copy the drawing from the back buffer into mFrontBuffer (the
    // widget) in EndRemoteDrawing().
  } else {
    // In BufferMode::BUFFER_NONE, the DrawTarget returned by
    // StartRemoteDrawingInRegion can cover different rectangles in window
    // space. It can either cover the entire window, or it can cover just the
    // invalid region. We discern between the two cases by comparing the
    // DrawTarget's size with the invalild region's size.
    IntRect invalidRect = mInvalidRegion.GetBounds();
    IntPoint dtLocation = dt->GetSize() == invalidRect.Size()
                              ? invalidRect.TopLeft()
                              : IntPoint(0, 0);
    IntRect dtBounds(dtLocation, dt->GetSize());

    // Set up a render target for drawing directly to dt.
    target = CreateRootRenderTarget(dt, dtBounds, clearRegion);
  }

  mCurrentFrameDest = FrameDestination::WINDOW;

  MOZ_RELEASE_ASSERT(target);
  SetRenderTarget(target);

  gfxUtils::ClipToRegion(mRenderTarget->mDrawTarget, mInvalidRegion);

  mRenderTarget->mDrawTarget->PushClipRect(Rect(aClipRect.valueOr(rect)));

  return Some(rect);
}

Maybe<gfx::IntRect> BasicCompositor::BeginFrameForTarget(
    const nsIntRegion& aInvalidRegion, const Maybe<IntRect>& aClipRect,
    const IntRect& aRenderBounds, const nsIntRegion& aOpaqueRegion,
    DrawTarget* aTarget, const IntRect& aTargetBounds) {
  if (mIsPendingEndRemoteDrawing) {
    // Force to end previous remote drawing.
    EndRemoteDrawing();
    MOZ_ASSERT(!mIsPendingEndRemoteDrawing);
  }

  MOZ_RELEASE_ASSERT(mCurrentFrameDest == FrameDestination::NO_CURRENT_FRAME,
                     "mCurrentFrameDest not restored properly");

  mInvalidRegion.And(aInvalidRegion, aTargetBounds);
  MOZ_RELEASE_ASSERT(!mInvalidRegion.IsEmpty());

  IntRegion clearRegion;
  clearRegion.Sub(mInvalidRegion, aOpaqueRegion);

  // Set up a render target for drawing directly to aTarget.
  RefPtr<CompositingRenderTarget> target =
      CreateRootRenderTarget(aTarget, aTargetBounds, clearRegion);
  MOZ_RELEASE_ASSERT(target);
  SetRenderTarget(target);

  mCurrentFrameDest = FrameDestination::TARGET;

  gfxUtils::ClipToRegion(mRenderTarget->mDrawTarget, mInvalidRegion);

  mRenderTarget->mDrawTarget->PushClipRect(
      Rect(aClipRect.valueOr(aTargetBounds)));

  return Some(aTargetBounds);
}

void BasicCompositor::BeginFrameForNativeLayers() {
  if (mIsPendingEndRemoteDrawing) {
    // Force to end previous remote drawing.
    EndRemoteDrawing();
    MOZ_ASSERT(!mIsPendingEndRemoteDrawing);
  }

  MOZ_RELEASE_ASSERT(mCurrentFrameDest == FrameDestination::NO_CURRENT_FRAME,
                     "mCurrentFrameDest not restored properly");

  mShouldInvalidateWindow = NeedToRecreateFullWindowRenderTarget();

  // Make a 1x1 dummy render target so that GetCurrentRenderTarget() returns
  // something non-null even outside of calls to
  // Begin/EndRenderingToNativeLayer.
  if (!mNativeLayersReferenceRT) {
    RefPtr<DrawTarget> dt = Factory::CreateDrawTarget(
        gfxVars::ContentBackend(), IntSize(1, 1), SurfaceFormat::B8G8R8A8);
    mNativeLayersReferenceRT =
        new BasicCompositingRenderTarget(dt, IntRect(0, 0, 1, 1), IntPoint());
  }
  SetRenderTarget(mNativeLayersReferenceRT);

  mCurrentFrameDest = FrameDestination::NATIVE_LAYERS;
}

Maybe<gfx::IntRect> BasicCompositor::BeginRenderingToNativeLayer(
    const nsIntRegion& aInvalidRegion, const Maybe<gfx::IntRect>& aClipRect,
    const nsIntRegion& aOpaqueRegion, NativeLayer* aNativeLayer) {
  IntRect rect = aNativeLayer->GetRect();

  if (mShouldInvalidateWindow) {
    mInvalidRegion = rect;
  } else {
    mInvalidRegion.And(aInvalidRegion, rect);
  }

  if (mInvalidRegion.IsEmpty()) {
    return Nothing();
  }

  RefPtr<CompositingRenderTarget> target;
#ifdef XP_MACOSX
  NativeLayerCA* nativeLayer = aNativeLayer->AsNativeLayerCA();
  MOZ_RELEASE_ASSERT(nativeLayer, "Unexpected native layer type");
  nativeLayer->SetSurfaceIsFlipped(false);
  CFTypeRefPtr<IOSurfaceRef> surf = nativeLayer->NextSurface();
  if (!surf) {
    return Nothing();
  }
  IntRegion invalidRelativeToLayer = mInvalidRegion.MovedBy(-rect.TopLeft());
  nativeLayer->InvalidateRegionThroughoutSwapchain(invalidRelativeToLayer);
  invalidRelativeToLayer = nativeLayer->CurrentSurfaceInvalidRegion();
  mInvalidRegion = invalidRelativeToLayer.MovedBy(rect.TopLeft());
  MOZ_RELEASE_ASSERT(!mInvalidRegion.IsEmpty());
  mCurrentNativeLayer = aNativeLayer;
  mCurrentIOSurface = new MacIOSurface(std::move(surf));
  mCurrentIOSurface->Lock(false);
  RefPtr<DrawTarget> dt =
      mCurrentIOSurface->GetAsDrawTargetLocked(BackendType::SKIA);
  IntRegion clearRegion;
  clearRegion.Sub(mInvalidRegion, aOpaqueRegion);
  // Set up a render target for drawing directly to dt.
  target = CreateRootRenderTarget(dt, rect, clearRegion);
#else
  MOZ_CRASH("Unexpected native layer on this platform");
#endif

  MOZ_RELEASE_ASSERT(target);
  SetRenderTarget(target);

  gfxUtils::ClipToRegion(mRenderTarget->mDrawTarget, mInvalidRegion);

  mRenderTarget->mDrawTarget->PushClipRect(Rect(aClipRect.valueOr(rect)));

  return Some(rect);
}

void BasicCompositor::EndRenderingToNativeLayer() {
  // Pop aClipRect/bounds rect
  mRenderTarget->mDrawTarget->PopClip();

  // Pop mInvalidRegion
  mRenderTarget->mDrawTarget->PopClip();

  MOZ_RELEASE_ASSERT(mCurrentNativeLayer);

  SetRenderTarget(mNativeLayersReferenceRT);

#ifdef XP_MACOSX
  NativeLayerCA* nativeLayer = mCurrentNativeLayer->AsNativeLayerCA();
  MOZ_RELEASE_ASSERT(nativeLayer, "Unexpected native layer type");
  mCurrentIOSurface->Unlock(false);
  mCurrentIOSurface = nullptr;
  nativeLayer->NotifySurfaceReady();
  mCurrentNativeLayer = nullptr;
#else
  MOZ_CRASH("Unexpected native layer on this platform");
#endif
}

void BasicCompositor::EndFrame() {
  Compositor::EndFrame();

  if (mCurrentFrameDest != FrameDestination::NATIVE_LAYERS) {
    // Pop aClipRect/bounds rect
    mRenderTarget->mDrawTarget->PopClip();

    if (StaticPrefs::nglayout_debug_widget_update_flashing()) {
      float r = float(rand()) / float(RAND_MAX);
      float g = float(rand()) / float(RAND_MAX);
      float b = float(rand()) / float(RAND_MAX);
      // We're still clipped to mInvalidRegion, so just fill the bounds.
      mRenderTarget->mDrawTarget->FillRect(Rect(mInvalidRegion.GetBounds()),
                                           ColorPattern(Color(r, g, b, 0.2f)));
    }

    // Pop aInvalidRegion
    mRenderTarget->mDrawTarget->PopClip();
  }

  // Reset the translation that was applied in CreateRootRenderTarget.
  mRenderTarget->mDrawTarget->SetTransform(gfx::Matrix());

  switch (mCurrentFrameDest) {
    case FrameDestination::NO_CURRENT_FRAME:
      MOZ_CRASH("EndFrame being called without BeginFrameForXYZ?");
      break;
    case FrameDestination::WINDOW:
      TryToEndRemoteDrawing();
      break;
    case FrameDestination::TARGET:
    case FrameDestination::NATIVE_LAYERS:
      mRenderTarget = nullptr;
      break;
  }
  mCurrentFrameDest = FrameDestination::NO_CURRENT_FRAME;
  mShouldInvalidateWindow = false;
}

void BasicCompositor::TryToEndRemoteDrawing() {
  if (mIsDestroyed || !mRenderTarget) {
    return;
  }

  // If it is not a good time to call EndRemoteDrawing, defer it.
  if (NeedsToDeferEndRemoteDrawing()) {
    mIsPendingEndRemoteDrawing = true;

    const uint32_t retryMs = 2;
    RefPtr<BasicCompositor> self = this;
    RefPtr<Runnable> runnable =
        NS_NewRunnableFunction("layers::BasicCompositor::TryToEndRemoteDrawing",
                               [self]() { self->TryToEndRemoteDrawing(); });
    MessageLoop::current()->PostDelayedTask(runnable.forget(), retryMs);
  } else {
    EndRemoteDrawing();
  }
}

void BasicCompositor::EndRemoteDrawing() {
  if (mIsDestroyed || !mRenderTarget) {
    return;
  }

  if (mFrontBuffer) {
    // This is the case where we have a back buffer for BufferMode::BUFFERED
    // drawing.
    // mRenderTarget->mDrawTarget is the back buffer.
    // mFrontBuffer is always located at (0, 0) in window space.
    RefPtr<SourceSurface> source = mWidget->EndBackBufferDrawing();
    IntPoint srcOffset = mRenderTarget->GetOrigin();

    // The source DrawTarget is clipped to the invalidation region, so we have
    // to copy the individual rectangles in the region or else we'll draw
    // garbage pixels.
    // CopySurface ignores both the transform and the clip.
    for (auto iter = mInvalidRegion.RectIter(); !iter.Done(); iter.Next()) {
      const IntRect& r = iter.Get();
      mFrontBuffer->CopySurface(source, r - srcOffset, r.TopLeft());
    }

    mWidget->EndRemoteDrawingInRegion(
        mFrontBuffer, LayoutDeviceIntRegion::FromUnknownRegion(mInvalidRegion));

    mFrontBuffer = nullptr;
  } else {
    mWidget->EndRemoteDrawingInRegion(
        mRenderTarget->mDrawTarget,
        LayoutDeviceIntRegion::FromUnknownRegion(mInvalidRegion));
  }

  mRenderTarget = nullptr;
  mIsPendingEndRemoteDrawing = false;
}

void BasicCompositor::NormalDrawingDone() {
  // Now is a good time to update mFullWindowRenderTarget.

  if (!ShouldRecordFrames()) {
    // If we are no longer recording a profile, we can drop the render target if
    // it exists.
    mFullWindowRenderTarget = nullptr;
    return;
  }

  if (NeedToRecreateFullWindowRenderTarget()) {
    // We have either (1) just started recording and not yet allocated a
    // buffer or (2) are already recording and have resized the window. In
    // either case, we need a new render target.
    IntRect windowRect(IntPoint(0, 0),
                       mWidget->GetClientSize().ToUnknownSize());
    RefPtr<gfx::DrawTarget> drawTarget =
        mRenderTarget->mDrawTarget->CreateSimilarDrawTarget(
            windowRect.Size(), mRenderTarget->mDrawTarget->GetFormat());

    mFullWindowRenderTarget =
        new BasicCompositingRenderTarget(drawTarget, windowRect, IntPoint());
  }

  RefPtr<SourceSurface> source = mRenderTarget->mDrawTarget->Snapshot();
  IntPoint srcOffset = mRenderTarget->GetOrigin();
  for (auto iter = mInvalidRegion.RectIter(); !iter.Done(); iter.Next()) {
    const IntRect& r = iter.Get();
    mFullWindowRenderTarget->mDrawTarget->CopySurface(source, r - srcOffset,
                                                      r.TopLeft());
  }
  mFullWindowRenderTarget->mDrawTarget->Flush();
}

bool BasicCompositor::NeedsToDeferEndRemoteDrawing() {
  return mFrontBuffer && mWidget->NeedsToDeferEndRemoteDrawing();
}

void BasicCompositor::FinishPendingComposite() { EndRemoteDrawing(); }

bool BasicCompositor::NeedToRecreateFullWindowRenderTarget() const {
  if (!ShouldRecordFrames()) {
    return false;
  }
  if (!mFullWindowRenderTarget) {
    return true;
  }
  IntSize windowSize = mWidget->GetClientSize().ToUnknownSize();
  return mFullWindowRenderTarget->mDrawTarget->GetSize() != windowSize;
}

}  // namespace layers
}  // namespace mozilla
