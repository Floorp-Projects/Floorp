/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DEBUG = false;
function debug(s) { dump("-*- SettingsManager: " + s + "\n"); }

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

XPCOMUtils.defineLazyServiceGetter(Services, "DOMRequest",
                                   "@mozilla.org/dom/dom-request-service;1",
                                   "nsIDOMRequestService");
XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");
XPCOMUtils.defineLazyServiceGetter(this, "mrm",
                                   "@mozilla.org/memory-reporter-manager;1",
                                   "nsIMemoryReporterManager");
XPCOMUtils.defineLazyServiceGetter(this, "uuidgen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

/**
 * In order to make SettingsManager work with Privileged Apps, we need the lock
 * to be OOP. However, the lock state needs to be managed on the child process,
 * while the IDB functions now happen on the parent process so we don't have to
 * expose IDB permissions at the child process level. We use the
 * DOMRequestHelper mechanism to deal with DOMRequests/promises across the
 * processes.
 *
 * However, due to the nature of the IDBTransaction lifetime, we need to relay
 * to the parent when to finalize the transaction once the child is done with the
 * lock. We keep a list of all open requests for a lock, and once the lock
 * reaches the end of its receiveMessage function with no more queued requests,
 * we consider it dead. At that point, we send a message to the parent to notify
 * it to finalize the transaction.
 */

function SettingsLock(aSettingsManager) {
  if (DEBUG) debug("settings lock init");
  this._open = true;
  this._settingsManager = aSettingsManager;
  this._id = uuidgen.generateUUID().toString();

  let closeHelper = function() {
    if (DEBUG) debug("closing lock " + this._id);
    this._open = false;
    this.runOrFinalizeQueries();
  }.bind(this);

  // DOMRequestIpcHelper.initHelper sets this._window
  this.initDOMRequestHelper(this._settingsManager._window, ["Settings:Get:OK", "Settings:Get:KO",
                                                            "Settings:Clear:OK", "Settings:Clear:KO",
                                                            "Settings:Set:OK", "Settings:Set:KO",
                                                            "Settings:Finalize:OK", "Settings:Finalize:KO"]);
  this.sendMessage("Settings:CreateLock", {lockID: this._id, isInternalLock: false});
  Services.tm.currentThread.dispatch(closeHelper, Ci.nsIThread.DISPATCH_NORMAL);
}

SettingsLock.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,
  set onsettingstransactionsuccess(aHandler) {
    this.__DOM_IMPL__.setEventHandler("onsettingstransactionsuccess", aHandler);
  },

  get onsettingstransactionsuccess() {
    return this.__DOM_IMPL__.getEventHandler("onsettingstransactionsuccess");
  },

  set onsettingstransactionfailure(aHandler) {
    this.__DOM_IMPL__.setEventHandler("onsettingstransactionfailure", aHandler);
  },

  get onsettingstransactionfailure() {
    return this.__DOM_IMPL__.getEventHandler("onsettingstransactionfailure");
  },

  get closed() {
    return !this._open;
  },

  _wrap: function _wrap(obj) {
    return Cu.cloneInto(obj, this._settingsManager._window);
  },

  sendMessage: function(aMessageName, aData) {
    cpmm.sendAsyncMessage(aMessageName,
                          aData,
                          undefined,
                          this._settingsManager._window.document.nodePrincipal);
  },

  runOrFinalizeQueries: function() {
    if (!this._requests || Object.keys(this._requests).length == 0) {
      this.sendMessage("Settings:Finalize", {lockID: this._id});
              } else {
      this.sendMessage("Settings:Run", {lockID: this._id});
    }
  },

  receiveMessage: function(aMessage) {
    let msg = aMessage.data;
    // SettingsRequestManager broadcasts changes to all locks in the child. If
    // our lock isn't being addressed, just return.
    if (msg.lockID != this._id) {
      return;
    }

    // Finalizing a transaction does not return a request ID since we are
    // supposed to fire callbacks.
    if (!msg.requestID) {
      let event;
      switch (aMessage.name) {
        case "Settings:Finalize:OK":
          if (DEBUG) debug("Lock finalize ok!");
          event = new this._window.MozSettingsTransactionEvent("settingstransactionsuccess", {});
          this.__DOM_IMPL__.dispatchEvent(event);
          break;
        case "Settings:Finalize:KO":
          if (DEBUG) debug("Lock finalize failed!");
          event = new this._window.MozSettingsTransactionEvent("settingstransactionfailure", {
            error: msg.errorMsg
          });
          this.__DOM_IMPL__.dispatchEvent(event);
          break;
        default:
          if (DEBUG) debug("Message type " + aMessage.name + " is missing a requestID");
      }
      return;
    }


    let req = this.getRequest(msg.requestID);
    if (!req) {
      if (DEBUG) debug("Matching request not found.");
      return;
    }
    this.removeRequest(msg.requestID);
    if (DEBUG) debug("receiveMessage: " + aMessage.name);
    switch (aMessage.name) {
      case "Settings:Get:OK":
        for (let i in msg.settings) {
          msg.settings[i] = this._wrap(msg.settings[i]);
        }
        this._open = true;
        Services.DOMRequest.fireSuccess(req.request, this._wrap(msg.settings));
        this._open = false;
        break;
      case "Settings:Set:OK":
      case "Settings:Clear:OK":
        this._open = true;
        Services.DOMRequest.fireSuccess(req.request, 0);
        this._open = false;
        break;
      case "Settings:Get:KO":
      case "Settings:Set:KO":
      case "Settings:Clear:KO":
        if (DEBUG) debug("error:" + msg.errorMsg);
        Services.DOMRequest.fireError(req.request, msg.errorMsg);
        break;
      default:
        if (DEBUG) debug("Wrong message: " + aMessage.name);
    }
    this.runOrFinalizeQueries();
  },

  get: function get(aName) {
    if (DEBUG) debug("get (" + this._id + "): " + aName);
    if (!this._open) {
      dump("Settings lock not open!\n");
      throw Components.results.NS_ERROR_ABORT;
    }
    let req = this.createRequest();
    let reqID = this.getRequestId({request: req});
    this.sendMessage("Settings:Get", {requestID: reqID,
                                      lockID: this._id,
                                      name: aName});
      return req;
  },

  set: function set(aSettings) {
    if (DEBUG) debug("send: " + JSON.stringify(aSettings));
    if (!this._open) {
      throw "Settings lock not open";
    }
    let req = this.createRequest();
    let reqID = this.getRequestId({request: req});
    this.sendMessage("Settings:Set", {requestID: reqID,
                                      lockID: this._id,
                                      settings: aSettings});
      return req;
  },

  clear: function clear() {
    if (DEBUG) if (DEBUG) debug("clear");
    if (!this._open) {
      throw "Settings lock not open";
    }
    let req = this.createRequest();
    let reqID = this.getRequestId({request: req});
    this.sendMessage("Settings:Clear", {requestID: reqID,
                                        lockID: this._id});
      return req;
  },

  classID: Components.ID("{60c9357c-3ae0-4222-8f55-da01428470d5}"),
  contractID: "@mozilla.org/settingsLock;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference])
};

