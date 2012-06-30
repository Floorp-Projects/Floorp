/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/PLayersParent.h"
#include "BasicLayersImpl.h"
#include "gfxUtils.h"
#include "gfxSharedImageSurface.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

class BasicImageLayer : public ImageLayer, public BasicImplData {
public:
  BasicImageLayer(BasicLayerManager* aLayerManager) :
    ImageLayer(aLayerManager, static_cast<BasicImplData*>(this)),
    mSize(-1, -1)
  {
    MOZ_COUNT_CTOR(BasicImageLayer);
  }
  virtual ~BasicImageLayer()
  {
    MOZ_COUNT_DTOR(BasicImageLayer);
  }

  virtual void SetVisibleRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ImageLayer::SetVisibleRegion(aRegion);
  }

  virtual void Paint(gfxContext* aContext, Layer* aMaskLayer);

  static void PaintContext(gfxPattern* aPattern,
                           const nsIntRegion& aVisible,
                           float aOpacity,
                           gfxContext* aContext,
                           Layer* aMaskLayer);

  virtual already_AddRefed<gfxASurface> GetAsSurface();

protected:
  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }

  // only paints the image if aContext is non-null
  already_AddRefed<gfxPattern>
  GetAndPaintCurrentImage(gfxContext* aContext,
                          float aOpacity,
                          Layer* aMaskLayer);

  gfxIntSize mSize;
};

void
BasicImageLayer::Paint(gfxContext* aContext, Layer* aMaskLayer)
{
  if (IsHidden())
    return;
  nsRefPtr<gfxPattern> dontcare =
    GetAndPaintCurrentImage(aContext, GetEffectiveOpacity(), aMaskLayer);
}

already_AddRefed<gfxPattern>
BasicImageLayer::GetAndPaintCurrentImage(gfxContext* aContext,
                                         float aOpacity,
                                         Layer* aMaskLayer)
{
  if (!mContainer)
    return nsnull;

  mContainer->SetImageFactory(mManager->IsCompositingCheap() ? nsnull : BasicManager()->GetImageFactory());

  nsRefPtr<gfxASurface> surface;
  AutoLockImage autoLock(mContainer, getter_AddRefs(surface));
  Image *image = autoLock.GetImage();
  gfxIntSize size = mSize = autoLock.GetSize();

  if (!surface || surface->CairoStatus()) {
    return nsnull;
  }

  NS_ASSERTION(surface->GetContentType() != gfxASurface::CONTENT_ALPHA,
               "Image layer has alpha image");

  nsRefPtr<gfxPattern> pat = new gfxPattern(surface);
  if (!pat) {
    return nsnull;
  }

  pat->SetFilter(mFilter);
  gfxIntSize sourceSize = surface->GetSize();
  if (mScaleMode != SCALE_NONE) {
    NS_ASSERTION(mScaleMode == SCALE_STRETCH,
      "No other scalemodes than stretch and none supported yet.");
    gfxMatrix mat = pat->GetMatrix();
    mat.Scale(float(sourceSize.width) / mScaleToSize.width, float(sourceSize.height) / mScaleToSize.height);
    pat->SetMatrix(mat);
    size = mScaleToSize;
  }

  // The visible region can extend outside the image, so just draw
  // within the image bounds.
  if (aContext) {
    AutoSetOperator setOperator(aContext, GetOperator());
    PaintContext(pat,
                 nsIntRegion(nsIntRect(0, 0, size.width, size.height)),
                 aOpacity, aContext, aMaskLayer);

    GetContainer()->NotifyPaintedImage(image);
  }

  return pat.forget();
}

