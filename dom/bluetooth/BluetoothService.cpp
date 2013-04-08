/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BluetoothService.h"

#include "BluetoothCommon.h"
#include "BluetoothManager.h"
#include "BluetoothParent.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothServiceChildProcess.h"
#include "BluetoothUtils.h"

#include "jsapi.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/unused.h"
#include "mozilla/Util.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/ipc/UnixSocket.h"
#include "nsContentUtils.h"
#include "nsIObserverService.h"
#include "nsISettingsService.h"
#include "nsISystemMessagesInternal.h"
#include "nsITimer.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"

#if defined(MOZ_WIDGET_GONK)
#include "cutils/properties.h"
#endif

#if defined(MOZ_B2G_BT)
# if defined(MOZ_BLUETOOTH_GONK)
#  include "BluetoothGonkService.h"
# elif defined(MOZ_BLUETOOTH_DBUS)
#  include "BluetoothDBusService.h"
# else
#  error No_suitable_backend_for_bluetooth!
# endif
#endif

#define MOZSETTINGS_CHANGED_ID      "mozsettings-changed"
#define BLUETOOTH_ENABLED_SETTING   "bluetooth.enabled"
#define BLUETOOTH_DEBUGGING_SETTING "bluetooth.debugging.enabled"

#define PROP_BLUETOOTH_ENABLED      "bluetooth.isEnabled"

#define DEFAULT_SHUTDOWN_TIMER_MS 5000

bool gBluetoothDebugFlag = false;

using namespace mozilla;
using namespace mozilla::dom;
USING_BLUETOOTH_NAMESPACE

namespace {

StaticRefPtr<BluetoothService> gBluetoothService;

bool gInShutdown = false;
bool gToggleInProgress = false;

bool
IsMainProcess()
{
  return XRE_GetProcessType() == GeckoProcessType_Default;
}

void
ShutdownTimeExceeded(nsITimer* aTimer, void* aClosure)
{
  MOZ_ASSERT(NS_IsMainThread());
  *static_cast<bool*>(aClosure) = true;
}

void
GetAllBluetoothActors(InfallibleTArray<BluetoothParent*>& aActors)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aActors.IsEmpty());

  nsAutoTArray<ContentParent*, 20> contentActors;
  ContentParent::GetAll(contentActors);

  for (uint32_t contentIndex = 0;
       contentIndex < contentActors.Length();
       contentIndex++) {
    MOZ_ASSERT(contentActors[contentIndex]);

    AutoInfallibleTArray<PBluetoothParent*, 5> bluetoothActors;
    contentActors[contentIndex]->ManagedPBluetoothParent(bluetoothActors);

    for (uint32_t bluetoothIndex = 0;
         bluetoothIndex < bluetoothActors.Length();
         bluetoothIndex++) {
      MOZ_ASSERT(bluetoothActors[bluetoothIndex]);

      BluetoothParent* actor =
        static_cast<BluetoothParent*>(bluetoothActors[bluetoothIndex]);
      aActors.AppendElement(actor);
    }
  }
}

} // anonymous namespace

class BluetoothService::ToggleBtAck : public nsRunnable
{
public:
  ToggleBtAck(bool aEnabled)
    : mEnabled(aEnabled)
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (!gBluetoothService) {
      return NS_OK;
    }

    if (!gInShutdown) {
      gBluetoothService->SetEnabled(mEnabled);

      nsAutoString signalName, signalPath;
      BluetoothValue v = true;
      if (mEnabled) {
        signalName = NS_LITERAL_STRING("Enabled");
      } else {
        signalName = NS_LITERAL_STRING("Disabled");
      }
      signalPath = NS_LITERAL_STRING(KEY_MANAGER);
      BluetoothSignal signal(signalName, signalPath, v);
      gBluetoothService->DistributeSignal(signal);
    }

