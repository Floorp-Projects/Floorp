/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
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
#include "mozilla/Maybe.h"
#include "mozilla/gfx/2D.h"             // for DrawTarget, Factory
#include "mozilla/gfx/BasePoint.h"      // for BasePoint
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/gfx/Types.h"
#include "mozilla/layers/CompositorBridgeChild.h" // for CompositorBridgeChild
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/LayersMessages.h"  // for ThebesBufferData
#include "mozilla/layers/LayersTypes.h"
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

#include <vector>

using namespace std;

namespace mozilla {

using namespace gfx;

namespace layers {

static TextureFlags TextureFlagsForRotatedContentBufferFlags(uint32_t aBufferFlags)
{
  TextureFlags result = TextureFlags::NO_FLAGS;

  if (aBufferFlags & RotatedContentBuffer::BUFFER_COMPONENT_ALPHA) {
    result |= TextureFlags::COMPONENT_ALPHA;
  }

  return result;
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
ContentClient::BeginAsyncPaint()
{
  mInAsyncPaint = true;
}

void
ContentClient::EndPaint(nsTArray<ReadbackProcessor::Update>* aReadbackUpdates)
{
  mInAsyncPaint = false;
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
  : ContentClient(nullptr)
  , RotatedContentBuffer(ContainsVisibleBounds)
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
  if (!EnsureBuffer()) {
    return;
  }

  RotatedContentBuffer::DrawTo(aLayer, aTarget, aOpacity, aOp,
                               aMask, aMaskTransform);
}

void
ContentClientBasic::CreateBuffer(ContentType aType,
                                 const IntRect& aRect,
                                 uint32_t aFlags,
                                 RefPtr<gfx::DrawTarget>* aBlackDT,
                                 RefPtr<gfx::DrawTarget>* aWhiteDT)
{
  MOZ_ASSERT(!(aFlags & BUFFER_COMPONENT_ALPHA));
  if (aFlags & BUFFER_COMPONENT_ALPHA) {
    gfxDevCrash(LogReason::AlphaWithBasicClient) << "Asking basic content client for component alpha";
  }

  IntSize size(aRect.Width(), aRect.Height());
#ifdef XP_WIN
  if (mBackend == BackendType::CAIRO && 
      (aType == gfxContentType::COLOR || aType == gfxContentType::COLOR_ALPHA)) {
    RefPtr<gfxASurface> surf =
      new gfxWindowsSurface(size, aType == gfxContentType::COLOR ? gfxImageFormat::X8R8G8B8_UINT32 :
                                                                   gfxImageFormat::A8R8G8B8_UINT32);
    *aBlackDT = gfxPlatform::GetPlatform()->CreateDrawTargetForSurface(surf, size);

    if (*aBlackDT) {
      return;
    }
  }
#endif

  *aBlackDT = gfxPlatform::GetPlatform()->CreateDrawTargetForBackend(
    mBackend, size,
    gfxPlatform::GetPlatform()->Optimal2DFormatForContent(aType));
}

RefPtr<CapturedPaintState>
ContentClientBasic::BorrowDrawTargetForRecording(PaintState& aPaintState,
                                                 RotatedContentBuffer::DrawIterator* aIter)
{
  // BasicLayers does not yet support OMTP.
  return nullptr;
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

RefPtr<CapturedPaintState>
ContentClientRemoteBuffer::BorrowDrawTargetForRecording(PaintState& aPaintState,
                                                        RotatedContentBuffer::DrawIterator* aIter)
{
  RefPtr<CapturedPaintState> cps = RotatedContentBuffer::BorrowDrawTargetForRecording(aPaintState, aIter);
  if (!cps) {
    return nullptr;
  }

  cps->mTextureClient = mTextureClient;
  cps->mTextureClientOnWhite = mTextureClientOnWhite;
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
ContentClientRemoteBuffer::BeginAsyncPaint()
{
  BeginPaint();
  mInAsyncPaint = true;
}

void
ContentClientRemoteBuffer::EndPaint(nsTArray<ReadbackProcessor::Update>* aReadbackUpdates)
{
  MOZ_ASSERT(!mTextureClientOnWhite || !aReadbackUpdates || aReadbackUpdates->Length() == 0);

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
    if (aReadbackUpdates && aReadbackUpdates->Length() > 0) {
      RefPtr<TextureReadbackSink> readbackSink = new RemoteBufferReadbackProcessor(aReadbackUpdates, mBufferRect, mBufferRotation);

      mTextureClient->SetReadbackSink(readbackSink);
    }

    mTextureClient->Unlock();
    mTextureClient->SyncWithObject(mForwarder->GetSyncObject());
  }
  if (mTextureClientOnWhite && mTextureClientOnWhite->IsLocked()) {
    mTextureClientOnWhite->Unlock();
    mTextureClientOnWhite->SyncWithObject(mForwarder->GetSyncObject());
  }

  ContentClient::EndPaint(aReadbackUpdates);
}

void
ContentClientRemoteBuffer::BuildTextureClients(SurfaceFormat aFormat,
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

  mIsNewBuffer = true;

  DestroyBuffers();

  mSurfaceFormat = aFormat;
  mSize = IntSize(aRect.Width(), aRect.Height());
  mTextureFlags = TextureFlagsForRotatedContentBufferFlags(aFlags);

  if (aFlags & BUFFER_COMPONENT_ALPHA) {
    mTextureFlags |= TextureFlags::COMPONENT_ALPHA;
  }

  CreateBackBuffer(mBufferRect);
}

void
ContentClientRemoteBuffer::CreateBackBuffer(const IntRect& aBufferRect)
{
  // gfx::BackendType::NONE means fallback to the content backend
  TextureAllocationFlags textureAllocFlags
                         = (mTextureFlags & TextureFlags::COMPONENT_ALPHA) ?
                            TextureAllocationFlags::ALLOC_CLEAR_BUFFER_BLACK :
                            TextureAllocationFlags::ALLOC_CLEAR_BUFFER;

  mTextureClient = CreateTextureClientForDrawing(
    mSurfaceFormat, mSize, BackendSelector::Content,
    mTextureFlags | ExtraTextureFlags(),
    textureAllocFlags
  );
  if (!mTextureClient || !AddTextureClient(mTextureClient)) {
    AbortTextureClientCreation();
    return;
  }
  mTextureClient->EnableBlockingReadLock();

  if (mTextureFlags & TextureFlags::COMPONENT_ALPHA) {
    mTextureClientOnWhite = mTextureClient->CreateSimilar(
      mForwarder->GetCompositorBackendType(),
      mTextureFlags | ExtraTextureFlags(),
      TextureAllocationFlags::ALLOC_CLEAR_BUFFER_WHITE
    );
    if (!mTextureClientOnWhite || !AddTextureClient(mTextureClientOnWhite)) {
      AbortTextureClientCreation();
      return;
    }
    // We don't enable the readlock for the white buffer since we always
    // use them together and waiting on the lock for the black
    // should be sufficient.
  }
}

void
ContentClientRemoteBuffer::CreateBuffer(ContentType aType,
                                        const IntRect& aRect,
                                        uint32_t aFlags,
                                        RefPtr<gfx::DrawTarget>* aBlackDT,
                                        RefPtr<gfx::DrawTarget>* aWhiteDT)
{
  BuildTextureClients(gfxPlatform::GetPlatform()->Optimal2DFormatForContent(aType), aRect, aFlags);
  if (!mTextureClient) {
    return;
  }

  OpenMode mode = OpenMode::OPEN_READ_WRITE;
  if (mInAsyncPaint) {
    mode |= OpenMode::OPEN_ASYNC_WRITE;
  }

  // We just created the textures and we are about to get their draw targets
  // so we have to lock them here.
  DebugOnly<bool> locked = mTextureClient->Lock(mode);
  MOZ_ASSERT(locked, "Could not lock the TextureClient");

  *aBlackDT = mTextureClient->BorrowDrawTarget();
  if (aFlags & BUFFER_COMPONENT_ALPHA) {
    locked = mTextureClientOnWhite->Lock(mode);
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
    updatedRegion = aVisibleRegion.GetBounds();
    mIsNewBuffer = false;
  } else {
    updatedRegion = aRegionToDraw;
  }

  NS_ASSERTION(BufferRect().Contains(aRegionToDraw.GetBounds()),
               "Update outside of buffer rect!");
  MOZ_ASSERT(mTextureClient, "should have a back buffer by now");

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
    AutoTArray<CompositableForwarder::TimedTextureClient,1> textures;
    CompositableForwarder::TimedTextureClient* t = textures.AppendElement();
    t->mTextureClient = mTextureClient;
    IntSize size = mTextureClient->GetSize();
    t->mPictureRect = nsIntRect(0, 0, size.width, size.height);
    GetForwarder()->UseTextures(this, textures);
  }
  // This forces a synchronous transaction, so we can swap buffers now
  // and know that we'll have sole ownership of the old front buffer
  // by the time we paint next.
  mForwarder->UpdateTextureRegion(this,
                                  ThebesBufferData(BufferRect(),
                                                   BufferRotation()),
                                  updatedRegion);
  SwapBuffers(updatedRegion);
}

void
ContentClientRemoteBuffer::SwapBuffers(const nsIntRegion& aFrontUpdatedRegion)
{
  mFrontAndBackBufferDiffer = true;
}

bool
ContentClientRemoteBuffer::LockBuffers()
{
  OpenMode mode = OpenMode::OPEN_READ_WRITE;
  if (mInAsyncPaint) {
    mode |= OpenMode::OPEN_ASYNC_WRITE;
  }
  if (mTextureClient) {
    bool locked = mTextureClient->Lock(mode);
    if (!locked) {
      return false;
    }
  }
  if (mTextureClientOnWhite) {
    bool locked = mTextureClientOnWhite->Lock(mode);
    if (!locked) {
      UnlockBuffers();
      return false;
    }
  }
  return true;
}

void
ContentClientRemoteBuffer::UnlockBuffers()
{
  if (mTextureClient && mTextureClient->IsLocked()) {
    mTextureClient->Unlock();
  }
  if (mTextureClientOnWhite && mTextureClientOnWhite->IsLocked()) {
    mTextureClientOnWhite->Unlock();
  }
}

void
ContentClientRemoteBuffer::Dump(std::stringstream& aStream,
                                const char* aPrefix,
                                bool aDumpHtml, TextureDumpMode aCompress)
{
  // TODO We should combine the OnWhite/OnBlack here an just output a single image.
  if (!aDumpHtml) {
    aStream << "\n" << aPrefix << "Surface: ";
  }
  CompositableClient::DumpTextureClient(aStream, mTextureClient, aCompress);
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
  if (mFrontBuffer) {
    CompositableClient::DumpTextureClient(aStream, mFrontBuffer->GetClient(), aCompress);
  }
}

void
ContentClientDoubleBuffered::DestroyFrontBuffer()
{
  if (mFrontBuffer) {
    RefPtr<TextureClient> client = mFrontBuffer->GetClient();
    RefPtr<TextureClient> clientOnWhite = mFrontBuffer->GetClientOnWhite();

    if (client) {
      mOldTextures.AppendElement(client);
    }
    if (clientOnWhite) {
      mOldTextures.AppendElement(clientOnWhite);
    }

    mFrontBuffer = Nothing();
  }
}

void
ContentClientDoubleBuffered::Updated(const nsIntRegion& aRegionToDraw,
                                     const nsIntRegion& aVisibleRegion,
                                     bool aDidSelfCopy)
{
  ContentClientRemoteBuffer::Updated(aRegionToDraw, aVisibleRegion, aDidSelfCopy);
}

void
ContentClientDoubleBuffered::SwapBuffers(const nsIntRegion& aFrontUpdatedRegion)
{
  mFrontUpdatedRegion = aFrontUpdatedRegion;

  RefPtr<TextureClient> newBack;
  RefPtr<TextureClient> newBackOnWhite;
  IntRect newBackBufferRect;
  nsIntPoint newBackBufferRotation;

  if (mFrontBuffer) {
    newBack = mFrontBuffer->GetClient();
    newBackOnWhite = mFrontBuffer->GetClientOnWhite();
    newBackBufferRect = mFrontBuffer->BufferRect();
    newBackBufferRotation = mFrontBuffer->BufferRotation();
  }

  mFrontBuffer = Some(RemoteRotatedBuffer(mTextureClient, mTextureClientOnWhite,
                                          mBufferRect, mBufferRotation));

  mTextureClient = newBack;
  mTextureClientOnWhite = newBackOnWhite;
  mBufferRect = newBackBufferRect;
  mBufferRotation = newBackBufferRotation;

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

  if (!mFrontBuffer) {
    mFrontAndBackBufferDiffer = false;
    return;
  }

  if (mDidSelfCopy) {
    // We can't easily draw our front buffer into us, since we're going to be
    // copying stuff around anyway it's easiest if we just move our situation
    // to non-rotated while we're at it. If this situation occurs we'll have
    // hit a self-copy path in PaintThebes before as well anyway.
    mBufferRect.MoveTo(mFrontBuffer->BufferRect().TopLeft());
    mBufferRotation = nsIntPoint();
    return;
  }
  mBufferRect = mFrontBuffer->BufferRect();
  mBufferRotation = mFrontBuffer->BufferRotation();
}

void
ContentClientDoubleBuffered::BeginAsyncPaint()
{
  BeginPaint();
  mInAsyncPaint = true;
}

// Sync front/back buffers content
// After executing, the new back buffer has the same (interesting) pixels as
// the new front buffer, and mValidRegion et al. are correct wrt the new
// back buffer (i.e. as they were for the old back buffer)
void
ContentClientDoubleBuffered::FinalizeFrame(const nsIntRegion& aRegionToDraw)
{
  if (!mFrontAndBackBufferDiffer) {
    MOZ_ASSERT(!mDidSelfCopy, "If we have to copy the world, then our buffers are different, right?");
    return;
  }
  MOZ_ASSERT(mFrontBuffer);
  if (!mFrontBuffer) {
    return;
  }

  MOZ_LAYERS_LOG(("BasicShadowableThebes(%p): reading back <x=%d,y=%d,w=%d,h=%d>",
                  this,
                  mFrontUpdatedRegion.GetBounds().x,
                  mFrontUpdatedRegion.GetBounds().y,
                  mFrontUpdatedRegion.GetBounds().Width(),
                  mFrontUpdatedRegion.GetBounds().Height()));

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

  if (!EnsureBuffer() ||
      (HaveBufferOnWhite() && !EnsureBufferOnWhite())) {
    return;
  }

  if (mFrontBuffer->Lock(OpenMode::OPEN_READ_ONLY)) {
    UpdateDestinationFrom(*mFrontBuffer, updateRegion);
    mFrontBuffer->Unlock();
  }
}

void
ContentClientDoubleBuffered::EnsureBackBufferIfFrontBuffer()
{
  if (!mTextureClient && mFrontBuffer) {
    CreateBackBuffer(mFrontBuffer->BufferRect());

    mBufferRect = mFrontBuffer->BufferRect();
    mBufferRotation = mFrontBuffer->BufferRotation();
  }
}

} // namespace layers
} // namespace mozilla
