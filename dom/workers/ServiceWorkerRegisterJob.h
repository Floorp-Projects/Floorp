/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_serviceworkerregisterjob_h
#define mozilla_dom_workers_serviceworkerregisterjob_h

#include "ServiceWorkerUpdateJob.h"

namespace mozilla {
namespace dom {
namespace workers {

// The register job.  This implements the steps in the spec Register algorithm,
// but then uses ServiceWorkerUpdateJob to implement the Update and Install
// spec algorithms.
class ServiceWorkerRegisterJob final : public ServiceWorkerUpdateJob
{
  const nsLoadFlags mLoadFlags;

public:
  ServiceWorkerRegisterJob(nsIPrincipal* aPrincipal,
                           const nsACString& aScope,
                           const nsACString& aScriptSpec,
                           nsILoadGroup* aLoadGroup,
                           nsLoadFlags aLoadFlags);

private:
  // Implement the Register algorithm steps and then call the parent class
  // Update() to complete the job execution.
  virtual void
  AsyncExecute() override;

  virtual ~ServiceWorkerRegisterJob();
};

} // namespace workers
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_workers_serviceworkerregisterjob_h
