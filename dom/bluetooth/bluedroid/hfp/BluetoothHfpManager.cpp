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
#include "nsIDOMIccInfo.h"
#include "nsIDOMMobileConnection.h"
#include "nsIIccProvider.h"
#include "nsIMobileConnectionProvider.h"
#include "nsIObserverService.h"
#include "nsISettingsService.h"
#include "nsITelephonyProvider.h"
#include "nsRadioInterfaceLayer.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

#define MOZSETTINGS_CHANGED_ID               "mozsettings-changed"
#define AUDIO_VOLUME_BT_SCO_ID               "audio.volume.bt_sco"

/**
 * Dispatch task with arguments to main thread.
 */
#define BT_HF_DISPATCH_MAIN(args...) \
  NS_DispatchToMainThread(new MainThreadTask(args))

/**
 * Process bluedroid callbacks with corresponding handlers.
 */
#define BT_HF_PROCESS_CB(func, args...)          \
  do {                                           \
    NS_ENSURE_TRUE_VOID(sBluetoothHfpManager);   \
    sBluetoothHfpManager->func(args);            \
  } while(0)

using namespace mozilla;
using namespace mozilla::ipc;
USING_BLUETOOTH_NAMESPACE

namespace {
  StaticRefPtr<BluetoothHfpManager> sBluetoothHfpManager;
  static const bthf_interface_t* sBluetoothHfpInterface = nullptr;

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

// Main thread task commands
enum MainThreadTaskCmd {
  NOTIFY_CONN_STATE_CHANGED,
  NOTIFY_DIALER,
  NOTIFY_SCO_VOLUME_CHANGED,
  POST_TASK_RESPOND_TO_BLDN,
  POST_TASK_CLOSE_SCO
};

static void
ConnectionStateCallback(bthf_connection_state_t state, bt_bdaddr_t* bd_addr)
{
  BT_HF_PROCESS_CB(ProcessConnectionState, state, bd_addr);
}

static void
AudioStateCallback(bthf_audio_state_t state, bt_bdaddr_t* bd_addr)
{
  BT_HF_PROCESS_CB(ProcessAudioState, state, bd_addr);
}

static void
VoiceRecognitionCallback(bthf_vr_state_t state)
{
  // No support
}

static void
AnswerCallCallback()
{
  BT_HF_PROCESS_CB(ProcessAnswerCall);
}

static void
HangupCallCallback()
{
  BT_HF_PROCESS_CB(ProcessHangupCall);
}

static void
VolumeControlCallback(bthf_volume_type_t type, int volume)
{
  BT_HF_PROCESS_CB(ProcessVolumeControl, type, volume);
}

static void
DialCallCallback(char *number)
{
  BT_HF_PROCESS_CB(ProcessDialCall, number);
}

static void
DtmfCmdCallback(char dtmf)
{
  BT_HF_PROCESS_CB(ProcessDtmfCmd, dtmf);
}

static void
NoiceReductionCallback(bthf_nrec_t nrec)
{
  // No support
}

static void
AtChldCallback(bthf_chld_type_t chld)
{
  BT_HF_PROCESS_CB(ProcessAtChld, chld);
}

static void
AtCnumCallback()
{
  BT_HF_PROCESS_CB(ProcessAtCnum);
}

static void
AtCindCallback()
{
  BT_HF_PROCESS_CB(ProcessAtCind);
}

static void
AtCopsCallback()
{
  BT_HF_PROCESS_CB(ProcessAtCops);
}

static void
AtClccCallback()
{
  BT_HF_PROCESS_CB(ProcessAtClcc);
}

static void
UnknownAtCallback(char *at_string)
{
  BT_HF_PROCESS_CB(ProcessUnknownAt, at_string);
}

static void
KeyPressedCallback()
{
  // No support
}

static bthf_callbacks_t sBluetoothHfpCallbacks = {
  sizeof(sBluetoothHfpCallbacks),
  ConnectionStateCallback,
  AudioStateCallback,
  VoiceRecognitionCallback,
  AnswerCallCallback,
  HangupCallCallback,
  VolumeControlCallback,
  DialCallCallback,
  DtmfCmdCallback,
  NoiceReductionCallback,
  AtChldCallback,
  AtCnumCallback,
  AtCindCallback,
  AtCopsCallback,
  AtClccCallback,
  UnknownAtCallback,
  KeyPressedCallback
};

static bool
IsValidDtmf(const char aChar) {
  // Valid DTMF: [*#0-9ABCD]
  return (aChar == '*' || aChar == '#') ||
         (aChar >= '0' && aChar <= '9') ||
         (aChar >= 'A' && aChar <= 'D');
}

class BluetoothHfpManager::GetVolumeTask : public nsISettingsServiceCallback
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
};

