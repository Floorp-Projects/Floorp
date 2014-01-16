/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BasicCompositor.h"
#include "TextureHostBasic.h"
#include "ipc/AutoOpenSurface.h"
#include "mozilla/layers/Effects.h"
#include "mozilla/layers/YCbCrImageDataSerializer.h"
#include "nsIWidget.h"
#include "gfx2DGlue.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Helpers.h"
#include "gfxUtils.h"
#include "YCbCrUtils.h"
#include <algorithm>
#include "ImageContainer.h"
#define PIXMAN_DONT_DEFINE_STDINT
#include "pixman.h"                     // for pixman_f_transform, etc

namespace mozilla {
using namespace mozilla::gfx;

namespace layers {

class DataTextureSourceBasic : public DataTextureSource
                             , public TextureSourceBasic
{
public:

  virtual TextureSourceBasic* AsSourceBasic() MOZ_OVERRIDE { return this; }

  virtual gfx::SourceSurface* GetSurface() MOZ_OVERRIDE { return mSurface; }

  SurfaceFormat GetFormat() const MOZ_OVERRIDE
  {
    return mSurface->GetFormat();
  }

  virtual IntSize GetSize() const MOZ_OVERRIDE
  {
    return mSurface->GetSize();
  }

  virtual bool Update(gfx::DataSourceSurface* aSurface,
                      nsIntRegion* aDestRegion = nullptr,
                      gfx::IntPoint* aSrcOffset = nullptr) MOZ_OVERRIDE
  {
    // XXX - For this to work with IncrementalContentHost we will need to support
    // the aDestRegion and aSrcOffset parameters properly;
    mSurface = aSurface;
    return true;
  }

  virtual void DeallocateDeviceData() MOZ_OVERRIDE
  {
    mSurface = nullptr;
    SetUpdateSerial(0);
  }

public:
  RefPtr<gfx::DataSourceSurface> mSurface;
};

/**
 * Texture source and host implementaion for software compositing.
 */
class DeprecatedTextureHostBasic : public DeprecatedTextureHost
                                 , public TextureSourceBasic
{
public:
  DeprecatedTextureHostBasic()
  : mCompositor(nullptr)
  {}

  SurfaceFormat GetFormat() const MOZ_OVERRIDE { return mFormat; }

  virtual IntSize GetSize() const MOZ_OVERRIDE { return mSize; }

  virtual TextureSourceBasic* AsSourceBasic() MOZ_OVERRIDE { return this; }

  SourceSurface *GetSurface() MOZ_OVERRIDE { return mSurface; }

  virtual void SetCompositor(Compositor* aCompositor)
  {
    mCompositor = static_cast<BasicCompositor*>(aCompositor);
  }

  virtual const char *Name() { return "DeprecatedTextureHostBasic"; }

protected:
  virtual void UpdateImpl(const SurfaceDescriptor& aImage,
                          nsIntRegion *aRegion,
                          nsIntPoint*) MOZ_OVERRIDE
  {
    AutoOpenSurface surf(OPEN_READ_ONLY, aImage);
    nsRefPtr<gfxASurface> surface = ShadowLayerForwarder::OpenDescriptor(OPEN_READ_ONLY, aImage);
    nsRefPtr<gfxImageSurface> image = surface->GetAsImageSurface();
    mFormat = ImageFormatToSurfaceFormat(image->Format());
    mSize = IntSize(image->Width(), image->Height());
    mSurface = Factory::CreateWrappingDataSourceSurface(image->Data(),
                                                        image->Stride(),
                                                        mSize,
                                                        mFormat);
  }

  virtual bool EnsureSurface() {
    return true;
  }

  virtual bool Lock() MOZ_OVERRIDE {
    return EnsureSurface();
  }

  virtual TemporaryRef<gfx::DataSourceSurface> GetAsSurface() MOZ_OVERRIDE {
    if (!mSurface) {
        return nullptr;
    }
    return mSurface->GetDataSurface();
  }

  BasicCompositor *mCompositor;
  RefPtr<SourceSurface> mSurface;
  IntSize mSize;
  SurfaceFormat mFormat;
};

void
DeserializerToPlanarYCbCrImageData(YCbCrImageDataDeserializer& aDeserializer, PlanarYCbCrData& aData)
{
  aData.mYChannel = aDeserializer.GetYData();
  aData.mYStride = aDeserializer.GetYStride();
  aData.mYSize = aDeserializer.GetYSize();
  aData.mCbChannel = aDeserializer.GetCbData();
  aData.mCrChannel = aDeserializer.GetCrData();
  aData.mCbCrStride = aDeserializer.GetCbCrStride();
  aData.mCbCrSize = aDeserializer.GetCbCrSize();
  aData.mPicSize = aDeserializer.GetYSize();
}

class YCbCrDeprecatedTextureHostBasic : public DeprecatedTextureHostBasic
{
public:
  virtual void UpdateImpl(const SurfaceDescriptor& aImage,
                          nsIntRegion *aRegion,
                          nsIntPoint*) MOZ_OVERRIDE
  {
    MOZ_ASSERT(aImage.type() == SurfaceDescriptor::TYCbCrImage);
    mSurface = nullptr;
    ConvertImageToRGB(aImage);
  }

