/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BluetoothService.h"

#include "BluetoothCommon.h"
#include "BluetoothA2dpManager.h"
#include "BluetoothHfpManager.h"
#include "BluetoothHidManager.h"
#include "BluetoothManager.h"
#include "BluetoothOppManager.h"
#include "BluetoothParent.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothServiceChildProcess.h"
#include "BluetoothUtils.h"

#include "jsapi.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/unused.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/ipc/UnixSocket.h"
#include "mozilla/LazyIdleThread.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "nsIObserverService.h"
#include "nsISettingsService.h"
#include "nsISystemMessagesInternal.h"
#include "nsITimer.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"

#if defined(MOZ_WIDGET_GONK)
#include "cutils/properties.h"
#endif

#if defined(MOZ_B2G_BT)
#if defined(MOZ_B2G_BT_BLUEZ)
/**
 * B2G blueZ:
 *   MOZ_B2G_BT and MOZ_B2G_BT_BLUEZ are both defined.
 */
#include "BluetoothGonkService.h"
#elif defined(MOZ_B2G_BT_BLUEDROID)
/**
 * B2G bluedroid:
 *   MOZ_B2G_BT and MOZ_B2G_BT_BLUEDROID are both defined;
 *   MOZ_B2G_BLUEZ is not defined.
 */
#include "BluetoothServiceBluedroid.h"
#endif
#elif defined(MOZ_BLUETOOTH_DBUS)
/**
 * Desktop bluetooth:
 *   MOZ_B2G_BT is not defined; MOZ_BLUETOOTH_DBUS is defined.
 */
#include "BluetoothDBusService.h"
#else
#error No backend
#endif

#define MOZSETTINGS_CHANGED_ID      "mozsettings-changed"
#define BLUETOOTH_ENABLED_SETTING   "bluetooth.enabled"
#define BLUETOOTH_DEBUGGING_SETTING "bluetooth.debugging.enabled"

#define PROP_BLUETOOTH_ENABLED      "bluetooth.isEnabled"

#define DEFAULT_THREAD_TIMEOUT_MS 3000
#define DEFAULT_SHUTDOWN_TIMER_MS 5000

bool gBluetoothDebugFlag = false;

using namespace mozilla;
using namespace mozilla::dom;
USING_BLUETOOTH_NAMESPACE

namespace {

StaticRefPtr<BluetoothService> sBluetoothService;

bool sInShutdown = false;
bool sToggleInProgress = false;

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

    NS_ENSURE_TRUE(sBluetoothService, NS_OK);

    if (sInShutdown) {
      sBluetoothService = nullptr;
      return NS_OK;
    }

    // Update mEnabled of BluetoothService object since
    // StartInternal/StopInternal have been already done.
    sBluetoothService->SetEnabled(mEnabled);
    sToggleInProgress = false;

    nsAutoString signalName;
    signalName = mEnabled ? NS_LITERAL_STRING("Enabled")
                          : NS_LITERAL_STRING("Disabled");
    BluetoothSignal signal(signalName, NS_LITERAL_STRING(KEY_MANAGER), true);
    sBluetoothService->DistributeSignal(signal);

    // Event 'AdapterAdded' has to be fired after firing 'Enabled'
    sBluetoothService->TryFiringAdapterAdded();

    return NS_OK;
  }

private:
  bool mEnabled;
};

class BluetoothService::ToggleBtTask : public nsRunnable
{
public:
  ToggleBtTask(bool aEnabled, bool aIsStartup)
    : mEnabled(aEnabled)
    , mIsStartup(aIsStartup)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    /**
     * mEnabled: expected status of bluetooth
     * sBluetoothService->IsEnabled(): real status of bluetooth
     *
     * When two values are the same, we don't switch on/off bluetooth
     * but we still do ToggleBtAck task. One special case happens at startup
     * stage. At startup, the initialization of BluetoothService still has to
     * be done even if mEnabled is equal to the status of Bluetooth firmware.
     *
     * Please see bug 892392 for more information.
     */
    if (!mIsStartup && mEnabled == sBluetoothService->IsEnabledInternal()) {
      BT_WARNING("Bluetooth has already been enabled/disabled before.");
    } else {
      // Switch on/off bluetooth
      if (mEnabled) {
        if (NS_FAILED(sBluetoothService->StartInternal())) {
          BT_WARNING("Bluetooth service failed to start!");
          mEnabled = !mEnabled;
        }
      } else {
        if (NS_FAILED(sBluetoothService->StopInternal())) {
          BT_WARNING("Bluetooth service failed to stop!");
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
      BT_WARNING("Failed to set bluetooth enabled property");
    }
#endif

    nsCOMPtr<nsIRunnable> ackTask = new BluetoothService::ToggleBtAck(mEnabled);
    if (NS_FAILED(NS_DispatchToMainThread(ackTask))) {
      BT_WARNING("Failed to dispatch to main thread!");
    }

    return NS_OK;
  }

private:
  bool mEnabled;
  bool mIsStartup;
};

class BluetoothService::StartupTask : public nsISettingsServiceCallback
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD Handle(const nsAString& aName, JS::Handle<JS::Value> aResult)
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (!aResult.isBoolean()) {
      BT_WARNING("Setting for '" BLUETOOTH_ENABLED_SETTING "' is not a boolean!");
      return NS_OK;
    }

