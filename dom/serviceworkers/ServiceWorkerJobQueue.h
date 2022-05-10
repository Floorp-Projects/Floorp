/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_serviceworkerjobqueue_h
#define mozilla_dom_serviceworkerjobqueue_h

#include "mozilla/RefPtr.h"
#include "nsTArray.h"

namespace mozilla::dom {

class ServiceWorkerJob;

class ServiceWorkerJobQueue final {
  class Callback;

  nsTArray<RefPtr<ServiceWorkerJob>> mJobList;

  ~ServiceWorkerJobQueue();

  void JobFinished(ServiceWorkerJob* aJob);

  void RunJob();

 public:
  ServiceWorkerJobQueue();

  void ScheduleJob(ServiceWorkerJob* aJob);

  void CancelAll();

  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerJobQueue)
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_serviceworkerjobqueue_h
