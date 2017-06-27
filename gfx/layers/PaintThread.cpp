/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PaintThread.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/SyncRunnable.h"

namespace mozilla {
namespace layers {

using namespace gfx;

StaticAutoPtr<PaintThread> PaintThread::sSingleton;

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
  mThreadId = PlatformThread::CurrentId();
}

bool
PaintThread::Init()
{
  MOZ_ASSERT(NS_IsMainThread());
  nsresult rv = NS_NewNamedThread("PaintThread", getter_AddRefs(mThread));

  if (NS_FAILED(rv)) {
    return false;
  }

  nsCOMPtr<nsIRunnable> paintInitTask =
    NewRunnableMethod("PaintThread::InitOnPaintThread",
                      this, &PaintThread::InitOnPaintThread);

  SyncRunnable::DispatchToThread(PaintThread::sSingleton->mThread, paintInitTask);
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

/* static */ void
PaintThread::Shutdown()
{
  if (!PaintThread::sSingleton) {
    return;
  }

  PaintThread::sSingleton->ShutdownImpl();
  PaintThread::sSingleton = nullptr;
}

void
PaintThread::ShutdownImpl()
{
  MOZ_ASSERT(NS_IsMainThread());
  PaintThread::sSingleton->mThread->AsyncShutdown();
}

bool
PaintThread::IsOnPaintThread()
{
  MOZ_ASSERT(mThread);
  return PlatformThread::CurrentId() == mThreadId;
}

void
PaintThread::PaintContents(DrawTargetCapture* aCapture,
                           DrawTarget* aTarget)
{
  if (!IsOnPaintThread()) {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIRunnable> paintTask =
      NewRunnableMethod<DrawTargetCapture*, DrawTarget*>("PaintThread::PaintContents",
                                                         this,
                                                         &PaintThread::PaintContents,
                                                         aCapture, aTarget);

    SyncRunnable::DispatchToThread(mThread, paintTask);
    return;
  }

  // Draw all the things into the actual dest target.
  aTarget->DrawCapturedDT(aCapture, Matrix());
}

} // namespace layers
} // namespace mozilla
