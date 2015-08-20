/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict'

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

function debug(s) {
  //dump('DEBUG RequestSyncService: ' + s + '\n');
}

const RSYNCDB_VERSION = 1;
const RSYNCDB_NAME = "requestSync";
const RSYNC_MIN_INTERVAL = 100;

const RSYNC_OPERATION_TIMEOUT = 120000 // 2 minutes

const RSYNC_STATE_ENABLED = "enabled";
const RSYNC_STATE_DISABLED = "disabled";
const RSYNC_STATE_WIFIONLY = "wifiOnly";

Cu.import("resource://gre/modules/BrowserUtils.jsm");
Cu.import('resource://gre/modules/IndexedDBHelper.jsm');
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.importGlobalProperties(["indexedDB"]);


XPCOMUtils.defineLazyServiceGetter(this, "appsService",
                                   "@mozilla.org/AppsService;1",
                                   "nsIAppsService");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsISyncMessageSender");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageBroadcaster");

XPCOMUtils.defineLazyServiceGetter(this, "systemMessenger",
                                   "@mozilla.org/system-message-internal;1",
                                   "nsISystemMessagesInternal");

XPCOMUtils.defineLazyServiceGetter(this, "secMan",
                                   "@mozilla.org/scriptsecuritymanager;1",
                                   "nsIScriptSecurityManager");

XPCOMUtils.defineLazyModuleGetter(this, "AlarmService",
                                  "resource://gre/modules/AlarmService.jsm");