class BluetoothHfpManager::CloseScoTask : public Task
{
private:
  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(sBluetoothHfpManager);
    sBluetoothHfpManager->DisconnectSco();
  }
};

class BluetoothHfpManager::RespondToBLDNTask : public Task
{
private:
  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(sBluetoothHfpManager);

    if (!sBluetoothHfpManager->mDialingRequestProcessed) {
      sBluetoothHfpManager->mDialingRequestProcessed = true;
      sBluetoothHfpManager->SendResponse(BTHF_AT_RESPONSE_ERROR);
    }
  }
};

class BluetoothHfpManager::MainThreadTask : public nsRunnable
{
public:
  MainThreadTask(const int aCommand,
                 const nsAString& aParameter = EmptyString())
    : mCommand(aCommand), mParameter(aParameter)
  {
  }

  nsresult Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(sBluetoothHfpManager);

    switch (mCommand) {
      case MainThreadTaskCmd::NOTIFY_CONN_STATE_CHANGED:
        sBluetoothHfpManager->NotifyConnectionStateChanged(mParameter);
        break;
      case MainThreadTaskCmd::NOTIFY_DIALER:
        sBluetoothHfpManager->NotifyDialer(mParameter);
        break;
      case MainThreadTaskCmd::NOTIFY_SCO_VOLUME_CHANGED:
        {
          nsCOMPtr<nsIObserverService> os =
            mozilla::services::GetObserverService();
          NS_ENSURE_TRUE(os, NS_OK);

          os->NotifyObservers(nullptr, "bluetooth-volume-change",
                              mParameter.get());
        }
        break;
      case MainThreadTaskCmd::POST_TASK_RESPOND_TO_BLDN:
        MessageLoop::current()->
          PostDelayedTask(FROM_HERE, new RespondToBLDNTask(),
                          sWaitingForDialingInterval);
        break;
      case MainThreadTaskCmd::POST_TASK_CLOSE_SCO:
        MessageLoop::current()->
          PostDelayedTask(FROM_HERE, new CloseScoTask(),
                          sBusyToneInterval);
        break;
      default:
        BT_WARNING("MainThreadTask: Unknown command %d", mCommand);
        break;
    }

    return NS_OK;
  }

private:
  int mCommand;
  nsString mParameter;
};

NS_IMPL_ISUPPORTS1(BluetoothHfpManager::GetVolumeTask,
                   nsISettingsServiceCallback);

/**
 *  Call
 */
Call::Call()
{
  Reset();
}

void
Call::Reset()
{
  mState = nsITelephonyProvider::CALL_STATE_DISCONNECTED;
  mDirection = BTHF_CALL_DIRECTION_OUTGOING;
  mNumber.Truncate();
  mType = BTHF_CALL_ADDRTYPE_UNKNOWN;
}

bool
Call::IsActive()
{
  return (mState == nsITelephonyProvider::CALL_STATE_CONNECTED);
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

void
BluetoothHfpManager::Reset()
{
  mReceiveVgsFlag = false;
  mDialingRequestProcessed = true;

  mConnectionState = BTHF_CONNECTION_STATE_DISCONNECTED;
  mPrevConnectionState = BTHF_CONNECTION_STATE_DISCONNECTED;
  mAudioState = BTHF_AUDIO_STATE_DISCONNECTED;

  // Phone & Device CIND
  ResetCallArray();
  mCallSetupState = nsITelephonyProvider::CALL_STATE_DISCONNECTED;
  mBattChg = 5;
  mService = 0;
  mRoam = 0;
  mSignal = 0;

  mController = nullptr;
}

bool
BluetoothHfpManager::Init()
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE(InitHfpInterface(), false);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE(obs, false);

  if (NS_FAILED(obs->AddObserver(this, MOZSETTINGS_CHANGED_ID, false)) ||
      NS_FAILED(obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false))) {
    BT_WARNING("Failed to add observers!");
    return false;
  }

  hal::RegisterBatteryObserver(this);

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

