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

function debug(s) {
  dump("-*- RadioInterfaceLayer: " + s + "\n");
}

const RADIOINTERFACELAYER_CID =
  Components.ID("{2d831c8d-6017-435b-a80c-e5d422810cea}");
const RADIOINTERFACE_CID =
  Components.ID("{6a7c91f0-a2b3-4193-8562-8969296c0b54}");
const RILNETWORKINTERFACE_CID =
  Components.ID("{3bdd52a9-3965-4130-b569-0ac5afed045e}");

const nsIAudioManager = Ci.nsIAudioManager;
const nsITelephonyProvider = Ci.nsITelephonyProvider;

const kNetworkInterfaceStateChangedTopic = "network-interface-state-changed";
const kSmsReceivedObserverTopic          = "sms-received";
const kSilentSmsReceivedObserverTopic    = "silent-sms-received";
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
const kClirModePreference                = "ril.clirMode";

const DOM_MOBILE_MESSAGE_DELIVERY_RECEIVED = "received";
const DOM_MOBILE_MESSAGE_DELIVERY_SENDING  = "sending";
const DOM_MOBILE_MESSAGE_DELIVERY_SENT     = "sent";
const DOM_MOBILE_MESSAGE_DELIVERY_ERROR    = "error";

const CALL_WAKELOCK_TIMEOUT              = 5000;
const RADIO_POWER_OFF_TIMEOUT            = 30000;

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
  "RIL:RegisterTelephonyMsg",
  "RIL:ConferenceCall",
  "RIL:SeparateCall",
  "RIL:HoldConference",
  "RIL:ResumeConference"
];

const RIL_IPC_MOBILECONNECTION_MSG_NAMES = [
  "RIL:GetRilContext",
  "RIL:GetAvailableNetworks",
  "RIL:SelectNetwork",
  "RIL:SelectNetworkAuto",
  "RIL:SendMMI",
  "RIL:CancelMMI",
  "RIL:RegisterMobileConnectionMsg",
  "RIL:SetCallForwardingOption",
  "RIL:GetCallForwardingOption",
  "RIL:SetCallBarringOption",
  "RIL:GetCallBarringOption",
  "RIL:ChangeCallBarringPassword",
  "RIL:SetCallWaitingOption",
  "RIL:GetCallWaitingOption",
  "RIL:SetCallingLineIdRestriction",
  "RIL:GetCallingLineIdRestriction",
  "RIL:SetRoamingPreference",
  "RIL:GetRoamingPreference",
  "RIL:ExitEmergencyCbMode",
  "RIL:SetVoicePrivacyMode",
  "RIL:GetVoicePrivacyMode"
];

const RIL_IPC_ICCMANAGER_MSG_NAMES = [
  "RIL:SendStkResponse",
  "RIL:SendStkMenuSelection",
  "RIL:SendStkTimerExpiration",
  "RIL:SendStkEventDownload",
  "RIL:GetCardLockState",
  "RIL:UnlockCardLock",
  "RIL:SetCardLock",
  "RIL:GetCardLockRetryCount",
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

XPCOMUtils.defineLazyServiceGetter(this, "gSmsService",
                                   "@mozilla.org/sms/smsservice;1",
                                   "nsISmsService");

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
    default:
      throw new Error("Unknown rilCallState: " + state);
  }
}

function convertRILSuppSvcNotification(notification) {
  switch (notification) {
    case RIL.GECKO_SUPP_SVC_NOTIFICATION_REMOTE_HELD:
      return nsITelephonyProvider.NOTIFICATION_REMOTE_HELD;
    case RIL.GECKO_SUPP_SVC_NOTIFICATION_REMOTE_RESUMED:
      return nsITelephonyProvider.NOTIFICATION_REMOTE_RESUMED;
    default:
      throw new Error("Unknown rilSuppSvcNotification: " + notification);
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
    if (DEBUG) debug("Using fake audio manager.");
    return FakeAudioManager;
  }
});

XPCOMUtils.defineLazyGetter(this, "gMessageManager", function () {
  return {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIMessageListener,
                                           Ci.nsIObserver]),

    ril: null,

    // Manage message targets in terms of topic. Only the authorized and
    // registered contents can receive related messages.
    targetsByTopic: {},
    topics: [],

    targetMessageQueue: [],
    ready: false,

    init: function init(ril) {
      this.ril = ril;

      Services.obs.addObserver(this, "xpcom-shutdown", false);
      Services.obs.addObserver(this, kSysMsgListenerReadyObserverTopic, false);
      this._registerMessageListeners();
    },

    _shutdown: function _shutdown() {
      this.ril = null;

      Services.obs.removeObserver(this, "xpcom-shutdown");
      this._unregisterMessageListeners();
    },

    _registerMessageListeners: function _registerMessageListeners() {
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
    },

    _unregisterMessageListeners: function _unregisterMessageListeners() {
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
      ppmm = null;
    },

    _registerMessageTarget: function _registerMessageTarget(topic, target) {
      let targets = this.targetsByTopic[topic];
      if (!targets) {
        targets = this.targetsByTopic[topic] = [];
        let list = this.topics;
        if (list.indexOf(topic) == -1) {
          list.push(topic);
        }
      }

      if (targets.indexOf(target) != -1) {
        if (DEBUG) debug("Already registered this target!");
        return;
      }

      targets.push(target);
      if (DEBUG) debug("Registered " + topic + " target: " + target);
    },

    _unregisterMessageTarget: function _unregisterMessageTarget(topic, target) {
      if (topic == null) {
        // Unregister the target for every topic when no topic is specified.
        for (let type of this.topics) {
          this._unregisterMessageTarget(type, target);
        }
        return;
      }

      // Unregister the target for a specified topic.
      let targets = this.targetsByTopic[topic];
      if (!targets) {
        return;
      }

      let index = targets.indexOf(target);
      if (index != -1) {
        targets.splice(index, 1);
        if (DEBUG) debug("Unregistered " + topic + " target: " + target);
      }
    },

    _enqueueTargetMessage: function _enqueueTargetMessage(topic, message, options) {
      let msg = { topic : topic,
                  message : message,
                  options : options };
      // Remove previous queued message of same message type, only one message
      // per message type is allowed in queue.
      let messageQueue = this.targetMessageQueue;
      for(let i = 0; i < messageQueue.length; i++) {
        if (messageQueue[i].message === message) {
          messageQueue.splice(i, 1);
          break;
        }
      }

      messageQueue.push(msg);
    },

    _sendTargetMessage: function _sendTargetMessage(topic, message, options) {
      if (!this.ready) {
        this._enqueueTargetMessage(topic, message, options);
        return;
      }

      let targets = this.targetsByTopic[topic];
      if (!targets) {
        return;
      }

      for (let target of targets) {
        target.sendAsyncMessage(message, options);
      }
    },

    _resendQueuedTargetMessage: function _resendQueuedTargetMessage() {
      this.ready = true;

      // Here uses this._sendTargetMessage() to resend message, which will
      // enqueue message if listener is not ready.
      // So only resend after listener is ready, or it will cause infinate loop and
      // hang the system.

      // Dequeue and resend messages.
      for each (let msg in this.targetMessageQueue) {
        this._sendTargetMessage(msg.topic, msg.message, msg.options);
      }
      this.targetMessageQueue = null;
    },

    /**
     * nsIMessageListener interface methods.
     */

    receiveMessage: function receiveMessage(msg) {
      if (DEBUG) debug("Received '" + msg.name + "' message from content process");
      if (msg.name == "child-process-shutdown") {
        // By the time we receive child-process-shutdown, the child process has
        // already forgotten its permissions so we need to unregister the target
        // for every permission.
        this._unregisterMessageTarget(null, msg.target);
        return;
      }

      if (RIL_IPC_TELEPHONY_MSG_NAMES.indexOf(msg.name) != -1) {
        if (!msg.target.assertPermission("telephony")) {
          if (DEBUG) {
            debug("Telephony message " + msg.name +
                  " from a content process with no 'telephony' privileges.");
          }
          return null;
        }
      } else if (RIL_IPC_MOBILECONNECTION_MSG_NAMES.indexOf(msg.name) != -1) {
        if (!msg.target.assertPermission("mobileconnection")) {
          if (DEBUG) {
            debug("MobileConnection message " + msg.name +
                  " from a content process with no 'mobileconnection' privileges.");
          }
          return null;
        }
      } else if (RIL_IPC_ICCMANAGER_MSG_NAMES.indexOf(msg.name) != -1) {
        if (!msg.target.assertPermission("mobileconnection")) {
          if (DEBUG) {
            debug("IccManager message " + msg.name +
                  " from a content process with no 'mobileconnection' privileges.");
          }
          return null;
        }
      } else if (RIL_IPC_VOICEMAIL_MSG_NAMES.indexOf(msg.name) != -1) {
        if (!msg.target.assertPermission("voicemail")) {
          if (DEBUG) {
            debug("Voicemail message " + msg.name +
                  " from a content process with no 'voicemail' privileges.");
          }
          return null;
        }
      } else if (RIL_IPC_CELLBROADCAST_MSG_NAMES.indexOf(msg.name) != -1) {
        if (!msg.target.assertPermission("cellbroadcast")) {
          if (DEBUG) {
            debug("Cell Broadcast message " + msg.name +
                  " from a content process with no 'cellbroadcast' privileges.");
          }
          return null;
        }
      } else {
        if (DEBUG) debug("Ignoring unknown message type: " + msg.name);
        return null;
      }

      switch (msg.name) {
        case "RIL:RegisterTelephonyMsg":
          this._registerMessageTarget("telephony", msg.target);
          return;
        case "RIL:RegisterMobileConnectionMsg":
          this._registerMessageTarget("mobileconnection", msg.target);
          return;
        case "RIL:RegisterIccMsg":
          this._registerMessageTarget("icc", msg.target);
          return;
        case "RIL:RegisterVoicemailMsg":
          this._registerMessageTarget("voicemail", msg.target);
          return;
        case "RIL:RegisterCellBroadcastMsg":
          this._registerMessageTarget("cellbroadcast", msg.target);
          return;
      }

      let clientId = msg.json.clientId || 0;
      let radioInterface = this.ril.getRadioInterface(clientId);
      if (!radioInterface) {
        if (DEBUG) debug("No such radio interface: " + clientId);
        return null;
      }

      return radioInterface.receiveMessage(msg);
    },

    /**
     * nsIObserver interface methods.
     */

    observe: function observe(subject, topic, data) {
      switch (topic) {
        case kSysMsgListenerReadyObserverTopic:
          Services.obs.removeObserver(this, kSysMsgListenerReadyObserverTopic);
          this._resendQueuedTargetMessage();
          break;
        case "xpcom-shutdown":
          this._shutdown();
          break;
      }
    },

    sendTelephonyMessage: function sendTelephonyMessage(message, clientId, data) {
      this._sendTargetMessage("telephony", message, {
        clientId: clientId,
        data: data
      });
    },

    sendMobileConnectionMessage: function sendMobileConnectionMessage(message, clientId, data) {
      this._sendTargetMessage("mobileconnection", message, {
        clientId: clientId,
        data: data
      });
    },

    sendVoicemailMessage: function sendVoicemailMessage(message, clientId, data) {
      this._sendTargetMessage("voicemail", message, {
        clientId: clientId,
        data: data
      });
    },

    sendCellBroadcastMessage: function sendCellBroadcastMessage(message, clientId, data) {
      this._sendTargetMessage("cellbroadcast", message, {
        clientId: clientId,
        data: data
      });
    },

    sendIccMessage: function sendIccMessage(message, clientId, data) {
      this._sendTargetMessage("icc", message, {
        clientId: clientId,
        data: data
      });
    }
  };
});

