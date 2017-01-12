/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import('resource://gre/modules/SettingsRequestManager.jsm');

/* static functions */
var DEBUG = false;
var VERBOSE = false;

try {
  DEBUG   =
    Services.prefs.getBoolPref("dom.mozSettings.SettingsService.debug.enabled");
  VERBOSE =
    Services.prefs.getBoolPref("dom.mozSettings.SettingsService.verbose.enabled");
} catch (ex) { }

function debug(s) {
  dump("-*- SettingsService: " + s + "\n");
}

XPCOMUtils.defineLazyServiceGetter(this, "uuidgen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");
XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");
XPCOMUtils.defineLazyServiceGetter(this, "mrm",
                                   "@mozilla.org/memory-reporter-manager;1",
                                   "nsIMemoryReporterManager");

const nsIClassInfo                   = Ci.nsIClassInfo;
const kXpcomShutdownObserverTopic    = "xpcom-shutdown";

const SETTINGSSERVICELOCK_CONTRACTID = "@mozilla.org/settingsServiceLock;1";
const SETTINGSSERVICELOCK_CID        = Components.ID("{d7a395a0-e292-11e1-834e-1761d57f5f99}");
const nsISettingsServiceLock         = Ci.nsISettingsServiceLock;

function makeSettingsServiceRequest(aCallback, aName, aValue) {
  return {
    callback: aCallback,
    name: aName,
    value: aValue
  };
};

const kLockListeners = ["Settings:Get:OK", "Settings:Get:KO",
                        "Settings:Clear:OK", "Settings:Clear:KO",
                        "Settings:Set:OK", "Settings:Set:KO",
                        "Settings:Finalize:OK", "Settings:Finalize:KO"];

function SettingsServiceLock(aSettingsService, aTransactionCallback) {
  if (VERBOSE) debug("settingsServiceLock constr!");
  this._open = true;
  this._settingsService = aSettingsService;
  this._id = uuidgen.generateUUID().toString();
  this._transactionCallback = aTransactionCallback;
  this._requests = {};
  let closeHelper = function() {
    if (VERBOSE) debug("closing lock " + this._id);
    this._open = false;
    this.runOrFinalizeQueries();
  }.bind(this);

  this.addListeners();

  let createLockPayload = {
    lockID: this._id,
    isServiceLock: true,
    windowID: undefined,
    lockStack: (new Error).stack
  };

  this.returnMessage("Settings:CreateLock", createLockPayload);
  Services.tm.currentThread.dispatch(closeHelper, Ci.nsIThread.DISPATCH_NORMAL);
}

SettingsServiceLock.prototype = {
  get closed() {
    return !this._open;
  },

  addListeners: function() {
    for (let msg of kLockListeners) {
      cpmm.addMessageListener(msg, this);
    }
  },

  removeListeners: function() {
    for (let msg of kLockListeners) {
      cpmm.removeMessageListener(msg, this);
    }
  },

  returnMessage: function(aMessage, aData) {
    SettingsRequestManager.receiveMessage({
      name: aMessage,
      data: aData,
      target: undefined,
      principal: Services.scriptSecurityManager.getSystemPrincipal()
    });
  },

  runOrFinalizeQueries: function() {
    if (!this._requests || Object.keys(this._requests).length == 0) {
      this.returnMessage("Settings:Finalize", {lockID: this._id});
    } else {
      this.returnMessage("Settings:Run", {lockID: this._id});
    }
  },

  receiveMessage: function(aMessage) {

    let msg = aMessage.data;
    // SettingsRequestManager broadcasts changes to all locks in the child. If
    // our lock isn't being addressed, just return.
    if(msg.lockID != this._id) {
      return;
    }
    if (VERBOSE) debug("receiveMessage (" + this._id + "): " + aMessage.name);
    // Finalizing a transaction does not return a request ID since we are
    // supposed to fire callbacks.
    if (!msg.requestID) {
      switch (aMessage.name) {
        case "Settings:Finalize:OK":
          if (VERBOSE) debug("Lock finalize ok!");
          this.callTransactionHandle();
          break;
        case "Settings:Finalize:KO":
          if (DEBUG) debug("Lock finalize failed!");
          this.callAbort();
          break;
        default:
          if (DEBUG) debug("Message type " + aMessage.name + " is missing a requestID");
      }

      this._settingsService.unregisterLock(this._id);
      return;
    }

    let req = this._requests[msg.requestID];
    if (!req) {
      if (DEBUG) debug("Matching request not found.");
      return;
    }
    delete this._requests[msg.requestID];
    switch (aMessage.name) {
      case "Settings:Get:OK":
        this._open = true;
        let settings_names = Object.keys(msg.settings);
        if (settings_names.length > 0) {
          let name = settings_names[0];
          if (DEBUG && settings_names.length > 1) {
            debug("Warning: overloaded setting:" + name);
          }
          let result = msg.settings[name];
          this.callHandle(req.callback, name, result);
        } else {
          this.callHandle(req.callback, req.name, null);
        }
        this._open = false;
        break;
      case "Settings:Set:OK":
        this._open = true;
        // We don't pass values back from sets in SettingsManager...
        this.callHandle(req.callback, req.name, req.value);
        this._open = false;
        break;
      case "Settings:Get:KO":
      case "Settings:Set:KO":
        if (DEBUG) debug("error:" + msg.errorMsg);
        this.callError(req.callback, msg.error);
        break;
      default:
        if (DEBUG) debug("Wrong message: " + aMessage.name);
    }
    this.runOrFinalizeQueries();
  },

  get: function get(aName, aCallback) {
    if (VERBOSE) debug("get (" + this._id + "): " + aName);
    if (!this._open) {
      if (DEBUG) debug("Settings lock not open!\n");
      throw Components.results.NS_ERROR_ABORT;
    }
    let reqID = uuidgen.generateUUID().toString();
    this._requests[reqID] = makeSettingsServiceRequest(aCallback, aName);
    this.returnMessage("Settings:Get", {requestID: reqID,
                                        lockID: this._id,
                                        name: aName});
  },

  set: function set(aName, aValue, aCallback) {
    if (VERBOSE) debug("set: " + aName + " " + aValue);
    if (!this._open) {
      throw "Settings lock not open";
    }
    let reqID = uuidgen.generateUUID().toString();
    this._requests[reqID] = makeSettingsServiceRequest(aCallback, aName, aValue);
    let settings = {};
    settings[aName] = aValue;
    this.returnMessage("Settings:Set", {requestID: reqID,
                                        lockID: this._id,
                                        settings: settings});
  },

  callHandle: function callHandle(aCallback, aName, aValue) {
    try {
        aCallback && aCallback.handle ? aCallback.handle(aName, aValue) : null;
    } catch (e) {
      if (DEBUG) debug("settings 'handle' for " + aName + " callback threw an exception, dropping: " + e + "\n");
    }
  },

  callAbort: function callAbort(aCallback, aMessage) {
    try {
      aCallback && aCallback.handleAbort ? aCallback.handleAbort(aMessage) : null;
    } catch (e) {
      if (DEBUG) debug("settings 'abort' callback threw an exception, dropping: " + e + "\n");
    }
  },

  callError: function callError(aCallback, aMessage) {
    try {
      aCallback && aCallback.handleError ? aCallback.handleError(aMessage) : null;
    } catch (e) {
      if (DEBUG) debug("settings 'error' callback threw an exception, dropping: " + e + "\n");
    }
  },

  callTransactionHandle: function callTransactionHandle() {
    try {
      this._transactionCallback && this._transactionCallback.handle ? this._transactionCallback.handle() : null;
    } catch (e) {
      if (DEBUG) debug("settings 'Transaction handle' callback threw an exception, dropping: " + e + "\n");
    }
  },

  classID : SETTINGSSERVICELOCK_CID,
  QueryInterface : XPCOMUtils.generateQI([nsISettingsServiceLock])
};

