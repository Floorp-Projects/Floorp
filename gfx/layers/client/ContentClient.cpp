/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/ContentClient.h"
#include "BasicLayers.h"                // for BasicLayerManager
#include "gfxContext.h"                 // for gfxContext, etc
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxEnv.h"                     // for gfxEnv
#include "gfxPrefs.h"                   // for gfxPrefs
#include "gfxPoint.h"                   // for IntSize, gfxPoint
#include "gfxUtils.h"                   // for gfxUtils
#include "ipc/ShadowLayers.h"           // for ShadowLayerForwarder
#include "mozilla/ArrayUtils.h"         // for ArrayLength
#include "mozilla/gfx/2D.h"             // for DrawTarget, Factory
#include "mozilla/gfx/BasePoint.h"      // for BasePoint
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/gfx/Types.h"
#include "mozilla/layers/CompositorBridgeChild.h" // for CompositorBridgeChild
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/LayersMessages.h"  // for ThebesBufferData
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/PaintThread.h"
#include "nsDebug.h"                    // for NS_ASSERTION, NS_WARNING, etc
#include "nsISupportsImpl.h"            // for gfxContext::Release, etc
#include "nsIWidget.h"                  // for nsIWidget
#include "nsLayoutUtils.h"
#ifdef XP_WIN
#include "gfxWindowsPlatform.h"
#endif
#ifdef MOZ_WIDGET_GTK
#include "gfxPlatformGtk.h"
#endif
#include "ReadbackLayer.h"

#include <utility>
#include <vector>

using namespace std;

