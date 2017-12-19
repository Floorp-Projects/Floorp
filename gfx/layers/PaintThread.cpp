/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PaintThread.h"

#include "base/task.h"
#include "gfxPrefs.h"
#include "GeckoProfiler.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/ShadowLayers.h"
#include "mozilla/layers/SyncObject.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/Preferences.h"
#include "mozilla/SyncRunnable.h"

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

// RAII make sure we clean up and restore our draw targets
// when we paint async.
struct MOZ_STACK_CLASS AutoCapturedPaintSetup
{
  AutoCapturedPaintSetup(CapturedPaintState* aState, CompositorBridgeChild* aBridge)
  : mState(aState)
  , mTarget(aState->mTargetDual)
  , mRestorePermitsSubpixelAA(mTarget->GetPermitSubpixelAA())
  , mOldTransform(mTarget->GetTransform())
  , mBridge(aBridge)
  {
    mTarget->SetTransform(aState->mCapture->GetTransform());
    mTarget->SetPermitSubpixelAA(aState->mCapture->GetPermitSubpixelAA());
  }

  ~AutoCapturedPaintSetup()
  {
    mTarget->SetTransform(mOldTransform);
    mTarget->SetPermitSubpixelAA(mRestorePermitsSubpixelAA);
    mBridge->NotifyFinishedAsyncPaint(mState);
  }

  RefPtr<CapturedPaintState> mState;
  RefPtr<DrawTarget> mTarget;
  bool mRestorePermitsSubpixelAA;
  Matrix mOldTransform;
  RefPtr<CompositorBridgeChild> mBridge;
};

PaintThread::PaintThread()
  : mInAsyncPaintGroup(false)
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

  sThread->Dispatch(NewRunnableFunction(DestroyPaintThread, Move(pt)));
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

void
PaintThread::BeginLayerTransaction()
{
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(!mInAsyncPaintGroup);
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

  if (!mInAsyncPaintGroup) {
    mInAsyncPaintGroup = true;
    PROFILER_TRACING("Paint", "Rasterize", TRACING_INTERVAL_START);
  }

  if (!aState->PrepareBuffer()) {
    gfxCriticalNote << "Failed to prepare buffers on the paint thread.";
  }

  aBridge->NotifyFinishedAsyncPaint(aState);
}

void
PaintThread::PaintContents(CapturedPaintState* aState,
                           PrepDrawTargetForPaintingCallback aCallback)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aState);

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

  if (!mInAsyncPaintGroup) {
    mInAsyncPaintGroup = true;
    PROFILER_TRACING("Paint", "Rasterize", TRACING_INTERVAL_START);
  }

  DrawTarget* target = aState->mTargetDual;
  DrawTargetCapture* capture = aState->mCapture;

  AutoCapturedPaintSetup setup(aState, aBridge);

  if (!aCallback(aState)) {
    return;
  }

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

void
PaintThread::PaintTiledContents(CapturedTiledPaintState* aState)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aState);

  RefPtr<CompositorBridgeChild> cbc(CompositorBridgeChild::Get());
  RefPtr<CapturedTiledPaintState> state(aState);

  cbc->NotifyBeginAsyncPaint(state);

  RefPtr<PaintThread> self = this;
  RefPtr<Runnable> task = NS_NewRunnableFunction("PaintThread::PaintTiledContents",
    [self, cbc, state]() -> void
  {
    self->AsyncPaintTiledContents(cbc,
                                  state);
  });

#ifndef OMTP_FORCE_SYNC
  sThread->Dispatch(task.forget());
#else
  SyncRunnable::DispatchToThread(sThread, task);
#endif
}

void
PaintThread::AsyncPaintTiledContents(CompositorBridgeChild* aBridge,
                                     CapturedTiledPaintState* aState)
{
  MOZ_ASSERT(IsOnPaintThread());
  MOZ_ASSERT(aState);

  if (!mInAsyncPaintGroup) {
    mInAsyncPaintGroup = true;
    PROFILER_TRACING("Paint", "Rasterize", TRACING_INTERVAL_START);
  }

  for (auto& copy : aState->mCopies) {
    copy.CopyBuffer();
  }

  for (auto& clear : aState->mClears) {
    clear.ClearBuffer();
  }

  DrawTarget* target = aState->mTargetTiled;
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

  aBridge->NotifyFinishedAsyncPaint(aState);
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
  RefPtr<SyncObjectClient> syncObject(aSyncObject);

  cbc->NotifyBeginAsyncEndLayerTransaction();

  RefPtr<PaintThread> self = this;
  RefPtr<Runnable> task = NS_NewRunnableFunction("PaintThread::AsyncEndLayerTransaction",
    [self, cbc, syncObject]() -> void
  {
    self->AsyncEndLayerTransaction(cbc, syncObject);
  });

#ifndef OMTP_FORCE_SYNC
  sThread->Dispatch(task.forget());
#else
  SyncRunnable::DispatchToThread(sThread, task);
#endif
}

void
PaintThread::AsyncEndLayerTransaction(CompositorBridgeChild* aBridge,
                                      SyncObjectClient* aSyncObject)
{
  MOZ_ASSERT(IsOnPaintThread());
  MOZ_ASSERT(mInAsyncPaintGroup);

  if (aSyncObject) {
    aSyncObject->Synchronize();
  }

  mInAsyncPaintGroup = false;
  PROFILER_TRACING("Paint", "Rasterize", TRACING_INTERVAL_END);

  aBridge->NotifyFinishedAsyncEndLayerTransaction();
}

} // namespace layers
} // namespace mozilla
