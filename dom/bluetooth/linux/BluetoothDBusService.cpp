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
#include "nsDataHashtable.h"
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

#define B2G_AGENT_CAPABILITIES "DisplayYesNo"
#define DBUS_MANAGER_IFACE BLUEZ_DBUS_BASE_IFC ".Manager"
#define DBUS_ADAPTER_IFACE BLUEZ_DBUS_BASE_IFC ".Adapter"
#define DBUS_DEVICE_IFACE BLUEZ_DBUS_BASE_IFC ".Device"
#define DBUS_AGENT_IFACE BLUEZ_DBUS_BASE_IFC ".Agent"
#define BLUEZ_DBUS_BASE_PATH      "/org/bluez"
#define BLUEZ_DBUS_BASE_IFC       "org.bluez"
#define BLUEZ_ERROR_IFC           "org.bluez.Error"

typedef struct {
  const char* name;
  int type;
} Properties;

static Properties sDeviceProperties[] = {
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

static Properties sAdapterProperties[] = {
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

static Properties sManagerProperties[] = {
  {"Adapters", DBUS_TYPE_ARRAY},
};

static const char* sBluetoothDBusIfaces[] =
{
  DBUS_MANAGER_IFACE,
  DBUS_ADAPTER_IFACE,
  DBUS_DEVICE_IFACE
};

static const char* sBluetoothDBusSignals[] =
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

/**
 * DBus Connection held for the BluetoothCommandThread to use. Should never be
 * used by any other thread.
 * 
 */
static nsAutoPtr<RawDBusConnection> gThreadConnection;
static nsDataHashtable<nsStringHashKey, DBusMessage* > sPairingReqTable;
static nsDataHashtable<nsStringHashKey, DBusMessage* > sAuthorizeReqTable;

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
    if (!bs) {
      NS_WARNING("BluetoothService not available!");
      return NS_ERROR_FAILURE;
    }    
    return bs->DistributeSignal(mSignal);
  }  
};

static bool
IsDBusMessageError(DBusMessage* aMsg, DBusError* aErr, nsAString& aErrorStr)
{
  if(aErr && dbus_error_is_set(aErr)) {
    aErrorStr = NS_ConvertUTF8toUTF16(aErr->message);
    LOG_AND_FREE_DBUS_ERROR(aErr);
    return true;
  }
  
  DBusError err;
  dbus_error_init(&err);
  if (dbus_message_get_type(aMsg) == DBUS_MESSAGE_TYPE_ERROR) {
    const char* error_msg;
    if (!dbus_message_get_args(aMsg, &err, DBUS_TYPE_STRING,
                               &error_msg, DBUS_TYPE_INVALID) ||
        !error_msg) {
      if (dbus_error_is_set(&err)) {
        aErrorStr = NS_ConvertUTF8toUTF16(err.message);
        LOG_AND_FREE_DBUS_ERROR(&err);
        return true;
      } else {
        aErrorStr.AssignLiteral("Unknown Error");
        return true;
      }
    } else {
      aErrorStr = NS_ConvertUTF8toUTF16(error_msg);
      return true;
    }
  }
  return false;
}

static void
DispatchBluetoothReply(BluetoothReplyRunnable* aRunnable,
                       const BluetoothValue& aValue, const nsAString& aErrorStr)
{
  // Reply will be deleted by the runnable after running on main thread
  BluetoothReply* reply;
  if (!aErrorStr.IsEmpty()) {
    nsString err(aErrorStr);
    reply = new BluetoothReply(BluetoothReplyError(err));
  } else {
    reply = new BluetoothReply(BluetoothReplySuccess(aValue));
  }
  
  aRunnable->SetReply(reply);
  if (NS_FAILED(NS_DispatchToMainThread(aRunnable))) {
    NS_WARNING("Failed to dispatch to main thread!");
  }
}

static void
UnpackObjectPathMessage(DBusMessage* aMsg, DBusError* aErr,
                        BluetoothValue& aValue, nsAString& aErrorStr)
{
  DBusError err;
  dbus_error_init(&err);
  if (!IsDBusMessageError(aMsg, aErr, aErrorStr)) {
    NS_ASSERTION(dbus_message_get_type(aMsg) == DBUS_MESSAGE_TYPE_METHOD_RETURN,
                 "Got dbus callback that's not a METHOD_RETURN!");
    const char* object_path;
    if (!dbus_message_get_args(aMsg, &err, DBUS_TYPE_OBJECT_PATH,
                               &object_path, DBUS_TYPE_INVALID) ||
        !object_path) {
      if (dbus_error_is_set(&err)) {
        aErrorStr = NS_ConvertUTF8toUTF16(err.message);
        LOG_AND_FREE_DBUS_ERROR(&err);
      }
    } else {
      aValue = NS_ConvertUTF8toUTF16(object_path);
    }
  }
}

typedef void (*UnpackFunc)(DBusMessage*, DBusError*, BluetoothValue&, nsAString&);

static nsString
GetObjectPathFromAddress(const nsAString& aAdapterPath,
                         const nsAString& aDeviceAddress)
{
  // The object path would be like /org/bluez/2906/hci0/dev_00_23_7F_CB_B4_F1,
  // and the adapter path would be the first part of the object path, accoring
  // to the example above, it's /org/bluez/2906/hci0.
  nsString devicePath(aAdapterPath);
  devicePath.AppendLiteral("/dev_");
  devicePath.Append(aDeviceAddress);
  devicePath.ReplaceChar(':', '_');
  return devicePath;
}

static nsString
GetAddressFromObjectPath(const nsAString& aObjectPath)
{
  // The object path would be like /org/bluez/2906/hci0/dev_00_23_7F_CB_B4_F1,
  // and the adapter path would be the first part of the object path, accoring
  // to the example above, it's /org/bluez/2906/hci0.
  nsString address(aObjectPath);
  int addressHead = address.RFind("/") + 5;

  MOZ_ASSERT(addressHead + BLUETOOTH_ADDRESS_LENGTH == address.Length());

  address.Cut(0, addressHead);
  address.ReplaceChar('_', ':');

  return address;
}

static void
KeepDBusPairingMessage(const nsString& aDeviceAddress, DBusMessage* aMsg)
{
  sPairingReqTable.Put(aDeviceAddress, aMsg);

  // Increase ref count here because we need this message later. 
  // It'll be unrefed when set*Internal() is called.
  dbus_message_ref(aMsg);
}

