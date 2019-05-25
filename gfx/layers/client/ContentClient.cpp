/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/ContentClient.h"
#include "BasicLayers.h"            // for BasicLayerManager
#include "gfxContext.h"             // for gfxContext, etc
#include "gfxPlatform.h"            // for gfxPlatform
#include "gfxEnv.h"                 // for gfxEnv
#include "gfxPrefs.h"               // for gfxPrefs
#include "gfxPoint.h"               // for IntSize, gfxPoint
#include "gfxUtils.h"               // for gfxUtils
#include "ipc/ShadowLayers.h"       // for ShadowLayerForwarder
#include "mozilla/ArrayUtils.h"     // for ArrayLength
#include "mozilla/gfx/2D.h"         // for DrawTarget, Factory
#include "mozilla/gfx/BasePoint.h"  // for BasePoint
#include "mozilla/gfx/BaseSize.h"   // for BaseSize
#include "mozilla/gfx/Rect.h"       // for Rect
#include "mozilla/gfx/Types.h"
#include "mozilla/layers/CompositorBridgeChild.h"  // for CompositorBridgeChild
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/LayersMessages.h"  // for ThebesBufferData
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/PaintThread.h"
#include "nsDebug.h"          // for NS_ASSERTION, NS_WARNING, etc
#include "nsISupportsImpl.h"  // for gfxContext::Release, etc
#include "nsIWidget.h"        // for nsIWidget
#include "nsLayoutUtils.h"
#ifdef XP_WIN
#  include "gfxWindowsPlatform.h"
#endif
#ifdef MOZ_WIDGET_GTK
#  include "gfxPlatformGtk.h"
#endif
#include "ReadbackLayer.h"

#include <utility>
#include <vector>

using namespace std;

namespace mozilla {

using namespace gfx;

namespace layers {

static TextureFlags TextureFlagsForContentClientFlags(uint32_t aBufferFlags) {
  TextureFlags result = TextureFlags::NO_FLAGS;

  if (aBufferFlags & ContentClient::BUFFER_COMPONENT_ALPHA) {
    result |= TextureFlags::COMPONENT_ALPHA;
  }

  return result;
}

static IntRect ComputeBufferRect(const IntRect& aRequestedRect) {
  IntRect rect(aRequestedRect);
  // Set a minimum width to guarantee a minimum size of buffers we
  // allocate (and work around problems on some platforms with smaller
  // dimensions). 64 used to be the magic number needed to work around
  // a rendering glitch on b2g (see bug 788411). Now that we don't support
  // this device anymore we should be fine with 8 pixels as the minimum.
  rect.SetWidth(std::max(aRequestedRect.Width(), 8));
  return rect;
}

/* static */
already_AddRefed<ContentClient> ContentClient::CreateContentClient(
    CompositableForwarder* aForwarder) {
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

void ContentClient::Clear() { mBuffer = nullptr; }

ContentClient::PaintState ContentClient::BeginPaint(PaintedLayer* aLayer,
                                                    uint32_t aFlags) {
  BufferDecision dest = CalculateBufferForPaint(aLayer, aFlags);

  PaintState result;
  result.mAsyncPaint = (aFlags & PAINT_ASYNC);
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
        printf_stderr(
            "Invalidating entire rotated buffer (layer %p): content type "
            "changed\n",
            aLayer);
      } else if ((dest.mBufferMode == SurfaceMode::SURFACE_COMPONENT_ALPHA) !=
                 mBuffer->HaveBufferOnWhite()) {
        printf_stderr(
            "Invalidating entire rotated buffer (layer %p): component alpha "
            "changed\n",
            aLayer);
      }
    }
#endif
    Clear();
  }

  result.mRegionToDraw.Sub(dest.mNeededRegion, dest.mValidRegion);

  if (result.mRegionToDraw.IsEmpty()) return result;

