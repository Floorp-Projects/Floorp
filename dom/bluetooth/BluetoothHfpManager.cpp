/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BluetoothHfpManager.h"

#include "BluetoothA2dpManager.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "BluetoothSocket.h"
#include "BluetoothUtils.h"
#include "BluetoothUuid.h"

#include "MobileConnection.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsContentUtils.h"
#include "nsIAudioManager.h"
#include "nsIObserverService.h"
#include "nsISettingsService.h"
#include "nsITelephonyProvider.h"
#include "nsRadioInterfaceLayer.h"

/**
 * BRSF bitmask of AG supported features. See 4.34.1 "Bluetooth Defined AT
 * Capabilities" in Bluetooth hands-free profile 1.6
 */
#define BRSF_BIT_THREE_WAY_CALLING         1
#define BSRF_BIT_EC_NR_FUNCTION            (1 << 1)
#define BRSF_BIT_VOICE_RECOGNITION         (1 << 2)
#define BRSF_BIT_IN_BAND_RING_TONE         (1 << 3)
#define BRSF_BIT_ATTACH_NUM_TO_VOICE_TAG   (1 << 4)
#define BRSF_BIT_ABILITY_TO_REJECT_CALL    (1 << 5)
#define BRSF_BIT_ENHANCED_CALL_STATUS      (1 << 6)
#define BRSF_BIT_ENHANCED_CALL_CONTROL     (1 << 7)
#define BRSF_BIT_EXTENDED_ERR_RESULT_CODES (1 << 8)
#define BRSF_BIT_CODEC_NEGOTIATION         (1 << 9)

/**
 * These constants are used in result code such as +CLIP and +CCWA. The value
 * of these constants is the same as TOA_INTERNATIONAL/TOA_UNKNOWN defined in
 * ril_consts.js
 */
#define TOA_UNKNOWN 0x81
#define TOA_INTERNATIONAL 0x91

#define CR_LF "\xd\xa";

#define MOZSETTINGS_CHANGED_ID               "mozsettings-changed"
#define MOBILE_CONNECTION_ICCINFO_CHANGED_ID "mobile-connection-iccinfo-changed"
#define MOBILE_CONNECTION_VOICE_CHANGED_ID   "mobile-connection-voice-changed"
#define AUDIO_VOLUME_BT_SCO_ID               "audio.volume.bt_sco"

using namespace mozilla;
using namespace mozilla::ipc;
USING_BLUETOOTH_NAMESPACE

namespace {
  StaticRefPtr<BluetoothHfpManager> gBluetoothHfpManager;
  bool gInShutdown = false;
  static const char kHfpCrlf[] = "\xd\xa";

  // Sending ringtone related
  static bool sStopSendingRingFlag = true;
  static int sRingInterval = 3000; //unit: ms

  // Wait for 2 seconds for Dialer processing event 'BLDN'. '2' seconds is a
  // magic number. The mechanism should be revised once we can get call history.
  static int sWaitingForDialingInterval = 2000; //unit: ms

  // Wait 3.7 seconds until Dialer stops playing busy tone. '3' seconds is the
  // time window set in Dialer and the extra '0.7' second is a magic number.
  // The mechanism should be revised once we know the exact time at which
  // Dialer stops playing.
  static int sBusyToneInterval = 3700; //unit: ms
} // anonymous namespace

/* CallState for sCINDItems[CINDType::CALL].value
 * - NO_CALL: there are no calls in progress
 * - IN_PROGRESS: at least one call is in progress
 */
enum CallState {
  NO_CALL,
  IN_PROGRESS
};

/* CallSetupState for sCINDItems[CINDType::CALLSETUP].value
 * - NO_CALLSETUP: not currently in call set up
 * - INCOMING: an incoming call process ongoing
 * - OUTGOING: an outgoing call set up is ongoing
 * - OUTGOING_ALERTING: remote party being alerted in an outgoing call
 */
enum CallSetupState {
  NO_CALLSETUP,
  INCOMING,
  OUTGOING,
  OUTGOING_ALERTING
};

/* CallHeldState for sCINDItems[CINDType::CALLHELD].value
 * - NO_CALLHELD: no calls held
 * - ONHOLD_ACTIVE: both an active and a held call
 * - ONHOLD_NOACTIVE: call on hold, no active call
 */
enum CallHeldState {
  NO_CALLHELD,
  ONHOLD_ACTIVE,
  ONHOLD_NOACTIVE
};

typedef struct {
  const char* name;
  const char* range;
  int value;
  bool activated;
} CINDItem;

enum CINDType {
  BATTCHG = 1,
  CALL,
  CALLHELD,
  CALLSETUP,
  SERVICE,
  SIGNAL,
  ROAM
};

static CINDItem sCINDItems[] = {
  {},
  {"battchg", "0-5", 5, true},
  {"call", "0,1", CallState::NO_CALL, true},
  {"callheld", "0-2", CallHeldState::NO_CALLHELD, true},
  {"callsetup", "0-3", CallSetupState::NO_CALLSETUP, true},
  {"service", "0,1", 0, true},
  {"signal", "0-5", 0, true},
  {"roam", "0,1", 0, true}
};

class mozilla::dom::bluetooth::Call {
  public:
    Call(uint16_t aState = nsITelephonyProvider::CALL_STATE_DISCONNECTED,
         bool aDirection = false,
         const nsAString& aNumber = EmptyString(),
         int aType = TOA_UNKNOWN)
      : mState(aState), mDirection(aDirection), mNumber(aNumber), mType(aType)
    {
    }

    uint16_t mState;
    bool mDirection; // true: incoming call; false: outgoing call
    nsString mNumber;
    int mType;
};

class BluetoothHfpManager::GetVolumeTask : public nsISettingsServiceCallback
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD
  Handle(const nsAString& aName, const JS::Value& aResult)
  {
    MOZ_ASSERT(NS_IsMainThread());

    JSContext *cx = nsContentUtils::GetCurrentJSContext();
    NS_ENSURE_TRUE(cx, NS_OK);

    if (!aResult.isNumber()) {
      NS_WARNING("'" AUDIO_VOLUME_BT_SCO_ID "' is not a number!");
      return NS_OK;
    }

    BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
    hfp->mCurrentVgs = aResult.toNumber();

    return NS_OK;
  }

  NS_IMETHOD
  HandleError(const nsAString& aName)
  {
    NS_WARNING("Unable to get value for '" AUDIO_VOLUME_BT_SCO_ID "'");
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS1(BluetoothHfpManager::GetVolumeTask,
                   nsISettingsServiceCallback);

NS_IMETHODIMP
BluetoothHfpManager::Observe(nsISupports* aSubject,
                             const char* aTopic,
                             const PRUnichar* aData)
{
  if (!strcmp(aTopic, MOZSETTINGS_CHANGED_ID)) {
    HandleVolumeChanged(nsDependentString(aData));
  } else if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    HandleShutdown();
  } else if (!strcmp(aTopic, MOBILE_CONNECTION_ICCINFO_CHANGED_ID)) {
    HandleIccInfoChanged();
  } else if (!strcmp(aTopic, MOBILE_CONNECTION_VOICE_CHANGED_ID)) {
    HandleVoiceConnectionChanged();
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
  int level = ceil(aBatteryInfo.level() * 5.0);
  if (level != sCINDItems[CINDType::BATTCHG].value) {
    sCINDItems[CINDType::BATTCHG].value = level;
    SendCommand("+CIEV:", CINDType::BATTCHG);
  }
}

class BluetoothHfpManager::RespondToBLDNTask : public Task
{
private:
  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(gBluetoothHfpManager);

    if (!gBluetoothHfpManager->mDialingRequestProcessed) {
      gBluetoothHfpManager->mDialingRequestProcessed = true;
      gBluetoothHfpManager->SendLine("ERROR");
    }
  }
};