function RadioInterfaceLayer() {
  gMessageManager.init(this);

  let options = {
    debug: debugPref,
    cellBroadcastDisabled: false,
    clirMode: RIL.CLIR_DEFAULT
  };

  try {
    options.cellBroadcastDisabled =
      Services.prefs.getBoolPref(kCellBroadcastDisabled);
  } catch(e) {}

  try {
    options.clirMode = Services.prefs.getIntPref(kClirModePreference);
  } catch(e) {}

  let numIfaces = this.numRadioInterfaces;
  debug(numIfaces + " interfaces");
  this.radioInterfaces = [];
  for (let clientId = 0; clientId < numIfaces; clientId++) {
    options.clientId = clientId;
    this.radioInterfaces.push(new RadioInterface(options));
  }
}
RadioInterfaceLayer.prototype = {

  classID:   RADIOINTERFACELAYER_CID,
  classInfo: XPCOMUtils.generateCI({classID: RADIOINTERFACELAYER_CID,
                                    classDescription: "RadioInterfaceLayer",
                                    interfaces: [Ci.nsIRadioInterfaceLayer]}),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIRadioInterfaceLayer,
                                         Ci.nsIObserver]),

  /**
   * nsIObserver interface methods.
   */

  observe: function observe(subject, topic, data) {
    // Nothing to do now. Just for profile-after-change.
  },

  /**
   * nsIRadioInterfaceLayer interface methods.
   */

  getRadioInterface: function getRadioInterface(clientId) {
    return this.radioInterfaces[clientId];
  }
};

XPCOMUtils.defineLazyGetter(RadioInterfaceLayer.prototype,
                            "numRadioInterfaces", function () {
  try {
    return Services.prefs.getIntPref("ril.numRadioInterfaces");
  } catch (e) {
    return 1;
  }
});

function WorkerMessenger(radioInterface, options) {
  // Initial owning attributes.
  this.radioInterface = radioInterface;
  this.tokenCallbackMap = {};

  // Add a convenient alias to |radioInterface.debug()|.
  this.debug = radioInterface.debug.bind(radioInterface);

  if (DEBUG) this.debug("Starting RIL Worker[" + options.clientId + "]");
  this.worker = new ChromeWorker("resource://gre/modules/ril_worker.js");
  this.worker.onerror = this.onerror.bind(this);
  this.worker.onmessage = this.onmessage.bind(this);

  this.send("setInitialOptions", options);

  gSystemWorkerManager.registerRilWorker(options.clientId, this.worker);
}
WorkerMessenger.prototype = {
  radioInterface: null,
  worker: null,

  // This gets incremented each time we send out a message.
  token: 1,

  // Maps tokens we send out with messages to the message callback.
  tokenCallbackMap: null,

  onerror: function onerror(event) {
    if (DEBUG) {
      this.debug("Got an error: " + event.filename + ":" +
                 event.lineno + ": " + event.message + "\n");
    }
    event.preventDefault();
  },

  /**
   * Process the incoming message from the RIL worker.
   */
  onmessage: function onmessage(event) {
    let message = event.data;
    if (DEBUG) {
      this.debug("Received message from worker: " + JSON.stringify(message));
    }

    let token = message.rilMessageToken;
    if (token == null) {
      // That's an unsolicited message.  Pass to RadioInterface directly.
      this.radioInterface.handleUnsolicitedWorkerMessage(message);
      return;
    }

    let callback = this.tokenCallbackMap[message.rilMessageToken];
    if (!callback) {
      if (DEBUG) this.debug("Ignore orphan token: " + message.rilMessageToken);
      return;
    }

    let keep = false;
    try {
      keep = callback(message);
    } catch(e) {
      if (DEBUG) this.debug("callback throws an exception: " + e);
    }

    if (!keep) {
      delete this.tokenCallbackMap[message.rilMessageToken];
    }
  },

  /**
   * Send arbitrary message to worker.
   *
   * @param rilMessageType
   *        A text message type.
   * @param message [optional]
   *        An optional message object to send.
   * @param callback [optional]
   *        An optional callback function which is called when worker replies
   *        with an message containing a 'rilMessageToken' attribute of the
   *        same value we passed.  This callback function accepts only one
   *        parameter -- the reply from worker.  It also returns a boolean
   *        value true to keep current token-callback mapping and wait for
   *        another worker reply, or false to remove the mapping.
   */
  send: function send(rilMessageType, message, callback) {
    message = message || {};

    message.rilMessageToken = this.token;
    this.token++;

    if (callback) {
      // Only create the map if callback is provided.  For sending a request
      // and intentionally leaving the callback undefined, that reply will
      // be dropped in |this.onmessage| because of that orphan token.
      //
      // For sending a request that never replied at all, we're fine with this
      // because no callback shall be passed and we leave nothing to be cleaned
      // up later.
      this.tokenCallbackMap[message.rilMessageToken] = callback;
    }

    message.rilMessageType = rilMessageType;
    this.worker.postMessage(message);
  },

  /**
   * Send message to worker and return worker reply to RILContentHelper.
   *
   * @param msg
   *        A message object from ppmm.
   * @param rilMessageType
   *        A text string for worker message type.
   * @param ipcType [optinal]
   *        A text string for ipc message type. 'msg.name' if omitted.
   *
   * @TODO: Bug 815526 - deprecate RILContentHelper.
   */
  sendWithIPCMessage: function sendWithIPCMessage(msg, rilMessageType, ipcType) {
    this.send(rilMessageType, msg.json.data, function(reply) {
      ipcType = ipcType || msg.name;
      msg.target.sendAsyncMessage(ipcType, reply);
      return false;
    });
  }
};

