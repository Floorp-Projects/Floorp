/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BluetoothHfpManager.h"
#include "BluetoothProfileController.h"
#include "BluetoothUtils.h"

#include "jsapi.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsContentUtils.h"
#include "nsIAudioManager.h"
#include "nsIIccInfo.h"
#include "nsIIccService.h"
#include "nsIMobileConnectionInfo.h"
#include "nsIMobileConnectionService.h"
#include "nsIMobileNetworkInfo.h"
#include "nsIObserverService.h"
#include "nsISettingsService.h"
#include "nsITelephonyService.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/SettingChangeNotificationBinding.h"

#define MOZSETTINGS_CHANGED_ID               "mozsettings-changed"
#define AUDIO_VOLUME_BT_SCO_ID               "audio.volume.bt_sco"

/**
 * Dispatch task with arguments to main thread.
 */
using namespace mozilla;
using namespace mozilla::ipc;
USING_BLUETOOTH_NAMESPACE

namespace {
  StaticRefPtr<BluetoothHfpManager> sBluetoothHfpManager;
  static BluetoothHandsfreeInterface* sBluetoothHfpInterface = nullptr;

  bool sInShutdown = false;

  // Wait for 2 seconds for Dialer processing event 'BLDN'. '2' seconds is a
  // magic number. The mechanism should be revised once we can get call history.
  static int sWaitingForDialingInterval = 2000; //unit: ms

  // Wait 3.7 seconds until Dialer stops playing busy tone. '3' seconds is the
  // time window set in Dialer and the extra '0.7' second is a magic number.
  // The mechanism should be revised once we know the exact time at which
  // Dialer stops playing.
  static int sBusyToneInterval = 3700; //unit: ms
} // anonymous namespace

const int BluetoothHfpManager::MAX_NUM_CLIENTS = 1;

static bool
IsValidDtmf(const char aChar) {
  // Valid DTMF: [*#0-9ABCD]
  return (aChar == '*' || aChar == '#') ||
         (aChar >= '0' && aChar <= '9') ||
         (aChar >= 'A' && aChar <= 'D');
}

static bool
IsSupportedChld(const int aChld) {
  // We currently only support CHLD=0~3.
  return (aChld >= 0 && aChld <= 3);
}

class BluetoothHfpManager::GetVolumeTask final
  : public nsISettingsServiceCallback
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD
  Handle(const nsAString& aName, JS::Handle<JS::Value> aResult)
  {
    MOZ_ASSERT(NS_IsMainThread());

    JSContext *cx = nsContentUtils::GetCurrentJSContext();
    NS_ENSURE_TRUE(cx, NS_OK);

    if (!aResult.isNumber()) {
      BT_WARNING("'" AUDIO_VOLUME_BT_SCO_ID "' is not a number!");
      return NS_OK;
    }

    BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
    hfp->mCurrentVgs = aResult.toNumber();

    return NS_OK;
  }

  NS_IMETHOD
  HandleError(const nsAString& aName)
  {
    BT_WARNING("Unable to get value for '" AUDIO_VOLUME_BT_SCO_ID "'");
    return NS_OK;
  }

protected:
  ~GetVolumeTask() { }
};

class BluetoothHfpManager::CloseScoTask : public Task
{
private:
  void Run() override
  {
    MOZ_ASSERT(sBluetoothHfpManager);
    sBluetoothHfpManager->DisconnectSco();
  }
};

class BluetoothHfpManager::CloseScoRunnable : public nsRunnable
{
public:
  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    MessageLoop::current()->PostDelayedTask(
      FROM_HERE, new CloseScoTask(), sBusyToneInterval);

    return NS_OK;
  }
};

class BluetoothHfpManager::RespondToBLDNTask : public Task
{
private:
  void Run() override
  {
    MOZ_ASSERT(sBluetoothHfpManager);

    if (!sBluetoothHfpManager->mDialingRequestProcessed) {
      sBluetoothHfpManager->mDialingRequestProcessed = true;
      sBluetoothHfpManager->SendResponse(HFP_AT_RESPONSE_ERROR);
    }
  }
};

NS_IMPL_ISUPPORTS(BluetoothHfpManager::GetVolumeTask,
                  nsISettingsServiceCallback);

/**
 *  Call
 */
Call::Call()
{
  Reset();
}

void
Call::Set(const nsAString& aNumber, const bool aIsOutgoing)
{
  mNumber = aNumber;
  mDirection = (aIsOutgoing) ? HFP_CALL_DIRECTION_OUTGOING :
                               HFP_CALL_DIRECTION_INCOMING;
  // Same logic as implementation in ril_worker.js
  if (aNumber.Length() && aNumber[0] == '+') {
    mType = HFP_CALL_ADDRESS_TYPE_INTERNATIONAL;
  } else {
    mType = HFP_CALL_ADDRESS_TYPE_UNKNOWN;
  }
}

void
Call::Reset()
{
  mState = nsITelephonyService::CALL_STATE_DISCONNECTED;
  mDirection = HFP_CALL_DIRECTION_OUTGOING;
  mNumber.Truncate();
  mType = HFP_CALL_ADDRESS_TYPE_UNKNOWN;
}

bool
Call::IsActive()
{
  return (mState == nsITelephonyService::CALL_STATE_CONNECTED);
}

/**
 *  BluetoothHfpManager
 */
BluetoothHfpManager::BluetoothHfpManager() : mPhoneType(PhoneType::NONE)
{
  Reset();
}

void
BluetoothHfpManager::ResetCallArray()
{
  mCurrentCallArray.Clear();
  // Append a call object at the beginning of mCurrentCallArray since call
  // index from RIL starts at 1.
  Call call;
  mCurrentCallArray.AppendElement(call);

  if (mPhoneType == PhoneType::CDMA) {
    mCdmaSecondCall.Reset();
  }
}

#ifdef MOZ_B2G_BT_API_V2
void
BluetoothHfpManager::Reset()
{
  mReceiveVgsFlag = false;
  mDialingRequestProcessed = true;

  mConnectionState = HFP_CONNECTION_STATE_DISCONNECTED;
  mPrevConnectionState = HFP_CONNECTION_STATE_DISCONNECTED;
  mAudioState = HFP_AUDIO_STATE_DISCONNECTED;

  // Phone & Device CIND
  ResetCallArray();
  mBattChg = 5;
  mService = HFP_NETWORK_STATE_NOT_AVAILABLE;
  mRoam = HFP_SERVICE_TYPE_HOME;
  mSignal = 0;

  mController = nullptr;
}
#else
void
BluetoothHfpManager::Cleanup()
{
  mReceiveVgsFlag = false;
  mDialingRequestProcessed = true;

  mConnectionState = HFP_CONNECTION_STATE_DISCONNECTED;
  mPrevConnectionState = HFP_CONNECTION_STATE_DISCONNECTED;
  mBattChg = 5;
  mService = HFP_NETWORK_STATE_NOT_AVAILABLE;
  mRoam = HFP_SERVICE_TYPE_HOME;
  mSignal = 0;

  mController = nullptr;
}

