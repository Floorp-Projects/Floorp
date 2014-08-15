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

XPCOMUtils.defineLazyGetter(this, "NFC", function () {
  let obj = {};
  Cu.import("resource://gre/modules/nfc_consts.js", obj);
  return obj;
});

Cu.import("resource://gre/modules/systemlibs.js");
const NFC_ENABLED = libcutils.property_get("ro.moz.nfc.enabled", "false") === "true";

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
  "NFC:CheckSessionToken"
];

const NFC_IPC_READ_PERM_MSG_NAMES = [
  "NFC:ReadNDEF",
  "NFC:GetDetailsNDEF",
  "NFC:Connect",
  "NFC:Close",
];

const NFC_IPC_WRITE_PERM_MSG_NAMES = [
  "NFC:WriteNDEF",
  "NFC:MakeReadOnlyNDEF",
  "NFC:SendFile",
  "NFC:RegisterPeerReadyTarget",
  "NFC:UnregisterPeerReadyTarget"
];

const NFC_IPC_MANAGER_PERM_MSG_NAMES = [
  "NFC:CheckP2PRegistration",
  "NFC:NotifyUserAcceptedP2P",
  "NFC:NotifySendFileStatus",
  "NFC:StartPoll",
  "NFC:StopPoll",
  "NFC:PowerOff"
];

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageBroadcaster");
XPCOMUtils.defineLazyServiceGetter(this, "gSystemMessenger",
                                   "@mozilla.org/system-message-internal;1",
                                   "nsISystemMessagesInternal");
XPCOMUtils.defineLazyServiceGetter(this, "UUIDGenerator",
                                    "@mozilla.org/uuid-generator;1",
                                    "nsIUUIDGenerator");
