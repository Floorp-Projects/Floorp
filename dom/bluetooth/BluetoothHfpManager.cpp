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
#include "BluetoothServiceUuid.h"
#include "BluetoothUtils.h"

#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsContentUtils.h"
#include "nsIAudioManager.h"
#include "nsIObserverService.h"
#include "nsIRadioInterfaceLayer.h"
#include "nsVariant.h"

#include <unistd.h> /* usleep() */

#define MOZSETTINGS_CHANGED_ID "mozsettings-changed"
#define BLUETOOTH_SCO_STATUS_CHANGED "bluetooth-sco-status-changed"
#define AUDIO_VOLUME_MASTER "audio.volume.master"
#define HANDSFREE_UUID mozilla::dom::bluetooth::BluetoothServiceUuidStr::Handsfree
#define HEADSET_UUID mozilla::dom::bluetooth::BluetoothServiceUuidStr::Headset

using namespace mozilla;
using namespace mozilla::ipc;
USING_BLUETOOTH_NAMESPACE

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

    return true;
  }

  bool Shutdown()
  {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (!obs ||
        (NS_FAILED(obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) ||
         NS_FAILED(obs->RemoveObserver(this, MOZSETTINGS_CHANGED_ID)))) {
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

namespace {
  StaticRefPtr<BluetoothHfpManager> gBluetoothHfpManager;
  StaticRefPtr<BluetoothHfpManagerObserver> sHfpObserver;
  bool gInShutdown = false;
  static nsCOMPtr<nsIThread> sHfpCommandThread;
  static bool sStopSendingRingFlag = true;

  static int kRingInterval = 3000000;  //unit: us
} // anonymous namespace

NS_IMPL_ISUPPORTS1(BluetoothHfpManagerObserver, nsIObserver)

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
  }

  MOZ_ASSERT(false, "BluetoothHfpManager got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

class SendRingIndicatorTask : public nsRunnable
{
public:
  SendRingIndicatorTask()
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    while (!sStopSendingRingFlag) {
      gBluetoothHfpManager->SendLine("RING");

      usleep(kRingInterval);
    }

    return NS_OK;
  }
};

void
OpenScoSocket(const nsAString& aDevicePath)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothScoManager* sco = BluetoothScoManager::Get();
  if (!sco) {
    NS_WARNING("BluetoothScoManager is not available!");
    return;
  }

  if (!sco->Connect(aDevicePath)) {
    NS_WARNING("Failed to create a sco socket!");
  }
}

void
CloseScoSocket()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIAudioManager> am = do_GetService("@mozilla.org/telephony/audiomanager;1");
  if (!am) {
    NS_WARNING("Failed to get AudioManager Service!");
    return;
  }
  am->SetForceForUse(am->USE_COMMUNICATION, am->FORCE_NONE);

  BluetoothScoManager* sco = BluetoothScoManager::Get();
  if (!sco) {
    NS_WARNING("BluetoothScoManager is not available!");
    return;
  }

  if (sco->GetConnected()) {
    nsCOMPtr<nsIObserverService> obs = do_GetService("@mozilla.org/observer-service;1");
    if (obs) {
      if (NS_FAILED(obs->NotifyObservers(nullptr, BLUETOOTH_SCO_STATUS_CHANGED, nullptr))) {
        NS_WARNING("Failed to notify bluetooth-sco-status-changed observsers!");
        return;
      }
    }
    sco->Disconnect();
  }
}

BluetoothHfpManager::BluetoothHfpManager()
  : mCurrentVgs(-1)
  , mCurrentCallIndex(0)
  , mCurrentCallState(nsIRadioInterfaceLayer::CALL_STATE_DISCONNECTED)
  , mCall(0)
  , mCallSetup(0)
  , mCallHeld(0)
{
}