bool
BluetoothHfpManager::InitHfpInterface()
{
  const bt_interface_t* btInf = GetBluetoothInterface();
  NS_ENSURE_TRUE(btInf, false);

  if (sBluetoothHfpInterface) {
    sBluetoothHfpInterface->cleanup();
    sBluetoothHfpInterface = nullptr;
  }

  bthf_interface_t *interface = (bthf_interface_t *)
    btInf->get_profile_interface(BT_PROFILE_HANDSFREE_ID);
  NS_ENSURE_TRUE(interface, false);

  NS_ENSURE_TRUE(BT_STATUS_SUCCESS ==
    interface->init(&sBluetoothHfpCallbacks), false);
  sBluetoothHfpInterface = interface;

  return true;
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
  DeinitHfpInterface();
}

void
BluetoothHfpManager::DeinitHfpInterface()
{
  NS_ENSURE_TRUE_VOID(GetBluetoothInterface());

  if (sBluetoothHfpInterface) {
    sBluetoothHfpInterface->cleanup();
    sBluetoothHfpInterface = nullptr;
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
    HandleVolumeChanged(nsDependentString(aData));
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
  mBattChg = (int) ceil(aBatteryInfo.level() * 5.0);
  UpdateDeviceCIND();
}

void
BluetoothHfpManager::ProcessConnectionState(bthf_connection_state_t aState,
                                            bt_bdaddr_t* aBdAddress)
{
  BT_LOGR("state %d", aState);

  mPrevConnectionState = mConnectionState;
  mConnectionState = aState;

  if (aState == BTHF_CONNECTION_STATE_CONNECTED) {
    BdAddressTypeToString(aBdAddress, mDeviceAddress);
    BT_HF_DISPATCH_MAIN(MainThreadTaskCmd::NOTIFY_CONN_STATE_CHANGED,
                        NS_LITERAL_STRING(BLUETOOTH_HFP_STATUS_CHANGED_ID));
  } else if (aState == BTHF_CONNECTION_STATE_DISCONNECTED) {
    DisconnectSco();
    BT_HF_DISPATCH_MAIN(MainThreadTaskCmd::NOTIFY_CONN_STATE_CHANGED,
                        NS_LITERAL_STRING(BLUETOOTH_HFP_STATUS_CHANGED_ID));
  }
}

void
BluetoothHfpManager::ProcessAudioState(bthf_audio_state_t aState,
                                       bt_bdaddr_t* aBdAddress)
{
  BT_LOGR("state %d", aState);

  mAudioState = aState;

  if (aState == BTHF_AUDIO_STATE_CONNECTED ||
      aState == BTHF_AUDIO_STATE_DISCONNECTED) {
    BT_HF_DISPATCH_MAIN(MainThreadTaskCmd::NOTIFY_CONN_STATE_CHANGED,
                        NS_LITERAL_STRING(BLUETOOTH_SCO_STATUS_CHANGED_ID));
  }
}

void
BluetoothHfpManager::ProcessAnswerCall()
{
  BT_HF_DISPATCH_MAIN(MainThreadTaskCmd::NOTIFY_DIALER,
                      NS_LITERAL_STRING("ATA"));
}

void
BluetoothHfpManager::ProcessHangupCall()
{
  BT_HF_DISPATCH_MAIN(MainThreadTaskCmd::NOTIFY_DIALER,
                      NS_LITERAL_STRING("CHUP"));
}

void
BluetoothHfpManager::ProcessVolumeControl(bthf_volume_type_t aType,
                                          int aVolume)
{
  NS_ENSURE_TRUE_VOID(aVolume >= 0 && aVolume <= 15);

  if (aType == BTHF_VOLUME_TYPE_MIC) {
    mCurrentVgm = aVolume;
  } else if (aType == BTHF_VOLUME_TYPE_SPK) {
    mReceiveVgsFlag = true;

    if (aVolume == mCurrentVgs) {
      // Keep current volume
      return;
    }

    nsString data;
    data.AppendInt(aVolume);
    BT_HF_DISPATCH_MAIN(MainThreadTaskCmd::NOTIFY_SCO_VOLUME_CHANGED, data);
  }
}

void
BluetoothHfpManager::ProcessDtmfCmd(char aDtmf)
{
  NS_ENSURE_TRUE_VOID(IsValidDtmf(aDtmf));

  nsAutoCString message("VTS=");
  message += aDtmf;
  BT_HF_DISPATCH_MAIN(MainThreadTaskCmd::NOTIFY_DIALER,
                      NS_ConvertUTF8toUTF16(message));
}

void
BluetoothHfpManager::ProcessAtChld(bthf_chld_type_t aChld)
{
  nsAutoCString message("CHLD=");
  message.AppendInt((int)aChld);
  BT_HF_DISPATCH_MAIN(MainThreadTaskCmd::NOTIFY_DIALER,
                      NS_ConvertUTF8toUTF16(message));
}

void BluetoothHfpManager::ProcessDialCall(char *aNumber)
{
  nsAutoCString message(aNumber);
  if (message.IsEmpty()) {
    // Redial: BLDN
    mDialingRequestProcessed = false;
    BT_HF_DISPATCH_MAIN(MainThreadTaskCmd::NOTIFY_DIALER,
                        NS_LITERAL_STRING("BLDN"));
  } else {
    nsAutoCString newMsg("ATD");

    if (message[0] == '>') {
      // Memory dial: ATD>xxx
      mDialingRequestProcessed = false;
      newMsg += message;
    } else {
      // Dial number: ATDxxx
      int end = message.FindChar(';');
      if (end < 0) {
        BT_WARNING("Couldn't get the number to dial");
        SendResponse(BTHF_AT_RESPONSE_OK);
        return;
      }
      newMsg += nsDependentCSubstring(message, 0, end);
    }

    BT_HF_DISPATCH_MAIN(MainThreadTaskCmd::NOTIFY_DIALER,
                        NS_ConvertUTF8toUTF16(newMsg));
  }

  if (!mDialingRequestProcessed) {
    BT_HF_DISPATCH_MAIN(MainThreadTaskCmd::POST_TASK_RESPOND_TO_BLDN);
  }
}

void
BluetoothHfpManager::ProcessAtCnum()
{
  NS_ENSURE_TRUE_VOID(!mMsisdn.IsEmpty());
  NS_ENSURE_TRUE_VOID(sBluetoothHfpInterface);

  nsAutoCString message("+CNUM: ,\"");
  message.Append(NS_ConvertUTF16toUTF8(mMsisdn).get());
  message.AppendLiteral("\",");
  message.AppendInt(BTHF_CALL_ADDRTYPE_UNKNOWN);
  message.AppendLiteral(",,4");

  NS_ENSURE_TRUE_VOID(BT_STATUS_SUCCESS ==
    sBluetoothHfpInterface->formatted_at_response(message.get()));
  NS_ENSURE_TRUE_VOID(BT_STATUS_SUCCESS ==
    sBluetoothHfpInterface->at_response(BTHF_AT_RESPONSE_OK, 0));
}

void
BluetoothHfpManager::ProcessAtCind()
{
  NS_ENSURE_TRUE_VOID(sBluetoothHfpInterface);

  int numActive = GetNumberOfCalls(nsITelephonyProvider::CALL_STATE_CONNECTED);
  int numHeld = GetNumberOfCalls(nsITelephonyProvider::CALL_STATE_HELD);

  bt_status_t status = sBluetoothHfpInterface->cind_response(
                          mService,
                          numActive,
                          numHeld,
                          ConvertToBthfCallState(mCallSetupState),
                          mSignal,
                          mRoam,
                          mBattChg);
  NS_ENSURE_TRUE_VOID(status == BT_STATUS_SUCCESS);
}

void
BluetoothHfpManager::ProcessAtCops()
{
  NS_ENSURE_TRUE_VOID(sBluetoothHfpInterface);
  NS_ENSURE_TRUE_VOID(BT_STATUS_SUCCESS ==
    sBluetoothHfpInterface->cops_response(
      NS_ConvertUTF16toUTF8(mOperatorName).get()));
}

void
BluetoothHfpManager::ProcessAtClcc()
{
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
}

void
BluetoothHfpManager::ProcessUnknownAt(char *aAtString)
{
  BT_LOGR("[%s]", aAtString);

  NS_ENSURE_TRUE_VOID(sBluetoothHfpInterface);
  NS_ENSURE_TRUE_VOID(BT_STATUS_SUCCESS ==
    sBluetoothHfpInterface->at_response(BTHF_AT_RESPONSE_ERROR, 0));
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
    } else if (mConnectionState == BTHF_CONNECTION_STATE_DISCONNECTED) {
      mDeviceAddress.AssignLiteral(BLUETOOTH_ADDRESS_NONE);
      if (mPrevConnectionState == BTHF_CONNECTION_STATE_DISCONNECTED) {
        // Bug 979160: This implies the outgoing connection failure.
        // When the outgoing hfp connection fails, state changes to disconnected
        // state. Since bluedroid would not report connecting state, but only
        // report connected/disconnected.
        OnConnect(NS_LITERAL_STRING("SocketConnectionError"));
      } else {
        OnDisconnect(EmptyString());
      }
      Reset();
    }
  }
}

