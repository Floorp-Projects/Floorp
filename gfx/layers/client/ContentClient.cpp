/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/ContentClient.h"
#include "BasicLayers.h"                // for BasicLayerManager
#include "Layers.h"                     // for ThebesLayer, Layer, etc
#include "gfxColor.h"                   // for gfxRGBA
#include "gfxContext.h"                 // for gfxContext, etc
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxPoint.h"                   // for gfxIntSize, gfxPoint
#include "gfxTeeSurface.h"              // for gfxTeeSurface
#include "gfxUtils.h"                   // for gfxUtils
#include "ipc/ShadowLayers.h"           // for ShadowLayerForwarder
#include "mozilla/Util.h"               // for ArrayLength
#include "mozilla/gfx/2D.h"             // for DrawTarget, Factory
#include "mozilla/gfx/BasePoint.h"      // for BasePoint
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/gfx/Types.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/LayersMessages.h"  // for ThebesBufferData
#include "mozilla/layers/LayersTypes.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsDebug.h"                    // for NS_ASSERTION, NS_WARNING, etc
#include "nsISupportsImpl.h"            // for gfxContext::Release, etc
#include "nsIWidget.h"                  // for nsIWidget
#include "prenv.h"                      // for PR_GetEnv
#ifdef XP_WIN
#include "gfxWindowsPlatform.h"
#endif
#include "gfx2DGlue.h"

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
  : ContentClient(aForwarder)
  , RotatedContentBuffer(ContainsVisibleBounds)
  , mManager(aManager)
{}

