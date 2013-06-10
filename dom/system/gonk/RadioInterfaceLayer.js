/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

var RIL = {};
Cu.import("resource://gre/modules/ril_consts.js", RIL);

// set to true in ril_consts.js to see debug messages
var DEBUG = RIL.DEBUG_RIL;

// Read debug setting from pref
let debugPref = false;
try {
  debugPref = Services.prefs.getBoolPref("ril.debugging.enabled");
} catch(e) {
  debugPref = false;
}
DEBUG = RIL.DEBUG_RIL || debugPref;

let debug;
if (DEBUG) {
  debug = function (s) {
    dump("-*- RadioInterfaceLayer: " + s + "\n");
  };
} else {
  debug = function (s) {};
}

const RADIOINTERFACELAYER_CID =
  Components.ID("{2d831c8d-6017-435b-a80c-e5d422810cea}");
const RILNETWORKINTERFACE_CID =
  Components.ID("{3bdd52a9-3965-4130-b569-0ac5afed045e}");

const nsIAudioManager = Ci.nsIAudioManager;
const nsITelephonyProvider = Ci.nsITelephonyProvider;

const kNetworkInterfaceStateChangedTopic = "network-interface-state-changed";
const kSmsReceivedObserverTopic          = "sms-received";
const kSmsSendingObserverTopic           = "sms-sending";
const kSmsSentObserverTopic              = "sms-sent";
const kSmsFailedObserverTopic            = "sms-failed";
const kSmsDeliverySuccessObserverTopic   = "sms-delivery-success";
const kSmsDeliveryErrorObserverTopic     = "sms-delivery-error";
const kMozSettingsChangedObserverTopic   = "mozsettings-changed";
const kSysMsgListenerReadyObserverTopic  = "system-message-listener-ready";
const kSysClockChangeObserverTopic       = "system-clock-change";
const kScreenStateChangedTopic           = "screen-state-changed";
const kTimeNitzAutomaticUpdateEnabled    = "time.nitz.automatic-update.enabled";
const kTimeNitzAvailable                 = "time.nitz.available";
const kCellBroadcastSearchList           = "ril.cellbroadcast.searchlist";
const kCellBroadcastDisabled             = "ril.cellbroadcast.disabled";
const kPrefenceChangedObserverTopic      = "nsPref:changed";

const DOM_MOBILE_MESSAGE_DELIVERY_RECEIVED = "received";
const DOM_MOBILE_MESSAGE_DELIVERY_SENDING  = "sending";
const DOM_MOBILE_MESSAGE_DELIVERY_SENT     = "sent";
const DOM_MOBILE_MESSAGE_DELIVERY_ERROR    = "error";

const CALL_WAKELOCK_TIMEOUT              = 5000;

const RIL_IPC_TELEPHONY_MSG_NAMES = [
  "RIL:EnumerateCalls",
  "RIL:GetMicrophoneMuted",
  "RIL:SetMicrophoneMuted",
  "RIL:GetSpeakerEnabled",
  "RIL:SetSpeakerEnabled",
  "RIL:StartTone",
  "RIL:StopTone",
  "RIL:Dial",
  "RIL:DialEmergency",
  "RIL:HangUp",
  "RIL:AnswerCall",
  "RIL:RejectCall",
  "RIL:HoldCall",
  "RIL:ResumeCall",
  "RIL:RegisterTelephonyMsg"
];

const RIL_IPC_MOBILECONNECTION_MSG_NAMES = [
  "RIL:GetRilContext",
  "RIL:GetAvailableNetworks",
  "RIL:SelectNetwork",
  "RIL:SelectNetworkAuto",
  "RIL:GetCardLock",
  "RIL:UnlockCardLock",
  "RIL:SetCardLock",
  "RIL:SendMMI",
  "RIL:CancelMMI",
  "RIL:RegisterMobileConnectionMsg",
  "RIL:SetCallForwardingOption",
  "RIL:GetCallForwardingOption",
  "RIL:SetCallBarringOption",
  "RIL:GetCallBarringOption",
  "RIL:SetCallWaitingOption",
  "RIL:GetCallWaitingOption"
];

const RIL_IPC_ICCMANAGER_MSG_NAMES = [
  "RIL:SendStkResponse",
  "RIL:SendStkMenuSelection",
  "RIL:SendStkTimerExpiration",
  "RIL:SendStkEventDownload",
  "RIL:IccOpenChannel",
  "RIL:IccExchangeAPDU",
  "RIL:IccCloseChannel",
  "RIL:ReadIccContacts",
  "RIL:UpdateIccContact",
  "RIL:RegisterIccMsg"
];

const RIL_IPC_VOICEMAIL_MSG_NAMES = [
  "RIL:RegisterVoicemailMsg",
  "RIL:GetVoicemailInfo"
];

const RIL_IPC_CELLBROADCAST_MSG_NAMES = [
  "RIL:RegisterCellBroadcastMsg"
];

XPCOMUtils.defineLazyServiceGetter(this, "gPowerManagerService",
                                   "@mozilla.org/power/powermanagerservice;1",
                                   "nsIPowerManagerService");

XPCOMUtils.defineLazyServiceGetter(this, "gMobileMessageService",
                                   "@mozilla.org/mobilemessage/mobilemessageservice;1",
                                   "nsIMobileMessageService");

XPCOMUtils.defineLazyServiceGetter(this, "gMobileMessageDatabaseService",
                                   "@mozilla.org/mobilemessage/rilmobilemessagedatabaseservice;1",
                                   "nsIRilMobileMessageDatabaseService");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageBroadcaster");

XPCOMUtils.defineLazyServiceGetter(this, "gSettingsService",
                                   "@mozilla.org/settingsService;1",
                                   "nsISettingsService");

XPCOMUtils.defineLazyServiceGetter(this, "gSystemMessenger",
                                   "@mozilla.org/system-message-internal;1",
                                   "nsISystemMessagesInternal");

XPCOMUtils.defineLazyServiceGetter(this, "gNetworkManager",
                                   "@mozilla.org/network/manager;1",
                                   "nsINetworkManager");

XPCOMUtils.defineLazyServiceGetter(this, "gTimeService",
                                   "@mozilla.org/time/timeservice;1",
                                   "nsITimeService");

XPCOMUtils.defineLazyServiceGetter(this, "gSystemWorkerManager",
                                   "@mozilla.org/telephony/system-worker-manager;1",
                                   "nsISystemWorkerManager");

XPCOMUtils.defineLazyGetter(this, "WAP", function () {
  let wap = {};
  Cu.import("resource://gre/modules/WapPushManager.js", wap);
  return wap;
});

XPCOMUtils.defineLazyGetter(this, "PhoneNumberUtils", function () {
  let ns = {};
  Cu.import("resource://gre/modules/PhoneNumberUtils.jsm", ns);
  return ns.PhoneNumberUtils;
});

function convertRILCallState(state) {
  switch (state) {
    case RIL.CALL_STATE_ACTIVE:
      return nsITelephonyProvider.CALL_STATE_CONNECTED;
    case RIL.CALL_STATE_HOLDING:
      return nsITelephonyProvider.CALL_STATE_HELD;
    case RIL.CALL_STATE_DIALING:
      return nsITelephonyProvider.CALL_STATE_DIALING;
    case RIL.CALL_STATE_ALERTING:
      return nsITelephonyProvider.CALL_STATE_ALERTING;
    case RIL.CALL_STATE_INCOMING:
    case RIL.CALL_STATE_WAITING:
      return nsITelephonyProvider.CALL_STATE_INCOMING;
    case RIL.CALL_STATE_BUSY:
      return nsITelephonyProvider.CALL_STATE_BUSY;
    default:
      throw new Error("Unknown rilCallState: " + state);
  }
}

/**
 * Fake nsIAudioManager implementation so that we can run the telephony
 * code in a non-Gonk build.
 */
let FakeAudioManager = {
  microphoneMuted: false,
  masterVolume: 1.0,
  masterMuted: false,
  phoneState: nsIAudioManager.PHONE_STATE_CURRENT,
  _forceForUse: {},
  setForceForUse: function setForceForUse(usage, force) {
    this._forceForUse[usage] = force;
  },
  getForceForUse: function setForceForUse(usage) {
    return this._forceForUse[usage] || nsIAudioManager.FORCE_NONE;
  }
};

XPCOMUtils.defineLazyGetter(this, "gAudioManager", function getAudioManager() {
  try {
    return Cc["@mozilla.org/telephony/audiomanager;1"]
             .getService(nsIAudioManager);
  } catch (ex) {
    //TODO on the phone this should not fall back as silently.
    debug("Using fake audio manager.");
    return FakeAudioManager;
  }
});


