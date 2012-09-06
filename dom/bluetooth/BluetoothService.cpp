/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BluetoothService.h"
#include "BluetoothManager.h"
#include "BluetoothTypes.h"
#include "BluetoothReplyRunnable.h"

#include "jsapi.h"
#include "mozilla/Services.h"
#include "mozilla/Util.h"
#include "nsContentUtils.h"
#include "nsIDOMDOMRequest.h"
#include "nsIObserverService.h"
#include "nsISettingsService.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "nsXPCOMCIDInternal.h"

#define MOZSETTINGS_CHANGED_ID "mozsettings-changed"
#define BLUETOOTH_ENABLED_SETTING "bluetooth.enabled"

using namespace mozilla;

USING_BLUETOOTH_NAMESPACE

static nsRefPtr<BluetoothService> gBluetoothService;
static bool gInShutdown = false;

NS_IMPL_ISUPPORTS1(BluetoothService, nsIObserver)

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
      // Notify all the managers about the state change.
      gBluetoothService->SetEnabled(mEnabled);
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

    if (mEnabled) {
      if (NS_FAILED(gBluetoothService->StartInternal())) {
        NS_WARNING("Bluetooth service failed to start!");
        mEnabled = !mEnabled;
      }
    }
    else if (NS_FAILED(gBluetoothService->StopInternal())) {
      NS_WARNING("Bluetooth service failed to stop!");
      mEnabled = !mEnabled;
    }

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

  NS_IMETHOD Handle(const nsAString& aName, const jsval& aResult, JSContext* aCx)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(gBluetoothService);

    if (!aResult.isBoolean()) {
      NS_WARNING("Setting for '" BLUETOOTH_ENABLED_SETTING "' is not a boolean!");
      return NS_OK;
    }

    return aResult.toBoolean() ? gBluetoothService->Start() : NS_OK;
  }

  NS_IMETHOD HandleError(const nsAString& aName, JSContext* aCx)
  {
    NS_WARNING("Unable to get value for '" BLUETOOTH_ENABLED_SETTING "'");
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS1(BluetoothService::StartupTask, nsISettingsServiceCallback);

nsresult
BluetoothService::RegisterBluetoothSignalHandler(const nsAString& aNodeName,
                                                 BluetoothSignalObserver* aHandler)
{
  MOZ_ASSERT(NS_IsMainThread());
  BluetoothSignalObserverList* ol;
  if (!mBluetoothSignalObserverTable.Get(aNodeName, &ol)) {
    ol = new BluetoothSignalObserverList();
    mBluetoothSignalObserverTable.Put(aNodeName, ol);
  }
  ol->AddObserver(aHandler);
  return NS_OK;
}

nsresult
BluetoothService::UnregisterBluetoothSignalHandler(const nsAString& aNodeName,
                                                   BluetoothSignalObserver* aHandler)
{
  MOZ_ASSERT(NS_IsMainThread());
  BluetoothSignalObserverList* ol;
  if (!mBluetoothSignalObserverTable.Get(aNodeName, &ol)) {
    NS_WARNING("Node does not exist to remove BluetoothSignalListener from!");
    return NS_OK;
  }
  ol->RemoveObserver(aHandler);
  if (ol->Length() == 0) {
    mBluetoothSignalObserverTable.Remove(aNodeName);
  }
  return NS_OK;
}

nsresult
BluetoothService::DistributeSignal(const BluetoothSignal& signal)
{
  MOZ_ASSERT(NS_IsMainThread());
  // Notify observers that a message has been sent
  BluetoothSignalObserverList* ol;
  if (!mBluetoothSignalObserverTable.Get(signal.path(), &ol)) {
#if DEBUG
    nsString msg;
    msg.AssignLiteral("No observer registered for path");
    msg.Append(signal.path());
    NS_WARNING(NS_ConvertUTF16toUTF8(msg).get());
#endif
    return NS_OK;
  }
#if DEBUG
  if (ol->Length() == 0) {
    NS_WARNING("Distributing to observer list of 0");
  }
#endif
  ol->Broadcast(signal);
  return NS_OK;
}

nsresult
BluetoothService::StartStopBluetooth(bool aStart)
{
  MOZ_ASSERT(NS_IsMainThread());

#ifdef DEBUG
  if (aStart && mLastRequestedEnable) {
    MOZ_ASSERT(false, "Calling Start twice in a row!");
  }
  else if (!aStart && !mLastRequestedEnable) {
    MOZ_ASSERT(false, "Calling Stop twice in a row!");
  }
  mLastRequestedEnable = aStart;
#endif

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

  if (aEnabled == mEnabled) {
    // Nothing to do, maybe something failed.
    return;
  }

  mEnabled = aEnabled;

  BluetoothManagerList::ForwardIterator iter(mLiveManagers);
  while (iter.HasMore()) {
    if (NS_FAILED(iter.GetNext()->FireEnabledDisabledEvent(mEnabled))) {
      NS_WARNING("FireEnabledDisabledEvent failed!");
    }
  }
}

nsresult
BluetoothService::Start()
{
  MOZ_ASSERT(NS_IsMainThread());
  return StartStopBluetooth(true);
}

nsresult
BluetoothService::Stop()
{
  MOZ_ASSERT(NS_IsMainThread());
  return StartStopBluetooth(false);
}

nsresult
BluetoothService::HandleStartup()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsISettingsService> settings =
    do_GetService("@mozilla.org/settingsService;1");
  NS_ENSURE_TRUE(settings, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsISettingsServiceLock> settingsLock;
  nsresult rv = settings->GetLock(getter_AddRefs(settingsLock));
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<StartupTask> callback = new StartupTask();
  rv = settingsLock->Get(BLUETOOTH_ENABLED_SETTING, callback);
  NS_ENSURE_SUCCESS(rv, rv);

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

  JSBool match;
  if (!JS_StringEqualsAscii(cx, key.toString(), BLUETOOTH_ENABLED_SETTING,
                            &match)) {
    MOZ_ASSERT(!JS_IsExceptionPending(cx));
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!match) {
    return NS_OK;
  }

  JS::Value value;
  if (!JS_GetProperty(cx, &obj, "value", &value)) {
    MOZ_ASSERT(!JS_IsExceptionPending(cx));
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!value.isBoolean()) {
    MOZ_ASSERT(false, "Expecting a boolean for 'bluetooth.enabled'!");
    return NS_ERROR_UNEXPECTED;
  }

  if (value.toBoolean() == IsEnabled()) {
    // Nothing to do here.
    return NS_OK;
  }

  nsresult rv;

  if (IsEnabled()) {
    rv = Stop();
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  rv = Start();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
BluetoothService::HandleShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

  gInShutdown = true;

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs &&
      (NS_FAILED(obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) ||
       NS_FAILED(obs->RemoveObserver(this, MOZSETTINGS_CHANGED_ID)))) {
    NS_WARNING("Can't unregister observers!");
  }

  return Stop();
}

void
BluetoothService::RegisterManager(BluetoothManager* aManager)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(!mLiveManagers.Contains(aManager));

  mLiveManagers.AppendElement(aManager);
  RegisterBluetoothSignalHandler(aManager->GetPath(), aManager);
}

void
BluetoothService::UnregisterManager(BluetoothManager* aManager)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(mLiveManagers.Contains(aManager));

  UnregisterBluetoothSignalHandler(aManager->GetPath(), aManager);
  mLiveManagers.RemoveElement(aManager);
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

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE(obs, nullptr);

  if (NS_FAILED(obs->AddObserver(service, NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                 false)) ||
      NS_FAILED(obs->AddObserver(service, MOZSETTINGS_CHANGED_ID, false))) {
    NS_WARNING("AddObserver failed!");
    return nullptr;
  }

  gBluetoothService.swap(service);
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
