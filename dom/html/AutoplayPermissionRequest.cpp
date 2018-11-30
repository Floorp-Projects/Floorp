/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AutoplayPermissionRequest.h"
#include "mozilla/AutoplayPermissionManager.h"

#include "mozilla/Logging.h"

extern mozilla::LazyLogModule gAutoplayPermissionLog;

#define PLAY_REQUEST_LOG(msg, ...) \
  MOZ_LOG(gAutoplayPermissionLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

namespace mozilla {

NS_IMPL_CYCLE_COLLECTION_INHERITED(AutoplayPermissionRequest,
                                   ContentPermissionRequestBase)

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(AutoplayPermissionRequest,
                                               ContentPermissionRequestBase)

AutoplayPermissionRequest::AutoplayPermissionRequest(
    AutoplayPermissionManager* aManager, nsGlobalWindowInner* aWindow,
    nsIPrincipal* aNodePrincipal)
    : ContentPermissionRequestBase(
          aNodePrincipal, false, aWindow,
          NS_LITERAL_CSTRING(""),  // No testing pref used in this class
          NS_LITERAL_CSTRING("autoplay-media")),
      mManager(aManager) {}

AutoplayPermissionRequest::~AutoplayPermissionRequest() { Cancel(); }

NS_IMETHODIMP
AutoplayPermissionRequest::Cancel() {
  if (mManager) {
    mManager->DenyPlayRequestIfExists();
    // Clear reference to manager, so we can't double report a result.
    // This could happen in particular if we call Cancel() in the destructor.
    mManager = nullptr;
  }
  return NS_OK;
}

NS_IMETHODIMP
AutoplayPermissionRequest::Allow(JS::HandleValue aChoices) {
  if (mManager) {
    mManager->ApprovePlayRequestIfExists();
    // Clear reference to manager, so we can't double report a result.
    // This could happen in particular if we call Cancel() in the destructor.
    mManager = nullptr;
  }
  return NS_OK;
}

already_AddRefed<AutoplayPermissionRequest> AutoplayPermissionRequest::Create(
    nsGlobalWindowInner* aWindow, AutoplayPermissionManager* aManager) {
  if (!aWindow || !aWindow->GetPrincipal()) {
    return nullptr;
  }
  RefPtr<AutoplayPermissionRequest> request =
      new AutoplayPermissionRequest(aManager, aWindow, aWindow->GetPrincipal());
  PLAY_REQUEST_LOG("AutoplayPermissionRequest %p Create()", request.get());
  return request.forget();
}

}  // namespace mozilla
