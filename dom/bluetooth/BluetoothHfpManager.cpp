/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BluetoothHfpManager.h"

#include "BluetoothReplyRunnable.h"
#include "BluetoothScoManager.h"
#include "BluetoothService.h"
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
#include "nsIRadioInterfaceLayer.h"
#include "nsRadioInterfaceLayer.h"

#define AUDIO_VOLUME_BT_SCO "audio.volume.bt_sco"
#define MOZSETTINGS_CHANGED_ID "mozsettings-changed"
#define MOBILE_CONNECTION_ICCINFO_CHANGED "mobile-connection-iccinfo-changed"
#define MOBILE_CONNECTION_VOICE_CHANGED "mobile-connection-voice-changed"

/**
 * These constants are used in result code such as +CLIP and +CCWA. The value
 * of these constants is the same as TOA_INTERNATIONAL/TOA_UNKNOWN defined in
 * ril_consts.js
 */
#define TOA_UNKNOWN 0x81
#define TOA_INTERNATIONAL 0x91

using namespace mozilla;
using namespace mozilla::ipc;
USING_BLUETOOTH_NAMESPACE

namespace {
  StaticRefPtr<BluetoothHfpManager> gBluetoothHfpManager;
  StaticRefPtr<BluetoothHfpManagerObserver> sHfpObserver;
  bool gInShutdown = false;
  static bool sStopSendingRingFlag = true;

  static int sRingInterval = 3000; //unit: ms
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
  {"battchg", "0-5", 5},
  {"call", "0,1", CallState::NO_CALL},
  {"callheld", "0-2", CallHeldState::NO_CALLHELD},
  {"callsetup", "0-3", CallSetupState::NO_CALLSETUP},
  {"service", "0,1", 0},
  {"signal", "0-5", 0},
  {"roam", "0,1", 0}
};

class mozilla::dom::bluetooth::Call {
  public:
    Call(uint16_t aState = nsIRadioInterfaceLayer::CALL_STATE_DISCONNECTED,
         bool aDirection = false,
         const nsAString& aNumber = NS_LITERAL_STRING(""),
         int aType = TOA_UNKNOWN)
      : mState(aState), mDirection(aDirection), mNumber(aNumber), mType(aType)
    {
    }

    uint16_t mState;
    bool mDirection; // true: incoming call; false: outgoing call
    nsString mNumber;
    int mType;
};

class mozilla::dom::bluetooth::BluetoothHfpManagerObserver : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  BluetoothHfpManagerObserver()
  {
  }

  bool Init()
  {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    MOZ_ASSERT(obs);
    if (NS_FAILED(obs->AddObserver(this, MOZSETTINGS_CHANGED_ID, false))) {
      NS_WARNING("Failed to add settings change observer!");
      return false;
    }

    if (NS_FAILED(obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false))) {
      NS_WARNING("Failed to add shutdown observer!");
      return false;
    }

    if (NS_FAILED(obs->AddObserver(this, MOBILE_CONNECTION_ICCINFO_CHANGED, false))) {
      NS_WARNING("Failed to add mobile connection iccinfo change observer!");
      return false;
    }

    if (NS_FAILED(obs->AddObserver(this, MOBILE_CONNECTION_VOICE_CHANGED, false))) {
      NS_WARNING("Failed to add mobile connection voice change observer!");
      return false;
    }

    return true;
  }

  bool Shutdown()
  {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (!obs ||
        NS_FAILED(obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) ||
        NS_FAILED(obs->RemoveObserver(this, MOZSETTINGS_CHANGED_ID)) ||
        NS_FAILED(obs->RemoveObserver(this, MOBILE_CONNECTION_ICCINFO_CHANGED)) ||
        NS_FAILED(obs->RemoveObserver(this, MOBILE_CONNECTION_VOICE_CHANGED))) {
      NS_WARNING("Can't unregister observers, or already unregistered!");
      return false;
    }
    return true;
  }

  ~BluetoothHfpManagerObserver()
  {
    Shutdown();
  }
};

