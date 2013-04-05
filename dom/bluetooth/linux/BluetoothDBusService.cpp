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
#include "BluetoothHfpManager.h"
#include "BluetoothOppManager.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothScoManager.h"
#include "BluetoothUnixSocketConnector.h"
#include "BluetoothUtils.h"
#include "BluetoothUuid.h"

#include <cstdio>
#include <dbus/dbus.h>

#include "pratom.h"
#include "nsAutoPtr.h"
#include "nsThreadUtils.h"
#include "nsDebug.h"
#include "nsDataHashtable.h"
#include "mozilla/Hal.h"
#include "mozilla/ipc/UnixSocket.h"
#include "mozilla/ipc/DBusThread.h"
#include "mozilla/ipc/DBusUtils.h"
#include "mozilla/ipc/RawDBusConnection.h"
#include "mozilla/Util.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#if defined(MOZ_WIDGET_GONK)
#include "cutils/properties.h"
#endif

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

#define B2G_AGENT_CAPABILITIES "DisplayYesNo"
#define DBUS_MANAGER_IFACE BLUEZ_DBUS_BASE_IFC ".Manager"
#define DBUS_ADAPTER_IFACE BLUEZ_DBUS_BASE_IFC ".Adapter"
#define DBUS_DEVICE_IFACE BLUEZ_DBUS_BASE_IFC ".Device"
#define DBUS_AGENT_IFACE BLUEZ_DBUS_BASE_IFC ".Agent"
#define BLUEZ_DBUS_BASE_PATH      "/org/bluez"
#define BLUEZ_DBUS_BASE_IFC       "org.bluez"
#define BLUEZ_ERROR_IFC           "org.bluez.Error"

#define PROP_DEVICE_CONNECTED_TYPE "org.bluez.device.conn.type"

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
  {"Paired", DBUS_TYPE_BOOLEAN},
  {"Connected", DBUS_TYPE_ARRAY},
  {"Trusted", DBUS_TYPE_BOOLEAN},
  {"Blocked", DBUS_TYPE_BOOLEAN},
  {"Alias", DBUS_TYPE_STRING},
  {"Nodes", DBUS_TYPE_ARRAY},
  {"Adapter", DBUS_TYPE_OBJECT_PATH},
  {"LegacyPairing", DBUS_TYPE_BOOLEAN},
  {"RSSI", DBUS_TYPE_INT16},
  {"TX", DBUS_TYPE_UINT32},
  {"Type", DBUS_TYPE_STRING},
  {"Broadcaster", DBUS_TYPE_BOOLEAN},
  {"Services", DBUS_TYPE_ARRAY}
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
  {"Type", DBUS_TYPE_STRING}
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
static int32_t sIsPairing = 0;
static nsString sAdapterPath;

typedef void (*UnpackFunc)(DBusMessage*, DBusError*, BluetoothValue&, nsAString&);

class RemoveDeviceTask : public nsRunnable {
public:
  RemoveDeviceTask(const nsACString& aDeviceObjectPath,
                   BluetoothReplyRunnable* aRunnable)
    : mDeviceObjectPath(aDeviceObjectPath)
    , mRunnable(aRunnable)
  {
    MOZ_ASSERT(!aDeviceObjectPath.IsEmpty());
    MOZ_ASSERT(aRunnable);
  }

  nsresult Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    BluetoothValue v = true;
    nsAutoString errorStr;

    const char* tempDeviceObjectPath = mDeviceObjectPath.get();

    DBusMessage *reply =
      dbus_func_args(gThreadConnection->GetConnection(),
                     NS_ConvertUTF16toUTF8(sAdapterPath).get(),
                     DBUS_ADAPTER_IFACE, "RemoveDevice",
                     DBUS_TYPE_OBJECT_PATH, &tempDeviceObjectPath,
                     DBUS_TYPE_INVALID);

    if (reply) {
      dbus_message_unref(reply);
    } else {
      errorStr.AssignLiteral("RemoveDevice failed");
    }

    DispatchBluetoothReply(mRunnable, v, errorStr);

    return NS_OK;
  }

private:
  nsCString mDeviceObjectPath;
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
};

class SendDiscoveryTask : public nsRunnable {
public:
  SendDiscoveryTask(const char* aMessageName,
                    BluetoothReplyRunnable* aRunnable)
    : mMessageName(aMessageName)
    , mRunnable(aRunnable)
  {
    MOZ_ASSERT(aMessageName);
    MOZ_ASSERT(aRunnable);
  }

  nsresult Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    DBusMessage *reply =
      dbus_func_args(gThreadConnection->GetConnection(),
                     NS_ConvertUTF16toUTF8(sAdapterPath).get(),
                     DBUS_ADAPTER_IFACE, mMessageName,
                     DBUS_TYPE_INVALID);

    if (reply) {
      dbus_message_unref(reply);
    }

    BluetoothValue v = true;
    nsAutoString errorStr;
    DispatchBluetoothReply(mRunnable, v, errorStr);

    return NS_OK;
  }

private:
  const char* mMessageName;
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
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
    if (!bs) {
      NS_WARNING("BluetoothService not available!");
      return NS_ERROR_FAILURE;
    }
    bs->DistributeSignal(mSignal);
    return NS_OK;
  }
};

class PrepareAdapterTask : public nsRunnable {
public:
  PrepareAdapterTask(const nsAString& aPath) :
    mPath(aPath)
  {
  }

  NS_IMETHOD
  Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);
    sAdapterPath = mPath;

    // Due to the fact that we need to queue the dbus call to the command
    // thread inside the bluetoothservice, we have to route the call down
    // to the main thread and then back out to the command thread. There has
    // to be a better way to do this.
    if (NS_FAILED(bs->PrepareAdapterInternal())) {
      NS_WARNING("Prepare adapter failed");
      return NS_ERROR_FAILURE;
    }
    return NS_OK;
  }

private:
  nsString mPath;
};

class DevicePropertiesSignalHandler : public nsRunnable
{
public:
  DevicePropertiesSignalHandler(const BluetoothSignal& aSignal) :
    mSignal(aSignal)
  {
  }

  NS_IMETHODIMP
  Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    // Get device properties and then send to BluetoothAdapter
    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);

    // Due to the fact that we need to queue the dbus call to the command thread
    // inside the bluetoothservice, we have to route the call down to the main
    // thread and then back out to the command thread. There has to be a better
    // way to do this.
    if (NS_FAILED(bs->GetDevicePropertiesInternal(mSignal))) {
      NS_WARNING("Get device properties failed");
      return NS_ERROR_FAILURE;
    }
    return NS_OK;
  }

private:
  BluetoothSignal mSignal;
};

