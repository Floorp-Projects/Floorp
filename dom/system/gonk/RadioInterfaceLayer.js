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
Cu.import("resource://gre/modules/Sntp.jsm");
Cu.import("resource://gre/modules/systemlibs.js");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "RIL", function () {
  let obj = {};
  Cu.import("resource://gre/modules/ril_consts.js", obj);
  return obj;
});

// Ril quirk to always turn the radio off for the client without SIM card
// except hw default client.
var RILQUIRKS_RADIO_OFF_WO_CARD =
  libcutils.property_get("ro.moz.ril.radio_off_wo_card", "false") == "true";

const RADIOINTERFACELAYER_CID =
  Components.ID("{2d831c8d-6017-435b-a80c-e5d422810cea}");
const RADIOINTERFACE_CID =
  Components.ID("{6a7c91f0-a2b3-4193-8562-8969296c0b54}");

const NS_XPCOM_SHUTDOWN_OBSERVER_ID      = "xpcom-shutdown";
const kNetworkConnStateChangedTopic      = "network-connection-state-changed";
const kMozSettingsChangedObserverTopic   = "mozsettings-changed";
const kSysMsgListenerReadyObserverTopic  = "system-message-listener-ready";
const kSysClockChangeObserverTopic       = "system-clock-change";
const kScreenStateChangedTopic           = "screen-state-changed";

const kSettingsClockAutoUpdateEnabled = "time.clock.automatic-update.enabled";
const kSettingsClockAutoUpdateAvailable = "time.clock.automatic-update.available";
const kSettingsTimezoneAutoUpdateEnabled = "time.timezone.automatic-update.enabled";
const kSettingsTimezoneAutoUpdateAvailable = "time.timezone.automatic-update.available";

const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID = "nsPref:changed";

const kPrefRilNumRadioInterfaces = "ril.numRadioInterfaces";
const kPrefRilDebuggingEnabled = "ril.debugging.enabled";

const RADIO_POWER_OFF_TIMEOUT = 30000;
const HW_DEFAULT_CLIENT_ID = 0;

const NETWORK_TYPE_WIFI        = Ci.nsINetworkInfo.NETWORK_TYPE_WIFI;
const NETWORK_TYPE_MOBILE      = Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE;

// set to true in ril_consts.js to see debug messages
var DEBUG = RIL.DEBUG_RIL;

function updateDebugFlag() {
  // Read debug setting from pref
  let debugPref = Services.prefs.getBoolPref(kPrefRilDebuggingEnabled, false);
  DEBUG = RIL.DEBUG_RIL || debugPref;
}
updateDebugFlag();

function debug(s) {
  dump("-*- RadioInterfaceLayer: " + s + "\n");
}

XPCOMUtils.defineLazyServiceGetter(this, "gIccService",
                                   "@mozilla.org/icc/gonkiccservice;1",
                                   "nsIGonkIccService");

XPCOMUtils.defineLazyServiceGetter(this, "gMobileMessageService",
                                   "@mozilla.org/mobilemessage/mobilemessageservice;1",
                                   "nsIMobileMessageService");

XPCOMUtils.defineLazyServiceGetter(this, "gSmsService",
                                   "@mozilla.org/sms/gonksmsservice;1",
                                   "nsIGonkSmsService");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageBroadcaster");

XPCOMUtils.defineLazyServiceGetter(this, "gSettingsService",
                                   "@mozilla.org/settingsService;1",
                                   "nsISettingsService");

XPCOMUtils.defineLazyServiceGetter(this, "gNetworkManager",
                                   "@mozilla.org/network/manager;1",
                                   "nsINetworkManager");

XPCOMUtils.defineLazyServiceGetter(this, "gTimeService",
                                   "@mozilla.org/time/timeservice;1",
                                   "nsITimeService");

XPCOMUtils.defineLazyServiceGetter(this, "gSystemWorkerManager",
                                   "@mozilla.org/telephony/system-worker-manager;1",
                                   "nsISystemWorkerManager");

XPCOMUtils.defineLazyServiceGetter(this, "gTelephonyService",
                                   "@mozilla.org/telephony/telephonyservice;1",
                                   "nsIGonkTelephonyService");

XPCOMUtils.defineLazyServiceGetter(this, "gMobileConnectionService",
                                   "@mozilla.org/mobileconnection/mobileconnectionservice;1",
                                   "nsIGonkMobileConnectionService");

XPCOMUtils.defineLazyServiceGetter(this, "gCellBroadcastService",
                                   "@mozilla.org/cellbroadcast/cellbroadcastservice;1",
                                   "nsIGonkCellBroadcastService");

XPCOMUtils.defineLazyServiceGetter(this, "gDataCallManager",
                                   "@mozilla.org/datacall/manager;1",
                                   "nsIDataCallManager");

XPCOMUtils.defineLazyServiceGetter(this, "gDataCallInterfaceService",
                                   "@mozilla.org/datacall/interfaceservice;1",
                                   "nsIGonkDataCallInterfaceService");

XPCOMUtils.defineLazyServiceGetter(this, "gStkCmdFactory",
                                   "@mozilla.org/icc/stkcmdfactory;1",
                                   "nsIStkCmdFactory");