  // We need to disable rotation if we're going to be resampled when
  // drawing, because we might sample across the rotation boundary.
  // Also disable buffer rotation when using webrender.
  bool canHaveRotation =
      gfxPlatform::BufferRotationEnabled() &&
      !(aFlags & (PAINT_WILL_RESAMPLE | PAINT_NO_ROTATION)) &&
      !(aLayer->Manager()->AsWebRenderLayerManager());
  bool canDrawRotated = aFlags & PAINT_CAN_DRAW_ROTATED;
  OpenMode readMode =
      result.mAsyncPaint ? OpenMode::OPEN_READ_ASYNC : OpenMode::OPEN_READ;
  OpenMode writeMode = result.mAsyncPaint ? OpenMode::OPEN_READ_WRITE_ASYNC
                                          : OpenMode::OPEN_READ_WRITE;

  IntRect drawBounds = result.mRegionToDraw.GetBounds();

  if (result.mAsyncPaint) {
    result.mAsyncTask.reset(new PaintTask());
  }

  // Try to acquire the back buffer, copy over contents if we are using a new
  // buffer, and rotate or unrotate the buffer as necessary
  if (mBuffer && dest.mCanReuseBuffer) {
    if (mBuffer->Lock(writeMode)) {
      auto newParameters = mBuffer->AdjustedParameters(dest.mBufferRect);

      bool needsUnrotate =
          (!canHaveRotation && newParameters.IsRotated()) ||
          (!canDrawRotated && newParameters.RectWrapsBuffer(drawBounds));
      bool canUnrotate =
          !result.mAsyncPaint || mBuffer->BufferRotation() == IntPoint(0, 0);

      // Only begin a frame and copy over the previous frame if we don't need
      // to unrotate, or we can try to unrotate it. This is to ensure that we
      // don't have a paint task that depends on another paint task.
      if (!needsUnrotate || canUnrotate) {
        // If we're async painting then begin to capture draw commands
        if (result.mAsyncPaint) {
          mBuffer->BeginCapture();
        }

        // Do not modify result.mRegionToDraw or result.mContentType after this
        // call.
        FinalizeFrame(result);
      }

      // Try to rotate the buffer or unrotate it if we cannot be rotated
      if (needsUnrotate) {
        if (canUnrotate && mBuffer->UnrotateBufferTo(newParameters)) {
          newParameters.SetUnrotated();
          mBuffer->SetParameters(newParameters);
        } else {
          MOZ_ASSERT(GetFrontBuffer());
          mBuffer->Unlock();
          dest.mBufferRect = ComputeBufferRect(dest.mNeededRegion.GetBounds());
          dest.mCanReuseBuffer = false;
        }
      } else {
        mBuffer->SetParameters(newParameters);
      }
    } else {
      result.mRegionToDraw = dest.mNeededRegion;
      dest.mCanReuseBuffer = false;
      Clear();
    }
  }

  MOZ_ASSERT(dest.mBufferRect.Contains(result.mRegionToDraw.GetBounds()));

  NS_ASSERTION(!(aFlags & PAINT_WILL_RESAMPLE) ||
                   dest.mBufferRect == dest.mNeededRegion.GetBounds(),
               "If we're resampling, we need to validate the entire buffer");

