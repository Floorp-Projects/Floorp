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

let NFC = {};
Cu.import("resource://gre/modules/nfc_consts.js", NFC);

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
  "NFC:PeerEvent"
];

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsISyncMessageSender");

function NfcContentHelper() {
  this.initDOMRequestHelper(/* aWindow */ null, NFC_IPC_MSG_NAMES);
  Services.obs.addObserver(this, "xpcom-shutdown", false);

  this._requestMap = [];

  // Maintains an array of PeerEvent related callbacks, mainly
  // one for 'peerReady' and another for 'peerLost'.
  this.peerEventsCallbackMap = {};
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
  peerEventsCallbackMap: null,

  /* TODO: Bug 815526: This is a limitation when a DOMString is used in sequences of Moz DOM Objects.
   *       Strings such as 'type', 'id' 'payload' will not be acccessible to NfcWorker.
   *       Therefore this function exists till the bug is addressed.
   */
  encodeNdefRecords: function encodeNdefRecords(records) {
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
      return;
    }
    // Report session to Nfc.js only.
    cpmm.sendAsyncMessage("NFC:SetSessionToken", {
      sessionToken: sessionToken,
    });
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

    let encodedRecords = this.encodeNdefRecords(records);
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

  registerTargetForPeerEvent: function registerTargetForPeerEvent(window,
                                                  appId, event, callback) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    this.peerEventsCallbackMap[event] = callback;
    cpmm.sendAsyncMessage("NFC:RegisterPeerTarget", {
      appId: appId,
      event: event
    });
  },

  unregisterTargetForPeerEvent: function unregisterTargetForPeerEvent(window,
                                                                appId, event) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let callback = this.peerEventsCallbackMap[event];
    if (callback != null) {
      delete this.peerEventsCallbackMap[event];
    }

    cpmm.sendAsyncMessage("NFC:UnregisterPeerTarget", {
      appId: appId,
      event: event
    });
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

  // nsIObserver
  observe: function observe(subject, topic, data) {
    if (topic == "xpcom-shutdown") {
      this.removeMessageListener();
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

  fireRequestError: function fireRequestError(requestId, error) {
    let request = this.takeRequest(requestId);
    if (!request) {
      debug("not firing error for id: " + requestId +
            ", error: " + JSON.stringify(error));
      return;
    }

    debug("fire request error, id: " + requestId +
          ", result: " + JSON.stringify(error));
    Services.DOMRequest.fireError(request, error);
  },

  receiveMessage: function receiveMessage(message) {
    debug("Message received: " + JSON.stringify(message));
    switch (message.name) {
      case "NFC:ReadNDEFResponse":
        this.handleReadNDEFResponse(message.json);
        break;
      case "NFC:ConnectResponse": // Fall through.
      case "NFC:CloseResponse":
      case "NFC:WriteNDEFResponse":
      case "NFC:MakeReadOnlyNDEFResponse":
      case "NFC:GetDetailsNDEFResponse":
      case "NFC:CheckP2PRegistrationResponse":
        this.handleResponse(message.json);
        break;
      case "NFC:PeerEvent":
        let callback = this.peerEventsCallbackMap[message.json.event];
        if (callback) {
          callback.peerNotification(message.json.event,
                                    message.json.sessionToken);
        } else {
          debug("PeerEvent: No valid callback registered for the event " +
                message.json.event);
        }
        break;
    }
  },

  handleReadNDEFResponse: function handleReadNDEFResponse(message) {
    debug("ReadNDEFResponse(" + JSON.stringify(message) + ")");
    let requester = this._requestMap[message.requestId];
    if (!requester) {
       debug("ReadNDEFResponse Invalid requester=" + requester +
             " message.sessionToken=" + message.sessionToken);
       return; // Nothing to do in this instance.
    }
    delete this._requestMap[message.requestId];
    let records = message.records;
    let requestId = atob(message.requestId);

    if (message.status !== NFC.GECKO_NFC_ERROR_SUCCESS) {
      this.fireRequestError(requestId, message.status);
    } else {
      let ndefMsg = [];
      for (let i = 0; i < records.length; i++) {
        let record = records[i];
        ndefMsg.push(new requester.MozNdefRecord(record.tnf,
                                                 record.type,
                                                 record.id,
                                                 record.payload));
      }
      this.fireRequestSuccess(requestId, ndefMsg);
    }
  },

  handleResponse: function handleResponse(message) {
    debug("Response(" + JSON.stringify(message) + ")");
    let requester = this._requestMap[message.requestId];
    if (!requester) {
       debug("Response Invalid requester=" + requester +
             " message.sessionToken=" + message.sessionToken);
       return; // Nothing to do in this instance.
    }
    delete this._requestMap[message.requestId];
    let result = message;
    let requestId = atob(message.requestId);

    if (message.status !== NFC.GECKO_NFC_ERROR_SUCCESS) {
      this.fireRequestError(requestId, result.status);
    } else {
      this.fireRequestSuccess(requestId, result);
    }
  },
};

if (NFC_ENABLED) {
  this.NSGetFactory = XPCOMUtils.generateNSGetFactory([NfcContentHelper]);
}