namespace mozilla {

using namespace gfx;

namespace layers {

static TextureFlags TextureFlagsForContentClientFlags(uint32_t aBufferFlags)
{
  TextureFlags result = TextureFlags::NO_FLAGS;

  if (aBufferFlags & ContentClient::BUFFER_COMPONENT_ALPHA) {
    result |= TextureFlags::COMPONENT_ALPHA;
  }

  return result;
}

static IntRect
ComputeBufferRect(const IntRect& aRequestedRect)
{
  IntRect rect(aRequestedRect);
  // Set a minimum width to guarantee a minimum size of buffers we
  // allocate (and work around problems on some platforms with smaller
  // dimensions). 64 used to be the magic number needed to work around
  // a rendering glitch on b2g (see bug 788411). Now that we don't support
  // this device anymore we should be fine with 8 pixels as the minimum.
  rect.SetWidth(std::max(aRequestedRect.Width(), 8));
  return rect;
}

/* static */ already_AddRefed<ContentClient>
ContentClient::CreateContentClient(CompositableForwarder* aForwarder)
{
  LayersBackend backend = aForwarder->GetCompositorBackendType();
  if (backend != LayersBackend::LAYERS_OPENGL &&
      backend != LayersBackend::LAYERS_D3D11 &&
      backend != LayersBackend::LAYERS_WR &&
      backend != LayersBackend::LAYERS_BASIC) {
    return nullptr;
  }

  bool useDoubleBuffering = false;

#ifdef XP_WIN
  if (backend == LayersBackend::LAYERS_D3D11) {
    useDoubleBuffering = gfxWindowsPlatform::GetPlatform()->IsDirect2DBackend();
  } else
#endif
#ifdef MOZ_WIDGET_GTK
  // We can't use double buffering when using image content with
  // Xrender support on Linux, as ContentHostDoubleBuffered is not
  // suited for direct uploads to the server.
  if (!gfxPlatformGtk::GetPlatform()->UseImageOffscreenSurfaces() ||
      !gfxVars::UseXRender())
#endif
  {
    useDoubleBuffering = backend == LayersBackend::LAYERS_BASIC;
  }

  if (useDoubleBuffering || gfxEnv::ForceDoubleBuffering()) {
    return MakeAndAddRef<ContentClientDoubleBuffered>(aForwarder);
  }
  return MakeAndAddRef<ContentClientSingleBuffered>(aForwarder);
}

void
ContentClient::Clear()
{
  mBuffer = nullptr;
}

ContentClient::PaintState
ContentClient::BeginPaint(PaintedLayer* aLayer,
                          uint32_t aFlags)
{
  PaintState result;

  BufferDecision dest = CalculateBufferForPaint(aLayer, aFlags);
  result.mContentType = dest.mBufferContentType;

  if (!dest.mCanKeepBufferContents) {
    // We're effectively clearing the valid region, so we need to draw
    // the entire needed region now.
    MOZ_ASSERT(!dest.mCanReuseBuffer);
    MOZ_ASSERT(dest.mValidRegion.IsEmpty());

    result.mRegionToInvalidate = aLayer->GetValidRegion();

#if defined(MOZ_DUMP_PAINTING)
    if (nsLayoutUtils::InvalidationDebuggingIsEnabled()) {
      if (result.mContentType != mBuffer->GetContentType()) {
        printf_stderr("Invalidating entire rotated buffer (layer %p): content type changed\n", aLayer);
      } else if ((dest.mBufferMode == SurfaceMode::SURFACE_COMPONENT_ALPHA) != mBuffer->HaveBufferOnWhite()) {
        printf_stderr("Invalidating entire rotated buffer (layer %p): component alpha changed\n", aLayer);
      }
    }
#endif
    Clear();
  }

  result.mRegionToDraw.Sub(dest.mNeededRegion,
                           dest.mValidRegion);

  if (result.mRegionToDraw.IsEmpty())
    return result;

  // We need to disable rotation if we're going to be resampled when
  // drawing, because we might sample across the rotation boundary.
  // Also disable buffer rotation when using webrender.
  bool canHaveRotation = gfxPlatform::BufferRotationEnabled() &&
                         !(aFlags & (PAINT_WILL_RESAMPLE | PAINT_NO_ROTATION)) &&
                         !(aLayer->Manager()->AsWebRenderLayerManager());
  bool canDrawRotated = aFlags & PAINT_CAN_DRAW_ROTATED;
  bool asyncPaint = (aFlags & PAINT_ASYNC);

  IntRect drawBounds = result.mRegionToDraw.GetBounds();
  OpenMode lockMode = asyncPaint ? OpenMode::OPEN_READ_WRITE_ASYNC
                                 : OpenMode::OPEN_READ_WRITE;

  if (asyncPaint) {
    result.mBufferState = new CapturedBufferState();
  }

  if (mBuffer) {
    if (mBuffer->Lock(lockMode)) {
      // Do not modify result.mRegionToDraw or result.mContentType after this call.
      Maybe<CapturedBufferState::Copy> bufferFinalize =
        FinalizeFrame(result.mRegionToDraw);

      if (asyncPaint) {
        result.mBufferState->mBufferFinalize = Move(bufferFinalize);
      } else if (bufferFinalize) {
        bufferFinalize->CopyBuffer();
      }
    } else {
      result.mRegionToDraw = dest.mNeededRegion;
      dest.mCanReuseBuffer = false;
      Clear();
    }
  }

  if (dest.mCanReuseBuffer) {
    MOZ_ASSERT(mBuffer);

    bool canReuseBuffer = false;

    auto newParameters = mBuffer->AdjustedParameters(dest.mBufferRect);
    Maybe<CapturedBufferState::Unrotate> bufferUnrotate = Nothing();

    if ((!canHaveRotation && newParameters.IsRotated()) ||
        (!canDrawRotated && newParameters.RectWrapsBuffer(drawBounds))) {
      bufferUnrotate = Some(CapturedBufferState::Unrotate {
        newParameters,
        mBuffer->ShallowCopy(),
      });
    }

    // If we're async painting then return the buffer state to
    // be dispatched to the paint thread, otherwise do it now
    if (asyncPaint) {
      // We cannot do a buffer unrotate if the buffer is already rotated
      // and we're async painting as that may fail
      if (!bufferUnrotate ||
          mBuffer->BufferRotation() == IntPoint(0,0)) {
        result.mBufferState->mBufferUnrotate = Move(bufferUnrotate);

        // We can then assume that preparing the buffer will always
        // succeed and update our parameters unconditionally
        if (result.mBufferState->mBufferUnrotate) {
          newParameters.SetUnrotated();
        }
        mBuffer->SetParameters(newParameters);
        canReuseBuffer = true;
      }
    } else {
      if (!bufferUnrotate || bufferUnrotate->UnrotateBuffer()) {
        if (bufferUnrotate) {
          newParameters.SetUnrotated();
        }
        mBuffer->SetParameters(newParameters);
        canReuseBuffer = true;
      }
    }

    if (!canReuseBuffer) {
      dest.mBufferRect = ComputeBufferRect(dest.mNeededRegion.GetBounds());
      dest.mCanReuseBuffer = false;
    }
  }

  NS_ASSERTION(!(aFlags & PAINT_WILL_RESAMPLE) || dest.mBufferRect == dest.mNeededRegion.GetBounds(),
               "If we're resampling, we need to validate the entire buffer");

  // We never had a buffer, the buffer wasn't big enough, the content changed
  // types, or we failed to unrotate the buffer when requested. In any case,
  // we need to allocate a new one and prepare it for drawing.
  if (!dest.mCanReuseBuffer) {
    uint32_t bufferFlags = 0;
    if (dest.mBufferMode == SurfaceMode::SURFACE_COMPONENT_ALPHA) {
      bufferFlags |= BUFFER_COMPONENT_ALPHA;
    }

    RefPtr<RotatedBuffer> newBuffer = CreateBuffer(result.mContentType,
                                                   dest.mBufferRect,
                                                   bufferFlags);

    if (!newBuffer) {
      if (Factory::ReasonableSurfaceSize(IntSize(dest.mBufferRect.Width(), dest.mBufferRect.Height()))) {
        gfxCriticalNote << "Failed buffer for "
                        << dest.mBufferRect.X() << ", "
                        << dest.mBufferRect.Y() << ", "
                        << dest.mBufferRect.Width() << ", "
                        << dest.mBufferRect.Height();
      }
      Clear();
      return result;
    }

    if (!newBuffer->Lock(lockMode)) {
      gfxCriticalNote << "Failed to lock new back buffer.";
      Clear();
      return result;
    }

    // If we have an existing front buffer, copy it into the new back buffer
    if (mBuffer) {
      if (mBuffer->IsLocked()) {
        mBuffer->Unlock();
      }

      nsIntRegion updateRegion = newBuffer->BufferRect();
      updateRegion.Sub(updateRegion, result.mRegionToDraw);

      if (!updateRegion.IsEmpty()) {
        auto bufferInitialize = CapturedBufferState::Copy {
          mBuffer->ShallowCopy(),
          newBuffer->ShallowCopy(),
          updateRegion.GetBounds(),
        };

        // If we're async painting then return the buffer state to
        // be dispatched to the paint thread, otherwise do it now
        if (asyncPaint) {
          result.mBufferState->mBufferInitialize = Some(Move(bufferInitialize));
        } else {
          if (!bufferInitialize.CopyBuffer()) {
            gfxCriticalNote << "Failed to copy front buffer to back buffer.";
            return result;
          }
        }
      }
    }

    Clear();
    mBuffer = newBuffer;
  }

  NS_ASSERTION(canHaveRotation || mBuffer->BufferRotation() == IntPoint(0,0),
               "Rotation disabled, but we have nonzero rotation?");

  nsIntRegion invalidate;
  invalidate.Sub(aLayer->GetValidRegion(), dest.mBufferRect);
  result.mRegionToInvalidate.Or(result.mRegionToInvalidate, invalidate);

  result.mClip = DrawRegionClip::DRAW;
  result.mMode = dest.mBufferMode;

  return result;
}

DrawTarget*
ContentClient::BorrowDrawTargetForPainting(ContentClient::PaintState& aPaintState,
                                           RotatedBuffer::DrawIterator* aIter /* = nullptr */)
{
  RefPtr<CapturedPaintState> capturedState =
    ContentClient::BorrowDrawTargetForRecording(aPaintState, aIter, true);

  if (!capturedState) {
    return nullptr;
  }

  if (!ContentClient::PrepareDrawTargetForPainting(capturedState)) {
    return nullptr;
  }

  return capturedState->mTargetDual;
}

nsIntRegion
ExpandDrawRegion(ContentClient::PaintState& aPaintState,
                 RotatedBuffer::DrawIterator* aIter,
                 BackendType aBackendType)
{
  nsIntRegion* drawPtr = &aPaintState.mRegionToDraw;
  if (aIter) {
    // The iterators draw region currently only contains the bounds of the region,
    // this makes it the precise region.
    aIter->mDrawRegion.And(aIter->mDrawRegion, aPaintState.mRegionToDraw);
    drawPtr = &aIter->mDrawRegion;
  }
  if (aBackendType == BackendType::DIRECT2D ||
      aBackendType == BackendType::DIRECT2D1_1) {
    // Simplify the draw region to avoid hitting expensive drawing paths
    // for complex regions.
    drawPtr->SimplifyOutwardByArea(100 * 100);
  }
  return *drawPtr;
}

RefPtr<CapturedPaintState>
ContentClient::BorrowDrawTargetForRecording(ContentClient::PaintState& aPaintState,
                                            RotatedBuffer::DrawIterator* aIter,
                                            bool aSetTransform)
{
  if (aPaintState.mMode == SurfaceMode::SURFACE_NONE || !mBuffer) {
    return nullptr;
  }

  Matrix transform;
  DrawTarget* result = mBuffer->BorrowDrawTargetForQuadrantUpdate(
                                  aPaintState.mRegionToDraw.GetBounds(),
                                  RotatedBuffer::BUFFER_BOTH, aIter,
                                  aSetTransform,
                                  &transform);
  if (!result) {
    return nullptr;
  }

  nsIntRegion regionToDraw =
    ExpandDrawRegion(aPaintState, aIter, result->GetBackendType());

  RefPtr<CapturedPaintState> state =
    new CapturedPaintState(regionToDraw,
                           result,
                           mBuffer->GetDTBuffer(),
                           mBuffer->GetDTBufferOnWhite(),
                           transform,
                           aPaintState.mMode,
                           aPaintState.mContentType);
  return state;
}

void
ContentClient::ReturnDrawTarget(gfx::DrawTarget*& aReturned)
{
  mBuffer->ReturnDrawTarget(aReturned);
}

/*static */ bool
ContentClient::PrepareDrawTargetForPainting(CapturedPaintState* aState)
{
  MOZ_ASSERT(aState);
  RefPtr<DrawTarget> target = aState->mTarget;
  RefPtr<DrawTarget> whiteTarget = aState->mTargetOnWhite;

  if (aState->mSurfaceMode == SurfaceMode::SURFACE_COMPONENT_ALPHA) {
    if (!target || !target->IsValid() ||
        !whiteTarget || !whiteTarget->IsValid()) {
      // This can happen in release builds if allocating one of the two buffers
      // failed. This in turn can happen if unreasonably large textures are
      // requested.
      return false;
    }
    for (auto iter = aState->mRegionToDraw.RectIter(); !iter.Done(); iter.Next()) {
      const IntRect& rect = iter.Get();
      target->FillRect(Rect(rect.X(), rect.Y(), rect.Width(), rect.Height()),
                            ColorPattern(Color(0.0, 0.0, 0.0, 1.0)));
      whiteTarget->FillRect(Rect(rect.X(), rect.Y(), rect.Width(), rect.Height()),
                                 ColorPattern(Color(1.0, 1.0, 1.0, 1.0)));
    }
  } else if (aState->mContentType == gfxContentType::COLOR_ALPHA &&
             target->IsValid()) {
    // HaveBuffer() => we have an existing buffer that we must clear
    for (auto iter = aState->mRegionToDraw.RectIter(); !iter.Done(); iter.Next()) {
      const IntRect& rect = iter.Get();
      target->ClearRect(Rect(rect.X(), rect.Y(), rect.Width(), rect.Height()));
    }
  }

  return true;
}

ContentClient::BufferDecision
ContentClient::CalculateBufferForPaint(PaintedLayer* aLayer,
                                       uint32_t aFlags)
{
  gfxContentType layerContentType =
    aLayer->CanUseOpaqueSurface() ? gfxContentType::COLOR :
                                    gfxContentType::COLOR_ALPHA;

  SurfaceMode mode;
  gfxContentType contentType;
  IntRect destBufferRect;
  nsIntRegion neededRegion;
  nsIntRegion validRegion = aLayer->GetValidRegion();

  bool canReuseBuffer = !!mBuffer;
  bool canKeepBufferContents = true;

  while (true) {
    mode = aLayer->GetSurfaceMode();
    neededRegion = aLayer->GetVisibleRegion().ToUnknownRegion();
    canReuseBuffer = canReuseBuffer && ValidBufferSize(mBufferSizePolicy,
                                                       mBuffer->BufferRect().Size(),
                                                       neededRegion.GetBounds().Size());
    contentType = layerContentType;

    if (canReuseBuffer) {
      if (mBuffer->BufferRect().Contains(neededRegion.GetBounds())) {
        // We don't need to adjust mBufferRect.
        destBufferRect = mBuffer->BufferRect();
      } else if (neededRegion.GetBounds().Size() <= mBuffer->BufferRect().Size()) {
        // The buffer's big enough but doesn't contain everything that's
        // going to be visible. We'll move it.
        destBufferRect = IntRect(neededRegion.GetBounds().TopLeft(), mBuffer->BufferRect().Size());
      } else {
        destBufferRect = neededRegion.GetBounds();
      }
    } else {
      destBufferRect = ComputeBufferRect(neededRegion.GetBounds());
    }

    if (mode == SurfaceMode::SURFACE_COMPONENT_ALPHA) {
#if defined(MOZ_GFX_OPTIMIZE_MOBILE)
      mode = SurfaceMode::SURFACE_SINGLE_CHANNEL_ALPHA;
#else
      if (!aLayer->GetParent() ||
          !aLayer->GetParent()->SupportsComponentAlphaChildren() ||
          !aLayer->AsShadowableLayer() ||
          !aLayer->AsShadowableLayer()->HasShadow()) {
        mode = SurfaceMode::SURFACE_SINGLE_CHANNEL_ALPHA;
      } else {
        contentType = gfxContentType::COLOR;
      }
#endif
    }

    if ((aFlags & PAINT_WILL_RESAMPLE) &&
        (!neededRegion.GetBounds().IsEqualInterior(destBufferRect) ||
         neededRegion.GetNumRects() > 1))
    {
      // The area we add to neededRegion might not be painted opaquely.
      if (mode == SurfaceMode::SURFACE_OPAQUE) {
        contentType = gfxContentType::COLOR_ALPHA;
        mode = SurfaceMode::SURFACE_SINGLE_CHANNEL_ALPHA;
      }

      // We need to validate the entire buffer, to make sure that only valid
      // pixels are sampled.
      neededRegion = destBufferRect;
    }

    // If we have an existing buffer, but the content type has changed or we
    // have transitioned into/out of component alpha, then we need to recreate it.
    bool needsComponentAlpha = (mode == SurfaceMode::SURFACE_COMPONENT_ALPHA);
    bool backBufferChangedSurface = mBuffer &&
                                    (contentType != mBuffer->GetContentType() ||
                                     needsComponentAlpha != mBuffer->HaveBufferOnWhite());
    if (canKeepBufferContents && backBufferChangedSurface) {
      // Restart the decision process; we won't re-enter since we guard on
      // being able to keep the buffer contents.
      canReuseBuffer = false;
      canKeepBufferContents = false;
      validRegion.SetEmpty();
      continue;
    }
    break;
  }

  NS_ASSERTION(destBufferRect.Contains(neededRegion.GetBounds()),
               "Destination rect doesn't contain what we need to paint");

  BufferDecision dest;
  dest.mNeededRegion = Move(neededRegion);
  dest.mValidRegion = Move(validRegion);
  dest.mBufferRect = destBufferRect;
  dest.mBufferMode = mode;
  dest.mBufferContentType = contentType;
  dest.mCanReuseBuffer = canReuseBuffer;
  dest.mCanKeepBufferContents = canKeepBufferContents;
  return dest;
}

bool
ContentClient::ValidBufferSize(BufferSizePolicy aPolicy,
                               const gfx::IntSize& aBufferSize,
                               const gfx::IntSize& aVisibleBoundsSize)
{
  return (aVisibleBoundsSize == aBufferSize ||
          (SizedToVisibleBounds != aPolicy &&
           aVisibleBoundsSize < aBufferSize));
}

void
ContentClient::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  aStream << aPrefix;
  aStream << nsPrintfCString("ContentClient (0x%p)", this).get();
}

// We pass a null pointer for the ContentClient Forwarder argument, which means
// this client will not have a ContentHost on the other side.
ContentClientBasic::ContentClientBasic(gfx::BackendType aBackend)
  : ContentClient(nullptr, ContainsVisibleBounds)
  , mBackend(aBackend)
{}

void
ContentClientBasic::DrawTo(PaintedLayer* aLayer,
                           gfx::DrawTarget* aTarget,
                           float aOpacity,
                           gfx::CompositionOp aOp,
                           gfx::SourceSurface* aMask,
                           const gfx::Matrix* aMaskTransform)
{
  if (!mBuffer) {
    return;
  }

  mBuffer->DrawTo(aLayer, aTarget, aOpacity, aOp,
                  aMask, aMaskTransform);
}

RefPtr<RotatedBuffer>
ContentClientBasic::CreateBuffer(gfxContentType aType,
                                 const IntRect& aRect,
                                 uint32_t aFlags)
{
  MOZ_ASSERT(!(aFlags & BUFFER_COMPONENT_ALPHA));
  if (aFlags & BUFFER_COMPONENT_ALPHA) {
    gfxDevCrash(LogReason::AlphaWithBasicClient) << "Asking basic content client for component alpha";
  }

  IntSize size(aRect.Width(), aRect.Height());
  RefPtr<gfx::DrawTarget> drawTarget;

#ifdef XP_WIN
  if (mBackend == BackendType::CAIRO && 
      (aType == gfxContentType::COLOR || aType == gfxContentType::COLOR_ALPHA)) {
    RefPtr<gfxASurface> surf =
      new gfxWindowsSurface(size, aType == gfxContentType::COLOR ? gfxImageFormat::X8R8G8B8_UINT32 :
                                                                   gfxImageFormat::A8R8G8B8_UINT32);
    drawTarget = gfxPlatform::GetPlatform()->CreateDrawTargetForSurface(surf, size);
  }
#endif

  if (!drawTarget) {
    drawTarget = gfxPlatform::GetPlatform()->CreateDrawTargetForBackend(
      mBackend, size,
      gfxPlatform::GetPlatform()->Optimal2DFormatForContent(aType));
  }

  if (!drawTarget) {
    return nullptr;
  }

  return new DrawTargetRotatedBuffer(drawTarget, nullptr, aRect, IntPoint(0,0));
}

RefPtr<CapturedPaintState>
ContentClientBasic::BorrowDrawTargetForRecording(ContentClient::PaintState& aPaintState,
                                                 RotatedBuffer::DrawIterator* aIter,
                                                 bool aSetTransform)
{
  // BasicLayers does not yet support OMTP.
  return nullptr;
}

RefPtr<CapturedPaintState>
ContentClientRemoteBuffer::BorrowDrawTargetForRecording(ContentClient::PaintState& aPaintState,
                                                        RotatedBuffer::DrawIterator* aIter,
                                                        bool aSetTransform)
{
  RefPtr<CapturedPaintState> cps = ContentClient::BorrowDrawTargetForRecording(aPaintState, aIter, aSetTransform);
  if (!cps) {
    return nullptr;
  }

  RemoteRotatedBuffer* remoteBuffer = GetRemoteBuffer();
  cps->mTextureClient = remoteBuffer->GetClient();
  cps->mTextureClientOnWhite = remoteBuffer->GetClientOnWhite();
  return cps.forget();
}

class RemoteBufferReadbackProcessor : public TextureReadbackSink
{
public:
  RemoteBufferReadbackProcessor(nsTArray<ReadbackProcessor::Update>* aReadbackUpdates,
                                const IntRect& aBufferRect, const nsIntPoint& aBufferRotation)
    : mReadbackUpdates(*aReadbackUpdates)
    , mBufferRect(aBufferRect)
    , mBufferRotation(aBufferRotation)
  {
    for (uint32_t i = 0; i < mReadbackUpdates.Length(); ++i) {
      mLayerRefs.push_back(mReadbackUpdates[i].mLayer);
    }
  }