XPCOMUtils.defineLazyGetter(this, "gMessageManager", function () {
  return {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIMessageListener,
                                           Ci.nsIObserver]),

    nfc: null,

    // Manage registered Peer Targets
    peerTargetsMap: {},
    currentPeerAppId: null,

    init: function init(nfc) {
      this.nfc = nfc;

      Services.obs.addObserver(this, NFC.TOPIC_XPCOM_SHUTDOWN, false);
      this._registerMessageListeners();
    },

    _shutdown: function _shutdown() {
      this.nfc.shutdown();
      this.nfc = null;

      Services.obs.removeObserver(this, NFC.TOPIC_XPCOM_SHUTDOWN);
      this._unregisterMessageListeners();
    },

    _registerMessageListeners: function _registerMessageListeners() {
      ppmm.addMessageListener("child-process-shutdown", this);

      for (let msgname of NFC_IPC_MSG_NAMES) {
        ppmm.addMessageListener(msgname, this);
      }

      for (let msgname of NFC_IPC_READ_PERM_MSG_NAMES) {
        ppmm.addMessageListener(msgname, this);
      }

      for (let msgname of NFC_IPC_WRITE_PERM_MSG_NAMES) {
        ppmm.addMessageListener(msgname, this);
      }

      for (let msgname of NFC_IPC_MANAGER_PERM_MSG_NAMES) {
        ppmm.addMessageListener(msgname, this);
      }
    },

    _unregisterMessageListeners: function _unregisterMessageListeners() {
      ppmm.removeMessageListener("child-process-shutdown", this);

      for (let msgname of NFC_IPC_MSG_NAMES) {
        ppmm.removeMessageListener(msgname, this);
      }

      for (let msgname of NFC_IPC_READ_PERM_MSG_NAMES) {
        ppmm.removeMessageListener(msgname, this);
      }

      for (let msgname of NFC_IPC_WRITE_PERM_MSG_NAMES) {
        ppmm.removeMessageListener(msgname, this);
      }

      for (let msgname of NFC_IPC_MANAGER_PERM_MSG_NAMES) {
        ppmm.removeMessageListener(msgname, this);
      }

      ppmm = null;
    },

    registerPeerReadyTarget: function registerPeerReadyTarget(msg) {
      let appInfo = msg.json;
      let targets = this.peerTargetsMap;
      let targetInfo = targets[appInfo.appId];
      // If the application Id is already registered
      if (targetInfo) {
        return;
      }

      // Target not registered yet! Add to the target map
      let newTargetInfo = { target : msg.target,
                            isPeerReadyCalled: false };
      targets[appInfo.appId] = newTargetInfo;
    },

    unregisterPeerReadyTarget: function unregisterPeerReadyTarget(msg) {
      let appInfo = msg.json;
      let targets = this.peerTargetsMap;
      let targetInfo = targets[appInfo.appId];
      if (targetInfo) {
        // Remove the target from the list of registered targets
        delete targets[appInfo.appId]
      }
    },

    removePeerTarget: function removePeerTarget(target) {
      let targets = this.peerTargetsMap;
      Object.keys(targets).forEach((appId) => {
        let targetInfo = targets[appId];
        if (targetInfo && targetInfo.target === target) {
          // Remove the target from the list of registered targets
          delete targets[appId];
        }
      });
    },

    isPeerReadyTarget: function isPeerReadyTarget(appId) {
      let targetInfo = this.peerTargetsMap[appId];
      return (targetInfo != null);
    },

    isPeerReadyCalled: function isPeerReadyCalled(appId) {
      let targetInfo = this.peerTargetsMap[appId];
      return !!targetInfo.IsPeerReadyCalled;
    },

    notifyPeerEvent: function notifyPeerEvent(appId, event, sessionToken) {
      let targetInfo = this.peerTargetsMap[appId];
      targetInfo.target.sendAsyncMessage("NFC:PeerEvent", {
        event: event,
        sessionToken: sessionToken
      });
    },

    checkP2PRegistration: function checkP2PRegistration(msg) {
      // Check if the session and application id yeild a valid registered
      // target.  It should have registered for NFC_PEER_EVENT_READY
      let isValid = !!this.nfc.sessionTokenMap[this.nfc._currentSessionId] &&
                    this.isPeerReadyTarget(msg.json.appId);
      // Remember the current AppId if registered.
      this.currentPeerAppId = (isValid) ? msg.json.appId : null;

      let respMsg = { requestId: msg.json.requestId };
      if(!isValid) {
        respMsg.errorMsg = this.nfc.getErrorMessage(NFC.NFC_GECKO_ERROR_P2P_REG_INVALID);
      }
      // Notify the content process immediately of the status
      msg.target.sendAsyncMessage(msg.name + "Response", respMsg);
    },

    onPeerLost: function onPeerLost(sessionToken) {
      let appId = this.currentPeerAppId;
      // For peerlost, the message is delievered to the target which
      // registered onpeerready and onpeerready has been called before.
      if (this.isPeerReadyTarget(appId) && this.isPeerReadyCalled(appId)) {
        this.notifyPeerEvent(appId, NFC.NFC_PEER_EVENT_LOST, sessionToken);
        this.currentPeerAppId = null;
      }
    },

    /**
     * nsIMessageListener interface methods.
     */

    receiveMessage: function receiveMessage(msg) {
      debug("Received '" + msg.name + "' message from content process");
      if (msg.name == "child-process-shutdown") {
        this.removePeerTarget(msg.target);
        return null;
      }

      if (NFC_IPC_MSG_NAMES.indexOf(msg.name) != -1) {
        // Do nothing.
      } else if (NFC_IPC_READ_PERM_MSG_NAMES.indexOf(msg.name) != -1) {
        if (!msg.target.assertPermission("nfc-read")) {
          debug("Nfc message " + msg.name +
                " from a content process with no 'nfc-read' privileges.");
          return null;
        }
      } else if (NFC_IPC_WRITE_PERM_MSG_NAMES.indexOf(msg.name) != -1) {
        if (!msg.target.assertPermission("nfc-write")) {
          debug("Nfc Peer message  " + msg.name +
                " from a content process with no 'nfc-write' privileges.");
          return null;
        }
      } else if (NFC_IPC_MANAGER_PERM_MSG_NAMES.indexOf(msg.name) != -1) {
        if (!msg.target.assertPermission("nfc-manager")) {
          debug("NFC message " + message.name +
                " from a content process with no 'nfc-manager' privileges.");
          return null;
        }
      } else {
        debug("Ignoring unknown message type: " + msg.name);
        return null;
      }

      switch (msg.name) {
        case "NFC:CheckSessionToken":
          if (msg.json.sessionToken !== this.nfc.sessionTokenMap[this.nfc._currentSessionId]) {
            debug("Received invalid Session Token: " + msg.json.sessionToken +
                  ", current SessionToken: " + this.nfc.sessionTokenMap[this.nfc._currentSessionId]);
            return NFC.NFC_ERROR_BAD_SESSION_ID;
          }
          return NFC.NFC_SUCCESS;
        case "NFC:RegisterPeerReadyTarget":
          this.registerPeerReadyTarget(msg);
          return null;
        case "NFC:UnregisterPeerReadyTarget":
          this.unregisterPeerReadyTarget(msg);
          return null;
        case "NFC:CheckP2PRegistration":
          this.checkP2PRegistration(msg);
          return null;
        case "NFC:NotifyUserAcceptedP2P":
          // Notify the 'NFC_PEER_EVENT_READY' since user has acknowledged
          if (!this.isPeerReadyTarget(msg.json.appId)) {
            debug("Application ID : " + msg.json.appId + " is not a registered PeerReadytarget");
            return null;
          }

          let targetInfo = this.peerTargetsMap[msg.json.appId];
          targetInfo.IsPeerReadyCalled = true;
          let sessionToken = this.nfc.sessionTokenMap[this.nfc._currentSessionId];
          this.notifyPeerEvent(msg.json.appId, NFC.NFC_PEER_EVENT_READY, sessionToken);
          return null;
        case "NFC:NotifySendFileStatus":
          // Upon receiving the status of sendFile operation, send the response
          // to appropriate content process.
          msg.json.type = "NotifySendFileStatus";
          if (msg.json.status !== NFC.NFC_SUCCESS) {
            msg.json.errorMsg =
              this.nfc.getErrorMessage(NFC.NFC_GECKO_ERROR_SEND_FILE_FAILED);
          }
          this.nfc.sendNfcResponse(msg.json);
          return null;
        default:
          return this.nfc.receiveMessage(msg);
      }
    },

    /**
     * nsIObserver interface methods.
     */

    observe: function observe(subject, topic, data) {
      switch (topic) {
        case NFC.TOPIC_XPCOM_SHUTDOWN:
          this._shutdown();
          break;
      }
    },
  };
});

