/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DBusHelpers.h"
#include "mozilla/ipc/DBusMessageRefPtr.h"
#include "mozilla/ipc/DBusPendingCallRefPtr.h"
#include "mozilla/ipc/DBusWatcher.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "nsThreadUtils.h"

#undef CHROMIUM_LOG
#define CHROMIUM_LOG(args...)  printf(args);

namespace mozilla {
namespace ipc {

//
// DBus I/O
//

namespace {

class Notification final
{
public:
  Notification(DBusReplyCallback aCallback, void* aData)
    : mCallback(aCallback)
    , mData(aData)
  { }

  // Callback function for DBus replies. Only run it on I/O thread.
  //
  static void Handle(DBusPendingCall* aCall, void* aData)
  {
    MOZ_ASSERT(!NS_IsMainThread());

    RefPtr<DBusPendingCall> call = already_AddRefed<DBusPendingCall>(aCall);

    UniquePtr<Notification> ntfn(static_cast<Notification*>(aData));

    RefPtr<DBusMessage> reply = already_AddRefed<DBusMessage>(
      dbus_pending_call_steal_reply(call));

    // The reply can be null if the timeout has been reached.
    if (reply) {
      ntfn->RunCallback(reply);
    }

    dbus_pending_call_cancel(call);
  }

private:
  void RunCallback(DBusMessage* aMessage)
  {
    if (mCallback) {
      mCallback(aMessage, mData);
    }
  }

  DBusReplyCallback mCallback;
  void*             mData;
};

static already_AddRefed<DBusMessage>
BuildDBusMessage(const char* aDestination,
                 const char* aPath,
                 const char* aIntf,
                 const char* aFunc,
                 int aFirstArgType,
                 va_list aArgs)
{
  RefPtr<DBusMessage> msg = already_AddRefed<DBusMessage>(
    dbus_message_new_method_call(aDestination, aPath, aIntf, aFunc));

  if (!msg) {
    CHROMIUM_LOG("dbus_message_new_method_call failed");
    return nullptr;
  }

  auto success = dbus_message_append_args_valist(msg, aFirstArgType, aArgs);

  if (!success) {
    CHROMIUM_LOG("dbus_message_append_args_valist failed");
    return nullptr;
  }

  return msg.forget();
}

} // anonymous namespace

nsresult
DBusWatchConnection(DBusConnection* aConnection)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConnection);

  auto success =
    dbus_connection_set_watch_functions(aConnection,
                                        DBusWatcher::AddWatchFunction,
                                        DBusWatcher::RemoveWatchFunction,
                                        DBusWatcher::ToggleWatchFunction,
                                        aConnection, nullptr);
  if (!success) {
    CHROMIUM_LOG("dbus_connection_set_watch_functions failed");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

void
DBusUnwatchConnection(DBusConnection* aConnection)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConnection);

  auto success = dbus_connection_set_watch_functions(aConnection,
                                                     nullptr, nullptr, nullptr,
                                                     nullptr, nullptr);
  if (!success) {
    CHROMIUM_LOG("dbus_connection_set_watch_functions failed");
  }
}

nsresult
DBusSendMessage(DBusConnection* aConnection, DBusMessage* aMessage)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(aMessage);

  auto success = dbus_connection_send(aConnection, aMessage, nullptr);

  if (!success) {
    CHROMIUM_LOG("dbus_connection_send failed");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
DBusSendMessageWithReply(DBusConnection* aConnection,
                         DBusReplyCallback aCallback, void* aData,
                         int aTimeout,
                         DBusMessage* aMessage)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(aMessage);

  UniquePtr<Notification> ntfn = MakeUnique<Notification>(aCallback, aData);

  auto call = static_cast<DBusPendingCall*>(nullptr);

  auto success = dbus_connection_send_with_reply(aConnection,
                                                 aMessage,
                                                 &call,
                                                 aTimeout);
  if (!success) {
    CHROMIUM_LOG("dbus_connection_send_with_reply failed");
    return NS_ERROR_FAILURE;
  }

  success = dbus_pending_call_set_notify(call, Notification::Handle,
                                         ntfn.get(), nullptr);
  if (!success) {
    CHROMIUM_LOG("dbus_pending_call_set_notify failed");
    return NS_ERROR_FAILURE;
  }

  Unused << ntfn.release(); // Picked up in |Notification::Handle|

  return NS_OK;
}

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
                         va_list aArgs)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConnection);

  RefPtr<DBusMessage> msg =
    BuildDBusMessage(aDestination, aPath, aIntf, aFunc, aFirstArgType, aArgs);

  if (!msg) {
    return NS_ERROR_FAILURE;
  }

  return DBusSendMessageWithReply(aConnection, aCallback, aData, aTimeout, msg);
}

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
                         ...)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConnection);

  va_list args;
  va_start(args, aFirstArgType);

  auto rv = DBusSendMessageWithReply(aConnection,
                                     aCallback, aData,
                                     aTimeout,
                                     aDestination, aPath, aIntf, aFunc,
                                     aFirstArgType, args);
  va_end(args);

  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

}
}
