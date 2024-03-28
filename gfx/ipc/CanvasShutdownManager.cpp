/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasShutdownManager.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/dom/CanvasRenderingContext2D.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/gfx/CanvasManagerChild.h"

using namespace mozilla::dom;

namespace mozilla::gfx {

// The owning thread will tell us to close when it is shutdown, either via
// CanvasShutdownManager::Shutdown for the main thread, or via a shutdown
// callback from ThreadSafeWorkerRef for worker threads.
MOZ_THREAD_LOCAL(CanvasShutdownManager*) CanvasShutdownManager::sLocalManager;

CanvasShutdownManager::CanvasShutdownManager(StrongWorkerRef* aWorkerRef)
    : mWorkerRef(new ThreadSafeWorkerRef(aWorkerRef)) {}

CanvasShutdownManager::CanvasShutdownManager() = default;
CanvasShutdownManager::~CanvasShutdownManager() = default;

void CanvasShutdownManager::Destroy() {
  std::set<CanvasRenderingContext2D*> activeCanvas = std::move(mActiveCanvas);
  for (const auto& i : activeCanvas) {
    i->OnShutdown();
  }

  CanvasManagerChild::Shutdown();
  mWorkerRef = nullptr;
}

/* static */ void CanvasShutdownManager::Shutdown() {
  auto* manager = MaybeGet();
  if (!manager) {
    return;
  }

  sLocalManager.set(nullptr);
  manager->Destroy();
  delete manager;
}

/* static */ CanvasShutdownManager* CanvasShutdownManager::MaybeGet() {
  if (NS_WARN_IF(!sLocalManager.init())) {
    return nullptr;
  }

  return sLocalManager.get();
}

/* static */ CanvasShutdownManager* CanvasShutdownManager::Get() {
  if (NS_WARN_IF(!sLocalManager.init())) {
    return nullptr;
  }

  CanvasShutdownManager* managerWeak = sLocalManager.get();
  if (managerWeak) {
    return managerWeak;
  }

  if (WorkerPrivate* worker = GetCurrentThreadWorkerPrivate()) {
    // The ThreadSafeWorkerRef will let us know when the worker is shutting
    // down. This will let us clear our threadlocal reference and close the
    // actor. We rely upon an explicit shutdown for the main thread.
    RefPtr<StrongWorkerRef> workerRef = StrongWorkerRef::Create(
        worker, "CanvasShutdownManager", []() { Shutdown(); });
    if (NS_WARN_IF(!workerRef)) {
      return nullptr;
    }

    CanvasShutdownManager* manager = new CanvasShutdownManager(workerRef);
    sLocalManager.set(manager);
    return manager;
  }

  if (NS_IsMainThread()) {
    if (NS_WARN_IF(
            AppShutdown::IsInOrBeyond(ShutdownPhase::AppShutdownConfirmed))) {
      return nullptr;
    }

    CanvasShutdownManager* manager = new CanvasShutdownManager();
    sLocalManager.set(manager);
    return manager;
  }

  MOZ_ASSERT_UNREACHABLE("Can only be used on main or DOM worker threads!");
  return nullptr;
}

void CanvasShutdownManager::AddShutdownObserver(
    dom::CanvasRenderingContext2D* aCanvas) {
  mActiveCanvas.insert(aCanvas);
}

void CanvasShutdownManager::RemoveShutdownObserver(
    dom::CanvasRenderingContext2D* aCanvas) {
  mActiveCanvas.erase(aCanvas);
}

}  // namespace mozilla::gfx
