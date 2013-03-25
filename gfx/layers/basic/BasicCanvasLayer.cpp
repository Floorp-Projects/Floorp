/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/PLayersParent.h"
#include "gfxImageSurface.h"
#include "GLContext.h"
#include "gfxUtils.h"
#include "gfxPlatform.h"
#include "mozilla/Preferences.h"
#include "SurfaceStream.h"
#include "SharedSurfaceGL.h"
#include "SharedSurfaceEGL.h"
#include "sampler.h"

#include "BasicLayersImpl.h"
#include "nsXULAppAPI.h"

using namespace mozilla::gfx;
using namespace mozilla::gl;

namespace mozilla {
namespace layers {

class BasicCanvasLayer : public CanvasLayer,
                         public BasicImplData
{
public:
  BasicCanvasLayer(BasicLayerManager* aLayerManager) :
    CanvasLayer(aLayerManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicCanvasLayer);
    mForceReadback = Preferences::GetBool("webgl.force-layers-readback", false);
  }
  virtual ~BasicCanvasLayer()
  {
    MOZ_COUNT_DTOR(BasicCanvasLayer);
  }

  virtual void SetVisibleRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    CanvasLayer::SetVisibleRegion(aRegion);
  }

  virtual void Initialize(const Data& aData);
  virtual void Paint(gfxContext* aContext, Layer* aMaskLayer);

  virtual void PaintWithOpacity(gfxContext* aContext,
                                float aOpacity,
                                Layer* aMaskLayer);

protected:
  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }
  void UpdateSurface(gfxASurface* aDestSurface = nullptr, Layer* aMaskLayer = nullptr);

  nsRefPtr<gfxASurface> mSurface;
  mozilla::RefPtr<mozilla::gfx::DrawTarget> mDrawTarget;
  nsRefPtr<mozilla::gl::GLContext> mGLContext;

  bool mIsGLAlphaPremult;
  bool mNeedsYFlip;
  bool mForceReadback;

  nsRefPtr<gfxImageSurface> mCachedTempSurface;
  gfxIntSize mCachedSize;
  gfxImageFormat mCachedFormat;

  gfxImageSurface* GetTempSurface(const gfxIntSize& aSize, const gfxImageFormat aFormat)
  {
    if (!mCachedTempSurface ||
        aSize.width != mCachedSize.width ||
        aSize.height != mCachedSize.height ||
        aFormat != mCachedFormat)
    {
      mCachedTempSurface = new gfxImageSurface(aSize, aFormat);
      mCachedSize = aSize;
      mCachedFormat = aFormat;
    }

    MOZ_ASSERT(mCachedTempSurface->Stride() == mCachedTempSurface->Width() * 4);
    return mCachedTempSurface;
  }

  void DiscardTempSurface()
  {
    mCachedTempSurface = nullptr;
  }
};

void
BasicCanvasLayer::Initialize(const Data& aData)
{
  NS_ASSERTION(mSurface == nullptr, "BasicCanvasLayer::Initialize called twice!");

  if (aData.mSurface) {
    mSurface = aData.mSurface;
    NS_ASSERTION(!aData.mGLContext, "CanvasLayer can't have both surface and GLContext");
    mNeedsYFlip = false;
  } else if (aData.mGLContext) {
    mGLContext = aData.mGLContext;
    mIsGLAlphaPremult = aData.mIsGLAlphaPremult;
    mNeedsYFlip = true;
    MOZ_ASSERT(mGLContext->IsOffscreen(), "canvas gl context isn't offscreen");

    // [Basic Layers, non-OMTC] WebGL layer init.
    // `GLScreenBuffer::Morph`ing is only needed in BasicShadowableCanvasLayer.
  } else if (aData.mDrawTarget) {
    mDrawTarget = aData.mDrawTarget;
    mSurface = gfxPlatform::GetPlatform()->CreateThebesSurfaceAliasForDrawTarget_hack(mDrawTarget);
    mNeedsYFlip = false;
  } else {
    NS_ERROR("CanvasLayer created without mSurface, mDrawTarget or mGLContext?");
  }

  mBounds.SetRect(0, 0, aData.mSize.width, aData.mSize.height);
}

