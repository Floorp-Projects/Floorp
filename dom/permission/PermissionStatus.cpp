/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PermissionStatus.h"
#include "mozilla/PermissionDelegateHandler.h"

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/Permission.h"
#include "mozilla/Services.h"
#include "nsIPermissionManager.h"
#include "PermissionObserver.h"
#include "PermissionUtils.h"

namespace mozilla::dom {

/* static */
RefPtr<PermissionStatus::CreatePromise> PermissionStatus::Create(
    nsPIDOMWindowInner* aWindow, PermissionName aName) {
  RefPtr<PermissionStatus> status = new PermissionStatus(aWindow, aName);
  return status->Init()->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [status](nsresult aOk) {
        MOZ_ASSERT(NS_SUCCEEDED(aOk));
        return MozPromise<RefPtr<PermissionStatus>, nsresult,
                          true>::CreateAndResolve(status, __func__);
      },
      [](nsresult aError) {
        MOZ_ASSERT(NS_FAILED(aError));
        return MozPromise<RefPtr<PermissionStatus>, nsresult,
                          true>::CreateAndReject(aError, __func__);
      });
}

PermissionStatus::PermissionStatus(nsPIDOMWindowInner* aWindow,
                                   PermissionName aName)
    : DOMEventTargetHelper(aWindow),
      mName(aName),
      mState(PermissionState::Denied) {
  KeepAliveIfHasListenersFor(nsGkAtoms::onchange);
}

RefPtr<PermissionStatus::SimplePromise> PermissionStatus::Init() {
  mObserver = PermissionObserver::GetInstance();
  if (NS_WARN_IF(!mObserver)) {
    return SimplePromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  mObserver->AddSink(this);

  return UpdateState();
}

PermissionStatus::~PermissionStatus() {
  if (mObserver) {
    mObserver->RemoveSink(this);
  }
}

JSObject* PermissionStatus::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return PermissionStatus_Binding::Wrap(aCx, this, aGivenProto);
}

nsLiteralCString PermissionStatus::GetPermissionType() {
  return PermissionNameToType(mName);
}

RefPtr<PermissionStatus::SimplePromise> PermissionStatus::UpdateState() {
  nsCOMPtr<nsPIDOMWindowInner> window = GetOwner();
  if (NS_WARN_IF(!window)) {
    return SimplePromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  RefPtr<Document> document = window->GetExtantDoc();
  if (NS_WARN_IF(!document)) {
    return SimplePromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  uint32_t action = nsIPermissionManager::DENY_ACTION;

  PermissionDelegateHandler* permissionHandler =
      document->GetPermissionDelegateHandler();
  if (NS_WARN_IF(!permissionHandler)) {
    return SimplePromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  nsresult rv = permissionHandler->GetPermissionForPermissionsAPI(
      GetPermissionType(), &action);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return SimplePromise::CreateAndReject(rv, __func__);
  }

  mState = ActionToPermissionState(action);
  return SimplePromise::CreateAndResolve(NS_OK, __func__);
  ;
}

bool PermissionStatus::MaybeUpdatedBy(nsIPermission* aPermission) const {
  NS_ENSURE_TRUE(aPermission, false);
  nsCOMPtr<nsPIDOMWindowInner> window = GetOwner();
  if (NS_WARN_IF(!window)) {
    return false;
  }

  Document* doc = window->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    return false;
  }

  nsCOMPtr<nsIPrincipal> principal =
      Permission::ClonePrincipalForPermission(doc->NodePrincipal());
  NS_ENSURE_TRUE(principal, false);
  nsCOMPtr<nsIPrincipal> permissionPrincipal;
  aPermission->GetPrincipal(getter_AddRefs(permissionPrincipal));
  if (!permissionPrincipal) {
    return false;
  }
  return permissionPrincipal->Equals(principal);
}

bool PermissionStatus::MaybeUpdatedByNotifyOnly(
    nsPIDOMWindowInner* aInnerWindow) const {
  return false;
}

void PermissionStatus::PermissionChanged() {
  auto oldState = mState;
  RefPtr<PermissionStatus> self(this);
  UpdateState()->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [self, oldState]() {
        if (self->mState != oldState) {
          RefPtr<AsyncEventDispatcher> eventDispatcher =
              new AsyncEventDispatcher(self.get(), u"change"_ns,
                                       CanBubble::eNo);
          eventDispatcher->PostDOMEvent();
        }
      },
      []() {

      });
}

void PermissionStatus::DisconnectFromOwner() {
  IgnoreKeepAliveIfHasListenersFor(nsGkAtoms::onchange);

  if (mObserver) {
    mObserver->RemoveSink(this);
    mObserver = nullptr;
  }

  DOMEventTargetHelper::DisconnectFromOwner();
}

}  // namespace mozilla::dom
