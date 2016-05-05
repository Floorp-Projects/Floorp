/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_DBusConnectionRefPtr_h
#define mozilla_ipc_DBusConnectionRefPtr_h

#include <dbus/dbus.h>
#include "mozilla/RefPtr.h"

namespace mozilla {

/*
 * |RefPtrTraits<DBusConnection>| specializes |RefPtrTraits<>|
 * for managing |DBusConnection| with |RefPtr|.
 *
 * |RefPtrTraits<DBusConnection>| will _not_ close the DBus
 * connection upon the final unref. The caller is responsible
 * for closing the connection.
 *
 * See |DBusConnectionDelete| for auto-closing of connections.
 */
template<>
struct RefPtrTraits<DBusConnection>
{
  static void AddRef(DBusConnection* aConnection) {
    MOZ_ASSERT(aConnection);
    dbus_connection_ref(aConnection);
  }
  static void Release(DBusConnection* aConnection) {
    MOZ_ASSERT(aConnection);
    dbus_connection_unref(aConnection);
  }
};

} // namespace mozilla

#endif // mozilla_ipc_DBusConnectionRefPtr_h