static DBusHandlerResult
AgentEventFilter(DBusConnection *conn, DBusMessage *msg, void *data)
{
  if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_METHOD_CALL) {
    LOG("%s: agent handler not interested (not a method call).\n", __FUNCTION__);
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  DBusError err;
  dbus_error_init(&err);

  nsString signalPath = NS_ConvertUTF8toUTF16(dbus_message_get_path(msg));
  nsString signalName = NS_ConvertUTF8toUTF16(dbus_message_get_member(msg));
  nsString errorStr;
  BluetoothValue v;
  InfallibleTArray<BluetoothNamedValue> parameters;

  // The following descriptions of each signal are retrieved from:
  //
  // http://maemo.org/api_refs/5.0/beta/bluez/agent.html
  //
  if (dbus_message_is_method_call(msg, DBUS_AGENT_IFACE, "Cancel")) {
    // This method gets called to indicate that the agent request failed before a reply
    // was returned.

    // Return directly
    DBusMessage *reply = dbus_message_new_method_return(msg);

    if (!reply) {
      errorStr.AssignLiteral("Memory can't be allocated for the message.");
    } else {
      dbus_connection_send(conn, reply, NULL);
      dbus_message_unref(reply);
    }
  } else if (dbus_message_is_method_call(msg, DBUS_AGENT_IFACE, "Authorize")) {
    // This method gets called when the service daemon needs to authorize a
    // connection/service request.
    char *objectPath;
    const char *uuid;
    if (!dbus_message_get_args(msg, NULL,
                               DBUS_TYPE_OBJECT_PATH, &objectPath,
                               DBUS_TYPE_STRING, &uuid,
                               DBUS_TYPE_INVALID)) {
      LOG("%s: Invalid arguments for Authorize() method", __FUNCTION__);
      errorStr.AssignLiteral("Invalid arguments for Authorize() method");
    } else {
      nsString deviceAddress = GetAddressFromObjectPath(NS_ConvertUTF8toUTF16(objectPath));

      parameters.AppendElement(BluetoothNamedValue(NS_LITERAL_STRING("Device"), deviceAddress));
      parameters.AppendElement(BluetoothNamedValue(NS_LITERAL_STRING("UUID"),
                                                   NS_ConvertUTF8toUTF16(uuid)));

      // Because we may have authorization request and pairing request from the 
      // same remote device at the same time, we need two tables to keep these messages.
      sAuthorizeReqTable.Put(deviceAddress, msg);

      // Increase ref count here because we need this message later. 
      // It'll be unrefed when setAuthorizationInternal() is called.
      dbus_message_ref(msg);

      v = parameters;
    }  
  } else if (dbus_message_is_method_call(msg, DBUS_AGENT_IFACE, "RequestConfirmation")) {
    // This method gets called when the service daemon needs to confirm a passkey for
    // an authentication.
    char *objectPath;
    uint32_t passkey;
    if (!dbus_message_get_args(msg, NULL,
                               DBUS_TYPE_OBJECT_PATH, &objectPath,
                               DBUS_TYPE_UINT32, &passkey,
                               DBUS_TYPE_INVALID)) {
      LOG("%s: Invalid arguments for RequestConfirmation() method", __FUNCTION__);
      errorStr.AssignLiteral("Invalid arguments for RequestConfirmation() method");
    } else {
      nsString deviceAddress = GetAddressFromObjectPath(NS_ConvertUTF8toUTF16(objectPath));

      parameters.AppendElement(BluetoothNamedValue(NS_LITERAL_STRING("Device"), deviceAddress));
      parameters.AppendElement(BluetoothNamedValue(NS_LITERAL_STRING("Passkey"), passkey));
      
      KeepDBusPairingMessage(deviceAddress, msg);

      v = parameters;
    }
  } else if (dbus_message_is_method_call(msg, DBUS_AGENT_IFACE, "RequestPinCode")) {
    // This method gets called when the service daemon needs to get the passkey for an
    // authentication. The return value should be a string of 1-16 characters length.
    // The string can be alphanumeric.
    char *objectPath;
    if (!dbus_message_get_args(msg, NULL,
                               DBUS_TYPE_OBJECT_PATH, &objectPath,
                               DBUS_TYPE_INVALID)) {
      LOG("%s: Invalid arguments for RequestPinCode() method", __FUNCTION__);
      errorStr.AssignLiteral("Invalid arguments for RequestPinCode() method");
    } else {
      nsString deviceAddress = GetAddressFromObjectPath(NS_ConvertUTF8toUTF16(objectPath));

      parameters.AppendElement(BluetoothNamedValue(NS_LITERAL_STRING("Device"), deviceAddress));

      KeepDBusPairingMessage(deviceAddress, msg);

      v = parameters;
    }
  } else if (dbus_message_is_method_call(msg, DBUS_AGENT_IFACE, "RequestPasskey")) {
    // This method gets called when the service daemon needs to get the passkey for an
    // authentication. The return value should be a numeric value between 0-999999.
    char *objectPath;
    if (!dbus_message_get_args(msg, NULL,
                               DBUS_TYPE_OBJECT_PATH, &objectPath,
                               DBUS_TYPE_INVALID)) {
      LOG("%s: Invalid arguments for RequestPasskey() method", __FUNCTION__);
      errorStr.AssignLiteral("Invalid arguments for RequestPasskey() method");
    } else {
      nsString deviceAddress = GetAddressFromObjectPath(NS_ConvertUTF8toUTF16(objectPath));

      parameters.AppendElement(BluetoothNamedValue(NS_LITERAL_STRING("Device"), deviceAddress));

      KeepDBusPairingMessage(deviceAddress, msg);

      v = parameters;
    }
  } else if (dbus_message_is_method_call(msg, DBUS_AGENT_IFACE, "Release")) {
    // This method gets called when the service daemon unregisters the agent. An agent
    // can use it to do cleanup tasks. There is no need to unregister the agent, because
    // when this method gets called it has already been unregistered.
    DBusMessage *reply = dbus_message_new_method_return(msg);

    if (!reply) {
      errorStr.AssignLiteral("Memory can't be allocated for the message.");
    } else {
      dbus_connection_send(conn, reply, NULL);
      dbus_message_unref(reply);

      // Do not send an notification to upper layer, too annoying.
      return DBUS_HANDLER_RESULT_HANDLED;
    }
  } else {
    LOG("agent handler %s: Unhandled event. Ignore.", __FUNCTION__);
  }

  if (!errorStr.IsEmpty()) {
    NS_WARNING(NS_ConvertUTF16toUTF8(errorStr).get());
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  BluetoothSignal signal(signalName, signalPath, v);

  nsRefPtr<DistributeBluetoothSignalTask> t = new DistributeBluetoothSignalTask(signal);

  if (NS_FAILED(NS_DispatchToMainThread(t))) {
     NS_WARNING("Failed to dispatch to main thread!");
  }

  return DBUS_HANDLER_RESULT_HANDLED;
}

static const DBusObjectPathVTable agentVtable = {
  NULL, AgentEventFilter, NULL, NULL, NULL, NULL
};

// Local agent means agent for Adapter, not agent for Device. Some signals
// will be passed to local agent, some will be passed to device agent.
// For example, if a remote device would like to pair with us, then the
// signal will be passed to local agent. If we start pairing process with
// calling CreatePairedDevice, we'll get signal which should be passed to
// device agent.
static bool
RegisterLocalAgent(const char* adapterPath,
                   const char* agentPath,
                   const char* capabilities)
{
  MOZ_ASSERT(!NS_IsMainThread());

  if (!dbus_connection_register_object_path(gThreadConnection->GetConnection(),
                                            agentPath,
                                            &agentVtable,
                                            NULL)) {
    LOG("%s: Can't register object path %s for agent!",
        __FUNCTION__, agentPath);
    return false;
  }

  DBusMessage* msg =
    dbus_message_new_method_call("org.bluez", adapterPath,
                                 DBUS_ADAPTER_IFACE, "RegisterAgent");
  if (!msg) {
    LOG("%s: Can't allocate new method call for agent!", __FUNCTION__);
    return false;
  }

  if (!dbus_message_append_args(msg,
                                DBUS_TYPE_OBJECT_PATH, &agentPath,
                                DBUS_TYPE_STRING, &capabilities,
                                DBUS_TYPE_INVALID)) {
    LOG("%s: Couldn't append arguments to dbus message.", __FUNCTION__);
    return false;
  }

  DBusError err;
  dbus_error_init(&err);

  DBusMessage* reply =
    dbus_connection_send_with_reply_and_block(gThreadConnection->GetConnection(),
                                              msg, -1, &err);
  dbus_message_unref(msg);

  if (!reply) {
    if (dbus_error_is_set(&err)) {
      if(!strcmp(err.name, "org.bluez.Error.AlreadyExists")) {
        LOG_AND_FREE_DBUS_ERROR(&err);
#ifdef DEBUG
        LOG("Agent already registered, still returning true");
#endif
      } else {
        LOG_AND_FREE_DBUS_ERROR(&err);
        LOG("%s: Can't register agent!", __FUNCTION__);
        return false;
      }
    }
  } else {
    dbus_message_unref(reply);
  }
  
  dbus_connection_flush(gThreadConnection->GetConnection());
  return true;
}

static bool
RegisterAgent(const nsAString& aAdapterPath)
{
  MOZ_ASSERT(!NS_IsMainThread());

  if (!RegisterLocalAgent(NS_ConvertUTF16toUTF8(aAdapterPath).get(), 
                          LOCAL_AGENT_PATH, 
                          B2G_AGENT_CAPABILITIES)) {
    return false;
  }

  // There is no "RegisterAgent" function defined in device interface.
  // When we call "CreatePairedDevice", it will do device agent registration for us.
  // (See maemo.org/api_refs/5.0/beta/bluez/adapter.html)
  if (!dbus_connection_register_object_path(gThreadConnection->GetConnection(),
                                            REMOTE_AGENT_PATH,
                                            &agentVtable,
                                            NULL)) {
    LOG("%s: Can't register object path %s for remote device agent!",
        __FUNCTION__, REMOTE_AGENT_PATH);

    return false;
  }

  return true;
}

void
RunDBusCallback(DBusMessage* aMsg, void* aBluetoothReplyRunnable,
                UnpackFunc aFunc)
{
  MOZ_ASSERT(!NS_IsMainThread());
  nsRefPtr<BluetoothReplyRunnable> replyRunnable =
    dont_AddRef(static_cast< BluetoothReplyRunnable* >(aBluetoothReplyRunnable));

  NS_ASSERTION(replyRunnable, "Callback reply runnable is null!");

  nsString replyError;
  BluetoothValue v;
  aFunc(aMsg, nullptr, v, replyError);
  DispatchBluetoothReply(replyRunnable, v, replyError);  
}

void
GetObjectPathCallback(DBusMessage* aMsg, void* aBluetoothReplyRunnable)
{
  RunDBusCallback(aMsg, aBluetoothReplyRunnable,
                  UnpackObjectPathMessage);
}

void
UnpackVoidMessage(DBusMessage* aMsg, DBusError* aErr, BluetoothValue& aValue,
                  nsAString& aErrorStr)
{
  DBusError err;
  dbus_error_init(&err);
  if (!IsDBusMessageError(aMsg, aErr, aErrorStr) &&
      dbus_message_get_type(aMsg) == DBUS_MESSAGE_TYPE_METHOD_RETURN &&
      !dbus_message_get_args(aMsg, &err, DBUS_TYPE_INVALID)) {
    if (dbus_error_is_set(&err)) {
      aErrorStr = NS_ConvertUTF8toUTF16(err.message);
      LOG_AND_FREE_DBUS_ERROR(&err);
    }
  }
}

void
GetVoidCallback(DBusMessage* aMsg, void* aBluetoothReplyRunnable)
{
  RunDBusCallback(aMsg, aBluetoothReplyRunnable,
                  UnpackVoidMessage);
}

bool
GetProperty(DBusMessageIter aIter, Properties* aPropertyTypes,
            int aPropertyTypeLen, int* aPropIndex,
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

  for (i = 0; i < aPropertyTypeLen; i++) {
    if (!strncmp(property, aPropertyTypes[i].name, strlen(property))) {      
      break;
    }
  }

  if (i == aPropertyTypeLen) {
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
        // This happens when the array is 0-length. Apparently we get a
        // DBUS_TYPE_INVALID type.
        propertyValue = InfallibleTArray<nsString>();
#ifdef DEBUG
        NS_WARNING("Received array type that's not a string array!");
#endif
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
                BluetoothValue& aValue,
                nsAString& aErrorStr,
                Properties* aPropertyTypes,
                const int aPropertyTypeLen)
{
  DBusMessageIter dict_entry, dict;
  int prop_index = -1;

  NS_ASSERTION(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_ARRAY,
               "Trying to parse a property from something that's not an array!");

  dbus_message_iter_recurse(aIter, &dict);
  InfallibleTArray<BluetoothNamedValue> props;
  do {
    NS_ASSERTION(dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY,
                 "Trying to parse a property from something that's not an dict!");
    dbus_message_iter_recurse(&dict, &dict_entry);

    if (!GetProperty(dict_entry, aPropertyTypes, aPropertyTypeLen, &prop_index,
                     props)) {
      aErrorStr.AssignLiteral("Can't Create Property!");
      NS_WARNING("Can't create property!");
      return;
    }
  } while (dbus_message_iter_next(&dict));

  aValue = props;
}

void
UnpackPropertiesMessage(DBusMessage* aMsg, DBusError* aErr,
                        BluetoothValue& aValue, nsAString& aErrorStr,
                        Properties* aPropertyTypes,
                        const int aPropertyTypeLen)
{
  if (!IsDBusMessageError(aMsg, aErr, aErrorStr) &&
      dbus_message_get_type(aMsg) == DBUS_MESSAGE_TYPE_METHOD_RETURN) {
    DBusMessageIter iter;
    if (!dbus_message_iter_init(aMsg, &iter)) {
      aErrorStr.AssignLiteral("Cannot create dbus message iter!");
    } else {
      ParseProperties(&iter, aValue, aErrorStr, aPropertyTypes,
                      aPropertyTypeLen);
    }
  }
}

void
UnpackAdapterPropertiesMessage(DBusMessage* aMsg, DBusError* aErr,
                               BluetoothValue& aValue,
                               nsAString& aErrorStr)
{
  UnpackPropertiesMessage(aMsg, aErr, aValue, aErrorStr,
                          sAdapterProperties,
                          ArrayLength(sAdapterProperties));
}

void
UnpackDevicePropertiesMessage(DBusMessage* aMsg, DBusError* aErr,
                              BluetoothValue& aValue,
                              nsAString& aErrorStr)
{
  UnpackPropertiesMessage(aMsg, aErr, aValue, aErrorStr,
                          sDeviceProperties,
                          ArrayLength(sDeviceProperties));
}

void
UnpackManagerPropertiesMessage(DBusMessage* aMsg, DBusError* aErr,
                               BluetoothValue& aValue,
                               nsAString& aErrorStr)
{
  UnpackPropertiesMessage(aMsg, aErr, aValue, aErrorStr,
                          sManagerProperties,
                          ArrayLength(sManagerProperties));
}

void
GetManagerPropertiesCallback(DBusMessage* aMsg, void* aBluetoothReplyRunnable)
{
  RunDBusCallback(aMsg, aBluetoothReplyRunnable,
                  UnpackManagerPropertiesMessage);
}

void
GetAdapterPropertiesCallback(DBusMessage* aMsg, void* aBluetoothReplyRunnable)
{
  RunDBusCallback(aMsg, aBluetoothReplyRunnable,
                  UnpackAdapterPropertiesMessage);
}

void
GetDevicePropertiesCallback(DBusMessage* aMsg, void* aBluetoothReplyRunnable)
{
  RunDBusCallback(aMsg, aBluetoothReplyRunnable,
                  UnpackDevicePropertiesMessage);
}

static DBusCallback sBluetoothDBusPropCallbacks[] =
{
  GetManagerPropertiesCallback,
  GetAdapterPropertiesCallback,
  GetDevicePropertiesCallback
};

MOZ_STATIC_ASSERT(sizeof(sBluetoothDBusPropCallbacks) == sizeof(sBluetoothDBusIfaces),
  "DBus Property callback array and DBus interface array must be same size");

void
ParsePropertyChange(DBusMessage* aMsg, BluetoothValue& aValue,
                    nsAString& aErrorStr, Properties* aPropertyTypes,
                    const int aPropertyTypeLen)
{
  DBusMessageIter iter;
  DBusError err;
  int prop_index = -1;
  InfallibleTArray<BluetoothNamedValue> props;
  
  dbus_error_init(&err);
  if (!dbus_message_iter_init(aMsg, &iter)) {
    LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, aMsg);
    return;
  }
    
  if (!GetProperty(iter, aPropertyTypes, aPropertyTypeLen,
                   &prop_index, props)) {
    NS_WARNING("Can't get property!");
    aErrorStr.AssignLiteral("Can't get property!");
    return;
  }
  aValue = props;
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
  signalName = NS_ConvertUTF8toUTF16(dbus_message_get_member(aMsg));
  nsString errorStr;
  BluetoothValue v;
  
  if (dbus_message_is_signal(aMsg, DBUS_ADAPTER_IFACE, "DeviceFound")) {

    DBusMessageIter iter;

    if (!dbus_message_iter_init(aMsg, &iter)) {
      NS_WARNING("Can't create iterator!");
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    const char* addr;
    dbus_message_iter_get_basic(&iter, &addr);
    
    if (dbus_message_iter_next(&iter)) {
      ParseProperties(&iter,
                      v,
                      errorStr,
                      sDeviceProperties,
                      ArrayLength(sDeviceProperties));
      if (v.type() == BluetoothValue::TArrayOfBluetoothNamedValue)
      {
        // The DBus DeviceFound message actually passes back a key value object
        // with the address as the key and the rest of the device properties as
        // a dict value. After we parse out the properties, we need to go back
        // and add the address to the ipdl dict we've created to make sure we
        // have all of the information to correctly build the device.
        v.get_ArrayOfBluetoothNamedValue()
          .AppendElement(BluetoothNamedValue(NS_LITERAL_STRING("Address"),
                                             NS_ConvertUTF8toUTF16(addr)));
      }
    } else {
      errorStr.AssignLiteral("DBus device found message structure not as expected!");
    }
  } else if (dbus_message_is_signal(aMsg, DBUS_ADAPTER_IFACE, "DeviceDisappeared")) {
    const char* str;
    if (!dbus_message_get_args(aMsg, &err,
                               DBUS_TYPE_STRING, &str,
                               DBUS_TYPE_INVALID)) {
      LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, aMsg);
      errorStr.AssignLiteral("Cannot parse device address!");
    }
    v = NS_ConvertUTF8toUTF16(str);
  } else if (dbus_message_is_signal(aMsg, DBUS_ADAPTER_IFACE, "DeviceCreated")) {
    const char* str;
    if (!dbus_message_get_args(aMsg, &err,
                               DBUS_TYPE_OBJECT_PATH, &str,
                               DBUS_TYPE_INVALID)) {
      LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, aMsg);
      errorStr.AssignLiteral("Cannot parse device path!");
    }
    v = NS_ConvertUTF8toUTF16(str);
  } else if (dbus_message_is_signal(aMsg, DBUS_ADAPTER_IFACE, "PropertyChanged")) {
    ParsePropertyChange(aMsg,
                        v,
                        errorStr,
                        sAdapterProperties,
                        ArrayLength(sAdapterProperties));
  } else if (dbus_message_is_signal(aMsg, DBUS_DEVICE_IFACE, "PropertyChanged")) {
    ParsePropertyChange(aMsg,
                        v,
                        errorStr,
                        sDeviceProperties,
                        ArrayLength(sDeviceProperties));
  } else if (dbus_message_is_signal(aMsg, DBUS_MANAGER_IFACE, "PropertyChanged")) {
    ParsePropertyChange(aMsg,
                        v,
                        errorStr,
                        sManagerProperties,
                        ArrayLength(sManagerProperties));
  } else {
#ifdef DEBUG
    nsCAutoString signalStr;
    signalStr += dbus_message_get_member(aMsg);
    signalStr += " Signal not handled!";
    NS_WARNING(signalStr.get());
#endif
  }

  if (!errorStr.IsEmpty()) {
    NS_WARNING(NS_ConvertUTF16toUTF8(errorStr).get());
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
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
    NS_WARNING("Cannot start Main Thread DBus connection!");
    StopDBus();
    return NS_ERROR_FAILURE;
  }

  gThreadConnection = new RawDBusConnection();
  
  if (NS_FAILED(gThreadConnection->EstablishDBusConnection())) {
    NS_WARNING("Cannot start Sync Thread DBus connection!");
    StopDBus();
    return NS_ERROR_FAILURE;
  }

  DBusError err;
  dbus_error_init(&err);

  // Set which messages will be processed by this dbus connection.
  // Since we are maintaining a single thread for all the DBus bluez
  // signals we want, register all of them in this thread at startup.
  // The event handler will sort the destinations out as needed.
  for (uint32_t i = 0; i < ArrayLength(sBluetoothDBusSignals); ++i) {
    dbus_bus_add_match(mConnection,
                       sBluetoothDBusSignals[i],
                       &err);
    if (dbus_error_is_set(&err)) {
      LOG_AND_FREE_DBUS_ERROR(&err);
    }
  }

  // Add a filter for all incoming messages_base
  if (!dbus_connection_add_filter(mConnection, EventFilter,
                                  NULL, NULL)) {
    NS_WARNING("Cannot create DBus Event Filter for DBus Thread!");
    return NS_ERROR_FAILURE;
  }

  sPairingReqTable.Init();
  sAuthorizeReqTable.Init();

  return NS_OK;
}