    if (!mEnabled || gInShutdown) {
      // Shut down the command thread if it still exists.
      if (gBluetoothService->mBluetoothCommandThread) {
        nsCOMPtr<nsIThread> thread;
        gBluetoothService->mBluetoothCommandThread.swap(thread);
        if (NS_FAILED(thread->Shutdown())) {
          NS_WARNING("Failed to shut down the bluetooth command thread!");
        }
      }

      if (gInShutdown) {
        gBluetoothService = nullptr;
      }
    }

    return NS_OK;
  }

private:
  bool mEnabled;
};

class BluetoothService::ToggleBtTask : public nsRunnable
{
public:
  ToggleBtTask(bool aEnabled)
    : mEnabled(aEnabled)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    /*
     * mEnabled: expected status of bluetooth
     * gBluetoothService->IsEnabled(): real status of bluetooth
     *
     * When two values are the same, we don't switch on/off bluetooth,
     * but we still do ToggleBtAck task.
     */
    if (mEnabled == gBluetoothService->IsEnabled()) {
      NS_WARNING("Bluetooth has already been enabled/disabled before.");
    } else {
      // Switch on/off bluetooth
      if (mEnabled) {
        if (NS_FAILED(gBluetoothService->StartInternal())) {
          NS_WARNING("Bluetooth service failed to start!");
          mEnabled = !mEnabled;
        }
      } else {
        if (NS_FAILED(gBluetoothService->StopInternal())) {
          NS_WARNING("Bluetooth service failed to stop!");
          mEnabled = !mEnabled;
        }
      }
    }

    // This is requested in Bug 836516. With settings this property, WLAN
    // firmware could be aware of Bluetooth has been turned on/off, so that the
    // mecahnism of handling coexistence of WIFI and Bluetooth could be started.
    //
    // In the future, we may have our own way instead of setting a system
    // property to let firmware developers be able to sense that Bluetooth has
    // been toggled.
#if defined(MOZ_WIDGET_GONK)
    if (property_set(PROP_BLUETOOTH_ENABLED, mEnabled ? "true" : "false") != 0) {
      NS_WARNING("Failed to set bluetooth enabled property");
    }
#endif

    nsCOMPtr<nsIRunnable> ackTask = new BluetoothService::ToggleBtAck(mEnabled);
    if (NS_FAILED(NS_DispatchToMainThread(ackTask))) {
      NS_WARNING("Failed to dispatch to main thread!");
    }

    return NS_OK;
  }

private:
  bool mEnabled;
};

class BluetoothService::StartupTask : public nsISettingsServiceCallback
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD Handle(const nsAString& aName, const JS::Value& aResult)
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (!aResult.isBoolean()) {
      NS_WARNING("Setting for '" BLUETOOTH_ENABLED_SETTING "' is not a boolean!");
      return NS_OK;
    }

    // It is theoretically possible to shut down before the first settings check
    // has completed (though extremely unlikely).
    if (gBluetoothService) {
      return gBluetoothService->HandleStartupSettingsCheck(aResult.toBoolean());
    }

    return NS_OK;
  }

  NS_IMETHOD HandleError(const nsAString& aName)
  {
    NS_WARNING("Unable to get value for '" BLUETOOTH_ENABLED_SETTING "'");
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS1(BluetoothService::StartupTask, nsISettingsServiceCallback);

NS_IMPL_ISUPPORTS1(BluetoothService, nsIObserver)

bool
BluetoothService::IsToggling() const
{
  return gToggleInProgress;
}

BluetoothService::~BluetoothService()
{
  Cleanup();
}

PLDHashOperator
RemoveObserversExceptBluetoothManager
  (const nsAString& key,
   nsAutoPtr<BluetoothSignalObserverList>& value,
   void* arg)
{
  if (!key.EqualsLiteral(KEY_MANAGER)) {
    return PL_DHASH_REMOVE;
  }

  return PL_DHASH_NEXT;
}

void
BluetoothService::RemoveObserverFromTable(const nsAString& key)
{
  mBluetoothSignalObserverTable.Remove(key);
}

// static
BluetoothService*
BluetoothService::Create()
{
#if defined(MOZ_B2G_BT)
  if (!IsMainProcess()) {
    return BluetoothServiceChildProcess::Create();
  }
#endif

#if defined(MOZ_BLUETOOTH_GONK)
  return new BluetoothGonkService();
#elif defined(MOZ_BLUETOOTH_DBUS)
  return new BluetoothDBusService();
#endif
  NS_WARNING("No platform support for bluetooth!");
  return nullptr;
}

bool
BluetoothService::Init()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE(obs, false);

  if (NS_FAILED(obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                 false))) {
    NS_WARNING("Failed to add shutdown observer!");
    return false;
  }

  // Only the main process should observe bluetooth settings changes.
  if (IsMainProcess() &&
      NS_FAILED(obs->AddObserver(this, MOZSETTINGS_CHANGED_ID, false))) {
    NS_WARNING("Failed to add settings change observer!");
    return false;
  }

  return true;
}