void
BluetoothHfpManager::Reset()
{
  mFirstCKPD = false;
  // Phone & Device CIND
  ResetCallArray();
  // Clear Sco state
  mAudioState = HFP_AUDIO_STATE_DISCONNECTED;
  Cleanup();
}
#endif

bool
BluetoothHfpManager::Init()
{
#ifdef MOZ_B2G_BT_API_V2
  // The function must run at b2g process since it would access SettingsService.
  MOZ_ASSERT(IsMainProcess());
#else
  // Missing on bluetooth1
#endif

  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE(obs, false);

  if (NS_FAILED(obs->AddObserver(this, MOZSETTINGS_CHANGED_ID, false)) ||
      NS_FAILED(obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false))) {
    BT_WARNING("Failed to add observers!");
    return false;
  }

  hal::RegisterBatteryObserver(this);
  // Update to the latest battery level
  hal::BatteryInformation batteryInfo;
  hal::GetCurrentBatteryInformation(&batteryInfo);
  Notify(batteryInfo);

  mListener = new BluetoothRilListener();
  NS_ENSURE_TRUE(mListener->Listen(true), false);

  nsCOMPtr<nsISettingsService> settings =
    do_GetService("@mozilla.org/settingsService;1");
  NS_ENSURE_TRUE(settings, false);

  nsCOMPtr<nsISettingsServiceLock> settingsLock;
  nsresult rv = settings->CreateLock(nullptr, getter_AddRefs(settingsLock));
  NS_ENSURE_SUCCESS(rv, false);

  nsRefPtr<GetVolumeTask> callback = new GetVolumeTask();
  rv = settingsLock->Get(AUDIO_VOLUME_BT_SCO_ID, callback);
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

class BluetoothHfpManager::CleanupInitResultHandler final
  : public BluetoothHandsfreeResultHandler
{
public:
  CleanupInitResultHandler(BluetoothHandsfreeInterface* aInterface,
                           BluetoothProfileResultHandler* aRes)
    : mInterface(aInterface)
    , mRes(aRes)
  {
    MOZ_ASSERT(mInterface);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothHandsfreeInterface::Init failed: %d", (int)aStatus);
    if (mRes) {
      mRes->OnError(NS_ERROR_FAILURE);
    }
  }

  void Init() override
  {
    sBluetoothHfpInterface = mInterface;
    if (mRes) {
      mRes->Init();
    }
  }

  void Cleanup() override
  {
    sBluetoothHfpInterface = nullptr;
    /* During re-initialization, a previouly initialized
     * |BluetoothHandsfreeInterface| has now been cleaned
     * up, so we start initialization.
     */
    RunInit();
  }

  void RunInit()
  {
    BluetoothHfpManager* hfpManager = BluetoothHfpManager::Get();

    mInterface->Init(hfpManager, BluetoothHfpManager::MAX_NUM_CLIENTS, this);
  }

private:
  BluetoothHandsfreeInterface* mInterface;
  nsRefPtr<BluetoothProfileResultHandler> mRes;
};

class BluetoothHfpManager::InitResultHandlerRunnable final
  : public nsRunnable
{
public:
  InitResultHandlerRunnable(CleanupInitResultHandler* aRes)
    : mRes(aRes)
  {
    MOZ_ASSERT(mRes);
  }

  NS_IMETHOD Run() override
  {
    mRes->RunInit();
    return NS_OK;
  }

private:
  nsRefPtr<CleanupInitResultHandler> mRes;
};

class BluetoothHfpManager::OnErrorProfileResultHandlerRunnable final
  : public nsRunnable
{
public:
  OnErrorProfileResultHandlerRunnable(BluetoothProfileResultHandler* aRes,
                                      nsresult aRv)
    : mRes(aRes)
    , mRv(aRv)
  {
    MOZ_ASSERT(mRes);
  }

  NS_IMETHOD Run() override
  {
    mRes->OnError(mRv);
    return NS_OK;
  }

private:
  nsRefPtr<BluetoothProfileResultHandler> mRes;
  nsresult mRv;
};

// static
void
BluetoothHfpManager::InitHfpInterface(BluetoothProfileResultHandler* aRes)
{
  BluetoothInterface* btInf = BluetoothInterface::GetInstance();
  if (NS_WARN_IF(!btInf)) {
    // If there's no backend interface, we dispatch a runnable
    // that calls the profile result handler.
    nsRefPtr<nsRunnable> r =
      new OnErrorProfileResultHandlerRunnable(aRes, NS_ERROR_FAILURE);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch HFP OnError runnable");
    }
    return;
  }

  BluetoothHandsfreeInterface *interface =
    btInf->GetBluetoothHandsfreeInterface();
  if (NS_WARN_IF(!interface)) {
    // If there's no HFP interface, we dispatch a runnable
    // that calls the profile result handler.
    nsRefPtr<nsRunnable> r =
      new OnErrorProfileResultHandlerRunnable(aRes, NS_ERROR_FAILURE);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch HFP OnError runnable");
    }
    return;
  }

  nsRefPtr<CleanupInitResultHandler> res =
    new CleanupInitResultHandler(interface, aRes);

  if (sBluetoothHfpInterface) {
    // Cleanup an initialized HFP before initializing again.
    sBluetoothHfpInterface->Cleanup(res);
  } else {
    // If there's no HFP interface to cleanup first, we dispatch
    // a runnable that calls the profile result handler.
    nsRefPtr<nsRunnable> r = new InitResultHandlerRunnable(res);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch HFP init runnable");
    }
  }
}

BluetoothHfpManager::~BluetoothHfpManager()
{
  if (!mListener->Listen(false)) {
    BT_WARNING("Failed to stop listening RIL");
  }
  mListener = nullptr;

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE_VOID(obs);

  if (NS_FAILED(obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) ||
      NS_FAILED(obs->RemoveObserver(this, MOZSETTINGS_CHANGED_ID))) {
    BT_WARNING("Failed to remove observers!");
  }

  hal::UnregisterBatteryObserver(this);
}

