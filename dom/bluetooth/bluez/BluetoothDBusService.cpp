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
#include "BluetoothA2dpManager.h"
#include "BluetoothHfpManager.h"
#include "BluetoothHidManager.h"
#include "BluetoothOppManager.h"
#include "BluetoothProfileController.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothUnixSocketConnector.h"
#include "BluetoothUtils.h"
#include "BluetoothUuid.h"

#include <cstdio>
#include <dbus/dbus.h>

#include "nsAutoPtr.h"
#include "nsThreadUtils.h"
#include "nsDebug.h"
#include "nsDataHashtable.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Atomics.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/Hal.h"
#include "mozilla/ipc/UnixSocket.h"
#include "mozilla/ipc/DBusUtils.h"
#include "mozilla/ipc/RawDBusConnection.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/Mutex.h"
#include "mozilla/NullPtr.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/unused.h"

#if defined(MOZ_WIDGET_GONK)
#include "cutils/properties.h"
#include <dlfcn.h>
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
#define DBUS_MANAGER_IFACE BLUEZ_DBUS_BASE_IFC  ".Manager"
#define DBUS_ADAPTER_IFACE BLUEZ_DBUS_BASE_IFC  ".Adapter"
#define DBUS_DEVICE_IFACE  BLUEZ_DBUS_BASE_IFC  ".Device"
#define DBUS_AGENT_IFACE   BLUEZ_DBUS_BASE_IFC  ".Agent"
#define DBUS_SINK_IFACE    BLUEZ_DBUS_BASE_IFC  ".AudioSink"
#define DBUS_CTL_IFACE     BLUEZ_DBUS_BASE_IFC  ".Control"
#define DBUS_INPUT_IFACE   BLUEZ_DBUS_BASE_IFC  ".Input"
#define BLUEZ_DBUS_BASE_PATH      "/org/bluez"
#define BLUEZ_DBUS_BASE_IFC       "org.bluez"
#define BLUEZ_ERROR_IFC           "org.bluez.Error"

#define ERR_A2DP_IS_DISCONNECTED      "A2dpIsDisconnected"
#define ERR_AVRCP_IS_DISCONNECTED     "AvrcpIsDisconnected"

/**
 * To not lock Bluetooth switch button on Settings UI because of any accident,
 * we will force disabling Bluetooth 5 seconds after the user requesting to
 * turn off Bluetooth.
 */
#define TIMEOUT_FORCE_TO_DISABLE_BT 5

#define BT_LAZY_THREAD_TIMEOUT_MS 3000

#ifdef MOZ_WIDGET_GONK
class Bluedroid
{
  struct ScopedDlHandleTraits
  {
    typedef void* type;
    static void* empty()
    {
      return nullptr;
    }
    static void release(void* handle)
    {
      if (!handle) {
        return;
      }
      int res = dlclose(handle);
      if (res) {
        BT_WARNING("Failed to close libbluedroid.so: %s", dlerror());
      }
    }
  };

public:
  Bluedroid()
  : m_bt_enable(nullptr)
  , m_bt_disable(nullptr)
  , m_bt_is_enabled(nullptr)
  {}

  bool Enable()
  {
    MOZ_ASSERT(!NS_IsMainThread()); // BT thread

    if (!mHandle && !Init()) {
      return false;
    } else if (m_bt_is_enabled() == 1) {
      return true;
    }
    // 0 == success, -1 == error
    return !m_bt_enable();
  }

  bool Disable()
  {
    MOZ_ASSERT(!NS_IsMainThread()); // BT thread

    if (!IsEnabled()) {
      return true;
    }
    // 0 == success, -1 == error
    return !m_bt_disable();
  }

  bool IsEnabled() const
  {
    MOZ_ASSERT(!NS_IsMainThread()); // BT thread

    if (!mHandle) {
      return false;
    }
    // 1 == enabled, 0 == disabled, -1 == error
    return m_bt_is_enabled() > 0;
  }

private:
  bool Init()
  {
    MOZ_ASSERT(!mHandle);

    Scoped<ScopedDlHandleTraits> handle(dlopen("libbluedroid.so", RTLD_LAZY));
    if (!handle) {
      BT_WARNING("Failed to open libbluedroid.so: %s", dlerror());
      return false;
    }
    int (*bt_enable)() = (int (*)())dlsym(handle, "bt_enable");
    if (!bt_enable) {
      BT_WARNING("Failed to lookup bt_enable: %s", dlerror());
      return false;
    }
    int (*bt_disable)() = (int (*)())dlsym(handle, "bt_disable");
    if (!bt_disable) {
      BT_WARNING("Failed to lookup bt_disable: %s", dlerror());
      return false;
    }
    int (*bt_is_enabled)() = (int (*)())dlsym(handle, "bt_is_enabled");
    if (!bt_is_enabled) {
      BT_WARNING("Failed to lookup bt_is_enabled: %s", dlerror());
      return false;
    }

    m_bt_enable = bt_enable;
    m_bt_disable = bt_disable;
    m_bt_is_enabled = bt_is_enabled;
    mHandle.reset(handle.forget());

    return true;
  }

  Scoped<ScopedDlHandleTraits> mHandle;
  int (* m_bt_enable)(void);
  int (* m_bt_disable)(void);
  int (* m_bt_is_enabled)(void);
};

//
// BT-thread-only variables
//
// The variables below must only be accessed from within the BT thread.
//

static class Bluedroid sBluedroid;
#endif

//
// Read-only constants
//
// The constants below are read-only and may be accessed from any
// thread. Most of the contain DBus state or settings, so keep them
// on the I/O thread if somehow possible.
//

typedef struct {
  const char* name;
  int type;
} Properties;

static const Properties sDeviceProperties[] = {
  {"Address", DBUS_TYPE_STRING},
  {"Name", DBUS_TYPE_STRING},
  {"Icon", DBUS_TYPE_STRING},
  {"Class", DBUS_TYPE_UINT32},
  {"UUIDs", DBUS_TYPE_ARRAY},
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
  {"Type", DBUS_TYPE_STRING},
  {"Broadcaster", DBUS_TYPE_BOOLEAN},
  {"Services", DBUS_TYPE_ARRAY}
};

static const Properties sAdapterProperties[] = {
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

static const Properties sManagerProperties[] = {
  {"Adapters", DBUS_TYPE_ARRAY},
};

static const Properties sSinkProperties[] = {
  {"State", DBUS_TYPE_STRING},
  {"Connected", DBUS_TYPE_BOOLEAN},
  {"Playing", DBUS_TYPE_BOOLEAN}
};

static const Properties sControlProperties[] = {
  {"Connected", DBUS_TYPE_BOOLEAN}
};

static const Properties sInputProperties[] = {
  {"Connected", DBUS_TYPE_BOOLEAN}
};

static const char* const sBluetoothDBusIfaces[] = {
  DBUS_MANAGER_IFACE,
  DBUS_ADAPTER_IFACE,
  DBUS_DEVICE_IFACE
};

static const char* const sBluetoothDBusSignals[] = {
  "type='signal',interface='org.freedesktop.DBus'",
  "type='signal',interface='org.bluez.Adapter'",
  "type='signal',interface='org.bluez.Manager'",
  "type='signal',interface='org.bluez.Device'",
  "type='signal',interface='org.bluez.Input'",
  "type='signal',interface='org.bluez.Network'",
  "type='signal',interface='org.bluez.NetworkServer'",
  "type='signal',interface='org.bluez.HealthDevice'",
  "type='signal',interface='org.bluez.AudioSink'",
  "type='signal',interface='org.bluez.Control'"
};

// Only A2DP and HID are authorized.
static const BluetoothServiceClass sAuthorizedServiceClass[] = {
  BluetoothServiceClass::A2DP,
  BluetoothServiceClass::HID
};

/**
 * The adapter name may not be ready whenever event 'AdapterAdded' is received,
 * so we'd like to wait for a bit. Only used on main thread.
 */
static const int sWaitingForAdapterNameInterval = 1000; // unit: ms

//
// I/O-thread-only variables
//
// The variables below must be accessed from within the I/O thread.
//

// The DBus connection to the BlueZ daemon
static StaticAutoPtr<RawDBusConnection> sDBusConnection;

// Keep the pairing requests.
static unsigned int sIsPairing = 0;

static nsDataHashtable<nsStringHashKey, DBusMessage* >* sPairingReqTable;

// The object path of the adapter that should
// be updated after switching Bluetooth.
static nsString sAdapterPath;

//
// The variables below are currently accessed from within multiple
// threads and should be moved to one specific thread soon.
//
// TODO: cleanup access to variables below.
//

/**
 * Disconnect all profiles before turning off Bluetooth. Please see Bug 891257
 * for more details. TODO: should be replaced or implemented with Atomic<>.
 */
static int sConnectedDeviceCount = 0;

// sStopBluetoothMonitor protects sGetPropertyMonitor. TODO: should be reviewed
// and replaced or implemented with Atomic<>.
static StaticAutoPtr<Monitor> sGetPropertyMonitor;
static StaticAutoPtr<Monitor> sStopBluetoothMonitor;

// A queue for connect/disconnect request. See Bug 913372 for details.
static nsTArray<nsRefPtr<BluetoothProfileController> > sControllerArray;

typedef void (*UnpackFunc)(DBusMessage*, DBusError*, BluetoothValue&, nsAString&);
typedef bool (*FilterFunc)(const BluetoothValue&);

static void
DispatchToDBusThread(Task* task)
{
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE, task);
}

static nsresult
DispatchToBtThread(nsIRunnable* aRunnable)
{
  /* Due to the fact that the startup and shutdown of the Bluetooth
   * system can take an indefinite amount of time, a separate thread
   * is used for running blocking calls. The thread is not intended
   * for regular Bluetooth operations though.
   */
  static StaticRefPtr<LazyIdleThread> sBluetoothThread;

  MOZ_ASSERT(NS_IsMainThread());

  if (!sBluetoothThread) {
    sBluetoothThread = new LazyIdleThread(BT_LAZY_THREAD_TIMEOUT_MS,
                                          NS_LITERAL_CSTRING("BluetoothDBusService"),
                                          LazyIdleThread::ManualShutdown);
  }
  return sBluetoothThread->Dispatch(aRunnable, NS_DISPATCH_NORMAL);
}

BluetoothDBusService::BluetoothDBusService()
{
  sGetPropertyMonitor = new Monitor("BluetoothService.sGetPropertyMonitor");
  sStopBluetoothMonitor = new Monitor("BluetoothService.sStopBluetoothMonitor");
}

BluetoothDBusService::~BluetoothDBusService()
{
  sStopBluetoothMonitor = nullptr;
  sGetPropertyMonitor = nullptr;
}

static bool
GetConnectedDevicesFilter(const BluetoothValue& aValue)
{
  // We don't have to filter device here
  return true;
}

static bool
GetPairedDevicesFilter(const BluetoothValue& aValue)
{
  // Check property 'Paired' and only paired device will be returned
  if (aValue.type() != BluetoothValue::TArrayOfBluetoothNamedValue) {
    BT_WARNING("Not a BluetoothNamedValue array!");
    return false;
  }

  const InfallibleTArray<BluetoothNamedValue>& deviceProperties =
    aValue.get_ArrayOfBluetoothNamedValue();
  uint32_t length = deviceProperties.Length();
  for (uint32_t p = 0; p < length; ++p) {
    if (deviceProperties[p].name().EqualsLiteral("Paired")) {
      return deviceProperties[p].value().get_bool();
    }
  }

  return false;
}

class DistributeBluetoothSignalTask : public nsRunnable
{
public:
  DistributeBluetoothSignalTask(const BluetoothSignal& aSignal)
    : mSignal(aSignal)
  {
  }

  nsresult Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);
    bs->DistributeSignal(mSignal);

    return NS_OK;
  }

private:
  BluetoothSignal mSignal;
};

class ControlPropertyChangedHandler : public nsRunnable
{
public:
  ControlPropertyChangedHandler(const BluetoothSignal& aSignal)
    : mSignal(aSignal)
  {
  }

  nsresult Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (mSignal.value().type() != BluetoothValue::TArrayOfBluetoothNamedValue) {
       BT_WARNING("Wrong value type for ControlPropertyChangedHandler");
       return NS_ERROR_FAILURE;
    }

    InfallibleTArray<BluetoothNamedValue>& arr =
      mSignal.value().get_ArrayOfBluetoothNamedValue();
    MOZ_ASSERT(arr[0].name().EqualsLiteral("Connected"));
    MOZ_ASSERT(arr[0].value().type() == BluetoothValue::Tbool);
    bool connected = arr[0].value().get_bool();

    BluetoothA2dpManager* a2dp = BluetoothA2dpManager::Get();
    NS_ENSURE_TRUE(a2dp, NS_ERROR_FAILURE);
    a2dp->SetAvrcpConnected(connected);
    return NS_OK;
  }

private:
  BluetoothSignal mSignal;
};

class SinkPropertyChangedHandler : public nsRunnable
{
public:
  SinkPropertyChangedHandler(const BluetoothSignal& aSignal)
    : mSignal(aSignal)
  {
  }

  NS_IMETHOD
  Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mSignal.name().EqualsLiteral("PropertyChanged"));
    MOZ_ASSERT(mSignal.value().type() ==
               BluetoothValue::TArrayOfBluetoothNamedValue);

    // Replace object path with device address
    nsString address = GetAddressFromObjectPath(mSignal.path());
    mSignal.path() = address;

    BluetoothA2dpManager* a2dp = BluetoothA2dpManager::Get();
    NS_ENSURE_TRUE(a2dp, NS_ERROR_FAILURE);
    a2dp->HandleSinkPropertyChanged(mSignal);
    return NS_OK;
  }

private:
  BluetoothSignal mSignal;
};

class InputPropertyChangedHandler : public nsRunnable
{
public:
  InputPropertyChangedHandler(const BluetoothSignal& aSignal)
    : mSignal(aSignal)
  {
  }

  NS_IMETHOD
  Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mSignal.name().EqualsLiteral("PropertyChanged"));
    MOZ_ASSERT(mSignal.value().type() == BluetoothValue::TArrayOfBluetoothNamedValue);

    // Replace object path with device address
    nsString address = GetAddressFromObjectPath(mSignal.path());
    mSignal.path() = address;

    BluetoothHidManager* hid = BluetoothHidManager::Get();
    NS_ENSURE_TRUE(hid, NS_ERROR_FAILURE);
    hid->HandleInputPropertyChanged(mSignal);
    return NS_OK;
  }

private:
  BluetoothSignal mSignal;
};

class TryFiringAdapterAddedTask : public Task
{
public:
  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    bs->AdapterAddedReceived();
    bs->TryFiringAdapterAdded();
  }
};

class TryFiringAdapterAddedRunnable : public nsRunnable
{
public:
  TryFiringAdapterAddedRunnable(bool aDelay)
    : mDelay(aDelay)
  { }

  nsresult Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (mDelay) {
      MessageLoop::current()->
        PostDelayedTask(FROM_HERE, new TryFiringAdapterAddedTask(),
                        sWaitingForAdapterNameInterval);
    } else {
      MessageLoop::current()->
        PostTask(FROM_HERE, new TryFiringAdapterAddedTask());
    }

    return NS_OK;
  }

