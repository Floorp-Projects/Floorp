/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BasicCompositor.h"
#include "BasicLayersImpl.h"            // for FillRectWithMask
#include "TextureHostBasic.h"
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
#include "gfxPrefs.h"
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

  virtual gfx::SourceSurface* GetSurface(DrawTarget* aTarget) MOZ_OVERRIDE { return mSurface; }

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

BasicCompositor::BasicCompositor(nsIWidget *aWidget)
  : mWidget(aWidget)
{
  MOZ_COUNT_CTOR(BasicCompositor);
  SetBackend(LayersBackend::LAYERS_BASIC);
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
  return static_cast<EffectTypes>(aEffect) != EffectTypes::YCBCR;
}

static void
DrawSurfaceWithTextureCoords(DrawTarget *aDest,
                             const gfx::Rect& aDestRect,
                             SourceSurface *aSource,
                             const gfx::Rect& aTextureCoords,
                             gfx::Filter aFilter,
                             float aOpacity,
                             SourceSurface *aMask,
                             const Matrix* aMaskTransform)
{
  // Convert aTextureCoords into aSource's coordinate space
  gfxRect sourceRect(aTextureCoords.x * aSource->GetSize().width,
                     aTextureCoords.y * aSource->GetSize().height,
                     aTextureCoords.width * aSource->GetSize().width,
                     aTextureCoords.height * aSource->GetSize().height);

  // Floating point error can accumulate above and we know our visible region
  // is integer-aligned, so round it out.
  sourceRect.Round();

  // Compute a transform that maps sourceRect to aDestRect.
  gfxMatrix transform =
    gfxUtils::TransformRectToRect(sourceRect,
                                  gfxPoint(aDestRect.x, aDestRect.y),
                                  gfxPoint(aDestRect.XMost(), aDestRect.y),
                                  gfxPoint(aDestRect.XMost(), aDestRect.YMost()));
  Matrix matrix = ToMatrix(transform);

  // Only use REPEAT if aTextureCoords is outside (0, 0, 1, 1).
  gfx::Rect unitRect(0, 0, 1, 1);
  ExtendMode mode = unitRect.Contains(aTextureCoords) ? ExtendMode::CLAMP : ExtendMode::REPEAT;

  FillRectWithMask(aDest, aDestRect, aSource, aFilter, DrawOptions(aOpacity),
                   mode, aMask, aMaskTransform, &matrix);
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

  newTransform.PostTranslate(-offset.x, -offset.y);
  buffer->SetTransform(newTransform);

  RefPtr<SourceSurface> sourceMask;
  Matrix maskTransform;
  if (aEffectChain.mSecondaryEffects[EffectTypes::MASK]) {
    EffectMask *effectMask = static_cast<EffectMask*>(aEffectChain.mSecondaryEffects[EffectTypes::MASK].get());
    sourceMask = effectMask->mMaskTexture->AsSourceBasic()->GetSurface(dest);
    MOZ_ASSERT(effectMask->mMaskTransform.Is2D(), "How did we end up with a 3D transform here?!");
    MOZ_ASSERT(!effectMask->mIs3D);
    maskTransform = effectMask->mMaskTransform.As2D();
    maskTransform.Translate(-offset.x, -offset.y);
  }

  switch (aEffectChain.mPrimaryEffect->mType) {
    case EffectTypes::SOLID_COLOR: {
      EffectSolidColor* effectSolidColor =
        static_cast<EffectSolidColor*>(aEffectChain.mPrimaryEffect.get());

      FillRectWithMask(dest, aRect, effectSolidColor->mColor,
                       DrawOptions(aOpacity), sourceMask, &maskTransform);
      break;
    }
    case EffectTypes::RGB: {
      TexturedEffect* texturedEffect =
          static_cast<TexturedEffect*>(aEffectChain.mPrimaryEffect.get());
      TextureSourceBasic* source = texturedEffect->mTexture->AsSourceBasic();

      if (texturedEffect->mPremultiplied) {
          DrawSurfaceWithTextureCoords(dest, aRect,
                                       source->GetSurface(dest),
                                       texturedEffect->mTextureCoords,
                                       texturedEffect->mFilter,
                                       aOpacity, sourceMask, &maskTransform);
      } else {
          RefPtr<DataSourceSurface> srcData = source->GetSurface(dest)->GetDataSurface();

          // Yes, we re-create the premultiplied data every time.
          // This might be better with a cache, eventually.
          RefPtr<DataSourceSurface> premultData = gfxUtils::CreatePremultipliedDataSurface(srcData);

          DrawSurfaceWithTextureCoords(dest, aRect,
                                       premultData,
                                       texturedEffect->mTextureCoords,
                                       texturedEffect->mFilter,
                                       aOpacity, sourceMask, &maskTransform);
      }
      break;
    }
    case EffectTypes::YCBCR: {
      NS_RUNTIMEABORT("Can't (easily) support component alpha with BasicCompositor!");
      break;
    }
    case EffectTypes::RENDER_TARGET: {
      EffectRenderTarget* effectRenderTarget =
        static_cast<EffectRenderTarget*>(aEffectChain.mPrimaryEffect.get());
      RefPtr<BasicCompositingRenderTarget> surface
        = static_cast<BasicCompositingRenderTarget*>(effectRenderTarget->mRenderTarget.get());
      RefPtr<SourceSurface> sourceSurf = surface->mDrawTarget->Snapshot();

      DrawSurfaceWithTextureCoords(dest, aRect,
                                   sourceSurf,
                                   effectRenderTarget->mTextureCoords,
                                   effectRenderTarget->mFilter,
                                   aOpacity, sourceMask, &maskTransform);
      break;
    }
    case EffectTypes::COMPONENT_ALPHA: {
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
BasicCompositor::ClearRect(const gfx::Rect& aRect)
{
  mRenderTarget->mDrawTarget->ClearRect(aRect);
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
  mWidgetSize = gfx::ToIntSize(intRect.Size());

  // The result of GetClientBounds is shifted over by the size of the window
  // manager styling. We want to ignore that.
  intRect.MoveTo(0, 0);
  Rect rect = Rect(0, 0, intRect.width, intRect.height);

  // Sometimes the invalid region is larger than we want to draw.
  nsIntRegion invalidRegionSafe;
  invalidRegionSafe.And(aInvalidRegion, intRect);

  nsIntRect invalidRect = invalidRegionSafe.GetBounds();
  mInvalidRect = IntRect(invalidRect.x, invalidRect.y, invalidRect.width, invalidRect.height);
  mInvalidRegion = invalidRegionSafe;

  if (aRenderBoundsOut) {
    *aRenderBoundsOut = Rect();
  }

  if (mInvalidRect.width <= 0 || mInvalidRect.height <= 0) {
    return;
  }

  if (mTarget) {
    // If we have a copy target, then we don't have a widget-provided mDrawTarget (currently). Create a dummy
    // placeholder so that CreateRenderTarget() works.
    mDrawTarget = gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(IntSize(1,1), SurfaceFormat::B8G8R8A8);
  } else {
    mDrawTarget = mWidget->StartRemoteDrawing();
  }
  if (!mDrawTarget) {
    return;
  }

  // Setup an intermediate render target to buffer all compositing. We will
  // copy this into mDrawTarget (the widget), and/or mTarget in EndFrame()
  RefPtr<CompositingRenderTarget> target = CreateRenderTarget(mInvalidRect, INIT_MODE_CLEAR);
  SetRenderTarget(target);

  // We only allocate a surface sized to the invalidated region, so we need to
  // translate future coordinates.
  Matrix transform;
  transform.Translate(-invalidRect.x, -invalidRect.y);
  mRenderTarget->mDrawTarget->SetTransform(transform);

  gfxUtils::ClipToRegion(mRenderTarget->mDrawTarget, invalidRegionSafe);

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
  // Pop aClipRectIn/bounds rect
  mRenderTarget->mDrawTarget->PopClip();

  if (gfxPrefs::WidgetUpdateFlashing()) {
    float r = float(rand()) / RAND_MAX;
    float g = float(rand()) / RAND_MAX;
    float b = float(rand()) / RAND_MAX;
    // We're still clipped to mInvalidRegion, so just fill the bounds.
    mRenderTarget->mDrawTarget->FillRect(ToRect(mInvalidRegion.GetBounds()),
                                         ColorPattern(Color(r, g, b, 0.2f)));
  }

  // Pop aInvalidregion
  mRenderTarget->mDrawTarget->PopClip();

  // Note: Most platforms require us to buffer drawing to the widget surface.
  // That's why we don't draw to mDrawTarget directly.
  RefPtr<SourceSurface> source = mRenderTarget->mDrawTarget->Snapshot();
  RefPtr<DrawTarget> dest(mTarget ? mTarget : mDrawTarget);

  nsIntPoint offset = mTarget ? mTargetBounds.TopLeft() : nsIntPoint();

  // The source DrawTarget is clipped to the invalidation region, so we have
  // to copy the individual rectangles in the region or else we'll draw blank
  // pixels.
  nsIntRegionRectIterator iter(mInvalidRegion);
  for (const nsIntRect *r = iter.Next(); r; r = iter.Next()) {
    dest->CopySurface(source,
                      IntRect(r->x - mInvalidRect.x, r->y - mInvalidRect.y, r->width, r->height),
                      IntPoint(r->x - offset.x, r->y - offset.y));
  }
  if (!mTarget) {
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
