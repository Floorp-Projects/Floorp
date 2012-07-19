/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
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

#include "BluetoothUtils.h"

#include <cstdio>
#include <dbus/dbus.h>

#include "nsAutoPtr.h"
#include "nsThreadUtils.h"
#include "nsDebug.h"
#include "nsClassHashtable.h"
#include "mozilla/ipc/DBusUtils.h"
#include "mozilla/ipc/RawDBusConnection.h"


using namespace mozilla::ipc;

namespace mozilla {
namespace dom {
namespace bluetooth {

static nsAutoPtr<RawDBusConnection> sDBusConnection;

#undef LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GonkDBus", args);
#else
#define BTDEBUG true
#define LOG(args...) if (BTDEBUG) printf(args);
#endif

#define DBUS_ADAPTER_IFACE BLUEZ_DBUS_BASE_IFC ".Adapter"
#define DBUS_DEVICE_IFACE BLUEZ_DBUS_BASE_IFC ".Device"
#define BLUEZ_DBUS_BASE_PATH      "/org/bluez"
#define BLUEZ_DBUS_BASE_IFC       "org.bluez"
#define BLUEZ_ERROR_IFC           "org.bluez.Error"

static const char* BLUETOOTH_DBUS_SIGNALS[] =
{
  "type='signal',interface='org.freedesktop.DBus'",
  "type='signal',interface='org.bluez.Adapter'",
  "type='signal',interface='org.bluez.Manager'",
  "type='signal',interface='org.bluez.Device'",
  "type='signal',interface='org.bluez.Input'",
  "type='signal',interface='org.bluez.Network'",
  "type='signal',interface='org.bluez.NetworkServer'",
  "type='signal',interface='org.bluez.HealthDevice'",
  "type='signal',interface='org.bluez.AudioSink'"
};

typedef nsClassHashtable<nsCStringHashKey, BluetoothEventObserverList >
        BluetoothEventObserverTable;
static nsAutoPtr<BluetoothEventObserverTable> sBluetoothEventObserverTable;

nsresult
RegisterBluetoothEventHandler(const nsCString& aNodeName,
                              BluetoothEventObserver* aHandler)
{
  MOZ_ASSERT(NS_IsMainThread());
  BluetoothEventObserverList *ol;

  NS_ENSURE_TRUE(sBluetoothEventObserverTable, NS_ERROR_FAILURE);
  if (!sBluetoothEventObserverTable->Get(aNodeName, &ol)) {
    sBluetoothEventObserverTable->Put(aNodeName,
                                      new BluetoothEventObserverList());
  }
  sBluetoothEventObserverTable->Get(aNodeName, &ol);
  ol->AddObserver(aHandler);
  return NS_OK;
}

nsresult
UnregisterBluetoothEventHandler(const nsCString& aNodeName,
                                BluetoothEventObserver* aHandler)
{
  MOZ_ASSERT(NS_IsMainThread());
  BluetoothEventObserverList *ol;

  NS_ENSURE_TRUE(sBluetoothEventObserverTable, NS_ERROR_FAILURE);
  if (!sBluetoothEventObserverTable->Get(aNodeName, &ol)) {
    NS_WARNING("Node does not exist to remove BluetoothEventListener from!");
    return NS_ERROR_FAILURE;
  }
  sBluetoothEventObserverTable->Get(aNodeName, &ol);  
  ol->RemoveObserver(aHandler);
  if (ol->Length() == 0) {
    sBluetoothEventObserverTable->Remove(aNodeName);
  }
  return NS_OK;
}

struct DistributeDBusMessageTask : public nsRunnable {

  DistributeDBusMessageTask(DBusMessage* aMsg) : mMsg(aMsg)
  {
  }
  
  NS_IMETHOD Run()
  {
    if (dbus_message_get_path(mMsg.get()) == NULL) {
      return NS_OK;
    }    
    MOZ_ASSERT(NS_IsMainThread());
    
    // Notify observers that a message has been sent
    nsDependentCString path(dbus_message_get_path(mMsg.get()));
    nsDependentCString member(dbus_message_get_member(mMsg.get()));
    BluetoothEventObserverList *ol;
    if (!sBluetoothEventObserverTable->Get(path, &ol)) {
      LOG("No objects registered for %s, returning\n",
          dbus_message_get_path(mMsg.get()));
      return NS_OK;
    }
    BluetoothEvent e;
    e.mEventName = member;
    ol->Broadcast(e);
    return NS_OK;
  }