private:
  bool mDelay;
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
UnpackObjectPathMessage(DBusMessage* aMsg, DBusError* aErr,
                        BluetoothValue& aValue, nsAString& aErrorStr)
{
  DBusError err;
  dbus_error_init(&err);
  if (!IsDBusMessageError(aMsg, aErr, aErrorStr)) {
    MOZ_ASSERT(dbus_message_get_type(aMsg) == DBUS_MESSAGE_TYPE_METHOD_RETURN,
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

class PrepareProfileManagersRunnable : public nsRunnable
{
public:
  nsresult Run()
  {
    BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
    if (!hfp || !hfp->Listen()) {
      BT_WARNING("Failed to start listening for BluetoothHfpManager!");
      return NS_ERROR_FAILURE;
    }

    BluetoothOppManager* opp = BluetoothOppManager::Get();
    if (!opp || !opp->Listen()) {
      BT_WARNING("Failed to start listening for BluetoothOppManager!");
      return NS_ERROR_FAILURE;
    }

    BluetoothA2dpManager* a2dp = BluetoothA2dpManager::Get();
    NS_ENSURE_TRUE(a2dp, NS_ERROR_FAILURE);
    a2dp->Reset();

    return NS_OK;
  }
};

static void
RunDBusCallback(DBusMessage* aMsg, void* aBluetoothReplyRunnable,
                UnpackFunc aFunc)
{
#ifdef MOZ_WIDGET_GONK
  // Due to the fact that we're running two dbus loops on desktop implicitly by
  // being gtk based, sometimes we'll get signals/reply coming in on the main
  // thread. There's not a lot we can do about that for the time being and it
  // (technically) shouldn't hurt anything. However, on gonk, die.
  MOZ_ASSERT(!NS_IsMainThread()); // I/O thread
#endif
  nsRefPtr<BluetoothReplyRunnable> replyRunnable =
    dont_AddRef(static_cast< BluetoothReplyRunnable* >(aBluetoothReplyRunnable));

  MOZ_ASSERT(replyRunnable, "Callback reply runnable is null!");

  nsAutoString replyError;
  BluetoothValue v;
  aFunc(aMsg, nullptr, v, replyError);

  // Bug 941462. When blueZ replys 'I/O error', we treat it as 'internal error'.
  // This usually happned when the first pairing request has not yet finished,
  // the second pairing request issued immediately.
  if (replyError.EqualsLiteral("I/O error")) {
    replyError.AssignLiteral(ERR_INTERNAL_ERROR);
  }

  DispatchBluetoothReply(replyRunnable, v, replyError);
}

static void
GetObjectPathCallback(DBusMessage* aMsg, void* aBluetoothReplyRunnable)
{
  if (sIsPairing) {
    RunDBusCallback(aMsg, aBluetoothReplyRunnable,
                    UnpackObjectPathMessage);
    sIsPairing--;
  }
}

static void
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
  aValue = aErrorStr.IsEmpty();
}

static void
GetVoidCallback(DBusMessage* aMsg, void* aBluetoothReplyRunnable)
{
  RunDBusCallback(aMsg, aBluetoothReplyRunnable,
                  UnpackVoidMessage);
}

class ReplyErrorToProfileManager : public nsRunnable
{
public:
  ReplyErrorToProfileManager(BluetoothServiceClass aServiceClass,
                             bool aConnect,
                             const nsAString& aErrorString)
    : mServiceClass(aServiceClass)
    , mConnect(aConnect)
    , mErrorString(aErrorString)
  {
    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread
  }

  nsresult Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    BluetoothProfileManagerBase* profile;
    if (mServiceClass == BluetoothServiceClass::HID) {
      profile = BluetoothHidManager::Get();
    } else if (mServiceClass == BluetoothServiceClass::A2DP) {
      profile = BluetoothA2dpManager::Get();
    } else {
      MOZ_ASSERT(false);
      return NS_ERROR_FAILURE;
    }

    if (mConnect) {
      profile->OnConnect(mErrorString);
    } else {
      profile->OnDisconnect(mErrorString);
    }

    return NS_OK;
  }

private:
  BluetoothServiceClass mServiceClass;
  bool mConnect;
  nsString mErrorString;
};

static void
CheckDBusReply(DBusMessage* aMsg, void* aServiceClass, bool aConnect)
{
  MOZ_ASSERT(!NS_IsMainThread()); // I/O thread

  NS_ENSURE_TRUE_VOID(aMsg);

  BluetoothValue v;
  nsAutoString replyError;
  UnpackVoidMessage(aMsg, nullptr, v, replyError);

  nsAutoPtr<BluetoothServiceClass> serviceClass(
    static_cast<BluetoothServiceClass*>(aServiceClass));

  if (!replyError.IsEmpty()) {
    NS_DispatchToMainThread(
      new ReplyErrorToProfileManager(*serviceClass, aConnect, replyError));
  }
}

static void
InputConnectCallback(DBusMessage* aMsg, void* aParam)
{
  CheckDBusReply(aMsg, aParam, true);
}

static void
InputDisconnectCallback(DBusMessage* aMsg, void* aParam)
{
  CheckDBusReply(aMsg, aParam, false);
}

static void
SinkConnectCallback(DBusMessage* aMsg, void* aParam)
{
  CheckDBusReply(aMsg, aParam, true);
}

static void
SinkDisconnectCallback(DBusMessage* aMsg, void* aParam)
{
  CheckDBusReply(aMsg, aParam, false);
}

static bool
HasAudioService(uint32_t aCodValue)
{
  return ((aCodValue & 0x200000) == 0x200000);
}

static bool
ContainsIcon(const InfallibleTArray<BluetoothNamedValue>& aProperties)
{
  for (uint8_t i = 0; i < aProperties.Length(); i++) {
    if (aProperties[i].name().EqualsLiteral("Icon")) {
      return true;
    }
  }
  return false;
}

static bool
GetProperty(DBusMessageIter aIter, const Properties* aPropertyTypes,
            int aPropertyTypeLen, int* aPropIndex,
            InfallibleTArray<BluetoothNamedValue>& aProperties)
{
  /**
   * Ensure GetProperty runs in critical section otherwise
   * crash due to timing issue occurs when BT is enabled.
   *
   * TODO: Revise GetProperty to solve the crash
   */
  MonitorAutoLock lock(*sGetPropertyMonitor);

  DBusMessageIter prop_val, array_val_iter;
  char* property = nullptr;
  uint32_t array_type;
  int i, expectedType, receivedType;

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
    BT_LOGR("unknown property: %s", property);
    return false;
  }

  nsAutoString propertyName;
  propertyName.AssignASCII(aPropertyTypes[i].name);
  *aPropIndex = i;

  // Preprocessing
  dbus_message_iter_recurse(&aIter, &prop_val);
  expectedType = aPropertyTypes[*aPropIndex].type;
  receivedType = dbus_message_iter_get_arg_type(&prop_val);

  /**
   * Bug 857896. Since device property "Connected" could be a boolean value or
   * an 2-byte array, we need to check the value type here and convert the
   * first byte into a boolean manually.
   */
  bool convert = false;
  if (propertyName.EqualsLiteral("Connected") &&
      receivedType == DBUS_TYPE_ARRAY) {
    MOZ_ASSERT(aPropertyTypes == sDeviceProperties);
    convert = true;
  }

  if ((receivedType != expectedType) && !convert) {
    BT_WARNING("Iterator not type we expect! Property name: %s,"
      "Property Type Expected: %d, Property Type Received: %d",
      NS_ConvertUTF16toUTF8(propertyName).get(), expectedType, receivedType);
    return false;
  }

  // Extract data
  BluetoothValue propertyValue;
  switch (receivedType) {
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
      }
      break;
    default:
      NS_NOTREACHED("Cannot find dbus message type!");
  }

  // Postprocessing
  if (convert) {
    MOZ_ASSERT(propertyValue.type() == BluetoothValue::TArrayOfuint8_t);

    bool b = propertyValue.get_ArrayOfuint8_t()[0];
    propertyValue = BluetoothValue(b);
  } else if (propertyName.EqualsLiteral("Devices")) {
    MOZ_ASSERT(aPropertyTypes == sAdapterProperties);
    MOZ_ASSERT(propertyValue.type() == BluetoothValue::TArrayOfnsString);

    uint32_t length = propertyValue.get_ArrayOfnsString().Length();
    for (uint32_t i= 0; i < length; i++) {
      nsString& data = propertyValue.get_ArrayOfnsString()[i];
      data = GetAddressFromObjectPath(data);
    }
  }

  aProperties.AppendElement(BluetoothNamedValue(propertyName, propertyValue));
  return true;
}

static void
ParseProperties(DBusMessageIter* aIter,
                BluetoothValue& aValue,
                nsAString& aErrorStr,
                const Properties* aPropertyTypes,
                const int aPropertyTypeLen)
{
  DBusMessageIter dict_entry, dict;
  int prop_index = -1;

  MOZ_ASSERT(dbus_message_iter_get_arg_type(aIter) == DBUS_TYPE_ARRAY,
             "Trying to parse a property from sth. that's not an array");

  dbus_message_iter_recurse(aIter, &dict);
  InfallibleTArray<BluetoothNamedValue> props;
  do {
    MOZ_ASSERT(dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY,
               "Trying to parse a property from sth. that's not an dict!");
    dbus_message_iter_recurse(&dict, &dict_entry);

    if (!GetProperty(dict_entry, aPropertyTypes, aPropertyTypeLen, &prop_index,
                     props)) {
      aErrorStr.AssignLiteral("Can't Create Property!");
      BT_WARNING("Can't create property!");
      return;
    }
  } while (dbus_message_iter_next(&dict));

  aValue = props;
}

static bool
UnpackPropertiesMessage(DBusMessage* aMsg, DBusError* aErr,
                        BluetoothValue& aValue, const char* aIface)
{
  MOZ_ASSERT(aMsg);

  const Properties* propertyTypes;
  int propertyTypesLength;

  nsAutoString errorStr;
  if (IsDBusMessageError(aMsg, aErr, errorStr) ||
      dbus_message_get_type(aMsg) != DBUS_MESSAGE_TYPE_METHOD_RETURN) {
    BT_WARNING("dbus message has an error.");
    return false;
  }

  DBusMessageIter iter;
  if (!dbus_message_iter_init(aMsg, &iter)) {
    BT_WARNING("Cannot create dbus message iter!");
    return false;
  }

  if (!strcmp(aIface, DBUS_DEVICE_IFACE)) {
    propertyTypes = sDeviceProperties;
    propertyTypesLength = ArrayLength(sDeviceProperties);
  } else if (!strcmp(aIface, DBUS_ADAPTER_IFACE)) {
    propertyTypes = sAdapterProperties;
    propertyTypesLength = ArrayLength(sAdapterProperties);
  } else if (!strcmp(aIface, DBUS_MANAGER_IFACE)) {
    propertyTypes = sManagerProperties;
    propertyTypesLength = ArrayLength(sManagerProperties);
  } else {
    return false;
  }

  ParseProperties(&iter, aValue, errorStr, propertyTypes,
                  propertyTypesLength);
  return true;
}

static void
ParsePropertyChange(DBusMessage* aMsg, BluetoothValue& aValue,
                    nsAString& aErrorStr, const Properties* aPropertyTypes,
                    const int aPropertyTypeLen)
{
  DBusMessageIter iter;
  DBusError err;
  int prop_index = -1;
  InfallibleTArray<BluetoothNamedValue> props;

  dbus_error_init(&err);
  if (!dbus_message_iter_init(aMsg, &iter)) {
    BT_WARNING("Can't create iterator!");
    return;
  }

  if (!GetProperty(iter, aPropertyTypes, aPropertyTypeLen,
                   &prop_index, props)) {
    BT_WARNING("Can't get property!");
    aErrorStr.AssignLiteral("Can't get property!");
    return;
  }
  aValue = props;
}

class AppendDeviceNameReplyHandler: public DBusReplyHandler
{
public:
  AppendDeviceNameReplyHandler(const nsCString& aIface,
                               const nsString& aDevicePath,
                               const BluetoothSignal& aSignal)
    : mIface(aIface)
    , mDevicePath(aDevicePath)
    , mSignal(aSignal)
  {
    MOZ_ASSERT(!mIface.IsEmpty());
    MOZ_ASSERT(!mDevicePath.IsEmpty());
  }

  void Handle(DBusMessage* aReply) MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread

    if (!aReply || (dbus_message_get_type(aReply) == DBUS_MESSAGE_TYPE_ERROR)) {
      return;
    }

    // Get device properties from result of GetProperties

    DBusError err;
    dbus_error_init(&err);

    BluetoothValue deviceProperties;

    bool success = UnpackPropertiesMessage(aReply, &err, deviceProperties,
                                           mIface.get());
    if (!success) {
      BT_WARNING("Failed to get device properties");
      return;
    }

    // First we replace object path with device address.

    InfallibleTArray<BluetoothNamedValue>& parameters =
      mSignal.value().get_ArrayOfBluetoothNamedValue();
    nsString address =
      GetAddressFromObjectPath(mDevicePath);
    parameters[0].name().AssignLiteral("address");
    parameters[0].value() = address;

    // Then we append the device's name to the original signal's data.

    InfallibleTArray<BluetoothNamedValue>& properties =
      deviceProperties.get_ArrayOfBluetoothNamedValue();
    uint32_t i;
    for (i = 0; i < properties.Length(); i++) {
      if (properties[i].name().EqualsLiteral("Name")) {
        properties[i].name().AssignLiteral("name");
        parameters.AppendElement(properties[i]);
        break;
      }
    }
    MOZ_ASSERT_IF(i == properties.Length(), "failed to get device name");

    nsRefPtr<DistributeBluetoothSignalTask> task =
      new DistributeBluetoothSignalTask(mSignal);
    NS_DispatchToMainThread(task);
  }

private:
  nsCString mIface;
  nsString mDevicePath;
  BluetoothSignal mSignal;
};

