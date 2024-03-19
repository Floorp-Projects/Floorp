/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_gfx_ipc_CanvasShutdownManager_h__
#define _include_gfx_ipc_CanvasShutdownManager_h__

#include "mozilla/RefPtr.h"
#include "mozilla/ThreadLocal.h"
#include <set>

namespace mozilla {
namespace dom {
class CanvasRenderingContext2D;
class StrongWorkerRef;
class ThreadSafeWorkerRef;
}  // namespace dom

namespace gfx {

class CanvasShutdownManager final {
 public:
  static CanvasShutdownManager* Get();
  static CanvasShutdownManager* MaybeGet();
  static void Shutdown();

  dom::ThreadSafeWorkerRef* GetWorkerRef() const { return mWorkerRef; }
  void AddShutdownObserver(dom::CanvasRenderingContext2D* aCanvas);
  void RemoveShutdownObserver(dom::CanvasRenderingContext2D* aCanvas);

 private:
  explicit CanvasShutdownManager(dom::StrongWorkerRef* aWorkerRef);
  CanvasShutdownManager();
  ~CanvasShutdownManager();
  void Destroy();

  RefPtr<dom::ThreadSafeWorkerRef> mWorkerRef;
  std::set<dom::CanvasRenderingContext2D*> mActiveCanvas;
  static MOZ_THREAD_LOCAL(CanvasShutdownManager*) sLocalManager;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // _include_gfx_ipc_CanvasShutdownManager_h__
