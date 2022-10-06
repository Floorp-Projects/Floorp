/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(ENABLE_TESTS)
#  include "mozilla/ipc/UtilityProcessTest.h"
#  include "mozilla/ipc/UtilityProcessManager.h"
#  include "mozilla/dom/Promise.h"

namespace mozilla::ipc {

NS_IMETHODIMP
UtilityProcessTest::StartProcess(JSContext* aCx,
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

  RefPtr<UtilityProcessManager> utilityProc =
      UtilityProcessManager::GetSingleton();
  MOZ_ASSERT(utilityProc, "No UtilityprocessManager?");

  utilityProc->LaunchProcess(SandboxingKind::GENERIC_UTILITY)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise, utilityProc]() {
            Maybe<int32_t> utilityPid =
                utilityProc->ProcessPid(SandboxingKind::GENERIC_UTILITY);
            if (utilityPid.isSome()) {
              promise->MaybeResolve(*utilityPid);
            } else {
              promise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
            }
          },
          [promise](nsresult aError) {
            MOZ_ASSERT_UNREACHABLE(
                "UtilityProcessTest; failure to get Utility process");
            promise->MaybeReject(aError);
          });

  promise.forget(aOutPromise);
  return NS_OK;
}

NS_IMETHODIMP
UtilityProcessTest::StopProcess() {
  RefPtr<UtilityProcessManager> utilityProc =
      UtilityProcessManager::GetSingleton();
  MOZ_ASSERT(utilityProc, "No UtilityprocessManager?");

  utilityProc->CleanShutdown(SandboxingKind::GENERIC_UTILITY);
  Maybe<int32_t> utilityPid =
      utilityProc->ProcessPid(SandboxingKind::GENERIC_UTILITY);
  MOZ_RELEASE_ASSERT(utilityPid.isNothing(),
                     "Should not have a utility process PID anymore");

  return NS_OK;
}

NS_IMPL_ISUPPORTS(UtilityProcessTest, nsIUtilityProcessTest)

}  // namespace mozilla::ipc
#endif  // defined(ENABLE_TESTS)
