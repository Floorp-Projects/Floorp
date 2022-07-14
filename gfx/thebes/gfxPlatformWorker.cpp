/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxPlatformWorker.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/ThreadLocal.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;

MOZ_THREAD_LOCAL(gfxPlatformWorker*) gfxPlatformWorker::sInstance;

/* static */ gfxPlatformWorker* gfxPlatformWorker::Get() {
  if (!sInstance.init()) {
    return nullptr;
  }

  gfxPlatformWorker* instance = sInstance.get();
  if (instance) {
    return instance;
  }

  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  if (!workerPrivate) {
    return nullptr;
  }

  RefPtr<WeakWorkerRef> workerRef = WeakWorkerRef::Create(
      workerPrivate, []() { gfxPlatformWorker::Shutdown(); });
  if (!workerRef) {
    return nullptr;
  }

  instance = new gfxPlatformWorker(std::move(workerRef));
  sInstance.set(instance);
  return instance;
}

/* static */ void gfxPlatformWorker::Shutdown() {
  if (!sInstance.init()) {
    return;
  }

  gfxPlatformWorker* instance = sInstance.get();
  if (!instance) {
    return;
  }

  sInstance.set(nullptr);
  delete instance;
}

gfxPlatformWorker::gfxPlatformWorker(RefPtr<WeakWorkerRef>&& aWorkerRef)
    : mWorkerRef(std::move(aWorkerRef)) {}

gfxPlatformWorker::~gfxPlatformWorker() = default;

RefPtr<mozilla::gfx::DrawTarget>
gfxPlatformWorker::ScreenReferenceDrawTarget() {
  if (!mScreenReferenceDrawTarget) {
    mScreenReferenceDrawTarget = Factory::CreateDrawTarget(
        BackendType::SKIA, IntSize(1, 1), SurfaceFormat::B8G8R8A8);
  }
  return mScreenReferenceDrawTarget;
}
