/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/ContentClient.h"
#include "mozilla/gfx/2D.h"
#include "BasicThebesLayer.h"
#include "nsIWidget.h"
#include "gfxUtils.h"
#include "gfxPlatform.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "gfxTeeSurface.h"
#ifdef XP_WIN
#include "gfxWindowsPlatform.h"
#endif

namespace mozilla {

using namespace gfx;

namespace layers {

/* static */ TemporaryRef<ContentClient>
ContentClient::CreateContentClient(CompositableForwarder* aForwarder)
{
  LayersBackend backend = aForwarder->GetCompositorBackendType();
  if (backend != LAYERS_OPENGL &&
      backend != LAYERS_D3D9 &&
      backend != LAYERS_D3D11 &&
      backend != LAYERS_BASIC) {
    return nullptr;
  }

  bool useDoubleBuffering = false;

#ifdef XP_WIN
  if (backend == LAYERS_D3D11) {
    useDoubleBuffering = !!gfxWindowsPlatform::GetPlatform()->GetD2DDevice();
  } else
#endif
  {
    useDoubleBuffering = LayerManagerComposite::SupportsDirectTexturing() ||
                         backend == LAYERS_BASIC;
  }

  if (useDoubleBuffering || PR_GetEnv("MOZ_FORCE_DOUBLE_BUFFERING")) {
    return new ContentClientDoubleBuffered(aForwarder);
  }
#ifdef XP_MACOSX
  if (backend == LAYERS_OPENGL) {
    return new ContentClientIncremental(aForwarder);
  }
#endif
  return new ContentClientSingleBuffered(aForwarder);

}

ContentClientBasic::ContentClientBasic(CompositableForwarder* aForwarder,
                                       BasicLayerManager* aManager)
: ContentClient(aForwarder), ThebesLayerBuffer(ContainsVisibleBounds), mManager(aManager)
{}

already_AddRefed<gfxASurface>
ContentClientBasic::CreateBuffer(ContentType aType,
                                 const nsIntRect& aRect,
                                 uint32_t aFlags,
                                 gfxASurface**)
{
  nsRefPtr<gfxASurface> referenceSurface = GetBuffer();
  if (!referenceSurface) {
    gfxContext* defaultTarget = mManager->GetDefaultTarget();
    if (defaultTarget) {
      referenceSurface = defaultTarget->CurrentSurface();
    } else {
      nsIWidget* widget = mManager->GetRetainerWidget();
      if (!widget || !(referenceSurface = widget->GetThebesSurface())) {
        referenceSurface = mManager->GetTarget()->CurrentSurface();
      }
    }
  }
  return referenceSurface->CreateSimilarSurface(
    aType, gfxIntSize(aRect.width, aRect.height));
}

TemporaryRef<DrawTarget>
ContentClientBasic::CreateDTBuffer(ContentType aType,
                                 const nsIntRect& aRect,
                                 uint32_t aFlags)
{
  NS_RUNTIMEABORT("ContentClientBasic does not support Moz2D drawing yet!");
  // TODO[Bas] - Implement me!?
  return nullptr;
}

void
ContentClientRemoteBuffer::DestroyBuffers()
{
  if (!mDeprecatedTextureClient) {
    return;
  }

  MOZ_ASSERT(mDeprecatedTextureClient->GetAccessMode() == DeprecatedTextureClient::ACCESS_READ_WRITE);
  mDeprecatedTextureClient = nullptr;
  mDeprecatedTextureClientOnWhite = nullptr;

  DestroyFrontBuffer();

  mForwarder->DestroyThebesBuffer(this);
}

void
ContentClientRemoteBuffer::BeginPaint()
{
  // XXX: So we might not have a DeprecatedTextureClient yet.. because it will
  // only be created by CreateBuffer.. which will deliver a locked surface!.
  if (mDeprecatedTextureClient) {
    SetBufferProvider(mDeprecatedTextureClient);
  }
  if (mDeprecatedTextureClientOnWhite) {
    SetBufferProviderOnWhite(mDeprecatedTextureClientOnWhite);
  }
}

void
ContentClientRemoteBuffer::EndPaint()
{
  // XXX: We might still not have a texture client if PaintThebes
  // decided we didn't need one yet because the region to draw was empty.
  SetBufferProvider(nullptr);
  SetBufferProviderOnWhite(nullptr);
  mOldTextures.Clear();

  if (mDeprecatedTextureClient) {
    mDeprecatedTextureClient->Unlock();
  }
  if (mDeprecatedTextureClientOnWhite) {
    mDeprecatedTextureClientOnWhite->Unlock();
  }
}

bool
ContentClientRemoteBuffer::CreateAndAllocateDeprecatedTextureClient(RefPtr<DeprecatedTextureClient>& aClient)
{
  aClient = CreateDeprecatedTextureClient(TEXTURE_CONTENT);
  MOZ_ASSERT(aClient, "Failed to create texture client");

  if (!aClient->EnsureAllocated(mSize, mContentType)) {
    aClient = CreateDeprecatedTextureClient(TEXTURE_FALLBACK);
    MOZ_ASSERT(aClient, "Failed to create texture client");
    if (!aClient->EnsureAllocated(mSize, mContentType)) {
      NS_WARNING("Could not allocate texture client");
      return false;
    }
  }

  MOZ_ASSERT(IsSurfaceDescriptorValid(*aClient->GetDescriptor()));
  return true;
}

void
ContentClientRemoteBuffer::BuildDeprecatedTextureClients(ContentType aType,
                                               const nsIntRect& aRect,
                                               uint32_t aFlags)
{
  NS_ABORT_IF_FALSE(!mIsNewBuffer,
                    "Bad! Did we create a buffer twice without painting?");

  mIsNewBuffer = true;

  if (mDeprecatedTextureClient) {
    mOldTextures.AppendElement(mDeprecatedTextureClient);
    if (mDeprecatedTextureClientOnWhite) {
      mOldTextures.AppendElement(mDeprecatedTextureClientOnWhite);
    }
    DestroyBuffers();
  }

  mContentType = aType;
  mSize = gfx::IntSize(aRect.width, aRect.height);
  mTextureInfo.mTextureFlags = aFlags | TEXTURE_DEALLOCATE_HOST;

  if (!CreateAndAllocateDeprecatedTextureClient(mDeprecatedTextureClient)) {
    return;
  }
  
  if (aFlags & BUFFER_COMPONENT_ALPHA) {
    if (!CreateAndAllocateDeprecatedTextureClient(mDeprecatedTextureClientOnWhite)) {
      return;
    }
    mTextureInfo.mTextureFlags |= ComponentAlpha;
  }

  CreateFrontBufferAndNotify(aRect);
}

TemporaryRef<DrawTarget>
ContentClientRemoteBuffer::CreateDTBuffer(ContentType aType,
                                          const nsIntRect& aRect,
                                          uint32_t aFlags)
{
  MOZ_ASSERT(!(aFlags & BUFFER_COMPONENT_ALPHA), "We don't support component alpha here!");
  BuildDeprecatedTextureClients(aType, aRect, aFlags);

  RefPtr<DrawTarget> ret = mDeprecatedTextureClient->LockDrawTarget();
  return ret.forget();
}

already_AddRefed<gfxASurface>
ContentClientRemoteBuffer::CreateBuffer(ContentType aType,
                                        const nsIntRect& aRect,
                                        uint32_t aFlags,
                                        gfxASurface** aWhiteSurface)
{
  BuildDeprecatedTextureClients(aType, aRect, aFlags);

  nsRefPtr<gfxASurface> ret = mDeprecatedTextureClient->LockSurface();
  if (aFlags & BUFFER_COMPONENT_ALPHA) {
    nsRefPtr<gfxASurface> retWhite = mDeprecatedTextureClientOnWhite->LockSurface();
    *aWhiteSurface = retWhite.forget().get();
  }
  return ret.forget();
}

nsIntRegion
ContentClientRemoteBuffer::GetUpdatedRegion(const nsIntRegion& aRegionToDraw,
                                            const nsIntRegion& aVisibleRegion,
                                            bool aDidSelfCopy)
{
  nsIntRegion updatedRegion;
  if (mIsNewBuffer || aDidSelfCopy) {
    // A buffer reallocation clears both buffers. The front buffer has all the
    // content by now, but the back buffer is still clear. Here, in effect, we
    // are saying to copy all of the pixels of the front buffer to the back.
    // Also when we self-copied in the buffer, the buffer space
    // changes and some changed buffer content isn't reflected in the
    // draw or invalidate region (on purpose!).  When this happens, we
    // need to read back the entire buffer too.
    updatedRegion = aVisibleRegion;
    mIsNewBuffer = false;
  } else {
    updatedRegion = aRegionToDraw;
  }

  NS_ASSERTION(BufferRect().Contains(aRegionToDraw.GetBounds()),
               "Update outside of buffer rect!");
  NS_ABORT_IF_FALSE(mDeprecatedTextureClient, "should have a back buffer by now");

  return updatedRegion;
}

void
ContentClientRemoteBuffer::Updated(const nsIntRegion& aRegionToDraw,
                                   const nsIntRegion& aVisibleRegion,
                                   bool aDidSelfCopy)
{
  nsIntRegion updatedRegion = GetUpdatedRegion(aRegionToDraw,
                                               aVisibleRegion,
                                               aDidSelfCopy);

  MOZ_ASSERT(mDeprecatedTextureClient);
  mDeprecatedTextureClient->SetAccessMode(DeprecatedTextureClient::ACCESS_NONE);
  if (mDeprecatedTextureClientOnWhite) {
    mDeprecatedTextureClientOnWhite->SetAccessMode(DeprecatedTextureClient::ACCESS_NONE);
  }
  LockFrontBuffer();
  mForwarder->UpdateTextureRegion(this,
                                  ThebesBufferData(BufferRect(),
                                                   BufferRotation()),
                                  updatedRegion);
}

void
ContentClientRemoteBuffer::SwapBuffers(const nsIntRegion& aFrontUpdatedRegion)
{
  MOZ_ASSERT(mDeprecatedTextureClient->GetAccessMode() == DeprecatedTextureClient::ACCESS_NONE);
  MOZ_ASSERT(!mDeprecatedTextureClientOnWhite || mDeprecatedTextureClientOnWhite->GetAccessMode() == DeprecatedTextureClient::ACCESS_NONE);
  MOZ_ASSERT(mDeprecatedTextureClient);

  mFrontAndBackBufferDiffer = true;
  mDeprecatedTextureClient->SetAccessMode(DeprecatedTextureClient::ACCESS_READ_WRITE);
  if (mDeprecatedTextureClientOnWhite) {
    mDeprecatedTextureClientOnWhite->SetAccessMode(DeprecatedTextureClient::ACCESS_READ_WRITE);
  }
}

ContentClientDoubleBuffered::~ContentClientDoubleBuffered()
{
  if (mDeprecatedTextureClient) {
    MOZ_ASSERT(mFrontClient);
    mDeprecatedTextureClient->SetDescriptor(SurfaceDescriptor());
    mFrontClient->SetDescriptor(SurfaceDescriptor());
  }
  if (mDeprecatedTextureClientOnWhite) {
    MOZ_ASSERT(mFrontClientOnWhite);
    mDeprecatedTextureClientOnWhite->SetDescriptor(SurfaceDescriptor());
    mFrontClientOnWhite->SetDescriptor(SurfaceDescriptor());
  }
}

void
ContentClientDoubleBuffered::CreateFrontBufferAndNotify(const nsIntRect& aBufferRect)
{
  if (!CreateAndAllocateDeprecatedTextureClient(mFrontClient)) {
    return;
  }

  if (mTextureInfo.mTextureFlags & ComponentAlpha) {
    if (!CreateAndAllocateDeprecatedTextureClient(mFrontClientOnWhite)) {
      return;
    }
  }

  mFrontBufferRect = aBufferRect;
  mFrontBufferRotation = nsIntPoint();
  
  mForwarder->CreatedDoubleBuffer(this,
                                  *mFrontClient->GetDescriptor(),
                                  *mDeprecatedTextureClient->GetDescriptor(),
                                  mTextureInfo,
                                  mFrontClientOnWhite ? mFrontClientOnWhite->GetDescriptor() : nullptr,
                                  mDeprecatedTextureClientOnWhite ? mDeprecatedTextureClientOnWhite->GetDescriptor() : nullptr);
}

void
ContentClientDoubleBuffered::DestroyFrontBuffer()
{
  MOZ_ASSERT(mFrontClient);
  MOZ_ASSERT(mFrontClient->GetAccessMode() != DeprecatedTextureClient::ACCESS_NONE);

  mFrontClient = nullptr;
  mFrontClientOnWhite = nullptr;
}

void
ContentClientDoubleBuffered::LockFrontBuffer()
{
  MOZ_ASSERT(mFrontClient);
  mFrontClient->SetAccessMode(DeprecatedTextureClient::ACCESS_NONE);
  if (mFrontClientOnWhite) {
    mFrontClientOnWhite->SetAccessMode(DeprecatedTextureClient::ACCESS_NONE);
  }
}

void
ContentClientDoubleBuffered::SwapBuffers(const nsIntRegion& aFrontUpdatedRegion)
{
  mFrontUpdatedRegion = aFrontUpdatedRegion;

  RefPtr<DeprecatedTextureClient> oldBack = mDeprecatedTextureClient;
  mDeprecatedTextureClient = mFrontClient;
  mFrontClient = oldBack;

  oldBack = mDeprecatedTextureClientOnWhite;
  mDeprecatedTextureClientOnWhite = mFrontClientOnWhite;
  mFrontClientOnWhite = oldBack;

  nsIntRect oldBufferRect = mBufferRect;
  mBufferRect = mFrontBufferRect;
  mFrontBufferRect = oldBufferRect;

  nsIntPoint oldBufferRotation = mBufferRotation;
  mBufferRotation = mFrontBufferRotation;
  mFrontBufferRotation = oldBufferRotation;

  MOZ_ASSERT(mFrontClient);
  mFrontClient->SetAccessMode(DeprecatedTextureClient::ACCESS_READ_ONLY);
  if (mFrontClientOnWhite) {
    mFrontClientOnWhite->SetAccessMode(DeprecatedTextureClient::ACCESS_READ_ONLY);
  }

  ContentClientRemoteBuffer::SwapBuffers(aFrontUpdatedRegion);
}

struct AutoDeprecatedTextureClient {
  AutoDeprecatedTextureClient()
    : mTexture(nullptr)
  {}
  ~AutoDeprecatedTextureClient()
  {
    if (mTexture) {
      mTexture->Unlock();
    }
  }
  gfxASurface* GetSurface(DeprecatedTextureClient* aTexture)
  {
    MOZ_ASSERT(!mTexture);
    mTexture = aTexture;
    if (mTexture) {
      return mTexture->LockSurface();
    }
    return nullptr;
  }
  DrawTarget* GetDrawTarget(DeprecatedTextureClient* aTexture)
  {
    MOZ_ASSERT(!mTexture);
    mTexture = aTexture;
    if (mTexture) {
      return mTexture->LockDrawTarget();
    }
    return nullptr;
  }
private:
  DeprecatedTextureClient* mTexture;
};

void
ContentClientDoubleBuffered::SyncFrontBufferToBackBuffer()
{
  if (!mFrontAndBackBufferDiffer) {
    return;
  }
  MOZ_ASSERT(mFrontClient);
  MOZ_ASSERT(mFrontClient->GetAccessMode() == DeprecatedTextureClient::ACCESS_READ_ONLY);
  MOZ_ASSERT(!mFrontClientOnWhite ||
             mFrontClientOnWhite->GetAccessMode() == DeprecatedTextureClient::ACCESS_READ_ONLY);

  MOZ_LAYERS_LOG(("BasicShadowableThebes(%p): reading back <x=%d,y=%d,w=%d,h=%d>",
                  this,
                  mFrontUpdatedRegion.GetBounds().x,
                  mFrontUpdatedRegion.GetBounds().y,
                  mFrontUpdatedRegion.GetBounds().width,
                  mFrontUpdatedRegion.GetBounds().height));

  nsIntRegion updateRegion = mFrontUpdatedRegion;

  int32_t xBoundary = mBufferRect.XMost() - mBufferRotation.x;
  int32_t yBoundary = mBufferRect.YMost() - mBufferRotation.y;

  // Figure out whether the area we want to copy wraps the edges of our buffer.
  bool needFullCopy = (xBoundary < updateRegion.GetBounds().XMost() &&
                       xBoundary > updateRegion.GetBounds().x) ||
                      (yBoundary < updateRegion.GetBounds().YMost() &&
                       yBoundary > updateRegion.GetBounds().y);
  
  // This is a tricky trade off, we're going to get stuff out of our
  // frontbuffer now, but the next PaintThebes might throw it all (or mostly)
  // away if the visible region has changed. This is why in reality we want
  // this code integrated with PaintThebes to always do the optimal thing.

  if (needFullCopy) {
    // We can't easily draw our front buffer into us, since we're going to be
    // copying stuff around anyway it's easiest if we just move our situation
    // to non-rotated while we're at it. If this situation occurs we'll have
    // hit a self-copy path in PaintThebes before as well anyway.
    mBufferRect.MoveTo(mFrontBufferRect.TopLeft());
    mBufferRotation = nsIntPoint();
    updateRegion = mBufferRect;
  } else {
    mBufferRect = mFrontBufferRect;
    mBufferRotation = mFrontBufferRotation;
  }
 
  AutoDeprecatedTextureClient autoTextureFront;
  AutoDeprecatedTextureClient autoTextureFrontOnWhite;
  if (gfxPlatform::GetPlatform()->SupportsAzureContent()) {
    RotatedBuffer frontBuffer(autoTextureFront.GetDrawTarget(mFrontClient),
                              autoTextureFrontOnWhite.GetDrawTarget(mFrontClientOnWhite),
                              mFrontBufferRect,
                              mFrontBufferRotation);
    UpdateDestinationFrom(frontBuffer, updateRegion);
  } else {
    RotatedBuffer frontBuffer(autoTextureFront.GetSurface(mFrontClient),
                              autoTextureFrontOnWhite.GetSurface(mFrontClientOnWhite),
                              mFrontBufferRect,
                              mFrontBufferRotation);
    UpdateDestinationFrom(frontBuffer, updateRegion);
  }

  mIsNewBuffer = false;
  mFrontAndBackBufferDiffer = false;
}

void
ContentClientDoubleBuffered::UpdateDestinationFrom(const RotatedBuffer& aSource,
                                                   const nsIntRegion& aUpdateRegion)
{
  nsRefPtr<gfxContext> destCtx =
    GetContextForQuadrantUpdate(aUpdateRegion.GetBounds(), BUFFER_BLACK);
  destCtx->SetOperator(gfxContext::OPERATOR_SOURCE);

  bool isClippingCheap = IsClippingCheap(destCtx, aUpdateRegion);
  if (isClippingCheap) {
    gfxUtils::ClipToRegion(destCtx, aUpdateRegion);
  }

  if (gfxPlatform::GetPlatform()->SupportsAzureContent()) {
    MOZ_ASSERT(!destCtx->IsCairo());

    if (destCtx->GetDrawTarget()->GetFormat() == FORMAT_B8G8R8A8) {
      destCtx->GetDrawTarget()->ClearRect(Rect(0, 0, mFrontBufferRect.width, mFrontBufferRect.height));
    }
    aSource.DrawBufferWithRotation(destCtx->GetDrawTarget(), BUFFER_BLACK);
  } else {
    aSource.DrawBufferWithRotation(destCtx, BUFFER_BLACK);
  }

  if (aSource.HaveBufferOnWhite()) {
    MOZ_ASSERT(HaveBufferOnWhite());
    nsRefPtr<gfxContext> destCtx =
      GetContextForQuadrantUpdate(aUpdateRegion.GetBounds(), BUFFER_WHITE);
    destCtx->SetOperator(gfxContext::OPERATOR_SOURCE);

    bool isClippingCheap = IsClippingCheap(destCtx, aUpdateRegion);
    if (isClippingCheap) {
      gfxUtils::ClipToRegion(destCtx, aUpdateRegion);
    }

    if (gfxPlatform::GetPlatform()->SupportsAzureContent()) {
      MOZ_ASSERT(!destCtx->IsCairo());

      if (destCtx->GetDrawTarget()->GetFormat() == FORMAT_B8G8R8A8) {
        destCtx->GetDrawTarget()->ClearRect(Rect(0, 0, mFrontBufferRect.width, mFrontBufferRect.height));
      }
      aSource.DrawBufferWithRotation(destCtx->GetDrawTarget(), BUFFER_WHITE);
    } else {
      aSource.DrawBufferWithRotation(destCtx, BUFFER_WHITE);
    }
  }
}

ContentClientSingleBuffered::~ContentClientSingleBuffered()
{
  if (mDeprecatedTextureClient) {
    mDeprecatedTextureClient->SetDescriptor(SurfaceDescriptor());
  }
  if (mDeprecatedTextureClientOnWhite) {
    mDeprecatedTextureClientOnWhite->SetDescriptor(SurfaceDescriptor());
  }
}

void
ContentClientSingleBuffered::CreateFrontBufferAndNotify(const nsIntRect& aBufferRect)
{
  mForwarder->CreatedSingleBuffer(this,
                                  *mDeprecatedTextureClient->GetDescriptor(),
                                  mTextureInfo,
                                  mDeprecatedTextureClientOnWhite ? mDeprecatedTextureClientOnWhite->GetDescriptor() : nullptr);
}

void
ContentClientSingleBuffered::SyncFrontBufferToBackBuffer()
{
  if (!mFrontAndBackBufferDiffer) {
    return;
  }

  gfxASurface* backBuffer = GetBuffer();
  if (!backBuffer && mDeprecatedTextureClient) {
    backBuffer = mDeprecatedTextureClient->LockSurface();
  }

  nsRefPtr<gfxASurface> oldBuffer;
  oldBuffer = SetBuffer(backBuffer,
                        mBufferRect,
                        mBufferRotation);

  backBuffer = GetBufferOnWhite();
  if (!backBuffer && mDeprecatedTextureClientOnWhite) {
    backBuffer = mDeprecatedTextureClientOnWhite->LockSurface();
  }

  oldBuffer = SetBufferOnWhite(backBuffer);

  mIsNewBuffer = false;
  mFrontAndBackBufferDiffer = false;
}

static void
WrapRotationAxis(int32_t* aRotationPoint, int32_t aSize)
{
  if (*aRotationPoint < 0) {
    *aRotationPoint += aSize;
  } else if (*aRotationPoint >= aSize) {
    *aRotationPoint -= aSize;
  }
}

static void
FillSurface(gfxASurface* aSurface, const nsIntRegion& aRegion,
            const nsIntPoint& aOffset, const gfxRGBA& aColor)
{
  nsRefPtr<gfxContext> ctx = new gfxContext(aSurface);
  ctx->Translate(-gfxPoint(aOffset.x, aOffset.y));
  gfxUtils::ClipToRegion(ctx, aRegion);
  ctx->SetColor(aColor);
  ctx->Paint();
}

ThebesLayerBuffer::PaintState
ContentClientIncremental::BeginPaintBuffer(ThebesLayer* aLayer,
                                           ThebesLayerBuffer::ContentType aContentType,
                                           uint32_t aFlags)
{
  mTextureInfo.mDeprecatedTextureHostFlags = 0;
  PaintState result;
  // We need to disable rotation if we're going to be resampled when
  // drawing, because we might sample across the rotation boundary.
  bool canHaveRotation =  !(aFlags & ThebesLayerBuffer::PAINT_WILL_RESAMPLE);

  nsIntRegion validRegion = aLayer->GetValidRegion();

  Layer::SurfaceMode mode;
  ContentType contentType;
  nsIntRegion neededRegion;
  bool canReuseBuffer;
  nsIntRect destBufferRect;

  while (true) {
    mode = aLayer->GetSurfaceMode();
    contentType = aContentType;
    neededRegion = aLayer->GetVisibleRegion();
    // If we're going to resample, we need a buffer that's in clamp mode.
    canReuseBuffer = neededRegion.GetBounds().Size() <= mBufferRect.Size() &&
      mHasBuffer &&
      (!(aFlags & ThebesLayerBuffer::PAINT_WILL_RESAMPLE) ||
       !(mTextureInfo.mTextureFlags & AllowRepeat));

    if (canReuseBuffer) {
      if (mBufferRect.Contains(neededRegion.GetBounds())) {
        // We don't need to adjust mBufferRect.
        destBufferRect = mBufferRect;
      } else {
        // The buffer's big enough but doesn't contain everything that's
        // going to be visible. We'll move it.
        destBufferRect = nsIntRect(neededRegion.GetBounds().TopLeft(), mBufferRect.Size());
      }
    } else {
      destBufferRect = neededRegion.GetBounds();
    }

    if (mode == Layer::SURFACE_COMPONENT_ALPHA) {
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
      mode = Layer::SURFACE_SINGLE_CHANNEL_ALPHA;
#else
      if (!aLayer->GetParent() || !aLayer->GetParent()->SupportsComponentAlphaChildren()) {
        mode = Layer::SURFACE_SINGLE_CHANNEL_ALPHA;
      } else {
        contentType = gfxASurface::CONTENT_COLOR;
      }
 #endif
    }

    if ((aFlags & ThebesLayerBuffer::PAINT_WILL_RESAMPLE) &&
        (!neededRegion.GetBounds().IsEqualInterior(destBufferRect) ||
         neededRegion.GetNumRects() > 1)) {
      // The area we add to neededRegion might not be painted opaquely
      if (mode == Layer::SURFACE_OPAQUE) {
        contentType = gfxASurface::CONTENT_COLOR_ALPHA;
        mode = Layer::SURFACE_SINGLE_CHANNEL_ALPHA;
      }
      // For component alpha layers, we leave contentType as CONTENT_COLOR.

      // We need to validate the entire buffer, to make sure that only valid
      // pixels are sampled
      neededRegion = destBufferRect;
    }

    if (mHasBuffer &&
        (mContentType != contentType ||
         (mode == Layer::SURFACE_COMPONENT_ALPHA) != mHasBufferOnWhite)) {
      // We're effectively clearing the valid region, so we need to draw
      // the entire needed region now.
      result.mRegionToInvalidate = aLayer->GetValidRegion();
      validRegion.SetEmpty();
      mHasBuffer = false;
      mHasBufferOnWhite = false;
      mBufferRect.SetRect(0, 0, 0, 0);
      mBufferRotation.MoveTo(0, 0);
      // Restart decision process with the cleared buffer. We can only go
      // around the loop one more iteration, since mTexImage is null now.
      continue;
    }

    break;
  }

  result.mRegionToDraw.Sub(neededRegion, validRegion);
  if (result.mRegionToDraw.IsEmpty())
    return result;

  if (destBufferRect.width > mForwarder->GetMaxTextureSize() ||
      destBufferRect.height > mForwarder->GetMaxTextureSize()) {
    return result;
  }

  // BlitTextureImage depends on the FBO texture target being
  // TEXTURE_2D.  This isn't the case on some older X1600-era Radeons.
  if (!mForwarder->SupportsTextureBlitting() ||
      !mForwarder->SupportsPartialUploads()) {
    result.mRegionToDraw = neededRegion;
    validRegion.SetEmpty();
    mHasBuffer = false;
    mHasBufferOnWhite = false;
    mBufferRect.SetRect(0, 0, 0, 0);
    mBufferRotation.MoveTo(0, 0);
    canReuseBuffer = false;
  }

  nsIntRect drawBounds = result.mRegionToDraw.GetBounds();
  bool createdBuffer = false;

  uint32_t bufferFlags = canHaveRotation ? AllowRepeat : 0;
  if (mode == Layer::SURFACE_COMPONENT_ALPHA) {
    bufferFlags |= ComponentAlpha;
  }
  if (canReuseBuffer) {
    nsIntRect keepArea;
    if (keepArea.IntersectRect(destBufferRect, mBufferRect)) {
      // Set mBufferRotation so that the pixels currently in mBuffer
      // will still be rendered in the right place when mBufferRect
      // changes to destBufferRect.
      nsIntPoint newRotation = mBufferRotation +
        (destBufferRect.TopLeft() - mBufferRect.TopLeft());
      WrapRotationAxis(&newRotation.x, mBufferRect.width);
      WrapRotationAxis(&newRotation.y, mBufferRect.height);
      NS_ASSERTION(nsIntRect(nsIntPoint(0,0), mBufferRect.Size()).Contains(newRotation),
                   "newRotation out of bounds");
      int32_t xBoundary = destBufferRect.XMost() - newRotation.x;
      int32_t yBoundary = destBufferRect.YMost() - newRotation.y;
      if ((drawBounds.x < xBoundary && xBoundary < drawBounds.XMost()) ||
          (drawBounds.y < yBoundary && yBoundary < drawBounds.YMost()) ||
          (newRotation != nsIntPoint(0,0) && !canHaveRotation)) {
        // The stuff we need to redraw will wrap around an edge of the
        // buffer, so we will need to do a self-copy
        // If mBufferRotation == nsIntPoint(0,0) we could do a real
        // self-copy but we're not going to do that in GL yet.
        // We can't do a real self-copy because the buffer is rotated.
        // So allocate a new buffer for the destination.
        destBufferRect = neededRegion.GetBounds();
        createdBuffer = true;
      } else {
        mBufferRect = destBufferRect;
        mBufferRotation = newRotation;
      }
    } else {
      // No pixels are going to be kept. The whole visible region
      // will be redrawn, so we don't need to copy anything, so we don't
      // set destBuffer.
      mBufferRect = destBufferRect;
      mBufferRotation = nsIntPoint(0,0);
    }
  } else {
    // The buffer's not big enough, so allocate a new one
    createdBuffer = true;
  }
  NS_ASSERTION(!(aFlags & ThebesLayerBuffer::PAINT_WILL_RESAMPLE) ||
               destBufferRect == neededRegion.GetBounds(),
               "If we're resampling, we need to validate the entire buffer");

  if (!createdBuffer && !mHasBuffer) {
    return result;
  }

  if (createdBuffer) {
    if (mHasBuffer &&
        (mode != Layer::SURFACE_COMPONENT_ALPHA || mHasBufferOnWhite)) {
      mTextureInfo.mDeprecatedTextureHostFlags = TEXTURE_HOST_COPY_PREVIOUS;
    }

    mHasBuffer = true;
    if (mode == Layer::SURFACE_COMPONENT_ALPHA) {
      mHasBufferOnWhite = true;
    }
    mBufferRect = destBufferRect;
    mBufferRotation = nsIntPoint(0,0);
    NotifyBufferCreated(contentType, bufferFlags);
  }

  NS_ASSERTION(canHaveRotation || mBufferRotation == nsIntPoint(0,0),
               "Rotation disabled, but we have nonzero rotation?");

  nsIntRegion invalidate;
  invalidate.Sub(aLayer->GetValidRegion(), destBufferRect);
  result.mRegionToInvalidate.Or(result.mRegionToInvalidate, invalidate);

  // BeginUpdate is allowed to modify the given region,
  // if it wants more to be repainted than we request.
  if (mode == Layer::SURFACE_COMPONENT_ALPHA) {
    nsIntRegion drawRegionCopy = result.mRegionToDraw;
    nsRefPtr<gfxASurface> onBlack = GetUpdateSurface(BUFFER_BLACK, drawRegionCopy);
    nsRefPtr<gfxASurface> onWhite = GetUpdateSurface(BUFFER_WHITE, result.mRegionToDraw);
    if (onBlack && onWhite) {
      NS_ASSERTION(result.mRegionToDraw == drawRegionCopy,
                   "BeginUpdate should always modify the draw region in the same way!");
      FillSurface(onBlack, result.mRegionToDraw, nsIntPoint(drawBounds.x, drawBounds.y), gfxRGBA(0.0, 0.0, 0.0, 1.0));
      FillSurface(onWhite, result.mRegionToDraw, nsIntPoint(drawBounds.x, drawBounds.y), gfxRGBA(1.0, 1.0, 1.0, 1.0));
      if (gfxPlatform::GetPlatform()->SupportsAzureContent()) {
        RefPtr<DrawTarget> onBlackDT = gfxPlatform::GetPlatform()->CreateDrawTargetForUpdateSurface(onBlack, onBlack->GetSize());
        RefPtr<DrawTarget> onWhiteDT = gfxPlatform::GetPlatform()->CreateDrawTargetForUpdateSurface(onWhite, onWhite->GetSize());
        RefPtr<DrawTarget> dt = Factory::CreateDualDrawTarget(onBlackDT, onWhiteDT);
        result.mContext = new gfxContext(dt);
      } else {
        gfxASurface* surfaces[2] = { onBlack.get(), onWhite.get() };
        nsRefPtr<gfxTeeSurface> surf = new gfxTeeSurface(surfaces, ArrayLength(surfaces));

        // XXX If the device offset is set on the individual surfaces instead of on
        // the tee surface, we render in the wrong place. Why?
        gfxPoint deviceOffset = onBlack->GetDeviceOffset();
        onBlack->SetDeviceOffset(gfxPoint(0, 0));
        onWhite->SetDeviceOffset(gfxPoint(0, 0));
        surf->SetDeviceOffset(deviceOffset);

        // Using this surface as a source will likely go horribly wrong, since
        // only the onBlack surface will really be used, so alpha information will
        // be incorrect.
        surf->SetAllowUseAsSource(false);
        result.mContext = new gfxContext(surf);
      }
    } else {
      result.mContext = nullptr;
    }
  } else {
    nsRefPtr<gfxASurface> surf = GetUpdateSurface(BUFFER_BLACK, result.mRegionToDraw);
    if (gfxPlatform::GetPlatform()->SupportsAzureContent()) {
      RefPtr<DrawTarget> dt = gfxPlatform::GetPlatform()->CreateDrawTargetForUpdateSurface(surf, surf->GetSize());
      result.mContext = new gfxContext(dt);
    } else {
      result.mContext = new gfxContext(surf);
    }
  }
  if (!result.mContext) {
    NS_WARNING("unable to get context for update");
    return result;
  }
  result.mContext->Translate(-gfxPoint(drawBounds.x, drawBounds.y));

  // If we do partial updates, we have to clip drawing to the regionToDraw.
  // If we don't clip, background images will be fillrect'd to the region correctly,
  // while text or lines will paint outside of the regionToDraw. This becomes apparent
  // with concave regions. Right now the scrollbars invalidate a narrow strip of the bar
  // although they never cover it. This leads to two draw rects, the narow strip and the actually
  // newly exposed area. It would be wise to fix this glitch in any way to have simpler
  // clip and draw regions.
  gfxUtils::ClipToRegion(result.mContext, result.mRegionToDraw);

  if (mContentType == gfxASurface::CONTENT_COLOR_ALPHA) {
    result.mContext->SetOperator(gfxContext::OPERATOR_CLEAR);
    result.mContext->Paint();
    result.mContext->SetOperator(gfxContext::OPERATOR_OVER);
  }

  return result;
}

void
ContentClientIncremental::Updated(const nsIntRegion& aRegionToDraw,
                                  const nsIntRegion& aVisibleRegion,
                                  bool aDidSelfCopy)
{
  if (IsSurfaceDescriptorValid(mUpdateDescriptor)) {
    ShadowLayerForwarder::CloseDescriptor(mUpdateDescriptor);

    mForwarder->UpdateTextureIncremental(this,
                                         TextureFront,
                                         mUpdateDescriptor,
                                         aRegionToDraw,
                                         mBufferRect,
                                         mBufferRotation);
    mUpdateDescriptor = SurfaceDescriptor();
  }
  if (IsSurfaceDescriptorValid(mUpdateDescriptorOnWhite)) {
    ShadowLayerForwarder::CloseDescriptor(mUpdateDescriptorOnWhite);

    mForwarder->UpdateTextureIncremental(this,
                                         TextureOnWhiteFront,
                                         mUpdateDescriptorOnWhite,
                                         aRegionToDraw,
                                         mBufferRect,
                                         mBufferRotation);
    mUpdateDescriptorOnWhite = SurfaceDescriptor();
  }

}

already_AddRefed<gfxASurface>
ContentClientIncremental::GetUpdateSurface(BufferType aType,
                                           nsIntRegion& aUpdateRegion)
{
  nsIntRect rgnSize = aUpdateRegion.GetBounds();
  if (!mBufferRect.Contains(rgnSize)) {
    NS_ERROR("update outside of image");
    return nullptr;
  }
  SurfaceDescriptor desc;
  if (!mForwarder->AllocSurfaceDescriptor(gfxIntSize(rgnSize.width, rgnSize.height),
                                          mContentType,
                                          &desc)) {
    NS_WARNING("creating SurfaceDescriptor failed!");
    return nullptr;
  }

  nsRefPtr<gfxASurface> tmpASurface =
    ShadowLayerForwarder::OpenDescriptor(OPEN_READ_WRITE, desc);

  if (aType == BUFFER_BLACK) {
    MOZ_ASSERT(!IsSurfaceDescriptorValid(mUpdateDescriptor));
    mUpdateDescriptor = desc;
  } else {
    MOZ_ASSERT(!IsSurfaceDescriptorValid(mUpdateDescriptorOnWhite));
    MOZ_ASSERT(aType == BUFFER_WHITE);
    mUpdateDescriptorOnWhite = desc;
  }

  return tmpASurface.forget();
}

}
}