void
BasicCanvasLayer::UpdateSurface(gfxASurface* aDestSurface, Layer* aMaskLayer)
{
  if (!IsDirty())
    return;
  Painted();

  if (mDrawTarget) {
    mDrawTarget->Flush();
    if (mDrawTarget->GetType() == BACKEND_COREGRAPHICS_ACCELERATED) {
      // We have an accelerated CG context which has changed, unlike a bitmap surface
      // where we can alias the bits on initializing the mDrawTarget, we need to readback
      // and copy the accelerated surface each frame. We want to support this for quick
      // thumbnail but if we're going to be doing this every frame it likely is better
      // to use a non accelerated (bitmap) canvas.
      mSurface = gfxPlatform::GetPlatform()->GetThebesSurfaceForDrawTarget(mDrawTarget);
    }
  }

  if (!mGLContext && aDestSurface) {
    nsRefPtr<gfxContext> tmpCtx = new gfxContext(aDestSurface);
    tmpCtx->SetOperator(gfxContext::OPERATOR_SOURCE);
    BasicCanvasLayer::PaintWithOpacity(tmpCtx, 1.0f, aMaskLayer);
    return;
  }

  if (mGLContext) {
    if (aDestSurface && aDestSurface->GetType() != gfxASurface::SurfaceTypeImage) {
      MOZ_ASSERT(false, "Destination surface must be ImageSurface type.");
      return;
    }

    nsRefPtr<gfxImageSurface> readSurf;
    nsRefPtr<gfxImageSurface> resultSurf;

    SharedSurface* sharedSurf = mGLContext->RequestFrame();
    if (!sharedSurf) {
      NS_WARNING("Null frame received.");
      return;
    }

    gfxIntSize readSize(sharedSurf->Size());
    gfxImageFormat format = (GetContentFlags() & CONTENT_OPAQUE)
                            ? gfxASurface::ImageFormatRGB24
                            : gfxASurface::ImageFormatARGB32;

    if (aDestSurface) {
      resultSurf = static_cast<gfxImageSurface*>(aDestSurface);
    } else {
      resultSurf = GetTempSurface(readSize, format);
    }
    MOZ_ASSERT(resultSurf);
    if (resultSurf->CairoStatus() != 0) {
      MOZ_ASSERT(false, "Bad resultSurf->CairoStatus().");
      return;
    }

    MOZ_ASSERT(sharedSurf->APIType() == APITypeT::OpenGL);
    SharedSurface_GL* surfGL = SharedSurface_GL::Cast(sharedSurf);

    if (surfGL->Type() == SharedSurfaceType::Basic) {
      SharedSurface_Basic* sharedSurf_Basic = SharedSurface_Basic::Cast(surfGL);
      readSurf = sharedSurf_Basic->GetData();
    } else {
      if (resultSurf->Format() == format &&
          resultSurf->GetSize() == readSize)
      {
        readSurf = resultSurf;
      } else {
        readSurf = GetTempSurface(readSize, format);
      }

      // Readback handles Flush/MarkDirty.
      mGLContext->Screen()->Readback(surfGL, readSurf);
    }
    MOZ_ASSERT(readSurf);

    bool needsPremult = surfGL->HasAlpha() && !mIsGLAlphaPremult;
    if (needsPremult) {
      gfxImageSurface* sizedReadSurf = nullptr;
      if (readSurf->Format()  == resultSurf->Format() &&
          readSurf->GetSize() == resultSurf->GetSize())
      {
        sizedReadSurf = readSurf;
      } else {
        readSurf->Flush();
        nsRefPtr<gfxContext> ctx = new gfxContext(resultSurf);
        ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
        ctx->SetSource(readSurf);
        ctx->Paint();

        sizedReadSurf = resultSurf;
      }
      MOZ_ASSERT(sizedReadSurf);

      readSurf->Flush();
      resultSurf->Flush();
      gfxUtils::PremultiplyImageSurface(readSurf, resultSurf);
      resultSurf->MarkDirty();
    } else if (resultSurf != readSurf) {
      // Didn't need premult, but we do need to blit to resultSurf
      readSurf->Flush();
      nsRefPtr<gfxContext> ctx = new gfxContext(resultSurf);
      ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
      ctx->SetSource(readSurf);
      ctx->Paint();
    }

    // stick our surface into mSurface, so that the Paint() path is the same
    if (!aDestSurface) {
      mSurface = resultSurf;
    }
  }
}

