/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PermissionStatus.h"

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/Services.h"
#include "nsIPermissionManager.h"
#include "PermissionObserver.h"
#include "PermissionUtils.h"

namespace mozilla {
namespace dom {

/* static */ already_AddRefed<PermissionStatus>
PermissionStatus::Create(nsPIDOMWindowInner* aWindow,
                         PermissionName aName,
                         ErrorResult& aRv)
{
  RefPtr<PermissionStatus> status = new PermissionStatus(aWindow, aName);
  aRv = status->Init();
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return status.forget();
}

PermissionStatus::PermissionStatus(nsPIDOMWindowInner* aWindow,
                                   PermissionName aName)
  : DOMEventTargetHelper(aWindow)
  , mName(aName)
  , mState(PermissionState::Denied)
{
}

nsresult
PermissionStatus::Init()
{
  mObserver = PermissionObserver::GetInstance();
  if (NS_WARN_IF(!mObserver)) {
    return NS_ERROR_FAILURE;
  }

  mObserver->AddSink(this);

  nsresult rv = UpdateState();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

PermissionStatus::~PermissionStatus()
{
  if (mObserver) {
    mObserver->RemoveSink(this);
  }
}

JSObject*
PermissionStatus::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PermissionStatusBinding::Wrap(aCx, this, aGivenProto);
}

nsresult
PermissionStatus::UpdateState()
{
  nsCOMPtr<nsIPermissionManager> permMgr = services::GetPermissionManager();
  if (NS_WARN_IF(!permMgr)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsPIDOMWindowInner> window = GetOwner();
  if (NS_WARN_IF(!window)) {
    return NS_ERROR_FAILURE;
  }

  uint32_t action = nsIPermissionManager::DENY_ACTION;
  nsresult rv = permMgr->TestPermissionFromWindow(window,
                                                  PermissionNameToType(mName),
                                                  &action);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mState = ActionToPermissionState(action);
  return NS_OK;
}

nsIPrincipal*
PermissionStatus::GetPrincipal() const
{
  nsCOMPtr<nsPIDOMWindowInner> window = GetOwner();
  if (NS_WARN_IF(!window)) {
    return nullptr;
  }

  nsIDocument* doc = window->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    return nullptr;
  }

  return doc->NodePrincipal();
}

void
PermissionStatus::PermissionChanged()
{
  auto oldState = mState;
  UpdateState();
  if (mState != oldState) {
    RefPtr<AsyncEventDispatcher> eventDispatcher =
      new AsyncEventDispatcher(this, NS_LITERAL_STRING("change"), false);
    eventDispatcher->PostDOMEvent();
  }
}

} // namespace dom
} // namespace mozilla
