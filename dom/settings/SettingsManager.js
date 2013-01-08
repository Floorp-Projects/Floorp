/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DEBUG = false;
function debug(s) {
  if (DEBUG) dump("-*- SettingsManager: " + s + "\n");
}

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/SettingsQueue.jsm");
Cu.import("resource://gre/modules/SettingsDB.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

const nsIClassInfo            = Ci.nsIClassInfo;
const SETTINGSLOCK_CONTRACTID = "@mozilla.org/settingsLock;1";
const SETTINGSLOCK_CID        = Components.ID("{ef95ddd0-6308-11e1-b86c-0800200c9a66}");
const nsIDOMSettingsLock      = Ci.nsIDOMSettingsLock;

function SettingsLock(aSettingsManager)
{
  this._open = true;
  this._isBusy = false;
  this._requests = new Queue();
  this._settingsManager = aSettingsManager;
  this._transaction = null;
}

SettingsLock.prototype = {

  process: function process() {
    let lock = this;
    lock._open = false;
    let store = lock._transaction.objectStore(SETTINGSSTORE_NAME);

    while (!lock._requests.isEmpty()) {
      let info = lock._requests.dequeue();
      if (DEBUG) debug("info: " + info.intent);
      let request = info.request;
      switch (info.intent) {
        case "clear":
          let clearReq = store.clear();
          clearReq.onsuccess = function() {
            this._open = true;
            Services.DOMRequest.fireSuccess(request, 0);
            this._open = false;
          }.bind(lock);
          clearReq.onerror = function() {
            Services.DOMRequest.fireError(request, 0)
          };
          break;
        case "set":
          let keys = Object.getOwnPropertyNames(info.settings);
          for (let i = 0; i < keys.length; i++) {
            let key = keys[i];
            let last = i === keys.length - 1;
            if (DEBUG) debug("key: " + key + ", val: " + JSON.stringify(info.settings[key]) + ", type: " + typeof(info.settings[key]));
            lock._isBusy = true;
            let checkKeyRequest = store.get(key);

            checkKeyRequest.onsuccess = function (event) {
              let defaultValue;
              let userValue = info.settings[key];
              if (event.target.result) {
                defaultValue = event.target.result.defaultValue;
              } else {
                defaultValue = null;
                if (DEBUG) debug("MOZSETTINGS-SET-WARNING: " + key + " is not in the database.\n");
              }

              let setReq;
              if (typeof(info.settings[key]) != 'object') {
                let obj = {settingName: key, defaultValue: defaultValue, userValue: userValue};
                if (DEBUG) debug("store1: " + JSON.stringify(obj));
                setReq = store.put(obj);
              } else {
                //Workaround for cloning issues
                let defaultVal = JSON.parse(JSON.stringify(defaultValue));
                let userVal = JSON.parse(JSON.stringify(userValue));
                let obj = {settingName: key, defaultValue: defaultVal, userValue: userVal};
                if (DEBUG) debug("store2: " + JSON.stringify(obj));
                setReq = store.put(obj);
              }

              setReq.onsuccess = function() {
                lock._isBusy = false;
                cpmm.sendAsyncMessage("Settings:Changed", { key: key, value: userValue });
                if (last && !request.error) {
                  lock._open = true;
                  Services.DOMRequest.fireSuccess(request, 0);
                  lock._open = false;
                  if (!lock._requests.isEmpty()) {
                    lock.process();
                  }
                }
              };

              setReq.onerror = function() {
                if (!request.error) {
                  Services.DOMRequest.fireError(request, setReq.error.name)
                }
              };
            }
            checkKeyRequest.onerror = function(event) {
              if (!request.error) {
                Services.DOMRequest.fireError(request, checkKeyRequest.error.name)
              }
            };
          }
          break;
        case "get":
          let getReq = (info.name === "*") ? store.mozGetAll()
                                           : store.mozGetAll(info.name);

          getReq.onsuccess = function(event) {
            if (DEBUG) debug("Request for '" + info.name + "' successful. " + 
                  "Record count: " + event.target.result.length);

            if (event.target.result.length == 0) {
              if (DEBUG) debug("MOZSETTINGS-GET-WARNING: " + info.name + " is not in the database.\n");
            }

            let results = {
              __exposedProps__: {
              }
            };

            for (var i in event.target.result) {
              let result = event.target.result[i];
              var name = result.settingName;
              if (DEBUG) debug("VAL: " + result.userValue +", " + result.defaultValue + "\n");
              var value = result.userValue !== undefined ? result.userValue : result.defaultValue;
              results[name] = value;
              results.__exposedProps__[name] = "r";
              // If the value itself is an object, expose the properties.
              if (typeof value == "object" && value != null) {
                var exposed = {};
                Object.keys(value).forEach(function(key) { exposed[key] = 'r'; });
                results[name].__exposedProps__ = exposed;
              }
            }

            this._open = true;
            Services.DOMRequest.fireSuccess(request, results);
            this._open = false;
          }.bind(lock);

          getReq.onerror = function() {
            Services.DOMRequest.fireError(request, 0)
          };
          break;
      }
    }
    lock._open = true;
  },

  createTransactionAndProcess: function() {
    if (this._settingsManager._settingsDB._db) {
      var lock;
      while (lock = this._settingsManager._locks.dequeue()) {
        if (!lock._transaction) {
          let transactionType = this._settingsManager.hasWritePrivileges ? "readwrite" : "readonly";
          lock._transaction = lock._settingsManager._settingsDB._db.transaction(SETTINGSSTORE_NAME, transactionType);
        }
        if (!lock._isBusy) {
          lock.process();
        } else {
          this._settingsManager._locks.enqueue(lock);
        }
      }
      if (!this._requests.isEmpty() && !this._isBusy) {
        this.process();
      }
    }
  },

  get: function get(aName) {
    if (!this._open) {
      dump("Settings lock not open!\n");
      throw Components.results.NS_ERROR_ABORT;
    }

    if (this._settingsManager.hasReadPrivileges) {
      let req = Services.DOMRequest.createRequest(this._settingsManager._window);
      this._requests.enqueue({ request: req, intent:"get", name: aName });
      this.createTransactionAndProcess();
      return req;
    } else {
      if (DEBUG) debug("get not allowed");
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    }
  },

  set: function set(aSettings) {
    if (!this._open) {
      dump("Settings lock not open!\n");
      throw Components.results.NS_ERROR_ABORT;
    }

    if (this._settingsManager.hasWritePrivileges) {
      let req = Services.DOMRequest.createRequest(this._settingsManager._window);
      if (DEBUG) debug("send: " + JSON.stringify(aSettings));
      let settings = JSON.parse(JSON.stringify(aSettings));
      this._requests.enqueue({request: req, intent: "set", settings: settings});
      this.createTransactionAndProcess();
      return req;
    } else {
      if (DEBUG) debug("set not allowed");
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    }
  },

  clear: function clear() {
    if (!this._open) {
      dump("Settings lock not open!\n");
      throw Components.results.NS_ERROR_ABORT;
    }

    if (this._settingsManager.hasWritePrivileges) {
      let req = Services.DOMRequest.createRequest(this._settingsManager._window);
      this._requests.enqueue({ request: req, intent: "clear"});
      this.createTransactionAndProcess();
      return req;
    } else {
      if (DEBUG) debug("clear not allowed");
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    }
  },

  classID : SETTINGSLOCK_CID,
  QueryInterface : XPCOMUtils.generateQI([nsIDOMSettingsLock]),

  classInfo : XPCOMUtils.generateCI({classID: SETTINGSLOCK_CID,
                                     contractID: SETTINGSLOCK_CONTRACTID,
                                     classDescription: "SettingsLock",
                                     interfaces: [nsIDOMSettingsLock],
                                     flags: nsIClassInfo.DOM_OBJECT})
};

const SETTINGSMANAGER_CONTRACTID = "@mozilla.org/settingsManager;1";
const SETTINGSMANAGER_CID        = Components.ID("{c40b1c70-00fb-11e2-a21f-0800200c9a66}");
const nsIDOMSettingsManager      = Ci.nsIDOMSettingsManager;

let myGlobal = this;

function SettingsManager()
{
  this._locks = new Queue();
  if (!("indexedDB" in myGlobal)) {
    let idbManager = Components.classes["@mozilla.org/dom/indexeddb/manager;1"].getService(Ci.nsIIndexedDatabaseManager);
    idbManager.initWindowless(myGlobal);
  }
  this._settingsDB = new SettingsDB();
  this._settingsDB.init(myGlobal);
}

SettingsManager.prototype = {
  _onsettingchange: null,
  _callbacks: null,

  nextTick: function nextTick(aCallback, thisObj) {
    if (thisObj)
      aCallback = aCallback.bind(thisObj);

    Services.tm.currentThread.dispatch(aCallback, Ci.nsIThread.DISPATCH_NORMAL);
  },

  set onsettingchange(aCallback) {
    if (this.hasReadPrivileges) {
      if (!this._onsettingchange) {
        cpmm.sendAsyncMessage("Settings:RegisterForMessages");
      }
      this._onsettingchange = aCallback;
    } else {
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    }
  },

  get onsettingchange() {
    return this._onsettingchange;
  },

  createLock: function() {
    if (DEBUG) debug("get lock!");
    var lock = new SettingsLock(this);
    this._locks.enqueue(lock);
    this._settingsDB.ensureDB(
      function() { lock.createTransactionAndProcess(); },
      function() { dump("Cannot open Settings DB. Trying to open an old version?\n"); },
      myGlobal );
    this.nextTick(function() { this._open = false; }, lock);
    return lock;
  },

  receiveMessage: function(aMessage) {
    if (DEBUG) debug("Settings::receiveMessage: " + aMessage.name);
    let msg = aMessage.json;

    switch (aMessage.name) {
      case "Settings:Change:Return:OK":
        if (DEBUG) debug("Settings:Change:Return:OK");
        if (this._onsettingchange || this._callbacks) {
          if (DEBUG) debug('data:' + msg.key + ':' + msg.value + '\n');

          if (this._onsettingchange) {
            let event = new this._window.MozSettingsEvent("settingchanged", {
              settingName: msg.key,
              settingValue: msg.value
            });
            this._onsettingchange.handleEvent(event);
          }
          if (this._callbacks && this._callbacks[msg.key]) {
            if (DEBUG) debug("observe callback called! " + msg.key + " " + this._callbacks[msg.key].length);
            this._callbacks[msg.key].forEach(function(cb) {
              cb({settingName: msg.key, settingValue: msg.value,
                  __exposedProps__: {settingName: 'r', settingValue: 'r'}});
            });
          }
        } else {
          if (DEBUG) debug("no observers stored!");
        }
        break;
      default: 
        if (DEBUG) debug("Wrong message: " + aMessage.name);
    }
  },

  addObserver: function addObserver(aName, aCallback) {
    if (DEBUG) debug("addObserver " + aName);
    if (!this._callbacks) {
      cpmm.sendAsyncMessage("Settings:RegisterForMessages");
      this._callbacks = {};
    }
    if (!this._callbacks[aName]) {
      this._callbacks[aName] = [aCallback];
    } else {
      this._callbacks[aName].push(aCallback);
    }
  },

  removeObserver: function removeObserver(aName, aCallback) {
    if (DEBUG) debug("deleteObserver " + aName);
    if (this._callbacks && this._callbacks[aName]) {
      let index = this._callbacks[aName].indexOf(aCallback)
      if (index != -1) {
        this._callbacks[aName].splice(index, 1)
      } else {
        if (DEBUG) debug("Callback not found for: " + aName);
      }
    } else {
      if (DEBUG) debug("No observers stored for " + aName);
    }
  },

  init: function(aWindow) {
    // Set navigator.mozSettings to null.
    if (!Services.prefs.getBoolPref("dom.mozSettings.enabled"))
      return null;

    cpmm.addMessageListener("Settings:Change:Return:OK", this);
    this._window = aWindow;
    Services.obs.addObserver(this, "inner-window-destroyed", false);
    let util = aWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    this.innerWindowID = util.currentInnerWindowID;

    let readPerm = Services.perms.testExactPermissionFromPrincipal(aWindow.document.nodePrincipal, "settings-read");
    let writePerm = Services.perms.testExactPermissionFromPrincipal(aWindow.document.nodePrincipal, "settings-write");
    this.hasReadPrivileges = readPerm == Ci.nsIPermissionManager.ALLOW_ACTION;
    this.hasWritePrivileges = writePerm == Ci.nsIPermissionManager.ALLOW_ACTION;

    if (!this.hasReadPrivileges && !this.hasWritePrivileges) {
      Cu.reportError("NO SETTINGS PERMISSION FOR: " + aWindow.document.nodePrincipal.origin + "\n");
      return null;
    }
  },

  observe: function(aSubject, aTopic, aData) {
    if (DEBUG) debug("Topic: " + aTopic);
    if (aTopic == "inner-window-destroyed") {
      let wId = aSubject.QueryInterface(Ci.nsISupportsPRUint64).data;
      if (wId == this.innerWindowID) {
        Services.obs.removeObserver(this, "inner-window-destroyed");
        cpmm.removeMessageListener("Settings:Change:Return:OK", this);
        this._requests = null;
        this._window = null;
        this._innerWindowID = null;
        this._onsettingchange = null;
        this._settingsDB.close();
      }
    }
  },

  classID : SETTINGSMANAGER_CID,
  QueryInterface : XPCOMUtils.generateQI([nsIDOMSettingsManager, Ci.nsIDOMGlobalPropertyInitializer]),

  classInfo : XPCOMUtils.generateCI({classID: SETTINGSMANAGER_CID,
                                     contractID: SETTINGSMANAGER_CONTRACTID,
                                     classDescription: "SettingsManager",
                                     interfaces: [nsIDOMSettingsManager],
                                     flags: nsIClassInfo.DOM_OBJECT})
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SettingsManager, SettingsLock])
