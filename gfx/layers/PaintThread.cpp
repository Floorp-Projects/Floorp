/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PaintThread.h"

#include <algorithm>

#include "base/task.h"
#include "gfxPlatform.h"
#include "gfxPrefs.h"
#include "GeckoProfiler.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/ShadowLayers.h"
#include "mozilla/layers/SyncObject.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/Preferences.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/SyncRunnable.h"
#include "nsIPropertyBag2.h"
#include "nsServiceManagerUtils.h"
#include "nsSystemInfo.h"

// Uncomment the following line to dispatch sync runnables when
// painting so that rasterization happens synchronously from
// the perspective of the main thread
// #define OMTP_FORCE_SYNC

namespace mozilla {
namespace layers {

using namespace gfx;

bool
CapturedBufferState::Copy::CopyBuffer()
{
  if (mSource->Lock(OpenMode::OPEN_READ_ONLY)) {
    mDestination->UpdateDestinationFrom(*mSource, mBounds);
    mSource->Unlock();
    return true;
  }
  return false;
}

bool
CapturedBufferState::Unrotate::UnrotateBuffer()
{
  return mBuffer->UnrotateBufferTo(mParameters);
}

bool
CapturedBufferState::PrepareBuffer()
{
  return (!mBufferFinalize || mBufferFinalize->CopyBuffer()) &&
         (!mBufferUnrotate || mBufferUnrotate->UnrotateBuffer()) &&
         (!mBufferInitialize || mBufferInitialize->CopyBuffer());
}

bool
CapturedTiledPaintState::Copy::CopyBuffer()
{
  RefPtr<gfx::SourceSurface> source = mSource->Snapshot();

  // This operation requires the destination draw target to be untranslated,
  // but the destination will have a transform from being part of a tiled draw
  // target. However in this case, CopySurface ignores transforms so we don't
  // need to do anything.
  mDestination->CopySurface(source,
                            mSourceBounds,
                            mDestinationPoint);
  return true;
}

void
CapturedTiledPaintState::Clear::ClearBuffer()
{
  // See the comment in CopyBuffer for why we need to temporarily reset
  // the transform of the draw target.
  Matrix oldTransform = mTarget->GetTransform();
  mTarget->SetTransform(Matrix());

  if (mTargetOnWhite) {
    mTargetOnWhite->SetTransform(Matrix());
    for (auto iter = mDirtyRegion.RectIter(); !iter.Done(); iter.Next()) {
      const gfx::Rect drawRect(iter.Get().X(), iter.Get().Y(),
                               iter.Get().Width(), iter.Get().Height());
      mTarget->FillRect(drawRect, ColorPattern(Color(0.0, 0.0, 0.0, 1.0)));
      mTargetOnWhite->FillRect(drawRect, ColorPattern(Color(1.0, 1.0, 1.0, 1.0)));
    }
    mTargetOnWhite->SetTransform(oldTransform);
  } else {
    for (auto iter = mDirtyRegion.RectIter(); !iter.Done(); iter.Next()) {
      const gfx::Rect drawRect(iter.Get().X(), iter.Get().Y(),
                               iter.Get().Width(), iter.Get().Height());
      mTarget->ClearRect(drawRect);
    }
  }

  mTarget->SetTransform(oldTransform);
}

StaticAutoPtr<PaintThread> PaintThread::sSingleton;
StaticRefPtr<nsIThread> PaintThread::sThread;
PlatformThreadId PaintThread::sThreadId;

PaintThread::PaintThread()
{
}

void
PaintThread::Release()
{
}

void
PaintThread::AddRef()
{
}

/* static */ int32_t
PaintThread::CalculatePaintWorkerCount()
{
  int32_t cpuCores = 1;
  nsCOMPtr<nsIPropertyBag2> systemInfo = do_GetService(NS_SYSTEMINFO_CONTRACTID);
  if (systemInfo) {
    nsresult rv = systemInfo->GetPropertyAsInt32(NS_LITERAL_STRING("cpucores"), &cpuCores);
    if (NS_FAILED(rv)) {
      cpuCores = 1;
    }
  }

  int32_t workerCount = gfxPrefs::LayersOMTPPaintWorkers();

  // If not manually specified, default to (cpuCores * 3) / 4, and clamp
  // between 1 and 4. If a user wants more, they can manually specify it
  if (workerCount < 1) {
    workerCount = std::min(std::max((cpuCores * 3) / 4, 1), 4);
  }

  return workerCount;
}

/* static */ void
PaintThread::Start()
{
  PaintThread::sSingleton = new PaintThread();

  if (!PaintThread::sSingleton->Init()) {
    gfxCriticalNote << "Unable to start paint thread";
    PaintThread::sSingleton = nullptr;
  }
}

bool
PaintThread::Init()
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<nsIThread> thread;
  nsresult rv = NS_NewNamedThread("PaintThread", getter_AddRefs(thread));
  if (NS_FAILED(rv)) {
    return false;
  }
  sThread = thread;