static void
AppendDeviceName(BluetoothSignal& aSignal)
{
  MOZ_ASSERT(!NS_IsMainThread()); // I/O thread
  MOZ_ASSERT(sDBusConnection);

  BluetoothValue v = aSignal.value();
  if (v.type() != BluetoothValue::TArrayOfBluetoothNamedValue ||
      v.get_ArrayOfBluetoothNamedValue().Length() == 0) {
    BT_WARNING("Invalid argument type for AppendDeviceNameRunnable");
    return;
  }
  const InfallibleTArray<BluetoothNamedValue>& arr =
    v.get_ArrayOfBluetoothNamedValue();

  // Device object path should be put in the first element
  if (!arr[0].name().EqualsLiteral("path") ||
       arr[0].value().type() != BluetoothValue::TnsString) {
    BT_WARNING("Invalid object path for AppendDeviceNameRunnable");
    return;
  }

  nsString devicePath = arr[0].value().get_nsString();

  nsRefPtr<AppendDeviceNameReplyHandler> handler =
    new AppendDeviceNameReplyHandler(nsCString(DBUS_DEVICE_IFACE),
                                     devicePath, aSignal);

  bool success = sDBusConnection->SendWithReply(
    AppendDeviceNameReplyHandler::Callback, handler.get(), 1000,
    NS_ConvertUTF16toUTF8(devicePath).get(), DBUS_DEVICE_IFACE,
    "GetProperties", DBUS_TYPE_INVALID);

  NS_ENSURE_TRUE_VOID(success);

  unused << handler.forget(); // picked up by callback handler
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

  BT_LOGD("%s: %s, %s", __FUNCTION__,
                       dbus_message_get_path(msg),
                       dbus_message_get_member(msg));

  nsString signalPath = NS_ConvertUTF8toUTF16(dbus_message_get_path(msg));
  nsString signalName = NS_ConvertUTF8toUTF16(dbus_message_get_member(msg));
  nsString errorStr;
  BluetoothValue v;
  InfallibleTArray<BluetoothNamedValue> parameters;
  bool isPairingReq = false;
  BluetoothSignal signal(signalName, signalPath, v);
  char *objectPath;

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
      goto handle_error;
    }

    dbus_connection_send(conn, reply, nullptr);
    dbus_message_unref(reply);
    v = parameters;
  } else if (dbus_message_is_method_call(msg, DBUS_AGENT_IFACE, "Authorize")) {
    // This method gets called when the service daemon needs to authorize a
    // connection/service request.
    const char *uuid;
    if (!dbus_message_get_args(msg, nullptr,
                               DBUS_TYPE_OBJECT_PATH, &objectPath,
                               DBUS_TYPE_STRING, &uuid,
                               DBUS_TYPE_INVALID)) {
      errorStr.AssignLiteral("Invalid arguments for Authorize() method");
      goto handle_error;
    }

    NS_ConvertUTF8toUTF16 uuidStr(uuid);
    BluetoothServiceClass serviceClass =
      BluetoothUuidHelper::GetBluetoothServiceClass(uuidStr);
    if (serviceClass == BluetoothServiceClass::UNKNOWN) {
      errorStr.AssignLiteral("Failed to get service class");
      goto handle_error;
    }

    DBusMessage* reply = nullptr;
    uint32_t i;
    for (i = 0; i < MOZ_ARRAY_LENGTH(sAuthorizedServiceClass); i++) {
      if (serviceClass == sAuthorizedServiceClass[i]) {
        reply = dbus_message_new_method_return(msg);
        break;
      }
    }

    // The uuid isn't authorized
    if (i == MOZ_ARRAY_LENGTH(sAuthorizedServiceClass)) {
      BT_WARNING("Uuid is not authorized.");
      reply = dbus_message_new_error(msg, "org.bluez.Error.Rejected",
                                     "The uuid is not authorized");
    }

    if (!reply) {
      errorStr.AssignLiteral("Memory can't be allocated for the message.");
      goto handle_error;
    }

    dbus_connection_send(conn, reply, nullptr);
    dbus_message_unref(reply);
    return DBUS_HANDLER_RESULT_HANDLED;
  } else if (dbus_message_is_method_call(msg, DBUS_AGENT_IFACE,
                                         "RequestConfirmation")) {
    // This method gets called when the service daemon needs to confirm a
    // passkey for an authentication.
    uint32_t passkey;
    if (!dbus_message_get_args(msg, nullptr,
                               DBUS_TYPE_OBJECT_PATH, &objectPath,
                               DBUS_TYPE_UINT32, &passkey,
                               DBUS_TYPE_INVALID)) {
      errorStr.AssignLiteral("Invalid arguments: RequestConfirmation()");
      goto handle_error;
    }

    parameters.AppendElement(
      BluetoothNamedValue(NS_LITERAL_STRING("path"),
                          NS_ConvertUTF8toUTF16(objectPath)));
    parameters.AppendElement(
      BluetoothNamedValue(NS_LITERAL_STRING("method"),
                          NS_LITERAL_STRING("confirmation")));
    parameters.AppendElement(
      BluetoothNamedValue(NS_LITERAL_STRING("passkey"), passkey));

    v = parameters;
    isPairingReq = true;
  } else if (dbus_message_is_method_call(msg, DBUS_AGENT_IFACE,
                                         "RequestPinCode")) {
    // This method gets called when the service daemon needs to get the passkey
    // for an authentication. The return value should be a string of 1-16
    // characters length. The string can be alphanumeric.
    if (!dbus_message_get_args(msg, nullptr,
                               DBUS_TYPE_OBJECT_PATH, &objectPath,
                               DBUS_TYPE_INVALID)) {
      errorStr.AssignLiteral("Invalid arguments for RequestPinCode() method");
      goto handle_error;
    }

    parameters.AppendElement(
      BluetoothNamedValue(NS_LITERAL_STRING("path"),
                          NS_ConvertUTF8toUTF16(objectPath)));
    parameters.AppendElement(
      BluetoothNamedValue(NS_LITERAL_STRING("method"),
                          NS_LITERAL_STRING("pincode")));

    v = parameters;
    isPairingReq = true;
  } else if (dbus_message_is_method_call(msg, DBUS_AGENT_IFACE,
                                         "RequestPasskey")) {
    // This method gets called when the service daemon needs to get the passkey
    // for an authentication. The return value should be a numeric value
    // between 0-999999.
    if (!dbus_message_get_args(msg, nullptr,
                               DBUS_TYPE_OBJECT_PATH, &objectPath,
                               DBUS_TYPE_INVALID)) {
      errorStr.AssignLiteral("Invalid arguments for RequestPasskey() method");
      goto handle_error;
    }

    parameters.AppendElement(BluetoothNamedValue(
                               NS_LITERAL_STRING("path"),
                               NS_ConvertUTF8toUTF16(objectPath)));
    parameters.AppendElement(BluetoothNamedValue(
                               NS_LITERAL_STRING("method"),
                               NS_LITERAL_STRING("passkey")));

    v = parameters;
    isPairingReq = true;
  } else if (dbus_message_is_method_call(msg, DBUS_AGENT_IFACE, "Release")) {
    // This method gets called when the service daemon unregisters the agent.
    // An agent can use it to do cleanup tasks. There is no need to unregister
    // the agent, because when this method gets called it has already been
    // unregistered.
    DBusMessage *reply = dbus_message_new_method_return(msg);

    if (!reply) {
      errorStr.AssignLiteral("Memory can't be allocated for the message.");
      goto handle_error;
    }

    dbus_connection_send(conn, reply, nullptr);
    dbus_message_unref(reply);

    // Do not send an notification to upper layer, too annoying.
    return DBUS_HANDLER_RESULT_HANDLED;
  } else {
#ifdef DEBUG
    BT_WARNING("agent handler %s: Unhandled event. Ignore.", __FUNCTION__);
#endif
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  if (!errorStr.IsEmpty()) {
    BT_WARNING(NS_ConvertUTF16toUTF8(errorStr).get());
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  // Update value after parsing DBus message
  signal.value() = v;

  if (isPairingReq) {
    sPairingReqTable->Put(
      GetAddressFromObjectPath(NS_ConvertUTF8toUTF16(objectPath)), msg);

    // Increase ref count here because we need this message later.
    // It'll be unrefed when set*Internal() is called.
    dbus_message_ref(msg);

    AppendDeviceName(signal);
  } else {
    NS_DispatchToMainThread(new DistributeBluetoothSignalTask(signal));
  }

  return DBUS_HANDLER_RESULT_HANDLED;

handle_error:
  BT_WARNING(NS_ConvertUTF16toUTF8(errorStr).get());
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

class RegisterAgentReplyHandler : public DBusReplyHandler
{
public:
  RegisterAgentReplyHandler(const DBusObjectPathVTable* aAgentVTable)
    : mAgentVTable(aAgentVTable)
  {
    MOZ_ASSERT(aAgentVTable);
  }

  void Handle(DBusMessage* aReply)
  {
    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread
    MOZ_ASSERT(sDBusConnection);

    if (!aReply || (dbus_message_get_type(aReply) == DBUS_MESSAGE_TYPE_ERROR)) {
      return;
    }

    // There is no "RegisterAgent" function defined in device interface.
    // When we call "CreatePairedDevice", it will do device agent registration
    // for us. (See maemo.org/api_refs/5.0/beta/bluez/adapter.html)
    if (!dbus_connection_register_object_path(sDBusConnection->GetConnection(),
                                              KEY_REMOTE_AGENT,
                                              mAgentVTable,
                                              nullptr)) {
      BT_WARNING("%s: Can't register object path %s for remote device agent!",
                 __FUNCTION__, KEY_REMOTE_AGENT);
      return;
    }

    NS_DispatchToMainThread(new PrepareProfileManagersRunnable());
  }

private:
  const DBusObjectPathVTable* mAgentVTable;
};

class AddReservedServiceRecordsReplyHandler : public DBusReplyHandler
{
public:
  void Handle(DBusMessage* aReply)
  {
    static const DBusObjectPathVTable sAgentVTable = {
      nullptr, AgentEventFilter, nullptr, nullptr, nullptr, nullptr
    };

    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread

    if (!aReply || (dbus_message_get_type(aReply) == DBUS_MESSAGE_TYPE_ERROR)) {
      return;
    }

    // TODO/qdot: This needs to be held for the life of the bluetooth connection
    // so we could clean it up. For right now though, we can throw it away.
    nsTArray<uint32_t> handles;

    ExtractHandles(aReply, handles);

    if(!RegisterAgent(&sAgentVTable)) {
      BT_WARNING("Failed to register agent");
    }
  }

private:
  void ExtractHandles(DBusMessage *aMessage, nsTArray<uint32_t>& aOutHandles)
  {
    DBusError error;
    int length;
    uint32_t* handles = nullptr;

    dbus_error_init(&error);

    bool success = dbus_message_get_args(aMessage, &error,
                                         DBUS_TYPE_ARRAY,
                                         DBUS_TYPE_UINT32,
                                         &handles, &length,
                                         DBUS_TYPE_INVALID);
    if (success != TRUE) {
      LOG_AND_FREE_DBUS_ERROR(&error);
      return;
    }

    if (!handles) {
      BT_WARNING("Null array in extract_handles");
      return;
    }

    for (int i = 0; i < length; ++i) {
      aOutHandles.AppendElement(handles[i]);
    }
  }

  bool RegisterAgent(const DBusObjectPathVTable* aAgentVTable)
  {
    const char* agentPath = KEY_LOCAL_AGENT;
    const char* capabilities = B2G_AGENT_CAPABILITIES;

    MOZ_ASSERT(sDBusConnection);

    // Local agent means agent for Adapter, not agent for Device. Some signals
    // will be passed to local agent, some will be passed to device agent.
    // For example, if a remote device would like to pair with us, then the
    // signal will be passed to local agent. If we start pairing process with
    // calling CreatePairedDevice, we'll get signal which should be passed to
    // device agent.
    if (!dbus_connection_register_object_path(sDBusConnection->GetConnection(),
                                              KEY_LOCAL_AGENT,
                                              aAgentVTable,
                                              nullptr)) {
      BT_WARNING("%s: Can't register object path %s for agent!",
                 __FUNCTION__, KEY_LOCAL_AGENT);
      return false;
    }

    nsRefPtr<RegisterAgentReplyHandler> handler =
      new RegisterAgentReplyHandler(aAgentVTable);
    MOZ_ASSERT(!sAdapterPath.IsEmpty());

    bool success = sDBusConnection->SendWithReply(
      RegisterAgentReplyHandler::Callback, handler.get(), -1,
      NS_ConvertUTF16toUTF8(sAdapterPath).get(),
      DBUS_ADAPTER_IFACE, "RegisterAgent",
      DBUS_TYPE_OBJECT_PATH, &agentPath,
      DBUS_TYPE_STRING, &capabilities,
      DBUS_TYPE_INVALID);

    NS_ENSURE_TRUE(success, false);

    unused << handler.forget(); // picked up by callback handler

    return true;
  }
};

class AddReservedServiceRecordsTask : public Task
{
public:
  AddReservedServiceRecordsTask()
  { }

  void Run()
  {
    static const dbus_uint32_t sServices[] = {
      BluetoothServiceClass::HANDSFREE_AG,
      BluetoothServiceClass::HEADSET_AG,
      BluetoothServiceClass::OBJECT_PUSH
    };

    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread
    MOZ_ASSERT(sDBusConnection);
    MOZ_ASSERT(!sAdapterPath.IsEmpty());

    nsRefPtr<DBusReplyHandler> handler =
      new AddReservedServiceRecordsReplyHandler();

    const dbus_uint32_t* services = sServices;

    bool success = sDBusConnection->SendWithReply(
      DBusReplyHandler::Callback, handler.get(), -1,
      NS_ConvertUTF16toUTF8(sAdapterPath).get(),
      DBUS_ADAPTER_IFACE, "AddReservedServiceRecords",
      DBUS_TYPE_ARRAY, DBUS_TYPE_UINT32,
      &services, ArrayLength(sServices), DBUS_TYPE_INVALID);

    NS_ENSURE_TRUE_VOID(success);

    unused << handler.forget(); /* picked up by callback handler */
  }
};

class PrepareAdapterRunnable : public nsRunnable
{
public:
  PrepareAdapterRunnable()
  { }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    Task* task = new AddReservedServiceRecordsTask();
    DispatchToDBusThread(task);

    return NS_OK;
  }
};

class RequestPlayStatusTask : public nsRunnable
{
public:
  RequestPlayStatusTask()
  {
    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread
  }

  nsresult Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    BluetoothSignal signal(NS_LITERAL_STRING(REQUEST_MEDIA_PLAYSTATUS_ID),
                           NS_LITERAL_STRING(KEY_ADAPTER),
                           InfallibleTArray<BluetoothNamedValue>());

    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);
    bs->DistributeSignal(signal);

    return NS_OK;
  }
};