    // It is theoretically possible to shut down before the first settings check
    // has completed (though extremely unlikely).
    if (sBluetoothService) {
      return sBluetoothService->HandleStartupSettingsCheck(aResult.toBoolean());
    }

    return NS_OK;
  }

  NS_IMETHOD HandleError(const nsAString& aName)
  {
    BT_WARNING("Unable to get value for '" BLUETOOTH_ENABLED_SETTING "'");
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS1(BluetoothService::StartupTask, nsISettingsServiceCallback);

NS_IMPL_ISUPPORTS1(BluetoothService, nsIObserver)

bool
BluetoothService::IsToggling() const
{
  return sToggleInProgress;
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

#if defined(MOZ_B2G_BT_BLUEZ)
  return new BluetoothGonkService();
#elif defined(MOZ_B2G_BT_BLUEDROID)
  return new BluetoothServiceBluedroid();
#endif
#elif defined(MOZ_BLUETOOTH_DBUS)
  return new BluetoothDBusService();
#endif

  BT_WARNING("No platform support for bluetooth!");
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
    BT_WARNING("Failed to add shutdown observer!");
    return false;
  }

  // Only the main process should observe bluetooth settings changes.
  if (IsMainProcess() &&
      NS_FAILED(obs->AddObserver(this, MOZSETTINGS_CHANGED_ID, false))) {
    BT_WARNING("Failed to add settings change observer!");
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
    BT_WARNING("Can't unregister observers!");
  }
}

void
BluetoothService::RegisterBluetoothSignalHandler(
                                              const nsAString& aNodeName,
                                              BluetoothSignalObserver* aHandler)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aHandler);

  BT_LOGD("[S] %s: %s", __FUNCTION__, NS_ConvertUTF16toUTF8(aNodeName).get());

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

  BT_LOGD("[S] %s: %s", __FUNCTION__, NS_ConvertUTF16toUTF8(aNodeName).get());

  BluetoothSignalObserverList* ol;
  if (mBluetoothSignalObserverTable.Get(aNodeName, &ol)) {
    ol->RemoveObserver(aHandler);
    // We shouldn't have duplicate instances in the ObserverList, but there's
    // no appropriate way to do duplication check while registering, so
    // assertions are added here.
    MOZ_ASSERT(!ol->RemoveObserver(aHandler));
    if (ol->Length() == 0) {
      mBluetoothSignalObserverTable.Remove(aNodeName);
    }
  }
  else {
    BT_WARNING("Node was never registered!");
  }
}

PLDHashOperator
RemoveAllSignalHandlers(const nsAString& aKey,
                        nsAutoPtr<BluetoothSignalObserverList>& aData,
                        void* aUserArg)
{
  BluetoothSignalObserver* handler = static_cast<BluetoothSignalObserver*>(aUserArg);
  aData->RemoveObserver(handler);
  // We shouldn't have duplicate instances in the ObserverList, but there's
  // no appropriate way to do duplication check while registering, so
  // assertions are added here.
  MOZ_ASSERT(!aData->RemoveObserver(handler));
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
    BT_WARNING(msg.get());
#endif
    return;
  }
  MOZ_ASSERT(ol->Length());
  ol->Broadcast(aSignal);
}

