/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Permissions.h"

#include "mozilla/dom/Document.h"
#include "mozilla/dom/MidiPermissionStatus.h"
#include "mozilla/dom/PermissionSetParametersBinding.h"
#include "mozilla/dom/PermissionStatus.h"
#include "mozilla/dom/PermissionsBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/StorageAccessPermissionStatus.h"
#include "PermissionUtils.h"

namespace mozilla::dom {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Permissions)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Permissions)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Permissions)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Permissions, mWindow)

Permissions::Permissions(nsPIDOMWindowInner* aWindow) : mWindow(aWindow) {}

Permissions::~Permissions() = default;

JSObject* Permissions::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return Permissions_Binding::Wrap(aCx, this, aGivenProto);
}

namespace {

// Steps to parse PermissionDescriptor in
// https://w3c.github.io/permissions/#query-method and relevant WebDriver
// commands
RefPtr<PermissionStatus> CreatePermissionStatus(
    JSContext* aCx, JS::Handle<JSObject*> aPermissionDesc,
    nsPIDOMWindowInner* aWindow, ErrorResult& aRv) {
  // Step 2: Let rootDesc be the object permissionDesc refers to, converted to
  // an IDL value of type PermissionDescriptor.
  PermissionDescriptor rootDesc;
  JS::Rooted<JS::Value> permissionDescValue(
      aCx, JS::ObjectOrNullValue(aPermissionDesc));
  if (NS_WARN_IF(!rootDesc.Init(aCx, permissionDescValue))) {
    // Step 3: If the conversion throws an exception, return a promise rejected
    // with that exception.
    // Step 4: If rootDesc["name"] is not supported, return a promise rejected
    // with a TypeError. (This is done by `enum PermissionName`, as the spec
    // note says: "implementers are encouraged to use their own custom enum
    // here")
    aRv.NoteJSContextException(aCx);
    return nullptr;
  }

  // Step 5: Let typedDescriptor be the object permissionDesc refers to,
  // converted to an IDL value of rootDesc's name's permission descriptor type.
  // Step 6: If the conversion throws an exception, return a promise rejected
  // with that exception.
  // Step 8.1: Let status be create a PermissionStatus with typedDescriptor.
  // (The rest is done by the caller)
  switch (rootDesc.mName) {
    case PermissionName::Midi: {
      MidiPermissionDescriptor midiPerm;
      if (NS_WARN_IF(!midiPerm.Init(aCx, permissionDescValue))) {
        aRv.NoteJSContextException(aCx);
        return nullptr;
      }

      return new MidiPermissionStatus(aWindow, midiPerm.mSysex);
    }
    case PermissionName::Storage_access:
      return new StorageAccessPermissionStatus(aWindow);
    case PermissionName::Geolocation:
    case PermissionName::Notifications:
    case PermissionName::Push:
    case PermissionName::Persistent_storage:
    case PermissionName::Screen_wake_lock:
      return new PermissionStatus(aWindow, rootDesc.mName);
    default:
      MOZ_ASSERT_UNREACHABLE("Unhandled type");
      aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
      return nullptr;
  }
}

}  // namespace

// https://w3c.github.io/permissions/#query-method
already_AddRefed<Promise> Permissions::Query(JSContext* aCx,
                                             JS::Handle<JSObject*> aPermission,
                                             ErrorResult& aRv) {
  // Step 1: If this's relevant global object is a Window object, then:
  // Step 1.1: If the current settings object's associated Document is not fully
  // active, return a promise rejected with an "InvalidStateError" DOMException.
  //
  // TODO(krosylight): The spec allows worker global while we don't, see bug
  // 1193373.
  if (!mWindow || !mWindow->IsFullyActive()) {
    aRv.ThrowInvalidStateError("The document is not fully active.");
    return nullptr;
  }

  // Step 2 - 6 and 8.1:
  RefPtr<PermissionStatus> status =
      CreatePermissionStatus(aCx, aPermission, mWindow, aRv);
  if (!status) {
    return nullptr;
  }

  // Step 7: Let promise be a new promise.
  RefPtr<Promise> promise = Promise::Create(mWindow->AsGlobal(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Step 8.2 - 8.3: (Done by the Init method)
  // Step 8.4: Queue a global task on the permissions task source with this's
  // relevant global object to resolve promise with status.
  status->Init()->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [status, promise]() {
        promise->MaybeResolve(status);
        return;
      },
      [promise](nsresult aError) {
        MOZ_ASSERT(NS_FAILED(aError));
        NS_WARNING("Failed PermissionStatus creation");
        promise->MaybeReject(aError);
        return;
      });

  return promise.forget();
}

already_AddRefed<PermissionStatus> Permissions::ParseSetParameters(
    JSContext* aCx, const PermissionSetParameters& aParameters,
    ErrorResult& aRv) {
  // Step 1: Let parametersDict be the parameters argument, converted to an IDL
  // value of type PermissionSetParameters. If this throws an exception,
  // return an invalid argument error.
  // (Done by IDL layer, and the error type should be handled by the caller)

  // Step 2: If parametersDict.state is an inappropriate permission state for
  // any implementation-defined reason, return a invalid argument error.
  // (We don't do this)

  // Step 3: Let rootDesc be parametersDict.descriptor.
  JS::Rooted<JSObject*> rootDesc(aCx, aParameters.mDescriptor);

  // Step 4: Let typedDescriptor be the object rootDesc refers to, converted
  // to an IDL value of rootDesc.name's permission descriptor type. If this
  // throws an exception, return a invalid argument error.
  //
  // We use PermissionStatus as the typed object.
  RefPtr<PermissionStatus> status =
      CreatePermissionStatus(aCx, rootDesc, nullptr, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Set the state too so that the caller can use it for step 5.
  status->SetState(aParameters.mState);

  return status.forget();
}

}  // namespace mozilla::dom