const SETTINGSSERVICE_CID        = Components.ID("{f656f0c0-f776-11e1-a21f-0800200c9a66}");

function SettingsService()
{
  if (VERBOSE) debug("settingsService Constructor");
  this._locks = [];
  this._serviceLocks = {};
  this._createdLocks = 0;
  this._unregisteredLocks = 0;
  this.init();
}

SettingsService.prototype = {

  init: function() {
    Services.obs.addObserver(this, kXpcomShutdownObserverTopic, false);
    mrm.registerStrongReporter(this);
  },

  uninit: function() {
    Services.obs.removeObserver(this, kXpcomShutdownObserverTopic);
    mrm.unregisterStrongReporter(this);
  },

  observe: function(aSubject, aTopic, aData) {
    if (VERBOSE) debug("observe: " + aTopic);
    if (aTopic === kXpcomShutdownObserverTopic) {
      this.uninit();
    }
  },

  receiveMessage: function(aMessage) {
    if (VERBOSE) debug("Entering receiveMessage");

    let lockID = aMessage.data.lockID;
    if (!lockID) {
      if (DEBUG) debug("No lock ID");
      return;
    }

    if (!(lockID in this._serviceLocks)) {
      if (DEBUG) debug("Received message for lock " + lockID + " but no lock");
      return;
    }

    if (VERBOSE) debug("Delivering message");
    this._serviceLocks[lockID].receiveMessage(aMessage);
  },

  createLock: function createLock(aCallback) {
    if (VERBOSE) debug("Calling createLock");
    var lock = new SettingsServiceLock(this, aCallback);
    if (VERBOSE) debug("Created lock " + lock._id);
    this.registerLock(lock);
    return lock;
  },

  registerLock: function(aLock) {
    if (VERBOSE) debug("Registering lock " + aLock._id);
    this._locks.push(aLock._id);
    this._serviceLocks[aLock._id] = aLock;
    this._createdLocks++;
  },

  unregisterLock: function(aLockID) {
    let lock_index = this._locks.indexOf(aLockID);
    if (lock_index != -1) {
      if (VERBOSE) debug("Unregistering lock " + aLockID);
      this._locks.splice(lock_index, 1);
      this._serviceLocks[aLockID].removeListeners();
      this._serviceLocks[aLockID] = null;
      delete this._serviceLocks[aLockID];
      this._unregisteredLocks++;
    }
  },

  collectReports: function(aCallback, aData, aAnonymize) {
    aCallback.callback("",
                       "settings-service-locks/alive",
                       Ci.nsIMemoryReporter.KIND_OTHER,
                       Ci.nsIMemoryReporter.UNITS_COUNT,
                       this._locks.length,
                       "The number of service locks that are currently alives.",
                       aData);

    aCallback.callback("",
                       "settings-service-locks/created",
                       Ci.nsIMemoryReporter.KIND_OTHER,
                       Ci.nsIMemoryReporter.UNITS_COUNT,
                       this._createdLocks,
                       "The number of service locks that were created.",
                       aData);

    aCallback.callback("",
                       "settings-service-locks/deleted",
                       Ci.nsIMemoryReporter.KIND_OTHER,
                       Ci.nsIMemoryReporter.UNITS_COUNT,
                       this._unregisteredLocks,
                       "The number of service locks that were deleted.",
                       aData);
  },

  classID : SETTINGSSERVICE_CID,
  QueryInterface : XPCOMUtils.generateQI([Ci.nsISettingsService,
                                          Ci.nsIObserver,
                                          Ci.nsIMemoryReporter])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SettingsService, SettingsServiceLock]);
