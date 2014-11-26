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
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

XPCOMUtils.defineLazyGetter(this, "NFC", function () {
  let obj = {};
  Cu.import("resource://gre/modules/nfc_consts.js", obj);
  return obj;
});

Cu.import("resource://gre/modules/systemlibs.js");
const NFC_ENABLED = libcutils.property_get("ro.moz.nfc.enabled", "false") === "true";

// set to true to in nfc_consts.js to see debug messages
let DEBUG = NFC.DEBUG_CONTENT_HELPER;

let debug;
function updateDebug() {
  if (DEBUG || NFC.DEBUG_CONTENT_HELPER) {
    debug = function (s) {
      dump("-*- NfcContentHelper: " + s + "\n");
    };
  } else {
    debug = function (s) {};
  }
};
updateDebug();

const NFCCONTENTHELPER_CID =
  Components.ID("{4d72c120-da5f-11e1-9b23-0800200c9a66}");

const NFC_IPC_MSG_NAMES = [
  "NFC:ReadNDEFResponse",
  "NFC:WriteNDEFResponse",
  "NFC:MakeReadOnlyResponse",
  "NFC:FormatResponse",
  "NFC:ConnectResponse",
  "NFC:CloseResponse",
  "NFC:CheckP2PRegistrationResponse",
  "NFC:DOMEvent",
  "NFC:NotifySendFileStatusResponse",
  "NFC:ChangeRFStateResponse"
];

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsISyncMessageSender");

function NfcContentHelper() {
  Services.obs.addObserver(this, NFC.TOPIC_MOZSETTINGS_CHANGED, false);
  Services.obs.addObserver(this, "xpcom-shutdown", false);

  this._requestMap = [];
}

