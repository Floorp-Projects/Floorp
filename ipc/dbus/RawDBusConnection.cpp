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

namespace mozilla {
namespace ipc {

//
// DBusWatcher
//

class DBusWatcher : public MessageLoopForIO::Watcher
{
public:
  DBusWatcher(RawDBusConnection* aConnection, DBusWatch* aWatch)
  : mConnection(aConnection),
    mWatch(aWatch)
  {
    MOZ_ASSERT(mConnection);
    MOZ_ASSERT(mWatch);
  }

  ~DBusWatcher()
  { }

  void StartWatching();
  void StopWatching();

  static void        FreeFunction(void* aData);
  static dbus_bool_t AddWatchFunction(DBusWatch* aWatch, void* aData);
  static void        RemoveWatchFunction(DBusWatch* aWatch, void* aData);
  static void        ToggleWatchFunction(DBusWatch* aWatch, void* aData);

  RawDBusConnection* GetConnection();

private:
  void OnFileCanReadWithoutBlocking(int aFd);
  void OnFileCanWriteWithoutBlocking(int aFd);

  // Read watcher for libevent. Only to be accessed on IO Thread.
  MessageLoopForIO::FileDescriptorWatcher mReadWatcher;

  // Write watcher for libevent. Only to be accessed on IO Thread.
  MessageLoopForIO::FileDescriptorWatcher mWriteWatcher;

  // DBus structures
  RawDBusConnection* mConnection;
  DBusWatch* mWatch;
};

RawDBusConnection*
DBusWatcher::GetConnection()
{
  return mConnection;
}

void DBusWatcher::StartWatching()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(mWatch);

  int fd = dbus_watch_get_unix_fd(mWatch);

  MessageLoopForIO* ioLoop = MessageLoopForIO::current();

  unsigned int flags = dbus_watch_get_flags(mWatch);

  if (flags & DBUS_WATCH_READABLE) {
    ioLoop->WatchFileDescriptor(fd, true, MessageLoopForIO::WATCH_READ,
                                &mReadWatcher, this);
  }
  if (flags & DBUS_WATCH_WRITABLE) {
    ioLoop->WatchFileDescriptor(fd, true, MessageLoopForIO::WATCH_WRITE,
                                &mWriteWatcher, this);
  }
}

void DBusWatcher::StopWatching()
{
  MOZ_ASSERT(!NS_IsMainThread());

  unsigned int flags = dbus_watch_get_flags(mWatch);

  if (flags & DBUS_WATCH_READABLE) {
    mReadWatcher.StopWatchingFileDescriptor();
  }
  if (flags & DBUS_WATCH_WRITABLE) {
    mWriteWatcher.StopWatchingFileDescriptor();
  }
}

// DBus utility functions, used as function pointers in DBus setup

void
DBusWatcher::FreeFunction(void* aData)
{
  delete static_cast<DBusWatcher*>(aData);
}

dbus_bool_t
DBusWatcher::AddWatchFunction(DBusWatch* aWatch, void* aData)
{
  MOZ_ASSERT(!NS_IsMainThread());

  RawDBusConnection* connection = static_cast<RawDBusConnection*>(aData);

  DBusWatcher* dbusWatcher = new DBusWatcher(connection, aWatch);
  dbus_watch_set_data(aWatch, dbusWatcher, DBusWatcher::FreeFunction);

  if (dbus_watch_get_enabled(aWatch)) {
    dbusWatcher->StartWatching();
  }

  return TRUE;
}

void
DBusWatcher::RemoveWatchFunction(DBusWatch* aWatch, void* aData)
{
  MOZ_ASSERT(!NS_IsMainThread());

  DBusWatcher* dbusWatcher =
    static_cast<DBusWatcher*>(dbus_watch_get_data(aWatch));
  dbusWatcher->StopWatching();
}

void
DBusWatcher::ToggleWatchFunction(DBusWatch* aWatch, void* aData)
{
  MOZ_ASSERT(!NS_IsMainThread());

  DBusWatcher* dbusWatcher =
    static_cast<DBusWatcher*>(dbus_watch_get_data(aWatch));

  if (dbus_watch_get_enabled(aWatch)) {
    dbusWatcher->StartWatching();
  } else {
    dbusWatcher->StopWatching();
  }
}

// I/O-loop callbacks

void
DBusWatcher::OnFileCanReadWithoutBlocking(int aFd)
{
  MOZ_ASSERT(!NS_IsMainThread());

  dbus_watch_handle(mWatch, DBUS_WATCH_READABLE);

  DBusDispatchStatus dbusDispatchStatus;
  do {
    dbusDispatchStatus =
      dbus_connection_dispatch(mConnection->GetConnection());
  } while (dbusDispatchStatus == DBUS_DISPATCH_DATA_REMAINS);
}

void
DBusWatcher::OnFileCanWriteWithoutBlocking(int aFd)
{
  MOZ_ASSERT(!NS_IsMainThread());

  dbus_watch_handle(mWatch, DBUS_WATCH_WRITABLE);
}

//
// Notification
//

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

bool RawDBusConnection::Watch()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(MessageLoop::current());

  dbus_bool_t success =
    dbus_connection_set_watch_functions(mConnection,
                                        DBusWatcher::AddWatchFunction,
                                        DBusWatcher::RemoveWatchFunction,
                                        DBusWatcher::ToggleWatchFunction,
                                        this, nullptr);
  NS_ENSURE_TRUE(success == TRUE, false);

  return true;
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