PLDHashOperator
UnrefDBusMessages(const nsAString& key, DBusMessage* value, void* arg)
{
  dbus_message_unref(value);

  return PL_DHASH_NEXT;
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

  DBusError err;
  dbus_error_init(&err);
  for (uint32_t i = 0; i < ArrayLength(sBluetoothDBusSignals); ++i) {
    dbus_bus_remove_match(mConnection,
                          sBluetoothDBusSignals[i],
                          &err);
    if (dbus_error_is_set(&err)) {
      LOG_AND_FREE_DBUS_ERROR(&err);
    }
  }

  dbus_connection_remove_filter(mConnection, EventFilter, nullptr);
  
  mConnection = nullptr;
  gThreadConnection = nullptr;
  mBluetoothSignalObserverTable.Clear();

  // unref stored DBusMessages before clear the hashtable
  sPairingReqTable.EnumerateRead(UnrefDBusMessages, nullptr);
  sPairingReqTable.Clear();

  sAuthorizeReqTable.EnumerateRead(UnrefDBusMessages, nullptr);
  sAuthorizeReqTable.Clear();

  StopDBus();
  return NS_OK;
}

class DefaultAdapterPropertiesRunnable : public nsRunnable
{
public:
  DefaultAdapterPropertiesRunnable(BluetoothReplyRunnable* aRunnable)
    : mRunnable(dont_AddRef(aRunnable))
  {
  }

