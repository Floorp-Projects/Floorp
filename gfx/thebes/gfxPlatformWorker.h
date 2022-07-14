/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PLATFORM_WORKER_H
#define GFX_PLATFORM_WORKER_H

#include "mozilla/ThreadLocal.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace dom {
class WeakWorkerRef;
}  // namespace dom

namespace gfx {
class DrawTarget;
}  // namespace gfx
}  // namespace mozilla

/**
 * Threadlocal instance of gfxPlatform data that may be used/shared on a DOM
 * worker thread.
 */
class gfxPlatformWorker final {
 public:
  static gfxPlatformWorker* Get();
  static void Shutdown();

  RefPtr<mozilla::gfx::DrawTarget> ScreenReferenceDrawTarget();

 private:
  explicit gfxPlatformWorker(RefPtr<mozilla::dom::WeakWorkerRef>&& aWorkerRef);
  ~gfxPlatformWorker();

  static MOZ_THREAD_LOCAL(gfxPlatformWorker*) sInstance;

  RefPtr<mozilla::dom::WeakWorkerRef> mWorkerRef;

  RefPtr<mozilla::gfx::DrawTarget> mScreenReferenceDrawTarget;
};

#endif  // GFX_PLATFORM_WORKER_H