// Called by dbus during WaitForAndDispatchEventNative()
// This function is called on the IOThread
static DBusHandlerResult
EventFilter(DBusConnection* aConn, DBusMessage* aMsg, void* aData)
{
  // I/O thread
  MOZ_ASSERT(!NS_IsMainThread(), "Shouldn't be called from Main Thread!");

  if (dbus_message_get_type(aMsg) != DBUS_MESSAGE_TYPE_SIGNAL) {
    BT_WARNING("%s: event handler not interested in %s (not a signal).\n",
        __FUNCTION__, dbus_message_get_member(aMsg));
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  if (dbus_message_get_path(aMsg) == nullptr) {
    BT_WARNING("DBusMessage %s has no bluetooth destination, ignoring\n",
               dbus_message_get_member(aMsg));
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  DBusError err;
  dbus_error_init(&err);

  nsAutoString signalPath;
  nsAutoString signalName;
  nsAutoString signalInterface;

  BT_LOGD("%s: %s, %s, %s", __FUNCTION__,
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
      BT_WARNING("Can't create iterator!");
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    const char* addr;
    dbus_message_iter_get_basic(&iter, &addr);

    if (!dbus_message_iter_next(&iter)) {
      errorStr.AssignLiteral("Unexpected message struct in msg DeviceFound");
    } else {
      ParseProperties(&iter,
                      v,
                      errorStr,
                      sDeviceProperties,
                      ArrayLength(sDeviceProperties));

      InfallibleTArray<BluetoothNamedValue>& properties =
        v.get_ArrayOfBluetoothNamedValue();

      // The DBus DeviceFound message actually passes back a key value object
      // with the address as the key and the rest of the device properties as
      // a dict value. After we parse out the properties, we need to go back
      // and add the address to the ipdl dict we've created to make sure we
      // have all of the information to correctly build the device.
      nsAutoString address = NS_ConvertUTF8toUTF16(addr);
      properties.AppendElement(
        BluetoothNamedValue(NS_LITERAL_STRING("Address"), address));
      properties.AppendElement(
        BluetoothNamedValue(NS_LITERAL_STRING("Path"),
                            GetObjectPathFromAddress(signalPath, address)));

      if (!ContainsIcon(properties)) {
        for (uint32_t i = 0; i < properties.Length(); i++) {
          // It is possible that property Icon missed due to CoD of major
          // class is TOY but service class is "Audio", we need to assign
          // Icon as audio-card. This is for PTS test TC_AG_COD_BV_02_I.
          // As HFP specification defined that
          // service class is "Audio" can be considered as HFP AG.
          if (properties[i].name().EqualsLiteral("Class")) {
            if (HasAudioService(properties[i].value().get_uint32_t())) {
              v.get_ArrayOfBluetoothNamedValue().AppendElement(
                BluetoothNamedValue(NS_LITERAL_STRING("Icon"),
                                    NS_LITERAL_STRING("audio-card")));
            }
            break;
          }
        }
      }
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
    ParsePropertyChange(aMsg,
                        v,
                        errorStr,
                        sDeviceProperties,
                        ArrayLength(sDeviceProperties));

    if (v.type() == BluetoothValue::T__None) {
      BT_WARNING("PropertyChanged event couldn't be parsed.");
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    BluetoothNamedValue& property = v.get_ArrayOfBluetoothNamedValue()[0];
    if (property.name().EqualsLiteral("Paired")) {
      // Original approach: Broadcast system message of
      // "bluetooth-pairedstatuschanged" from BluetoothService.
      BluetoothValue newValue(v);
      ToLowerCase(newValue.get_ArrayOfBluetoothNamedValue()[0].name());
      BluetoothSignal signal(NS_LITERAL_STRING(PAIRED_STATUS_CHANGED_ID),
                             NS_LITERAL_STRING(KEY_LOCAL_AGENT),
                             newValue);
      NS_DispatchToMainThread(new DistributeBluetoothSignalTask(signal));

      // New approach: Dispatch event from BluetoothAdapter
      bool status = property.value();
      InfallibleTArray<BluetoothNamedValue> parameters;
      parameters.AppendElement(
        BluetoothNamedValue(NS_LITERAL_STRING("address"), signalPath));
      parameters.AppendElement(
        BluetoothNamedValue(NS_LITERAL_STRING("status"), status));
      signal.path() = NS_LITERAL_STRING(KEY_ADAPTER);
      signal.value() = parameters;
      NS_DispatchToMainThread(new DistributeBluetoothSignalTask(signal));
    } else if (property.name().EqualsLiteral("Connected")) {
      MonitorAutoLock lock(*sStopBluetoothMonitor);

      if (property.value().get_bool()) {
        ++sConnectedDeviceCount;
      } else {
        MOZ_ASSERT(sConnectedDeviceCount > 0);
        if (--sConnectedDeviceCount == 0) {
          lock.Notify();
        }
      }
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
      sAdapterPath = v.get_nsString();
      NS_DispatchToMainThread(new TryFiringAdapterAddedRunnable(true));
      NS_DispatchToMainThread(new PrepareAdapterRunnable());

      /**
       * The adapter name isn't ready for the time being. Wait for the upcoming
       * signal PropertyChanged of adapter name, and then propagate signal
       * AdapterAdded to BluetoothManager.
       */
      return DBUS_HANDLER_RESULT_HANDLED;
    }
  } else if (dbus_message_is_signal(aMsg, DBUS_MANAGER_IFACE,
                                    "PropertyChanged")) {
    ParsePropertyChange(aMsg,
                        v,
                        errorStr,
                        sManagerProperties,
                        ArrayLength(sManagerProperties));
  } else if (dbus_message_is_signal(aMsg, DBUS_SINK_IFACE,
                                    "PropertyChanged")) {
    ParsePropertyChange(aMsg,
                        v,
                        errorStr,
                        sSinkProperties,
                        ArrayLength(sSinkProperties));
  } else if (dbus_message_is_signal(aMsg, DBUS_CTL_IFACE, "GetPlayStatus")) {
    NS_DispatchToMainThread(new RequestPlayStatusTask());
    return DBUS_HANDLER_RESULT_HANDLED;
  } else if (dbus_message_is_signal(aMsg, DBUS_CTL_IFACE, "PropertyChanged")) {
    ParsePropertyChange(aMsg,
                        v,
                        errorStr,
                        sControlProperties,
                        ArrayLength(sControlProperties));
  } else if (dbus_message_is_signal(aMsg, DBUS_INPUT_IFACE,
                                    "PropertyChanged")) {
    ParsePropertyChange(aMsg,
                        v,
                        errorStr,
                        sInputProperties,
                        ArrayLength(sInputProperties));
  } else {
    errorStr = NS_ConvertUTF8toUTF16(dbus_message_get_member(aMsg));
    errorStr.AppendLiteral(" Signal not handled!");
  }

  if (!errorStr.IsEmpty()) {
    BT_WARNING(NS_ConvertUTF16toUTF8(errorStr).get());
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  BluetoothSignal signal(signalName, signalPath, v);
  nsRefPtr<nsRunnable> task;
  if (signalInterface.EqualsLiteral(DBUS_SINK_IFACE)) {
    task = new SinkPropertyChangedHandler(signal);
  } else if (signalInterface.EqualsLiteral(DBUS_CTL_IFACE)) {
    task = new ControlPropertyChangedHandler(signal);
  } else if (signalInterface.EqualsLiteral(DBUS_INPUT_IFACE)) {
    task = new InputPropertyChangedHandler(signal);
  } else {
    task = new DistributeBluetoothSignalTask(signal);
  }

  NS_DispatchToMainThread(task);

  return DBUS_HANDLER_RESULT_HANDLED;
}

static void
OnDefaultAdapterReply(DBusMessage* aReply, void* aData)
{
  MOZ_ASSERT(!NS_IsMainThread()); // I/O thread

  if (!aReply || dbus_message_is_error(aReply, DBUS_ERROR_TIMEOUT)) {
    return;
  }

  DBusError err;
  dbus_error_init(&err);

  BluetoothValue v;
  nsAutoString errorString;

  UnpackObjectPathMessage(aReply, &err, v, errorString);

  if (!errorString.IsEmpty()) {
    return;
  }

  sAdapterPath = v.get_nsString();

  nsRefPtr<PrepareAdapterRunnable> b = new PrepareAdapterRunnable();
  if (NS_FAILED(NS_DispatchToMainThread(b))) {
    BT_WARNING("Failed to dispatch to main thread!");
  }
}

bool
BluetoothDBusService::IsReady()
{
  if (!IsEnabled() || !sDBusConnection || IsToggling()) {
    BT_WARNING("Bluetooth service is not ready yet!");
    return false;
  }
  return true;
}

class StartDBusConnectionTask : public Task
{
public:
  StartDBusConnectionTask(RawDBusConnection* aConnection)
  : mConnection(aConnection)
  {
    MOZ_ASSERT(mConnection);
  }

  void Run()
  {
    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread

    if (sDBusConnection) {
      BT_WARNING("DBus connection has already been established.");
      nsRefPtr<nsRunnable> runnable = new BluetoothService::ToggleBtAck(true);
      if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
        BT_WARNING("Failed to dispatch to main thread!");
      }
      return;
    }

    // Add a filter for all incoming messages_base
    if (!dbus_connection_add_filter(mConnection->GetConnection(),
                                    EventFilter, nullptr, nullptr)) {
      BT_WARNING("Cannot create DBus Event Filter for DBus Thread!");
      nsRefPtr<nsRunnable> runnable = new BluetoothService::ToggleBtAck(false);
      if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
        BT_WARNING("Failed to dispatch to main thread!");
      }
      return;
    }

    mConnection->Watch();

    if (!sPairingReqTable) {
      sPairingReqTable = new nsDataHashtable<nsStringHashKey, DBusMessage* >;
    }

    sDBusConnection = mConnection.forget();

    nsRefPtr<nsRunnable> runnable =
      new BluetoothService::ToggleBtAck(true);
    if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
      BT_WARNING("Failed to dispatch to main thread!");
      return;
    }

    /* Normally we'll receive the signal 'AdapterAdded' with the adapter object
     * path from the DBus daemon during start up. So, there's no need to query
     * the object path of default adapter here. However, if we restart from a
     * crash, the default adapter might already be available, so we ask the daemon
     * explicitly here.
     */
    if (sAdapterPath.IsEmpty()) {
      bool success = sDBusConnection->SendWithReply(OnDefaultAdapterReply, nullptr,
                                                    1000, "/",
                                                    DBUS_MANAGER_IFACE,
                                                    "DefaultAdapter",
                                                    DBUS_TYPE_INVALID);
      if (!success) {
        BT_WARNING("Failed to query default adapter!");
      }
    }
  }

private:
  nsAutoPtr<RawDBusConnection> mConnection;
};

class StartBluetoothRunnable MOZ_FINAL : public nsRunnable
{
public:
  NS_IMETHOD Run()
  {
    // This could block. It should never be run on the main thread.
    MOZ_ASSERT(!NS_IsMainThread()); // BT thread

#ifdef MOZ_WIDGET_GONK
    if (!sBluedroid.Enable()) {
      BT_WARNING("Bluetooth not available.");
      nsRefPtr<nsRunnable> runnable = new BluetoothService::ToggleBtAck(false);
      if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
        BT_WARNING("Failed to dispatch to main thread!");
      }
      return NS_ERROR_FAILURE;
    }
#endif

    RawDBusConnection* connection = new RawDBusConnection();
    nsresult rv = connection->EstablishDBusConnection();
    if (NS_FAILED(rv)) {
      BT_WARNING("Failed to establish connection to BlueZ daemon");
      nsRefPtr<nsRunnable> runnable = new BluetoothService::ToggleBtAck(false);
      if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
        BT_WARNING("Failed to dispatch to main thread!");
      }
      return NS_ERROR_FAILURE;
    }

    DBusError err;
    dbus_error_init(&err);

    // Set which messages will be processed by this dbus connection.
    // Since we are maintaining a single thread for all the DBus bluez
    // signals we want, register all of them in this thread at startup.
    // The event handler will sort the destinations out as needed. The
    // call to dbus_bus_add_match has to run on the BT thread because
    // it can block.
    for (uint32_t i = 0; i < ArrayLength(sBluetoothDBusSignals); ++i) {
      dbus_bus_add_match(connection->GetConnection(),
                         sBluetoothDBusSignals[i],
                         &err);
      if (dbus_error_is_set(&err)) {
        LOG_AND_FREE_DBUS_ERROR(&err);
      }
    }

    Task* task = new StartDBusConnectionTask(connection);
    DispatchToDBusThread(task);

    return NS_OK;
  }
};

nsresult
BluetoothDBusService::StartInternal()
{
  nsRefPtr<nsRunnable> runnable = new StartBluetoothRunnable();
  nsresult rv = DispatchToBtThread(runnable);
  if (NS_FAILED(rv)) {
    BT_WARNING("Failed to dispatch to BT thread!");
  }
  return rv;
}

class DisableBluetoothRunnable MOZ_FINAL : public nsRunnable
{
public:
  NS_IMETHOD Run()
  {
    if (NS_IsMainThread()) {
      // Forward this runnable to BT thread
      return DispatchToBtThread(this);
    }

#ifdef MOZ_WIDGET_GONK
    MOZ_ASSERT(sBluedroid.IsEnabled());
    // Disable() return true on success, so we need to invert it
    bool isEnabled = !sBluedroid.Disable();
#else
    bool isEnabled = false;
#endif

    nsRefPtr<nsRunnable> runnable =
      new BluetoothService::ToggleBtAck(isEnabled);
    nsresult rv = NS_DispatchToMainThread(runnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("Failed to dispatch to main thread!");
    }
    return rv;
  }
};

class DeleteDBusConnectionTask MOZ_FINAL : public Task
{
public:
  DeleteDBusConnectionTask()
  { }

  void Run()
  {
    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread

    if (!sDBusConnection) {
      BT_WARNING("DBus connection has not been established.");
      nsRefPtr<nsRunnable> runnable = new BluetoothService::ToggleBtAck(false);
      if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
        BT_WARNING("Failed to dispatch to main thread!");
      }
      return;
    }

    for (uint32_t i = 0; i < ArrayLength(sBluetoothDBusSignals); ++i) {
      dbus_bus_remove_match(sDBusConnection->GetConnection(),
                            sBluetoothDBusSignals[i], NULL);
    }

    dbus_connection_remove_filter(sDBusConnection->GetConnection(),
                                  EventFilter, nullptr);

    if (!dbus_connection_unregister_object_path(sDBusConnection->GetConnection(),
                                                KEY_LOCAL_AGENT)) {
      BT_WARNING("%s: Can't unregister object path %s for agent!",
          __FUNCTION__, KEY_LOCAL_AGENT);
    }

    if (!dbus_connection_unregister_object_path(sDBusConnection->GetConnection(),
                                                KEY_REMOTE_AGENT)) {
      BT_WARNING("%s: Can't unregister object path %s for agent!",
          __FUNCTION__, KEY_REMOTE_AGENT);
    }

    // unref stored DBusMessages before clearing the hashtable
    sPairingReqTable->EnumerateRead(UnrefDBusMessage, nullptr);
    sPairingReqTable->Clear();

    sIsPairing = 0;
    sConnectedDeviceCount = 0;

    sControllerArray.Clear();

    // This command closes the DBus connection and all its instances
    // of DBusWatch will be removed and free'd.
    sDBusConnection = nullptr;

    // We can only dispatch to the BT thread if we're on the main
    // thread. Thus we dispatch our runnable to the main thread
    // from where it will forward itself to the BT thread.
    nsRefPtr<nsRunnable> runnable = new DisableBluetoothRunnable();
    if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
      BT_WARNING("Failed to dispatch to BT thread!");
    }
  }

private:
  static PLDHashOperator
  UnrefDBusMessage(const nsAString& key, DBusMessage* value, void* arg)
  {
    dbus_message_unref(value);
    return PL_DHASH_NEXT;
  }
};

class StopBluetoothRunnable MOZ_FINAL : public nsRunnable
{
public:
  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread()); // BT thread

    // This could block. It should never be run on the main thread.
    MonitorAutoLock lock(*sStopBluetoothMonitor);
    if (sConnectedDeviceCount > 0) {
      lock.Wait(PR_SecondsToInterval(TIMEOUT_FORCE_TO_DISABLE_BT));
    }

    DispatchToDBusThread(new DeleteDBusConnectionTask());

    return NS_OK;
  }
};

nsresult
BluetoothDBusService::StopInternal()
{
  nsRefPtr<nsRunnable> runnable = new StopBluetoothRunnable();
  nsresult rv = DispatchToBtThread(runnable);
  if (NS_FAILED(rv)) {
    BT_WARNING("Failed to dispatch to BT thread!");
  }
  return rv;
}

class DefaultAdapterPathReplyHandler : public DBusReplyHandler
{
public:
  DefaultAdapterPathReplyHandler(BluetoothReplyRunnable* aRunnable)
    : mRunnable(aRunnable)
  {
    MOZ_ASSERT(mRunnable);
  }

  void Handle(DBusMessage* aReply) MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread

    if (!aReply || (dbus_message_get_type(aReply) == DBUS_MESSAGE_TYPE_ERROR)) {
      const char* errStr = "Timeout in DefaultAdapterPathReplyHandler";
      if (aReply) {
        errStr = dbus_message_get_error_name(aReply);
        if (!errStr) {
          errStr = "Bluetooth DBus Error";
        }
      }
      DispatchBluetoothReply(mRunnable, BluetoothValue(),
                             NS_ConvertUTF8toUTF16(errStr));
      return;
    }

    bool success;
    nsAutoString replyError;

    if (mAdapterPath.IsEmpty()) {
      success = HandleDefaultAdapterPathReply(aReply, replyError);
    } else {
      success = HandleGetPropertiesReply(aReply, replyError);
    }

    if (!success) {
      DispatchBluetoothReply(mRunnable, BluetoothValue(), replyError);
    }
  }