  nsresult
  Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());
    DBusError err;
    dbus_error_init(&err);
   
    BluetoothValue v;
    nsString replyError;

    DBusMessage* msg = dbus_func_args_timeout(gThreadConnection->GetConnection(),
                                              1000,
                                              &err,
                                              "/",
                                              DBUS_MANAGER_IFACE,
                                              "DefaultAdapter",
                                              DBUS_TYPE_INVALID);
    UnpackObjectPathMessage(msg, &err, v, replyError);
    if(msg) {
      dbus_message_unref(msg);
    }
    if(!replyError.IsEmpty()) {
      DispatchBluetoothReply(mRunnable, v, replyError);
      return NS_ERROR_FAILURE;
    }

    nsString path = v.get_nsString();
    nsCString tmp_path = NS_ConvertUTF16toUTF8(path);
    const char* object_path = tmp_path.get();
   
    v = InfallibleTArray<BluetoothNamedValue>();
    msg = dbus_func_args_timeout(gThreadConnection->GetConnection(),
                                 1000,
                                 &err,
                                 object_path,
                                 "org.bluez.Adapter",
                                 "GetProperties",
                                 DBUS_TYPE_INVALID);
    UnpackAdapterPropertiesMessage(msg, &err, v, replyError);
   
    if(!replyError.IsEmpty()) {
      DispatchBluetoothReply(mRunnable, v, replyError);
      return NS_ERROR_FAILURE;
    }
    if(msg) {
      dbus_message_unref(msg);
    }
    // We have to manually attach the path to the rest of the elements
    v.get_ArrayOfBluetoothNamedValue().AppendElement(BluetoothNamedValue(NS_LITERAL_STRING("Path"),
                                                                         path));

    RegisterAgent(path);

    DispatchBluetoothReply(mRunnable, v, replyError);
   
    return NS_OK;
  }

