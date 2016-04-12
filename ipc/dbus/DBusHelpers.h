/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_DBusHelpers_h
#define mozilla_ipc_DBusHelpers_h

#include <dbus/dbus.h>
#include <stdarg.h>
#include "nsError.h"

namespace mozilla {
namespace ipc {

//
// DBus I/O
//

typedef void (*DBusReplyCallback)(DBusMessage*, void*);

nsresult
DBusWatchConnection(DBusConnection* aConnection);

void
DBusUnwatchConnection(DBusConnection* aConnection);

nsresult
DBusSendMessage(DBusConnection* aConnection, DBusMessage* aMessage);

nsresult
DBusSendMessageWithReply(DBusConnection* aConnection,
                         DBusReplyCallback aCallback, void* aData,
                         int aTimeout,
                         DBusMessage* aMessage);

nsresult
DBusSendMessageWithReply(DBusConnection* aConnection,
                         DBusReplyCallback aCallback,
                         void* aData,
                         int aTimeout,
                         const char* aDestination,
                         const char* aPath,
                         const char* aIntf,
                         const char* aFunc,
                         int aFirstArgType,
                         va_list aArgs);

nsresult
DBusSendMessageWithReply(DBusConnection* aConnection,
                         DBusReplyCallback aCallback,
                         void* aData,
                         int aTimeout,
                         const char* aDestination,
                         const char* aPath,
                         const char* aIntf,
                         const char* aFunc,
                         int aFirstArgType,
                         ...);

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_DBusHelpers_h
