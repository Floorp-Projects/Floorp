/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(ENABLE_TESTS)
#  include "mozilla/ipc/UtilityProcessManager.h"
#  include "mozilla/ipc/UtilityProcessTest.h"
#  include "mozilla/dom/Promise.h"
#  include "mozilla/ProcInfo.h"
#  include "mozilla/IntentionalCrash.h"

namespace mozilla::ipc {

static UtilityActorName UtilityActorNameFromString(
    const nsACString& aStringName) {
  using namespace mozilla::dom;

  // We use WebIDLUtilityActorNames because UtilityActorNames is not designed
  // for iteration.
  for (size_t i = 0; i < WebIDLUtilityActorNameValues::Count; ++i) {
    auto idlName = static_cast<UtilityActorName>(i);
    const nsDependentCSubstring idlNameString(
        WebIDLUtilityActorNameValues::GetString(idlName));
    if (idlNameString.Equals(aStringName)) {
      return idlName;
    }
  }
  MOZ_CRASH("Unknown utility actor name");
}

// Find the utility process with the given actor or any utility process if
// the actor is UtilityActorName::EndGuard_.
static SandboxingKind FindUtilityProcessWithActor(UtilityActorName aActorName) {
  RefPtr<UtilityProcessManager> utilityProc =
      UtilityProcessManager::GetSingleton();
  MOZ_ASSERT(utilityProc, "No UtilityprocessManager?");

  for (size_t i = 0; i < SandboxingKind::COUNT; ++i) {
    auto sbKind = static_cast<SandboxingKind>(i);
    if (!utilityProc->Process(sbKind)) {
      continue;
    }
    if (aActorName == UtilityActorName::EndGuard_) {
      return sbKind;
    }
    for (auto actor : utilityProc->GetActors(sbKind)) {
      if (actor == aActorName) {
        return sbKind;
      }
    }
  }

  return SandboxingKind::COUNT;
}

NS_IMETHODIMP
UtilityProcessTest::StartProcess(const nsTArray<nsCString>& aActorsToRegister,
                                 JSContext* aCx,
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

  auto actors = aActorsToRegister.Clone();

  utilityProc->LaunchProcess(SandboxingKind::GENERIC_UTILITY)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise, utilityProc, actors = std::move(actors)] {
            RefPtr<UtilityProcessParent> utilityParent =
                utilityProc->GetProcessParent(SandboxingKind::GENERIC_UTILITY);
            Maybe<int32_t> utilityPid =
                utilityProc->ProcessPid(SandboxingKind::GENERIC_UTILITY);
            for (size_t i = 0; i < actors.Length(); ++i) {
              auto uan = UtilityActorNameFromString(actors[i]);
              utilityProc->RegisterActor(utilityParent, uan);
            }
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
UtilityProcessTest::NoteIntentionalCrash(uint32_t aPid) {
  mozilla::NoteIntentionalCrash("utility", aPid);
  return NS_OK;
}

NS_IMETHODIMP
UtilityProcessTest::StopProcess(const char* aActorName) {
  using namespace mozilla::dom;

  SandboxingKind sbKind;
  if (aActorName) {
    const nsDependentCString actorStringName(aActorName);
    UtilityActorName actorName = UtilityActorNameFromString(actorStringName);
    sbKind = FindUtilityProcessWithActor(actorName);
  } else {
    sbKind = FindUtilityProcessWithActor(UtilityActorName::EndGuard_);
  }

  if (sbKind == SandboxingKind::COUNT) {
    MOZ_ASSERT_UNREACHABLE(
        "Attempted to stop process for actor when no "
        "such process exists");
    return NS_ERROR_FAILURE;
  }

  RefPtr<UtilityProcessManager> utilityProc =
      UtilityProcessManager::GetSingleton();
  MOZ_ASSERT(utilityProc, "No UtilityprocessManager?");

  utilityProc->CleanShutdown(sbKind);
  Maybe<int32_t> utilityPid = utilityProc->ProcessPid(sbKind);
  MOZ_RELEASE_ASSERT(utilityPid.isNothing(),
                     "Should not have a utility process PID anymore");

  return NS_OK;
}

NS_IMETHODIMP
UtilityProcessTest::TestTelemetryProbes() {
  RefPtr<UtilityProcessManager> utilityProc =
      UtilityProcessManager::GetSingleton();
  MOZ_ASSERT(utilityProc, "No UtilityprocessManager?");

  for (RefPtr<UtilityProcessParent>& parent :
       utilityProc->GetAllProcessesProcessParent()) {
    Unused << parent->SendTestTelemetryProbes();
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(UtilityProcessTest, nsIUtilityProcessTest)

}  // namespace mozilla::ipc
#endif  // defined(ENABLE_TESTS)
