/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_DBusMessageRefPtr_h
#define mozilla_ipc_DBusMessageRefPtr_h

#include <dbus/dbus.h>
#include "mozilla/RefPtr.h"

namespace mozilla {

template<>
struct RefPtrTraits<DBusMessage>
{
  static void AddRef(DBusMessage* aMessage) {
    MOZ_ASSERT(aMessage);
    dbus_message_ref(aMessage);
  }
  static void Release(DBusMessage* aMessage) {
    MOZ_ASSERT(aMessage);
    dbus_message_unref(aMessage);
  }
};

} // namespace mozilla

#endif // mozilla_ipc_DBusMessageRefPtr_h