function RadioInterfaceLayer() {
  this.clientId = 0;

  this.dataNetworkInterface = new RILNetworkInterface(this, Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE);
  this.mmsNetworkInterface = new RILNetworkInterface(this, Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS);
  this.suplNetworkInterface = new RILNetworkInterface(this, Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_SUPL);

  debug("Starting RIL Worker");
  this.worker = new ChromeWorker("resource://gre/modules/ril_worker.js");
  this.worker.onerror = this.onerror.bind(this);
  this.worker.onmessage = this.onmessage.bind(this);

  let cellBroadcastDisabledPref = false;
  try {
    cellBroadcastDisabledPref =
      Services.prefs.getBoolPref(kCellBroadcastDisabled);
  } catch(e) {}
  // Pass initial options to ril_worker.
  this.worker.postMessage({
    rilMessageType: "setInitialOptions",
    debug: debugPref,
    clientId: this.clientId,
    cellBroadcastDisabled: cellBroadcastDisabledPref
  });

  this.rilContext = {
    radioState:     RIL.GECKO_RADIOSTATE_UNAVAILABLE,
    cardState:      RIL.GECKO_CARDSTATE_UNKNOWN,
    retryCount:     0,  // TODO: Please see bug 868896
    networkSelectionMode: RIL.GECKO_NETWORK_SELECTION_UNKNOWN,
    iccInfo:        null,
    imsi:           null,

    // These objects implement the nsIDOMMozMobileConnectionInfo interface,
    // although the actual implementation lives in the content process. So are
    // the child attributes `network` and `cell`, which implement
    // nsIDOMMozMobileNetworkInfo and nsIDOMMozMobileCellInfo respectively.
    voice:          {connected: false,
                     emergencyCallsOnly: false,
                     roaming: false,
                     network: null,
                     lastKnownMcc: null,
                     cell: null,
                     type: null,
                     signalStrength: null,
                     relSignalStrength: null},
    data:           {connected: false,
                     emergencyCallsOnly: false,
                     roaming: false,
                     network: null,
                     lastKnownMcc: null,
                     cell: null,
                     type: null,
                     signalStrength: null,
                     relSignalStrength: null},
  };

  try {
    this.rilContext.voice.lastKnownMcc =
      Services.prefs.getCharPref("ril.lastKnownMcc");
  } catch (e) {}

  this.voicemailInfo = {
    number: null,
    displayName: null
  };

  // Read the 'ril.radio.disabled' setting in order to start with a known
  // value at boot time.
  let lock = gSettingsService.createLock();
  lock.get("ril.radio.disabled", this);

  // Read preferred network type from the setting DB.
  lock.get("ril.radio.preferredNetworkType", this);

  // Read the APN data from the settings DB.
  lock.get("ril.data.apn", this);
  lock.get("ril.data.user", this);
  lock.get("ril.data.passwd", this);
  lock.get("ril.data.httpProxyHost", this);
  lock.get("ril.data.httpProxyPort", this);
  lock.get("ril.data.roaming_enabled", this);
  lock.get("ril.data.enabled", this);
  this._dataCallSettingsToRead = ["ril.data.enabled",
                                  "ril.data.roaming_enabled",
                                  "ril.data.apn",
                                  "ril.data.user",
                                  "ril.data.passwd",
                                  "ril.data.httpProxyHost",
                                  "ril.data.httpProxyPort"];

  // Read secondary APNs from the settings DB.
  lock.get("ril.mms.apn", this);
  lock.get("ril.mms.user", this);
  lock.get("ril.mms.passwd", this);
  lock.get("ril.mms.httpProxyHost", this);
  lock.get("ril.mms.httpProxyPort", this);
  lock.get("ril.mms.mmsc", this);
  lock.get("ril.mms.mmsproxy", this);
  lock.get("ril.mms.mmsport", this);
  lock.get("ril.supl.apn", this);
  lock.get("ril.supl.user", this);
  lock.get("ril.supl.passwd", this);
  lock.get("ril.supl.httpProxyHost", this);
  lock.get("ril.supl.httpProxyPort", this);

  // Read the 'time.nitz.automatic-update.enabled' setting to see if
  // we need to adjust the system clock time and time zone by NITZ.
  lock.get(kTimeNitzAutomaticUpdateEnabled, this);

  // Set "time.nitz.available" to false when starting up.
  this.setNitzAvailable(false);

  // Read the Cell Broadcast Search List setting, string of integers or integer
  // ranges separated by comma, to set listening channels.
  lock.get(kCellBroadcastSearchList, this);

  this._messageManagerByRequest = {};

  // Manage message targets in terms of topic. Only the authorized and
  // registered contents can receive related messages.
  this._messageManagerByTopic = {};
  this._topicList = [];

  ppmm.addMessageListener("child-process-shutdown", this);
  for (let msgname of RIL_IPC_TELEPHONY_MSG_NAMES) {
    ppmm.addMessageListener(msgname, this);
  }
  for (let msgname of RIL_IPC_MOBILECONNECTION_MSG_NAMES) {
    ppmm.addMessageListener(msgname, this);
  }
  for (let msgName of RIL_IPC_ICCMANAGER_MSG_NAMES) {
    ppmm.addMessageListener(msgName, this);
  }
  for (let msgname of RIL_IPC_VOICEMAIL_MSG_NAMES) {
    ppmm.addMessageListener(msgname, this);
  }
  for (let msgname of RIL_IPC_CELLBROADCAST_MSG_NAMES) {
    ppmm.addMessageListener(msgname, this);
  }
  Services.obs.addObserver(this, "xpcom-shutdown", false);
  Services.obs.addObserver(this, kMozSettingsChangedObserverTopic, false);
  Services.obs.addObserver(this, kSysMsgListenerReadyObserverTopic, false);
  Services.obs.addObserver(this, kSysClockChangeObserverTopic, false);
  Services.obs.addObserver(this, kScreenStateChangedTopic, false);

  Services.prefs.addObserver(kCellBroadcastDisabled, this, false);

  this._sentSmsEnvelopes = {};

  this.portAddressedSmsApps = {};
  this.portAddressedSmsApps[WAP.WDP_PORT_PUSH] = this.handleSmsWdpPortPush.bind(this);

  this._targetMessageQueue = [];

  gSystemWorkerManager.registerRilWorker(this.clientId, this.worker);
}
RadioInterfaceLayer.prototype = {

  classID:   RADIOINTERFACELAYER_CID,
  classInfo: XPCOMUtils.generateCI({classID: RADIOINTERFACELAYER_CID,
                                    classDescription: "RadioInterfaceLayer",
                                    interfaces: [Ci.nsIRadioInterfaceLayer]}),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIRadioInterfaceLayer,
                                         Ci.nsIObserver,
                                         Ci.nsISettingsServiceCallback]),

  /**
   * Process a message from the content process.
   */
  receiveMessage: function receiveMessage(msg) {
    debug("Received '" + msg.name + "' message from content process");
    if (msg.name == "child-process-shutdown") {
      // By the time we receive child-process-shutdown, the child process has
      // already forgotten its permissions so we need to unregister the target
      // for every permission.
      this.unregisterMessageTarget(null, msg.target);
      return;
    }

    if (RIL_IPC_TELEPHONY_MSG_NAMES.indexOf(msg.name) != -1) {
      if (!msg.target.assertPermission("telephony")) {
        debug("Telephony message " + msg.name +
              " from a content process with no 'telephony' privileges.");
        return null;
      }
    } else if (RIL_IPC_MOBILECONNECTION_MSG_NAMES.indexOf(msg.name) != -1) {
      if (!msg.target.assertPermission("mobileconnection")) {
        debug("MobileConnection message " + msg.name +
              " from a content process with no 'mobileconnection' privileges.");
        return null;
      }
    } else if (RIL_IPC_ICCMANAGER_MSG_NAMES.indexOf(msg.name) != -1) {
      if (!msg.target.assertPermission("mobileconnection")) {
        debug("IccManager message " + msg.name +
              " from a content process with no 'mobileconnection' privileges.");
        return null;
      }
    } else if (RIL_IPC_VOICEMAIL_MSG_NAMES.indexOf(msg.name) != -1) {
      if (!msg.target.assertPermission("voicemail")) {
        debug("Voicemail message " + msg.name +
              " from a content process with no 'voicemail' privileges.");
        return null;
      }
    } else if (RIL_IPC_CELLBROADCAST_MSG_NAMES.indexOf(msg.name) != -1) {
      if (!msg.target.assertPermission("cellbroadcast")) {
        debug("Cell Broadcast message " + msg.name +
              " from a content process with no 'cellbroadcast' privileges.");
        return null;
      }
    } else {
      debug("Ignoring unknown message type: " + msg.name);
      return null;
    }

    switch (msg.name) {
      case "RIL:GetRilContext":
        // This message is sync.
        return this.rilContext;
      case "RIL:EnumerateCalls":
        this.saveRequestTarget(msg);
        this.enumerateCalls(msg.json);
        break;
      case "RIL:GetMicrophoneMuted":
        // This message is sync.
        return this.microphoneMuted;
      case "RIL:SetMicrophoneMuted":
        this.microphoneMuted = msg.json;
        break;
      case "RIL:GetSpeakerEnabled":
        // This message is sync.
        return this.speakerEnabled;
      case "RIL:SetSpeakerEnabled":
        this.speakerEnabled = msg.json;
        break;
      case "RIL:StartTone":
        this.startTone(msg.json);
        break;
      case "RIL:StopTone":
        this.stopTone();
        break;
      case "RIL:Dial":
        this.dial(msg.json);
        break;
      case "RIL:DialEmergency":
        this.dialEmergency(msg.json);
        break;
      case "RIL:HangUp":
        this.hangUp(msg.json);
        break;
      case "RIL:AnswerCall":
        this.answerCall(msg.json);
        break;
      case "RIL:RejectCall":
        this.rejectCall(msg.json);
        break;
      case "RIL:HoldCall":
        this.holdCall(msg.json);
        break;
      case "RIL:ResumeCall":
        this.resumeCall(msg.json);
        break;
      case "RIL:RegisterTelephonyMsg":
        this.registerMessageTarget("telephony", msg.target);
        break;
      case "RIL:GetAvailableNetworks":
        this.saveRequestTarget(msg);
        this.getAvailableNetworks(msg.json.requestId);
        break;
      case "RIL:SelectNetwork":
        this.saveRequestTarget(msg);
        this.selectNetwork(msg.json);
        break;
      case "RIL:SelectNetworkAuto":
        this.saveRequestTarget(msg);
        this.selectNetworkAuto(msg.json.requestId);
        break;
      case "RIL:GetCardLock":
        this.saveRequestTarget(msg);
        this.getCardLock(msg.json);
        break;
      case "RIL:UnlockCardLock":
        this.saveRequestTarget(msg);
        this.unlockCardLock(msg.json);
        break;
      case "RIL:SetCardLock":
        this.saveRequestTarget(msg);
        this.setCardLock(msg.json);
        break;
      case "RIL:SendMMI":
        this.saveRequestTarget(msg);
        this.sendMMI(msg.json);
        break;
      case "RIL:CancelMMI":
        this.saveRequestTarget(msg);
        this.cancelMMI(msg.json);
        break;
      case "RIL:SendStkResponse":
        this.sendStkResponse(msg.json);
        break;
      case "RIL:SendStkMenuSelection":
        this.sendStkMenuSelection(msg.json);
        break;
      case "RIL:SendStkTimerExpiration":
        this.sendStkTimerExpiration(msg.json);
        break;
      case "RIL:SendStkEventDownload":
        this.sendStkEventDownload(msg.json);
        break;
      case "RIL:IccOpenChannel":
        this.saveRequestTarget(msg);
        this.iccOpenChannel(msg.json);
        break;
      case "RIL:IccCloseChannel":
        this.saveRequestTarget(msg);
        this.iccCloseChannel(msg.json);
        break;
      case "RIL:IccExchangeAPDU":
        this.saveRequestTarget(msg);
        this.iccExchangeAPDU(msg.json);
        break;
      case "RIL:ReadIccContacts":
        this.saveRequestTarget(msg);
        this.readIccContacts(msg.json);
        break;
      case "RIL:UpdateIccContact":
        this.saveRequestTarget(msg);
        this.updateIccContact(msg.json);
        break;
      case "RIL:RegisterMobileConnectionMsg":
        this.registerMessageTarget("mobileconnection", msg.target);
        break;
      case "RIL:RegisterIccMsg":
        this.registerMessageTarget("icc", msg.target);
        break;
      case "RIL:RegisterVoicemailMsg":
        this.registerMessageTarget("voicemail", msg.target);
        break;
      case "RIL:SetCallForwardingOption":
        this.saveRequestTarget(msg);
        this.setCallForwardingOption(msg.json);
        break;
      case "RIL:GetCallForwardingOption":
        this.saveRequestTarget(msg);
        this.getCallForwardingOption(msg.json);
        break;
      case "RIL:SetCallBarringOption":
        this.saveRequestTarget(msg);
        this.setCallBarringOption(msg.json);
        break;
      case "RIL:GetCallBarringOption":
        this.saveRequestTarget(msg);
        this.getCallBarringOption(msg.json);
        break;
      case "RIL:SetCallWaitingOption":
        this.saveRequestTarget(msg);
        this.setCallWaitingOption(msg.json);
        break;
      case "RIL:GetCallWaitingOption":
        this.saveRequestTarget(msg);
        this.getCallWaitingOption(msg.json);
        break;
      case "RIL:RegisterCellBroadcastMsg":
        this.registerMessageTarget("cellbroadcast", msg.target);
        break;
      case "RIL:GetVoicemailInfo":
        // This message is sync.
        return this.voicemailInfo;
    }
  },

  onerror: function onerror(event) {
    debug("Got an error: " + event.filename + ":" +
          event.lineno + ": " + event.message + "\n");
    event.preventDefault();
  },

  /**
   * Process the incoming message from the RIL worker. This roughly
   * works as follows:
   * (1) Update local state.
   * (2) Update state in related systems such as the audio.
   * (3) Multiplex the message to callbacks / listeners (typically the DOM).
   */
  onmessage: function onmessage(event) {
    let message = event.data;
    debug("Received message from worker: " + JSON.stringify(message));
    switch (message.rilMessageType) {
      case "callRing":
        this.handleCallRing();
        break;
      case "callStateChange":
        // This one will handle its own notifications.
        this.handleCallStateChange(message.call);
        break;
      case "callDisconnected":
        // This one will handle its own notifications.
        this.handleCallDisconnected(message.call);
        break;
      case "enumerateCalls":
        // This one will handle its own notifications.
        this.handleEnumerateCalls(message);
        break;
      case "callError":
        this.handleCallError(message);
        break;
      case "iccOpenChannel":
        this.handleIccOpenChannel(message);
        break;
      case "iccCloseChannel":
        this.handleIccCloseChannel(message);
        break;
      case "iccExchangeAPDU":
        this.handleIccExchangeAPDU(message);
        break;
      case "getAvailableNetworks":
        this.handleGetAvailableNetworks(message);
        break;
      case "selectNetwork":
        this.handleSelectNetwork(message);
        break;
      case "selectNetworkAuto":
        this.handleSelectNetworkAuto(message);
        break;
      case "networkinfochanged":
        this.updateNetworkInfo(message);
        break;
      case "networkselectionmodechange":
        this.updateNetworkSelectionMode(message);
        break;
      case "voiceregistrationstatechange":
        this.updateVoiceConnection(message);
        break;
      case "dataregistrationstatechange":
        this.updateDataConnection(message);
        break;
      case "datacallerror":
        this.handleDataCallError(message);
        break;
      case "signalstrengthchange":
        this.handleSignalStrengthChange(message);
        break;
      case "operatorchange":
        this.handleOperatorChange(message);
        break;
      case "radiostatechange":
        this.handleRadioStateChange(message);
        break;
      case "cardstatechange":
        this.rilContext.cardState = message.cardState;
        this._sendMobileConnectionMessage("RIL:CardStateChanged", message);
        break;
      case "sms-received":
        let ackOk = this.handleSmsReceived(message);
        if (ackOk) {
          this.worker.postMessage({
            rilMessageType: "ackSMS",
            result: RIL.PDU_FCS_OK
          });
        }
        return;
      case "sms-sent":
        this.handleSmsSent(message);
        return;
      case "sms-delivery":
        this.handleSmsDelivery(message);
        return;
      case "sms-send-failed":
        this.handleSmsSendFailed(message);
        return;
      case "cellbroadcast-received":
        message.timestamp = Date.now();
        this._sendCellBroadcastMessage("RIL:CellBroadcastReceived", message);
        break;
      case "datacallstatechange":
        this.handleDataCallState(message);
        break;
      case "datacalllist":
        this.handleDataCallList(message);
        break;
      case "nitzTime":
        this.handleNitzTime(message);
        break;
      case "iccinfochange":
        this.handleIccInfoChange(message);
        break;
      case "iccimsi":
        this.rilContext.imsi = message.imsi;
        break;
      case "iccGetCardLock":
      case "iccSetCardLock":
      case "iccUnlockCardLock":
        this.handleIccCardLockResult(message);
        break;
      case "icccontacts":
        this.handleReadIccContacts(message);
        break;
      case "icccontactupdate":
        this.handleUpdateIccContact(message);
        break;
      case "iccmbdn":
        this.handleIccMbdn(message);
        break;
      case "USSDReceived":
        debug("USSDReceived " + JSON.stringify(message));
        this.handleUSSDReceived(message);
        break;
      case "sendMMI":
      case "sendUSSD":
        this.handleSendMMI(message);
        break;
      case "cancelMMI":
      case "cancelUSSD":
        this.handleCancelMMI(message);
        break;
      case "stkcommand":
        this.handleStkProactiveCommand(message);
        break;
      case "stksessionend":
        this._sendIccMessage("RIL:StkSessionEnd", null);
        break;
      case "setPreferredNetworkType":
        this.handleSetPreferredNetworkType(message);
        break;
      case "queryCallForwardStatus":
        this.handleQueryCallForwardStatus(message);
        break;
      case "setCallForward":
        this.handleSetCallForward(message);
        break;
      case "queryCallBarringStatus":
        this.handleQueryCallBarringStatus(message);
        break;
      case "setCallBarring":
        this.handleSetCallBarring(message);
        break;
      case "queryCallWaiting":
        this.handleQueryCallWaiting(message);
        break;
      case "setCallWaiting":
        this.handleSetCallWaiting(message);
        break;
      case "setCellBroadcastSearchList":
        this.handleSetCellBroadcastSearchList(message);
        break;
      case "setRadioEnabled":
        let lock = gSettingsService.createLock();
        lock.set("ril.radio.disabled", !message.on, null, null);
        break;
      default:
        throw new Error("Don't know about this message type: " +
                        message.rilMessageType);
    }
  },

  _messageManagerByRequest: null,
  saveRequestTarget: function saveRequestTarget(msg) {
    let requestId = msg.json.requestId;
    if (!requestId) {
      // The content is not interested in a response;
      return;
    }

    this._messageManagerByRequest[requestId] = msg.target;
  },

  _sendRequestResults: function _sendRequestResults(requestType, options) {
    let target = this._messageManagerByRequest[options.requestId];
    delete this._messageManagerByRequest[options.requestId];

    if (!target) {
      return;
    }

    target.sendAsyncMessage(requestType, options);
  },

  _messageManagerByTopic: null,
  _topicList: null,
  registerMessageTarget: function registerMessageTarget(topic, target) {
    let targets = this._messageManagerByTopic[topic];
    if (!targets) {
      targets = this._messageManagerByTopic[topic] = [];
      let list = this._topicList;
      if (list.indexOf(topic) == -1) {
        list.push(topic);
      }
    }

    if (targets.indexOf(target) != -1) {
      debug("Already registered this target!");
      return;
    }

    targets.push(target);
    debug("Registered " + topic + " target: " + target);
  },

  unregisterMessageTarget: function unregisterMessageTarget(topic, target) {
    if (topic == null) {
      // Unregister the target for every topic when no topic is specified.
      for (let type of this._topicList) {
        this.unregisterMessageTarget(type, target);
      }
      return;
    }

    // Unregister the target for a specified topic.
    let targets = this._messageManagerByTopic[topic];
    if (!targets) {
      return;
    }

    let index = targets.indexOf(target);
    if (index != -1) {
      targets.splice(index, 1);
      debug("Unregistered " + topic + " target: " + target);
    }
  },

  _sendTargetMessage: function _sendTargetMessage(topic, message, options) {

    if (!this._sysMsgListenerReady) {
      this._enqueueTargetMessage(topic, message, options);
      return;
    }

    let targets = this._messageManagerByTopic[topic];
    if (!targets) {
      return;
    }

    for (let target of targets) {
      target.sendAsyncMessage(message, options);
    }
  },

  _sendTelephonyMessage: function sendTelephonyMessage(message, options) {
    this._sendTargetMessage("telephony", message, options);
  },

  _sendMobileConnectionMessage: function sendMobileConnectionMessage(message, options) {
    this._sendTargetMessage("mobileconnection", message, options);
  },

  _sendVoicemailMessage: function sendVoicemailMessage(message, options) {
    this._sendTargetMessage("voicemail", message, options);
  },

  _sendCellBroadcastMessage: function sendCellBroadcastMessage(message, options) {
    this._sendTargetMessage("cellbroadcast", message, options);
  },

  _sendIccMessage: function sendIccMessage(message, options) {
    this._sendTargetMessage("icc", message, options);
  },

  updateNetworkInfo: function updateNetworkInfo(message) {
    let voiceMessage = message[RIL.NETWORK_INFO_VOICE_REGISTRATION_STATE];
    let dataMessage = message[RIL.NETWORK_INFO_DATA_REGISTRATION_STATE];
    let operatorMessage = message[RIL.NETWORK_INFO_OPERATOR];
    let selectionMessage = message[RIL.NETWORK_INFO_NETWORK_SELECTION_MODE];

    // Batch the *InfoChanged messages together
    if (voiceMessage) {
      voiceMessage.batch = true;
      this.updateVoiceConnection(voiceMessage);
    }

    if (dataMessage) {
      dataMessage.batch = true;
      this.updateDataConnection(dataMessage);
    }

    if (operatorMessage) {
      operatorMessage.batch = true;
      this.handleOperatorChange(operatorMessage);
    }

    let voice = this.rilContext.voice;
    let data = this.rilContext.data;

    this.checkRoamingBetweenOperators(voice);
    this.checkRoamingBetweenOperators(data);

    if (voiceMessage || operatorMessage) {
      this._sendMobileConnectionMessage("RIL:VoiceInfoChanged", voice);
    }
    if (dataMessage || operatorMessage) {
      this._sendMobileConnectionMessage("RIL:DataInfoChanged", data);
    }

    if (selectionMessage) {
      this.updateNetworkSelectionMode(selectionMessage);
    }
  },

  /**
    * Fix the roaming. RIL can report roaming in some case it is not
    * really the case. See bug 787967
    *
    * @param registration  The voiceMessage or dataMessage from which the
    *                      roaming state will be changed (maybe, if needed).
    */
  checkRoamingBetweenOperators: function checkRoamingBetweenOperators(registration) {
    let iccInfo = this.rilContext.iccInfo;
    if (!iccInfo || !registration.connected) {
      return;
    }

    let spn = iccInfo.spn && iccInfo.spn.toLowerCase();
    let operator = registration.network;
    let longName = operator.longName && operator.longName.toLowerCase();
    let shortName = operator.shortName && operator.shortName.toLowerCase();

    let equalsLongName = longName && (spn == longName);
    let equalsShortName = shortName && (spn == shortName);
    let equalsMcc = iccInfo.mcc == operator.mcc;

    registration.roaming = registration.roaming &&
                           !(equalsMcc && (equalsLongName || equalsShortName));
  },

  /**
   * Sends the RIL:VoiceInfoChanged message when the voice
   * connection's state has changed.
   *
   * @param newInfo The new voice connection information. When newInfo.batch is true,
   *                the RIL:VoiceInfoChanged message will not be sent.
   */
  updateVoiceConnection: function updateVoiceConnection(newInfo) {
    let voiceInfo = this.rilContext.voice;
    voiceInfo.state = newInfo.state;
    voiceInfo.connected = newInfo.connected;
    voiceInfo.roaming = newInfo.roaming;
    voiceInfo.emergencyCallsOnly = newInfo.emergencyCallsOnly;
    voiceInfo.type = newInfo.type;

    // Make sure we also reset the operator and signal strength information
    // if we drop off the network.
    if (newInfo.regState == RIL.NETWORK_CREG_STATE_UNKNOWN) {
      voiceInfo.network = null;
      voiceInfo.signalStrength = null;
      voiceInfo.relSignalStrength = null;
    }

    let newCell = newInfo.cell;
    if ((newCell.gsmLocationAreaCode < 0) || (newCell.gsmCellId < 0)) {
      voiceInfo.cell = null;
    } else {
      voiceInfo.cell = newCell;
    }

    if (!newInfo.batch) {
      this._sendMobileConnectionMessage("RIL:VoiceInfoChanged", voiceInfo);
    }
  },

  updateDataConnection: function updateDataConnection(newInfo) {
    let dataInfo = this.rilContext.data;
    dataInfo.state = newInfo.state;
    dataInfo.roaming = newInfo.roaming;
    dataInfo.emergencyCallsOnly = newInfo.emergencyCallsOnly;
    dataInfo.type = newInfo.type;
    // For the data connection, the `connected` flag indicates whether
    // there's an active data call.
    dataInfo.connected = this.dataNetworkInterface.connected;

    // Make sure we also reset the operator and signal strength information
    // if we drop off the network.
    if (newInfo.regState == RIL.NETWORK_CREG_STATE_UNKNOWN) {
      dataInfo.network = null;
      dataInfo.signalStrength = null;
      dataInfo.relSignalStrength = null;
    }

    let newCell = newInfo.cell;
    if ((newCell.gsmLocationAreaCode < 0) || (newCell.gsmCellId < 0)) {
      dataInfo.cell = null;
    } else {
      dataInfo.cell = newCell;
    }

    if (!newInfo.batch) {
      this._sendMobileConnectionMessage("RIL:DataInfoChanged", dataInfo);
    }
    this.updateRILNetworkInterface();
  },

  /**
   * Handle data errors
   */
  handleDataCallError: function handleDataCallError(message) {
    // Notify data call error only for data APN
    if (message.apn == this.dataCallSettings.apn) {
      this._sendMobileConnectionMessage("RIL:DataError", message);
    }

    this._deliverDataCallCallback("dataCallError", [message]);
  },

  _preferredNetworkType: null,
  setPreferredNetworkType: function setPreferredNetworkType(value) {
    let networkType = RIL.RIL_PREFERRED_NETWORK_TYPE_TO_GECKO.indexOf(value);
    if (networkType < 0) {
      networkType = (this._preferredNetworkType != null)
                    ? RIL.RIL_PREFERRED_NETWORK_TYPE_TO_GECKO[this._preferredNetworkType]
                    : RIL.GECKO_PREFERRED_NETWORK_TYPE_DEFAULT;
      gSettingsService.createLock().set("ril.radio.preferredNetworkType",
                                        networkType, null);
      return;
    }

    if (networkType == this._preferredNetworkType) {
      return;
    }

    this.worker.postMessage({rilMessageType: "setPreferredNetworkType",
                             networkType: networkType});
  },

  handleSetPreferredNetworkType: function handleSetPreferredNetworkType(message) {
    if ((this._preferredNetworkType != null) && !message.success) {
      gSettingsService.createLock().set("ril.radio.preferredNetworkType",
                                        this._preferredNetworkType,
                                        null);
      return;
    }

    this._preferredNetworkType = message.networkType;
    debug("_preferredNetworkType is now " +
          RIL.RIL_PREFERRED_NETWORK_TYPE_TO_GECKO[this._preferredNetworkType]);
  },

  setCellBroadcastSearchList: function setCellBroadcastSearchList(newSearchListStr) {
    if (newSearchListStr == this._cellBroadcastSearchListStr) {
      return;
    }

    this.worker.postMessage({
      rilMessageType: "setCellBroadcastSearchList",
      searchListStr: newSearchListStr
    });
  },

  handleSetCellBroadcastSearchList: function handleSetCellBroadcastSearchList(message) {
    if (message.rilRequestError != RIL.ERROR_SUCCESS) {
      let lock = gSettingsService.createLock();
      lock.set(kCellBroadcastSearchList, this._cellBroadcastSearchListStr, null);
      return;
    }

    this._cellBroadcastSearchListStr = message.searchListStr;
  },

  handleSignalStrengthChange: function handleSignalStrengthChange(message) {
    let voiceInfo = this.rilContext.voice;
    // TODO CDMA, EVDO, LTE, etc. (see bug 726098)
    if (voiceInfo.signalStrength != message.gsmDBM ||
        voiceInfo.relSignalStrength != message.gsmRelative) {
      voiceInfo.signalStrength = message.gsmDBM;
      voiceInfo.relSignalStrength = message.gsmRelative;
      this._sendMobileConnectionMessage("RIL:VoiceInfoChanged", voiceInfo);
    }

    let dataInfo = this.rilContext.data;
    if (dataInfo.signalStrength != message.gsmDBM ||
        dataInfo.relSignalStrength != message.gsmRelative) {
      dataInfo.signalStrength = message.gsmDBM;
      dataInfo.relSignalStrength = message.gsmRelative;
      this._sendMobileConnectionMessage("RIL:DataInfoChanged", dataInfo);
    }
  },

  networkChanged: function networkChanged(srcNetwork, destNetwork) {
    return !destNetwork ||
      destNetwork.longName != srcNetwork.longName ||
      destNetwork.shortName != srcNetwork.shortName ||
      destNetwork.mnc != srcNetwork.mnc ||
      destNetwork.mcc != srcNetwork.mcc;
  },

  handleOperatorChange: function handleOperatorChange(message) {
    let voice = this.rilContext.voice;
    let data = this.rilContext.data;

    if (this.networkChanged(message, voice.network)) {
      // Update lastKnownMcc.
      if (message.mcc) {
        voice.lastKnownMcc = message.mcc;
        // Update pref if mcc is changed.
        // !voice.network is in case voice.network is still null.
        if (!voice.network || voice.network.mcc != message.mcc) {
          try {
            Services.prefs.setCharPref("ril.lastKnownMcc", message.mcc);
          } catch (e) {}
        }
      }

      // Update lastKnownNetwork
      if (message.mcc && message.mnc) {
        try {
          Services.prefs.setCharPref("ril.lastKnownNetwork",
                                     message.mcc + "-" + message.mnc);
        } catch (e) {}
      }

      voice.network = message;
      if (!message.batch) {
        this._sendMobileConnectionMessage("RIL:VoiceInfoChanged", voice);
      }
    }

    if (this.networkChanged(message, data.network)) {
      data.network = message;
      if (!message.batch) {
        this._sendMobileConnectionMessage("RIL:DataInfoChanged", data);
      }
    }
  },

  handleRadioStateChange: function handleRadioStateChange(message) {
    this._changingRadioPower = false;

    let newState = message.radioState;
    if (this.rilContext.radioState == newState) {
      return;
    }
    this.rilContext.radioState = newState;
    //TODO Should we notify this change as a card state change?

    this._ensureRadioState();
  },

  _enqueueTargetMessage: function _enqueueTargetMessage(topic, message, options) {
    let msg = { topic : topic,
                message : message,
                options : options };
    // Remove previous queued message of same message type, only one message
    // per message type is allowed in queue.
    let messageQueue = this._targetMessageQueue;
    for(let i = 0; i < messageQueue.length; i++) {
      if (messageQueue[i].message === message) {
        messageQueue.splice(i, 1);
        break;
      }
    }

    messageQueue.push(msg);
  },

  _resendQueuedTargetMessage: function _resendQueuedTargetMessage() {
    // Here uses this._sendTargetMessage() to resend message, which will
    // enqueue message if listener is not ready.
    // So only resend after listener is ready, or it will cause infinate loop and
    // hang the system.

    // Dequeue and resend messages.
    for (let msg of this._targetMessageQueue) {
      this._sendTargetMessage(msg.topic, msg.message, msg.options);
    }
    this._targetMessageQueue = null;
  },

  _ensureRadioState: function _ensureRadioState() {
    debug("Reported radio state is " + this.rilContext.radioState +
          ", desired radio enabled state is " + this._radioEnabled);
    if (this._radioEnabled == null) {
      // We haven't read the initial value from the settings DB yet.
      // Wait for that.
      return;
    }
    if (!this._sysMsgListenerReady) {
      // The UI's system app isn't ready yet for us to receive any
      // events (e.g. incoming SMS, etc.). Wait for that.
      return;
    }
    if (this.rilContext.radioState == RIL.GECKO_RADIOSTATE_UNKNOWN) {
      // We haven't received a radio state notification from the RIL
      // yet. Wait for that.
      return;
    }
    if (this._changingRadioPower) {
      // We're changing the radio power currently, ignore any changes.
      return;
    }

    if (this.rilContext.radioState == RIL.GECKO_RADIOSTATE_OFF &&
        this._radioEnabled) {
      this.setRadioEnabled(true);
    }
    if (this.rilContext.radioState == RIL.GECKO_RADIOSTATE_READY &&
        !this._radioEnabled) {
      this.setRadioEnabled(false);
    }
  },

  updateRILNetworkInterface: function updateRILNetworkInterface() {
    if (this._dataCallSettingsToRead.length) {
      debug("We haven't read completely the APN data from the " +
            "settings DB yet. Wait for that.");
      return;
    }

    // This check avoids data call connection if the radio is not ready
    // yet after toggling off airplane mode.
    if (this.rilContext.radioState != RIL.GECKO_RADIOSTATE_READY) {
      debug("RIL is not ready for data connection: radio's not ready");
      return;
    }

    // We only watch at "ril.data.enabled" flag changes for connecting or
    // disconnecting the data call. If the value of "ril.data.enabled" is
    // true and any of the remaining flags change the setting application
    // should turn this flag to false and then to true in order to reload
    // the new values and reconnect the data call.
    if (this._oldRilDataEnabledState == this.dataCallSettings.enabled) {
      debug("No changes for ril.data.enabled flag. Nothing to do.");
      return;
    }

    if (this.dataNetworkInterface.state == RIL.GECKO_NETWORK_STATE_CONNECTING ||
        this.dataNetworkInterface.state == RIL.GECKO_NETWORK_STATE_DISCONNECTING) {
      debug("Nothing to do during connecting/disconnecting in progress.");
      return;
    }

    let dataInfo = this.rilContext.data;
    let isRegistered =
      dataInfo.state == RIL.GECKO_MOBILE_CONNECTION_STATE_REGISTERED;
    let haveDataConnection =
      dataInfo.type != RIL.GECKO_MOBILE_CONNECTION_STATE_UNKNOWN;
    if (!isRegistered || !haveDataConnection) {
      debug("RIL is not ready for data connection: Phone's not registered " +
            "or doesn't have data connection.");
      return;
    }
    let wifi_active = false;
    if (gNetworkManager.active &&
        gNetworkManager.active.type == Ci.nsINetworkInterface.NETWORK_TYPE_WIFI) {
      wifi_active = true;
    }

    if (this.dataNetworkInterface.connected &&
        (!this.dataCallSettings.enabled ||
         (dataInfo.roaming && !this.dataCallSettings.roaming_enabled) ||
         (wifi_active && this.shareDefaultAPNCounter === 0))) {
      debug("Data call settings: disconnect data call.");
      this.dataNetworkInterface.disconnect();
      return;
    }
    if (!this.dataCallSettings.enabled || this.dataNetworkInterface.connected) {
      debug("Data call settings: nothing to do.");
      return;
    }
    if (dataInfo.roaming && !this.dataCallSettings.roaming_enabled) {
      debug("We're roaming, but data roaming is disabled.");
      return;
    }
    if (wifi_active && this.shareDefaultAPNCounter === 0) {
      debug("Don't connect data call when Wifi is connected.");
      return;
    }
    if (this._changingRadioPower) {
      // We're changing the radio power currently, ignore any changes.
      return;
    }

    debug("Data call settings: connect data call.");
    this.dataNetworkInterface.connect(this.dataCallSettings);
  },

  /**
   * Track the active call and update the audio system as its state changes.
   */
  _activeCall: null,
  updateCallAudioState: function updateCallAudioState(call) {
    switch (call.state) {
      case nsITelephonyProvider.CALL_STATE_DIALING: // Fall through...
      case nsITelephonyProvider.CALL_STATE_ALERTING:
      case nsITelephonyProvider.CALL_STATE_CONNECTED:
        call.isActive = true;
        this._activeCall = call;
        gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_IN_CALL;
        if (this.speakerEnabled) {
          gAudioManager.setForceForUse(nsIAudioManager.USE_COMMUNICATION,
                                       nsIAudioManager.FORCE_SPEAKER);
        }
        debug("Active call, put audio system into PHONE_STATE_IN_CALL: "
              + gAudioManager.phoneState);
        break;
      case nsITelephonyProvider.CALL_STATE_INCOMING:
        call.isActive = false;
        if (!this._activeCall) {
          // We can change the phone state into RINGTONE only when there's
          // no active call.
          gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_RINGTONE;
          debug("Incoming call, put audio system into PHONE_STATE_RINGTONE: "
                + gAudioManager.phoneState);
        }
        break;
      case nsITelephonyProvider.CALL_STATE_HELD: // Fall through...
      case nsITelephonyProvider.CALL_STATE_DISCONNECTED:
        call.isActive = false;
        if (this._activeCall &&
            this._activeCall.callIndex == call.callIndex) {
          // Previously active call is not active now.
          this._activeCall = null;
        }

        if (!this._activeCall) {
          // No active call. Disable the audio.
          gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_NORMAL;
          debug("No active call, put audio system into PHONE_STATE_NORMAL: "
                + gAudioManager.phoneState);
        }
        break;
    }
  },

  _callRingWakeLock: null,
  _callRingWakeLockTimer: null,
  _cancelCallRingWakeLockTimer: function _cancelCallRingWakeLockTimer() {
    if (this._callRingWakeLockTimer) {
      this._callRingWakeLockTimer.cancel();
    }
    if (this._callRingWakeLock) {
      this._callRingWakeLock.unlock();
      this._callRingWakeLock = null;
    }
  },

  /**
   * Handle an incoming call.
   *
   * Not much is known about this call at this point, but it's enough
   * to start bringing up the Phone app already.
   */
  handleCallRing: function handleCallRing() {
    if (!this._callRingWakeLock) {
      this._callRingWakeLock = gPowerManagerService.newWakeLock("cpu");
    }
    if (!this._callRingWakeLockTimer) {
      this._callRingWakeLockTimer =
        Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    }
    this._callRingWakeLockTimer
        .initWithCallback(this._cancelCallRingWakeLockTimer.bind(this),
                          CALL_WAKELOCK_TIMEOUT, Ci.nsITimer.TYPE_ONE_SHOT);

    gSystemMessenger.broadcastMessage("telephony-new-call", {});
  },

  /**
   * Handle call state changes by updating our current state and the audio
   * system.
   */
  handleCallStateChange: function handleCallStateChange(call) {
    debug("handleCallStateChange: " + JSON.stringify(call));
    call.state = convertRILCallState(call.state);

    if (call.state == nsITelephonyProvider.CALL_STATE_DIALING) {
      gSystemMessenger.broadcastMessage("telephony-new-call", {});
    }
    this.updateCallAudioState(call);
    this._sendTelephonyMessage("RIL:CallStateChanged", call);
  },

  /**
   * Handle call disconnects by updating our current state and the audio system.
   */
  handleCallDisconnected: function handleCallDisconnected(call) {
    debug("handleCallDisconnected: " + JSON.stringify(call));
    call.state = nsITelephonyProvider.CALL_STATE_DISCONNECTED;
    let duration = ("started" in call && typeof call.started == "number") ?
      new Date().getTime() - call.started : 0;
    let data = {
      number: call.number,
      duration: duration,
      direction: call.isOutgoing ? "outgoing" : "incoming"
    };
    gSystemMessenger.broadcastMessage("telephony-call-ended", data);
    this.updateCallAudioState(call);
    this._sendTelephonyMessage("RIL:CallStateChanged", call);
  },

  /**
   * Handle calls delivered in response to a 'enumerateCalls' request.
   */
  handleEnumerateCalls: function handleEnumerateCalls(options) {
    debug("handleEnumerateCalls: " + JSON.stringify(options));
    for (let i in options.calls) {
      options.calls[i].state = convertRILCallState(options.calls[i].state);
      options.calls[i].isActive = this._activeCall ?
        options.calls[i].callIndex == this._activeCall.callIndex : false;
    }
    this._sendRequestResults("RIL:EnumerateCalls", options);
  },

  handleReadIccContacts: function handleReadIccContacts(message) {
    debug("handleReadIccContacts: " + JSON.stringify(message));
    this._sendRequestResults("RIL:ReadIccContacts", message);
  },

  handleUpdateIccContact: function handleUpdateIccContact(message) {
    debug("handleUpdateIccContact: " + JSON.stringify(message));
    this._sendRequestResults("RIL:UpdateIccContact", message);
  },

  /**
   * Open Logical UICC channel (aid) for Secure Element access
   */
  handleIccOpenChannel: function handleIccOpenChannel(message) {
    debug("handleIccOpenChannel: " + JSON.stringify(message));
    this._sendRequestResults("RIL:IccOpenChannel", message);
  },

  /**
   * Close Logical UICC channel
   */
  handleIccCloseChannel: function handleIccCloseChannel(message) {
    debug("handleIccCloseChannel: " + JSON.stringify(message));
    this._sendRequestResults("RIL:IccCloseChannel", message);
  },

  /**
   * Exchange APDU data on an open Logical UICC channel
   */
  handleIccExchangeAPDU: function handleIccExchangeAPDU(message) {
    debug("handleIccExchangeAPDU: " + JSON.stringify(message));
    this._sendRequestResults("RIL:IccExchangeAPDU", message);
  },

  /**
   * Handle available networks returned by the 'getAvailableNetworks' request.
   */
  handleGetAvailableNetworks: function handleGetAvailableNetworks(message) {
    debug("handleGetAvailableNetworks: " + JSON.stringify(message));

    this._sendRequestResults("RIL:GetAvailableNetworks", message);
  },

  /**
   * Update network selection mode
   */
  updateNetworkSelectionMode: function updateNetworkSelectionMode(message) {
    debug("updateNetworkSelectionMode: " + JSON.stringify(message));
    this.rilContext.networkSelectionMode = message.mode;
    this._sendMobileConnectionMessage("RIL:NetworkSelectionModeChanged", message);
  },

  /**
   * Handle "manual" network selection request.
   */
  handleSelectNetwork: function handleSelectNetwork(message) {
    debug("handleSelectNetwork: " + JSON.stringify(message));
    this._sendRequestResults("RIL:SelectNetwork", message);
  },

  /**
   * Handle "automatic" network selection request.
   */
  handleSelectNetworkAuto: function handleSelectNetworkAuto(message) {
    debug("handleSelectNetworkAuto: " + JSON.stringify(message));
    this._sendRequestResults("RIL:SelectNetworkAuto", message);
  },

  /**
   * Handle call error.
   */
  handleCallError: function handleCallError(message) {
    this._sendTelephonyMessage("RIL:CallError", message);
  },

  /**
   * Handle WDP port push PDU. Constructor WDP bearer information and deliver
   * to WapPushManager.
   *
   * @param message
   *        A SMS message.
   */
  handleSmsWdpPortPush: function handleSmsWdpPortPush(message) {
    if (message.encoding != RIL.PDU_DCS_MSG_CODING_8BITS_ALPHABET) {
      debug("Got port addressed SMS but not encoded in 8-bit alphabet. Drop!");
      return;
    }

    let options = {
      bearer: WAP.WDP_BEARER_GSM_SMS_GSM_MSISDN,
      sourceAddress: message.sender,
      sourcePort: message.header.originatorPort,
      destinationAddress: this.rilContext.iccInfo.msisdn,
      destinationPort: message.header.destinationPort,
    };
    WAP.WapPushManager.receiveWdpPDU(message.fullData, message.fullData.length,
                                     0, options);
  },

  /**
   * A helper to broadcast the system message to launch registered apps
   * like Costcontrol, Notification and Message app... etc.
   *
   * @param aName
   *        The system message name.
   * @param aDomMessage
   *        The nsIDOMMozSmsMessage object.
   */
  broadcastSmsSystemMessage: function broadcastSmsSystemMessage(aName, aDomMessage) {
    debug("Broadcasting the SMS system message: " + aName);

    // Sadly we cannot directly broadcast the aDomMessage object
    // because the system message mechamism will rewrap the object
    // based on the content window, which needs to know the properties.
    gSystemMessenger.broadcastMessage(aName, {
      type:           aDomMessage.type,
      id:             aDomMessage.id,
      threadId:       aDomMessage.threadId,
      delivery:       aDomMessage.delivery,
      deliveryStatus: aDomMessage.deliveryStatus,
      sender:         aDomMessage.sender,
      receiver:       aDomMessage.receiver,
      body:           aDomMessage.body,
      messageClass:   aDomMessage.messageClass,
      timestamp:      aDomMessage.timestamp,
      read:           aDomMessage.read
    });
  },

  portAddressedSmsApps: null,
  handleSmsReceived: function handleSmsReceived(message) {
    debug("handleSmsReceived: " + JSON.stringify(message));

    // FIXME: Bug 737202 - Typed arrays become normal arrays when sent to/from workers
    if (message.encoding == RIL.PDU_DCS_MSG_CODING_8BITS_ALPHABET) {
      message.fullData = new Uint8Array(message.fullData);
    }

    // Dispatch to registered handler if application port addressing is
    // available. Note that the destination port can possibly be zero when
    // representing a UDP/TCP port.
    if (message.header && message.header.destinationPort != null) {
      let handler = this.portAddressedSmsApps[message.header.destinationPort];
      if (handler) {
        handler(message);
      }
      return true;
    }

    if (message.encoding == RIL.PDU_DCS_MSG_CODING_8BITS_ALPHABET) {
      // Don't know how to handle binary data yet.
      return true;
    }

    message.type = "sms";
    message.sender = message.sender || null;
    message.receiver = message.receiver || null;
    message.body = message.fullBody = message.fullBody || null;
    message.timestamp = Date.now();

    // TODO: Bug #768441
    // For now we don't store indicators persistently. When the mwi.discard
    // flag is false, we'll need to persist the indicator to EFmwis.
    // See TS 23.040 9.2.3.24.2

    let mwi = message.mwi;
    if (mwi) {
      mwi.returnNumber = message.sender;
      mwi.returnMessage = message.fullBody;
      this._sendVoicemailMessage("RIL:VoicemailNotification", mwi);
      return true;
    }

    let notifyReceived = function notifyReceived(rv, domMessage) {
      let success = Components.isSuccessCode(rv);

      // Acknowledge the reception of the SMS.
      message.rilMessageType = "ackSMS";
      if (!success) {
        message.result = RIL.PDU_FCS_MEMORY_CAPACITY_EXCEEDED;
      }
      this.worker.postMessage(message);

      if (!success) {
        // At this point we could send a message to content to notify the user
        // that storing an incoming SMS failed, most likely due to a full disk.
        debug("Could not store SMS " + message.id + ", error code " + rv);
        return;
      }

      this.broadcastSmsSystemMessage("sms-received", domMessage);
      Services.obs.notifyObservers(domMessage, kSmsReceivedObserverTopic, null);
    }.bind(this);

    if (message.messageClass != RIL.GECKO_SMS_MESSAGE_CLASSES[RIL.PDU_DCS_MSG_CLASS_0]) {
      message.id = gMobileMessageDatabaseService.saveReceivedMessage(message,
                                                                     notifyReceived);
    } else {
      message.id = -1;
      message.threadId = 0;
      message.delivery = DOM_MOBILE_MESSAGE_DELIVERY_RECEIVED;
      message.deliveryStatus = RIL.GECKO_SMS_DELIVERY_STATUS_SUCCESS;
      message.read = false;

      let domMessage =
        gMobileMessageService.createSmsMessage(message.id,
                                               message.threadId,
                                               message.delivery,
                                               message.deliveryStatus,
                                               message.sender,
                                               message.receiver,
                                               message.body,
                                               message.messageClass,
                                               message.timestamp,
                                               message.read);

      notifyReceived(Cr.NS_OK, domMessage);
    }

    // SMS ACK will be sent in notifyReceived. Return false here.
    return false;
  },

  /**
   * Local storage for sent SMS messages.
   */
  _sentSmsEnvelopes: null,
  createSmsEnvelope: function createSmsEnvelope(options) {
    let i;
    for (i = 1; this._sentSmsEnvelopes[i]; i++) {
      // Do nothing.
    }

    debug("createSmsEnvelope: assigned " + i);
    this._sentSmsEnvelopes[i] = options;
    return i;
  },

  handleSmsSent: function handleSmsSent(message) {
    debug("handleSmsSent: " + JSON.stringify(message));

    let options = this._sentSmsEnvelopes[message.envelopeId];
    if (!options) {
      return;
    }

    gMobileMessageDatabaseService.setMessageDelivery(options.sms.id,
                                                     null,
                                                     DOM_MOBILE_MESSAGE_DELIVERY_SENT,
                                                     options.sms.deliveryStatus,
                                                     function notifyResult(rv, domMessage) {
      // TODO bug 832140 handle !Components.isSuccessCode(rv)
      this.broadcastSmsSystemMessage("sms-sent", domMessage);

      if (!options.requestStatusReport) {
        // No more used if STATUS-REPORT not requested.
        delete this._sentSmsEnvelopes[message.envelopeId];
      } else {
        options.sms = domMessage;
      }

      options.request.notifyMessageSent(domMessage);
      Services.obs.notifyObservers(domMessage, kSmsSentObserverTopic, null);
    }.bind(this));
  },

  handleSmsDelivery: function handleSmsDelivery(message) {
    debug("handleSmsDelivery: " + JSON.stringify(message));

    let options = this._sentSmsEnvelopes[message.envelopeId];
    if (!options) {
      return;
    }
    delete this._sentSmsEnvelopes[message.envelopeId];

    gMobileMessageDatabaseService.setMessageDelivery(options.sms.id,
                                                     null,
                                                     options.sms.delivery,
                                                     message.deliveryStatus,
                                                     function notifyResult(rv, domMessage) {
      // TODO bug 832140 handle !Components.isSuccessCode(rv)
      let topic = (message.deliveryStatus == RIL.GECKO_SMS_DELIVERY_STATUS_SUCCESS)
                  ? kSmsDeliverySuccessObserverTopic
                  : kSmsDeliveryErrorObserverTopic;
      Services.obs.notifyObservers(domMessage, topic, null);
    }.bind(this));
  },

  handleSmsSendFailed: function handleSmsSendFailed(message) {
    debug("handleSmsSendFailed: " + JSON.stringify(message));

    let options = this._sentSmsEnvelopes[message.envelopeId];
    if (!options) {
      return;
    }
    delete this._sentSmsEnvelopes[message.envelopeId];

    let error = Ci.nsIMobileMessageCallback.UNKNOWN_ERROR;
    switch (message.errorMsg) {
      case RIL.ERROR_RADIO_NOT_AVAILABLE:
        error = Ci.nsIMobileMessageCallback.NO_SIGNAL_ERROR;
        break;
    }

    gMobileMessageDatabaseService.setMessageDelivery(options.sms.id,
                                                     null,
                                                     DOM_MOBILE_MESSAGE_DELIVERY_ERROR,
                                                     RIL.GECKO_SMS_DELIVERY_STATUS_ERROR,
                                                     function notifyResult(rv, domMessage) {
      // TODO bug 832140 handle !Components.isSuccessCode(rv)
      options.request.notifySendMessageFailed(error);
      Services.obs.notifyObservers(domMessage, kSmsFailedObserverTopic, null);
    }.bind(this));
  },

  /**
   * Handle data call state changes.
   */
  handleDataCallState: function handleDataCallState(datacall) {
    let data = this.rilContext.data;

    if (datacall.ifname &&
        datacall.apn == this.dataCallSettings.apn) {
      data.connected = (datacall.state == RIL.GECKO_NETWORK_STATE_CONNECTED);
      this._sendMobileConnectionMessage("RIL:DataInfoChanged", data);
    }

    this._deliverDataCallCallback("dataCallStateChanged",
                                  [datacall]);
  },

  /**
   * Handle data call list.
   */
  handleDataCallList: function handleDataCallList(message) {
    this._deliverDataCallCallback("receiveDataCallList",
                                  [message.datacalls, message.datacalls.length]);
  },

  /**
   * Set the setting value of "time.nitz.available".
   */
  setNitzAvailable: function setNitzAvailable(value) {
    gSettingsService.createLock().set(kTimeNitzAvailable, value, null,
                                      "fromInternalSetting");
  },

  /**
   * Set the NITZ message in our system time.
   */
  setNitzTime: function setNitzTime(message) {
    // To set the system clock time. Note that there could be a time diff
    // between when the NITZ was received and when the time is actually set.
    gTimeService.set(
      message.networkTimeInMS + (Date.now() - message.receiveTimeInMS));

    // To set the sytem timezone. Note that we need to convert the time zone
    // value to a UTC repesentation string in the format of "UTC(+/-)hh:mm".
    // Ex, time zone -480 is "UTC-08:00"; time zone 630 is "UTC+10:30".
    //
    // We can unapply the DST correction if we want the raw time zone offset:
    // message.networkTimeZoneInMinutes -= message.networkDSTInMinutes;
    if (message.networkTimeZoneInMinutes != (new Date()).getTimezoneOffset()) {
      let absTimeZoneInMinutes = Math.abs(message.networkTimeZoneInMinutes);
      let timeZoneStr = "UTC";
      timeZoneStr += (message.networkTimeZoneInMinutes >= 0 ? "+" : "-");
      timeZoneStr += ("0" + Math.floor(absTimeZoneInMinutes / 60)).slice(-2);
      timeZoneStr += ":";
      timeZoneStr += ("0" + absTimeZoneInMinutes % 60).slice(-2);
      gSettingsService.createLock().set("time.timezone", timeZoneStr, null);
    }
  },

  /**
   * Handle the NITZ message.
   */
  handleNitzTime: function handleNitzTime(message) {
    // Got the NITZ info received from the ril_worker.
    this.setNitzAvailable(true);

    // Cache the latest NITZ message whenever receiving it.
    this._lastNitzMessage = message;

    // Set the received NITZ time if the setting is enabled.
    if (this._nitzAutomaticUpdateEnabled) {
      this.setNitzTime(message);
    }
  },

  handleIccMbdn: function handleIccMbdn(message) {
    let voicemailInfo = this.voicemailInfo;

    voicemailInfo.number = message.number;
    voicemailInfo.displayName = message.alphaId;

    this._sendVoicemailMessage("RIL:VoicemailInfoChanged", voicemailInfo);
  },

  handleIccInfoChange: function handleIccInfoChange(message) {
    let oldIccInfo = this.rilContext.iccInfo;
    this.rilContext.iccInfo = message;

    let iccInfoChanged = !oldIccInfo ||
                          oldIccInfo.iccid != message.iccid ||
                          oldIccInfo.mcc != message.mcc ||
                          oldIccInfo.mnc != message.mnc ||
                          oldIccInfo.spn != message.spn ||
                          oldIccInfo.isDisplayNetworkNameRequired != message.isDisplayNetworkNameRequired ||
                          oldIccInfo.isDisplaySpnRequired != message.isDisplaySpnRequired ||
                          oldIccInfo.msisdn != message.msisdn;
    if (!iccInfoChanged) {
      return;
    }
    // RIL:IccInfoChanged corresponds to a DOM event that gets fired only
    // when the MCC or MNC codes have changed.
    this._sendMobileConnectionMessage("RIL:IccInfoChanged", message);

    // Update lastKnownHomeNetwork.
    if (message.mcc && message.mnc) {
      try {
        Services.prefs.setCharPref("ril.lastKnownHomeNetwork",
                                   message.mcc + "-" + message.mnc);
      } catch (e) {}
    }

    // If spn becomes available, we should check roaming again.
    let oldSpn = oldIccInfo ? oldIccInfo.spn : null;
    if (!oldSpn && message.spn) {
      let voice = this.rilContext.voice;
      let data = this.rilContext.data;
      let voiceRoaming = voice.roaming;
      let dataRoaming = data.roaming;
      this.checkRoamingBetweenOperators(voice);
      this.checkRoamingBetweenOperators(data);
      if (voiceRoaming != voice.roaming) {
        this._sendMobileConnectionMessage("RIL:VoiceInfoChanged", voice);
      }
      if (dataRoaming != data.roaming) {
        this._sendMobileConnectionMessage("RIL:DataInfoChanged", data);
      }
    }
  },

  handleIccCardLockResult: function handleIccCardLockResult(message) {
    this._sendRequestResults("RIL:CardLockResult", message);
  },

  handleUSSDReceived: function handleUSSDReceived(ussd) {
    debug("handleUSSDReceived " + JSON.stringify(ussd));
    gSystemMessenger.broadcastMessage("ussd-received", ussd);
    this._sendMobileConnectionMessage("RIL:USSDReceived", ussd);
  },

  handleSendMMI: function handleSendMMI(message) {
    debug("handleSendMMI " + JSON.stringify(message));
    let messageType = message.success ? "RIL:SendMMI:Return:OK" :
                                        "RIL:SendMMI:Return:KO";
    this._sendRequestResults(messageType, message);
  },

  handleCancelMMI: function handleCancelMMI(message) {
    debug("handleCancelMMI " + JSON.stringify(message));
    let messageType = message.success ? "RIL:CancelMMI:Return:OK" :
                                        "RIL:CancelMMI:Return:KO";
    this._sendRequestResults(messageType, message);
  },

  handleStkProactiveCommand: function handleStkProactiveCommand(message) {
    debug("handleStkProactiveCommand " + JSON.stringify(message));
    gSystemMessenger.broadcastMessage("icc-stkcommand", message);
    this._sendIccMessage("RIL:StkCommand", message);
  },

  handleQueryCallForwardStatus: function handleQueryCallForwardStatus(message) {
    debug("handleQueryCallForwardStatus: " + JSON.stringify(message));
    this._sendRequestResults("RIL:GetCallForwardingOption", message);
  },

  handleSetCallForward: function handleSetCallForward(message) {
    debug("handleSetCallForward: " + JSON.stringify(message));
    this._sendMobileConnectionMessage("RIL:CfStateChanged", message);

    let messageType;
    if (message.isSendMMI) {
      messageType = message.success ? "RIL:SendMMI:Return:OK" :
                                      "RIL:SendMMI:Return:KO";
    } else {
      messageType = "RIL:SetCallForwardingOption";
    }
    this._sendRequestResults(messageType, message);
  },

  handleQueryCallBarringStatus: function handleQueryCallBarringStatus(message) {
    debug("handleQueryCallBarringStatus: " + JSON.stringify(message));
    this._sendRequestResults("RIL:GetCallBarringOption", message);
  },

  handleSetCallBarring: function handleSetCallBarring(message) {
    debug("handleSetCallBarring: " + JSON.stringify(message));
    this._sendRequestResults("RIL:SetCallBarringOption", message);
  },

  handleQueryCallWaiting: function handleQueryCallWaiting(message) {
    debug("handleQueryCallWaiting: " + JSON.stringify(message));
    this._sendRequestResults("RIL:GetCallWaitingOption", message);
  },

  handleSetCallWaiting: function handleSetCallWaiting(message) {
    debug("handleSetCallWaiting: " + JSON.stringify(message));
    this._sendRequestResults("RIL:SetCallWaitingOption", message);
  },

  // nsIObserver

  observe: function observe(subject, topic, data) {
    switch (topic) {
      case kSysMsgListenerReadyObserverTopic:
        Services.obs.removeObserver(this, kSysMsgListenerReadyObserverTopic);
        this._sysMsgListenerReady = true;
        this._resendQueuedTargetMessage();
        this._ensureRadioState();
        break;
      case kMozSettingsChangedObserverTopic:
        let setting = JSON.parse(data);
        this.handleSettingsChange(setting.key, setting.value, setting.message);
        break;
      case kPrefenceChangedObserverTopic:
        if (data === kCellBroadcastDisabled) {
          let value = false;
          try {
            value = Services.prefs.getBoolPref(kCellBroadcastDisabled);
          } catch(e) {}
          this.worker.postMessage({
            rilMessageType: "setCellBroadcastDisabled",
            disabled: value
          });
        }
        break;
      case "xpcom-shutdown":
        ppmm.removeMessageListener("child-process-shutdown", this);
        for (let msgname of RIL_IPC_TELEPHONY_MSG_NAMES) {
          ppmm.removeMessageListener(msgname, this);
        }
        for (let msgname of RIL_IPC_MOBILECONNECTION_MSG_NAMES) {
          ppmm.removeMessageListener(msgname, this);
        }
        for (let msgName of RIL_IPC_ICCMANAGER_MSG_NAMES) {
          ppmm.removeMessageListener(msgName, this);
        }
        for (let msgname of RIL_IPC_VOICEMAIL_MSG_NAMES) {
          ppmm.removeMessageListener(msgname, this);
        }
        for (let msgname of RIL_IPC_CELLBROADCAST_MSG_NAMES) {
          ppmm.removeMessageListener(msgname, this);
        }
        // Cancel the timer for the call-ring wake lock.
        this._cancelCallRingWakeLockTimer();
        // Shutdown all RIL network interfaces
        this.dataNetworkInterface.shutdown();
        this.mmsNetworkInterface.shutdown();
        this.suplNetworkInterface.shutdown();
        ppmm = null;
        Services.obs.removeObserver(this, "xpcom-shutdown");
        Services.obs.removeObserver(this, kMozSettingsChangedObserverTopic);
        Services.obs.removeObserver(this, kSysClockChangeObserverTopic);
        Services.obs.removeObserver(this, kScreenStateChangedTopic);
        Services.prefs.removeObserver(kCellBroadcastDisabled, this);
        break;
      case kSysClockChangeObserverTopic:
        if (this._lastNitzMessage) {
          this._lastNitzMessage.receiveTimeInMS += parseInt(data, 10);
        }
        break;
      case kScreenStateChangedTopic:
        debug("Received a screen-state-changed event: " + JSON.stringify(data));
        this.setScreenState(data);
        break;
    }
  },

  // Flag to determine whether the UI's system app is ready to receive
  // events yet.
  _sysMsgListenerReady: false,

  // Flag to determine the radio state to start with when we boot up. It
  // corresponds to the 'ril.radio.disabled' setting from the UI.
  _radioEnabled: null,

  // Flag to ignore any radio power change requests during We're changing
  // the radio power.
  _changingRadioPower: false,

  // APN data for making data calls.
  dataCallSettings: {},
  dataCallSettingsMMS: {},
  dataCallSettingsSUPL: {},
  _dataCallSettingsToRead: [],
  _oldRilDataEnabledState: null,

  // Flag to determine whether to use NITZ. It corresponds to the
  // 'time.nitz.automatic-update.enabled' setting from the UI.
  _nitzAutomaticUpdateEnabled: null,

  // Remember the last NITZ message so that we can set the time based on
  // the network immediately when users enable network-based time.
  _lastNitzMessage: null,

  // Cell Broadcast settings values.
  _cellBroadcastSearchListStr: null,

  handleSettingsChange: function handleSettingsChange(aName, aResult, aMessage) {
    // Don't allow any content processes to modify the setting
    // "time.nitz.available" except for the chrome process.
    let isNitzAvailable = (this._lastNitzMessage !== null);
    if (aName === kTimeNitzAvailable && aMessage !== "fromInternalSetting" &&
        aResult !== isNitzAvailable) {
      debug("Content processes cannot modify 'time.nitz.available'. Restore!");
      // Restore the setting to the current value.
      this.setNitzAvailable(isNitzAvailable);
    }

    this.handle(aName, aResult);
  },

  // nsISettingsServiceCallback
  handle: function handle(aName, aResult) {
    switch(aName) {
      case "ril.radio.disabled":
        debug("'ril.radio.disabled' is now " + aResult);
        this._radioEnabled = !aResult;
        this._ensureRadioState();
        break;
      case "ril.radio.preferredNetworkType":
        debug("'ril.radio.preferredNetworkType' is now " + aResult);
        this.setPreferredNetworkType(aResult);
        break;
      case "ril.data.enabled":
        this._oldRilDataEnabledState = this.dataCallSettings.enabled;
        // Fall through.
      case "ril.data.roaming_enabled":
      case "ril.data.apn":
      case "ril.data.user":
      case "ril.data.passwd":
      case "ril.data.httpProxyHost":
      case "ril.data.httpProxyPort":
        let key = aName.slice(9);
        this.dataCallSettings[key] = aResult;
        debug("'" + aName + "'" + " is now " + this.dataCallSettings[key]);
        let index = this._dataCallSettingsToRead.indexOf(aName);
        if (index != -1) {
          this._dataCallSettingsToRead.splice(index, 1);
        }
        this.updateRILNetworkInterface();
        break;
      case "ril.mms.apn":
      case "ril.mms.user":
      case "ril.mms.passwd":
      case "ril.mms.httpProxyHost":
      case "ril.mms.httpProxyPort":
      case "ril.mms.mmsc":
      case "ril.mms.mmsproxy":
      case "ril.mms.mmsport":
        key = aName.slice(8);
        this.dataCallSettingsMMS[key] = aResult;
        break;
      case "ril.supl.apn":
      case "ril.supl.user":
      case "ril.supl.passwd":
      case "ril.supl.httpProxyHost":
      case "ril.supl.httpProxyPort":
        key = aName.slice(9);
        this.dataCallSettingsSUPL[key] = aResult;
        break;
      case kTimeNitzAutomaticUpdateEnabled:
        this._nitzAutomaticUpdateEnabled = aResult;

        // Set the latest cached NITZ time if the setting is enabled.
        if (this._nitzAutomaticUpdateEnabled && this._lastNitzMessage) {
          this.setNitzTime(this._lastNitzMessage);
        }
        break;
      case kCellBroadcastSearchList:
        debug("'" + kCellBroadcastSearchList + "' is now " + aResult);
        this.setCellBroadcastSearchList(aResult);
        break;
    }
  },

  handleError: function handleError(aErrorMessage) {
    debug("There was an error while reading RIL settings.");

    // Default radio to on.
    this._radioEnabled = true;
    this._ensureRadioState();

    // Clean data call setting.
    this.dataCallSettings = {};
    this.dataCallSettings.enabled = false;
  },

  // nsIRadioWorker

  worker: null,

  // nsIRadioInterfaceLayer

  setRadioEnabled: function setRadioEnabled(value) {
    debug("Setting radio power to " + value);
    this._changingRadioPower = true;
    this.worker.postMessage({rilMessageType: "setRadioPower", on: value});
  },

  rilContext: null,

  // Handle phone functions of nsIRILContentHelper

  enumerateCalls: function enumerateCalls(message) {
    debug("Requesting enumeration of calls for callback");
    message.rilMessageType = "enumerateCalls";
    this.worker.postMessage(message);
  },

  _validateNumber: function _validateNumber(number) {
    // note: isPlainPhoneNumber also accepts USSD and SS numbers
    if (PhoneNumberUtils.isPlainPhoneNumber(number)) {
      return true;
    }

    this.handleCallError({
      callIndex: -1,
      errorMsg: RIL.RIL_CALL_FAILCAUSE_TO_GECKO_CALL_ERROR[RIL.CALL_FAIL_UNOBTAINABLE_NUMBER]
    });
    debug("Number '" + number + "' doesn't seem to be a viable number. Drop.");

    return false;
  },

  dial: function dial(number) {
    debug("Dialing " + number);
    number = PhoneNumberUtils.normalize(number);
    if (this._validateNumber(number)) {
      this.worker.postMessage({rilMessageType: "dial",
                              number: number,
                              isDialEmergency: false});
    }
  },

  dialEmergency: function dialEmergency(number) {
    debug("Dialing emergency " + number);
    // we don't try to be too clever here, as the phone is probably in the
    // locked state. Let's just check if it's a number without normalizing
    if (this._validateNumber(number)) {
      this.worker.postMessage({rilMessageType: "dial",
                              number: number,
                              isDialEmergency: true});
    }
  },

  hangUp: function hangUp(callIndex) {
    debug("Hanging up call no. " + callIndex);
    this.worker.postMessage({rilMessageType: "hangUp",
                             callIndex: callIndex});
  },

  startTone: function startTone(dtmfChar) {
    debug("Sending Tone for " + dtmfChar);
    this.worker.postMessage({rilMessageType: "startTone",
                             dtmfChar: dtmfChar});
  },

  stopTone: function stopTone() {
    debug("Stopping Tone");
    this.worker.postMessage({rilMessageType: "stopTone"});
  },

  answerCall: function answerCall(callIndex) {
    this.worker.postMessage({rilMessageType: "answerCall",
                             callIndex: callIndex});
  },

  rejectCall: function rejectCall(callIndex) {
    this.worker.postMessage({rilMessageType: "rejectCall",
                             callIndex: callIndex});
  },

  holdCall: function holdCall(callIndex) {
    this.worker.postMessage({rilMessageType: "holdCall",
                             callIndex: callIndex});
  },

  resumeCall: function resumeCall(callIndex) {
    this.worker.postMessage({rilMessageType: "resumeCall",
                             callIndex: callIndex});
  },

  getAvailableNetworks: function getAvailableNetworks(requestId) {
    this.worker.postMessage({rilMessageType: "getAvailableNetworks",
                             requestId: requestId});
  },

  setScreenState: function setScreenState(state) {
    debug("setScreenState: " + JSON.stringify(state));
    this.worker.postMessage({
      rilMessageType: "setScreenState",
      on: (state === "on")
    });
  },

  sendMMI: function sendMMI(message) {
    debug("SendMMI " + JSON.stringify(message));
    message.rilMessageType = "sendMMI";
    this.worker.postMessage(message);
  },

  cancelMMI: function cancelMMI(message) {
    // Some MMI codes trigger radio operations, but unfortunately the RIL only
    // supports cancelling USSD requests so far. Despite that, in order to keep
    // the API uniformity, we are wrapping the cancelUSSD function within the
    // cancelMMI funcion.
    debug("Cancel pending USSD");
    message.rilMessageType = "cancelUSSD";
    this.worker.postMessage(message);
  },

  selectNetworkAuto: function selectNetworkAuto(requestId) {
    this.worker.postMessage({rilMessageType: "selectNetworkAuto",
                             requestId: requestId});
  },

  selectNetwork: function selectNetwork(message) {
    message.rilMessageType = "selectNetwork";
    this.worker.postMessage(message);
  },

  sendStkResponse: function sendStkResponse(message) {
    message.rilMessageType = "sendStkTerminalResponse";
    this.worker.postMessage(message);
  },

  sendStkMenuSelection: function sendStkMenuSelection(message) {
    message.rilMessageType = "sendStkMenuSelection";
    this.worker.postMessage(message);
  },

  sendStkTimerExpiration: function sendStkTimerExpiration(message) {
    message.rilMessageType = "sendStkTimerExpiration";
    this.worker.postMessage(message);
  },

  sendStkEventDownload: function sendStkEventDownload(message) {
    message.rilMessageType = "sendStkEventDownload";
    this.worker.postMessage(message);
  },

  iccOpenChannel: function iccOpenChannel(message) {
    debug("ICC Open Channel");
    message.rilMessageType = "iccOpenChannel";
    this.worker.postMessage(message);
  },

  iccCloseChannel: function iccCloseChannel(message) {
    debug("ICC Close Channel");
    message.rilMessageType = "iccCloseChannel";
    this.worker.postMessage(message);
  },

  iccExchangeAPDU: function iccExchangeAPDU(message) {
    debug("ICC Exchange APDU");
    message.rilMessageType = "iccExchangeAPDU";
    this.worker.postMessage(message);
  },

  setCallForwardingOption: function setCallForwardingOption(message) {
    debug("setCallForwardingOption: " + JSON.stringify(message));
    message.rilMessageType = "setCallForward";
    message.serviceClass = RIL.ICC_SERVICE_CLASS_VOICE;
    this.worker.postMessage(message);
  },

  getCallForwardingOption: function getCallForwardingOption(message) {
    debug("getCallForwardingOption: " + JSON.stringify(message));
    message.rilMessageType = "queryCallForwardStatus";
    message.serviceClass = RIL.ICC_SERVICE_CLASS_NONE;
    message.number = null;
    this.worker.postMessage(message);
  },

  setCallBarringOption: function setCallBarringingOption(message) {
    debug("setCallBarringOption: " + JSON.stringify(message));
    message.rilMessageType = "setCallBarring";
    this.worker.postMessage(message);
  },

  getCallBarringOption: function getCallBarringOption(message) {
    debug("getCallBarringOption: " + JSON.stringify(message));
    message.rilMessageType = "queryCallBarringStatus";
    this.worker.postMessage(message);
  },

  setCallWaitingOption: function setCallWaitingOption(message) {
    debug("setCallWaitingOption: " + JSON.stringify(message));
    message.rilMessageType = "setCallWaiting";
    this.worker.postMessage(message);
  },

  getCallWaitingOption: function getCallWaitingOption(message) {
    debug("getCallWaitingOption: " + JSON.stringify(message));
    message.rilMessageType = "queryCallWaiting";
    this.worker.postMessage(message);
  },

  get microphoneMuted() {
    return gAudioManager.microphoneMuted;
  },
  set microphoneMuted(value) {
    if (value == this.microphoneMuted) {
      return;
    }
    gAudioManager.microphoneMuted = value;

    if (!this._activeCall) {
      gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_NORMAL;
    }
  },

  get speakerEnabled() {
    return (gAudioManager.getForceForUse(nsIAudioManager.USE_COMMUNICATION) ==
            nsIAudioManager.FORCE_SPEAKER);
  },
  set speakerEnabled(value) {
    if (value == this.speakerEnabled) {
      return;
    }
    let force = value ? nsIAudioManager.FORCE_SPEAKER :
                        nsIAudioManager.FORCE_NONE;
    gAudioManager.setForceForUse(nsIAudioManager.USE_COMMUNICATION, force);

    if (!this._activeCall) {
      gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_NORMAL;
    }
  },

  /**
   * List of tuples of national language identifier pairs.
   *
   * TODO: Support static/runtime settings, see bug 733331.
   */
  enabledGsmTableTuples: [
    [RIL.PDU_NL_IDENTIFIER_DEFAULT, RIL.PDU_NL_IDENTIFIER_DEFAULT],
  ],

  /**
   * Use 16-bit reference number for concatenated outgoint messages.
   *
   * TODO: Support static/runtime settings, see bug 733331.
   */
  segmentRef16Bit: false,

  /**
   * Get valid SMS concatenation reference number.
   */
  _segmentRef: 0,
  get nextSegmentRef() {
    let ref = this._segmentRef++;

    this._segmentRef %= (this.segmentRef16Bit ? 65535 : 255);

    // 0 is not a valid SMS concatenation reference number.
    return ref + 1;
  },

  /**
   * Calculate encoded length using specified locking/single shift table
   *
   * @param message
   *        message string to be encoded.
   * @param langTable
   *        locking shift table string.
   * @param langShiftTable
   *        single shift table string.
   * @param strict7BitEncoding
   *        Optional. Enable Latin characters replacement with corresponding
   *        ones in GSM SMS 7-bit default alphabet.
   *
   * @return encoded length in septets.
   *
   * @note that the algorithm used in this function must match exactly with
   * GsmPDUHelper#writeStringAsSeptets.
   */
  _countGsm7BitSeptets: function _countGsm7BitSeptets(message, langTable, langShiftTable, strict7BitEncoding) {
    let length = 0;
    for (let msgIndex = 0; msgIndex < message.length; msgIndex++) {
      let c = message.charAt(msgIndex);
      if (strict7BitEncoding) {
        c = RIL.GSM_SMS_STRICT_7BIT_CHARMAP[c] || c;
      }

      let septet = langTable.indexOf(c);

      // According to 3GPP TS 23.038, section 6.1.1 General notes, "The
      // characters marked '1)' are not used but are displayed as a space."
      if (septet == RIL.PDU_NL_EXTENDED_ESCAPE) {
        continue;
      }

      if (septet >= 0) {
        length++;
        continue;
      }

      septet = langShiftTable.indexOf(c);
      if (septet < 0) {
        if (!strict7BitEncoding) {
          return -1;
        }

        // Bug 816082, when strict7BitEncoding is enabled, we should replace
        // characters that can't be encoded with GSM 7-Bit alphabets with '*'.
        c = '*';
        if (langTable.indexOf(c) >= 0) {
          length++;
        } else if (langShiftTable.indexOf(c) >= 0) {
          length += 2;
        } else {
          // We can't even encode a '*' character with current configuration.
          return -1;
        }

        continue;
      }

      // According to 3GPP TS 23.038 B.2, "This code represents a control
      // character and therefore must not be used for language specific
      // characters."
      if (septet == RIL.PDU_NL_RESERVED_CONTROL) {
        continue;
      }

      // The character is not found in locking shfit table, but could be
      // encoded as <escape><char> with single shift table. Note that it's
      // still possible for septet to has the value of PDU_NL_EXTENDED_ESCAPE,
      // but we can display it as a space in this case as said in previous
      // comment.
      length += 2;
    }

    return length;
  },

  /**
   * Calculate user data length of specified message string encoded in GSM 7Bit
   * alphabets.
   *
   * @param message
   *        a message string to be encoded.
   * @param strict7BitEncoding
   *        Optional. Enable Latin characters replacement with corresponding
   *        ones in GSM SMS 7-bit default alphabet.
   *
   * @return null or an options object with attributes `dcs`,
   *         `userDataHeaderLength`, `encodedFullBodyLength`, `langIndex`,
   *         `langShiftIndex`, `segmentMaxSeq` set.
   *
   * @see #_calculateUserDataLength().
   */
  _calculateUserDataLength7Bit: function _calculateUserDataLength7Bit(message, strict7BitEncoding) {
    let options = null;
    let minUserDataSeptets = Number.MAX_VALUE;
    for (let i = 0; i < this.enabledGsmTableTuples.length; i++) {
      let [langIndex, langShiftIndex] = this.enabledGsmTableTuples[i];

      const langTable = RIL.PDU_NL_LOCKING_SHIFT_TABLES[langIndex];
      const langShiftTable = RIL.PDU_NL_SINGLE_SHIFT_TABLES[langShiftIndex];

      let bodySeptets = this._countGsm7BitSeptets(message,
                                                  langTable,
                                                  langShiftTable,
                                                  strict7BitEncoding);
      if (bodySeptets < 0) {
        continue;
      }

      let headerLen = 0;
      if (langIndex != RIL.PDU_NL_IDENTIFIER_DEFAULT) {
        headerLen += 3; // IEI + len + langIndex
      }
      if (langShiftIndex != RIL.PDU_NL_IDENTIFIER_DEFAULT) {
        headerLen += 3; // IEI + len + langShiftIndex
      }

      // Calculate full user data length, note the extra byte is for header len
      let headerSeptets = Math.ceil((headerLen ? headerLen + 1 : 0) * 8 / 7);
      let segmentSeptets = RIL.PDU_MAX_USER_DATA_7BIT;
      if ((bodySeptets + headerSeptets) > segmentSeptets) {
        headerLen += this.segmentRef16Bit ? 6 : 5;
        headerSeptets = Math.ceil((headerLen + 1) * 8 / 7);
        segmentSeptets -= headerSeptets;
      }

      let segments = Math.ceil(bodySeptets / segmentSeptets);
      let userDataSeptets = bodySeptets + headerSeptets * segments;
      if (userDataSeptets >= minUserDataSeptets) {
        continue;
      }

      minUserDataSeptets = userDataSeptets;

      options = {
        dcs: RIL.PDU_DCS_MSG_CODING_7BITS_ALPHABET,
        encodedFullBodyLength: bodySeptets,
        userDataHeaderLength: headerLen,
        langIndex: langIndex,
        langShiftIndex: langShiftIndex,
        segmentMaxSeq: segments,
        segmentChars: segmentSeptets,
      };
    }

    return options;
  },

  /**
   * Calculate user data length of specified message string encoded in UCS2.
   *
   * @param message
   *        a message string to be encoded.
   *
   * @return an options object with attributes `dcs`, `userDataHeaderLength`,
   *         `encodedFullBodyLength`, `segmentMaxSeq` set.
   *
   * @see #_calculateUserDataLength().
   */
  _calculateUserDataLengthUCS2: function _calculateUserDataLengthUCS2(message) {
    let bodyChars = message.length;
    let headerLen = 0;
    let headerChars = Math.ceil((headerLen ? headerLen + 1 : 0) / 2);
    let segmentChars = RIL.PDU_MAX_USER_DATA_UCS2;
    if ((bodyChars + headerChars) > segmentChars) {
      headerLen += this.segmentRef16Bit ? 6 : 5;
      headerChars = Math.ceil((headerLen + 1) / 2);
      segmentChars -= headerChars;
    }

    let segments = Math.ceil(bodyChars / segmentChars);

    return {
      dcs: RIL.PDU_DCS_MSG_CODING_16BITS_ALPHABET,
      encodedFullBodyLength: bodyChars * 2,
      userDataHeaderLength: headerLen,
      segmentMaxSeq: segments,
      segmentChars: segmentChars,
    };
  },

  /**
   * Calculate user data length and its encoding.
   *
   * @param message
   *        a message string to be encoded.
   * @param strict7BitEncoding
   *        Optional. Enable Latin characters replacement with corresponding
   *        ones in GSM SMS 7-bit default alphabet.
   *
   * @return an options object with some or all of following attributes set:
   *
   * @param dcs
   *        Data coding scheme. One of the PDU_DCS_MSG_CODING_*BITS_ALPHABET
   *        constants.
   * @param userDataHeaderLength
   *        Length of embedded user data header, in bytes. The whole header
   *        size will be userDataHeaderLength + 1; 0 for no header.
   * @param encodedFullBodyLength
   *        Length of the message body when encoded with the given DCS. For
   *        UCS2, in bytes; for 7-bit, in septets.
   * @param langIndex
   *        Table index used for normal 7-bit encoded character lookup.
   * @param langShiftIndex
   *        Table index used for escaped 7-bit encoded character lookup.
   * @param segmentMaxSeq
   *        Max sequence number of a multi-part messages, or 1 for single one.
   *        This number might not be accurate for a multi-part message until
   *        it's processed by #_fragmentText() again.
   */
  _calculateUserDataLength: function _calculateUserDataLength(message, strict7BitEncoding) {
    let options = this._calculateUserDataLength7Bit(message, strict7BitEncoding);
    if (!options) {
      options = this._calculateUserDataLengthUCS2(message);
    }

    debug("_calculateUserDataLength: " + JSON.stringify(options));
    return options;
  },

  /**
   * Fragment GSM 7-Bit encodable string for transmission.
   *
   * @param text
   *        text string to be fragmented.
   * @param langTable
   *        locking shift table string.
   * @param langShiftTable
   *        single shift table string.
   * @param segmentSeptets
   *        Number of available spetets per segment.
   * @param strict7BitEncoding
   *        Optional. Enable Latin characters replacement with corresponding
   *        ones in GSM SMS 7-bit default alphabet.
   *
   * @return an array of objects. See #_fragmentText() for detailed definition.
   */
  _fragmentText7Bit: function _fragmentText7Bit(text, langTable, langShiftTable, segmentSeptets, strict7BitEncoding) {
    let ret = [];
    let body = "", len = 0;
    for (let i = 0, inc = 0; i < text.length; i++) {
      let c = text.charAt(i);
      if (strict7BitEncoding) {
        c = RIL.GSM_SMS_STRICT_7BIT_CHARMAP[c] || c;
      }

      let septet = langTable.indexOf(c);
      if (septet == RIL.PDU_NL_EXTENDED_ESCAPE) {
        continue;
      }

      if (septet >= 0) {
        inc = 1;
      } else {
        septet = langShiftTable.indexOf(c);
        if (septet == RIL.PDU_NL_RESERVED_CONTROL) {
          continue;
        }

        inc = 2;
        if (septet < 0) {
          if (!strict7BitEncoding) {
            throw new Error("Given text cannot be encoded with GSM 7-bit Alphabet!");
          }

          // Bug 816082, when strict7BitEncoding is enabled, we should replace
          // characters that can't be encoded with GSM 7-Bit alphabets with '*'.
          c = '*';
          if (langTable.indexOf(c) >= 0) {
            inc = 1;
          }
        }
      }

      if ((len + inc) > segmentSeptets) {
        ret.push({
          body: body,
          encodedBodyLength: len,
        });
        body = c;
        len = inc;
      } else {
        body += c;
        len += inc;
      }
    }

    if (len) {
      ret.push({
        body: body,
        encodedBodyLength: len,
      });
    }

    return ret;
  },

  /**
   * Fragment UCS2 encodable string for transmission.
   *
   * @param text
   *        text string to be fragmented.
   * @param segmentChars
   *        Number of available characters per segment.
   *
   * @return an array of objects. See #_fragmentText() for detailed definition.
   */
  _fragmentTextUCS2: function _fragmentTextUCS2(text, segmentChars) {
    let ret = [];
    for (let offset = 0; offset < text.length; offset += segmentChars) {
      let str = text.substr(offset, segmentChars);
      ret.push({
        body: str,
        encodedBodyLength: str.length * 2,
      });
    }

    return ret;
  },

  /**
   * Fragment string for transmission.
   *
   * Fragment input text string into an array of objects that contains
   * attributes `body`, substring for this segment, `encodedBodyLength`,
   * length of the encoded segment body in septets.
   *
   * @param text
   *        Text string to be fragmented.
   * @param options
   *        Optional pre-calculated option object. The output array will be
   *        stored at options.segments if there are multiple segments.
   * @param strict7BitEncoding
   *        Optional. Enable Latin characters replacement with corresponding
   *        ones in GSM SMS 7-bit default alphabet.
   *
   * @return Populated options object.
   */
  _fragmentText: function _fragmentText(text, options, strict7BitEncoding) {
    if (!options) {
      options = this._calculateUserDataLength(text, strict7BitEncoding);
    }

    if (options.dcs == RIL.PDU_DCS_MSG_CODING_7BITS_ALPHABET) {
      const langTable = RIL.PDU_NL_LOCKING_SHIFT_TABLES[options.langIndex];
      const langShiftTable = RIL.PDU_NL_SINGLE_SHIFT_TABLES[options.langShiftIndex];
      options.segments = this._fragmentText7Bit(text,
                                                langTable, langShiftTable,
                                                options.segmentChars,
                                                strict7BitEncoding);
    } else {
      options.segments = this._fragmentTextUCS2(text,
                                                options.segmentChars);
    }

    // Re-sync options.segmentMaxSeq with actual length of returning array.
    options.segmentMaxSeq = options.segments.length;

    return options;
  },

  getSegmentInfoForText: function getSegmentInfoForText(text) {
    let strict7BitEncoding;
    try {
      strict7BitEncoding = Services.prefs.getBoolPref("dom.sms.strict7BitEncoding");
    } catch (e) {
      strict7BitEncoding = false;
    }

    let options = this._fragmentText(text, null, strict7BitEncoding);
    let charsInLastSegment;
    if (options.segmentMaxSeq) {
      let lastSegment = options.segments[options.segmentMaxSeq - 1];
      charsInLastSegment = lastSegment.encodedBodyLength;
      if (options.dcs == RIL.PDU_DCS_MSG_CODING_16BITS_ALPHABET) {
        // In UCS2 encoding, encodedBodyLength is in octets.
        charsInLastSegment /= 2;
      }
    } else {
      charsInLastSegment = 0;
    }

    let result = gMobileMessageService.createSmsSegmentInfo(options.segmentMaxSeq,
                                                            options.segmentChars,
                                                            options.segmentChars - charsInLastSegment);
    return result;
  },

  sendSMS: function sendSMS(number, message, request) {
    let strict7BitEncoding;
    try {
      strict7BitEncoding = Services.prefs.getBoolPref("dom.sms.strict7BitEncoding");
    } catch (e) {
      strict7BitEncoding = false;
    }

    let options = this._fragmentText(message, null, strict7BitEncoding);
    options.rilMessageType = "sendSMS";
    options.number = PhoneNumberUtils.normalize(number);
    let requestStatusReport;
    try {
      requestStatusReport =
        Services.prefs.getBoolPref("dom.sms.requestStatusReport");
    } catch (e) {
      requestStatusReport = true;
    }
    options.requestStatusReport = requestStatusReport;
    if (options.segmentMaxSeq > 1) {
      options.segmentRef16Bit = this.segmentRef16Bit;
      options.segmentRef = this.nextSegmentRef;
    }

    let sendingMessage = {
      type: "sms",
      receiver: number,
      body: message,
      deliveryStatusRequested: options.requestStatusReport,
      timestamp: Date.now()
    };

    let id = gMobileMessageDatabaseService.saveSendingMessage(
      sendingMessage,
      function notifyResult(rv, domMessage) {

        // TODO bug 832140 handle !Components.isSuccessCode(rv)
        Services.obs.notifyObservers(domMessage, kSmsSendingObserverTopic, null);

        // Keep current SMS message info for sent/delivered notifications
        options.envelopeId = this.createSmsEnvelope({
          request: request,
          sms: domMessage,
          requestStatusReport: options.requestStatusReport
        });

        if (PhoneNumberUtils.isPlainPhoneNumber(options.number)) {
          this.worker.postMessage(options);
        } else {
          debug('Number ' + options.number + ' is not sendable.');
          this.handleSmsSendFailed(options);
        }

      }.bind(this));
  },

  registerDataCallCallback: function registerDataCallCallback(callback) {
    if (this._datacall_callbacks) {
      if (this._datacall_callbacks.indexOf(callback) != -1) {
        throw new Error("Already registered this callback!");
      }
    } else {
      this._datacall_callbacks = [];
    }
    this._datacall_callbacks.push(callback);
    debug("Registering callback: " + callback);
  },

  unregisterDataCallCallback: function unregisterDataCallCallback(callback) {
    if (!this._datacall_callbacks) {
      return;
    }
    let index = this._datacall_callbacks.indexOf(callback);
    if (index != -1) {
      this._datacall_callbacks.splice(index, 1);
      debug("Unregistering callback: " + callback);
    }
  },

  _deliverDataCallCallback: function _deliverDataCallCallback(name, args) {
    // We need to worry about callback registration state mutations during the
    // callback firing. The behaviour we want is to *not* call any callbacks
    // that are added during the firing and to *not* call any callbacks that are
    // removed during the firing. To address this, we make a copy of the
    // callback list before dispatching and then double-check that each callback
    // is still registered before calling it.
    if (!this._datacall_callbacks) {
      return;
    }
    let callbacks = this._datacall_callbacks.slice();
    for (let callback of callbacks) {
      if (this._datacall_callbacks.indexOf(callback) == -1) {
        continue;
      }
      let handler = callback[name];
      if (typeof handler != "function") {
        throw new Error("No handler for " + name);
      }
      try {
        handler.apply(callback, args);
      } catch (e) {
        debug("callback handler for " + name + " threw an exception: " + e);
      }
    }
  },

  /**
   * Determine whether secondary APN goes through default APN.
   */
  usingDefaultAPN: function usingDefaultAPN(apntype) {
    switch (apntype) {
      case "mms":
        return (this.dataCallSettingsMMS.apn == this.dataCallSettings.apn);
      case "supl":
        return (this.dataCallSettingsSUPL.apn == this.dataCallSettings.apn);
      default:
        return false;
    }
  },

  /**
   * Number of activated secondary APN data call that shares with default APN.
   */
  shareDefaultAPNCounter: 0,

  setupDataCallByType: function setupDataCallByType(apntype) {
    if (apntype != "default" && this.usingDefaultAPN(apntype)) {
      debug("Setup secondary APN type " + apntype + " which goes through default APN.");
      this.shareDefaultAPNCounter++;
      this.updateRILNetworkInterface();
      return;
    }
    switch (apntype) {
      case "default":
        this.dataNetworkInterface.connect(this.dataCallSettings);
        break;
      case "mms":
        this.mmsNetworkInterface.connect(this.dataCallSettingsMMS);
        break;
      case "supl":
        this.suplNetworkInterface.connect(this.dataCallSettingsSUPL);
        break;
      default:
        debug("Unsupported APN type " + apntype);
        break;
    }
  },

  deactivateDataCallByType: function deactivateDataCallByType(apntype) {
    if (apntype != "default" && this.usingDefaultAPN(apntype)) {
      debug("Deactivate secondary APN type " + apntype + " which goes through default APN.");
      if (this.shareDefaultAPNCounter > 0) {
        this.shareDefaultAPNCounter--;
        this.updateRILNetworkInterface();
      }
      return;
    }
    switch (apntype) {
      case "default":
        this.dataNetworkInterface.disconnect();
        break;
      case "mms":
        this.mmsNetworkInterface.disconnect();
        break;
      case "supl":
        this.suplNetworkInterface.disconnect();
        break;
      default:
        debug("Unsupported APN type " + apntype);
        break;
    }
  },

  getDataCallStateByType: function getDataCallStateByType(apntype) {
    if (apntype != "default" && this.usingDefaultAPN(apntype)) {
      return this.dataNetworkInterface.state;
    }
    switch (apntype) {
      case "default":
        return this.dataNetworkInterface.state;
      case "mms":
        return this.mmsNetworkInterface.state;
      case "supl":
        return this.suplNetworkInterface.state;
      default:
        return RIL.GECKO_NETWORK_STATE_UNKNOWN;
    }
  },

  setupDataCall: function setupDataCall(radioTech, apn, user, passwd, chappap, pdptype) {
    this.worker.postMessage({rilMessageType: "setupDataCall",
                             radioTech: radioTech,
                             apn: apn,
                             user: user,
                             passwd: passwd,
                             chappap: chappap,
                             pdptype: pdptype});
  },

  deactivateDataCall: function deactivateDataCall(cid, reason) {
    this.worker.postMessage({rilMessageType: "deactivateDataCall",
                             cid: cid,
                             reason: reason});
  },

  getDataCallList: function getDataCallList() {
    this.worker.postMessage({rilMessageType: "getDataCallList"});
  },

  getCardLock: function getCardLock(message) {
    message.rilMessageType = "iccGetCardLock";
    this.worker.postMessage(message);
  },

  unlockCardLock: function unlockCardLock(message) {
    message.rilMessageType = "iccUnlockCardLock";
    this.worker.postMessage(message);
  },

  setCardLock: function setCardLock(message) {
    message.rilMessageType = "iccSetCardLock";
    this.worker.postMessage(message);
  },

  readIccContacts: function readIccContacts(message) {
    message.rilMessageType = "readICCContacts";
    this.worker.postMessage(message);
  },

  updateIccContact: function updateIccContact(message) {
    message.rilMessageType = "updateICCContact";
    this.worker.postMessage(message);
  },
};