private:
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
};

nsresult
BluetoothDBusService::GetDefaultAdapterPathInternal(BluetoothReplyRunnable* aRunnable)
{
  if (!mConnection || !gThreadConnection) {
    NS_ERROR("Bluetooth service not started yet!");
    return NS_ERROR_FAILURE;
  }

  NS_ASSERTION(NS_IsMainThread(), "Must be called from main thread!");
  nsRefPtr<BluetoothReplyRunnable> runnable = aRunnable;

  nsRefPtr<nsRunnable> func(new DefaultAdapterPropertiesRunnable(runnable));
  if (NS_FAILED(mBluetoothCommandThread->Dispatch(func, NS_DISPATCH_NORMAL))) {
    NS_WARNING("Cannot dispatch firmware loading task!");
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

  NS_ConvertUTF16toUTF8 s(aAdapterPath);
  if (!dbus_func_args_async(mConnection,
                            1000,
                            GetVoidCallback,
                            (void*)aRunnable,
                            s.get(),
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

class BluetoothPairedDevicePropertiesRunnable : public nsRunnable
{
public:
  BluetoothPairedDevicePropertiesRunnable(BluetoothReplyRunnable* aRunnable,
                                          const nsTArray<nsString>& aDeviceAddresses)
    : mRunnable(dont_AddRef(aRunnable)),
      mDeviceAddresses(aDeviceAddresses)
  {
  }

  nsresult
  Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());
    DBusError err;
    dbus_error_init(&err);

    nsString replyError;
    DBusMessage* msg;
    BluetoothValue values = InfallibleTArray<BluetoothNamedValue>();

    for (int i = 0; i < mDeviceAddresses.Length(); i++) {
      BluetoothValue v = InfallibleTArray<BluetoothNamedValue>();
      msg = dbus_func_args_timeout(gThreadConnection->GetConnection(),
                                   1000,
                                   &err,
                                   NS_ConvertUTF16toUTF8(mDeviceAddresses[i]).get(),
                                   DBUS_DEVICE_IFACE,
                                   "GetProperties",
                                   DBUS_TYPE_INVALID);
      UnpackDevicePropertiesMessage(msg, &err, v, replyError);
      if (!replyError.IsEmpty()) {
        DispatchBluetoothReply(mRunnable, v, replyError);
        return NS_ERROR_FAILURE;
      }
      if (msg) {
        dbus_message_unref(msg);
      }
      v.get_ArrayOfBluetoothNamedValue().AppendElement(
        BluetoothNamedValue(NS_LITERAL_STRING("Path"), mDeviceAddresses[i])
      );

      InfallibleTArray<BluetoothNamedValue>& deviceProperties = v.get_ArrayOfBluetoothNamedValue();
      for (uint32_t p = 0; p < v.get_ArrayOfBluetoothNamedValue().Length(); ++p) {
        BluetoothNamedValue& property = v.get_ArrayOfBluetoothNamedValue()[p];
        // Only paired devices will be return back to main thread
        if (property.name().EqualsLiteral("Paired")) {
          bool paired = property.value();
          if (paired) {
            values.get_ArrayOfBluetoothNamedValue().AppendElement(
              BluetoothNamedValue(mDeviceAddresses[i], deviceProperties)
            );
          }
          break;
        }
      }
    }

    mRunnable->SetReply(new BluetoothReply(BluetoothReplySuccess(values)));
    if (NS_FAILED(NS_DispatchToMainThread(mRunnable))) {
      NS_WARNING("Failed to dispatch to main thread!");
    }
    return NS_OK;
  }

private:
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
  nsTArray<nsString> mDeviceAddresses;
};

nsresult
BluetoothDBusService::GetProperties(BluetoothObjectType aType,
                                    const nsAString& aPath,
                                    BluetoothReplyRunnable* aRunnable)
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called from main thread!");

  MOZ_ASSERT(aType < ArrayLength(sBluetoothDBusIfaces));
  MOZ_ASSERT(aType < ArrayLength(sBluetoothDBusPropCallbacks));
  
  const char* interface = sBluetoothDBusIfaces[aType];
  DBusCallback callback = sBluetoothDBusPropCallbacks[aType];
  
  nsRefPtr<BluetoothReplyRunnable> runnable = aRunnable;

  if (!dbus_func_args_async(mConnection,
                            1000,
                            callback,
                            (void*)aRunnable,
                            NS_ConvertUTF16toUTF8(aPath).get(),
                            interface,
                            "GetProperties",
                            DBUS_TYPE_INVALID)) {
    NS_WARNING("Could not start async function!");
    return NS_ERROR_FAILURE;
  }
  runnable.forget();
  return NS_OK;
}

nsresult
BluetoothDBusService::GetPairedDevicePropertiesInternal(const nsTArray<nsString>& aDeviceAddresses,
                                                        BluetoothReplyRunnable* aRunnable)
{
  if (!mConnection || !gThreadConnection) {
    NS_ERROR("Bluetooth service not started yet!");
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(!NS_IsMainThread());
  nsRefPtr<BluetoothReplyRunnable> runnable = aRunnable;

  nsRefPtr<nsRunnable> func(new BluetoothPairedDevicePropertiesRunnable(runnable, aDeviceAddresses));
  if (NS_FAILED(mBluetoothCommandThread->Dispatch(func, NS_DISPATCH_NORMAL))) {
    NS_WARNING("Cannot dispatch firmware loading task!");
    return NS_ERROR_FAILURE;
  }

  runnable.forget();
  return NS_OK;
}

nsresult
BluetoothDBusService::SetProperty(BluetoothObjectType aType,
                                  const nsAString& aPath,
                                  const BluetoothNamedValue& aValue,
                                  BluetoothReplyRunnable* aRunnable)
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called from main thread!");

  MOZ_ASSERT(aType < ArrayLength(sBluetoothDBusIfaces));
  const char* interface = sBluetoothDBusIfaces[aType];

  /* Compose the command */
  DBusMessage* msg = dbus_message_new_method_call("org.bluez",
                                                  NS_ConvertUTF16toUTF8(aPath).get(),
                                                  interface,
                                                  "SetProperty");

  if (!msg) {
    NS_WARNING("Could not allocate D-Bus message object!");
    return NS_ERROR_FAILURE;
  }

  const char* propName = NS_ConvertUTF16toUTF8(aValue.name()).get();
  if (!dbus_message_append_args(msg, DBUS_TYPE_STRING, &propName, DBUS_TYPE_INVALID)) {
    NS_WARNING("Couldn't append arguments to dbus message!");
    return NS_ERROR_FAILURE;
  }
  
  int type;
  int tmp_int;
  void* val;
  nsCString str;
  if (aValue.value().type() == BluetoothValue::Tuint32_t) {
    tmp_int = aValue.value().get_uint32_t();
    val = &tmp_int;
    type = DBUS_TYPE_UINT32;
  } else if (aValue.value().type() == BluetoothValue::TnsString) {
    str = NS_ConvertUTF16toUTF8(aValue.value().get_nsString());
    val = (void*)str.get();
    type = DBUS_TYPE_STRING;
  } else if (aValue.value().type() == BluetoothValue::Tbool) {
    tmp_int = aValue.value().get_bool() ? 1 : 0;
    val = &(tmp_int);
    type = DBUS_TYPE_BOOLEAN;
  } else {
    NS_WARNING("Property type not handled!");
    dbus_message_unref(msg);
    return NS_ERROR_FAILURE;
  }
  
  DBusMessageIter value_iter, iter;
  dbus_message_iter_init_append(msg, &iter);
  char var_type[2] = {(char)type, '\0'};
  if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, var_type, &value_iter) ||
      !dbus_message_iter_append_basic(&value_iter, type, val) ||
      !dbus_message_iter_close_container(&iter, &value_iter)) {
    NS_WARNING("Could not append argument to method call!");
    dbus_message_unref(msg);
    return NS_ERROR_FAILURE;
  }
  nsRefPtr<BluetoothReplyRunnable> runnable = aRunnable;

  // msg is unref'd as part of dbus_func_send_async 
  if (!dbus_func_send_async(mConnection,
                            msg,
                            1000,
                            GetVoidCallback,
                            (void*)aRunnable)) {
    NS_WARNING("Could not start async function!");
    return NS_ERROR_FAILURE;
  }
  runnable.forget();
  return NS_OK;
}