  DBusMessageRefPtr mMsg;
};

// Called by dbus during WaitForAndDispatchEventNative()
// This function is called on the IOThread
static DBusHandlerResult
EventFilter(DBusConnection *aConn, DBusMessage *aMsg,
            void *aData)
{
  DBusError err;

  dbus_error_init(&err);

  if (dbus_message_get_type(aMsg) != DBUS_MESSAGE_TYPE_SIGNAL) {
    LOG("%s: not interested (not a signal).\n", __FUNCTION__);
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  LOG("%s: Received signal %s:%s from %s\n", __FUNCTION__,
      dbus_message_get_interface(aMsg), dbus_message_get_member(aMsg),
      dbus_message_get_path(aMsg));

  // TODO: Parse DBusMessage* on the IOThread and return as a BluetoothEvent so
  // we aren't passing the pointer at all, as well as offloading parsing (not
  // that it's that heavy.)
  nsCOMPtr<DistributeDBusMessageTask> t(new DistributeDBusMessageTask(aMsg));
  if (NS_FAILED(NS_DispatchToMainThread(t))) {
    NS_WARNING("Failed to dispatch to main thread!");
  }

  return DBUS_HANDLER_RESULT_HANDLED;
}

nsresult
StartBluetoothConnection()
{
  if(sDBusConnection) {
    NS_WARNING("DBusConnection already established, skipping");
    return NS_OK;    
  }
  sBluetoothEventObserverTable = new BluetoothEventObserverTable();
  sBluetoothEventObserverTable->Init(100);

  sDBusConnection = new RawDBusConnection();
  sDBusConnection->EstablishDBusConnection();
	
  // Add a filter for all incoming messages_base
  if (!dbus_connection_add_filter(sDBusConnection->mConnection, EventFilter,
                                  NULL, NULL)) {
    NS_WARNING("Cannot create DBus Event Filter for DBus Thread!");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
StopBluetoothConnection()
{
  if(!sDBusConnection) {
    NS_WARNING("DBusConnection does not exist, nothing to stop, skipping.");
    return NS_OK;
  }
  dbus_connection_remove_filter(sDBusConnection->mConnection, EventFilter, NULL);
  sDBusConnection = NULL;
  sBluetoothEventObserverTable->Clear();
  sBluetoothEventObserverTable = NULL;
  return NS_OK;
}

nsresult
GetDefaultAdapterPathInternal(nsCString& aAdapterPath)
{
  DBusMessage *msg = NULL, *reply = NULL;
  DBusError err;
  const char *device_path = NULL;
  int attempt = 0;

  for (attempt = 0; attempt < 1000 && reply == NULL; attempt ++) {
    msg = dbus_message_new_method_call("org.bluez", "/",
                                       "org.bluez.Manager", "DefaultAdapter");
    if (!msg) {
      LOG("%s: Can't allocate new method call for get_adapter_path!",
             __FUNCTION__);
      return NS_ERROR_FAILURE;
    }
    dbus_message_append_args(msg, DBUS_TYPE_INVALID);
    dbus_error_init(&err);
    reply = dbus_connection_send_with_reply_and_block(
      sDBusConnection->mConnection, msg, -1, &err);

    if (!reply) {
      if (dbus_error_is_set(&err)) {
        if (dbus_error_has_name(&err,
                                "org.freedesktop.DBus.Error.ServiceUnknown")) {
          // bluetoothd is still down, retry
          LOG("Service unknown\n");
          dbus_error_free(&err);
          //usleep(10000); // 10 ms
          continue;
        } else if (dbus_error_has_name(&err,
                                       "org.bluez.Error.NoSuchAdapter")) {
          LOG("No adapter found\n");
          dbus_error_free(&err);
          goto failed;
        } else {
          // Some other error we weren't expecting
          LOG("other error\n");
          dbus_error_free(&err);
        }
      }
    }
  }
  if (attempt == 1000) {
    LOG("timeout\n");
    //printfE("Time out while trying to get Adapter path, is bluetoothd up ?");
    goto failed;
  }

  if (!dbus_message_get_args(reply, &err, DBUS_TYPE_OBJECT_PATH,
                             &device_path, DBUS_TYPE_INVALID)
      || !device_path) {
    if (dbus_error_is_set(&err)) {
      dbus_error_free(&err);
    }
    goto failed;
  }
  dbus_message_unref(msg);
  aAdapterPath = nsDependentCString(device_path);
  return NS_OK;
failed:
  dbus_message_unref(msg);
  return NS_ERROR_FAILURE;
}

}
}
}