function RILNetworkInterface(ril, type)
{
  this.mRIL = ril;
  this.type = type;
}

RILNetworkInterface.prototype = {
  classID:   RILNETWORKINTERFACE_CID,
  classInfo: XPCOMUtils.generateCI({classID: RILNETWORKINTERFACE_CID,
                                    classDescription: "RILNetworkInterface",
                                    interfaces: [Ci.nsINetworkInterface,
                                                 Ci.nsIRILDataCallback]}),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsINetworkInterface,
                                         Ci.nsIRILDataCallback]),

  // nsINetworkInterface

  NETWORK_STATE_UNKNOWN:       Ci.nsINetworkInterface.NETWORK_STATE_UNKNOWN,
  NETWORK_STATE_CONNECTING:    Ci.nsINetworkInterface.CONNECTING,
  NETWORK_STATE_CONNECTED:     Ci.nsINetworkInterface.CONNECTED,
  NETWORK_STATE_DISCONNECTING: Ci.nsINetworkInterface.DISCONNECTING,
  NETWORK_STATE_DISCONNECTED:  Ci.nsINetworkInterface.DISCONNECTED,

  state: Ci.nsINetworkInterface.NETWORK_STATE_UNKNOWN,

  NETWORK_TYPE_WIFI:        Ci.nsINetworkInterface.NETWORK_TYPE_WIFI,
  NETWORK_TYPE_MOBILE:      Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE,
  NETWORK_TYPE_MOBILE_MMS:  Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS,
  NETWORK_TYPE_MOBILE_SUPL: Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_SUPL,

  /**
   * Standard values for the APN connection retry process
   * Retry funcion: time(secs) = A * numer_of_retries^2 + B
   */
  NETWORK_APNRETRY_FACTOR: 8,
  NETWORK_APNRETRY_ORIGIN: 3,
  NETWORK_APNRETRY_MAXRETRIES: 10,

  // Event timer for connection retries
  timer: null,

  type: Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE,

  name: null,

  dhcp: false,

  ip: null,

  netmask: null,

  broadcast: null,

  dns1: null,

  dns2: null,

  httpProxyHost: null,

  httpProxyPort: null,

  // nsIRILDataCallback

  dataCallError: function dataCallError(message) {
    if (message.apn != this.dataCallSettings.apn) {
      return;
    }
    debug("Data call error on APN: " + message.apn);
    this.reset();
  },

  dataCallStateChanged: function dataCallStateChanged(datacall) {
    if (this.cid && this.cid != datacall.cid) {
    // If data call for this connection existed but cid mismatched,
    // it means this datacall state change is not for us.
      return;
    }
    // If data call for this connection does not exist, it could be state
    // change for new data call.  We only update data call state change
    // if APN name matched.
    if (!this.cid && datacall.apn != this.dataCallSettings.apn) {
      return;
    }
    debug("Data call ID: " + datacall.cid + ", interface name: " +
          datacall.ifname + ", APN name: " + datacall.apn);
    if (this.connecting &&
        (datacall.state == RIL.GECKO_NETWORK_STATE_CONNECTING ||
         datacall.state == RIL.GECKO_NETWORK_STATE_CONNECTED)) {
      this.connecting = false;
      this.cid = datacall.cid;
      this.name = datacall.ifname;
      this.ip = datacall.ip;
      this.netmask = datacall.netmask;
      this.broadcast = datacall.broadcast;
      this.gateway = datacall.gw;
      if (datacall.dns) {
        this.dns1 = datacall.dns[0];
        this.dns2 = datacall.dns[1];
      }
      if (!this.registeredAsNetworkInterface) {
        gNetworkManager.registerNetworkInterface(this);
        this.registeredAsNetworkInterface = true;
      }
    }
    // In current design, we don't update status of secondary APN if it shares
    // same APN name with the default APN.  In this condition, this.cid will
    // not be set and we don't want to update its status.
    if (this.cid == null) {
      return;
    }
    if (this.state == datacall.state) {
      return;
    }

    this.state = datacall.state;

    // In case the data setting changed while the datacall was being started or
    // ended, let's re-check the setting and potentially adjust the datacall
    // state again.
    if (this == this.mRIL.dataNetworkInterface) {
      this.mRIL.updateRILNetworkInterface();
    }

    if (this.state == RIL.GECKO_NETWORK_STATE_UNKNOWN &&
       this.registeredAsNetworkInterface) {
      gNetworkManager.unregisterNetworkInterface(this);
      this.registeredAsNetworkInterface = false;
      this.cid = null;
      return;
    }

    Services.obs.notifyObservers(this,
                                 kNetworkInterfaceStateChangedTopic,
                                 null);
  },

  receiveDataCallList: function receiveDataCallList(dataCalls, length) {
  },

  // Helpers

  cid: null,
  registeredAsDataCallCallback: false,
  registeredAsNetworkInterface: false,
  connecting: false,
  dataCallSettings: {},

  // APN failed connections. Retry counter
  apnRetryCounter: 0,

  get connected() {
    return this.state == RIL.GECKO_NETWORK_STATE_CONNECTED;
  },

  connect: function connect(options) {
    if (this.connecting || this.connected) {
      return;
    }

    if (!this.registeredAsDataCallCallback) {
      this.mRIL.registerDataCallCallback(this);
      this.registeredAsDataCallCallback = true;
    }

    if (options) {
      // Save the APN data locally for using them in connection retries.
      this.dataCallSettings = options;
    }

    if (!this.dataCallSettings.apn) {
      debug("APN name is empty, nothing to do.");
      return;
    }

    this.httpProxyHost = this.dataCallSettings.httpProxyHost;
    this.httpProxyPort = this.dataCallSettings.httpProxyPort;

    debug("Going to set up data connection with APN " + this.dataCallSettings.apn);
    let radioTechType = this.mRIL.rilContext.data.type;
    let radioTechnology = RIL.GECKO_RADIO_TECH.indexOf(radioTechType);
    this.mRIL.setupDataCall(radioTechnology,
                            this.dataCallSettings.apn,
                            this.dataCallSettings.user,
                            this.dataCallSettings.passwd,
                            RIL.DATACALL_AUTH_PAP_OR_CHAP, "IP");
    this.connecting = true;
  },

  reset: function reset() {
    let apnRetryTimer;
    this.connecting = false;
    // We will retry the connection in increasing times
    // based on the function: time = A * numer_of_retries^2 + B
    if (this.apnRetryCounter >= this.NETWORK_APNRETRY_MAXRETRIES) {
      this.apnRetryCounter = 0;
      this.timer = null;
      debug("Too many APN Connection retries - STOP retrying");
      return;
    }

    apnRetryTimer = this.NETWORK_APNRETRY_FACTOR *
                    (this.apnRetryCounter * this.apnRetryCounter) +
                    this.NETWORK_APNRETRY_ORIGIN;
    this.apnRetryCounter++;
    debug("Data call - APN Connection Retry Timer (secs-counter): " +
          apnRetryTimer + "-" + this.apnRetryCounter);

    if (this.timer == null) {
      // Event timer for connection retries
      this.timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    }
    this.timer.initWithCallback(this, apnRetryTimer * 1000,
                                Ci.nsITimer.TYPE_ONE_SHOT);
  },

  disconnect: function disconnect() {
    if (this.state == RIL.GECKO_NETWORK_STATE_DISCONNECTING ||
        this.state == RIL.GECKO_NETWORK_STATE_DISCONNECTED) {
      return;
    }
    let reason = RIL.DATACALL_DEACTIVATE_NO_REASON;
    debug("Going to disconnet data connection " + this.cid);
    this.mRIL.deactivateDataCall(this.cid, reason);
  },

  // Entry method for timer events. Used to reconnect to a failed APN
  notify: function(timer) {
    this.connect();
  },

  shutdown: function() {
    this.timer = null;
  }

};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([RadioInterfaceLayer]);

