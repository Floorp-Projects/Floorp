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

#include <dbus/dbus.h>
#include "nsAutoPtr.h"
#include "DBusUtils.h"

#undef LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk", args);
#else
#define LOG(args...)  printf(args);
#endif

namespace mozilla {
namespace ipc {

//
// DBusMessageRefPtr
//

DBusMessageRefPtr::DBusMessageRefPtr(DBusMessage* aMsg)
  : mMsg(aMsg)
{
  if (mMsg) {
    dbus_message_ref(mMsg);
  }
}

DBusMessageRefPtr::~DBusMessageRefPtr()
{
  if (mMsg) {
    dbus_message_unref(mMsg);
  }
}

//
// DBusReplyHandler
//

void DBusReplyHandler::Callback(DBusMessage* aReply, void* aData)
{
  MOZ_ASSERT(aData);

  nsRefPtr<DBusReplyHandler> handler =
    already_AddRefed<DBusReplyHandler>(static_cast<DBusReplyHandler*>(aData));

  handler->Handle(aReply);
}

//
// Utility functions
//

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

}
}