NfcContentHelper.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsINfcContentHelper,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver]),
  classID:   NFCCONTENTHELPER_CID,
  classInfo: XPCOMUtils.generateCI({
    classID:          NFCCONTENTHELPER_CID,
    classDescription: "NfcContentHelper",
    interfaces:       [Ci.nsINfcContentHelper]
  }),

  _window: null,
  _requestMap: null,
  _rfState: null,
  eventListener: null,

  init: function init(aWindow) {
    if (aWindow == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    this._window = aWindow;
    this.initDOMRequestHelper(this._window, NFC_IPC_MSG_NAMES);

    if (!NFC.DEBUG_CONTENT_HELPER && this._window.navigator.mozSettings) {
      let lock = this._window.navigator.mozSettings.createLock();
      var nfcDebug = lock.get(NFC.SETTING_NFC_DEBUG);
      nfcDebug.onsuccess = function _nfcDebug() {
        DEBUG = nfcDebug.result[NFC.SETTING_NFC_DEBUG];
        updateDebug();
      };
    }

    let info = cpmm.sendSyncMessage("NFC:QueryInfo")[0];
    this._rfState = info.rfState;
  },

  queryRFState: function queryRFState() {
    return this._rfState;
  },

  encodeNDEFRecords: function encodeNDEFRecords(records) {
    let encodedRecords = [];
    for (let i = 0; i < records.length; i++) {
      let record = records[i];
      encodedRecords.push({
        tnf: record.tnf,
        type: record.type || undefined,
        id: record.id || undefined,
        payload: record.payload || undefined,
      });
    }
    return encodedRecords;
  },

  // NFCTag interface
  readNDEF: function readNDEF(sessionToken, callback) {
    let requestId = callback.getCallbackId();
    this._requestMap[requestId] = callback;

    cpmm.sendAsyncMessage("NFC:ReadNDEF", {
      requestId: requestId,
      sessionToken: sessionToken
    });
  },

  writeNDEF: function writeNDEF(records, sessionToken, callback) {
    let requestId = callback.getCallbackId();
    this._requestMap[requestId] = callback;

    let encodedRecords = this.encodeNDEFRecords(records);
    cpmm.sendAsyncMessage("NFC:WriteNDEF", {
      requestId: requestId,
      sessionToken: sessionToken,
      records: encodedRecords
    });
  },

  makeReadOnly: function makeReadOnly(sessionToken, callback) {
    let requestId = callback.getCallbackId();
    this._requestMap[requestId] = callback;

    cpmm.sendAsyncMessage("NFC:MakeReadOnly", {
      requestId: requestId,
      sessionToken: sessionToken
    });
  },

  format: function format(sessionToken, callback) {
    let requestId = callback.getCallbackId();
    this._requestMap[requestId] = callback;

    cpmm.sendAsyncMessage("NFC:Format", {
      requestId: requestId,
      sessionToken: sessionToken
    });
  },

  connect: function connect(techType, sessionToken, callback) {
    let requestId = callback.getCallbackId();
    this._requestMap[requestId] = callback;

    cpmm.sendAsyncMessage("NFC:Connect", {
      requestId: requestId,
      sessionToken: sessionToken,
      techType: techType
    });
  },

  close: function close(sessionToken, callback) {
    let requestId = callback.getCallbackId();
    this._requestMap[requestId] = callback;

    cpmm.sendAsyncMessage("NFC:Close", {
      requestId: requestId,
      sessionToken: sessionToken
    });
  },

  sendFile: function sendFile(data, sessionToken, callback) {
    let requestId = callback.getCallbackId();
    this._requestMap[requestId] = callback;

    cpmm.sendAsyncMessage("NFC:SendFile", {
      requestId: requestId,
      sessionToken: sessionToken,
      blob: data.blob
    });
  },

  notifySendFileStatus: function notifySendFileStatus(status, requestId) {
    cpmm.sendAsyncMessage("NFC:NotifySendFileStatus", {
      status: status,
      requestId: requestId
    });
  },

  addEventListener: function addEventListener(listener) {
    this.eventListener = listener;
    cpmm.sendAsyncMessage("NFC:AddEventListener");
  },

  registerTargetForPeerReady: function registerTargetForPeerReady(appId) {
    cpmm.sendAsyncMessage("NFC:RegisterPeerReadyTarget", { appId: appId });
  },

  unregisterTargetForPeerReady: function unregisterTargetForPeerReady(appId) {
    cpmm.sendAsyncMessage("NFC:UnregisterPeerReadyTarget", { appId: appId });
  },

  checkP2PRegistration: function checkP2PRegistration(appId, callback) {
    let requestId = callback.getCallbackId();
    this._requestMap[requestId] = callback;

    cpmm.sendAsyncMessage("NFC:CheckP2PRegistration", {
      appId: appId,
      requestId: requestId
    });
  },

  notifyUserAcceptedP2P: function notifyUserAcceptedP2P(appId) {
    cpmm.sendAsyncMessage("NFC:NotifyUserAcceptedP2P", {
      appId: appId
    });
  },

  changeRFState: function changeRFState(rfState, callback) {
    let requestId = callback.getCallbackId();
    this._requestMap[requestId] = callback;

    cpmm.sendAsyncMessage("NFC:ChangeRFState",
                          {requestId: requestId,
                           rfState: rfState});
  },

  // nsIObserver
  observe: function observe(subject, topic, data) {
    if (topic == "xpcom-shutdown") {
      this.destroyDOMRequestHelper();
      Services.obs.removeObserver(this, NFC.TOPIC_MOZSETTINGS_CHANGED);
      Services.obs.removeObserver(this, "xpcom-shutdown");
      cpmm = null;
    } else if (topic == NFC.TOPIC_MOZSETTINGS_CHANGED) {
      if ("wrappedJSObject" in subject) {
        subject = subject.wrappedJSObject;
      }
      if (subject) {
        this.handle(subject.key, subject.value);
      }
    }
  },

  // nsIMessageListener
  receiveMessage: function receiveMessage(message) {
    DEBUG && debug("Message received: " + JSON.stringify(message));
    let result = message.json;

    switch (message.name) {
      case "NFC:ReadNDEFResponse":
        this.handleReadNDEFResponse(result);
        break;
      case "NFC:CheckP2PRegistrationResponse":
        this.handleCheckP2PRegistrationResponse(result);
        break;
      case "NFC:ConnectResponse": // Fall through.
      case "NFC:CloseResponse":
      case "NFC:WriteNDEFResponse":
      case "NFC:MakeReadOnlyResponse":
      case "NFC:FormatResponse":
      case "NFC:NotifySendFileStatusResponse":
      case "NFC:ChangeRFStateResponse":
        this.handleGeneralResponse(result);
        break;
      case "NFC:DOMEvent":
        switch (result.event) {
          case NFC.PEER_EVENT_READY:
            this.eventListener.notifyPeerFound(result.sessionToken, /* isPeerReady */ true);
            break;
          case NFC.PEER_EVENT_FOUND:
            this.eventListener.notifyPeerFound(result.sessionToken);
            break;
          case NFC.PEER_EVENT_LOST:
            this.eventListener.notifyPeerLost(result.sessionToken);
            break;
          case NFC.TAG_EVENT_FOUND:
            let event = new NfcTagEvent(result.techList,
                                        result.tagType,
                                        result.maxNDEFSize,
                                        result.isReadOnly,
                                        result.isFormatable);

            this.eventListener.notifyTagFound(result.sessionToken, event, result.records);
            break;
          case NFC.TAG_EVENT_LOST:
            this.eventListener.notifyTagLost(result.sessionToken);
            break;
          case NFC.RF_EVENT_STATE_CHANGE:
            this._rfState = result.rfState;
            this.eventListener.notifyRFStateChange(this._rfState);
            break;
        }
        break;
    }
  },

  handle: function handle(name, result) {
    switch (name) {
      case NFC.SETTING_NFC_DEBUG:
        DEBUG = result;
        updateDebug();
        break;
    }
  },

  handleGeneralResponse: function handleReadNDEFResponse(result) {
    let requestId = result.requestId;
    let callback = this._requestMap[requestId];
    if (!callback) {
      debug("not firing message " + result.type + " for id: " + requestId);
      return;
    }
    delete this._requestMap[requestId];

    if (result.errorMsg) {
      callback.notifyError(result.errorMsg);
    } else {
      callback.notifySuccess();
    }
  },

  handleReadNDEFResponse: function handleReadNDEFResponse(result) {
    let requestId = result.requestId;
    let callback = this._requestMap[requestId];
    if (!callback) {
      debug("not firing message handleReadNDEFResponse for id: " + requestId);
      return;
    }
    delete this._requestMap[requestId];

    if (result.errorMsg) {
      callback.notifyError(result.errorMsg);
      return;
    }

    let ndefMsg = [];
    let records = result.records;
    for (let i = 0; i < records.length; i++) {
      let record = records[i];
      ndefMsg.push(new this._window.MozNDEFRecord({tnf: record.tnf,
                                                   type: record.type,
                                                   id: record.id,
                                                   payload: record.payload}));
    }
    callback.notifySuccessWithNDEFRecords(ndefMsg);
  },

  handleCheckP2PRegistrationResponse: function handleCheckP2PRegistrationResponse(result) {
    let requestId = result.requestId;
    let callback = this._requestMap[requestId];
    if (!callback) {
      debug("not firing message handleCheckP2PRegistrationResponse for id: " + requestId);
      return;
    }
    delete this._requestMap[requestId];

    // Privilaged status API. Always fire success to avoid using exposed props.
    // The receiver must check the boolean mapped status code to handle.
    callback.notifySuccessWithBoolean(!result.errorMsg);
  },
};

function NfcTagEvent(techList, tagType, maxNDEFSize, isReadOnly, isFormatable) {
  this.techList = techList;
  this.tagType = tagType;
  this.maxNDEFSize = maxNDEFSize;
  this.isReadOnly = isReadOnly;
  this.isFormatable = isFormatable;
}
NfcTagEvent.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsINfcTagEvent]),

  techList: null,
  tagType: null,
  maxNDEFSize: 0,
  isReadOnly: false,
  isFormatable: false
};

if (NFC_ENABLED) {
  this.NSGetFactory = XPCOMUtils.generateNSGetFactory([NfcContentHelper]);
}