protected:
  bool HandleDefaultAdapterPathReply(DBusMessage* aReply,
                                     nsAString& aReplyError)
  {
    BluetoothValue value;
    DBusError error;
    dbus_error_init(&error);

    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread
    MOZ_ASSERT(sDBusConnection);

    UnpackObjectPathMessage(aReply, &error, value, aReplyError);

    if (!aReplyError.IsEmpty()) {
      return false;
    }

    mAdapterPath = value.get_nsString();

    // Acquire another reference to this reply handler
    nsRefPtr<DefaultAdapterPathReplyHandler> handler = this;

    bool success = sDBusConnection->SendWithReply(
      DefaultAdapterPathReplyHandler::Callback, handler.get(), 1000,
      NS_ConvertUTF16toUTF8(mAdapterPath).get(),
      DBUS_ADAPTER_IFACE, "GetProperties", DBUS_TYPE_INVALID);

    if (!success) {
      aReplyError = NS_LITERAL_STRING("SendWithReply failed");
      return false;
    }

    unused << handler.forget(); // picked up by callback handler

    return true;
  }

  bool HandleGetPropertiesReply(DBusMessage* aReply,
                                nsAutoString& aReplyError)
  {
    BluetoothValue value;
    DBusError error;
    dbus_error_init(&error);

    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread

    bool success = UnpackPropertiesMessage(aReply, &error, value,
                                           DBUS_ADAPTER_IFACE);
    if (!success) {
      aReplyError = NS_ConvertUTF8toUTF16(error.message);
      return false;
    }

    // We have to manually attach the path to the rest of the elements
    value.get_ArrayOfBluetoothNamedValue().AppendElement(
      BluetoothNamedValue(NS_LITERAL_STRING("Path"), mAdapterPath));

    // Dispatch result
    DispatchBluetoothReply(mRunnable, value, aReplyError);

    return true;
  }

private:
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
  nsString mAdapterPath;
};

class DefaultAdapterTask : public Task
{
public:
  DefaultAdapterTask(BluetoothReplyRunnable* aRunnable)
    : mRunnable(aRunnable)
  {
    MOZ_ASSERT(mRunnable);
  }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread
    MOZ_ASSERT(sDBusConnection);

    nsRefPtr<DefaultAdapterPathReplyHandler> handler =
      new DefaultAdapterPathReplyHandler(mRunnable);

    bool success = sDBusConnection->SendWithReply(
      DefaultAdapterPathReplyHandler::Callback,
      handler.get(), 1000,
      "/", DBUS_MANAGER_IFACE, "DefaultAdapter",
      DBUS_TYPE_INVALID);
    NS_ENSURE_TRUE_VOID(success);

    unused << handler.forget(); // picked up by callback handler
  }

private:
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
};

nsresult
BluetoothDBusService::GetDefaultAdapterPathInternal(
                                              BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!IsReady()) {
    NS_NAMED_LITERAL_STRING(errorStr, "Bluetooth service is not ready yet!");
    DispatchBluetoothReply(aRunnable, BluetoothValue(), errorStr);
    return NS_OK;
  }

  Task* task = new DefaultAdapterTask(aRunnable);
  DispatchToDBusThread(task);

  return NS_OK;
}

static void
OnSendDiscoveryMessageReply(DBusMessage *aReply, void *aData)
{
  MOZ_ASSERT(!NS_IsMainThread()); // I/O thread

  nsAutoString errorStr;

  if (!aReply) {
    errorStr.AssignLiteral("SendDiscovery failed");
  }

  nsRefPtr<BluetoothReplyRunnable> runnable =
    dont_AddRef<BluetoothReplyRunnable>(static_cast<BluetoothReplyRunnable*>(aData));

  DispatchBluetoothReply(runnable.get(), BluetoothValue(true), errorStr);
}

class SendDiscoveryMessageTask : public Task
{
public:
  SendDiscoveryMessageTask(const char* aMessageName,
                           BluetoothReplyRunnable* aRunnable)
    : mMessageName(aMessageName)
    , mRunnable(aRunnable)
  {
    MOZ_ASSERT(!mMessageName.IsEmpty());
    MOZ_ASSERT(mRunnable);
  }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread
    MOZ_ASSERT(sDBusConnection);
    MOZ_ASSERT(!sAdapterPath.IsEmpty());

    bool success = sDBusConnection->SendWithReply(
      OnSendDiscoveryMessageReply,
      static_cast<void*>(mRunnable.get()), -1,
      NS_ConvertUTF16toUTF8(sAdapterPath).get(),
      DBUS_ADAPTER_IFACE, mMessageName.get(),
      DBUS_TYPE_INVALID);
    NS_ENSURE_TRUE_VOID(success);

    unused << mRunnable.forget(); // picked up by callback handler
  }

private:
  const nsCString mMessageName;
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
};

nsresult
BluetoothDBusService::SendDiscoveryMessage(const char* aMessageName,
                                           BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sAdapterPath.IsEmpty());

  if (!IsReady()) {
    NS_NAMED_LITERAL_STRING(errorStr, "Bluetooth service is not ready yet!");
    DispatchBluetoothReply(aRunnable, BluetoothValue(), errorStr);
    return NS_OK;
  }

  Task* task = new SendDiscoveryMessageTask(aMessageName, aRunnable);
  DispatchToDBusThread(task);

  return NS_OK;
}

nsresult
BluetoothDBusService::SendInputMessage(const nsAString& aDeviceAddress,
                                       const nsAString& aMessage)
{
  DBusReplyCallback callback;
  if (aMessage.EqualsLiteral("Connect")) {
    callback = InputConnectCallback;
  } else if (aMessage.EqualsLiteral("Disconnect")) {
    callback = InputDisconnectCallback;
  } else {
    MOZ_ASSERT(false);
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(!sAdapterPath.IsEmpty());
  nsString objectPath = GetObjectPathFromAddress(sAdapterPath, aDeviceAddress);
  return SendAsyncDBusMessage(objectPath, DBUS_INPUT_IFACE, aMessage, callback);
}

class SendAsyncDBusMessageTask : public Task
{
public:
  SendAsyncDBusMessageTask(DBusReplyCallback aCallback,
                           BluetoothServiceClass* aServiceClass,
                           const nsACString& aObjectPath,
                           const char* aInterface,
                           const nsACString& aMessage)
    : mCallback(aCallback)
    , mServiceClass(aServiceClass)
    , mObjectPath(aObjectPath)
    , mInterface(aInterface)
    , mMessage(aMessage)
  {
    MOZ_ASSERT(mServiceClass);
    MOZ_ASSERT(!mObjectPath.IsEmpty());
    MOZ_ASSERT(!mInterface.IsEmpty());
    MOZ_ASSERT(!mMessage.IsEmpty());
  }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread
    MOZ_ASSERT(sDBusConnection);

    bool success = sDBusConnection->SendWithReply(
      mCallback, static_cast<void*>(mServiceClass), -1,
      mObjectPath.get(), mInterface.get(), mMessage.get(),
      DBUS_TYPE_INVALID);
    NS_ENSURE_TRUE_VOID(success);

    mServiceClass.forget();
  }

private:
  DBusReplyCallback mCallback;
  nsAutoPtr<BluetoothServiceClass> mServiceClass;
  const nsCString mObjectPath;
  const nsCString mInterface;
  const nsCString mMessage;
};

nsresult
BluetoothDBusService::SendAsyncDBusMessage(const nsAString& aObjectPath,
                                           const char* aInterface,
                                           const nsAString& aMessage,
                                           DBusReplyCallback aCallback)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(IsEnabled());
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(!aObjectPath.IsEmpty());
  MOZ_ASSERT(aInterface);

  nsAutoPtr<BluetoothServiceClass> serviceClass(new BluetoothServiceClass());
  if (!strcmp(aInterface, DBUS_SINK_IFACE)) {
    *serviceClass = BluetoothServiceClass::A2DP;
  } else if (!strcmp(aInterface, DBUS_INPUT_IFACE)) {
    *serviceClass = BluetoothServiceClass::HID;
  } else {
    MOZ_ASSERT(false);
    return NS_ERROR_FAILURE;
  }

  Task* task = new SendAsyncDBusMessageTask(aCallback,
                                            serviceClass.forget(),
                                            NS_ConvertUTF16toUTF8(aObjectPath),
                                            aInterface,
                                            NS_ConvertUTF16toUTF8(aMessage));
  DispatchToDBusThread(task);

  return NS_OK;
}

nsresult
BluetoothDBusService::SendSinkMessage(const nsAString& aDeviceAddress,
                                      const nsAString& aMessage)
{
  DBusReplyCallback callback;
  if (aMessage.EqualsLiteral("Connect")) {
    callback = SinkConnectCallback;
  } else if (aMessage.EqualsLiteral("Disconnect")) {
    callback = SinkDisconnectCallback;
  } else {
    MOZ_ASSERT(false);
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(!sAdapterPath.IsEmpty());
  nsString objectPath = GetObjectPathFromAddress(sAdapterPath, aDeviceAddress);
  return SendAsyncDBusMessage(objectPath, DBUS_SINK_IFACE, aMessage, callback);
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

class BluetoothArrayOfDevicePropertiesReplyHandler : public DBusReplyHandler
{
public:
  BluetoothArrayOfDevicePropertiesReplyHandler(
    const nsTArray<nsString>& aDeviceAddresses,
    const FilterFunc aFilterFunc, BluetoothReplyRunnable* aRunnable)
    : mDeviceAddresses(aDeviceAddresses)
    , mProcessedDeviceAddresses(0)
    , mFilterFunc(aFilterFunc)
    , mRunnable(aRunnable)
    , mValues(InfallibleTArray<BluetoothNamedValue>())
  {
    MOZ_ASSERT(mRunnable);
  }

  void Handle(DBusMessage* aReply) MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread
    MOZ_ASSERT(!sAdapterPath.IsEmpty());
    MOZ_ASSERT(!mObjectPath.IsEmpty());
    MOZ_ASSERT(mProcessedDeviceAddresses < mDeviceAddresses.Length());

    const nsTArray<nsString>::index_type i = mProcessedDeviceAddresses++;

    if (!aReply ||
        (dbus_message_get_type(aReply) == DBUS_MESSAGE_TYPE_ERROR)) {
      BT_WARNING("Invalid DBus message");
      ProcessRemainingDeviceAddresses();
      return;
    }

    // Get device properties from result of GetProperties

    DBusError err;
    dbus_error_init(&err);

    BluetoothValue deviceProperties;

    bool success = UnpackPropertiesMessage(aReply, &err, deviceProperties,
                                           DBUS_DEVICE_IFACE);
    if (!success) {
      BT_WARNING("Failed to get device properties");
      ProcessRemainingDeviceAddresses();
      return;
    }

    InfallibleTArray<BluetoothNamedValue>& devicePropertiesArray =
      deviceProperties.get_ArrayOfBluetoothNamedValue();

    // We have to manually attach the path to the rest of the elements
    devicePropertiesArray.AppendElement(
      BluetoothNamedValue(NS_LITERAL_STRING("Path"), mObjectPath));

    // It is possible that property Icon missed due to CoD of major
    // class is TOY but service class is "Audio", we need to assign
    // Icon as audio-card. This is for PTS test TC_AG_COD_BV_02_I.
    // As HFP specification defined that
    // service class is "Audio" can be considered as HFP AG.
    if (!ContainsIcon(devicePropertiesArray)) {
      for (uint32_t j = 0; j < devicePropertiesArray.Length(); ++j) {
        BluetoothNamedValue& deviceProperty = devicePropertiesArray[j];
        if (deviceProperty.name().EqualsLiteral("Class")) {
          if (HasAudioService(deviceProperty.value().get_uint32_t())) {
            devicePropertiesArray.AppendElement(
              BluetoothNamedValue(NS_LITERAL_STRING("Icon"),
                                  NS_LITERAL_STRING("audio-card")));
          }
          break;
        }
      }
    }

    if (mFilterFunc(deviceProperties)) {
      mValues.get_ArrayOfBluetoothNamedValue().AppendElement(
        BluetoothNamedValue(mDeviceAddresses[i], deviceProperties));
    }

    ProcessRemainingDeviceAddresses();
  }

  void ProcessRemainingDeviceAddresses()
  {
    if (mProcessedDeviceAddresses < mDeviceAddresses.Length()) {
      if (!SendNextGetProperties()) {
        DispatchBluetoothReply(mRunnable, BluetoothValue(),
                               NS_LITERAL_STRING(
                                 "SendNextGetProperties failed"));
      }
    } else {
      // Send resulting device properties
      DispatchBluetoothReply(mRunnable, mValues, EmptyString());
    }
  }

protected:
  bool SendNextGetProperties()
  {
    MOZ_ASSERT(mProcessedDeviceAddresses < mDeviceAddresses.Length());
    MOZ_ASSERT(!sAdapterPath.IsEmpty());
    MOZ_ASSERT(sDBusConnection);

    // cache object path for reply
    mObjectPath = GetObjectPathFromAddress(sAdapterPath,
      mDeviceAddresses[mProcessedDeviceAddresses]);

    nsRefPtr<BluetoothArrayOfDevicePropertiesReplyHandler> handler = this;

    bool success = sDBusConnection->SendWithReply(
      BluetoothArrayOfDevicePropertiesReplyHandler::Callback,
      handler.get(), 1000,
      NS_ConvertUTF16toUTF8(mObjectPath).get(),
      DBUS_DEVICE_IFACE, "GetProperties",
      DBUS_TYPE_INVALID);

    NS_ENSURE_TRUE(success, false);

    unused << handler.forget(); // picked up by callback handler

    return true;
  }

private:
  nsString mObjectPath;
  const nsTArray<nsString> mDeviceAddresses;
  nsTArray<nsString>::size_type mProcessedDeviceAddresses;
  const FilterFunc mFilterFunc;
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
  BluetoothValue mValues;
};

class ProcessRemainingDeviceAddressesTask : public Task
{
public:
  ProcessRemainingDeviceAddressesTask(
    BluetoothArrayOfDevicePropertiesReplyHandler* aHandler,
    BluetoothReplyRunnable* aRunnable)
    : mHandler(aHandler)
    , mRunnable(aRunnable)
  {
    MOZ_ASSERT(mHandler);
    MOZ_ASSERT(mRunnable);
  }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread

    mHandler->ProcessRemainingDeviceAddresses();
  }

private:
  nsRefPtr<BluetoothArrayOfDevicePropertiesReplyHandler> mHandler;
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
};

nsresult
BluetoothDBusService::GetConnectedDevicePropertiesInternal(uint16_t aServiceUuid,
                                              BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoString errorStr;
  BluetoothValue values = InfallibleTArray<BluetoothNamedValue>();
  if (!IsReady()) {
    NS_NAMED_LITERAL_STRING(errorStr, "Bluetooth service is not ready yet!");
    DispatchBluetoothReply(aRunnable, BluetoothValue(), errorStr);
    return NS_OK;
  }

  nsTArray<nsString> deviceAddresses;
  BluetoothProfileManagerBase* profile =
    BluetoothUuidHelper::GetBluetoothProfileManager(aServiceUuid);
  if (!profile) {
    DispatchBluetoothReply(aRunnable, values,
                           NS_LITERAL_STRING(ERR_UNKNOWN_PROFILE));
    return NS_OK;
  }

  if (profile->IsConnected()) {
    nsString address;
    profile->GetAddress(address);
    deviceAddresses.AppendElement(address);
  }

  BluetoothArrayOfDevicePropertiesReplyHandler* handler =
    new BluetoothArrayOfDevicePropertiesReplyHandler(deviceAddresses,
                                                     GetConnectedDevicesFilter,
                                                     aRunnable);
  Task* task = new ProcessRemainingDeviceAddressesTask(handler, aRunnable);
  DispatchToDBusThread(task);

  return NS_OK;
}

nsresult
BluetoothDBusService::GetPairedDevicePropertiesInternal(
                                     const nsTArray<nsString>& aDeviceAddresses,
                                     BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!IsReady()) {
    NS_NAMED_LITERAL_STRING(errorStr, "Bluetooth service is not ready yet!");
    DispatchBluetoothReply(aRunnable, BluetoothValue(), errorStr);
    return NS_OK;
  }

  BluetoothArrayOfDevicePropertiesReplyHandler* handler =
    new BluetoothArrayOfDevicePropertiesReplyHandler(aDeviceAddresses,
                                                     GetPairedDevicesFilter,
                                                     aRunnable);
  Task* task = new ProcessRemainingDeviceAddressesTask(handler, aRunnable);
  DispatchToDBusThread(task);

  return NS_OK;
}