void
BasicCanvasLayer::Paint(gfxContext* aContext, Layer* aMaskLayer)
{
  if (IsHidden())
    return;

  FirePreTransactionCallback();
  UpdateSurface();
  FireDidTransactionCallback();

  PaintWithOpacity(aContext, GetEffectiveOpacity(), aMaskLayer);
}

void
BasicCanvasLayer::PaintWithOpacity(gfxContext* aContext,
                                   float aOpacity,
                                   Layer* aMaskLayer)
{
  NS_ASSERTION(BasicManager()->InDrawing(),
               "Can only draw in drawing phase");

  if (!mSurface) {
    NS_WARNING("No valid surface to draw!");
    return;
  }

  nsRefPtr<gfxPattern> pat = new gfxPattern(mSurface);

  pat->SetFilter(mFilter);
  pat->SetExtend(gfxPattern::EXTEND_PAD);

  gfxMatrix m;
  if (mNeedsYFlip) {
    m = aContext->CurrentMatrix();
    aContext->Translate(gfxPoint(0.0, mBounds.height));
    aContext->Scale(1.0, -1.0);
  }

  // If content opaque, then save off current operator and set to source.
  // This ensures that alpha is not applied even if the source surface
  // has an alpha channel
  gfxContext::GraphicsOperator savedOp;
  if (GetContentFlags() & CONTENT_OPAQUE) {
    savedOp = aContext->CurrentOperator();
    aContext->SetOperator(gfxContext::OPERATOR_SOURCE);
  }

  AutoSetOperator setOperator(aContext, GetOperator());
  aContext->NewPath();
  // No need to snap here; our transform is already set up to snap our rect
  aContext->Rectangle(gfxRect(0, 0, mBounds.width, mBounds.height));
  aContext->SetPattern(pat);

  FillWithMask(aContext, aOpacity, aMaskLayer);

  // Restore surface operator
  if (GetContentFlags() & CONTENT_OPAQUE) {
    aContext->SetOperator(savedOp);
  }  

  if (mNeedsYFlip) {
    aContext->SetMatrix(m);
  }
}

class BasicShadowableCanvasLayer : public BasicCanvasLayer,
                                   public BasicShadowableLayer
{
public:
  BasicShadowableCanvasLayer(BasicShadowLayerManager* aManager) :
    BasicCanvasLayer(aManager),
    mBufferIsOpaque(false)
  {
    MOZ_COUNT_CTOR(BasicShadowableCanvasLayer);
  }
  virtual ~BasicShadowableCanvasLayer()
  {
    DestroyBackBuffer();
    MOZ_COUNT_DTOR(BasicShadowableCanvasLayer);
  }

  virtual void Initialize(const Data& aData);
  virtual void Paint(gfxContext* aContext, Layer* aMaskLayer);

  virtual void ClearCachedResources() MOZ_OVERRIDE
  {
    DestroyBackBuffer();
  }

  virtual void FillSpecificAttributes(SpecificLayerAttributes& aAttrs)
  {
    aAttrs = CanvasLayerAttributes(mFilter);
  }

  virtual Layer* AsLayer() { return this; }
  virtual ShadowableLayer* AsShadowableLayer() { return this; }

  virtual void SetBackBuffer(const SurfaceDescriptor& aBuffer)
  {
    mBackBuffer = aBuffer;
  }

  virtual void Disconnect()
  {
    mBackBuffer = SurfaceDescriptor();
    BasicShadowableLayer::Disconnect();
  }

  void DestroyBackBuffer()
  {
    MOZ_ASSERT(mBackBuffer.type() != SurfaceDescriptor::TSharedTextureDescriptor);
    if (IsSurfaceDescriptorValid(mBackBuffer)) {
      BasicManager()->ShadowLayerForwarder::DestroySharedSurface(&mBackBuffer);
      mBackBuffer = SurfaceDescriptor();
    }
  }

private:
  BasicShadowLayerManager* BasicManager()
  {
    return static_cast<BasicShadowLayerManager*>(mManager);
  }

  SurfaceDescriptor mBackBuffer;
  bool mBufferIsOpaque;
};

