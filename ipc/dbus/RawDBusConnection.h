/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_dbus_gonk_rawdbusconnection_h__
#define mozilla_ipc_dbus_gonk_rawdbusconnection_h__

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "nscore.h"
#include "mozilla/Scoped.h"
#include <mozilla/RefPtr.h>
#include <mozilla/Mutex.h>

struct DBusConnection;
struct DBusError;
struct DBusMessage;

namespace mozilla {
namespace ipc {

typedef void (*DBusReplyCallback)(DBusMessage*, void*);

class RawDBusConnection : public AtomicRefCounted<RawDBusConnection>
{
  struct ScopedDBusConnectionPtrTraits : ScopedFreePtrTraits<DBusConnection>
  {
    static void release(DBusConnection* ptr);
  };

public:
  RawDBusConnection();
  virtual ~RawDBusConnection();

  nsresult EstablishDBusConnection();

  DBusConnection* GetConnection()
  {
    return mConnection;
  }

  bool Send(DBusMessage* aMessage);

  bool SendWithReply(DBusReplyCallback aCallback, void* aData,
                     int aTimeout, DBusMessage* aMessage);

  bool SendWithReply(DBusReplyCallback aCallback, void* aData,
                     int aTimeout, const char* aPath, const char* aIntf,
                     const char *aFunc, int aFirstArgType, ...);

  /* Legacy interface, don't use in new code */
  bool SendWithError(DBusMessage** aReply, DBusError* aError, int aTimeout,
                     DBusMessage* aMessage);

  /* Legacy interface, don't use in new code */
  bool SendWithError(DBusMessage** aReply, DBusError* aError, int aTimeout,
                     const char* aPath, const char* aIntf, const char* aFunc,
                     int aFirstArgType, ...);

protected:
  DBusMessage* BuildDBusMessage(const char* aPath, const char* aIntf,
                                const char* aFunc, int aFirstArgType,
                                va_list args);

  Scoped<ScopedDBusConnectionPtrTraits> mConnection;

private:
  static bool sDBusIsInit;
};

}
}

#endif