class SetPropertyTask : public Task
{
public:
  SetPropertyTask(BluetoothObjectType aType,
                  const nsACString& aName,
                  BluetoothReplyRunnable* aRunnable)
    : mType(aType)
    , mName(aName)
    , mRunnable(aRunnable)
  {
    MOZ_ASSERT(mRunnable);
  }

  void Send(unsigned int aType, const void* aValue)
  {
    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread
    MOZ_ASSERT(sDBusConnection);
    MOZ_ASSERT(!sAdapterPath.IsEmpty());

    DBusMessage* msg =
      dbus_message_new_method_call("org.bluez",
                                   NS_ConvertUTF16toUTF8(sAdapterPath).get(),
                                   sBluetoothDBusIfaces[mType],
                                   "SetProperty");
    if (!msg) {
      BT_WARNING("Could not allocate D-Bus message object!");
      return;
    }

    const char* name = mName.get();
    if (!dbus_message_append_args(msg, DBUS_TYPE_STRING, &name,
                                  DBUS_TYPE_INVALID)) {
      BT_WARNING("Couldn't append arguments to dbus message!");
      return;
    }

    DBusMessageIter value_iter, iter;
    dbus_message_iter_init_append(msg, &iter);
    char var_type[2] = {(char)aType, '\0'};
    if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT,
                                          var_type, &value_iter) ||
        !dbus_message_iter_append_basic(&value_iter, aType, aValue) ||
        !dbus_message_iter_close_container(&iter, &value_iter)) {
      BT_WARNING("Could not append argument to method call!");
      dbus_message_unref(msg);
      return;
    }

    // msg is unref'd as part of SendWithReply
    bool success = sDBusConnection->SendWithReply(
      GetVoidCallback,
      static_cast<void*>(mRunnable),
      1000, msg);
    NS_ENSURE_TRUE_VOID(success);

    unused << mRunnable.forget(); // picked up by callback handler
  }

private:
  BluetoothObjectType mType;
  const nsCString mName;
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
};

class SetUInt32PropertyTask : public SetPropertyTask
{
public:
  SetUInt32PropertyTask(BluetoothObjectType aType,
                        const nsACString& aName,
                        uint32_t aValue,
                        BluetoothReplyRunnable* aRunnable)
    : SetPropertyTask(aType, aName, aRunnable)
    , mValue(aValue)
  { }

  void Run() MOZ_OVERRIDE
  {
    Send(DBUS_TYPE_UINT32, &mValue);
  }

private:
  dbus_uint32_t mValue;
};

class SetStringPropertyTask : public SetPropertyTask
{
public:
  SetStringPropertyTask(BluetoothObjectType aType,
                        const nsACString& aName,
                        const nsACString& aValue,
                        BluetoothReplyRunnable* aRunnable)
    : SetPropertyTask(aType, aName, aRunnable)
    , mValue(aValue)
  { }

  void Run() MOZ_OVERRIDE
  {
    const char* value = mValue.get();
    Send(DBUS_TYPE_STRING, &value);
  }

private:
  const nsCString mValue;
};

class SetBooleanPropertyTask : public SetPropertyTask
{
public:
  SetBooleanPropertyTask(BluetoothObjectType aType,
                         const nsACString& aName,
                         dbus_bool_t aValue,
                         BluetoothReplyRunnable* aRunnable)
    : SetPropertyTask(aType, aName, aRunnable)
    , mValue(aValue)
  {
  }

  void Run() MOZ_OVERRIDE
  {
    Send(DBUS_TYPE_BOOLEAN, &mValue);
  }

private:
  dbus_bool_t mValue;
};

nsresult
BluetoothDBusService::SetProperty(BluetoothObjectType aType,
                                  const BluetoothNamedValue& aValue,
                                  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!IsReady()) {
    NS_NAMED_LITERAL_STRING(errorStr, "Bluetooth service is not ready yet!");
    DispatchBluetoothReply(aRunnable, BluetoothValue(), errorStr);
    return NS_OK;
  }

  Task* task;

  if (aValue.value().type() == BluetoothValue::Tuint32_t) {
    task = new SetUInt32PropertyTask(aType,
      NS_ConvertUTF16toUTF8(aValue.name()),
      aValue.value().get_uint32_t(), aRunnable);
  } else if (aValue.value().type() == BluetoothValue::TnsString) {
    task = new SetStringPropertyTask(aType,
      NS_ConvertUTF16toUTF8(aValue.name()),
      NS_ConvertUTF16toUTF8(aValue.value().get_nsString()), aRunnable);
  } else if (aValue.value().type() == BluetoothValue::Tbool) {
    task = new SetBooleanPropertyTask(aType,
      NS_ConvertUTF16toUTF8(aValue.name()),
      aValue.value().get_bool(), aRunnable);
  } else {
    BT_WARNING("Property type not handled!");
    return NS_ERROR_FAILURE;
  }
  DispatchToDBusThread(task);

  return NS_OK;
}

class CreatePairedDeviceInternalTask : public Task
{
public:
  CreatePairedDeviceInternalTask(const nsACString& aDeviceAddress,
                                 int aTimeout,
                                 BluetoothReplyRunnable* aRunnable)
    : mDeviceAddress(aDeviceAddress)
    , mTimeout(aTimeout)
    , mRunnable(aRunnable)
  {
    MOZ_ASSERT(!mDeviceAddress.IsEmpty());
    MOZ_ASSERT(mRunnable);
  }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread
    MOZ_ASSERT(sDBusConnection);
    MOZ_ASSERT(!sAdapterPath.IsEmpty());

    const char *deviceAddress = mDeviceAddress.get();
    const char *deviceAgentPath = KEY_REMOTE_AGENT;
    const char *capabilities = B2G_AGENT_CAPABILITIES;

    // Then send CreatePairedDevice, it will register a temp device agent then
    // unregister it after pairing process is over
    bool success = sDBusConnection->SendWithReply(
      GetObjectPathCallback, static_cast<void*>(mRunnable), mTimeout,
      NS_ConvertUTF16toUTF8(sAdapterPath).get(),
      DBUS_ADAPTER_IFACE,
      "CreatePairedDevice",
      DBUS_TYPE_STRING, &deviceAddress,
      DBUS_TYPE_OBJECT_PATH, &deviceAgentPath,
      DBUS_TYPE_STRING, &capabilities,
      DBUS_TYPE_INVALID);
    NS_ENSURE_TRUE_VOID(success);

    unused << mRunnable.forget(); // picked up by callback handler

    /**
     * FIXME: Bug 820274
     *
     * If the user turns off Bluetooth in the middle of pairing process,
     * the callback function GetObjectPathCallback may still be called
     * while enabling next time by dbus daemon. To prevent this from
     * happening, added a flag to distinguish if Bluetooth has been
     * turned off. Nevertheless, we need a check if there is a better
     * solution.
     *
     * Please see Bug 818696 for more information.
     */
    sIsPairing++;
  }

private:
  const nsCString mDeviceAddress;
  int mTimeout;
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
};

nsresult
BluetoothDBusService::CreatePairedDeviceInternal(
                                              const nsAString& aDeviceAddress,
                                              int aTimeout,
                                              BluetoothReplyRunnable* aRunnable)
{
  Task* task = new CreatePairedDeviceInternalTask(
    NS_ConvertUTF16toUTF8(aDeviceAddress),
    aTimeout, aRunnable);
  DispatchToDBusThread(task);

  return NS_OK;
}

class RemoveDeviceTask : public Task
{
public:
  RemoveDeviceTask(const nsAString& aDeviceAddress,
                   BluetoothReplyRunnable* aRunnable)
    : mDeviceAddress(aDeviceAddress)
    , mRunnable(aRunnable)
  {
    MOZ_ASSERT(!mDeviceAddress.IsEmpty());
    MOZ_ASSERT(mRunnable);
  }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread
    MOZ_ASSERT(sDBusConnection);
    MOZ_ASSERT(!sAdapterPath.IsEmpty());

    nsCString deviceObjectPath =
      NS_ConvertUTF16toUTF8(GetObjectPathFromAddress(sAdapterPath,
                                                     mDeviceAddress));
    const char* cstrDeviceObjectPath = deviceObjectPath.get();

    bool success = sDBusConnection->SendWithReply(
      OnRemoveDeviceReply, static_cast<void*>(mRunnable.get()), -1,
      NS_ConvertUTF16toUTF8(sAdapterPath).get(),
      DBUS_ADAPTER_IFACE, "RemoveDevice",
      DBUS_TYPE_OBJECT_PATH, &cstrDeviceObjectPath,
      DBUS_TYPE_INVALID);
    NS_ENSURE_TRUE_VOID(success);

    unused << mRunnable.forget(); // picked up by callback handler
  }

protected:
  static void OnRemoveDeviceReply(DBusMessage* aReply, void* aData)
  {
    nsAutoString errorStr;

    if (!aReply) {
      errorStr.AssignLiteral("RemoveDevice failed");
    }

    nsRefPtr<BluetoothReplyRunnable> runnable =
      dont_AddRef<BluetoothReplyRunnable>(
        static_cast<BluetoothReplyRunnable*>(aData));

    DispatchBluetoothReply(runnable.get(), BluetoothValue(true), errorStr);
  }

private:
  const nsString mDeviceAddress;
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
};

nsresult
BluetoothDBusService::RemoveDeviceInternal(const nsAString& aDeviceAddress,
                                           BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!IsReady()) {
    NS_NAMED_LITERAL_STRING(errorStr, "Bluetooth service is not ready yet!");
    DispatchBluetoothReply(aRunnable, BluetoothValue(), errorStr);
    return NS_OK;
  }

  Task* task = new RemoveDeviceTask(aDeviceAddress, aRunnable);
  DispatchToDBusThread(task);

  return NS_OK;
}

class SetPinCodeTask : public Task
{
public:
  SetPinCodeTask(const nsAString& aDeviceAddress,
                 const nsACString& aPinCode,
                 BluetoothReplyRunnable* aRunnable)
    : mDeviceAddress(aDeviceAddress)
    , mPinCode(aPinCode)
    , mRunnable(aRunnable)
  {
    MOZ_ASSERT(!mDeviceAddress.IsEmpty());
    MOZ_ASSERT(mRunnable);
  }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread

    nsAutoString errorStr;
    BluetoothValue v = true;
    DBusMessage *msg;
    if (!sPairingReqTable->Get(mDeviceAddress, &msg)) {
      BT_WARNING("%s: Couldn't get original request message.", __FUNCTION__);
      errorStr.AssignLiteral("Couldn't get original request message.");
      DispatchBluetoothReply(mRunnable, v, errorStr);
      return;
    }

    DBusMessage *reply = dbus_message_new_method_return(msg);

    if (!reply) {
      BT_WARNING("%s: Memory can't be allocated for the message.", __FUNCTION__);
      dbus_message_unref(msg);
      errorStr.AssignLiteral("Memory can't be allocated for the message.");
      DispatchBluetoothReply(mRunnable, v, errorStr);
      return;
    }

    const char* pinCode = mPinCode.get();

    if (!dbus_message_append_args(reply,
                                  DBUS_TYPE_STRING, &pinCode,
                                  DBUS_TYPE_INVALID)) {
      BT_WARNING("%s: Couldn't append arguments to dbus message.", __FUNCTION__);
      errorStr.AssignLiteral("Couldn't append arguments to dbus message.");
    } else {
      MOZ_ASSERT(sDBusConnection);
      sDBusConnection->Send(reply);
    }

    dbus_message_unref(msg);
    dbus_message_unref(reply);

    sPairingReqTable->Remove(mDeviceAddress);
    DispatchBluetoothReply(mRunnable, v, errorStr);
  }

private:
  const nsString mDeviceAddress;
  const nsCString mPinCode;
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
};

bool
BluetoothDBusService::SetPinCodeInternal(const nsAString& aDeviceAddress,
                                         const nsAString& aPinCode,
                                         BluetoothReplyRunnable* aRunnable)
{
  Task* task = new SetPinCodeTask(aDeviceAddress,
                                  NS_ConvertUTF16toUTF8(aPinCode),
                                  aRunnable);
  DispatchToDBusThread(task);

  return true;
}

class SetPasskeyTask : public Task
{
public:
  SetPasskeyTask(const nsAString& aDeviceAddress,
                 uint32_t aPasskey,
                 BluetoothReplyRunnable* aRunnable)
    : mDeviceAddress(aDeviceAddress)
    , mPasskey(aPasskey)
    , mRunnable(aRunnable)
  {
    MOZ_ASSERT(!mDeviceAddress.IsEmpty());
    MOZ_ASSERT(mRunnable);
  }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread

    nsAutoString errorStr;
    BluetoothValue v = true;
    DBusMessage *msg;
    if (!sPairingReqTable->Get(mDeviceAddress, &msg)) {
      BT_WARNING("%s: Couldn't get original request message.", __FUNCTION__);
      errorStr.AssignLiteral("Couldn't get original request message.");
      DispatchBluetoothReply(mRunnable, v, errorStr);
      return;
    }

    DBusMessage *reply = dbus_message_new_method_return(msg);

    if (!reply) {
      BT_WARNING("%s: Memory can't be allocated for the message.", __FUNCTION__);
      dbus_message_unref(msg);
      errorStr.AssignLiteral("Memory can't be allocated for the message.");
      DispatchBluetoothReply(mRunnable, v, errorStr);
      return;
    }

    uint32_t passkey = mPasskey;

    if (!dbus_message_append_args(reply,
                                  DBUS_TYPE_UINT32, &passkey,
                                  DBUS_TYPE_INVALID)) {
      BT_WARNING("%s: Couldn't append arguments to dbus message.", __FUNCTION__);
      errorStr.AssignLiteral("Couldn't append arguments to dbus message.");
    } else {
      MOZ_ASSERT(sDBusConnection);
      sDBusConnection->Send(reply);
    }

    dbus_message_unref(msg);
    dbus_message_unref(reply);

    sPairingReqTable->Remove(mDeviceAddress);
    DispatchBluetoothReply(mRunnable, v, errorStr);
  }

private:
  nsString mDeviceAddress;
  uint32_t mPasskey;
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
};

bool
BluetoothDBusService::SetPasskeyInternal(const nsAString& aDeviceAddress,
                                         uint32_t aPasskey,
                                         BluetoothReplyRunnable* aRunnable)
{
  Task* task = new SetPasskeyTask(aDeviceAddress,
                                  aPasskey,
                                  aRunnable);
  DispatchToDBusThread(task);

  return true;
}

class SetPairingConfirmationTask : public Task
{
public:
  SetPairingConfirmationTask(const nsAString& aDeviceAddress,
                             bool aConfirm,
                             BluetoothReplyRunnable* aRunnable)
    : mDeviceAddress(aDeviceAddress)
    , mConfirm(aConfirm)
    , mRunnable(aRunnable)
  {
    MOZ_ASSERT(!mDeviceAddress.IsEmpty());
    MOZ_ASSERT(mRunnable);
  }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread
    MOZ_ASSERT(sDBusConnection);

    nsAutoString errorStr;
    BluetoothValue v = true;
    DBusMessage *msg;
    if (!sPairingReqTable->Get(mDeviceAddress, &msg)) {
      BT_WARNING("%s: Couldn't get original request message.", __FUNCTION__);
      errorStr.AssignLiteral("Couldn't get original request message.");
      DispatchBluetoothReply(mRunnable, v, errorStr);
      return;
    }

    DBusMessage *reply;