class BluetoothHfpManager::GetVolumeTask : public nsISettingsServiceCallback
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD
  Handle(const nsAString& aName, const jsval& aResult)
  {
    MOZ_ASSERT(NS_IsMainThread());

    JSContext *cx = nsContentUtils::GetCurrentJSContext();
    NS_ENSURE_TRUE(cx, NS_OK);

    if (!aResult.isNumber()) {
      NS_WARNING("'" AUDIO_VOLUME_BT_SCO "' is not a number!");
      return NS_OK;
    }

    BluetoothHfpManager* hfp = BluetoothHfpManager::Get();
    hfp->mCurrentVgs = aResult.toNumber();

    return NS_OK;
  }

  NS_IMETHOD
  HandleError(const nsAString& aName)
  {
    NS_WARNING("Unable to get value for '" AUDIO_VOLUME_BT_SCO "'");
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS1(BluetoothHfpManager::GetVolumeTask,
                   nsISettingsServiceCallback);

NS_IMETHODIMP
BluetoothHfpManagerObserver::Observe(nsISupports* aSubject,
                                     const char* aTopic,
                                     const PRUnichar* aData)
{
  MOZ_ASSERT(gBluetoothHfpManager);

  if (!strcmp(aTopic, MOZSETTINGS_CHANGED_ID)) {
    return gBluetoothHfpManager->HandleVolumeChanged(nsDependentString(aData));
  } else if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    return gBluetoothHfpManager->HandleShutdown();
  } else if (!strcmp(aTopic, MOBILE_CONNECTION_ICCINFO_CHANGED)) {
    return gBluetoothHfpManager->HandleIccInfoChanged();
  } else if (!strcmp(aTopic, MOBILE_CONNECTION_VOICE_CHANGED)) {
    return gBluetoothHfpManager->HandleVoiceConnectionChanged();
  }

  MOZ_ASSERT(false, "BluetoothHfpManager got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMPL_ISUPPORTS1(BluetoothHfpManagerObserver, nsIObserver)

class SendRingIndicatorTask : public Task
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

    NS_ENSURE_FALSE_VOID(sStopSendingRingFlag);

    if (!gBluetoothHfpManager) {
      NS_WARNING("BluetoothHfpManager no longer exists, cannot send ring!");
      return;
    }

    const char* kHfpCrlf = "\xd\xa";
    nsAutoCString ringMsg(kHfpCrlf);
    ringMsg += "RING";
    ringMsg += kHfpCrlf;
    gBluetoothHfpManager->SendSocketData(ringMsg);

    if (!mNumber.IsEmpty()) {
      nsAutoCString clipMsg(kHfpCrlf);
      clipMsg += "+CLIP: \"";
      clipMsg += NS_ConvertUTF16toUTF8(mNumber).get();
      clipMsg += "\",";
      clipMsg.AppendInt(mType);
      clipMsg += kHfpCrlf;
      gBluetoothHfpManager->SendSocketData(clipMsg);
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

void
OpenScoSocket(const nsAString& aDeviceAddress)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothScoManager* sco = BluetoothScoManager::Get();
  if (!sco) {
    NS_WARNING("BluetoothScoManager is not available!");
    return;
  }

  if (!sco->Connect(aDeviceAddress)) {
    NS_WARNING("Failed to create a sco socket!");
  }
}

void
CloseScoSocket()
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothScoManager* sco = BluetoothScoManager::Get();
  if (!sco) {
    NS_WARNING("BluetoothScoManager is not available!");
    return;
  }
  sco->Disconnect();
}

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

BluetoothHfpManager::BluetoothHfpManager()
{
  Reset();
}

void
BluetoothHfpManager::ResetCallArray()
{
  mCurrentCallIndex = 0;
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

  mCLIP = false;
  mCMEE = false;
  mCMER = false;
  mReceiveVgsFlag = false;

  ResetCallArray();
}

bool
BluetoothHfpManager::Init()
{
  MOZ_ASSERT(NS_IsMainThread());

  mSocketStatus = GetConnectionStatus();

  sHfpObserver = new BluetoothHfpManagerObserver();
  if (!sHfpObserver->Init()) {
    NS_WARNING("Cannot set up Hfp Observers!");
  }

  mListener = new BluetoothRilListener();
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
  rv = settingsLock->Get(AUDIO_VOLUME_BT_SCO, callback);
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

BluetoothHfpManager::~BluetoothHfpManager()
{
  Cleanup();
}

void
BluetoothHfpManager::Cleanup()
{
  if (!mListener->StopListening()) {
    NS_WARNING("Failed to stop listening RIL");
  }
  mListener = nullptr;

  sHfpObserver->Shutdown();
  sHfpObserver = nullptr;
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
  nsRefPtr<BluetoothHfpManager> manager = new BluetoothHfpManager();
  NS_ENSURE_TRUE(manager, nullptr);

  if (!manager->Init()) {
    return nullptr;
  }

  gBluetoothHfpManager = manager;
  return gBluetoothHfpManager;
}

void
BluetoothHfpManager::NotifySettings()
{
  nsString type, name;
  BluetoothValue v;
  InfallibleTArray<BluetoothNamedValue> parameters;
  type.AssignLiteral("bluetooth-hfp-status-changed");

  name.AssignLiteral("connected");
  v = (GetConnectionStatus() == SocketConnectionStatus::SOCKET_CONNECTED)
    ? true : false ;
  parameters.AppendElement(BluetoothNamedValue(name, v));

  name.AssignLiteral("address");
  v = mDevicePath;
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

nsresult
BluetoothHfpManager::HandleVolumeChanged(const nsAString& aData)
{
  MOZ_ASSERT(NS_IsMainThread());

  // The string that we're interested in will be a JSON string that looks like:
  //  {"key":"volumeup", "value":10}
  //  {"key":"volumedown", "value":2}

  JSContext* cx = nsContentUtils::GetSafeJSContext();
  if (!cx) {
    NS_WARNING("Failed to get JSContext");
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
  if (!JS_StringEqualsAscii(cx, key.toString(), AUDIO_VOLUME_BT_SCO, &match)) {
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

  if (!value.isNumber()) {
    return NS_ERROR_UNEXPECTED;
  }

  mCurrentVgs = value.toNumber();

  // Adjust volume by headset and we don't have to send volume back to headset
  if (mReceiveVgsFlag) {
    mReceiveVgsFlag = false;
    return NS_OK;
  }

  // Only send volume back when there's a connected headset
  if (GetConnectionStatus() == SocketConnectionStatus::SOCKET_CONNECTED) {
    SendCommand("+VGS: ", mCurrentVgs);
  }

  return NS_OK;
}

nsresult
BluetoothHfpManager::HandleVoiceConnectionChanged()
{
  nsCOMPtr<nsIMobileConnectionProvider> connection =
    do_GetService(NS_RILCONTENTHELPER_CONTRACTID);
  NS_ENSURE_TRUE(connection, NS_ERROR_FAILURE);

  nsIDOMMozMobileConnectionInfo* voiceInfo;
  connection->GetVoiceConnectionInfo(&voiceInfo);
  NS_ENSURE_TRUE(voiceInfo, NS_ERROR_FAILURE);

  bool roaming;
  voiceInfo->GetRoaming(&roaming);
  if (roaming != sCINDItems[CINDType::ROAM].value) {
    sCINDItems[CINDType::ROAM].value = roaming;
    SendCommand("+CIEV: ", CINDType::ROAM);
  }

  bool service = false;
  nsString regState;
  voiceInfo->GetState(regState);
  if (regState.EqualsLiteral("registered")) {
    service = true;
  }
  if (service != sCINDItems[CINDType::SERVICE].value) {
    sCINDItems[CINDType::SERVICE].value = service;
    SendCommand("+CIEV: ", CINDType::SERVICE);
  }

  uint8_t signal;
  JS::Value value;
  voiceInfo->GetRelSignalStrength(&value);
  if (!value.isNumber()) {
    NS_WARNING("Failed to get relSignalStrength in BluetoothHfpManager");
    return NS_ERROR_FAILURE;
  }
  signal = ceil(value.toNumber() / 20.0);

  if (signal != sCINDItems[CINDType::SIGNAL].value) {
    sCINDItems[CINDType::SIGNAL].value = signal;
    SendCommand("+CIEV: ", CINDType::SIGNAL);
  }

  return NS_OK;
}

nsresult
BluetoothHfpManager::HandleIccInfoChanged()
{
  nsCOMPtr<nsIMobileConnectionProvider> connection =
    do_GetService(NS_RILCONTENTHELPER_CONTRACTID);
  NS_ENSURE_TRUE(connection, NS_ERROR_FAILURE);

  nsIDOMMozMobileICCInfo* iccInfo;
  connection->GetIccInfo(&iccInfo);
  NS_ENSURE_TRUE(iccInfo, NS_ERROR_FAILURE);

  nsString msisdn;
  iccInfo->GetMsisdn(msisdn);

  if (!msisdn.Equals(mMsisdn)) {
    mMsisdn = msisdn;
  }

  return NS_OK;
}

nsresult
BluetoothHfpManager::HandleShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  gInShutdown = true;
  CloseSocket();
  gBluetoothHfpManager = nullptr;
  return NS_OK;
}

// Virtual function of class SocketConsumer
void
BluetoothHfpManager::ReceiveSocketData(UnixSocketRawData* aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoCString msg((const char*)aMessage->mData.get());
  msg.StripWhitespace();

  nsTArray<nsCString> atCommandValues;

  uint16_t currentCallState = mCurrentCallArray[mCurrentCallIndex].mState;

  // For more information, please refer to 4.34.1 "Bluetooth Defined AT
  // Capabilities" in Bluetooth hands-free profile 1.6
  if (msg.Find("AT+BRSF=") != -1) {
    SendCommand("+BRSF: ", 33);
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
    SendLine("+CHLD: (1,2)");
  } else if (msg.Find("AT+CHLD=") != -1) {
    ParseAtCommand(msg, 8, atCommandValues);

    if (atCommandValues.IsEmpty()) {
      NS_WARNING("Could't get the value of command [AT+VGS=]");
      goto respond_with_ok;
    }

    nsresult rv;
    int chld = atCommandValues[0].ToInteger(&rv);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to extract volume value from bluetooth headset!");
      goto respond_with_ok;
    }

    switch(chld) {
      case 1:
        // Releases active calls and accepts the other (held or waiting) call
        NotifyDialer(NS_LITERAL_STRING("CHUP+ATA"));
        break;
      case 2:
        // Places active calls on hold and accepts the other (held or waiting) call
        NotifyDialer(NS_LITERAL_STRING("CHLD+ATA"));
        break;
      default:
#ifdef DEBUG
        NS_WARNING("Not handling chld value");
#endif
        break;
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

#ifdef DEBUG
    NS_ASSERTION(newVgs >= 0 && newVgs <= 15, "Received invalid VGS value");
#endif

    nsString data;
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    data.AppendInt(newVgs);
    os->NotifyObservers(nullptr, "bluetooth-volume-change", data.get());
  } else if (msg.Find("AT+BLDN") != -1) {
    NotifyDialer(NS_LITERAL_STRING("BLDN"));
  } else if (msg.Find("ATA") != -1) {
    NotifyDialer(NS_LITERAL_STRING("ATA"));
  } else if (msg.Find("AT+CHUP") != -1) {
    NotifyDialer(NS_LITERAL_STRING("CHUP"));
  } else if (msg.Find("ATD>") != -1) {
    // Currently, we don't support memory dialing in Dialer app
    SendLine("ERROR");
    return;
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

    mCLIP = (atCommandValues[0].EqualsLiteral("1"));
  } else if (msg.Find("AT+CKPD") != -1) {
    // For Headset Profile (HSP)
    switch (currentCallState) {
      case nsIRadioInterfaceLayer::CALL_STATE_INCOMING:
        NotifyDialer(NS_LITERAL_STRING("ATA"));
        break;
      case nsIRadioInterfaceLayer::CALL_STATE_CONNECTED:
      case nsIRadioInterfaceLayer::CALL_STATE_DIALING:
      case nsIRadioInterfaceLayer::CALL_STATE_ALERTING:
        NotifyDialer(NS_LITERAL_STRING("CHUP"));
        break;
      case nsIRadioInterfaceLayer::CALL_STATE_DISCONNECTED:
        NotifyDialer(NS_LITERAL_STRING("BLDN"));
        break;
      default:
#ifdef DEBUG
        NS_WARNING("Not handling state changed");
#endif
        break;
    }
  } else if (msg.Find("AT+CNUM") != -1) {
    if (!mMsisdn.IsEmpty()) {
      nsAutoCString message("+CNUM: ,\"");
      message += NS_ConvertUTF16toUTF8(mMsisdn).get();
      message += "\",";
      message.AppendInt(TOA_UNKNOWN);
      message += ",,4";
      SendLine(message.get());
    }
  } else {
#ifdef DEBUG
    nsCString warningMsg;
    warningMsg.AssignLiteral("Not handling HFP message, reply ok: ");
    warningMsg.Append(msg);
    NS_WARNING(warningMsg.get());
#endif
  }

respond_with_ok:
  // We always respond to remote device with "OK" in general cases.
  SendLine("OK");
}

bool
BluetoothHfpManager::Connect(const nsAString& aDevicePath,
                             const bool aIsHandsfree,
                             BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (gInShutdown) {
    NS_WARNING("Connect called while in shutdown!");
    return false;
  }

  if (GetConnectionStatus() == SocketConnectionStatus::SOCKET_CONNECTED ||
      GetConnectionStatus() == SocketConnectionStatus::SOCKET_CONNECTING) {
    NS_WARNING("BluetoothHfpManager has connected/is connecting to a headset!");
    return false;
  }

  CloseSocket();

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, false);

  nsString uuid;
  if (aIsHandsfree) {
    BluetoothUuidHelper::GetString(BluetoothServiceClass::HANDSFREE, uuid);
  } else {
    BluetoothUuidHelper::GetString(BluetoothServiceClass::HEADSET, uuid);
  }

  mRunnable = aRunnable;

  nsresult rv = bs->GetSocketViaService(aDevicePath,
                                        uuid,
                                        BluetoothSocketType::RFCOMM,
                                        true,
                                        true,
                                        this,
                                        mRunnable);

  return NS_FAILED(rv) ? false : true;
}

bool
BluetoothHfpManager::Listen()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (gInShutdown) {
    MOZ_ASSERT(false, "Listen called while in shutdown!");
    return false;
  }

  if (GetConnectionStatus() == SocketConnectionStatus::SOCKET_LISTENING) {
    NS_WARNING("BluetoothHfpManager has been already listening");
    return true;
  }

  CloseSocket();

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE(bs, false);

  nsresult rv =
    bs->ListenSocketViaService(BluetoothReservedChannels::CHANNEL_HANDSFREE_AG,
                               BluetoothSocketType::RFCOMM,
                               true,
                               true,
                               this);

  mSocketStatus = GetConnectionStatus();

  return NS_FAILED(rv) ? false : true;
}

void
BluetoothHfpManager::Disconnect()
{
  if (GetConnectionStatus() == SocketConnectionStatus::SOCKET_DISCONNECTED) {
    NS_WARNING("BluetoothHfpManager has been disconnected!");
    return;
  }

  CloseScoSocket();
  CloseSocket();
}

bool
BluetoothHfpManager::SendLine(const char* aMessage)
{
  const char* kHfpCrlf = "\xd\xa";
  nsAutoCString msg;

  msg += kHfpCrlf;
  msg += aMessage;
  msg += kHfpCrlf;

  return SendSocketData(msg);
}

bool
BluetoothHfpManager::SendCommand(const char* aCommand, const uint16_t aValue)
{
  if (mSocketStatus != SocketConnectionStatus::SOCKET_CONNECTED) {
    return false;
  }

  nsAutoCString message;
  int value = aValue;
  message += aCommand;

  if (!strcmp(aCommand, "+CIEV: ")) {
    if (!mCMER) {
      // Indicator status update is disabled
      return true;
    }

    if ((aValue < 1) || (aValue > ArrayLength(sCINDItems) - 1)) {
      NS_WARNING("unexpected CINDType for CIEV command");
      return false;
    }

    message.AppendInt(aValue);
    message += ",";
    message.AppendInt(sCINDItems[aValue].value);
  } else if (!strcmp(aCommand, "+CIND: ")) {
    if (!aValue) {
      // Query for range
      for (uint8_t i = 1; i < ArrayLength(sCINDItems); i++) {
        message += "(\"";
        message += sCINDItems[i].name;
        message += "\",(";
        message += sCINDItems[i].range;
        message += ")";
        if (i == (ArrayLength(sCINDItems) - 1)) {
          message +=")";
          break;
        }
        message += "),";
      }
    } else {
      // Query for value
      for (uint8_t i = 1; i < ArrayLength(sCINDItems); i++) {
        message.AppendInt(sCINDItems[i].value);
        if (i == (ArrayLength(sCINDItems) - 1)) {
          break;
        }
        message += ",";
      }
    }
  } else if (!strcmp(aCommand, "+CLCC: ")) {
    bool rv = true;
    uint32_t callNumbers = mCurrentCallArray.Length();
    for (uint32_t i = 1; i < callNumbers; i++) {
      Call& call = mCurrentCallArray[i];
      if (call.mState == nsIRadioInterfaceLayer::CALL_STATE_DISCONNECTED) {
        continue;
      }

      message.AssignLiteral("+CLCC: ");
      message.AppendInt(i);
      message += ",";
      message.AppendInt(call.mDirection);
      message += ",";

      switch (call.mState) {
        case nsIRadioInterfaceLayer::CALL_STATE_CONNECTED:
          message.AppendInt(0);
          break;
        case nsIRadioInterfaceLayer::CALL_STATE_HELD:
          message.AppendInt(1);
          break;
        case nsIRadioInterfaceLayer::CALL_STATE_DIALING:
          message.AppendInt(2);
          break;
        case nsIRadioInterfaceLayer::CALL_STATE_ALERTING:
          message.AppendInt(3);
          break;
        case nsIRadioInterfaceLayer::CALL_STATE_INCOMING:
          message.AppendInt((i == mCurrentCallIndex) ? 4 : 5);
          break;
        default:
          NS_WARNING("Not handling call status for CLCC");
          break;
      }
      message += ",0,0,\"";
      message += NS_ConvertUTF16toUTF8(call.mNumber).get();
      message += "\",";
      message.AppendInt(call.mType);

      rv &= SendLine(message.get());
    }
    return rv;
  } else {
    message.AppendInt(value);
  }

  return SendLine(message.get());
}

void
BluetoothHfpManager::SetupCIND(uint32_t aCallIndex, uint16_t aCallState,
                               const nsAString& aNumber, bool aInitial)
{
  nsRefPtr<nsRunnable> sendRingTask;
  nsString address;

  while (aCallIndex >= mCurrentCallArray.Length()) {
    Call call;
    mCurrentCallArray.AppendElement(call);
  }

  // Same logic as implementation in ril_worker.js
  if (aNumber.Length() && aNumber[0] == '+') {
    mCurrentCallArray[aCallIndex].mType = TOA_INTERNATIONAL;
  }
  mCurrentCallArray[aCallIndex].mNumber = aNumber;

  uint16_t currentCallState = mCurrentCallArray[aCallIndex].mState;

  switch (aCallState) {
    case nsIRadioInterfaceLayer::CALL_STATE_INCOMING:
      mCurrentCallArray[aCallIndex].mDirection = true;
      sCINDItems[CINDType::CALLSETUP].value = CallSetupState::INCOMING;
      if (!aInitial) {
        SendCommand("+CIEV: ", CINDType::CALLSETUP);
      }

      if (!mCurrentCallIndex) {
        // Start sending RING indicator to HF
        sStopSendingRingFlag = false;

        nsAutoString number(aNumber);
        if (!mCLIP) {
          number.AssignLiteral("");
        }

        MessageLoop::current()->PostDelayedTask(FROM_HERE,
          new SendRingIndicatorTask(number, mCurrentCallArray[aCallIndex].mType),
          sRingInterval);
      }
      break;
    case nsIRadioInterfaceLayer::CALL_STATE_DIALING:
      mCurrentCallArray[aCallIndex].mDirection = false;
      sCINDItems[CINDType::CALLSETUP].value = CallSetupState::OUTGOING;
      if (!aInitial) {
        SendCommand("+CIEV: ", CINDType::CALLSETUP);

        GetSocketAddr(address);
        OpenScoSocket(address);
      }
      break;
    case nsIRadioInterfaceLayer::CALL_STATE_ALERTING:
      mCurrentCallArray[aCallIndex].mDirection = false;
      sCINDItems[CINDType::CALLSETUP].value = CallSetupState::OUTGOING_ALERTING;
      if (!aInitial) {
        SendCommand("+CIEV: ", CINDType::CALLSETUP);
      }
      break;
    case nsIRadioInterfaceLayer::CALL_STATE_CONNECTED:
      mCurrentCallIndex = aCallIndex;
      if (aInitial) {
        sCINDItems[CINDType::CALL].value = CallState::IN_PROGRESS;
        sCINDItems[CINDType::CALLSETUP].value = CallSetupState::NO_CALLSETUP;
      } else {
        switch (currentCallState) {
          case nsIRadioInterfaceLayer::CALL_STATE_INCOMING:
            // Incoming call, no break
            sStopSendingRingFlag = true;

            GetSocketAddr(address);
            OpenScoSocket(address);
          case nsIRadioInterfaceLayer::CALL_STATE_DISCONNECTED:
          case nsIRadioInterfaceLayer::CALL_STATE_ALERTING:
            // Outgoing call
            sCINDItems[CINDType::CALL].value = CallState::IN_PROGRESS;
            SendCommand("+CIEV: ", CINDType::CALL);
            sCINDItems[CINDType::CALLSETUP].value = CallSetupState::NO_CALLSETUP;
            SendCommand("+CIEV: ", CINDType::CALLSETUP);
            break;
          default:
#ifdef DEBUG
            NS_WARNING("Not handling state changed");
#endif
            break;
        }
      }
      break;
    case nsIRadioInterfaceLayer::CALL_STATE_DISCONNECTED:
      if (!aInitial) {
        switch (currentCallState) {
          case nsIRadioInterfaceLayer::CALL_STATE_INCOMING:
          case nsIRadioInterfaceLayer::CALL_STATE_BUSY:
            // Incoming call, no break
            sStopSendingRingFlag = true;
          case nsIRadioInterfaceLayer::CALL_STATE_DIALING:
          case nsIRadioInterfaceLayer::CALL_STATE_ALERTING:
            // Outgoing call
            sCINDItems[CINDType::CALLSETUP].value = CallSetupState::NO_CALLSETUP;
            SendCommand("+CIEV: ", CINDType::CALLSETUP);
            break;
          case nsIRadioInterfaceLayer::CALL_STATE_CONNECTED:
            sCINDItems[CINDType::CALL].value = CallState::NO_CALL;
            SendCommand("+CIEV: ", CINDType::CALL);
            break;
          case nsIRadioInterfaceLayer::CALL_STATE_HELD:
            sCINDItems[CINDType::CALLHELD].value = NO_CALLHELD;
            SendCommand("+CIEV: ", CINDType::CALLHELD);
          default:
#ifdef DEBUG
            NS_WARNING("Not handling state changed");
#endif
            break;
        }

        if (aCallIndex == mCurrentCallIndex) {
          NS_ASSERTION(mCurrentCallArray.Length() > aCallIndex,
            "Call index out of bounds!");
          mCurrentCallArray[aCallIndex].mState = aCallState;

          // Find the first non-disconnected call (like connected, held),
          // and update mCurrentCallIndex
          uint32_t c = 1;
          uint32_t length = mCurrentCallArray.Length();
          while (c < length) {
            if (mCurrentCallArray[c].mState !=
                nsIRadioInterfaceLayer::CALL_STATE_DISCONNECTED) {
              mCurrentCallIndex = c;
              break;
            }
            c++;
          }

          // There is no call, close Sco and clear mCurrentCallArray
          if (c == length) {
            CloseScoSocket();
            ResetCallArray();
          }
        }
      }
      break;
    case nsIRadioInterfaceLayer::CALL_STATE_HELD:
      sCINDItems[CINDType::CALLHELD].value = CallHeldState::ONHOLD_ACTIVE;

      if (!aInitial) {
        SendCommand("+CIEV: ", CINDType::CALLHELD);
      }

      break;
    default:
#ifdef DEBUG
      NS_WARNING("Not handling state changed");
      sCINDItems[CINDType::CALL].value = CallState::NO_CALL;
      sCINDItems[CINDType::CALLSETUP].value = CallSetupState::NO_CALLSETUP;
      sCINDItems[CINDType::CALLHELD].value = CallHeldState::NO_CALLHELD;
#endif
      break;
  }

  mCurrentCallArray[aCallIndex].mState = aCallState;
}

/*
 * EnumerateCallState will be called for each call
 */
void
BluetoothHfpManager::EnumerateCallState(uint32_t aCallIndex, uint16_t aCallState,
                                        const nsAString& aNumber, bool aIsActive)
{
  SetupCIND(aCallIndex, aCallState, aNumber, true);

  if (sCINDItems[CINDType::CALL].value == CallState::IN_PROGRESS ||
      sCINDItems[CINDType::CALLSETUP].value == CallSetupState::OUTGOING ||
      sCINDItems[CINDType::CALLSETUP].value == CallSetupState::OUTGOING_ALERTING) {
    nsString address;
    GetSocketAddr(address);
    OpenScoSocket(address);
  }
}

/*
 * CallStateChanged will be called whenever call status is changed, and it
 * also means we need to notify HS about the change. For more information,
 * please refer to 4.13 ~ 4.15 in Bluetooth hands-free profile 1.6.
 */
void
BluetoothHfpManager::CallStateChanged(uint32_t aCallIndex, uint16_t aCallState,
                                      const nsAString& aNumber, bool aIsActive)
{
  if (GetConnectionStatus() != SocketConnectionStatus::SOCKET_CONNECTED) {
    return;
  }

  SetupCIND(aCallIndex, aCallState, aNumber, false);
}

void
BluetoothHfpManager::OnConnectSuccess()
{
  // For active connection request, we need to reply the DOMRequest
  if (mRunnable) {
    BluetoothValue v = true;
    nsString errorStr;
    DispatchBluetoothReply(mRunnable, v, errorStr);

    mRunnable.forget();
  }

  // Cache device path for NotifySettings() since we can't get socket address
  // when a headset disconnect with us
  GetSocketAddr(mDevicePath);
  mSocketStatus = GetConnectionStatus();

  nsCOMPtr<nsIRILContentHelper> ril =
    do_GetService(NS_RILCONTENTHELPER_CONTRACTID);
  NS_ENSURE_TRUE_VOID(ril);
  ril->EnumerateCalls(mListener->GetCallback());

  NotifySettings();
}

void
BluetoothHfpManager::OnConnectError()
{
  // For active connection request, we need to reply the DOMRequest
  if (mRunnable) {
    BluetoothValue v;
    nsString errorStr;
    errorStr.AssignLiteral("Failed to connect with a bluetooth headset!");
    DispatchBluetoothReply(mRunnable, v, errorStr);

    mRunnable.forget();
  }

  // If connecting for some reason didn't work, restart listening
  CloseSocket();
  mSocketStatus = GetConnectionStatus();
  Listen();
}

void
BluetoothHfpManager::OnDisconnect()
{
  // When we close a connected socket, then restart listening again and
  // notify Settings app.
  if (mSocketStatus == SocketConnectionStatus::SOCKET_CONNECTED) {
    Listen();
    NotifySettings();
  } else if (mSocketStatus == SocketConnectionStatus::SOCKET_CONNECTING) {
    NS_WARNING("BluetoothHfpManager got unexpected socket status!");
  }

  Reset();
}
