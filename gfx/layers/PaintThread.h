/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_PAINTTHREAD_H
#define MOZILLA_LAYERS_PAINTTHREAD_H

#include "base/platform_thread.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/layers/TextureClient.h"
#include "RotatedBuffer.h"
#include "nsThreadUtils.h"

class nsIThreadPool;

namespace mozilla {
namespace gfx {
class DrawTarget;
class DrawTargetCapture;
};

namespace layers {

// Holds the key parts from a RotatedBuffer::PaintState
// required to draw the captured paint state
class CapturedPaintState {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CapturedPaintState)
public:
  CapturedPaintState(nsIntRegion& aRegionToDraw,
                     gfx::DrawTarget* aTargetDual,
                     gfx::DrawTarget* aTarget,
                     gfx::DrawTarget* aTargetOnWhite,
                     const gfx::Matrix& aTargetTransform,
                     SurfaceMode aSurfaceMode,
                     gfxContentType aContentType)
  : mRegionToDraw(aRegionToDraw)
  , mTargetDual(aTargetDual)
  , mTarget(aTarget)
  , mTargetOnWhite(aTargetOnWhite)
  , mTargetTransform(aTargetTransform)
  , mSurfaceMode(aSurfaceMode)
  , mContentType(aContentType)
  {}

  template<typename F>
  void ForEachTextureClient(F aClosure) const
  {
    aClosure(mTextureClient);
    if (mTextureClientOnWhite) {
      aClosure(mTextureClientOnWhite);
    }
  }

  void DropTextureClients()
  {
    mTextureClient = nullptr;
    mTextureClientOnWhite = nullptr;
  }

  nsIntRegion mRegionToDraw;
  RefPtr<TextureClient> mTextureClient;
  RefPtr<TextureClient> mTextureClientOnWhite;
  RefPtr<gfx::DrawTargetCapture> mCapture;
  RefPtr<gfx::DrawTarget> mTargetDual;
  RefPtr<gfx::DrawTarget> mTarget;
  RefPtr<gfx::DrawTarget> mTargetOnWhite;
  gfx::Matrix mTargetTransform;
  SurfaceMode mSurfaceMode;
  gfxContentType mContentType;

protected:
  virtual ~CapturedPaintState() {}
};

// Holds the key operations for a ContentClient to prepare
// its buffers for painting
class CapturedBufferState final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CapturedBufferState)
public:
  struct Copy {
    Copy(RefPtr<RotatedBuffer> aSource,
         RefPtr<RotatedBuffer> aDestination,
         gfx::IntRect aBounds)
      : mSource(aSource)
      , mDestination(aDestination)
      , mBounds(aBounds)
    {}

    bool CopyBuffer();

    RefPtr<RotatedBuffer> mSource;
    RefPtr<RotatedBuffer> mDestination;
    gfx::IntRect mBounds;
  };

  struct Unrotate {
    Unrotate(RotatedBuffer::Parameters aParameters,
             RefPtr<RotatedBuffer> aBuffer)
      : mParameters(aParameters)
      , mBuffer(aBuffer)
    {}

    bool UnrotateBuffer();

    RotatedBuffer::Parameters mParameters;
    RefPtr<RotatedBuffer> mBuffer;
  };

  /**
   * Prepares the rotated buffers for painting by copying a previous frame
   * into the buffer and/or unrotating the pixels and returns whether the
   * operations were successful. If this fails a new buffer should be created
   * for the frame.
   */
  bool PrepareBuffer();

  bool HasOperations() const
  {
    return mBufferFinalize || mBufferUnrotate || mBufferInitialize;
  }

  template<typename F>
  void ForEachTextureClient(F aClosure) const
  {
    if (mBufferFinalize) {
      if (TextureClient* source = mBufferFinalize->mSource->GetClient()) {
        aClosure(source);
      }
      if (TextureClient* sourceOnWhite = mBufferFinalize->mSource->GetClientOnWhite()) {
        aClosure(sourceOnWhite);
      }
      if (TextureClient* destination = mBufferFinalize->mDestination->GetClient()) {
        aClosure(destination);
      }
      if (TextureClient* destinationOnWhite = mBufferFinalize->mDestination->GetClientOnWhite()) {
        aClosure(destinationOnWhite);
      }
    }

    if (mBufferUnrotate) {
      if (TextureClient* client = mBufferUnrotate->mBuffer->GetClient()) {
        aClosure(client);
      }
      if (TextureClient* clientOnWhite = mBufferUnrotate->mBuffer->GetClientOnWhite()) {
        aClosure(clientOnWhite);
      }
    }

    if (mBufferInitialize) {
      if (TextureClient* source = mBufferInitialize->mSource->GetClient()) {
        aClosure(source);
      }
      if (TextureClient* sourceOnWhite = mBufferInitialize->mSource->GetClientOnWhite()) {
        aClosure(sourceOnWhite);
      }
      if (TextureClient* destination = mBufferInitialize->mDestination->GetClient()) {
        aClosure(destination);
      }
      if (TextureClient* destinationOnWhite = mBufferInitialize->mDestination->GetClientOnWhite()) {
        aClosure(destinationOnWhite);
      }
    }
  }

  void DropTextureClients()
  {
    mBufferFinalize = Nothing();
    mBufferUnrotate = Nothing();
    mBufferInitialize = Nothing();
  }

  Maybe<Copy> mBufferFinalize;
  Maybe<Unrotate> mBufferUnrotate;
  Maybe<Copy> mBufferInitialize;