/*static*/ void
BasicImageLayer::PaintContext(gfxPattern* aPattern,
                              const nsIntRegion& aVisible,
                              float aOpacity,
                              gfxContext* aContext,
                              Layer* aMaskLayer)
{
  // Set PAD mode so that when the video is being scaled, we do not sample
  // outside the bounds of the video image.
  gfxPattern::GraphicsExtend extend = gfxPattern::EXTEND_PAD;

  if (aContext->IsCairo()) {
    // PAD is slow with X11 and Quartz surfaces, so prefer speed over correctness
    // and use NONE.
    nsRefPtr<gfxASurface> target = aContext->CurrentSurface();
    gfxASurface::gfxSurfaceType type = target->GetType();
    if (type == gfxASurface::SurfaceTypeXlib ||
        type == gfxASurface::SurfaceTypeXcb ||
        type == gfxASurface::SurfaceTypeQuartz) {
      extend = gfxPattern::EXTEND_NONE;
    }
  }

  aContext->NewPath();
  // No need to snap here; our transform has already taken care of it.
  // XXX true for arbitrary regions?  Don't care yet though
  gfxUtils::PathFromRegion(aContext, aVisible);
  aPattern->SetExtend(extend);
  aContext->SetPattern(aPattern);
  FillWithMask(aContext, aOpacity, aMaskLayer);

  // Reset extend mode for callers that need to reuse the pattern
  aPattern->SetExtend(extend);
}

already_AddRefed<gfxASurface>
BasicImageLayer::GetAsSurface()
{
  if (!mContainer) {
    return nsnull;
  }

  gfxIntSize dontCare;
  return mContainer->GetCurrentAsSurface(&dontCare);
}

class BasicShadowableImageLayer : public BasicImageLayer,
                                  public BasicShadowableLayer
{
public:
  BasicShadowableImageLayer(BasicShadowLayerManager* aManager) :
    BasicImageLayer(aManager),
    mBufferIsOpaque(false)
  {
    MOZ_COUNT_CTOR(BasicShadowableImageLayer);
  }
  virtual ~BasicShadowableImageLayer()
  {
    DestroyBackBuffer();
    MOZ_COUNT_DTOR(BasicShadowableImageLayer);
  }

  virtual void Paint(gfxContext* aContext, Layer* aMaskLayer);

  virtual void FillSpecificAttributes(SpecificLayerAttributes& aAttrs)
  {
    aAttrs = ImageLayerAttributes(mFilter, mForceSingleTile);
  }

  virtual Layer* AsLayer() { return this; }
  virtual ShadowableLayer* AsShadowableLayer() { return this; }

  virtual void SetBackBuffer(const SurfaceDescriptor& aBuffer)
  {
    mBackBuffer = aBuffer;
  }

  virtual void SetBackBufferYUVImage(gfxSharedImageSurface* aYBuffer,
                                     gfxSharedImageSurface* aUBuffer,
                                     gfxSharedImageSurface* aVBuffer)
  {
    mBackBufferY = aYBuffer;
    mBackBufferU = aUBuffer;
    mBackBufferV = aVBuffer;
  }

  virtual void Disconnect()
  {
    mBackBufferY = mBackBufferU = mBackBufferV = nsnull;
    mBackBuffer = SurfaceDescriptor();
    BasicShadowableLayer::Disconnect();
  }

  void DestroyBackBuffer()
  {
    if (IsSurfaceDescriptorValid(mBackBuffer)) {
      BasicManager()->ShadowLayerForwarder::DestroySharedSurface(&mBackBuffer);
    }
    if (mBackBufferY) {
      BasicManager()->ShadowLayerForwarder::DestroySharedSurface(mBackBufferY);
      BasicManager()->ShadowLayerForwarder::DestroySharedSurface(mBackBufferU);
      BasicManager()->ShadowLayerForwarder::DestroySharedSurface(mBackBufferV);
    }
  }

private:
  BasicShadowLayerManager* BasicManager()
  {
    return static_cast<BasicShadowLayerManager*>(mManager);
  }

  // For YUV Images these are the 3 planes (Y, Cb and Cr),
  // for RGB images only mBackSurface is used.
  SurfaceDescriptor mBackBuffer;
  bool mBufferIsOpaque;
  nsRefPtr<gfxSharedImageSurface> mBackBufferY;
  nsRefPtr<gfxSharedImageSurface> mBackBufferU;
  nsRefPtr<gfxSharedImageSurface> mBackBufferV;
  gfxIntSize mCbCrSize;
};
 