class BluetoothHfpManager::CleanupResultHandler final
  : public BluetoothHandsfreeResultHandler
{
public:
  CleanupResultHandler(BluetoothProfileResultHandler* aRes)
    : mRes(aRes)
  { }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothHandsfreeInterface::Cleanup failed: %d", (int)aStatus);

    sBluetoothHfpInterface = nullptr;

    if (mRes) {
      mRes->OnError(NS_ERROR_FAILURE);
    }
  }

  void Cleanup() override
  {
    sBluetoothHfpInterface = nullptr;
    if (mRes) {
      mRes->Deinit();
    }
  }

private:
  nsRefPtr<BluetoothProfileResultHandler> mRes;
};

class BluetoothHfpManager::DeinitResultHandlerRunnable final
  : public nsRunnable
{
public:
  DeinitResultHandlerRunnable(BluetoothProfileResultHandler* aRes)
    : mRes(aRes)
  {
    MOZ_ASSERT(mRes);
  }

  NS_IMETHOD Run() override
  {
    mRes->Deinit();
    return NS_OK;
  }

private:
  nsRefPtr<BluetoothProfileResultHandler> mRes;
};

// static
void
BluetoothHfpManager::DeinitHfpInterface(BluetoothProfileResultHandler* aRes)
{
  if (sBluetoothHfpInterface) {
    sBluetoothHfpInterface->Cleanup(new CleanupResultHandler(aRes));
  } else if (aRes) {
    // We dispatch a runnable here to make the profile resource handler
    // behave as if HFP was initialized.
    nsRefPtr<nsRunnable> r = new DeinitResultHandlerRunnable(aRes);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch cleanup-result-handler runnable");
    }
  }
}

//static
BluetoothHfpManager*
BluetoothHfpManager::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  // If sBluetoothHfpManager already exists, exit early
  if (sBluetoothHfpManager) {
    return sBluetoothHfpManager;
  }

  // If we're in shutdown, don't create a new instance
  NS_ENSURE_FALSE(sInShutdown, nullptr);

  // Create a new instance, register, and return
  BluetoothHfpManager* manager = new BluetoothHfpManager();
  NS_ENSURE_TRUE(manager->Init(), nullptr);

  sBluetoothHfpManager = manager;
  return sBluetoothHfpManager;
}

NS_IMETHODIMP
BluetoothHfpManager::Observe(nsISupports* aSubject,
                             const char* aTopic,
                             const char16_t* aData)
{
  if (!strcmp(aTopic, MOZSETTINGS_CHANGED_ID)) {
    HandleVolumeChanged(aSubject);
  } else if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    HandleShutdown();
  } else {
    MOZ_ASSERT(false, "BluetoothHfpManager got unexpected topic!");
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

void
BluetoothHfpManager::Notify(const hal::BatteryInformation& aBatteryInfo)
{
  // Range of battery level: [0, 1], double
  // Range of CIND::BATTCHG: [0, 5], int
  mBattChg = (int) round(aBatteryInfo.level() * 5.0);
  UpdateDeviceCIND();
}

void
BluetoothHfpManager::NotifyConnectionStateChanged(const nsAString& aType)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Notify Gecko observers
  nsCOMPtr<nsIObserverService> obs =
    do_GetService("@mozilla.org/observer-service;1");
  NS_ENSURE_TRUE_VOID(obs);

  if (NS_FAILED(obs->NotifyObservers(this, NS_ConvertUTF16toUTF8(aType).get(),
                                     mDeviceAddress.get()))) {
    BT_WARNING("Failed to notify observsers!");
  }

  // Dispatch an event of status change
  bool status;
  nsAutoString eventName;
  if (aType.EqualsLiteral(BLUETOOTH_HFP_STATUS_CHANGED_ID)) {
    status = IsConnected();
    eventName.AssignLiteral(HFP_STATUS_CHANGED_ID);
  } else if (aType.EqualsLiteral(BLUETOOTH_SCO_STATUS_CHANGED_ID)) {
    status = IsScoConnected();
    eventName.AssignLiteral(SCO_STATUS_CHANGED_ID);
  } else {
    MOZ_ASSERT(false);
    return;
  }

  DispatchStatusChangedEvent(eventName, mDeviceAddress, status);

  // Notify profile controller
  if (aType.EqualsLiteral(BLUETOOTH_HFP_STATUS_CHANGED_ID)) {
    if (IsConnected()) {
      MOZ_ASSERT(mListener);

      // Enumerate current calls
      mListener->EnumerateCalls();

      OnConnect(EmptyString());
    } else if (mConnectionState == HFP_CONNECTION_STATE_DISCONNECTED) {
      mDeviceAddress.AssignLiteral(BLUETOOTH_ADDRESS_NONE);
      if (mPrevConnectionState == HFP_CONNECTION_STATE_DISCONNECTED) {
        // Bug 979160: This implies the outgoing connection failure.
        // When the outgoing hfp connection fails, state changes to disconnected
        // state. Since bluedroid would not report connecting state, but only
        // report connected/disconnected.
        OnConnect(NS_LITERAL_STRING(ERR_CONNECTION_FAILED));
      } else {
        OnDisconnect(EmptyString());
      }
#ifdef MOZ_B2G_BT_API_V2
      Reset();
#else
      Cleanup();
#endif
    }
  }
}

void
BluetoothHfpManager::NotifyDialer(const nsAString& aCommand)
{
  NS_NAMED_LITERAL_STRING(type, "bluetooth-dialer-command");
  InfallibleTArray<BluetoothNamedValue> parameters;

  BT_APPEND_NAMED_VALUE(parameters, "command", nsString(aCommand));

  BT_ENSURE_TRUE_VOID_BROADCAST_SYSMSG(type, parameters);
}

class BluetoothHfpManager::VolumeControlResultHandler final
  : public BluetoothHandsfreeResultHandler
{
public:
  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothHandsfreeInterface::VolumeControl failed: %d",
               (int)aStatus);
  }
};

void
BluetoothHfpManager::HandleVolumeChanged(nsISupports* aSubject)
{
  MOZ_ASSERT(NS_IsMainThread());

  // The string that we're interested in will be a JSON string that looks like:
  //  {"key":"volumeup", "value":10}
  //  {"key":"volumedown", "value":2}
  RootedDictionary<dom::SettingChangeNotification> setting(nsContentUtils::RootingCx());
  if (!WrappedJSToDictionary(aSubject, setting)) {
    return;
  }
  if (!setting.mKey.EqualsASCII(AUDIO_VOLUME_BT_SCO_ID)) {
    return;
  }
  if (!setting.mValue.isNumber()) {
    return;
  }

  mCurrentVgs = setting.mValue.toNumber();

  // Adjust volume by headset and we don't have to send volume back to headset
  if (mReceiveVgsFlag) {
    mReceiveVgsFlag = false;
    return;
  }

  // Only send volume back when there's a connected headset
  if (IsConnected()) {
    NS_ENSURE_TRUE_VOID(sBluetoothHfpInterface);
    sBluetoothHfpInterface->VolumeControl(
      HFP_VOLUME_TYPE_SPEAKER, mCurrentVgs, mDeviceAddress,
      new VolumeControlResultHandler());
  }
}

