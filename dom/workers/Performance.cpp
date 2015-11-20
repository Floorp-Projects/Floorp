/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Performance.h"
#include "mozilla/dom/PerformanceBinding.h"

#include "WorkerPrivate.h"

BEGIN_WORKERS_NAMESPACE

Performance::Performance(WorkerPrivate* aWorkerPrivate)
  : mWorkerPrivate(aWorkerPrivate)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
}

Performance::~Performance()
{
  mWorkerPrivate->AssertIsOnWorkerThread();
}

JSObject*
Performance::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PerformanceBinding_workers::Wrap(aCx, this, aGivenProto);
}

DOMHighResTimeStamp
Performance::Now() const
{
  TimeDuration duration =
    TimeStamp::Now() - mWorkerPrivate->CreationTimeStamp();
  return RoundTime(duration.ToMilliseconds());
}

// To be removed once bug 1124165 lands
bool
Performance::IsPerformanceTimingAttribute(const nsAString& aName)
{
  // In workers we just support navigationStart.
  return aName.EqualsASCII("navigationStart");
}

DOMHighResTimeStamp
Performance::GetPerformanceTimingFromString(const nsAString& aProperty)
{
  if (!IsPerformanceTimingAttribute(aProperty)) {
    return 0;
  }

  if (aProperty.EqualsLiteral("navigationStart")) {
    return mWorkerPrivate->CreationTime();
  }

  MOZ_CRASH("IsPerformanceTimingAttribute and GetPerformanceTimingFromString are out of sync");
  return 0;
}

void
Performance::InsertUserEntry(PerformanceEntry* aEntry)
{
  if (mWorkerPrivate->PerformanceLoggingEnabled()) {
    nsAutoCString uri;
    nsCOMPtr<nsIURI> scriptURI = mWorkerPrivate->GetResolvedScriptURI();
    if (!scriptURI || NS_FAILED(scriptURI->GetHost(uri))) {
      // If we have no URI, just put in "none".
      uri.AssignLiteral("none");
    }
    PerformanceBase::LogEntry(aEntry, uri);
  }
  PerformanceBase::InsertUserEntry(aEntry);
}

TimeStamp
Performance::CreationTimeStamp() const
{
  return mWorkerPrivate->CreationTimeStamp();
}

DOMHighResTimeStamp
Performance::CreationTime() const
{
  return mWorkerPrivate->CreationTime();
}

void
Performance::DispatchBufferFullEvent()
{
  // This method is needed just for InsertResourceEntry, but this method is not
  // exposed to workers.
  MOZ_CRASH("This should not be called.");
}

END_WORKERS_NAMESPACE