void
BasicShadowableImageLayer::Paint(gfxContext* aContext, Layer* aMaskLayer)
{
  if (!HasShadow()) {
    BasicImageLayer::Paint(aContext, aMaskLayer);
    return;
  }

  if (!mContainer) {
    return;
  }

  nsRefPtr<gfxASurface> surface;
  AutoLockImage autoLock(mContainer, getter_AddRefs(surface));

  Image *image = autoLock.GetImage();

  if (!image) {
    return;
  }

  if (aMaskLayer) {
    static_cast<BasicImplData*>(aMaskLayer->ImplData())
      ->Paint(aContext, nsnull);
  }

  if (image->GetFormat() == Image::PLANAR_YCBCR && BasicManager()->IsCompositingCheap()) {
    PlanarYCbCrImage *YCbCrImage = static_cast<PlanarYCbCrImage*>(image);
    const PlanarYCbCrImage::Data *data = YCbCrImage->GetData();
    NS_ASSERTION(data, "Must be able to retrieve yuv data from image!");

    if (mSize != data->mYSize || mCbCrSize != data->mCbCrSize || !mBackBufferY) {
      DestroyBackBuffer();
      mSize = data->mYSize;
      mCbCrSize = data->mCbCrSize;

      if (!BasicManager()->AllocBuffer(mSize, gfxASurface::CONTENT_ALPHA,
                                       getter_AddRefs(mBackBufferY)) ||
          !BasicManager()->AllocBuffer(mCbCrSize, gfxASurface::CONTENT_ALPHA,
                                       getter_AddRefs(mBackBufferU)) ||
          !BasicManager()->AllocBuffer(mCbCrSize, gfxASurface::CONTENT_ALPHA,
                                       getter_AddRefs(mBackBufferV))) {
        NS_RUNTIMEABORT("creating ImageLayer 'front buffer' failed!");
      }
    }

    for (int i = 0; i < data->mYSize.height; i++) {
      memcpy(mBackBufferY->Data() + i * mBackBufferY->Stride(),
             data->mYChannel + i * data->mYStride,
             data->mYSize.width);
    }
    for (int i = 0; i < data->mCbCrSize.height; i++) {
      memcpy(mBackBufferU->Data() + i * mBackBufferU->Stride(),
             data->mCbChannel + i * data->mCbCrStride,
             data->mCbCrSize.width);
      memcpy(mBackBufferV->Data() + i * mBackBufferV->Stride(),
             data->mCrChannel + i * data->mCbCrStride,
             data->mCbCrSize.width);
    }

    YUVImage yuv(mBackBufferY->GetShmem(),
                 mBackBufferU->GetShmem(),
                 mBackBufferV->GetShmem(),
                 data->GetPictureRect());

    BasicManager()->PaintedImage(BasicManager()->Hold(this),
                                 yuv);
    return;
  }

  gfxIntSize oldSize = mSize;
  nsRefPtr<gfxPattern> pat = GetAndPaintCurrentImage
    (aContext, GetEffectiveOpacity(), nsnull);
  if (!pat)
    return;

  bool isOpaque = (GetContentFlags() & CONTENT_OPAQUE);
  if (oldSize != mSize || 
      !IsSurfaceDescriptorValid(mBackBuffer) ||
      isOpaque != mBufferIsOpaque) {
    DestroyBackBuffer();
    mBufferIsOpaque = isOpaque;

    gfxASurface::gfxContentType type = gfxASurface::CONTENT_COLOR_ALPHA;
    if (surface) {
      type = surface->GetContentType();
    }
    if (type != gfxASurface::CONTENT_ALPHA &&
        isOpaque) {
      type = gfxASurface::CONTENT_COLOR;
    }

    if (!BasicManager()->AllocBuffer(mSize, type, &mBackBuffer))
      NS_RUNTIMEABORT("creating ImageLayer 'front buffer' failed!");
  }

  nsRefPtr<gfxASurface> backSurface =
    BasicManager()->OpenDescriptor(mBackBuffer);
  nsRefPtr<gfxContext> tmpCtx = new gfxContext(backSurface);
  tmpCtx->SetOperator(gfxContext::OPERATOR_SOURCE);
  PaintContext(pat,
               nsIntRegion(nsIntRect(0, 0, mSize.width, mSize.height)),
               1.0, tmpCtx, nsnull);

  BasicManager()->PaintedImage(BasicManager()->Hold(this),
                               mBackBuffer);
}