XPCOMUtils.defineLazyGetter(this, "gRadioEnabledController", function() {
  let _ril = null;
  let _pendingMessages = [];  // For queueing "setRadioEnabled" message.
  let _isProcessingPending = false;
  let _timer = null;
  let _request = null;
  let _deactivatingDeferred = {};
  let _initializedCardState = {};
  let _allCardStateInitialized = !RILQUIRKS_RADIO_OFF_WO_CARD;

  return {
    init: function(ril) {
      _ril = ril;
    },

    receiveCardState: function(clientId) {
      if (_allCardStateInitialized) {
        return;
      }

      if (DEBUG) debug("RadioControl: receive cardState from " + clientId);
      _initializedCardState[clientId] = true;
      if (Object.keys(_initializedCardState).length == _ril.numRadioInterfaces) {
        _allCardStateInitialized = true;
        this._startProcessingPending();
      }
    },

    setRadioEnabled: function(clientId, data, callback) {
      if (DEBUG) debug("setRadioEnabled: " + clientId + ": " + JSON.stringify(data));
      let message = {
        clientId: clientId,
        data: data,
        callback: callback
      };
      _pendingMessages.push(message);
      this._startProcessingPending();
    },

    notifyRadioStateChanged: function(clientId, radioState) {
      gMobileConnectionService.notifyRadioStateChanged(clientId, radioState);
    },

    _startProcessingPending: function() {
      if (!_isProcessingPending) {
        if (DEBUG) debug("RadioControl: start dequeue");
        _isProcessingPending = true;
        this._processNextMessage();
      }
    },

    _processNextMessage: function() {
      if (_pendingMessages.length === 0 || !_allCardStateInitialized) {
        if (DEBUG) debug("RadioControl: stop dequeue");
        _isProcessingPending = false;
        return;
      }

      let msg = _pendingMessages.shift();
      this._handleMessage(msg);
    },

    _getNumCards: function() {
      let numCards = 0;
      for (let i = 0, N = _ril.numRadioInterfaces; i < N; ++i) {
        if (_ril.getRadioInterface(i).isCardPresent()) {
          numCards++;
        }
      }
      return numCards;
    },

    _isRadioAbleToEnableAtClient: function(clientId, numCards) {
      if (!RILQUIRKS_RADIO_OFF_WO_CARD) {
        return true;
      }

      // We could only turn on the radio for clientId if
      // 1. a SIM card is presented or
      // 2. it is the default clientId and there is no any SIM card at any client.

      if (_ril.getRadioInterface(clientId).isCardPresent()) {
        return true;
      }

      numCards = numCards == null ? this._getNumCards() : numCards;
      if (clientId === HW_DEFAULT_CLIENT_ID && numCards === 0) {
        return true;
      }

      return false;
    },

    _handleMessage: function(message) {
      if (DEBUG) debug("RadioControl: handleMessage: " + JSON.stringify(message));
      let clientId = message.clientId || 0;
      let connection =
        gMobileConnectionService.getItemByServiceId(clientId);
      let radioState = connection && connection.radioState;

      if (message.data.enabled) {
        if (this._isRadioAbleToEnableAtClient(clientId)) {
          this._setRadioEnabledInternal(message);
        } else {
          // Not really do it but respond success.
          message.callback(message.data);
        }

        this._processNextMessage();
      } else {
        _request = this._setRadioEnabledInternal.bind(this, message);

        // In 2G network, modem takes 35+ seconds to process deactivate data
        // call request if device has active voice call (please see bug 964974
        // for more details). Therefore we should hangup all active voice calls
        // first. And considering some DSDS architecture, toggling one radio may
        // toggle both, so we send hangUpAll to all clients.
        let hangUpCallback = {
          QueryInterface: XPCOMUtils.generateQI([Ci.nsITelephonyCallback]),
          notifySuccess: function() {},
          notifyError: function() {}
        };

        gTelephonyService.enumerateCalls({
          QueryInterface: XPCOMUtils.generateQI([Ci.nsITelephonyListener]),
          enumerateCallState: function(aInfo) {
            gTelephonyService.hangUpCall(aInfo.clientId, aInfo.callIndex,
                                         hangUpCallback);
          },
          enumerateCallStateComplete: function() {}
        });

        // In some DSDS architecture with only one modem, toggling one radio may
        // toggle both. Therefore, for safely turning off, we should first
        // explicitly deactivate all data calls from all clients.
        this._deactivateDataCalls().then(() => {
          if (DEBUG) debug("RadioControl: deactivation done");
          this._executeRequest();
        });

        this._createTimer();
      }
    },

    _setRadioEnabledInternal: function(message) {
      let clientId = message.clientId || 0;
      let enabled = message.data.enabled || false;
      let radioInterface = _ril.getRadioInterface(clientId);

      radioInterface.workerMessenger.send("setRadioEnabled", message.data,
                                          (function(response) {
        if (response.errorMsg) {
          // If request fails, set current radio state to unknown, since we will
          // handle it in |mobileConnectionService|.
          this.notifyRadioStateChanged(clientId,
                                       Ci.nsIMobileConnection.MOBILE_RADIO_STATE_UNKNOWN);
        }
        return message.callback(response);
      }).bind(this));
    },

    _deactivateDataCalls: function() {
      if (DEBUG) debug("RadioControl: deactivating data calls...");
      _deactivatingDeferred = {};

      let promise = Promise.resolve();
      for (let i = 0, N = _ril.numRadioInterfaces; i < N; ++i) {
        promise = promise.then(this._deactivateDataCallsForClient(i));
      }

      return promise;
    },

    _deactivateDataCallsForClient: function(clientId) {
      return function() {
        let deferred = _deactivatingDeferred[clientId] = Promise.defer();
        let dataCallHandler = gDataCallManager.getDataCallHandler(clientId);

        dataCallHandler.deactivateDataCalls(function() {
          deferred.resolve();
        });

        return deferred.promise;
      };
    },

    _createTimer: function() {
      if (!_timer) {
        _timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      }
      _timer.initWithCallback(this._executeRequest.bind(this),
                              RADIO_POWER_OFF_TIMEOUT,
                              Ci.nsITimer.TYPE_ONE_SHOT);
    },

    _cancelTimer: function() {
      if (_timer) {
        _timer.cancel();
      }
    },

    _executeRequest: function() {
      if (typeof _request === "function") {
        if (DEBUG) debug("RadioControl: executeRequest");
        this._cancelTimer();
        _request();
        _request = null;
      }
      this._processNextMessage();
    },
  };
});