void
ContentClientBasic::CreateBuffer(ContentType aType,
                                 const nsIntRect& aRect,
                                 uint32_t aFlags,
                                 RefPtr<gfx::DrawTarget>* aBlackDT,
                                 RefPtr<gfx::DrawTarget>* aWhiteDT)
{
  MOZ_ASSERT(!(aFlags & BUFFER_COMPONENT_ALPHA));
  MOZ_ASSERT(gfxPlatform::GetPlatform()->SupportsAzureContent());
  gfxImageFormat format =
    gfxPlatform::GetPlatform()->OptimalFormatForContent(aType);

  *aBlackDT = gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
    IntSize(aRect.width, aRect.height),
    ImageFormatToSurfaceFormat(format));
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
  aClient = CreateDeprecatedTextureClient(TEXTURE_CONTENT, mContentType);
  MOZ_ASSERT(aClient, "Failed to create texture client");

  if (!aClient->EnsureAllocated(mSize, mContentType)) {
    aClient = CreateDeprecatedTextureClient(TEXTURE_FALLBACK, mContentType);
    MOZ_ASSERT(aClient, "Failed to create texture client");
    if (!aClient->EnsureAllocated(mSize, mContentType)) {
      NS_WARNING("Could not allocate texture client");
      aClient->SetFlags(0);
      aClient = nullptr;
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

  if (mDeprecatedTextureClient) {
    mOldTextures.AppendElement(mDeprecatedTextureClient);
    if (mDeprecatedTextureClientOnWhite) {
      mOldTextures.AppendElement(mDeprecatedTextureClientOnWhite);
    }
    DestroyBuffers();
  }

  mContentType = aType;
  mSize = gfx::IntSize(aRect.width, aRect.height);
  mTextureInfo.mTextureFlags = aFlags & ~TEXTURE_DEALLOCATE_CLIENT;

  if (!CreateAndAllocateDeprecatedTextureClient(mDeprecatedTextureClient)) {
    return;
  }
  
  if (aFlags & BUFFER_COMPONENT_ALPHA) {
    if (!CreateAndAllocateDeprecatedTextureClient(mDeprecatedTextureClientOnWhite)) {
      mDeprecatedTextureClient->SetFlags(0);
      mDeprecatedTextureClient = nullptr;
      return;
    }
    mTextureInfo.mTextureFlags |= TEXTURE_COMPONENT_ALPHA;
  }

  CreateFrontBufferAndNotify(aRect);
  mIsNewBuffer = true;
}

void
ContentClientRemoteBuffer::CreateBuffer(ContentType aType,
                                        const nsIntRect& aRect,
                                        uint32_t aFlags,
                                        RefPtr<gfx::DrawTarget>* aBlackDT,
                                        RefPtr<gfx::DrawTarget>* aWhiteDT)
{
  BuildDeprecatedTextureClients(aType, aRect, aFlags);
  if (!mDeprecatedTextureClient) {
    return;
  }

  MOZ_ASSERT(gfxPlatform::GetPlatform()->SupportsAzureContentForType(
        mDeprecatedTextureClient->BackendType()));
  *aBlackDT = mDeprecatedTextureClient->LockDrawTarget();
  if (aFlags & BUFFER_COMPONENT_ALPHA) {
    *aWhiteDT = mDeprecatedTextureClientOnWhite->LockDrawTarget();
  }
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


void
ContentClientRemoteBuffer::OnActorDestroy()
{
  if (mDeprecatedTextureClient) {
    mDeprecatedTextureClient->OnActorDestroy();
  }
  if (mDeprecatedTextureClientOnWhite) {
    mDeprecatedTextureClientOnWhite->OnActorDestroy();
  }
  for (size_t i = 0; i < mOldTextures.Length(); ++i) {
    mOldTextures[i]->OnActorDestroy();
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
    mDeprecatedTextureClient->SetFlags(0);
    mDeprecatedTextureClient = nullptr;
    if (mDeprecatedTextureClientOnWhite) {
      mDeprecatedTextureClientOnWhite->SetFlags(0);
      mDeprecatedTextureClientOnWhite = nullptr;
    }
    return;
  }

  if (mTextureInfo.mTextureFlags & TEXTURE_COMPONENT_ALPHA) {
    if (!CreateAndAllocateDeprecatedTextureClient(mFrontClientOnWhite)) {
      mDeprecatedTextureClient->SetFlags(0);
      mDeprecatedTextureClient = nullptr;
      mDeprecatedTextureClientOnWhite->SetFlags(0);
      mDeprecatedTextureClientOnWhite = nullptr;
      mFrontClient->SetFlags(0);
      mFrontClient = nullptr;
      return;
    }
  }

  mFrontBufferRect = aBufferRect;
  mFrontBufferRotation = nsIntPoint();
  
  mForwarder->CreatedDoubleBuffer(this,
                                  *mFrontClient->LockSurfaceDescriptor(),
                                  *mDeprecatedTextureClient->LockSurfaceDescriptor(),
                                  mTextureInfo,
                                  mFrontClientOnWhite ? mFrontClientOnWhite->LockSurfaceDescriptor() : nullptr,
                                  mDeprecatedTextureClientOnWhite ? mDeprecatedTextureClientOnWhite->LockSurfaceDescriptor() : nullptr);
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

void
ContentClientDoubleBuffered::OnActorDestroy()
{
  if (mDeprecatedTextureClient) {
    mDeprecatedTextureClient->OnActorDestroy();
  }
  if (mDeprecatedTextureClientOnWhite) {
    mDeprecatedTextureClientOnWhite->OnActorDestroy();
  }
  for (size_t i = 0; i < mOldTextures.Length(); ++i) {
    mOldTextures[i]->OnActorDestroy();
  }
  if (mFrontClient) {
    mFrontClient->OnActorDestroy();
  }
  if (mFrontClientOnWhite) {
    mFrontClientOnWhite->OnActorDestroy();
  }
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
  mIsNewBuffer = false;

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

  // This is a tricky trade off, we're going to get stuff out of our
  // frontbuffer now, but the next PaintThebes might throw it all (or mostly)
  // away if the visible region has changed. This is why in reality we want
  // this code integrated with PaintThebes to always do the optimal thing.

  if (mDidSelfCopy) {
    mDidSelfCopy = false;
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
  // We need to ensure that we lock these two buffers in the same
  // order as the compositor to prevent deadlocks.
  DrawTarget* dt = autoTextureFront.GetDrawTarget(mFrontClient);
  DrawTarget* dtOnWhite = autoTextureFrontOnWhite.GetDrawTarget(mFrontClientOnWhite);
  RotatedBuffer frontBuffer(dt,
                            dtOnWhite,
                            mFrontBufferRect,
                            mFrontBufferRotation);
  UpdateDestinationFrom(frontBuffer, updateRegion);

  mFrontAndBackBufferDiffer = false;
}

void
ContentClientDoubleBuffered::UpdateDestinationFrom(const RotatedBuffer& aSource,
                                                   const nsIntRegion& aUpdateRegion)
{
  nsRefPtr<gfxContext> destCtx =
    GetContextForQuadrantUpdate(aUpdateRegion.GetBounds(), BUFFER_BLACK);
  if (!destCtx) {
    return;
  }
  destCtx->SetOperator(gfxContext::OPERATOR_SOURCE);

  bool isClippingCheap = IsClippingCheap(destCtx, aUpdateRegion);
  if (isClippingCheap) {
    gfxUtils::ClipToRegion(destCtx, aUpdateRegion);
  }

  aSource.DrawBufferWithRotation(destCtx->GetDrawTarget(), BUFFER_BLACK, 1.0, OP_SOURCE);

  if (aSource.HaveBufferOnWhite()) {
    MOZ_ASSERT(HaveBufferOnWhite());
    nsRefPtr<gfxContext> destCtx =
      GetContextForQuadrantUpdate(aUpdateRegion.GetBounds(), BUFFER_WHITE);
    destCtx->SetOperator(gfxContext::OPERATOR_SOURCE);

    bool isClippingCheap = IsClippingCheap(destCtx, aUpdateRegion);
    if (isClippingCheap) {
      gfxUtils::ClipToRegion(destCtx, aUpdateRegion);
    }

    aSource.DrawBufferWithRotation(destCtx->GetDrawTarget(), BUFFER_WHITE, 1.0, OP_SOURCE);
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
                                  *mDeprecatedTextureClient->LockSurfaceDescriptor(),
                                  mTextureInfo,
                                  mDeprecatedTextureClientOnWhite ? mDeprecatedTextureClientOnWhite->LockSurfaceDescriptor() : nullptr);
}

void
ContentClientSingleBuffered::SyncFrontBufferToBackBuffer()
{
  mIsNewBuffer = false;
  if (!mFrontAndBackBufferDiffer) {
    return;
  }
  mFrontAndBackBufferDiffer = false;

  DrawTarget* backBuffer = GetDTBuffer();
  if (!backBuffer && mDeprecatedTextureClient) {
    backBuffer = mDeprecatedTextureClient->LockDrawTarget();
  }
  if (!backBuffer) {
    NS_WARNING("Could not lock texture client");
    return;
  }

  RefPtr<DrawTarget> oldBuffer;
  oldBuffer = SetDTBuffer(backBuffer,
                          mBufferRect,
                          mBufferRotation);

  backBuffer = GetDTBufferOnWhite();
  if (!backBuffer && mDeprecatedTextureClientOnWhite) {
    backBuffer = mDeprecatedTextureClientOnWhite->LockDrawTarget();
  }
  if (!backBuffer) {
    NS_WARNING("Could not lock texture client (on white)");
    return;
  }

  oldBuffer = SetDTBufferOnWhite(backBuffer);
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

RotatedContentBuffer::PaintState
ContentClientIncremental::BeginPaintBuffer(ThebesLayer* aLayer,
                                           RotatedContentBuffer::ContentType aContentType,
                                           uint32_t aFlags)
{
  mTextureInfo.mDeprecatedTextureHostFlags = 0;
  PaintState result;
  // We need to disable rotation if we're going to be resampled when
  // drawing, because we might sample across the rotation boundary.
  bool canHaveRotation =  !(aFlags & RotatedContentBuffer::PAINT_WILL_RESAMPLE);

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
      (!(aFlags & RotatedContentBuffer::PAINT_WILL_RESAMPLE) ||
       !(mTextureInfo.mTextureFlags & TEXTURE_ALLOW_REPEAT));

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
      if (!gfxPlatform::ComponentAlphaEnabled() ||
          !aLayer->GetParent() ||
          !aLayer->GetParent()->SupportsComponentAlphaChildren()) {
        mode = Layer::SURFACE_SINGLE_CHANNEL_ALPHA;
      } else {
        contentType = GFX_CONTENT_COLOR;
      }
    }

    if ((aFlags & RotatedContentBuffer::PAINT_WILL_RESAMPLE) &&
        (!neededRegion.GetBounds().IsEqualInterior(destBufferRect) ||
         neededRegion.GetNumRects() > 1)) {
      // The area we add to neededRegion might not be painted opaquely
      if (mode == Layer::SURFACE_OPAQUE) {
        contentType = GFX_CONTENT_COLOR_ALPHA;
        mode = Layer::SURFACE_SINGLE_CHANNEL_ALPHA;
      }
      // For component alpha layers, we leave contentType as GFX_CONTENT_COLOR.

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

  uint32_t bufferFlags = canHaveRotation ? TEXTURE_ALLOW_REPEAT : 0;
  if (mode == Layer::SURFACE_COMPONENT_ALPHA) {
    bufferFlags |= TEXTURE_COMPONENT_ALPHA;
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
  NS_ASSERTION(!(aFlags & RotatedContentBuffer::PAINT_WILL_RESAMPLE) ||
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
      MOZ_ASSERT(gfxPlatform::GetPlatform()->SupportsAzureContent());
      RefPtr<DrawTarget> onBlackDT = gfxPlatform::GetPlatform()->CreateDrawTargetForUpdateSurface(onBlack, onBlack->GetSize());
      RefPtr<DrawTarget> onWhiteDT = gfxPlatform::GetPlatform()->CreateDrawTargetForUpdateSurface(onWhite, onWhite->GetSize());
      RefPtr<DrawTarget> dt = Factory::CreateDualDrawTarget(onBlackDT, onWhiteDT);
      result.mContext = new gfxContext(dt);
    } else {
      result.mContext = nullptr;
    }
  } else {
    nsRefPtr<gfxASurface> surf = GetUpdateSurface(BUFFER_BLACK, result.mRegionToDraw);
    MOZ_ASSERT(gfxPlatform::GetPlatform()->SupportsAzureContent());
    RefPtr<DrawTarget> dt = gfxPlatform::GetPlatform()->CreateDrawTargetForUpdateSurface(surf, surf->GetSize());
    result.mContext = new gfxContext(dt);
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
  result.mClip = CLIP_DRAW;

  if (mContentType == GFX_CONTENT_COLOR_ALPHA) {
    result.mContext->Save();
    gfxUtils::ClipToRegion(result.mContext, result.mRegionToDraw);
    result.mContext->SetOperator(gfxContext::OPERATOR_CLEAR);
    result.mContext->Paint();
    result.mContext->Restore();
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
