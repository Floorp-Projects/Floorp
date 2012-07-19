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

#include "base/basictypes.h"
#include "BluetoothDBusService.h"
#include "BluetoothTypes.h"
#include "BluetoothReplyRunnable.h"

#include <cstdio>
#include <dbus/dbus.h>

#include "nsIDOMDOMRequest.h"
#include "nsAutoPtr.h"
#include "nsThreadUtils.h"
#include "nsDebug.h"
#include "nsClassHashtable.h"
#include "mozilla/ipc/DBusThread.h"
#include "mozilla/ipc/DBusUtils.h"
#include "mozilla/ipc/RawDBusConnection.h"
#include "mozilla/Util.h"

/**
 * Some rules for dealing with memory in DBus:
 * - A DBusError only needs to be deleted if it's been set, not just
 *   initialized. This is why LOG_AND_FREE... is called only when an error is
 *   set, and the macro cleans up the error itself.
 * - A DBusMessage needs to be unrefed when it is newed explicitly. DBusMessages
 *   from signals do not need to be unrefed, as they will be cleaned by DBus
 *   after DBUS_HANDLER_RESULT_HANDLED is returned from the filter.
 */

using namespace mozilla;
using namespace mozilla::ipc;
USING_BLUETOOTH_NAMESPACE

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

typedef struct {
  const char* name;
  int type;
} Properties;

static Properties remote_device_properties[] = {
  {"Address", DBUS_TYPE_STRING},
  {"Name", DBUS_TYPE_STRING},
  {"Icon", DBUS_TYPE_STRING},
  {"Class", DBUS_TYPE_UINT32},
  {"UUIDs", DBUS_TYPE_ARRAY},
  {"Services", DBUS_TYPE_ARRAY},
  {"Paired", DBUS_TYPE_BOOLEAN},
  {"Connected", DBUS_TYPE_BOOLEAN},
  {"Trusted", DBUS_TYPE_BOOLEAN},
  {"Blocked", DBUS_TYPE_BOOLEAN},
  {"Alias", DBUS_TYPE_STRING},
  {"Nodes", DBUS_TYPE_ARRAY},
  {"Adapter", DBUS_TYPE_OBJECT_PATH},
  {"LegacyPairing", DBUS_TYPE_BOOLEAN},
  {"RSSI", DBUS_TYPE_INT16},
  {"TX", DBUS_TYPE_UINT32},
  {"Broadcaster", DBUS_TYPE_BOOLEAN}
};

static Properties adapter_properties[] = {
  {"Address", DBUS_TYPE_STRING},
  {"Name", DBUS_TYPE_STRING},
  {"Class", DBUS_TYPE_UINT32},
  {"Powered", DBUS_TYPE_BOOLEAN},
  {"Discoverable", DBUS_TYPE_BOOLEAN},
  {"DiscoverableTimeout", DBUS_TYPE_UINT32},
  {"Pairable", DBUS_TYPE_BOOLEAN},
  {"PairableTimeout", DBUS_TYPE_UINT32},
  {"Discovering", DBUS_TYPE_BOOLEAN},
  {"Devices", DBUS_TYPE_ARRAY},
  {"UUIDs", DBUS_TYPE_ARRAY},
};

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

class DistributeBluetoothSignalTask : public nsRunnable {
  BluetoothSignal mSignal;
public:
  DistributeBluetoothSignalTask(const BluetoothSignal& aSignal) :
    mSignal(aSignal)
  {
  }

  NS_IMETHOD
  Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    BluetoothService* bs = BluetoothService::Get();
    MOZ_ASSERT(bs);
    return bs->DistributeSignal(mSignal);
  }  
};


