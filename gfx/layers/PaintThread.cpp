/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PaintThread.h"

#include "base/task.h"
#include "gfxPrefs.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/ShadowLayers.h"
#include "mozilla/layers/SyncObject.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/Preferences.h"
#include "mozilla/SyncRunnable.h"

namespace mozilla {
namespace layers {

using namespace gfx;

StaticAutoPtr<PaintThread> PaintThread::sSingleton;
StaticRefPtr<nsIThread> PaintThread::sThread;
PlatformThreadId PaintThread::sThreadId;

// RAII make sure we clean up and restore our draw targets
// when we paint async.
struct MOZ_STACK_CLASS AutoCapturedPaintSetup
{
  AutoCapturedPaintSetup(CapturedPaintState* aState, CompositorBridgeChild* aBridge)
  : mState(aState)
  , mTarget(aState->mTarget)
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

    if (mBridge) {
      mBridge->NotifyFinishedAsyncPaint(mState);
    }
  }

  RefPtr<CapturedPaintState> mState;
  RefPtr<DrawTarget> mTarget;
  bool mRestorePermitsSubpixelAA;
  Matrix mOldTransform;
  RefPtr<CompositorBridgeChild> mBridge;
};

void
PaintThread::Release()
{
}

void
PaintThread::AddRef()
{
}

void
PaintThread::InitOnPaintThread()
{
  MOZ_ASSERT(!NS_IsMainThread());
  sThreadId = PlatformThread::CurrentId();
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

/* static */ void
PaintThread::Start()
{
  PaintThread::sSingleton = new PaintThread();

  if (!PaintThread::sSingleton->Init()) {
    gfxCriticalNote << "Unable to start paint thread";
    PaintThread::sSingleton = nullptr;
  }
}

/* static */ PaintThread*
PaintThread::Get()
{
  MOZ_ASSERT(NS_IsMainThread());
  return PaintThread::sSingleton.get();
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

/* static */ bool
PaintThread::IsOnPaintThread()
{
  return sThreadId == PlatformThread::CurrentId();
}

void
PaintThread::PaintContentsAsync(CompositorBridgeChild* aBridge,
                                CapturedPaintState* aState,
                                PrepDrawTargetForPaintingCallback aCallback)
{
  MOZ_ASSERT(IsOnPaintThread());
  MOZ_ASSERT(aState);

  DrawTarget* target = aState->mTarget;
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
}

void
PaintThread::FinishedLayerBatch()
{
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<CompositorBridgeChild> cbc;
  if (!gfxPrefs::LayersOMTPForceSync()) {
    cbc = CompositorBridgeChild::Get();
  }

  RefPtr<PaintThread> self = this;
  RefPtr<Runnable> task = NS_NewRunnableFunction("PaintThread::EndAsyncPainting",
  [self, cbc]() -> void
  {
    self->EndAsyncPainting(cbc);
  });

  if (cbc) {
    sThread->Dispatch(task.forget());
  } else {
    SyncRunnable::DispatchToThread(sThread, task);
  }
}

void
PaintThread::EndAsyncPainting(CompositorBridgeChild* aBridge)
{
  MOZ_ASSERT(IsOnPaintThread());
  // Textureclient forces a flush once we "end paint", so
  // users of this texture expect all the drawing to be complete.
  // Force a flush now.
  for (size_t i = 0; i < mDrawTargetsToFlush.Length(); i++) {
    mDrawTargetsToFlush[i]->Flush();
  }

  mDrawTargetsToFlush.Clear();

  if (aBridge) {
    aBridge->NotifyFinishedAsyncPaintLayer();
  }
}

void
PaintThread::SynchronizePaintTextures(SyncObjectClient* aSyncObject)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aSyncObject);

  RefPtr<SyncObjectClient> syncObject(aSyncObject);
  RefPtr<PaintThread> self = this;
  RefPtr<Runnable> task = NS_NewRunnableFunction("PaintThread::SyncTextureData",
    [self, syncObject]() -> void
  {
    self->SyncTextureData(syncObject);
  });

  if (!gfxPrefs::LayersOMTPForceSync()) {
    sThread->Dispatch(task.forget());
  } else {
    SyncRunnable::DispatchToThread(sThread, task);
  }
}

void
PaintThread::SyncTextureData(SyncObjectClient* aSyncObject)
{
  MOZ_ASSERT(IsOnPaintThread());
  MOZ_ASSERT(aSyncObject);

  aSyncObject->Synchronize();
}

void
PaintThread::PaintContents(CapturedPaintState* aState,
                           PrepDrawTargetForPaintingCallback aCallback)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aState);

  // If painting asynchronously, we need to acquire the compositor bridge which
  // owns the underlying MessageChannel. Otherwise we leave it null and use
  // synchronous dispatch.
  RefPtr<CompositorBridgeChild> cbc;
  if (!gfxPrefs::LayersOMTPForceSync()) {
    cbc = CompositorBridgeChild::Get();
    cbc->NotifyBeginAsyncPaint(aState);
  }
  RefPtr<CapturedPaintState> state(aState);
  RefPtr<DrawTargetCapture> capture(aState->mCapture);

  RefPtr<PaintThread> self = this;
  RefPtr<Runnable> task = NS_NewRunnableFunction("PaintThread::PaintContents",
    [self, cbc, capture, state, aCallback]() -> void
  {
    self->PaintContentsAsync(cbc,
                             state,
                             aCallback);
  });

  if (cbc) {
    sThread->Dispatch(task.forget());
  } else {
    SyncRunnable::DispatchToThread(sThread, task);
  }
}

} // namespace layers
} // namespace mozilla
