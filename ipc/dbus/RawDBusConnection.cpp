/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RawDBusConnection.h"
#include <dbus/dbus.h>

using namespace mozilla::ipc;

bool RawDBusConnection::sDBusIsInit(false);

RawDBusConnection::RawDBusConnection() {
}

RawDBusConnection::~RawDBusConnection() {
}

nsresult RawDBusConnection::EstablishDBusConnection()
{
  if (!sDBusIsInit) {
    dbus_bool_t success = dbus_threads_init_default();
    NS_ENSURE_TRUE(success == TRUE, NS_ERROR_FAILURE);
    sDBusIsInit = true;
  }
  DBusError err;
  dbus_error_init(&err);
  mConnection = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
  if (dbus_error_is_set(&err)) {
    dbus_error_free(&err);
    return NS_ERROR_FAILURE;
  }
  dbus_connection_set_exit_on_disconnect(mConnection, FALSE);
  return NS_OK;
}

void RawDBusConnection::ScopedDBusConnectionPtrTraits::release(DBusConnection* ptr)
{
  if (ptr) {
    dbus_connection_unref(ptr);
  }
}