bool
IsDBusMessageError(DBusMessage* aMsg, nsAString& aError)
{
  DBusError err;
  dbus_error_init(&err);
  if (dbus_message_get_type(aMsg) == DBUS_MESSAGE_TYPE_ERROR) {
    const char* error_msg;
    if (!dbus_message_get_args(aMsg, &err, DBUS_TYPE_STRING,
                               &error_msg, DBUS_TYPE_INVALID) ||
        !error_msg) {
      if (dbus_error_is_set(&err)) {
        aError = NS_ConvertUTF8toUTF16(err.message);
        LOG_AND_FREE_DBUS_ERROR(&err);
        return true;
      }
    } else {
      aError = NS_ConvertUTF8toUTF16(error_msg);
      return true;
    }
  }
  return false;
}

void
DispatchBluetoothReply(BluetoothReplyRunnable* aRunnable,
                       const BluetoothValue& aValue, const nsAString& aError)
{
  // Reply will be deleted by the runnable after running on main thread
  BluetoothReply* reply;
  if (!aError.IsEmpty()) {
    nsString err(aError);
    reply = new BluetoothReply(BluetoothReplyError(err));
  }
  else {
    reply = new BluetoothReply(BluetoothReplySuccess(aValue));
  }
  
  aRunnable->SetReply(reply);
  if (NS_FAILED(NS_DispatchToMainThread(aRunnable))) {
    NS_WARNING("Failed to dispatch to main thread!");
  }
}
  
void
GetObjectPathCallback(DBusMessage* aMsg, void* aBluetoothReplyRunnable)
{
  MOZ_ASSERT(!NS_IsMainThread());
  DBusError err;
  dbus_error_init(&err);
  nsRefPtr<BluetoothReplyRunnable> replyRunnable =
    dont_AddRef(static_cast< BluetoothReplyRunnable* >(aBluetoothReplyRunnable));

  NS_ASSERTION(replyRunnable, "Callback reply runnable is null!");

  nsString replyError;
  nsString replyPath;

  nsTArray<BluetoothNamedValue> replyValues;
  BluetoothValue v;
  if (!IsDBusMessageError(aMsg, replyError)) {
    NS_ASSERTION(dbus_message_get_type(aMsg) == DBUS_MESSAGE_TYPE_METHOD_RETURN,
                 "Got dbus callback that's not a METHOD_RETURN!");
    const char* object_path;
    if (!dbus_message_get_args(aMsg, &err, DBUS_TYPE_OBJECT_PATH,
                               &object_path, DBUS_TYPE_INVALID) ||
        !object_path) {
      if (dbus_error_is_set(&err)) {
        replyError = NS_ConvertUTF8toUTF16(err.message);
        LOG_AND_FREE_DBUS_ERROR(&err);
      }
    } else {
      v = NS_ConvertUTF8toUTF16(object_path);
    }
  }
  DispatchBluetoothReply(replyRunnable, v, replyError);
}

void
GetVoidCallback(DBusMessage* aMsg, void* aBluetoothReplyRunnable)
{
  MOZ_ASSERT(!NS_IsMainThread());
  DBusError err;
  dbus_error_init(&err);
  nsRefPtr<BluetoothReplyRunnable> replyRunnable =
    dont_AddRef(static_cast< BluetoothReplyRunnable* >(aBluetoothReplyRunnable));

  NS_ASSERTION(replyRunnable, "Callback reply runnable is null!");

  nsString replyError;
  BluetoothValue v;
  if (!IsDBusMessageError(aMsg, replyError) &&
      dbus_message_get_type(aMsg) == DBUS_MESSAGE_TYPE_METHOD_RETURN &&
      !dbus_message_get_args(aMsg, &err, DBUS_TYPE_INVALID)) {
    if (dbus_error_is_set(&err)) {
      replyError = NS_ConvertUTF8toUTF16(err.message);
      LOG_AND_FREE_DBUS_ERROR(&err);
    }
  }
  DispatchBluetoothReply(replyRunnable, v, replyError);  
}