  virtual void SwapTexturesImpl(const SurfaceDescriptor& aImage,
                                nsIntRegion* aRegion) MOZ_OVERRIDE
  {
    MOZ_ASSERT(aImage.type() == SurfaceDescriptor::TYCbCrImage);
    mSurface = nullptr;
  }

  virtual bool EnsureSurface() MOZ_OVERRIDE
  {
    if (mSurface) {
      return true;
    }
    if (!mBuffer) {
      return false;
    }
    return ConvertImageToRGB(*mBuffer);
  }

  bool ConvertImageToRGB(const SurfaceDescriptor& aImage)
  {
    YCbCrImageDataDeserializer deserializer(aImage.get_YCbCrImage().data().get<uint8_t>());
    PlanarYCbCrData data;
    DeserializerToPlanarYCbCrImageData(deserializer, data);

    gfx::SurfaceFormat format = SurfaceFormat::B8G8R8X8;
    gfx::IntSize size;
    gfx::GetYCbCrToRGBDestFormatAndSize(data, format, size);
    if (size.width > PlanarYCbCrImage::MAX_DIMENSION ||
        size.height > PlanarYCbCrImage::MAX_DIMENSION) {
      NS_ERROR("Illegal image dest width or height");
      return false;
    }

    mSize = size;
    mFormat = format;

    RefPtr<DataSourceSurface> surface = Factory::CreateDataSourceSurface(mSize, mFormat);
    gfx::ConvertYCbCrToRGB(data, format, size, surface->GetData(), surface->Stride());

    mSurface = surface;
    return true;
  }
};

TemporaryRef<DeprecatedTextureHost>
CreateBasicDeprecatedTextureHost(SurfaceDescriptorType aDescriptorType,
                             uint32_t aTextureHostFlags,
                             uint32_t aTextureFlags)
{
  RefPtr<DeprecatedTextureHost> result = nullptr;
  if (aDescriptorType == SurfaceDescriptor::TYCbCrImage) {
    result = new YCbCrDeprecatedTextureHostBasic();
  } else {
    MOZ_ASSERT(aDescriptorType == SurfaceDescriptor::TShmem ||
               aDescriptorType == SurfaceDescriptor::TMemoryImage,
               "We can only support Shmem currently");
    result = new DeprecatedTextureHostBasic();
  }

  result->SetFlags(aTextureFlags);
  return result.forget();
}

BasicCompositor::BasicCompositor(nsIWidget *aWidget)
  : mWidget(aWidget)
{
  MOZ_COUNT_CTOR(BasicCompositor);
  sBackend = LAYERS_BASIC;
}

BasicCompositor::~BasicCompositor()
{
  MOZ_COUNT_DTOR(BasicCompositor);
}

void BasicCompositor::Destroy()
{
  mWidget->CleanupRemoteDrawing();
  mWidget = nullptr;
}

TemporaryRef<CompositingRenderTarget>
BasicCompositor::CreateRenderTarget(const IntRect& aRect, SurfaceInitMode aInit)
{
  RefPtr<DrawTarget> target = mDrawTarget->CreateSimilarDrawTarget(aRect.Size(), SurfaceFormat::B8G8R8A8);

  RefPtr<BasicCompositingRenderTarget> rt = new BasicCompositingRenderTarget(target, aRect);

  return rt.forget();
}

TemporaryRef<CompositingRenderTarget>
BasicCompositor::CreateRenderTargetFromSource(const IntRect &aRect,
                                              const CompositingRenderTarget *aSource,
                                              const IntPoint &aSourcePoint)
{
  RefPtr<DrawTarget> target = mDrawTarget->CreateSimilarDrawTarget(aRect.Size(), SurfaceFormat::B8G8R8A8);
  RefPtr<BasicCompositingRenderTarget> rt = new BasicCompositingRenderTarget(target, aRect);

  DrawTarget *source;
  if (aSource) {
    const BasicCompositingRenderTarget* sourceSurface =
      static_cast<const BasicCompositingRenderTarget*>(aSource);
    source = sourceSurface->mDrawTarget;
  } else {
    source = mDrawTarget;
  }

  RefPtr<SourceSurface> snapshot = source->Snapshot();

  IntRect sourceRect(aSourcePoint, aRect.Size());
  rt->mDrawTarget->CopySurface(snapshot, sourceRect, IntPoint(0, 0));
  return rt.forget();
}

TemporaryRef<DataTextureSource>
BasicCompositor::CreateDataTextureSource(TextureFlags aFlags)
{
  RefPtr<DataTextureSource> result = new DataTextureSourceBasic();
  return result.forget();
}

bool
BasicCompositor::SupportsEffect(EffectTypes aEffect)
{
  return static_cast<EffectTypes>(aEffect) != EFFECT_YCBCR;
}

static void
DrawSurfaceWithTextureCoords(DrawTarget *aDest,
                             const gfx::Rect& aDestRect,
                             SourceSurface *aSource,
                             const gfx::Rect& aTextureCoords,
                             float aOpacity,
                             SourceSurface *aMask,
                             const Matrix& aMaskTransform)
{
  // Convert aTextureCoords into aSource's coordinate space
  gfxRect sourceRect(aTextureCoords.x * aSource->GetSize().width,
                     aTextureCoords.y * aSource->GetSize().height,
                     aTextureCoords.width * aSource->GetSize().width,
                     aTextureCoords.height * aSource->GetSize().height);
  // Compute a transform that maps sourceRect to aDestRect.
  gfxMatrix transform =
    gfxUtils::TransformRectToRect(sourceRect,
                                  gfxPoint(aDestRect.x, aDestRect.y),
                                  gfxPoint(aDestRect.XMost(), aDestRect.y),
                                  gfxPoint(aDestRect.XMost(), aDestRect.YMost()));
  Matrix matrix = ToMatrix(transform);
  if (aMask) {
    aDest->PushClipRect(aDestRect);
    Matrix maskTransformInverse = aMaskTransform;
    maskTransformInverse.Invert();
    Matrix dtTransform = aDest->GetTransform();
    aDest->SetTransform(aMaskTransform);
    Matrix patternMatrix = maskTransformInverse * dtTransform * matrix;
    aDest->MaskSurface(SurfacePattern(aSource, ExtendMode::REPEAT, patternMatrix),
                       aMask, Point(), DrawOptions(aOpacity));
    aDest->SetTransform(dtTransform);
    aDest->PopClip();
  } else {
    aDest->FillRect(aDestRect,
                    SurfacePattern(aSource, ExtendMode::REPEAT, matrix),
                    DrawOptions(aOpacity));
  }
}

static pixman_transform
Matrix3DToPixman(const gfx3DMatrix& aMatrix)
{
  pixman_f_transform transform;

  transform.m[0][0] = aMatrix._11;
  transform.m[0][1] = aMatrix._21;
  transform.m[0][2] = aMatrix._41;
  transform.m[1][0] = aMatrix._12;
  transform.m[1][1] = aMatrix._22;
  transform.m[1][2] = aMatrix._42;
  transform.m[2][0] = aMatrix._14;
  transform.m[2][1] = aMatrix._24;
  transform.m[2][2] = aMatrix._44;

  pixman_transform result;
  pixman_transform_from_pixman_f_transform(&result, &transform);

  return result;
}

static void
PixmanTransform(DataSourceSurface* aDest,
                DataSourceSurface* aSource,
                const gfx3DMatrix& aTransform,
                gfxPoint aDestOffset)
{
  IntSize destSize = aDest->GetSize();
  pixman_image_t* dest = pixman_image_create_bits(PIXMAN_a8r8g8b8,
                                                  destSize.width,
                                                  destSize.height,
                                                  (uint32_t*)aDest->GetData(),
                                                  aDest->Stride());

  IntSize srcSize = aSource->GetSize();
  pixman_image_t* src = pixman_image_create_bits(PIXMAN_a8r8g8b8,
                                                 srcSize.width,
                                                 srcSize.height,
                                                 (uint32_t*)aSource->GetData(),
                                                 aSource->Stride());

  NS_ABORT_IF_FALSE(src && dest, "Failed to create pixman images?");

  pixman_transform pixTransform = Matrix3DToPixman(aTransform);
  pixman_transform pixTransformInverted;

  // If the transform is singular then nothing would be drawn anyway, return here
  if (!pixman_transform_invert(&pixTransformInverted, &pixTransform)) {
    pixman_image_unref(dest);
    pixman_image_unref(src);
    return;
  }
  pixman_image_set_transform(src, &pixTransformInverted);

  pixman_image_composite32(PIXMAN_OP_SRC,
                           src,
                           nullptr,
                           dest,
                           aDestOffset.x,
                           aDestOffset.y,
                           0,
                           0,
                           0,
                           0,
                           destSize.width,
                           destSize.height);

  pixman_image_unref(dest);
  pixman_image_unref(src);
}

static inline IntRect
RoundOut(Rect r)
{
  r.RoundOut();
  return IntRect(r.x, r.y, r.width, r.height);
}

void
BasicCompositor::DrawQuad(const gfx::Rect& aRect,
                          const gfx::Rect& aClipRect,
                          const EffectChain &aEffectChain,
                          gfx::Float aOpacity,
                          const gfx::Matrix4x4 &aTransform)
{
  RefPtr<DrawTarget> buffer = mRenderTarget->mDrawTarget;

  // For 2D drawing, |dest| and |buffer| are the same surface. For 3D drawing,
  // |dest| is a temporary surface.
  RefPtr<DrawTarget> dest = buffer;

  buffer->PushClipRect(aClipRect);
  AutoSaveTransform autoSaveTransform(dest);

  Matrix newTransform;
  Rect transformBounds;
  gfx3DMatrix new3DTransform;
  IntPoint offset = mRenderTarget->GetOrigin();

  if (aTransform.Is2D()) {
    newTransform = aTransform.As2D();
  } else {
    // Create a temporary surface for the transform.
    dest = gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(RoundOut(aRect).Size(), SurfaceFormat::B8G8R8A8);
    if (!dest) {
      return;
    }

    // Get the bounds post-transform.
    To3DMatrix(aTransform, new3DTransform);
    gfxRect bounds = new3DTransform.TransformBounds(ThebesRect(aRect));
    bounds.IntersectRect(bounds, gfxRect(offset.x, offset.y, buffer->GetSize().width, buffer->GetSize().height));

    transformBounds = ToRect(bounds);
    transformBounds.RoundOut();

    // Propagate the coordinate offset to our 2D draw target.
    newTransform.Translate(transformBounds.x, transformBounds.y);

    // When we apply the 3D transformation, we do it against a temporary
    // surface, so undo the coordinate offset.
    new3DTransform = new3DTransform * gfx3DMatrix::Translation(-transformBounds.x, -transformBounds.y, 0);

    transformBounds.MoveTo(0, 0);
  }

  newTransform.Translate(-offset.x, -offset.y);
  buffer->SetTransform(newTransform);

  RefPtr<SourceSurface> sourceMask;
  Matrix maskTransform;
  if (aEffectChain.mSecondaryEffects[EFFECT_MASK]) {
    EffectMask *effectMask = static_cast<EffectMask*>(aEffectChain.mSecondaryEffects[EFFECT_MASK].get());
    sourceMask = effectMask->mMaskTexture->AsSourceBasic()->GetSurface();
    MOZ_ASSERT(effectMask->mMaskTransform.Is2D(), "How did we end up with a 3D transform here?!");
    MOZ_ASSERT(!effectMask->mIs3D);
    maskTransform = effectMask->mMaskTransform.As2D();
  }

  switch (aEffectChain.mPrimaryEffect->mType) {
    case EFFECT_SOLID_COLOR: {
      EffectSolidColor* effectSolidColor =
        static_cast<EffectSolidColor*>(aEffectChain.mPrimaryEffect.get());

      dest->FillRect(aRect,
                     ColorPattern(effectSolidColor->mColor),
                     DrawOptions(aOpacity));
      break;
    }
    case EFFECT_BGRA:
    case EFFECT_BGRX:
    case EFFECT_RGBA:
    case EFFECT_RGBX: {
      TexturedEffect* texturedEffect =
          static_cast<TexturedEffect*>(aEffectChain.mPrimaryEffect.get());
      TextureSourceBasic* source = texturedEffect->mTexture->AsSourceBasic();

      DrawSurfaceWithTextureCoords(dest, aRect,
                                   source->GetSurface(),
                                   texturedEffect->mTextureCoords,
                                   aOpacity, sourceMask, maskTransform);
      break;
    }
    case EFFECT_YCBCR: {
      NS_RUNTIMEABORT("Can't (easily) support component alpha with BasicCompositor!");
      break;
    }
    case EFFECT_RENDER_TARGET: {
      EffectRenderTarget* effectRenderTarget =
        static_cast<EffectRenderTarget*>(aEffectChain.mPrimaryEffect.get());
      RefPtr<BasicCompositingRenderTarget> surface
        = static_cast<BasicCompositingRenderTarget*>(effectRenderTarget->mRenderTarget.get());
      RefPtr<SourceSurface> sourceSurf = surface->mDrawTarget->Snapshot();

      DrawSurfaceWithTextureCoords(dest, aRect,
                                   sourceSurf,
                                   effectRenderTarget->mTextureCoords,
                                   aOpacity, sourceMask, maskTransform);
      break;
    }
    case EFFECT_COMPONENT_ALPHA: {
      NS_RUNTIMEABORT("Can't (easily) support component alpha with BasicCompositor!");
      break;
    }
    default: {
      NS_RUNTIMEABORT("Invalid effect type!");
      break;
    }
  }

  if (!aTransform.Is2D()) {
    dest->Flush();

    RefPtr<SourceSurface> snapshot = dest->Snapshot();
    RefPtr<DataSourceSurface> source = snapshot->GetDataSurface();
    RefPtr<DataSourceSurface> temp =
      Factory::CreateDataSourceSurface(RoundOut(transformBounds).Size(), SurfaceFormat::B8G8R8A8);
    if (!temp) {
      return;
    }

    PixmanTransform(temp, source, new3DTransform, gfxPoint(0, 0));

    buffer->DrawSurface(temp, transformBounds, transformBounds);
  }

  buffer->PopClip();
}

void
BasicCompositor::BeginFrame(const nsIntRegion& aInvalidRegion,
                            const gfx::Rect *aClipRectIn,
                            const gfx::Matrix& aTransform,
                            const gfx::Rect& aRenderBounds,
                            gfx::Rect *aClipRectOut /* = nullptr */,
                            gfx::Rect *aRenderBoundsOut /* = nullptr */)
{
  nsIntRect intRect;
  mWidget->GetClientBounds(intRect);
  Rect rect = Rect(0, 0, intRect.width, intRect.height);

  nsIntRect invalidRect = aInvalidRegion.GetBounds();
  mInvalidRect = IntRect(invalidRect.x, invalidRect.y, invalidRect.width, invalidRect.height);
  mInvalidRegion = aInvalidRegion;

  if (aRenderBoundsOut) {
    *aRenderBoundsOut = Rect();
  }

  if (mInvalidRect.width <= 0 || mInvalidRect.height <= 0) {
    return;
  }

  if (mCopyTarget) {
    // If we have a copy target, then we don't have a widget-provided mDrawTarget (currently). Create a dummy
    // placeholder so that CreateRenderTarget() works.
    mDrawTarget = gfxPlatform::GetPlatform()->CreateOffscreenCanvasDrawTarget(IntSize(1,1), SurfaceFormat::B8G8R8A8);
  } else {
    mDrawTarget = mWidget->StartRemoteDrawing();
  }
  if (!mDrawTarget) {
    return;
  }

  // Setup an intermediate render target to buffer all compositing. We will
  // copy this into mDrawTarget (the widget), and/or mCopyTarget in EndFrame()
  RefPtr<CompositingRenderTarget> target = CreateRenderTarget(mInvalidRect, INIT_MODE_CLEAR);
  SetRenderTarget(target);

  // We only allocate a surface sized to the invalidated region, so we need to
  // translate future coordinates.
  Matrix transform;
  transform.Translate(-invalidRect.x, -invalidRect.y);
  mRenderTarget->mDrawTarget->SetTransform(transform);

  gfxUtils::ClipToRegion(mRenderTarget->mDrawTarget, aInvalidRegion);

  if (aRenderBoundsOut) {
    *aRenderBoundsOut = rect;
  }

  if (aClipRectIn) {
    mRenderTarget->mDrawTarget->PushClipRect(*aClipRectIn);
  } else {
    mRenderTarget->mDrawTarget->PushClipRect(rect);
    if (aClipRectOut) {
      *aClipRectOut = rect;
    }
  }
}

void
BasicCompositor::EndFrame()
{
  mRenderTarget->mDrawTarget->PopClip();
  mRenderTarget->mDrawTarget->PopClip();

  // Note: Most platforms require us to buffer drawing to the widget surface.
  // That's why we don't draw to mDrawTarget directly.
  RefPtr<SourceSurface> source = mRenderTarget->mDrawTarget->Snapshot();
  RefPtr<DrawTarget> dest(mCopyTarget ? mCopyTarget : mDrawTarget);
  
  // The source DrawTarget is clipped to the invalidation region, so we have
  // to copy the individual rectangles in the region or else we'll draw blank
  // pixels.
  nsIntRegionRectIterator iter(mInvalidRegion);
  for (const nsIntRect *r = iter.Next(); r; r = iter.Next()) {
    dest->CopySurface(source,
                      IntRect(r->x - mInvalidRect.x, r->y - mInvalidRect.y, r->width, r->height),
                      IntPoint(r->x, r->y));
  }
  if (!mCopyTarget) {
    mWidget->EndRemoteDrawing();
  }

  mDrawTarget = nullptr;
  mRenderTarget = nullptr;
}

void
BasicCompositor::AbortFrame()
{
  mRenderTarget->mDrawTarget->PopClip();
  mRenderTarget->mDrawTarget->PopClip();
  mDrawTarget = nullptr;
  mRenderTarget = nullptr;
}

}
}