  // Only create paint workers for tiling if we are using tiling or could
  // expect to dynamically switch to tiling in the future
  if (gfxPlatform::GetPlatform()->UsesTiling() ||
      gfxPrefs::LayersTilesEnabledIfSkiaPOMTP()) {
    int32_t paintWorkerCount = PaintThread::CalculatePaintWorkerCount();
    mPaintWorkers = SharedThreadPool::Get(NS_LITERAL_CSTRING("PaintWorker"), paintWorkerCount);
  }

  nsCOMPtr<nsIRunnable> paintInitTask =
    NewRunnableMethod("PaintThread::InitOnPaintThread",
                      this, &PaintThread::InitOnPaintThread);
  SyncRunnable::DispatchToThread(sThread, paintInitTask);
  return true;
}

void
PaintThread::InitOnPaintThread()
{
  MOZ_ASSERT(!NS_IsMainThread());
  sThreadId = PlatformThread::CurrentId();
}

void
DestroyPaintThread(UniquePtr<PaintThread>&& pt)
{
  MOZ_ASSERT(PaintThread::IsOnPaintThread());
  pt->ShutdownOnPaintThread();
}

/* static */ void
PaintThread::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

  UniquePtr<PaintThread> pt(sSingleton.forget());
  if (!pt) {
    return;
  }

  sThread->Dispatch(NewRunnableFunction("DestroyPaintThreadRunnable",
                                        DestroyPaintThread,
                                        Move(pt)));
  sThread->Shutdown();
  sThread = nullptr;
}

void
PaintThread::ShutdownOnPaintThread()
{
  MOZ_ASSERT(IsOnPaintThread());
}

/* static */ PaintThread*
PaintThread::Get()
{
  MOZ_ASSERT(NS_IsMainThread());
  return PaintThread::sSingleton.get();
}

/* static */ bool
PaintThread::IsOnPaintThread()
{
  return sThreadId == PlatformThread::CurrentId();
}

bool
PaintThread::IsOnPaintWorkerThread()
{
  return mPaintWorkers && mPaintWorkers->IsOnCurrentThread();
}

void
PaintThread::PrepareBuffer(CapturedBufferState* aState)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aState);

  // If painting asynchronously, we need to acquire the compositor bridge which
  // owns the underlying MessageChannel. Otherwise we leave it null and use
  // synchronous dispatch.
  RefPtr<CompositorBridgeChild> cbc(CompositorBridgeChild::Get());
  RefPtr<CapturedBufferState> state(aState);

  cbc->NotifyBeginAsyncPaint(state);

  RefPtr<PaintThread> self = this;
  RefPtr<Runnable> task = NS_NewRunnableFunction("PaintThread::PrepareBuffer",
    [self, cbc, state]() -> void
  {
    self->AsyncPrepareBuffer(cbc,
                             state);
  });

#ifndef OMTP_FORCE_SYNC
  sThread->Dispatch(task.forget());
#else
  SyncRunnable::DispatchToThread(sThread, task);
#endif
}

void
PaintThread::AsyncPrepareBuffer(CompositorBridgeChild* aBridge,
                                CapturedBufferState* aState)
{
  MOZ_ASSERT(IsOnPaintThread());
  MOZ_ASSERT(aState);

  if (!aState->PrepareBuffer()) {
    gfxCriticalNote << "Failed to prepare buffers on the paint thread.";
  }

  if (aBridge->NotifyFinishedAsyncWorkerPaint(aState)) {
    // We need to dispatch this task to ourselves so it runs after
    // AsyncEndLayer
    DispatchEndLayerTransaction(aBridge);
  }
}