protected:
  ~CapturedBufferState() {}
};

typedef bool (*PrepDrawTargetForPaintingCallback)(CapturedPaintState* aPaintState);

// Holds the key operations needed to update a tiled content client on the
// paint thread.
class CapturedTiledPaintState {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CapturedTiledPaintState)
public:
  struct Copy {
    Copy(RefPtr<gfx::DrawTarget> aSource,
         RefPtr<gfx::DrawTarget> aDestination,
         gfx::IntRect aSourceBounds,
         gfx::IntPoint aDestinationPoint)
      : mSource(aSource)
      , mDestination(aDestination)
      , mSourceBounds(aSourceBounds)
      , mDestinationPoint(aDestinationPoint)
    {}

    bool CopyBuffer();

    RefPtr<gfx::DrawTarget> mSource;
    RefPtr<gfx::DrawTarget> mDestination;
    gfx::IntRect mSourceBounds;
    gfx::IntPoint mDestinationPoint;
  };

  struct Clear {
    Clear(RefPtr<gfx::DrawTarget> aTarget,
            RefPtr<gfx::DrawTarget> aTargetOnWhite,
            nsIntRegion aDirtyRegion)
      : mTarget(aTarget)
      , mTargetOnWhite(aTargetOnWhite)
      , mDirtyRegion(aDirtyRegion)
    {}

    void ClearBuffer();

    RefPtr<gfx::DrawTarget> mTarget;
    RefPtr<gfx::DrawTarget> mTargetOnWhite;
    nsIntRegion mDirtyRegion;
  };

  CapturedTiledPaintState()
  {}
  CapturedTiledPaintState(gfx::DrawTarget* aTarget,
                          gfx::DrawTargetCapture* aCapture)
  : mTarget(aTarget)
  , mCapture(aCapture)
  {}

  template<typename F>
  void ForEachTextureClient(F aClosure) const
  {
    for (auto client : mClients) {
      aClosure(client);
    }
  }

  void DropTextureClients()
  {
    mClients.clear();
  }

  RefPtr<gfx::DrawTarget> mTarget;
  RefPtr<gfx::DrawTargetCapture> mCapture;
  std::vector<Copy> mCopies;
  std::vector<Clear> mClears;

  std::vector<RefPtr<TextureClient>> mClients;

protected:
  virtual ~CapturedTiledPaintState() {}
};

class CompositorBridgeChild;

class PaintThread final
{
  friend void DestroyPaintThread(UniquePtr<PaintThread>&& aPaintThread);

public:
  static void Start();
  static void Shutdown();
  static PaintThread* Get();

  // Helper for asserts.
  static bool IsOnPaintThread();
  bool IsOnPaintWorkerThread();

  void UpdateRenderMode();

  void PrepareBuffer(CapturedBufferState* aState);

  void PaintContents(CapturedPaintState* aState,
                     PrepDrawTargetForPaintingCallback aCallback);

  void PaintTiledContents(CapturedTiledPaintState* aState);

  // Must be called on the main thread. Signifies that the current
  // batch of CapturedPaintStates* for PaintContents have been recorded
  // and the main thread is finished recording this layer.
  void EndLayer();

  // This allows external users to run code on the paint thread.
  void Dispatch(RefPtr<Runnable>& aRunnable);

  // Must be called on the main thread. Signifies that the current
  // layer tree transaction has been finished and any async paints
  // for it have been queued on the paint thread. This MUST be called
  // at the end of a layer transaction as it will be used to do an optional
  // texture sync and then unblock the main thread if it is waiting to paint
  // a new frame.
  void EndLayerTransaction(SyncObjectClient* aSyncObject);

  // Sync Runnables need threads to be ref counted,
  // But this thread lives through the whole process.
  // We're only temporarily using sync runnables so
  // Override release/addref but don't do anything.
  void Release();
  void AddRef();

  static int32_t CalculatePaintWorkerCount();

private:
  PaintThread();

  bool Init();
  void ShutdownOnPaintThread();
  void InitOnPaintThread();
  void InitPaintWorkers();

  void AsyncPrepareBuffer(CompositorBridgeChild* aBridge,
                          CapturedBufferState* aState);
  void AsyncPaintContents(CompositorBridgeChild* aBridge,
                          CapturedPaintState* aState,
                          PrepDrawTargetForPaintingCallback aCallback);
  void AsyncPaintTiledContents(CompositorBridgeChild* aBridge,
                               CapturedTiledPaintState* aState);
  void AsyncPaintTiledContentsFinished(CompositorBridgeChild* aBridge,
                                       CapturedTiledPaintState* aState);
  void AsyncEndLayer();
  void AsyncEndLayerTransaction(CompositorBridgeChild* aBridge);

  void DispatchEndLayerTransaction(CompositorBridgeChild* aBridge);

  static StaticAutoPtr<PaintThread> sSingleton;
  static StaticRefPtr<nsIThread> sThread;
  static PlatformThreadId sThreadId;

  RefPtr<nsIThreadPool> mPaintWorkers;

  // This shouldn't be very many elements, so a list should be fine.
  // Should only be accessed on the paint thread.
  nsTArray<RefPtr<gfx::DrawTarget>> mDrawTargetsToFlush;
};

} // namespace layers
} // namespace mozilla

#endif