class BluetoothHfpManager::SendRingIndicatorTask : public Task
{
public:
  SendRingIndicatorTask(const nsAString& aNumber, int aType)
    : mNumber(aNumber)
    , mType(aType)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    // Stop sending RING indicator
    if (sStopSendingRingFlag) {
      return;
    }

    if (!gBluetoothHfpManager) {
      NS_WARNING("BluetoothHfpManager no longer exists, cannot send ring!");
      return;
    }

    nsAutoCString ringMsg("RING");
    gBluetoothHfpManager->SendLine(ringMsg.get());

    if (!mNumber.IsEmpty()) {
      nsAutoCString clipMsg("+CLIP: \"");
      clipMsg.Append(NS_ConvertUTF16toUTF8(mNumber).get());
      clipMsg.AppendLiteral("\",");
      clipMsg.AppendInt(mType);
      gBluetoothHfpManager->SendLine(clipMsg.get());
    }

    MessageLoop::current()->
      PostDelayedTask(FROM_HERE,
                      new SendRingIndicatorTask(mNumber, mType),
                      sRingInterval);
  }

private:
  nsString mNumber;
  int mType;
};

class BluetoothHfpManager::CloseScoTask : public Task
{
private:
  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(gBluetoothHfpManager);

    gBluetoothHfpManager->DisconnectSco();
  }
};

static bool
IsValidDtmf(const char aChar) {
  // Valid DTMF: [*#0-9ABCD]
  if (aChar == '*' || aChar == '#') {
    return true;
  } else if (aChar >= '0' && aChar <= '9') {
    return true;
  } else if (aChar >= 'A' && aChar <= 'D') {
    return true;
  }
  return false;
}

static bool
IsMandatoryIndicator(const CINDType aType) {
  return (aType == CINDType::CALL) ||
         (aType == CINDType::CALLHELD) ||
         (aType == CINDType::CALLSETUP);
}

BluetoothHfpManager::BluetoothHfpManager()
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
}

void
BluetoothHfpManager::Reset()
{
  sStopSendingRingFlag = true;
  sCINDItems[CINDType::CALL].value = CallState::NO_CALL;
  sCINDItems[CINDType::CALLSETUP].value = CallSetupState::NO_CALLSETUP;
  sCINDItems[CINDType::CALLHELD].value = CallHeldState::NO_CALLHELD;
  for (uint8_t i = 1; i < ArrayLength(sCINDItems); i++) {
    sCINDItems[i].activated = true;
  }

  mCCWA = false;
  mCLIP = false;
  mCMEE = false;
  mCMER = false;
  mReceiveVgsFlag = false;
  mDialingRequestProcessed = true;

  // We disable BSIR by default as it requires OEM implement BT SCO + SPEAKER
  // output audio path in audio driver. OEM can enable BSIR by setting
  // mBSIR=true here.
  //
  // Please see Bug 878728 for more information.
  mBSIR = false;

  ResetCallArray();
}

bool
BluetoothHfpManager::Init()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE(obs, false);

  if (NS_FAILED(obs->AddObserver(this, MOZSETTINGS_CHANGED_ID, false)) ||
      NS_FAILED(obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false)) ||
      NS_FAILED(obs->AddObserver(this, MOBILE_CONNECTION_ICCINFO_CHANGED_ID, false)) ||
      NS_FAILED(obs->AddObserver(this, MOBILE_CONNECTION_VOICE_CHANGED_ID, false))) {
    BT_WARNING("Failed to add observers!");
    return false;
  }

  hal::RegisterBatteryObserver(this);

  mListener = new BluetoothTelephonyListener();
  if (!mListener->StartListening()) {
    NS_WARNING("Failed to start listening RIL");
    return false;
  }

  nsCOMPtr<nsISettingsService> settings =
    do_GetService("@mozilla.org/settingsService;1");
  NS_ENSURE_TRUE(settings, false);

  nsCOMPtr<nsISettingsServiceLock> settingsLock;
  nsresult rv = settings->CreateLock(getter_AddRefs(settingsLock));
  NS_ENSURE_SUCCESS(rv, false);

  nsRefPtr<GetVolumeTask> callback = new GetVolumeTask();
  rv = settingsLock->Get(AUDIO_VOLUME_BT_SCO_ID, callback);
  NS_ENSURE_SUCCESS(rv, false);

  Listen();

  mScoSocket = new BluetoothSocket(this,
                                   BluetoothSocketType::SCO,
                                   true,
                                   false);
  mScoSocketStatus = mScoSocket->GetConnectionStatus();
  ListenSco();
  return true;
}

BluetoothHfpManager::~BluetoothHfpManager()
{
  if (!mListener->StopListening()) {
    NS_WARNING("Failed to stop listening RIL");
  }
  mListener = nullptr;

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE_VOID(obs);

  if (NS_FAILED(obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) ||
      NS_FAILED(obs->RemoveObserver(this, MOZSETTINGS_CHANGED_ID)) ||
      NS_FAILED(obs->RemoveObserver(this, MOBILE_CONNECTION_ICCINFO_CHANGED_ID)) ||
      NS_FAILED(obs->RemoveObserver(this, MOBILE_CONNECTION_VOICE_CHANGED_ID))) {
    BT_WARNING("Failed to remove observers!");
  }

  hal::UnregisterBatteryObserver(this);
}

//static
BluetoothHfpManager*
BluetoothHfpManager::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  // If we already exist, exit early
  if (gBluetoothHfpManager) {
    return gBluetoothHfpManager;
  }

  // If we're in shutdown, don't create a new instance
  if (gInShutdown) {
    NS_WARNING("BluetoothHfpManager can't be created during shutdown");
    return nullptr;
  }

  // Create new instance, register, return
  BluetoothHfpManager* manager = new BluetoothHfpManager();
  NS_ENSURE_TRUE(manager->Init(), nullptr);

  gBluetoothHfpManager = manager;
  return gBluetoothHfpManager;
}

