/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_performance_h__
#define mozilla_dom_workers_performance_h__

#include "nsWrapperCache.h"
#include "js/TypeDecls.h"
#include "Workers.h"
#include "nsISupportsImpl.h"
#include "nsCycleCollectionParticipant.h"
#include "nsPerformance.h"

BEGIN_WORKERS_NAMESPACE

class WorkerPrivate;

class Performance final : public PerformanceBase
{
public:
  explicit Performance(WorkerPrivate* aWorkerPrivate);

private:
  ~Performance();

  void InsertUserEntry(PerformanceEntry* aEntry) override;

  WorkerPrivate* mWorkerPrivate;

public:
  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL (public APIs)
  DOMHighResTimeStamp Now() const override;

  using PerformanceBase::Mark;
  using PerformanceBase::ClearMarks;
  using PerformanceBase::Measure;
  using PerformanceBase::ClearMeasures;

private:
  nsISupports* GetAsISupports() override
  {
    return nullptr;
  }

  void DispatchBufferFullEvent() override;

  bool IsPerformanceTimingAttribute(const nsAString& aName) override;

  DOMHighResTimeStamp
  GetPerformanceTimingFromString(const nsAString& aTimingName) override;

  DOMHighResTimeStamp
  DeltaFromNavigationStart(DOMHighResTimeStamp aTime) override;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_performance_h__