  virtual void ProcessReadback(gfx::DataSourceSurface *aSourceSurface)
  {
    SourceRotatedBuffer rotBuffer(aSourceSurface, nullptr, mBufferRect, mBufferRotation);

    for (uint32_t i = 0; i < mReadbackUpdates.Length(); ++i) {
      ReadbackProcessor::Update& update = mReadbackUpdates[i];
      nsIntPoint offset = update.mLayer->GetBackgroundLayerOffset();

      ReadbackSink* sink = update.mLayer->GetSink();

      if (!sink) {
        continue;
      }

      if (!aSourceSurface) {
        sink->SetUnknown(update.mSequenceCounter);
        continue;
      }

      RefPtr<DrawTarget> dt =
        sink->BeginUpdate(update.mUpdateRect + offset, update.mSequenceCounter);
      if (!dt) {
        continue;
      }

      dt->SetTransform(Matrix::Translation(offset.x, offset.y));

      rotBuffer.DrawBufferWithRotation(dt, RotatedBuffer::BUFFER_BLACK);

      update.mLayer->GetSink()->EndUpdate(update.mUpdateRect + offset);
    }
  }

private:
  nsTArray<ReadbackProcessor::Update> mReadbackUpdates;
  // This array is used to keep the layers alive until the callback.
  vector<RefPtr<Layer>> mLayerRefs;

