/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <dbus/dbus.h>
#include "mozilla/Monitor.h"
#include "nsThreadUtils.h"
#include "DBusThread.h"
#include "DBusUtils.h"
#include "RawDBusConnection.h"

#ifdef LOG
#undef LOG
#endif

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk", args);
#else
#define LOG(args...)  printf(args);
#endif

/* TODO: Remove BlueZ constant */
#define BLUEZ_DBUS_BASE_IFC "org.bluez"

using namespace mozilla::ipc;

//
// Runnables
//

namespace mozilla {
namespace ipc {

class DBusConnectionSendRunnableBase : public nsRunnable
{
protected:
  DBusConnectionSendRunnableBase(DBusConnection* aConnection,
                                 DBusMessage* aMessage)
  : mConnection(aConnection),
    mMessage(aMessage)
  {
    MOZ_ASSERT(mConnection);
    MOZ_ASSERT(mMessage);
  }

  virtual ~DBusConnectionSendRunnableBase()
  { }

  DBusConnection*   mConnection;
  DBusMessageRefPtr mMessage;
};

//
// Sends a message and returns the message's serial number to the
// disaptching thread. Only run it in DBus thread.
//
class DBusConnectionSendRunnable : public DBusConnectionSendRunnableBase
{
public:
  DBusConnectionSendRunnable(DBusConnection* aConnection,
                             DBusMessage* aMessage)
  : DBusConnectionSendRunnableBase(aConnection, aMessage)
  { }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    dbus_bool_t success = dbus_connection_send(mConnection, mMessage, nullptr);

    NS_ENSURE_TRUE(success == TRUE, NS_ERROR_FAILURE);

    return NS_OK;
  }

protected:
  ~DBusConnectionSendRunnable()
  { }
};

//
// Sends a message and executes a callback function for the reply. Only
// run it in DBus thread.
//
class DBusConnectionSendWithReplyRunnable : public DBusConnectionSendRunnableBase
{
private:
  class NotifyData
  {
  public:
    NotifyData(DBusReplyCallback aCallback, void* aData)
    : mCallback(aCallback),
      mData(aData)
    { }

    void RunNotifyCallback(DBusMessage* aMessage)
    {
      if (mCallback) {
        mCallback(aMessage, mData);
      }
    }

  private:
    DBusReplyCallback mCallback;
    void*             mData;
  };

  // Callback function for DBus replies. Only run it in DBus thread.
  //
  static void Notify(DBusPendingCall* aCall, void* aData)
  {
    MOZ_ASSERT(!NS_IsMainThread());

    nsAutoPtr<NotifyData> data(static_cast<NotifyData*>(aData));

    // The reply can be non-null if the timeout
    // has been reached.
    DBusMessage* reply = dbus_pending_call_steal_reply(aCall);

    if (reply) {
      data->RunNotifyCallback(reply);
      dbus_message_unref(reply);
    }

    dbus_pending_call_cancel(aCall);
    dbus_pending_call_unref(aCall);
  }

public:
  DBusConnectionSendWithReplyRunnable(DBusConnection* aConnection,
                                      DBusMessage* aMessage,
                                      int aTimeout,
                                      DBusReplyCallback aCallback,
                                      void* aData)
  : DBusConnectionSendRunnableBase(aConnection, aMessage),
    mCallback(aCallback),
    mData(aData),
    mTimeout(aTimeout)
  { }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    // Freed at end of Notify
    nsAutoPtr<NotifyData> data(new NotifyData(mCallback, mData));
    NS_ENSURE_TRUE(data, NS_ERROR_OUT_OF_MEMORY);

    DBusPendingCall* call;

    dbus_bool_t success = dbus_connection_send_with_reply(mConnection,
                                                          mMessage,
                                                          &call,
                                                          mTimeout);
    NS_ENSURE_TRUE(success == TRUE, NS_ERROR_FAILURE);

    success = dbus_pending_call_set_notify(call, Notify, data, nullptr);
    NS_ENSURE_TRUE(success == TRUE, NS_ERROR_FAILURE);

    data.forget();
    dbus_message_unref(mMessage);

    return NS_OK;
  };

protected:
  ~DBusConnectionSendWithReplyRunnable()
  { }

private:
  DBusReplyCallback mCallback;
  void*             mData;
  int               mTimeout;
};

}
}

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

bool RawDBusConnection::Send(DBusMessage* aMessage)
{
  nsRefPtr<DBusConnectionSendRunnable> t(
    new DBusConnectionSendRunnable(mConnection, aMessage));
  MOZ_ASSERT(t);

  nsresult rv = DispatchToDBusThread(t);

  if (NS_FAILED(rv)) {
    if (aMessage) {
      dbus_message_unref(aMessage);
    }
    return false;
  }

  return true;
}

bool RawDBusConnection::SendWithReply(DBusReplyCallback aCallback,
                                      void* aData,
                                      int aTimeout,
                                      DBusMessage* aMessage)
{
  nsRefPtr<nsIRunnable> t(
    new DBusConnectionSendWithReplyRunnable(mConnection, aMessage,
                                            aTimeout, aCallback, aData));
  MOZ_ASSERT(t);

  nsresult rv = DispatchToDBusThread(t);

  if (NS_FAILED(rv)) {
    if (aMessage) {
      dbus_message_unref(aMessage);
    }
    return false;
  }

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
    LOG("Could not allocate D-Bus message object!");
    return nullptr;
  }

  /* append arguments */
  if (!dbus_message_append_args_valist(msg, aFirstArgType, aArgs)) {
    LOG("Could not append argument to method call!");
    dbus_message_unref(msg);
    return nullptr;
  }

  return msg;
}
