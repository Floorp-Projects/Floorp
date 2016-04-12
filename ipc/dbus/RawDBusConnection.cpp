/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RawDBusConnection.h"
#include "base/message_loop.h"
#include "mozilla/ipc/DBusHelpers.h"
#include "mozilla/ipc/DBusWatcher.h"

namespace mozilla {
namespace ipc {

//
// RawDBusConnection
//

bool RawDBusConnection::sDBusIsInit(false);

RawDBusConnection::RawDBusConnection()
{
}

RawDBusConnection::~RawDBusConnection()
{
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

  mConnection = already_AddRefed<DBusConnection>(
    dbus_bus_get_private(DBUS_BUS_SYSTEM, &err));

  if (dbus_error_is_set(&err)) {
    dbus_error_free(&err);
    return NS_ERROR_FAILURE;
  }

  dbus_connection_set_exit_on_disconnect(mConnection, FALSE);

  return NS_OK;
}

bool RawDBusConnection::Watch()
{
  MOZ_ASSERT(MessageLoop::current());

  return NS_SUCCEEDED(DBusWatchConnection(mConnection));
}

bool RawDBusConnection::Send(DBusMessage* aMessage)
{
  MOZ_ASSERT(MessageLoop::current());

  auto rv = DBusSendMessage(mConnection, aMessage);

  if (NS_FAILED(rv)) {
    dbus_message_unref(aMessage);
    return false;
  }

  return true;
}

bool RawDBusConnection::SendWithReply(DBusReplyCallback aCallback,
                                      void* aData,
                                      int aTimeout,
                                      DBusMessage* aMessage)
{
  MOZ_ASSERT(MessageLoop::current());

  auto rv = DBusSendMessageWithReply(mConnection, aCallback, aData, aTimeout,
                                     aMessage);
  if (NS_FAILED(rv)) {
    return false;
  }

  dbus_message_unref(aMessage);

  return true;
}

bool RawDBusConnection::SendWithReply(DBusReplyCallback aCallback,
                                      void* aData,
                                      int aTimeout,
                                      const char* aDestination,
                                      const char* aPath,
                                      const char* aIntf,
                                      const char* aFunc,
                                      int aFirstArgType,
                                      ...)
{
  va_list args;
  va_start(args, aFirstArgType);

  auto rv = DBusSendMessageWithReply(mConnection, aCallback, aData,
                                     aTimeout, aDestination, aPath, aIntf,
                                     aFunc, aFirstArgType, args);
  va_end(args);

  return NS_SUCCEEDED(rv);
}

}
}