nsresult
BluetoothService::StartStopBluetooth(bool aStart, bool aIsStartup)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (sInShutdown) {
    if (aStart) {
      // Don't try to start if we're already shutting down.
      MOZ_ASSERT(false, "Start called while in shutdown!");
      return NS_ERROR_FAILURE;
    }

    if (!mBluetoothThread) {
      // Don't create a new thread after we've begun shutdown since bluetooth
      // can't be running.
      return NS_OK;
    }
  }

  if (!aStart) {
    BluetoothProfileManagerBase* profile;
    profile = BluetoothHfpManager::Get();
    NS_ENSURE_TRUE(profile, NS_ERROR_FAILURE);
    if (profile->IsConnected()) {
      profile->Disconnect(nullptr);
    }

    profile = BluetoothOppManager::Get();
    NS_ENSURE_TRUE(profile, NS_ERROR_FAILURE);
    if (profile->IsConnected()) {
      profile->Disconnect(nullptr);
    }

    profile = BluetoothA2dpManager::Get();
    NS_ENSURE_TRUE(profile, NS_ERROR_FAILURE);
    if (profile->IsConnected()) {
      profile->Disconnect(nullptr);
    }

    profile = BluetoothHidManager::Get();
    NS_ENSURE_TRUE(profile, NS_ERROR_FAILURE);
    if (profile->IsConnected()) {
      profile->Disconnect(nullptr);
    }
  }

  if (!mBluetoothThread) {
    mBluetoothThread = new LazyIdleThread(DEFAULT_THREAD_TIMEOUT_MS,
                                          NS_LITERAL_CSTRING("Bluetooth"),
                                          LazyIdleThread::ManualShutdown);
  }

  mAdapterAddedReceived = false;

  nsCOMPtr<nsIRunnable> runnable = new ToggleBtTask(aStart, aIsStartup);
  nsresult rv = mBluetoothThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
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
    BT_WARNING("Bluetooth has already been enabled/disabled before"
               "or the toggling is failed.");
  }

  mEnabled = aEnabled;
}

nsresult
BluetoothService::HandleStartup()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sToggleInProgress);

  nsCOMPtr<nsISettingsService> settings =
    do_GetService("@mozilla.org/settingsService;1");
  NS_ENSURE_TRUE(settings, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsISettingsServiceLock> settingsLock;
  nsresult rv = settings->CreateLock(getter_AddRefs(settingsLock));
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<StartupTask> callback = new StartupTask();
  rv = settingsLock->Get(BLUETOOTH_ENABLED_SETTING, callback);
  NS_ENSURE_SUCCESS(rv, rv);

  sToggleInProgress = true;
  return NS_OK;
}

nsresult
BluetoothService::HandleStartupSettingsCheck(bool aEnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  return StartStopBluetooth(aEnable, true);
}

nsresult
BluetoothService::HandleSettingsChanged(const nsAString& aData)
{
  MOZ_ASSERT(NS_IsMainThread());

  // The string that we're interested in will be a JSON string that looks like:
  //  {"key":"bluetooth.enabled","value":true}

  AutoSafeJSContext cx;
  if (!cx) {
    return NS_OK;
  }

  JS::Rooted<JS::Value> val(cx);
  if (!JS_ParseJSON(cx, aData.BeginReading(), aData.Length(), &val)) {
    return JS_ReportPendingException(cx) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
  }

  if (!val.isObject()) {
    return NS_OK;
  }

  JSObject& obj(val.toObject());

  JS::Rooted<JS::Value> key(cx);
  if (!JS_GetProperty(cx, &obj, "key", &key)) {
    MOZ_ASSERT(!JS_IsExceptionPending(cx));
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!key.isString()) {
    return NS_OK;
  }

  // First, check if the string equals to BLUETOOTH_DEBUGGING_SETTING
  bool match;
  if (!JS_StringEqualsAscii(cx, key.toString(), BLUETOOTH_DEBUGGING_SETTING, &match)) {
    MOZ_ASSERT(!JS_IsExceptionPending(cx));
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (match) {
    JS::Rooted<JS::Value> value(cx);
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
    JS::Rooted<JS::Value> value(cx);
    if (!JS_GetProperty(cx, &obj, "value", &value)) {
      MOZ_ASSERT(!JS_IsExceptionPending(cx));
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!value.isBoolean()) {
      MOZ_ASSERT(false, "Expecting a boolean for 'bluetooth.enabled'!");
      return NS_ERROR_UNEXPECTED;
    }

    if (sToggleInProgress || value.toBoolean() == IsEnabled()) {
      // Nothing to do here.
      return NS_OK;
    }

    sToggleInProgress = true;

    nsresult rv = StartStopBluetooth(value.toBoolean(), false);
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

  sInShutdown = true;

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
          MOZ_CRASH("Failed to cancel shutdown timer, this will crash!");
        }
      }
      else {
        MOZ_ASSERT(false, "Failed to initialize shutdown timer!");
      }
    }
  }

  if (IsEnabled() && NS_FAILED(StartStopBluetooth(false, false))) {
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
  if (sBluetoothService) {
    return sBluetoothService;
  }

  // If we're in shutdown, don't create a new instance
  if (sInShutdown) {
    BT_WARNING("BluetoothService can't be created during shutdown");
    return nullptr;
  }

  // Create new instance, register, return
  nsRefPtr<BluetoothService> service = BluetoothService::Create();
  NS_ENSURE_TRUE(service, nullptr);

  if (!service->Init()) {
    service->Cleanup();
    return nullptr;
  }

  sBluetoothService = service;
  return sBluetoothService;
}

