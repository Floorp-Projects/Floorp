/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function debug(s) {
//  dump("-*- SettingsManager: " + s + "\n");
}

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/SettingsQueue.jsm");
Cu.import("resource://gre/modules/SettingsDB.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "cpmm", function() {
  return Cc["@mozilla.org/childprocessmessagemanager;1"].getService(Ci.nsIFrameMessageManager);
});

const nsIClassInfo            = Ci.nsIClassInfo;
const SETTINGSLOCK_CONTRACTID = "@mozilla.org/settingsLock;1";
const SETTINGSLOCK_CID        = Components.ID("{ef95ddd0-6308-11e1-b86c-0800200c9a66}");
const nsIDOMSettingsLock      = Ci.nsIDOMSettingsLock;

function SettingsLock(aSettingsManager)
{
  this._open = true;
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
      debug("info: " + info.intent);
      let request = info.request;
      switch (info.intent) {
        case "clear":
          let req = store.clear();
          req.onsuccess = function() { this._open = true;
                                       Services.DOMRequest.fireSuccess(request, 0);
                                       this._open = false; }.bind(lock);
          req.onerror = function() { Services.DOMRequest.fireError(request, 0) };
          break;
        case "set":
          for (let key in info.settings) {
            debug("key: " + key + ", val: " + JSON.stringify(info.settings[key]) + ", type: " + typeof(info.settings[key]));

            let checkKeyRequest = store.get(key);
            checkKeyRequest.onsuccess = function (event) {
              if (!event.target.result) {
                dump("MOZSETTINGS-SET-WARNING: " + key + " is not in the database. Please add it to build/settings.js\n");
              }
            }

            if(typeof(info.settings[key]) != 'object') {
              req = store.put({settingName: key, settingValue: info.settings[key]});
            } else {
              //Workaround for cloning issues
              let obj = JSON.parse(JSON.stringify(info.settings[key]));
              req = store.put({settingName: key, settingValue: obj});
            }

            req.onsuccess = function() { 
              lock._open = true;
              Services.DOMRequest.fireSuccess(request, 0);
              cpmm.sendAsyncMessage("Settings:Changed", { key: key, value: info.settings[key] });
              lock._open = false;
            };

            req.onerror = function() {
              Services.DOMRequest.fireError(request, 0)
            };
          }
          break;
        case "get":
          req = (info.name === "*") ? store.mozGetAll()
                                    : store.mozGetAll(info.name);

          req.onsuccess = function(event) {
            debug("Request for '" + info.name + "' successful. " + 
                  "Record count: " + event.target.result.length);
            debug("result: " + JSON.stringify(event.target.result));

            if (event.target.result.length == 0) {
              dump("MOZSETTINGS-GET-WARNING: " + info.name + " is not in the database. Please add it to build/settings.js\n");
            }

            let results = {
              __exposedProps__: {
              }
            };

            for (var i in event.target.result) {
              let result = event.target.result[i];
              results[result.settingName] = result.settingValue;
              results.__exposedProps__[result.settingName] = "r";
            }

            this._open = true;
            Services.DOMRequest.fireSuccess(request, results);
            this._open = false;
          }.bind(lock);

          req.onerror = function() {
            Services.DOMRequest.fireError(request, 0)
          };
          break;
      }
    }
    if (!lock._requests.isEmpty())
      throw Components.results.NS_ERROR_ABORT;
    lock._open = true;
  },

  createTransactionAndProcess: function() {
    if (this._settingsManager._settingsDB._db) {
      var lock;
      while (lock = this._settingsManager._locks.dequeue()) {
        if (!lock._transaction) {
          lock._transaction = lock._settingsManager._settingsDB._db.transaction(SETTINGSSTORE_NAME, "readwrite");
        }
        lock.process();
      }
      if (!this._requests.isEmpty())
        this.process();
    }
  },

  get: function get(aName) {
    if (!this._open) {
      dump("Settings lock not open!\n");
      throw Components.results.NS_ERROR_ABORT;
    }

    if (this._settingsManager.hasReadPrivileges || this._settingsManager.hasReadWritePrivileges) {
      let req = Services.DOMRequest.createRequest(this._settingsManager._window);
      this._requests.enqueue({ request: req, intent:"get", name: aName });
      this.createTransactionAndProcess();
      return req;
    } else {
      debug("get not allowed");
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    }
  },

  set: function set(aSettings) {
    if (!this._open) {
      dump("Settings lock not open!\n");
      throw Components.results.NS_ERROR_ABORT;
    }

    if (this._settingsManager.hasReadWritePrivileges) {
      let req = Services.DOMRequest.createRequest(this._settingsManager._window);
      debug("send: " + JSON.stringify(aSettings));
      this._requests.enqueue({request: req, intent: "set", settings: aSettings});
      this.createTransactionAndProcess();
      return req;
    } else {
      debug("set not allowed");
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    }
  },

  clear: function clear() {
    if (!this._open) {
      dump("Settings lock not open!\n");
      throw Components.results.NS_ERROR_ABORT;
    }

    if (this._settingsManager.hasReadWritePrivileges) {
      let req = Services.DOMRequest.createRequest(this._settingsManager._window);
      this._requests.enqueue({ request: req, intent: "clear"});
      this.createTransactionAndProcess();
      return req;
    } else {
      debug("clear not allowed");
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
const SETTINGSMANAGER_CID        = Components.ID("{dd9f5380-a454-11e1-b3dd-0800200c9a66}");
const nsIDOMSettingsManager      = Ci.nsIDOMSettingsManager;

let myGlobal = this;

function SettingsManager()
{
  this._locks = new Queue();
  var idbManager = Components.classes["@mozilla.org/dom/indexeddb/manager;1"].getService(Ci.nsIIndexedDatabaseManager);
  idbManager.initWindowless(myGlobal);
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
    if (this.hasReadPrivileges || this.hasReadWritePrivileges)
      this._onsettingchange = aCallback;
    else
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },

  get onsettingchange() {
    return this._onsettingchange;
  },

  getLock: function() {
    debug("get lock!");
    var lock = new SettingsLock(this);
    this._locks.enqueue(lock);
    this._settingsDB.ensureDB(
      function() { lock.createTransactionAndProcess(); },
      function() { dump("ensureDB error cb!\n"); },
      myGlobal );
    this.nextTick(function() { this._open = false; }, lock);
    return lock;
  },

  receiveMessage: function(aMessage) {
    debug("Settings::receiveMessage: " + aMessage.name);
    let msg = aMessage.json;

    switch (aMessage.name) {
      case "Settings:Change:Return:OK":
        debug("Settings:Change:Return:OK");
        if (this._onsettingchange || this._callbacks) {
          debug('data:' + msg.key + ':' + msg.value + '\n');

          if (this._onsettingchange) {
            let event = new this._window.MozSettingsEvent("settingchanged", {
              settingName: msg.key,
              settingValue: msg.value
            });
            this._onsettingchange.handleEvent(event);
          }
          if (this._callbacks && this._callbacks[msg.key]) {
            debug("observe callback called! " + msg.key + " " + this._callbacks[msg.key].length);
            for (let cb in this._callbacks[msg.key]) {
              this._callbacks[msg.key].forEach(function(cb) {
                cb({settingName: msg.key, settingValue: msg.value});
              });
            }
          }
        } else {
          debug("no observers stored!");
        }
        break;
      default: 
        debug("Wrong message: " + aMessage.name);
    }
  },

  addObserver: function addObserver(aName, aCallback) {
    debug("addObserver " + aName);
    if (!this._callbacks)
      this._callbacks = {};
    if (!this._callbacks[aName]) {
      this._callbacks[aName] = [aCallback];
    } else {
      this._callbacks[aName].push(aCallback);
    }
  },

  removeObserver: function removeObserver(aName, aCallback) {
    debug("deleteObserver " + aName);
    if (this._callbacks && this._callbacks[aName]) {
      let index = this._callbacks[aName].indexOf(aCallback)
      if (index != -1) {
        this._callbacks[aName].splice(index, 1)
      } else {
        debug("Callback not found for: " + aName);
      }
    } else {
      debug("No observers stored for " + aName);
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

    let principal = aWindow.document.nodePrincipal;
    let secMan = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(Ci.nsIScriptSecurityManager);
    let readPerm = principal == secMan.getSystemPrincipal() ? Ci.nsIPermissionManager.ALLOW_ACTION : Services.perms.testExactPermission(principal.URI, "websettings-read");
    let readwritePerm = principal == secMan.getSystemPrincipal() ? Ci.nsIPermissionManager.ALLOW_ACTION : Services.perms.testExactPermission(principal.URI, "websettings-readwrite");
    this.hasReadPrivileges = readPerm == Ci.nsIPermissionManager.ALLOW_ACTION;
    this.hasReadWritePrivileges = readwritePerm == Ci.nsIPermissionManager.ALLOW_ACTION;
    debug("has read privileges :" + this.hasReadPrivileges + ", has read-write privileges: " + this.hasReadWritePrivileges);
  },

  observe: function(aSubject, aTopic, aData) {
    debug("Topic: " + aTopic);
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

const NSGetFactory = XPCOMUtils.generateNSGetFactory([SettingsManager, SettingsLock])