function SettingsManager() {
}

SettingsManager.prototype = {
  _callbacks: null,
  _isRegistered: false,
  _perms: [],

  _wrap: function _wrap(obj) {
    return Cu.cloneInto(obj, this._window);
  },

  set onsettingchange(aHandler) {
    this.__DOM_IMPL__.setEventHandler("onsettingchange", aHandler);
    this.checkMessageRegistration();
  },

  get onsettingchange() {
    return this.__DOM_IMPL__.getEventHandler("onsettingchange");
  },

  createLock: function() {
    if (DEBUG) debug("creating lock");
    let lock = new SettingsLock(this);
    return lock;
  },

  receiveMessage: function(aMessage) {
    if (DEBUG) debug("Settings::receiveMessage: " + aMessage.name);
    let msg = aMessage.json;

    switch (aMessage.name) {
      case "Settings:Change:Return:OK":
        if (DEBUG) debug('data:' + msg.key + ':' + msg.value + '\n');

        let event = new this._window.MozSettingsEvent("settingchange", this._wrap({
          settingName: msg.key,
          settingValue: msg.value
        }));
        this.__DOM_IMPL__.dispatchEvent(event);

        if (this._callbacks && this._callbacks[msg.key]) {
          if (DEBUG) debug("observe callback called! " + msg.key + " " + this._callbacks[msg.key].length);
          this._callbacks[msg.key].forEach(function(cb) {
            cb(this._wrap({settingName: msg.key, settingValue: msg.value}));
          }.bind(this));
        } else {
          if (DEBUG) debug("no observers stored!");
        }
        break;
      default:
        if (DEBUG) debug("Wrong message: " + aMessage.name);
    }
  },

  // If we have either observer callbacks or an event handler,
  // register for messages from the main thread. Otherwise, if no one
  // is listening, unregister to reduce parent load.
  checkMessageRegistration: function checkRegistration() {
    let handler = this.__DOM_IMPL__.getEventHandler("onsettingchange");
    if (!this._isRegistered) {
      if (DEBUG) debug("Registering for messages");
      cpmm.sendAsyncMessage("Settings:RegisterForMessages",
                            undefined,
                            undefined,
                            this._window.document.nodePrincipal);
      this._isRegistered = true;
    } else {
      if ((!this._callbacks || Object.keys(this._callbacks).length == 0)  &&
          !handler) {
        if (DEBUG) debug("Unregistering for messages");
        cpmm.sendAsyncMessage("Settings:UnregisterForMessages",
                              undefined,
                              undefined,
                              this._window.document.nodePrincipal);
        this._isRegistered = false;
        this._callbacks = null;
      }
    }
  },
  
  addObserver: function addObserver(aName, aCallback) {
    if (DEBUG) debug("addObserver " + aName);
    if (!this._callbacks) {
      this._callbacks = {};
    }
    if (!this._callbacks[aName]) {
      this._callbacks[aName] = [aCallback];
    } else {
      this._callbacks[aName].push(aCallback);
    }
    this.checkMessageRegistration();
  },

  removeObserver: function removeObserver(aName, aCallback) {
    if (DEBUG) debug("deleteObserver " + aName);
    if (this._callbacks && this._callbacks[aName]) {
      let index = this._callbacks[aName].indexOf(aCallback);
      if (index != -1) {
        this._callbacks[aName].splice(index, 1);
        if (this._callbacks[aName].length == 0) {
          delete this._callbacks[aName];
        }
      } else {
        if (DEBUG) debug("Callback not found for: " + aName);
      }
    } else {
      if (DEBUG) debug("No observers stored for " + aName);
    }
    this.checkMessageRegistration();
  },

  init: function(aWindow) {
    if (DEBUG) debug("SettingsManager init");
    mrm.registerStrongReporter(this);
    cpmm.addMessageListener("Settings:Change:Return:OK", this);
    this._window = aWindow;
    Services.obs.addObserver(this, "inner-window-destroyed", false);
    let util = aWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    this.innerWindowID = util.currentInnerWindowID;
  },

  observe: function(aSubject, aTopic, aData) {
    if (DEBUG) debug("Topic: " + aTopic);
    if (aTopic == "inner-window-destroyed") {
      let wId = aSubject.QueryInterface(Ci.nsISupportsPRUint64).data;
      if (wId == this.innerWindowID) {
        this.cleanup();
      }
    }
  },

  collectReports: function(aCallback, aData, aAnonymize) {
    for (let topic in this._callbacks) {
      let length = this._callbacks[topic].length;
      if (length == 0) {
        continue;
      }

      let path;
      if (length < 20) {
        path = "settings-observers";
      } else {
        path = "settings-observers-suspect/referent(topic=" +
               (aAnonymize ? "<anonymized>" : topic) + ")";
      }

      aCallback.callback("", path,
                         Ci.nsIMemoryReporter.KIND_OTHER,
                         Ci.nsIMemoryReporter.UNITS_COUNT,
                         this._callbacks[topic].length,
                         "The number of settings observers for this topic.",
                         aData);
    }
  },

  cleanup: function() {
    Services.obs.removeObserver(this, "inner-window-destroyed");
    cpmm.removeMessageListener("Settings:Change:Return:OK", this);
    mrm.unregisterStrongReporter(this);
    this._requests = null;
    this._window = null;
    this._innerWindowID = null;
  },

  classID: Components.ID("{c40b1c70-00fb-11e2-a21f-0800200c9a66}"),
  contractID: "@mozilla.org/settingsManager;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIDOMGlobalPropertyInitializer,
                                         Ci.nsIObserver,
                                         Ci.nsIMemoryReporter]),
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SettingsManager, SettingsLock]);