function Nfc() {
  debug("Starting Nfc Service");

  let nfcService = Cc["@mozilla.org/nfc/service;1"].getService(Ci.nsINfcService);
  if (!nfcService) {
    debug("No nfc service component available!");
    return;
  }

  nfcService.start(this);
  this.nfcService = nfcService;

  gMessageManager.init(this);

  // Maps sessionId (that are generated from nfcd) with a unique guid : 'SessionToken'
  this.sessionTokenMap = {};
  this.targetsByRequestId = {};
}

Nfc.prototype = {

  classID:   NFC_CID,
  classInfo: XPCOMUtils.generateCI({classID: NFC_CID,
                                    classDescription: "Nfc",
                                    interfaces: [Ci.nsINfcService]}),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver, Ci.nsINfcEventListener]),

  _currentSessionId: null,

  powerLevel: NFC.NFC_POWER_LEVEL_UNKNOWN,

  /**
   * Send arbitrary message to Nfc service.
   *
   * @param nfcMessageType
   *        A text message type.
   * @param message [optional]
   *        An optional message object to send.
   */
  sendToNfcService: function sendToNfcService(nfcMessageType, message) {
    message = message || {};
    message.type = nfcMessageType;
    this.nfcService.sendCommand(message);
  },

  sendNfcResponse: function sendNfcResponse(message) {
    let target = this.targetsByRequestId[message.requestId];
    if (!target) {
      debug("No target for requestId: " + message.requestId);
      return;
    }
    delete this.targetsByRequestId[message.requestId];

    target.sendAsyncMessage("NFC:" + message.type, message);
  },

  /**
   * Send Error response to content. This is used only
   * in case of discovering an error in message received from
   * content process.
   *
   * @param message
   *        An nsIMessageListener's message parameter.
   */
  sendNfcErrorResponse: function sendNfcErrorResponse(message, errorCode) {
    if (!message.target) {
      return;
    }

    let nfcMsgType = message.name + "Response";
    message.json.errorMsg = this.getErrorMessage(errorCode);
    message.target.sendAsyncMessage(nfcMsgType, message.json);
  },

  getErrorMessage: function getErrorMessage(errorCode) {
    return NFC.NFC_ERROR_MSG[errorCode] ||
           NFC.NFC_ERROR_MSG[NFC.NFC_GECKO_ERROR_GENERIC_FAILURE];
  },

  /**
   * Process the incoming message from the NFC Service.
   */
  onEvent: function onEvent(event) {
    let message = Cu.cloneInto(event, this);
    debug("Received message from NFC Service: " + JSON.stringify(message));

    // mapping error code to error message
    if (message.status !== undefined && message.status !== NFC.NFC_SUCCESS) {
      message.errorMsg = this.getErrorMessage(message.status);
    }

    switch (message.type) {
      case "InitializedNotification":
        // Do nothing.
        break;
      case "TechDiscoveredNotification":
        message.type = "techDiscovered";
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
      case "TechLostNotification":
        message.type = "techLost";

        // Update the upper layers with a session token (alias)
        message.sessionToken = this.sessionTokenMap[this._currentSessionId];
        // Do not expose the actual session to the content
        delete message.sessionId;

        gSystemMessenger.broadcastMessage("nfc-manager-tech-lost", message);
        gMessageManager.onPeerLost(this.sessionTokenMap[this._currentSessionId]);

        delete this.sessionTokenMap[this._currentSessionId];
        this._currentSessionId = null;

        break;
     case "ConfigResponse":
        if (message.status === NFC.NFC_SUCCESS) {
          this.powerLevel = message.powerLevel;
        }

        this.sendNfcResponse(message);
        break;
      case "ConnectResponse": // Fall through.
      case "CloseResponse":
      case "GetDetailsNDEFResponse":
      case "ReadNDEFResponse":
      case "MakeReadOnlyNDEFResponse":
      case "WriteNDEFResponse":
        this.sendNfcResponse(message);
        break;
      default:
        throw new Error("Don't know about this message type: " + message.type);
    }
  },

  nfcService: null,

  sessionTokenMap: null,

  targetsByRequestId: null,

  /**
   * Process a message from the content process.
   */
  receiveMessage: function receiveMessage(message) {
    debug("Received '" + JSON.stringify(message) + "' message from content process");

    let isPowerAPI = message.name == "NFC:StartPoll" ||
                     message.name == "NFC:StopPoll"  ||
                     message.name == "NFC:PowerOff";

    if (!isPowerAPI) {
      if (this.powerLevel != NFC.NFC_POWER_LEVEL_ENABLED) {
        debug("NFC is not enabled. current powerLevel:" + this.powerLevel);
        this.sendNfcErrorResponse(message, NFC.NFC_GECKO_ERROR_NOT_ENABLED);
        return null;
      }

      // Update the current sessionId before sending to the NFC service.
      message.json.sessionId = this._currentSessionId;
    }

    // Sanity check on sessionId
    let sessionToken = this.sessionTokenMap[this._currentSessionId];
    if (message.json.sessionToken && (message.json.sessionToken !== sessionToken)) {
      debug("Invalid Session Token: " + message.json.sessionToken +
            " Expected Session Token: " + sessionToken);
      this.sendNfcErrorResponse(message, NFC.NFC_ERROR_BAD_SESSION_ID);
      return null;
    }

    switch (message.name) {
      case "NFC:StartPoll":
        this.setConfig({powerLevel: NFC.NFC_POWER_LEVEL_ENABLED,
                        requestId: message.json.requestId});
        break;
      case "NFC:StopPoll":
        this.setConfig({powerLevel: NFC.NFC_POWER_LEVEL_LOW,
                        requestId: message.json.requestId});
        break;
      case "NFC:PowerOff":
        this.setConfig({powerLevel: NFC.NFC_POWER_LEVEL_DISABLED,
                        requestId: message.json.requestId});
        break;
      case "NFC:GetDetailsNDEF":
        this.sendToNfcService("getDetailsNDEF", message.json);
        break;
      case "NFC:ReadNDEF":
        this.sendToNfcService("readNDEF", message.json);
        break;
      case "NFC:WriteNDEF":
        this.sendToNfcService("writeNDEF", message.json);
        break;
      case "NFC:MakeReadOnlyNDEF":
        this.sendToNfcService("makeReadOnlyNDEF", message.json);
        break;
      case "NFC:Connect":
        this.sendToNfcService("connect", message.json);
        break;
      case "NFC:Close":
        this.sendToNfcService("close", message.json);
        break;
      case "NFC:SendFile":
        // Chrome process is the arbitrator / mediator between
        // system app (content process) that issued nfc 'sendFile' operation
        // and system app that handles the system message :
        // 'nfc-manager-send-file'. System app subsequently handover's
        // the data to alternate carrier's (BT / WiFi) 'sendFile' interface.

        // Notify system app to initiate BT send file operation
        gSystemMessenger.broadcastMessage("nfc-manager-send-file",
                                           message.json);
        break;
      default:
        debug("UnSupported : Message Name " + message.name);
        return null;
    }
    this.targetsByRequestId[message.json.requestId] = message.target;

    return null;
  },

  /**
   * nsIObserver interface methods.
   */
  observe: function(subject, topic, data) {
    if (topic != "profile-after-change") {
      debug("Should receive 'profile-after-change' only, received " + topic);
    }
  },

  setConfig: function setConfig(prop) {
    this.sendToNfcService("config", prop);
  },

  shutdown: function shutdown() {
    this.nfcService.shutdown();
    this.nfcService = null;
  }
};

if (NFC_ENABLED) {
  this.NSGetFactory = XPCOMUtils.generateNSGetFactory([Nfc]);
}
