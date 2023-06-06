/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceWorker.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/StaticPrefs_dom.h"

namespace mozilla::dom {

PerformanceWorker::PerformanceWorker(WorkerGlobalScope* aGlobalScope)
    : Performance(aGlobalScope) {
  MOZ_ASSERT(aGlobalScope);
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_DIAGNOSTIC_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();
}

PerformanceWorker::~PerformanceWorker() {
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  if (workerPrivate) {
    workerPrivate->AssertIsOnWorkerThread();
  }
}

void PerformanceWorker::InsertUserEntry(PerformanceEntry* aEntry) {
  if (StaticPrefs::dom_performance_enable_user_timing_logging()) {
    nsAutoCString uri;
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_DIAGNOSTIC_ASSERT(workerPrivate);
    nsCOMPtr<nsIURI> scriptURI = workerPrivate->GetResolvedScriptURI();
    if (!scriptURI || NS_FAILED(scriptURI->GetHost(uri))) {
      // If we have no URI, just put in "none".
      uri.AssignLiteral("none");
    }
    Performance::LogEntry(aEntry, uri);
  }
  Performance::InsertUserEntry(aEntry);
}

TimeStamp PerformanceWorker::CreationTimeStamp() const {
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_DIAGNOSTIC_ASSERT(workerPrivate);
  return workerPrivate->CreationTimeStamp();
}

DOMHighResTimeStamp PerformanceWorker::CreationTime() const {
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_DIAGNOSTIC_ASSERT(workerPrivate);
  return workerPrivate->CreationTime();
}

uint64_t PerformanceWorker::GetRandomTimelineSeed() {
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_DIAGNOSTIC_ASSERT(workerPrivate);
  return workerPrivate->GetRandomTimelineSeed();
}

}  // namespace mozilla::dom
