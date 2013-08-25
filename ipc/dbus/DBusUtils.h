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

#ifndef mozilla_ipc_dbus_dbusutils_h__
#define mozilla_ipc_dbus_dbusutils_h__

#include <dbus/dbus.h>
#include "mozilla/RefPtr.h"
#include "mozilla/Scoped.h"

// LOGE and free a D-Bus error
// Using #define so that __FUNCTION__ resolves usefully
#define LOG_AND_FREE_DBUS_ERROR_WITH_MSG(err, msg) log_and_free_dbus_error(err, __FUNCTION__, msg);
#define LOG_AND_FREE_DBUS_ERROR(err) log_and_free_dbus_error(err, __FUNCTION__);

struct DBusMessage;
struct DBusError;

namespace mozilla {
namespace ipc {

class DBusMessageRefPtr
{
public:
  DBusMessageRefPtr(DBusMessage* aMsg) : mMsg(aMsg)
  {
    if (mMsg) dbus_message_ref(mMsg);
  }
  ~DBusMessageRefPtr()
  {
    if (mMsg) dbus_message_unref(mMsg);
  }
  operator DBusMessage*() { return mMsg; }
  DBusMessage* get() { return mMsg; }
private:
  DBusMessage* mMsg;
};

/**
 * DBusReplyHandler represents a handler for DBus reply messages. Inherit
 * from this class and implement the Handle method. The method Callback
 * should be passed to the DBus send function, with the class instance as
 * user-data argument.
 */
class DBusReplyHandler : public mozilla::RefCounted<DBusReplyHandler>
{
public:
  virtual ~DBusReplyHandler() {
  }

  /**
   * Implements a call-back function for DBus. The supplied value for
   * aData must be a pointer to an instance of DBusReplyHandler.
   */
  static void Callback(DBusMessage* aReply, void* aData);

  /**
   * Call-back method for handling the reply message from DBus.
   */
  virtual void Handle(DBusMessage* aReply) = 0;

protected:
  DBusReplyHandler()
  {
  }

  DBusReplyHandler(const DBusReplyHandler& aHandler)
  {
  }

  DBusReplyHandler& operator = (const DBusReplyHandler& aRhs)
  {
    return *this;
  }
};

typedef void (*DBusCallback)(DBusMessage *, void *);

void log_and_free_dbus_error(DBusError* err,
                             const char* function,
                             DBusMessage* msg = nullptr);

dbus_bool_t dbus_func_send(DBusConnection *aConnection,
                           dbus_uint32_t *aSerial,
                           DBusMessage *aMessage);

dbus_bool_t dbus_func_args_send(DBusConnection *aConnection,
                                dbus_uint32_t *aSerial,
                                const char *aPath,
                                const char *aInterface,
                                const char *aFunction,
                                int aFirstArgType, ...);

dbus_bool_t dbus_func_send_async(DBusConnection* conn,
                                 DBusMessage* msg,
                                 int timeout_ms,
                                 DBusCallback user_cb,
                                 void* user);

dbus_bool_t dbus_func_args_async(DBusConnection* conn,
                                 int timeout_ms,
                                 DBusCallback reply,
                                 void* user,
                                 const char* path,
                                 const char* ifc,
                                 const char* func,
                                 int first_arg_type,
                                 ...);

dbus_bool_t dbus_func_send_and_block(DBusConnection* aConnection,
                                     int aTimeout,
                                     DBusMessage** aReply,
                                     DBusError* aError,
                                     DBusMessage* aMessage);

DBusMessage*  dbus_func_args(DBusConnection* conn,
                             const char* path,
                             const char* ifc,
                             const char* func,
                             int first_arg_type,
                             ...);

DBusMessage*  dbus_func_args_error(DBusConnection* conn,
                                   DBusError* err,
                                   const char* path,
                                   const char* ifc,
                                   const char* func,
                                   int first_arg_type,
                                   ...);

DBusMessage*  dbus_func_args_timeout(DBusConnection* conn,
                                     int timeout_ms,
                                     DBusError* err,
                                     const char* path,
                                     const char* ifc,
                                     const char* func,
                                     int first_arg_type,
                                     ...);

DBusMessage*  dbus_func_args_timeout_valist(DBusConnection* conn,
                                            int timeout_ms,
                                            DBusError* err,
                                            const char* path,
                                            const char* ifc,
                                            const char* func,
                                            int first_arg_type,
                                            va_list args);

int dbus_returns_int32(DBusMessage *reply);

}
}

#endif

