/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_DBusConnectionDelete_h
#define mozilla_ipc_DBusConnectionDelete_h

#include <dbus/dbus.h>
#include "mozilla/UniquePtr.h"

namespace mozilla {

/*
 * |DBusConnectionDelete| is a deleter for managing instances
 * of |DBusConnection| in |UniquePtr|. Upon destruction, it
 * will close an open connection before unref'ing the data
 * structure.
 *
 * Do not use |UniquePtr| with shared DBus connections. For
 * shared connections, use |RefPtr|.
 */
class DBusConnectionDelete
{
public:
  MOZ_CONSTEXPR DBusConnectionDelete()
  { }

  void operator()(DBusConnection* aConnection) const
  {
    MOZ_ASSERT(aConnection);
    if (dbus_connection_get_is_connected(aConnection)) {
      dbus_connection_close(aConnection);
    }
    dbus_connection_unref(aConnection);
  }
};

} // namespace mozilla

#endif // mozilla_ipc_DBusConnectionDelete_h
