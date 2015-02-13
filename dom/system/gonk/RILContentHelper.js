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

Cu.import("resource://gre/modules/DOMRequestHelper.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "RIL", function () {
  let obj = {};
  Cu.import("resource://gre/modules/ril_consts.js", obj);
  return obj;
});

const NS_XPCOM_SHUTDOWN_OBSERVER_ID = "xpcom-shutdown";

const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID = "nsPref:changed";

const kPrefRilNumRadioInterfaces = "ril.numRadioInterfaces";
const kPrefRilDebuggingEnabled = "ril.debugging.enabled";

let DEBUG;
function debug(s) {
  dump("-*- RILContentHelper: " + s + "\n");
}

const RILCONTENTHELPER_CID =
  Components.ID("{472816e1-1fd6-4405-996c-806f9ea68174}");

const RIL_IPC_MSG_NAMES = [
  "RIL:CardStateChanged",
  "RIL:IccInfoChanged",
  "RIL:GetCardLockResult",
  "RIL:SetUnlockCardLockResult",
  "RIL:CardLockRetryCount",
  "RIL:StkCommand",
  "RIL:StkSessionEnd",
  "RIL:IccOpenChannel",
  "RIL:IccCloseChannel",
  "RIL:IccExchangeAPDU",
  "RIL:ReadIccContacts",
  "RIL:UpdateIccContact",
  "RIL:MatchMvno",
  "RIL:GetServiceState"
];

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsISyncMessageSender");

XPCOMUtils.defineLazyServiceGetter(this, "UUIDGenerator",
                  "@mozilla.org/uuid-generator;1",
                  "nsIUUIDGenerator");

XPCOMUtils.defineLazyGetter(this, "gNumRadioInterfaces", function() {
  let appInfo = Cc["@mozilla.org/xre/app-info;1"];
  let isParentProcess = !appInfo || appInfo.getService(Ci.nsIXULRuntime)
                          .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;

  if (isParentProcess) {
    let ril = Cc["@mozilla.org/ril;1"].getService(Ci.nsIRadioInterfaceLayer);
    return ril.numRadioInterfaces;
  }

  return Services.prefs.getIntPref(kPrefRilNumRadioInterfaces);
});

function IccInfo() {}
IccInfo.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIIccInfo]),

  // nsIIccInfo

  iccType: null,
  iccid: null,
  mcc: null,
  mnc: null,
  spn: null,
  isDisplayNetworkNameRequired: null,
  isDisplaySpnRequired: null
};

function GsmIccInfo() {}
GsmIccInfo.prototype = {
  __proto__: IccInfo.prototype,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIGsmIccInfo,
                                         Ci.nsIIccInfo]),

  // nsIGsmIccInfo

  msisdn: null
};

function CdmaIccInfo() {}
CdmaIccInfo.prototype = {
  __proto__: IccInfo.prototype,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICdmaIccInfo,
                                         Ci.nsIIccInfo]),

  // nsICdmaIccInfo

  mdn: null,
  prlVersion: 0
};

function RILContentHelper() {
  this.updateDebugFlag();

  this.numClients = gNumRadioInterfaces;
  if (DEBUG) debug("Number of clients: " + this.numClients);

  this.rilContexts = [];
  for (let clientId = 0; clientId < this.numClients; clientId++) {
    this.rilContexts[clientId] = {
      cardState: Ci.nsIIccProvider.CARD_STATE_UNKNOWN,
      iccInfo: null
    };
  }

  this.initDOMRequestHelper(/* aWindow */ null, RIL_IPC_MSG_NAMES);
  this._windowsMap = [];
  this._iccListeners = [];
  this._iccChannelCallback = [];

  Services.obs.addObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);

  Services.prefs.addObserver(kPrefRilDebuggingEnabled, this, false);
}