void
BluetoothService::Cleanup()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs &&
      (NS_FAILED(obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) ||
       NS_FAILED(obs->RemoveObserver(this, MOZSETTINGS_CHANGED_ID)))) {
    NS_WARNING("Can't unregister observers!");
  }
}

void
BluetoothService::RegisterBluetoothSignalHandler(
                                              const nsAString& aNodeName,
                                              BluetoothSignalObserver* aHandler)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aHandler);

  BT_LOG("[S] %s: %s", __FUNCTION__, NS_ConvertUTF16toUTF8(aNodeName).get());

  BluetoothSignalObserverList* ol;
  if (!mBluetoothSignalObserverTable.Get(aNodeName, &ol)) {
    ol = new BluetoothSignalObserverList();
    mBluetoothSignalObserverTable.Put(aNodeName, ol);
  }

  ol->AddObserver(aHandler);
}

void
BluetoothService::UnregisterBluetoothSignalHandler(
                                              const nsAString& aNodeName,
                                              BluetoothSignalObserver* aHandler)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aHandler);

  BT_LOG("[S] %s: %s", __FUNCTION__, NS_ConvertUTF16toUTF8(aNodeName).get());

  BluetoothSignalObserverList* ol;
  if (mBluetoothSignalObserverTable.Get(aNodeName, &ol)) {
    ol->RemoveObserver(aHandler);
    if (ol->Length() == 0) {
      mBluetoothSignalObserverTable.Remove(aNodeName);
    }
  }
  else {
    NS_WARNING("Node was never registered!");
  }
}

PLDHashOperator
RemoveAllSignalHandlers(const nsAString& aKey,
                        nsAutoPtr<BluetoothSignalObserverList>& aData,
                        void* aUserArg)
{
  aData->RemoveObserver(static_cast<BluetoothSignalObserver*>(aUserArg));
  return aData->Length() ? PL_DHASH_NEXT : PL_DHASH_REMOVE;
}

void
BluetoothService::UnregisterAllSignalHandlers(BluetoothSignalObserver* aHandler)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aHandler);

  mBluetoothSignalObserverTable.Enumerate(RemoveAllSignalHandlers, aHandler);
}

void
BluetoothService::DistributeSignal(const BluetoothSignal& aSignal)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aSignal.path().EqualsLiteral(KEY_LOCAL_AGENT)) {
    Notify(aSignal);
    return;
  } else if (aSignal.path().EqualsLiteral(KEY_REMOTE_AGENT)) {
    Notify(aSignal);
    return;
  }

  BluetoothSignalObserverList* ol;
  if (!mBluetoothSignalObserverTable.Get(aSignal.path(), &ol)) {
#if DEBUG
    nsAutoCString msg("No observer registered for path ");
    msg.Append(NS_ConvertUTF16toUTF8(aSignal.path()));
    NS_WARNING(msg.get());
#endif
    return;
  }
  MOZ_ASSERT(ol->Length());
  ol->Broadcast(aSignal);
}

