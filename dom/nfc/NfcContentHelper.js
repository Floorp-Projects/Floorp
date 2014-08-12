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
if (DEBUG) {
  debug = function (s) {
    dump("-*- NfcContentHelper: " + s + "\n");
  };
} else {
  debug = function (s) {};
}

const NFCCONTENTHELPER_CID =
  Components.ID("{4d72c120-da5f-11e1-9b23-0800200c9a66}");

const NFC_IPC_MSG_NAMES = [
  "NFC:ReadNDEFResponse",
  "NFC:WriteNDEFResponse",
  "NFC:GetDetailsNDEFResponse",
  "NFC:MakeReadOnlyNDEFResponse",
  "NFC:ConnectResponse",
  "NFC:CloseResponse",
  "NFC:CheckP2PRegistrationResponse",
  "NFC:PeerEvent",
  "NFC:NotifySendFileStatusResponse",
  "NFC:ConfigResponse"
];

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsISyncMessageSender");

function GetDetailsNDEFResponse(details) {
  this.canBeMadeReadOnly = details.canBeMadeReadOnly;
  this.isReadOnly = details.isReadOnly;
  this.maxSupportedLength = details.maxSupportedLength;
}
GetDetailsNDEFResponse.prototype = {
  __exposedProps__ : {canBeMadeReadOnly: 'r',
                      isReadOnly: 'r',
                      maxSupportedLength: 'r'}
};

