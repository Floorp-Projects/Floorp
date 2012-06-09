/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

var RIL = {};
Cu.import("resource://gre/modules/ril_consts.js", RIL);

const DEBUG = false; // set to true to see debug messages

const RADIOINTERFACELAYER_CID =
  Components.ID("{2d831c8d-6017-435b-a80c-e5d422810cea}");
const DATACALLINFO_CID =
  Components.ID("{ef474cd9-94f7-4c05-a31b-29b9de8a10d2}");

const nsIAudioManager = Ci.nsIAudioManager;
const nsIRadioInterfaceLayer = Ci.nsIRadioInterfaceLayer;

const kNetworkInterfaceStateChangedTopic = "network-interface-state-changed";
const kSmsReceivedObserverTopic          = "sms-received";
const kSmsDeliveredObserverTopic         = "sms-delivered";
const kMozSettingsChangedObserverTopic   = "mozsettings-changed";
const DOM_SMS_DELIVERY_RECEIVED          = "received";
const DOM_SMS_DELIVERY_SENT              = "sent";

const RIL_IPC_MSG_NAMES = [
  "RIL:GetRadioState",
  "RIL:EnumerateCalls",
  "RIL:GetMicrophoneMuted",
  "RIL:SetMicrophoneMuted",
  "RIL:GetSpeakerEnabled",
  "RIL:SetSpeakerEnabled",
  "RIL:StartTone",
  "RIL:StopTone",
  "RIL:Dial",
  "RIL:HangUp",
  "RIL:AnswerCall",
  "RIL:RejectCall",
  "RIL:HoldCall",
  "RIL:ResumeCall",
  "RIL:GetAvailableNetworks",
  "RIL:GetCardLock",
  "RIL:UnlockCardLock",
  "RIL:SetCardLock",
  "RIL:SendUSSD",
  "RIL:CancelUSSD"
];

XPCOMUtils.defineLazyServiceGetter(this, "gSmsService",
                                   "@mozilla.org/sms/smsservice;1",
                                   "nsISmsService");

XPCOMUtils.defineLazyServiceGetter(this, "gSmsRequestManager",
                                   "@mozilla.org/sms/smsrequestmanager;1",
                                   "nsISmsRequestManager");

XPCOMUtils.defineLazyServiceGetter(this, "gSmsDatabaseService",
                                   "@mozilla.org/sms/rilsmsdatabaseservice;1",
                                   "nsISmsDatabaseService");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIFrameMessageManager");

XPCOMUtils.defineLazyServiceGetter(this, "gSettingsService",
                                   "@mozilla.org/settingsService;1",
                                   "nsISettingsService");

XPCOMUtils.defineLazyGetter(this, "WAP", function () {
  let WAP = {};
  Cu.import("resource://gre/modules/WapPushManager.js", WAP);
  return WAP;
});

function convertRILCallState(state) {
  switch (state) {
    case RIL.CALL_STATE_ACTIVE:
      return nsIRadioInterfaceLayer.CALL_STATE_CONNECTED;
    case RIL.CALL_STATE_HOLDING:
      return nsIRadioInterfaceLayer.CALL_STATE_HELD;
    case RIL.CALL_STATE_DIALING:
      return nsIRadioInterfaceLayer.CALL_STATE_DIALING;
    case RIL.CALL_STATE_ALERTING:
      return nsIRadioInterfaceLayer.CALL_STATE_ALERTING;
    case RIL.CALL_STATE_INCOMING:
    case RIL.CALL_STATE_WAITING:
      return nsIRadioInterfaceLayer.CALL_STATE_INCOMING; 
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


function DataCallInfo(state, cid, apn) {
  this.callState = state;
  this.cid = cid;
  this.apn = apn;
}
DataCallInfo.protoptype = {
  classID:      DATACALLINFO_CID,
  classInfo:    XPCOMUtils.generateCI({classID: DATACALLINFO_CID,
                                       classDescription: "DataCallInfo",
                                       interfaces: [Ci.nsIDataCallInfo]}),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDataCallInfo]),
};


