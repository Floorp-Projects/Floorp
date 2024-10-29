/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GeolocationIPCUtils_h
#define mozilla_dom_GeolocationIPCUtils_h

#include "ipc/EnumSerializer.h"

namespace mozilla::dom::geolocation {

enum class SystemGeolocationPermissionBehavior {
  // OS geolocation permission may be granted or not, but Gecko should not ask
  // the user for it.  Gecko may not even know if it already has geolocation
  // permission.  This value is used when Gecko is done asking the user for
  // permission, when Gecko doesn't have to ask the user because permission is
  // already granted, and when Gecko is running on a platform where it cannot
  // perform the system UX operations required to get permission (such as
  // opening system preferences).
  NoPrompt,
  // Gecko does not have OS geolocation permission.  The OS will ask the user
  // for location permission before responding to requests.
  SystemWillPromptUser,
  // Gecko does not have OS geolocation permission.  Gecko will open OS
  // preferences and ask the user to either grant location permission or to
  // cancel the request.  It will wait for an answer before responding to the
  // geolocation request.
  GeckoWillPromptUser,
  Last = GeckoWillPromptUser,
};

enum class GeolocationPermissionStatus {
  Canceled,
  Granted,
  Error,
  Last = Error
};

}  // namespace mozilla::dom::geolocation

namespace IPC {

template <>
struct ParamTraits<
    mozilla::dom::geolocation::SystemGeolocationPermissionBehavior>
    : public ContiguousEnumSerializerInclusive<
          mozilla::dom::geolocation::SystemGeolocationPermissionBehavior,
          mozilla::dom::geolocation::SystemGeolocationPermissionBehavior::
              NoPrompt,
          mozilla::dom::geolocation::SystemGeolocationPermissionBehavior::
              Last> {};

template <>
struct ParamTraits<mozilla::dom::geolocation::GeolocationPermissionStatus>
    : public ContiguousEnumSerializerInclusive<
          mozilla::dom::geolocation::GeolocationPermissionStatus,
          mozilla::dom::geolocation::GeolocationPermissionStatus::Canceled,
          mozilla::dom::geolocation::GeolocationPermissionStatus::Last> {};

}  // namespace IPC

#endif /* mozilla_dom_GeolocationIPCUtils_h */