void
BluetoothHfpManager::HandleVoiceConnectionChanged(uint32_t aClientId)
{
  nsCOMPtr<nsIMobileConnectionService> mcService =
    do_GetService(NS_MOBILE_CONNECTION_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE_VOID(mcService);

  nsCOMPtr<nsIMobileConnection> connection;
  mcService->GetItemByServiceId(aClientId, getter_AddRefs(connection));
  NS_ENSURE_TRUE_VOID(connection);

  nsCOMPtr<nsIMobileConnectionInfo> voiceInfo;
  connection->GetVoice(getter_AddRefs(voiceInfo));
  NS_ENSURE_TRUE_VOID(voiceInfo);

  nsString type;
  voiceInfo->GetType(type);
  mPhoneType = GetPhoneType(type);

  // Roam
  bool roaming;
  voiceInfo->GetRoaming(&roaming);
  mRoam = (roaming) ? HFP_SERVICE_TYPE_ROAMING : HFP_SERVICE_TYPE_HOME;

  // Service
  nsString regState;
  voiceInfo->GetState(regState);

#ifdef MOZ_B2G_BT_API_V2
  BluetoothHandsfreeNetworkState service =
    (regState.EqualsLiteral("registered")) ? HFP_NETWORK_STATE_AVAILABLE :
                                             HFP_NETWORK_STATE_NOT_AVAILABLE;
  if (service != mService) {
    // Notify BluetoothRilListener of service change
    mListener->ServiceChanged(aClientId, service);
  }
  mService = service;
#else
  int service = (regState.EqualsLiteral("registered")) ? 1 : 0;
  if (service != mService) {
    // Notify BluetoothRilListener of service change
    mListener->ServiceChanged(aClientId, service);
  }
  mService = service ? HFP_NETWORK_STATE_AVAILABLE :
                       HFP_NETWORK_STATE_NOT_AVAILABLE;
#endif

  // Signal
  JS::Rooted<JS::Value> value(nsContentUtils::RootingCxForThread());
  voiceInfo->GetRelSignalStrength(&value);
  NS_ENSURE_TRUE_VOID(value.isNumber());
  mSignal = (int)ceil(value.toNumber() / 20.0);

  UpdateDeviceCIND();

  // Operator name
  nsCOMPtr<nsIMobileNetworkInfo> network;
  voiceInfo->GetNetwork(getter_AddRefs(network));
  NS_ENSURE_TRUE_VOID(network);
  network->GetLongName(mOperatorName);

  // According to GSM 07.07, "<format> indicates if the format is alphanumeric
  // or numeric; long alphanumeric format can be upto 16 characters long and
  // short format up to 8 characters (refer GSM MoU SE.13 [9])..."
  // However, we found that the operator name may sometimes be longer than 16
  // characters. After discussion, we decided to fix this here but not in RIL
  // or modem.
  //
  // Please see Bug 871366 for more information.
  if (mOperatorName.Length() > 16) {
    BT_WARNING("The operator name was longer than 16 characters. We cut it.");
    mOperatorName.Left(mOperatorName, 16);
  }
}

void
BluetoothHfpManager::HandleIccInfoChanged(uint32_t aClientId)
{
  nsCOMPtr<nsIIccService> service =
    do_GetService(ICC_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE_VOID(service);

  nsCOMPtr<nsIIcc> icc;
  service->GetIccByServiceId(aClientId, getter_AddRefs(icc));
  NS_ENSURE_TRUE_VOID(icc);

  nsCOMPtr<nsIIccInfo> iccInfo;
  icc->GetIccInfo(getter_AddRefs(iccInfo));
  NS_ENSURE_TRUE_VOID(iccInfo);

  nsCOMPtr<nsIGsmIccInfo> gsmIccInfo = do_QueryInterface(iccInfo);
  NS_ENSURE_TRUE_VOID(gsmIccInfo);
  gsmIccInfo->GetMsisdn(mMsisdn);
}

void
BluetoothHfpManager::HandleShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  sInShutdown = true;
  Disconnect(nullptr);
  DisconnectSco();
  sBluetoothHfpManager = nullptr;
}

class BluetoothHfpManager::ClccResponseResultHandler final
  : public BluetoothHandsfreeResultHandler
{
public:
  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothHandsfreeInterface::ClccResponse failed: %d",
               (int)aStatus);
  }
};

void
BluetoothHfpManager::SendCLCC(Call& aCall, int aIndex)
{
  NS_ENSURE_TRUE_VOID(aCall.mState !=
                        nsITelephonyService::CALL_STATE_DISCONNECTED);
  NS_ENSURE_TRUE_VOID(sBluetoothHfpInterface);

  BluetoothHandsfreeCallState callState =
    ConvertToBluetoothHandsfreeCallState(aCall.mState);

  if (mPhoneType == PhoneType::CDMA && aIndex == 1 && aCall.IsActive()) {
    callState = (mCdmaSecondCall.IsActive()) ? HFP_CALL_STATE_HELD :
                                               HFP_CALL_STATE_ACTIVE;
  }

  if (callState == HFP_CALL_STATE_INCOMING &&
      FindFirstCall(nsITelephonyService::CALL_STATE_CONNECTED)) {
    callState = HFP_CALL_STATE_WAITING;
  }

  sBluetoothHfpInterface->ClccResponse(
    aIndex, aCall.mDirection, callState, HFP_CALL_MODE_VOICE,
    HFP_CALL_MPTY_TYPE_SINGLE, aCall.mNumber,
    aCall.mType, mDeviceAddress, new ClccResponseResultHandler());
}

class BluetoothHfpManager::FormattedAtResponseResultHandler final
  : public BluetoothHandsfreeResultHandler
{
public:
  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothHandsfreeInterface::FormattedAtResponse failed: %d",
               (int)aStatus);
  }
};

void
BluetoothHfpManager::SendLine(const char* aMessage)
{
  NS_ENSURE_TRUE_VOID(sBluetoothHfpInterface);

  sBluetoothHfpInterface->FormattedAtResponse(
    aMessage, mDeviceAddress, new FormattedAtResponseResultHandler());
}

class BluetoothHfpManager::AtResponseResultHandler final
  : public BluetoothHandsfreeResultHandler
{
public:
  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothHandsfreeInterface::AtResponse failed: %d",
               (int)aStatus);
  }
};