function RadioInterfaceLayer() {
  debug("Starting RIL Worker");
  this.worker = new ChromeWorker("resource://gre/modules/ril_worker.js");
  this.worker.onerror = this.onerror.bind(this);
  this.worker.onmessage = this.onmessage.bind(this);

  this.radioState = {
    radioState:     RIL.GECKO_RADIOSTATE_UNAVAILABLE,
    cardState:      RIL.GECKO_CARDSTATE_UNAVAILABLE,
    icc:            null,
    cell:           null,

    // These objects implement the nsIDOMMozMobileConnectionInfo interface,
    // although the actual implementation lives in the content process.
    voice:          {connected: false,
                     emergencyCallsOnly: false,
                     roaming: false,
                     operator: null,
                     type: null,
                     signalStrength: null,
                     relSignalStrength: null},
    data:          {connected: false,
                     emergencyCallsOnly: false,
                     roaming: false,
                     operator: null,
                     type: null,
                     signalStrength: null,
                     relSignalStrength: null},
  };

  // Read the 'ril.radio.disabled' setting in order to start with a known
  // value at boot time.
  gSettingsService.getLock().get("ril.radio.disabled", this);

  for each (let msgname in RIL_IPC_MSG_NAMES) {
    ppmm.addMessageListener(msgname, this);
  }
  Services.obs.addObserver(this, "xpcom-shutdown", false);
  Services.obs.addObserver(this, kMozSettingsChangedObserverTopic, false);

  this._sentSmsEnvelopes = {};

  this.portAddressedSmsApps = {};
  this.portAddressedSmsApps[WAP.WDP_PORT_PUSH] = this.handleSmsWdpPortPush.bind(this);
}
RadioInterfaceLayer.prototype = {

  classID:   RADIOINTERFACELAYER_CID,
  classInfo: XPCOMUtils.generateCI({classID: RADIOINTERFACELAYER_CID,
                                    classDescription: "RadioInterfaceLayer",
                                    interfaces: [Ci.nsIWorkerHolder,
                                                 Ci.nsIRadioInterfaceLayer]}),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWorkerHolder,
                                         Ci.nsIRadioInterfaceLayer,
                                         Ci.nsIObserver,
                                         Ci.nsISettingsServiceCallback]),

  /**
   * Process a message from the content process.
   */
  receiveMessage: function receiveMessage(msg) {
    debug("Received '" + msg.name + "' message from content process");
    switch (msg.name) {
      case "RIL:GetRadioState":
        // This message is sync.
        return this.radioState;
      case "RIL:EnumerateCalls":
        this.enumerateCalls();
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
      case "RIL:GetAvailableNetworks":
        this.getAvailableNetworks(msg.json);
        break;
      case "RIL:GetCardLock":
        this.getCardLock(msg.json);
        break;
      case "RIL:UnlockCardLock":
        this.unlockCardLock(msg.json);
        break;
      case "RIL:SetCardLock":
        this.setCardLock(msg.json);
        break;
      case "RIL:SendUSSD":
        this.sendUSSD(msg.json);
        break;
      case "RIL:CancelUSSD":
        this.cancelUSSD(msg.json);
        break;
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
    switch (message.type) {
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
        this.handleEnumerateCalls(message.calls);
        break;
      case "callError":
        this.handleCallError(message);
        break;
      case "getAvailableNetworks":
        this.handleGetAvailableNetworks(message);
        break;
      case "voiceregistrationstatechange":
        this.updateVoiceConnection(message);
        break;
      case "dataregistrationstatechange":
        this.updateDataConnection(message);
        break;
      case "datacallerror":
        // 3G Network revoked the data connection, possible unavailable APN
        debug("Received data registration error message. Failed APN " +
              Services.prefs.getCharPref("ril.data.apn"));
        RILNetworkInterface.reset();
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
        this.radioState.cardState = message.cardState;
        ppmm.sendAsyncMessage("RIL:CardStateChanged", message);
        break;
      case "sms-received":
        this.handleSmsReceived(message);
        return;
      case "sms-sent":
        this.handleSmsSent(message);
        return;
      case "sms-delivered":
        this.handleSmsDelivered(message);
        return;
      case "sms-send-failed":
        this.handleSmsSendFailed(message);
        return;
      case "datacallstatechange":
        this.handleDataCallState(message.datacall);
        break;
      case "datacalllist":
        this.handleDataCallList(message);
        break;
      case "nitzTime":
        // TODO bug 714349
        // Send information to time manager to decide what to do with it
        // Message contains networkTimeInSeconds, networkTimeZoneInMinutes,
        // dstFlag,localTimeStampInMS
        // indicating the time, daylight savings flag, and timezone
        // sent from the network and a timestamp of when the message was received
        // so an offset can be added if/when the time is actually set.
        debug("nitzTime networkTime=" + message.networkTimeInSeconds +
              " timezone=" + message.networkTimeZoneInMinutes +
              " dst=" + message.dstFlag +
              " timestamp=" + message.localTimeStampInMS);
        break;
      case "iccinfochange":
        this.radioState.icc = message;
        break;
      case "iccgetcardlock":
        this.handleICCGetCardLock(message);
        break;
      case "iccsetcardlock":
        this.handleICCSetCardLock(message);
        break;
      case "iccunlockcardlock":
        this.handleICCUnlockCardLock(message);
        break;
      case "icccontacts":
        if (!this._contactsCallbacks) {
          return;
        }
        let callback = this._contactsCallbacks[message.requestId];
        if (callback) {
          delete this._contactsCallbacks[message.requestId];
          callback.receiveContactsList(message.contactType, message.contacts);
        }
        break;
      case "celllocationchanged":
        this.radioState.cell = message;
        break;
      case "ussdreceived":
        debug("ussdreceived " + JSON.stringify(message));
        this.handleUSSDReceived(message);
        break;
      case "sendussd":
        this.handleSendUSSD(message);
        break;
      case "cancelussd":
        this.handleCancelUSSD(message);
        break;
      default:
        throw new Error("Don't know about this message type: " + message.type);
    }
  },

  updateVoiceConnection: function updateVoiceConnection(state) {
    let voiceInfo = this.radioState.voice;
    voiceInfo.type = "gsm"; //TODO see bug 726098.
    if (!state || state.regState == RIL.NETWORK_CREG_STATE_UNKNOWN) {
      voiceInfo.connected = false;
      voiceInfo.emergencyCallsOnly = false;
      voiceInfo.roaming = false;
      voiceInfo.operator = null;
      voiceInfo.type = null;
      voiceInfo.signalStrength = null;
      voiceInfo.relSignalStrength = null;
      ppmm.sendAsyncMessage("RIL:VoiceThis.RadioState.VoiceChanged",
                            voiceInfo);
      return;
    }
    voiceInfo.emergencyCallsOnly = state.emergencyCallsOnly;
    voiceInfo.connected =
      (state.regState == RIL.NETWORK_CREG_STATE_REGISTERED_HOME) ||
      (state.regState == RIL.NETWORK_CREG_STATE_REGISTERED_ROAMING);
    voiceInfo.roaming =
      voiceInfo.connected &&
      (state == RIL.NETWORK_CREG_STATE_REGISTERED_ROAMING);    
    voiceInfo.type =
      RIL.GECKO_RADIO_TECH[state.radioTech] || null;
    ppmm.sendAsyncMessage("RIL:VoiceInfoChanged", voiceInfo);

  },

  _isDataEnabled: function _isDataEnabled() {
    try {
      return Services.prefs.getBoolPref("ril.data.enabled");
    } catch(ex) {
      return false;
    }
  },

  _isDataRoamingEnabled: function _isDataRoamingEnabled() {
    try {
      return Services.prefs.getBoolPref("ril.data.roaming.enabled");
    } catch(ex) {
      return false;
    }
  },

  updateDataConnection: function updateDataConnection(state) {
    if (!this._isDataEnabled()) {
      return;
    }

    let isRegistered =
      state.regState == RIL.NETWORK_CREG_STATE_REGISTERED_HOME ||
        (this._isDataRoamingEnabled() &&
         state.regState == RIL.NETWORK_CREG_STATE_REGISTERED_ROAMING);
    let haveDataConnection =
      state.radioTech != RIL.NETWORK_CREG_TECH_UNKNOWN;

    if (isRegistered && haveDataConnection) {
      debug("Radio is ready for data connection.");
      // RILNetworkInterface will ignore this if it's already connected.
      RILNetworkInterface.connect();
    }
    //TODO need to keep track of some of the state information, and then
    // notify the content when state changes (connected, technology
    // changes, etc.). This should be done in RILNetworkInterface.
  },

  handleSignalStrengthChange: function handleSignalStrengthChange(message) {
    // TODO CDMA, EVDO, LTE, etc. (see bug 726098)
    this.radioState.voice.signalStrength = message.gsmDBM;
    this.radioState.voice.relSignalStrength = message.gsmRelative;
    ppmm.sendAsyncMessage("RIL:VoiceInfoChanged", this.radioState.voice);

    this.radioState.data.signalStrength = message.gsmDBM;
    this.radioState.data.relSignalStrength = message.gsmRelative;
    ppmm.sendAsyncMessage("RIL:DataInfoChanged", this.radioState.data);
  },

  handleOperatorChange: function handleOperatorChange(message) {
    let operator = message.alphaLong;
    if (operator != this.radioState.voice.operator) {
      this.radioState.voice.operator = operator;
      ppmm.sendAsyncMessage("RIL:VoiceInfoChanged", this.radioState.voice);
    }
    if (operator != this.radioState.data.operator) {
      this.radioState.data.operator = operator;
      ppmm.sendAsyncMessage("RIL:DataInfoChanged", this.radioState.data);
    }
  },

  handleRadioStateChange: function handleRadioStateChange(message) {
    let newState = message.radioState;
    if (this.radioState.radioState == newState) {
      return;
    }
    this.radioState.radioState = newState;
    //TODO Should we notify this change as a card state change?

    this._ensureRadioState();
  },

  _ensureRadioState: function _ensureRadioState() {
    debug("Reported radio state is " + this.radioState.radioState +
          ", desired radio enabled state is " + this._radioEnabled);
    if (this._radioEnabled == null) {
      // We haven't read the initial value from the settings DB yet.
      // Wait for that.
      return;
    }
    if (this.radioState.radioState == RIL.GECKO_RADIOSTATE_UNKNOWN) {
      // We haven't received a radio state notification from the RIL
      // yet. Wait for that.
      return;
    }

    if (this.radioState.radioState == RIL.GECKO_RADIOSTATE_OFF &&
        this._radioEnabled) {
      this.setRadioEnabled(true);
    }
    if (this.radioState.radioState == RIL.GECKO_RADIOSTATE_READY &&
        !this._radioEnabled) {
      this.setRadioEnabled(false);
    }
  },

  /**
   * Track the active call and update the audio system as its state changes.
   */
  _activeCall: null,
  updateCallAudioState: function updateCallAudioState() {
    if (!this._activeCall) {
      // Disable audio.
      gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_NORMAL;
      debug("No active call, put audio system into PHONE_STATE_NORMAL: "
            + gAudioManager.phoneState);
      return;
    }
    switch (this._activeCall.state) {
      case nsIRadioInterfaceLayer.CALL_STATE_INCOMING:
        gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_RINGTONE;
        debug("Incoming call, put audio system into PHONE_STATE_RINGTONE: "
              + gAudioManager.phoneState);
        break;
      case nsIRadioInterfaceLayer.CALL_STATE_DIALING: // Fall through...
      case nsIRadioInterfaceLayer.CALL_STATE_CONNECTED:
        gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_IN_CALL;
        gAudioManager.setForceForUse(nsIAudioManager.USE_COMMUNICATION,
                                     nsIAudioManager.FORCE_NONE);
        debug("Active call, put audio system into PHONE_STATE_IN_CALL: "
              + gAudioManager.phoneState);
        break;
    }
  },

  /**
   * Handle call state changes by updating our current state and the audio
   * system.
   */
  handleCallStateChange: function handleCallStateChange(call) {
    debug("handleCallStateChange: " + JSON.stringify(call));
    call.state = convertRILCallState(call.state);
    if (call.isActive) {
      this._activeCall = call;
    } else if (this._activeCall &&
               this._activeCall.callIndex == call.callIndex) {
      // Previously active call is not active now.
      this._activeCall = null;
    }
    this.updateCallAudioState();
    ppmm.sendAsyncMessage("RIL:CallStateChanged", call);
  },

  /**
   * Handle call disconnects by updating our current state and the audio system.
   */
  handleCallDisconnected: function handleCallDisconnected(call) {
    debug("handleCallDisconnected: " + JSON.stringify(call));
    if (call.isActive) {
      this._activeCall = null;
    }
    this.updateCallAudioState();
    call.state = nsIRadioInterfaceLayer.CALL_STATE_DISCONNECTED;
    ppmm.sendAsyncMessage("RIL:CallStateChanged", call);
  },

  /**
   * Handle calls delivered in response to a 'enumerateCalls' request.
   */
  handleEnumerateCalls: function handleEnumerateCalls(calls) {
    debug("handleEnumerateCalls: " + JSON.stringify(calls));
    for (let i in calls) {
      calls[i].state = convertRILCallState(calls[i].state);
    }
    ppmm.sendAsyncMessage("RIL:EnumerateCalls", calls);
  },

  /**
   * Handle available networks returned by the 'getAvailableNetworks' request.
   */
  handleGetAvailableNetworks: function handleGetAvailableNetworks(message) {
    debug("handleGetAvailableNetworks: " + JSON.stringify(message));

    ppmm.sendAsyncMessage("RIL:GetAvailableNetworks", message);
  },

  /**
   * Handle call error.
   */
  handleCallError: function handleCallError(message) {
    ppmm.sendAsyncMessage("RIL:CallError", message);   
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
      destinationAddress: this.radioState.icc.MSISDN,
      destinationPort: message.header.destinationPort,
    };
    WAP.WapPushManager.receiveWdpPDU(message.fullData, message.fullData.length,
                                     0, options);
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
      return;
    }

    if (message.encoding == RIL.PDU_DCS_MSG_CODING_8BITS_ALPHABET) {
      // Don't know how to handle binary data yet.
      return;
    }

    let id = gSmsDatabaseService.saveReceivedMessage(message.sender || null,
                                                     message.fullBody || null,
                                                     message.timestamp);
    let sms = gSmsService.createSmsMessage(id,
                                           DOM_SMS_DELIVERY_RECEIVED,
                                           message.sender || null,
                                           message.receiver || null,
                                           message.fullBody || null,
                                           message.timestamp,
                                           false);
    Services.obs.notifyObservers(sms, kSmsReceivedObserverTopic, null);
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
    options.envelopeId = i;
    this._sentSmsEnvelopes[i] = options;
  },

  handleSmsSent: function handleSmsSent(message) {
    debug("handleSmsSent: " + JSON.stringify(message));

    let options = this._sentSmsEnvelopes[message.envelopeId];
    if (!options) {
      return;
    }

    let timestamp = Date.now();
    let id = gSmsDatabaseService.saveSentMessage(options.number,
                                                 options.fullBody,
                                                 timestamp);
    let sms = gSmsService.createSmsMessage(id,
                                           DOM_SMS_DELIVERY_SENT,
                                           null,
                                           options.number,
                                           options.fullBody,
                                           timestamp,
                                           true);

    if (!options.requestStatusReport) {
      // No more used if STATUS-REPORT not requested.
      delete this._sentSmsEnvelopes[message.envelopeId];
    } else {
      options.sms = sms;
    }

    gSmsRequestManager.notifySmsSent(options.requestId, sms);
  },

  handleSmsDelivered: function handleSmsDelivered(message) {
    debug("handleSmsDelivered: " + JSON.stringify(message));

    let options = this._sentSmsEnvelopes[message.envelopeId];
    if (!options) {
      return;
    }
    delete this._sentSmsEnvelopes[message.envelopeId];

    Services.obs.notifyObservers(options.sms, kSmsDeliveredObserverTopic, null);
  },

  handleSmsSendFailed: function handleSmsSendFailed(message) {
    debug("handleSmsSendFailed: " + JSON.stringify(message));

    let options = this._sentSmsEnvelopes[message.envelopeId];
    if (!options) {
      return;
    }
    delete this._sentSmsEnvelopes[message.envelopeId];

    let error = gSmsRequestManager.UNKNOWN_ERROR;
    switch (message.error) {
      case RIL.ERROR_RADIO_NOT_AVAILABLE:
        error = gSmsRequestManager.NO_SIGNAL_ERROR;
        break;
    }

    gSmsRequestManager.notifySmsSendFailed(options.requestId, error);
  },

  /**
   * Handle data call state changes.
   */
  handleDataCallState: function handleDataCallState(datacall) {
    this._deliverDataCallCallback("dataCallStateChanged",
                                  [datacall.cid, datacall.ifname, datacall.state]);
  },

  /**
   * Handle data call list.
   */
  handleDataCallList: function handleDataCallList(message) {
    let datacalls = [];
    for each (let datacall in message.datacalls) {
      datacalls.push(new DataCallInfo(datacall.state,
                                      datacall.cid,
                                      datacall.apn));
    }
    this._deliverDataCallCallback("receiveDataCallList",
                                  [datacalls, datacalls.length]);
  },

  /**
   * Handle setting changes.
   */
  handleMozSettingsChanged: function handleMozSettingsChanged(setting) {
    switch (setting.key) {
      case "ril.radio.disabled":
        this._radioEnabled = !setting.value;
        this._ensureRadioState();
        break;
      case "ril.data.enabled":
        // We only watch at "ril.data.enabled" flag changes for connecting or
        // disconnecting the data call. If the value of "ril.data.enabled" is
        // true and any of the remaining flags change the setting application
        // should turn this flag to false and then to true in order to reload
        // the new values and reconnect the data call.
        if (!setting.value && RILNetworkInterface.connected) {
          debug("Data call settings: disconnect data call.");
          RILNetworkInterface.disconnect();
        }
        if (setting.value && !RILNetworkInterface.connected) {
          debug("Data call settings connect data call.");
          RILNetworkInterface.connect();
        }
        break;
    }
  },

  handleICCGetCardLock: function handleICCGetCardLock(message) {
    ppmm.sendAsyncMessage("RIL:GetCardLock:Return:OK", message);
  },

  handleICCSetCardLock: function handleICCSetCardLock(message) {
    ppmm.sendAsyncMessage("RIL:SetCardLock:Return:OK", message);
  },

  handleICCUnlockCardLock: function handleICCUnlockCardLock(message) {
    ppmm.sendAsyncMessage("RIL:UnlockCardLock:Return:OK", message);
  },

  handleUSSDReceived: function handleUSSDReceived(ussd) {
    debug("handleUSSDReceived " + JSON.stringify(ussd));
    ppmm.sendAsyncMessage("RIL:UssdReceived", ussd);
  },

  handleSendUSSD: function handleSendUSSD(message) {
    debug("handleSendUSSD " + JSON.stringify(message));
    let messageType = message.success ? "RIL:SendUssd:Return:OK" :
                                        "RIL:SendUssd:Return:KO";
    ppmm.sendAsyncMessage(messageType, message);
  },

  handleCancelUSSD: function handleCancelUSSD(message) {
    debug("handleCancelUSSD " + JSON.stringify(message));
    let messageType = message.success ? "RIL:CancelUssd:Return:OK" :
                                        "RIL:CancelUssd:Return:KO";
    ppmm.sendAsyncMessage(messageType, message);
  },

  // nsIObserver

  observe: function observe(subject, topic, data) {
    switch (topic) {
      case kMozSettingsChangedObserverTopic:
        let setting = JSON.parse(data);
        this.handleMozSettingsChanged(setting);
        break;
      case "xpcom-shutdown":
        for each (let msgname in RIL_IPC_MSG_NAMES) {
          ppmm.removeMessageListener(msgname, this);
        }
        RILNetworkInterface.shutdown();
        ppmm = null;
        Services.obs.removeObserver(this, "xpcom-shutdown");
        Services.obs.removeObserver(this, kMozSettingsChangedObserverTopic);
        break;
    }
  },

  // nsISettingsServiceCallback

  // Flag to determine the radio state to start with when we boot up. It
  // corresponds to the 'ril.radio.disabled' setting from the UI.
  _radioEnabled: null,

  handle: function handle(aName, aResult) {
    if (aName == "ril.radio.disabled") {
      debug("'ril.radio.disabled' is " + aResult);
      this._radioEnabled = !aResult;
      this._ensureRadioState();
    }
  },

  handleError: function handleError(aErrorMessage) {
    debug("There was an error reading the 'ril.radio.disabled' setting., " +
          "default to radio on.");
    this._radioEnabled = true;
    this._ensureRadioState();
  },

  // nsIRadioWorker

  worker: null,

  // nsIRadioInterfaceLayer

  setRadioEnabled: function setRadioEnabled(value) {
    debug("Setting radio power to " + value);
    this.worker.postMessage({type: "setRadioPower", on: value});
  },

  radioState: null,

  // Handle phone functions of nsIRILContentHelper

  enumerateCalls: function enumerateCalls() {
    debug("Requesting enumeration of calls for callback");
    this.worker.postMessage({type: "enumerateCalls"});
  },

  dial: function dial(number) {
    debug("Dialing " + number);
    this.worker.postMessage({type: "dial", number: number});
  },

  hangUp: function hangUp(callIndex) {
    debug("Hanging up call no. " + callIndex);
    this.worker.postMessage({type: "hangUp", callIndex: callIndex});
  },

  startTone: function startTone(dtmfChar) {
    debug("Sending Tone for " + dtmfChar);
    this.worker.postMessage({type: "startTone", dtmfChar: dtmfChar});
  },

  stopTone: function stopTone() {
    debug("Stopping Tone");
    this.worker.postMessage({type: "stopTone"});
  },

  answerCall: function answerCall(callIndex) {
    this.worker.postMessage({type: "answerCall", callIndex: callIndex});
  },

  rejectCall: function rejectCall(callIndex) {
    this.worker.postMessage({type: "rejectCall", callIndex: callIndex});
  },
 
  holdCall: function holdCall(callIndex) {
    this.worker.postMessage({type: "holdCall", callIndex: callIndex});
  },

  resumeCall: function resumeCall(callIndex) {
    this.worker.postMessage({type: "resumeCall", callIndex: callIndex});
  },

  getAvailableNetworks: function getAvailableNetworks(requestId) {
    this.worker.postMessage({type: "getAvailableNetworks", requestId: requestId});
  },

  sendUSSD: function sendUSSD(message) {
    debug("SendUSSD " + JSON.stringify(message));
    message.type = "sendUSSD";
    this.worker.postMessage(message);
  },

  cancelUSSD: function cancelUSSD(message) {
    debug("Cancel pending USSD");
    message.type = "cancelUSSD";
    this.worker.postMessage(message);
  },

  get microphoneMuted() {
    return gAudioManager.microphoneMuted;
  },
  set microphoneMuted(value) {
    if (value == this.microphoneMuted) {
      return;
    }
    gAudioManager.phoneState = value ?
      nsIAudioManager.PHONE_STATE_IN_COMMUNICATION :
      nsIAudioManager.PHONE_STATE_IN_CALL;  //XXX why is this needed?
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
    gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_IN_CALL; // XXX why is this needed?
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
   *
   * @return encoded length in septets.
   *
   * @note that the algorithm used in this function must match exactly with
   * GsmPDUHelper#writeStringAsSeptets.
   */
  _countGsm7BitSeptets: function _countGsm7BitSeptets(message, langTable, langShiftTable) {
    let length = 0;
    for (let msgIndex = 0; msgIndex < message.length; msgIndex++) {
      let septet = langTable.indexOf(message.charAt(msgIndex));

      // According to 3GPP TS 23.038, section 6.1.1 General notes, "The
      // characters marked '1)' are not used but are displayed as a space."
      if (septet == RIL.PDU_NL_EXTENDED_ESCAPE) {
        continue;
      }

      if (septet >= 0) {
        length++;
        continue;
      }

      septet = langShiftTable.indexOf(message.charAt(msgIndex));
      if (septet < 0) {
        return -1;
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
   *
   * @return null or an options object with attributes `dcs`,
   *         `userDataHeaderLength`, `encodedFullBodyLength`, `langIndex`,
   *         `langShiftIndex`, `segmentMaxSeq` set.
   *
   * @see #_calculateUserDataLength().
   */
  _calculateUserDataLength7Bit: function _calculateUserDataLength7Bit(message) {
    let options = null;
    let minUserDataSeptets = Number.MAX_VALUE;
    for (let i = 0; i < this.enabledGsmTableTuples.length; i++) {
      let [langIndex, langShiftIndex] = this.enabledGsmTableTuples[i];

      const langTable = RIL.PDU_NL_LOCKING_SHIFT_TABLES[langIndex];
      const langShiftTable = RIL.PDU_NL_SINGLE_SHIFT_TABLES[langShiftIndex];

      let bodySeptets = this._countGsm7BitSeptets(message,
                                                  langTable,
                                                  langShiftTable);
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
      let userDataSeptets = bodySeptets + headerSeptets;
      let segments = bodySeptets ? 1 : 0;
      if (userDataSeptets > RIL.PDU_MAX_USER_DATA_7BIT) {
        if (this.segmentRef16Bit) {
          headerLen += 6;
        } else {
          headerLen += 5;
        }

        headerSeptets = Math.ceil((headerLen + 1) * 8 / 7);
        let segmentSeptets = RIL.PDU_MAX_USER_DATA_7BIT - headerSeptets;
        segments = Math.ceil(bodySeptets / segmentSeptets);
        userDataSeptets = bodySeptets + headerSeptets * segments;
      }

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
    let segments = bodyChars ? 1 : 0;
    if ((bodyChars + headerChars) > RIL.PDU_MAX_USER_DATA_UCS2) {
      if (this.segmentRef16Bit) {
        headerLen += 6;
      } else {
        headerLen += 5;
      }

      headerChars = Math.ceil((headerLen + 1) / 2);
      let segmentChars = RIL.PDU_MAX_USER_DATA_UCS2 - headerChars;
      segments = Math.ceil(bodyChars / segmentChars);
    }

    return {
      dcs: RIL.PDU_DCS_MSG_CODING_16BITS_ALPHABET,
      encodedFullBodyLength: bodyChars * 2,
      userDataHeaderLength: headerLen,
      segmentMaxSeq: segments,
    };
  },

  /**
   * Calculate user data length and its encoding.
   *
   * @param message
   *        a message string to be encoded.
   *
   * @return an options object with some or all of following attributes set:
   *
   * @param dcs
   *        Data coding scheme. One of the PDU_DCS_MSG_CODING_*BITS_ALPHABET
   *        constants.
   * @param fullBody
   *        Original unfragmented text message.
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
  _calculateUserDataLength: function _calculateUserDataLength(message) {
    let options = this._calculateUserDataLength7Bit(message);
    if (!options) {
      options = this._calculateUserDataLengthUCS2(message);
    }

    if (options) {
      options.fullBody = message;
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
   * @param headerLen
   *        Length of prepended user data header.
   *
   * @return an array of objects. See #_fragmentText() for detailed definition.
   */
  _fragmentText7Bit: function _fragmentText7Bit(text, langTable, langShiftTable, headerLen) {
    const headerSeptets = Math.ceil((headerLen ? headerLen + 1 : 0) * 8 / 7);
    const segmentSeptets = RIL.PDU_MAX_USER_DATA_7BIT - headerSeptets;
    let ret = [];
    let begin = 0, len = 0;
    for (let i = 0, inc = 0; i < text.length; i++) {
      let septet = langTable.indexOf(text.charAt(i));
      if (septet == RIL.PDU_NL_EXTENDED_ESCAPE) {
        continue;
      }

      if (septet >= 0) {
        inc = 1;
      } else {
        septet = langShiftTable.indexOf(text.charAt(i));
        if (septet < 0) {
          throw new Error("Given text cannot be encoded with GSM 7-bit Alphabet!");
        }

        if (septet == RIL.PDU_NL_RESERVED_CONTROL) {
          continue;
        }

        inc = 2;
      }

      if ((len + inc) > segmentSeptets) {
        ret.push({
          body: text.substring(begin, i),
          encodedBodyLength: len,
        });
        begin = i;
        len = 0;
      }

      len += inc;
    }

    if (len) {
      ret.push({
        body: text.substring(begin),
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
   * @param headerLen
   *        Length of prepended user data header.
   *
   * @return an array of objects. See #_fragmentText() for detailed definition.
   */
  _fragmentTextUCS2: function _fragmentTextUCS2(text, headerLen) {
    const headerChars = Math.ceil((headerLen ? headerLen + 1 : 0) / 2);
    const segmentChars = RIL.PDU_MAX_USER_DATA_UCS2 - headerChars;
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
   *
   * @return Populated options object.
   */
  _fragmentText: function _fragmentText(text, options) {
    if (!options) {
      options = this._calculateUserDataLength(text);
    }

    if (options.segmentMaxSeq <= 1) {
      options.segments = null;
      return options;
    }

    if (options.dcs == RIL.PDU_DCS_MSG_CODING_7BITS_ALPHABET) {
      const langTable = RIL.PDU_NL_LOCKING_SHIFT_TABLES[options.langIndex];
      const langShiftTable = RIL.PDU_NL_SINGLE_SHIFT_TABLES[options.langShiftIndex];
      options.segments = this._fragmentText7Bit(options.fullBody,
                                                langTable, langShiftTable,
                                                options.userDataHeaderLength);
    } else {
      options.segments = this._fragmentTextUCS2(options.fullBody,
                                                options.userDataHeaderLength);
    }

    // Re-sync options.segmentMaxSeq with actual length of returning array.
    options.segmentMaxSeq = options.segments.length;

    return options;
  },

  getNumberOfMessagesForText: function getNumberOfMessagesForText(text) {
    return this._fragmentText(text).segmentMaxSeq;
  },

  sendSMS: function sendSMS(number, message, requestId, processId) {
    let options = this._calculateUserDataLength(message);
    options.type = "sendSMS";
    options.number = number;
    options.requestId = requestId;
    options.processId = processId;
    options.requestStatusReport = true;

    this._fragmentText(message, options);
    if (options.segmentMaxSeq > 1) {
      options.segmentRef16Bit = this.segmentRef16Bit;
      options.segmentRef = this.nextSegmentRef;
    }

    // Keep current SMS message info for sent/delivered notifications
    this.createSmsEnvelope(options);

    this.worker.postMessage(options);
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
    for each (let callback in callbacks) {
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

  setupDataCall: function setupDataCall(radioTech, apn, user, passwd, chappap, pdptype) {
    this.worker.postMessage({type: "setupDataCall",
                             radioTech: radioTech,
                             apn: apn,
                             user: user,
                             passwd: passwd,
                             chappap: chappap,
                             pdptype: pdptype});
  },

  deactivateDataCall: function deactivateDataCall(cid, reason) {
    this.worker.postMessage({type: "deactivateDataCall",
                             cid: cid,
                             reason: reason});
  },

  getDataCallList: function getDataCallList() {
    this.worker.postMessage({type: "getDataCallList"});
  },

  getCardLock: function getCardLock(message) {
    // Currently only support pin.
    switch (message.lockType) {
      case "pin" :
        message.type = "getICCPinLock";
        break;
      default:
        ppmm.sendAsyncMessage("RIL:GetCardLock:Return:KO",
                              {errorMsg: "Unsupported Card Lock.",
                               requestId: message.requestId});
        return;
    }
    this.worker.postMessage(message);
  },

  unlockCardLock: function unlockCardLock(message) {
    switch (message.lockType) {
      case "pin":
        message.type = "enterICCPIN";
        break;
      case "pin2":
        message.type = "enterICCPIN2";
        break;
      case "puk":
        message.type = "enterICCPUK";
        break;
      case "puk2":
        message.type = "enterICCPUK2";
        break;
      default:
        ppmm.sendAsyncMessage("RIL:UnlockCardLock:Return:KO",
                              {errorMsg: "Unsupported Card Lock.",
                               requestId: message.requestId});
        return;
    }
    this.worker.postMessage(message);
  },

  setCardLock: function setCardLock(message) {
    // Change pin.
    if (message.newPin !== undefined) {
      switch (message.lockType) {
        case "pin":
          message.type = "changeICCPIN";
          break;
        case "pin2":
          message.type = "changeICCPIN2";
          break;
        default:
          ppmm.sendAsyncMessage("RIL:SetCardLock:Return:KO",
                                {errorMsg: "Unsupported Card Lock.",
                                 requestId: message.requestId});
          return;
      }
    } else { // Enable/Disable pin lock.
      if (message.lockType != "pin") {
          ppmm.sendAsyncMessage("RIL:SetCardLock:Return:KO",
                                {errorMsg: "Unsupported Card Lock.",
                                 requestId: message.requestId});
      }
      message.type = "setICCPinLock";
    }
    this.worker.postMessage(message);
  },

  _contactsCallbacks: null,
  getICCContacts: function getICCContacts(type, callback) {
    if (!this._contactsCallbacks) {
      this._contactsCallbacks = {};
    } 
    let requestId = Math.floor(Math.random() * 1000);
    this._contactsCallbacks[requestId] = callback;
    
    let msgType;
    switch (type) {
      case "ADN": 
        msgType = "getPBR";
        break;
      case "FDN":
        msgType = "getFDN";
        break;
      default:
        debug("Unknown contact type. " + type);
        return;
    }
    this.worker.postMessage({type: msgType, requestId: requestId});
  }
};

let RILNetworkInterface = {

  QueryInterface: XPCOMUtils.generateQI([Ci.nsINetworkInterface,
                                         Ci.nsIRILDataCallback]),

  // nsINetworkInterface

  NETWORK_STATE_UNKNOWN:       Ci.nsINetworkInterface.NETWORK_STATE_UNKNOWN,
  NETWORK_STATE_CONNECTING:    Ci.nsINetworkInterface.CONNECTING,
  NETWORK_STATE_CONNECTED:     Ci.nsINetworkInterface.CONNECTED,
  NETWORK_STATE_SUSPENDED:     Ci.nsINetworkInterface.SUSPENDED,
  NETWORK_STATE_DISCONNECTING: Ci.nsINetworkInterface.DISCONNECTING,
  NETWORK_STATE_DISCONNECTED:  Ci.nsINetworkInterface.DISCONNECTED,

  state: Ci.nsINetworkInterface.NETWORK_STATE_UNKNOWN,

  NETWORK_TYPE_WIFI:       Ci.nsINetworkInterface.NETWORK_TYPE_WIFI,
  NETWORK_TYPE_MOBILE:     Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE,
  NETWORK_TYPE_MOBILE_MMS: Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS,

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

  // nsIRILDataCallback

  dataCallStateChanged: function dataCallStateChanged(cid, interfaceName, callState) {
    debug("Data call ID: " + cid + ", interface name: " + interfaceName);
    if (this.connecting &&
        (callState == RIL.GECKO_NETWORK_STATE_CONNECTING ||
         callState == RIL.GECKO_NETWORK_STATE_CONNECTED)) {
      this.connecting = false;
      this.cid = cid;
      this.name = interfaceName;
      if (!this.registeredAsNetworkInterface) {
        let networkManager = Cc["@mozilla.org/network/manager;1"]
                               .getService(Ci.nsINetworkManager);
        networkManager.registerNetworkInterface(this);
        this.registeredAsNetworkInterface = true;
      }
    }
    if (this.cid != cid) {
      return;
    }
    if (this.state == callState) {
      return;
    }

    this.state = callState;
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

  // APN failed connections. Retry counter
  apnRetryCounter: 0,

  get mRIL() {
    delete this.mRIL;
    return this.mRIL = Cc["@mozilla.org/telephony/system-worker-manager;1"]
                         .getService(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIRadioInterfaceLayer);
  },

  get connected() {
    return this.state == RIL.GECKO_NETWORK_STATE_CONNECTED;
  },

  connect: function connect() {
    if (this.connecting ||
        this.state == RIL.GECKO_NETWORK_STATE_CONNECTED ||
        this.state == RIL.GECKO_NETWORK_STATE_SUSPENDED) {
      return;
    }
    if (!this.registeredAsDataCallCallback) {
      this.mRIL.registerDataCallCallback(this);
      this.registeredAsDataCallCallback = true;
    }

    let apn, user, passwd;
    // Eventually these values would be retrieved from the user's preferences
    // via the settings API. For now we just use Gecko's preferences.
    try {
      apn = Services.prefs.getCharPref("ril.data.apn");
      user = Services.prefs.getCharPref("ril.data.user");
      passwd = Services.prefs.getCharPref("ril.data.passwd");
    } catch (ex) {
      debug("No APN settings found, not going to set up data connection.");
      return;
    }
    debug("Going to set up data connection with APN " + apn);
    this.mRIL.setupDataCall(RIL.DATACALL_RADIOTECHNOLOGY_GSM,
                            apn, user, passwd,
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
    RILNetworkInterface.connect();
  },

  shutdown: function() {
    this.timer = null;
  }

};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([RadioInterfaceLayer]);

let debug;
if (DEBUG) {
  debug = function (s) {
    dump("-*- RadioInterfaceLayer: " + s + "\n");
  };
} else {
  debug = function (s) {};
}
