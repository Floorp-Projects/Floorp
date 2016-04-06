/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_serviceworkerjobqueue_h
#define mozilla_dom_workers_serviceworkerjobqueue_h

#include "mozilla/RefPtr.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {
namespace workers {

class ServiceWorkerJob2;

class ServiceWorkerJobQueue2 final
{
  class Callback;

  nsTArray<RefPtr<ServiceWorkerJob2>> mJobList;

  ~ServiceWorkerJobQueue2();

  void
  JobFinished(ServiceWorkerJob2* aJob);

  void
  RunJob();

public:
  ServiceWorkerJobQueue2();

  void
  ScheduleJob(ServiceWorkerJob2* aJob);

  void
  CancelAll();

  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerJobQueue2)
};

} // namespace workers
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_workers_serviceworkerjobqueue_h