bool
BluetoothDBusService::GetDevicePath(const nsAString& aAdapterPath,
                                    const nsAString& aDeviceAddress,
                                    nsAString& aDevicePath)
{
  aDevicePath = GetObjectPathFromAddress(aAdapterPath, aDeviceAddress);
  return true;
}

int
BluetoothDBusService::GetDeviceServiceChannelInternal(const nsAString& aObjectPath,
                                                      const nsAString& aPattern,
                                                      int aAttributeId)
{
  // This is a blocking call, should not be run on main thread.
  MOZ_ASSERT(!NS_IsMainThread());

  nsCString tempPattern = NS_ConvertUTF16toUTF8(aPattern);
  const char* pattern = tempPattern.get();

  DBusMessage *reply =
    dbus_func_args(gThreadConnection->GetConnection(),
                   NS_ConvertUTF16toUTF8(aObjectPath).get(),
                   DBUS_DEVICE_IFACE, "GetServiceAttributeValue",
                   DBUS_TYPE_STRING, &pattern,
                   DBUS_TYPE_UINT16, &aAttributeId,
                   DBUS_TYPE_INVALID);

  return reply ? dbus_returns_int32(reply) : -1;
}

static void
ExtractHandles(DBusMessage *aReply, nsTArray<PRUint32>& aOutHandles)
{
  uint32_t* handles = NULL;
  int len;

  DBusError err;
  dbus_error_init(&err);

  if (dbus_message_get_args(aReply, &err,
                            DBUS_TYPE_ARRAY, DBUS_TYPE_UINT32, &handles, &len,
                            DBUS_TYPE_INVALID)) {
     if (!handles) {
       LOG("Null array in extract_handles");
     } else {
        for (int i = 0; i < len; ++i) {
        aOutHandles.AppendElement(handles[i]);
      }
    }
  } else {
    LOG_AND_FREE_DBUS_ERROR(&err);
  }
}

