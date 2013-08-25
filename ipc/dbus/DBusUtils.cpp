/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/*
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include "DBusUtils.h"
#include "DBusThread.h"
#include "nsThreadUtils.h"
#include "mozilla/Monitor.h"
#include "nsAutoPtr.h"
#include <cstdio>
#include <cstring>

#undef LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk", args);
#else
#define LOG(args...)  printf(args);
#endif

#define BLUEZ_DBUS_BASE_PATH      "/org/bluez"
#define BLUEZ_DBUS_BASE_IFC       "org.bluez"
#define BLUEZ_ERROR_IFC           "org.bluez.Error"

namespace mozilla {
namespace ipc {

void
log_and_free_dbus_error(DBusError* err, const char* function, DBusMessage* msg)
{
  if (msg) {
    LOG("%s: D-Bus error in %s: %s (%s)", function,
        dbus_message_get_member((msg)), (err)->name, (err)->message);
  }	else {
    LOG("%s: D-Bus error: %s (%s)", __FUNCTION__,
        (err)->name, (err)->message);
  }
  dbus_error_free((err));
}

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

class DBusConnectionSendSyncRunnable : public DBusConnectionSendRunnableBase
{
public:
  bool WaitForCompletion()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    MonitorAutoLock autoLock(mCompletedMonitor);
    while (!mCompleted) {
      mCompletedMonitor.Wait();
    }
    return mSuccess;
  }

protected:
  DBusConnectionSendSyncRunnable(DBusConnection* aConnection,
                                 DBusMessage* aMessage)
  : DBusConnectionSendRunnableBase(aConnection, aMessage),
    mCompletedMonitor("DBusConnectionSendSyncRunnable.mCompleted"),
    mCompleted(false),
    mSuccess(false)
  { }

  virtual ~DBusConnectionSendSyncRunnable()
  { }

  // Call this function at the end of Run() to notify waiting
  // threads.
  void Completed(bool aSuccess)
  {
    MonitorAutoLock autoLock(mCompletedMonitor);
    MOZ_ASSERT(!mCompleted);
    mSuccess = aSuccess;
    mCompleted = true;
    mCompletedMonitor.Notify();
  }

private:
  Monitor mCompletedMonitor;
  bool    mCompleted;
  bool    mSuccess;
};

//
// Sends a message and returns the message's serial number to the
// disaptching thread. Only run it in DBus thread.
//
class DBusConnectionSendRunnable : public DBusConnectionSendSyncRunnable
{
public:
  DBusConnectionSendRunnable(DBusConnection* aConnection,
                             DBusMessage* aMessage,
                             dbus_uint32_t* aSerial)
  : DBusConnectionSendSyncRunnable(aConnection, aMessage),
    mSerial(aSerial)
  { }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    dbus_bool_t success = dbus_connection_send(mConnection, mMessage, mSerial);
    Completed(success == TRUE);

    NS_ENSURE_TRUE(success == TRUE, NS_ERROR_FAILURE);

    return NS_OK;
  }

protected:
  ~DBusConnectionSendRunnable()
  { }

private:
  dbus_uint32_t* mSerial;
};

dbus_bool_t
dbus_func_send(DBusConnection* aConnection, dbus_uint32_t* aSerial,
               DBusMessage* aMessage)
{
  nsRefPtr<DBusConnectionSendRunnable> t(
    new DBusConnectionSendRunnable(aConnection, aMessage, aSerial));
  MOZ_ASSERT(t);

  nsresult rv = DispatchToDBusThread(t);

  if (NS_FAILED(rv)) {
    if (aMessage) {
      dbus_message_unref(aMessage);
    }
    return FALSE;
  }

  if (aSerial && !t->WaitForCompletion()) {
    return FALSE;
  }

  return TRUE;
}

static dbus_bool_t
dbus_func_args_send_valist(DBusConnection* aConnection,
                           dbus_uint32_t* aSerial,
                           const char* aPath,
                           const char* aInterface,
                           const char* aFunction,
                           int aFirstArgType,
                           va_list aArgs)
{
  // Compose the command...
  DBusMessage* message = dbus_message_new_method_call(BLUEZ_DBUS_BASE_IFC,
                                                      aPath,
                                                      aInterface,
                                                      aFunction);
  if (!message) {
    LOG("Could not allocate D-Bus message object!");
    goto done;
  }

  // ... and append arguments.
  if (!dbus_message_append_args_valist(message, aFirstArgType, aArgs)) {
    LOG("Could not append argument to method call!");
    goto done;
  }

  return dbus_func_send(aConnection, aSerial, message);

done:
  if (message) {
    dbus_message_unref(message);
  }
  return FALSE;
}

dbus_bool_t
dbus_func_args_send(DBusConnection* aConnection,
                    dbus_uint32_t* aSerial,
                    const char* aPath,
                    const char* aInterface,
                    const char* aFunction,
                    int aFirstArgType, ...)
{
  va_list args;
  va_start(args, aFirstArgType);

  dbus_bool_t success = dbus_func_args_send_valist(aConnection,
                                                   aSerial,
                                                   aPath,
                                                   aInterface,
                                                   aFunction,
                                                   aFirstArgType,
                                                   args);
  va_end(args);

  return success;
}

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
    NotifyData(void (*aCallback)(DBusMessage*, void*), void* aData)
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
    void (*mCallback)(DBusMessage*, void*);
    void*  mData;
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
                                      void (*aCallback)(DBusMessage*, void*),
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
  void (*mCallback)(DBusMessage*, void*);
  void*  mData;
  int    mTimeout;
};

dbus_bool_t dbus_func_send_async(DBusConnection* conn,
                                 DBusMessage* msg,
                                 int timeout_ms,
                                 void (*user_cb)(DBusMessage*,
                                                 void*),
                                 void* user)
{
  nsRefPtr<nsIRunnable> t(new DBusConnectionSendWithReplyRunnable(conn, msg,
                                                                  timeout_ms,
                                                                  user_cb,
                                                                  user));
  MOZ_ASSERT(t);

  nsresult rv = DispatchToDBusThread(t);

  if (NS_FAILED(rv)) {
    if (msg) {
      dbus_message_unref(msg);
    }
    return FALSE;
  }

  return TRUE;
}

static dbus_bool_t dbus_func_args_async_valist(DBusConnection *conn,
                                               int timeout_ms,
                                               void (*user_cb)(DBusMessage*,
                                                               void*),
                                               void *user,
                                               const char *path,
                                               const char *ifc,
                                               const char *func,
                                               int first_arg_type,
                                               va_list args) {
  DBusMessage *msg = nullptr;
  /* Compose the command */
  msg = dbus_message_new_method_call(BLUEZ_DBUS_BASE_IFC, path, ifc, func);

  if (msg == nullptr) {
    LOG("Could not allocate D-Bus message object!");
    goto done;
  }

  /* append arguments */
  if (!dbus_message_append_args_valist(msg, first_arg_type, args)) {
    LOG("Could not append argument to method call!");
    goto done;
  }

  return dbus_func_send_async(conn, msg, timeout_ms, user_cb, user);