nsresult
BluetoothService::StartStopBluetooth(bool aStart)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (gInShutdown) {
    if (aStart) {
      // Don't try to start if we're already shutting down.
      MOZ_ASSERT(false, "Start called while in shutdown!");
      return NS_ERROR_FAILURE;
    }

    if (!mBluetoothCommandThread) {
      // Don't create a new thread after we've begun shutdown since bluetooth
      // can't be running.
      return NS_OK;
    }
  }

  nsresult rv;
  if (!mBluetoothCommandThread) {
    MOZ_ASSERT(!gInShutdown);

    rv = NS_NewNamedThread("BluetoothCmd",
                           getter_AddRefs(mBluetoothCommandThread));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIRunnable> runnable = new ToggleBtTask(aStart);
  rv = mBluetoothCommandThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
BluetoothService::SetEnabled(bool aEnabled)
{
  MOZ_ASSERT(NS_IsMainThread());

  AutoInfallibleTArray<BluetoothParent*, 10> childActors;
  GetAllBluetoothActors(childActors);

  for (uint32_t index = 0; index < childActors.Length(); index++) {
    unused << childActors[index]->SendEnabled(aEnabled);
  }

  if (!aEnabled) {
    /**
     * Remove all handlers except BluetoothManager when turning off bluetooth
     * since it is possible that the event 'onAdapterAdded' would be fired after
     * BluetoothManagers of child process are registered. Please see Bug 827759
     * for more details.
     */
    mBluetoothSignalObserverTable.Enumerate(
      RemoveObserversExceptBluetoothManager, nullptr);
  }

  /**
   * mEnabled: real status of bluetooth
   * aEnabled: expected status of bluetooth
   */
  if (mEnabled == aEnabled) {
    NS_WARNING("Bluetooth has already been enabled/disabled before\
                or the toggling is failed.");
  }

  mEnabled = aEnabled;

  gToggleInProgress = false;
}

nsresult
BluetoothService::HandleStartup()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!gToggleInProgress);

  nsCOMPtr<nsISettingsService> settings =
    do_GetService("@mozilla.org/settingsService;1");
  NS_ENSURE_TRUE(settings, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsISettingsServiceLock> settingsLock;
  nsresult rv = settings->CreateLock(getter_AddRefs(settingsLock));
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<StartupTask> callback = new StartupTask();
  rv = settingsLock->Get(BLUETOOTH_ENABLED_SETTING, callback);
  NS_ENSURE_SUCCESS(rv, rv);

  gToggleInProgress = true;
  return NS_OK;
}

nsresult
BluetoothService::HandleStartupSettingsCheck(bool aEnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aEnable) {
    return StartStopBluetooth(true);
  }

  /*
   * Since BLUETOOTH_ENABLED_SETTING is false, we don't have to turn on
   * bluetooth here, and set gToggleInProgress back to false.
   */
  gToggleInProgress = false;

  return NS_OK;
}

