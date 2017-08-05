/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 ts=8 et tw=99 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_PAINTTHREAD_H
#define MOZILLA_LAYERS_PAINTTHREAD_H

#include "base/platform_thread.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/UniquePtr.h"
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
                     gfx::DrawTargetCapture* aCapture,
                     gfx::DrawTarget* aTarget,
                     gfx::DrawTarget* aTargetOnWhite,
                     gfx::Matrix aTargetTransform,
                     SurfaceMode aSurfaceMode,
                     gfxContentType aContentType)
  : mRegionToDraw(aRegionToDraw)
  , mCapture(aCapture)
  , mTarget(aTarget)
  , mTargetOnWhite(aTargetOnWhite)
  , mTargetTransform(aTargetTransform)
  , mSurfaceMode(aSurfaceMode)
  , mContentType(aContentType)
  {}

  nsIntRegion mRegionToDraw;
  RefPtr<gfx::DrawTargetCapture> mCapture;
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
  void PaintContents(CapturedPaintState* aState,
                     PrepDrawTargetForPaintingCallback aCallback);

  // Sync Runnables need threads to be ref counted,
  // But this thread lives through the whole process.
  // We're only temporarily using sync runnables so
  // Override release/addref but don't do anything.
  void Release();
  void AddRef();

  // Helper for asserts.
  static bool IsOnPaintThread();

private:
  bool Init();
  void ShutdownOnPaintThread();
  void InitOnPaintThread();
  void PaintContentsAsync(CompositorBridgeChild* aBridge,
                          CapturedPaintState* aState,
                          PrepDrawTargetForPaintingCallback aCallback);

  static StaticAutoPtr<PaintThread> sSingleton;
  static StaticRefPtr<nsIThread> sThread;
  static PlatformThreadId sThreadId;
};

} // namespace layers
} // namespace mozilla

#endif