this.RequestSyncService = {
  __proto__: IndexedDBHelper.prototype,

  children: [],

  _messages: [ "RequestSync:Register",
               "RequestSync:Unregister",
               "RequestSync:Registrations",
               "RequestSync:Registration",
               "RequestSyncManager:Registrations",
               "RequestSyncManager:SetPolicy",
               "RequestSyncManager:RunTask" ],

  _pendingOperation: false,
  _pendingMessages: [],

  _registrations: {},

  _wifi: false,

  _activeTask: null,
  _queuedTasks: [],

  _timers: {},
  _pendingRequests: {},

  // This array contains functions to be executed after the scheduling of the
  // current task or immediately if there are not scheduling in progress.
  _afterSchedulingTasks: [],

  // Initialization of the RequestSyncService.
  init: function() {
    debug("init");

    this._messages.forEach((function(msgName) {
      ppmm.addMessageListener(msgName, this);
    }).bind(this));

    Services.obs.addObserver(this, 'xpcom-shutdown', false);
    Services.obs.addObserver(this, 'clear-origin-data', false);
    Services.obs.addObserver(this, 'wifi-state-changed', false);

    this.initDBHelper("requestSync", RSYNCDB_VERSION, [RSYNCDB_NAME]);

    // Loading all the data from the database into the _registrations map.
    // Any incoming message will be stored and processed when the async
    // operation is completed.

    this.dbTxn("readonly", function(aStore) {
      aStore.openCursor().onsuccess = function(event) {
        let cursor = event.target.result;
        if (cursor) {
          this.addRegistration(cursor.value, cursor.continue);
        }
      }
    }.bind(this),
    function() {
      debug("initialization done");
    },
    function() {
      dump("ERROR!! RequestSyncService - Failed to retrieve data from the database.\n");
    });
  },

  // Shutdown the RequestSyncService.
  shutdown: function() {
    debug("shutdown");

    this._messages.forEach((function(msgName) {
      ppmm.removeMessageListener(msgName, this);
    }).bind(this));

    Services.obs.removeObserver(this, 'xpcom-shutdown');
    Services.obs.removeObserver(this, 'clear-origin-data');
    Services.obs.removeObserver(this, 'wifi-state-changed');

    this.close();

    // Removing all the registrations will delete the pending timers.
    this.forEachRegistration(function(aObj) {
      let key = this.principalToKey(aObj.principal);
      this.removeRegistrationInternal(aObj.data.task, key);
    }.bind(this));
  },

  observe: function(aSubject, aTopic, aData) {
    debug("observe");

    switch (aTopic) {
      case 'xpcom-shutdown':
        this.executeAfterScheduling(function() {
          this.shutdown();
        }.bind(this));
        break;

      case 'clear-origin-data':
        this.executeAfterScheduling(function() {
          this.clearData(aData);
        }.bind(this));
        break;

      case 'wifi-state-changed':
        this.executeAfterScheduling(function() {
          this.wifiStateChanged(aSubject == 'enabled');
        }.bind(this));
        break;

      default:
        debug("Wrong observer topic: " + aTopic);
        break;
    }
  },

  // When an app is uninstalled, we have to clean all its tasks.
  clearData: function(aData) {
    debug('clearData');

    if (!aData) {
      return;
    }

    let pattern = JSON.parse(aData);
    let dbKeys = [];

    for (let key in this._registrations) {
      let prin = BrowserUtils.principalFromOrigin(key);
      if (!ChromeUtils.originAttributesMatchPattern(prin.originAttributes, pattern)) {
        continue;
      }

      for (let task in this._registrations[key]) {
        dbKeys = this._registrations[key][task].dbKey;
        this.removeRegistrationInternal(task, key);
      }
    }

    if (dbKeys.length == 0) {
      return;
    }

    // Remove the tasks from the database.
    this.dbTxn('readwrite', function(aStore) {
      for (let i = 0; i < dbKeys.length; ++i) {
        aStore.delete(dbKeys[i]);
      }
    },
    function() {
      debug("ClearData completed");
    }, function() {
      debug("ClearData failed");
    });
  },

  // Creation of the schema for the database.
  upgradeSchema: function(aTransaction, aDb, aOldVersion, aNewVersion) {
    debug('updateSchema');
    aDb.createObjectStore(RSYNCDB_NAME, { autoIncrement: true });
  },

  // This method generates the key for the indexedDB object storage.
  principalToKey: function(aPrincipal) {
    return aPrincipal.origin;
  },

  // Add a task to the _registrations map and create the timer if it's needed.
  addRegistration: function(aObj, aCb) {
    debug('addRegistration');

    let key = this.principalToKey(aObj.principal);
    if (!(key in this._registrations)) {
      this._registrations[key] = {};
    }

    this.scheduleTimer(aObj, function() {
      this._registrations[key][aObj.data.task] = aObj;
      if (aCb) {
        aCb();
      }
    }.bind(this));
  },

  // Remove a task from the _registrations map and delete the timer if it's
  // needed. It also checks if the principal is correct before doing the real
  // operation.
  removeRegistration: function(aTaskName, aKey, aPrincipal) {
    debug('removeRegistration');

    if (!(aKey in this._registrations) ||
        !(aTaskName in this._registrations[aKey])) {
      return false;
    }

    // Additional security check.
    if (!aPrincipal.equals(this._registrations[aKey][aTaskName].principal)) {
      return false;
    }

    this.removeRegistrationInternal(aTaskName, aKey);
    return true;
  },

  removeRegistrationInternal: function(aTaskName, aKey) {
    debug('removeRegistrationInternal');

    let obj = this._registrations[aKey][aTaskName];

    this.removeTimer(obj);

    // It can be that this task has been already schedulated.
    this.removeTaskFromQueue(obj);

    // It can be that this object is already in scheduled, or in the queue of a
    // iDB transacation. In order to avoid rescheduling it, we must disable it.
    obj.active = false;

    delete this._registrations[aKey][aTaskName];

    // Lets remove the key in case there are not tasks registered.
    for (let key in this._registrations[aKey]) {
      return;
    }
    delete this._registrations[aKey];
  },

  removeTaskFromQueue: function(aObj) {
    let pos = this._queuedTasks.indexOf(aObj);
    if (pos != -1) {
      this._queuedTasks.splice(pos, 1);
    }
  },

  // The communication from the exposed objects and the service is done using
  // messages. This function receives and processes them.
  receiveMessage: function(aMessage) {
    debug("receiveMessage");

    // We cannot process this request now.
    if (this._pendingOperation) {
      this._pendingMessages.push(aMessage);
      return;
    }

    // The principal is used to validate the message.
    let principal = aMessage.principal;
    if (!principal) {
      return;
    }

    switch (aMessage.name) {
      case "RequestSync:Register":
        this.executeAfterScheduling(function() {
          this.register(aMessage.target, aMessage.data, principal);
        }.bind(this));
        break;

      case "RequestSync:Unregister":
        this.executeAfterScheduling(function() {
          this.unregister(aMessage.target, aMessage.data, principal);
        }.bind(this));
        break;

      case "RequestSync:Registrations":
        this.executeAfterScheduling(function() {
          this.registrations(aMessage.target, aMessage.data, principal);
        }.bind(this));
        break;

      case "RequestSync:Registration":
        this.executeAfterScheduling(function() {
          this.registration(aMessage.target, aMessage.data, principal);
        }.bind(this));
        break;

      case "RequestSyncManager:Registrations":
        this.executeAfterScheduling(function() {
          this.managerRegistrations(aMessage.target, aMessage.data, principal);
        }.bind(this));
        break;

      case "RequestSyncManager:SetPolicy":
        this.executeAfterScheduling(function() {
          this.managerSetPolicy(aMessage.target, aMessage.data, principal);
        }.bind(this));
        break;

      case "RequestSyncManager:RunTask":
        this.executeAfterScheduling(function() {
          this.managerRunTask(aMessage.target, aMessage.data, principal);
        }.bind(this));
        break;

      default:
        debug("Wrong message: " + aMessage.name);
        break;
    }
  },

  // Basic validation.
  validateRegistrationParams: function(aParams) {
    if (aParams === null) {
      return false;
    }

    // We must have a page.
    if (!("wakeUpPage" in aParams) ||
        aParams.wakeUpPage.length == 0) {
      return false;
    }

    let minInterval = RSYNC_MIN_INTERVAL;
    try {
      minInterval = Services.prefs.getIntPref("dom.requestSync.minInterval");
    } catch(e) {}

    if (!("minInterval" in aParams) ||
        aParams.minInterval < minInterval) {
      return false;
    }

    return true;
  },

  // Registration of a new task.
  register: function(aTarget, aData, aPrincipal) {
    debug("register");

    if (!this.validateRegistrationParams(aData.params)) {
      aTarget.sendAsyncMessage("RequestSync:Register:Return",
                               { requestID: aData.requestID,
                                 error: "ParamsError" } );
      return;
    }

    let key = this.principalToKey(aPrincipal);
    if (key in this._registrations &&
        aData.task in this._registrations[key]) {
      // if this task already exists we overwrite it.
      this.removeRegistrationInternal(aData.task, key);
    }

    // This creates a RequestTaskFull object.
    aData.params.task = aData.task;
    aData.params.lastSync = 0;
    aData.params.principal = aPrincipal;

    aData.params.state = RSYNC_STATE_ENABLED;
    if (aData.params.wifiOnly) {
      aData.params.state = RSYNC_STATE_WIFIONLY;
    }

    aData.params.overwrittenMinInterval = 0;

    let dbKey = aData.task + "|" + key;

    let data = { principal: aPrincipal,
                 dbKey: dbKey,
                 data: aData.params,
                 active: true };

    let self = this;
    this.dbTxn('readwrite', function(aStore) {
      aStore.put(data, data.dbKey);
    },
    function() {
      self.addRegistration(data, function() {
        aTarget.sendAsyncMessage("RequestSync:Register:Return",
                                 { requestID: aData.requestID });
      });
    },
    function() {
      aTarget.sendAsyncMessage("RequestSync:Register:Return",
                               { requestID: aData.requestID,
                                 error: "IndexDBError" } );
    });
  },

  // Unregister a task.
  unregister: function(aTarget, aData, aPrincipal) {
    debug("unregister");

    let key = this.principalToKey(aPrincipal);
    if (!(key in this._registrations) ||
        !(aData.task in this._registrations[key])) {
      aTarget.sendAsyncMessage("RequestSync:Unregister:Return",
                               { requestID: aData.requestID,
                                 error: "UnknownTaskError" });
      return;
    }

    let dbKey = this._registrations[key][aData.task].dbKey;
    this.removeRegistration(aData.task, key, aPrincipal);

    let self = this;
    this.dbTxn('readwrite', function(aStore) {
      aStore.delete(dbKey);
    },
    function() {
      aTarget.sendAsyncMessage("RequestSync:Unregister:Return",
                               { requestID: aData.requestID });
    },
    function() {
      aTarget.sendAsyncMessage("RequestSync:Unregister:Return",
                               { requestID: aData.requestID,
                                 error: "IndexDBError" } );
    });
  },

  // Get the list of registered tasks for this principal.
  registrations: function(aTarget, aData, aPrincipal) {
    debug("registrations");

    let results = [];
    let key = this.principalToKey(aPrincipal);
    if (key in this._registrations) {
      for (let i in this._registrations[key]) {
        results.push(this.createPartialTaskObject(
          this._registrations[key][i].data));
      }
    }

    aTarget.sendAsyncMessage("RequestSync:Registrations:Return",
                             { requestID: aData.requestID,
                               results: results });
  },

  // Get a particular registered task for this principal.
  registration: function(aTarget, aData, aPrincipal) {
    debug("registration");

    let results = null;
    let key = this.principalToKey(aPrincipal);
    if (key in this._registrations &&
        aData.task in this._registrations[key]) {
      results = this.createPartialTaskObject(
        this._registrations[key][aData.task].data);
    }

    aTarget.sendAsyncMessage("RequestSync:Registration:Return",
                             { requestID: aData.requestID,
                               results: results });
  },

  // Get the list of the registered tasks.
  managerRegistrations: function(aTarget, aData, aPrincipal) {
    debug("managerRegistrations");

    let results = [];
    let self = this;
    this.forEachRegistration(function(aObj) {
      results.push(self.createFullTaskObject(aObj.data));
    });

    aTarget.sendAsyncMessage("RequestSyncManager:Registrations:Return",
                             { requestID: aData.requestID,
                               results: results });
  },

  // Set a policy to a task.
  managerSetPolicy: function(aTarget, aData, aPrincipal) {
    debug("managerSetPolicy");

    let toSave = null;
    let self = this;
    this.forEachRegistration(function(aObj) {
      if (aObj.data.task != aData.task) {
        return;
      }

      if (aObj.principal.isInBrowserElement != aData.isInBrowserElement ||
          aObj.principal.originNoSuffix != aData.origin) {
        return;
      }

      let app = appsService.getAppByLocalId(aObj.principal.appId);
      if (app && app.manifestURL != aData.manifestURL ||
          (!app && aData.manifestURL != "")) {
        return;
      }

      if ("overwrittenMinInterval" in aData) {
        aObj.data.overwrittenMinInterval = aData.overwrittenMinInterval;
      }

      aObj.data.state = aData.state;

      if (toSave) {
        dump("ERROR!! RequestSyncService - SetPolicy matches more than 1 task.\n");
        return;
      }

      toSave = aObj;
    });

    if (!toSave) {
      aTarget.sendAsyncMessage("RequestSyncManager:SetPolicy:Return",
                               { requestID: aData.requestID, error: "UnknownTaskError" });
      return;
    }

    this.updateObjectInDB(toSave, function() {
      self.scheduleTimer(toSave, function() {
        aTarget.sendAsyncMessage("RequestSyncManager:SetPolicy:Return",
                                 { requestID: aData.requestID });
      });
    });
  },

  // Run a task now.
  managerRunTask: function(aTarget, aData, aPrincipal) {
    debug("runTask");

    let task = null;
    this.forEachRegistration(function(aObj) {
      if (aObj.data.task != aData.task) {
        return;
      }

      if (aObj.principal.isInBrowserElement != aData.isInBrowserElement ||
          aObj.principal.originNoSuffix != aData.origin) {
        return;
      }

      let app = appsService.getAppByLocalId(aObj.principal.appId);
      if (app && app.manifestURL != aData.manifestURL ||
          (!app && aData.manifestURL != "")) {
        return;
      }

      if (task) {
        dump("ERROR!! RequestSyncService - RunTask matches more than 1 task.\n");
        return;
      }

      task = aObj;
    });

    if (!task) {
      aTarget.sendAsyncMessage("RequestSyncManager:RunTask:Return",
                               { requestID: aData.requestID, error: "UnknownTaskError" });
      return;
    }

    // Storing the requestID into the task for the callback.
    this.storePendingRequest(task, aTarget, aData.requestID);
    this.timeout(task);
  },

  // We cannot expose the full internal object to content but just a subset.
  // This method creates this subset.
  createPartialTaskObject: function(aObj) {
    return { task: aObj.task,
             lastSync: aObj.lastSync,
             oneShot: aObj.oneShot,
             minInterval: aObj.minInterval,
             wakeUpPage: aObj.wakeUpPage,
             wifiOnly: aObj.wifiOnly,
             data: aObj.data };
  },

  createFullTaskObject: function(aObj) {
    let obj = this.createPartialTaskObject(aObj);

    obj.app = { manifestURL: '',
                origin: aObj.principal.originNoSuffix,
                isInBrowserElement: aObj.principal.isInBrowserElement };

    let app = appsService.getAppByLocalId(aObj.principal.appId);
    if (app) {
      obj.app.manifestURL = app.manifestURL;
    }

    obj.state = aObj.state;
    obj.overwrittenMinInterval = aObj.overwrittenMinInterval;
    return obj;
  },

  // Creation of the timer for a particular task object.
  scheduleTimer: function(aObj, aCb) {
    debug("scheduleTimer");

    aCb = aCb || function() {};

    this.removeTimer(aObj);

    // A  registration can be already inactive if it was 1 shot.
    if (!aObj.active) {
      aCb();
      return;
    }

    if (aObj.data.state == RSYNC_STATE_DISABLED) {
      aCb();
      return;
    }

    // WifiOnly check.
    if (aObj.data.state == RSYNC_STATE_WIFIONLY && !this._wifi) {
      aCb();
      return;
    }

    if (this.scheduling) {
      dump("ERROR!! RequestSyncService - ScheduleTimer called into ScheduleTimer.\n");
      aCb();
      return;
    }

    this.scheduling = true;

    this.createTimer(aObj, function() {
      this.scheduling = false;

      while (this._afterSchedulingTasks.length) {
        var cb = this._afterSchedulingTasks.shift();
        cb();
      }

      aCb();
    }.bind(this));
  },

  executeAfterScheduling: function(aCb) {
    if (!this.scheduling) {
      aCb();
      return;
    }

    this._afterSchedulingTasks.push(aCb);
  },

  timeout: function(aObj) {
    debug("timeout");

    if (this._activeTask) {
      debug("queueing tasks");
      // We have an active task, let's queue this as next task.
      if (this._queuedTasks.indexOf(aObj) == -1) {
        this._queuedTasks.push(aObj);
      }
      return;
    }

    let app = appsService.getAppByLocalId(aObj.principal.appId);
    if (!app) {
      dump("ERROR!! RequestSyncService - Failed to retrieve app data from a principal.\n");
      aObj.active = false;
      this.updateObjectInDB(aObj);
      return;
    }

    let manifestURL = Services.io.newURI(app.manifestURL, null, null);
    let pageURL = Services.io.newURI(aObj.data.wakeUpPage, null, aObj.principal.URI);

    // Maybe need to be rescheduled?
    if (this.hasPendingMessages('request-sync', manifestURL, pageURL)) {
      this.scheduleTimer(aObj);
      return;
    }

    this.removeTimer(aObj);
    this._activeTask = aObj;

    if (!manifestURL || !pageURL) {
      dump("ERROR!! RequestSyncService - Failed to create URI for the page or the manifest\n");
      aObj.active = false;
      this.updateObjectInDB(aObj);
      return;
    }

    // We don't want to run more than 1 task at the same time. We do this using
    // the promise created by sendMessage(). But if the task takes more than
    // RSYNC_OPERATION_TIMEOUT millisecs, we have to ignore the promise and
    // continue processing other tasks.

    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

    let done = false;
    let self = this;
    function taskCompleted() {
      debug("promise or timeout for task calls taskCompleted");

      if (!done) {
        done = true;
        self.operationCompleted();
      }

      timer.cancel();
      timer = null;
    }

    let timeout = RSYNC_OPERATION_TIMEOUT;
    try {
      let tmp = Services.prefs.getIntPref("dom.requestSync.maxTaskTimeout");
      timeout = tmp;
    } catch(e) {}

    timer.initWithCallback(function() {
      debug("Task is taking too much, let's ignore the promise.");
      taskCompleted();
    }, timeout, Ci.nsITimer.TYPE_ONE_SHOT);

    // Sending the message.
    debug("Sending message.");
    let promise =
      systemMessenger.sendMessage('request-sync',
                                  this.createPartialTaskObject(aObj.data),
                                  pageURL, manifestURL);

    promise.then(function() {
      debug("promise resolved");
      taskCompleted();
    }, function() {
      debug("promise rejected");
      taskCompleted();
    });
  },

  operationCompleted: function() {
    debug("operationCompleted");

    if (!this._activeTask) {
      dump("ERROR!! RequestSyncService - OperationCompleted called without an active task\n");
      return;
    }

    // One shot? Then this is not active.
    this._activeTask.active = !this._activeTask.data.oneShot;
    this._activeTask.data.lastSync = new Date();

    let pendingRequests = this.stealPendingRequests(this._activeTask);
    for (let i = 0; i < pendingRequests.length; ++i) {
      pendingRequests[i]
          .target.sendAsyncMessage("RequestSyncManager:RunTask:Return",
                                   { requestID: pendingRequests[i].requestID });
    }

    this.updateObjectInDB(this._activeTask, function() {
      if (!this._activeTask.data.oneShot) {
        this.scheduleTimer(this._activeTask, function() {
          this.processNextTask();
        }.bind(this));
      } else {
        this.processNextTask();
      }
    }.bind(this));
  },

  processNextTask: function() {
    debug("processNextTask");

    this._activeTask = null;

    if (this._queuedTasks.length == 0) {
      return;
    }

    let task = this._queuedTasks.shift();
    this.timeout(task);
  },

  hasPendingMessages: function(aMessageName, aManifestURL, aPageURL) {
    let hasPendingMessages =
      cpmm.sendSyncMessage("SystemMessageManager:HasPendingMessages",
                           { type: aMessageName,
                             pageURL: aPageURL.spec,
                             manifestURL: aManifestURL.spec })[0];

    debug("Pending messages: " + hasPendingMessages);
    return hasPendingMessages;
  },

  // Update the object into the database.
  updateObjectInDB: function(aObj, aCb) {
    debug("updateObjectInDB");

    this.dbTxn('readwrite', function(aStore) {
      aStore.put(aObj, aObj.dbKey);
    },
    function() {
      if (aCb) {
        aCb();
      }
      debug("UpdateObjectInDB completed");
    }, function() {
      debug("UpdateObjectInDB failed");
    });
  },

  pendingOperationStarted: function() {
    debug('pendingOperationStarted');
    this._pendingOperation = true;
  },

  pendingOperationDone: function() {
    debug('pendingOperationDone');

    this._pendingOperation = false;

    // managing the pending messages now that the initialization is completed.
    while (this._pendingMessages.length && !this._pendingOperation) {
      this.receiveMessage(this._pendingMessages.shift());
    }
  },

  // This method creates a transaction and runs callbacks. Plus it manages the
  // pending operations system.
  dbTxn: function(aType, aCb, aSuccessCb, aErrorCb) {
    debug('dbTxn');

    this.pendingOperationStarted();

    let self = this;
    this.newTxn(aType, RSYNCDB_NAME, function(aTxn, aStore) {
      aCb(aStore);
    },
    function() {
      self.pendingOperationDone();
      aSuccessCb();
    },
    function() {
      self.pendingOperationDone();
      aErrorCb();
    });
  },

  forEachRegistration: function(aCb) {
    // This method is used also to remove registations from the map, so we have
    // to make a new list and let _registations free to be used.
    let list = [];
    for (let key in this._registrations) {
      for (let task in this._registrations[key]) {
        list.push(this._registrations[key][task]);
      }
    }

    for (let i = 0; i < list.length; ++i) {
      aCb(list[i]);
    }
  },

  wifiStateChanged: function(aEnabled) {
    debug("onWifiStateChanged");

    this._wifi = aEnabled;

    if (!this._wifi) {
      // Disable all the wifiOnly tasks.
      this.forEachRegistration(function(aObj) {
        if (aObj.data.state == RSYNC_STATE_WIFIONLY && this.hasTimer(aObj)) {
          this.removeTimer(aObj);

          // It can be that this task has been already schedulated.
          this.removeTaskFromQueue(aObj);
        }
      }.bind(this));
      return;
    }

    // Enable all the tasks.
    this.forEachRegistration(function(aObj) {
      if (aObj.active && !this.hasTimer(aObj)) {
        if (!aObj.data.wifiOnly) {
          dump("ERROR - Found a disabled task that is not wifiOnly.");
        }

        this.scheduleTimer(aObj);
      }
    }.bind(this));
  },

  createTimer: function(aObj, aCb) {
    aCb = aCb || function() {};

    let interval = aObj.data.minInterval;
    if (aObj.data.overwrittenMinInterval > 0) {
      interval = aObj.data.overwrittenMinInterval;
    }

    AlarmService.add(
      { date: new Date(Date.now() + interval * 1000),
        ignoreTimezone: false },
      () => this.timeout(aObj),
      function(aTimerId) {
        this._timers[aObj.dbKey] = aTimerId;
        aCb();
      }.bind(this),
      () => aCb());
  },

  hasTimer: function(aObj) {
    return (aObj.dbKey in this._timers);
  },

  removeTimer: function(aObj) {
    if (aObj.dbKey in this._timers) {
      AlarmService.remove(this._timers[aObj.dbKey]);
      delete this._timers[aObj.dbKey];
    }
  },

  storePendingRequest: function(aObj, aTarget, aRequestID) {
    if (!(aObj.dbKey in this._pendingRequests)) {
      this._pendingRequests[aObj.dbKey] = [];
    }

    this._pendingRequests[aObj.dbKey].push({ target: aTarget,
                                             requestID: aRequestID });
  },

  stealPendingRequests: function(aObj) {
    if (!(aObj.dbKey in this._pendingRequests)) {
      return [];
    }

    let requests = this._pendingRequests[aObj.dbKey];
    delete this._pendingRequests[aObj.dbKey];
    return requests;
  }
}

RequestSyncService.init();

this.EXPORTED_SYMBOLS = [""];
