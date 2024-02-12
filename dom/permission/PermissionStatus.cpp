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

PermissionStatus::PermissionStatus(nsPIDOMWindowInner* aWindow,
                                   PermissionName aName)
    : DOMEventTargetHelper(aWindow),
      mName(aName),
      mState(PermissionState::Denied) {
  KeepAliveIfHasListenersFor(nsGkAtoms::onchange);
}

// https://w3c.github.io/permissions/#onchange-attribute and
// https://w3c.github.io/permissions/#query-method
RefPtr<PermissionStatus::SimplePromise> PermissionStatus::Init() {
  // Covers the onchange part
  // Whenever the user agent is aware that the state of a PermissionStatus
  // instance status has changed: ...
  // (The observer calls PermissionChanged() to do the steps)
  mObserver = PermissionObserver::GetInstance();
  if (NS_WARN_IF(!mObserver)) {
    return SimplePromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  mObserver->AddSink(this);

  // Covers the query part (Step 8.2 - 8.4)
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

nsLiteralCString PermissionStatus::GetPermissionType() const {
  return PermissionNameToType(mName);
}

// Covers the calling part of "permission query algorithm" of query() method and
// update steps, which calls
// https://w3c.github.io/permissions/#dfn-default-permission-query-algorithm
// and then https://w3c.github.io/permissions/#dfn-permission-state
RefPtr<PermissionStatus::SimplePromise> PermissionStatus::UpdateState() {
  // Step 1: If settings wasn't passed, set it to the current settings object.
  // Step 2: If settings is a non-secure context, return "denied".
  // XXX(krosylight): No such steps here, and no WPT coverage?

  // The permission handler covers the rest of the steps, although the model
  // does not exactly match what the spec has. (Not passing "permission key" for
  // example)

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

// https://w3c.github.io/permissions/#dfn-permissionstatus-update-steps
void PermissionStatus::PermissionChanged() {
  auto oldState = mState;
  RefPtr<PermissionStatus> self(this);
  // Step 1: If this's relevant global object is a Window object, then:
  // Step 1.1: Let document be status's relevant global object's associated
  // Document.
  // Step 1.2: If document is null or document is not fully active,
  // terminate this algorithm.
  // TODO(krosylight): WPT /permissions/non-fully-active.https.html fails
  // because we don't do this. See bug 1876470.

  // Step 2 - 3 is covered by UpdateState()
  UpdateState()->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [self, oldState]() {
        if (self->mState != oldState) {
          // Step 4: Queue a task on the permissions task source to fire an
          // event named change at status.
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

void PermissionStatus::GetType(nsACString& aName) const {
  aName.Assign(GetPermissionType());
}

}  // namespace mozilla::dom