function RadioInterface(options) {
  this.clientId = options.clientId;
  this.workerMessenger = new WorkerMessenger(this, options);

  this.dataCallSettings = {
    oldEnabled: false,
    enabled: false,
    roamingEnabled: false
  };

  // apnSettings is used to keep all APN settings.
  // byApn[] makes it easier to get the APN settings via APN, user
  // name, and password.
  // byType[] makes it easier to get the APN settings via APN types.
  this.apnSettings = {
      byType: {},
      byAPN: {}
  };

  this.rilContext = {
    radioState:     RIL.GECKO_RADIOSTATE_UNAVAILABLE,
    cardState:      RIL.GECKO_CARDSTATE_UNKNOWN,
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
                     cell: null,
                     type: null,
                     signalStrength: null,
                     relSignalStrength: null},
    data:           {connected: false,
                     emergencyCallsOnly: false,
                     roaming: false,
                     network: null,
                     cell: null,
                     type: null,
                     signalStrength: null,
                     relSignalStrength: null},
  };

  this.voicemailInfo = {
    number: null,
    displayName: null
  };

  this.operatorInfo = {};

  // Read the 'ril.radio.disabled' setting in order to start with a known
  // value at boot time.
  let lock = gSettingsService.createLock();
  lock.get("ril.radio.disabled", this);

  // Read preferred network type from the setting DB.
  lock.get("ril.radio.preferredNetworkType", this);

  // Read the APN data from the settings DB.
  lock.get("ril.data.roaming_enabled", this);
  lock.get("ril.data.enabled", this);
  lock.get("ril.data.apnSettings", this);

  // Read the 'time.nitz.automatic-update.enabled' setting to see if
  // we need to adjust the system clock time and time zone by NITZ.
  lock.get(kTimeNitzAutomaticUpdateEnabled, this);

  // Set "time.nitz.available" to false when starting up.
  this.setNitzAvailable(false);

  // Read the Cell Broadcast Search List setting, string of integers or integer
  // ranges separated by comma, to set listening channels.
  lock.get(kCellBroadcastSearchList, this);

  Services.obs.addObserver(this, "xpcom-shutdown", false);
  Services.obs.addObserver(this, kMozSettingsChangedObserverTopic, false);
  Services.obs.addObserver(this, kSysMsgListenerReadyObserverTopic, false);
  Services.obs.addObserver(this, kSysClockChangeObserverTopic, false);
  Services.obs.addObserver(this, kScreenStateChangedTopic, false);

  Services.prefs.addObserver(kCellBroadcastDisabled, this, false);

  this.portAddressedSmsApps = {};
  this.portAddressedSmsApps[WAP.WDP_PORT_PUSH] = this.handleSmsWdpPortPush.bind(this);
}
RadioInterface.prototype = {

  classID:   RADIOINTERFACE_CID,
  classInfo: XPCOMUtils.generateCI({classID: RADIOINTERFACE_CID,
                                    classDescription: "RadioInterface",
                                    interfaces: [Ci.nsIRadioInterface]}),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIRadioInterface,
                                         Ci.nsIObserver,
                                         Ci.nsISettingsServiceCallback]),

  // A private WorkerMessenger instance.
  workerMessenger: null,

  debug: function debug(s) {
    dump("-*- RadioInterface[" + this.clientId + "]: " + s + "\n");
  },

  /**
   * A utility function to copy objects. The srcInfo may contain
   * 'rilMessageType', should ignore it.
   */
  updateInfo: function updateInfo(srcInfo, destInfo) {
    for (let key in srcInfo) {
      if (key === 'rilMessageType') {
        continue;
      }
      destInfo[key] = srcInfo[key];
    }
  },

  /**
   * A utility function to compare objects. The srcInfo may contain
   * 'rilMessageType', should ignore it.
   */
  isInfoChanged: function isInfoChanged(srcInfo, destInfo) {
    if (!destInfo) {
      return true;
    }

    for (let key in srcInfo) {
      if (key === 'rilMessageType') {
        continue;
      }
      if (srcInfo[key] !== destInfo[key]) {
        return true;
      }
    }

    return false;
  },

  /**
   * Process a message from the content process.
   */
  receiveMessage: function receiveMessage(msg) {
    switch (msg.name) {
      case "RIL:GetRilContext":
        // This message is sync.
        return this.rilContext;
      case "RIL:EnumerateCalls":
        this.enumerateCalls(msg.target, msg.json.data);
        break;
      case "RIL:GetMicrophoneMuted":
        // This message is sync.
        return this.microphoneMuted;
      case "RIL:SetMicrophoneMuted":
        this.microphoneMuted = msg.json.data;
        break;
      case "RIL:GetSpeakerEnabled":
        // This message is sync.
        return this.speakerEnabled;
      case "RIL:SetSpeakerEnabled":
        this.speakerEnabled = msg.json.data;
        break;
      case "RIL:StartTone":
        this.workerMessenger.send("startTone", { dtmfChar: msg.json.data });
        break;
      case "RIL:StopTone":
        this.workerMessenger.send("stopTone");
        break;
      case "RIL:Dial":
        this.dial(msg.json.data);
        break;
      case "RIL:DialEmergency":
        this.dialEmergency(msg.json.data);
        break;
      case "RIL:HangUp":
        this.workerMessenger.send("hangUp", { callIndex: msg.json.data });
        break;
      case "RIL:AnswerCall":
        this.workerMessenger.send("answerCall", { callIndex: msg.json.data });
        break;
      case "RIL:RejectCall":
        this.workerMessenger.send("rejectCall", { callIndex: msg.json.data });
        break;
      case "RIL:HoldCall":
        this.workerMessenger.send("holdCall", { callIndex: msg.json.data });
        break;
      case "RIL:ResumeCall":
        this.workerMessenger.send("resumeCall", { callIndex: msg.json.data });
        break;
      case "RIL:ConferenceCall":
        this.workerMessenger.send("conferenceCall");
        break;
      case "RIL:SeparateCall":
        this.workerMessenger.send("separateCall",
                                  { callIndex: msg.json.data });
        break;
      case "RIL:HoldConference":
        this.workerMessenger.send("holdConference");
        break;
      case "RIL:ResumeConference":
        this.workerMessenger.send("resumeConference");
        break;
      case "RIL:GetAvailableNetworks":
        this.workerMessenger.sendWithIPCMessage(msg, "getAvailableNetworks");
        break;
      case "RIL:SelectNetwork":
        this.workerMessenger.sendWithIPCMessage(msg, "selectNetwork");
        break;
      case "RIL:SelectNetworkAuto":
        this.workerMessenger.sendWithIPCMessage(msg, "selectNetworkAuto");
        break;
      case "RIL:GetCardLockState":
        this.workerMessenger.sendWithIPCMessage(msg, "iccGetCardLockState",
                                                "RIL:CardLockResult");
        break;
      case "RIL:UnlockCardLock":
        this.workerMessenger.sendWithIPCMessage(msg, "iccUnlockCardLock",
                                                "RIL:CardLockResult");
        break;
      case "RIL:SetCardLock":
        this.workerMessenger.sendWithIPCMessage(msg, "iccSetCardLock",
                                                "RIL:CardLockResult");
        break;
      case "RIL:GetCardLockRetryCount":
        this.workerMessenger.sendWithIPCMessage(msg, "iccGetCardLockRetryCount",
                                                "RIL:CardLockRetryCount");
        break;
      case "RIL:SendMMI":
        this.sendMMI(msg.target, msg.json.data);
        break;
      case "RIL:CancelMMI":
        this.workerMessenger.sendWithIPCMessage(msg, "cancelUSSD");
        break;
      case "RIL:SendStkResponse":
        this.workerMessenger.send("sendStkTerminalResponse", msg.json.data);
        break;
      case "RIL:SendStkMenuSelection":
        this.workerMessenger.send("sendStkMenuSelection", msg.json.data);
        break;
      case "RIL:SendStkTimerExpiration":
        this.workerMessenger.send("sendStkTimerExpiration", msg.json.data);
        break;
      case "RIL:SendStkEventDownload":
        this.workerMessenger.send("sendStkEventDownload", msg.json.data);
        break;
      case "RIL:IccOpenChannel":
        this.workerMessenger.sendWithIPCMessage(msg, "iccOpenChannel");
        break;
      case "RIL:IccCloseChannel":
        this.workerMessenger.sendWithIPCMessage(msg, "iccCloseChannel");
        break;
      case "RIL:IccExchangeAPDU":
        this.workerMessenger.sendWithIPCMessage(msg, "iccExchangeAPDU");
        break;
      case "RIL:ReadIccContacts":
        this.workerMessenger.sendWithIPCMessage(msg, "readICCContacts");
        break;
      case "RIL:UpdateIccContact":
        this.workerMessenger.sendWithIPCMessage(msg, "updateICCContact");
        break;
      case "RIL:SetCallForwardingOption":
        this.setCallForwardingOption(msg.target, msg.json.data);
        break;
      case "RIL:GetCallForwardingOption":
        this.workerMessenger.sendWithIPCMessage(msg, "queryCallForwardStatus");
        break;
      case "RIL:SetCallBarringOption":
        this.workerMessenger.sendWithIPCMessage(msg, "setCallBarring");
        break;
      case "RIL:GetCallBarringOption":
        this.workerMessenger.sendWithIPCMessage(msg, "queryCallBarringStatus");
        break;
      case "RIL:ChangeCallBarringPassword":
        this.workerMessenger.sendWithIPCMessage(msg, "changeCallBarringPassword");
        break;
      case "RIL:SetCallWaitingOption":
        this.workerMessenger.sendWithIPCMessage(msg, "setCallWaiting");
        break;
      case "RIL:GetCallWaitingOption":
        this.workerMessenger.sendWithIPCMessage(msg, "queryCallWaiting");
        break;
      case "RIL:SetCallingLineIdRestriction":
        this.setCallingLineIdRestriction(msg.target, msg.json.data);
        break;
      case "RIL:GetCallingLineIdRestriction":
        this.workerMessenger.sendWithIPCMessage(msg, "getCLIR");
        break;
      case "RIL:ExitEmergencyCbMode":
        this.workerMessenger.sendWithIPCMessage(msg, "exitEmergencyCbMode");
        break;
      case "RIL:GetVoicemailInfo":
        // This message is sync.
        return this.voicemailInfo;
      case "RIL:SetRoamingPreference":
        this.workerMessenger.sendWithIPCMessage(msg, "setRoamingPreference");
        break;
      case "RIL:GetRoamingPreference":
        this.workerMessenger.sendWithIPCMessage(msg, "queryRoamingPreference");
        break;
      case "RIL:SetVoicePrivacyMode":
        this.workerMessenger.sendWithIPCMessage(msg, "setVoicePrivacyMode");
        break;
      case "RIL:GetVoicePrivacyMode":
        this.workerMessenger.sendWithIPCMessage(msg, "queryVoicePrivacyMode");
        break;
    }
  },

  handleUnsolicitedWorkerMessage: function handleUnsolicitedWorkerMessage(message) {
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
      case "conferenceCallStateChanged":
        this.handleConferenceCallStateChanged(message.state);
        break;
      case "cdmaCallWaiting":
        gMessageManager.sendTelephonyMessage("RIL:CdmaCallWaiting",
                                             this.clientId, message.number);
        break;
      case "callError":
        this.handleCallError(message);
        break;
      case "suppSvcNotification":
        this.handleSuppSvcNotification(message);
        break;
      case "emergencyCbModeChange":
        this.handleEmergencyCbModeChange(message);
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
      case "otastatuschange":
        this.handleOtaStatus(message);
        break;
      case "radiostatechange":
        this.handleRadioStateChange(message);
        break;
      case "cardstatechange":
        this.rilContext.cardState = message.cardState;
        gMessageManager.sendIccMessage("RIL:CardStateChanged",
                                       this.clientId, message);
        break;
      case "sms-received":
        let ackOk = this.handleSmsReceived(message);
        if (ackOk) {
          this.workerMessenger.send("ackSMS", { result: RIL.PDU_FCS_OK });
        }
        return;
      case "cellbroadcast-received":
        message.timestamp = Date.now();
        gMessageManager.sendCellBroadcastMessage("RIL:CellBroadcastReceived",
                                                 this.clientId, message);
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
      case "iccmbdn":
        this.handleIccMbdn(message);
        break;
      case "USSDReceived":
        if (DEBUG) this.debug("USSDReceived " + JSON.stringify(message));
        this.handleUSSDReceived(message);
        break;
      case "stkcommand":
        this.handleStkProactiveCommand(message);
        break;
      case "stksessionend":
        gMessageManager.sendIccMessage("RIL:StkSessionEnd", this.clientId, null);
        break;
      case "setRadioEnabled":
        let lock = gSettingsService.createLock();
        lock.set("ril.radio.disabled", !message.on, null, null);
        break;
      case "exitEmergencyCbMode":
        this.handleExitEmergencyCbMode(message);
        break;
      default:
        throw new Error("Don't know about this message type: " +
                        message.rilMessageType);
    }
  },

  getMsisdn: function getMsisdn() {
    let iccInfo = this.rilContext.iccInfo;
    let number = iccInfo ? iccInfo.msisdn : null;

    // Workaround an xpconnect issue with undefined string objects.
    // See bug 808220
    if (number === undefined || number === "undefined") {
      return null;
    }
    return number;
  },

  updateNetworkInfo: function updateNetworkInfo(message) {
    let voiceMessage = message[RIL.NETWORK_INFO_VOICE_REGISTRATION_STATE];
    let dataMessage = message[RIL.NETWORK_INFO_DATA_REGISTRATION_STATE];
    let operatorMessage = message[RIL.NETWORK_INFO_OPERATOR];
    let selectionMessage = message[RIL.NETWORK_INFO_NETWORK_SELECTION_MODE];
    let signalMessage = message[RIL.NETWORK_INFO_SIGNAL];

    // Batch the *InfoChanged messages together
    if (voiceMessage) {
      this.updateVoiceConnection(voiceMessage, true);
    }

    if (dataMessage) {
      this.updateDataConnection(dataMessage, true);
    }

    if (operatorMessage) {
      this.handleOperatorChange(operatorMessage, true);
    }

    if (signalMessage) {
      this.handleSignalStrengthChange(signalMessage, true);
    }

    let voice = this.rilContext.voice;
    let data = this.rilContext.data;

    this.checkRoamingBetweenOperators(voice);
    this.checkRoamingBetweenOperators(data);

    if (voiceMessage || operatorMessage || signalMessage) {
      gMessageManager.sendMobileConnectionMessage("RIL:VoiceInfoChanged",
                                                  this.clientId, voice);
    }
    if (dataMessage || operatorMessage || signalMessage) {
      gMessageManager.sendMobileConnectionMessage("RIL:DataInfoChanged",
                                                  this.clientId, data);
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
   * Handle data connection changes.
   *
   * @param newInfo The new voice connection information.
   * @param batch   When batch is true, the RIL:VoiceInfoChanged message will
   *                not be sent.
   */
  updateVoiceConnection: function updateVoiceConnection(newInfo, batch) {
    let voiceInfo = this.rilContext.voice;
    voiceInfo.state = newInfo.state;
    voiceInfo.connected = newInfo.connected;
    voiceInfo.roaming = newInfo.roaming;
    voiceInfo.emergencyCallsOnly = newInfo.emergencyCallsOnly;
    voiceInfo.type = newInfo.type;

    // Make sure we also reset the operator and signal strength information
    // if we drop off the network.
    if (newInfo.state !== RIL.GECKO_MOBILE_CONNECTION_STATE_REGISTERED) {
      voiceInfo.cell = null;
      voiceInfo.network = null;
      voiceInfo.signalStrength = null;
      voiceInfo.relSignalStrength = null;
    } else {
      voiceInfo.cell = newInfo.cell;
      voiceInfo.network = this.operatorInfo;
    }

    if (!batch) {
      gMessageManager.sendMobileConnectionMessage("RIL:VoiceInfoChanged",
                                                  this.clientId, voiceInfo);
    }
  },

  /**
   * Handle the data connection's state has changed.
   *
   * @param newInfo The new data connection information.
   * @param batch   When batch is true, the RIL:DataInfoChanged message will
   *                not be sent.
   */
  updateDataConnection: function updateDataConnection(newInfo, batch) {
    let dataInfo = this.rilContext.data;
    dataInfo.state = newInfo.state;
    dataInfo.roaming = newInfo.roaming;
    dataInfo.emergencyCallsOnly = newInfo.emergencyCallsOnly;
    dataInfo.type = newInfo.type;
    // For the data connection, the `connected` flag indicates whether
    // there's an active data call.
    let apnSetting = this.apnSettings.byType.default;
    dataInfo.connected = false;
    if (apnSetting) {
      dataInfo.connected = (this.getDataCallStateByType("default") ==
                            RIL.GECKO_NETWORK_STATE_CONNECTED);
    }

    // Make sure we also reset the operator and signal strength information
    // if we drop off the network.
    if (newInfo.state !== RIL.GECKO_MOBILE_CONNECTION_STATE_REGISTERED) {
      dataInfo.cell = null;
      dataInfo.network = null;
      dataInfo.signalStrength = null;
      dataInfo.relSignalStrength = null;
    } else {
      dataInfo.cell = newInfo.cell;
      dataInfo.network = this.operatorInfo;
    }

    if (!batch) {
      gMessageManager.sendMobileConnectionMessage("RIL:DataInfoChanged",
                                                  this.clientId, dataInfo);
    }
    this.updateRILNetworkInterface();
  },

  /**
   * Handle data errors
   */
  handleDataCallError: function handleDataCallError(message) {
    // Notify data call error only for data APN
    if (this.apnSettings.byType.default) {
      let apnSetting = this.apnSettings.byType.default;
      if (message.apn == apnSetting.apn &&
          apnSetting.iface.inConnectedTypes("default")) {
        gMessageManager.sendMobileConnectionMessage("RIL:DataError",
                                                    this.clientId, message);
      }
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

    this.workerMessenger.send("setPreferredNetworkType",
                              { networkType: networkType },
                              (function(response) {
      if ((this._preferredNetworkType != null) && !response.success) {
        gSettingsService.createLock().set("ril.radio.preferredNetworkType",
                                          this._preferredNetworkType,
                                          null);
        return false;
      }

      this._preferredNetworkType = response.networkType;
      if (DEBUG) {
        this.debug("_preferredNetworkType is now " +
                   RIL.RIL_PREFERRED_NETWORK_TYPE_TO_GECKO[this._preferredNetworkType]);
      }

      return false;
    }).bind(this));
  },

  setCellBroadcastSearchList: function setCellBroadcastSearchList(newSearchListStr) {
    if (newSearchListStr == this._cellBroadcastSearchListStr) {
      return;
    }

    this.workerMessenger.send("setCellBroadcastSearchList",
                              { searchListStr: newSearchListStr },
                              (function callback(response) {
      if (!response.success) {
        let lock = gSettingsService.createLock();
        lock.set(kCellBroadcastSearchList,
                 this._cellBroadcastSearchListStr, null);
      } else {
        this._cellBroadcastSearchListStr = response.searchListStr;
      }

      return false;
    }).bind(this));
  },

  /**
   * Handle signal strength changes.
   *
   * @param message The new signal strength.
   * @param batch   When batch is true, the RIL:VoiceInfoChanged and
   *                RIL:DataInfoChanged message will not be sent.
   */
  handleSignalStrengthChange: function handleSignalStrengthChange(message, batch) {
    let voiceInfo = this.rilContext.voice;
    // If the voice is not registered, need not to update signal information.
    if (voiceInfo.state === RIL.GECKO_MOBILE_CONNECTION_STATE_REGISTERED &&
        this.isInfoChanged(message.voice, voiceInfo)) {
      this.updateInfo(message.voice, voiceInfo);
      if (!batch) {
        gMessageManager.sendMobileConnectionMessage("RIL:VoiceInfoChanged",
                                                    this.clientId, voiceInfo);
      }
    }

    let dataInfo = this.rilContext.data;
    // If the data is not registered, need not to update signal information.
    if (dataInfo.state === RIL.GECKO_MOBILE_CONNECTION_STATE_REGISTERED &&
        this.isInfoChanged(message.data, dataInfo)) {
      this.updateInfo(message.data, dataInfo);
      if (!batch) {
        gMessageManager.sendMobileConnectionMessage("RIL:DataInfoChanged",
                                                    this.clientId, dataInfo);
      }
    }
  },

  /**
   * Handle operator information changes.
   *
   * @param message The new operator information.
   * @param batch   When batch is true, the RIL:VoiceInfoChanged and
   *                RIL:DataInfoChanged message will not be sent.
   */
  handleOperatorChange: function handleOperatorChange(message, batch) {
    let operatorInfo = this.operatorInfo;
    let voice = this.rilContext.voice;
    let data = this.rilContext.data;

    if (this.isInfoChanged(message, operatorInfo)) {
      this.updateInfo(message, operatorInfo);

      // Update lastKnownNetwork
      if (message.mcc && message.mnc) {
        try {
          Services.prefs.setCharPref("ril.lastKnownNetwork",
                                     message.mcc + "-" + message.mnc);
        } catch (e) {}
      }

      // If the voice is unregistered, no need to send RIL:VoiceInfoChanged.
      if (voice.network && !batch) {
        gMessageManager.sendMobileConnectionMessage("RIL:VoiceInfoChanged",
                                                    this.clientId, voice);
      }

      // If the data is unregistered, no need to send RIL:DataInfoChanged.
      if (data.network && !batch) {
        gMessageManager.sendMobileConnectionMessage("RIL:DataInfoChanged",
                                                    this.clientId, data);
      }
    }
  },

  handleOtaStatus: function handleOtaStatus(message) {
    if (message.status < 0 ||
        RIL.CDMA_OTA_PROVISION_STATUS_TO_GECKO.length <= message.status) {
      return;
    }

    let status = RIL.CDMA_OTA_PROVISION_STATUS_TO_GECKO[message.status];

    gMessageManager.sendMobileConnectionMessage("RIL:OtaStatusChanged",
                                                this.clientId, status);
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

  _ensureRadioState: function _ensureRadioState() {
    if (DEBUG) {
      this.debug("Reported radio state is " + this.rilContext.radioState +
                 ", desired radio enabled state is " + this._radioEnabled);
    }
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
      this._changingRadioPower = true;
      this.setRadioEnabled(true);
    }
    if (this.rilContext.radioState == RIL.GECKO_RADIOSTATE_READY &&
        !this._radioEnabled) {
      this._changingRadioPower = true;
      this.powerOffRadioSafely();
    }
  },

  _radioOffTimer: null,
  _cancelRadioOffTimer: function _cancelRadioOffTimer() {
    if (this._radioOffTimer) {
      this._radioOffTimer.cancel();
    }
  },
  _fireRadioOffTimer: function _fireRadioOffTimer() {
    if (DEBUG) this.debug("Radio off timer expired, set radio power off right away.");
    this.setRadioEnabled(false);
  },

  /**
   * Clean up all existing data calls before turning radio off.
   */
  powerOffRadioSafely: function powerOffRadioSafely() {
    let dataDisconnecting = false;
    for each (let apnSetting in this.apnSettings.byAPN) {
      for each (let type in apnSetting.types) {
        if (this.getDataCallStateByType(type) ==
            RIL.GECKO_NETWORK_STATE_CONNECTED) {
          this.deactivateDataCallByType(type);
          dataDisconnecting = true;
        }
      }
    }
    if (dataDisconnecting) {
      if (this._radioOffTimer == null) {
        this._radioOffTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      }
      this._radioOffTimer.initWithCallback(this._fireRadioOffTimer.bind(this),
                                           RADIO_POWER_OFF_TIMEOUT, Ci.nsITimer.TYPE_ONE_SHOT);
      return;
    }
    this.setRadioEnabled(false);
  },

  /**
   * This function will do the following steps:
   * 1. Clear the old APN settings.
   * 2. Combine APN, user name, and password as the key of byAPN{} and store
   *    corresponding APN setting into byApn{}, which makes it easiler to get
   *    the APN setting.
   * 3. Use APN type as the index of byType{} and store the link of
   *    corresponding APN setting into byType{}, which makes it easier to get
   *    the APN setting via APN types.
   */
  updateApnSettings: function updateApnSettings(allApnSettings) {
    let thisSimApnSettings = allApnSettings[this.clientId];
    if (!thisSimApnSettings) {
      return;
    }

    // Clear old APN settings.
    for each (let apnSetting in this.apnSettings.byAPN) {
      // Clear all connections of this APN settings.
      for each (let type in apnSetting.types) {
        if (this.getDataCallStateByType(type) ==
            RIL.GECKO_NETWORK_STATE_CONNECTED) {
          this.deactivateDataCallByType(type);
        }
      }
      if (apnSetting.iface.name in gNetworkManager.networkInterfaces) {
        gNetworkManager.unregisterNetworkInterface(apnSetting.iface);
      }
      this.unregisterDataCallCallback(apnSetting.iface);
      delete apnSetting.iface;
    }
    this.apnSettings.byAPN = {};
    this.apnSettings.byType = {};

    // Create new APN settings.
    for (let apnIndex = 0; thisSimApnSettings[apnIndex]; apnIndex++) {
      let inputApnSetting = thisSimApnSettings[apnIndex];
      if (!this.validateApnSetting(inputApnSetting)) {
        continue;
      }

      // Combine APN, user name, and password as the key of byAPN{} to get
      // the corresponding APN setting.
      let apnKey = inputApnSetting.apn + (inputApnSetting.user || '') +
                   (inputApnSetting.password || '');
      if (!this.apnSettings.byAPN[apnKey]) {
        this.apnSettings.byAPN[apnKey] = {};
        this.apnSettings.byAPN[apnKey] = inputApnSetting;
        this.apnSettings.byAPN[apnKey].iface =
          new RILNetworkInterface(this, this.apnSettings.byAPN[apnKey]);
      } else {
        this.apnSettings.byAPN[apnKey].types.push(inputApnSetting.types);
      }
      for each (let type in inputApnSetting.types) {
        this.apnSettings.byType[type] = {};
        this.apnSettings.byType[type] = this.apnSettings.byAPN[apnKey];
      }
    }
  },

  /**
   * Check if we get all necessary APN data.
   */
  validateApnSetting: function validateApnSetting(apnSetting) {
    return (apnSetting &&
            apnSetting.apn &&
            apnSetting.types &&
            apnSetting.types.length);
  },

  updateRILNetworkInterface: function updateRILNetworkInterface() {
    let apnSetting = this.apnSettings.byType.default;
    if (!this.validateApnSetting(apnSetting)) {
      if (DEBUG) {
        this.debug("We haven't gotten completely the APN data.");
      }
      return;
    }

    // This check avoids data call connection if the radio is not ready
    // yet after toggling off airplane mode.
    if (this.rilContext.radioState != RIL.GECKO_RADIOSTATE_READY) {
      if (DEBUG) {
        this.debug("RIL is not ready for data connection: radio's not ready");
      }
      return;
    }

    // We only watch at "ril.data.enabled" flag changes for connecting or
    // disconnecting the data call. If the value of "ril.data.enabled" is
    // true and any of the remaining flags change the setting application
    // should turn this flag to false and then to true in order to reload
    // the new values and reconnect the data call.
    if (this.dataCallSettings.oldEnabled == this.dataCallSettings.enabled) {
      if (DEBUG) {
        this.debug("No changes for ril.data.enabled flag. Nothing to do.");
      }
      return;
    }

    let defaultDataCallState = this.getDataCallStateByType("default");
    if (defaultDataCallState == RIL.GECKO_NETWORK_STATE_CONNECTING ||
        defaultDataCallState == RIL.GECKO_NETWORK_STATE_DISCONNECTING) {
      if (DEBUG) {
        this.debug("Nothing to do during connecting/disconnecting in progress.");
      }
      return;
    }

    let dataInfo = this.rilContext.data;
    let isRegistered =
      dataInfo.state == RIL.GECKO_MOBILE_CONNECTION_STATE_REGISTERED;
    let haveDataConnection =
      dataInfo.type != RIL.GECKO_MOBILE_CONNECTION_STATE_UNKNOWN;
    if (!isRegistered || !haveDataConnection) {
      if (DEBUG) {
        this.debug("RIL is not ready for data connection: Phone's not " +
                   "registered or doesn't have data connection.");
      }
      return;
    }
    let wifi_active = false;
    if (gNetworkManager.active &&
        gNetworkManager.active.type == Ci.nsINetworkInterface.NETWORK_TYPE_WIFI) {
      wifi_active = true;
    }

    let defaultDataCallConnected = defaultDataCallState ==
                                   RIL.GECKO_NETWORK_STATE_CONNECTED;
    if (defaultDataCallConnected &&
        (!this.dataCallSettings.enabled ||
         (dataInfo.roaming && !this.dataCallSettings.roamingEnabled))) {
      if (DEBUG) this.debug("Data call settings: disconnect data call.");
      this.deactivateDataCallByType("default");
      return;
    }

    if (defaultDataCallConnected && wifi_active) {
      if (DEBUG) this.debug("Disconnect data call when Wifi is connected.");
      this.deactivateDataCallByType("default");
      return;
    }

    if (!this.dataCallSettings.enabled || defaultDataCallConnected) {
      if (DEBUG) this.debug("Data call settings: nothing to do.");
      return;
    }
    if (dataInfo.roaming && !this.dataCallSettings.roamingEnabled) {
      if (DEBUG) this.debug("We're roaming, but data roaming is disabled.");
      return;
    }
    if (wifi_active) {
      if (DEBUG) this.debug("Don't connect data call when Wifi is connected.");
      return;
    }
    if (this._changingRadioPower) {
      // We're changing the radio power currently, ignore any changes.
      return;
    }

    if (DEBUG) this.debug("Data call settings: connect data call.");
    this.setupDataCallByType("default");
  },

  /**
   * Track the active call and update the audio system as its state changes.
   */
  _activeCall: null,
  updateCallAudioState: function updateCallAudioState(options) {
    if (options.conferenceState === nsITelephonyProvider.CALL_STATE_CONNECTED) {
      gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_IN_CALL;
      if (this.speakerEnabled) {
        gAudioManager.setForceForUse(nsIAudioManager.USE_COMMUNICATION,
                                     nsIAudioManager.FORCE_SPEAKER);
      }
      return;
    }
    if (options.conferenceState === nsITelephonyProvider.CALL_STATE_UNKNOWN ||
        options.conferenceState === nsITelephonyProvider.CALL_STATE_HELD) {
      if (!this._activeCall) {
        gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_NORMAL;
      }
      return;
    }

    if (!options.call) {
      return;
    }

    if (options.call.isConference) {
      if (this._activeCall && this._activeCall.callIndex == options.call.callIndex) {
        this._activeCall = null;
      }
      return;
    }

    let call = options.call;
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
        if (DEBUG) {
          this.debug("Active call, put audio system into PHONE_STATE_IN_CALL: "
                     + gAudioManager.phoneState);
        }
        break;
      case nsITelephonyProvider.CALL_STATE_INCOMING:
        call.isActive = false;
        if (!this._activeCall) {
          // We can change the phone state into RINGTONE only when there's
          // no active call.
          gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_RINGTONE;
          if (DEBUG) {
            this.debug("Incoming call, put audio system into " +
                       "PHONE_STATE_RINGTONE: " + gAudioManager.phoneState);
          }
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
          if (DEBUG) {
            this.debug("No active call, put audio system into " +
                       "PHONE_STATE_NORMAL: " + gAudioManager.phoneState);
          }
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
    if (DEBUG) this.debug("handleCallStateChange: " + JSON.stringify(call));
    call.state = convertRILCallState(call.state);

    if (call.state == nsITelephonyProvider.CALL_STATE_DIALING) {
      gSystemMessenger.broadcastMessage("telephony-new-call", {});
    }
    this.updateCallAudioState({call: call});
    gMessageManager.sendTelephonyMessage("RIL:CallStateChanged",
                                         this.clientId, call);
  },

  /**
   * Handle call disconnects by updating our current state and the audio system.
   */
  handleCallDisconnected: function handleCallDisconnected(call) {
    if (DEBUG) this.debug("handleCallDisconnected: " + JSON.stringify(call));
    call.state = nsITelephonyProvider.CALL_STATE_DISCONNECTED;
    let duration = ("started" in call && typeof call.started == "number") ?
      new Date().getTime() - call.started : 0;
    let data = {
      number: call.number,
      duration: duration,
      direction: call.isOutgoing ? "outgoing" : "incoming"
    };
    gSystemMessenger.broadcastMessage("telephony-call-ended", data);
    this.updateCallAudioState({call: call});
    gMessageManager.sendTelephonyMessage("RIL:CallStateChanged",
                                         this.clientId, call);
  },

  handleConferenceCallStateChanged: function handleConferenceCallStateChanged(state) {
    debug("handleConferenceCallStateChanged: " + state);
    state = state != null ? convertRILCallState(state) :
                            nsITelephonyProvider.CALL_STATE_UNKNOWN;
    this.updateCallAudioState({conferenceState: state});
    gMessageManager.sendTelephonyMessage("RIL:ConferenceCallStateChanged",
                                         this.clientId, state);
  },

  /**
   * Update network selection mode
   */
  updateNetworkSelectionMode: function updateNetworkSelectionMode(message) {
    if (DEBUG) this.debug("updateNetworkSelectionMode: " + JSON.stringify(message));
    this.rilContext.networkSelectionMode = message.mode;
    gMessageManager.sendMobileConnectionMessage("RIL:NetworkSelectionModeChanged",
                                                this.clientId, message);
  },

  /**
   * Handle emergency callback mode change.
   */
  handleEmergencyCbModeChange: function handleEmergencyCbModeChange(message) {
    if (DEBUG) this.debug("handleEmergencyCbModeChange: " + JSON.stringify(message));
    gMessageManager.sendMobileConnectionMessage("RIL:EmergencyCbModeChanged",
                                                this.clientId, message);
  },

  /**
   * Handle call error.
   */
  handleCallError: function handleCallError(message) {
    gMessageManager.sendTelephonyMessage("RIL:CallError",
                                         this.clientId, message);
  },

  /**
   * Handle supplementary service notification.
   */
  handleSuppSvcNotification: function handleSuppSvcNotification(message) {
    message.notification = convertRILSuppSvcNotification(message.notification);
    gMessageManager.sendTelephonyMessage("RIL:SuppSvcNotification",
                                         this.clientId, message);
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
      if (DEBUG) {
        this.debug("Got port addressed SMS but not encoded in 8-bit alphabet." +
                   " Drop!");
      }
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
    if (DEBUG) this.debug("Broadcasting the SMS system message: " + aName);

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
    if (DEBUG) this.debug("handleSmsReceived: " + JSON.stringify(message));

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
    message.receiver = this.getMsisdn();
    message.body = message.fullBody = message.fullBody || null;
    message.timestamp = Date.now();

    if (gSmsService.isSilentNumber(message.sender)) {
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

      Services.obs.notifyObservers(domMessage,
                                   kSilentSmsReceivedObserverTopic,
                                   null);
      return true;
    }

    // TODO: Bug #768441
    // For now we don't store indicators persistently. When the mwi.discard
    // flag is false, we'll need to persist the indicator to EFmwis.
    // See TS 23.040 9.2.3.24.2

    let mwi = message.mwi;
    if (mwi) {
      mwi.returnNumber = message.sender;
      mwi.returnMessage = message.fullBody;
      gMessageManager.sendVoicemailMessage("RIL:VoicemailNotification",
                                           this.clientId, mwi);
      return true;
    }

    let notifyReceived = function notifyReceived(rv, domMessage) {
      let success = Components.isSuccessCode(rv);

      // Acknowledge the reception of the SMS.
      this.workerMessenger.send("ackSMS", {
        result: (success ? RIL.PDU_FCS_OK
                         : RIL.PDU_FCS_MEMORY_CAPACITY_EXCEEDED)
      });

      if (!success) {
        // At this point we could send a message to content to notify the user
        // that storing an incoming SMS failed, most likely due to a full disk.
        if (DEBUG) {
          this.debug("Could not store SMS " + message.id + ", error code " + rv);
        }
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
   * Handle data call state changes.
   */
  handleDataCallState: function handleDataCallState(datacall) {
    let data = this.rilContext.data;
    let apnSetting = this.apnSettings.byType.default;
    let dataCallConnected =
        (datacall.state == RIL.GECKO_NETWORK_STATE_CONNECTED);
    if (apnSetting && datacall.ifname) {
      if (dataCallConnected && datacall.apn == apnSetting.apn &&
          apnSetting.iface.inConnectedTypes("default")) {
        data.connected = dataCallConnected;
        gMessageManager.sendMobileConnectionMessage("RIL:DataInfoChanged",
                                                     this.clientId, data);
        data.apn = datacall.apn;
      } else if (!dataCallConnected && datacall.apn == data.apn) {
        data.connected = dataCallConnected;
        delete data.apn;
        gMessageManager.sendMobileConnectionMessage("RIL:DataInfoChanged",
                                                     this.clientId, data);
      }
    }

    this._deliverDataCallCallback("dataCallStateChanged",
                                  [datacall]);

    // Process pending radio power off request after all data calls
    // are disconnected.
    if (datacall.state == RIL.GECKO_NETWORK_STATE_UNKNOWN &&
        this._changingRadioPower) {
      let anyDataConnected = false;
      for each (let apnSetting in this.apnSettings.byAPN) {
        for each (let type in apnSetting.types) {
          if (this.getDataCallStateByType(type) == RIL.GECKO_NETWORK_STATE_CONNECTED) {
            anyDataConnected = true;
            break;
          }
        }
        if (anyDataConnected) {
          break;
        }
      }
      if (!anyDataConnected) {
        if (DEBUG) this.debug("All data connections are disconnected, set radio off.");
        this._cancelRadioOffTimer();
        this.setRadioEnabled(false);
      }
    }
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

    gMessageManager.sendVoicemailMessage("RIL:VoicemailInfoChanged",
                                         this.clientId, voicemailInfo);
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
    gMessageManager.sendIccMessage("RIL:IccInfoChanged",
                                   this.clientId, message);

    // Update lastKnownSimMcc.
    if (message.mcc) {
      try {
        Services.prefs.setCharPref("ril.lastKnownSimMcc",
                                   message.mcc.toString());
      } catch (e) {}
    }

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
        gMessageManager.sendMobileConnectionMessage("RIL:VoiceInfoChanged",
                                                    this.clientId, voice);
      }
      if (dataRoaming != data.roaming) {
        gMessageManager.sendMobileConnectionMessage("RIL:DataInfoChanged",
                                                    this.clientId, data);
      }
    }
  },

  handleUSSDReceived: function handleUSSDReceived(ussd) {
    if (DEBUG) this.debug("handleUSSDReceived " + JSON.stringify(ussd));
    gSystemMessenger.broadcastMessage("ussd-received", ussd);
    gMessageManager.sendMobileConnectionMessage("RIL:USSDReceived",
                                                this.clientId, ussd);
  },

  handleStkProactiveCommand: function handleStkProactiveCommand(message) {
    if (DEBUG) this.debug("handleStkProactiveCommand " + JSON.stringify(message));
    gSystemMessenger.broadcastMessage("icc-stkcommand", message);
    gMessageManager.sendIccMessage("RIL:StkCommand", this.clientId, message);
  },

  handleExitEmergencyCbMode: function handleExitEmergencyCbMode(message) {
    if (DEBUG) this.debug("handleExitEmergencyCbMode: " + JSON.stringify(message));
    gMessageManager.sendRequestResults("RIL:ExitEmergencyCbMode", message);
  },

  // nsIObserver

  observe: function observe(subject, topic, data) {
    switch (topic) {
      case kSysMsgListenerReadyObserverTopic:
        Services.obs.removeObserver(this, kSysMsgListenerReadyObserverTopic);
        this._sysMsgListenerReady = true;
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
          this.workerMessenger.send("setCellBroadcastDisabled",
                                    { disabled: value });
        }
        break;
      case "xpcom-shutdown":
        // Cancel the timer for the call-ring wake lock.
        this._cancelCallRingWakeLockTimer();
        // Shutdown all RIL network interfaces
        for each (let apnSetting in this.apnSettings.byAPN) {
          if (apnSetting.iface) {
            apnSetting.iface.shutdown();
          }
        }
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
        this.workerMessenger.send("setScreenState", { on: (data === "on") });
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

  // Data calls setting.
  dataCallSettings: null,

  apnSettings: null,

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
      if (DEBUG) {
        this.debug("Content processes cannot modify 'time.nitz.available'. Restore!");
      }
      // Restore the setting to the current value.
      this.setNitzAvailable(isNitzAvailable);
    }

    this.handle(aName, aResult);
  },

  // nsISettingsServiceCallback
  handle: function handle(aName, aResult) {
    switch(aName) {
      case "ril.radio.disabled":
        if (DEBUG) this.debug("'ril.radio.disabled' is now " + aResult);
        this._radioEnabled = !aResult;
        this._ensureRadioState();
        break;
      case "ril.radio.preferredNetworkType":
        if (DEBUG) this.debug("'ril.radio.preferredNetworkType' is now " + aResult);
        this.setPreferredNetworkType(aResult);
        break;
      case "ril.data.enabled":
        if (DEBUG) this.debug("'ril.data.enabled' is now " + aResult);
        let enabled;
        if (Array.isArray(aResult)) {
          enabled = aResult[this.clientId];
        } else {
          // Backward compability
          enabled = aResult;
        }
        this.dataCallSettings.oldEnabled = this.dataCallSettings.enabled;
        this.dataCallSettings.enabled = enabled;
        this.updateRILNetworkInterface();
        break;
      case "ril.data.roaming_enabled":
        if (DEBUG) this.debug("'ril.data.roaming_enabled' is now " + aResult);
        this.dataCallSettings.roamingEnabled = aResult;
        this.updateRILNetworkInterface();
        break;
      case "ril.data.apnSettings":
        if (DEBUG) this.debug("'ril.data.apnSettings' is now " + JSON.stringify(aResult));
        if (aResult) {
          this.updateApnSettings(aResult);
          this.updateRILNetworkInterface();
        }
        break;
      case kTimeNitzAutomaticUpdateEnabled:
        this._nitzAutomaticUpdateEnabled = aResult;

        // Set the latest cached NITZ time if the setting is enabled.
        if (this._nitzAutomaticUpdateEnabled && this._lastNitzMessage) {
          this.setNitzTime(this._lastNitzMessage);
        }
        break;
      case kCellBroadcastSearchList:
        if (DEBUG) {
          this.debug("'" + kCellBroadcastSearchList + "' is now " + aResult);
        }
        this.setCellBroadcastSearchList(aResult);
        break;
    }
  },

  handleError: function handleError(aErrorMessage) {
    if (DEBUG) this.debug("There was an error while reading RIL settings.");

    // Default radio to on.
    this._radioEnabled = true;
    this._ensureRadioState();

    // Clean data call setting.
    this.dataCallSettings.oldEnabled = false;
    this.dataCallSettings.enabled = false;
    this.dataCallSettings.roamingEnabled = false;
    this.apnSettings = {
      byType: {},
      byAPN: {},
    };
  },

  // nsIRadioInterface

  setRadioEnabled: function setRadioEnabled(value) {
    if (DEBUG) this.debug("Setting radio power to " + value);
    this.workerMessenger.send("setRadioPower", { on: value });
  },

  rilContext: null,

  // Handle phone functions of nsIRILContentHelper

  enumerateCalls: function enumerateCalls(target, message) {
    if (DEBUG) this.debug("Requesting enumeration of calls for callback");
    this.workerMessenger.send("enumerateCalls", message, (function(response) {
      for (let call of response.calls) {
        call.state = convertRILCallState(call.state);
        call.isActive = this._activeCall ?
          call.callIndex == this._activeCall.callIndex : false;
      }
      target.sendAsyncMessage("RIL:EnumerateCalls", response);
      return false;
    }).bind(this));
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
    if (DEBUG) {
      this.debug("Number '" + number + "' doesn't seem to be a viable number." +
                 " Drop.");
    }

    return false;
  },

  dial: function dial(number) {
    if (DEBUG) this.debug("Dialing " + number);
    number = PhoneNumberUtils.normalize(number);
    if (this._validateNumber(number)) {
      this.workerMessenger.send("dial", { number: number,
                                          isDialEmergency: false });
    }
  },

  dialEmergency: function dialEmergency(number) {
    if (DEBUG) this.debug("Dialing emergency " + number);
    // we don't try to be too clever here, as the phone is probably in the
    // locked state. Let's just check if it's a number without normalizing
    if (this._validateNumber(number)) {
      this.workerMessenger.send("dial", { number: number,
                                          isDialEmergency: true });
    }
  },

  _sendCfStateChanged: function _sendCfStateChanged(message) {
    gMessageManager.sendMobileConnectionMessage("RIL:CfStateChanged",
                                                this.clientId, message);
  },

  _updateCallingLineIdRestrictionPref:
    function _updateCallingLineIdRestrictionPref(mode) {
    try {
      Services.prefs.setIntPref(kClirModePreference, mode);
      Services.prefs.savePrefFile(null);
      if (DEBUG) {
        this.debug(kClirModePreference + " pref is now " + mode);
      }
    } catch (e) {}
  },

  sendMMI: function sendMMI(target, message) {
    if (DEBUG) this.debug("SendMMI " + JSON.stringify(message));
    this.workerMessenger.send("sendMMI", message, (function(response) {
      if (response.isSetCallForward) {
        this._sendCfStateChanged(response);
      } else if (response.isSetCLIR && response.success) {
        this._updateCallingLineIdRestrictionPref(response.clirMode);
      }

      target.sendAsyncMessage("RIL:SendMMI", response);
      return false;
    }).bind(this));
  },

  setCallForwardingOption: function setCallForwardingOption(target, message) {
    if (DEBUG) this.debug("setCallForwardingOption: " + JSON.stringify(message));
    message.serviceClass = RIL.ICC_SERVICE_CLASS_VOICE;
    this.workerMessenger.send("setCallForward", message, (function(response) {
      this._sendCfStateChanged(response);
      target.sendAsyncMessage("RIL:SetCallForwardingOption", response);
      return false;
    }).bind(this));
  },

  setCallingLineIdRestriction: function setCallingLineIdRestriction(target,
                                                                    message) {
    if (DEBUG) {
      this.debug("setCallingLineIdRestriction: " + JSON.stringify(message));
    }
    this.workerMessenger.send("setCLIR", message, (function(response) {
      if (response.success) {
        this._updateCallingLineIdRestrictionPref(response.clirMode);
      }
      target.sendAsyncMessage("RIL:SetCallingLineIdRestriction", response);
      return false;
    }).bind(this));
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

    if (DEBUG) this.debug("_calculateUserDataLength: " + JSON.stringify(options));
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

  sendSMS: function sendSMS(number, message, silent, request) {
    let strict7BitEncoding;
    try {
      strict7BitEncoding = Services.prefs.getBoolPref("dom.sms.strict7BitEncoding");
    } catch (e) {
      strict7BitEncoding = false;
    }

    let options = this._fragmentText(message, null, strict7BitEncoding);
    options.number = PhoneNumberUtils.normalize(number);
    let requestStatusReport;
    try {
      requestStatusReport =
        Services.prefs.getBoolPref("dom.sms.requestStatusReport");
    } catch (e) {
      requestStatusReport = true;
    }
    options.requestStatusReport = requestStatusReport && !silent;
    if (options.segmentMaxSeq > 1) {
      options.segmentRef16Bit = this.segmentRef16Bit;
      options.segmentRef = this.nextSegmentRef;
    }

    let notifyResult = (function notifyResult(rv, domMessage) {
      // TODO bug 832140 handle !Components.isSuccessCode(rv)
      if (!silent) {
        Services.obs.notifyObservers(domMessage, kSmsSendingObserverTopic, null);
      }

      // If the radio is disabled or the SIM card is not ready, just directly
      // return with the corresponding error code.
      let errorCode;
      if (!PhoneNumberUtils.isPlainPhoneNumber(options.number)) {
        if (DEBUG) this.debug("Error! Address is invalid when sending SMS: " +
                              options.number);
        errorCode = Ci.nsIMobileMessageCallback.INVALID_ADDRESS_ERROR;
      } else if (!this._radioEnabled) {
        if (DEBUG) this.debug("Error! Radio is disabled when sending SMS.");
        errorCode = Ci.nsIMobileMessageCallback.RADIO_DISABLED_ERROR;
      } else if (this.rilContext.cardState != "ready") {
        if (DEBUG) this.debug("Error! SIM card is not ready when sending SMS.");
        errorCode = Ci.nsIMobileMessageCallback.NO_SIM_CARD_ERROR;
      }
      if (errorCode) {
        if (silent) {
          request.notifySendMessageFailed(errorCode);
          return;
        }

        gMobileMessageDatabaseService
          .setMessageDeliveryByMessageId(domMessage.id,
                                         null,
                                         DOM_MOBILE_MESSAGE_DELIVERY_ERROR,
                                         RIL.GECKO_SMS_DELIVERY_STATUS_ERROR,
                                         null,
                                         function notifyResult(rv, domMessage) {
          // TODO bug 832140 handle !Components.isSuccessCode(rv)
          request.notifySendMessageFailed(errorCode);
          Services.obs.notifyObservers(domMessage, kSmsFailedObserverTopic, null);
        });
        return;
      }

      // Keep current SMS message info for sent/delivered notifications
      let context = {
        request: request,
        sms: domMessage,
        requestStatusReport: options.requestStatusReport,
        silent: silent
      };

      // This is the entry point starting to send SMS.
      this.workerMessenger.send("sendSMS", options,
                                (function(context, response) {
        if (response.errorMsg) {
          // Failed to send SMS out.
          let error = Ci.nsIMobileMessageCallback.UNKNOWN_ERROR;
          switch (message.errorMsg) {
            case RIL.ERROR_RADIO_NOT_AVAILABLE:
              error = Ci.nsIMobileMessageCallback.NO_SIGNAL_ERROR;
              break;
          }

          if (context.silent) {
            context.request.notifySendMessageFailed(error);
            return false;
          }

          gMobileMessageDatabaseService
            .setMessageDeliveryByMessageId(context.sms.id,
                                           null,
                                           DOM_MOBILE_MESSAGE_DELIVERY_ERROR,
                                           RIL.GECKO_SMS_DELIVERY_STATUS_ERROR,
                                           null,
                                           function notifyResult(rv, domMessage) {
            // TODO bug 832140 handle !Components.isSuccessCode(rv)
            context.request.notifySendMessageFailed(error);
            Services.obs.notifyObservers(domMessage, kSmsFailedObserverTopic, null);
          });
          return false;
        } // End of send failure.

        if (response.deliveryStatus) {
          // Message delivery.
          gMobileMessageDatabaseService
            .setMessageDeliveryByMessageId(context.sms.id,
                                           null,
                                           context.sms.delivery,
                                           message.deliveryStatus,
                                           null,
                                           function notifyResult(rv, domMessage) {
            // TODO bug 832140 handle !Components.isSuccessCode(rv)
            let topic = (message.deliveryStatus == RIL.GECKO_SMS_DELIVERY_STATUS_SUCCESS)
                        ? kSmsDeliverySuccessObserverTopic
                        : kSmsDeliveryErrorObserverTopic;
            Services.obs.notifyObservers(domMessage, topic, null);
          });

          // Send transaction has ended completely.
          return false;
        } // End of message delivery.

        // Message sent.
        if (context.silent) {
          // There is no way to modify nsIDOMMozSmsMessage attributes as they are
          // read only so we just create a new sms instance to send along with
          // the notification.
          let sms = context.sms;
          context.request.notifyMessageSent(
            gMobileMessageService.createSmsMessage(sms.id,
                                                   sms.threadId,
                                                   DOM_MOBILE_MESSAGE_DELIVERY_SENT,
                                                   sms.deliveryStatus,
                                                   sms.sender,
                                                   sms.receiver,
                                                   sms.body,
                                                   sms.messageClass,
                                                   sms.timestamp,
                                                   sms.read));
          // We don't wait for SMS-DELIVER-REPORT for silent one.
          return false;
        }

        gMobileMessageDatabaseService
          .setMessageDeliveryByMessageId(context.sms.id,
                                         null,
                                         DOM_MOBILE_MESSAGE_DELIVERY_SENT,
                                         context.sms.deliveryStatus,
                                         null,
                                         (function notifyResult(rv, domMessage) {
          // TODO bug 832140 handle !Components.isSuccessCode(rv)
          this.broadcastSmsSystemMessage("sms-sent", domMessage);

          if (context.requestStatusReport) {
            context.sms = domMessage;
          }

          context.request.notifyMessageSent(domMessage);
          Services.obs.notifyObservers(domMessage, kSmsSentObserverTopic, null);
        }).bind(this));

        // Only keep current context if we have requested for delivery report.
        return context.requestStatusReport;
      }).bind(this, context)); // End of |workerMessenger.send| callback.
    }).bind(this); // End of DB saveSendingMessage callback.

    let sendingMessage = {
      type: "sms",
      sender: this.getMsisdn(),
      receiver: number,
      body: message,
      deliveryStatusRequested: options.requestStatusReport,
      timestamp: Date.now()
    };

    if (silent) {
      let deliveryStatus = RIL.GECKO_SMS_DELIVERY_STATUS_PENDING;
      let delivery = DOM_MOBILE_MESSAGE_DELIVERY_SENDING;
      let domMessage =
        gMobileMessageService.createSmsMessage(-1, // id
                                               0,  // threadId
                                               delivery,
                                               deliveryStatus,
                                               sendingMessage.sender,
                                               sendingMessage.receiver,
                                               sendingMessage.body,
                                               "normal", // message class
                                               sendingMessage.timestamp,
                                               false);
      notifyResult(Cr.NS_OK, domMessage);
      return;
    }

    let id = gMobileMessageDatabaseService.saveSendingMessage(
      sendingMessage, notifyResult);
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
    if (DEBUG) this.debug("Registering callback: " + callback);
  },

  unregisterDataCallCallback: function unregisterDataCallCallback(callback) {
    if (!this._datacall_callbacks) {
      return;
    }
    let index = this._datacall_callbacks.indexOf(callback);
    if (index != -1) {
      this._datacall_callbacks.splice(index, 1);
      if (DEBUG) this.debug("Unregistering callback: " + callback);
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
        if (DEBUG) {
          this.debug("callback handler for " + name + " threw an exception: " + e);
        }
      }
    }
  },

  setupDataCallByType: function setupDataCallByType(apntype) {
    let apnSetting = this.apnSettings.byType[apntype];
    if (!apnSetting) {
      return;
    }

    let dataInfo = this.rilContext.data;
    if (dataInfo.state != RIL.GECKO_MOBILE_CONNECTION_STATE_REGISTERED ||
        dataInfo.type == RIL.GECKO_MOBILE_CONNECTION_STATE_UNKNOWN) {
      return;
    }

    apnSetting.iface.connect(apntype);
    // We just call connect() function, so this interface should be in
    // connecting state. If this interface is already in connected state, we
    // are sure that this interface have successfully established connection
    // for other data call types before we call connect() function for current
    // data call type. In this circumstance, we have to directly update the
    // necessary data call and interface information to RILContentHelper
    // and network manager for current data call type.
    if (apnSetting.iface.connected) {
      if (apntype == "default" && !dataInfo.connected) {
        dataInfo.connected = true;
        gMessageManager.sendMobileConnectionMessage("RIL:DataInfoChanged",
                                                    this.clientId, dataInfo);
      }

      // Update the interface status via-registration if the interface has
      // already been registered in the network manager.
      if (apnSetting.iface.name in gNetworkManager.networkInterfaces) {
        gNetworkManager.unregisterNetworkInterface(apnSetting.iface);
      }
      gNetworkManager.registerNetworkInterface(apnSetting.iface);

      Services.obs.notifyObservers(apnSetting.iface,
                                   kNetworkInterfaceStateChangedTopic,
                                   null);
    }
  },

  deactivateDataCallByType: function deactivateDataCallByType(apntype) {
    let apnSetting = this.apnSettings.byType[apntype];
    if (!apnSetting) {
      return;
    }

    apnSetting.iface.disconnect(apntype);
    // We just call disconnect() function, so this interface should be in
    // disconnecting state. If this interface is still in connected state, we
    // are sure that other data call types still need this connection of this
    // interface. In this circumstance, we have to directly update the
    // necessary data call and interface information to RILContentHelper
    // and network manager for current data call type.
    if (apnSetting.iface.connectedTypes.length && apnSetting.iface.connected) {
      let dataInfo = this.rilContext.data;
      if (apntype == "default" && dataInfo.connected) {
        dataInfo.connected = false;
        gMessageManager.sendMobileConnectionMessage("RIL:DataInfoChanged",
                                                    this.clientId, dataInfo);
      }

      // Update the interface status via-registration if the interface has
      // already been registered in the network manager.
      if (apnSetting.iface.name in gNetworkManager.networkInterfaces) {
        gNetworkManager.unregisterNetworkInterface(apnSetting.iface);
      }
      gNetworkManager.registerNetworkInterface(apnSetting.iface);

      Services.obs.notifyObservers(apnSetting.iface,
                                   kNetworkInterfaceStateChangedTopic,
                                   null);
    }
  },

  getDataCallStateByType: function getDataCallStateByType(apntype) {
    let apnSetting = this.apnSettings.byType[apntype];
    if (!apnSetting) {
       return RIL.GECKO_NETWORK_STATE_UNKNOWN;
    }
    if (!apnSetting.iface.inConnectedTypes(apntype)) {
      return RIL.GECKO_NETWORK_STATE_DISCONNECTED;
    }
    return apnSetting.iface.state;
  },

  setupDataCall: function setupDataCall(radioTech, apn, user, passwd, chappap, pdptype) {
    this.workerMessenger.send("setupDataCall", { radioTech: radioTech,
                                                 apn: apn,
                                                 user: user,
                                                 passwd: passwd,
                                                 chappap: chappap,
                                                 pdptype: pdptype });
  },

  deactivateDataCall: function deactivateDataCall(cid, reason) {
    this.workerMessenger.send("deactivateDataCall", { cid: cid,
                                                      reason: reason });
  },
};

function RILNetworkInterface(radioInterface, apnSetting) {
  this.radioInterface = radioInterface;
  this.apnSetting = apnSetting;

  this.connectedTypes = [];
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
  // The network manager should only need to add the host route for "other"
  // types, which is the same handling method as the supl type. So let the
  // definition of other types to be the same as the one of supl type.
  NETWORK_TYPE_MOBILE_OTHERS: Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_SUPL,

  /**
   * Standard values for the APN connection retry process
   * Retry funcion: time(secs) = A * numer_of_retries^2 + B
   */
  NETWORK_APNRETRY_FACTOR: 8,
  NETWORK_APNRETRY_ORIGIN: 3,
  NETWORK_APNRETRY_MAXRETRIES: 10,

  // Event timer for connection retries
  timer: null,

  get type() {
    if (this.connectedTypes.indexOf("default") != -1) {
      return this.NETWORK_TYPE_MOBILE;
    }
    if (this.connectedTypes.indexOf("mms") != -1) {
      return this.NETWORK_TYPE_MOBILE_MMS;
    }
    if (this.connectedTypes.indexOf("supl") != -1) {
      return this.NETWORK_TYPE_MOBILE_SUPL;
    }
    return this.NETWORK_TYPE_MOBILE_OTHERS;
  },

  name: null,

  ip: null,

  netmask: null,

  broadcast: null,

  dns1: null,

  dns2: null,

  get httpProxyHost() {
    return this.apnSetting.proxy || '';
  },

  get httpProxyPort() {
    return this.apnSetting.port || '';
  },

  debug: function debug(s) {
    dump("-*- RILNetworkInterface[" + this.radioInterface.clientId + ":" +
         this.type + "]: " + s + "\n");
  },

  // nsIRILDataCallback

  dataCallError: function dataCallError(message) {
    if (message.apn != this.apnSetting.apn) {
      return;
    }
    if (DEBUG) this.debug("Data call error on APN: " + message.apn);
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
    if (!this.cid && datacall.apn != this.apnSetting.apn) {
      return;
    }
    if (DEBUG) {
      this.debug("Data call ID: " + datacall.cid + ", interface name: " +
                 datacall.ifname + ", APN name: " + datacall.apn);
    }
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
    if (this.radioInterface.apnSettings.byType.default &&
        (this.radioInterface.apnSettings.byType.default.apn ==
         this.apnSetting.apn)) {
      this.radioInterface.updateRILNetworkInterface();
    }

    if (this.state == RIL.GECKO_NETWORK_STATE_UNKNOWN &&
        this.registeredAsNetworkInterface) {
      gNetworkManager.unregisterNetworkInterface(this);
      this.registeredAsNetworkInterface = false;
      this.cid = null;
      this.connectedTypes = [];
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
  apnSetting: null,

  // APN failed connections. Retry counter
  apnRetryCounter: 0,

  connectedTypes: null,

  inConnectedTypes: function inConnectedTypes(type) {
    return this.connectedTypes.indexOf(type) != -1;
  },

  get connected() {
    return this.state == RIL.GECKO_NETWORK_STATE_CONNECTED;
  },

  connect: function connect(apntype) {
    if (apntype && !this.inConnectedTypes(apntype)) {
      this.connectedTypes.push(apntype);
    }

    if (this.connecting || this.connected) {
      return;
    }

    // When the retry mechanism is running in background and someone calls
    // disconnect(), this.connectedTypes.length has chances to become 0.
    if (!this.connectedTypes.length) {
      return;
    }

    if (!this.registeredAsDataCallCallback) {
      this.radioInterface.registerDataCallCallback(this);
      this.registeredAsDataCallCallback = true;
    }

    if (!this.apnSetting.apn) {
      if (DEBUG) this.debug("APN name is empty, nothing to do.");
      return;
    }

    if (DEBUG) {
      this.debug("Going to set up data connection with APN " +
                 this.apnSetting.apn);
    }
    let radioTechType = this.radioInterface.rilContext.data.type;
    let radioTechnology = RIL.GECKO_RADIO_TECH.indexOf(radioTechType);
    let authType = RIL.RIL_DATACALL_AUTH_TO_GECKO.indexOf(this.apnSetting.authtype);
    // Use the default authType if the value in database is invalid.
    // For the case that user might not select the authentication type.
    if (authType == -1) {
      if (DEBUG) {
        this.debug("Invalid authType " + this.apnSetting.authtype);
      }
      authType = RIL.RIL_DATACALL_AUTH_TO_GECKO.indexOf(RIL.GECKO_DATACALL_AUTH_DEFAULT);
    }
    this.radioInterface.setupDataCall(radioTechnology,
                                      this.apnSetting.apn,
                                      this.apnSetting.user,
                                      this.apnSetting.password,
                                      authType,
                                      "IP");
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
      this.connectedTypes = [];
      if (DEBUG) this.debug("Too many APN Connection retries - STOP retrying");
      return;
    }

    apnRetryTimer = this.NETWORK_APNRETRY_FACTOR *
                    (this.apnRetryCounter * this.apnRetryCounter) +
                    this.NETWORK_APNRETRY_ORIGIN;
    this.apnRetryCounter++;
    if (DEBUG) {
      this.debug("Data call - APN Connection Retry Timer (secs-counter): " +
                 apnRetryTimer + "-" + this.apnRetryCounter);
    }

    if (this.timer == null) {
      // Event timer for connection retries
      this.timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    }
    this.timer.initWithCallback(this, apnRetryTimer * 1000,
                                Ci.nsITimer.TYPE_ONE_SHOT);
  },

  disconnect: function disconnect(apntype) {
    let index = this.connectedTypes.indexOf(apntype);
    if (index != -1) {
      this.connectedTypes.splice(index, 1);
    }

    if (this.connectedTypes.length) {
      return;
    }

    if (this.state == RIL.GECKO_NETWORK_STATE_DISCONNECTING ||
        this.state == RIL.GECKO_NETWORK_STATE_DISCONNECTED ||
        this.state == RIL.GECKO_NETWORK_STATE_UNKNOWN) {
      return;
    }
    let reason = RIL.DATACALL_DEACTIVATE_NO_REASON;
    if (DEBUG) this.debug("Going to disconnet data connection " + this.cid);
    this.radioInterface.deactivateDataCall(this.cid, reason);
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

