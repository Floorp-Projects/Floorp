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

#include <unistd.h> /* usleep() */

#define MOZSETTINGS_CHANGED_ID "mozsettings-changed"
#define AUDIO_VOLUME_MASTER "audio.volume.master"
#define HANDSFREE_UUID mozilla::dom::bluetooth::BluetoothServiceUuidStr::Handsfree
#define HEADSET_UUID mozilla::dom::bluetooth::BluetoothServiceUuidStr::Headset

using namespace mozilla;
using namespace mozilla::ipc;
USING_BLUETOOTH_NAMESPACE

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
  SIGNAL,
  SERVICE,
  CALL,
  CALLSETUP,
  CALLHELD,
  ROAM,
};

static CINDItem sCINDItems[] = {
  {},
  {"battchg", "0-5", 5},
  {"signal", "0-5", 5},
  {"service", "0,1", 1},
  {"call", "0,1", CallState::NO_CALL},
  {"callsetup", "0-3", CallSetupState::NO_CALLSETUP},
  {"callheld", "0-2", CallHeldState::NO_CALLHELD},
  {"roam", "0,1", 0}
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

BluetoothHfpManager::BluetoothHfpManager()
  : mCurrentVgs(-1)
  , mCurrentCallIndex(0)
  , mCurrentCallState(nsIRadioInterfaceLayer::CALL_STATE_DISCONNECTED)
{
  sCINDItems[CINDType::CALL].value = CallState::NO_CALL;
  sCINDItems[CINDType::CALLSETUP].value = CallSetupState::NO_CALLSETUP;
  sCINDItems[CINDType::CALLHELD].value = CallHeldState::NO_CALLHELD;
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
BluetoothHfpManager::NotifySettings()
{
  nsString type, name;
  BluetoothValue v;
  InfallibleTArray<BluetoothNamedValue> parameters;
  type.AssignLiteral("bluetooth-hfp-status-changed");

  name.AssignLiteral("connected");
  uint32_t status = GetConnectionStatus();
  v = status;
  parameters.AppendElement(BluetoothNamedValue(name, v));

  name.AssignLiteral("address");
  nsString address;
  GetSocketAddr(address);
  v = address;
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

  if (GetConnectionStatus() != SocketConnectionStatus::SOCKET_CONNECTED) {
    return NS_OK;
  }

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
  // HS volume range: [0, 15]
  float volume = value.toNumber();
  mCurrentVgs = ceil(volume * 15);

  SendCommand("+VGS: ", mCurrentVgs);

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

  const char* msg = (const char*)aMessage->mData;

  // For more information, please refer to 4.34.1 "Bluetooth Defined AT
  // Capabilities" in Bluetooth hands-free profile 1.6
  if (!strncmp(msg, "AT+BRSF=", 8)) {
    SendCommand("+BRSF: ", 23);
    SendLine("OK");
  } else if (!strncmp(msg, "AT+CIND=?", 9)) {
    // Asking for CIND range
    SendCommand("+CIND: ", 0);
    SendLine("OK");
  } else if (!strncmp(msg, "AT+CIND?", 8)) {
    // Asking for CIND value
    SendCommand("+CIND: ", 1);
    SendLine("OK");
  } else if (!strncmp(msg, "AT+CMER=", 8)) {
    // SLC establishment
    SendLine("OK");
  } else if (!strncmp(msg, "AT+CHLD=?", 9)) {
    SendLine("+CHLD: (0,1,2,3)");
    SendLine("OK");
  } else if (!strncmp(msg, "AT+CHLD=", 8)) {
    SendLine("OK");
  } else if (!strncmp(msg, "AT+VGS=", 7)) {
    // HS volume range: [0, 15]
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
BluetoothHfpManager::Connect(const nsAString& aDevicePath,
                             const bool aIsHandsfree,
                             BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (gInShutdown) {
    MOZ_ASSERT(false, "Connect called while in shutdown!");
    return false;
  }

  if (GetConnectionStatus() == SocketConnectionStatus::SOCKET_CONNECTED) {
    NS_WARNING("BluetoothHfpManager has connected to a headset/handsfree!");
    return false;
  }

  CloseSocket();

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    NS_WARNING("BluetoothService not available!");
    return false;
  }

  nsString serviceUuidStr;
  if (aIsHandsfree) {
    serviceUuidStr = NS_ConvertUTF8toUTF16(HANDSFREE_UUID);
  } else {
    serviceUuidStr = NS_ConvertUTF8toUTF16(HEADSET_UUID);
  }

  nsCOMPtr<nsIRILContentHelper> ril =
    do_GetService("@mozilla.org/ril/content-helper;1");
  if (!ril) {
    MOZ_ASSERT("Failed to get RIL Content Helper");
  }
  ril->EnumerateCalls(mListener->GetCallback());

  nsRefPtr<BluetoothReplyRunnable> runnable = aRunnable;

  nsresult rv = bs->GetSocketViaService(aDevicePath,
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

  CloseSocket();

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
  if (GetConnectionStatus() == SocketConnectionStatus::SOCKET_DISCONNECTED) {
    NS_WARNING("BluetoothHfpManager has disconnected!");
    return;
  }

  CloseSocket();
  Listen();
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
BluetoothHfpManager::SendCommand(const char* aCommand, const int aValue)
{
  nsAutoCString message;
  int value = aValue;
  message += aCommand;

  if (!strcmp(aCommand, "+CIEV: ")) {
    message.AppendInt(aValue);
    message += ",";
    switch (aValue) {
      case CINDType::CALL:
        message.AppendInt(sCINDItems[CINDType::CALL].value);
        break;
      case CINDType::CALLSETUP:
        message.AppendInt(sCINDItems[CINDType::CALLSETUP].value);
        break;
      case CINDType::CALLHELD:
        message.AppendInt(sCINDItems[CINDType::CALLHELD].value);
        break;
      default:
#ifdef DEBUG
        NS_WARNING("unexpected CINDType for CIEV command");
#endif
        return false;
    }
  } else if (!strcmp(aCommand, "+CIND: ")) {
    if (!aValue) {
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
      for (uint8_t i = 1; i < ArrayLength(sCINDItems); i++) {
        message.AppendInt(sCINDItems[i].value);
        if (i == (ArrayLength(sCINDItems) - 1)) {
          break;
        }
        message += ",";
      }
    }
  } else {
    message.AppendInt(value);
  }

  return SendLine(message.get());
}

void
BluetoothHfpManager::SetupCIND(int aCallIndex, int aCallState, bool aInitial)
{
  nsRefPtr<nsRunnable> sendRingTask;
  nsString address;

  switch (aCallState) {
    case nsIRadioInterfaceLayer::CALL_STATE_INCOMING:
      sCINDItems[CINDType::CALLSETUP].value = CallSetupState::INCOMING;
      if (!aInitial) {
        SendCommand("+CIEV: ", CINDType::CALLSETUP);
      }

      // Start sending RING indicator to HF
      sStopSendingRingFlag = false;
      sendRingTask = new SendRingIndicatorTask();

      if (NS_FAILED(sHfpCommandThread->Dispatch(sendRingTask, NS_DISPATCH_NORMAL))) {
        NS_WARNING("Cannot dispatch ring task!");
        return;
      };
      break;
    case nsIRadioInterfaceLayer::CALL_STATE_DIALING:
      sCINDItems[CINDType::CALLSETUP].value = CallSetupState::OUTGOING;
      if (!aInitial) {
        SendCommand("+CIEV: ", CINDType::CALLSETUP);

        GetSocketAddr(address);
        OpenScoSocket(address);
      }
      break;
    case nsIRadioInterfaceLayer::CALL_STATE_ALERTING:
      sCINDItems[CINDType::CALLSETUP].value = CallSetupState::OUTGOING_ALERTING;
      if (!aInitial) {
        SendCommand("+CIEV: ", CINDType::CALLSETUP);
      }
      break;
    case nsIRadioInterfaceLayer::CALL_STATE_CONNECTED:
      if (aInitial) {
        sCINDItems[CINDType::CALL].value = CallState::IN_PROGRESS;
        sCINDItems[CINDType::CALLSETUP].value = CallSetupState::NO_CALLSETUP;
      } else {
        switch (mCurrentCallState) {
          case nsIRadioInterfaceLayer::CALL_STATE_INCOMING:
            // Incoming call, no break
            sStopSendingRingFlag = true;

            GetSocketAddr(address);
            OpenScoSocket(address);
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
        switch (mCurrentCallState) {
          case nsIRadioInterfaceLayer::CALL_STATE_INCOMING:
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
          default:
#ifdef DEBUG
            NS_WARNING("Not handling state changed");
#endif
            break;
        }
        CloseScoSocket();
      }
      break;
    default:
#ifdef DEBUG
      NS_WARNING("Not handling state changed");
      sCINDItems[CINDType::CALL].value = CallState::NO_CALL;
      sCINDItems[CINDType::CALLSETUP].value = CallSetupState::NO_CALLSETUP;
#endif
      break;
  }

  mCurrentCallIndex = aCallIndex;
  mCurrentCallState = aCallState;
}

/*
 * EnumerateCallState will be called for each call
 */
void
BluetoothHfpManager::EnumerateCallState(int aCallIndex, int aCallState,
                                        const char* aNumber, bool aIsActive)
{
  if (aIsActive &&
      (sCINDItems[CINDType::CALLHELD].value == CallHeldState::ONHOLD_NOACTIVE)) {
    sCINDItems[CINDType::CALLHELD].value = CallHeldState::ONHOLD_ACTIVE;
  }

  if (sCINDItems[CINDType::CALL].value || sCINDItems[CINDType::CALLSETUP].value) {
    return;
  }

  SetupCIND(aCallIndex, aCallState, true);
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
  if (GetConnectionStatus() != SocketConnectionStatus::SOCKET_CONNECTED) {
    return;
  }

  SetupCIND(aCallIndex, aCallState, false);
}

void
BluetoothHfpManager::OnConnectSuccess()
{
  if (mCurrentCallState == nsIRadioInterfaceLayer::CALL_STATE_CONNECTED ||
      mCurrentCallState == nsIRadioInterfaceLayer::CALL_STATE_DIALING ||
      mCurrentCallState == nsIRadioInterfaceLayer::CALL_STATE_ALERTING) {
    nsString address;
    GetSocketAddr(address);
    OpenScoSocket(address);
  }

  NotifySettings();
}

void
BluetoothHfpManager::OnConnectError()
{
  CloseSocket();
  // If connecting for some reason didn't work, restart listening
  Listen();
}

void
BluetoothHfpManager::OnDisconnect()
{
  NotifySettings();
  sCINDItems[CINDType::CALL].value = CallState::NO_CALL;
  sCINDItems[CINDType::CALLSETUP].value = CallSetupState::NO_CALLSETUP;
  sCINDItems[CINDType::CALLHELD].value = CallHeldState::NO_CALLHELD;
}