    if (mConfirm) {
      reply = dbus_message_new_method_return(msg);
    } else {
      reply = dbus_message_new_error(msg, "org.bluez.Error.Rejected",
                                     "User rejected confirmation");
    }

    if (!reply) {
      BT_WARNING("%s: Memory can't be allocated for the message.", __FUNCTION__);
      dbus_message_unref(msg);
      errorStr.AssignLiteral("Memory can't be allocated for the message.");
      DispatchBluetoothReply(mRunnable, v, errorStr);
      return;
    }

    bool result = sDBusConnection->Send(reply);
    if (!result) {
      errorStr.AssignLiteral("Can't send message!");
    }
    dbus_message_unref(msg);
    dbus_message_unref(reply);

    sPairingReqTable->Remove(mDeviceAddress);
    DispatchBluetoothReply(mRunnable, v, errorStr);
  }

private:
  nsString mDeviceAddress;
  bool mConfirm;
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
};

bool
BluetoothDBusService::SetPairingConfirmationInternal(
                                              const nsAString& aDeviceAddress,
                                              bool aConfirm,
                                              BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  Task* task = new SetPairingConfirmationTask(aDeviceAddress,
                                              aConfirm,
                                              aRunnable);
  DispatchToDBusThread(task);

  return true;
}

static void
NextBluetoothProfileController()
{
  MOZ_ASSERT(NS_IsMainThread());

  // First, remove the task at the front which has been already done.
  NS_ENSURE_FALSE_VOID(sControllerArray.IsEmpty());
  sControllerArray.RemoveElementAt(0);

  // Re-check if the task array is empty, if it's not, the next task will begin.
  NS_ENSURE_FALSE_VOID(sControllerArray.IsEmpty());
  sControllerArray[0]->StartSession();
}

static void
ConnectDisconnect(bool aConnect, const nsAString& aDeviceAddress,
                  BluetoothReplyRunnable* aRunnable,
                  uint16_t aServiceUuid, uint32_t aCod = 0)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  BluetoothProfileController* controller =
    new BluetoothProfileController(aConnect, aDeviceAddress, aRunnable,
                                   NextBluetoothProfileController,
                                   aServiceUuid, aCod);
  sControllerArray.AppendElement(controller);

  /**
   * If the request is the first element of the quene, start from here. Note
   * that other request is pushed into the quene and is popped out after the
   * first one is completed. See NextBluetoothProfileController() for details.
   */
  if (sControllerArray.Length() == 1) {
    sControllerArray[0]->StartSession();
  }
}

void
BluetoothDBusService::Connect(const nsAString& aDeviceAddress,
                              uint32_t aCod,
                              uint16_t aServiceUuid,
                              BluetoothReplyRunnable* aRunnable)
{
  ConnectDisconnect(true, aDeviceAddress, aRunnable, aServiceUuid, aCod);
}

void
BluetoothDBusService::Disconnect(const nsAString& aDeviceAddress,
                                 uint16_t aServiceUuid,
                                 BluetoothReplyRunnable* aRunnable)
{
  ConnectDisconnect(false, aDeviceAddress, aRunnable, aServiceUuid);
}

bool
BluetoothDBusService::IsConnected(const uint16_t aServiceUuid)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothProfileManagerBase* profile =
    BluetoothUuidHelper::GetBluetoothProfileManager(aServiceUuid);
  if (!profile) {
    BT_WARNING(ERR_UNKNOWN_PROFILE);
    return false;
  }

  NS_ENSURE_TRUE(profile, false);
  return profile->IsConnected();
}

#ifdef MOZ_B2G_RIL
void
BluetoothDBusService::AnswerWaitingCall(BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  hfp->AnswerWaitingCall();

  DispatchBluetoothReply(aRunnable, BluetoothValue(true), EmptyString());
}

void
BluetoothDBusService::IgnoreWaitingCall(BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  hfp->IgnoreWaitingCall();

  DispatchBluetoothReply(aRunnable, BluetoothValue(true), EmptyString());
}

void
BluetoothDBusService::ToggleCalls(BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  hfp->ToggleCalls();

  DispatchBluetoothReply(aRunnable, BluetoothValue(true), EmptyString());
}
#endif // MOZ_B2G_RIL

class OnUpdateSdpRecordsRunnable : public nsRunnable
{
public:
  OnUpdateSdpRecordsRunnable(const nsAString& aObjectPath,
                             BluetoothProfileManagerBase* aManager)
    : mManager(aManager)
  {
    MOZ_ASSERT(!aObjectPath.IsEmpty());
    MOZ_ASSERT(aManager);

    mDeviceAddress = GetAddressFromObjectPath(aObjectPath);
  }

  nsresult
  Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    mManager->OnUpdateSdpRecords(mDeviceAddress);

    return NS_OK;
  }

private:
  nsString mDeviceAddress;
  BluetoothProfileManagerBase* mManager;
};

class OnGetServiceChannelRunnable : public nsRunnable
{
public:
  OnGetServiceChannelRunnable(const nsAString& aDeviceAddress,
                              const nsAString& aServiceUuid,
                              int aChannel,
                              BluetoothProfileManagerBase* aManager)
    : mDeviceAddress(aDeviceAddress)
    , mServiceUuid(aServiceUuid)
    , mChannel(aChannel)
    , mManager(aManager)
  {
    MOZ_ASSERT(!aDeviceAddress.IsEmpty());
    MOZ_ASSERT(!aServiceUuid.IsEmpty());
    MOZ_ASSERT(aManager);
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    mManager->OnGetServiceChannel(mDeviceAddress, mServiceUuid, mChannel);

    return NS_OK;
  }

private:
  nsString mDeviceAddress;
  nsString mServiceUuid;
  int mChannel;
  BluetoothProfileManagerBase* mManager;
};

class OnGetServiceChannelReplyHandler : public DBusReplyHandler
{
public:
  OnGetServiceChannelReplyHandler(const nsAString& aDeviceAddress,
                                  const nsAString& aServiceUUID,
                                  BluetoothProfileManagerBase* aBluetoothProfileManager)
  : mDeviceAddress(aDeviceAddress),
    mServiceUUID(aServiceUUID),
    mBluetoothProfileManager(aBluetoothProfileManager)
  {
    MOZ_ASSERT(mBluetoothProfileManager);
  }

  void Handle(DBusMessage* aReply)
  {
    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread

    // The default channel is an invalid value of -1. We
    // update it if we have received a correct reply. Both
    // cases, valid and invalid channel numbers, are handled
    // in BluetoothProfileManagerBase::OnGetServiceChannel.

    int channel = -1;

    if (aReply && (dbus_message_get_type(aReply) != DBUS_MESSAGE_TYPE_ERROR)) {
      channel = dbus_returns_int32(aReply);
    }

    nsRefPtr<nsRunnable> r = new OnGetServiceChannelRunnable(mDeviceAddress,
                                                             mServiceUUID,
                                                             channel,
                                                             mBluetoothProfileManager);
    nsresult rv = NS_DispatchToMainThread(r);
    NS_ENSURE_SUCCESS_VOID(rv);
  }

private:
  nsString mDeviceAddress;
  nsString mServiceUUID;
  BluetoothProfileManagerBase* mBluetoothProfileManager;
};

class GetServiceChannelTask : public Task
{
public:
  GetServiceChannelTask(const nsAString& aDeviceAddress,
                        const nsAString& aServiceUUID,
                        BluetoothProfileManagerBase* aBluetoothProfileManager)
    : mDeviceAddress(aDeviceAddress)
    , mServiceUUID(aServiceUUID)
    , mBluetoothProfileManager(aBluetoothProfileManager)
  {
    MOZ_ASSERT(!mDeviceAddress.IsEmpty());
    MOZ_ASSERT(mBluetoothProfileManager);
  }

  void Run() MOZ_OVERRIDE
  {
    static const int sProtocolDescriptorList = 0x0004;

    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread
    MOZ_ASSERT(sDBusConnection);
    MOZ_ASSERT(!sAdapterPath.IsEmpty());

    nsString objectPath =
      GetObjectPathFromAddress(sAdapterPath, mDeviceAddress);

    nsRefPtr<OnGetServiceChannelReplyHandler> handler =
      new OnGetServiceChannelReplyHandler(mDeviceAddress, mServiceUUID,
                                          mBluetoothProfileManager);

    nsCString serviceUUID = NS_ConvertUTF16toUTF8(mServiceUUID);
    const char* cstrServiceUUID = serviceUUID.get();

    bool success = sDBusConnection->SendWithReply(
      OnGetServiceChannelReplyHandler::Callback, handler, -1,
      NS_ConvertUTF16toUTF8(objectPath).get(),
      DBUS_DEVICE_IFACE, "GetServiceAttributeValue",
      DBUS_TYPE_STRING, &cstrServiceUUID,
      DBUS_TYPE_UINT16, &sProtocolDescriptorList,
      DBUS_TYPE_INVALID);
    NS_ENSURE_TRUE_VOID(success);

    unused << handler.forget(); // picked up by callback handler
  }

private:
  nsString mDeviceAddress;
  nsString mServiceUUID;
  BluetoothProfileManagerBase* mBluetoothProfileManager;
};

nsresult
BluetoothDBusService::GetServiceChannel(const nsAString& aDeviceAddress,
                                        const nsAString& aServiceUUID,
                                        BluetoothProfileManagerBase* aManager)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!IsReady()) {
    NS_NAMED_LITERAL_STRING(errorStr, "Bluetooth service is not ready yet!");
    return NS_OK;
  }

#ifdef MOZ_WIDGET_GONK
  // GetServiceAttributeValue only exists in android's bluez dbus binding
  // implementation
  Task* task = new GetServiceChannelTask(aDeviceAddress,
                                         aServiceUUID,
                                         aManager);
  DispatchToDBusThread(task);
#else
  // FIXME/Bug 793977 qdot: Just set something for desktop, until we have a
  // parser for the GetServiceAttributes xml block
  //
  // Even though we are on the main thread already, we need to dispatch a
  // runnable here. OnGetServiceChannel needs mRunnable to be set, which
  // happens after GetServiceChannel returns.
  nsRefPtr<nsRunnable> r = new OnGetServiceChannelRunnable(aDeviceAddress,
                                                           aServiceUUID,
                                                           1,
                                                           aManager);
  NS_DispatchToMainThread(r);
#endif

  return NS_OK;
}

class UpdateSdpRecordsTask : public Task
{
public:
  UpdateSdpRecordsTask(const nsAString& aDeviceAddress,
                       BluetoothProfileManagerBase* aBluetoothProfileManager)
    : mDeviceAddress(aDeviceAddress)
    , mBluetoothProfileManager(aBluetoothProfileManager)
  {
    MOZ_ASSERT(!mDeviceAddress.IsEmpty());
    MOZ_ASSERT(mBluetoothProfileManager);
  }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread
    MOZ_ASSERT(sDBusConnection);
    MOZ_ASSERT(!sAdapterPath.IsEmpty());

    const nsString objectPath =
      GetObjectPathFromAddress(sAdapterPath, mDeviceAddress);

    // I choose to use raw pointer here because this is going to be passed as an
    // argument into SendWithReply() at once.
    OnUpdateSdpRecordsRunnable* callbackRunnable =
      new OnUpdateSdpRecordsRunnable(objectPath, mBluetoothProfileManager);

    sDBusConnection->SendWithReply(DiscoverServicesCallback,
                                   (void*)callbackRunnable, -1,
                                   NS_ConvertUTF16toUTF8(objectPath).get(),
                                   DBUS_DEVICE_IFACE,
                                   "DiscoverServices",
                                   DBUS_TYPE_STRING, &EmptyCString(),
                                   DBUS_TYPE_INVALID);
  }

protected:
  static void DiscoverServicesCallback(DBusMessage* aMsg, void* aData)
  {
    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread

    nsRefPtr<OnUpdateSdpRecordsRunnable> r(
      static_cast<OnUpdateSdpRecordsRunnable*>(aData));
    NS_DispatchToMainThread(r);
  }

private:
  const nsString mDeviceAddress;
  BluetoothProfileManagerBase* mBluetoothProfileManager;
};

bool
BluetoothDBusService::UpdateSdpRecords(const nsAString& aDeviceAddress,
                                       BluetoothProfileManagerBase* aManager)
{
  MOZ_ASSERT(NS_IsMainThread());

  Task* task = new UpdateSdpRecordsTask(aDeviceAddress, aManager);
  DispatchToDBusThread(task);

  return true;
}

void
BluetoothDBusService::SendFile(const nsAString& aDeviceAddress,
                               BlobParent* aBlobParent,
                               BlobChild* aBlobChild,
                               BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Currently we only support one device sending one file at a time,
  // so we don't need aDeviceAddress here because the target device
  // has been determined when calling 'Connect()'. Nevertheless, keep
  // it for future use.
  BluetoothOppManager* opp = BluetoothOppManager::Get();
  nsAutoString errorStr;
  if (!opp || !opp->SendFile(aDeviceAddress, aBlobParent)) {
    errorStr.AssignLiteral("Calling SendFile() failed");
  }

  DispatchBluetoothReply(aRunnable, BluetoothValue(true), errorStr);
}

void
BluetoothDBusService::SendFile(const nsAString& aDeviceAddress,
                               nsIDOMBlob* aBlob,
                               BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Currently we only support one device sending one file at a time,
  // so we don't need aDeviceAddress here because the target device
  // has been determined when calling 'Connect()'. Nevertheless, keep
  // it for future use.
  BluetoothOppManager* opp = BluetoothOppManager::Get();
  nsAutoString errorStr;
  if (!opp || !opp->SendFile(aDeviceAddress, aBlob)) {
    errorStr.AssignLiteral("Calling SendFile() failed");
  }

  DispatchBluetoothReply(aRunnable, BluetoothValue(true), errorStr);
}

void
BluetoothDBusService::StopSendingFile(const nsAString& aDeviceAddress,
                                      BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Currently we only support one device sending one file at a time,
  // so we don't need aDeviceAddress here because the target device
  // has been determined when calling 'Connect()'. Nevertheless, keep
  // it for future use.
  BluetoothOppManager* opp = BluetoothOppManager::Get();
  nsAutoString errorStr;
  if (!opp || !opp->StopSendingFile()) {
    errorStr.AssignLiteral("Calling StopSendingFile() failed");
  }

  DispatchBluetoothReply(aRunnable, BluetoothValue(true), errorStr);
}

void
BluetoothDBusService::ConfirmReceivingFile(const nsAString& aDeviceAddress,
                                           bool aConfirm,
                                           BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be called from main thread!");

  // Currently we only support one device sending one file at a time,
  // so we don't need aDeviceAddress here because the target device
  // has been determined when calling 'Connect()'. Nevertheless, keep
  // it for future use.
  BluetoothOppManager* opp = BluetoothOppManager::Get();
  nsAutoString errorStr;
  if (!opp || !opp->ConfirmReceivingFile(aConfirm)) {
    errorStr.AssignLiteral("Calling ConfirmReceivingFile() failed");
  }

  DispatchBluetoothReply(aRunnable, BluetoothValue(true), errorStr);
}

void
BluetoothDBusService::ConnectSco(BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  if (!hfp || !hfp->ConnectSco(aRunnable)) {
    NS_NAMED_LITERAL_STRING(replyError, "Calling ConnectSco() failed");
    DispatchBluetoothReply(aRunnable, BluetoothValue(), replyError);
  }
}

void
BluetoothDBusService::DisconnectSco(BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  if (!hfp || !hfp->DisconnectSco()) {
    NS_NAMED_LITERAL_STRING(replyError, "Calling DisconnectSco() failed");
    DispatchBluetoothReply(aRunnable, BluetoothValue(), replyError);
    return;
  }

  DispatchBluetoothReply(aRunnable, BluetoothValue(true), EmptyString());
}

