/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_dbus_gonk_rawdbusconnection_h__
#define mozilla_ipc_dbus_gonk_rawdbusconnection_h__

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <stdlib.h>
#include "mozilla/Scoped.h"
#include "dbus/dbus.h"

struct DBusConnection;

namespace mozilla {
namespace ipc {

class RawDBusConnection
{
  struct ScopedDBusConnectionPtrTraits : ScopedFreePtrTraits<DBusConnection>
  {
    static void release(DBusConnection* ptr) { if(ptr) dbus_connection_unref(ptr); }
  };

public:
  RawDBusConnection();
  ~RawDBusConnection();
  bool Create();
protected:
  Scoped<ScopedDBusConnectionPtrTraits> mConnection;
};

}
}

#endif