nsTArray<PRUint32>
BluetoothDBusService::AddReservedServicesInternal(const nsAString& aAdapterPath,
                                                  const nsTArray<PRUint32>& aServices)
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsTArray<PRUint32> ret;

  int length = aServices.Length();
  if (length == 0) return ret;

  const uint32_t* services = aServices.Elements();
  DBusMessage* reply =
    dbus_func_args(gThreadConnection->GetConnection(),
                   NS_ConvertUTF16toUTF8(aAdapterPath).get(),
                   DBUS_ADAPTER_IFACE, "AddReservedServiceRecords",
                   DBUS_TYPE_ARRAY, DBUS_TYPE_UINT32,
                   &services, length, DBUS_TYPE_INVALID);

  if (!reply) {
    LOG("Null DBus message. Couldn't extract handles.");
    return ret;
  }

  ExtractHandles(reply, ret);
  return ret;
}

bool
BluetoothDBusService::RemoveReservedServicesInternal(const nsAString& aAdapterPath,
                                                     const nsTArray<PRUint32>& aServiceHandles)
{
  MOZ_ASSERT(!NS_IsMainThread());

  int length = aServiceHandles.Length();
  if (length == 0) return false;

  const uint32_t* services = aServiceHandles.Elements();

  DBusMessage* reply =
    dbus_func_args(gThreadConnection->GetConnection(),
                   NS_ConvertUTF16toUTF8(aAdapterPath).get(),
                   DBUS_ADAPTER_IFACE, "RemoveReservedServiceRecords",
                   DBUS_TYPE_ARRAY, DBUS_TYPE_UINT32,
                   &services, length, DBUS_TYPE_INVALID);

  if (!reply) return false;

  dbus_message_unref(reply);
  return true;
}

