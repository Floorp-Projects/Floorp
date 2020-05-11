/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheWorkerRef_h
#define mozilla_dom_cache_CacheWorkerRef_h

#include "mozilla/dom/SafeRefPtr.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

class IPCWorkerRef;
class StrongWorkerRef;
class WorkerPrivate;

namespace cache {

class ActorChild;

class CacheWorkerRef final : public SafeRefCounted<CacheWorkerRef> {
 public:
  enum Behavior {
    eStrongWorkerRef,
    eIPCWorkerRef,
  };

  static SafeRefPtr<CacheWorkerRef> Create(WorkerPrivate* aWorkerPrivate,
                                           Behavior aBehavior);

  static SafeRefPtr<CacheWorkerRef> PreferBehavior(
      SafeRefPtr<CacheWorkerRef> aCurrentRef, Behavior aBehavior);

  void AddActor(ActorChild* aActor);
  void RemoveActor(ActorChild* aActor);

  bool Notified() const;

 private:
  struct ConstructorGuard {};

  void Notify();

  nsTArray<ActorChild*> mActorList;

  Behavior mBehavior;
  bool mNotified;

  RefPtr<StrongWorkerRef> mStrongWorkerRef;
  RefPtr<IPCWorkerRef> mIPCWorkerRef;

 public:
  CacheWorkerRef(Behavior aBehavior, ConstructorGuard);

  ~CacheWorkerRef();

  NS_DECL_OWNINGTHREAD
  MOZ_DECLARE_REFCOUNTED_TYPENAME(mozilla::dom::cache::CacheWorkerRef)
};

}  // namespace cache
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_cache_CacheWorkerRef_h