void
PaintThread::PaintContents(CapturedPaintState* aState,
                           PrepDrawTargetForPaintingCallback aCallback)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aState);

  if (gfxPrefs::LayersOMTPDumpCapture() && aState->mCapture) {
    aState->mCapture->Dump();
  }

  RefPtr<CompositorBridgeChild> cbc(CompositorBridgeChild::Get());
  RefPtr<CapturedPaintState> state(aState);

  cbc->NotifyBeginAsyncPaint(state);

  RefPtr<PaintThread> self = this;
  RefPtr<Runnable> task = NS_NewRunnableFunction("PaintThread::PaintContents",
    [self, cbc, state, aCallback]() -> void
  {
    self->AsyncPaintContents(cbc,
                             state,
                             aCallback);
  });

#ifndef OMTP_FORCE_SYNC
  sThread->Dispatch(task.forget());
#else
  SyncRunnable::DispatchToThread(sThread, task);
#endif
}

void
PaintThread::AsyncPaintContents(CompositorBridgeChild* aBridge,
                                CapturedPaintState* aState,
                                PrepDrawTargetForPaintingCallback aCallback)
{
  MOZ_ASSERT(IsOnPaintThread());
  MOZ_ASSERT(aState);

  DrawTarget* target = aState->mTargetDual;
  DrawTargetCapture* capture = aState->mCapture;

  Matrix oldTransform = target->GetTransform();
  bool oldPermitsSubpixelAA = target->GetPermitSubpixelAA();

  target->SetTransform(capture->GetTransform());
  target->SetPermitSubpixelAA(capture->GetPermitSubpixelAA());

  if (aCallback(aState)) {
    // Draw all the things into the actual dest target.
    target->DrawCapturedDT(capture, Matrix());

    if (!mDrawTargetsToFlush.Contains(target)) {
      mDrawTargetsToFlush.AppendElement(target);
    }

    if (gfxPrefs::LayersOMTPReleaseCaptureOnMainThread()) {
      // This should ensure the capture drawtarget, which may hold on to UnscaledFont objects,
      // gets destroyed on the main thread (See bug 1404742). This assumes (unflushed) target
      // DrawTargets do not themselves hold on to UnscaledFonts.
      NS_ReleaseOnMainThreadSystemGroup("CapturePaintState::DrawTargetCapture", aState->mCapture.forget());
    }
  }

  target->SetTransform(oldTransform);
  target->SetPermitSubpixelAA(oldPermitsSubpixelAA);

  if (aBridge->NotifyFinishedAsyncWorkerPaint(aState)) {
    // We need to dispatch this task to ourselves so it runs after
    // AsyncEndLayer
    DispatchEndLayerTransaction(aBridge);
  }
}

void
PaintThread::PaintTiledContents(CapturedTiledPaintState* aState)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aState);

  if (gfxPrefs::LayersOMTPDumpCapture() && aState->mCapture) {
    aState->mCapture->Dump();
  }

  RefPtr<CompositorBridgeChild> cbc(CompositorBridgeChild::Get());
  RefPtr<CapturedTiledPaintState> state(aState);

  cbc->NotifyBeginAsyncPaint(state);

  RefPtr<PaintThread> self = this;
  RefPtr<Runnable> task = NS_NewRunnableFunction("PaintThread::PaintTiledContents",
    [self, cbc, state]() -> void
  {
    self->AsyncPaintTiledContents(cbc, state);
  });

#ifndef OMTP_FORCE_SYNC
  mPaintWorkers->Dispatch(task.forget());
#else
  SyncRunnable::DispatchToThread(mPaintWorkers, task);
#endif
}

