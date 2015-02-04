/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BluetoothService.h"

#include "BluetoothCommon.h"
#include "BluetoothParent.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothServiceChildProcess.h"
#include "BluetoothUtils.h"

#include "jsapi.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/unused.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/dom/ipc/BlobChild.h"
#include "mozilla/dom/ipc/BlobParent.h"
#include "nsContentUtils.h"
#include "nsIObserverService.h"
#include "nsISettingsService.h"
#include "nsISystemMessagesInternal.h"
#include "nsITimer.h"
#include "nsServiceManagerUtils.h"
#include "nsXPCOM.h"
#include "mozilla/dom/SettingChangeNotificationBinding.h"

#if defined(MOZ_WIDGET_GONK)
#include "cutils/properties.h"
#endif

#if defined(MOZ_B2G_BT)
#if defined(MOZ_B2G_BT_BLUEZ)
/**
 * B2G blueZ:
 *   MOZ_B2G_BT and MOZ_B2G_BT_BLUEZ are both defined.
 */
#include "BluetoothDBusService.h"
#elif defined(MOZ_B2G_BT_BLUEDROID)
/**
 * B2G bluedroid:
 *   MOZ_B2G_BT and MOZ_B2G_BT_BLUEDROID are both defined;
 *   MOZ_B2G_BLUEZ or MOZ_B2G_DAEMON are not defined.
 */
#include "BluetoothServiceBluedroid.h"
#elif defined(MOZ_B2G_BT_DAEMON)
/**
 * B2G Bluetooth daemon:
 *   MOZ_B2G_BT, MOZ_B2G_BLUEDROID and MOZ_B2G_BT_DAEMON are defined;
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

#define DEFAULT_SHUTDOWN_TIMER_MS 5000

bool gBluetoothDebugFlag = false;

using namespace mozilla;
using namespace mozilla::dom;
USING_BLUETOOTH_NAMESPACE

namespace {

StaticRefPtr<BluetoothService> sBluetoothService;

bool sInShutdown = false;
bool sToggleInProgress = false;

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

BluetoothService::ToggleBtAck::ToggleBtAck(bool aEnabled)
  : mEnabled(aEnabled)
{ }

NS_METHOD
BluetoothService::ToggleBtAck::Run()
{
  BluetoothService::AcknowledgeToggleBt(mEnabled);

  return NS_OK;
}

class BluetoothService::StartupTask MOZ_FINAL : public nsISettingsServiceCallback
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

NS_IMPL_ISUPPORTS(BluetoothService::StartupTask, nsISettingsServiceCallback);

NS_IMPL_ISUPPORTS(BluetoothService, nsIObserver)

bool
BluetoothService::IsToggling() const
{
  return sToggleInProgress;
}

BluetoothService::~BluetoothService()
{
  Cleanup();
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
  return new BluetoothDBusService();
#elif defined(MOZ_B2G_BT_BLUEDROID)
  return new BluetoothServiceBluedroid();
#elif defined(MOZ_B2G_BT_DAEMON)
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

  // Distribute pending pairing requests when pairing listener has been added
  // to signal observer table.
  if (IsMainProcess() &&
      !mPendingPairReqSignals.IsEmpty() &&
      aNodeName.EqualsLiteral(KEY_PAIRING_LISTENER)) {
    for (uint32_t i = 0; i < mPendingPairReqSignals.Length(); ++i) {
      DistributeSignal(mPendingPairReqSignals[i]);
    }
    mPendingPairReqSignals.Clear();
  }
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

  BluetoothSignalObserverList* ol;
  if (!mBluetoothSignalObserverTable.Get(aSignal.path(), &ol)) {
    // If there is no BluetoohPairingListener in observer table, put the signal
    // into a pending queue of pairing requests and send a system message to
    // launch bluetooth certified app.
    if (aSignal.path().EqualsLiteral(KEY_PAIRING_LISTENER)) {
      mPendingPairReqSignals.AppendElement(aSignal);

      BT_ENSURE_TRUE_VOID_BROADCAST_SYSMSG(
        NS_LITERAL_STRING(SYS_MSG_BT_PAIRING_REQ),
        BluetoothValue(EmptyString()));
    } else {
      BT_WARNING("No observer registered for path %s",
                 NS_ConvertUTF16toUTF8(aSignal.path()).get());
    }
    return;
  }

  MOZ_ASSERT(ol->Length());
  ol->Broadcast(aSignal);
}

nsresult
BluetoothService::StartBluetooth(bool aIsStartup,
                                 BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (sInShutdown) {
    // Don't try to start if we're already shutting down.
    MOZ_ASSERT(false, "Start called while in shutdown!");
    return NS_ERROR_FAILURE;
  }

  /* When IsEnabled() is true, we don't switch on Bluetooth but we still
   * send ToggleBtAck task. One special case happens at startup stage. At
   * startup, the initialization of BluetoothService still has to be done
   * even if Bluetooth is already enabled.
   *
   * Please see bug 892392 for more information.
   */
  if (aIsStartup || !IsEnabled()) {
    // Switch Bluetooth on
    nsresult rv = StartInternal(aRunnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("Bluetooth service failed to start!");
      return rv;
    }
  } else {
    BT_WARNING("Bluetooth has already been enabled before.");
    nsRefPtr<nsRunnable> runnable = new BluetoothService::ToggleBtAck(true);
    if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
      BT_WARNING("Failed to dispatch to main thread!");
    }
  }

  return NS_OK;
}