bool
BluetoothHfpManager::Init()
{
  sHfpObserver = new BluetoothHfpManagerObserver();
  if (!sHfpObserver->Init()) {
    NS_WARNING("Cannot set up Hfp Observers!");
  }

  mListener = new BluetoothRilListener();
  if (!mListener->StartListening()) {
    NS_WARNING("Failed to start listening RIL");
    return false;
  }

  if (!sHfpCommandThread) {
    if (NS_FAILED(NS_NewThread(getter_AddRefs(sHfpCommandThread)))) {
      NS_ERROR("Failed to new thread for sHfpCommandThread");
      return false;
    }
  }

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

  // Shut down the command thread if it still exists.
  if (sHfpCommandThread) {
    nsCOMPtr<nsIThread> thread;
    sHfpCommandThread.swap(thread);
    if (NS_FAILED(thread->Shutdown())) {
      NS_WARNING("Failed to shut down the bluetooth hfpmanager command thread!");
    }
  }

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
BluetoothHfpManager::NotifySettings(const bool aConnected)
{
  nsString type, name;
  BluetoothValue v;
  InfallibleTArray<BluetoothNamedValue> parameters;
  type.AssignLiteral("bluetooth-hfp-status-changed");

  name.AssignLiteral("connected");
  v = aConnected;
  parameters.AppendElement(BluetoothNamedValue(name, v));

  name.AssignLiteral("address");
  v = GetAddressFromObjectPath(mDevicePath);
  parameters.AppendElement(BluetoothNamedValue(name, v));

  if (!BroadcastSystemMessage(type, parameters)) {
    NS_WARNING("Failed to broadcast system message to dialer");
    return;
  }
}

void
BluetoothHfpManager::NotifyDialer(const nsAString& aCommand)
{
  nsString type, name, command;
  command = aCommand;
  InfallibleTArray<BluetoothNamedValue> parameters;
  type.AssignLiteral("bluetooth-dialer-command");

  BluetoothValue v(command);
  parameters.AppendElement(BluetoothNamedValue(type, v));

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
  //  {"key":"volumeup", "value":1.0}
  //  {"key":"volumedown", "value":0.2}

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
  if (!JS_StringEqualsAscii(cx, key.toString(), AUDIO_VOLUME_MASTER, &match)) {
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

  // AG volume range: [0.0, 1.0]
  float volume = value.toNumber();

  // HS volume range: [0, 15]
  mCurrentVgs = ceil(volume * 15);

  nsDiscriminatedUnion du;
  du.mType = 0;
  du.u.mInt8Value = mCurrentVgs;

  nsCString vgs;
  if (NS_FAILED(nsVariant::ConvertToACString(du, vgs))) {
    NS_WARNING("Failed to convert volume to string");
    return NS_ERROR_FAILURE;
  }

  nsAutoCString newVgs;
  newVgs += "+VGS: ";
  newVgs += vgs;

  SendLine(newVgs.get());

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

bool
AppendIntegerToString(nsAutoCString& aString, int aValue)
{
  nsDiscriminatedUnion du;
  du.mType = 0;
  du.u.mInt8Value = aValue;

  nsCString temp;
  if (NS_FAILED(nsVariant::ConvertToACString(du, temp))) {
    NS_WARNING("Failed to convert mCall/mCallSetup/mCallHeld to string");
    return false;
  }
  aString += temp;
  aString += ",";
  return true;
}

// Virtual function of class SocketConsumer
void
BluetoothHfpManager::ReceiveSocketData(UnixSocketRawData* aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());

  const char* msg = (const char*)aMessage->mData;

  // For more information, please refer to 4.34.1 "Bluetooth Defined AT
  // Capabilities" in Bluetooth hands-free profile 1.6
  if (!strncmp(msg, "AT+BRSF=", 8)) {
    SendLine("+BRSF: 23");
    SendLine("OK");
  } else if (!strncmp(msg, "AT+CIND=?", 9)) {
    nsAutoCString cindRange;

    cindRange += "+CIND: ";
    cindRange += "(\"battchg\",(0-5)),";
    cindRange += "(\"signal\",(0-5)),";
    cindRange += "(\"service\",(0,1)),";
    cindRange += "(\"call\",(0,1)),";
    cindRange += "(\"callsetup\",(0-3)),";
    cindRange += "(\"callheld\",(0-2)),";
    cindRange += "(\"roam\",(0,1))";

    SendLine(cindRange.get());
    SendLine("OK");
  } else if (!strncmp(msg, "AT+CIND?", 8)) {
    nsAutoCString cind;
    cind += "+CIND: 5,5,1,";

    if (!AppendIntegerToString(cind, mCall) ||
        !AppendIntegerToString(cind, mCallSetup) ||
        !AppendIntegerToString(cind, mCallHeld)) {
      NS_WARNING("Failed to convert mCall/mCallSetup/mCallHeld to string");
    } 
    cind += "0";

    SendLine(cind.get());
    SendLine("OK");
  } else if (!strncmp(msg, "AT+CMER=", 8)) {
    // SLC establishment
    NotifySettings(true);
    SendLine("OK");
  } else if (!strncmp(msg, "AT+CHLD=?", 9)) {
    SendLine("+CHLD: (0,1,2,3)");
    SendLine("OK");
  } else if (!strncmp(msg, "AT+CHLD=", 8)) {
    SendLine("OK");
  } else if (!strncmp(msg, "AT+VGS=", 7)) {
    // VGS range: [0, 15]
    int newVgs = msg[7] - '0';

    if (strlen(msg) > 8) {
      newVgs *= 10;
      newVgs += (msg[8] - '0');
    }

#ifdef DEBUG
    NS_ASSERTION(newVgs >= 0 && newVgs <= 15, "Received invalid VGS value");
#endif

    // Currently, we send volume up/down commands to represent that
    // volume has been changed by Bluetooth headset, and that will affect
    // the main stream volume of our device. In the future, we may want to
    // be able to set volume by stream.
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (newVgs > mCurrentVgs) {
      os->NotifyObservers(nullptr, "bluetooth-volume-change", NS_LITERAL_STRING("up").get());
    } else if (newVgs < mCurrentVgs) {
      os->NotifyObservers(nullptr, "bluetooth-volume-change", NS_LITERAL_STRING("down").get());
    }

    mCurrentVgs = newVgs;

    SendLine("OK");
  } else if (!strncmp(msg, "AT+BLDN", 7)) {
    NotifyDialer(NS_LITERAL_STRING("BLDN"));
    SendLine("OK");
  } else if (!strncmp(msg, "ATA", 3)) {
    NotifyDialer(NS_LITERAL_STRING("ATA"));
    SendLine("OK");
  } else if (!strncmp(msg, "AT+CHUP", 7)) {
    NotifyDialer(NS_LITERAL_STRING("CHUP"));
    SendLine("OK");
  } else if (!strncmp(msg, "AT+CKPD", 7)) {
    // For Headset
    switch (mCurrentCallState) {
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
    SendLine("OK");
  } else {
#ifdef DEBUG
    nsCString warningMsg;
    warningMsg.AssignLiteral("Not handling HFP message, reply ok: ");
    warningMsg.Append(msg);
    NS_WARNING(warningMsg.get());
#endif
    SendLine("OK");
  }
}

bool
BluetoothHfpManager::Connect(const nsAString& aDeviceObjectPath,
                             const bool aIsHandsfree,
                             BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (gInShutdown) {
    MOZ_ASSERT(false, "Connect called while in shutdown!");
    return false;
  }

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    NS_WARNING("BluetoothService not available!");
    return false;
  }
  mDevicePath = aDeviceObjectPath;

  nsString serviceUuidStr;
  if (aIsHandsfree) {
    serviceUuidStr = NS_ConvertUTF8toUTF16(HANDSFREE_UUID);
  } else {
    serviceUuidStr = NS_ConvertUTF8toUTF16(HEADSET_UUID);
  }

  nsCOMPtr<nsIRILContentHelper> ril =
    do_GetService("@mozilla.org/ril/content-helper;1");
  NS_ENSURE_TRUE(ril, NS_ERROR_UNEXPECTED);
  ril->EnumerateCalls(mListener->GetCallback());

  nsRefPtr<BluetoothReplyRunnable> runnable = aRunnable;

  nsresult rv = bs->GetSocketViaService(aDeviceObjectPath,
                                        serviceUuidStr,
                                        BluetoothSocketType::RFCOMM,
                                        true,
                                        false,
                                        this,
                                        runnable);

  runnable.forget();
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

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    NS_WARNING("BluetoothService not available!");
    return false;
  }

  nsresult rv = bs->ListenSocketViaService(BluetoothReservedChannels::HANDSFREE_AG,
                                           BluetoothSocketType::RFCOMM,
                                           true,
                                           false,
                                           this);
  return NS_FAILED(rv) ? false : true;
}

void
BluetoothHfpManager::Disconnect()
{
  NotifySettings(false);
  CloseSocket();
  mCall = 0;
  mCallSetup = 0;
  mCallHeld = 0;
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

/*
 * EnumerateCallState will be called for each call
 */
void
BluetoothHfpManager::EnumerateCallState(int aCallIndex, int aCallState,
                                        const char* aNumber, bool aIsActive)
{
  // TODO: enums for mCall, mCallSetup, mCallHeld
  /* mCall - 0: there are no calls in progress
   *       - 1: at least one call is in progress
   * mCallSetup - 0: not currently in call set up
   *            - 1: an incoming call process ongoing
   *            - 2: an outgoing call set up is ongoing
   *            - 3: remote party being alerted in an outgoing call
   * mCallHeld - 0: no calls held
   *           - 1: call is placed on hold or active/held calls swapped
   *           - 2: call o hold, no active call
   */

  if (mCallHeld == 2 && aIsActive) {
    mCallHeld = 1;
  }

  if (mCall || mCallSetup) {
    return;
  }

  switch (aCallState) {
    case nsIRadioInterfaceLayer::CALL_STATE_ALERTING:
      mCall = 1;
      mCallSetup = 3;
      break;
    case nsIRadioInterfaceLayer::CALL_STATE_INCOMING:
      mCall = 1;
      mCallSetup = 1;
      break;
    case nsIRadioInterfaceLayer::CALL_STATE_CONNECTING:
    case nsIRadioInterfaceLayer::CALL_STATE_CONNECTED:
    case nsIRadioInterfaceLayer::CALL_STATE_DISCONNECTING:
    case nsIRadioInterfaceLayer::CALL_STATE_DISCONNECTED:
    case nsIRadioInterfaceLayer::CALL_STATE_BUSY:
      mCall = 1;
      mCallSetup = 2;
      break;
    case nsIRadioInterfaceLayer::CALL_STATE_HOLDING:
    case nsIRadioInterfaceLayer::CALL_STATE_HELD:
    case nsIRadioInterfaceLayer::CALL_STATE_RESUMING:
      mCall = 1;
      mCallHeld = 2;
    default:
      mCall = 0;
      mCallSetup = 0;
  }
}

/*
 * CallStateChanged will be called whenever call status is changed, and it
 * also means we need to notify HS about the change. For more information, 
 * please refer to 4.13 ~ 4.15 in Bluetooth hands-free profile 1.6.
 */
void
BluetoothHfpManager::CallStateChanged(int aCallIndex, int aCallState,
                                      const char* aNumber, bool aIsActive)
{
  nsRefPtr<nsRunnable> sendRingTask;

  switch (aCallState) {
    case nsIRadioInterfaceLayer::CALL_STATE_INCOMING:
      // Send "CallSetup = 1"
      SendLine("+CIEV: 5,1");

      // Start sending RING indicator to HF
      sStopSendingRingFlag = false;
      sendRingTask = new SendRingIndicatorTask();

      if (NS_FAILED(sHfpCommandThread->Dispatch(sendRingTask, NS_DISPATCH_NORMAL))) {
        NS_WARNING("Cannot dispatch ring task!");
        return;
      };
      break;
    case nsIRadioInterfaceLayer::CALL_STATE_DIALING:
      // Send "CallSetup = 2"
      SendLine("+CIEV: 5,2");
      break;
    case nsIRadioInterfaceLayer::CALL_STATE_ALERTING:
      // Send "CallSetup = 3"
      if (mCurrentCallIndex == nsIRadioInterfaceLayer::CALL_STATE_DIALING) {
        SendLine("+CIEV: 5,3");
      } else {
#ifdef DEBUG
        NS_WARNING("Not handling state changed");
#endif
      }
      break;
    case nsIRadioInterfaceLayer::CALL_STATE_CONNECTED:
      switch (mCurrentCallState) {
        case nsIRadioInterfaceLayer::CALL_STATE_INCOMING:
          sStopSendingRingFlag = true;
          // Continue executing, no break
        case nsIRadioInterfaceLayer::CALL_STATE_DIALING:
          // Send "Call = 1, CallSetup = 0"
          SendLine("+CIEV: 4,1");
          SendLine("+CIEV: 5,0");
          break;
        default:
#ifdef DEBUG
          NS_WARNING("Not handling state changed");
#endif
          break;
      }
      OpenScoSocket(mDevicePath);
      break;
    case nsIRadioInterfaceLayer::CALL_STATE_DISCONNECTED:
      switch (mCurrentCallState) {
        case nsIRadioInterfaceLayer::CALL_STATE_INCOMING:
          sStopSendingRingFlag = true;
          // Continue executing, no break
        case nsIRadioInterfaceLayer::CALL_STATE_DIALING:
        case nsIRadioInterfaceLayer::CALL_STATE_ALERTING:
          // Send "CallSetup = 0"
          SendLine("+CIEV: 5,0");
          break;
        case nsIRadioInterfaceLayer::CALL_STATE_CONNECTED:
          // Send "Call = 0"
          SendLine("+CIEV: 4,0");
          break;
        default:
#ifdef DEBUG
          NS_WARNING("Not handling state changed");
#endif
          break;
      }
      CloseScoSocket();
      break;

    default:
#ifdef DEBUG
      NS_WARNING("Not handling state changed");
#endif
      break;
  }

  mCurrentCallIndex = aCallIndex;
  mCurrentCallState = aCallState;
}