void
BluetoothHfpManager::NotifyDialer(const nsAString& aCommand)
{
  BluetoothValue v;
  InfallibleTArray<BluetoothNamedValue> parameters;

  NS_NAMED_LITERAL_STRING(type, "bluetooth-dialer-command");
  NS_NAMED_LITERAL_STRING(name, "command");

  v = nsString(aCommand);
  parameters.AppendElement(BluetoothNamedValue(name, v));

  if (!BroadcastSystemMessage(type, parameters)) {
    BT_WARNING("Failed to broadcast system message to dialer");
  }
}

void
BluetoothHfpManager::HandleVolumeChanged(const nsAString& aData)
{
  MOZ_ASSERT(NS_IsMainThread());

  // The string that we're interested in will be a JSON string that looks like:
  //  {"key":"volumeup", "value":10}
  //  {"key":"volumedown", "value":2}
  JSContext* cx = nsContentUtils::GetSafeJSContext();
  NS_ENSURE_TRUE_VOID(cx);

  JS::Rooted<JS::Value> val(cx);
  NS_ENSURE_TRUE_VOID(JS_ParseJSON(cx, aData.BeginReading(), aData.Length(), &val));
  NS_ENSURE_TRUE_VOID(val.isObject());

  JS::Rooted<JSObject*> obj(cx, &val.toObject());
  JS::Rooted<JS::Value> key(cx);
  if (!JS_GetProperty(cx, obj, "key", &key) || !key.isString()) {
    return;
  }

  bool match;
  if (!JS_StringEqualsAscii(cx, key.toString(), AUDIO_VOLUME_BT_SCO_ID, &match) ||
      !match) {
    return;
  }

  JS::Rooted<JS::Value> value(cx);
  if (!JS_GetProperty(cx, obj, "value", &value) ||
      !value.isNumber()) {
    return;
  }

  mCurrentVgs = value.toNumber();

  // Adjust volume by headset and we don't have to send volume back to headset
  if (mReceiveVgsFlag) {
    mReceiveVgsFlag = false;
    return;
  }

  // Only send volume back when there's a connected headset
  if (IsConnected()) {
    NS_ENSURE_TRUE_VOID(sBluetoothHfpInterface);
    NS_ENSURE_TRUE_VOID(BT_STATUS_SUCCESS ==
      sBluetoothHfpInterface->volume_control(BTHF_VOLUME_TYPE_SPK,
                                             mCurrentVgs));
  }
}