done:
  if (msg) dbus_message_unref(msg);
  return FALSE;
}

dbus_bool_t dbus_func_args_async(DBusConnection *conn,
                                 int timeout_ms,
                                 void (*reply)(DBusMessage *, void *),
                                 void *user,
                                 const char *path,
                                 const char *ifc,
                                 const char *func,
                                 int first_arg_type,
                                 ...) {
  dbus_bool_t ret;
  va_list lst;
  va_start(lst, first_arg_type);

  ret = dbus_func_args_async_valist(conn,
                                    timeout_ms,
                                    reply, user,
                                    path, ifc, func,
                                    first_arg_type, lst);
  va_end(lst);
  return ret;
}

//
// Sends a message and allows the dispatching thread to wait for the
// reply. Only run it in DBus thread.
//
class DBusConnectionSendAndBlockRunnable : public DBusConnectionSendSyncRunnable
{
private:
  static void Notify(DBusPendingCall* aCall, void* aData)
  {
    DBusConnectionSendAndBlockRunnable* runnable(
        static_cast<DBusConnectionSendAndBlockRunnable*>(aData));

    runnable->mReply = dbus_pending_call_steal_reply(aCall);

    bool success = !!runnable->mReply;

    if (runnable->mError) {
      success = success && !dbus_error_is_set(runnable->mError);

      if (!dbus_set_error_from_message(runnable->mError, runnable->mReply)) {
        dbus_error_init(runnable->mError);
      }
    }

    dbus_pending_call_cancel(aCall);
    dbus_pending_call_unref(aCall);

    runnable->Completed(success);
  }

public:
  DBusConnectionSendAndBlockRunnable(DBusConnection* aConnection,
                                     DBusMessage* aMessage,
                                     int aTimeout,
                                     DBusError* aError)
  : DBusConnectionSendSyncRunnable(aConnection, aMessage),
    mError(aError),
    mReply(nullptr),
    mTimeout(aTimeout)
  { }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    DBusPendingCall* call = nullptr;

    dbus_bool_t success = dbus_connection_send_with_reply(mConnection,
                                                          mMessage,
                                                          &call,
                                                          mTimeout);
    if (!success) {
      if (mError) {
        if (!call) {
          dbus_set_error(mError, DBUS_ERROR_DISCONNECTED, "Connection is closed");
        } else {
          dbus_error_init(mError);
        }
      }
      goto done;
    }

