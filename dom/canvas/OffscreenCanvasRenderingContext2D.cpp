/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OffscreenCanvasRenderingContext2D.h"
#include "mozilla/dom/OffscreenCanvasRenderingContext2DBinding.h"
#include "mozilla/dom/OffscreenCanvas.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerRef.h"

namespace mozilla::dom {

class OffscreenCanvasShutdownObserver final {
  NS_INLINE_DECL_REFCOUNTING(OffscreenCanvasShutdownObserver)

 public:
  explicit OffscreenCanvasShutdownObserver(
      OffscreenCanvasRenderingContext2D* aOwner)
      : mOwner(aOwner) {}

  void OnShutdown() {
    if (mOwner) {
      mOwner->OnShutdown();
      mOwner = nullptr;
    }
  }

  void ClearOwner() { mOwner = nullptr; }

 private:
  ~OffscreenCanvasShutdownObserver() = default;

  OffscreenCanvasRenderingContext2D* mOwner;
};

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_INHERITED(
    OffscreenCanvasRenderingContext2D, CanvasRenderingContext2D)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(OffscreenCanvasRenderingContext2D)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END_INHERITING(CanvasRenderingContext2D)

NS_IMPL_ADDREF_INHERITED(OffscreenCanvasRenderingContext2D,
                         CanvasRenderingContext2D)
NS_IMPL_RELEASE_INHERITED(OffscreenCanvasRenderingContext2D,
                          CanvasRenderingContext2D)

OffscreenCanvasRenderingContext2D::OffscreenCanvasRenderingContext2D(
    layers::LayersBackend aCompositorBackend)
    : CanvasRenderingContext2D(aCompositorBackend) {}

OffscreenCanvasRenderingContext2D::~OffscreenCanvasRenderingContext2D() =
    default;

JSObject* OffscreenCanvasRenderingContext2D::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return OffscreenCanvasRenderingContext2D_Binding::Wrap(aCx, this,
                                                         aGivenProto);
}

nsIGlobalObject* OffscreenCanvasRenderingContext2D::GetParentObject() const {
  return mOffscreenCanvas->GetOwnerGlobal();
}

void OffscreenCanvasRenderingContext2D::AddShutdownObserver() {
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  if (!workerPrivate) {
    // We may be using OffscreenCanvas on the main thread.
    CanvasRenderingContext2D::AddShutdownObserver();
    return;
  }

  mOffscreenShutdownObserver =
      MakeAndAddRef<OffscreenCanvasShutdownObserver>(this);
  mWorkerRef = WeakWorkerRef::Create(
      workerPrivate,
      [observer = mOffscreenShutdownObserver] { observer->OnShutdown(); });
}

void OffscreenCanvasRenderingContext2D::RemoveShutdownObserver() {
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  if (!workerPrivate) {
    // We may be using OffscreenCanvas on the main thread.
    CanvasRenderingContext2D::RemoveShutdownObserver();
    return;
  }

  if (mOffscreenShutdownObserver) {
    mOffscreenShutdownObserver->ClearOwner();
  }
  mOffscreenShutdownObserver = nullptr;
  mWorkerRef = nullptr;
}

void OffscreenCanvasRenderingContext2D::OnShutdown() {
  if (mOffscreenShutdownObserver) {
    mOffscreenShutdownObserver->ClearOwner();
    mOffscreenShutdownObserver = nullptr;
  }

  CanvasRenderingContext2D::OnShutdown();
}

void OffscreenCanvasRenderingContext2D::Commit(ErrorResult& aRv) {
  if (!mOffscreenCanvas->IsTransferredFromElement()) {
    aRv.ThrowInvalidStateError(
        "Cannot commit on an OffscreenCanvas that is not transferred from an "
        "HTMLCanvasElement.");
    return;
  }

  mOffscreenCanvas->CommitFrameToCompositor();
}

}  // namespace mozilla::dom