bool
GetProperty(DBusMessageIter aIter, Properties* aPropertyTypes,
            int aPropertiesTypeLen, int* aPropIndex,
            InfallibleTArray<BluetoothNamedValue>& aProperties)
{
  DBusMessageIter prop_val, array_val_iter;
  char* property = NULL;
  uint32_t array_type;
  int i, type;

  if (dbus_message_iter_get_arg_type(&aIter) != DBUS_TYPE_STRING) {    
    return false;
  }

  dbus_message_iter_get_basic(&aIter, &property);

  if (!dbus_message_iter_next(&aIter) ||
      dbus_message_iter_get_arg_type(&aIter) != DBUS_TYPE_VARIANT) {
    return false;
  }

  for (i = 0; i <  aPropertiesTypeLen; i++) {
    if (!strncmp(property, aPropertyTypes[i].name, strlen(property))) {      
      break;
    }
  }

  if (i == aPropertiesTypeLen) {
    return false;
  }

  nsString propertyName;
  propertyName.AssignASCII(aPropertyTypes[i].name);
  *aPropIndex = i;

  dbus_message_iter_recurse(&aIter, &prop_val);
  type = aPropertyTypes[*aPropIndex].type;

  NS_ASSERTION(dbus_message_iter_get_arg_type(&prop_val) == type,
               "Iterator not type we expect!");
  
  BluetoothValue propertyValue;
  switch (type) {
    case DBUS_TYPE_STRING:
    case DBUS_TYPE_OBJECT_PATH:
      const char* c;
      dbus_message_iter_get_basic(&prop_val, &c);
      propertyValue = NS_ConvertUTF8toUTF16(c);
      break;
    case DBUS_TYPE_UINT32:
    case DBUS_TYPE_INT16:
      uint32_t i;
      dbus_message_iter_get_basic(&prop_val, &i);
      propertyValue = i;
      break;
    case DBUS_TYPE_BOOLEAN:
      bool b;
      dbus_message_iter_get_basic(&prop_val, &b);
      propertyValue = b;
      break;
    case DBUS_TYPE_ARRAY:
      dbus_message_iter_recurse(&prop_val, &array_val_iter);
      array_type = dbus_message_iter_get_arg_type(&array_val_iter);
      if (array_type == DBUS_TYPE_OBJECT_PATH ||
          array_type == DBUS_TYPE_STRING){
        InfallibleTArray<nsString> arr;
        do {
          const char* tmp;
          dbus_message_iter_get_basic(&array_val_iter, &tmp);
          nsString s;
          s = NS_ConvertUTF8toUTF16(tmp);
          arr.AppendElement(s);
        } while (dbus_message_iter_next(&array_val_iter));
        propertyValue = arr;
      } else {
        NS_WARNING("Received array type that's not a string array!");
      }
      break;
    default:
      NS_NOTREACHED("Cannot find dbus message type!");
  }
  aProperties.AppendElement(BluetoothNamedValue(propertyName, propertyValue));
  return true;
}

void 
ParseProperties(DBusMessageIter* aIter, 
                Properties* aPropertyTypes,
                const int aPropertiesTypeLen,
                InfallibleTArray<BluetoothNamedValue>& aProperties)
{
  DBusMessageIter dict_entry, dict;
  int prop_index = -1;

  NS_ASSERTION(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_ARRAY,
               "Trying to parse a property from something that's not an array!");

  dbus_message_iter_recurse(aIter, &dict);

  do {
    NS_ASSERTION(dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY,
                 "Trying to parse a property from something that's not an dict!");
    dbus_message_iter_recurse(&dict, &dict_entry);

    if (!GetProperty(dict_entry, aPropertyTypes, aPropertiesTypeLen, &prop_index,
                     aProperties)) {
      NS_WARNING("Can't create property!");
      return;
    }
  } while (dbus_message_iter_next(&dict));
}

