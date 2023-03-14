/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(ENABLE_TESTS)
#  include "mozilla/RDDProcessManager.h"
#  include "mozilla/RDDChild.h"
#  include "mozilla/RddProcessTest.h"
#  include "mozilla/dom/Promise.h"

namespace mozilla {

NS_IMETHODIMP
RddProcessTest::TestTelemetryProbes(JSContext* aCx,
                                    mozilla::dom::Promise** aOutPromise) {
  NS_ENSURE_ARG(aOutPromise);
  *aOutPromise = nullptr;
  nsIGlobalObject* global = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!global)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult erv;
  RefPtr<dom::Promise> promise = dom::Promise::Create(global, erv);
  if (NS_WARN_IF(erv.Failed())) {
    return erv.StealNSResult();
  }

  RDDProcessManager* rddProc = RDDProcessManager::Get();
  MOZ_ASSERT(rddProc, "No RddProcessManager?");

  rddProc->LaunchRDDProcess()->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [promise, rddProc]() {
        RDDChild* child = rddProc->GetRDDChild();
        if (!rddProc) {
          promise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
        }
        MOZ_ASSERT(rddProc, "No RDD Proc?");

        if (!child) {
          promise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
        }
        MOZ_ASSERT(child, "No RDD Child?");

        Unused << child->SendTestTelemetryProbes();
        promise->MaybeResolve((int32_t)rddProc->RDDProcessPid());
      },
      [promise](nsresult aError) {
        MOZ_ASSERT_UNREACHABLE("RddProcessTest; failure to get RDD child");
        promise->MaybeReject(aError);
      });

  promise.forget(aOutPromise);
  return NS_OK;
}

NS_IMETHODIMP
RddProcessTest::StopProcess() {
  RDDProcessManager::RDDProcessShutdown();
  return NS_OK;
}

NS_IMPL_ISUPPORTS(RddProcessTest, nsIRddProcessTest)

}  // namespace mozilla
#endif  // defined(ENABLE_TESTS)