// Initialize shared preference "ril.numRadioInterfaces" according to system
// property.
try {
  Services.prefs.setIntPref(kPrefRilNumRadioInterfaces, (function() {
    // When Gonk property "ro.moz.ril.numclients" is not set, return 1; if
    // explicitly set to any number larger-equal than 0, return num; else, return
    // 1 for compatibility.
    try {
      let numString = libcutils.property_get("ro.moz.ril.numclients", "1");
      let num = parseInt(numString, 10);
      if (num >= 0) {
        return num;
      }
    } catch (e) {}

    return 1;
  })());
} catch (e) {}

function DataCall(aAttributes) {
  for (let key in aAttributes) {
    if (key === "pdpType") {
      // Convert pdp type into constant int value.
      this[key] = RIL.RIL_DATACALL_PDP_TYPES.indexOf(aAttributes[key]);
      continue;
    }

    this[key] = aAttributes[key];
  }
}
DataCall.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDataCall]),

  failCause: Ci.nsIDataCallInterface.DATACALL_FAIL_NONE,
  suggestedRetryTime: -1,
  cid: -1,
  active: -1,
  pdpType: -1,
  ifname: null,
  addreses: null,
  dnses: null,
  gateways: null,
  pcscf: null,
  mtu: -1
};

function RadioInterfaceLayer() {
  let workerMessenger = new WorkerMessenger();
  workerMessenger.init();
  this.setWorkerDebugFlag = workerMessenger.setDebugFlag.bind(workerMessenger);

  let numIfaces = this.numRadioInterfaces;
  if (DEBUG) debug(numIfaces + " interfaces");
  this.radioInterfaces = [];
  for (let clientId = 0; clientId < numIfaces; clientId++) {
    this.radioInterfaces.push(new RadioInterface(clientId, workerMessenger));
  }

  Services.obs.addObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  Services.prefs.addObserver(kPrefRilDebuggingEnabled, this, false);

  gRadioEnabledController.init(this);
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

  observe: function(subject, topic, data) {
    switch (topic) {
      case NS_XPCOM_SHUTDOWN_OBSERVER_ID:
        for (let radioInterface of this.radioInterfaces) {
          radioInterface.shutdown();
        }
        this.radioInterfaces = null;
        Services.obs.removeObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
        break;

      case NS_PREFBRANCH_PREFCHANGE_TOPIC_ID:
        if (data === kPrefRilDebuggingEnabled) {
          updateDebugFlag();
          this.setWorkerDebugFlag(DEBUG);
        }
        break;
    }
  },

  /**
   * nsIRadioInterfaceLayer interface methods.
   */

  getRadioInterface: function(clientId) {
    return this.radioInterfaces[clientId];
  },

  setMicrophoneMuted: function(muted) {
    for (let clientId = 0; clientId < this.numRadioInterfaces; clientId++) {
      let radioInterface = this.radioInterfaces[clientId];
      radioInterface.workerMessenger.send("setMute", { muted: muted });
    }
  }
};

XPCOMUtils.defineLazyGetter(RadioInterfaceLayer.prototype,
                            "numRadioInterfaces", function() {
  try {
    return Services.prefs.getIntPref(kPrefRilNumRadioInterfaces);
  } catch(e) {}

  return 1;
});

