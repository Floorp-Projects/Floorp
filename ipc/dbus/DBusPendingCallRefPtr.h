/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_DBusPendingCallRefPtr_h
#define mozilla_ipc_DBusPendingCallRefPtr_h

#include <dbus/dbus.h>
#include "mozilla/RefPtr.h"

namespace mozilla {

template<>
struct RefPtrTraits<DBusPendingCall>
{
  static void AddRef(DBusPendingCall* aPendingCall) {
    MOZ_ASSERT(aPendingCall);
    dbus_pending_call_ref(aPendingCall);
  }
  static void Release(DBusPendingCall* aPendingCall) {
    MOZ_ASSERT(aPendingCall);
    dbus_pending_call_unref(aPendingCall);
  }
};

} // namespace mozilla

#endif // mozilla_ipc_DBusPendingCallRefPtr_h