nsresult
BluetoothDBusService::CreatePairedDeviceInternal(const nsAString& aAdapterPath,
                                                 const nsAString& aDeviceAddress,
                                                 int aTimeout,
                                                 BluetoothReplyRunnable* aRunnable)
{
  const char *capabilities = B2G_AGENT_CAPABILITIES;
  const char *deviceAgentPath = REMOTE_AGENT_PATH;

  nsCString tempDeviceAddress = NS_ConvertUTF16toUTF8(aDeviceAddress);
  const char *deviceAddress = tempDeviceAddress.get();

  nsRefPtr<BluetoothReplyRunnable> runnable = aRunnable;
  // Then send CreatePairedDevice, it will register a temp device agent then
  // unregister it after pairing process is over
  bool ret = dbus_func_args_async(mConnection,
                                  aTimeout,
                                  GetObjectPathCallback,
                                  (void*)runnable,
                                  NS_ConvertUTF16toUTF8(aAdapterPath).get(),
                                  DBUS_ADAPTER_IFACE,
                                  "CreatePairedDevice",
                                  DBUS_TYPE_STRING, &deviceAddress,
                                  DBUS_TYPE_OBJECT_PATH, &deviceAgentPath,
                                  DBUS_TYPE_STRING, &capabilities,
                                  DBUS_TYPE_INVALID);

  if (!ret) {
    NS_WARNING("Could not start async function!");
    return NS_ERROR_FAILURE;
  }

  runnable.forget();
  return NS_OK;
}

nsresult
BluetoothDBusService::RemoveDeviceInternal(const nsAString& aAdapterPath,
                                           const nsAString& aDeviceAddress,
                                           BluetoothReplyRunnable* aRunnable)
{
  nsCString tempDeviceObjectPath =
    NS_ConvertUTF16toUTF8(GetObjectPathFromAddress(aAdapterPath, aDeviceAddress));
  const char* deviceObjectPath = tempDeviceObjectPath.get();

  nsRefPtr<BluetoothReplyRunnable> runnable = aRunnable;

  // We don't really care about how long it would take on removing a device,
  // just to make sure that the value of timeout is reasonable. So, we use 
  // -1 for the timeout, which means a reasonable default timeout will be used.
  bool ret = dbus_func_args_async(mConnection,
                                  -1,
                                  GetVoidCallback,
                                  (void*)runnable,
                                  NS_ConvertUTF16toUTF8(aAdapterPath).get(),
                                  DBUS_ADAPTER_IFACE,
                                  "RemoveDevice",
                                  DBUS_TYPE_OBJECT_PATH, &deviceObjectPath,
                                  DBUS_TYPE_INVALID);
   if (!ret) {
    NS_WARNING("Could not start async function!");
    return NS_ERROR_FAILURE;
  }

  runnable.forget();
  return NS_OK;
}

bool
BluetoothDBusService::SetPinCodeInternal(const nsAString& aDeviceAddress, const nsAString& aPinCode)
{
  DBusMessage *msg;
  if (!sPairingReqTable.Get(aDeviceAddress, &msg)) {
    LOG("%s: Couldn't get original request message.", __FUNCTION__);
    return false;
  }

  DBusMessage *reply = dbus_message_new_method_return(msg);

  if (!reply) {
    LOG("%s: Memory can't be allocated for the message.", __FUNCTION__);
    dbus_message_unref(msg);
    return false;
  }

  bool result;

  nsCString tempPinCode = NS_ConvertUTF16toUTF8(aPinCode);
  const char* pinCode = tempPinCode.get();

  if (!dbus_message_append_args(reply,
                                DBUS_TYPE_STRING, &pinCode,
                                DBUS_TYPE_INVALID)) {
    LOG("%s: Couldn't append arguments to dbus message.", __FUNCTION__);
    result = false;
  } else {
    result = dbus_connection_send(mConnection, reply, NULL);
  }

  dbus_message_unref(msg);
  dbus_message_unref(reply);

  sPairingReqTable.Remove(aDeviceAddress);

  return result;
}

bool
BluetoothDBusService::SetPasskeyInternal(const nsAString& aDeviceAddress, PRUint32 aPasskey)
{
  DBusMessage *msg;
  if (!sPairingReqTable.Get(aDeviceAddress, &msg)) {
    LOG("%s: Couldn't get original request message.", __FUNCTION__);
    return false;
  }

  DBusMessage *reply = dbus_message_new_method_return(msg);

  if (!reply) {
    LOG("%s: Memory can't be allocated for the message.", __FUNCTION__);
    dbus_message_unref(msg);
    return false;
  }

  uint32_t passkey = aPasskey;
  bool result;

  if (!dbus_message_append_args(reply,
                                DBUS_TYPE_UINT32, &passkey,
                                DBUS_TYPE_INVALID)) {
    LOG("%s: Couldn't append arguments to dbus message.", __FUNCTION__);
    result = false;
  } else {
    result = dbus_connection_send(mConnection, reply, NULL);
  }

  dbus_message_unref(msg);
  dbus_message_unref(reply);

  sPairingReqTable.Remove(aDeviceAddress);

  return result;
}

bool
BluetoothDBusService::SetPairingConfirmationInternal(const nsAString& aDeviceAddress, bool aConfirm)
{
  DBusMessage *msg;
  if (!sPairingReqTable.Get(aDeviceAddress, &msg)) {
    LOG("%s: Couldn't get original request message.", __FUNCTION__);
    return false;
  }

  DBusMessage *reply;

  if (aConfirm) {
    reply = dbus_message_new_method_return(msg);   
  } else {
    reply = dbus_message_new_error(msg, "org.bluez.Error.Rejected", "User rejected confirmation");
  }

  if (!reply) {
    LOG("%s: Memory can't be allocated for the message.", __FUNCTION__);
    dbus_message_unref(msg);
    return false;
  }

  bool result = dbus_connection_send(mConnection, reply, NULL);
  dbus_message_unref(msg);
  dbus_message_unref(reply);

  sPairingReqTable.Remove(aDeviceAddress);

  return result;
}

bool
BluetoothDBusService::SetAuthorizationInternal(const nsAString& aDeviceAddress, bool aAllow)
{
  DBusMessage *msg;
  
  if (!sAuthorizeReqTable.Get(aDeviceAddress, &msg)) {
    LOG("%s: Couldn't get original request message.", __FUNCTION__);
    return false;
  }

  DBusMessage *reply;

  if (aAllow) {
    reply = dbus_message_new_method_return(msg);   
  } else {
    reply = dbus_message_new_error(msg, "org.bluez.Error.Rejected", "Authorization rejected");
  }

  if (!reply) {
    LOG("%s: Memory can't be allocated for the message.", __FUNCTION__);
    dbus_message_unref(msg);
    return false;
  }

  bool result = dbus_connection_send(mConnection, reply, NULL);
  dbus_message_unref(msg);
  dbus_message_unref(reply);

  sAuthorizeReqTable.Remove(aDeviceAddress);

  return result;
}