void
BluetoothHfpManager::NotifyStatusChanged(const nsAString& aType)
{
  nsString type, name;
  BluetoothValue v;
  InfallibleTArray<BluetoothNamedValue> parameters;
  type = aType;

  name.AssignLiteral("connected");
  if (type.EqualsLiteral("bluetooth-hfp-status-changed")) {
    v = IsConnected();
  } else if (type.EqualsLiteral("bluetooth-sco-status-changed")) {
    v = IsScoConnected();
  } else {
    NS_WARNING("Wrong type for NotifyStatusChanged");
    return;
  }
  parameters.AppendElement(BluetoothNamedValue(name, v));

  name.AssignLiteral("address");
  v = mDeviceAddress;
  parameters.AppendElement(BluetoothNamedValue(name, v));

  if (!BroadcastSystemMessage(type, parameters)) {
    NS_WARNING("Failed to broadcast system message to settings");
    return;
  }
}

void
BluetoothHfpManager::NotifyDialer(const nsAString& aCommand)
{
  nsString type, name;
  BluetoothValue v;
  InfallibleTArray<BluetoothNamedValue> parameters;
  type.AssignLiteral("bluetooth-dialer-command");

  name.AssignLiteral("command");
  v = nsString(aCommand);
  parameters.AppendElement(BluetoothNamedValue(name, v));

  if (!BroadcastSystemMessage(type, parameters)) {
    NS_WARNING("Failed to broadcast system message to dialer");
    return;
  }
}

void
BluetoothHfpManager::NotifyAudioManager(bool aStatus)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs =
    do_GetService("@mozilla.org/observer-service;1");
  NS_ENSURE_TRUE_VOID(obs);

  nsAutoString data;
  data.AppendInt(aStatus);

  if (NS_FAILED(obs->NotifyObservers(this,
                                     BLUETOOTH_SCO_STATUS_CHANGED_ID,
                                     data.BeginReading()))) {
    NS_WARNING("Failed to notify bluetooth-sco-status-changed observsers!");
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
  if (!JS_GetProperty(cx, obj, "key", key.address()) || !key.isString()) {
    return;
  }

  JSBool match;
  if (!JS_StringEqualsAscii(cx, key.toString(), AUDIO_VOLUME_BT_SCO_ID, &match) ||
      (match != JS_TRUE)) {
    return;
  }

  JS::Rooted<JS::Value> value(cx);
  if (!JS_GetProperty(cx, obj, "value", value.address())||
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
    SendCommand("+VGS: ", mCurrentVgs);
  }
}

void
BluetoothHfpManager::HandleVoiceConnectionChanged()
{
  nsCOMPtr<nsIMobileConnectionProvider> connection =
    do_GetService(NS_RILCONTENTHELPER_CONTRACTID);
  NS_ENSURE_TRUE_VOID(connection);

  nsIDOMMozMobileConnectionInfo* voiceInfo;
  connection->GetVoiceConnectionInfo(&voiceInfo);
  NS_ENSURE_TRUE_VOID(voiceInfo);

  bool roaming;
  voiceInfo->GetRoaming(&roaming);
  UpdateCIND(CINDType::ROAM, roaming);

  bool service = false;
  nsString regState;
  voiceInfo->GetState(regState);
  if (regState.EqualsLiteral("registered")) {
    service = true;
  }
  UpdateCIND(CINDType::SERVICE, service);

  uint8_t signal;
  JS::Value value;
  voiceInfo->GetRelSignalStrength(&value);
  if (!value.isNumber()) {
    NS_WARNING("Failed to get relSignalStrength in BluetoothHfpManager");
    return;
  }
  signal = ceil(value.toNumber() / 20.0);
  UpdateCIND(CINDType::SIGNAL, signal);

  /**
   * Possible return values for mode are:
   * - null (unknown): set mNetworkSelectionMode to 0 (auto)
   * - automatic: set mNetworkSelectionMode to 0 (auto)
   * - manual: set mNetworkSelectionMode to 1 (manual)
   */
  nsString mode;
  connection->GetNetworkSelectionMode(mode);
  if (mode.EqualsLiteral("manual")) {
    mNetworkSelectionMode = 1;
  } else {
    mNetworkSelectionMode = 0;
  }

  nsIDOMMozMobileNetworkInfo* network;
  voiceInfo->GetNetwork(&network);
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
    NS_WARNING("The operator name was longer than 16 characters. We cut it.");
    mOperatorName.Left(mOperatorName, 16);
  }
}

void
BluetoothHfpManager::HandleIccInfoChanged()
{
  nsCOMPtr<nsIMobileConnectionProvider> connection =
    do_GetService(NS_RILCONTENTHELPER_CONTRACTID);
  NS_ENSURE_TRUE_VOID(connection);

  nsIDOMMozMobileICCInfo* iccInfo;
  connection->GetIccInfo(&iccInfo);
  NS_ENSURE_TRUE_VOID(iccInfo);
  iccInfo->GetMsisdn(mMsisdn);
}

void
BluetoothHfpManager::HandleShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  gInShutdown = true;
  Disconnect();
  DisconnectSco();
  gBluetoothHfpManager = nullptr;
}

