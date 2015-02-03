/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

this.EXPORTED_SYMBOLS = ["SettingsRequestManager"];

Cu.import("resource://gre/modules/SettingsDB.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PermissionsTable.jsm");

let DEBUG = false;
let VERBOSE = false;
let TRACK = false;

try {
  DEBUG   =
    Services.prefs.getBoolPref("dom.mozSettings.SettingsRequestManager.debug.enabled");
  VERBOSE =
    Services.prefs.getBoolPref("dom.mozSettings.SettingsRequestManager.verbose.enabled");
  TRACK =
    Services.prefs.getBoolPref("dom.mozSettings.trackTasksUsage");
} catch (ex) { }

let allowForceReadOnly = false;
try {
  allowForceReadOnly = Services.prefs.getBoolPref("dom.mozSettings.allowForceReadOnly");
} catch (ex) { }

function debug(s) {
  dump("-*- SettingsRequestManager: " + s + "\n");
}

const kXpcomShutdownObserverTopic      = "xpcom-shutdown";
const kInnerWindowDestroyed            = "inner-window-destroyed";
const kMozSettingsChangedObserverTopic = "mozsettings-changed";
const kSettingsReadSuffix              = "-read";
const kSettingsWriteSuffix             = "-write";
const kSettingsClearPermission         = "settings-clear";
const kAllSettingsReadPermission       = "settings" + kSettingsReadSuffix;
const kAllSettingsWritePermission      = "settings" + kSettingsWriteSuffix;
// Any application with settings permissions, be it for all settings
// or a single one, will need to be able to access the settings API.
// The settings-api permission allows an app to see the mozSettings
// API in order to create locks and queue tasks. Whether these tasks
// will be allowed depends on the exact permissions the app has.
const kSomeSettingsReadPermission      = "settings-api" + kSettingsReadSuffix;
const kSomeSettingsWritePermission     = "settings-api" + kSettingsWriteSuffix;
// Time, in seconds, to consider the API is starting to jam
const kSoftLockupDelta                 = 30;

XPCOMUtils.defineLazyServiceGetter(this, "mrm",
                                   "@mozilla.org/memory-reporter-manager;1",
                                   "nsIMemoryReporterManager");
XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageBroadcaster");
XPCOMUtils.defineLazyServiceGetter(this, "uuidgen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");
XPCOMUtils.defineLazyServiceGetter(this, "gSettingsService",
                                   "@mozilla.org/settingsService;1",
                                   "nsISettingsService");

let SettingsPermissions = {
  checkPermission: function(aPrincipal, aPerm) {
    if (!aPrincipal) {
      Cu.reportError("SettingsPermissions.checkPermission was passed a null principal. Denying all permissions.");
      return false;
    }
    if (aPrincipal.origin == "[System Principal]" ||
        Services.perms.testExactPermissionFromPrincipal(aPrincipal, aPerm) == Ci.nsIPermissionManager.ALLOW_ACTION) {
      return true;
    }
    return false;
  },
  hasAllReadPermission: function(aPrincipal) {
    return this.checkPermission(aPrincipal, kAllSettingsReadPermission);
  },
  hasAllWritePermission: function(aPrincipal) {
    return this.checkPermission(aPrincipal, kAllSettingsWritePermission);
  },
  hasSomeReadPermission: function(aPrincipal) {
    return this.checkPermission(aPrincipal, kSomeSettingsReadPermission);
  },
  hasSomeWritePermission: function(aPrincipal) {
    return this.checkPermission(aPrincipal, kSomeSettingsWritePermission);
  },
  hasClearPermission: function(aPrincipal) {
    return this.checkPermission(aPrincipal, kSettingsClearPermission);
  },
  hasReadPermission: function(aPrincipal, aSettingsName) {
    return this.hasAllReadPermission(aPrincipal) || this.checkPermission(aPrincipal, "settings:" + aSettingsName + kSettingsReadSuffix);
  },
  hasWritePermission: function(aPrincipal, aSettingsName) {
    return this.hasAllWritePermission(aPrincipal) || this.checkPermission(aPrincipal, "settings:" + aSettingsName + kSettingsWriteSuffix);
  }
};