nsresult
BluetoothService::Observe(nsISupports* aSubject, const char* aTopic,
                          const char16_t* aData)
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
BluetoothService::TryFiringAdapterAdded()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (IsToggling() || !mAdapterAddedReceived) {
    return;
  }

  BluetoothSignal signal(NS_LITERAL_STRING("AdapterAdded"),
                         NS_LITERAL_STRING(KEY_MANAGER), true);
  DistributeSignal(signal);
}

void
BluetoothService::AdapterAddedReceived()
{
  MOZ_ASSERT(NS_IsMainThread());

  mAdapterAddedReceived = true;
}

void
BluetoothService::Notify(const BluetoothSignal& aData)
{
  nsString type = NS_LITERAL_STRING("bluetooth-pairing-request");

  AutoSafeJSContext cx;
  JS::Rooted<JSObject*> obj(cx, JS_NewObject(cx, nullptr, JS::NullPtr(),
                                             JS::NullPtr()));
  NS_ENSURE_TRUE_VOID(obj);

  if (!SetJsObject(cx, aData.value(), obj)) {
    BT_WARNING("Failed to set properties of system message!");
    return;
  }

  BT_LOGD("[S] %s: %s", __FUNCTION__, NS_ConvertUTF16toUTF8(aData.name()).get());

  if (aData.name().EqualsLiteral("RequestConfirmation")) {
    MOZ_ASSERT(aData.value().get_ArrayOfBluetoothNamedValue().Length() == 4,
      "RequestConfirmation: Wrong length of parameters");
  } else if (aData.name().EqualsLiteral("RequestPinCode")) {
    MOZ_ASSERT(aData.value().get_ArrayOfBluetoothNamedValue().Length() == 3,
      "RequestPinCode: Wrong length of parameters");
  } else if (aData.name().EqualsLiteral("RequestPasskey")) {
    MOZ_ASSERT(aData.value().get_ArrayOfBluetoothNamedValue().Length() == 3,
      "RequestPinCode: Wrong length of parameters");
  } else if (aData.name().EqualsLiteral("Cancel")) {
    MOZ_ASSERT(aData.value().get_ArrayOfBluetoothNamedValue().Length() == 0,
      "Cancel: Wrong length of parameters");
    type.AssignLiteral("bluetooth-cancel");
  } else if (aData.name().EqualsLiteral(PAIRED_STATUS_CHANGED_ID)) {
    MOZ_ASSERT(aData.value().get_ArrayOfBluetoothNamedValue().Length() == 1,
      "pairedstatuschanged: Wrong length of parameters");
    type.AssignLiteral("bluetooth-pairedstatuschanged");
  } else {
    nsCString warningMsg;
    warningMsg.AssignLiteral("Not handling service signal: ");
    warningMsg.Append(NS_ConvertUTF16toUTF8(aData.name()));
    BT_WARNING(warningMsg.get());
    return;
  }

  nsCOMPtr<nsISystemMessagesInternal> systemMessenger =
    do_GetService("@mozilla.org/system-message-internal;1");
  NS_ENSURE_TRUE_VOID(systemMessenger);

  JS::Rooted<JS::Value> value(cx, JS::ObjectValue(*obj));
  systemMessenger->BroadcastMessage(type, value,
                                    JS::UndefinedHandleValue);
}