// Virtual function of class SocketConsumer
void
BluetoothHfpManager::ReceiveSocketData(BluetoothSocket* aSocket,
                                       nsAutoPtr<UnixSocketRawData>& aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aSocket);

  nsAutoCString msg((const char*)aMessage->mData.get(), aMessage->mSize);
  msg.StripWhitespace();

  nsTArray<nsCString> atCommandValues;

  // For more information, please refer to 4.34.1 "Bluetooth Defined AT
  // Capabilities" in Bluetooth hands-free profile 1.6
  if (msg.Find("AT+BRSF=") != -1) {
    uint32_t brsf = BRSF_BIT_THREE_WAY_CALLING |
                    BRSF_BIT_ABILITY_TO_REJECT_CALL |
                    BRSF_BIT_ENHANCED_CALL_STATUS;

    if (mBSIR) {
      brsf |= BRSF_BIT_IN_BAND_RING_TONE;
    }

    SendCommand("+BRSF: ", brsf);
  } else if (msg.Find("AT+CIND=?") != -1) {
    // Asking for CIND range
    SendCommand("+CIND: ", 0);
  } else if (msg.Find("AT+CIND?") != -1) {
    // Asking for CIND value
    SendCommand("+CIND: ", 1);
  } else if (msg.Find("AT+CMER=") != -1) {
    /**
     * SLC establishment is done when AT+CMER has been received.
     * Do nothing but respond with "OK".
     */
    ParseAtCommand(msg, 8, atCommandValues);

    if (atCommandValues.Length() < 4) {
      NS_WARNING("Could't get the value of command [AT+CMER=]");
      goto respond_with_ok;
    }

    if (!atCommandValues[0].EqualsLiteral("3") ||
        !atCommandValues[1].EqualsLiteral("0") ||
        !atCommandValues[2].EqualsLiteral("0")) {
      NS_WARNING("Wrong value of CMER");
      goto respond_with_ok;
    }

    mCMER = atCommandValues[3].EqualsLiteral("1");
  } else if (msg.Find("AT+CMEE=") != -1) {
    ParseAtCommand(msg, 8, atCommandValues);

    if (atCommandValues.IsEmpty()) {
      NS_WARNING("Could't get the value of command [AT+CMEE=]");
      goto respond_with_ok;
    }

    // AT+CMEE = 0: +CME ERROR shall not be used
    // AT+CMEE = 1: use numeric <err>
    // AT+CMEE = 2: use verbose <err>
    mCMEE = !atCommandValues[0].EqualsLiteral("0");
  } else if (msg.Find("AT+COPS=") != -1) {
    ParseAtCommand(msg, 8, atCommandValues);

    if (atCommandValues.Length() != 2) {
      NS_WARNING("Could't get the value of command [AT+COPS=]");
      goto respond_with_ok;
    }

    // Handsfree only support AT+COPS=3,0
    if (!atCommandValues[0].EqualsLiteral("3") ||
        !atCommandValues[1].EqualsLiteral("0")) {
      if (mCMEE) {
        SendCommand("+CME ERROR: ", BluetoothCmeError::OPERATION_NOT_SUPPORTED);
      } else {
        SendLine("ERROR");
      }
      return;
    }
  } else if (msg.Find("AT+COPS?") != -1) {
    nsAutoCString message("+COPS: ");
    message.AppendInt(mNetworkSelectionMode);
    message.AppendLiteral(",0,\"");
    message.Append(NS_ConvertUTF16toUTF8(mOperatorName));
    message.AppendLiteral("\"");
    SendLine(message.get());
  } else if (msg.Find("AT+VTS=") != -1) {
    ParseAtCommand(msg, 7, atCommandValues);

    if (atCommandValues.Length() != 1) {
      NS_WARNING("Couldn't get the value of command [AT+VTS=]");
      goto respond_with_ok;
    }

    if (IsValidDtmf(atCommandValues[0].get()[0])) {
      nsAutoCString message("VTS=");
      message += atCommandValues[0].get()[0];
      NotifyDialer(NS_ConvertUTF8toUTF16(message));
    }
  } else if (msg.Find("AT+VGM=") != -1) {
    ParseAtCommand(msg, 7, atCommandValues);

    if (atCommandValues.IsEmpty()) {
      NS_WARNING("Couldn't get the value of command [AT+VGM]");
      goto respond_with_ok;
    }

    nsresult rv;
    int vgm = atCommandValues[0].ToInteger(&rv);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to extract microphone volume from bluetooth headset!");
      goto respond_with_ok;
    }

    NS_ASSERTION(vgm >= 0 && vgm <= 15, "Received invalid VGM value");
    mCurrentVgm = vgm;
  } else if (msg.Find("AT+CHLD=?") != -1) {
    SendLine("+CHLD: (0,1,2)");
  } else if (msg.Find("AT+CHLD=") != -1) {
    ParseAtCommand(msg, 8, atCommandValues);

    if (atCommandValues.IsEmpty()) {
      NS_WARNING("Could't get the value of command [AT+CHLD=]");
      goto respond_with_ok;
    }

    /**
     * The following three cases are supported:
     * AT+CHLD=0 - Releases all held calls or sets User Determined User Busy
     *             (UDUB) for a waiting call
     * AT+CHLD=1 - Releases active calls and accepts the other (held or
     *             waiting) call
     * AT+CHLD=2 - Places active calls on hold and accepts the other (held
     *             or waiting) call
     *
     * The following cases are NOT supported yet:
     * AT+CHLD=1<idx>, AT+CHLD=2<idx>, AT+CHLD=3, AT+CHLD=4
     * Please see 4.33.2 in Bluetooth hands-free profile 1.6 for more
     * information.
     */
    char chld = atCommandValues[0][0];
    bool valid = true;
    if (atCommandValues[0].Length() > 1) {
      NS_WARNING("No index should be included in command [AT+CHLD]");
      valid = false;
    } else if (chld == '3' || chld == '4') {
      NS_WARNING("The value of command [AT+CHLD] is not supported");
      valid = false;
    } else if (chld == '0') {
      // We need to rename these dialer commands for better readability
      // and expandability.
      // See bug 884190 for more information.
      NotifyDialer(NS_LITERAL_STRING("CHLD=0"));
    } else if (chld == '1') {
      NotifyDialer(NS_LITERAL_STRING("CHUP+ATA"));
    } else if (chld == '2') {
      NotifyDialer(NS_LITERAL_STRING("CHLD+ATA"));
    } else {
      NS_WARNING("Wrong value of command [AT+CHLD]");
      valid = false;
    }

    if (!valid) {
      SendLine("ERROR");
      return;
    }
  } else if (msg.Find("AT+VGS=") != -1) {
    // Adjust volume by headset
    mReceiveVgsFlag = true;
    ParseAtCommand(msg, 7, atCommandValues);

    if (atCommandValues.IsEmpty()) {
      NS_WARNING("Could't get the value of command [AT+VGS=]");
      goto respond_with_ok;
    }

    nsresult rv;
    int newVgs = atCommandValues[0].ToInteger(&rv);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to extract volume value from bluetooth headset!");
      goto respond_with_ok;
    }

    if (newVgs == mCurrentVgs) {
      goto respond_with_ok;
    }

    NS_ASSERTION(newVgs >= 0 && newVgs <= 15, "Received invalid VGS value");

    nsString data;
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    data.AppendInt(newVgs);
    os->NotifyObservers(nullptr, "bluetooth-volume-change", data.get());
  } else if ((msg.Find("AT+BLDN") != -1) || (msg.Find("ATD>") != -1)) {
    // Dialer app of FFOS v1 does not have plan to support Memory Dailing.
    // However, in order to pass Bluetooth HFP certification, we still have to
    // make a call when we receive AT command 'ATD>n'.
    mDialingRequestProcessed = false;

    if (msg.Find("AT+BLDN") != -1) {
      NotifyDialer(NS_LITERAL_STRING("BLDN"));
    } else {
      NotifyDialer(NS_ConvertUTF8toUTF16(msg));
    }

    MessageLoop::current()->
      PostDelayedTask(FROM_HERE, new RespondToBLDNTask(),
                      sWaitingForDialingInterval);

    // Don't send response 'OK' here because we'll respond later in either
    // RespondToBLDNTask or HandleCallStateChanged()
    return;
  } else if (msg.Find("ATA") != -1) {
    NotifyDialer(NS_LITERAL_STRING("ATA"));
  } else if (msg.Find("AT+CHUP") != -1) {
    NotifyDialer(NS_LITERAL_STRING("CHUP"));
  } else if (msg.Find("AT+CLCC") != -1) {
    SendCommand("+CLCC: ");
  } else if (msg.Find("ATD") != -1) {
    nsAutoCString message(msg), newMsg;
    int end = message.FindChar(';');
    if (end < 0) {
      NS_WARNING("Could't get the value of command [ATD]");
      goto respond_with_ok;
    }

    newMsg += nsDependentCSubstring(message, 0, end);
    NotifyDialer(NS_ConvertUTF8toUTF16(newMsg));
  } else if (msg.Find("AT+CLIP=") != -1) {
    ParseAtCommand(msg, 8, atCommandValues);

    if (atCommandValues.IsEmpty()) {
      NS_WARNING("Could't get the value of command [AT+CLIP=]");
      goto respond_with_ok;
    }

    mCLIP = atCommandValues[0].EqualsLiteral("1");
  } else if (msg.Find("AT+CCWA=") != -1) {
    ParseAtCommand(msg, 8, atCommandValues);

    if (atCommandValues.IsEmpty()) {
      NS_WARNING("Could't get the value of command [AT+CCWA=]");
      goto respond_with_ok;
    }

    mCCWA = atCommandValues[0].EqualsLiteral("1");
  } else if (msg.Find("AT+CKPD") != -1) {
    if (!sStopSendingRingFlag) {
      // Bluetooth HSP spec 4.2.2
      // There is an incoming call, notify Dialer to pick up the phone call
      // and SCO will be established after we get the CallStateChanged event
      // indicating the call is answered successfully.
      NotifyDialer(NS_LITERAL_STRING("ATA"));
    } else {
      if (!IsScoConnected()) {
        // Bluetooth HSP spec 4.3
        // If there's no SCO, set up a SCO link.
        ConnectSco();
      } else if (!mFirstCKPD) {
        // Bluetooth HSP spec 4.5
        // There are two ways to release SCO: sending CHUP to dialer or closing
        // SCO socket directly. We notify dialer only if there is at least one
        // active call.
        if (mCurrentCallArray.Length() > 1) {
          NotifyDialer(NS_LITERAL_STRING("CHUP"));
        } else {
          DisconnectSco();
        }
      } else {
        // Three conditions have to be matched to come in here:
        // (1) Not sending RING indicator
        // (2) A SCO link exists
        // (3) This is the very first AT+CKPD=200 of this session
        // It is the case of Figure 4.3, Bluetooth HSP spec. Do nothing.
        NS_WARNING("AT+CKPD=200: Do nothing");
      }
    }

    mFirstCKPD = false;
  } else if (msg.Find("AT+CNUM") != -1) {
    if (!mMsisdn.IsEmpty()) {
      nsAutoCString message("+CNUM: ,\"");
      message.Append(NS_ConvertUTF16toUTF8(mMsisdn).get());
      message.AppendLiteral("\",");
      message.AppendInt(TOA_UNKNOWN);
      message.AppendLiteral(",,4");
      SendLine(message.get());
    }
  } else if (msg.Find("AT+BIA=") != -1) {
    ParseAtCommand(msg, 7, atCommandValues);

    for (uint8_t i = 0; i < atCommandValues.Length(); i++) {
      CINDType indicatorType = (CINDType) (i + 1);
      if (indicatorType >= ArrayLength(sCINDItems)) {
        // Ignore excess parameters at the end
        break;
      }

      if (!IsMandatoryIndicator(indicatorType)) {
        /**
         * Accept only following indicator states:
         * - "1": activate
         * - "0": deactivate
         * - "" : maintain current state
         * Otherwise we regard the command incorrectly formatted.
         */
        if (atCommandValues[i].EqualsLiteral("1")) {
          sCINDItems[indicatorType].activated = 1;
        } else if (atCommandValues[i].EqualsLiteral("0")) {
          sCINDItems[indicatorType].activated = 0;
        } else if (!atCommandValues[i].EqualsLiteral("")) {
          SendLine("ERROR");
          return;
        }
      } else {
        // Ignore requests to activate/deactivate mandatory indicators
      }
    }
  } else {
    nsCString warningMsg;
    warningMsg.Append(NS_LITERAL_CSTRING("Unsupported AT command: "));
    warningMsg.Append(msg);
    warningMsg.Append(NS_LITERAL_CSTRING(", reply with ERROR"));
    NS_WARNING(warningMsg.get());

    SendLine("ERROR");
    return;
  }

