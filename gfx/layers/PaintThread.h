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
#include "nsThreadUtils.h"

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

typedef bool (*PrepDrawTargetForPaintingCallback)(CapturedPaintState* aPaintState);

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

  void PaintContents(CapturedPaintState* aState,
                     PrepDrawTargetForPaintingCallback aCallback);

  // Must be called on the main thread. Signifies that the current
  // batch of CapturedPaintStates* for PaintContents have been recorded
  // and the main thread is finished recording this layer.
  void EndLayer();

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

private:
  bool Init();
  void ShutdownOnPaintThread();
  void InitOnPaintThread();

  void AsyncPaintContents(CompositorBridgeChild* aBridge,
                          CapturedPaintState* aState,
                          PrepDrawTargetForPaintingCallback aCallback);
  void AsyncEndLayer();
  void AsyncEndLayerTransaction(CompositorBridgeChild* aBridge,
                                SyncObjectClient* aSyncObject);

  static StaticAutoPtr<PaintThread> sSingleton;
  static StaticRefPtr<nsIThread> sThread;
  static PlatformThreadId sThreadId;

  // This shouldn't be very many elements, so a list should be fine.
  // Should only be accessed on the paint thread.
  nsTArray<RefPtr<gfx::DrawTarget>> mDrawTargetsToFlush;
};

} // namespace layers
} // namespace mozilla

#endif
