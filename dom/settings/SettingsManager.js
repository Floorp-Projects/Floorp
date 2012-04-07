/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

/* static functions */
let DEBUG = 0;
if (DEBUG)
  debug = function (s) { dump("-*- SettingsManager: " + s + "\n"); }
else
  debug = function (s) {}

function Queue() {
  this._queue = [];
  this._index = 0;
}

Queue.prototype = {
  getLength: function() { return (this._queue.length - this._index); },

  isEmpty: function() { return (this._queue.length == 0); },

  enqueue: function(item) { this._queue.push(item); },

  dequeue: function() {
    if(this.isEmpty())
      return undefined;

    var item = this._queue[this._index];
    if (++this._index * 2 >= this._queue.length){
      this._queue  = this._queue.slice(this._index);
      this._index = 0;
    }
    return item;
  }
}

const DB_NAME = "settings";
const DB_VERSION = 1;
const STORE_NAME = "settings";

function SettingsDB() {}

SettingsDB.prototype = {

  db: null,

  close: function close() {
    if (this.db)
      this.db.close();
  },

  /**
   * Prepare the database. This may include opening the database and upgrading
   * it to the latest schema version.
   * 
   * @return (via callback) a database ready for use.
   */
  ensureDB: function ensureDB(aSuccessCb, aFailureCb, aGlobal) {
    if (this.db) {
      debug("ensureDB: already have a database, returning early.");
      return;
    }

    let self = this;
    debug("try to open database:" + DB_NAME + " " + DB_VERSION + " " + this._indexedDB);
    let req = aGlobal.mozIndexedDB.open(DB_NAME, DB_VERSION);
    req.onsuccess = function (event) {
      debug("Opened database:", DB_NAME, DB_VERSION);
      self.db = event.target.result;
      self.db.onversionchange = function(event) {
        debug("WARNING: DB modified from a different window.");
      }
      aSuccessCb();
    };
    req.onupgradeneeded = function (aEvent) {
      debug("Database needs upgrade:" + DB_NAME + aEvent.oldVersion + aEvent.newVersion);
      debug("Correct new database version:" + aEvent.newVersion == DB_VERSION);

      let db = aEvent.target.result;
      switch (aEvent.oldVersion) {
        case 0:
          debug("New database");
          self.createSchema(db);
          break;

        default:
          debug("No idea what to do with old database version:" + aEvent.oldVersion);
          aFailureCb(aEvent.target.errorMessage);
          break;
      }
    };
    req.onerror = function (aEvent) {
      debug("Failed to open database:", DB_NAME);
      aFailureCb(aEvent.target.errorMessage);
    };
    req.onblocked = function (aEvent) {
      debug("Opening database request is blocked.");
    };
  },

  createSchema: function createSchema(aDb) {
    let objectStore = aDb.createObjectStore(STORE_NAME, {keyPath: "settingName"});
    objectStore.createIndex("settingValue", "settingValue", { unique: false });
    debug("Created object stores and indexes");
  }
}

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

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
    let store = lock._transaction.objectStore(STORE_NAME);

    while (!lock._requests.isEmpty()) {
      let info = lock._requests.dequeue();
      debug("info:" + info.intent);
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
              Services.obs.notifyObservers(lock, "mozsettings-changed", JSON.stringify({
                key: key,
                value: info.settings[key]
              }));
              lock._open = false;
            };

            req.onerror = function() { Services.DOMRequest.fireError(request, 0) };
          }
          break;
        case "get":
          if (info.name == "*") {
            req = store.getAll();
          } else {
            req = store.getAll(info.name);
          }
          req.onsuccess = function(event) {
            debug("Request successful. Record count:" + event.target.result.length);
            debug("result: " + JSON.stringify(event.target.result));
            var result = {};
            for (var i in event.target.result)
              result[event.target.result[i].settingName] = event.target.result[i].settingValue;
            this._open = true;
            Services.DOMRequest.fireSuccess(request, result);
            this._open = false;
          }.bind(lock);
          req.onerror = function() { Services.DOMRequest.fireError(request, 0)};
          break;
      }
    }
    if (!lock._requests.isEmpty())
      throw Components.results.NS_ERROR_ABORT;
    lock._open = true;
  },

  createTransactionAndProcess: function() {
    if (this._settingsManager._settingsDB.db) {
      var lock;
      while (lock = this._settingsManager._locks.dequeue()) {
        if (!lock._transaction) {
          lock._transaction = lock._settingsManager._settingsDB.db.transaction(STORE_NAME, "readwrite");
        }
        lock.process();
      }
      if (!this._requests.isEmpty())
        this.process();
    }
  },

  get: function get(aName) {
    if (!this._open)
      throw Components.results.NS_ERROR_ABORT;

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
    if (!this._open)
      throw Components.results.NS_ERROR_ABORT;

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
    if (!this._open)
      throw Components.results.NS_ERROR_ABORT;

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
const SETTINGSMANAGER_CID        = Components.ID("{5609d0a0-52a9-11e1-b86c-0800200c9a66}");
const nsIDOMSettingsManager      = Ci.nsIDOMSettingsManager;

let myGlobal = this;

function SettingsManager()
{
  this._locks = new Queue();
  var idbManager = Components.classes["@mozilla.org/dom/indexeddb/manager;1"].getService(Ci.nsIIndexedDatabaseManager);
  idbManager.initWindowless(myGlobal);
  this._settingsDB = new SettingsDB();
}

SettingsManager.prototype = {
  _onsettingchange: null,

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

  init: function(aWindow) {
    // Set navigator.mozSettings to null.
    if (!Services.prefs.getBoolPref("dom.mozSettings.enabled"))
      return null;

    this._window = aWindow;
    Services.obs.addObserver(this, "inner-window-destroyed", false);
    Services.obs.addObserver(this, "mozsettings-changed", false);
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
        Services.obs.removeObserver(this, "mozsettings-changed");
        this._requests = null;
        this._window = null;
        this._innerWindowID = null;
        this._onsettingchange = null;
        this._settingsDB.close();
      }
    } else if (aTopic == "mozsettings-changed") {
      if (!this._onsettingchange)
        return;

      let data = JSON.parse(aData);
      debug('data:' + data.key + ':' + data.value + '\n');

      let event = new this._window.MozSettingsEvent("settingchanged", {
        settingName: data.key,
        settingValue: data.value
      });

      this._onsettingchange.handleEvent(event);
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
