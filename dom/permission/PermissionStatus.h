/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PermissionStatus_h_
#define mozilla_dom_PermissionStatus_h_

#include "mozilla/dom/PermissionsBinding.h"
#include "mozilla/dom/PermissionStatusBinding.h"
#include "mozilla/DOMEventTargetHelper.h"

namespace mozilla {
namespace dom {

class PermissionObserver;

class PermissionStatus final
  : public DOMEventTargetHelper
{
  friend class PermissionObserver;

public:
  ~PermissionStatus();

  static nsresult Create(nsPIDOMWindow* aWindow,
                         PermissionName aName,
                         PermissionStatus** aStatus);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  PermissionState State() const { return mState; }

  IMPL_EVENT_HANDLER(change)

private:
  PermissionStatus(nsPIDOMWindow* aWindow, PermissionName aName);

  nsresult Init();

  nsresult UpdateState();

  nsIPrincipal* GetPrincipal() const;

  void PermissionChanged();

  PermissionName mName;
  PermissionState mState;

  nsRefPtr<PermissionObserver> mObserver;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_permissionstatus_h_