void
ParsePropertyChange(DBusMessage* aMsg, Properties* aPropertyTypes,
                    const int aPropertiesTypeLen,
                    InfallibleTArray<BluetoothNamedValue>& aProperties)
{
  DBusMessageIter iter;
  DBusError err;
  int prop_index = -1;
  
  dbus_error_init(&err);
  if (!dbus_message_iter_init(aMsg, &iter)) {
    LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, aMsg);
    return;
  }
    
  if (!GetProperty(iter, aPropertyTypes, aPropertiesTypeLen,
                   &prop_index, aProperties)) {
    NS_WARNING("Can't get property!");
  }
}

// Called by dbus during WaitForAndDispatchEventNative()
// This function is called on the IOThread
static
DBusHandlerResult
EventFilter(DBusConnection* aConn, DBusMessage* aMsg, void* aData)
{
  NS_ASSERTION(!NS_IsMainThread(), "Shouldn't be called from Main Thread!");
  
  if (dbus_message_get_type(aMsg) != DBUS_MESSAGE_TYPE_SIGNAL) {
    LOG("%s: not interested (not a signal).\n", __FUNCTION__);
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }  

  if (dbus_message_get_path(aMsg) == NULL) {
    LOG("DBusMessage %s has no bluetooth destination, ignoring\n",
        dbus_message_get_member(aMsg));
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  DBusError err;
  nsString signalPath;
  nsString signalName;
  dbus_error_init(&err);
  signalPath = NS_ConvertUTF8toUTF16(dbus_message_get_path(aMsg));
  LOG("%s: Received signal %s:%s from %s\n", __FUNCTION__,
      dbus_message_get_interface(aMsg), dbus_message_get_member(aMsg),
      dbus_message_get_path(aMsg));

  signalName = NS_ConvertUTF8toUTF16(dbus_message_get_member(aMsg));
  BluetoothValue v;
  
  if (dbus_message_is_signal(aMsg, DBUS_ADAPTER_IFACE, "DeviceFound")) {

    DBusMessageIter iter;

    NS_ASSERTION(dbus_message_iter_init(aMsg, &iter),
                 "Can't create message iterator!");

    InfallibleTArray<BluetoothNamedValue> value;
    const char* addr;
    dbus_message_iter_get_basic(&iter, &addr);
    value.AppendElement(BluetoothNamedValue(NS_LITERAL_STRING("Address"),
                                            NS_ConvertUTF8toUTF16(addr)));
    
    if (dbus_message_iter_next(&iter)) {
      ParseProperties(&iter,
                      remote_device_properties,
                      ArrayLength(remote_device_properties),
                      value);
      NS_ASSERTION(value.Length() != 0, "Properties returned empty!");
      v = value;
    } else {
      NS_WARNING("DBus iterator not as long as expected!");
    }
  } else if (dbus_message_is_signal(aMsg, DBUS_ADAPTER_IFACE, "DeviceDisappeared")) {
    const char* str;
    if (!dbus_message_get_args(aMsg, &err,
                               DBUS_TYPE_STRING, &str,
                               DBUS_TYPE_INVALID)) {
      LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, aMsg);
    }
    v = NS_ConvertUTF8toUTF16(str);
  } else if (dbus_message_is_signal(aMsg, DBUS_ADAPTER_IFACE, "DeviceCreated")) {
    const char* str;
    if (!dbus_message_get_args(aMsg, &err,
                               DBUS_TYPE_OBJECT_PATH, &str,
                               DBUS_TYPE_INVALID)) {
      LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, aMsg);
    }
    v = NS_ConvertUTF8toUTF16(str);
  } else if (dbus_message_is_signal(aMsg, DBUS_ADAPTER_IFACE, "PropertyChanged")) {
    InfallibleTArray<BluetoothNamedValue> value;
    ParsePropertyChange(aMsg,
                        (Properties*)&adapter_properties,
                        ArrayLength(adapter_properties),
                        value);
    NS_ASSERTION(value.Length() != 0, "Properties returned empty!");
    v = value;
  }

  BluetoothSignal signal(signalName, signalPath, v);
  
  nsRefPtr<DistributeBluetoothSignalTask>
    t = new DistributeBluetoothSignalTask(signal);
  if (NS_FAILED(NS_DispatchToMainThread(t))) {
    NS_WARNING("Failed to dispatch to main thread!");
  }

  return DBUS_HANDLER_RESULT_HANDLED;
}