void
BluetoothDBusService::IsScoConnected(BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
  if (!hfp) {
    NS_NAMED_LITERAL_STRING(replyError, "Fail to get BluetoothHfpManager");
    DispatchBluetoothReply(aRunnable, BluetoothValue(), replyError);
    return;
  }

  DispatchBluetoothReply(aRunnable, hfp->IsScoConnected(), EmptyString());
}

class SendMetadataTask : public Task
{
public:
  SendMetadataTask(const nsAString& aDeviceAddress,
                   const nsACString& aTitle,
                   const nsACString& aArtist,
                   const nsACString& aAlbum,
                   int64_t aMediaNumber,
                   int64_t aTotalMediaCount,
                   int64_t aDuration,
                   BluetoothReplyRunnable* aRunnable)
    : mDeviceAddress(aDeviceAddress)
    , mTitle(aTitle)
    , mArtist(aArtist)
    , mAlbum(aAlbum)
    , mMediaNumber(aMediaNumber)
    , mTotalMediaCount(aTotalMediaCount)
    , mDuration(aDuration)
    , mRunnable(aRunnable)
  {
    MOZ_ASSERT(!mDeviceAddress.IsEmpty());
    MOZ_ASSERT(mRunnable);
  }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread
    MOZ_ASSERT(sDBusConnection);
    MOZ_ASSERT(!sAdapterPath.IsEmpty());

    // We currently don't support genre field in music player.
    // In order to send media metadata through AVRCP, we set genre to an empty
    // string to match the BlueZ method "UpdateMetaData" with signature "sssssss",
    // which takes genre field as the last parameter.
    nsCString tempGenre = EmptyCString();
    nsCString tempMediaNumber = EmptyCString();
    nsCString tempTotalMediaCount = EmptyCString();
    nsCString tempDuration = EmptyCString();

    if (mMediaNumber >= 0) {
      tempMediaNumber.AppendInt(mMediaNumber);
    }
    if (mTotalMediaCount >= 0) {
      tempTotalMediaCount.AppendInt(mTotalMediaCount);
    }
    if (mDuration >= 0) {
      tempDuration.AppendInt(mDuration);
    }

    const nsCString objectPath = NS_ConvertUTF16toUTF8(
      GetObjectPathFromAddress(sAdapterPath, mDeviceAddress));

    const char* title = mTitle.get();
    const char* album = mAlbum.get();
    const char* artist = mArtist.get();
    const char* mediaNumber = tempMediaNumber.get();
    const char* totalMediaCount = tempTotalMediaCount.get();
    const char* duration = tempDuration.get();
    const char* genre = tempGenre.get();

    bool success = sDBusConnection->SendWithReply(
      GetVoidCallback, static_cast<void*>(mRunnable.get()), -1,
      objectPath.get(),
      DBUS_CTL_IFACE, "UpdateMetaData",
      DBUS_TYPE_STRING, &title,
      DBUS_TYPE_STRING, &artist,
      DBUS_TYPE_STRING, &album,
      DBUS_TYPE_STRING, &mediaNumber,
      DBUS_TYPE_STRING, &totalMediaCount,
      DBUS_TYPE_STRING, &duration,
      DBUS_TYPE_STRING, &genre,
      DBUS_TYPE_INVALID);
    NS_ENSURE_TRUE_VOID(success);

    unused << mRunnable.forget(); // picked up by callback handler
  }

private:
  const nsString mDeviceAddress;
  const nsCString mTitle;
  const nsCString mArtist;
  const nsCString mAlbum;
  int64_t mMediaNumber;
  int64_t mTotalMediaCount;
  int64_t mDuration;
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
};

void
BluetoothDBusService::SendMetaData(const nsAString& aTitle,
                                   const nsAString& aArtist,
                                   const nsAString& aAlbum,
                                   int64_t aMediaNumber,
                                   int64_t aTotalMediaCount,
                                   int64_t aDuration,
                                   BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!IsReady()) {
    NS_NAMED_LITERAL_STRING(errorStr, "Bluetooth service is not ready yet!");
    DispatchBluetoothReply(aRunnable, BluetoothValue(), errorStr);
    return;
  }

  BluetoothA2dpManager* a2dp = BluetoothA2dpManager::Get();
  NS_ENSURE_TRUE_VOID(a2dp);

  if (!a2dp->IsConnected()) {
    DispatchBluetoothReply(aRunnable, BluetoothValue(),
                           NS_LITERAL_STRING(ERR_A2DP_IS_DISCONNECTED));
    return;
  } else if (!a2dp->IsAvrcpConnected()) {
    DispatchBluetoothReply(aRunnable, BluetoothValue(),
                           NS_LITERAL_STRING(ERR_AVRCP_IS_DISCONNECTED));
    return;
  }

  nsAutoString prevTitle, prevAlbum;
  a2dp->GetTitle(prevTitle);
  a2dp->GetAlbum(prevAlbum);

  if (aMediaNumber != a2dp->GetMediaNumber() ||
      !aTitle.Equals(prevTitle) ||
      !aAlbum.Equals(prevAlbum)) {
    UpdateNotification(ControlEventId::EVENT_TRACK_CHANGED, aMediaNumber);
  }

  nsAutoString deviceAddress;
  a2dp->GetAddress(deviceAddress);

  Task* task = new SendMetadataTask(
    deviceAddress,
    NS_ConvertUTF16toUTF8(aTitle),
    NS_ConvertUTF16toUTF8(aArtist),
    NS_ConvertUTF16toUTF8(aAlbum),
    aMediaNumber,
    aTotalMediaCount,
    aDuration,
    aRunnable);
  DispatchToDBusThread(task);

  a2dp->UpdateMetaData(aTitle, aArtist, aAlbum,
                       aMediaNumber, aTotalMediaCount, aDuration);
}

static ControlPlayStatus
PlayStatusStringToControlPlayStatus(const nsAString& aPlayStatus)
{
  ControlPlayStatus playStatus = ControlPlayStatus::PLAYSTATUS_UNKNOWN;
  if (aPlayStatus.EqualsLiteral("STOPPED")) {
    playStatus = ControlPlayStatus::PLAYSTATUS_STOPPED;
  } else if (aPlayStatus.EqualsLiteral("PLAYING")) {
    playStatus = ControlPlayStatus::PLAYSTATUS_PLAYING;
  } else if (aPlayStatus.EqualsLiteral("PAUSED")) {
    playStatus = ControlPlayStatus::PLAYSTATUS_PAUSED;
  } else if (aPlayStatus.EqualsLiteral("FWD_SEEK")) {
    playStatus = ControlPlayStatus::PLAYSTATUS_FWD_SEEK;
  } else if (aPlayStatus.EqualsLiteral("REV_SEEK")) {
    playStatus = ControlPlayStatus::PLAYSTATUS_REV_SEEK;
  } else if (aPlayStatus.EqualsLiteral("ERROR")) {
    playStatus = ControlPlayStatus::PLAYSTATUS_ERROR;
  }

  return playStatus;
}

class SendPlayStatusTask : public Task
{
public:
  SendPlayStatusTask(const nsAString& aDeviceAddress,
                     int64_t aDuration,
                     int64_t aPosition,
                     ControlPlayStatus aPlayStatus,
                     BluetoothReplyRunnable* aRunnable)
    : mDeviceAddress(aDeviceAddress)
    , mDuration(aDuration)
    , mPosition(aPosition)
    , mPlayStatus(aPlayStatus)
    , mRunnable(aRunnable)
  {
    MOZ_ASSERT(!mDeviceAddress.IsEmpty());
    MOZ_ASSERT(mRunnable);
  }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread
    MOZ_ASSERT(sDBusConnection);
    MOZ_ASSERT(!sAdapterPath.IsEmpty());

    const nsCString objectPath = NS_ConvertUTF16toUTF8(
      GetObjectPathFromAddress(sAdapterPath, mDeviceAddress));

    uint32_t tempPlayStatus = mPlayStatus;

    bool success = sDBusConnection->SendWithReply(
      GetVoidCallback, static_cast<void*>(mRunnable.get()), -1,
      objectPath.get(),
      DBUS_CTL_IFACE, "UpdatePlayStatus",
      DBUS_TYPE_UINT32, &mDuration,
      DBUS_TYPE_UINT32, &mPosition,
      DBUS_TYPE_UINT32, &tempPlayStatus,
      DBUS_TYPE_INVALID);
    NS_ENSURE_TRUE_VOID(success);

    unused << mRunnable.forget(); // picked up by callback handler
  }

private:
  const nsString mDeviceAddress;
  int64_t mDuration;
  int64_t mPosition;
  ControlPlayStatus mPlayStatus;
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
};

void
BluetoothDBusService::SendPlayStatus(int64_t aDuration,
                                     int64_t aPosition,
                                     const nsAString& aPlayStatus,
                                     BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!IsReady()) {
    NS_NAMED_LITERAL_STRING(errorStr, "Bluetooth service is not ready yet!");
    DispatchBluetoothReply(aRunnable, BluetoothValue(), errorStr);
    return;
  }

  ControlPlayStatus playStatus =
    PlayStatusStringToControlPlayStatus(aPlayStatus);
  if (playStatus == ControlPlayStatus::PLAYSTATUS_UNKNOWN) {
    DispatchBluetoothReply(aRunnable, BluetoothValue(),
                           NS_LITERAL_STRING("Invalid play status"));
    return;
  } else if (aDuration < 0) {
    DispatchBluetoothReply(aRunnable, BluetoothValue(),
                           NS_LITERAL_STRING("Invalid duration"));
    return;
  } else if (aPosition < 0) {
    DispatchBluetoothReply(aRunnable, BluetoothValue(),
                           NS_LITERAL_STRING("Invalid position"));
    return;
  }

  BluetoothA2dpManager* a2dp = BluetoothA2dpManager::Get();
  NS_ENSURE_TRUE_VOID(a2dp);

  if (!a2dp->IsConnected()) {
    DispatchBluetoothReply(aRunnable, BluetoothValue(),
                           NS_LITERAL_STRING(ERR_A2DP_IS_DISCONNECTED));
    return;
  } else if (!a2dp->IsAvrcpConnected()) {
    DispatchBluetoothReply(aRunnable, BluetoothValue(),
                           NS_LITERAL_STRING(ERR_AVRCP_IS_DISCONNECTED));
    return;
  }

  if (playStatus != a2dp->GetPlayStatus()) {
    UpdateNotification(ControlEventId::EVENT_PLAYBACK_STATUS_CHANGED,
                       playStatus);
  } else if (aPosition != a2dp->GetPosition()) {
    UpdateNotification(ControlEventId::EVENT_PLAYBACK_POS_CHANGED, aPosition);
  }

  nsAutoString deviceAddress;
  a2dp->GetAddress(deviceAddress);

  Task* task = new SendPlayStatusTask(deviceAddress,
                                      aDuration,
                                      aPosition,
                                      playStatus,
                                      aRunnable);
  DispatchToDBusThread(task);

  a2dp->UpdatePlayStatus(aDuration, aPosition, playStatus);
}

static void
ControlCallback(DBusMessage* aMsg, void* aParam)
{
  NS_ENSURE_TRUE_VOID(aMsg);

  BluetoothValue v;
  nsAutoString replyError;
  UnpackVoidMessage(aMsg, nullptr, v, replyError);
  if (!v.get_bool()) {
    BT_WARNING(NS_ConvertUTF16toUTF8(replyError).get());
  }
}

class UpdatePlayStatusTask : public Task
{
public:
  UpdatePlayStatusTask(const nsAString& aDeviceAddress,
                       int32_t aDuration,
                       int32_t aPosition,
                       ControlPlayStatus aPlayStatus)
    : mDeviceAddress(aDeviceAddress)
    , mDuration(aDuration)
    , mPosition(aPosition)
    , mPlayStatus(aPlayStatus)
  {
    MOZ_ASSERT(!mDeviceAddress.IsEmpty());
  }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread
    MOZ_ASSERT(sDBusConnection);
    MOZ_ASSERT(!sAdapterPath.IsEmpty());

    const nsCString objectPath = NS_ConvertUTF16toUTF8(
      GetObjectPathFromAddress(sAdapterPath, mDeviceAddress));

    uint32_t tempPlayStatus = mPlayStatus;

    bool success = sDBusConnection->SendWithReply(
      ControlCallback, nullptr, -1,
      objectPath.get(),
      DBUS_CTL_IFACE, "UpdatePlayStatus",
      DBUS_TYPE_UINT32, &mDuration,
      DBUS_TYPE_UINT32, &mPosition,
      DBUS_TYPE_UINT32, &tempPlayStatus,
      DBUS_TYPE_INVALID);
    NS_ENSURE_TRUE_VOID(success);
  }

private:
  const nsString mDeviceAddress;
  int32_t mDuration;
  int32_t mPosition;
  ControlPlayStatus mPlayStatus;
};

void
BluetoothDBusService::UpdatePlayStatus(uint32_t aDuration,
                                       uint32_t aPosition,
                                       ControlPlayStatus aPlayStatus)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE_VOID(this->IsReady());

  BluetoothA2dpManager* a2dp = BluetoothA2dpManager::Get();
  NS_ENSURE_TRUE_VOID(a2dp);
  MOZ_ASSERT(a2dp->IsConnected());
  MOZ_ASSERT(a2dp->IsAvrcpConnected());
  MOZ_ASSERT(!sAdapterPath.IsEmpty());

  nsAutoString deviceAddress;
  a2dp->GetAddress(deviceAddress);

  Task* task = new UpdatePlayStatusTask(deviceAddress,
                                        aDuration,
                                        aPosition,
                                        aPlayStatus);
  DispatchToDBusThread(task);
}

class UpdateNotificationTask : public Task
{
public:
  UpdateNotificationTask(const nsAString& aDeviceAddress,
                         BluetoothDBusService::ControlEventId aEventId,
                         uint64_t aData)
    : mDeviceAddress(aDeviceAddress)
    , mEventId(aEventId)
    , mData(aData)
  {
    MOZ_ASSERT(!mDeviceAddress.IsEmpty());
  }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(!NS_IsMainThread()); // I/O thread
    MOZ_ASSERT(sDBusConnection);
    MOZ_ASSERT(!sAdapterPath.IsEmpty());

    const nsCString objectPath = NS_ConvertUTF16toUTF8(
      GetObjectPathFromAddress(sAdapterPath, mDeviceAddress));

    uint16_t eventId = mEventId;

    bool success = sDBusConnection->SendWithReply(
      ControlCallback, nullptr, -1,
      objectPath.get(),
      DBUS_CTL_IFACE, "UpdateNotification",
      DBUS_TYPE_UINT16, &eventId,
      DBUS_TYPE_UINT64, &mData,
      DBUS_TYPE_INVALID);
    NS_ENSURE_TRUE_VOID(success);
  }

private:
  const nsString mDeviceAddress;
  int16_t mEventId;
  int32_t mData;
};

void
BluetoothDBusService::UpdateNotification(ControlEventId aEventId,
                                         uint64_t aData)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE_VOID(this->IsReady());

  BluetoothA2dpManager* a2dp = BluetoothA2dpManager::Get();
  NS_ENSURE_TRUE_VOID(a2dp);
  MOZ_ASSERT(a2dp->IsConnected());
  MOZ_ASSERT(a2dp->IsAvrcpConnected());
  MOZ_ASSERT(!sAdapterPath.IsEmpty());

  nsAutoString deviceAddress;
  a2dp->GetAddress(deviceAddress);

  Task* task = new UpdateNotificationTask(deviceAddress, aEventId, aData);
  DispatchToDBusThread(task);
}
