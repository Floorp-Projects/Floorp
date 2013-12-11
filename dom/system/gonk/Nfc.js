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

/* Copyright © 2013, Deutsche Telekom, Inc. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

let NFC = {};
Cu.import("resource://gre/modules/nfc_consts.js", NFC);

// set to true in nfc_consts.js to see debug messages
let DEBUG = NFC.DEBUG_NFC;

let debug;
if (DEBUG) {
  debug = function (s) {
    dump("-*- Nfc: " + s + "\n");
  };
} else {
  debug = function (s) {};
}

const NFC_CONTRACTID = "@mozilla.org/nfc;1";
const NFC_CID =
  Components.ID("{2ff24790-5e74-11e1-b86c-0800200c9a66}");

const NFC_IPC_MSG_NAMES = [
  "NFC:SetSessionToken",
  "NFC:ReadNDEF",
  "NFC:WriteNDEF",
  "NFC:GetDetailsNDEF",
  "NFC:MakeReadOnlyNDEF",
  "NFC:Connect",
  "NFC:Close"
];

const NFC_IPC_PEER_MSG_NAMES = [
  "NFC:RegisterPeerTarget",
  "NFC:UnregisterPeerTarget",
  "NFC:CheckP2PRegistration",
  "NFC:NotifyUserAcceptedP2P"
];

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageBroadcaster");
XPCOMUtils.defineLazyServiceGetter(this, "gSystemMessenger",
                                   "@mozilla.org/system-message-internal;1",
                                   "nsISystemMessagesInternal");
XPCOMUtils.defineLazyServiceGetter(this, "gSystemWorkerManager",
                                   "@mozilla.org/telephony/system-worker-manager;1",
                                   "nsISystemWorkerManager");
XPCOMUtils.defineLazyServiceGetter(this, "gSettingsService",
                                   "@mozilla.org/settingsService;1",
                                   "nsISettingsService");
XPCOMUtils.defineLazyServiceGetter(this, "UUIDGenerator",
                                    "@mozilla.org/uuid-generator;1",
                                    "nsIUUIDGenerator");
XPCOMUtils.defineLazyGetter(this, "gMessageManager", function () {
  return {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIMessageListener,
                                           Ci.nsIObserver]),

    nfc: null,

    // Manage message targets in terms of sessionToken. Only the authorized and
    // registered contents can receive related messages.
    targetsBySessionTokens: {},
    sessionTokens: [],

    // Manage registered Peer Targets
    peerTargetsMap: {},
    currentPeerAppId: null,

    init: function init(nfc) {
      this.nfc = nfc;

      Services.obs.addObserver(this, "xpcom-shutdown", false);
      this._registerMessageListeners();
    },

    _shutdown: function _shutdown() {
      this.nfc = null;

      Services.obs.removeObserver(this, "xpcom-shutdown");
      this._unregisterMessageListeners();
    },

    _registerMessageListeners: function _registerMessageListeners() {
      ppmm.addMessageListener("child-process-shutdown", this);
      for (let msgname of NFC_IPC_MSG_NAMES) {
        ppmm.addMessageListener(msgname, this);
      }

      for (let msgname of NFC_IPC_PEER_MSG_NAMES) {
        ppmm.addMessageListener(msgname, this);
      }
    },

    _unregisterMessageListeners: function _unregisterMessageListeners() {
      ppmm.removeMessageListener("child-process-shutdown", this);
      for (let msgname of NFC_IPC_MSG_NAMES) {
        ppmm.removeMessageListener(msgname, this);
      }

      for (let msgname of NFC_IPC_PEER_MSG_NAMES) {
        ppmm.removeMessageListener(msgname, this);
      }
      ppmm = null;
    },

    _registerMessageTarget: function _registerMessageTarget(sessionToken, target) {
      let targets = this.targetsBySessionTokens[sessionToken];
      if (!targets) {
        targets = this.targetsBySessionTokens[sessionToken] = [];
        let list = this.sessionTokens;
        if (list.indexOf(sessionToken) == -1) {
          list.push(sessionToken);
        }
      }

      if (targets.indexOf(target) != -1) {
        debug("Already registered this target!");
        return;
      }

      targets.push(target);
      debug("Registered :" + sessionToken + " target: " + target);
    },

    _unregisterMessageTarget: function _unregisterMessageTarget(sessionToken, target) {
      if (sessionToken == null) {
        // Unregister the target for every sessionToken when no sessionToken is specified.
        for (let session of this.sessionTokens) {
          this._unregisterMessageTarget(session, target);
        }
        return;
      }

      // Unregister the target for a specified sessionToken.
      let targets = this.targetsBySessionTokens[sessionToken];
      if (!targets) {
        return;
      }

      if (target == null) {
        debug("Unregistered all targets for the " + sessionToken + " targets: " + targets);
        targets = [];
        let list = this.sessionTokens;
        if (sessionToken !== null) {
          let index = list.indexOf(sessionToken);
          if (index > -1) {
            list.splice(index, 1);
          }
        }
        return;
      }

      let index = targets.indexOf(target);
      if (index != -1) {
        targets.splice(index, 1);
      }
    },

    _sendTargetMessage: function _sendTargetMessage(sessionToken, message, options) {
      let targets = this.targetsBySessionTokens[sessionToken];
      if (!targets) {
        return;
      }

      for (let target of targets) {
        target.sendAsyncMessage(message, options);
      }
    },

    registerPeerTarget: function registerPeerTarget(msg) {
      let appInfo = msg.json;
      // Sanity check on PeerEvent
      if (!this.isValidPeerEvent(appInfo.event)) {
        return;
      }
      let targets = this.peerTargetsMap;
      let targetInfo = targets[appInfo.appId];
      // If the application Id is already registered
      if (targetInfo) {
        // If the event is not registered
        if (targetInfo.event !== appInfo.event) {
          // Update the event field ONLY
          targetInfo.event |= appInfo.event;
        }
        // Otherwise event is already registered, return!
        return;
      }
      // Target not registered yet! Add to the target map

      // Registered targetInfo target consists of 2 fields (values)
      // target : Target to notify the right content for peer notifications
      // event  : NFC_PEER_EVENT_READY (0x01) Or NFC_PEER_EVENT_LOST (0x02)
      let newTargetInfo = { target : msg.target,
                            event  : appInfo.event };
      targets[appInfo.appId] = newTargetInfo;
    },

    unregisterPeerTarget: function unregisterPeerTarget(msg) {
      let appInfo = msg.json;
      // Sanity check on PeerEvent
      if (!this.isValidPeerEvent(appInfo.event)) {
        return;
      }
      let targets = this.peerTargetsMap;
      let targetInfo = targets[appInfo.appId];
      if (targetInfo) {
        // Application Id registered and the event exactly matches.
        if (targetInfo.event === appInfo.event) {
          // Remove the target from the list of registered targets
          delete targets[appInfo.appId]
        }
        else {
          // Otherwise, update the event field ONLY, by removing the event flag
          targetInfo.event &= ~appInfo.event;
        }
      }
    },

    isRegisteredP2PTarget: function isRegisteredP2PTarget(appId, event) {
      let targetInfo = this.peerTargetsMap[appId];
      // Check if it is a registered target for the 'event'
      return ((targetInfo != null) && (targetInfo.event & event !== 0));
    },

    notifyPeerEvent: function notifyPeerEvent(appId, event) {
      let targetInfo = this.peerTargetsMap[appId];
      // Check if the application id is a registeredP2PTarget
      if (this.isRegisteredP2PTarget(appId, event)) {
        targetInfo.target.sendAsyncMessage("NFC:PeerEvent", {
          event: event,
          sessionToken: this.nfc.sessionTokenMap[this.nfc._currentSessionId]
        });
        return;
      }
      debug("Application ID : " + appId + " is not a registered target" +
                             "for the event " + event + " notification");
    },

    isValidPeerEvent: function isValidPeerEvent(event) {
      // Valid values : 0x01, 0x02 Or 0x03
      return ((event === NFC.NFC_PEER_EVENT_READY) ||
              (event === NFC.NFC_PEER_EVENT_LOST)  ||
              (event === (NFC.NFC_PEER_EVENT_READY | NFC.NFC_PEER_EVENT_LOST)));
    },

    /**
     * nsIMessageListener interface methods.
     */

    receiveMessage: function receiveMessage(msg) {
      debug("Received '" + msg.name + "' message from content process");
      if (msg.name == "child-process-shutdown") {
        // By the time we receive child-process-shutdown, the child process has
        // already forgotten its permissions so we need to unregister the target
        // for every permission.
        this._unregisterMessageTarget(null, msg.target);
        return null;
      }

      if (NFC_IPC_MSG_NAMES.indexOf(msg.name) != -1) {
        if (!msg.target.assertPermission("nfc-read")) {
          debug("Nfc message " + msg.name +
                " from a content process with no 'nfc-read' privileges.");
          return null;
        }
      } else if (NFC_IPC_PEER_MSG_NAMES.indexOf(msg.name) != -1) {
        if (!msg.target.assertPermission("nfc-write")) {
          debug("Nfc Peer message  " + msg.name +
                " from a content process with no 'nfc-write' privileges.");
          return null;
        }

        // Add extra permission check for two IPC Peer events:
        // 'NFC:CheckP2PRegistration' , 'NFC:NotifyUserAcceptedP2P'
        if ((msg.name == "NFC:CheckP2PRegistration") ||
            (msg.name == "NFC:NotifyUserAcceptedP2P")) {
          // ONLY privileged Content can send these two events
          if (!msg.target.assertPermission("nfc-manager")) {
            debug("NFC message " + message.name +
                  " from a content process with no 'nfc-manager' privileges.");
            return null;
          }
        }
      } else {
        debug("Ignoring unknown message type: " + msg.name);
        return null;
      }

      switch (msg.name) {
        case "NFC:SetSessionToken":
          this._registerMessageTarget(this.nfc.sessionTokenMap[this.nfc._currentSessionId], msg.target);
          debug("Registering target for this SessionToken : " +
                this.nfc.sessionTokenMap[this.nfc._currentSessionId]);
          break;
        case "NFC:RegisterPeerTarget":
          this.registerPeerTarget(msg);
          break;
        case "NFC:UnregisterPeerTarget":
          this.unregisterPeerTarget(msg);
          break;
        case "NFC:CheckP2PRegistration":
          // Check if the application id is a valid registered target.
          // (It should have registered for NFC_PEER_EVENT_READY).
          let isRegistered = this.isRegisteredP2PTarget(msg.json.appId,
                                                        NFC.NFC_PEER_EVENT_READY);
          // Remember the current AppId if registered.
          this.currentPeerAppId = (isRegistered) ? msg.json.appId : null;
          let status = (isRegistered) ? NFC.GECKO_NFC_ERROR_SUCCESS :
                                        NFC.GECKO_NFC_ERROR_GENERIC_FAILURE;
          // Notify the content process immediately of the status
          msg.target.sendAsyncMessage(msg.name + "Response", {
            status: status,
            requestId: msg.json.requestId
          });
          break;
        case "NFC:NotifyUserAcceptedP2P":
          // Notify the 'NFC_PEER_EVENT_READY' since user has acknowledged
          this.notifyPeerEvent(msg.json.appId, NFC.NFC_PEER_EVENT_READY);
          break;
      }
      return null;
    },

    /**
     * nsIObserver interface methods.
     */

    observe: function observe(subject, topic, data) {
      switch (topic) {
        case "xpcom-shutdown":
          this._shutdown();
          break;
      }
    },

    sendNfcResponseMessage: function sendNfcResponseMessage(message, data) {
      this._sendTargetMessage(this.nfc.sessionTokenMap[this.nfc._currentSessionId], message, data);
    },
  };
});