void
BasicShadowableCanvasLayer::Initialize(const Data& aData)
{
  BasicCanvasLayer::Initialize(aData);
  if (!HasShadow())
      return;

  if (mGLContext) {
    GLScreenBuffer* screen = mGLContext->Screen();
    SurfaceStreamType streamType =
        SurfaceStream::ChooseGLStreamType(SurfaceStream::OffMainThread,
                                          screen->PreserveBuffer());
    SurfaceFactory_GL* factory = nullptr;
    if (!mForceReadback) {
      if (BasicManager()->GetParentBackendType() == mozilla::layers::LAYERS_OPENGL) {
        if (mGLContext->GetEGLContext()) {
          bool isCrossProcess = !(XRE_GetProcessType() == GeckoProcessType_Default);

          if (!isCrossProcess) {
            // [Basic/OGL Layers, OMTC] WebGL layer init.
            factory = SurfaceFactory_EGLImage::Create(mGLContext, screen->Caps());
          } else {
            // [Basic/OGL Layers, OOPC] WebGL layer init. (Out Of Process Compositing)
            // Fall back to readback.
          }
        } else {
          // [Basic Layers, OMTC] WebGL layer init.
          // Well, this *should* work...
          factory = new SurfaceFactory_GLTexture(mGLContext, mGLContext, screen->Caps());
        }
      }
    }

    if (factory) {
      screen->Morph(factory, streamType);
    }
  }

  if (IsSurfaceDescriptorValid(mBackBuffer)) {
    AutoOpenSurface backSurface(OPEN_READ_ONLY, mBackBuffer);
    if (gfxIntSize(mBounds.width, mBounds.height) != backSurface.Size()) {
      DestroyBackBuffer();
    }
  }
}

void
BasicShadowableCanvasLayer::Paint(gfxContext* aContext, Layer* aMaskLayer)
{
  SAMPLE_LABEL("BasicShadowableCanvasLayer", "Paint");
  if (!HasShadow()) {
    BasicCanvasLayer::Paint(aContext, aMaskLayer);
    return;
  }

  if (!IsDirty())
    return;

  if (mGLContext &&
      !mForceReadback &&
      BasicManager()->GetParentBackendType() == mozilla::layers::LAYERS_OPENGL)
  {
    bool isCrossProcess = XRE_GetProcessType() != GeckoProcessType_Default;
    // Todo: If isCrossProcess (OMPC), spin off mini-thread to RequestFrame
    // and forward Gralloc handle. Signal WaitSync complete with XPC mutex?
    if (!isCrossProcess) {
      FirePreTransactionCallback();
      GLScreenBuffer* screen = mGLContext->Screen();
      SurfaceStreamHandle handle = screen->Stream()->GetShareHandle();

      mBackBuffer = SurfaceStreamDescriptor(handle, false);

      // Call Painted() to reset our dirty 'bit'.
      Painted();
      FireDidTransactionCallback();
      BasicManager()->PaintedCanvasNoSwap(BasicManager()->Hold(this),
                                          mNeedsYFlip,
                                          mBackBuffer);
      // Move SharedTextureHandle ownership to ShadowLayer
      mBackBuffer = SurfaceDescriptor();
      return;
    }
  }

  bool isOpaque = (GetContentFlags() & CONTENT_OPAQUE);
  if (!IsSurfaceDescriptorValid(mBackBuffer) ||
      isOpaque != mBufferIsOpaque) {
    DestroyBackBuffer();
    mBufferIsOpaque = isOpaque;

    gfxIntSize size(mBounds.width, mBounds.height);
    gfxASurface::gfxContentType type = isOpaque ?
        gfxASurface::CONTENT_COLOR : gfxASurface::CONTENT_COLOR_ALPHA;

    if (!BasicManager()->AllocBuffer(size, type, &mBackBuffer)) {
      NS_RUNTIMEABORT("creating CanvasLayer back buffer failed!");
    }
  }

  AutoOpenSurface autoBackSurface(OPEN_READ_WRITE, mBackBuffer);

  if (aMaskLayer) {
    static_cast<BasicImplData*>(aMaskLayer->ImplData())
      ->Paint(aContext, nullptr);
  }
  FirePreTransactionCallback();
  UpdateSurface(autoBackSurface.Get(), nullptr);
  FireDidTransactionCallback();

  BasicManager()->PaintedCanvas(BasicManager()->Hold(this),
                                mNeedsYFlip, mBackBuffer);
}