function NfcContentHelper() {
  this.initDOMRequestHelper(/* aWindow */ null, NFC_IPC_MSG_NAMES);
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

  _requestMap: null,
  peerEventListener: null,

  encodeNDEFRecords: function encodeNDEFRecords(records) {
    let encodedRecords = [];
    for (let i = 0; i < records.length; i++) {
      let record = records[i];
      encodedRecords.push({
        tnf: record.tnf,
        type: record.type,
        id: record.id,
        payload: record.payload,
      });
    }
    return encodedRecords;
  },

  // NFC interface:
  setSessionToken: function setSessionToken(sessionToken) {
    if (sessionToken == null) {
      throw Components.Exception("No session token!",
                                  Cr.NS_ERROR_UNEXPECTED);
      return false;
    }
    // Report session to Nfc.js only.
    let val = cpmm.sendSyncMessage("NFC:SetSessionToken", {
      sessionToken: sessionToken
    });
    return (val[0] === NFC.NFC_SUCCESS);
  },

  // NFCTag interface
  getDetailsNDEF: function getDetailsNDEF(window, sessionToken) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = btoa(this.getRequestId(request));
    this._requestMap[requestId] = window;

    cpmm.sendAsyncMessage("NFC:GetDetailsNDEF", {
      requestId: requestId,
      sessionToken: sessionToken
    });
    return request;
  },

  readNDEF: function readNDEF(window, sessionToken) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = btoa(this.getRequestId(request));
    this._requestMap[requestId] = window;

    cpmm.sendAsyncMessage("NFC:ReadNDEF", {
      requestId: requestId,
      sessionToken: sessionToken
    });
    return request;
  },

  writeNDEF: function writeNDEF(window, records, sessionToken) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = btoa(this.getRequestId(request));
    this._requestMap[requestId] = window;

    let encodedRecords = this.encodeNDEFRecords(records);
    cpmm.sendAsyncMessage("NFC:WriteNDEF", {
      requestId: requestId,
      sessionToken: sessionToken,
      records: encodedRecords
    });
    return request;
  },

  makeReadOnlyNDEF: function makeReadOnlyNDEF(window, sessionToken) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = btoa(this.getRequestId(request));
    this._requestMap[requestId] = window;

    cpmm.sendAsyncMessage("NFC:MakeReadOnlyNDEF", {
      requestId: requestId,
      sessionToken: sessionToken
    });
    return request;
  },

  connect: function connect(window, techType, sessionToken) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = btoa(this.getRequestId(request));
    this._requestMap[requestId] = window;

    cpmm.sendAsyncMessage("NFC:Connect", {
      requestId: requestId,
      sessionToken: sessionToken,
      techType: techType
    });
    return request;
  },

  close: function close(window, sessionToken) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = btoa(this.getRequestId(request));
    this._requestMap[requestId] = window;

    cpmm.sendAsyncMessage("NFC:Close", {
      requestId: requestId,
      sessionToken: sessionToken
    });
    return request;
  },

  sendFile: function sendFile(window, data, sessionToken) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = btoa(this.getRequestId(request));
    this._requestMap[requestId] = window;

    cpmm.sendAsyncMessage("NFC:SendFile", {
      requestId: requestId,
      sessionToken: sessionToken,
      blob: data.blob
    });
    return request;
  },

  notifySendFileStatus: function notifySendFileStatus(window, status,
                                                      requestId) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    cpmm.sendAsyncMessage("NFC:NotifySendFileStatus", {
      status: status,
      requestId: requestId
    });
  },

  registerPeerEventListener: function registerPeerEventListener(listener) {
    this.peerEventListener = listener;
  },

  registerTargetForPeerReady: function registerTargetForPeerReady(window, appId) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    cpmm.sendAsyncMessage("NFC:RegisterPeerReadyTarget", { appId: appId });
  },

  unregisterTargetForPeerReady: function unregisterTargetForPeerReady(window, appId) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    cpmm.sendAsyncMessage("NFC:UnregisterPeerReadyTarget", { appId: appId });
  },

  checkP2PRegistration: function checkP2PRegistration(window, appId) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = btoa(this.getRequestId(request));
    this._requestMap[requestId] = window;

    cpmm.sendAsyncMessage("NFC:CheckP2PRegistration", {
      appId: appId,
      requestId: requestId
    });
    return request;
  },

  notifyUserAcceptedP2P: function notifyUserAcceptedP2P(window, appId) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    cpmm.sendAsyncMessage("NFC:NotifyUserAcceptedP2P", {
      appId: appId
    });
  },

  startPoll: function startPoll(window) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = btoa(this.getRequestId(request));
    this._requestMap[requestId] = window;

    cpmm.sendAsyncMessage("NFC:StartPoll",
                          {requestId: requestId});
    return request;
  },

  stopPoll: function stopPoll(window) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = btoa(this.getRequestId(request));
    this._requestMap[requestId] = window;

    cpmm.sendAsyncMessage("NFC:StopPoll",
                          {requestId: requestId});
    return request;
  },

  powerOff: function powerOff(window) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = btoa(this.getRequestId(request));
    this._requestMap[requestId] = window;

    cpmm.sendAsyncMessage("NFC:PowerOff",
                          {requestId: requestId});
    return request;
  },

  // nsIObserver
  observe: function observe(subject, topic, data) {
    if (topic == "xpcom-shutdown") {
      this.destroyDOMRequestHelper();
      Services.obs.removeObserver(this, "xpcom-shutdown");
      cpmm = null;
    }
  },

  // nsIMessageListener

  fireRequestSuccess: function fireRequestSuccess(requestId, result) {
    let request = this.takeRequest(requestId);
    if (!request) {
      debug("not firing success for id: " + requestId +
            ", result: " + JSON.stringify(result));
      return;
    }

    debug("fire request success, id: " + requestId +
          ", result: " + JSON.stringify(result));
    Services.DOMRequest.fireSuccess(request, result);
  },

  fireRequestError: function fireRequestError(requestId, errorMsg) {
    let request = this.takeRequest(requestId);
    if (!request) {
      debug("not firing error for id: " + requestId +
            ", errormsg: " + errorMsg);
      return;
    }

    debug("fire request error, id: " + requestId +
          ", errormsg: " + errorMsg);
    Services.DOMRequest.fireError(request, errorMsg);
  },

  receiveMessage: function receiveMessage(message) {
    debug("Message received: " + JSON.stringify(message));
    let result = message.json;

    switch (message.name) {
      case "NFC:ReadNDEFResponse":
        this.handleReadNDEFResponse(result);
        break;
      case "NFC:GetDetailsNDEFResponse":
        this.handleGetDetailsNDEFResponse(result);
        break;
      case "NFC:CheckP2PRegistrationResponse":
        this.handleCheckP2PRegistrationResponse(result);
        break;
      case "NFC:ConnectResponse": // Fall through.
      case "NFC:CloseResponse":
      case "NFC:WriteNDEFResponse":
      case "NFC:MakeReadOnlyNDEFResponse":
      case "NFC:NotifySendFileStatusResponse":
      case "NFC:ConfigResponse":
        if (result.errorMsg) {
          this.fireRequestError(atob(result.requestId), result.errorMsg);
        } else {
          this.fireRequestSuccess(atob(result.requestId), result);
        }
        break;
      case "NFC:PeerEvent":
        switch (result.event) {
          case NFC.NFC_PEER_EVENT_READY:
            this.peerEventListener.notifyPeerReady(result.sessionToken);
            break;
          case NFC.NFC_PEER_EVENT_LOST:
            this.peerEventListener.notifyPeerLost(result.sessionToken);
            break;
        }
        break;
    }
  },

  handleReadNDEFResponse: function handleReadNDEFResponse(result) {
    let requester = this._requestMap[result.requestId];
    if (!requester) {
      debug("Response Invalid requestId=" + result.requestId);
      return;
    }
    delete this._requestMap[result.requestId];

    if (result.errorMsg) {
      this.fireRequestError(atob(result.requestId), result.errorMsg);
      return;
    }

    let requestId = atob(result.requestId);
    let ndefMsg = [];
    let records = result.records;
    for (let i = 0; i < records.length; i++) {
      let record = records[i];
      ndefMsg.push(new requester.MozNDEFRecord(record.tnf,
                                               record.type,
                                               record.id,
                                               record.payload));
    }
    this.fireRequestSuccess(requestId, ndefMsg);
  },

  handleGetDetailsNDEFResponse: function handleGetDetailsNDEFResponse(result) {
    if (result.errorMsg) {
      this.fireRequestError(atob(result.requestId), result.errorMsg);
      return;
    }

    let requestId = atob(result.requestId);
    this.fireRequestSuccess(requestId, new GetDetailsNDEFResponse(result));
  },

  handleCheckP2PRegistrationResponse: function handleCheckP2PRegistrationResponse(result) {
    // Privilaged status API. Always fire success to avoid using exposed props.
    // The receiver must check the boolean mapped status code to handle.
    let requestId = atob(result.requestId);
    this.fireRequestSuccess(requestId, !result.errorMsg);
  },
};

if (NFC_ENABLED) {
  this.NSGetFactory = XPCOMUtils.generateNSGetFactory([NfcContentHelper]);
}
