/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/PLayersParent.h"
#include "BasicLayersImpl.h"
#include "SharedTextureImage.h"
#include "gfxUtils.h"
#include "gfxSharedImageSurface.h"
#include "mozilla/layers/ImageContainerChild.h"

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

  virtual bool GetAsSurface(gfxASurface** aSurface,
                            SurfaceDescriptor* aDescriptor);

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

bool
BasicImageLayer::GetAsSurface(gfxASurface** aSurface,
                              SurfaceDescriptor* aDescriptor)
{
  if (!mContainer) {
    return false;
  }

  gfxIntSize dontCare;
  nsRefPtr<gfxASurface> surface = mContainer->GetCurrentAsSurface(&dontCare);
  *aSurface = surface.forget().get();
  return true;
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

  virtual void SetBackBufferYUVImage(const SurfaceDescriptor& aYBuffer,
                                     const SurfaceDescriptor& aUBuffer,
                                     const SurfaceDescriptor& aVBuffer)
  {
    mBackBufferY = aYBuffer;
    mBackBufferU = aUBuffer;
    mBackBufferV = aVBuffer;
  }

  virtual void Disconnect()
  {
    mBackBufferY = SurfaceDescriptor();
    mBackBufferU = SurfaceDescriptor();
    mBackBufferV = SurfaceDescriptor();
    mBackBuffer = SurfaceDescriptor();
    BasicShadowableLayer::Disconnect();
  }

  void DestroyBackBuffer()
  {
    if (IsSurfaceDescriptorValid(mBackBuffer)) {
      BasicManager()->ShadowLayerForwarder::DestroySharedSurface(&mBackBuffer);
    }
    if (IsSurfaceDescriptorValid(mBackBufferY)) {
      BasicManager()->ShadowLayerForwarder::DestroySharedSurface(&mBackBufferY);
      BasicManager()->ShadowLayerForwarder::DestroySharedSurface(&mBackBufferU);
      BasicManager()->ShadowLayerForwarder::DestroySharedSurface(&mBackBufferV);
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
  SurfaceDescriptor mBackBufferY;
  SurfaceDescriptor mBackBufferU;
  SurfaceDescriptor mBackBufferV;
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

  if (mContainer->IsAsync()) {
    PRUint32 containerID = mContainer->GetAsyncContainerID();
    BasicManager()->PaintedImage(BasicManager()->Hold(this), 
                                 SharedImageID(containerID));
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

  if (image->GetFormat() == Image::SHARED_TEXTURE &&
      BasicManager()->GetParentBackendType() == mozilla::layers::LAYERS_OPENGL) {
    SharedTextureImage *sharedImage = static_cast<SharedTextureImage*>(image);
    const SharedTextureImage::Data *data = sharedImage->GetData();

    SharedTextureDescriptor texture(data->mShareType, data->mHandle, data->mSize, data->mInverted);
    SurfaceDescriptor descriptor(texture);
    BasicManager()->PaintedImage(BasicManager()->Hold(this), descriptor);
    return;
  }

  if (image->GetFormat() == Image::PLANAR_YCBCR && BasicManager()->IsCompositingCheap()) {
    PlanarYCbCrImage *YCbCrImage = static_cast<PlanarYCbCrImage*>(image);
    const PlanarYCbCrImage::Data *data = YCbCrImage->GetData();
    NS_ASSERTION(data, "Must be able to retrieve yuv data from image!");

    if (mSize != data->mYSize || mCbCrSize != data->mCbCrSize || !IsSurfaceDescriptorValid(mBackBufferY)) {
      DestroyBackBuffer();
      mSize = data->mYSize;
      mCbCrSize = data->mCbCrSize;

      // We either allocate all three planes or none.
      if (!BasicManager()->AllocBufferWithCaps(mSize,
                                               gfxASurface::CONTENT_ALPHA,
                                               MAP_AS_IMAGE_SURFACE,
                                               &mBackBufferY) ||
          !BasicManager()->AllocBufferWithCaps(mCbCrSize,
                                               gfxASurface::CONTENT_ALPHA,
                                               MAP_AS_IMAGE_SURFACE,
                                               &mBackBufferU) ||
          !BasicManager()->AllocBufferWithCaps(mCbCrSize,
                                               gfxASurface::CONTENT_ALPHA,
                                               MAP_AS_IMAGE_SURFACE,
                                               &mBackBufferV)) {
        NS_RUNTIMEABORT("creating ImageLayer 'front buffer' failed!");
      }
    }

    AutoOpenSurface dyas(OPEN_READ_WRITE, mBackBufferY);
    gfxImageSurface* dy = dyas.GetAsImage();

    for (int i = 0; i < data->mYSize.height; i++) {
      memcpy(dy->Data() + i * dy->Stride(),
             data->mYChannel + i * data->mYStride,
             data->mYSize.width);
    }

    AutoOpenSurface duas(OPEN_READ_WRITE, mBackBufferU);
    gfxImageSurface* du = duas.GetAsImage();
    AutoOpenSurface dvas(OPEN_READ_WRITE, mBackBufferV);
    gfxImageSurface* dv = dvas.GetAsImage();

    for (int i = 0; i < data->mCbCrSize.height; i++) {
      memcpy(du->Data() + i * du->Stride(),
             data->mCbChannel + i * data->mCbCrStride,
             data->mCbCrSize.width);
      memcpy(dv->Data() + i * dv->Stride(),
             data->mCrChannel + i * data->mCbCrStride,
             data->mCbCrSize.width);
    }

    YUVImage yuv(mBackBufferY, mBackBufferU, mBackBufferV,
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

  AutoOpenSurface backSurface(OPEN_READ_WRITE, mBackBuffer);
  nsRefPtr<gfxContext> tmpCtx = new gfxContext(backSurface.Get());
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
  virtual bool GetAsSurface(gfxASurface** aSurface,
                            SurfaceDescriptor* aDescriptor);

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
  AutoOpenSurface autoSurface(OPEN_READ_ONLY, aNewFront);
  // Destroy mFrontBuffer if size different or image type is different
  bool surfaceConfigChanged = autoSurface.Size() != mSize;
  if (IsSurfaceDescriptorValid(mFrontBuffer)) {
    AutoOpenSurface autoFront(OPEN_READ_ONLY, mFrontBuffer);
    surfaceConfigChanged = surfaceConfigChanged ||
                           autoSurface.ContentType() != autoFront.ContentType();
  }
  if (surfaceConfigChanged) {
    DestroyFrontBuffer();
    mSize = autoSurface.Size();
  }

  // If mFrontBuffer
  if (IsSurfaceDescriptorValid(mFrontBuffer)) {
    *aNewBack = mFrontBuffer;
  } else {
    *aNewBack = null_t();
  }
  mFrontBuffer = aNewFront;
}

void
BasicShadowImageLayer::Paint(gfxContext* aContext, Layer* aMaskLayer)
{
  if (!IsSurfaceDescriptorValid(mFrontBuffer)) {
    return;
  }

  AutoOpenSurface autoSurface(OPEN_READ_ONLY, mFrontBuffer);
  nsRefPtr<gfxPattern> pat = new gfxPattern(autoSurface.Get());
  pat->SetFilter(mFilter);

  // The visible region can extend outside the image, so just draw
  // within the image bounds.
  AutoSetOperator setOperator(aContext, GetOperator());
  BasicImageLayer::PaintContext(pat,
                                nsIntRegion(nsIntRect(0, 0, mSize.width, mSize.height)),
                                GetEffectiveOpacity(), aContext,
                                aMaskLayer);
}

bool
BasicShadowImageLayer::GetAsSurface(gfxASurface** aSurface,
                                    SurfaceDescriptor* aDescriptor)
{
  if (!IsSurfaceDescriptorValid(mFrontBuffer)) {
    return false;
  }

  *aDescriptor = mFrontBuffer;
  return true;
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