void
BluetoothHfpManager::SendResponse(BluetoothHandsfreeAtResponse aResponseCode)
{
  NS_ENSURE_TRUE_VOID(sBluetoothHfpInterface);

  sBluetoothHfpInterface->AtResponse(
    aResponseCode, 0, mDeviceAddress, new AtResponseResultHandler());
}

class BluetoothHfpManager::PhoneStateChangeResultHandler final
  : public BluetoothHandsfreeResultHandler
{
public:
  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothHandsfreeInterface::PhoneStateChange failed: %d",
               (int)aStatus);
  }
};

void
BluetoothHfpManager::UpdatePhoneCIND(uint32_t aCallIndex)
{
  NS_ENSURE_TRUE_VOID(sBluetoothHfpInterface);

  int numActive = GetNumberOfCalls(nsITelephonyService::CALL_STATE_CONNECTED);
  int numHeld = GetNumberOfCalls(nsITelephonyService::CALL_STATE_HELD);
  BluetoothHandsfreeCallState callSetupState =
    ConvertToBluetoothHandsfreeCallState(GetCallSetupState());
  BluetoothHandsfreeCallAddressType type = mCurrentCallArray[aCallIndex].mType;

  BT_LOGR("[%d] state %d => BTHF: active[%d] held[%d] setupstate[%d]",
          aCallIndex, mCurrentCallArray[aCallIndex].mState,
          numActive, numHeld, callSetupState);

  sBluetoothHfpInterface->PhoneStateChange(
    numActive, numHeld, callSetupState,
    mCurrentCallArray[aCallIndex].mNumber, type,
    new PhoneStateChangeResultHandler());
}

class BluetoothHfpManager::DeviceStatusNotificationResultHandler final
  : public BluetoothHandsfreeResultHandler
{
public:
  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING(
      "BluetoothHandsfreeInterface::DeviceStatusNotification failed: %d",
      (int)aStatus);
  }
};

void
BluetoothHfpManager::UpdateDeviceCIND()
{
  if (sBluetoothHfpInterface) {
    sBluetoothHfpInterface->DeviceStatusNotification(
      mService,
      mRoam,
      mSignal,
      mBattChg, new DeviceStatusNotificationResultHandler());
  }
}

uint32_t
BluetoothHfpManager::FindFirstCall(uint16_t aState)
{
  uint32_t callLength = mCurrentCallArray.Length();

  for (uint32_t i = 1; i < callLength; ++i) {
    if (mCurrentCallArray[i].mState == aState) {
      return i;
    }
  }

  return 0;
}

uint32_t
BluetoothHfpManager::GetNumberOfCalls(uint16_t aState)
{
  uint32_t num = 0;
  uint32_t callLength = mCurrentCallArray.Length();

  for (uint32_t i = 1; i < callLength; ++i) {
    if (mCurrentCallArray[i].mState == aState) {
      ++num;
    }
  }

  return num;
}

uint16_t
BluetoothHfpManager::GetCallSetupState()
{
  uint32_t callLength = mCurrentCallArray.Length();

  for (uint32_t i = 1; i < callLength; ++i) {
    switch (mCurrentCallArray[i].mState) {
      case nsITelephonyService::CALL_STATE_INCOMING:
      case nsITelephonyService::CALL_STATE_DIALING:
      case nsITelephonyService::CALL_STATE_ALERTING:
        return mCurrentCallArray[i].mState;
      default:
        break;
    }
  }

  return nsITelephonyService::CALL_STATE_DISCONNECTED;
}

BluetoothHandsfreeCallState
BluetoothHfpManager::ConvertToBluetoothHandsfreeCallState(int aCallState) const
{
  BluetoothHandsfreeCallState state;

  // Refer to AOSP BluetoothPhoneService.convertCallState
  if (aCallState == nsITelephonyService::CALL_STATE_INCOMING) {
    state = HFP_CALL_STATE_INCOMING;
  } else if (aCallState == nsITelephonyService::CALL_STATE_DIALING) {
    state = HFP_CALL_STATE_DIALING;
  } else if (aCallState == nsITelephonyService::CALL_STATE_ALERTING) {
    state = HFP_CALL_STATE_ALERTING;
  } else if (aCallState == nsITelephonyService::CALL_STATE_CONNECTED) {
    state = HFP_CALL_STATE_ACTIVE;
  } else if (aCallState == nsITelephonyService::CALL_STATE_HELD) {
    state = HFP_CALL_STATE_HELD;
  } else { // disconnected
    state = HFP_CALL_STATE_IDLE;
  }

  return state;
}

bool
BluetoothHfpManager::IsTransitionState(uint16_t aCallState, bool aIsConference)
{
  /**
   * Regard this callstate change as during CHLD=2 transition state if
   * - the call becomes active, and numActive > 1
   * - the call becomes held, and numHeld > 1 or an incoming call exists
   *
   * TODO:
   * 1) handle CHLD=1 transition state
   * 2) handle conference call cases
   */
  if (!aIsConference) {
    switch (aCallState) {
      case nsITelephonyService::CALL_STATE_CONNECTED:
        return (GetNumberOfCalls(aCallState) > 1);
      case nsITelephonyService::CALL_STATE_HELD:
        return (GetNumberOfCalls(aCallState) > 1 ||
                FindFirstCall(nsITelephonyService::CALL_STATE_INCOMING));
      default:
        break;
    }
  }

  return false;
}

void
BluetoothHfpManager::HandleCallStateChanged(uint32_t aCallIndex,
                                            uint16_t aCallState,
                                            const nsAString& aError,
                                            const nsAString& aNumber,
                                            const bool aIsOutgoing,
                                            const bool aIsConference,
                                            bool aSend)
{
  // aCallIndex can be UINT32_MAX for the pending outgoing call state update.
  // aCallIndex will be updated again after real call state changes. See Bug
  // 990467.
  if (aCallIndex == UINT32_MAX) {
    return;
  }

  // Update call state only
  while (aCallIndex >= mCurrentCallArray.Length()) {
    Call call;
    mCurrentCallArray.AppendElement(call);
  }
  mCurrentCallArray[aCallIndex].mState = aCallState;

  // Return if SLC is disconnected
  if (!IsConnected()) {
    return;
  }

  // Update call information besides call state
  mCurrentCallArray[aCallIndex].Set(aNumber, aIsOutgoing);

  // Notify bluedroid of phone state change if this
  // call state change is not during transition state
  if (!IsTransitionState(aCallState, aIsConference)) {
    UpdatePhoneCIND(aCallIndex);
  }

  switch (aCallState) {
    case nsITelephonyService::CALL_STATE_DIALING:
      // We've send Dialer a dialing request and this is the response.
      if (!mDialingRequestProcessed) {
        SendResponse(HFP_AT_RESPONSE_OK);
        mDialingRequestProcessed = true;
      }
      break;
    case nsITelephonyService::CALL_STATE_DISCONNECTED:
      // -1 is necessary because call 0 is an invalid (padding) call object.
      if (mCurrentCallArray.Length() - 1 ==
          GetNumberOfCalls(nsITelephonyService::CALL_STATE_DISCONNECTED)) {
        // In order to let user hear busy tone via connected Bluetooth headset,
        // we postpone the timing of dropping SCO.
        if (aError.EqualsLiteral("BusyError")) {
          // FIXME: UpdatePhoneCIND later since it causes SCO close but
          // Dialer is still playing busy tone via HF.
          NS_DispatchToMainThread(new CloseScoRunnable());
        }

        ResetCallArray();
      }
      break;
    default:
      break;
  }
}

