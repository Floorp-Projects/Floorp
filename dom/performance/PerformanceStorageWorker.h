/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PerformanceStorageWorker_h
#define mozilla_dom_PerformanceStorageWorker_h

#include "mozilla/Mutex.h"
#include "PerformanceStorage.h"

namespace mozilla {
namespace dom {

class WeakWorkerRef;
class WorkerPrivate;

class PerformanceProxyData;

class PerformanceStorageWorker final : public PerformanceStorage {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PerformanceStorageWorker, override)

  static already_AddRefed<PerformanceStorageWorker> Create(
      WorkerPrivate* aWorkerPrivate);

  void ShutdownOnWorker();

  void AddEntry(nsIHttpChannel* aChannel,
                nsITimedChannel* aTimedChannel) override;
  void AddEntry(const nsString& aEntryName, const nsString& aInitiatorType,
                UniquePtr<PerformanceTimingData>&& aData) override;
  void AddEntryOnWorker(UniquePtr<PerformanceProxyData>&& aData);

 private:
  PerformanceStorageWorker();
  ~PerformanceStorageWorker();

  Mutex mMutex;

  // Protected by mutex.
  // Created and released on worker-thread. Used also on main-thread.
  RefPtr<WeakWorkerRef> mWorkerRef;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_PerformanceStorageWorker_h
