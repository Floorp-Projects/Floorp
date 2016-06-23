/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_ActioChild_h
#define mozilla_dom_cache_ActioChild_h

#include "mozilla/RefPtr.h"

namespace mozilla {
namespace dom {
namespace cache {

class CacheWorkerHolder;

class ActorChild
{
public:
  virtual void
  StartDestroy() = 0;

  void
  SetWorkerHolder(CacheWorkerHolder* aWorkerHolder);

  void
  RemoveWorkerHolder();

  CacheWorkerHolder*
  GetWorkerHolder() const;

  bool
  WorkerHolderNotified() const;

protected:
  ActorChild();
  ~ActorChild();

private:
  RefPtr<CacheWorkerHolder> mWorkerHolder;
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_ActioChild_h
