/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BasicCompositor.h"
#include "BasicLayersImpl.h"            // for FillRectWithMask
#include "TextureHostBasic.h"
#include "mozilla/layers/Effects.h"
#include "nsIWidget.h"
#include "gfx2DGlue.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/Helpers.h"
#include "mozilla/gfx/Tools.h"
#include "mozilla/gfx/ssse3-scaler.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/SSE.h"
#include "gfxUtils.h"
#include "YCbCrUtils.h"
#include <algorithm>
#include "ImageContainer.h"
#include "gfxPrefs.h"

namespace mozilla {
using namespace mozilla::gfx;

namespace layers {

class DataTextureSourceBasic : public DataTextureSource
                             , public TextureSourceBasic
{
public:
  virtual const char* Name() const override { return "DataTextureSourceBasic"; }

  explicit DataTextureSourceBasic(DataSourceSurface* aSurface)
  : mSurface(aSurface)
  , mWrappingExistingData(!!aSurface)
  {}

  virtual DataTextureSource* AsDataTextureSource() override
  {
    // If the texture wraps someone else's memory we'd rather not use it as
    // a DataTextureSource per say (that is call Update on it).
    return mWrappingExistingData ? nullptr : this;
  }

  virtual TextureSourceBasic* AsSourceBasic() override { return this; }

  virtual gfx::SourceSurface* GetSurface(DrawTarget* aTarget) override { return mSurface; }

  SurfaceFormat GetFormat() const override
  {
    return mSurface ? mSurface->GetFormat() : gfx::SurfaceFormat::UNKNOWN;
  }

  virtual IntSize GetSize() const override
  {
    return mSurface ? mSurface->GetSize() : gfx::IntSize(0, 0);
  }

  virtual bool Update(gfx::DataSourceSurface* aSurface,
                      nsIntRegion* aDestRegion = nullptr,
                      gfx::IntPoint* aSrcOffset = nullptr) override
  {
    MOZ_ASSERT(!mWrappingExistingData);
    if (mWrappingExistingData) {
      return false;
    }
    mSurface = aSurface;
    return true;
  }

  virtual void DeallocateDeviceData() override
  {
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
class WrappingTextureSourceYCbCrBasic : public DataTextureSource
                                      , public TextureSourceBasic
{
public:
  virtual const char* Name() const override { return "WrappingTextureSourceYCbCrBasic"; }

  explicit WrappingTextureSourceYCbCrBasic(BufferTextureHost* aTexture)
  : mTexture(aTexture)
  , mSize(aTexture->GetSize())
  , mNeedsUpdate(true)
  {
    mFromYCBCR = true;
  }

  virtual DataTextureSource* AsDataTextureSource() override
  {
    return this;
  }

  virtual TextureSourceBasic* AsSourceBasic() override { return this; }

  virtual WrappingTextureSourceYCbCrBasic* AsWrappingTextureSourceYCbCrBasic() override { return this; }

  virtual gfx::SourceSurface* GetSurface(DrawTarget* aTarget) override
  {
    if (mSurface && !mNeedsUpdate) {
      return mSurface;
    }
    MOZ_ASSERT(mTexture);
    if (!mTexture) {
      return nullptr;
    }

    if (!mSurface) {
      mSurface = Factory::CreateDataSourceSurface(mSize, gfx::SurfaceFormat::B8G8R8X8);
    }
    if (!mSurface) {
      return nullptr;
    }
    MOZ_ASSERT(mTexture->GetBufferDescriptor().type() == BufferDescriptor::TYCbCrDescriptor);
    MOZ_ASSERT(mTexture->GetSize() == mSize);

    mSurface =
      ImageDataSerializer::DataSourceSurfaceFromYCbCrDescriptor(
        mTexture->GetBuffer(),
        mTexture->GetBufferDescriptor().get_YCbCrDescriptor(),
        mSurface);
    mNeedsUpdate = false;
    return mSurface;
  }

  SurfaceFormat GetFormat() const override
  {
    return gfx::SurfaceFormat::B8G8R8X8;
  }

  virtual IntSize GetSize() const override
  {
    return mSize;
  }

  virtual bool Update(gfx::DataSourceSurface* aSurface,
                      nsIntRegion* aDestRegion = nullptr,
                      gfx::IntPoint* aSrcOffset = nullptr) override
  {
    return false;
  }

  virtual void DeallocateDeviceData() override
  {
    mTexture = nullptr;
    mSurface = nullptr;
    SetUpdateSerial(0);
  }

  virtual void Unbind() override
  {
    mNeedsUpdate = true;
  }

  void SetBufferTextureHost(BufferTextureHost* aTexture) override
  {
    mTexture = aTexture;
    mNeedsUpdate = true;
  }

  void ConvertAndScale(const SurfaceFormat& aDestFormat,
                       const IntSize& aDestSize,
                       unsigned char* aDestBuffer,
                       int32_t aStride)
  {
    MOZ_ASSERT(mTexture);
    if (!mTexture) {
      return;
    }
    MOZ_ASSERT(mTexture->GetBufferDescriptor().type() == BufferDescriptor::TYCbCrDescriptor);
    MOZ_ASSERT(mTexture->GetSize() == mSize);
    ImageDataSerializer::ConvertAndScaleFromYCbCrDescriptor(
      mTexture->GetBuffer(),
      mTexture->GetBufferDescriptor().get_YCbCrDescriptor(),
      aDestFormat,
      aDestSize,
      aDestBuffer,
      aStride);
  }
public:
  BufferTextureHost* mTexture;
  const gfx::IntSize mSize;
  RefPtr<gfx::DataSourceSurface> mSurface;
  bool mNeedsUpdate;
};

BasicCompositor::BasicCompositor(CompositorBridgeParent* aParent, widget::CompositorWidget* aWidget)
  : Compositor(aWidget, aParent)
  , mIsPendingEndRemoteDrawing(false)
{
  MOZ_COUNT_CTOR(BasicCompositor);

  mMaxTextureSize = Factory::GetMaxSurfaceSize(gfxVars::ContentBackend());
}

BasicCompositor::~BasicCompositor()
{
  MOZ_COUNT_DTOR(BasicCompositor);
}

bool
BasicCompositor::Initialize(nsCString* const out_failureReason)
{
  return mWidget ? mWidget->InitCompositor(this) : false;
};

int32_t
BasicCompositor::GetMaxTextureSize() const
{
  return mMaxTextureSize;
}

void
BasicCompositingRenderTarget::BindRenderTarget()
{
  if (mClearOnBind) {
    mDrawTarget->ClearRect(Rect(0, 0, mSize.width, mSize.height));
    mClearOnBind = false;
  }
}

void BasicCompositor::DetachWidget()
{
  if (mWidget) {
    if (mIsPendingEndRemoteDrawing) {
      // Force to end previous remote drawing.
      TryToEndRemoteDrawing(/* aForceToEnd */ true);
      MOZ_ASSERT(!mIsPendingEndRemoteDrawing);
    }
    mWidget->CleanupRemoteDrawing();
  }
  Compositor::DetachWidget();
}

TextureFactoryIdentifier
BasicCompositor::GetTextureFactoryIdentifier()
{
  TextureFactoryIdentifier ident(LayersBackend::LAYERS_BASIC,
                                 XRE_GetProcessType(),
                                 GetMaxTextureSize());
  return ident;
}

already_AddRefed<CompositingRenderTarget>
BasicCompositor::CreateRenderTarget(const IntRect& aRect, SurfaceInitMode aInit)
{
  MOZ_ASSERT(!aRect.IsZeroArea(), "Trying to create a render target of invalid size");

  if (aRect.IsZeroArea()) {
    return nullptr;
  }

  RefPtr<DrawTarget> target = mDrawTarget->CreateSimilarDrawTarget(aRect.Size(), SurfaceFormat::B8G8R8A8);

  if (!target) {
    return nullptr;
  }

  RefPtr<BasicCompositingRenderTarget> rt = new BasicCompositingRenderTarget(target, aRect);

  return rt.forget();
}

already_AddRefed<CompositingRenderTarget>
BasicCompositor::CreateRenderTargetFromSource(const IntRect &aRect,
                                              const CompositingRenderTarget *aSource,
                                              const IntPoint &aSourcePoint)
{
  MOZ_CRASH("GFX: Shouldn't be called!");
  return nullptr;
}

already_AddRefed<CompositingRenderTarget>
BasicCompositor::CreateRenderTargetForWindow(const LayoutDeviceIntRect& aRect, const LayoutDeviceIntRect& aClearRect, BufferMode aBufferMode)
{
  MOZ_ASSERT(mDrawTarget);
  MOZ_ASSERT(!aRect.IsZeroArea(), "Trying to create a render target of invalid size");

  if (aRect.IsZeroArea()) {
    return nullptr;
  }

  RefPtr<BasicCompositingRenderTarget> rt;
  IntRect rect = aRect.ToUnknownRect();

  if (aBufferMode != BufferMode::BUFFER_NONE) {
    RefPtr<DrawTarget> target = mWidget->GetBackBufferDrawTarget(mDrawTarget, aRect, aClearRect);
    if (!target) {
      return nullptr;
    }
    MOZ_ASSERT(target != mDrawTarget);
    rt = new BasicCompositingRenderTarget(target, rect);
  } else {
    IntRect windowRect = rect;
    // Adjust bounds rect to account for new origin at (0, 0).
    if (windowRect.Size() != mDrawTarget->GetSize()) {
      windowRect.ExpandToEnclose(IntPoint(0, 0));
    }
    rt = new BasicCompositingRenderTarget(mDrawTarget, windowRect);
    if (!aClearRect.IsEmpty()) {
      IntRect clearRect = aRect.ToUnknownRect();
      mDrawTarget->ClearRect(Rect(clearRect - rt->GetOrigin()));
    }
  }

  return rt.forget();
}

already_AddRefed<DataTextureSource>
BasicCompositor::CreateDataTextureSource(TextureFlags aFlags)
{
  RefPtr<DataTextureSourceBasic> result = new DataTextureSourceBasic(nullptr);
  if (aFlags & TextureFlags::RGB_FROM_YCBCR) {
      result->mFromYCBCR = true;
  }
  return result.forget();
}

already_AddRefed<DataTextureSource>
BasicCompositor::CreateDataTextureSourceAround(DataSourceSurface* aSurface)
{
  RefPtr<DataTextureSource> result = new DataTextureSourceBasic(aSurface);
  return result.forget();
}

already_AddRefed<DataTextureSource>
BasicCompositor::CreateDataTextureSourceAroundYCbCr(TextureHost* aTexture)
{
  BufferTextureHost* bufferTexture = aTexture->AsBufferTextureHost();
  MOZ_ASSERT(bufferTexture);

  if (!bufferTexture) {
    return nullptr;
  }
  RefPtr<DataTextureSource> result = new WrappingTextureSourceYCbCrBasic(bufferTexture);
  return result.forget();
}

bool
BasicCompositor::SupportsEffect(EffectTypes aEffect)
{
  return aEffect != EffectTypes::YCBCR && aEffect != EffectTypes::COMPONENT_ALPHA;
}

bool
BasicCompositor::SupportsLayerGeometry() const
{
  return gfxPrefs::BasicLayerGeometry();
}

static RefPtr<gfx::Path>
BuildPathFromPolygon(const RefPtr<DrawTarget>& aDT,
                     const gfx::Polygon& aPolygon)
{
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

static void
DrawSurface(gfx::DrawTarget* aDest,
            const gfx::Rect& aDestRect,
            const gfx::Rect& /* aClipRect */,
            const gfx::Color& aColor,
            const gfx::DrawOptions& aOptions,
            gfx::SourceSurface* aMask,
            const gfx::Matrix* aMaskTransform)
{
  FillRectWithMask(aDest, aDestRect, aColor,
                   aOptions, aMask, aMaskTransform);
}

static void
DrawSurface(gfx::DrawTarget* aDest,
            const gfx::Polygon& aPolygon,
            const gfx::Rect& aClipRect,
            const gfx::Color& aColor,
            const gfx::DrawOptions& aOptions,
            gfx::SourceSurface* aMask,
            const gfx::Matrix* aMaskTransform)
{
  RefPtr<Path> path = BuildPathFromPolygon(aDest, aPolygon);
  FillPathWithMask(aDest, path, aClipRect, aColor,
                   aOptions, aMask, aMaskTransform);
}

static void
DrawTextureSurface(gfx::DrawTarget* aDest,
                   const gfx::Rect& aDestRect,
                   const gfx::Rect& /* aClipRect */,
                   gfx::SourceSurface* aSource,
                   gfx::SamplingFilter aSamplingFilter,
                   const gfx::DrawOptions& aOptions,
                   ExtendMode aExtendMode,
                   gfx::SourceSurface* aMask,
                   const gfx::Matrix* aMaskTransform,
                   const Matrix* aSurfaceTransform)
{
  FillRectWithMask(aDest, aDestRect, aSource, aSamplingFilter, aOptions,
                   aExtendMode, aMask, aMaskTransform, aSurfaceTransform);
}

static void
DrawTextureSurface(gfx::DrawTarget* aDest,
                   const gfx::Polygon& aPolygon,
                   const gfx::Rect& aClipRect,
                   gfx::SourceSurface* aSource,
                   gfx::SamplingFilter aSamplingFilter,
                   const gfx::DrawOptions& aOptions,
                   ExtendMode aExtendMode,
                   gfx::SourceSurface* aMask,
                   const gfx::Matrix* aMaskTransform,
                   const Matrix* aSurfaceTransform)
{
  RefPtr<Path> path = BuildPathFromPolygon(aDest, aPolygon);
  FillPathWithMask(aDest, path, aClipRect, aSource, aSamplingFilter, aOptions,
                   aExtendMode, aMask, aMaskTransform, aSurfaceTransform);
}

template<typename Geometry>
static void
DrawSurfaceWithTextureCoords(gfx::DrawTarget* aDest,
                             const Geometry& aGeometry,
                             const gfx::Rect& aDestRect,
                             gfx::SourceSurface* aSource,
                             const gfx::Rect& aTextureCoords,
                             gfx::SamplingFilter aSamplingFilter,
                             const gfx::DrawOptions& aOptions,
                             gfx::SourceSurface* aMask,
                             const gfx::Matrix* aMaskTransform)
{
  if (!aSource) {
    gfxWarning() << "DrawSurfaceWithTextureCoords problem " << gfx::hexa(aSource) << " and " << gfx::hexa(aMask);
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
  Matrix matrix =
    gfxUtils::TransformRectToRect(sourceRect,
                                  gfx::IntPoint::Truncate(aDestRect.X(), aDestRect.Y()),
                                  gfx::IntPoint::Truncate(aDestRect.XMost(), aDestRect.Y()),
                                  gfx::IntPoint::Truncate(aDestRect.XMost(), aDestRect.YMost()));

  // Only use REPEAT if aTextureCoords is outside (0, 0, 1, 1).
  gfx::Rect unitRect(0, 0, 1, 1);
  ExtendMode mode = unitRect.Contains(aTextureCoords) ? ExtendMode::CLAMP : ExtendMode::REPEAT;

  DrawTextureSurface(aDest, aGeometry, aDestRect, aSource, aSamplingFilter,
                     aOptions, mode, aMask, aMaskTransform, &matrix);
}

static void
SetupMask(const EffectChain& aEffectChain,
          DrawTarget* aDest,
          const IntPoint& aOffset,
          RefPtr<SourceSurface>& aMaskSurface,
          Matrix& aMaskTransform)
{
  if (aEffectChain.mSecondaryEffects[EffectTypes::MASK]) {
    EffectMask *effectMask = static_cast<EffectMask*>(aEffectChain.mSecondaryEffects[EffectTypes::MASK].get());
    aMaskSurface = effectMask->mMaskTexture->AsSourceBasic()->GetSurface(aDest);
    if (!aMaskSurface) {
      gfxWarning() << "Invalid sourceMask effect";
    }
    MOZ_ASSERT(effectMask->mMaskTransform.Is2D(), "How did we end up with a 3D transform here?!");
    aMaskTransform = effectMask->mMaskTransform.As2D();
    aMaskTransform.PostTranslate(-aOffset.x, -aOffset.y);
  }
}

static bool
AttemptVideoScale(TextureSourceBasic* aSource, const SourceSurface* aSourceMask,
                       gfx::Float aOpacity, CompositionOp aBlendMode,
                       const TexturedEffect* aTexturedEffect,
                       const Matrix& aNewTransform, const gfx::Rect& aRect,
                       const gfx::Rect& aClipRect,
                       DrawTarget* aDest, const DrawTarget* aBuffer)
{
#ifdef MOZILLA_SSE_HAVE_CPUID_DETECTION
  if (!mozilla::supports_ssse3())
      return false;
  if (aNewTransform.IsTranslation()) // unscaled painting should take the regular path
      return false;
  if (aNewTransform.HasNonAxisAlignedTransform() || aNewTransform.HasNegativeScaling())
      return false;
  if (aSourceMask || aOpacity != 1.0f)
      return false;
  if (aBlendMode != CompositionOp::OP_OVER && aBlendMode != CompositionOp::OP_SOURCE)
      return false;

  IntRect dstRect;
  // the compiler should know a lot about aNewTransform at this point
  // maybe it can do some sophisticated optimization of the following
  if (!aNewTransform.TransformBounds(aRect).ToIntRect(&dstRect))
      return false;

  IntRect clipRect;
  if (!aClipRect.ToIntRect(&clipRect))
      return false;

  if (!(aTexturedEffect->mTextureCoords == Rect(0.0f, 0.0f, 1.0f, 1.0f)))
      return false;
  if (aDest->GetFormat() == SurfaceFormat::R5G6B5_UINT16)
      return false;

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

    RefPtr<DataSourceSurface> srcSource = aSource->GetSurface(aDest)->GetDataSurface();
    DataSourceSurface::ScopedMap mapSrc(srcSource, DataSourceSurface::READ);

    ssse3_scale_data((uint32_t*)mapSrc.GetData(), srcSource->GetSize().width, srcSource->GetSize().height,
                     mapSrc.GetStride()/4,
                     ((uint32_t*)dstData) + fillRect.X() + (dstStride / 4) * fillRect.Y(), dstRect.Width(), dstRect.Height(),
                     dstStride / 4,
                     offset.x, offset.y,
                     fillRect.Width(), fillRect.Height());

    aDest->ReleaseBits(dstData);
    return true;
  } else
#endif // MOZILLA_SSE_HAVE_CPUID_DETECTION
    return false;
}

static bool
AttemptVideoConvertAndScale(TextureSource* aSource, const SourceSurface* aSourceMask,
                            gfx::Float aOpacity, CompositionOp aBlendMode,
                            const TexturedEffect* aTexturedEffect,
                            const Matrix& aNewTransform, const gfx::Rect& aRect,
                            const gfx::Rect& aClipRect,
                            DrawTarget* aDest, const DrawTarget* aBuffer)
{
#if defined(XP_WIN) && defined(_M_X64)
  // libyuv does not support SIMD scaling on win 64bit. See Bug 1295927.
  return false;
#endif

  WrappingTextureSourceYCbCrBasic* wrappingSource = aSource->AsWrappingTextureSourceYCbCrBasic();
  if (!wrappingSource)
    return false;
#ifdef MOZILLA_SSE_HAVE_CPUID_DETECTION
  if (!mozilla::supports_ssse3()) // libyuv requests SSSE3 for fast YUV conversion.
    return false;
  if (aNewTransform.HasNonAxisAlignedTransform() || aNewTransform.HasNegativeScaling())
      return false;
  if (aSourceMask || aOpacity != 1.0f)
    return false;
  if (aBlendMode != CompositionOp::OP_OVER && aBlendMode != CompositionOp::OP_SOURCE)
    return false;

  IntRect dstRect;
  // the compiler should know a lot about aNewTransform at this point
  // maybe it can do some sophisticated optimization of the following
  if (!aNewTransform.TransformBounds(aRect).ToIntRect(&dstRect))
    return false;

  IntRect clipRect;
  if (!aClipRect.ToIntRect(&clipRect))
    return false;

  if (!(aTexturedEffect->mTextureCoords == Rect(0.0f, 0.0f, 1.0f, 1.0f)))
    return false;
  if (aDest->GetFormat() == SurfaceFormat::R5G6B5_UINT16)
    return false;

  if (aDest == aBuffer && !clipRect.Contains(dstRect))
    return false;
  if (!IntRect(IntPoint(0, 0), aDest->GetSize()).Contains(dstRect))
    return false;

  uint8_t* dstData;
  IntSize dstSize;
  int32_t dstStride;
  SurfaceFormat dstFormat;
  if (aDest->LockBits(&dstData, &dstSize, &dstStride, &dstFormat)) {
    wrappingSource->ConvertAndScale(dstFormat,
                                    dstRect.Size(),
                                    dstData + ptrdiff_t(dstRect.X()) * BytesPerPixel(dstFormat) + ptrdiff_t(dstRect.Y()) * dstStride,
                                    dstStride);
    aDest->ReleaseBits(dstData);
    return true;
  } else
#endif // MOZILLA_SSE_HAVE_CPUID_DETECTION
    return false;
}

void
BasicCompositor::DrawQuad(const gfx::Rect& aRect,
                          const gfx::IntRect& aClipRect,
                          const EffectChain &aEffectChain,
                          gfx::Float aOpacity,
                          const gfx::Matrix4x4& aTransform,
                          const gfx::Rect& aVisibleRect)
{
  DrawGeometry(aRect, aRect, aClipRect, aEffectChain,
               aOpacity, aTransform, aVisibleRect, true);
}

void
BasicCompositor::DrawPolygon(const gfx::Polygon& aPolygon,
                             const gfx::Rect& aRect,
                             const gfx::IntRect& aClipRect,
                             const EffectChain& aEffectChain,
                             gfx::Float aOpacity,
                             const gfx::Matrix4x4& aTransform,
                             const gfx::Rect& aVisibleRect)
{
  DrawGeometry(aPolygon, aRect, aClipRect, aEffectChain,
               aOpacity, aTransform, aVisibleRect, false);
}


template<typename Geometry>
void
BasicCompositor::DrawGeometry(const Geometry& aGeometry,
                              const gfx::Rect& aRect,
                              const gfx::IntRect& aClipRect,
                              const EffectChain& aEffectChain,
                              gfx::Float aOpacity,
                              const gfx::Matrix4x4& aTransform,
                              const gfx::Rect& aVisibleRect,
                              const bool aEnableAA)
{
  RefPtr<DrawTarget> buffer = mRenderTarget->mDrawTarget;

  // For 2D drawing, |dest| and |buffer| are the same surface. For 3D drawing,
  // |dest| is a temporary surface.
  RefPtr<DrawTarget> dest = buffer;

  AutoRestoreTransform autoRestoreTransform(dest);

  Matrix newTransform;
  Rect transformBounds;
  Matrix4x4 new3DTransform;
  IntPoint offset = mRenderTarget->GetOrigin();

  if (aTransform.Is2D()) {
    newTransform = aTransform.As2D();
  } else {
    // Create a temporary surface for the transform.
    dest = Factory::CreateDrawTarget(gfxVars::ContentBackend(), RoundedOut(aRect).Size(), SurfaceFormat::B8G8R8A8);
    if (!dest) {
      return;
    }

    dest->SetTransform(Matrix::Translation(-aRect.X(), -aRect.Y()));

    // Get the bounds post-transform.
    transformBounds = aTransform.TransformAndClipBounds(aRect, Rect(offset.x, offset.y, buffer->GetSize().width, buffer->GetSize().height));
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

  // XXX the transform is probably just an integer offset so this whole
  // business here is a bit silly.
  Rect transformedClipRect = buffer->GetTransform().TransformBounds(Rect(aClipRect));

  buffer->PushClipRect(Rect(aClipRect));

  newTransform.PostTranslate(-offset.x, -offset.y);
  buffer->SetTransform(newTransform);

  RefPtr<SourceSurface> sourceMask;
  Matrix maskTransform;
  if (aTransform.Is2D()) {
    SetupMask(aEffectChain, dest, offset, sourceMask, maskTransform);
  }

  CompositionOp blendMode = CompositionOp::OP_OVER;
  if (Effect* effect = aEffectChain.mSecondaryEffects[EffectTypes::BLEND_MODE].get()) {
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

      DrawSurface(dest, aGeometry, aRect, effectSolidColor->mColor,
                  drawOptions, sourceMask, &maskTransform);

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
            AttemptVideoConvertAndScale(texturedEffect->mTexture, sourceMask, aOpacity, blendMode,
                                        texturedEffect,
                                        newTransform, aRect, transformedClipRect,
                                        dest, buffer)) {
          // we succeeded in convert and scaling
        } else if (source->mFromYCBCR &&
                   !source->GetSurface(dest)) {
          gfxWarning() << "Failed to get YCbCr to rgb surface.";
        } else if (source->mFromYCBCR &&
            AttemptVideoScale(source, sourceMask, aOpacity, blendMode,
                              texturedEffect,
                              newTransform, aRect, transformedClipRect,
                              dest, buffer)) {
          // we succeeded in scaling
        } else {
          DrawSurfaceWithTextureCoords(dest, aGeometry, aRect,
                                       source->GetSurface(dest),
                                       texturedEffect->mTextureCoords,
                                       texturedEffect->mSamplingFilter,
                                       drawOptions,
                                       sourceMask, &maskTransform);
        }
      } else if (source) {
        SourceSurface* srcSurf = source->GetSurface(dest);
        if (srcSurf) {
          RefPtr<DataSourceSurface> srcData = srcSurf->GetDataSurface();

          // Yes, we re-create the premultiplied data every time.
          // This might be better with a cache, eventually.
          RefPtr<DataSourceSurface> premultData = gfxUtils::CreatePremultipliedDataSurface(srcData);

          DrawSurfaceWithTextureCoords(dest, aGeometry, aRect,
                                       premultData,
                                       texturedEffect->mTextureCoords,
                                       texturedEffect->mSamplingFilter,
                                       drawOptions,
                                       sourceMask, &maskTransform);
        }
      } else {
        gfxDevCrash(LogReason::IncompatibleBasicTexturedEffect) << "Bad for basic with " << texturedEffect->mTexture->Name() << " and " << gfx::hexa(sourceMask);
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
      RefPtr<BasicCompositingRenderTarget> surface
        = static_cast<BasicCompositingRenderTarget*>(effectRenderTarget->mRenderTarget.get());
      RefPtr<SourceSurface> sourceSurf = surface->mDrawTarget->Snapshot();

      DrawSurfaceWithTextureCoords(dest, aGeometry, aRect,
                                   sourceSurf,
                                   effectRenderTarget->mTextureCoords,
                                   effectRenderTarget->mSamplingFilter,
                                   drawOptions,
                                   sourceMask, &maskTransform);
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
      RefPtr<DrawTarget> transformDT =
        dest->CreateSimilarDrawTarget(IntSize::Truncate(transformBounds.Width(), transformBounds.Height()),
                                      SurfaceFormat::B8G8R8A8);
      new3DTransform.PostTranslate(-transformBounds.X(), -transformBounds.Y(), 0);
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

        SurfacePattern source(transformSnapshot, ExtendMode::CLAMP, sourceTransform);

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

void
BasicCompositor::ClearRect(const gfx::Rect& aRect)
{
  mRenderTarget->mDrawTarget->ClearRect(aRect);
}

void
BasicCompositor::BeginFrame(const nsIntRegion& aInvalidRegion,
                            const gfx::IntRect *aClipRectIn,
                            const gfx::IntRect& aRenderBounds,
                            const nsIntRegion& aOpaqueRegion,
                            gfx::IntRect *aClipRectOut /* = nullptr */,
                            gfx::IntRect *aRenderBoundsOut /* = nullptr */)
{
  if (mIsPendingEndRemoteDrawing) {
    // Force to end previous remote drawing.
    TryToEndRemoteDrawing(/* aForceToEnd */ true);
    MOZ_ASSERT(!mIsPendingEndRemoteDrawing);
  }

  LayoutDeviceIntRect intRect(LayoutDeviceIntPoint(), mWidget->GetClientSize());
  IntRect rect = IntRect(0, 0, intRect.Width(), intRect.Height());

  LayoutDeviceIntRegion invalidRegionSafe;
  // Sometimes the invalid region is larger than we want to draw.
  invalidRegionSafe.And(
      LayoutDeviceIntRegion::FromUnknownRegion(aInvalidRegion), intRect);

  mInvalidRegion = invalidRegionSafe;
  mInvalidRect = mInvalidRegion.GetBounds();

  if (aRenderBoundsOut) {
    *aRenderBoundsOut = IntRect();
  }

  BufferMode bufferMode = BufferMode::BUFFERED;
  if (mTarget) {
    // If we have a copy target, then we don't have a widget-provided mDrawTarget (currently). Use a dummy
    // placeholder so that CreateRenderTarget() works. This is only used to create a new buffered
    // draw target that we composite into, then copy the results the destination.
    mDrawTarget = mTarget;
    bufferMode = BufferMode::BUFFER_NONE;
  } else {
    // StartRemoteDrawingInRegion can mutate mInvalidRegion.
    mDrawTarget = mWidget->StartRemoteDrawingInRegion(mInvalidRegion, &bufferMode);
    if (!mDrawTarget) {
      return;
    }
    mInvalidRect = mInvalidRegion.GetBounds();
    if (mInvalidRect.IsEmpty()) {
      mWidget->EndRemoteDrawingInRegion(mDrawTarget, mInvalidRegion);
      return;
    }
  }

  if (!mDrawTarget || mInvalidRect.IsEmpty()) {
    return;
  }

  LayoutDeviceIntRect clearRect;
  if (!aOpaqueRegion.IsEmpty()) {
    LayoutDeviceIntRegion clearRegion = mInvalidRegion;
    clearRegion.SubOut(LayoutDeviceIntRegion::FromUnknownRegion(aOpaqueRegion));
    clearRect = clearRegion.GetBounds();
  } else {
    clearRect = mInvalidRect;
  }

  // Prevent CreateRenderTargetForWindow from clearing unwanted area.
  gfxUtils::ClipToRegion(mDrawTarget,
                         mInvalidRegion.ToUnknownRegion());

  // Setup an intermediate render target to buffer all compositing. We will
  // copy this into mDrawTarget (the widget), and/or mTarget in EndFrame()
  RefPtr<CompositingRenderTarget> target =
    CreateRenderTargetForWindow(mInvalidRect,
                                clearRect,
                                bufferMode);

  mDrawTarget->PopClip();

  if (!target) {
    if (!mTarget) {
      mWidget->EndRemoteDrawingInRegion(mDrawTarget, mInvalidRegion);
    }
    return;
  }
  SetRenderTarget(target);

  // We only allocate a surface sized to the invalidated region, so we need to
  // translate future coordinates.
  mRenderTarget->mDrawTarget->SetTransform(Matrix::Translation(-mRenderTarget->GetOrigin()));

  gfxUtils::ClipToRegion(mRenderTarget->mDrawTarget,
                         mInvalidRegion.ToUnknownRegion());

  if (aRenderBoundsOut) {
    *aRenderBoundsOut = rect;
  }

  if (aClipRectIn) {
    mRenderTarget->mDrawTarget->PushClipRect(Rect(*aClipRectIn));
  } else {
    mRenderTarget->mDrawTarget->PushClipRect(Rect(rect));
    if (aClipRectOut) {
      *aClipRectOut = rect;
    }
  }
}

void
BasicCompositor::EndFrame()
{
  Compositor::EndFrame();

  // Pop aClipRectIn/bounds rect
  mRenderTarget->mDrawTarget->PopClip();

  if (gfxPrefs::WidgetUpdateFlashing()) {
    float r = float(rand()) / RAND_MAX;
    float g = float(rand()) / RAND_MAX;
    float b = float(rand()) / RAND_MAX;
    // We're still clipped to mInvalidRegion, so just fill the bounds.
    mRenderTarget->mDrawTarget->FillRect(
      IntRectToRect(mInvalidRegion.GetBounds()).ToUnknownRect(),
      ColorPattern(Color(r, g, b, 0.2f)));
  }

  // Pop aInvalidregion
  mRenderTarget->mDrawTarget->PopClip();

  TryToEndRemoteDrawing();
}

void
BasicCompositor::TryToEndRemoteDrawing(bool aForceToEnd)
{
  if (mIsDestroyed || !mRenderTarget) {
    return;
  }

  // It it is not a good timing for EndRemoteDrawing, defter to call it.
  if (!aForceToEnd && !mTarget && NeedsToDeferEndRemoteDrawing()) {
    mIsPendingEndRemoteDrawing = true;

    const uint32_t retryMs = 2;
    RefPtr<BasicCompositor> self = this;
    RefPtr<Runnable> runnable =
      NS_NewRunnableFunction("layers::BasicCompositor::TryToEndRemoteDrawing",
                             [self]() { self->TryToEndRemoteDrawing(); });
    MessageLoop::current()->PostDelayedTask(runnable.forget(), retryMs);
    return;
  }

  if (mRenderTarget->mDrawTarget != mDrawTarget) {
    // Note: Most platforms require us to buffer drawing to the widget surface.
    // That's why we don't draw to mDrawTarget directly.
    RefPtr<SourceSurface> source;
    if (mRenderTarget->mDrawTarget != mDrawTarget) {
      source = mWidget->EndBackBufferDrawing();
    } else {
      source = mRenderTarget->mDrawTarget->Snapshot();
    }
    RefPtr<DrawTarget> dest(mTarget ? mTarget : mDrawTarget);

    nsIntPoint offset = mTarget ? mTargetBounds.TopLeft() : nsIntPoint();

    // The source DrawTarget is clipped to the invalidation region, so we have
    // to copy the individual rectangles in the region or else we'll draw blank
    // pixels.
    for (auto iter = mInvalidRegion.RectIter(); !iter.Done(); iter.Next()) {
      const LayoutDeviceIntRect& r = iter.Get();
      dest->CopySurface(source,
                        IntRect(r.X(), r.Y(), r.Width(), r.Height()) - mRenderTarget->GetOrigin(),
                        IntPoint(r.X(), r.Y()) - offset);
    }
  }

  if (aForceToEnd || !mTarget) {
    mWidget->EndRemoteDrawingInRegion(mDrawTarget, mInvalidRegion);
  }

  mDrawTarget = nullptr;
  mRenderTarget = nullptr;
  mIsPendingEndRemoteDrawing = false;
}

bool
BasicCompositor::NeedsToDeferEndRemoteDrawing()
{
  MOZ_ASSERT(mDrawTarget);
  MOZ_ASSERT(mRenderTarget);

  if (mTarget || mRenderTarget->mDrawTarget == mDrawTarget) {
    return false;
  }

  return mWidget->NeedsToDeferEndRemoteDrawing();
}

void
BasicCompositor::FinishPendingComposite()
{
  TryToEndRemoteDrawing(/* aForceToEnd */ true);
}

} // namespace layers
} // namespace mozilla