PhoneType
BluetoothHfpManager::GetPhoneType(const nsAString& aType)
{
  // FIXME: Query phone type from RIL after RIL implements new API (bug 912019)
  if (aType.EqualsLiteral("gsm") || aType.EqualsLiteral("gprs") ||
      aType.EqualsLiteral("edge") || aType.EqualsLiteral("umts") ||
      aType.EqualsLiteral("hspa") || aType.EqualsLiteral("hsdpa") ||
      aType.EqualsLiteral("hsupa") || aType.EqualsLiteral("hspa+")) {
    return PhoneType::GSM;
  } else if (aType.EqualsLiteral("is95a") || aType.EqualsLiteral("is95b") ||
             aType.EqualsLiteral("1xrtt") || aType.EqualsLiteral("evdo0") ||
             aType.EqualsLiteral("evdoa") || aType.EqualsLiteral("evdob")) {
    return PhoneType::CDMA;
  }

  return PhoneType::NONE;
}

void
BluetoothHfpManager::UpdateSecondNumber(const nsAString& aNumber)
{
  MOZ_ASSERT(mPhoneType == PhoneType::CDMA);

  // Always regard second call as incoming call since v1.2 RIL
  // doesn't support outgoing second call in CDMA.
  mCdmaSecondCall.Set(aNumber, false);

  // FIXME: check CDMA + bluedroid
  //UpdateCIND(CINDType::CALLSETUP, CallSetupState::INCOMING, true);
}

void
BluetoothHfpManager::AnswerWaitingCall()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mPhoneType == PhoneType::CDMA);

  // Pick up second call. First call is held now.
  mCdmaSecondCall.mState = nsITelephonyService::CALL_STATE_CONNECTED;
  // FIXME: check CDMA + bluedroid
  //UpdateCIND(CINDType::CALLSETUP, CallSetupState::NO_CALLSETUP, true);

  //sCINDItems[CINDType::CALLHELD].value = CallHeldState::ONHOLD_ACTIVE;
  //SendCommand("+CIEV: ", CINDType::CALLHELD);
}

void
BluetoothHfpManager::IgnoreWaitingCall()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mPhoneType == PhoneType::CDMA);

  mCdmaSecondCall.Reset();
  // FIXME: check CDMA + bluedroid
  //UpdateCIND(CINDType::CALLSETUP, CallSetupState::NO_CALLSETUP, true);
}

void
BluetoothHfpManager::ToggleCalls()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mPhoneType == PhoneType::CDMA);

  // Toggle acitve and held calls
  mCdmaSecondCall.mState = (mCdmaSecondCall.IsActive()) ?
                             nsITelephonyService::CALL_STATE_HELD :
                             nsITelephonyService::CALL_STATE_CONNECTED;
}

class BluetoothHfpManager::ConnectAudioResultHandler final
  : public BluetoothHandsfreeResultHandler
{
public:
  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothHandsfreeInterface::ConnectAudio failed: %d",
               (int)aStatus);
  }
};

bool
BluetoothHfpManager::ConnectSco()
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE(!sInShutdown, false);
  NS_ENSURE_TRUE(IsConnected() && !IsScoConnected(), false);
  NS_ENSURE_TRUE(sBluetoothHfpInterface, false);

  sBluetoothHfpInterface->ConnectAudio(mDeviceAddress,
                                       new ConnectAudioResultHandler());

  return true;
}

class BluetoothHfpManager::DisconnectAudioResultHandler final
  : public BluetoothHandsfreeResultHandler
{
public:
  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothHandsfreeInterface::DisconnectAudio failed: %d",
               (int)aStatus);
  }
};

bool
BluetoothHfpManager::DisconnectSco()
{
  NS_ENSURE_TRUE(IsScoConnected(), false);
  NS_ENSURE_TRUE(sBluetoothHfpInterface, false);

  sBluetoothHfpInterface->DisconnectAudio(mDeviceAddress,
                                          new DisconnectAudioResultHandler());

  return true;
}

bool
BluetoothHfpManager::IsScoConnected()
{
  return (mAudioState == HFP_AUDIO_STATE_CONNECTED);
}

bool
BluetoothHfpManager::IsConnected()
{
  return (mConnectionState == HFP_CONNECTION_STATE_SLC_CONNECTED);
}

void
BluetoothHfpManager::OnConnectError()
{
  MOZ_ASSERT(NS_IsMainThread());

  mController->NotifyCompletion(NS_LITERAL_STRING(ERR_CONNECTION_FAILED));

  mController = nullptr;
  mDeviceAddress.Truncate();
}

class BluetoothHfpManager::ConnectResultHandler final
  : public BluetoothHandsfreeResultHandler
{
public:
  ConnectResultHandler(BluetoothHfpManager* aHfpManager)
    : mHfpManager(aHfpManager)
  {
    MOZ_ASSERT(mHfpManager);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothHandsfreeInterface::Connect failed: %d",
               (int)aStatus);
    mHfpManager->OnConnectError();
  }

private:
  BluetoothHfpManager* mHfpManager;
};

void
BluetoothHfpManager::Connect(const nsAString& aDeviceAddress,
                             BluetoothProfileController* aController)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aController && !mController);

  if (sInShutdown) {
    aController->NotifyCompletion(NS_LITERAL_STRING(ERR_NO_AVAILABLE_RESOURCE));
    return;
  }

  if (!sBluetoothHfpInterface) {
    BT_LOGR("sBluetoothHfpInterface is null");
    aController->NotifyCompletion(NS_LITERAL_STRING(ERR_NO_AVAILABLE_RESOURCE));
    return;
  }

  mDeviceAddress = aDeviceAddress;
  mController = aController;

  sBluetoothHfpInterface->Connect(mDeviceAddress,
                                  new ConnectResultHandler(this));
}