class BasicShadowImageLayer : public ShadowImageLayer, public BasicImplData {
public:
  BasicShadowImageLayer(BasicShadowLayerManager* aLayerManager) :
    ShadowImageLayer(aLayerManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicShadowImageLayer);
  }
  virtual ~BasicShadowImageLayer()
  {
    MOZ_COUNT_DTOR(BasicShadowImageLayer);
  }

  virtual void Disconnect()
  {
    DestroyFrontBuffer();
    ShadowImageLayer::Disconnect();
  }

  virtual void Swap(const SharedImage& aNewFront,
                    SharedImage* aNewBack);

  virtual void DestroyFrontBuffer()
  {
    if (mAllocator && IsSurfaceDescriptorValid(mFrontBuffer)) {
      mAllocator->DestroySharedSurface(&mFrontBuffer);
    }
  }

  virtual void Paint(gfxContext* aContext, Layer* aMaskLayer);
  already_AddRefed<gfxASurface> GetAsSurface();

protected:
  BasicShadowLayerManager* BasicManager()
  {
    return static_cast<BasicShadowLayerManager*>(mManager);
  }

  SurfaceDescriptor mFrontBuffer;
  gfxIntSize mSize;
};

void
BasicShadowImageLayer::Swap(const SharedImage& aNewFront,
                            SharedImage* aNewBack)
{
  nsRefPtr<gfxASurface> surface =
    BasicManager()->OpenDescriptor(aNewFront);
  // Destroy mFrontBuffer if size different or image type is different
  bool surfaceConfigChanged = surface->GetSize() != mSize;
  if (IsSurfaceDescriptorValid(mFrontBuffer)) {
    nsRefPtr<gfxASurface> front = BasicManager()->OpenDescriptor(mFrontBuffer);
    surfaceConfigChanged = surfaceConfigChanged ||
                           surface->GetContentType() != front->GetContentType();
  }
  if (surfaceConfigChanged) {
    DestroyFrontBuffer();
    mSize = surface->GetSize();
  }

  // If mFrontBuffer
  if (IsSurfaceDescriptorValid(mFrontBuffer)) {
    *aNewBack = mFrontBuffer;
  } else {
    *aNewBack = null_t();
  }
  mFrontBuffer = aNewFront.get_SurfaceDescriptor();
}

void
BasicShadowImageLayer::Paint(gfxContext* aContext, Layer* aMaskLayer)
{
  if (!IsSurfaceDescriptorValid(mFrontBuffer)) {
    return;
  }

  nsRefPtr<gfxASurface> surface =
    BasicManager()->OpenDescriptor(mFrontBuffer);
  nsRefPtr<gfxPattern> pat = new gfxPattern(surface);
  pat->SetFilter(mFilter);

  // The visible region can extend outside the image, so just draw
  // within the image bounds.
  AutoSetOperator setOperator(aContext, GetOperator());
  BasicImageLayer::PaintContext(pat,
                                nsIntRegion(nsIntRect(0, 0, mSize.width, mSize.height)),
                                GetEffectiveOpacity(), aContext,
                                aMaskLayer);
}

already_AddRefed<gfxASurface>
BasicShadowImageLayer::GetAsSurface()
{
  if (!IsSurfaceDescriptorValid(mFrontBuffer)) {
    return nsnull;
  }

  return BasicManager()->OpenDescriptor(mFrontBuffer);
 }

already_AddRefed<ImageLayer>
BasicLayerManager::CreateImageLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ImageLayer> layer = new BasicImageLayer(this);
  return layer.forget();
}

already_AddRefed<ImageLayer>
BasicShadowLayerManager::CreateImageLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<BasicShadowableImageLayer> layer =
    new BasicShadowableImageLayer(this);
  MAYBE_CREATE_SHADOW(Image);
  return layer.forget();
}

already_AddRefed<ShadowImageLayer>
BasicShadowLayerManager::CreateShadowImageLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ShadowImageLayer> layer = new BasicShadowImageLayer(this);
  return layer.forget();
}

}
}
