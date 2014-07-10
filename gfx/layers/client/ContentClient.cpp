/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/ContentClient.h"
#include "BasicLayers.h"                // for BasicLayerManager
#include "CompositorChild.h"            // for CompositorChild
#include "gfxColor.h"                   // for gfxRGBA
#include "gfxContext.h"                 // for gfxContext, etc
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxPrefs.h"                   // for gfxPrefs
#include "gfxPoint.h"                   // for gfxIntSize, gfxPoint
#include "gfxTeeSurface.h"              // for gfxTeeSurface
#include "gfxUtils.h"                   // for gfxUtils
#include "ipc/ShadowLayers.h"           // for ShadowLayerForwarder
#include "mozilla/ArrayUtils.h"         // for ArrayLength
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
#include "nsLayoutUtils.h"
#ifdef XP_WIN
#include "gfxWindowsPlatform.h"
#endif
#include "gfx2DGlue.h"

namespace mozilla {

using namespace gfx;

namespace layers {

static TextureFlags TextureFlagsForRotatedContentBufferFlags(uint32_t aBufferFlags)
{
  TextureFlags result = TextureFlags::NO_FLAGS;

  if (aBufferFlags & RotatedContentBuffer::BUFFER_COMPONENT_ALPHA) {
    result |= TextureFlags::COMPONENT_ALPHA;
  }

  if (aBufferFlags & RotatedContentBuffer::ALLOW_REPEAT) {
    result |= TextureFlags::ALLOW_REPEAT;
  }

  return result;
}

/* static */ TemporaryRef<ContentClient>
ContentClient::CreateContentClient(CompositableForwarder* aForwarder)
{
  LayersBackend backend = aForwarder->GetCompositorBackendType();
  if (backend != LayersBackend::LAYERS_OPENGL &&
      backend != LayersBackend::LAYERS_D3D9 &&
      backend != LayersBackend::LAYERS_D3D11 &&
      backend != LayersBackend::LAYERS_BASIC) {
    return nullptr;
  }

  bool useDoubleBuffering = false;

#ifdef XP_WIN
  if (backend == LayersBackend::LAYERS_D3D11) {
    useDoubleBuffering = !!gfxWindowsPlatform::GetPlatform()->GetD2DDevice();
  } else
#endif
  {
    useDoubleBuffering = (LayerManagerComposite::SupportsDirectTexturing() &&
                         backend != LayersBackend::LAYERS_D3D9) ||
                         backend == LayersBackend::LAYERS_BASIC;
  }

  if (useDoubleBuffering || PR_GetEnv("MOZ_FORCE_DOUBLE_BUFFERING")) {
    return new ContentClientDoubleBuffered(aForwarder);
  }
#ifdef XP_MACOSX
  if (backend == LayersBackend::LAYERS_OPENGL) {
    return new ContentClientIncremental(aForwarder);
  }
#endif
  return new ContentClientSingleBuffered(aForwarder);
}

void
ContentClient::EndPaint()
{
  // It is very important that this is called after any overridden EndPaint behaviour,
  // because destroying textures is a three stage process:
  // 1. We are done with the buffer and move it to ContentClient::mOldTextures,
  // that happens in DestroyBuffers which is may be called indirectly from
  // PaintThebes.
  // 2. The content client calls RemoveTextureClient on the texture clients in
  // mOldTextures and forgets them. They then become invalid. The compositable
  // client keeps a record of IDs. This happens in EndPaint.
  // 3. An IPC message is sent to destroy the corresponding texture host. That
  // happens from OnTransaction.
  // It is important that these steps happen in order.
  OnTransaction();
}

// We pass a null pointer for the ContentClient Forwarder argument, which means
// this client will not have a ContentHost on the other side.
ContentClientBasic::ContentClientBasic()
  : ContentClient(nullptr)
  , RotatedContentBuffer(ContainsVisibleBounds)
{}

void
ContentClientBasic::CreateBuffer(ContentType aType,
                                 const nsIntRect& aRect,
                                 uint32_t aFlags,
                                 RefPtr<gfx::DrawTarget>* aBlackDT,
                                 RefPtr<gfx::DrawTarget>* aWhiteDT)
{
  MOZ_ASSERT(!(aFlags & BUFFER_COMPONENT_ALPHA));
  gfxImageFormat format =
    gfxPlatform::GetPlatform()->OptimalFormatForContent(aType);

  *aBlackDT = gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
    IntSize(aRect.width, aRect.height),
    ImageFormatToSurfaceFormat(format));
}

void
ContentClientRemoteBuffer::DestroyBuffers()
{
  if (!mTextureClient) {
    return;
  }

  mOldTextures.AppendElement(mTextureClient);
  mTextureClient = nullptr;
  if (mTextureClientOnWhite) {
    mOldTextures.AppendElement(mTextureClientOnWhite);
    mTextureClientOnWhite = nullptr;
  }

  DestroyFrontBuffer();
}

void
ContentClientRemoteBuffer::BeginPaint()
{
  EnsureBackBufferIfFrontBuffer();

  // XXX: So we might not have a TextureClient yet.. because it will
  // only be created by CreateBuffer.. which will deliver a locked surface!.
  if (mTextureClient) {
    SetBufferProvider(mTextureClient);
  }
  if (mTextureClientOnWhite) {
    SetBufferProviderOnWhite(mTextureClientOnWhite);
  }
}

void
ContentClientRemoteBuffer::EndPaint()
{
  // XXX: We might still not have a texture client if PaintThebes
  // decided we didn't need one yet because the region to draw was empty.
  SetBufferProvider(nullptr);
  SetBufferProviderOnWhite(nullptr);
  for (unsigned i = 0; i< mOldTextures.Length(); ++i) {
    if (mOldTextures[i]->IsLocked()) {
      mOldTextures[i]->Unlock();
    }
  }
  mOldTextures.Clear();

  if (mTextureClient && mTextureClient->IsLocked()) {
    mTextureClient->Unlock();
  }
  if (mTextureClientOnWhite && mTextureClientOnWhite->IsLocked()) {
    mTextureClientOnWhite->Unlock();
  }
  ContentClientRemote::EndPaint();
}

bool
ContentClientRemoteBuffer::CreateAndAllocateTextureClient(RefPtr<TextureClient>& aClient,
                                                          TextureFlags aFlags)
{
  TextureAllocationFlags allocFlags = TextureAllocationFlags::ALLOC_CLEAR_BUFFER;
  if (aFlags & TextureFlags::ON_WHITE) {
    allocFlags = TextureAllocationFlags::ALLOC_CLEAR_BUFFER_WHITE;
  }

  // gfx::BackendType::NONE means fallback to the content backend
  aClient = CreateTextureClientForDrawing(mSurfaceFormat, mSize,
                                          gfx::BackendType::NONE,
                                          mTextureInfo.mTextureFlags | aFlags,
                                          allocFlags);
  if (!aClient) {
    // try with ALLOC_FALLBACK
    aClient = CreateTextureClientForDrawing(mSurfaceFormat, mSize,
                                            gfx::BackendType::NONE,
                                            mTextureInfo.mTextureFlags
                                            | TextureFlags::ALLOC_FALLBACK
                                            | aFlags,
                                            allocFlags);
  }

  if (!aClient) {
    return false;
  }

  NS_WARN_IF_FALSE(aClient->IsValid(), "Created an invalid texture client");
  return true;
}

void
ContentClientRemoteBuffer::BuildTextureClients(SurfaceFormat aFormat,
                                               const nsIntRect& aRect,
                                               uint32_t aFlags)
{
  // If we hit this assertion, then it might be due to an empty transaction
  // followed by a real transaction. Our buffers should be created (but not
  // painted in the empty transaction) and then painted (but not created) in the
  // real transaction. That is kind of fragile, and this assert will catch
  // circumstances where we screw that up, e.g., by unnecessarily recreating our
  // buffers.
  NS_ABORT_IF_FALSE(!mIsNewBuffer,
                    "Bad! Did we create a buffer twice without painting?");

  mIsNewBuffer = true;

  DestroyBuffers();

  mSurfaceFormat = aFormat;
  mSize = gfx::IntSize(aRect.width, aRect.height);
  mTextureInfo.mTextureFlags = TextureFlagsForRotatedContentBufferFlags(aFlags);

  if (aFlags & BUFFER_COMPONENT_ALPHA) {
    mTextureInfo.mTextureFlags |= TextureFlags::COMPONENT_ALPHA;
  }

  CreateBackBuffer(mBufferRect);
}

void
ContentClientRemoteBuffer::CreateBackBuffer(const nsIntRect& aBufferRect)
{
  if (!CreateAndAllocateTextureClient(mTextureClient, TextureFlags::ON_BLACK) ||
    !AddTextureClient(mTextureClient)) {
    AbortTextureClientCreation();
    return;
  }
  if (mTextureInfo.mTextureFlags & TextureFlags::COMPONENT_ALPHA) {
    if (!CreateAndAllocateTextureClient(mTextureClientOnWhite, TextureFlags::ON_WHITE) ||
      !AddTextureClient(mTextureClientOnWhite)) {
      AbortTextureClientCreation();
      return;
    }
  }
}

void
ContentClientRemoteBuffer::CreateBuffer(ContentType aType,
                                        const nsIntRect& aRect,
                                        uint32_t aFlags,
                                        RefPtr<gfx::DrawTarget>* aBlackDT,
                                        RefPtr<gfx::DrawTarget>* aWhiteDT)
{
  BuildTextureClients(gfxPlatform::GetPlatform()->Optimal2DFormatForContent(aType), aRect, aFlags);
  if (!mTextureClient) {
    return;
  }

  // We just created the textures and we are about to get their draw targets
  // so we have to lock them here.
  DebugOnly<bool> locked = mTextureClient->Lock(OpenMode::OPEN_READ_WRITE);
  MOZ_ASSERT(locked, "Could not lock the TextureClient");

  *aBlackDT = mTextureClient->BorrowDrawTarget();
  if (aFlags & BUFFER_COMPONENT_ALPHA) {
    locked = mTextureClientOnWhite->Lock(OpenMode::OPEN_READ_WRITE);
    MOZ_ASSERT(locked, "Could not lock the second TextureClient for component alpha");

    *aWhiteDT = mTextureClientOnWhite->BorrowDrawTarget();
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
  NS_ABORT_IF_FALSE(mTextureClient, "should have a back buffer by now");

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

  MOZ_ASSERT(mTextureClient);
  if (mTextureClientOnWhite) {
    mForwarder->UseComponentAlphaTextures(this, mTextureClient,
                                          mTextureClientOnWhite);
  } else {
    mForwarder->UseTexture(this, mTextureClient);
  }
  mForwarder->UpdateTextureRegion(this,
                                  ThebesBufferData(BufferRect(),
                                                   BufferRotation()),
                                  updatedRegion);
}

void
ContentClientRemoteBuffer::SwapBuffers(const nsIntRegion& aFrontUpdatedRegion)
{
  mFrontAndBackBufferDiffer = true;
}

void
ContentClientDoubleBuffered::DestroyFrontBuffer()
{
  if (mFrontClient) {
    mOldTextures.AppendElement(mFrontClient);
    mFrontClient = nullptr;
  }

  if (mFrontClientOnWhite) {
    mOldTextures.AppendElement(mFrontClientOnWhite);
    mFrontClientOnWhite = nullptr;
  }
}

void
ContentClientDoubleBuffered::Updated(const nsIntRegion& aRegionToDraw,
                                     const nsIntRegion& aVisibleRegion,
                                     bool aDidSelfCopy)
{
  ContentClientRemoteBuffer::Updated(aRegionToDraw, aVisibleRegion, aDidSelfCopy);

#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
  if (mFrontClient) {
    // remove old buffer from CompositableHost
    RefPtr<AsyncTransactionTracker> tracker = new RemoveTextureFromCompositableTracker();
    // Hold TextureClient until transaction complete.
    tracker->SetTextureClient(mFrontClient);
    mFrontClient->SetRemoveFromCompositableTracker(tracker);
    // RemoveTextureFromCompositableAsync() expects CompositorChild's presence.
    GetForwarder()->RemoveTextureFromCompositableAsync(tracker, this, mFrontClient);
  }

  if (mFrontClientOnWhite) {
    // remove old buffer from CompositableHost
    RefPtr<AsyncTransactionTracker> tracker = new RemoveTextureFromCompositableTracker();
    // Hold TextureClient until transaction complete.
    tracker->SetTextureClient(mFrontClientOnWhite);
    mFrontClientOnWhite->SetRemoveFromCompositableTracker(tracker);
    // RemoveTextureFromCompositableAsync() expects CompositorChild's presence.
    GetForwarder()->RemoveTextureFromCompositableAsync(tracker, this, mFrontClientOnWhite);
  }
#endif
}

void
ContentClientDoubleBuffered::SwapBuffers(const nsIntRegion& aFrontUpdatedRegion)
{
  mFrontUpdatedRegion = aFrontUpdatedRegion;

  RefPtr<TextureClient> oldBack = mTextureClient;
  mTextureClient = mFrontClient;
  mFrontClient = oldBack;

  oldBack = mTextureClientOnWhite;
  mTextureClientOnWhite = mFrontClientOnWhite;
  mFrontClientOnWhite = oldBack;

  nsIntRect oldBufferRect = mBufferRect;
  mBufferRect = mFrontBufferRect;
  mFrontBufferRect = oldBufferRect;

  nsIntPoint oldBufferRotation = mBufferRotation;
  mBufferRotation = mFrontBufferRotation;
  mFrontBufferRotation = oldBufferRotation;

  MOZ_ASSERT(mFrontClient);

  ContentClientRemoteBuffer::SwapBuffers(aFrontUpdatedRegion);
}

void
ContentClientDoubleBuffered::BeginPaint()
{
  ContentClientRemoteBuffer::BeginPaint();

  mIsNewBuffer = false;

  if (!mFrontAndBackBufferDiffer) {
    return;
  }

  if (mDidSelfCopy) {
    // We can't easily draw our front buffer into us, since we're going to be
    // copying stuff around anyway it's easiest if we just move our situation
    // to non-rotated while we're at it. If this situation occurs we'll have
    // hit a self-copy path in PaintThebes before as well anyway.
    mBufferRect.MoveTo(mFrontBufferRect.TopLeft());
    mBufferRotation = nsIntPoint();
    return;
  }
  mBufferRect = mFrontBufferRect;
  mBufferRotation = mFrontBufferRotation;
}

// Sync front/back buffers content
// After executing, the new back buffer has the same (interesting) pixels as
// the new front buffer, and mValidRegion et al. are correct wrt the new
// back buffer (i.e. as they were for the old back buffer)
void
ContentClientDoubleBuffered::FinalizeFrame(const nsIntRegion& aRegionToDraw)
{
  if (mTextureClient) {
    DebugOnly<bool> locked = mTextureClient->Lock(OpenMode::OPEN_READ_WRITE);
    MOZ_ASSERT(locked);
  }
  if (mTextureClientOnWhite) {
    DebugOnly<bool> locked = mTextureClientOnWhite->Lock(OpenMode::OPEN_READ_WRITE);
    MOZ_ASSERT(locked);
  }

  if (!mFrontAndBackBufferDiffer) {
    MOZ_ASSERT(!mDidSelfCopy, "If we have to copy the world, then our buffers are different, right?");
    return;
  }
  MOZ_ASSERT(mFrontClient);
  if (!mFrontClient) {
    return;
  }

  MOZ_LAYERS_LOG(("BasicShadowableThebes(%p): reading back <x=%d,y=%d,w=%d,h=%d>",
                  this,
                  mFrontUpdatedRegion.GetBounds().x,
                  mFrontUpdatedRegion.GetBounds().y,
                  mFrontUpdatedRegion.GetBounds().width,
                  mFrontUpdatedRegion.GetBounds().height));

  mFrontAndBackBufferDiffer = false;

  nsIntRegion updateRegion = mFrontUpdatedRegion;
  if (mDidSelfCopy) {
    mDidSelfCopy = false;
    updateRegion = mBufferRect;
  }

  // No point in sync'ing what we are going to draw over anyway. And if there is
  // nothing to sync at all, there is nothing to do and we can go home early.
  updateRegion.Sub(updateRegion, aRegionToDraw);
  if (updateRegion.IsEmpty()) {
    return;
  }

  // We need to ensure that we lock these two buffers in the same
  // order as the compositor to prevent deadlocks.
  if (!mFrontClient->Lock(OpenMode::OPEN_READ_ONLY)) {
    return;
  }
  if (mFrontClientOnWhite &&
      !mFrontClientOnWhite->Lock(OpenMode::OPEN_READ_ONLY)) {
    mFrontClient->Unlock();
    return;
  }
  {
    // Restrict the DrawTargets and frontBuffer to a scope to make
    // sure there is no more external references to the DrawTargets
    // when we Unlock the TextureClients.
    RefPtr<DrawTarget> dt = mFrontClient->BorrowDrawTarget();
    RefPtr<DrawTarget> dtOnWhite = mFrontClientOnWhite
      ? mFrontClientOnWhite->BorrowDrawTarget()
      : nullptr;
    RotatedBuffer frontBuffer(dt,
                              dtOnWhite,
                              mFrontBufferRect,
                              mFrontBufferRotation);
    UpdateDestinationFrom(frontBuffer, updateRegion);
  }

  mFrontClient->Unlock();
  if (mFrontClientOnWhite) {
    mFrontClientOnWhite->Unlock();
  }
}

void
ContentClientDoubleBuffered::EnsureBackBufferIfFrontBuffer()
{
  if (!mTextureClient && mFrontClient) {
    CreateBackBuffer(mFrontBufferRect);

    mBufferRect = mFrontBufferRect;
    mBufferRotation = mFrontBufferRotation;
  }
}

void
ContentClientDoubleBuffered::UpdateDestinationFrom(const RotatedBuffer& aSource,
                                                   const nsIntRegion& aUpdateRegion)
{
  DrawIterator iter;
  while (DrawTarget* destDT =
    BorrowDrawTargetForQuadrantUpdate(aUpdateRegion.GetBounds(), BUFFER_BLACK, &iter)) {
    bool isClippingCheap = IsClippingCheap(destDT, iter.mDrawRegion);
    if (isClippingCheap) {
      gfxUtils::ClipToRegion(destDT, iter.mDrawRegion);
    }

    aSource.DrawBufferWithRotation(destDT, BUFFER_BLACK, 1.0, CompositionOp::OP_SOURCE);
    if (isClippingCheap) {
      destDT->PopClip();
    }
    // Flush the destination before the sources become inaccessible (Unlock).
    destDT->Flush();
    ReturnDrawTargetToBuffer(destDT);
  }

  if (aSource.HaveBufferOnWhite()) {
    MOZ_ASSERT(HaveBufferOnWhite());
    DrawIterator whiteIter;
    while (DrawTarget* destDT =
      BorrowDrawTargetForQuadrantUpdate(aUpdateRegion.GetBounds(), BUFFER_WHITE, &whiteIter)) {
      bool isClippingCheap = IsClippingCheap(destDT, whiteIter.mDrawRegion);
      if (isClippingCheap) {
        gfxUtils::ClipToRegion(destDT, whiteIter.mDrawRegion);
      }

      aSource.DrawBufferWithRotation(destDT, BUFFER_WHITE, 1.0, CompositionOp::OP_SOURCE);
      if (isClippingCheap) {
        destDT->PopClip();
      }
      // Flush the destination before the sources become inaccessible (Unlock).
      destDT->Flush();
      ReturnDrawTargetToBuffer(destDT);
    }
  }
}

void
ContentClientSingleBuffered::FinalizeFrame(const nsIntRegion& aRegionToDraw)
{
  if (mTextureClient) {
    DebugOnly<bool> locked = mTextureClient->Lock(OpenMode::OPEN_READ_WRITE);
    MOZ_ASSERT(locked);
  }
  if (mTextureClientOnWhite) {
    DebugOnly<bool> locked = mTextureClientOnWhite->Lock(OpenMode::OPEN_READ_WRITE);
    MOZ_ASSERT(locked);
  }
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
FillSurface(DrawTarget* aDT, const nsIntRegion& aRegion,
            const nsIntPoint& aOffset, const gfxRGBA& aColor)
{
  nsIntRegionRectIterator iter(aRegion);
  const nsIntRect* r;
  while ((r = iter.Next()) != nullptr) {
    aDT->FillRect(Rect(r->x - aOffset.x, r->y - aOffset.y,
                       r->width, r->height),
                  ColorPattern(ToColor(aColor)));
  }
}

void
ContentClientIncremental::NotifyBufferCreated(ContentType aType, TextureFlags aFlags)
{
  mTextureInfo.mTextureFlags = aFlags;
  mContentType = aType;

  mForwarder->CreatedIncrementalBuffer(this,
                                        mTextureInfo,
                                        mBufferRect);

}

RotatedContentBuffer::PaintState
ContentClientIncremental::BeginPaintBuffer(ThebesLayer* aLayer,
                                           uint32_t aFlags)
{
  mTextureInfo.mDeprecatedTextureHostFlags = DeprecatedTextureHostFlags::DEFAULT;
  PaintState result;
  // We need to disable rotation if we're going to be resampled when
  // drawing, because we might sample across the rotation boundary.
  bool canHaveRotation =  !(aFlags & RotatedContentBuffer::PAINT_WILL_RESAMPLE);

  nsIntRegion validRegion = aLayer->GetValidRegion();

  bool canUseOpaqueSurface = aLayer->CanUseOpaqueSurface();
  ContentType contentType =
    canUseOpaqueSurface ? gfxContentType::COLOR :
                          gfxContentType::COLOR_ALPHA;

  SurfaceMode mode;
  nsIntRegion neededRegion;
  bool canReuseBuffer;
  nsIntRect destBufferRect;

  while (true) {
    mode = aLayer->GetSurfaceMode();
    neededRegion = aLayer->GetVisibleRegion();
    // If we're going to resample, we need a buffer that's in clamp mode.
    canReuseBuffer = neededRegion.GetBounds().Size() <= mBufferRect.Size() &&
      mHasBuffer &&
      (!(aFlags & RotatedContentBuffer::PAINT_WILL_RESAMPLE) ||
       !(mTextureInfo.mTextureFlags & TextureFlags::ALLOW_REPEAT));

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

    if (mode == SurfaceMode::SURFACE_COMPONENT_ALPHA) {
      if (!gfxPrefs::ComponentAlphaEnabled() ||
          !aLayer->GetParent() ||
          !aLayer->GetParent()->SupportsComponentAlphaChildren()) {
        mode = SurfaceMode::SURFACE_SINGLE_CHANNEL_ALPHA;
      } else {
        contentType = gfxContentType::COLOR;
      }
    }

    if ((aFlags & RotatedContentBuffer::PAINT_WILL_RESAMPLE) &&
        (!neededRegion.GetBounds().IsEqualInterior(destBufferRect) ||
         neededRegion.GetNumRects() > 1)) {
      // The area we add to neededRegion might not be painted opaquely
      if (mode == SurfaceMode::SURFACE_OPAQUE) {
        contentType = gfxContentType::COLOR_ALPHA;
        mode = SurfaceMode::SURFACE_SINGLE_CHANNEL_ALPHA;
      }
      // For component alpha layers, we leave contentType as gfxContentType::COLOR.

      // We need to validate the entire buffer, to make sure that only valid
      // pixels are sampled
      neededRegion = destBufferRect;
    }

    if (mHasBuffer &&
        (mContentType != contentType ||
         (mode == SurfaceMode::SURFACE_COMPONENT_ALPHA) != mHasBufferOnWhite)) {
#ifdef MOZ_DUMP_PAINTING
      if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
        if (mContentType != contentType) {
          printf_stderr("Layer's content type has changed\n");
        }
        if ((mode == SurfaceMode::SURFACE_COMPONENT_ALPHA) != mHasBufferOnWhite) {
          printf_stderr("Layer's component alpha status has changed\n");
        }
        printf_stderr("Invalidating entire layer %p\n", aLayer);
      }
#endif
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

  TextureFlags bufferFlags = TextureFlags::NO_FLAGS;
  if (canHaveRotation) {
    bufferFlags |= TextureFlags::ALLOW_REPEAT;
  }
  if (mode == SurfaceMode::SURFACE_COMPONENT_ALPHA) {
    bufferFlags |= TextureFlags::COMPONENT_ALPHA;
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
        (mode != SurfaceMode::SURFACE_COMPONENT_ALPHA || mHasBufferOnWhite)) {
      mTextureInfo.mDeprecatedTextureHostFlags = DeprecatedTextureHostFlags::COPY_PREVIOUS;
    }

    mHasBuffer = true;
    if (mode == SurfaceMode::SURFACE_COMPONENT_ALPHA) {
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

  // If we do partial updates, we have to clip drawing to the regionToDraw.
  // If we don't clip, background images will be fillrect'd to the region correctly,
  // while text or lines will paint outside of the regionToDraw. This becomes apparent
  // with concave regions. Right now the scrollbars invalidate a narrow strip of the bar
  // although they never cover it. This leads to two draw rects, the narow strip and the actually
  // newly exposed area. It would be wise to fix this glitch in any way to have simpler
  // clip and draw regions.
  result.mClip = DrawRegionClip::DRAW;
  result.mMode = mode;

  return result;
}

DrawTarget*
ContentClientIncremental::BorrowDrawTargetForPainting(PaintState& aPaintState,
                                                      RotatedContentBuffer::DrawIterator* aIter)
{
  if (aPaintState.mMode == SurfaceMode::SURFACE_NONE) {
    return nullptr;
  }

  if (aIter) {
    if (aIter->mCount++ > 0) {
      return nullptr;
    }
    aIter->mDrawRegion = aPaintState.mRegionToDraw;
  }

  DrawTarget* result = nullptr;

  nsIntRect drawBounds = aPaintState.mRegionToDraw.GetBounds();
  MOZ_ASSERT(!mLoanedDrawTarget);

  // BeginUpdate is allowed to modify the given region,
  // if it wants more to be repainted than we request.
  if (aPaintState.mMode == SurfaceMode::SURFACE_COMPONENT_ALPHA) {
    nsIntRegion drawRegionCopy = aPaintState.mRegionToDraw;
    RefPtr<DrawTarget> onBlack = GetUpdateSurface(BUFFER_BLACK, drawRegionCopy);
    RefPtr<DrawTarget> onWhite = GetUpdateSurface(BUFFER_WHITE, aPaintState.mRegionToDraw);
    if (onBlack && onWhite) {
      NS_ASSERTION(aPaintState.mRegionToDraw == drawRegionCopy,
                   "BeginUpdate should always modify the draw region in the same way!");
      FillSurface(onBlack, aPaintState.mRegionToDraw, nsIntPoint(drawBounds.x, drawBounds.y), gfxRGBA(0.0, 0.0, 0.0, 1.0));
      FillSurface(onWhite, aPaintState.mRegionToDraw, nsIntPoint(drawBounds.x, drawBounds.y), gfxRGBA(1.0, 1.0, 1.0, 1.0));
      mLoanedDrawTarget = Factory::CreateDualDrawTarget(onBlack, onWhite);
    } else {
      mLoanedDrawTarget = nullptr;
    }
  } else {
    mLoanedDrawTarget = GetUpdateSurface(BUFFER_BLACK, aPaintState.mRegionToDraw);
  }
  if (!mLoanedDrawTarget) {
    NS_WARNING("unable to get context for update");
    return nullptr;
  }

  result = mLoanedDrawTarget;
  mLoanedTransform = mLoanedDrawTarget->GetTransform();
  mLoanedTransform.Translate(-drawBounds.x, -drawBounds.y);
  result->SetTransform(mLoanedTransform);
  mLoanedTransform.Translate(drawBounds.x, drawBounds.y);

  if (mContentType == gfxContentType::COLOR_ALPHA) {
    gfxUtils::ClipToRegion(result, aPaintState.mRegionToDraw);
    nsIntRect bounds = aPaintState.mRegionToDraw.GetBounds();
    result->ClearRect(Rect(bounds.x, bounds.y, bounds.width, bounds.height));
  }

  return result;
}

void
ContentClientIncremental::Updated(const nsIntRegion& aRegionToDraw,
                                  const nsIntRegion& aVisibleRegion,
                                  bool aDidSelfCopy)
{
  if (IsSurfaceDescriptorValid(mUpdateDescriptor)) {
    mForwarder->UpdateTextureIncremental(this,
                                         TextureIdentifier::Front,
                                         mUpdateDescriptor,
                                         aRegionToDraw,
                                         mBufferRect,
                                         mBufferRotation);
    mUpdateDescriptor = SurfaceDescriptor();
  }
  if (IsSurfaceDescriptorValid(mUpdateDescriptorOnWhite)) {
    mForwarder->UpdateTextureIncremental(this,
                                         TextureIdentifier::OnWhiteFront,
                                         mUpdateDescriptorOnWhite,
                                         aRegionToDraw,
                                         mBufferRect,
                                         mBufferRotation);
    mUpdateDescriptorOnWhite = SurfaceDescriptor();
  }

}

TemporaryRef<DrawTarget>
ContentClientIncremental::GetUpdateSurface(BufferType aType,
                                           const nsIntRegion& aUpdateRegion)
{
  nsIntRect rgnSize = aUpdateRegion.GetBounds();
  if (!mBufferRect.Contains(rgnSize)) {
    NS_ERROR("update outside of image");
    return nullptr;
  }
  SurfaceDescriptor desc;
  if (!mForwarder->AllocSurfaceDescriptor(rgnSize.Size().ToIntSize(),
                                          mContentType,
                                          &desc)) {
    NS_WARNING("creating SurfaceDescriptor failed!");
    Clear();
    return nullptr;
  }

  if (aType == BUFFER_BLACK) {
    MOZ_ASSERT(!IsSurfaceDescriptorValid(mUpdateDescriptor));
    mUpdateDescriptor = desc;
  } else {
    MOZ_ASSERT(!IsSurfaceDescriptorValid(mUpdateDescriptorOnWhite));
    MOZ_ASSERT(aType == BUFFER_WHITE);
    mUpdateDescriptorOnWhite = desc;
  }

  return GetDrawTargetForDescriptor(desc, gfx::BackendType::COREGRAPHICS);
}

}
}
