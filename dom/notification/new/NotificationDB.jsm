/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [];

const DEBUG = false;
function debug(s) { dump("-*- NotificationDB component: " + s + "\n"); }

ChromeUtils.defineModuleGetter(this, "FileUtils", "resource://gre/modules/FileUtils.jsm");
ChromeUtils.defineModuleGetter(this, "KeyValueService", "resource://gre/modules/kvstore.jsm");
ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
ChromeUtils.defineModuleGetter(this, "Services", "resource://gre/modules/Services.jsm");

const kMessages = [
  "Notification:Save",
  "Notification:Delete",
  "Notification:GetAll",
];

// Given its origin and ID, produce the key that uniquely identifies
// a notification.
function makeKey(origin, id) {
  return origin.concat("\t", id);
}

var NotificationDB = {

  // Ensure we won't call init() while xpcom-shutdown is performed
  _shutdownInProgress: false,

  // A handle to the kvstore, retrieved lazily when we load the data.
  _store: null,

  // A promise that resolves once the store has been loaded.
  // The promise doesn't resolve to a value; it merely captures the state
  // of the load via its resolution.
  _loadPromise: null,

  init() {
    if (this._shutdownInProgress) {
      return;
    }

    this.tasks = []; // read/write operation queue
    this.runningTask = null;

    Services.obs.addObserver(this, "xpcom-shutdown");
    this.registerListeners();
  },

  registerListeners() {
    for (let message of kMessages) {
      Services.ppmm.addMessageListener(message, this);
    }
  },

  unregisterListeners() {
    for (let message of kMessages) {
      Services.ppmm.removeMessageListener(message, this);
    }
  },

  observe(aSubject, aTopic, aData) {
    if (DEBUG) debug("Topic: " + aTopic);
    if (aTopic == "xpcom-shutdown") {
      this._shutdownInProgress = true;
      Services.obs.removeObserver(this, "xpcom-shutdown");
      this.unregisterListeners();
    }
  },

  filterNonAppNotifications(notifications) {
    for (let origin in notifications) {
      let persistentNotificationCount = 0;
      for (let id in notifications[origin]) {
        if (notifications[origin][id].serviceWorkerRegistrationScope) {
          persistentNotificationCount++;
        } else {
          delete notifications[origin][id];
        }
      }
      if (persistentNotificationCount == 0) {
        if (DEBUG) debug("Origin " + origin + " is not linked to an app manifest, deleting.");
        delete notifications[origin];
      }
    }

    return notifications;
  },

  async maybeMigrateData() {
    // We avoid using OS.File until we know we're going to migrate data
    // to avoid the performance cost of loading that module.
    const oldStore = FileUtils.getFile("ProfD", ["notificationstore.json"]);

    if (!oldStore.exists()) {
      if (DEBUG) { debug("Old store doesn't exist; not migrating data."); }
      return;
    }

    let data;
    try {
      data = await OS.File.read(oldStore.path, { encoding: "utf-8"});
    } catch (ex) {
      // If read failed, we assume we have no notifications to migrate.
      if (DEBUG) { debug("Failed to read old store; not migrating data."); }
      return;
    } finally {
      // Finally, delete the old file so we don't try to migrate it again.
      await OS.File.remove(oldStore.path);
    }

    if (data.length > 0) {
      // Preprocessing phase intends to cleanly separate any migration-related
      // tasks.
      //
      // NB: This code existed before we migrated the data to a kvstore,
      // and the "migration-related tasks" it references are from an earlier
      // migration.  We used to do it every time we read the JSON file;
      // now we do it once, when migrating the JSON file to the kvstore.
      const notifications = this.filterNonAppNotifications(JSON.parse(data));

      // Copy the data from the JSON file to the kvstore.
      // TODO: use a transaction to improve the performance of these operations
      // once the kvstore API supports it (bug 1515096).
      for (const origin in notifications) {
        for (const id in notifications[origin]) {
          await this._store.put(makeKey(origin, id),
            JSON.stringify(notifications[origin][id]));
        }
      }
    }
  },

  // Attempt to read notification file, if it's not there we will create it.
  async load() {
    // Get and cache a handle to the kvstore.
    const dir = FileUtils.getDir("ProfD", ["notificationstore"], true);
    this._store = await KeyValueService.getOrCreate(dir.path, "notifications");

    // Migrate data from the old JSON file to the new kvstore if the old file
    // is present in the user's profile directory.
    await this.maybeMigrateData();
  },

  // Helper function: promise will be resolved once file exists and/or is loaded.
  ensureLoaded() {
    if (!this._loadPromise) {
      this._loadPromise = this.load();
    }
    return this._loadPromise;
  },

  receiveMessage(message) {
    if (DEBUG) { debug("Received message:" + message.name); }

    // sendAsyncMessage can fail if the child process exits during a
    // notification storage operation, so always wrap it in a try/catch.
    function returnMessage(name, data) {
      try {
        message.target.sendAsyncMessage(name, data);
      } catch (e) {
        if (DEBUG) { debug("Return message failed, " + name); }
      }
    }

    switch (message.name) {
      case "Notification:GetAll":
        this.queueTask("getall", message.data).then(function(notifications) {
          returnMessage("Notification:GetAll:Return:OK", {
            requestID: message.data.requestID,
            origin: message.data.origin,
            notifications,
          });
        }).catch(function(error) {
          returnMessage("Notification:GetAll:Return:KO", {
            requestID: message.data.requestID,
            origin: message.data.origin,
            errorMsg: error,
          });
        });
        break;

      case "Notification:Save":
        this.queueTask("save", message.data).then(function() {
          returnMessage("Notification:Save:Return:OK", {
            requestID: message.data.requestID,
          });
        }).catch(function(error) {
          returnMessage("Notification:Save:Return:KO", {
            requestID: message.data.requestID,
            errorMsg: error,
          });
        });
        break;

      case "Notification:Delete":
        this.queueTask("delete", message.data).then(function() {
          returnMessage("Notification:Delete:Return:OK", {
            requestID: message.data.requestID,
          });
        }).catch(function(error) {
          returnMessage("Notification:Delete:Return:KO", {
            requestID: message.data.requestID,
            errorMsg: error,
          });
        });
        break;

      default:
        if (DEBUG) { debug("Invalid message name" + message.name); }
    }
  },

  // We need to make sure any read/write operations are atomic,
  // so use a queue to run each operation sequentially.
  queueTask(operation, data) {
    if (DEBUG) { debug("Queueing task: " + operation); }

    var defer = {};

    this.tasks.push({ operation, data, defer });

    var promise = new Promise(function(resolve, reject) {
      defer.resolve = resolve;
      defer.reject = reject;
    });

    // Only run immediately if we aren't currently running another task.
    if (!this.runningTask) {
      if (DEBUG) { debug("Task queue was not running, starting now..."); }
      this.runNextTask();
    }

    return promise;
  },

  runNextTask() {
    if (this.tasks.length === 0) {
      if (DEBUG) { debug("No more tasks to run, queue depleted"); }
      this.runningTask = null;
      return;
    }
    this.runningTask = this.tasks.shift();

    // Always make sure we are loaded before performing any read/write tasks.
    this.ensureLoaded()
    .then(() => {
      var task = this.runningTask;

      switch (task.operation) {
        case "getall":
          return this.taskGetAll(task.data);

        case "save":
          return this.taskSave(task.data);

        case "delete":
          return this.taskDelete(task.data);
      }

      throw new Error(`Unknown task operation: ${task.operation}`);
    })
    .then(payload => {
      if (DEBUG) {
        debug("Finishing task: " + this.runningTask.operation);
      }
      this.runningTask.defer.resolve(payload);
    })
    .catch(err => {
      if (DEBUG) {
        debug("Error while running " + this.runningTask.operation + ": " + err);
      }
      this.runningTask.defer.reject(err);
    })
    .then(() => {
      this.runNextTask();
    });
  },

  enumerate(origin) {
    // The "from" and "to" key parameters to nsIKeyValueStore.enumerate()
    // are inclusive and exclusive, respectively, and keys are tuples
    // of origin and ID joined by a tab (\t), which is character code 9;
    // so enumerating ["origin", "origin\n"), where the line feed (\n)
    // is character code 10, enumerates all pairs with the given origin.
    return this._store.enumerate(origin, `${origin}\n`);
  },

  async taskGetAll(data) {
    if (DEBUG) { debug("Task, getting all"); }
    var origin = data.origin;
    var notifications = [];

    for (const {value} of await this.enumerate(origin)) {
      notifications.push(JSON.parse(value));
    }

    if (data.tag) {
      notifications = notifications.filter(n => n.tag === data.tag);
    }

    return notifications;
  },

  async taskSave(data) {
    if (DEBUG) { debug("Task, saving"); }
    var origin = data.origin;
    var notification = data.notification;

    // We might have existing notification with this tag,
    // if so we need to remove it before saving the new one.
    if (notification.tag) {
      for (const {key, value} of await this.enumerate(origin)) {
        const oldNotification = JSON.parse(value);
        if (oldNotification.tag === notification.tag) {
          await this._store.delete(key);
        }
      }
    }

    await this._store.put(makeKey(origin, notification.id),
      JSON.stringify(notification));
  },

  async taskDelete(data) {
    if (DEBUG) { debug("Task, deleting"); }
    await this._store.delete(makeKey(data.origin, data.id));
  },
};

NotificationDB.init();