void
BluetoothHfpManager::OnDisconnectError()
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE_VOID(mController);

  mController->NotifyCompletion(NS_LITERAL_STRING(ERR_CONNECTION_FAILED));
}

class BluetoothHfpManager::DisconnectResultHandler final
  : public BluetoothHandsfreeResultHandler
{
public:
  DisconnectResultHandler(BluetoothHfpManager* aHfpManager)
    : mHfpManager(aHfpManager)
  {
    MOZ_ASSERT(mHfpManager);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothHandsfreeInterface::Disconnect failed: %d",
               (int)aStatus);
    mHfpManager->OnDisconnectError();
  }

private:
  BluetoothHfpManager* mHfpManager;
};


void
BluetoothHfpManager::Disconnect(BluetoothProfileController* aController)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mController);

  if (!sBluetoothHfpInterface) {
    BT_LOGR("sBluetoothHfpInterface is null");
    if (aController) {
      aController->NotifyCompletion(NS_LITERAL_STRING(ERR_NO_AVAILABLE_RESOURCE));
    }
    return;
  }

  mController = aController;

  sBluetoothHfpInterface->Disconnect(mDeviceAddress,
                                     new DisconnectResultHandler(this));
}

void
BluetoothHfpManager::OnConnect(const nsAString& aErrorStr)
{
  MOZ_ASSERT(NS_IsMainThread());

  mFirstCKPD = true;

  /**
   * On the one hand, notify the controller that we've done for outbound
   * connections. On the other hand, we do nothing for inbound connections.
   */
  NS_ENSURE_TRUE_VOID(mController);

  mController->NotifyCompletion(aErrorStr);
  mController = nullptr;
}

void
BluetoothHfpManager::OnDisconnect(const nsAString& aErrorStr)
{
  MOZ_ASSERT(NS_IsMainThread());

  /**
   * On the one hand, notify the controller that we've done for outbound
   * connections. On the other hand, we do nothing for inbound connections.
   */
  NS_ENSURE_TRUE_VOID(mController);

  mController->NotifyCompletion(aErrorStr);
  mController = nullptr;
}

void
BluetoothHfpManager::OnUpdateSdpRecords(const nsAString& aDeviceAddress)
{
  // Bluedroid handles this part
  MOZ_ASSERT(false);
}

void
BluetoothHfpManager::OnGetServiceChannel(const nsAString& aDeviceAddress,
                                         const nsAString& aServiceUuid,
                                         int aChannel)
{
  // Bluedroid handles this part
  MOZ_ASSERT(false);
}

void
BluetoothHfpManager::GetAddress(nsAString& aDeviceAddress)
{
  aDeviceAddress = mDeviceAddress;
}

//
// Bluetooth notifications
//

void
BluetoothHfpManager::ConnectionStateNotification(
  BluetoothHandsfreeConnectionState aState, const nsAString& aBdAddress)
{
  MOZ_ASSERT(NS_IsMainThread());

  BT_LOGR("state %d", aState);

  mPrevConnectionState = mConnectionState;
  mConnectionState = aState;

  if (aState == HFP_CONNECTION_STATE_SLC_CONNECTED) {
    mDeviceAddress = aBdAddress;
    NotifyConnectionStateChanged(
      NS_LITERAL_STRING(BLUETOOTH_HFP_STATUS_CHANGED_ID));

  } else if (aState == HFP_CONNECTION_STATE_DISCONNECTED) {
    DisconnectSco();
    NotifyConnectionStateChanged(
      NS_LITERAL_STRING(BLUETOOTH_HFP_STATUS_CHANGED_ID));
  }
}

void
BluetoothHfpManager::AudioStateNotification(
  BluetoothHandsfreeAudioState aState, const nsAString& aBdAddress)
{
  MOZ_ASSERT(NS_IsMainThread());

  BT_LOGR("state %d", aState);

  mAudioState = aState;

  if (aState == HFP_AUDIO_STATE_CONNECTED ||
      aState == HFP_AUDIO_STATE_DISCONNECTED) {
    NotifyConnectionStateChanged(
      NS_LITERAL_STRING(BLUETOOTH_SCO_STATUS_CHANGED_ID));
  }
}

void
BluetoothHfpManager::AnswerCallNotification(const nsAString& aBdAddress)
{
  MOZ_ASSERT(NS_IsMainThread());

  NotifyDialer(NS_LITERAL_STRING("ATA"));
}

void
BluetoothHfpManager::HangupCallNotification(const nsAString& aBdAddress)
{
  MOZ_ASSERT(NS_IsMainThread());

  NotifyDialer(NS_LITERAL_STRING("CHUP"));
}

void
BluetoothHfpManager::VolumeNotification(
  BluetoothHandsfreeVolumeType aType, int aVolume, const nsAString& aBdAddress)
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE_VOID(aVolume >= 0 && aVolume <= 15);

  if (aType == HFP_VOLUME_TYPE_MICROPHONE) {
    mCurrentVgm = aVolume;
  } else if (aType == HFP_VOLUME_TYPE_SPEAKER) {
    mReceiveVgsFlag = true;

    if (aVolume == mCurrentVgs) {
      // Keep current volume
      return;
    }

    nsString data;
    data.AppendInt(aVolume);

    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    NS_ENSURE_TRUE_VOID(os);

    os->NotifyObservers(nullptr, "bluetooth-volume-change", data.get());
  }
}

void
BluetoothHfpManager::DtmfNotification(char aDtmf, const nsAString& aBdAddress)
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE_VOID(IsValidDtmf(aDtmf));

  nsAutoCString message("VTS=");
  message += aDtmf;
  NotifyDialer(NS_ConvertUTF8toUTF16(message));
}

void
BluetoothHfpManager::CallHoldNotification(BluetoothHandsfreeCallHoldType aChld,
                                          const nsAString& aBdAddress)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!IsSupportedChld((int)aChld)) {
    // We currently don't support Enhanced Call Control.
    // AT+CHLD=1x and AT+CHLD=2x will be ignored
    SendResponse(HFP_AT_RESPONSE_ERROR);
    return;
  }

  SendResponse(HFP_AT_RESPONSE_OK);

  nsAutoCString message("CHLD=");
  message.AppendInt((int)aChld);
  NotifyDialer(NS_ConvertUTF8toUTF16(message));
}