    success = dbus_pending_call_set_notify(call, Notify, this, nullptr);

  done:
    dbus_message_unref(mMessage);

    if (!success) {
      Completed(false);
      NS_ENSURE_TRUE(success == TRUE, NS_ERROR_FAILURE);
    }

    return NS_OK;
  }

  DBusMessage* GetReply()
  {
    return mReply;
  }

protected:
  ~DBusConnectionSendAndBlockRunnable()
  { }

private:
  DBusError*   mError;
  DBusMessage* mReply;
  int          mTimeout;
};

dbus_bool_t
dbus_func_send_and_block(DBusConnection* aConnection,
                         int aTimeout,
                         DBusMessage** aReply,
                         DBusError* aError,
                         DBusMessage* aMessage)
{
  nsRefPtr<DBusConnectionSendAndBlockRunnable> t(
    new DBusConnectionSendAndBlockRunnable(aConnection, aMessage,
                                           aTimeout, aError));
  MOZ_ASSERT(t);

  nsresult rv = DispatchToDBusThread(t);

  if (NS_FAILED(rv)) {
    if (aMessage) {
      dbus_message_unref(aMessage);
    }
    return FALSE;
  }

  if (!t->WaitForCompletion()) {
    return FALSE;
  }

  if (aReply) {
    *aReply = t->GetReply();
  }

  return TRUE;
}

// If err is nullptr, then any errors will be LOG'd, and free'd and the reply
// will be nullptr.
// If err is not nullptr, then it is assumed that dbus_error_init was already
// called, and error's will be returned to the caller without logging. The
// return value is nullptr iff an error was set. The client must free the
// error if set.
DBusMessage* dbus_func_args_timeout_valist(DBusConnection* conn,
                                           int timeout_ms,
                                           DBusError* err,
                                           const char* path,
                                           const char* ifc,
                                           const char* func,
                                           int first_arg_type,
                                           va_list args)
{
  /* Compose the command */
  DBusMessage* msg = dbus_message_new_method_call(BLUEZ_DBUS_BASE_IFC, path, ifc, func);

  if (!msg) {
    LOG("Could not allocate D-Bus message object!");
    goto done;
  }

  /* append arguments */
  if (!dbus_message_append_args_valist(msg, first_arg_type, args)) {
    LOG("Could not append argument to method call!");
    goto done;
  }

  DBusMessage* reply;

  return dbus_func_send_and_block(conn, timeout_ms, &reply, err, msg) ? reply : nullptr;

done:
  if (msg) {
    dbus_message_unref(msg);
  }
  return nullptr;
}

DBusMessage * dbus_func_args_timeout(DBusConnection *conn,
                                     int timeout_ms,
                                     DBusError* err,
                                     const char *path,
                                     const char *ifc,
                                     const char *func,
                                     int first_arg_type,
                                     ...) {
  DBusMessage *ret;
  va_list lst;
  va_start(lst, first_arg_type);
  ret = dbus_func_args_timeout_valist(conn, timeout_ms, err,
                                      path, ifc, func,
                                      first_arg_type, lst);
  va_end(lst);
  return ret;
}

DBusMessage * dbus_func_args(DBusConnection *conn,
                             const char *path,
                             const char *ifc,
                             const char *func,
                             int first_arg_type,
                             ...) {
  DBusMessage *ret;
  va_list lst;
  va_start(lst, first_arg_type);
  ret = dbus_func_args_timeout_valist(conn, -1, nullptr,
                                      path, ifc, func,
                                      first_arg_type, lst);
  va_end(lst);
  return ret;
}

DBusMessage * dbus_func_args_error(DBusConnection *conn,
                                   DBusError *err,
                                   const char *path,
                                   const char *ifc,
                                   const char *func,
                                   int first_arg_type,
                                   ...) {
  DBusMessage *ret;
  va_list lst;
  va_start(lst, first_arg_type);
  ret = dbus_func_args_timeout_valist(conn, -1, err,
                                      path, ifc, func,
                                      first_arg_type, lst);
  va_end(lst);
  return ret;
}

int dbus_returns_int32(DBusMessage *reply)
{
  DBusError err;
  int32_t ret = -1;

  dbus_error_init(&err);
  if (!dbus_message_get_args(reply, &err,
                             DBUS_TYPE_INT32, &ret,
                             DBUS_TYPE_INVALID)) {
    LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, reply);
  }

  return ret;
}

void DBusReplyHandler::Callback(DBusMessage* aReply, void* aData)
{
  MOZ_ASSERT(aData);

  nsRefPtr<DBusReplyHandler> handler =
    already_AddRefed<DBusReplyHandler>(static_cast<DBusReplyHandler*>(aData));

  handler->Handle(aReply);
}

}
}
