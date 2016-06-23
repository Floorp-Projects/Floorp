/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheWorkerHolder_h
#define mozilla_dom_cache_CacheWorkerHolder_h

#include "nsISupportsImpl.h"
#include "nsTArray.h"
#include "WorkerHolder.h"

namespace mozilla {

namespace workers {
class WorkerPrivate;
} // namespace workers

namespace dom {
namespace cache {

class ActorChild;

class CacheWorkerHolder final : public workers::WorkerHolder
{
public:
  static already_AddRefed<CacheWorkerHolder>
  Create(workers::WorkerPrivate* aWorkerPrivate);

  void AddActor(ActorChild* aActor);
  void RemoveActor(ActorChild* aActor);

  bool Notified() const;

  // WorkerHolder methods
  virtual bool Notify(workers::Status aStatus) override;

private:
  CacheWorkerHolder();
  ~CacheWorkerHolder();

  nsTArray<ActorChild*> mActorList;
  bool mNotified;

public:
  NS_INLINE_DECL_REFCOUNTING(mozilla::dom::cache::CacheWorkerHolder)
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CacheWorkerHolder_h