void BluetoothHfpManager::DialCallNotification(const nsAString& aNumber,
                                               const nsAString& aBdAddress)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCString message = NS_ConvertUTF16toUTF8(aNumber);

  // There are three cases based on aNumber,
  // 1) Empty value:    Redial, BLDN
  // 2) >xxx:           Memory dial, ATD>xxx
  // 3) xxx:            Normal dial, ATDxxx
  // We need to respond OK/Error for dial requests for every case listed above,
  // 1) and 2):         Respond in either RespondToBLDNTask or
  //                    HandleCallStateChanged()
  // 3):                Respond here
  if (message.IsEmpty()) {
    mDialingRequestProcessed = false;
    NotifyDialer(NS_LITERAL_STRING("BLDN"));

    MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                            new RespondToBLDNTask(),
                                            sWaitingForDialingInterval);
  } else if (message[0] == '>') {
    mDialingRequestProcessed = false;

    nsAutoCString newMsg("ATD");
    newMsg += StringHead(message, message.Length() - 1);
    NotifyDialer(NS_ConvertUTF8toUTF16(newMsg));

    MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                            new RespondToBLDNTask(),
                                            sWaitingForDialingInterval);
  } else {
    SendResponse(HFP_AT_RESPONSE_OK);

    nsAutoCString newMsg("ATD");
    newMsg += StringHead(message, message.Length() - 1);
    NotifyDialer(NS_ConvertUTF8toUTF16(newMsg));
  }
}

void
BluetoothHfpManager::CnumNotification(const nsAString& aBdAddress)
{
  static const uint8_t sAddressType[] {
    [HFP_CALL_ADDRESS_TYPE_UNKNOWN] = 0x81,
    [HFP_CALL_ADDRESS_TYPE_INTERNATIONAL] = 0x91 // for completeness
  };

  MOZ_ASSERT(NS_IsMainThread());

  if (!mMsisdn.IsEmpty()) {
    nsAutoCString message("+CNUM: ,\"");
    message.Append(NS_ConvertUTF16toUTF8(mMsisdn).get());
    message.AppendLiteral("\",");
    message.AppendInt(sAddressType[HFP_CALL_ADDRESS_TYPE_UNKNOWN]);
    message.AppendLiteral(",,4");

    SendLine(message.get());
  }

  SendResponse(HFP_AT_RESPONSE_OK);
}

class BluetoothHfpManager::CindResponseResultHandler final
  : public BluetoothHandsfreeResultHandler
{
public:
  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothHandsfreeInterface::CindResponse failed: %d",
               (int)aStatus);
  }
};

void
BluetoothHfpManager::CindNotification(const nsAString& aBdAddress)
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE_VOID(sBluetoothHfpInterface);

  int numActive = GetNumberOfCalls(nsITelephonyService::CALL_STATE_CONNECTED);
  int numHeld = GetNumberOfCalls(nsITelephonyService::CALL_STATE_HELD);
  BluetoothHandsfreeCallState callState =
    ConvertToBluetoothHandsfreeCallState(GetCallSetupState());

  sBluetoothHfpInterface->CindResponse(
    mService, numActive, numHeld,
    callState, mSignal, mRoam, mBattChg,
    aBdAddress,
    new CindResponseResultHandler());
}

class BluetoothHfpManager::CopsResponseResultHandler final
  : public BluetoothHandsfreeResultHandler
{
public:
  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothHandsfreeInterface::CopsResponse failed: %d",
               (int)aStatus);
  }
};

void
BluetoothHfpManager::CopsNotification(const nsAString& aBdAddress)
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE_VOID(sBluetoothHfpInterface);

  sBluetoothHfpInterface->CopsResponse(
    NS_ConvertUTF16toUTF8(mOperatorName).get(),
    aBdAddress, new CopsResponseResultHandler());
}

void
BluetoothHfpManager::ClccNotification(const nsAString& aBdAddress)
{
  MOZ_ASSERT(NS_IsMainThread());

  uint32_t callNumbers = mCurrentCallArray.Length();
  uint32_t i;
  for (i = 1; i < callNumbers; i++) {
    SendCLCC(mCurrentCallArray[i], i);
  }

  if (!mCdmaSecondCall.mNumber.IsEmpty()) {
    MOZ_ASSERT(mPhoneType == PhoneType::CDMA);
    MOZ_ASSERT(i == 2);

    SendCLCC(mCdmaSecondCall, 2);
  }

  SendResponse(HFP_AT_RESPONSE_OK);
}

void
BluetoothHfpManager::UnknownAtNotification(const nsACString& aAtString,
                                           const nsAString& aBdAddress)
{
  MOZ_ASSERT(NS_IsMainThread());

  BT_LOGR("[%s]", nsCString(aAtString).get());

  SendResponse(HFP_AT_RESPONSE_ERROR);
}

void
BluetoothHfpManager::KeyPressedNotification(const nsAString& aBdAddress)
{
  MOZ_ASSERT(NS_IsMainThread());

  bool hasActiveCall =
    (FindFirstCall(nsITelephonyService::CALL_STATE_CONNECTED) > 0);

  // Refer to AOSP HeadsetStateMachine.processKeyPressed
  if (FindFirstCall(nsITelephonyService::CALL_STATE_INCOMING)
      && !hasActiveCall) {
    /*
     * Bluetooth HSP spec 4.2.2
     * There is an incoming call, notify Dialer to pick up the phone call
     * and SCO will be established after we get the CallStateChanged event
     * indicating the call is answered successfully.
     */
    NotifyDialer(NS_LITERAL_STRING("ATA"));
  } else if (hasActiveCall) {
    if (!IsScoConnected()) {
      /*
       * Bluetooth HSP spec 4.3
       * If there's no SCO, set up a SCO link.
       */
      ConnectSco();
    } else if (mFirstCKPD) {
      /*
       * Bluetooth HSP spec 4.2 & 4.3
       * The SCO link connection may be set up prior to receiving the AT+CKPD=200
       * command from the HS.
       *
       * Once FxOS initiates a SCO connection before receiving the
       * AT+CKPD=200, we should ignore this key press.
       */
     } else {
      /*
       * Bluetooth HSP spec 4.5
       * There are two ways to release SCO: sending CHUP to dialer or closing
       * SCO socket directly. We notify dialer only if there is at least one
       * active call.
       */
      NotifyDialer(NS_LITERAL_STRING("CHUP"));
    }
    mFirstCKPD = false;
  } else {
    // BLDN
    mDialingRequestProcessed = false;

    NotifyDialer(NS_LITERAL_STRING("BLDN"));

    MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                            new RespondToBLDNTask(),
                                            sWaitingForDialingInterval);
  }
}

NS_IMPL_ISUPPORTS(BluetoothHfpManager, nsIObserver)