function SettingsLockInfo(aDB, aMsgMgr, aPrincipal, aLockID, aIsServiceLock, aWindowID, aLockStack) {
  return {
    // ID Shared with the object on the child side
    lockID: aLockID,
    // Is this a content lock or a settings service lock?
    isServiceLock: aIsServiceLock,
    // Which inner window ID
    windowID: aWindowID,
    // Where does this lock comes from
    lockStack: aLockStack,
    // Tasks to be run once the lock is at the head of the queue
    tasks: [],
    // This is set to true once a transaction is ready to run, but is not at the
    // head of the lock queue.
    consumable: false,
    // Holds values that are requested to be set until the lock lifetime ends,
    // then commits them to the DB.
    queuedSets: {},
    // Internal transaction object
    _transaction: undefined,
    // Message manager that controls the lock
    _mm: aMsgMgr,
    // If true, it means a permissions check failed, so just fail everything now
    _failed: false,
    // If we're slated to run finalize, set this to make sure we don't
    // somehow run other events afterward.
    finalizing: false,
    // Lets us know if we can use this lock for a clear command
    canClear: true,
    // Lets us know if this lock has been used to clear at any point.
    hasCleared: false,
    // forceReadOnly sets whether we want to do a read only transaction. Define
    // true by default, and let queueTask() set this to false if we queue any
    // "set" task. Since users of settings locks will queue all tasks before
    // any idb transaction is created, we know we will have all needed
    // information to set this before creating a transaction.
    forceReadOnly: true,
    // Principal the lock was created under. We assume that the lock
    // will continue to exist under this principal for the duration of
    // its lifetime.
    principal: aPrincipal,
    getObjectStore: function() {
      if (VERBOSE) debug("Getting transaction for " + this.lockID);
      let store;
      // Test for transaction validity via trying to get the
      // datastore. If it doesn't work, assume the transaction is
      // closed, create a new transaction and try again.
      if (this._transaction) {
        try {
          store = this._transaction.objectStore(SETTINGSSTORE_NAME);
        } catch (e) {
          if (e.name == "InvalidStateError") {
            if (VERBOSE) debug("Current transaction for " + this.lockID + " closed, trying to create new one.");
          } else {
            if (DEBUG) debug("Unexpected exception, throwing: " + e);
            throw e;
          }
        }
      }
      // Create one transaction with a global permission. This may be
      // slightly slower on apps with full settings permissions, but
      // it means we don't have to do our own transaction order
      // bookkeeping.
      let canReadOnly = allowForceReadOnly && this.forceReadOnly;
      if (canReadOnly || !SettingsPermissions.hasSomeWritePermission(this.principal)) {
        if (VERBOSE) debug("Making READONLY transaction for " + this.lockID);
        this._transaction = aDB._db.transaction(SETTINGSSTORE_NAME, "readonly");
      } else {
        if (VERBOSE) debug("Making READWRITE transaction for " + this.lockID);
        this._transaction = aDB._db.transaction(SETTINGSSTORE_NAME, "readwrite");
      }
      this._transaction.oncomplete = function() {
        if (VERBOSE) debug("Transaction for lock " + this.lockID + " closed");
      }.bind(this);
      this._transaction.onabort = function () {
        if (DEBUG) debug("Transaction for lock " + this.lockID + " aborted");
        this._failed = true;
      }.bind(this);
      try {
        store = this._transaction.objectStore(SETTINGSSTORE_NAME);
      } catch (e) {
          if (e.name == "InvalidStateError") {
            if (DEBUG) debug("Cannot create objectstore on transaction for " + this.lockID);
            return null;
          } else {
            if (DEBUG) debug("Unexpected exception, throwing: " + e);
            throw e;
          }
      }
      return store;
    }
  };
}