  IntRect mBufferRect;
  nsIntPoint mBufferRotation;
};

void
ContentClientRemoteBuffer::EndPaint(nsTArray<ReadbackProcessor::Update>* aReadbackUpdates)
{
  MOZ_ASSERT(!mBuffer || !mBuffer->HaveBufferOnWhite() ||
             !aReadbackUpdates || aReadbackUpdates->Length() == 0);

  RemoteRotatedBuffer* remoteBuffer = GetRemoteBuffer();

  if (remoteBuffer && remoteBuffer->IsLocked()) {
    if (aReadbackUpdates && aReadbackUpdates->Length() > 0) {
      RefPtr<TextureReadbackSink> readbackSink = new RemoteBufferReadbackProcessor(aReadbackUpdates,
                                                                                   remoteBuffer->BufferRect(),
                                                                                   remoteBuffer->BufferRotation());

      remoteBuffer->GetClient()->SetReadbackSink(readbackSink);
    }

    remoteBuffer->Unlock();
    remoteBuffer->SyncWithObject(mForwarder->GetSyncObject());
  }

  ContentClient::EndPaint(aReadbackUpdates);
}

RefPtr<RotatedBuffer>
ContentClientRemoteBuffer::CreateBuffer(gfxContentType aType,
                                        const IntRect& aRect,
                                        uint32_t aFlags)
{
  // If we hit this assertion, then it might be due to an empty transaction
  // followed by a real transaction. Our buffers should be created (but not
  // painted in the empty transaction) and then painted (but not created) in the
  // real transaction. That is kind of fragile, and this assert will catch
  // circumstances where we screw that up, e.g., by unnecessarily recreating our
  // buffers.
  MOZ_ASSERT(!mIsNewBuffer,
             "Bad! Did we create a buffer twice without painting?");

  gfx::SurfaceFormat format = gfxPlatform::GetPlatform()->Optimal2DFormatForContent(aType);

  TextureFlags textureFlags = TextureFlagsForContentClientFlags(aFlags);
  if (aFlags & BUFFER_COMPONENT_ALPHA) {
    textureFlags |= TextureFlags::COMPONENT_ALPHA;
  }

  RefPtr<RotatedBuffer> buffer = CreateBufferInternal(aRect, format, textureFlags);

  if (!buffer) {
    return nullptr;
  }

  mIsNewBuffer = true;
  mTextureFlags = textureFlags;

  return buffer;
}

RefPtr<RotatedBuffer>
ContentClientRemoteBuffer::CreateBufferInternal(const gfx::IntRect& aRect,
                                                gfx::SurfaceFormat aFormat,
                                                TextureFlags aFlags)
{
  TextureAllocationFlags textureAllocFlags
                         = (aFlags & TextureFlags::COMPONENT_ALPHA) ?
                            TextureAllocationFlags::ALLOC_CLEAR_BUFFER_BLACK :
                            TextureAllocationFlags::ALLOC_CLEAR_BUFFER;

  RefPtr<TextureClient> textureClient = CreateTextureClientForDrawing(
    aFormat, aRect.Size(), BackendSelector::Content,
    aFlags | ExtraTextureFlags(),
    textureAllocFlags
  );

  if (!textureClient || !AddTextureClient(textureClient)) {
    return nullptr;
  }
  textureClient->EnableBlockingReadLock();

  RefPtr<TextureClient> textureClientOnWhite;
  if (aFlags & TextureFlags::COMPONENT_ALPHA) {
    textureClientOnWhite = textureClient->CreateSimilar(
      mForwarder->GetCompositorBackendType(),
      aFlags | ExtraTextureFlags(),
      TextureAllocationFlags::ALLOC_CLEAR_BUFFER_WHITE
    );
    if (!textureClientOnWhite || !AddTextureClient(textureClientOnWhite)) {
      return nullptr;
    }
    // We don't enable the readlock for the white buffer since we always
    // use them together and waiting on the lock for the black
    // should be sufficient.
  }

  return new RemoteRotatedBuffer(textureClient,
                                 textureClientOnWhite,
                                 aRect,
                                 IntPoint(0,0));
}

nsIntRegion
ContentClientRemoteBuffer::GetUpdatedRegion(const nsIntRegion& aRegionToDraw,
                                            const nsIntRegion& aVisibleRegion)
{
  nsIntRegion updatedRegion;
  if (mIsNewBuffer || mBuffer->DidSelfCopy()) {
    // A buffer reallocation clears both buffers. The front buffer has all the
    // content by now, but the back buffer is still clear. Here, in effect, we
    // are saying to copy all of the pixels of the front buffer to the back.
    // Also when we self-copied in the buffer, the buffer space
    // changes and some changed buffer content isn't reflected in the
    // draw or invalidate region (on purpose!).  When this happens, we
    // need to read back the entire buffer too.
    updatedRegion = aVisibleRegion.GetBounds();
    mIsNewBuffer = false;
  } else {
    updatedRegion = aRegionToDraw;
  }

  MOZ_ASSERT(mBuffer, "should have a back buffer by now");
  NS_ASSERTION(mBuffer->BufferRect().Contains(aRegionToDraw.GetBounds()),
               "Update outside of buffer rect!");

  return updatedRegion;
}

void
ContentClientRemoteBuffer::Updated(const nsIntRegion& aRegionToDraw,
                                   const nsIntRegion& aVisibleRegion)
{
  nsIntRegion updatedRegion = GetUpdatedRegion(aRegionToDraw,
                                               aVisibleRegion);

  RemoteRotatedBuffer* remoteBuffer = GetRemoteBuffer();

  MOZ_ASSERT(remoteBuffer && remoteBuffer->GetClient());
  if (remoteBuffer->HaveBufferOnWhite()) {
    mForwarder->UseComponentAlphaTextures(this,
                                          remoteBuffer->GetClient(),
                                          remoteBuffer->GetClientOnWhite());
  } else {
    AutoTArray<CompositableForwarder::TimedTextureClient,1> textures;
    CompositableForwarder::TimedTextureClient* t = textures.AppendElement();
    t->mTextureClient = remoteBuffer->GetClient();
    IntSize size = remoteBuffer->GetClient()->GetSize();
    t->mPictureRect = nsIntRect(0, 0, size.width, size.height);
    GetForwarder()->UseTextures(this, textures);
  }

  // This forces a synchronous transaction, so we can swap buffers now
  // and know that we'll have sole ownership of the old front buffer
  // by the time we paint next.
  mForwarder->UpdateTextureRegion(this,
                                  ThebesBufferData(remoteBuffer->BufferRect(),
                                                   remoteBuffer->BufferRotation()),
                                  updatedRegion);
  SwapBuffers(updatedRegion);
}

void
ContentClientRemoteBuffer::Dump(std::stringstream& aStream,
                                const char* aPrefix,
                                bool aDumpHtml, TextureDumpMode aCompress)
{
  RemoteRotatedBuffer* remoteBuffer = GetRemoteBuffer();

  // TODO We should combine the OnWhite/OnBlack here an just output a single image.
  if (!aDumpHtml) {
    aStream << "\n" << aPrefix << "Surface: ";
  }
  CompositableClient::DumpTextureClient(aStream,
                                        remoteBuffer ? remoteBuffer->GetClient() : nullptr,
                                        aCompress);
}

void
ContentClientDoubleBuffered::Dump(std::stringstream& aStream,
                                  const char* aPrefix,
                                  bool aDumpHtml, TextureDumpMode aCompress)
{
  // TODO We should combine the OnWhite/OnBlack here an just output a single image.
  if (!aDumpHtml) {
    aStream << "\n" << aPrefix << "Surface: ";
  }
  CompositableClient::DumpTextureClient(aStream,
                                        mFrontBuffer ? mFrontBuffer->GetClient() : nullptr,
                                        aCompress);
}

void
ContentClientDoubleBuffered::Clear()
{
  ContentClient::Clear();
  mFrontBuffer = nullptr;
}

void
ContentClientDoubleBuffered::SwapBuffers(const nsIntRegion& aFrontUpdatedRegion)
{
  mFrontUpdatedRegion = aFrontUpdatedRegion;

  RefPtr<RemoteRotatedBuffer> frontBuffer = mFrontBuffer;
  RefPtr<RemoteRotatedBuffer> backBuffer = GetRemoteBuffer();

  std::swap(frontBuffer, backBuffer);

  mFrontBuffer = frontBuffer;
  mBuffer = backBuffer;

  mFrontAndBackBufferDiffer = true;
}

ContentClient::PaintState
ContentClientDoubleBuffered::BeginPaint(PaintedLayer* aLayer,
                                        uint32_t aFlags)
{
  EnsureBackBufferIfFrontBuffer();

  mIsNewBuffer = false;

  if (!mFrontBuffer || !mBuffer) {
    mFrontAndBackBufferDiffer = false;
  }

  if (mFrontAndBackBufferDiffer) {
    if (mFrontBuffer->DidSelfCopy()) {
      // We can't easily draw our front buffer into us, since we're going to be
      // copying stuff around anyway it's easiest if we just move our situation
      // to non-rotated while we're at it. If this situation occurs we'll have
      // hit a self-copy path in PaintThebes before as well anyway.
      gfx::IntRect backBufferRect = mBuffer->BufferRect();
      backBufferRect.MoveTo(mFrontBuffer->BufferRect().TopLeft());

      mBuffer->SetBufferRect(backBufferRect);
      mBuffer->SetBufferRotation(IntPoint(0,0));
    } else {
      mBuffer->SetBufferRect(mFrontBuffer->BufferRect());
      mBuffer->SetBufferRotation(mFrontBuffer->BufferRotation());
    }
  }

  return ContentClient::BeginPaint(aLayer, aFlags);
}

// Sync front/back buffers content
// After executing, the new back buffer has the same (interesting) pixels as
// the new front buffer, and mValidRegion et al. are correct wrt the new
// back buffer (i.e. as they were for the old back buffer)
Maybe<CapturedBufferState::Copy>
ContentClientDoubleBuffered::FinalizeFrame(const nsIntRegion& aRegionToDraw)
{
  if (!mFrontAndBackBufferDiffer) {
    MOZ_ASSERT(!mFrontBuffer || !mFrontBuffer->DidSelfCopy(),
               "If the front buffer did a self copy then our front and back buffer must be different.");
    return Nothing();
  }

  MOZ_ASSERT(mFrontBuffer && mBuffer);
  if (!mFrontBuffer || !mBuffer) {
    return Nothing();
  }

  MOZ_LAYERS_LOG(("BasicShadowableThebes(%p): reading back <x=%d,y=%d,w=%d,h=%d>",
                  this,
                  mFrontUpdatedRegion.GetBounds().X(),
                  mFrontUpdatedRegion.GetBounds().Y(),
                  mFrontUpdatedRegion.GetBounds().Width(),
                  mFrontUpdatedRegion.GetBounds().Height()));

  mFrontAndBackBufferDiffer = false;

  nsIntRegion updateRegion = mFrontUpdatedRegion;
  if (mFrontBuffer->DidSelfCopy()) {
    mFrontBuffer->ClearDidSelfCopy();
    updateRegion = mBuffer->BufferRect();
  }

  // No point in sync'ing what we are going to draw over anyway. And if there is
  // nothing to sync at all, there is nothing to do and we can go home early.
  updateRegion.Sub(updateRegion, aRegionToDraw);
  if (updateRegion.IsEmpty()) {
    return Nothing();
  }

  return Some(CapturedBufferState::Copy {
    mFrontBuffer->ShallowCopy(),
    mBuffer->ShallowCopy(),
    updateRegion.GetBounds(),
  });
}

void
ContentClientDoubleBuffered::EnsureBackBufferIfFrontBuffer()
{
  if (!mBuffer && mFrontBuffer) {
    mBuffer = CreateBufferInternal(mFrontBuffer->BufferRect(),
                                   mFrontBuffer->GetFormat(),
                                   mTextureFlags);
    MOZ_ASSERT(mBuffer);
  }
}

} // namespace layers
} // namespace mozilla