void
BluetoothHfpManager::HandleVoiceConnectionChanged(uint32_t aClientId)
{
  nsCOMPtr<nsIMobileConnectionProvider> connection =
    do_GetService(NS_RILCONTENTHELPER_CONTRACTID);
  NS_ENSURE_TRUE_VOID(connection);

  nsCOMPtr<nsIDOMMozMobileConnectionInfo> voiceInfo;
  connection->GetVoiceConnectionInfo(aClientId, getter_AddRefs(voiceInfo));
  NS_ENSURE_TRUE_VOID(voiceInfo);

  nsString type;
  voiceInfo->GetType(type);
  mPhoneType = GetPhoneType(type);

  bool roaming;
  voiceInfo->GetRoaming(&roaming);
  mRoam = (roaming) ? 1 : 0;

  // Service
  nsString regState;
  voiceInfo->GetState(regState);
  mService = (regState.EqualsLiteral("registered")) ? 1 : 0;

  // Signal
  JSContext* cx = nsContentUtils::GetSafeJSContext();
  NS_ENSURE_TRUE_VOID(cx);
  JS::Rooted<JS::Value> value(cx);
  voiceInfo->GetRelSignalStrength(&value);
  NS_ENSURE_TRUE_VOID(value.isNumber());
  mSignal = (int)ceil(value.toNumber() / 20.0);

  UpdateDeviceCIND();

  // Operator name
  nsCOMPtr<nsIDOMMozMobileNetworkInfo> network;
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
  nsCOMPtr<nsIIccProvider> icc =
    do_GetService(NS_RILCONTENTHELPER_CONTRACTID);
  NS_ENSURE_TRUE_VOID(icc);

  nsCOMPtr<nsIDOMMozIccInfo> iccInfo;
  icc->GetIccInfo(aClientId, getter_AddRefs(iccInfo));
  NS_ENSURE_TRUE_VOID(iccInfo);

  nsCOMPtr<nsIDOMMozGsmIccInfo> gsmIccInfo = do_QueryInterface(iccInfo);
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

void
BluetoothHfpManager::SendCLCC(Call& aCall, int aIndex)
{
  NS_ENSURE_TRUE_VOID(aCall.mState !=
                        nsITelephonyProvider::CALL_STATE_DISCONNECTED);
  NS_ENSURE_TRUE_VOID(sBluetoothHfpInterface);

  bthf_call_state_t callState = ConvertToBthfCallState(aCall.mState);

  if (mPhoneType == PhoneType::CDMA && aIndex == 1 && aCall.IsActive()) {
    callState = (mCdmaSecondCall.IsActive()) ? BTHF_CALL_STATE_HELD :
                                               BTHF_CALL_STATE_ACTIVE;
  }

  bt_status_t status = sBluetoothHfpInterface->clcc_response(
                          aIndex,
                          aCall.mDirection,
                          callState,
                          BTHF_CALL_TYPE_VOICE,
                          BTHF_CALL_MPTY_TYPE_SINGLE,
                          NS_ConvertUTF16toUTF8(aCall.mNumber).get(),
                          aCall.mType);
  NS_ENSURE_TRUE_VOID(status == BT_STATUS_SUCCESS);
}

void
BluetoothHfpManager::SendLine(const char* aMessage)
{
  NS_ENSURE_TRUE_VOID(sBluetoothHfpInterface);
  NS_ENSURE_TRUE_VOID(BT_STATUS_SUCCESS ==
    sBluetoothHfpInterface->formatted_at_response(aMessage));
}

void
BluetoothHfpManager::SendResponse(bthf_at_response_t aResponseCode)
{
  NS_ENSURE_TRUE_VOID(sBluetoothHfpInterface);
  NS_ENSURE_TRUE_VOID(BT_STATUS_SUCCESS ==
    sBluetoothHfpInterface->at_response(aResponseCode, 0));
}

void
BluetoothHfpManager::UpdatePhoneCIND(uint32_t aCallIndex, bool aSend)
{
  // Update callsetup state
  uint16_t callState = mCurrentCallArray[aCallIndex].mState;
  if (callState == nsITelephonyProvider::CALL_STATE_CONNECTED ||
      callState == nsITelephonyProvider::CALL_STATE_HELD) {
    mCallSetupState = nsITelephonyProvider::CALL_STATE_DISCONNECTED;
  } else {
    mCallSetupState = callState;
  }

  NS_ENSURE_TRUE_VOID(sBluetoothHfpInterface);

  int numActive = GetNumberOfCalls(nsITelephonyProvider::CALL_STATE_CONNECTED);
  int numHeld = GetNumberOfCalls(nsITelephonyProvider::CALL_STATE_HELD);
  bthf_call_state_t bthfCallState = ConvertToBthfCallState(mCallSetupState);
  nsAutoCString number = NS_ConvertUTF16toUTF8(mCurrentCallArray[aCallIndex].mNumber);
  bthf_call_addrtype_t type = mCurrentCallArray[aCallIndex].mType;

  BT_LOGR("[%d] state %d => BTHF: active[%d] held[%d] state[%d]",
          aCallIndex, callState, numActive, numHeld, bthfCallState);

  NS_ENSURE_TRUE_VOID(BT_STATUS_SUCCESS ==
    sBluetoothHfpInterface->phone_state_change(
      numActive, numHeld, bthfCallState, number.get(), type));
}

void
BluetoothHfpManager::UpdateDeviceCIND()
{
  NS_ENSURE_TRUE_VOID(sBluetoothHfpInterface);
  NS_ENSURE_TRUE_VOID(BT_STATUS_SUCCESS ==
    sBluetoothHfpInterface->device_status_notification(
      (bthf_network_state_t) mService,
      (bthf_service_type_t) mRoam,
      mSignal,
      mBattChg));
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

bthf_call_state_t
BluetoothHfpManager::ConvertToBthfCallState(int aCallState)
{
  bthf_call_state_t state;

  // Refer to AOSP BluetoothPhoneService.convertCallState
  if (aCallState == nsITelephonyProvider::CALL_STATE_INCOMING) {
    state = (FindFirstCall(nsITelephonyProvider::CALL_STATE_CONNECTED)) ?
              BTHF_CALL_STATE_WAITING :
              BTHF_CALL_STATE_INCOMING;
  } else if (aCallState == nsITelephonyProvider::CALL_STATE_DIALING) {
    state = BTHF_CALL_STATE_DIALING;
  } else if (aCallState == nsITelephonyProvider::CALL_STATE_ALERTING) {
    state = BTHF_CALL_STATE_ALERTING;
  } else if (aCallState == nsITelephonyProvider::CALL_STATE_CONNECTED) {
    state = BTHF_CALL_STATE_ACTIVE;
  } else if (aCallState == nsITelephonyProvider::CALL_STATE_HELD) {
    state = BTHF_CALL_STATE_HELD;
  } else { // disconnected
    state = BTHF_CALL_STATE_IDLE;
  }

  return state;
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
  if (!IsConnected()) {
    // Normal case. No need to print out warnings.
    return;
  }

  while (aCallIndex >= mCurrentCallArray.Length()) {
    Call call;
    mCurrentCallArray.AppendElement(call);
  }

  mCurrentCallArray[aCallIndex].mState = aCallState;
  mCurrentCallArray[aCallIndex].mNumber = aNumber;
  mCurrentCallArray[aCallIndex].mDirection = (aIsOutgoing) ?
                                              BTHF_CALL_DIRECTION_OUTGOING :
                                              BTHF_CALL_DIRECTION_INCOMING;
  // Same logic as implementation in ril_worker.js
  if (aNumber.Length() && aNumber[0] == '+') {
    mCurrentCallArray[aCallIndex].mType = BTHF_CALL_ADDRTYPE_INTERNATIONAL;
  }

  UpdatePhoneCIND(aCallIndex, aSend);

  switch (aCallState) {
    case nsITelephonyProvider::CALL_STATE_DIALING:
      // We've send Dialer a dialing request and this is the response.
      if (!mDialingRequestProcessed) {
        SendResponse(BTHF_AT_RESPONSE_OK);
        mDialingRequestProcessed = true;
      }
      break;
    case nsITelephonyProvider::CALL_STATE_DISCONNECTED:
      // -1 is necessary because call 0 is an invalid (padding) call object.
      if (mCurrentCallArray.Length() - 1 ==
          GetNumberOfCalls(nsITelephonyProvider::CALL_STATE_DISCONNECTED)) {
        // In order to let user hear busy tone via connected Bluetooth headset,
        // we postpone the timing of dropping SCO.
        if (aError.Equals(NS_LITERAL_STRING("BusyError"))) {
          // FIXME: UpdatePhoneCIND later since it causes SCO close but
          // Dialer is still playing busy tone via HF.
          BT_HF_DISPATCH_MAIN(MainThreadTaskCmd::POST_TASK_CLOSE_SCO);
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
  mCdmaSecondCall.mDirection = BTHF_CALL_DIRECTION_INCOMING;

  mCdmaSecondCall.mNumber = aNumber;
  if (aNumber[0] == '+') {
    mCdmaSecondCall.mType = BTHF_CALL_ADDRTYPE_INTERNATIONAL;
  }

  // FIXME: check CDMA + bluedroid
  //UpdateCIND(CINDType::CALLSETUP, CallSetupState::INCOMING, true);
}

void
BluetoothHfpManager::AnswerWaitingCall()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mPhoneType == PhoneType::CDMA);

  // Pick up second call. First call is held now.
  mCdmaSecondCall.mState = nsITelephonyProvider::CALL_STATE_CONNECTED;
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
                             nsITelephonyProvider::CALL_STATE_HELD :
                             nsITelephonyProvider::CALL_STATE_CONNECTED;
}

bool
BluetoothHfpManager::ConnectSco()
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE(!sInShutdown, false);
  NS_ENSURE_TRUE(IsConnected() && !IsScoConnected(), false);
  NS_ENSURE_TRUE(sBluetoothHfpInterface, false);

  bt_bdaddr_t deviceBdAddress;
  StringToBdAddressType(mDeviceAddress, &deviceBdAddress);
  NS_ENSURE_TRUE(BT_STATUS_SUCCESS ==
    sBluetoothHfpInterface->connect_audio(&deviceBdAddress), false);

  return true;
}

bool
BluetoothHfpManager::DisconnectSco()
{
  NS_ENSURE_TRUE(IsScoConnected(), false);
  NS_ENSURE_TRUE(sBluetoothHfpInterface, false);

  bt_bdaddr_t deviceBdAddress;
  StringToBdAddressType(mDeviceAddress, &deviceBdAddress);
  NS_ENSURE_TRUE(BT_STATUS_SUCCESS ==
    sBluetoothHfpInterface->disconnect_audio(&deviceBdAddress), false);

  return true;
}

bool
BluetoothHfpManager::IsScoConnected()
{
  return (mAudioState == BTHF_AUDIO_STATE_CONNECTED);
}

bool
BluetoothHfpManager::IsConnected()
{
  return (mConnectionState == BTHF_CONNECTION_STATE_SLC_CONNECTED ||
          mConnectionState == BTHF_CONNECTION_STATE_CONNECTED);
}

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

  bt_bdaddr_t deviceBdAddress;
  StringToBdAddressType(aDeviceAddress, &deviceBdAddress);

  bt_status_t result = sBluetoothHfpInterface->connect(&deviceBdAddress);
  if (BT_STATUS_SUCCESS != result) {
    BT_LOGR("Failed to connect: %x", result);
    aController->NotifyCompletion(NS_LITERAL_STRING(ERR_CONNECTION_FAILED));
    return;
  }

  mDeviceAddress = aDeviceAddress;
  mController = aController;
}

void
BluetoothHfpManager::Disconnect(BluetoothProfileController* aController)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mController);

  if (!sBluetoothHfpInterface) {
    BT_LOGR("sBluetoothHfpInterface is null");
    aController->NotifyCompletion(NS_LITERAL_STRING(ERR_NO_AVAILABLE_RESOURCE));
    return;
  }

  bt_bdaddr_t deviceBdAddress;
  StringToBdAddressType(mDeviceAddress, &deviceBdAddress);

  bt_status_t result = sBluetoothHfpInterface->disconnect(&deviceBdAddress);
  if (BT_STATUS_SUCCESS != result) {
    BT_LOGR("Failed to disconnect: %x", result);
    aController->NotifyCompletion(NS_LITERAL_STRING(ERR_DISCONNECTION_FAILED));
    return;
  }

  mController = aController;
}

void
BluetoothHfpManager::OnConnect(const nsAString& aErrorStr)
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

NS_IMPL_ISUPPORTS1(BluetoothHfpManager, nsIObserver)