void
PaintThread::AsyncPaintTiledContents(CompositorBridgeChild* aBridge,
                                     CapturedTiledPaintState* aState)
{
  MOZ_ASSERT(IsOnPaintWorkerThread());
  MOZ_ASSERT(aState);

  for (auto& copy : aState->mCopies) {
    copy.CopyBuffer();
  }

  for (auto& clear : aState->mClears) {
    clear.ClearBuffer();
  }

  DrawTarget* target = aState->mTarget;
  DrawTargetCapture* capture = aState->mCapture;

  // Draw all the things into the actual dest target.
  target->DrawCapturedDT(capture, Matrix());
  target->Flush();

  if (gfxPrefs::LayersOMTPReleaseCaptureOnMainThread()) {
    // This should ensure the capture drawtarget, which may hold on to UnscaledFont objects,
    // gets destroyed on the main thread (See bug 1404742). This assumes (unflushed) target
    // DrawTargets do not themselves hold on to UnscaledFonts.
    NS_ReleaseOnMainThreadSystemGroup("CapturePaintState::DrawTargetCapture", aState->mCapture.forget());
  }

  {
    RefPtr<CompositorBridgeChild> cbc(aBridge);
    RefPtr<CapturedTiledPaintState> state(aState);

    RefPtr<PaintThread> self = this;
    RefPtr<Runnable> task = NS_NewRunnableFunction("PaintThread::AsyncPaintTiledContentsFinished",
      [self, cbc, state]() -> void
    {
      self->AsyncPaintTiledContentsFinished(cbc, state);
    });

  #ifndef OMTP_FORCE_SYNC
    sThread->Dispatch(task.forget());
  #else
    SyncRunnable::DispatchToThread(sThread, task);
  #endif
  }
}

void
PaintThread::AsyncPaintTiledContentsFinished(CompositorBridgeChild* aBridge,
                                             CapturedTiledPaintState* aState)
{
  MOZ_ASSERT(IsOnPaintThread());
  if (aBridge->NotifyFinishedAsyncWorkerPaint(aState)) {
    aBridge->NotifyFinishedAsyncEndLayerTransaction();
  }
}

void
PaintThread::EndLayer()
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<PaintThread> self = this;
  RefPtr<Runnable> task = NS_NewRunnableFunction("PaintThread::AsyncEndLayer",
  [self]() -> void
  {
    self->AsyncEndLayer();
  });

#ifndef OMTP_FORCE_SYNC
  sThread->Dispatch(task.forget());
#else
  SyncRunnable::DispatchToThread(sThread, task);
#endif
}

void
PaintThread::Dispatch(RefPtr<Runnable>& aRunnable)
{
#ifndef OMTP_FORCE_SYNC
  sThread->Dispatch(aRunnable.forget());
#else
  SyncRunnable::DispatchToThread(sThread, aRunnable);
#endif
}

void
PaintThread::AsyncEndLayer()
{
  MOZ_ASSERT(IsOnPaintThread());

  // Textureclient forces a flush once we "end paint", so
  // users of this texture expect all the drawing to be complete.
  // Force a flush now.
  for (size_t i = 0; i < mDrawTargetsToFlush.Length(); i++) {
    mDrawTargetsToFlush[i]->Flush();
  }

  mDrawTargetsToFlush.Clear();
}

void
PaintThread::EndLayerTransaction(SyncObjectClient* aSyncObject)
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<CompositorBridgeChild> cbc(CompositorBridgeChild::Get());

  if (cbc->NotifyBeginAsyncEndLayerTransaction(aSyncObject)) {
    RefPtr<PaintThread> self = this;
    RefPtr<Runnable> task = NS_NewRunnableFunction("PaintThread::AsyncEndLayerTransaction",
      [self, cbc]() -> void
    {
      self->AsyncEndLayerTransaction(cbc);
    });

  #ifndef OMTP_FORCE_SYNC
    sThread->Dispatch(task.forget());
  #else
    SyncRunnable::DispatchToThread(sThread, task);
  #endif
  }
}

void
PaintThread::AsyncEndLayerTransaction(CompositorBridgeChild* aBridge)
{
  MOZ_ASSERT(IsOnPaintThread());

  aBridge->NotifyFinishedAsyncEndLayerTransaction();
}

void
PaintThread::DispatchEndLayerTransaction(CompositorBridgeChild* aBridge)
{
  MOZ_ASSERT(IsOnPaintThread());

  RefPtr<CompositorBridgeChild> cbc = aBridge;
  RefPtr<PaintThread> self = this;
  RefPtr<Runnable> task = NS_NewRunnableFunction("PaintThread::AsyncEndLayerTransaction",
    [self, cbc]() -> void
  {
    self->AsyncEndLayerTransaction(cbc);
  });

  sThread->Dispatch(task.forget());
}

} // namespace layers
} // namespace mozilla