static bool
IsDBusMessageError(DBusMessage* aMsg, DBusError* aErr, nsAString& aErrorStr)
{
  if (aErr && dbus_error_is_set(aErr)) {
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
UnpackIntMessage(DBusMessage* aMsg, DBusError* aErr,
                 BluetoothValue& aValue, nsAString& aErrorStr)
{
  MOZ_ASSERT(aMsg);

  DBusError err;
  dbus_error_init(&err);
  if (!IsDBusMessageError(aMsg, aErr, aErrorStr)) {
    NS_ASSERTION(dbus_message_get_type(aMsg) == DBUS_MESSAGE_TYPE_METHOD_RETURN,
                 "Got dbus callback that's not a METHOD_RETURN!");
    int i;
    if (!dbus_message_get_args(aMsg, &err, DBUS_TYPE_INT32,
                               &i, DBUS_TYPE_INVALID)) {
      if (dbus_error_is_set(&err)) {
        aErrorStr = NS_ConvertUTF8toUTF16(err.message);
        LOG_AND_FREE_DBUS_ERROR(&err);
      }
    } else {
      aValue = (uint32_t)i;
    }
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
    BT_WARNING("%s: agent handler not interested (not a method call).\n",
               __FUNCTION__);
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  DBusError err;
  dbus_error_init(&err);

  BT_LOG("%s: %s, %s", __FUNCTION__,
                       dbus_message_get_path(msg),
                       dbus_message_get_member(msg));

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
    // This method gets called to indicate that the agent request failed before
    // a reply was returned.

    // Return directly
    DBusMessage *reply = dbus_message_new_method_return(msg);

    if (!reply) {
      errorStr.AssignLiteral("Memory can't be allocated for the message.");
    } else {
      dbus_connection_send(conn, reply, NULL);
      dbus_message_unref(reply);
      v = parameters;
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
      BT_WARNING("%s: Invalid arguments for Authorize() method", __FUNCTION__);
      errorStr.AssignLiteral("Invalid arguments for Authorize() method");
    } else {
      nsString deviceAddress =
        GetAddressFromObjectPath(NS_ConvertUTF8toUTF16(objectPath));

      parameters.AppendElement(BluetoothNamedValue(
                                 NS_LITERAL_STRING("deviceAddress"),
                                 deviceAddress));
      parameters.AppendElement(BluetoothNamedValue(
                                 NS_LITERAL_STRING("uuid"),
                                 NS_ConvertUTF8toUTF16(uuid)));

      // Because we may have authorization request and pairing request from the
      // same remote device at the same time, we need two tables to keep these
      // messages.
      sAuthorizeReqTable.Put(deviceAddress, msg);

      // Increase ref count here because we need this message later.
      // It'll be unrefed when setAuthorizationInternal() is called.
      dbus_message_ref(msg);

      v = parameters;
    }
  } else if (dbus_message_is_method_call(msg, DBUS_AGENT_IFACE,
                                         "RequestConfirmation")) {
    // This method gets called when the service daemon needs to confirm a
    // passkey for an authentication.
    char *objectPath;
    uint32_t passkey;
    if (!dbus_message_get_args(msg, NULL,
                               DBUS_TYPE_OBJECT_PATH, &objectPath,
                               DBUS_TYPE_UINT32, &passkey,
                               DBUS_TYPE_INVALID)) {
      BT_WARNING("%s: Invalid arguments: RequestConfirmation()", __FUNCTION__);
      errorStr.AssignLiteral("Invalid arguments: RequestConfirmation()");
    } else {
      parameters.AppendElement(BluetoothNamedValue(
                                 NS_LITERAL_STRING("path"),
                                 NS_ConvertUTF8toUTF16(objectPath)));
      parameters.AppendElement(BluetoothNamedValue(
                                 NS_LITERAL_STRING("passkey"),
                                 passkey));

      KeepDBusPairingMessage(GetAddressFromObjectPath(
                               NS_ConvertUTF8toUTF16(objectPath)), msg);

      BluetoothSignal signal(signalName, signalPath, parameters);

      // Fire a Device properties fetcher at the main thread
      nsRefPtr<DevicePropertiesSignalHandler> b =
        new DevicePropertiesSignalHandler(signal);
      if (NS_FAILED(NS_DispatchToMainThread(b))) {
        NS_WARNING("Failed to dispatch to main thread!");
      }
      // Since we're handling this in other threads, just fall out here
      return DBUS_HANDLER_RESULT_HANDLED;
    }
  } else if (dbus_message_is_method_call(msg, DBUS_AGENT_IFACE,
                                         "RequestPinCode")) {
    // This method gets called when the service daemon needs to get the passkey
    // for an authentication. The return value should be a string of 1-16
    // characters length. The string can be alphanumeric.
    char *objectPath;
    if (!dbus_message_get_args(msg, NULL,
                               DBUS_TYPE_OBJECT_PATH, &objectPath,
                               DBUS_TYPE_INVALID)) {
      BT_WARNING("%s: Invalid arguments for RequestPinCode() method",
                 __FUNCTION__);
      errorStr.AssignLiteral("Invalid arguments for RequestPinCode() method");
    } else {
      parameters.AppendElement(BluetoothNamedValue(
                                 NS_LITERAL_STRING("path"),
                                 NS_ConvertUTF8toUTF16(objectPath)));

      KeepDBusPairingMessage(GetAddressFromObjectPath(
                               NS_ConvertUTF8toUTF16(objectPath)), msg);

      BluetoothSignal signal(signalName, signalPath, parameters);

      // Fire a Device properties fetcher at the main thread
      nsRefPtr<DevicePropertiesSignalHandler> b =
        new DevicePropertiesSignalHandler(signal);
      if (NS_FAILED(NS_DispatchToMainThread(b))) {
        NS_WARNING("Failed to dispatch to main thread!");
      }
      // Since we're handling this in other threads, just fall out here
      return DBUS_HANDLER_RESULT_HANDLED;
    }
  } else if (dbus_message_is_method_call(msg, DBUS_AGENT_IFACE,
                                         "RequestPasskey")) {
    // This method gets called when the service daemon needs to get the passkey
    // for an authentication. The return value should be a numeric value
    // between 0-999999.
    char *objectPath;
    if (!dbus_message_get_args(msg, NULL,
                               DBUS_TYPE_OBJECT_PATH, &objectPath,
                               DBUS_TYPE_INVALID)) {
      BT_WARNING("%s: Invalid arguments for RequestPasskey() method",
                 __FUNCTION__);
      errorStr.AssignLiteral("Invalid arguments for RequestPasskey() method");
    } else {
      parameters.AppendElement(BluetoothNamedValue(
                                 NS_LITERAL_STRING("path"),
                                 NS_ConvertUTF8toUTF16(objectPath)));

      KeepDBusPairingMessage(GetAddressFromObjectPath(
                               NS_ConvertUTF8toUTF16(objectPath)), msg);

      BluetoothSignal signal(signalName, signalPath, parameters);

      // Fire a Device properties fetcher at the main thread
      nsRefPtr<DevicePropertiesSignalHandler> b =
        new DevicePropertiesSignalHandler(signal);
      if (NS_FAILED(NS_DispatchToMainThread(b))) {
        NS_WARNING("Failed to dispatch to main thread!");
      }
      // Since we're handling this in other threads, just fall out here
      return DBUS_HANDLER_RESULT_HANDLED;
    }
  } else if (dbus_message_is_method_call(msg, DBUS_AGENT_IFACE, "Release")) {
    // This method gets called when the service daemon unregisters the agent.
    // An agent can use it to do cleanup tasks. There is no need to unregister
    // the agent, because when this method gets called it has already been
    // unregistered.
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
#ifdef DEBUG
    BT_WARNING("agent handler %s: Unhandled event. Ignore.", __FUNCTION__);
#endif
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  if (!errorStr.IsEmpty()) {
    NS_WARNING(NS_ConvertUTF16toUTF8(errorStr).get());
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  BluetoothSignal signal(signalName, signalPath, v);
  nsRefPtr<DistributeBluetoothSignalTask> t =
    new DistributeBluetoothSignalTask(signal);
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
    BT_WARNING("%s: Can't register object path %s for agent!",
                __FUNCTION__, agentPath);
    return false;
  }

  DBusMessage* msg =
    dbus_message_new_method_call("org.bluez", adapterPath,
                                 DBUS_ADAPTER_IFACE, "RegisterAgent");
  if (!msg) {
    BT_WARNING("%s: Can't allocate new method call for agent!", __FUNCTION__);
    return false;
  }

  if (!dbus_message_append_args(msg,
                                DBUS_TYPE_OBJECT_PATH, &agentPath,
                                DBUS_TYPE_STRING, &capabilities,
                                DBUS_TYPE_INVALID)) {
    BT_WARNING("%s: Couldn't append arguments to dbus message.", __FUNCTION__);
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
      if (!strcmp(err.name, "org.bluez.Error.AlreadyExists")) {
        LOG_AND_FREE_DBUS_ERROR(&err);
#ifdef DEBUG
        BT_WARNING("Agent already registered, still returning true");
#endif
      } else {
        LOG_AND_FREE_DBUS_ERROR(&err);
        BT_WARNING("%s: Can't register agent!", __FUNCTION__);
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
RegisterAgent()
{
  MOZ_ASSERT(!NS_IsMainThread());

  if (!RegisterLocalAgent(NS_ConvertUTF16toUTF8(sAdapterPath).get(),
                          KEY_LOCAL_AGENT,
                          B2G_AGENT_CAPABILITIES)) {
    return false;
  }

  // There is no "RegisterAgent" function defined in device interface.
  // When we call "CreatePairedDevice", it will do device agent registration
  // for us. (See maemo.org/api_refs/5.0/beta/bluez/adapter.html)
  if (!dbus_connection_register_object_path(gThreadConnection->GetConnection(),
                                            KEY_REMOTE_AGENT,
                                            &agentVtable,
                                            NULL)) {
    BT_WARNING("%s: Can't register object path %s for remote device agent!",
               __FUNCTION__, KEY_REMOTE_AGENT);

    return false;
  }

  return true;
}

static void
ExtractHandles(DBusMessage *aReply, nsTArray<uint32_t>& aOutHandles)
{
  uint32_t* handles = NULL;
  int len;

  DBusError err;
  dbus_error_init(&err);

  if (dbus_message_get_args(aReply, &err,
                            DBUS_TYPE_ARRAY, DBUS_TYPE_UINT32, &handles, &len,
                            DBUS_TYPE_INVALID)) {
     if (!handles) {
       BT_WARNING("Null array in extract_handles");
     } else {
        for (int i = 0; i < len; ++i) {
        aOutHandles.AppendElement(handles[i]);
      }
    }
  } else {
    LOG_AND_FREE_DBUS_ERROR(&err);
  }
}

// static
bool
BluetoothDBusService::AddServiceRecords(const char* serviceName,
                                        unsigned long long uuidMsb,
                                        unsigned long long uuidLsb,
                                        int channel)
{
  MOZ_ASSERT(!NS_IsMainThread());

  DBusMessage *reply;
  reply = dbus_func_args(gThreadConnection->GetConnection(),
                         NS_ConvertUTF16toUTF8(sAdapterPath).get(),
                         DBUS_ADAPTER_IFACE, "AddRfcommServiceRecord",
                         DBUS_TYPE_STRING, &serviceName,
                         DBUS_TYPE_UINT64, &uuidMsb,
                         DBUS_TYPE_UINT64, &uuidLsb,
                         DBUS_TYPE_UINT16, &channel,
                         DBUS_TYPE_INVALID);

  return reply ? dbus_returns_uint32(reply) : -1;
}

// static
bool
BluetoothDBusService::AddReservedServicesInternal(
                                   const nsTArray<uint32_t>& aServices,
                                   nsTArray<uint32_t>& aServiceHandlesContainer)
{
  MOZ_ASSERT(!NS_IsMainThread());

  int length = aServices.Length();
  if (length == 0) return false;

  const uint32_t* services = aServices.Elements();
  DBusMessage* reply =
    dbus_func_args(gThreadConnection->GetConnection(),
                   NS_ConvertUTF16toUTF8(sAdapterPath).get(),
                   DBUS_ADAPTER_IFACE, "AddReservedServiceRecords",
                   DBUS_TYPE_ARRAY, DBUS_TYPE_UINT32,
                   &services, length, DBUS_TYPE_INVALID);

  if (!reply) {
    BT_WARNING("Null DBus message. Couldn't extract handles.");
    return false;
  }

  ExtractHandles(reply, aServiceHandlesContainer);
  return true;
}

void
BluetoothDBusService::DisconnectAllAcls(const nsAString& aAdapterPath)
{
  MOZ_ASSERT(!NS_IsMainThread());

  DBusMessage* reply =
    dbus_func_args(gThreadConnection->GetConnection(),
                   NS_ConvertUTF16toUTF8(aAdapterPath).get(),
                   DBUS_ADAPTER_IFACE, "DisconnectAllConnections",
                   DBUS_TYPE_INVALID);

  if (reply) {
    dbus_message_unref(reply);
  }
}

class PrepareProfileManagersRunnable : public nsRunnable
{
public:
  NS_IMETHOD
  Run()
  {
    BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
    if (!hfp || !hfp->Listen()) {
      NS_WARNING("Failed to start listening for BluetoothHfpManager!");
      return NS_ERROR_FAILURE;
    }

    BluetoothScoManager* sco = BluetoothScoManager::Get();
    if (!sco || !sco->Listen()) {
      NS_WARNING("Failed to start listening for BluetoothScoManager!");
      return NS_ERROR_FAILURE;
    }

    BluetoothOppManager* opp = BluetoothOppManager::Get();
    if (!opp || !opp->Listen()) {
      NS_WARNING("Failed to start listening for BluetoothOppManager!");
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }
};

class ShutdownProfileManagersRunnable : public nsRunnable
{
public:
  NS_IMETHOD
  Run()
  {
    BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
    if (hfp) {
      hfp->CloseSocket();
    }

    BluetoothOppManager* opp = BluetoothOppManager::Get();
    if (opp) {
      opp->CloseSocket();
    }

    BluetoothScoManager* sco = BluetoothScoManager::Get();
    if (sco) {
      sco->CloseSocket();
    }

    return NS_OK;
  }
};

class PrepareAdapterRunnable : public nsRunnable
{
public:
  PrepareAdapterRunnable()
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD
  Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    nsTArray<uint32_t> uuids;

    uuids.AppendElement(BluetoothServiceClass::HANDSFREE_AG);
    uuids.AppendElement(BluetoothServiceClass::HEADSET_AG);
    uuids.AppendElement(BluetoothServiceClass::OBJECT_PUSH);

    // TODO/qdot: This needs to be held for the life of the bluetooth connection
    // so we could clean it up. For right now though, we can throw it away.
    nsTArray<uint32_t> handles;

    if (!BluetoothDBusService::AddReservedServicesInternal(uuids, handles)) {
      NS_WARNING("Failed to add reserved services");
#ifdef MOZ_WIDGET_GONK
      return NS_ERROR_FAILURE;
#endif
    }

    if(!RegisterAgent()) {
      NS_WARNING("Failed to register agent");
      return NS_ERROR_FAILURE;
    }

    NS_DispatchToMainThread(new PrepareProfileManagersRunnable());
    return NS_OK;
  }
};

void
RunDBusCallback(DBusMessage* aMsg, void* aBluetoothReplyRunnable,
                UnpackFunc aFunc)
{
#ifdef MOZ_WIDGET_GONK
  // Due to the fact that we're running two dbus loops on desktop implicitly by
  // being gtk based, sometimes we'll get signals/reply coming in on the main
  // thread. There's not a lot we can do about that for the time being and it
  // (technically) shouldn't hurt anything. However, on gonk, die.

  // Due to the fact introducing workaround in Bug 827888, the callback for a
  // message gets executed immediately. The proper fix is in bug 830290, but
  // it's a intrusive change, it is better to remove assertion here since it
  // would not hurt anything.
  // Tracking bug 830290 for intrusive solution.

  // MOZ_ASSERT(!NS_IsMainThread());
#endif
  nsRefPtr<BluetoothReplyRunnable> replyRunnable =
    dont_AddRef(static_cast< BluetoothReplyRunnable* >(aBluetoothReplyRunnable));

  NS_ASSERTION(replyRunnable, "Callback reply runnable is null!");

  nsAutoString replyError;
  BluetoothValue v;
  aFunc(aMsg, nullptr, v, replyError);
  DispatchBluetoothReply(replyRunnable, v, replyError);
}

void
GetObjectPathCallback(DBusMessage* aMsg, void* aBluetoothReplyRunnable)
{
  if (sIsPairing) {
    RunDBusCallback(aMsg, aBluetoothReplyRunnable,
                    UnpackObjectPathMessage);
    PR_AtomicDecrement(&sIsPairing);
  }
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
  // XXXbent Need to figure out something better than this here.
  if (aErrorStr.IsEmpty()) {
    aValue = true;
  }
}

void
GetVoidCallback(DBusMessage* aMsg, void* aBluetoothReplyRunnable)
{
  RunDBusCallback(aMsg, aBluetoothReplyRunnable,
                  UnpackVoidMessage);
}

void
GetIntCallback(DBusMessage* aMsg, void* aBluetoothReplyRunnable)
{
  RunDBusCallback(aMsg, aBluetoothReplyRunnable,
                  UnpackIntMessage);
}

bool
IsDeviceConnectedTypeBoolean()
{
#if defined(MOZ_WIDGET_GONK)
  char connProp[PROPERTY_VALUE_MAX];

  property_get(PROP_DEVICE_CONNECTED_TYPE, connProp, "boolean");
  if (strcmp(connProp, "boolean") == 0) {
    return true;
  }
  return false;
#else
  // Assume it's always a boolean on desktop. Fixing someday in Bug 806457.
  return true;
#endif
}

void
CopyProperties(Properties* inProp, Properties* outProp, int aPropertyTypeLen)
{
  int i;

  for (i = 0; i < aPropertyTypeLen; i++) {
    outProp[i].name = inProp[i].name;
    outProp[i].type = inProp[i].type;
  }
}

int
GetPropertyIndex(Properties* prop, const char* propertyName,
                 int aPropertyTypeLen)
{
  int i;

  for (i = 0; i < aPropertyTypeLen; i++) {
    if (!strncmp(propertyName, prop[i].name, strlen(propertyName))) {
      return i;
    }
  }
  return -1;
}

bool
HasAudioService(uint32_t aCodValue)
{
  return ((aCodValue & 0x200000) == 0x200000);
}

bool
ContainsIcon(const InfallibleTArray<BluetoothNamedValue>& aProperties)
{
  for (uint8_t i = 0; i < aProperties.Length(); i++) {
    if (aProperties[i].name().EqualsLiteral("Icon")) {
      return true;
    }
  }
  return false;
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

  nsAutoString propertyName;
  propertyName.AssignASCII(aPropertyTypes[i].name);
  *aPropIndex = i;

  dbus_message_iter_recurse(&aIter, &prop_val);
  type = aPropertyTypes[*aPropIndex].type;

  if (dbus_message_iter_get_arg_type(&prop_val) != type) {
    NS_WARNING("Iterator not type we expect!");
    nsAutoCString str;
    str += "Property Name: ;";
    str += NS_ConvertUTF16toUTF8(propertyName);
    str += " Property Type Expected: ;";
    str += type;
    str += " Property Type Received: ";
    str += dbus_message_iter_get_arg_type(&prop_val);
    NS_WARNING(str.get());
    return false;
  }

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
          array_type == DBUS_TYPE_STRING) {
        InfallibleTArray<nsString> arr;
        do {
          const char* tmp;
          dbus_message_iter_get_basic(&array_val_iter, &tmp);
          nsAutoString s;
          s = NS_ConvertUTF8toUTF16(tmp);
          arr.AppendElement(s);
        } while (dbus_message_iter_next(&array_val_iter));
        propertyValue = arr;
      } else if (array_type == DBUS_TYPE_BYTE) {
        InfallibleTArray<uint8_t> arr;
        do {
          uint8_t tmp;
          dbus_message_iter_get_basic(&array_val_iter, &tmp);
          arr.AppendElement(tmp);
        } while (dbus_message_iter_next(&array_val_iter));
        propertyValue = arr;
      } else {
        // This happens when the array is 0-length. Apparently we get a
        // DBUS_TYPE_INVALID type.
        propertyValue = InfallibleTArray<nsString>();
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
                BluetoothValue& aValue,
                nsAString& aErrorStr,
                Properties* aPropertyTypes,
                const int aPropertyTypeLen)
{
  DBusMessageIter dict_entry, dict;
  int prop_index = -1;

  NS_ASSERTION(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_ARRAY,
               "Trying to parse a property from sth. that's not an array");

  dbus_message_iter_recurse(aIter, &dict);
  InfallibleTArray<BluetoothNamedValue> props;
  do {
    NS_ASSERTION(dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY,
                 "Trying to parse a property from sth. that's not an dict!");
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

bool
ReplaceConnectedType(Properties* sourceProperties,
                     Properties** destProperties,
                     int aPropertyTypeLen)
{
  if (!IsDeviceConnectedTypeBoolean()) {
    return false;
  }
  *destProperties = (Properties*)malloc(sizeof(Properties) * aPropertyTypeLen);
  if (*destProperties) {
    CopyProperties(sourceProperties, *destProperties, aPropertyTypeLen);
    int index = GetPropertyIndex(*destProperties,
                                 "Connected",
                                 aPropertyTypeLen);
    if (index >= 0) {
      (*destProperties)[index].type = DBUS_TYPE_BOOLEAN;
      return true;
    } else {
      free(*destProperties);
    }
  }
  return false;
}

void
UnpackDevicePropertiesMessage(DBusMessage* aMsg, DBusError* aErr,
                              BluetoothValue& aValue,
                              nsAString& aErrorStr)
{
  Properties* props = sDeviceProperties;
  Properties* newProps;
  bool replaced = ReplaceConnectedType(sDeviceProperties, &newProps,
                                       ArrayLength(sDeviceProperties));
  if (replaced) {
     props = newProps;
  }
  UnpackPropertiesMessage(aMsg, aErr, aValue, aErrorStr,
                          props,
                          ArrayLength(sDeviceProperties));
  if (replaced) {
     free(newProps);
  }
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

MOZ_STATIC_ASSERT(
  sizeof(sBluetoothDBusPropCallbacks) == sizeof(sBluetoothDBusIfaces),
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
    NS_WARNING("Can't create iterator!");
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

bool
GetPropertiesInternal(const nsAString& aPath,
                      const char* aIface,
                      BluetoothValue& aValue)
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsAutoString replyError;
  DBusError err;

  dbus_error_init(&err);

  DBusMessage* msg = dbus_func_args_timeout(gThreadConnection->GetConnection(),
                                            1000,
                                            &err,
                                            NS_ConvertUTF16toUTF8(aPath).get(),
                                            aIface,
                                            "GetProperties",
                                            DBUS_TYPE_INVALID);
  if (!strcmp(aIface, DBUS_DEVICE_IFACE)) {
    UnpackDevicePropertiesMessage(msg, &err, aValue, replyError);
  } else if (!strcmp(aIface, DBUS_ADAPTER_IFACE)) {
    UnpackAdapterPropertiesMessage(msg, &err, aValue, replyError);
  } else if (!strcmp(aIface, DBUS_MANAGER_IFACE)) {
    UnpackManagerPropertiesMessage(msg, &err, aValue, replyError);
  } else {
    NS_WARNING("Unknown interface for GetProperties!");
    return false;
  }

  if (!replyError.IsEmpty()) {
    NS_WARNING("Failed to get device properties");
    return false;
  }
  if (msg) {
    dbus_message_unref(msg);
  }
  return true;
}

// Called by dbus during WaitForAndDispatchEventNative()
// This function is called on the IOThread
static
DBusHandlerResult
EventFilter(DBusConnection* aConn, DBusMessage* aMsg, void* aData)
{
  NS_ASSERTION(!NS_IsMainThread(), "Shouldn't be called from Main Thread!");

  if (dbus_message_get_type(aMsg) != DBUS_MESSAGE_TYPE_SIGNAL) {
    BT_WARNING("%s: event handler not interested in %s (not a signal).\n",
        __FUNCTION__, dbus_message_get_member(aMsg));
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  if (dbus_message_get_path(aMsg) == NULL) {
    BT_WARNING("DBusMessage %s has no bluetooth destination, ignoring\n",
        dbus_message_get_member(aMsg));
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  DBusError err;
  dbus_error_init(&err);

  nsAutoString signalPath;
  nsAutoString signalName;
  nsAutoString signalInterface;

  BT_LOG("%s: %s, %s, %s", __FUNCTION__,
                          dbus_message_get_interface(aMsg),
                          dbus_message_get_path(aMsg),
                          dbus_message_get_member(aMsg));

  signalInterface = NS_ConvertUTF8toUTF16(dbus_message_get_interface(aMsg));
  signalPath = NS_ConvertUTF8toUTF16(dbus_message_get_path(aMsg));
  signalName = NS_ConvertUTF8toUTF16(dbus_message_get_member(aMsg));
  nsString errorStr;
  BluetoothValue v;

  // Since the signalPath extracted from dbus message is a object path,
  // we'd like to re-assign them to corresponding key entry in
  // BluetoothSignalObserverTable
  if (signalInterface.EqualsLiteral(DBUS_MANAGER_IFACE)) {
    signalPath.AssignLiteral(KEY_MANAGER);
  } else if (signalInterface.EqualsLiteral(DBUS_ADAPTER_IFACE)) {
    signalPath.AssignLiteral(KEY_ADAPTER);
  } else if (signalInterface.EqualsLiteral(DBUS_DEVICE_IFACE)){
    signalPath = GetAddressFromObjectPath(signalPath);
  }

  if (dbus_message_is_signal(aMsg, DBUS_ADAPTER_IFACE, "DeviceFound")) {
    DBusMessageIter iter;

    if (!dbus_message_iter_init(aMsg, &iter)) {
      NS_WARNING("Can't create iterator!");
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    const char* addr;
    dbus_message_iter_get_basic(&iter, &addr);

    if (dbus_message_iter_next(&iter)) {
      Properties* props = sDeviceProperties;
      Properties* newProps;
      bool replaced = ReplaceConnectedType(sDeviceProperties, &newProps,
                                           ArrayLength(sDeviceProperties));
      if (replaced) {
        props = newProps;
      }
      ParseProperties(&iter,
                      v,
                      errorStr,
                      props,
                      ArrayLength(sDeviceProperties));
      if (replaced) {
        free(newProps);
      }
      if (v.type() == BluetoothValue::TArrayOfBluetoothNamedValue)
      {
        // The DBus DeviceFound message actually passes back a key value object
        // with the address as the key and the rest of the device properties as
        // a dict value. After we parse out the properties, we need to go back
        // and add the address to the ipdl dict we've created to make sure we
        // have all of the information to correctly build the device.
        nsAutoString addrstr = NS_ConvertUTF8toUTF16(addr);
        nsString path = GetObjectPathFromAddress(signalPath, addrstr);

        v.get_ArrayOfBluetoothNamedValue()
          .AppendElement(BluetoothNamedValue(NS_LITERAL_STRING("Address"),
                                             addrstr));

        // We also need to create a path for the device, to make sure we know
        // where to access it later.
        v.get_ArrayOfBluetoothNamedValue()
          .AppendElement(BluetoothNamedValue(NS_LITERAL_STRING("Path"),
                                             path));
        const InfallibleTArray<BluetoothNamedValue>& properties =
          v.get_ArrayOfBluetoothNamedValue();
        if (!ContainsIcon(properties)) {
          for (uint8_t i = 0; i < properties.Length(); i++) {
            // It is possible that property Icon missed due to CoD of major
            // class is TOY but service class is "Audio", we need to assign
            // Icon as audio-card. This is for PTS test TC_AG_COD_BV_02_I.
            // As HFP specification defined that
            // service class is "Audio" can be considered as HFP AG.
            if (properties[i].name().EqualsLiteral("Class")) {
              if (HasAudioService(properties[i].value().get_uint32_t())) {
                v.get_ArrayOfBluetoothNamedValue()
                  .AppendElement(
                    BluetoothNamedValue(NS_LITERAL_STRING("Icon"),
                                        NS_LITERAL_STRING("audio-card")));
              }
              break;
            }
          }
        }
      }
    } else {
      errorStr.AssignLiteral("Unexpected message struct in msg DeviceFound");
    }
  } else if (dbus_message_is_signal(aMsg, DBUS_ADAPTER_IFACE,
                                    "DeviceDisappeared")) {
    const char* str;
    if (!dbus_message_get_args(aMsg, &err,
                               DBUS_TYPE_STRING, &str,
                               DBUS_TYPE_INVALID)) {
      LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, aMsg);
      errorStr.AssignLiteral("Cannot parse device address!");
    } else {
      v = NS_ConvertUTF8toUTF16(str);
    }
  } else if (dbus_message_is_signal(aMsg, DBUS_ADAPTER_IFACE,
                                    "DeviceCreated")) {
    const char* str;
    if (!dbus_message_get_args(aMsg, &err,
                               DBUS_TYPE_OBJECT_PATH, &str,
                               DBUS_TYPE_INVALID)) {
      LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, aMsg);
      errorStr.AssignLiteral("Cannot parse device path!");
    } else {
      v = NS_ConvertUTF8toUTF16(str);
    }

    BluetoothSignal signal(signalName, signalPath, v);

    // Fire a Device properties fetcher at the main thread
    nsRefPtr<DevicePropertiesSignalHandler> b =
      new DevicePropertiesSignalHandler(signal);
    if (NS_FAILED(NS_DispatchToMainThread(b))) {
      NS_WARNING("Failed to dispatch to main thread!");
    }
    // Since we're handling this in other threads, just fall out here
    return DBUS_HANDLER_RESULT_HANDLED;
  } else if (dbus_message_is_signal(aMsg, DBUS_ADAPTER_IFACE,
                                    "DeviceRemoved")) {
    const char* str;
    if (!dbus_message_get_args(aMsg, &err,
                               DBUS_TYPE_OBJECT_PATH, &str,
                               DBUS_TYPE_INVALID)) {
      LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, aMsg);
      errorStr.AssignLiteral("Cannot parse device path!");
    } else {
      v = NS_ConvertUTF8toUTF16(str);
    }
  } else if (dbus_message_is_signal(aMsg, DBUS_ADAPTER_IFACE,
                                    "PropertyChanged")) {
    ParsePropertyChange(aMsg,
                        v,
                        errorStr,
                        sAdapterProperties,
                        ArrayLength(sAdapterProperties));
  } else if (dbus_message_is_signal(aMsg, DBUS_DEVICE_IFACE,
                                    "PropertyChanged")) {
    Properties* props = sDeviceProperties;
    Properties* newProps;
    bool replaced = ReplaceConnectedType(sDeviceProperties, &newProps,
                                         ArrayLength(sDeviceProperties));
    if (replaced) {
      props = newProps;
    }
    ParsePropertyChange(aMsg,
                        v,
                        errorStr,
                        props,
                        ArrayLength(sDeviceProperties));
    if (replaced) {
      free(newProps);
    }

    BluetoothNamedValue& property = v.get_ArrayOfBluetoothNamedValue()[0];
    if (property.name().EqualsLiteral("Paired")) {
      // transfer signal to BluetoothService and
      // broadcast system message of bluetooth-pairingstatuschanged
      signalName = NS_LITERAL_STRING("PairedStatusChanged");
      signalPath = NS_LITERAL_STRING(KEY_LOCAL_AGENT);
      property.name() = NS_LITERAL_STRING("paired");
    }
  } else if (dbus_message_is_signal(aMsg, DBUS_MANAGER_IFACE, "AdapterAdded")) {
    const char* str;
    if (!dbus_message_get_args(aMsg, &err,
                               DBUS_TYPE_OBJECT_PATH, &str,
                               DBUS_TYPE_INVALID)) {
      LOG_AND_FREE_DBUS_ERROR_WITH_MSG(&err, aMsg);
      errorStr.AssignLiteral("Cannot parse manager path!");
    } else {
      v = NS_ConvertUTF8toUTF16(str);
      nsRefPtr<PrepareAdapterTask> b = new PrepareAdapterTask(v.get_nsString());
      if (NS_FAILED(NS_DispatchToMainThread(b))) {
        NS_WARNING("Failed to dispatch to main thread!");
      }
    }
  } else if (dbus_message_is_signal(aMsg, DBUS_MANAGER_IFACE,
                                    "PropertyChanged")) {
    ParsePropertyChange(aMsg,
                        v,
                        errorStr,
                        sManagerProperties,
                        ArrayLength(sManagerProperties));
  } else {
    nsAutoCString signalStr;
    signalStr += dbus_message_get_member(aMsg);
    signalStr += " Signal not handled!";
    NS_WARNING(signalStr.get());
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
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
GetDefaultAdapterPath(BluetoothValue& aValue, nsString& aError)
{
  // This could block. It should never be run on the main thread.
  MOZ_ASSERT(!NS_IsMainThread());

  DBusError err;
  dbus_error_init(&err);

  DBusMessage* msg = dbus_func_args_timeout(gThreadConnection->GetConnection(),
                                            1000,
                                            &err,
                                            "/",
                                            DBUS_MANAGER_IFACE,
                                            "DefaultAdapter",
                                            DBUS_TYPE_INVALID);
  UnpackObjectPathMessage(msg, &err, aValue, aError);
  if (msg) {
    dbus_message_unref(msg);
  }
  if (!aError.IsEmpty()) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

bool
BluetoothDBusService::IsReady()
{
  if (!IsEnabled() || !mConnection || !gThreadConnection || IsToggling()) {
    NS_WARNING("Bluetooth service is not ready yet!");
    return false;
  }
  return true;
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

  if (!sPairingReqTable.IsInitialized()) {
    sPairingReqTable.Init();
  }

  if (!sAuthorizeReqTable.IsInitialized()) {
    sAuthorizeReqTable.Init();
  }

  BluetoothValue v;
  nsAutoString replyError;
  if (NS_FAILED(GetDefaultAdapterPath(v, replyError))) {
    // Adapter path is not ready yet
    // Let's do PrepareAdapterTask when we receive signal 'AdapterAdded'
  } else {
    // Adapter path has been ready. let's do PrepareAdapterTask now
    nsRefPtr<PrepareAdapterTask> b = new PrepareAdapterTask(v.get_nsString());
    if (NS_FAILED(NS_DispatchToMainThread(b))) {
      NS_WARNING("Failed to dispatch to main thread!");
    }
  }

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

  // If Bluetooth is turned off while connections exist, in order not to only
  // disconnect with profile connections with low level ACL connections alive,
  // we disconnect ACLs directly instead of closing each socket.
  DisconnectAllAcls(sAdapterPath);

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

  if (!dbus_connection_unregister_object_path(gThreadConnection->GetConnection(),
                                              KEY_LOCAL_AGENT)) {
    BT_WARNING("%s: Can't unregister object path %s for agent!",
        __FUNCTION__, KEY_LOCAL_AGENT);
  }

  if (!dbus_connection_unregister_object_path(gThreadConnection->GetConnection(),
                                              KEY_REMOTE_AGENT)) {
    BT_WARNING("%s: Can't unregister object path %s for agent!",
        __FUNCTION__, KEY_REMOTE_AGENT);
  }

  mConnection = nullptr;
  gThreadConnection = nullptr;

  // unref stored DBusMessages before clear the hashtable
  sPairingReqTable.EnumerateRead(UnrefDBusMessages, nullptr);
  sPairingReqTable.Clear();

  sAuthorizeReqTable.EnumerateRead(UnrefDBusMessages, nullptr);
  sAuthorizeReqTable.Clear();

  PR_AtomicSet(&sIsPairing, 0);

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

    BluetoothValue v;
    nsAutoString replyError;

    if (NS_FAILED(GetDefaultAdapterPath(v, replyError))) {
      DispatchBluetoothReply(mRunnable, v, replyError);
      return NS_ERROR_FAILURE;
    }

    DBusError err;
    dbus_error_init(&err);

    nsString objectPath = v.get_nsString();
    v = InfallibleTArray<BluetoothNamedValue>();
    if (!GetPropertiesInternal(objectPath, DBUS_ADAPTER_IFACE, v)) {
      NS_WARNING("Getting properties failed!");
      return NS_ERROR_FAILURE;
    }

    // We have to manually attach the path to the rest of the elements
    v.get_ArrayOfBluetoothNamedValue().AppendElement(
      BluetoothNamedValue(NS_LITERAL_STRING("Path"), objectPath));

    DispatchBluetoothReply(mRunnable, v, replyError);

    return NS_OK;
  }

private:
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
};

nsresult
BluetoothDBusService::GetDefaultAdapterPathInternal(
                                              BluetoothReplyRunnable* aRunnable)
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called from main thread!");

  if (!IsReady()) {
    BluetoothValue v;
    nsAutoString errorStr;
    errorStr.AssignLiteral("Bluetooth service is not ready yet!");
    DispatchBluetoothReply(aRunnable, v, errorStr);
    return NS_OK;
  }

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
BluetoothDBusService::SendDiscoveryMessage(const char* aMessageName,
                                           BluetoothReplyRunnable* aRunnable)
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called from main thread!");
  NS_ASSERTION(mConnection, "Must have a connection here!");

  if (!IsReady()) {
    BluetoothValue v;
    nsAutoString errorStr;
    errorStr.AssignLiteral("Bluetooth service is not ready yet!");
    DispatchBluetoothReply(aRunnable, v, errorStr);
    return NS_OK;
  }

  nsRefPtr<nsRunnable> task(new SendDiscoveryTask(aMessageName,
                                                  aRunnable));
  if (NS_FAILED(mBluetoothCommandThread->Dispatch(task, NS_DISPATCH_NORMAL))) {
    NS_WARNING("Cannot dispatch firmware loading task!");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
BluetoothDBusService::StopDiscoveryInternal(BluetoothReplyRunnable* aRunnable)
{
  return SendDiscoveryMessage("StopDiscovery", aRunnable);
}

nsresult
BluetoothDBusService::StartDiscoveryInternal(BluetoothReplyRunnable* aRunnable)
{
  return SendDiscoveryMessage("StartDiscovery", aRunnable);
}

class BluetoothDevicePropertiesRunnable : public nsRunnable
{
public:
  BluetoothDevicePropertiesRunnable(const BluetoothSignal& aSignal) :
    mSignal(aSignal)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    BluetoothValue v = mSignal.value();
    if (v.type() != BluetoothValue::TArrayOfBluetoothNamedValue ||
        v.get_ArrayOfBluetoothNamedValue().Length() == 0) {
      NS_WARNING("Invalid value type for GetDeviceProperties() method");
      return NS_ERROR_FAILURE;
    }

    const InfallibleTArray<BluetoothNamedValue>& arr =
      v.get_ArrayOfBluetoothNamedValue();
    NS_ASSERTION(arr[0].name().EqualsLiteral("path"),
                 "failed to get object path");
    NS_ASSERTION(arr[0].value().type() == BluetoothValue::TnsString,
                 "failed to get_nsString");
    nsString devicePath = arr[0].value().get_nsString();

    BluetoothValue prop;
    if (!GetPropertiesInternal(devicePath, DBUS_DEVICE_IFACE, prop)) {
      NS_WARNING("Getting properties failed!");
      return NS_ERROR_FAILURE;
    }
    InfallibleTArray<BluetoothNamedValue>& properties =
      prop.get_ArrayOfBluetoothNamedValue();

    // Return original dbus message parameters and also device name
    // for agent events "RequestConfirmation", "RequestPinCode",
    // and "RequestPasskey"
    InfallibleTArray<BluetoothNamedValue>& parameters =
      v.get_ArrayOfBluetoothNamedValue();

    // Replace object path with device address
    nsString address = GetAddressFromObjectPath(devicePath);
    parameters[0].name().AssignLiteral("address");
    parameters[0].value() = address;

    uint8_t i;
    for (i = 0; i < properties.Length(); i++) {
      // Append device name
      if (properties[i].name().EqualsLiteral("Name")) {
        properties[i].name().AssignLiteral("name");
        parameters.AppendElement(properties[i]);
        mSignal.value() = parameters;
        break;
      }
    }
    NS_ASSERTION(i != properties.Length(), "failed to get device name");

    nsRefPtr<DistributeBluetoothSignalTask> t =
      new DistributeBluetoothSignalTask(mSignal);
    if (NS_FAILED(NS_DispatchToMainThread(t))) {
       NS_WARNING("Failed to dispatch to main thread!");
       return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }

private:
  BluetoothSignal mSignal;
};

class BluetoothPairedDevicePropertiesRunnable : public nsRunnable
{
public:
  BluetoothPairedDevicePropertiesRunnable(
                                     BluetoothReplyRunnable* aRunnable,
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

    BluetoothValue values = InfallibleTArray<BluetoothNamedValue>();

    for (uint32_t i = 0; i < mDeviceAddresses.Length(); i++) {
      BluetoothValue v;
      if (!GetPropertiesInternal(mDeviceAddresses[i], DBUS_DEVICE_IFACE, v)) {
        nsAutoString errorStr;
        errorStr.AssignLiteral("Getting properties failed!");
        NS_WARNING(NS_ConvertUTF16toUTF8(errorStr).get());
        mRunnable->SetReply(new BluetoothReply(BluetoothReplyError(errorStr)));
        if (NS_FAILED(NS_DispatchToMainThread(mRunnable))) {
          NS_WARNING("Failed to dispatch to main thread!");
        }
        return NS_OK;
      }
      v.get_ArrayOfBluetoothNamedValue().AppendElement(
        BluetoothNamedValue(NS_LITERAL_STRING("Path"), mDeviceAddresses[i])
      );

      InfallibleTArray<BluetoothNamedValue>& deviceProperties =
        v.get_ArrayOfBluetoothNamedValue();
      for (uint32_t p = 0;
           p < v.get_ArrayOfBluetoothNamedValue().Length(); ++p) {
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
BluetoothDBusService::GetDevicePropertiesInternal(const BluetoothSignal& aSignal)
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called from main thread!");

  if (!mConnection || !gThreadConnection) {
    NS_ERROR("Bluetooth service not started yet!");
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<nsRunnable> func(new BluetoothDevicePropertiesRunnable(aSignal));
  if (NS_FAILED(mBluetoothCommandThread->Dispatch(func, NS_DISPATCH_NORMAL))) {
    NS_WARNING("Cannot dispatch task!");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
BluetoothDBusService::GetPairedDevicePropertiesInternal(
                                     const nsTArray<nsString>& aDeviceAddresses,
                                     BluetoothReplyRunnable* aRunnable)
{
  if (!IsReady()) {
    BluetoothValue v;
    nsAutoString errorStr;
    errorStr.AssignLiteral("Bluetooth service is not ready yet!");
    DispatchBluetoothReply(aRunnable, v, errorStr);
    return NS_OK;
  }

  nsRefPtr<BluetoothReplyRunnable> runnable = aRunnable;
  nsRefPtr<nsRunnable> func(
    new BluetoothPairedDevicePropertiesRunnable(runnable, aDeviceAddresses));
  if (NS_FAILED(mBluetoothCommandThread->Dispatch(func, NS_DISPATCH_NORMAL))) {
    NS_WARNING("Cannot dispatch task!");
    return NS_ERROR_FAILURE;
  }

  runnable.forget();
  return NS_OK;
}

nsresult
BluetoothDBusService::SetProperty(BluetoothObjectType aType,
                                  const BluetoothNamedValue& aValue,
                                  BluetoothReplyRunnable* aRunnable)
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called from main thread!");

  if (!IsReady()) {
    BluetoothValue v;
    nsAutoString errorStr;
    errorStr.AssignLiteral("Bluetooth service is not ready yet!");
    DispatchBluetoothReply(aRunnable, v, errorStr);
    return NS_OK;
  }

  MOZ_ASSERT(aType < ArrayLength(sBluetoothDBusIfaces));
  const char* interface = sBluetoothDBusIfaces[aType];

  /* Compose the command */
  DBusMessage* msg = dbus_message_new_method_call(
                                      "org.bluez",
                                      NS_ConvertUTF16toUTF8(sAdapterPath).get(),
                                      interface,
                                      "SetProperty");

  if (!msg) {
    NS_WARNING("Could not allocate D-Bus message object!");
    return NS_ERROR_FAILURE;
  }

  nsCString intermediatePropName(NS_ConvertUTF16toUTF8(aValue.name()));
  const char* propName = intermediatePropName.get();
  if (!dbus_message_append_args(msg, DBUS_TYPE_STRING, &propName,
                                DBUS_TYPE_INVALID)) {
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
    const char* tempStr = str.get();
    val = &tempStr;
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
  if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, 
                                        var_type, &value_iter) ||
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
GetDeviceServiceChannel(const nsAString& aObjectPath,
                        const nsAString& aPattern,
                        int aAttributeId)
{
  // This is a blocking call, should not be run on main thread.
  MOZ_ASSERT(!NS_IsMainThread());

#ifdef MOZ_WIDGET_GONK
  // GetServiceAttributeValue only exists in android's bluez dbus binding
  // implementation
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
#else
  // FIXME/Bug 793977 qdot: Just return something for desktop, until we have a
  // parser for the GetServiceAttributes xml block
  return 1;
#endif
}

// static
bool
BluetoothDBusService::RemoveReservedServicesInternal(
                                      const nsTArray<uint32_t>& aServiceHandles)
{
  MOZ_ASSERT(!NS_IsMainThread());

  int length = aServiceHandles.Length();
  if (length == 0) return false;

  const uint32_t* services = aServiceHandles.Elements();

  DBusMessage* reply =
    dbus_func_args(gThreadConnection->GetConnection(),
                   NS_ConvertUTF16toUTF8(sAdapterPath).get(),
                   DBUS_ADAPTER_IFACE, "RemoveReservedServiceRecords",
                   DBUS_TYPE_ARRAY, DBUS_TYPE_UINT32,
                   &services, length, DBUS_TYPE_INVALID);

  if (!reply) return false;

  dbus_message_unref(reply);
  return true;
}

nsresult
BluetoothDBusService::CreatePairedDeviceInternal(
                                              const nsAString& aDeviceAddress,
                                              int aTimeout,
                                              BluetoothReplyRunnable* aRunnable)
{
  const char *capabilities = B2G_AGENT_CAPABILITIES;
  const char *deviceAgentPath = KEY_REMOTE_AGENT;

  nsCString tempDeviceAddress = NS_ConvertUTF16toUTF8(aDeviceAddress);
  const char *deviceAddress = tempDeviceAddress.get();

  /**
   * FIXME: Bug 820274
   *
   * If the user turns off Bluetooth in the middle of pairing process, the
   * callback function GetObjectPathCallback (see the third argument of the
   * function call above) may still be called while enabling next time by
   * dbus daemon. To prevent this from happening, added a flag to distinguish
   * if Bluetooth has been turned off. Nevertheless, we need a check if there
   * is a better solution.
   *
   * Please see Bug 818696 for more information.
   */
  PR_AtomicIncrement(&sIsPairing);

  nsRefPtr<BluetoothReplyRunnable> runnable = aRunnable;
  // Then send CreatePairedDevice, it will register a temp device agent then
  // unregister it after pairing process is over
  bool ret = dbus_func_args_async(mConnection,
                                  aTimeout,
                                  GetObjectPathCallback,
                                  (void*)runnable,
                                  NS_ConvertUTF16toUTF8(sAdapterPath).get(),
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
BluetoothDBusService::RemoveDeviceInternal(const nsAString& aDeviceAddress,
                                           BluetoothReplyRunnable* aRunnable)
{
  if (!IsReady()) {
    BluetoothValue v;
    nsAutoString errorStr;
    errorStr.AssignLiteral("Bluetooth service is not ready yet!");
    DispatchBluetoothReply(aRunnable, v, errorStr);
    return NS_OK;
  }

  nsCString tempDeviceObjectPath =
    NS_ConvertUTF16toUTF8(GetObjectPathFromAddress(sAdapterPath,
                                                   aDeviceAddress));

  nsRefPtr<nsRunnable> task(new RemoveDeviceTask(tempDeviceObjectPath,
                                                 aRunnable));

  if (NS_FAILED(mBluetoothCommandThread->Dispatch(task, NS_DISPATCH_NORMAL))) {
    NS_WARNING("Cannot dispatch firmware loading task!");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

bool
BluetoothDBusService::SetPinCodeInternal(const nsAString& aDeviceAddress,
                                         const nsAString& aPinCode,
                                         BluetoothReplyRunnable* aRunnable)
{
  nsAutoString errorStr;
  BluetoothValue v = true;
  DBusMessage *msg;
  if (!sPairingReqTable.Get(aDeviceAddress, &msg)) {
    BT_WARNING("%s: Couldn't get original request message.", __FUNCTION__);
    errorStr.AssignLiteral("Couldn't get original request message.");
    DispatchBluetoothReply(aRunnable, v, errorStr);
    return false;
  }

  DBusMessage *reply = dbus_message_new_method_return(msg);

  if (!reply) {
    BT_WARNING("%s: Memory can't be allocated for the message.", __FUNCTION__);
    dbus_message_unref(msg);
    errorStr.AssignLiteral("Memory can't be allocated for the message.");
    DispatchBluetoothReply(aRunnable, v, errorStr);
    return false;
  }

  bool result;

  nsCString tempPinCode = NS_ConvertUTF16toUTF8(aPinCode);
  const char* pinCode = tempPinCode.get();

  if (!dbus_message_append_args(reply,
                                DBUS_TYPE_STRING, &pinCode,
                                DBUS_TYPE_INVALID)) {
    BT_WARNING("%s: Couldn't append arguments to dbus message.", __FUNCTION__);
    errorStr.AssignLiteral("Couldn't append arguments to dbus message.");
    result = false;
  } else {
    result = dbus_connection_send(mConnection, reply, NULL);
  }

  dbus_message_unref(msg);
  dbus_message_unref(reply);

  sPairingReqTable.Remove(aDeviceAddress);
  DispatchBluetoothReply(aRunnable, v, errorStr);
  return result;
}

bool
BluetoothDBusService::SetPasskeyInternal(const nsAString& aDeviceAddress,
                                         uint32_t aPasskey,
                                         BluetoothReplyRunnable* aRunnable)
{
  nsAutoString errorStr;
  BluetoothValue v = true;
  DBusMessage *msg;
  if (!sPairingReqTable.Get(aDeviceAddress, &msg)) {
    BT_WARNING("%s: Couldn't get original request message.", __FUNCTION__);
    errorStr.AssignLiteral("Couldn't get original request message.");
    DispatchBluetoothReply(aRunnable, v, errorStr);
    return false;
  }

  DBusMessage *reply = dbus_message_new_method_return(msg);

  if (!reply) {
    BT_WARNING("%s: Memory can't be allocated for the message.", __FUNCTION__);
    dbus_message_unref(msg);
    errorStr.AssignLiteral("Memory can't be allocated for the message.");
    DispatchBluetoothReply(aRunnable, v, errorStr);
    return false;
  }

  uint32_t passkey = aPasskey;
  bool result;

  if (!dbus_message_append_args(reply,
                                DBUS_TYPE_UINT32, &passkey,
                                DBUS_TYPE_INVALID)) {
    BT_WARNING("%s: Couldn't append arguments to dbus message.", __FUNCTION__);
    errorStr.AssignLiteral("Couldn't append arguments to dbus message.");
    result = false;
  } else {
    result = dbus_connection_send(mConnection, reply, NULL);
  }

  dbus_message_unref(msg);
  dbus_message_unref(reply);

  sPairingReqTable.Remove(aDeviceAddress);
  DispatchBluetoothReply(aRunnable, v, errorStr);
  return result;
}

bool
BluetoothDBusService::SetPairingConfirmationInternal(
                                              const nsAString& aDeviceAddress,
                                              bool aConfirm,
                                              BluetoothReplyRunnable* aRunnable)
{
  nsAutoString errorStr;
  BluetoothValue v = true;
  DBusMessage *msg;
  if (!sPairingReqTable.Get(aDeviceAddress, &msg)) {
    BT_WARNING("%s: Couldn't get original request message.", __FUNCTION__);
    errorStr.AssignLiteral("Couldn't get original request message.");
    DispatchBluetoothReply(aRunnable, v, errorStr);
    return false;
  }

  DBusMessage *reply;

  if (aConfirm) {
    reply = dbus_message_new_method_return(msg);
  } else {
    reply = dbus_message_new_error(msg, "org.bluez.Error.Rejected",
                                   "User rejected confirmation");
  }

  if (!reply) {
    BT_WARNING("%s: Memory can't be allocated for the message.", __FUNCTION__);
    dbus_message_unref(msg);
    errorStr.AssignLiteral("Memory can't be allocated for the message.");
    DispatchBluetoothReply(aRunnable, v, errorStr);
    return false;
  }

  bool result = dbus_connection_send(mConnection, reply, NULL);
  if (!result) {
    errorStr.AssignLiteral("Can't send message!");
  }
  dbus_message_unref(msg);
  dbus_message_unref(reply);

  sPairingReqTable.Remove(aDeviceAddress);
  DispatchBluetoothReply(aRunnable, v, errorStr);
  return result;
}

bool
BluetoothDBusService::SetAuthorizationInternal(
                                              const nsAString& aDeviceAddress,
                                              bool aAllow,
                                              BluetoothReplyRunnable* aRunnable)
{
  nsAutoString errorStr;
  BluetoothValue v = true;
  DBusMessage *msg;

  if (!sAuthorizeReqTable.Get(aDeviceAddress, &msg)) {
    BT_WARNING("%s: Couldn't get original request message.", __FUNCTION__);
    errorStr.AssignLiteral("Couldn't get original request message.");
    DispatchBluetoothReply(aRunnable, v, errorStr);
    return false;
  }

  DBusMessage *reply;

  if (aAllow) {
    reply = dbus_message_new_method_return(msg);
  } else {
    reply = dbus_message_new_error(msg, "org.bluez.Error.Rejected",
                                   "User rejected authorization");
  }

  if (!reply) {
    BT_WARNING("%s: Memory can't be allocated for the message.", __FUNCTION__);
    dbus_message_unref(msg);
    errorStr.AssignLiteral("Memory can't be allocated for the message.");
    DispatchBluetoothReply(aRunnable, v, errorStr);
    return false;
  }

  bool result = dbus_connection_send(mConnection, reply, NULL);
  if (!result) {
    errorStr.AssignLiteral("Can't send message!");
  }
  dbus_message_unref(msg);
  dbus_message_unref(reply);

  sAuthorizeReqTable.Remove(aDeviceAddress);
  DispatchBluetoothReply(aRunnable, v, errorStr);
  return result;
}

nsresult
BluetoothDBusService::PrepareAdapterInternal()
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called from main thread!");

  if (!mConnection || !gThreadConnection) {
    NS_ERROR("Bluetooth service not started yet!");
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<nsRunnable> func(new PrepareAdapterRunnable());
  if (NS_FAILED(mBluetoothCommandThread->Dispatch(func, NS_DISPATCH_NORMAL))) {
    NS_WARNING("Cannot dispatch task!");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

void
BluetoothDBusService::Connect(const nsAString& aDeviceAddress,
                              const uint16_t aProfileId,
                              BluetoothReplyRunnable* aRunnable)
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called from main thread!");

  BluetoothValue v;
  nsAutoString errorStr;
  if (aProfileId == BluetoothServiceClass::HANDSFREE) {
    BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
    if (!hfp->Connect(GetObjectPathFromAddress(sAdapterPath, aDeviceAddress),
                      true, aRunnable)) {
      errorStr.AssignLiteral("BluetoothHfpManager has connected/is connecting \
                              to a headset!");
      DispatchBluetoothReply(aRunnable, v, errorStr);
    }
  } else if (aProfileId == BluetoothServiceClass::HEADSET) {
    BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
    if (!hfp->Connect(GetObjectPathFromAddress(sAdapterPath, aDeviceAddress),
                      false, aRunnable)) {
      errorStr.AssignLiteral("BluetoothHfpManager has connected/is connecting \
                              to a headset!");
      DispatchBluetoothReply(aRunnable, v, errorStr);
    }
  } else if (aProfileId == BluetoothServiceClass::OBJECT_PUSH) {
    BluetoothOppManager* opp = BluetoothOppManager::Get();
    if (!opp->Connect(GetObjectPathFromAddress(sAdapterPath, aDeviceAddress),
                      aRunnable)) {
      errorStr.AssignLiteral("BluetoothOppManager has been connected/is \
                              connecting!");
      DispatchBluetoothReply(aRunnable, v, errorStr);
    }
  } else {
    NS_WARNING("Unknown Profile");
  }
}

void
BluetoothDBusService::Disconnect(const uint16_t aProfileId,
                                 BluetoothReplyRunnable* aRunnable)
{
  if (aProfileId == BluetoothServiceClass::HANDSFREE ||
      aProfileId == BluetoothServiceClass::HEADSET) {
    BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
    hfp->Disconnect();
  } else if (aProfileId == BluetoothServiceClass::OBJECT_PUSH) {
    BluetoothOppManager* opp = BluetoothOppManager::Get();
    opp->Disconnect();
  } else {
    NS_WARNING("Unknown profile");
    return;
  }

  // Currently, just fire success because Disconnect() doesn't fail,
  // but we still make aRunnable pass into this function for future
  // once Disconnect will fail.
  nsAutoString replyError;
  BluetoothValue v = true;
  DispatchBluetoothReply(aRunnable, v, replyError);
}

bool
BluetoothDBusService::IsConnected(const uint16_t aProfileId)
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called from main thread!");

  if (aProfileId == BluetoothServiceClass::HANDSFREE ||
      aProfileId == BluetoothServiceClass::HEADSET) {
    BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
    return (hfp->GetConnectionStatus() ==
            SocketConnectionStatus::SOCKET_CONNECTED);
  } else if (aProfileId == BluetoothServiceClass::OBJECT_PUSH) {
    BluetoothOppManager* opp = BluetoothOppManager::Get();
    return opp->IsTransferring();
  }

  return false;
}

class ConnectBluetoothSocketRunnable : public nsRunnable
{
public:
  ConnectBluetoothSocketRunnable(BluetoothReplyRunnable* aRunnable,
                                 UnixSocketConsumer* aConsumer,
                                 const nsAString& aObjectPath,
                                 const nsAString& aServiceUUID,
                                 BluetoothSocketType aType,
                                 bool aAuth,
                                 bool aEncrypt,
                                 int aChannel)
    : mRunnable(dont_AddRef(aRunnable))
    , mConsumer(aConsumer)
    , mObjectPath(aObjectPath)
    , mServiceUUID(aServiceUUID)
    , mType(aType)
    , mAuth(aAuth)
    , mEncrypt(aEncrypt)
    , mChannel(aChannel)
  {
  }

  nsresult
  Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsString address = GetAddressFromObjectPath(mObjectPath);
    BluetoothValue v;
    nsAutoString replyError;
    BluetoothUnixSocketConnector* c =
      new BluetoothUnixSocketConnector(mType, mChannel, mAuth, mEncrypt);
    if (!mConsumer->ConnectSocket(c, NS_ConvertUTF16toUTF8(address).get())) {
      replyError.AssignLiteral("SocketConnectionError");
      DispatchBluetoothReply(mRunnable, v, replyError);
      return NS_ERROR_FAILURE;
    }
    return NS_OK;
  }

private:
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
  nsRefPtr<UnixSocketConsumer> mConsumer;
  nsString mObjectPath;
  nsString mServiceUUID;
  BluetoothSocketType mType;
  bool mAuth;
  bool mEncrypt;
  int mChannel;
};

class GetDeviceChannelForConnectRunnable : public nsRunnable
{
public:
  GetDeviceChannelForConnectRunnable(BluetoothReplyRunnable* aRunnable,
                                     UnixSocketConsumer* aConsumer,
                                     const nsAString& aObjectPath,
                                     const nsAString& aServiceUUID,
                                     BluetoothSocketType aType,
                                     bool aAuth,
                                     bool aEncrypt)
    : mRunnable(dont_AddRef(aRunnable)),
      mConsumer(aConsumer),
      mObjectPath(aObjectPath),
      mServiceUUID(aServiceUUID),
      mType(aType),
      mAuth(aAuth),
      mEncrypt(aEncrypt)
  {
  }

  nsresult
  Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    int channel = GetDeviceServiceChannel(mObjectPath, mServiceUUID, 0x0004);
    BluetoothValue v;
    nsString replyError;
    if (channel < 0) {
      replyError.AssignLiteral("DeviceChannelRetrievalError");
      DispatchBluetoothReply(mRunnable, v, replyError);
      return NS_OK;
    }
    nsRefPtr<nsRunnable> func(new ConnectBluetoothSocketRunnable(mRunnable,
                                                                 mConsumer,
                                                                 mObjectPath,
                                                                 mServiceUUID,
                                                                 mType, mAuth,
                                                                 mEncrypt,
                                                                 channel));
    if (NS_FAILED(NS_DispatchToMainThread(func, NS_DISPATCH_NORMAL))) {
      NS_WARNING("Cannot dispatch connection task!");
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }

private:
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
  nsRefPtr<UnixSocketConsumer> mConsumer;
  nsString mObjectPath;
  nsString mServiceUUID;
  BluetoothSocketType mType;
  bool mAuth;
  bool mEncrypt;
};

nsresult
BluetoothDBusService::GetSocketViaService(
                                    const nsAString& aObjectPath,
                                    const nsAString& aService,
                                    BluetoothSocketType aType,
                                    bool aAuth,
                                    bool aEncrypt,
                                    mozilla::ipc::UnixSocketConsumer* aConsumer,
                                    BluetoothReplyRunnable* aRunnable)
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called from main thread!");
  if (!IsReady()) {
    BluetoothValue v;
    nsAutoString errorStr;
    errorStr.AssignLiteral("Bluetooth service is not ready yet!");
    DispatchBluetoothReply(aRunnable, v, errorStr);
    return NS_OK;
  }

  nsRefPtr<BluetoothReplyRunnable> runnable = aRunnable;

  nsRefPtr<nsRunnable> func(new GetDeviceChannelForConnectRunnable(
                              runnable,
                              aConsumer,
                              aObjectPath,
                              aService, aType,
                              aAuth, aEncrypt));
  if (NS_FAILED(mBluetoothCommandThread->Dispatch(func, NS_DISPATCH_NORMAL))) {
    NS_WARNING("Cannot dispatch firmware loading task!");
    return NS_ERROR_FAILURE;
  }

  runnable.forget();
  return NS_OK;
}

nsresult
BluetoothDBusService::GetScoSocket(const nsAString& aAddress,
                                   bool aAuth,
                                   bool aEncrypt,
                                   mozilla::ipc::UnixSocketConsumer* aConsumer)
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called from main thread!");

  if (!mConnection || !gThreadConnection) {
    NS_ERROR("Bluetooth service not started yet!");
    return NS_ERROR_FAILURE;
  }

  BluetoothUnixSocketConnector* c =
    new BluetoothUnixSocketConnector(BluetoothSocketType::SCO, -1,
                                     aAuth, aEncrypt);

  if (!aConsumer->ConnectSocket(c, NS_ConvertUTF16toUTF8(aAddress).get())) {
    nsAutoString replyError;
    replyError.AssignLiteral("SocketConnectionError");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

void
BluetoothDBusService::SendFile(const nsAString& aDeviceAddress,
                               BlobParent* aBlobParent,
                               BlobChild* aBlobChild,
                               BluetoothReplyRunnable* aRunnable)
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called from main thread!");

  // Currently we only support one device sending one file at a time,
  // so we don't need aDeviceAddress here because the target device
  // has been determined when calling 'Connect()'. Nevertheless, keep
  // it for future use.
  BluetoothOppManager* opp = BluetoothOppManager::Get();
  BluetoothValue v = true;
  nsAutoString errorStr;

  if (!opp->SendFile(aBlobParent)) {
    errorStr.AssignLiteral("Calling SendFile() failed");
  }

  DispatchBluetoothReply(aRunnable, v, errorStr);
}

void
BluetoothDBusService::StopSendingFile(const nsAString& aDeviceAddress,
                                      BluetoothReplyRunnable* aRunnable)
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called from main thread!");

  // Currently we only support one device sending one file at a time,
  // so we don't need aDeviceAddress here because the target device
  // has been determined when calling 'Connect()'. Nevertheless, keep
  // it for future use.
  BluetoothOppManager* opp = BluetoothOppManager::Get();
  BluetoothValue v = true;
  nsAutoString errorStr;

  if (!opp->StopSendingFile()) {
    errorStr.AssignLiteral("Calling StopSendingFile() failed");
  }

  DispatchBluetoothReply(aRunnable, v, errorStr);
}

void
BluetoothDBusService::ConfirmReceivingFile(const nsAString& aDeviceAddress,
                                           bool aConfirm,
                                           BluetoothReplyRunnable* aRunnable)
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called from main thread!");

  // Currently we only support one device sending one file at a time,
  // so we don't need aDeviceAddress here because the target device
  // has been determined when calling 'Connect()'. Nevertheless, keep
  // it for future use.
  BluetoothOppManager* opp = BluetoothOppManager::Get();
  BluetoothValue v = true;
  nsAutoString errorStr;

  if (!opp->ConfirmReceivingFile(aConfirm)) {
    errorStr.AssignLiteral("Calling ConfirmReceivingFile() failed");
  }

  DispatchBluetoothReply(aRunnable, v, errorStr);
}

nsresult
BluetoothDBusService::ListenSocketViaService(
                                    int aChannel,
                                    BluetoothSocketType aType,
                                    bool aAuth,
                                    bool aEncrypt,
                                    mozilla::ipc::UnixSocketConsumer* aConsumer)
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called from main thread!");

  BluetoothUnixSocketConnector* c =
    new BluetoothUnixSocketConnector(aType, aChannel, aAuth, aEncrypt);
  if (!aConsumer->ListenSocket(c)) {
    NS_WARNING("Can't listen on socket!");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}
