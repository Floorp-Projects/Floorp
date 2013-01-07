/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

/* static functions */
let DEBUG = 0;
let debug;
if (DEBUG)
  debug = function (s) { dump("-*- SettingsService: " + s + "\n"); }
else
  debug = function (s) {}

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/SettingsQueue.jsm");
Cu.import("resource://gre/modules/SettingsDB.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const nsIClassInfo            = Ci.nsIClassInfo;

const SETTINGSSERVICELOCK_CONTRACTID = "@mozilla.org/settingsServiceLock;1";
const SETTINGSSERVICELOCK_CID        = Components.ID("{d7a395a0-e292-11e1-834e-1761d57f5f99}");
const nsISettingsServiceLock         = Ci.nsISettingsServiceLock;

function SettingsServiceLock(aSettingsService)
{
  debug("settingsServiceLock constr!");
  this._open = true;
  this._requests = new Queue();
  this._settingsService = aSettingsService;
  this._transaction = null;
}

SettingsServiceLock.prototype = {

  process: function process() {
    debug("process!");
    let lock = this;
    lock._open = false;
    let store = lock._transaction.objectStore(SETTINGSSTORE_NAME);

    while (!lock._requests.isEmpty()) {
      let info = lock._requests.dequeue();
      debug("info:" + info.intent);
      let callback = info.callback;
      let req;
      let name = info.name;
      switch (info.intent) {
        case "set":
          let value = info.value;
          let message = info.message;
          if(typeof(value) == 'object')
            debug("object name:" + name + ", val: " + JSON.stringify(value));
          let checkKeyRequest = store.get(name);

          checkKeyRequest.onsuccess = function (event) {
            let defaultValue;
            if (event.target.result) {
              defaultValue = event.target.result.defaultValue;
            } else {
              defaultValue = null;
              if (DEBUG) debug("MOZSETTINGS-SET-WARNING: " + key + " is not in the database.\n");
            }

            req = store.put({ settingName: name, defaultValue: defaultValue, userValue: value });

            req.onsuccess = function() {
              if (DEBUG) debug("set on success");
              lock._open = true;
              if (callback)
                callback.handle(name, value);
              Services.obs.notifyObservers(lock, "mozsettings-changed", JSON.stringify({
                key: name,
                value: value,
                message: message
              }));
              lock._open = false;
            };

            req.onerror = function(event) { callback ? callback.handleError(event.target.errorMessage) : null; };
          }

          checkKeyRequest.onerror = function(event) { callback ? callback.handleError(event.target.errorMessage) : null; };
          break;
        case "get":
          req = store.mozGetAll(name);
          req.onsuccess = function(event) {
            debug("Request successful. Record count:" + event.target.result.length);
            debug("result: " + JSON.stringify(event.target.result));
            this._open = true;
            if (callback) {
              if (event.target.result[0]) {
                if (event.target.result.length > 1) {
                  debug("Warning: overloaded setting:" + name);
                }
                let result = event.target.result[0];
                let value = result.userValue !== undefined
                            ? result.userValue
                            : result.defaultValue;
                callback.handle(name, value);
              } else
                callback.handle(name, null);
            } else {
              debug("no callback defined!");
            }
            this._open = false;
          }.bind(lock);
          req.onerror = function error(event) { callback ? callback.handleError(event.target.errorMessage) : null; };
          break;
      }
    }
    if (!lock._requests.isEmpty())
      throw Components.results.NS_ERROR_ABORT;
    lock._open = true;
  },

  createTransactionAndProcess: function createTransactionAndProcess() {
    if (this._settingsService._settingsDB._db) {
      var lock;
      while (lock = this._settingsService._locks.dequeue()) {
        if (!lock._transaction) {
          lock._transaction = lock._settingsService._settingsDB._db.transaction(SETTINGSSTORE_NAME, "readwrite");
        }
        lock.process();
      }
      if (!this._requests.isEmpty())
        this.process();
    }
  },

  get: function get(aName, aCallback) {
    debug("get: " + aName + ", " + aCallback);
    this._requests.enqueue({ callback: aCallback, intent:"get", name: aName });
    this.createTransactionAndProcess();
  },

  set: function set(aName, aValue, aCallback, aMessage) {
    debug("set: " + aName + ": " + JSON.stringify(aValue));
    if (aMessage === undefined)
      aMessage = null;
    this._requests.enqueue({ callback: aCallback,
                             intent: "set", 
                             name: aName, 
                             value: aValue, 
                             message: aMessage });
    this.createTransactionAndProcess();
  },

  classID : SETTINGSSERVICELOCK_CID,
  QueryInterface : XPCOMUtils.generateQI([nsISettingsServiceLock]),

  classInfo : XPCOMUtils.generateCI({ classID: SETTINGSSERVICELOCK_CID,
                                      contractID: SETTINGSSERVICELOCK_CONTRACTID,
                                      classDescription: "SettingsServiceLock",
                                      interfaces: [nsISettingsServiceLock],
                                      flags: nsIClassInfo.DOM_OBJECT })
};

const SETTINGSSERVICE_CID        = Components.ID("{f656f0c0-f776-11e1-a21f-0800200c9a66}");

let myGlobal = this;

function SettingsService()
{
  debug("settingsService Constructor");
  this._locks = new Queue();
  if (!("indexedDB" in myGlobal)) {
    let idbManager = Components.classes["@mozilla.org/dom/indexeddb/manager;1"].getService(Ci.nsIIndexedDatabaseManager);
    idbManager.initWindowless(myGlobal);
  }
  this._settingsDB = new SettingsDB();
  this._settingsDB.init(myGlobal);
}

SettingsService.prototype = {

  nextTick: function nextTick(aCallback, thisObj) {
    if (thisObj)
      aCallback = aCallback.bind(thisObj);

    Services.tm.currentThread.dispatch(aCallback, Ci.nsIThread.DISPATCH_NORMAL);
  },

  createLock: function createLock() {
    debug("get lock!");
    var lock = new SettingsServiceLock(this);
    this._locks.enqueue(lock);
    this._settingsDB.ensureDB(
      function() { lock.createTransactionAndProcess(); },
      function() { dump("ensureDB error cb!\n"); },
      myGlobal );
    this.nextTick(function() { this._open = false; }, lock);
    return lock;
  },

  classID : SETTINGSSERVICE_CID,
  QueryInterface : XPCOMUtils.generateQI([Ci.nsISettingsService]),
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SettingsService, SettingsServiceLock])
