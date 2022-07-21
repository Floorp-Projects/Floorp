/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_ActioChild_h
#define mozilla_dom_cache_ActioChild_h

#include "mozilla/dom/SafeRefPtr.h"

namespace mozilla::dom::cache {

class CacheWorkerRef;

class ActorChild {
 public:
  virtual void StartDestroy() = 0;
  virtual void NoteDeletedActor() { /*no-op*/
  }

  void SetWorkerRef(SafeRefPtr<CacheWorkerRef> aWorkerRef);

  void RemoveWorkerRef();

  const SafeRefPtr<CacheWorkerRef>& GetWorkerRefPtr() const;

  bool WorkerRefNotified() const;

 protected:
  ActorChild();
  ~ActorChild();

 private:
  SafeRefPtr<CacheWorkerRef> mWorkerRef;
};

}  // namespace mozilla::dom::cache

#endif  // mozilla_dom_cache_ActioChild_h