function Nfc() {
  debug("Starting Worker");
  this.worker = new ChromeWorker("resource://gre/modules/nfc_worker.js");
  this.worker.onerror = this.onerror.bind(this);
  this.worker.onmessage = this.onmessage.bind(this);

  for each (let msgname in NFC_IPC_MSG_NAMES) {
    ppmm.addMessageListener(msgname, this);
  }

  Services.obs.addObserver(this, NFC.TOPIC_MOZSETTINGS_CHANGED, false);
  Services.obs.addObserver(this, NFC.TOPIC_XPCOM_SHUTDOWN, false);

  gMessageManager.init(this);
  let lock = gSettingsService.createLock();
  lock.get(NFC.SETTING_NFC_POWER_LEVEL, this);
  lock.get(NFC.SETTING_NFC_ENABLED, this);
  // Maps sessionId (that are generated from nfcd) with a unique guid : 'SessionToken'
  this.sessionTokenMap = {};

  gSystemWorkerManager.registerNfcWorker(this.worker);
}

Nfc.prototype = {

  classID:   NFC_CID,
  classInfo: XPCOMUtils.generateCI({classID: NFC_CID,
                                    classDescription: "Nfc",
                                    interfaces: [Ci.nsIWorkerHolder]}),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWorkerHolder,
                                         Ci.nsIObserver,
                                         Ci.nsISettingsServiceCallback]),

  _currentSessionId: null,
  _enabled: false,

  onerror: function onerror(event) {
    debug("Got an error: " + event.filename + ":" +
          event.lineno + ": " + event.message + "\n");
    event.preventDefault();
  },

  /**
   * Send arbitrary message to worker.
   *
   * @param nfcMessageType
   *        A text message type.
   * @param message [optional]
   *        An optional message object to send.
   */
  sendToWorker: function sendToWorker(nfcMessageType, message) {
    message = message || {};
    message.type = nfcMessageType;
    this.worker.postMessage(message);
  },

  /**
   * Send Error response to content.
   *
   * @param message
   *        An nsIMessageListener's message parameter.
   */
  sendNfcErrorResponse: function sendNfcErrorResponse(message) {
    if (!message.target) {
      return;
    }

    let nfcMsgType = message.name + "Response";
    message.target.sendAsyncMessage(nfcMsgType, {
      sessionId: message.json.sessionToken,
      requestId: message.json.requestId,
      status: NFC.GECKO_NFC_ERROR_GENERIC_FAILURE
    });
  },

  /**
   * Process the incoming message from the NFC worker
   */
  onmessage: function onmessage(event) {
    let message = event.data;
    debug("Received message from NFC worker: " + JSON.stringify(message));

    switch (message.type) {
      case "techDiscovered":
        this._currentSessionId = message.sessionId;

        // Check if the session token already exists. If exists, continue to use the same one.
        // If not, generate a new token.
        if (!this.sessionTokenMap[this._currentSessionId]) {
          this.sessionTokenMap[this._currentSessionId] = UUIDGenerator.generateUUID().toString();
        }
        // Update the upper layers with a session token (alias)
        message.sessionToken = this.sessionTokenMap[this._currentSessionId];
        // Do not expose the actual session to the content
        delete message.sessionId;

        gSystemMessenger.broadcastMessage("nfc-manager-tech-discovered", message);
        break;
      case "techLost":
        gMessageManager._unregisterMessageTarget(this.sessionTokenMap[this._currentSessionId], null);

        // Update the upper layers with a session token (alias)
        message.sessionToken = this.sessionTokenMap[this._currentSessionId];
        // Do not expose the actual session to the content
        delete message.sessionId;

        gSystemMessenger.broadcastMessage("nfc-manager-tech-lost", message);
        // Notify 'PeerLost' to appropriate registered target, if any
        gMessageManager.notifyPeerEvent(this.currentPeerAppId, NFC.NFC_PEER_EVENT_LOST);
        delete this.sessionTokenMap[this._currentSessionId];
        this._currentSessionId = null;
        this.currentPeerAppId = null;
        break;
     case "ConfigResponse":
        gSystemMessenger.broadcastMessage("nfc-powerlevel-change", message);
        break;
      case "ConnectResponse": // Fall through.
      case "CloseResponse":
      case "GetDetailsNDEFResponse":
      case "ReadNDEFResponse":
      case "MakeReadOnlyNDEFResponse":
      case "WriteNDEFResponse":
        message.sessionToken = this.sessionTokenMap[this._currentSessionId];
        // Do not expose the actual session to the content
        delete message.sessionId;
        gMessageManager.sendNfcResponseMessage("NFC:" + message.type, message);
        break;
      default:
        throw new Error("Don't know about this message type: " + message.type);
    }
  },

  // nsINfcWorker
  worker: null,

  powerLevel: NFC.NFC_POWER_LEVEL_DISABLED,

  sessionTokenMap: null,

  /**
   * Process a message from the content process.
   */
  receiveMessage: function receiveMessage(message) {
    debug("Received '" + JSON.stringify(message) + "' message from content process");

    if (!this._enabled) {
      debug("NFC is not enabled.");
      this.sendNfcErrorResponse(message);
      return null;
    }

    // Enforce bare minimums for NFC permissions
    switch (message.name) {
      case "NFC:Connect": // Fall through
      case "NFC:Close":
      case "NFC:GetDetailsNDEF":
      case "NFC:ReadNDEF":
        if (!message.target.assertPermission("nfc-read")) {
          debug("NFC message " + message.name +
                " from a content process with no 'nfc-read' privileges.");
          this.sendNfcErrorResponse(message);
          return null;
        }
        break;
      case "NFC:WriteNDEF": // Fall through
      case "NFC:MakeReadOnlyNDEF":
        if (!message.target.assertPermission("nfc-write")) {
          debug("NFC message " + message.name +
                " from a content process with no 'nfc-write' privileges.");
          this.sendNfcErrorResponse(message);
          return null;
        }
        break;
      case "NFC:SetSessionToken":
        //Do nothing here. No need to process this message further
        return null;
    }

    // Sanity check on sessionId
    if (message.json.sessionToken !== this.sessionTokenMap[this._currentSessionId]) {
      debug("Invalid Session Token: " + message.json.sessionToken +
            " Expected Session Token: " + this.sessionTokenMap[this._currentSessionId]);
      this.sendNfcErrorResponse(message);
      return null;
    }

    // Update the current sessionId before sending to the worker
    message.sessionId = this._currentSessionId;

    switch (message.name) {
      case "NFC:GetDetailsNDEF":
        this.sendToWorker("getDetailsNDEF", message.json);
        break;
      case "NFC:ReadNDEF":
        this.sendToWorker("readNDEF", message.json);
        break;
      case "NFC:WriteNDEF":
        this.sendToWorker("writeNDEF", message.json);
        break;
      case "NFC:MakeReadOnlyNDEF":
        this.sendToWorker("makeReadOnlyNDEF", message.json);
        break;
      case "NFC:Connect":
        this.sendToWorker("connect", message.json);
        break;
      case "NFC:Close":
        this.sendToWorker("close", message.json);
        break;
      default:
        debug("UnSupported : Message Name " + message.name);
        return null;
    }
  },

  /**
   * nsISettingsServiceCallback
   */

  handle: function handle(aName, aResult) {
    switch(aName) {
      case NFC.SETTING_NFC_ENABLED:
        debug("'nfc.enabled' is now " + aResult);
        this._enabled = aResult;
        // General power setting
        let powerLevel = this._enabled ? NFC.NFC_POWER_LEVEL_ENABLED :
                                         NFC.NFC_POWER_LEVEL_DISABLED;
        // Only if the value changes, set the power config and persist
        if (powerLevel !== this.powerLevel) {
          debug("New Power Level " + powerLevel);
          this.setConfig({powerLevel: powerLevel});
          this.powerLevel = powerLevel;
        }
        break;
    }
  },

  /**
   * nsIObserver
   */

  observe: function observe(subject, topic, data) {
    switch (topic) {
      case NFC.TOPIC_XPCOM_SHUTDOWN:
        for each (let msgname in NFC_IPC_MSG_NAMES) {
          ppmm.removeMessageListener(msgname, this);
        }
        ppmm = null;
        Services.obs.removeObserver(this, NFC.TOPIC_XPCOM_SHUTDOWN);
        break;
      case NFC.TOPIC_MOZSETTINGS_CHANGED:
        let setting = JSON.parse(data);
        if (setting) {
          let setting = JSON.parse(data);
          this.handle(setting.key, setting.value);
        }
        break;
    }
  },

  setConfig: function setConfig(prop) {
    this.sendToWorker("config", prop);
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([Nfc]);
