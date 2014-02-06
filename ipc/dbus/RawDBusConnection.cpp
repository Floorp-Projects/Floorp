/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <dbus/dbus.h>
#include "base/message_loop.h"
#include "nsThreadUtils.h"
#include "RawDBusConnection.h"

#ifdef CHROMIUM_LOG
#undef CHROMIUM_LOG
#endif

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define CHROMIUM_LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk", args);
#else
#define CHROMIUM_LOG(args...)  printf(args);
#endif

/* TODO: Remove BlueZ constant */
#define BLUEZ_DBUS_BASE_IFC "org.bluez"

//
// Runnables
//

namespace mozilla {
namespace ipc {

class Notification
{
public:
  Notification(DBusReplyCallback aCallback, void* aData)
  : mCallback(aCallback),
    mData(aData)
  { }

  // Callback function for DBus replies. Only run it on I/O thread.
  //
  static void Handle(DBusPendingCall* aCall, void* aData)
  {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(MessageLoop::current());

    nsAutoPtr<Notification> ntfn(static_cast<Notification*>(aData));

    // The reply can be non-null if the timeout has been reached.
    DBusMessage* reply = dbus_pending_call_steal_reply(aCall);

    if (reply) {
      ntfn->RunCallback(reply);
      dbus_message_unref(reply);
    }

    dbus_pending_call_cancel(aCall);
    dbus_pending_call_unref(aCall);
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
  mConnection = dbus_bus_get_private(DBUS_BUS_SYSTEM, &err);
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
    dbus_connection_close(ptr);
    dbus_connection_unref(ptr);
  }
}

bool RawDBusConnection::Send(DBusMessage* aMessage)
{
  MOZ_ASSERT(aMessage);
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(MessageLoop::current());

  dbus_bool_t success = dbus_connection_send(mConnection,
                                             aMessage,
                                             nullptr);
  if (success != TRUE) {
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
  MOZ_ASSERT(aMessage);
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(MessageLoop::current());

  nsAutoPtr<Notification> ntfn(new Notification(aCallback, aData));
  NS_ENSURE_TRUE(ntfn, false);

  DBusPendingCall* call;

  dbus_bool_t success = dbus_connection_send_with_reply(mConnection,
                                                        aMessage,
                                                        &call,
                                                        aTimeout);
  NS_ENSURE_TRUE(success == TRUE, false);

  success = dbus_pending_call_set_notify(call, Notification::Handle,
                                         ntfn, nullptr);
  NS_ENSURE_TRUE(success == TRUE, false);

  ntfn.forget();
  dbus_message_unref(aMessage);

  return true;
}

bool RawDBusConnection::SendWithReply(DBusReplyCallback aCallback,
                                      void* aData,
                                      int aTimeout,
                                      const char* aPath,
                                      const char* aIntf,
                                      const char* aFunc,
                                      int aFirstArgType,
                                      ...)
{
  MOZ_ASSERT(!NS_IsMainThread());
  va_list args;

  va_start(args, aFirstArgType);
  DBusMessage* msg = BuildDBusMessage(aPath, aIntf, aFunc,
                                      aFirstArgType, args);
  va_end(args);

  if (!msg) {
    return false;
  }

  return SendWithReply(aCallback, aData, aTimeout, msg);
}

DBusMessage* RawDBusConnection::BuildDBusMessage(const char* aPath,
                                                 const char* aIntf,
                                                 const char* aFunc,
                                                 int aFirstArgType,
                                                 va_list aArgs)
{
  DBusMessage* msg = dbus_message_new_method_call(BLUEZ_DBUS_BASE_IFC,
                                                  aPath, aIntf, aFunc);
  if (!msg) {
    CHROMIUM_LOG("Could not allocate D-Bus message object!");
    return nullptr;
  }

  /* append arguments */
  if (!dbus_message_append_args_valist(msg, aFirstArgType, aArgs)) {
    CHROMIUM_LOG("Could not append argument to method call!");
    dbus_message_unref(msg);
    return nullptr;
  }

  return msg;
}

}
}