nsresult
BluetoothService::HandleSettingsChanged(const nsAString& aData)
{
  MOZ_ASSERT(NS_IsMainThread());

  // The string that we're interested in will be a JSON string that looks like:
  //  {"key":"bluetooth.enabled","value":true}

  JSContext* cx = nsContentUtils::GetSafeJSContext();
  if (!cx) {
    return NS_OK;
  }

  JS::Value val;
  if (!JS_ParseJSON(cx, aData.BeginReading(), aData.Length(), &val)) {
    return JS_ReportPendingException(cx) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
  }

  if (!val.isObject()) {
    return NS_OK;
  }

  JSObject& obj(val.toObject());

  JS::Value key;
  if (!JS_GetProperty(cx, &obj, "key", &key)) {
    MOZ_ASSERT(!JS_IsExceptionPending(cx));
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!key.isString()) {
    return NS_OK;
  }

  // First, check if the string equals to BLUETOOTH_DEBUGGING_SETTING
  JSBool match;
  if (!JS_StringEqualsAscii(cx, key.toString(), BLUETOOTH_DEBUGGING_SETTING, &match)) {
    MOZ_ASSERT(!JS_IsExceptionPending(cx));
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (match) {
    JS::Value value;
    if (!JS_GetProperty(cx, &obj, "value", &value)) {
      MOZ_ASSERT(!JS_IsExceptionPending(cx));
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!value.isBoolean()) {
      MOZ_ASSERT(false, "Expecting a boolean for 'bluetooth.debugging.enabled'!");
      return NS_ERROR_UNEXPECTED;
    }

    SWITCH_BT_DEBUG(value.toBoolean());

    return NS_OK;
  }

  // Second, check if the string is BLUETOOTH_ENABLED_SETTING
  if (!JS_StringEqualsAscii(cx, key.toString(), BLUETOOTH_ENABLED_SETTING, &match)) {
    MOZ_ASSERT(!JS_IsExceptionPending(cx));
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (match) {
    JS::Value value;
    if (!JS_GetProperty(cx, &obj, "value", &value)) {
      MOZ_ASSERT(!JS_IsExceptionPending(cx));
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!value.isBoolean()) {
      MOZ_ASSERT(false, "Expecting a boolean for 'bluetooth.enabled'!");
      return NS_ERROR_UNEXPECTED;
    }

    if (gToggleInProgress || value.toBoolean() == IsEnabled()) {
      // Nothing to do here.
      return NS_OK;
    }

    gToggleInProgress = true;

    nsresult rv = StartStopBluetooth(value.toBoolean());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
BluetoothService::HandleShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

  // This is a two phase shutdown. First we notify all child processes that
  // bluetooth is going away, and then we wait for them to acknowledge. Then we
  // close down all the bluetooth machinery.

  gInShutdown = true;

  Cleanup();

  AutoInfallibleTArray<BluetoothParent*, 10> childActors;
  GetAllBluetoothActors(childActors);

  if (!childActors.IsEmpty()) {
    // Notify child processes that they should stop using bluetooth now.
    for (uint32_t index = 0; index < childActors.Length(); index++) {
      childActors[index]->BeginShutdown();
    }

    // Create a timer to ensure that we don't wait forever for a child process
    // or the bluetooth threads to finish. If we don't get a timer or can't use
    // it for some reason then we skip all the waiting entirely since we really
    // can't afford to hang on shutdown.
    nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID);
    MOZ_ASSERT(timer);

    if (timer) {
      bool timeExceeded = false;

      if (NS_SUCCEEDED(timer->InitWithFuncCallback(ShutdownTimeExceeded,
                                                   &timeExceeded,
                                                   DEFAULT_SHUTDOWN_TIMER_MS,
                                                   nsITimer::TYPE_ONE_SHOT))) {
        nsIThread* currentThread = NS_GetCurrentThread();
        MOZ_ASSERT(currentThread);

        // Wait for those child processes to acknowledge.
        while (!timeExceeded && !childActors.IsEmpty()) {
          if (!NS_ProcessNextEvent(currentThread)) {
            MOZ_ASSERT(false, "Something horribly wrong here!");
            break;
          }
          GetAllBluetoothActors(childActors);
        }

        if (NS_FAILED(timer->Cancel())) {
          MOZ_NOT_REACHED("Failed to cancel shutdown timer, this will crash!");
        }
      }
      else {
        MOZ_ASSERT(false, "Failed to initialize shutdown timer!");
      }
    }
  }

  if (IsEnabled() && NS_FAILED(StartStopBluetooth(false))) {
    MOZ_ASSERT(false, "Failed to deliver stop message!");
  }

  return NS_OK;
}

// static
BluetoothService*
BluetoothService::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  // If we already exist, exit early
  if (gBluetoothService) {
    return gBluetoothService;
  }

  // If we're in shutdown, don't create a new instance
  if (gInShutdown) {
    NS_WARNING("BluetoothService can't be created during shutdown");
    return nullptr;
  }

  // Create new instance, register, return
  nsRefPtr<BluetoothService> service = BluetoothService::Create();
  NS_ENSURE_TRUE(service, nullptr);

  if (!service->Init()) {
    service->Cleanup();
    return nullptr;
  }

  gBluetoothService = service;
  return gBluetoothService;
}

nsresult
BluetoothService::Observe(nsISupports* aSubject, const char* aTopic,
                          const PRUnichar* aData)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!strcmp(aTopic, "profile-after-change")) {
    return HandleStartup();
  }

  if (!strcmp(aTopic, MOZSETTINGS_CHANGED_ID)) {
    return HandleSettingsChanged(nsDependentString(aData));
  }

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    return HandleShutdown();
  }

  MOZ_ASSERT(false, "BluetoothService got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

void
BluetoothService::Notify(const BluetoothSignal& aData)
{
  nsString type;

  JSContext* cx = nsContentUtils::GetSafeJSContext();
  NS_ASSERTION(!::JS_IsExceptionPending(cx),
               "Shouldn't get here when an exception is pending!");

  JSAutoRequest jsar(cx);
  JSObject* obj = JS_NewObject(cx, NULL, NULL, NULL);
  if (!obj) {
    NS_WARNING("Failed to new JSObject for system message!");
    return;
  }

  if (!SetJsObject(cx, aData.value(), obj)) {
    NS_WARNING("Failed to set properties of system message!");
    return;
  }

  BT_LOG("[S] %s: %s", __FUNCTION__, NS_ConvertUTF16toUTF8(aData.name()).get());

  if (aData.name().EqualsLiteral("RequestConfirmation")) {
    NS_ASSERTION(aData.value().get_ArrayOfBluetoothNamedValue().Length() == 3,
      "RequestConfirmation: Wrong length of parameters");
    type.AssignLiteral("bluetooth-requestconfirmation");
  } else if (aData.name().EqualsLiteral("RequestPinCode")) {
    NS_ASSERTION(aData.value().get_ArrayOfBluetoothNamedValue().Length() == 2,
      "RequestPinCode: Wrong length of parameters");
    type.AssignLiteral("bluetooth-requestpincode");
  } else if (aData.name().EqualsLiteral("RequestPasskey")) {
    NS_ASSERTION(aData.value().get_ArrayOfBluetoothNamedValue().Length() == 2,
      "RequestPinCode: Wrong length of parameters");
    type.AssignLiteral("bluetooth-requestpasskey");
  } else if (aData.name().EqualsLiteral("Authorize")) {
    NS_ASSERTION(aData.value().get_ArrayOfBluetoothNamedValue().Length() == 2,
      "Authorize: Wrong length of parameters");
    type.AssignLiteral("bluetooth-authorize");
  } else if (aData.name().EqualsLiteral("Cancel")) {
    NS_ASSERTION(aData.value().get_ArrayOfBluetoothNamedValue().Length() == 0,
      "Cancel: Wrong length of parameters");
    type.AssignLiteral("bluetooth-cancel");
  } else if (aData.name().EqualsLiteral("PairedStatusChanged")) {
    NS_ASSERTION(aData.value().get_ArrayOfBluetoothNamedValue().Length() == 1,
      "PairedStatusChagned: Wrong length of parameters");
    type.AssignLiteral("bluetooth-pairedstatuschanged");
  } else {
#ifdef DEBUG
    nsCString warningMsg;
    warningMsg.AssignLiteral("Not handling service signal: ");
    warningMsg.Append(NS_ConvertUTF16toUTF8(aData.name()));
    NS_WARNING(warningMsg.get());
#endif
  }

  nsCOMPtr<nsISystemMessagesInternal> systemMessenger =
    do_GetService("@mozilla.org/system-message-internal;1");
  NS_ENSURE_TRUE_VOID(systemMessenger);

  systemMessenger->BroadcastMessage(type, OBJECT_TO_JSVAL(obj));
}
