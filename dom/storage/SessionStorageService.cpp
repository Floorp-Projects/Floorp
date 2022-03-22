/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SessionStorageService.h"

#include "MainThreadUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Components.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"

namespace mozilla::dom {

using namespace mozilla::ipc;

namespace {

StaticRefPtr<SessionStorageService> gSessionStorageService;

bool gShutdown(false);

}  // namespace

NS_IMPL_ISUPPORTS(SessionStorageService, nsISessionStorageService)

NS_IMETHODIMP
SessionStorageService::ClearStoragesForOrigin(nsIPrincipal* aPrincipal) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aPrincipal);

  QM_TRY_INSPECT(const auto& originAttrs,
                 MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(nsCString, aPrincipal,
                                                   GetOriginSuffix));

  QM_TRY_INSPECT(const auto& originKey,
                 MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(nsCString, aPrincipal,
                                                   GetStorageOriginKey));

  QM_TRY(OkIf(SendClearStoragesForOrigin(originAttrs, originKey)),
         NS_ERROR_FAILURE);

  return NS_OK;
}

SessionStorageService::SessionStorageService() { AssertIsOnMainThread(); }

SessionStorageService::~SessionStorageService() {
  AssertIsOnMainThread();
  MOZ_ASSERT_IF(mInitialized, mActorDestroyed);
}

// static
Result<RefPtr<SessionStorageService>, nsresult> SessionStorageService::Acquire(
    const CreateIfNonExistent&) {
  AssertIsOnMainThread();

  QM_TRY(OkIf(!gShutdown), Err(NS_ERROR_ILLEGAL_DURING_SHUTDOWN));

  if (gSessionStorageService) {
    return RefPtr<SessionStorageService>(gSessionStorageService);
  }

  auto sessionStorageService = MakeRefPtr<SessionStorageService>();

  QM_TRY(sessionStorageService->Init());

  gSessionStorageService = sessionStorageService;

  RunOnShutdown(
      [] {
        gShutdown = true;

        gSessionStorageService->Shutdown();

        gSessionStorageService = nullptr;
      },
      ShutdownPhase::XPCOMShutdown);

  return sessionStorageService;
}

// static
RefPtr<SessionStorageService> SessionStorageService::Acquire() {
  AssertIsOnMainThread();

  return gSessionStorageService;
}

Result<Ok, nsresult> SessionStorageService::Init() {
  AssertIsOnMainThread();

  PBackgroundChild* backgroundActor =
      BackgroundChild::GetOrCreateForCurrentThread();
  QM_TRY(OkIf(backgroundActor), Err(NS_ERROR_FAILURE));

  QM_TRY(OkIf(backgroundActor->SendPBackgroundSessionStorageServiceConstructor(
             this)),
         Err(NS_ERROR_FAILURE));

  mInitialized.Flip();

  return Ok{};
}

void SessionStorageService::Shutdown() {
  AssertIsOnMainThread();

  if (!mActorDestroyed) {
    QM_WARNONLY_TRY(OkIf(Send__delete__(this)));
  }
}

void SessionStorageService::ActorDestroy(ActorDestroyReason /* aWhy */) {
  AssertIsOnMainThread();

  mActorDestroyed.Flip();
}

}  // namespace mozilla::dom

NS_IMPL_COMPONENT_FACTORY(nsISessionStorageService) {
  mozilla::AssertIsOnMainThread();

  QM_TRY_UNWRAP(auto sessionStorageService,
                mozilla::dom::SessionStorageService::Acquire(
                    mozilla::CreateIfNonExistent{}),
                nullptr);

  return sessionStorageService.forget().downcast<nsISupports>();
}