respond_with_ok:
  // We always respond to remote device with "OK" in general cases.
  SendLine("OK");
}

void
BluetoothHfpManager::Connect(const nsAString& aDeviceAddress,
                             const bool aIsHandsfree,
                             BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  if (!bs || gInShutdown) {
    DispatchBluetoothReply(aRunnable, BluetoothValue(),
                           NS_LITERAL_STRING(ERR_NO_AVAILABLE_RESOURCE));
    return;
  }

  if (mSocket) {
    DispatchBluetoothReply(aRunnable, BluetoothValue(),
                           NS_LITERAL_STRING(ERR_REACHED_CONNECTION_LIMIT));
    return;
  }

  mNeedsUpdatingSdpRecords = true;
  mIsHandsfree = aIsHandsfree;

  nsString uuid;
  if (aIsHandsfree) {
    BluetoothUuidHelper::GetString(BluetoothServiceClass::HANDSFREE, uuid);
  } else {
    BluetoothUuidHelper::GetString(BluetoothServiceClass::HEADSET, uuid);
  }

  if (NS_FAILED(bs->GetServiceChannel(aDeviceAddress, uuid, this))) {
    DispatchBluetoothReply(aRunnable, BluetoothValue(),
                           NS_LITERAL_STRING(ERR_SERVICE_CHANNEL_NOT_FOUND));
    return;
  }

  // Stop listening because currently we only support one connection at a time.
  if (mHandsfreeSocket) {
    mHandsfreeSocket->Disconnect();
    mHandsfreeSocket = nullptr;
  }

  if (mHeadsetSocket) {
    mHeadsetSocket->Disconnect();
    mHeadsetSocket = nullptr;
  }

  MOZ_ASSERT(!mRunnable);

  mRunnable = aRunnable;
  mSocket =
    new BluetoothSocket(this, BluetoothSocketType::RFCOMM, true, true);
}

bool
BluetoothHfpManager::Listen()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (gInShutdown) {
    NS_WARNING("Listen called while in shutdown!");
    return false;
  }

  if (mSocket) {
    NS_WARNING("mSocket exists. Failed to listen.");
    return false;
  }

  if (!mHandsfreeSocket) {
    mHandsfreeSocket =
      new BluetoothSocket(this, BluetoothSocketType::RFCOMM, true, true);

    if (!mHandsfreeSocket->Listen(
          BluetoothReservedChannels::CHANNEL_HANDSFREE_AG)) {
      NS_WARNING("[HFP] Can't listen on RFCOMM socket!");
      mHandsfreeSocket = nullptr;
      return false;
    }
  }

  if (!mHeadsetSocket) {
    mHeadsetSocket =
      new BluetoothSocket(this, BluetoothSocketType::RFCOMM, true, true);

    if (!mHeadsetSocket->Listen(
          BluetoothReservedChannels::CHANNEL_HEADSET_AG)) {
      NS_WARNING("[HSP] Can't listen on RFCOMM socket!");
      mHandsfreeSocket->Disconnect();
      mHandsfreeSocket = nullptr;
      mHeadsetSocket = nullptr;
      return false;
    }
  }

  return true;
}

