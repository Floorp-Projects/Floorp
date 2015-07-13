/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_Feature_h
#define mozilla_dom_cache_Feature_h

#include "nsISupportsImpl.h"
#include "nsTArray.h"
#include "WorkerFeature.h"

namespace mozilla {

namespace workers {
class WorkerPrivate;
} // namespace workers

namespace dom {
namespace cache {

class ActorChild;

class Feature final : public workers::WorkerFeature
{
public:
  static already_AddRefed<Feature> Create(workers::WorkerPrivate* aWorkerPrivate);

  void AddActor(ActorChild* aActor);
  void RemoveActor(ActorChild* aActor);

  bool Notified() const;

  // WorkerFeature methods
  virtual bool Notify(JSContext* aCx, workers::Status aStatus) override;

private:
  explicit Feature(workers::WorkerPrivate *aWorkerPrivate);
  ~Feature();

  workers::WorkerPrivate* mWorkerPrivate;
  nsTArray<ActorChild*> mActorList;
  bool mNotified;

public:
  NS_INLINE_DECL_REFCOUNTING(mozilla::dom::cache::Feature)
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_Feature_h
