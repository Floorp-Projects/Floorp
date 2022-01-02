/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceWorker.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/StaticPrefs_dom.h"

namespace mozilla::dom {

PerformanceWorker::PerformanceWorker(WorkerPrivate* aWorkerPrivate)
    : Performance(aWorkerPrivate->GlobalScope(),
                  aWorkerPrivate->UsesSystemPrincipal()),
      mWorkerPrivate(aWorkerPrivate) {
  mWorkerPrivate->AssertIsOnWorkerThread();
}

PerformanceWorker::~PerformanceWorker() {
  mWorkerPrivate->AssertIsOnWorkerThread();
}

void PerformanceWorker::InsertUserEntry(PerformanceEntry* aEntry) {
  if (StaticPrefs::dom_performance_enable_user_timing_logging()) {
    nsAutoCString uri;
    nsCOMPtr<nsIURI> scriptURI = mWorkerPrivate->GetResolvedScriptURI();
    if (!scriptURI || NS_FAILED(scriptURI->GetHost(uri))) {
      // If we have no URI, just put in "none".
      uri.AssignLiteral("none");
    }
    Performance::LogEntry(aEntry, uri);
  }
  Performance::InsertUserEntry(aEntry);
}

TimeStamp PerformanceWorker::CreationTimeStamp() const {
  return mWorkerPrivate->CreationTimeStamp();
}

DOMHighResTimeStamp PerformanceWorker::CreationTime() const {
  return mWorkerPrivate->CreationTime();
}

uint64_t PerformanceWorker::GetRandomTimelineSeed() {
  return mWorkerPrivate->GetRandomTimelineSeed();
}

bool PerformanceWorker::CrossOriginIsolated() const {
  return mWorkerPrivate->CrossOriginIsolated();
}

}  // namespace mozilla::dom