class BasicShadowCanvasLayer : public ShadowCanvasLayer,
                               public BasicImplData
{
public:
  BasicShadowCanvasLayer(BasicShadowLayerManager* aLayerManager) :
    ShadowCanvasLayer(aLayerManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicShadowCanvasLayer);
  }
  virtual ~BasicShadowCanvasLayer()
  {
    MOZ_COUNT_DTOR(BasicShadowCanvasLayer);
  }

  virtual void Disconnect()
  {
    DestroyFrontBuffer();
    ShadowCanvasLayer::Disconnect();
  }

  virtual void Initialize(const Data& aData);
  void Swap(const CanvasSurface& aNewFront, bool needYFlip, CanvasSurface* aNewBack);

  virtual void DestroyFrontBuffer()
  {
    if (IsSurfaceDescriptorValid(mFrontSurface)) {
      mAllocator->DestroySharedSurface(&mFrontSurface);
    }
  }

  virtual void Paint(gfxContext* aContext, Layer* aMaskLayer);

private:
  BasicShadowLayerManager* BasicManager()
  {
    return static_cast<BasicShadowLayerManager*>(mManager);
  }

  SurfaceDescriptor mFrontSurface;
  bool mNeedsYFlip;
};


void
BasicShadowCanvasLayer::Initialize(const Data& aData)
{
  NS_RUNTIMEABORT("Incompatibe surface type");
}

void
BasicShadowCanvasLayer::Swap(const CanvasSurface& aNewFront, bool needYFlip,
                             CanvasSurface* aNewBack)
{
  AutoOpenSurface autoSurface(OPEN_READ_ONLY, aNewFront);
  // Destroy mFrontBuffer if size different
  gfxIntSize sz = autoSurface.Size();
  bool surfaceConfigChanged = sz != gfxIntSize(mBounds.width, mBounds.height);
  if (IsSurfaceDescriptorValid(mFrontSurface)) {
    AutoOpenSurface autoFront(OPEN_READ_ONLY, mFrontSurface);
    surfaceConfigChanged = surfaceConfigChanged ||
                           autoSurface.ContentType() != autoFront.ContentType();
  }
  if (surfaceConfigChanged) {
    DestroyFrontBuffer();
    mBounds.SetRect(0, 0, sz.width, sz.height);
  }

  mNeedsYFlip = needYFlip;
  // If mFrontBuffer
  if (IsSurfaceDescriptorValid(mFrontSurface)) {
    *aNewBack = mFrontSurface;
  } else {
    *aNewBack = null_t();
  }
  mFrontSurface = aNewFront;
}

void
BasicShadowCanvasLayer::Paint(gfxContext* aContext, Layer* aMaskLayer)
{
  NS_ASSERTION(BasicManager()->InDrawing(),
               "Can only draw in drawing phase");

  if (!IsSurfaceDescriptorValid(mFrontSurface)) {
    return;
  }

  AutoOpenSurface autoSurface(OPEN_READ_ONLY, mFrontSurface);
  nsRefPtr<gfxPattern> pat = new gfxPattern(autoSurface.Get());

  pat->SetFilter(mFilter);
  pat->SetExtend(gfxPattern::EXTEND_PAD);

  gfxRect r(0, 0, mBounds.width, mBounds.height);

  gfxMatrix m;
  if (mNeedsYFlip) {
    m = aContext->CurrentMatrix();
    aContext->Translate(gfxPoint(0.0, mBounds.height));
    aContext->Scale(1.0, -1.0);
  }

  AutoSetOperator setOperator(aContext, GetOperator());
  aContext->NewPath();
  // No need to snap here; our transform has already taken care of it
  aContext->Rectangle(r);
  aContext->SetPattern(pat);
  FillWithMask(aContext, GetEffectiveOpacity(), aMaskLayer);

  if (mNeedsYFlip) {
    aContext->SetMatrix(m);
  }
}

already_AddRefed<CanvasLayer>
BasicLayerManager::CreateCanvasLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<CanvasLayer> layer = new BasicCanvasLayer(this);
  return layer.forget();
}

already_AddRefed<CanvasLayer>
BasicShadowLayerManager::CreateCanvasLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<BasicShadowableCanvasLayer> layer =
    new BasicShadowableCanvasLayer(this);
  MAYBE_CREATE_SHADOW(Canvas);
  return layer.forget();
}

already_AddRefed<ShadowCanvasLayer>
BasicShadowLayerManager::CreateShadowCanvasLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ShadowCanvasLayer> layer = new BasicShadowCanvasLayer(this);
  return layer.forget();
}

}
}