nsresult
BluetoothService::StopBluetooth(bool aIsStartup,
                                BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  /* When IsEnabled() is false, we don't switch off Bluetooth but we still
   * send ToggleBtAck task. One special case happens at startup stage. At
   * startup, the initialization of BluetoothService still has to be done
   * even if Bluetooth is disabled.
   *
   * Please see bug 892392 for more information.
   */
  if (aIsStartup || IsEnabled()) {
    // Any connected Bluetooth profile would be disconnected.
    nsresult rv = StopInternal(aRunnable);
    if (NS_FAILED(rv)) {
      BT_WARNING("Bluetooth service failed to stop!");
      return rv;
    }
  } else {
    BT_WARNING("Bluetooth has already been enabled/disabled before.");
    nsRefPtr<nsRunnable> runnable = new BluetoothService::ToggleBtAck(false);
    if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
      BT_WARNING("Failed to dispatch to main thread!");
    }
  }

  return NS_OK;
}

nsresult
BluetoothService::StartStopBluetooth(bool aStart,
                                     bool aIsStartup,
                                     BluetoothReplyRunnable* aRunnable)
{
  nsresult rv;
  if (aStart) {
    rv = StartBluetooth(aIsStartup, aRunnable);
  } else {
    rv = StopBluetooth(aIsStartup, aRunnable);
  }
  return rv;
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

  /**
   * mEnabled: real status of bluetooth
   * aEnabled: expected status of bluetooth
   */
  if (mEnabled == aEnabled) {
    BT_WARNING("Bluetooth is already %s, or the toggling failed.",
               mEnabled ? "enabled" : "disabled");
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
  nsresult rv = settings->CreateLock(nullptr, getter_AddRefs(settingsLock));
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
  return StartStopBluetooth(aEnable, true, nullptr);
}

nsresult
BluetoothService::HandleSettingsChanged(nsISupports* aSubject)
{
  MOZ_ASSERT(NS_IsMainThread());

  // The string that we're interested in will be a JSON string that looks like:
  //  {"key":"bluetooth.enabled","value":true}

  RootedDictionary<SettingChangeNotification> setting(nsContentUtils::RootingCx());
  if (!WrappedJSToDictionary(aSubject, setting)) {
    return NS_OK;
  }
  if (!setting.mKey.EqualsASCII(BLUETOOTH_DEBUGGING_SETTING)) {
    return NS_OK;
  }
  if (!setting.mValue.isBoolean()) {
    MOZ_ASSERT(false, "Expecting a boolean for 'bluetooth.debugging.enabled'!");
    return NS_ERROR_UNEXPECTED;
  }

  SWITCH_BT_DEBUG(setting.mValue.toBoolean());

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

  if (IsEnabled() && NS_FAILED(StopBluetooth(false, nullptr))) {
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
  sBluetoothService = BluetoothService::Create();
  NS_ENSURE_TRUE(sBluetoothService, nullptr);

  if (!sBluetoothService->Init()) {
    sBluetoothService->Cleanup();
    return nullptr;
  }

  ClearOnShutdown(&sBluetoothService);
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
    return HandleSettingsChanged(aSubject);
  }

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    return HandleShutdown();
  }

  MOZ_ASSERT(false, "BluetoothService got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

/**
 * Enable/Disable the local adapter.
 *
 * There is only one adapter on the mobile in current use cases.
 * In addition, bluedroid couldn't enable/disable a single adapter.
 * So currently we will turn on/off BT to enable/disable the adapter.
 *
 * TODO: To support enable/disable single adapter in the future,
 *       we will need to implement EnableDisableInternal for different stacks.
 */
nsresult
BluetoothService::EnableDisable(bool aEnable,
                                BluetoothReplyRunnable* aRunnable)
{
  sToggleInProgress = true;
  return StartStopBluetooth(aEnable, false, aRunnable);
}

void
BluetoothService::FireAdapterStateChanged(bool aEnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  InfallibleTArray<BluetoothNamedValue> props;
  BT_APPEND_NAMED_VALUE(props, "State", aEnable);
  BluetoothValue value(props);

  BluetoothSignal signal(NS_LITERAL_STRING("PropertyChanged"),
                         NS_LITERAL_STRING(KEY_ADAPTER), value);
  DistributeSignal(signal);
}

void
BluetoothService::AcknowledgeToggleBt(bool aEnabled)
{
  MOZ_ASSERT(NS_IsMainThread());

#if defined(MOZ_WIDGET_GONK)
  // This is requested in Bug 836516. With settings this property, WLAN
  // firmware could be aware of Bluetooth has been turned on/off, so that the
  // mecahnism of handling coexistence of WIFI and Bluetooth could be started.
  //
  // In the future, we may have our own way instead of setting a system
  // property to let firmware developers be able to sense that Bluetooth has
  // been toggled.
  if (property_set(PROP_BLUETOOTH_ENABLED, aEnabled ? "true" : "false") != 0) {
    BT_WARNING("Failed to set bluetooth enabled property");
  }
#endif

  if (sInShutdown) {
    sBluetoothService = nullptr;
    return;
  }

  NS_ENSURE_TRUE_VOID(sBluetoothService);

  sBluetoothService->CompleteToggleBt(aEnabled);
}

void
BluetoothService::CompleteToggleBt(bool aEnabled)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Update |mEnabled| of |BluetoothService| object since
  // |StartInternal| and |StopInternal| have been already
  // done.
  SetEnabled(aEnabled);
  sToggleInProgress = false;

  FireAdapterStateChanged(aEnabled);
}