RILContentHelper.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIIccProvider,
                                         Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),
  classID:   RILCONTENTHELPER_CID,
  classInfo: XPCOMUtils.generateCI({classID: RILCONTENTHELPER_CID,
                                    classDescription: "RILContentHelper",
                                    interfaces: [Ci.nsIIccProvider]}),

  updateDebugFlag: function() {
    try {
      DEBUG = RIL.DEBUG_CONTENT_HELPER ||
              Services.prefs.getBoolPref(kPrefRilDebuggingEnabled);
    } catch (e) {}
  },

  // An utility function to copy objects.
  updateInfo: function(srcInfo, destInfo) {
    for (let key in srcInfo) {
      destInfo[key] = srcInfo[key];
    }
  },

  /**
   * We need to consider below cases when update iccInfo:
   * 1. Should clear iccInfo to null if there is no card detected.
   * 2. Need to create corresponding object based on iccType.
   */
  updateIccInfo: function(clientId, newInfo) {
    let rilContext = this.rilContexts[clientId];

    // Card is not detected, clear iccInfo to null.
    if (!newInfo || !newInfo.iccid) {
      if (rilContext.iccInfo) {
        rilContext.iccInfo = null;
      }
      return;
    }

    // If iccInfo is null, new corresponding object based on iccType.
    if (!rilContext.iccInfo) {
      if (newInfo.iccType === "ruim" || newInfo.iccType === "csim") {
        rilContext.iccInfo = new CdmaIccInfo();
      } else if (newInfo.iccType === "sim" || newInfo.iccType === "usim") {
        rilContext.iccInfo = new GsmIccInfo();
      } else {
        rilContext.iccInfo = new IccInfo();
      }
    }

    this.updateInfo(newInfo, rilContext.iccInfo);
  },

  _windowsMap: null,

  rilContexts: null,

  getRilContext: function(clientId) {
    // Update ril contexts by sending IPC message to chrome only when the first
    // time we require it. The information will be updated by following info
    // changed messages.
    this.getRilContext = function getRilContext(clientId) {
      return this.rilContexts[clientId];
    };

    for (let cId = 0; cId < this.numClients; cId++) {
      let rilContext =
        cpmm.sendSyncMessage("RIL:GetRilContext", {clientId: cId})[0];
      if (!rilContext) {
        if (DEBUG) debug("Received null rilContext from chrome process.");
        continue;
      }
      this.rilContexts[cId].cardState = rilContext.cardState;
      this.updateIccInfo(cId, rilContext.iccInfo);
    }

    return this.rilContexts[clientId];
  },

  /**
   * nsIIccProvider
   */

  getIccInfo: function(clientId) {
    let context = this.getRilContext(clientId);
    return context && context.iccInfo;
  },

  getCardState: function(clientId) {
    let context = this.getRilContext(clientId);
    return context && context.cardState;
  },

  matchMvno: function(clientId, window, mvnoType, mvnoData) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);

    cpmm.sendAsyncMessage("RIL:MatchMvno", {
      clientId: clientId,
      data: {
        requestId: requestId,
        mvnoType: mvnoType,
        mvnoData: mvnoData
      }
    });
    return request;
  },

  getCardLockEnabled: function(clientId, window, lockType) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);
    this._windowsMap[requestId] = window;

    cpmm.sendAsyncMessage("RIL:GetCardLockEnabled", {
      clientId: clientId,
      data: {
        lockType: lockType,
        requestId: requestId
      }
    });
    return request;
  },

  unlockCardLock: function(clientId, window, lockType, password, newPin) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);
    this._windowsMap[requestId] = window;

    cpmm.sendAsyncMessage("RIL:UnlockCardLock", {
      clientId: clientId,
      data: {
        lockType: lockType,
        password: password,
        newPin: newPin,
        requestId: requestId
      }
    });
    return request;
  },

  setCardLockEnabled: function(clientId, window, lockType, password, enabled) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);
    this._windowsMap[requestId] = window;

    cpmm.sendAsyncMessage("RIL:SetCardLockEnabled", {
      clientId: clientId,
      data: {
        lockType: lockType,
        password: password,
        enabled: enabled,
        requestId: requestId
      }
    });
    return request;
  },

  changeCardLockPassword: function(clientId, window, lockType, password,
                                   newPassword) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);
    this._windowsMap[requestId] = window;

    cpmm.sendAsyncMessage("RIL:ChangeCardLockPassword", {
      clientId: clientId,
      data: {
        lockType: lockType,
        password: password,
        newPassword: newPassword,
        requestId: requestId
      }
    });
    return request;
  },

  getCardLockRetryCount: function(clientId, window, lockType) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);
    this._windowsMap[requestId] = window;

    cpmm.sendAsyncMessage("RIL:GetCardLockRetryCount", {
      clientId: clientId,
      data: {
        lockType: lockType,
        requestId: requestId
      }
    });
    return request;
  },

  sendStkResponse: function(clientId, window, command, response) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    response.command = command;
    cpmm.sendAsyncMessage("RIL:SendStkResponse", {
      clientId: clientId,
      data: response
    });
  },

  sendStkMenuSelection: function(clientId, window, itemIdentifier,
                                 helpRequested) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    cpmm.sendAsyncMessage("RIL:SendStkMenuSelection", {
      clientId: clientId,
      data: {
        itemIdentifier: itemIdentifier,
        helpRequested: helpRequested
      }
    });
  },

  sendStkTimerExpiration: function(clientId, window, timer) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    cpmm.sendAsyncMessage("RIL:SendStkTimerExpiration", {
      clientId: clientId,
      data: {
        timer: timer
      }
    });
  },

  sendStkEventDownload: function(clientId, window, event) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }
    cpmm.sendAsyncMessage("RIL:SendStkEventDownload", {
      clientId: clientId,
      data: {
        event: event
      }
    });
  },

  iccOpenChannel: function(clientId, aid, callback) {
    let requestId = UUIDGenerator.generateUUID().toString();
    this._addIccChannelCallback(requestId, callback);

    cpmm.sendAsyncMessage("RIL:IccOpenChannel", {
      clientId: clientId,
      data: {
        requestId: requestId,
        aid: aid
      }
    });
  },

  iccExchangeAPDU: function(clientId, channel, cla, ins, p1, p2, p3, data, callback) {
    let requestId = UUIDGenerator.generateUUID().toString();
    this._addIccChannelCallback(requestId, callback);

    if (!data) {
      if (DEBUG) debug('data is not set , p3 : ' + p3);
    }

    let apdu = {
      cla: cla,
      command: ins,
      p1: p1,
      p2: p2,
      p3: p3,
      data: data
    };

    //Potentially you need serialization here and can't pass the jsval through
    cpmm.sendAsyncMessage("RIL:IccExchangeAPDU", {
      clientId: clientId,
      data: {
        requestId: requestId,
        channel: channel,
        apdu: apdu
      }
    });
  },

  iccCloseChannel: function(clientId, channel, callback) {
    let requestId = UUIDGenerator.generateUUID().toString();
    this._addIccChannelCallback(requestId, callback);

    cpmm.sendAsyncMessage("RIL:IccCloseChannel", {
      clientId: clientId,
      data: {
        requestId: requestId,
        channel: channel
      }
    });
  },

  readContacts: function(clientId, window, contactType) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);
    this._windowsMap[requestId] = window;

    cpmm.sendAsyncMessage("RIL:ReadIccContacts", {
      clientId: clientId,
      data: {
        requestId: requestId,
        contactType: contactType
      }
    });
    return request;
  },

  updateContact: function(clientId, window, contactType, contact, pin2) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    let request = Services.DOMRequest.createRequest(window);
    let requestId = this.getRequestId(request);
    this._windowsMap[requestId] = window;

    // Parsing nsDOMContact to Icc Contact format
    let iccContact = {};

    if (Array.isArray(contact.name) && contact.name[0]) {
      iccContact.alphaId = contact.name[0];
    }

    if (Array.isArray(contact.tel)) {
      iccContact.number = contact.tel[0] && contact.tel[0].value;
      let telArray = contact.tel.slice(1);
      let length = telArray.length;
      if (length > 0) {
        iccContact.anr = [];
      }
      for (let i = 0; i < telArray.length; i++) {
        iccContact.anr.push(telArray[i].value);
      }
    }

    if (Array.isArray(contact.email) && contact.email[0]) {
      iccContact.email = contact.email[0].value;
    }

    iccContact.contactId = contact.id;

    cpmm.sendAsyncMessage("RIL:UpdateIccContact", {
      clientId: clientId,
      data: {
        requestId: requestId,
        contactType: contactType,
        contact: iccContact,
        pin2: pin2
      }
    });

    return request;
  },

  getServiceState: function(clientId, window, service) {
    if (window == null) {
      throw Components.Exception("Can't get window object",
                                  Cr.NS_ERROR_UNEXPECTED);
    }

    return new window.Promise((resolve, reject) => {
      let requestId =
        this.getPromiseResolverId({resolve: resolve, reject: reject});
      this._windowsMap[requestId] = window;

      cpmm.sendAsyncMessage("RIL:GetServiceState", {
        clientId: clientId,
        data: {
          requestId: requestId,
          service: service
        }
      });
    });
  },

  _iccListeners: null,

  registerListener: function(listenerType, clientId, listener) {
    if (!this[listenerType]) {
      return;
    }
    let listeners = this[listenerType][clientId];
    if (!listeners) {
      listeners = this[listenerType][clientId] = [];
    }

    if (listeners.indexOf(listener) != -1) {
      throw new Error("Already registered this listener!");
    }

    listeners.push(listener);
    if (DEBUG) debug("Registered " + listenerType + " listener: " + listener);
  },

  unregisterListener: function(listenerType, clientId, listener) {
    if (!this[listenerType]) {
      return;
    }
    let listeners = this[listenerType][clientId];
    if (!listeners) {
      return;
    }

    let index = listeners.indexOf(listener);
    if (index != -1) {
      listeners.splice(index, 1);
      if (DEBUG) debug("Unregistered listener: " + listener);
    }
  },

  _iccChannelCallback: null,

  _addIccChannelCallback: function(requestId, channelCb) {
    let cbInterfaces = this._iccChannelCallback;
    if (!cbInterfaces[requestId] && channelCb) {
      cbInterfaces[requestId] = channelCb;
      return;
    }

    if (DEBUG) debug("Unable to add channelCbInterface for requestId : " + requestId);
  },

  _getIccChannelCallback: function(requestId) {
    let cb = this._iccChannelCallback[requestId];
    delete this._iccChannelCallback[requestId];
    return cb;
  },

  registerIccMsg: function(clientId, listener) {
    if (DEBUG) debug("Registering for ICC related messages");
    this.registerListener("_iccListeners", clientId, listener);
    cpmm.sendAsyncMessage("RIL:RegisterIccMsg");
  },

  unregisterIccMsg: function(clientId, listener) {
    this.unregisterListener("_iccListeners", clientId, listener);
  },

  // nsIObserver

  observe: function(subject, topic, data) {
    switch (topic) {
      case NS_PREFBRANCH_PREFCHANGE_TOPIC_ID:
        if (data == kPrefRilDebuggingEnabled) {
          this.updateDebugFlag();
        }
        break;

      case NS_XPCOM_SHUTDOWN_OBSERVER_ID:
        this.destroyDOMRequestHelper();
        Services.obs.removeObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
        break;
    }
  },

  // nsIMessageListener

  fireRequestSuccess: function(requestId, result) {
    let request = this.takeRequest(requestId);
    if (!request) {
      if (DEBUG) {
        debug("not firing success for id: " + requestId +
              ", result: " + JSON.stringify(result));
      }
      return;
    }

    if (DEBUG) {
      debug("fire request success, id: " + requestId +
            ", result: " + JSON.stringify(result));
    }
    Services.DOMRequest.fireSuccess(request, result);
  },

  dispatchFireRequestSuccess: function(requestId, result) {
    let currentThread = Services.tm.currentThread;

    currentThread.dispatch(this.fireRequestSuccess.bind(this, requestId, result),
                           Ci.nsIThread.DISPATCH_NORMAL);
  },

  fireRequestError: function(requestId, error) {
    let request = this.takeRequest(requestId);
    if (!request) {
      if (DEBUG) {
        debug("not firing error for id: " + requestId +
              ", error: " + JSON.stringify(error));
      }
      return;
    }

    if (DEBUG) {
      debug("fire request error, id: " + requestId +
            ", result: " + JSON.stringify(error));
    }
    Services.DOMRequest.fireError(request, error);
  },

  dispatchFireRequestError: function(requestId, error) {
    let currentThread = Services.tm.currentThread;

    currentThread.dispatch(this.fireRequestError.bind(this, requestId, error),
                           Ci.nsIThread.DISPATCH_NORMAL);
  },

  fireRequestDetailedError: function(requestId, detailedError) {
    let request = this.takeRequest(requestId);
    if (!request) {
      if (DEBUG) {
        debug("not firing detailed error for id: " + requestId +
              ", detailedError: " + JSON.stringify(detailedError));
      }
      return;
    }

    Services.DOMRequest.fireDetailedError(request, detailedError);
  },

  receiveMessage: function(msg) {
    let request;
    if (DEBUG) {
      debug("Received message '" + msg.name + "': " + JSON.stringify(msg.json));
    }

    let data = msg.json.data;
    let clientId = msg.json.clientId;
    switch (msg.name) {
      case "RIL:CardStateChanged":
        if (this.rilContexts[clientId].cardState != data.cardState) {
          this.rilContexts[clientId].cardState = data.cardState;
          this._deliverEvent(clientId,
                             "_iccListeners",
                             "notifyCardStateChanged",
                             null);
        }
        break;
      case "RIL:IccInfoChanged":
        this.updateIccInfo(clientId, data);
        this._deliverEvent(clientId,
                           "_iccListeners",
                           "notifyIccInfoChanged",
                           null);
        break;
      case "RIL:GetCardLockResult": {
        let requestId = data.requestId;
        let requestWindow = this._windowsMap[requestId];
        delete this._windowsMap[requestId];

        if (data.errorMsg) {
          this.fireRequestError(requestId, data.errorMsg);
          break;
        }

        this.fireRequestSuccess(requestId,
                                Cu.cloneInto({ enabled: data.enabled },
                                             requestWindow));
        break;
      }
      case "RIL:SetUnlockCardLockResult": {
        let requestId = data.requestId;
        let requestWindow = this._windowsMap[requestId];
        delete this._windowsMap[requestId];

        if (data.errorMsg) {
          let cardLockError = new requestWindow.IccCardLockError(data.errorMsg,
                                                                 data.retryCount);
          this.fireRequestDetailedError(requestId, cardLockError);
          break;
        }

        this.fireRequestSuccess(requestId, null);
        break;
      }
      case "RIL:CardLockRetryCount": {
        let requestId = data.requestId;
        let requestWindow = this._windowsMap[requestId];
        delete this._windowsMap[requestId];

        if (data.errorMsg) {
          this.fireRequestError(data.requestId, data.errorMsg);
          break;
        }

        this.fireRequestSuccess(data.requestId,
                                Cu.cloneInto({ retryCount: data.retryCount },
                                             requestWindow));
        break;
      }
      case "RIL:StkCommand":
        this._deliverEvent(clientId, "_iccListeners", "notifyStkCommand",
                           [JSON.stringify(data)]);
        break;
      case "RIL:StkSessionEnd":
        this._deliverEvent(clientId, "_iccListeners", "notifyStkSessionEnd", null);
        break;
      case "RIL:IccOpenChannel":
        this.handleIccOpenChannel(data);
        break;
      case "RIL:IccCloseChannel":
        this.handleIccCloseChannel(data);
        break;
      case "RIL:IccExchangeAPDU":
        this.handleIccExchangeAPDU(data);
        break;
      case "RIL:ReadIccContacts":
        this.handleReadIccContacts(data);
        break;
      case "RIL:UpdateIccContact":
        this.handleUpdateIccContact(data);
        break;
      case "RIL:MatchMvno":
        this.handleSimpleRequest(data.requestId, data.errorMsg, data.result);
        break;
      case "RIL:GetServiceState":
        this.handleGetServiceState(data);
        break;
    }
  },

  handleSimpleRequest: function(requestId, errorMsg, result) {
    if (errorMsg) {
      this.fireRequestError(requestId, errorMsg);
    } else {
      this.fireRequestSuccess(requestId, result);
    }
  },

  handleIccOpenChannel: function(message) {
    let requestId = message.requestId;
    let callback = this._getIccChannelCallback(requestId);
    if (!callback) {
      return;
    }

    return !message.errorMsg ? callback.notifyOpenChannelSuccess(message.channel) :
                               callback.notifyError(message.errorMsg);
  },

  handleIccCloseChannel: function(message) {
    let requestId = message.requestId;
    let callback = this._getIccChannelCallback(requestId);
    if (!callback) {
      return;
    }

    return !message.errorMsg ? callback.notifyCloseChannelSuccess() :
                               callback.notifyError(message.errorMsg);
  },

  handleIccExchangeAPDU: function(message) {
    let requestId = message.requestId;
    let callback = this._getIccChannelCallback(requestId);
    if (!callback) {
      return;
    }

    return !message.errorMsg ?
           callback.notifyExchangeAPDUResponse(message.sw1, message.sw2, message.simResponse) :
           callback.notifyError(message.errorMsg);
  },

  handleReadIccContacts: function(message) {
    if (message.errorMsg) {
      this.fireRequestError(message.requestId, message.errorMsg);
      return;
    }

    let window = this._windowsMap[message.requestId];
    delete this._windowsMap[message.requestId];
    let contacts = message.contacts;
    let result = new window.Array();
    contacts.forEach(function(c) {
      let prop = {name: [c.alphaId], tel: [{value: c.number}]};

      if (c.email) {
        prop.email = [{value: c.email}];
      }

      // ANR - Additional Number
      let anrLen = c.anr ? c.anr.length : 0;
      for (let i = 0; i < anrLen; i++) {
        prop.tel.push({value: c.anr[i]});
      }

      let contact = new window.mozContact(prop);
      contact.id = c.contactId;
      result.push(contact);
    });

    this.fireRequestSuccess(message.requestId, result);
  },

  handleUpdateIccContact: function(message) {
    if (message.errorMsg) {
      this.fireRequestError(message.requestId, message.errorMsg);
      return;
    }

    let window = this._windowsMap[message.requestId];
    delete this._windowsMap[message.requestId];
    let iccContact = message.contact;
    let prop = {name: [iccContact.alphaId], tel: [{value: iccContact.number}]};
    if (iccContact.email) {
      prop.email = [{value: iccContact.email}];
    }

    // ANR - Additional Number
    let anrLen = iccContact.anr ? iccContact.anr.length : 0;
    for (let i = 0; i < anrLen; i++) {
      prop.tel.push({value: iccContact.anr[i]});
    }

    let contact = new window.mozContact(prop);
    contact.id = iccContact.contactId;

    this.fireRequestSuccess(message.requestId, contact);
  },

  handleGetServiceState: function(message) {
    let requestId = message.requestId;
    let requestWindow = this._windowsMap[requestId];
    delete this._windowsMap[requestId];

    let resolver = this.takePromiseResolver(requestId);
    if (message.errorMsg) {
      resolver.reject(new requestWindow.DOMError(message.errorMsg));
      return;
    }

    resolver.resolve(message.result);
  },

  _deliverEvent: function(clientId, listenerType, name, args) {
    if (!this[listenerType]) {
      return;
    }
    let thisListeners = this[listenerType][clientId];
    if (!thisListeners) {
      return;
    }

    let listeners = thisListeners.slice();
    for (let listener of listeners) {
      if (thisListeners.indexOf(listener) == -1) {
        continue;
      }
      let handler = listener[name];
      if (typeof handler != "function") {
        throw new Error("No handler for " + name);
      }
      try {
        handler.apply(listener, args);
      } catch (e) {
        if (DEBUG) debug("listener for " + name + " threw an exception: " + e);
      }
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([RILContentHelper]);