void
BluetoothHfpManager::Disconnect()
{
  if (mSocket) {
    mSocket->Disconnect();
    mSocket = nullptr;
  }

  BluetoothA2dpManager* a2dp = BluetoothA2dpManager::Get();
  NS_ENSURE_TRUE_VOID(a2dp);
  a2dp->Disconnect();
}

bool
BluetoothHfpManager::SendLine(const char* aMessage)
{
  MOZ_ASSERT(mSocket);

  nsAutoCString msg;

  msg.AppendLiteral(kHfpCrlf);
  msg.Append(aMessage);
  msg.AppendLiteral(kHfpCrlf);

  return mSocket->SendSocketData(msg);
}

bool
BluetoothHfpManager::SendCommand(const char* aCommand, uint32_t aValue)
{
  if (!IsConnected()) {
    NS_WARNING("Trying to SendCommand() without a SLC");
    return false;
  }

  nsAutoCString message;
  message += aCommand;

  if (!strcmp(aCommand, "+CIEV: ")) {
    if (!mCMER || !sCINDItems[aValue].activated) {
      // Indicator status update is disabled
      return true;
    }

    if ((aValue < 1) || (aValue > ArrayLength(sCINDItems) - 1)) {
      NS_WARNING("unexpected CINDType for CIEV command");
      return false;
    }

    message.AppendInt(aValue);
    message.AppendLiteral(",");
    message.AppendInt(sCINDItems[aValue].value);
  } else if (!strcmp(aCommand, "+CIND: ")) {
    if (!aValue) {
      // Query for range
      for (uint8_t i = 1; i < ArrayLength(sCINDItems); i++) {
        message.AppendLiteral("(\"");
        message.Append(sCINDItems[i].name);
        message.AppendLiteral("\",(");
        message.Append(sCINDItems[i].range);
        message.AppendLiteral(")");
        if (i == (ArrayLength(sCINDItems) - 1)) {
          message.AppendLiteral(")");
          break;
        }
        message.AppendLiteral("),");
      }
    } else {
      // Query for value
      for (uint8_t i = 1; i < ArrayLength(sCINDItems); i++) {
        message.AppendInt(sCINDItems[i].value);
        if (i == (ArrayLength(sCINDItems) - 1)) {
          break;
        }
        message.AppendLiteral(",");
      }
    }
  } else if (!strcmp(aCommand, "+CLCC: ")) {
    bool rv = true;
    uint32_t callNumbers = mCurrentCallArray.Length();
    for (uint32_t i = 1; i < callNumbers; i++) {
      Call& call = mCurrentCallArray[i];
      if (call.mState == nsITelephonyProvider::CALL_STATE_DISCONNECTED) {
        continue;
      }

      message.AssignLiteral("+CLCC: ");
      message.AppendInt(i);
      message.AppendLiteral(",");
      message.AppendInt(call.mDirection);
      message.AppendLiteral(",");

      switch (call.mState) {
        case nsITelephonyProvider::CALL_STATE_CONNECTED:
          message.AppendInt(0);
          break;
        case nsITelephonyProvider::CALL_STATE_HELD:
          message.AppendInt(1);
          break;
        case nsITelephonyProvider::CALL_STATE_DIALING:
          message.AppendInt(2);
          break;
        case nsITelephonyProvider::CALL_STATE_ALERTING:
          message.AppendInt(3);
          break;
        case nsITelephonyProvider::CALL_STATE_INCOMING:
          if (!FindFirstCall(nsITelephonyProvider::CALL_STATE_CONNECTED)) {
            message.AppendInt(4);
          } else {
            message.AppendInt(5);
          }
          break;
        default:
          NS_WARNING("Not handling call status for CLCC");
          break;
      }
      message.AppendLiteral(",0,0,\"");
      message.Append(NS_ConvertUTF16toUTF8(call.mNumber));
      message.AppendLiteral("\",");
      message.AppendInt(call.mType);

      rv &= SendLine(message.get());
    }
    return rv;
  } else {
    message.AppendInt(aValue);
  }

  return SendLine(message.get());
}

