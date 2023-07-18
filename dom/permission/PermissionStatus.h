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
#include "mozilla/MozPromise.h"
#include "nsIPermission.h"

namespace mozilla::dom {

class PermissionObserver;

class PermissionStatus : public DOMEventTargetHelper {
  friend class PermissionObserver;

 public:
  using CreatePromise = MozPromise<RefPtr<PermissionStatus>, nsresult, true>;
  using SimplePromise = MozPromise<nsresult, nsresult, true>;

  static RefPtr<CreatePromise> Create(nsPIDOMWindowInner* aWindow,
                                      PermissionName aName);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  PermissionState State() const { return mState; }

  IMPL_EVENT_HANDLER(change)

  void DisconnectFromOwner() override;

  PermissionName Name() const { return mName; }

  RefPtr<SimplePromise> Init();

 protected:
  ~PermissionStatus();

  PermissionStatus(nsPIDOMWindowInner* aWindow, PermissionName aName);

  /**
   * This method returns the internal permission type, which should be equal to
   * the permission name for all but the MIDI permission because of the SysEx
   * support: internally, we have both "midi" and "midi-sysex" permission types
   * but we only have a "midi" (public) permission name.
   *
   * Note: the `MidiPermissionDescriptor` descriptor has an optional `sysex`
   * boolean, which is used to determine whether to return "midi" or
   * "midi-sysex" for the MIDI permission.
   */
  virtual nsLiteralCString GetPermissionType();

 private:
  virtual RefPtr<SimplePromise> UpdateState();

  // These functions should be called when an permission is updated which may
  // change the state of this PermissionStatus. MaybeUpdatedBy accepts the
  // permission object itself that is update. When the permission's key is not
  // same-origin with this object's owner window, such as for secondary-keyed
  // permissions like `3rdPartyFrameStorage^...`, MaybeUpdatedByNotifyOnly will
  // be called with the updated window as an argument. MaybeUpdatedByNotifyOnly
  // must be defined by PermissionStatus inheritors that are double-keyed.
  virtual bool MaybeUpdatedBy(nsIPermission* aPermission) const;
  virtual bool MaybeUpdatedByNotifyOnly(nsPIDOMWindowInner* aInnerWindow) const;

  void PermissionChanged();

  PermissionName mName;

  RefPtr<PermissionObserver> mObserver;

 protected:
  PermissionState mState;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_permissionstatus_h_