  // We never had a buffer, the buffer wasn't big enough, the content changed
  // types, or we failed to unrotate the buffer when requested. In any case,
  // we need to allocate a new one and prepare it for drawing.
  if (!dest.mCanReuseBuffer) {
    uint32_t bufferFlags = 0;
    if (dest.mBufferMode == SurfaceMode::SURFACE_COMPONENT_ALPHA) {
      bufferFlags |= BUFFER_COMPONENT_ALPHA;
    }

    RefPtr<RotatedBuffer> newBuffer =
        CreateBuffer(result.mContentType, dest.mBufferRect, bufferFlags);

    if (!newBuffer) {
      if (Factory::ReasonableSurfaceSize(
              IntSize(dest.mBufferRect.Width(), dest.mBufferRect.Height()))) {
        gfxCriticalNote << "Failed buffer for " << dest.mBufferRect.X() << ", "
                        << dest.mBufferRect.Y() << ", "
                        << dest.mBufferRect.Width() << ", "
                        << dest.mBufferRect.Height();
      }
      result.mAsyncTask = nullptr;
      Clear();
      return result;
    }

    if (!newBuffer->Lock(writeMode)) {
      gfxCriticalNote << "Failed to lock new back buffer.";
      result.mAsyncTask = nullptr;
      Clear();
      return result;
    }

    if (result.mAsyncPaint) {
      newBuffer->BeginCapture();
    }

    // If we have an existing front buffer, copy it into the new back buffer
    RefPtr<RotatedBuffer> frontBuffer = GetFrontBuffer();

    if (frontBuffer && frontBuffer->Lock(readMode)) {
      nsIntRegion updateRegion = newBuffer->BufferRect();
      updateRegion.Sub(updateRegion, result.mRegionToDraw);

      if (!updateRegion.IsEmpty()) {
        newBuffer->UpdateDestinationFrom(*frontBuffer,
                                         updateRegion.GetBounds());
      }

      frontBuffer->Unlock();
    } else {
      result.mRegionToDraw = dest.mNeededRegion;
    }

    Clear();
    mBuffer = newBuffer;
  }

  NS_ASSERTION(canHaveRotation || mBuffer->BufferRotation() == IntPoint(0, 0),
               "Rotation disabled, but we have nonzero rotation?");

  if (result.mAsyncPaint) {
    result.mAsyncTask->mTarget = mBuffer->GetBufferTarget();
    result.mAsyncTask->mClients.AppendElement(mBuffer->GetClient());
    if (mBuffer->GetClientOnWhite()) {
      result.mAsyncTask->mClients.AppendElement(mBuffer->GetClientOnWhite());
    }
  }

  nsIntRegion invalidate;
  invalidate.Sub(aLayer->GetValidRegion(), dest.mBufferRect);
  result.mRegionToInvalidate.Or(result.mRegionToInvalidate, invalidate);

  result.mClip = DrawRegionClip::DRAW;
  result.mMode = dest.mBufferMode;

  return result;
}

void ContentClient::EndPaint(
    PaintState& aPaintState,
    nsTArray<ReadbackProcessor::Update>* aReadbackUpdates) {
  if (aPaintState.mAsyncTask) {
    aPaintState.mAsyncTask->mCapture = mBuffer->EndCapture();
  }
}