function WorkerMessenger() {
  // Initial owning attributes.
  this.radioInterfaces = [];
  this.tokenCallbackMap = {};

  this.worker = new ChromeWorker("resource://gre/modules/ril_worker.js");
  this.worker.onerror = this.onerror.bind(this);
  this.worker.onmessage = this.onmessage.bind(this);
}
WorkerMessenger.prototype = {
  radioInterfaces: null,
  worker: null,

  // This gets incremented each time we send out a message.
  token: 1,

  // Maps tokens we send out with messages to the message callback.
  tokenCallbackMap: null,

  init: function() {
    let options = {
      debug: DEBUG,
      quirks: {
        callstateExtraUint32:
          libcutils.property_get("ro.moz.ril.callstate_extra_int", "false") === "true",
        requestUseDialEmergencyCall:
          libcutils.property_get("ro.moz.ril.dial_emergency_call", "false") === "true",
        simAppStateExtraFields:
          libcutils.property_get("ro.moz.ril.simstate_extra_field", "false") === "true",
        extraUint2ndCall:
          libcutils.property_get("ro.moz.ril.extra_int_2nd_call", "false") === "true",
        haveQueryIccLockRetryCount:
          libcutils.property_get("ro.moz.ril.query_icc_count", "false") === "true",
        sendStkProfileDownload:
          libcutils.property_get("ro.moz.ril.send_stk_profile_dl", "false") === "true",
        smscAddressFormat:
          libcutils.property_get("ro.moz.ril.smsc_address_format", "text"),
        dataRegistrationOnDemand:
          libcutils.property_get("ro.moz.ril.data_reg_on_demand", "false") === "true",
        subscriptionControl:
          libcutils.property_get("ro.moz.ril.subscription_control", "false") === "true",
        signalExtraInt:
          libcutils.property_get("ro.moz.ril.signal_extra_int", "false") === "true",
        availableNetworkExtraStr:
          libcutils.property_get("ro.moz.ril.avlbl_nw_extra_str", "false") === "true",
      }
    };

    this.send(null, "setInitialOptions", options);
  },

  setDebugFlag: function(aDebug) {
    let options = { debug: aDebug };
    this.send(null, "setDebugFlag", options);
  },

  debug: function(aClientId, aMessage) {
    // We use the same debug subject with RadioInterface's here.
    dump("-*- RadioInterface[" + aClientId + "]: " + aMessage + "\n");
  },

  onerror: function(event) {
    if (DEBUG) {
      this.debug("X", "Got an error: " + event.filename + ":" +
                 event.lineno + ": " + event.message + "\n");
    }
    event.preventDefault();
  },

  /**
   * Process the incoming message from the RIL worker.
   */
  onmessage: function(event) {
    let message = event.data;
    let clientId = message.rilMessageClientId;
    if (clientId === null) {
      return;
    }

    if (DEBUG) {
      this.debug(clientId, "Received message from worker: " + JSON.stringify(message));
    }

    let token = message.rilMessageToken;
    if (token == null) {
      // That's an unsolicited message.  Pass to RadioInterface directly.
      let radioInterface = this.radioInterfaces[clientId];
      radioInterface.handleUnsolicitedWorkerMessage(message);
      return;
    }

    let callback = this.tokenCallbackMap[message.rilMessageToken];
    if (!callback) {
      if (DEBUG) this.debug(clientId, "Ignore orphan token: " + message.rilMessageToken);
      return;
    }

    let keep = false;
    try {
      keep = callback(message);
    } catch(e) {
      if (DEBUG) this.debug(clientId, "callback throws an exception: " + e);
    }

    if (!keep) {
      delete this.tokenCallbackMap[message.rilMessageToken];
    }
  },

  registerClient: function(aClientId, aRadioInterface) {
    if (DEBUG) this.debug(aClientId, "Starting RIL Worker");

    // Keep a reference so that we can dispatch unsolicited messages to it.
    this.radioInterfaces[aClientId] = aRadioInterface;

    this.send(null, "registerClient", { clientId: aClientId });
    gSystemWorkerManager.registerRilWorker(aClientId, this.worker);
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
   *        with an message containing a "rilMessageToken" attribute of the
   *        same value we passed.  This callback function accepts only one
   *        parameter -- the reply from worker.  It also returns a boolean
   *        value true to keep current token-callback mapping and wait for
   *        another worker reply, or false to remove the mapping.
   */
  send: function(clientId, rilMessageType, message, callback) {
    message = message || {};

    message.rilMessageClientId = clientId;
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
  }
};

function RadioInterface(aClientId, aWorkerMessenger) {
  this.clientId = aClientId;
  this.workerMessenger = {
    send: aWorkerMessenger.send.bind(aWorkerMessenger, aClientId)
  };
  aWorkerMessenger.registerClient(aClientId, this);

  this.operatorInfo = {};

  let lock = gSettingsService.createLock();

  // Read the "time.clock.automatic-update.enabled" setting to see if
  // we need to adjust the system clock time by NITZ or SNTP.
  lock.get(kSettingsClockAutoUpdateEnabled, this);

  // Read the "time.timezone.automatic-update.enabled" setting to see if
  // we need to adjust the system timezone by NITZ.
  lock.get(kSettingsTimezoneAutoUpdateEnabled, this);

  // Set "time.clock.automatic-update.available" to false when starting up.
  this.setClockAutoUpdateAvailable(false);

  // Set "time.timezone.automatic-update.available" to false when starting up.
  this.setTimezoneAutoUpdateAvailable(false);

  Services.obs.addObserver(this, kMozSettingsChangedObserverTopic, false);
  Services.obs.addObserver(this, kSysClockChangeObserverTopic, false);
  Services.obs.addObserver(this, kScreenStateChangedTopic, false);

  Services.obs.addObserver(this, kNetworkConnStateChangedTopic, false);

  this._sntp = new Sntp(this.setClockBySntp.bind(this),
                        Services.prefs.getIntPref("network.sntp.maxRetryCount"),
                        Services.prefs.getIntPref("network.sntp.refreshPeriod"),
                        Services.prefs.getIntPref("network.sntp.timeout"),
                        Services.prefs.getCharPref("network.sntp.pools").split(";"),
                        Services.prefs.getIntPref("network.sntp.port"));
}

RadioInterface.prototype = {

  classID:   RADIOINTERFACE_CID,
  classInfo: XPCOMUtils.generateCI({classID: RADIOINTERFACE_CID,
                                    classDescription: "RadioInterface",
                                    interfaces: [Ci.nsIRadioInterface]}),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIRadioInterface,
                                         Ci.nsIObserver,
                                         Ci.nsISettingsServiceCallback]),

  // A private wrapped WorkerMessenger instance.
  workerMessenger: null,

  debug: function(s) {
    dump("-*- RadioInterface[" + this.clientId + "]: " + s + "\n");
  },

  shutdown: function() {
    Services.obs.removeObserver(this, kMozSettingsChangedObserverTopic);
    Services.obs.removeObserver(this, kSysClockChangeObserverTopic);
    Services.obs.removeObserver(this, kScreenStateChangedTopic);
    Services.obs.removeObserver(this, kNetworkConnStateChangedTopic);
  },

  isCardPresent: function() {
    let icc = gIccService.getIccByServiceId(this.clientId);
    let cardState = icc ? icc.cardState : Ci.nsIIcc.CARD_STATE_UNKNOWN;
    return cardState !== Ci.nsIIcc.CARD_STATE_UNDETECTED &&
      cardState !== Ci.nsIIcc.CARD_STATE_UNKNOWN;
  },

  handleUnsolicitedWorkerMessage: function(message) {
    switch (message.rilMessageType) {
      case "callRing":
        gTelephonyService.notifyCallRing();
        break;
      case "currentCalls":
        gTelephonyService.notifyCurrentCalls(this.clientId, message.calls);
        break;
      case "cdmaCallWaiting":
        gTelephonyService.notifyCdmaCallWaiting(this.clientId,
                                                message.waitingCall);
        break;
      case "suppSvcNotification":
        gTelephonyService.notifySupplementaryService(this.clientId,
                                                     message.number,
                                                     message.notification);
        break;
      case "ussdreceived":
        gTelephonyService.notifyUssdReceived(this.clientId, message.message,
                                             message.sessionEnded);
        break;
      case "datacalllistchanged":
        let dataCalls = message.datacalls.map(dataCall => new DataCall(dataCall));
        gDataCallInterfaceService.notifyDataCallListChanged(this.clientId,
                                                            dataCalls.length,
                                                            dataCalls);
        break;
      case "emergencyCbModeChange":
        gMobileConnectionService.notifyEmergencyCallbackModeChanged(this.clientId,
                                                                    message.active,
                                                                    message.timeoutMs);
        break;
      case "networkinfochanged":
        gMobileConnectionService.notifyNetworkInfoChanged(this.clientId,
                                                          message);
        break;
      case "networkselectionmodechange":
        gMobileConnectionService.notifyNetworkSelectModeChanged(this.clientId,
                                                                message.mode);
        break;
      case "voiceregistrationstatechange":
        gMobileConnectionService.notifyVoiceInfoChanged(this.clientId, message);
        break;
      case "dataregistrationstatechange":
        gMobileConnectionService.notifyDataInfoChanged(this.clientId, message);
        break;
      case "signalstrengthchange":
        gMobileConnectionService.notifySignalStrengthChanged(this.clientId,
                                                             message);
        break;
      case "operatorchange":
        gMobileConnectionService.notifyOperatorChanged(this.clientId, message);
        break;
      case "otastatuschange":
        gMobileConnectionService.notifyOtaStatusChanged(this.clientId, message.status);
        break;
      case "deviceidentitieschange":
        gMobileConnectionService.notifyDeviceIdentitiesChanged(this.clientId,
                                                               message.deviceIdentities.imei,
                                                               message.deviceIdentities.imeisv,
                                                               message.deviceIdentities.esn,
                                                               message.deviceIdentities.meid);
        break;
      case "radiostatechange":
        // gRadioEnabledController should know the radio state for each client,
        // so notify gRadioEnabledController here.
        gRadioEnabledController.notifyRadioStateChanged(this.clientId,
                                                        message.radioState);
        break;
      case "cardstatechange":
        gIccService.notifyCardStateChanged(this.clientId,
                                           message.cardState);
        gRadioEnabledController.receiveCardState(this.clientId);
        break;
      case "sms-received":
        this.handleSmsReceived(message);
        break;
      case "cellbroadcast-received":
        this.handleCellbroadcastMessageReceived(message);
        break;
      case "nitzTime":
        this.handleNitzTime(message);
        break;
      case "iccinfochange":
        gIccService.notifyIccInfoChanged(this.clientId,
                                         message.iccid ? message : null);
        break;
      case "iccimsi":
        gIccService.notifyImsiChanged(this.clientId, message.imsi);
        break;
      case "iccmbdn":
        this.handleIccMbdn(message);
        break;
      case "iccmwis":
        this.handleIccMwis(message.mwi);
        break;
      case "stkcommand":
        gIccService.notifyStkCommand(this.clientId,
                                     gStkCmdFactory.createCommand(message));
        break;
      case "stksessionend":
        gIccService.notifyStkSessionEnd(this.clientId);
        break;
      case "cdma-info-rec-received":
        this.handleCdmaInformationRecords(message.records);
        break;
      default:
        throw new Error("Don't know about this message type: " +
                        message.rilMessageType);
    }
  },

  setDataRegistration: function(attach) {
    let deferred = Promise.defer();
    this.workerMessenger.send("setDataRegistration",
                              {attach: attach},
                              (function(response) {
      // Always resolve to proceed with the following steps.
      deferred.resolve(response.errorMsg ? response.errorMsg : null);
    }).bind(this));

    return deferred.promise;
  },

  /**
   * TODO: Bug 911713 - B2G NetworkManager: Move policy control logic to
   *                    NetworkManager
   */
  updateRILNetworkInterface: function() {
    let connHandler = gDataCallManager.getDataCallHandler(this.clientId);
    connHandler.updateRILNetworkInterface();
  },

  /**
   * handle received SMS.
   */
  handleSmsReceived: function(aMessage) {
    let header = aMessage.header;
    // Concatenation Info:
    // - segmentRef: a modulo 256 counter indicating the reference number for a
    //               particular concatenated short message. '0' is a valid number.
    // - The concatenation info will not be available in |header| if
    //   segmentSeq or segmentMaxSeq is 0.
    // See 3GPP TS 23.040, 9.2.3.24.1 Concatenated Short Messages.
    let segmentRef = (header && header.segmentRef !== undefined)
      ? header.segmentRef : 1;
    let segmentSeq = header && header.segmentSeq || 1;
    let segmentMaxSeq = header && header.segmentMaxSeq || 1;
    // Application Ports:
    // The port number ranges from 0 to 49151.
    // see 3GPP TS 23.040, 9.2.3.24.3/4 Application Port Addressing.
    let originatorPort = (header && header.originatorPort !== undefined)
      ? header.originatorPort
      : Ci.nsIGonkSmsService.SMS_APPLICATION_PORT_INVALID;
    let destinationPort = (header && header.destinationPort !== undefined)
      ? header.destinationPort
      : Ci.nsIGonkSmsService.SMS_APPLICATION_PORT_INVALID;
    // MWI info:
    let mwiPresent = (aMessage.mwi)? true : false;
    let mwiDiscard = (mwiPresent)? aMessage.mwi.discard: false;
    let mwiMsgCount = (mwiPresent)? aMessage.mwi.msgCount: 0;
    let mwiActive = (mwiPresent)? aMessage.mwi.active: false;
    // CDMA related attributes:
    let cdmaMessageType = aMessage.messageType || 0;
    let cdmaTeleservice = aMessage.teleservice || 0;
    let cdmaServiceCategory = aMessage.serviceCategory || 0;

    gSmsService
      .notifyMessageReceived(this.clientId,
                             aMessage.SMSC || null,
                             aMessage.sentTimestamp,
                             aMessage.sender,
                             aMessage.pid,
                             aMessage.encoding,
                             RIL.GECKO_SMS_MESSAGE_CLASSES
                               .indexOf(aMessage.messageClass),
                             aMessage.language || null,
                             segmentRef,
                             segmentSeq,
                             segmentMaxSeq,
                             originatorPort,
                             destinationPort,
                             mwiPresent,
                             mwiDiscard,
                             mwiMsgCount,
                             mwiActive,
                             cdmaMessageType,
                             cdmaTeleservice,
                             cdmaServiceCategory,
                             aMessage.body || null,
                             aMessage.data || [],
                             (aMessage.data) ? aMessage.data.length : 0);
  },

  /**
   * Set the setting value of "time.clock.automatic-update.available".
   */
  setClockAutoUpdateAvailable: function(value) {
    gSettingsService.createLock().set(kSettingsClockAutoUpdateAvailable, value, null);
  },

  /**
   * Set the setting value of "time.timezone.automatic-update.available".
   */
  setTimezoneAutoUpdateAvailable: function(value) {
    gSettingsService.createLock().set(kSettingsTimezoneAutoUpdateAvailable, value, null);
  },

  /**
   * Set the system clock by NITZ.
   */
  setClockByNitz: function(message) {
    // To set the system clock time. Note that there could be a time diff
    // between when the NITZ was received and when the time is actually set.
    gTimeService.set(
      message.networkTimeInMS + (Date.now() - message.receiveTimeInMS));
  },

  /**
   * Set the system time zone by NITZ.
   */
  setTimezoneByNitz: function(message) {
    // To set the sytem timezone. Note that we need to convert the time zone
    // value to a UTC repesentation string in the format of "UTC(+/-)hh:mm".
    // Ex, time zone -480 is "UTC+08:00"; time zone 630 is "UTC-10:30".
    //
    // We can unapply the DST correction if we want the raw time zone offset:
    // message.networkTimeZoneInMinutes -= message.networkDSTInMinutes;
    if (message.networkTimeZoneInMinutes != (new Date()).getTimezoneOffset()) {
      let absTimeZoneInMinutes = Math.abs(message.networkTimeZoneInMinutes);
      let timeZoneStr = "UTC";
      timeZoneStr += (message.networkTimeZoneInMinutes > 0 ? "-" : "+");
      timeZoneStr += ("0" + Math.floor(absTimeZoneInMinutes / 60)).slice(-2);
      timeZoneStr += ":";
      timeZoneStr += ("0" + absTimeZoneInMinutes % 60).slice(-2);
      gSettingsService.createLock().set("time.timezone", timeZoneStr, null);
    }
  },

  /**
   * Handle the NITZ message.
   */
  handleNitzTime: function(message) {
    // Got the NITZ info received from the ril_worker.
    this.setClockAutoUpdateAvailable(true);
    this.setTimezoneAutoUpdateAvailable(true);

    // Cache the latest NITZ message whenever receiving it.
    this._lastNitzMessage = message;

    // Set the received NITZ clock if the setting is enabled.
    if (this._clockAutoUpdateEnabled) {
      this.setClockByNitz(message);
    }
    // Set the received NITZ timezone if the setting is enabled.
    if (this._timezoneAutoUpdateEnabled) {
      this.setTimezoneByNitz(message);
    }
  },

  /**
   * Set the system clock by SNTP.
   */
  setClockBySntp: function(offset) {
    // Got the SNTP info.
    this.setClockAutoUpdateAvailable(true);
    if (!this._clockAutoUpdateEnabled) {
      return;
    }
    if (this._lastNitzMessage) {
      if (DEBUG) debug("SNTP: NITZ available, discard SNTP");
      return;
    }
    gTimeService.set(Date.now() + offset);
  },

  handleIccMbdn: function(message) {
    let service = Cc["@mozilla.org/voicemail/voicemailservice;1"]
                  .getService(Ci.nsIGonkVoicemailService);
    service.notifyInfoChanged(this.clientId, message.number, message.alphaId);
  },

  handleIccMwis: function(mwi) {
    let service = Cc["@mozilla.org/voicemail/voicemailservice;1"]
                  .getService(Ci.nsIGonkVoicemailService);
    // Note: returnNumber and returnMessage is not available from UICC.
    service.notifyStatusChanged(this.clientId, mwi.active, mwi.msgCount,
                                null, null);
  },

  _convertCbGsmGeographicalScope: function(aGeographicalScope) {
    return (aGeographicalScope != null)
      ? aGeographicalScope
      : Ci.nsICellBroadcastService.GSM_GEOGRAPHICAL_SCOPE_INVALID;
  },

  _convertCbMessageClass: function(aMessageClass) {
    let index = RIL.GECKO_SMS_MESSAGE_CLASSES.indexOf(aMessageClass);
    return (index != -1)
      ? index
      : Ci.nsICellBroadcastService.GSM_MESSAGE_CLASS_NORMAL;
  },

  _convertCbEtwsWarningType: function(aWarningType) {
    return (aWarningType != null)
      ? aWarningType
      : Ci.nsICellBroadcastService.GSM_ETWS_WARNING_INVALID;
  },

  handleCellbroadcastMessageReceived: function(aMessage) {
    let etwsInfo = aMessage.etws;
    let hasEtwsInfo = etwsInfo != null;
    let serviceCategory = (aMessage.serviceCategory)
      ? aMessage.serviceCategory
      : Ci.nsICellBroadcastService.CDMA_SERVICE_CATEGORY_INVALID;

    gCellBroadcastService
      .notifyMessageReceived(this.clientId,
                             this._convertCbGsmGeographicalScope(aMessage.geographicalScope),
                             aMessage.messageCode,
                             aMessage.messageId,
                             aMessage.language,
                             aMessage.fullBody,
                             this._convertCbMessageClass(aMessage.messageClass),
                             Date.now(),
                             serviceCategory,
                             hasEtwsInfo,
                             (hasEtwsInfo)
                               ? this._convertCbEtwsWarningType(etwsInfo.warningType)
                               : Ci.nsICellBroadcastService.GSM_ETWS_WARNING_INVALID,
                             hasEtwsInfo ? etwsInfo.emergencyUserAlert : false,
                             hasEtwsInfo ? etwsInfo.popup : false);
  },

  handleCdmaInformationRecords: function(aRecords) {
    if (DEBUG) this.debug("cdma-info-rec-received: " + JSON.stringify(aRecords));

    let clientId = this.clientId;

    aRecords.forEach(function(aRecord) {
      if (aRecord.display) {
        gMobileConnectionService
          .notifyCdmaInfoRecDisplay(clientId, aRecord.display);
        return;
      }

      if (aRecord.calledNumber) {
        gMobileConnectionService
          .notifyCdmaInfoRecCalledPartyNumber(clientId,
                                              aRecord.calledNumber.type,
                                              aRecord.calledNumber.plan,
                                              aRecord.calledNumber.number,
                                              aRecord.calledNumber.pi,
                                              aRecord.calledNumber.si);
        return;
      }

      if (aRecord.callingNumber) {
        gMobileConnectionService
          .notifyCdmaInfoRecCallingPartyNumber(clientId,
                                               aRecord.callingNumber.type,
                                               aRecord.callingNumber.plan,
                                               aRecord.callingNumber.number,
                                               aRecord.callingNumber.pi,
                                               aRecord.callingNumber.si);
        return;
      }

      if (aRecord.connectedNumber) {
        gMobileConnectionService
          .notifyCdmaInfoRecConnectedPartyNumber(clientId,
                                                 aRecord.connectedNumber.type,
                                                 aRecord.connectedNumber.plan,
                                                 aRecord.connectedNumber.number,
                                                 aRecord.connectedNumber.pi,
                                                 aRecord.connectedNumber.si);
        return;
      }

      if (aRecord.signal) {
        gMobileConnectionService
          .notifyCdmaInfoRecSignal(clientId,
                                   aRecord.signal.type,
                                   aRecord.signal.alertPitch,
                                   aRecord.signal.signal);
        return;
      }

      if (aRecord.redirect) {
        gMobileConnectionService
          .notifyCdmaInfoRecRedirectingNumber(clientId,
                                              aRecord.redirect.type,
                                              aRecord.redirect.plan,
                                              aRecord.redirect.number,
                                              aRecord.redirect.pi,
                                              aRecord.redirect.si,
                                              aRecord.redirect.reason);
        return;
      }

      if (aRecord.lineControl) {
        gMobileConnectionService
          .notifyCdmaInfoRecLineControl(clientId,
                                        aRecord.lineControl.polarityIncluded,
                                        aRecord.lineControl.toggle,
                                        aRecord.lineControl.reverse,
                                        aRecord.lineControl.powerDenial);
        return;
      }

      if (aRecord.clirCause) {
        gMobileConnectionService
          .notifyCdmaInfoRecClir(clientId,
                                 aRecord.clirCause);
        return;
      }

      if (aRecord.audioControl) {
        gMobileConnectionService
          .notifyCdmaInfoRecAudioControl(clientId,
                                         aRecord.audioControl.upLink,
                                         aRecord.audioControl.downLink);
        return;
      }
    });
  },

  // nsIObserver

  observe: function(subject, topic, data) {
    switch (topic) {
      case kMozSettingsChangedObserverTopic:
        if ("wrappedJSObject" in subject) {
          subject = subject.wrappedJSObject;
        }
        this.handleSettingsChange(subject.key, subject.value, subject.isInternalChange);
        break;
      case kSysClockChangeObserverTopic:
        let offset = parseInt(data, 10);
        if (this._lastNitzMessage) {
          this._lastNitzMessage.receiveTimeInMS += offset;
        }
        this._sntp.updateOffset(offset);
        break;
      case kNetworkConnStateChangedTopic:
        let networkInfo = subject.QueryInterface(Ci.nsINetworkInfo);
        if (networkInfo.state != Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED) {
          return;
        }

        // SNTP can only update when we have mobile or Wifi connections.
        if (networkInfo.type != NETWORK_TYPE_WIFI &&
            networkInfo.type != NETWORK_TYPE_MOBILE) {
          return;
        }

        // If the network comes from RIL, make sure the RIL service is matched.
        if (subject instanceof Ci.nsIRilNetworkInfo) {
          networkInfo = subject.QueryInterface(Ci.nsIRilNetworkInfo);
          if (networkInfo.serviceId != this.clientId) {
            return;
          }
        }

        // SNTP won't update unless the SNTP is already expired.
        if (this._sntp.isExpired()) {
          this._sntp.request();
        }
        break;
      case kScreenStateChangedTopic:
        this.workerMessenger.send("setScreenState", { on: (data === "on") });
        break;
    }
  },

  // Flag to determine whether to update system clock automatically. It
  // corresponds to the "time.clock.automatic-update.enabled" setting.
  _clockAutoUpdateEnabled: null,

  // Flag to determine whether to update system timezone automatically. It
  // corresponds to the "time.clock.automatic-update.enabled" setting.
  _timezoneAutoUpdateEnabled: null,

  // Remember the last NITZ message so that we can set the time based on
  // the network immediately when users enable network-based time.
  _lastNitzMessage: null,

  // Object that handles SNTP.
  _sntp: null,

  // Cell Broadcast settings values.
  _cellBroadcastSearchList: null,

  handleSettingsChange: function(aName, aResult, aIsInternalSetting) {
    // Don't allow any content processes to modify the setting
    // "time.clock.automatic-update.available" except for the chrome process.
    if (aName === kSettingsClockAutoUpdateAvailable &&
        !aIsInternalSetting) {
      let isClockAutoUpdateAvailable = this._lastNitzMessage !== null ||
                                       this._sntp.isAvailable();
      if (aResult !== isClockAutoUpdateAvailable) {
        if (DEBUG) {
          debug("Content processes cannot modify 'time.clock.automatic-update.available'. Restore!");
        }
        // Restore the setting to the current value.
        this.setClockAutoUpdateAvailable(isClockAutoUpdateAvailable);
      }
    }

    // Don't allow any content processes to modify the setting
    // "time.timezone.automatic-update.available" except for the chrome
    // process.
    if (aName === kSettingsTimezoneAutoUpdateAvailable &&
        !aIsInternalSetting) {
      let isTimezoneAutoUpdateAvailable = this._lastNitzMessage !== null;
      if (aResult !== isTimezoneAutoUpdateAvailable) {
        if (DEBUG) {
          this.debug("Content processes cannot modify 'time.timezone.automatic-update.available'. Restore!");
        }
        // Restore the setting to the current value.
        this.setTimezoneAutoUpdateAvailable(isTimezoneAutoUpdateAvailable);
      }
    }

    this.handle(aName, aResult);
  },

  // nsISettingsServiceCallback
  handle: function(aName, aResult) {
    switch(aName) {
      case kSettingsClockAutoUpdateEnabled:
        this._clockAutoUpdateEnabled = aResult;
        if (!this._clockAutoUpdateEnabled) {
          break;
        }

        // Set the latest cached NITZ time if it's available.
        if (this._lastNitzMessage) {
          this.setClockByNitz(this._lastNitzMessage);
        } else if (gNetworkManager.activeNetworkInfo &&
                   gNetworkManager.activeNetworkInfo.state ==
                   Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED) {
          // Set the latest cached SNTP time if it's available.
          if (!this._sntp.isExpired()) {
            this.setClockBySntp(this._sntp.getOffset());
          } else {
            // Or refresh the SNTP.
            this._sntp.request();
          }
        } else {
          // Set a sane minimum time.
          let buildTime = libcutils.property_get("ro.build.date.utc", "0") * 1000;
          let file = FileUtils.File("/system/b2g/b2g");
          if (file.lastModifiedTime > buildTime) {
            buildTime = file.lastModifiedTime;
          }
          if (buildTime > Date.now()) {
            gTimeService.set(buildTime);
          }
        }
        break;
      case kSettingsTimezoneAutoUpdateEnabled:
        this._timezoneAutoUpdateEnabled = aResult;

        if (this._timezoneAutoUpdateEnabled) {
          // Apply the latest cached NITZ for timezone if it's available.
          if (this._timezoneAutoUpdateEnabled && this._lastNitzMessage) {
            this.setTimezoneByNitz(this._lastNitzMessage);
          }
        }
        break;
    }
  },

  handleError: function(aErrorMessage) {
    if (DEBUG) {
      this.debug("There was an error while reading RIL settings.");
    }
  },

  // nsIRadioInterface

  // TODO: Bug 928861 - B2G NetworkManager: Provide a more generic function
  //                    for connecting
  setupDataCallByType: function(networkType) {
    let connHandler = gDataCallManager.getDataCallHandler(this.clientId);
    connHandler.setupDataCallByType(networkType);
  },

  // TODO: Bug 928861 - B2G NetworkManager: Provide a more generic function
  //                    for connecting
  deactivateDataCallByType: function(networkType) {
    let connHandler = gDataCallManager.getDataCallHandler(this.clientId);
    connHandler.deactivateDataCallByType(networkType);
  },

  // TODO: Bug 904514 - [meta] NetworkManager enhancement
  getDataCallStateByType: function(networkType) {
    let connHandler = gDataCallManager.getDataCallHandler(this.clientId);
    return connHandler.getDataCallStateByType(networkType);
  },

  sendWorkerMessage: function(rilMessageType, message, callback) {
    // Special handler for setRadioEnabled.
    if (rilMessageType === "setRadioEnabled") {
      // Forward it to gRadioEnabledController.
      gRadioEnabledController.setRadioEnabled(this.clientId, message,
                                              callback.handleResponse);
      return;
    }

    if (callback) {
      this.workerMessenger.send(rilMessageType, message, function(response) {
        return callback.handleResponse(response);
      });
    } else {
      this.workerMessenger.send(rilMessageType, message);
    }
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([RadioInterfaceLayer]);
