/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOM_OFFSCREENCANVASRENDERINGCONTEXT2D_H_
#define MOZILLA_DOM_OFFSCREENCANVASRENDERINGCONTEXT2D_H_

#include "mozilla/RefPtr.h"
#include "mozilla/dom/CanvasRenderingContext2D.h"

struct JSContext;
class nsIGlobalObject;

namespace mozilla::dom {
class OffscreenCanvas;
class OffscreenCanvasShutdownObserver;
class WeakWorkerRef;

class OffscreenCanvasRenderingContext2D final
    : public CanvasRenderingContext2D {
 public:
  // nsISupports interface + CC
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_INHERITED(
      OffscreenCanvasRenderingContext2D, CanvasRenderingContext2D)

  explicit OffscreenCanvasRenderingContext2D(
      layers::LayersBackend aCompositorBackend);

  nsIGlobalObject* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  OffscreenCanvas* Canvas() { return mOffscreenCanvas; }
  const OffscreenCanvas* Canvas() const { return mOffscreenCanvas; }

  void Commit(ErrorResult& aRv);

  void OnShutdown() override;

  NS_IMETHOD InitializeWithDrawTarget(
      nsIDocShell* aShell, NotNull<gfx::DrawTarget*> aTarget) override;

 private:
  void AddShutdownObserver() override;
  void RemoveShutdownObserver() override;
  bool AlreadyShutDown() const override {
    return !mOffscreenShutdownObserver &&
           CanvasRenderingContext2D::AlreadyShutDown();
  }

  void AddZoneWaitingForGC() override;
  void AddAssociatedMemory() override;
  void RemoveAssociatedMemory() override;

  ~OffscreenCanvasRenderingContext2D() override;

  RefPtr<OffscreenCanvasShutdownObserver> mOffscreenShutdownObserver;
  RefPtr<WeakWorkerRef> mWorkerRef;
};

size_t BindingJSObjectMallocBytes(OffscreenCanvasRenderingContext2D* aContext);

}  // namespace mozilla::dom

#endif  // MOZILLA_DOM_OFFSCREENCANVASRENDERINGCONTEXT2D_H_
