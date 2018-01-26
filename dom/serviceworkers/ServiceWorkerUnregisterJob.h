/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_serviceworkerunregisterjob_h
#define mozilla_dom_workers_serviceworkerunregisterjob_h

#include "ServiceWorkerJob.h"

namespace mozilla {
namespace dom {
namespace workers {

class ServiceWorkerUnregisterJob final : public ServiceWorkerJob
{
public:
  ServiceWorkerUnregisterJob(nsIPrincipal* aPrincipal,
                             const nsACString& aScope,
                             bool aSendToParent);

  bool
  GetResult() const;

private:
  class PushUnsubscribeCallback;

  virtual ~ServiceWorkerUnregisterJob();

  virtual void
  AsyncExecute() override;

  void
  Unregister();

  bool mResult;
  bool mSendToParent;
};

} // namespace workers
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_workers_serviceworkerunregisterjob_h