void
BluetoothHfpManager::UpdateCIND(uint8_t aType, uint8_t aValue, bool aSend)
{
  if (sCINDItems[aType].value != aValue) {
    sCINDItems[aType].value = aValue;
    if (aSend) {
      SendCommand("+CIEV: ", aType);
    }
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

void
BluetoothHfpManager::HandleCallStateChanged(uint32_t aCallIndex,
                                            uint16_t aCallState,
                                            const nsAString& aNumber,
                                            const bool aIsOutgoing,
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

  uint16_t prevCallState = mCurrentCallArray[aCallIndex].mState;
  mCurrentCallArray[aCallIndex].mState = aCallState;
  mCurrentCallArray[aCallIndex].mDirection = !aIsOutgoing;

  // Same logic as implementation in ril_worker.js
  if (aNumber.Length() && aNumber[0] == '+') {
    mCurrentCallArray[aCallIndex].mType = TOA_INTERNATIONAL;
  }
  mCurrentCallArray[aCallIndex].mNumber = aNumber;

  nsRefPtr<nsRunnable> sendRingTask;
  nsString address;

  switch (aCallState) {
    case nsITelephonyProvider::CALL_STATE_HELD:
      if (!FindFirstCall(nsITelephonyProvider::CALL_STATE_CONNECTED)) {
        sCINDItems[CINDType::CALLHELD].value = CallHeldState::ONHOLD_NOACTIVE;
      } else {
        sCINDItems[CINDType::CALLHELD].value = CallHeldState::ONHOLD_ACTIVE;
      }
      SendCommand("+CIEV: ", CINDType::CALLHELD);
      break;
    case nsITelephonyProvider::CALL_STATE_INCOMING:
      if (FindFirstCall(nsITelephonyProvider::CALL_STATE_CONNECTED)) {
        if (mCCWA) {
          nsAutoCString ccwaMsg("+CCWA: \"");
          ccwaMsg.Append(NS_ConvertUTF16toUTF8(aNumber));
          ccwaMsg.AppendLiteral("\",");
          ccwaMsg.AppendInt(mCurrentCallArray[aCallIndex].mType);
          SendLine(ccwaMsg.get());
        }
        UpdateCIND(CINDType::CALLSETUP, CallSetupState::INCOMING, aSend);
      } else {
        // Start sending RING indicator to HF
        sStopSendingRingFlag = false;
        UpdateCIND(CINDType::CALLSETUP, CallSetupState::INCOMING, aSend);

        if (mBSIR) {
          // Setup audio connection for in-band ring tone
          ConnectSco();
        }

        nsAutoString number(aNumber);
        if (!mCLIP) {
          number.AssignLiteral("");
        }

        MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          new SendRingIndicatorTask(number,
                                    mCurrentCallArray[aCallIndex].mType),
          sRingInterval);
      }
      break;
    case nsITelephonyProvider::CALL_STATE_DIALING:
      if (!mDialingRequestProcessed) {
        SendLine("OK");
        mDialingRequestProcessed = true;
      }

      UpdateCIND(CINDType::CALLSETUP, CallSetupState::OUTGOING, aSend);
      ConnectSco();
      break;
    case nsITelephonyProvider::CALL_STATE_ALERTING:
      UpdateCIND(CINDType::CALLSETUP, CallSetupState::OUTGOING_ALERTING, aSend);

      // If there's an ongoing call when the headset is just connected, we have
      // to open a sco socket here.
      ConnectSco();
      break;
    case nsITelephonyProvider::CALL_STATE_CONNECTED:
      switch (prevCallState) {
        case nsITelephonyProvider::CALL_STATE_INCOMING:
        case nsITelephonyProvider::CALL_STATE_DISCONNECTED:
          // Incoming call, no break
          sStopSendingRingFlag = true;
          ConnectSco();
        case nsITelephonyProvider::CALL_STATE_ALERTING:
          // Outgoing call
          UpdateCIND(CINDType::CALL, CallState::IN_PROGRESS, aSend);
          UpdateCIND(CINDType::CALLSETUP, CallSetupState::NO_CALLSETUP, aSend);
          break;
        default:
          NS_WARNING("Not handling state changed");
      }

      // = Handle callheld separately =
      // Besides checking if there is still held calls, another thing we
      // need to consider is the state change when receiving AT+CHLD=2.
      // Assume that there is one active call(c1) and one call on hold(c2).
      // We got AT+CHLD=2, which swaps active/held position. The first
      // action would be c2 -> ACTIVE, then c1 -> HELD. When we get the
      // CallStateChanged event of c2 becoming ACTIVE, we enter here.
      // However we can't send callheld=0 at this time because we should
      // see c2 -> ACTIVE + c1 -> HELD as one operation. That's the reason
      // why I added the GetNumberOfCalls() condition check.
      if (GetNumberOfCalls(nsITelephonyProvider::CALL_STATE_CONNECTED) == 1) {
        if (FindFirstCall(nsITelephonyProvider::CALL_STATE_HELD)) {
          UpdateCIND(CINDType::CALLHELD, CallHeldState::ONHOLD_ACTIVE, aSend);
        } else if (prevCallState == nsITelephonyProvider::CALL_STATE_HELD) {
          UpdateCIND(CINDType::CALLHELD, CallHeldState::NO_CALLHELD, aSend);
        }
      }
      break;
    case nsITelephonyProvider::CALL_STATE_DISCONNECTED:
      switch (prevCallState) {
        case nsITelephonyProvider::CALL_STATE_INCOMING:
        case nsITelephonyProvider::CALL_STATE_BUSY:
          // Incoming call, no break
          sStopSendingRingFlag = true;
        case nsITelephonyProvider::CALL_STATE_DIALING:
        case nsITelephonyProvider::CALL_STATE_ALERTING:
          // Outgoing call
          UpdateCIND(CINDType::CALLSETUP, CallSetupState::NO_CALLSETUP, aSend);
          break;
        case nsITelephonyProvider::CALL_STATE_CONNECTED:
          // No call is ongoing
          if (sCINDItems[CINDType::CALLHELD].value ==
              CallHeldState::NO_CALLHELD) {
            UpdateCIND(CINDType::CALL, CallState::NO_CALL, aSend);
          }
          break;
        default:
          NS_WARNING("Not handling state changed");
      }

      // Handle held calls separately
      if (!FindFirstCall(nsITelephonyProvider::CALL_STATE_HELD)) {
        UpdateCIND(CINDType::CALLHELD, CallHeldState::NO_CALLHELD, aSend);
      } else if (!FindFirstCall(nsITelephonyProvider::CALL_STATE_CONNECTED)) {
        UpdateCIND(CINDType::CALLHELD, CallHeldState::ONHOLD_NOACTIVE, aSend);
      } else {
        UpdateCIND(CINDType::CALLHELD, CallHeldState::ONHOLD_ACTIVE, aSend);
      }

      // -1 is necessary because call 0 is an invalid (padding) call object.
      if (mCurrentCallArray.Length() - 1 ==
          GetNumberOfCalls(nsITelephonyProvider::CALL_STATE_DISCONNECTED)) {
        // In order to let user hear busy tone via connected Bluetooth headset,
        // we postpone the timing of dropping SCO.
        if (prevCallState != nsITelephonyProvider::CALL_STATE_BUSY) {
          DisconnectSco();
        } else {
          // Close Sco later since Dialer is still playing busy tone via HF.
          MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                                  new CloseScoTask(),
                                                  sBusyToneInterval);
        }

        ResetCallArray();
      }
      break;
    default:
      NS_WARNING("Not handling state changed");
      break;
  }
}

void
BluetoothHfpManager::OnConnectSuccess(BluetoothSocket* aSocket)
{
  MOZ_ASSERT(aSocket);

  // Success to create a SCO socket
  if (aSocket == mScoSocket) {
    OnScoConnectSuccess();
    return;
  }

  /**
   * If the created connection is an inbound connection, close another server
   * socket because currently only one SLC is allowed. After that, we need to
   * make sure that both server socket would be nulled out. As for outbound
   * connections, we do nothing since sockets have been already handled in
   * function Connect().
   */
  if (aSocket == mHandsfreeSocket) {
    MOZ_ASSERT(!mSocket);
    mHandsfreeSocket.swap(mSocket);

    mHeadsetSocket->Disconnect();
    mHeadsetSocket = nullptr;
  } else if (aSocket == mHeadsetSocket) {
    MOZ_ASSERT(!mSocket);
    mHeadsetSocket.swap(mSocket);

    mHandsfreeSocket->Disconnect();
    mHandsfreeSocket = nullptr;
  }

  nsCOMPtr<nsITelephonyProvider> provider =
    do_GetService(NS_RILCONTENTHELPER_CONTRACTID);
  NS_ENSURE_TRUE_VOID(provider);
  provider->EnumerateCalls(mListener->GetListener());

  // For active connection request, we need to reply the DOMRequest
  if (mRunnable) {
    BluetoothValue v = true;
    nsString errorStr;
    DispatchBluetoothReply(mRunnable, v, errorStr);

    mRunnable = nullptr;
  }

  mFirstCKPD = true;

  // Cache device path for NotifySettings() since we can't get socket address
  // when a headset disconnect with us
  mSocket->GetAddress(mDeviceAddress);
  NotifyStatusChanged(NS_LITERAL_STRING("bluetooth-hfp-status-changed"));

  ListenSco();

  BluetoothA2dpManager* a2dp = BluetoothA2dpManager::Get();
  NS_ENSURE_TRUE_VOID(a2dp);
  a2dp->Connect(mDeviceAddress);
}