let SettingsRequestManager = {
  // Access to the settings DB
  settingsDB: new SettingsDB(),
  // Remote messages to listen for from child
  messages: ["child-process-shutdown", "Settings:Get", "Settings:Set",
             "Settings:Clear", "Settings:Run", "Settings:Finalize",
             "Settings:CreateLock", "Settings:RegisterForMessages"],
  // Map of LockID to SettingsLockInfo objects
  lockInfo: {},
  // Storing soft lockup detection infos
  softLockup: {
    lockId: null, // last lock dealt with
    lockTs: null  // last time of dealing with
  },
  // Queue of LockIDs. The LockID on the front of the queue is the only lock
  // that will have requests processed, all other locks will queue requests
  // until they hit the front of the queue.
  settingsLockQueue: [],
  children: [],
  // Since we need to call observers at times when we may not have
  // just received a message from a child process, we cache principals
  // for message managers and check permissions on them before we send
  // settings notifications to child processes.
  observerPrincipalCache: new Map(),
  totalProcessed: 0,
  tasksConsumed: {},
  totalSetProcessed: 0,
  tasksSetConsumed: {},
  totalGetProcessed: 0,
  tasksGetConsumed: {},

  init: function() {
    if (VERBOSE) debug("init");
    this.settingsDB.init();
    this.messages.forEach((function(msgName) {
      ppmm.addMessageListener(msgName, this);
    }).bind(this));
    Services.obs.addObserver(this, kXpcomShutdownObserverTopic, false);
    Services.obs.addObserver(this, kInnerWindowDestroyed, false);
    mrm.registerStrongReporter(this);
  },

  _serializePreservingBinaries: function _serializePreservingBinaries(aObject) {
    function needsUUID(aValue) {
      if (!aValue || !aValue.constructor) {
        return false;
      }
      return (aValue.constructor.name == "Date") || (aValue instanceof Ci.nsIDOMFile) ||
             (aValue instanceof Ci.nsIDOMBlob);
    }
    // We need to serialize settings objects, otherwise they can change between
    // the set() call and the enqueued request being processed. We can't simply
    // parse(stringify(obj)) because that breaks things like Blobs, Files and
    // Dates, so we use stringify's replacer and parse's reviver parameters to
    // preserve binaries.
    let binaries = Object.create(null);
    let stringified = JSON.stringify(aObject, function(key, value) {
      value = this.settingsDB.prepareValue(value);
      if (needsUUID(value)) {
        let uuid = uuidgen.generateUUID().toString();
        binaries[uuid] = value;
        return uuid;
      }
      return value;
    }.bind(this));
    return JSON.parse(stringified, function(key, value) {
      if (value in binaries) {
        return binaries[value];
      }
      return value;
    });
  },

  queueTask: function(aOperation, aData) {
    if (VERBOSE) debug("Queueing task: " + aOperation);

    let defer = {};

    let lock = this.lockInfo[aData.lockID];

    if (!lock) {
      return Promise.reject({error: "Lock already dead, cannot queue task"});
    }

    if (aOperation == "set") {
      aData.settings = this._serializePreservingBinaries(aData.settings);
    }

    if (aOperation === "set" || aOperation === "clear") {
      lock.forceReadOnly = false;
    }

    lock.tasks.push({
      operation: aOperation,
      data: aData,
      defer: defer
    });

    let promise = new Promise(function(resolve, reject) {
      defer.resolve = resolve;
      defer.reject = reject;
    });

    return promise;
  },

  // Due to the fact that we're skipping the database in some places
  // by keeping a local "set" value cache, resolving some calls
  // without a call to the database would mean we could potentially
  // receive promise responses out of expected order if a get is
  // called before a set. Therefore, we wrap our resolve in a null
  // get, which means it will resolves afer the rest of the calls
  // queued to the DB.
  queueTaskReturn: function(aTask, aReturnValue) {
    if (VERBOSE) debug("Making task queuing transaction request.");
    let data = aTask.data;
    let lock = this.lockInfo[data.lockID];
    let store = lock.getObjectStore(lock.principal);
    if (!store) {
      if (DEBUG) debug("Rejecting task queue on lock " + aTask.data.lockID);
      return Promise.reject({task: aTask, error: "Cannot get object store"});
    }
    // Due to the fact that we're skipping the database, resolving
    // this without a call to the database would mean we could
    // potentially receive promise responses out of expected order if
    // a get is called before a set. Therefore, we wrap our resolve in
    // a null get, which means it will resolves afer the rest of the
    // calls queued to the DB.
    let getReq = store.get(0);

    let defer = {};
    let promiseWrapper = new Promise(function(resolve, reject) {
      defer.resolve = resolve;
      defer.reject = reject;
    });

    getReq.onsuccess = function(event) {
      return defer.resolve(aReturnValue);
    };
    getReq.onerror = function() {
      return defer.reject({task: aTask, error: getReq.error.name});
    };
    return promiseWrapper;
  },
  
  taskGet: function(aTask) {
    if (VERBOSE) debug("Running Get task on lock " + aTask.data.lockID);

    // Check that we have permissions for getting the value
    let data = aTask.data;
    let lock = this.lockInfo[data.lockID];

    if (!lock) {
      return Promise.reject({task: aTask, error: "Lock died, can't finalize"});
    }
    if (lock._failed) {
      if (DEBUG) debug("Lock failed. All subsequent requests will fail.");
      return Promise.reject({task: aTask, error: "Lock failed, all requests now failing."});
    }

    if (lock.hasCleared) {
      if (DEBUG) debug("Lock was used for a clear command. All subsequent requests will fail.");
      return Promise.reject({task: aTask, error: "Lock was used for a clear command. All subsequent requests will fail."});
    }

    lock.canClear = false;
    
    if (!SettingsPermissions.hasReadPermission(lock.principal, data.name)) {
      if (DEBUG) debug("get not allowed for " + data.name);
      lock._failed = true;
      return Promise.reject({task: aTask, error: "No permission to get " + data.name});
    }

    // If the value was set during this transaction, use the cached value
    if (data.name in lock.queuedSets) {
      if (VERBOSE) debug("Returning cached set value " + lock.queuedSets[data.name] + " for " + data.name);
      let local_results = {};
      local_results[data.name] = lock.queuedSets[data.name];
      return this.queueTaskReturn(aTask, {task: aTask, results: local_results});
    }

    // Create/Get transaction and make request
    if (VERBOSE) debug("Making get transaction request for " + data.name);
    let store = lock.getObjectStore(lock.principal);
    if (!store) {
      if (DEBUG) debug("Rejecting Get task on lock " + aTask.data.lockID);
      return Promise.reject({task: aTask, error: "Cannot get object store"});
    }

    if (VERBOSE) debug("Making get request for " + data.name);
    let getReq = (data.name === "*") ? store.mozGetAll() : store.mozGetAll(data.name);

    let defer = {};
    let promiseWrapper = new Promise(function(resolve, reject) {
      defer.resolve = resolve;
      defer.reject = reject;
    });

    getReq.onsuccess = function(event) {
      if (VERBOSE) debug("Request for '" + data.name + "' successful. " +
            "Record count: " + event.target.result.length);

      if (event.target.result.length == 0) {
        if (VERBOSE) debug("MOZSETTINGS-GET-WARNING: " + data.name + " is not in the database.\n");
      }

      let results = {};

      for (let i in event.target.result) {
        let result = event.target.result[i];
        let name = result.settingName;
        if (VERBOSE) debug(name + ": " + result.userValue +", " + result.defaultValue);
        let value = result.userValue !== undefined ? result.userValue : result.defaultValue;
        results[name] = value;
      }
      return defer.resolve({task: aTask, results: results});
    };
    getReq.onerror = function() {
      return defer.reject({task: aTask, error: getReq.error.name});
    };
    return promiseWrapper;
  },

  taskSet: function(aTask) {
    let data = aTask.data;
    let lock = this.lockInfo[data.lockID];
    let keys = Object.getOwnPropertyNames(data.settings);

    if (!lock) {
      return Promise.reject({task: aTask, error: "Lock died, can't finalize"});
    }
    if (lock._failed) {
      if (DEBUG) debug("Lock failed. All subsequent requests will fail.");
      return Promise.reject({task: aTask, error: "Lock failed a permissions check, all requests now failing."});
    }

    if (lock.hasCleared) {
      if (DEBUG) debug("Lock was used for a clear command. All subsequent requests will fail.");
      return Promise.reject({task: aTask, error: "Lock was used for a clear command. All other requests will fail."});
    }

    lock.canClear = false;

    // If we have no keys, resolve
    if (keys.length === 0) {
      if (DEBUG) debug("No keys to change entered!");
      return Promise.resolve({task: aTask});
    }

    for (let i = 0; i < keys.length; i++) {
      if (!SettingsPermissions.hasWritePermission(lock.principal, keys[i])) {
        if (DEBUG) debug("set not allowed on " + keys[i]);
        lock._failed = true;
        return Promise.reject({task: aTask, error: "No permission to set " + keys[i]});
      }
    }

    for (let i = 0; i < keys.length; i++) {
      let key = keys[i];
      if (VERBOSE) debug("key: " + key + ", val: " + JSON.stringify(data.settings[key]) + ", type: " + typeof(data.settings[key]));
      lock.queuedSets[key] = data.settings[key];
    }

    return this.queueTaskReturn(aTask, {task: aTask});
  },

  startRunning: function(aLockID) {
    let lock = this.lockInfo[aLockID];

    if (!lock) {
      if (DEBUG) debug("Lock no longer alive, cannot start running");
      return;
    }

    lock.consumable = true;
    if (aLockID == this.settingsLockQueue[0] || this.settingsLockQueue.length == 0) {
      // If a lock is currently at the head of the queue, run all tasks for
      // it.
      if (VERBOSE) debug("Start running tasks for " + aLockID);
      this.queueConsume();
    } else {
      // If a lock isn't at the head of the queue, but requests to be run,
      // simply mark it as consumable, which means it will automatically run
      // once it comes to the head of the queue.
      if (VERBOSE) debug("Queuing tasks for " + aLockID + " while waiting for " + this.settingsLockQueue[0]);
    }
  },

  queueConsume: function() {
    if (this.settingsLockQueue.length > 0 && this.lockInfo[this.settingsLockQueue[0]].consumable) {
      Services.tm.currentThread.dispatch(SettingsRequestManager.consumeTasks.bind(this), Ci.nsIThread.DISPATCH_NORMAL);
    }
  },

  finalizeSets: function(aTask) {
    let data = aTask.data;
    if (VERBOSE) debug("Finalizing tasks for lock " + data.lockID);
    let lock = this.lockInfo[data.lockID];

    if (!lock) {
      return Promise.reject({task: aTask, error: "Lock died, can't finalize"});
    }
    lock.finalizing = true;
    if (lock._failed) {
      this.removeLock(data.lockID);
      return Promise.reject({task: aTask, error: "Lock failed a permissions check, all requests now failing."});
    }
    // If we have cleared, there is no reason to continue finalizing
    // this lock. Just resolve promise with task and move on.
    if (lock.hasCleared) {
      if (VERBOSE) debug("Clear was called on lock, skipping finalize");
      this.removeLock(data.lockID);
      return Promise.resolve({task: aTask});
    }
    let keys = Object.getOwnPropertyNames(lock.queuedSets);
    if (keys.length === 0) {
      if (VERBOSE) debug("Nothing to finalize. Exiting.");
      this.removeLock(data.lockID);
      return Promise.resolve({task: aTask});
    }

    let store = lock.getObjectStore(lock.principal);
    if (!store) {
      if (DEBUG) debug("Rejecting Set task on lock " + aTask.data.lockID);
      this.removeLock(data.lockID);
      return Promise.reject({task: aTask, error: "Cannot get object store"});
    }

    // Due to the fact there may have multiple set operations to clear, and
    // they're all async, callbacks are gathered into promises, and the promises
    // are processed with Promises.all().
    let checkPromises = [];
    let finalValues = {};
    for (let i = 0; i < keys.length; i++) {
      let key = keys[i];
      if (VERBOSE) debug("key: " + key + ", val: " + lock.queuedSets[key] + ", type: " + typeof(lock.queuedSets[key]));
      let checkDefer = {};
      let checkPromise = new Promise(function(resolve, reject) {
        checkDefer.resolve = resolve;
        checkDefer.reject = reject;
      });

      // Get operation is used to fill in the default value, assuming there is
      // one. For the moment, if a value doesn't exist in the settings DB, we
      // allow the user to add it, and just pass back a null default value.
      let checkKeyRequest = store.get(key);
      checkKeyRequest.onsuccess = function (event) {
        let userValue = lock.queuedSets[key];
        let defaultValue;
        if (!event.target.result) {
          defaultValue = null;
          if (VERBOSE) debug("MOZSETTINGS-GET-WARNING: " + key + " is not in the database.\n");
        } else {
          defaultValue = event.target.result.defaultValue;
        }
        let obj = {settingName: key, defaultValue: defaultValue, userValue: userValue};
        finalValues[key] = {defaultValue: defaultValue, userValue: userValue};
        let setReq = store.put(obj);
        setReq.onsuccess = function() {
          if (VERBOSE) debug("Set successful!");
          if (VERBOSE) debug("key: " + key + ", val: " + finalValues[key] + ", type: " + typeof(finalValues[key]));
          return checkDefer.resolve({task: aTask});
        };
        setReq.onerror = function() {
          return checkDefer.reject({task: aTask, error: setReq.error.name});
        };
      }.bind(this);
      checkKeyRequest.onerror = function(event) {
        return checkDefer.reject({task: aTask, error: checkKeyRequest.error.name});
      };
      checkPromises.push(checkPromise);
    }

    let defer = {};
    let promiseWrapper = new Promise(function(resolve, reject) {
      defer.resolve = resolve;
      defer.reject = reject;
    });

    // Once all transactions are done, or any have failed, remove the lock and
    // start processing the tasks from the next lock in the queue.
    Promise.all(checkPromises).then(function() {
      // If all commits were successful, notify observers
      for (let i = 0; i < keys.length; i++) {
        this.sendSettingsChange(keys[i], finalValues[keys[i]].userValue, lock.isServiceLock);
      }
      this.removeLock(data.lockID);
      defer.resolve({task: aTask});
    }.bind(this), function(ret) {
      this.removeLock(data.lockID);
      defer.reject({task: aTask, error: "Set transaction failure"});
    }.bind(this));
    return promiseWrapper;
  },

  // Clear is only expected to be called via tests, and if a lock
  // calls clear, it should be the only thing the lock does. This
  // allows us to not have to deal with the possibility of query
  // integrity checking. Clear should never be called in the wild,
  // even by certified apps, which is why it has its own permission
  // (settings-clear).
  taskClear: function(aTask) {
    if (VERBOSE) debug("Clearing");
    let data = aTask.data;
    let lock = this.lockInfo[data.lockID];

    if (lock._failed) {
      if (DEBUG) debug("Lock failed, all requests now failing.");
      return Promise.reject({task: aTask, error: "Lock failed, all requests now failing."});
    }

    if (!lock.canClear) {
      if (DEBUG) debug("Lock tried to clear after queuing other tasks. Failing.");
      lock._failed = true;
      return Promise.reject({task: aTask, error: "Cannot call clear after queuing other tasks, all requests now failing."});
    }

    if (!SettingsPermissions.hasClearPermission(lock.principal)) {
      if (DEBUG) debug("clear not allowed");
      lock._failed = true;
      return Promise.reject({task: aTask, error: "No permission to clear DB"});
    }

    lock.hasCleared = true;

    let store = lock.getObjectStore(lock.principal);
    if (!store) {
      if (DEBUG) debug("Rejecting Clear task on lock " + aTask.data.lockID);
      return Promise.reject({task: aTask, error: "Cannot get object store"});
    }
    let defer = {};
    let promiseWrapper = new Promise(function(resolve, reject) {
      defer.resolve = resolve;
      defer.reject = reject;
    });

    let clearReq = store.clear();
    clearReq.onsuccess = function() {
      return defer.resolve({task: aTask});
    };
    clearReq.onerror = function() {
      return defer.reject({task: aTask});
    };
    return promiseWrapper;
  },

  ensureConnection : function() {
    if (VERBOSE) debug("Ensuring Connection");
    let defer = {};
    let promiseWrapper = new Promise(function(resolve, reject) {
      defer.resolve = resolve;
      defer.reject = reject;
    });
    this.settingsDB.ensureDB(
      function() { defer.resolve(); },
      function(error) {
        if (DEBUG) debug("Cannot open Settings DB. Trying to open an old version?\n");
        defer.reject(error);
      }
    );
    return promiseWrapper;
  },

  runTasks: function(aLockID) {
    if (VERBOSE) debug("Running tasks for " + aLockID);
    let lock = this.lockInfo[aLockID];
    if (!lock) {
      if (DEBUG) debug("Lock no longer alive, cannot run tasks");
      return;
    }
    let currentTask = lock.tasks.shift();
    let promises = [];
    if (TRACK) {
      if (this.tasksConsumed[aLockID] === undefined) {
        this.tasksConsumed[aLockID] = 0;
        this.tasksGetConsumed[aLockID] = 0;
        this.tasksSetConsumed[aLockID] = 0;
      }
    }
    while (currentTask) {
      if (VERBOSE) debug("Running Operation " + currentTask.operation);
      if (lock.finalizing) {
        // We should really never get to this point, but if we do,
        // fail every task that happens.
        Cu.reportError("Settings lock trying to run more tasks after finalizing. Ignoring tasks, but this is bad. Lock: " + aLockID);
        currentTask.defer.reject("Cannot call new task after finalizing");
      } else {
      let p;
      this.totalProcessed++;
      if (TRACK) {
        this.tasksConsumed[aLockID]++;
      }
      switch (currentTask.operation) {
        case "get":
          this.totalGetProcessed++;
          if (TRACK) {
            this.tasksGetConsumed[aLockID]++;
          }
          p = this.taskGet(currentTask);
          break;
        case "set":
          this.totalSetProcessed++;
          if (TRACK) {
            this.tasksSetConsumed[aLockID]++;
          }
          p = this.taskSet(currentTask);
          break;
        case "clear":
          p = this.taskClear(currentTask);
          break;
        case "finalize":
          p = this.finalizeSets(currentTask);
          break;
        default:
          if (DEBUG) debug("Invalid operation: " + currentTask.operation);
          p.reject("Invalid operation: " + currentTask.operation);
      }
      p.then(function(ret) {
        ret.task.defer.resolve("results" in ret ? ret.results : null);
      }.bind(currentTask), function(ret) {
        ret.task.defer.reject(ret.error);
      });
      promises.push(p);
      }
      currentTask = lock.tasks.shift();
    }
  },

  consumeTasks: function() {
    if (this.settingsLockQueue.length == 0) {
      if (VERBOSE) debug("Nothing to run!");
      return;
    }

    let lockID = this.settingsLockQueue[0];
    if (VERBOSE) debug("Consuming tasks for " + lockID);
    let lock = this.lockInfo[lockID];

    // If a process dies, we should clean up after it via the
    // child-process-shutdown event. But just in case we don't, we want to make
    // sure we never block on consuming.
    if (!lock) {
      if (DEBUG) debug("Lock no longer alive, cannot consume tasks");
      this.queueConsume();
      return;
    }

    if (!lock.consumable || lock.tasks.length === 0) {
      if (VERBOSE) debug("No more tasks to run or not yet consuamble.");
      return;
    }

    lock.consumable = false;
    this.ensureConnection().then(
      function(task) {
        this.runTasks(lockID);
        this.updateSoftLockup(lockID);
      }.bind(this), function(ret) {
        dump("-*- SettingsRequestManager: SETTINGS DATABASE ERROR: Cannot make DB connection!\n");
    });
  },

  observe: function(aSubject, aTopic, aData) {
    if (VERBOSE) debug("observe: " + aTopic);
    switch (aTopic) {
      case kXpcomShutdownObserverTopic:
        this.messages.forEach((function(msgName) {
          ppmm.removeMessageListener(msgName, this);
        }).bind(this));
        Services.obs.removeObserver(this, kXpcomShutdownObserverTopic);
        ppmm = null;
        mrm.unregisterStrongReporter(this);
        break;

      case kInnerWindowDestroyed:
        let wId = aSubject.QueryInterface(Ci.nsISupportsPRUint64).data;
        this.forceFinalizeChildLocksNonOOP(wId);
        break;

      default:
        if (DEBUG) debug("Wrong observer topic: " + aTopic);
        break;
    }
  },

  collectReports: function(aCallback, aData, aAnonymize) {
    for (let lockId of Object.keys(this.lockInfo)) {
      let lock = this.lockInfo[lockId];
      let length = lock.tasks.length;

      if (length === 0) {
        continue;
      }

      let path = "settings-locks/tasks/lock(id=" + lockId + ")/";

      aCallback.callback("", path + "alive",
                         Ci.nsIMemoryReporter.KIND_OTHER,
                         Ci.nsIMemoryReporter.UNITS_COUNT,
                         length,
                         "Alive tasks for this lock",
                         aData);
    }

    aCallback.callback("",
                       "settings-locks/tasks-total/processed",
                       Ci.nsIMemoryReporter.KIND_OTHER,
                       Ci.nsIMemoryReporter.UNITS_COUNT,
                       this.totalProcessed,
                       "The total number of tasks that were executed.",
                       aData);

    aCallback.callback("",
                       "settings-locks/tasks-total/set",
                       Ci.nsIMemoryReporter.KIND_OTHER,
                       Ci.nsIMemoryReporter.UNITS_COUNT,
                       this.totalSetProcessed,
                       "The total number of set tasks that were executed.",
                       aData);

    aCallback.callback("",
                       "settings-locks/tasks-total/get",
                       Ci.nsIMemoryReporter.KIND_OTHER,
                       Ci.nsIMemoryReporter.UNITS_COUNT,
                       this.totalGetProcessed,
                       "The total number of get tasks that were executed.",
                       aData);

    // if TRACK is not enabled, then, no details are available
    if (!TRACK) {
      return;
    }

    for (let lockId of Object.keys(this.tasksConsumed)) {
      let lock = this.lockInfo[lockId];
      let length = 0;
      if (lock) {
        length = lock.tasks.length;
      }

      let path = "settings-locks/tasks/lock(id=" + lockId + ")/";

      aCallback.callback("", path + "set",
                         Ci.nsIMemoryReporter.KIND_OTHER,
                         Ci.nsIMemoryReporter.UNITS_COUNT,
                         this.tasksSetConsumed[lockId],
                         "Set tasks for this lock.",
                         aData);

      aCallback.callback("", path + "get",
                         Ci.nsIMemoryReporter.KIND_OTHER,
                         Ci.nsIMemoryReporter.UNITS_COUNT,
                         this.tasksGetConsumed[lockId],
                         "Get tasks for this lock.",
                         aData);

      aCallback.callback("", path + "processed",
                         Ci.nsIMemoryReporter.KIND_OTHER,
                         Ci.nsIMemoryReporter.UNITS_COUNT,
                         this.tasksConsumed[lockId],
                         "Number of tasks that were executed.",
                         aData);
    }
  },

  sendSettingsChange: function(aKey, aValue, aIsServiceLock) {
    this.broadcastMessage("Settings:Change:Return:OK",
      { key: aKey, value: aValue });
    var setting = {
      key: aKey,
      value: aValue,
      isInternalChange: aIsServiceLock
    };
    setting.wrappedJSObject = setting;
    Services.obs.notifyObservers(setting, kMozSettingsChangedObserverTopic, "");
  },

  broadcastMessage: function broadcastMessage(aMsgName, aContent) {
    if (VERBOSE) debug("Broadcast");
    this.children.forEach(function(msgMgr) {
      let principal = this.observerPrincipalCache.get(msgMgr);
      if (!principal) {
        if (DEBUG) debug("Cannot find principal for message manager to check permissions");
      }
      else if (SettingsPermissions.hasReadPermission(principal, aContent.key)) {
        try {
          msgMgr.sendAsyncMessage(aMsgName, aContent);
        } catch (e) {
          if (DEBUG) debug("Failed sending message: " + aMsgName);
        }
      }
    }.bind(this));
    if (VERBOSE) debug("Finished Broadcasting");
  },

  addObserver: function(aMsgMgr, aPrincipal) {
    if (VERBOSE) debug("Add observer for " + aPrincipal.origin);
    if (this.children.indexOf(aMsgMgr) == -1) {
      this.children.push(aMsgMgr);
      this.observerPrincipalCache.set(aMsgMgr, aPrincipal);
    }
  },

  removeObserver: function(aMsgMgr) {
    if (VERBOSE) {
      let principal = this.observerPrincipalCache.get(aMsgMgr);
      if (principal) {
        debug("Remove observer for " + principal.origin);
      }
    }
    let index = this.children.indexOf(aMsgMgr);
    if (index != -1) {
      this.children.splice(index, 1);
      this.observerPrincipalCache.delete(aMsgMgr);
    }
    if (VERBOSE) debug("Principal/MessageManager pairs left in observer cache: " + this.observerPrincipalCache.size);
  },

  removeLock: function(aLockID) {
    if (VERBOSE) debug("Removing lock " + aLockID);
    if (this.lockInfo[aLockID]) {
      let transaction = this.lockInfo[aLockID]._transaction;
      if (transaction) {
        try {
          transaction.abort();
        } catch (e) {
          if (e.name == "InvalidStateError") {
            if (VERBOSE) debug("Transaction for " + aLockID + " closed already");
          } else {
            if (DEBUG) debug("Unexpected exception, throwing: " + e);
            throw e;
          }
        }
      }
      delete this.lockInfo[aLockID];
    }
    let index = this.settingsLockQueue.indexOf(aLockID);
    if (index > -1) {
      this.settingsLockQueue.splice(index, 1);
    }
    // If index is 0, the lock we just removed was at the head of
    // the queue, so possibly queue the next lock if it's
    // consumable.
    if (index == 0) {
      this.queueConsume();
    }
  },

  hasLockFinalizeTask: function(lock) {
    // Go in reverse order because finalize should be the last one
    for (let task_index = lock.tasks.length; task_index >= 0; task_index--) {
      if (lock.tasks[task_index]
          && lock.tasks[task_index].operation === "finalize") {
        return true;
      }
    }
    return false;
  },

  enqueueForceFinalize: function(lock) {
    if (!this.hasLockFinalizeTask(lock)) {
      if (VERBOSE) debug("Alive lock has pending tasks: " + lock.lockID);
      this.queueTask("finalize", {lockID: lock.lockID}).then(
        function() {
          if (VERBOSE) debug("Alive lock " + lock.lockID + " succeeded to force-finalize");
        },
        function(error) {
          if (DEBUG) debug("Alive lock " + lock.lockID + " failed to force-finalize due to error: " + error);
        }
      );
      // Finalize is considered a task running situation, but it also needs to
      // queue a task.
      this.startRunning(lock.lockID);
    }
  },

  forceFinalizeChildLocksNonOOP: function(windowId) {
    if (VERBOSE) debug("Forcing finalize on child locks, non OOP");

    for (let lockId of Object.keys(this.lockInfo)) {
      let lock = this.lockInfo[lockId];
      if (lock.windowID === windowId) {
        this.enqueueForceFinalize(lock);
      }
    }
  },

  forceFinalizeChildLocksOOP: function(aMsgMgr) {
    if (VERBOSE) debug("Forcing finalize on child locks, OOP");

    for (let lockId of Object.keys(this.lockInfo)) {
      let lock = this.lockInfo[lockId];
      if (lock._mm === aMsgMgr) {
        this.enqueueForceFinalize(lock);
      }
    }
  },

  updateSoftLockup: function(aLockId) {
    if (VERBOSE) debug("Treating lock " + aLockId + ", so updating soft lockup infos ...");

    this.softLockup = {
      lockId: aLockId,
      lockTs: new Date()
    };
  },

  checkSoftLockup: function() {
    if (VERBOSE) debug("Checking for soft lockup ...");

    if (this.settingsLockQueue.length === 0) {
      if (VERBOSE) debug("Empty settings lock queue, no soft lockup ...");
      return;
    }

    let head = this.settingsLockQueue[0];
    if (head !== this.softLockup.lockId) {
      if (VERBOSE) debug("Non matching head of settings lock queue, no soft lockup ...");
      return;
    }

    let delta = (new Date() - this.softLockup.lockTs) / 1000;
    if (delta < kSoftLockupDelta) {
      if (VERBOSE) debug("Matching head of settings lock queue, but delta (" + delta + ") < 30 secs, no soft lockup ...");
      return;
    }

    let msgBlocked = "Settings queue head blocked at " + head +
                     " for " + delta + " secs, Settings API may be soft lockup. Lock from: " +
                     this.lockInfo[head].lockStack;
    Cu.reportError(msgBlocked);
    if (DEBUG) debug(msgBlocked);
  },

  receiveMessage: function(aMessage) {
    if (VERBOSE) debug("receiveMessage " + aMessage.name + ": " + JSON.stringify(aMessage.data));

    let msg = aMessage.data;
    let mm = aMessage.target;

    function returnMessage(name, data) {
      if (mm) {
        try {
          mm.sendAsyncMessage(name, data);
        } catch (e) {
          if (DEBUG) debug("Return message failed, " + name + ": " + e);
        }
      } else {
        try {
          gSettingsService.receiveMessage({ name: name, data: data });
	} catch (e) {
          if (DEBUG) debug("Direct return message failed, " + name + ": " + e);
	}
      }
    }

    // For all message types that expect a lockID, we check to make
    // sure that we're accessing a lock that's part of our process. If
    // not, consider it a security violation and kill the app. Killing
    // based on creating a colliding lock ID happens as part of
    // CreateLock check below.
    switch (aMessage.name) {
      case "Settings:Get":
      case "Settings:Set":
      case "Settings:Clear":
      case "Settings:Run":
      case "Settings:Finalize":
        this.checkSoftLockup();
        let kill_process = false;
        if (!msg.lockID) {
          Cu.reportError("Process sending request for lock that does not exist. Killing.");
          kill_process = true;
        }
        else if (!this.lockInfo[msg.lockID]) {
          if (DEBUG) debug("Cannot find lock ID " + msg.lockID);
          // This doesn't kill, because we can have things that file
          // finalize, then die, and we may get the observer
          // notification before we get the IPC messages.
          return;
        }
        else if (mm != this.lockInfo[msg.lockID]._mm) {
          Cu.reportError("Process trying to access settings lock from another process. Killing.");
          kill_process = true;
        }
        if (kill_process) {
          // Kill the app by checking for a non-existent permission
          aMessage.target.assertPermission("message-manager-mismatch-kill");
          return;
        }
      default:
      break;
    }

    switch (aMessage.name) {
      case "child-process-shutdown":
        if (VERBOSE) debug("Child process shutdown received.");
        this.forceFinalizeChildLocksOOP(mm);
        this.removeObserver(mm);
        break;
      case "Settings:RegisterForMessages":
        if (!SettingsPermissions.hasSomeReadPermission(aMessage.principal)) {
          Cu.reportError("Settings message " + aMessage.name +
                         " from a content process with no 'settings-api-read' privileges.");
          aMessage.target.assertPermission("message-manager-no-read-kill");
          return;
        }
        this.addObserver(mm, aMessage.principal);
        break;
      case "Settings:UnregisterForMessages":
        this.removeObserver(mm);
        break;
      case "Settings:CreateLock":
        if (VERBOSE) debug("Received CreateLock for " + msg.lockID + " from " + aMessage.principal.origin + " window: " + msg.windowID);
        // If we try to create a lock ID that collides with one
        // already in the system, consider it a security violation and
        // kill.
        if (msg.lockID in this.settingsLockQueue) {
          Cu.reportError("Trying to queue a lock with the same ID as an already queued lock. Killing app.");
          aMessage.target.assertPermission("lock-id-duplicate-kill");
          return;
        }

        if (this.softLockup.lockId === null) {
          this.updateSoftLockup(msg.lockID);
        }

        this.settingsLockQueue.push(msg.lockID);
        this.lockInfo[msg.lockID] = SettingsLockInfo(this.settingsDB,
                                                     mm,
                                                     aMessage.principal,
                                                     msg.lockID,
                                                     msg.isServiceLock,
                                                     msg.windowID,
                                                     msg.lockStack);
        break;
      case "Settings:Get":
        if (VERBOSE) debug("Received getRequest from " + msg.lockID);
        this.queueTask("get", msg).then(function(settings) {
            returnMessage("Settings:Get:OK", {
              lockID: msg.lockID,
              requestID: msg.requestID,
              settings: settings
            });
          }, function(error) {
            if (DEBUG) debug("getRequest FAILED " + msg.name);
            returnMessage("Settings:Get:KO", {
              lockID: msg.lockID,
              requestID: msg.requestID,
              errorMsg: error
            });
        });
        break;
      case "Settings:Set":
        if (VERBOSE) debug("Received Set Request from " + msg.lockID);
        this.queueTask("set", msg).then(function(settings) {
          returnMessage("Settings:Set:OK", {
            lockID: msg.lockID,
            requestID: msg.requestID
          });
        }, function(error) {
          returnMessage("Settings:Set:KO", {
            lockID: msg.lockID,
            requestID: msg.requestID,
            errorMsg: error
          });
        });
        break;
      case "Settings:Clear":
        if (VERBOSE) debug("Received Clear Request from " + msg.lockID);
        this.queueTask("clear", msg).then(function() {
          returnMessage("Settings:Clear:OK", {
            lockID: msg.lockID,
            requestID: msg.requestID
          });
        }, function(error) {
          returnMessage("Settings:Clear:KO", {
            lockID: msg.lockID,
            requestID: msg.requestID,
            errorMsg: error
          });
        });
        break;
      case "Settings:Finalize":
        if (VERBOSE) debug("Received Finalize");
        this.queueTask("finalize", msg).then(function() {
          returnMessage("Settings:Finalize:OK", {
            lockID: msg.lockID
          });
        }, function(error) {
          returnMessage("Settings:Finalize:KO", {
            lockID: msg.lockID,
            errorMsg: error
          });
        });
      // YES THIS IS SUPPOSED TO FALL THROUGH. Finalize is considered a task
      // running situation, but it also needs to queue a task.
      case "Settings:Run":
        if (VERBOSE) debug("Received Run");
        this.startRunning(msg.lockID);
        break;
      default:
        if (DEBUG) debug("Wrong message: " + aMessage.name);
    }
  }
};

this.SettingsRequestManager = SettingsRequestManager;
SettingsRequestManager.init();