nsresult
BluetoothDBusService::StartInternal()
{
  // This could block. It should never be run on the main thread.
  MOZ_ASSERT(!NS_IsMainThread());
  
  if (!StartDBus()) {
    NS_WARNING("Cannot start DBus thread!");
    return NS_ERROR_FAILURE;
  }
  
  if (mConnection) {
    return NS_OK;    
  }

  if (NS_FAILED(EstablishDBusConnection())) {
    NS_WARNING("Cannot start DBus connection!");
    StopDBus();
    return NS_ERROR_FAILURE;
  }
	
  // Add a filter for all incoming messages_base
  if (!dbus_connection_add_filter(mConnection, EventFilter,
                                  NULL, NULL)) {
    NS_WARNING("Cannot create DBus Event Filter for DBus Thread!");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
BluetoothDBusService::StopInternal()
{
  // This could block. It should never be run on the main thread.
  MOZ_ASSERT(!NS_IsMainThread());
  
  if (!mConnection) {
    StopDBus();
    return NS_OK;
  }
  dbus_connection_remove_filter(mConnection, EventFilter, NULL);
  mConnection = nsnull;
  mBluetoothSignalObserverTable.Clear();
  StopDBus();
  return NS_OK;
}

nsresult
BluetoothDBusService::GetDefaultAdapterPathInternal(BluetoothReplyRunnable* aRunnable)
{
  if (!mConnection) {
    NS_ERROR("Bluetooth service not started yet!");
    return NS_ERROR_FAILURE;
  }
  NS_ASSERTION(NS_IsMainThread(), "Must be called from main thread!");

  nsRefPtr<BluetoothReplyRunnable> runnable = aRunnable;

  if (!dbus_func_args_async(mConnection,
                            1000,
                            GetObjectPathCallback,
                            (void*)aRunnable,
                            "/",
                            "org.bluez.Manager",
                            "DefaultAdapter",
                            DBUS_TYPE_INVALID)) {
    NS_WARNING("Could not start async function!");
    return NS_ERROR_FAILURE;
  }
  runnable.forget();
  return NS_OK;
}

nsresult
BluetoothDBusService::SendDiscoveryMessage(const nsAString& aAdapterPath,
                                           const char* aMessageName,
                                           BluetoothReplyRunnable* aRunnable)
{
  if (!mConnection) {
    NS_WARNING("Bluetooth service not started yet!");
    return NS_ERROR_FAILURE;
  }
  NS_ASSERTION(NS_IsMainThread(), "Must be called from main thread!");

  nsRefPtr<BluetoothReplyRunnable> runnable = aRunnable;

  const char* s = NS_ConvertUTF16toUTF8(aAdapterPath).get();
  if (!dbus_func_args_async(mConnection,
                            1000,
                            GetVoidCallback,
                            (void*)aRunnable,
                            s,
                            DBUS_ADAPTER_IFACE,
                            aMessageName,
                            DBUS_TYPE_INVALID)) {
    NS_WARNING("Could not start async function!");
    return NS_ERROR_FAILURE;
  }
  runnable.forget();
  return NS_OK;
}

nsresult
BluetoothDBusService::StopDiscoveryInternal(const nsAString& aAdapterPath,
                                            BluetoothReplyRunnable* aRunnable)
{
  return SendDiscoveryMessage(aAdapterPath, "StopDiscovery", aRunnable);
}
 
nsresult
BluetoothDBusService::StartDiscoveryInternal(const nsAString& aAdapterPath,
                                             BluetoothReplyRunnable* aRunnable)
{
  return SendDiscoveryMessage(aAdapterPath, "StartDiscovery", aRunnable);
}