void
BluetoothHfpManager::OnConnectError(BluetoothSocket* aSocket)
{
  // Failed to create a SCO socket
  if (aSocket == mScoSocket) {
    OnScoConnectError();
    return;
  }

  // For active connection request, we need to reply the DOMRequest
  if (mRunnable) {
    NS_NAMED_LITERAL_STRING(replyError,
                            "Failed to connect with a bluetooth headset!");
    DispatchBluetoothReply(mRunnable, BluetoothValue(), replyError);

    mRunnable = nullptr;
  }

  mSocket = nullptr;
  mHandsfreeSocket = nullptr;
  mHeadsetSocket = nullptr;

  // If connecting for some reason didn't work, restart listening
  Listen();
}

void
BluetoothHfpManager::OnDisconnect(BluetoothSocket* aSocket)
{
  MOZ_ASSERT(aSocket);

  if (aSocket == mScoSocket) {
    // SCO socket is closed
    OnScoDisconnect();
    return;
  }

  if (aSocket != mSocket) {
    // Do nothing when a listening server socket is closed.
    return;
  }

  mSocket = nullptr;
  DisconnectSco();

  Listen();
  NotifyStatusChanged(NS_LITERAL_STRING("bluetooth-hfp-status-changed"));
  Reset();
}

void
BluetoothHfpManager::OnUpdateSdpRecords(const nsAString& aDeviceAddress)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aDeviceAddress.IsEmpty());
  MOZ_ASSERT(mRunnable);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  nsString uuid;
  if (mIsHandsfree) {
    BluetoothUuidHelper::GetString(BluetoothServiceClass::HANDSFREE, uuid);
  } else {
    BluetoothUuidHelper::GetString(BluetoothServiceClass::HEADSET, uuid);
  }

  // Since we have updated SDP records of the target device, we should
  // try to get the channel of target service again.
  if (NS_FAILED(bs->GetServiceChannel(aDeviceAddress, uuid, this))) {
    DispatchBluetoothReply(mRunnable, BluetoothValue(),
                           NS_LITERAL_STRING(ERR_SERVICE_CHANNEL_NOT_FOUND));
    mRunnable = nullptr;
    mSocket = nullptr;
    Listen();
  }
}

void
BluetoothHfpManager::OnGetServiceChannel(const nsAString& aDeviceAddress,
                                         const nsAString& aServiceUuid,
                                         int aChannel)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aDeviceAddress.IsEmpty());
  MOZ_ASSERT(mRunnable);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  BluetoothValue v;

  if (aChannel < 0) {
    if (mNeedsUpdatingSdpRecords) {
      mNeedsUpdatingSdpRecords = false;
      bs->UpdateSdpRecords(aDeviceAddress, this);
    } else {
      DispatchBluetoothReply(mRunnable, v,
                             NS_LITERAL_STRING(ERR_SERVICE_CHANNEL_NOT_FOUND));
      mRunnable = nullptr;
      mSocket = nullptr;
      Listen();
    }

    return;
  }

  if (!mSocket->Connect(NS_ConvertUTF16toUTF8(aDeviceAddress), aChannel)) {
    DispatchBluetoothReply(mRunnable, v,
                           NS_LITERAL_STRING("SocketConnectionError"));
    mRunnable = nullptr;
    mSocket = nullptr;
    Listen();
  }
}

void
BluetoothHfpManager::OnScoConnectSuccess()
{
  // For active connection request, we need to reply the DOMRequest
  if (mScoRunnable) {
    DispatchBluetoothReply(mScoRunnable,
                           BluetoothValue(true), EmptyString());
    mScoRunnable = nullptr;
  }

  NotifyAudioManager(true);
  NotifyStatusChanged(NS_LITERAL_STRING("bluetooth-sco-status-changed"));

  mScoSocketStatus = mScoSocket->GetConnectionStatus();
}

void
BluetoothHfpManager::OnScoConnectError()
{
  if (mScoRunnable) {
    NS_NAMED_LITERAL_STRING(replyError, "Failed to create SCO socket!");
    DispatchBluetoothReply(mScoRunnable, BluetoothValue(), replyError);

    mScoRunnable = nullptr;
  }

  ListenSco();
}

void
BluetoothHfpManager::OnScoDisconnect()
{
  if (mScoSocketStatus == SocketConnectionStatus::SOCKET_CONNECTED) {
    ListenSco();
    NotifyAudioManager(false);
    NotifyStatusChanged(NS_LITERAL_STRING("bluetooth-sco-status-changed"));
  }
}

bool
BluetoothHfpManager::IsConnected()
{
  if (mSocket) {
    return mSocket->GetConnectionStatus() ==
           SocketConnectionStatus::SOCKET_CONNECTED;
  }

  return false;
}

void
BluetoothHfpManager::GetAddress(nsAString& aDeviceAddress)
{
  return mSocket->GetAddress(aDeviceAddress);
}

bool
BluetoothHfpManager::ConnectSco(BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (gInShutdown) {
    NS_WARNING("ConnecteSco called while in shutdown!");
    return false;
  }

  if (!IsConnected()) {
    NS_WARNING("BluetoothHfpManager is not connected");
    return false;
  }

  SocketConnectionStatus status = mScoSocket->GetConnectionStatus();
  if (status == SocketConnectionStatus::SOCKET_CONNECTED ||
      status == SocketConnectionStatus::SOCKET_CONNECTING ||
      (mScoRunnable && (mScoRunnable != aRunnable))) {
    NS_WARNING("SCO connection exists or is being established");
    return false;
  }

  mScoSocket->Disconnect();

  mScoRunnable = aRunnable;

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, false);
  nsresult rv = bs->GetScoSocket(mDeviceAddress, true, false, mScoSocket);

  mScoSocketStatus = mSocket->GetConnectionStatus();
  return NS_SUCCEEDED(rv);
}

bool
BluetoothHfpManager::DisconnectSco()
{
  if (!IsConnected()) {
    NS_WARNING("BluetoothHfpManager is not connected");
    return false;
  }

  SocketConnectionStatus status = mScoSocket->GetConnectionStatus();
  if (status != SOCKET_CONNECTED && status != SOCKET_CONNECTING) {
    NS_WARNING("No SCO exists");
    return false;
  }

  mScoSocket->Disconnect();
  return true;
}

bool
BluetoothHfpManager::ListenSco()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (gInShutdown) {
    NS_WARNING("ListenSco called while in shutdown!");
    return false;
  }

  if (mScoSocket->GetConnectionStatus() ==
      SocketConnectionStatus::SOCKET_LISTENING) {
    NS_WARNING("SCO socket has been already listening");
    return false;
  }

  mScoSocket->Disconnect();

  if (!mScoSocket->Listen(-1)) {
    NS_WARNING("Can't listen on SCO socket!");
    return false;
  }

  mScoSocketStatus = mScoSocket->GetConnectionStatus();
  return true;
}

bool
BluetoothHfpManager::IsScoConnected()
{
  if (mScoSocket) {
    return mScoSocket->GetConnectionStatus() ==
           SocketConnectionStatus::SOCKET_CONNECTED;
  }
  return false;
}

NS_IMPL_ISUPPORTS1(BluetoothHfpManager, nsIObserver)