static nsIntRegion ExpandDrawRegion(ContentClient::PaintState& aPaintState,
                                    RotatedBuffer::DrawIterator* aIter,
                                    BackendType aBackendType) {
  nsIntRegion* drawPtr = &aPaintState.mRegionToDraw;
  if (aIter) {
    // The iterators draw region currently only contains the bounds of the
    // region, this makes it the precise region.
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

DrawTarget* ContentClient::BorrowDrawTargetForPainting(
    ContentClient::PaintState& aPaintState,
    RotatedBuffer::DrawIterator* aIter /* = nullptr */) {
  if (aPaintState.mMode == SurfaceMode::SURFACE_NONE || !mBuffer) {
    return nullptr;
  }

  DrawTarget* result = mBuffer->BorrowDrawTargetForQuadrantUpdate(
      aPaintState.mRegionToDraw.GetBounds(), aIter);
  if (!result || !result->IsValid()) {
    if (result) {
      mBuffer->ReturnDrawTarget(result);
    }
    return nullptr;
  }

  nsIntRegion regionToDraw =
      ExpandDrawRegion(aPaintState, aIter, result->GetBackendType());

  if (aPaintState.mMode == SurfaceMode::SURFACE_COMPONENT_ALPHA ||
      aPaintState.mContentType == gfxContentType::COLOR_ALPHA) {
    // HaveBuffer() => we have an existing buffer that we must clear
    for (auto iter = regionToDraw.RectIter(); !iter.Done(); iter.Next()) {
      const IntRect& rect = iter.Get();
      result->ClearRect(Rect(rect.X(), rect.Y(), rect.Width(), rect.Height()));
    }
  }

  return result;
}

void ContentClient::ReturnDrawTarget(gfx::DrawTarget*& aReturned) {
  mBuffer->ReturnDrawTarget(aReturned);
}

ContentClient::BufferDecision ContentClient::CalculateBufferForPaint(
    PaintedLayer* aLayer, uint32_t aFlags) {
  gfxContentType layerContentType = aLayer->CanUseOpaqueSurface()
                                        ? gfxContentType::COLOR
                                        : gfxContentType::COLOR_ALPHA;

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
    canReuseBuffer =
        canReuseBuffer &&
        ValidBufferSize(mBufferSizePolicy, mBuffer->BufferRect().Size(),
                        neededRegion.GetBounds().Size());
    contentType = layerContentType;

    if (canReuseBuffer) {
      if (mBuffer->BufferRect().Contains(neededRegion.GetBounds())) {
        // We don't need to adjust mBufferRect.
        destBufferRect = mBuffer->BufferRect();
      } else if (neededRegion.GetBounds().Size() <=
                 mBuffer->BufferRect().Size()) {
        // The buffer's big enough but doesn't contain everything that's
        // going to be visible. We'll move it.
        destBufferRect = IntRect(neededRegion.GetBounds().TopLeft(),
                                 mBuffer->BufferRect().Size());
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
         neededRegion.GetNumRects() > 1)) {
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
    // have transitioned into/out of component alpha, then we need to recreate
    // it.
    bool needsComponentAlpha = (mode == SurfaceMode::SURFACE_COMPONENT_ALPHA);
    bool backBufferChangedSurface =
        mBuffer && (contentType != mBuffer->GetContentType() ||
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
  dest.mNeededRegion = std::move(neededRegion);
  dest.mValidRegion = std::move(validRegion);
  dest.mBufferRect = destBufferRect;
  dest.mBufferMode = mode;
  dest.mBufferContentType = contentType;
  dest.mCanReuseBuffer = canReuseBuffer;
  dest.mCanKeepBufferContents = canKeepBufferContents;
  return dest;
}

bool ContentClient::ValidBufferSize(BufferSizePolicy aPolicy,
                                    const gfx::IntSize& aBufferSize,
                                    const gfx::IntSize& aVisibleBoundsSize) {
  return (
      aVisibleBoundsSize == aBufferSize ||
      (SizedToVisibleBounds != aPolicy && aVisibleBoundsSize < aBufferSize));
}

void ContentClient::PrintInfo(std::stringstream& aStream, const char* aPrefix) {
  aStream << aPrefix;
  aStream << nsPrintfCString("ContentClient (0x%p)", this).get();
}

// We pass a null pointer for the ContentClient Forwarder argument, which means
// this client will not have a ContentHost on the other side.
ContentClientBasic::ContentClientBasic(gfx::BackendType aBackend)
    : ContentClient(nullptr, ContainsVisibleBounds), mBackend(aBackend) {}

void ContentClientBasic::DrawTo(PaintedLayer* aLayer, gfx::DrawTarget* aTarget,
                                float aOpacity, gfx::CompositionOp aOp,
                                gfx::SourceSurface* aMask,
                                const gfx::Matrix* aMaskTransform) {
  if (!mBuffer) {
    return;
  }

  mBuffer->DrawTo(aLayer, aTarget, aOpacity, aOp, aMask, aMaskTransform);
}

RefPtr<RotatedBuffer> ContentClientBasic::CreateBuffer(gfxContentType aType,
                                                       const IntRect& aRect,
                                                       uint32_t aFlags) {
  MOZ_ASSERT(!(aFlags & BUFFER_COMPONENT_ALPHA));
  if (aFlags & BUFFER_COMPONENT_ALPHA) {
    gfxDevCrash(LogReason::AlphaWithBasicClient)
        << "Asking basic content client for component alpha";
  }

  IntSize size(aRect.Width(), aRect.Height());
  RefPtr<gfx::DrawTarget> drawTarget;

#ifdef XP_WIN
  if (mBackend == BackendType::CAIRO &&
      (aType == gfxContentType::COLOR ||
       aType == gfxContentType::COLOR_ALPHA)) {
    RefPtr<gfxASurface> surf = new gfxWindowsSurface(
        size, aType == gfxContentType::COLOR ? gfxImageFormat::X8R8G8B8_UINT32
                                             : gfxImageFormat::A8R8G8B8_UINT32);
    drawTarget =
        gfxPlatform::GetPlatform()->CreateDrawTargetForSurface(surf, size);
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

  return new DrawTargetRotatedBuffer(drawTarget, nullptr, aRect,
                                     IntPoint(0, 0));
}

class RemoteBufferReadbackProcessor : public TextureReadbackSink {
 public:
  RemoteBufferReadbackProcessor(
      nsTArray<ReadbackProcessor::Update>* aReadbackUpdates,
      const IntRect& aBufferRect, const nsIntPoint& aBufferRotation)
      : mReadbackUpdates(*aReadbackUpdates),
        mBufferRect(aBufferRect),
        mBufferRotation(aBufferRotation) {
    for (uint32_t i = 0; i < mReadbackUpdates.Length(); ++i) {
      mLayerRefs.push_back(mReadbackUpdates[i].mLayer);
    }
  }

  virtual void ProcessReadback(
      gfx::DataSourceSurface* aSourceSurface) override {
    SourceRotatedBuffer rotBuffer(aSourceSurface, nullptr, mBufferRect,
                                  mBufferRotation);

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

      RefPtr<DrawTarget> dt = sink->BeginUpdate(update.mUpdateRect + offset,
                                                update.mSequenceCounter);
      if (!dt) {
        continue;
      }

      dt->SetTransform(Matrix::Translation(offset.x, offset.y));

      rotBuffer.DrawBufferWithRotation(dt);

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

void ContentClientRemoteBuffer::EndPaint(
    PaintState& aPaintState,
    nsTArray<ReadbackProcessor::Update>* aReadbackUpdates) {
  MOZ_ASSERT(!mBuffer || !mBuffer->HaveBufferOnWhite() || !aReadbackUpdates ||
             aReadbackUpdates->Length() == 0);

  RemoteRotatedBuffer* remoteBuffer = GetRemoteBuffer();

  if (remoteBuffer && remoteBuffer->IsLocked()) {
    if (aReadbackUpdates && aReadbackUpdates->Length() > 0) {
      RefPtr<TextureReadbackSink> readbackSink =
          new RemoteBufferReadbackProcessor(aReadbackUpdates,
                                            remoteBuffer->BufferRect(),
                                            remoteBuffer->BufferRotation());

      remoteBuffer->GetClient()->SetReadbackSink(readbackSink);
    }

    remoteBuffer->Unlock();
    remoteBuffer->SyncWithObject(mForwarder->GetSyncObject());
  }

  ContentClient::EndPaint(aPaintState, aReadbackUpdates);
}

RefPtr<RotatedBuffer> ContentClientRemoteBuffer::CreateBuffer(
    gfxContentType aType, const IntRect& aRect, uint32_t aFlags) {
  // If we hit this assertion, then it might be due to an empty transaction
  // followed by a real transaction. Our buffers should be created (but not
  // painted in the empty transaction) and then painted (but not created) in the
  // real transaction. That is kind of fragile, and this assert will catch
  // circumstances where we screw that up, e.g., by unnecessarily recreating our
  // buffers.
  MOZ_ASSERT(!mIsNewBuffer,
             "Bad! Did we create a buffer twice without painting?");

  gfx::SurfaceFormat format =
      gfxPlatform::GetPlatform()->Optimal2DFormatForContent(aType);

  TextureFlags textureFlags = TextureFlagsForContentClientFlags(aFlags);
  if (aFlags & BUFFER_COMPONENT_ALPHA) {
    textureFlags |= TextureFlags::COMPONENT_ALPHA;
  }

  RefPtr<RotatedBuffer> buffer =
      CreateBufferInternal(aRect, format, textureFlags);

  if (!buffer) {
    return nullptr;
  }

  mIsNewBuffer = true;
  mTextureFlags = textureFlags;

  return buffer;
}

RefPtr<RotatedBuffer> ContentClientRemoteBuffer::CreateBufferInternal(
    const gfx::IntRect& aRect, gfx::SurfaceFormat aFormat,
    TextureFlags aFlags) {
  TextureAllocationFlags textureAllocFlags =
      TextureAllocationFlags::ALLOC_DEFAULT;

  RefPtr<TextureClient> textureClient = CreateTextureClientForDrawing(
      aFormat, aRect.Size(), BackendSelector::Content,
      aFlags | ExtraTextureFlags() | TextureFlags::BLOCKING_READ_LOCK,
      textureAllocFlags);

  if (!textureClient || !AddTextureClient(textureClient)) {
    return nullptr;
  }

  RefPtr<TextureClient> textureClientOnWhite;
  if (aFlags & TextureFlags::COMPONENT_ALPHA) {
    TextureAllocationFlags allocFlags = TextureAllocationFlags::ALLOC_DEFAULT;
    if (mForwarder->SupportsTextureDirectMapping()) {
      allocFlags =
          TextureAllocationFlags(allocFlags | ALLOC_ALLOW_DIRECT_MAPPING);
    }
    textureClientOnWhite =
        textureClient->CreateSimilar(mForwarder->GetCompositorBackendType(),
                                     aFlags | ExtraTextureFlags(), allocFlags);
    if (!textureClientOnWhite || !AddTextureClient(textureClientOnWhite)) {
      return nullptr;
    }
    // We don't enable the readlock for the white buffer since we always
    // use them together and waiting on the lock for the black
    // should be sufficient.
  }

  return new RemoteRotatedBuffer(textureClient, textureClientOnWhite, aRect,
                                 IntPoint(0, 0));
}

nsIntRegion ContentClientRemoteBuffer::GetUpdatedRegion(
    const nsIntRegion& aRegionToDraw, const nsIntRegion& aVisibleRegion) {
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

void ContentClientRemoteBuffer::Updated(const nsIntRegion& aRegionToDraw,
                                        const nsIntRegion& aVisibleRegion) {
  nsIntRegion updatedRegion = GetUpdatedRegion(aRegionToDraw, aVisibleRegion);

  RemoteRotatedBuffer* remoteBuffer = GetRemoteBuffer();

  MOZ_ASSERT(remoteBuffer && remoteBuffer->GetClient());
  if (remoteBuffer->HaveBufferOnWhite()) {
    mForwarder->UseComponentAlphaTextures(this, remoteBuffer->GetClient(),
                                          remoteBuffer->GetClientOnWhite());
  } else {
    AutoTArray<CompositableForwarder::TimedTextureClient, 1> textures;
    CompositableForwarder::TimedTextureClient* t = textures.AppendElement();
    t->mTextureClient = remoteBuffer->GetClient();
    IntSize size = remoteBuffer->GetClient()->GetSize();
    t->mPictureRect = nsIntRect(0, 0, size.width, size.height);

    GetForwarder()->UseTextures(this, textures, Nothing());
  }

  // This forces a synchronous transaction, so we can swap buffers now
  // and know that we'll have sole ownership of the old front buffer
  // by the time we paint next.
  mForwarder->UpdateTextureRegion(
      this,
      ThebesBufferData(remoteBuffer->BufferRect(),
                       remoteBuffer->BufferRotation()),
      updatedRegion);
  SwapBuffers(updatedRegion);
}

void ContentClientRemoteBuffer::Dump(std::stringstream& aStream,
                                     const char* aPrefix, bool aDumpHtml,
                                     TextureDumpMode aCompress) {
  RemoteRotatedBuffer* remoteBuffer = GetRemoteBuffer();

  // TODO We should combine the OnWhite/OnBlack here an just output a single
  // image.
  if (!aDumpHtml) {
    aStream << "\n" << aPrefix << "Surface: ";
  }
  CompositableClient::DumpTextureClient(
      aStream, remoteBuffer ? remoteBuffer->GetClient() : nullptr, aCompress);
}

void ContentClientDoubleBuffered::Dump(std::stringstream& aStream,
                                       const char* aPrefix, bool aDumpHtml,
                                       TextureDumpMode aCompress) {
  // TODO We should combine the OnWhite/OnBlack here an just output a single
  // image.
  if (!aDumpHtml) {
    aStream << "\n" << aPrefix << "Surface: ";
  }
  CompositableClient::DumpTextureClient(
      aStream, mFrontBuffer ? mFrontBuffer->GetClient() : nullptr, aCompress);
}

void ContentClientDoubleBuffered::Clear() {
  ContentClient::Clear();
  mFrontBuffer = nullptr;
}

void ContentClientDoubleBuffered::SwapBuffers(
    const nsIntRegion& aFrontUpdatedRegion) {
  mFrontUpdatedRegion = aFrontUpdatedRegion;

  RefPtr<RemoteRotatedBuffer> frontBuffer = mFrontBuffer;
  RefPtr<RemoteRotatedBuffer> backBuffer = GetRemoteBuffer();

  std::swap(frontBuffer, backBuffer);

  mFrontBuffer = frontBuffer;
  mBuffer = backBuffer;

  mFrontAndBackBufferDiffer = true;
}

ContentClient::PaintState ContentClientDoubleBuffered::BeginPaint(
    PaintedLayer* aLayer, uint32_t aFlags) {
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
      mBuffer->SetBufferRotation(IntPoint(0, 0));
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
void ContentClientDoubleBuffered::FinalizeFrame(PaintState& aPaintState) {
  if (!mFrontAndBackBufferDiffer) {
    MOZ_ASSERT(!mFrontBuffer || !mFrontBuffer->DidSelfCopy(),
               "If the front buffer did a self copy then our front and back "
               "buffer must be different.");
    return;
  }

  MOZ_ASSERT(mFrontBuffer && mBuffer);
  if (!mFrontBuffer || !mBuffer) {
    return;
  }

  MOZ_LAYERS_LOG(
      ("BasicShadowableThebes(%p): reading back <x=%d,y=%d,w=%d,h=%d>", this,
       mFrontUpdatedRegion.GetBounds().X(), mFrontUpdatedRegion.GetBounds().Y(),
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
  updateRegion.Sub(updateRegion, aPaintState.mRegionToDraw);
  if (updateRegion.IsEmpty()) {
    return;
  }

  OpenMode openMode = aPaintState.mAsyncPaint ? OpenMode::OPEN_READ_ASYNC
                                              : OpenMode::OPEN_READ_ONLY;

  if (mFrontBuffer->Lock(openMode)) {
    mBuffer->UpdateDestinationFrom(*mFrontBuffer, updateRegion.GetBounds());

    if (aPaintState.mAsyncPaint) {
      aPaintState.mAsyncTask->mClients.AppendElement(mFrontBuffer->GetClient());
      if (mFrontBuffer->GetClientOnWhite()) {
        aPaintState.mAsyncTask->mClients.AppendElement(
            mFrontBuffer->GetClientOnWhite());
      }
    }

    mFrontBuffer->Unlock();
  }
}

void ContentClientDoubleBuffered::EnsureBackBufferIfFrontBuffer() {
  if (!mBuffer && mFrontBuffer) {
    mBuffer = CreateBufferInternal(mFrontBuffer->BufferRect(),
                                   mFrontBuffer->GetFormat(), mTextureFlags);
    MOZ_ASSERT(mBuffer);
  }
}

}  // namespace layers
}  // namespace mozilla
