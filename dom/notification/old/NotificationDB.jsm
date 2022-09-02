/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [];

const DEBUG = false;
function debug(s) {
  dump("-*- NotificationDB component: " + s + "\n");
}

const NOTIFICATION_STORE_DIR = PathUtils.profileDir;
const NOTIFICATION_STORE_PATH = PathUtils.join(
  NOTIFICATION_STORE_DIR,
  "notificationstore.json"
);

const kMessages = [
  "Notification:Save",
  "Notification:Delete",
  "Notification:GetAll",
];

var NotificationDB = {
  // Ensure we won't call init() while xpcom-shutdown is performed
  _shutdownInProgress: false,

  init() {
    if (this._shutdownInProgress) {
      return;
    }

    this.notifications = Object.create(null);
    this.byTag = Object.create(null);
    this.loaded = false;

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
    if (DEBUG) {
      debug("Topic: " + aTopic);
    }
    if (aTopic == "xpcom-shutdown") {
      this._shutdownInProgress = true;
      Services.obs.removeObserver(this, "xpcom-shutdown");
      this.unregisterListeners();
    }
  },

  filterNonAppNotifications(notifications) {
    let result = Object.create(null);
    for (let origin in notifications) {
      result[origin] = Object.create(null);
      let persistentNotificationCount = 0;
      for (let id in notifications[origin]) {
        if (notifications[origin][id].serviceWorkerRegistrationScope) {
          persistentNotificationCount++;
          result[origin][id] = notifications[origin][id];
        }
      }
      if (persistentNotificationCount == 0) {
        if (DEBUG) {
          debug(
            "Origin " + origin + " is not linked to an app manifest, deleting."
          );
        }
        delete result[origin];
      }
    }

    return result;
  },

  // Attempt to read notification file, if it's not there we will create it.
  load() {
    var promise = IOUtils.readUTF8(NOTIFICATION_STORE_PATH);
    return promise.then(
      data => {
        if (data.length) {
          // Preprocessing phase intends to cleanly separate any migration-related
          // tasks.
          this.notifications = this.filterNonAppNotifications(JSON.parse(data));
        }

        // populate the list of notifications by tag
        if (this.notifications) {
          for (var origin in this.notifications) {
            this.byTag[origin] = Object.create(null);
            for (var id in this.notifications[origin]) {
              var curNotification = this.notifications[origin][id];
              if (curNotification.tag) {
                this.byTag[origin][curNotification.tag] = curNotification;
              }
            }
          }
        }

        this.loaded = true;
      },

      // If read failed, we assume we have no notifications to load.
      reason => {
        this.loaded = true;
        return this.createStore();
      }
    );
  },

  // Creates the notification directory.
  createStore() {
    var promise = IOUtils.makeDirectory(NOTIFICATION_STORE_DIR, {
      ignoreExisting: true,
    });
    return promise.then(this.createFile.bind(this));
  },

  // Creates the notification file once the directory is created.
  createFile() {
    return IOUtils.writeUTF8(NOTIFICATION_STORE_PATH, "", {
      tmpPath: NOTIFICATION_STORE_PATH + ".tmp",
    });
  },

  // Save current notifications to the file.
  save() {
    var data = JSON.stringify(this.notifications);
    return IOUtils.writeUTF8(NOTIFICATION_STORE_PATH, data, {
      tmpPath: NOTIFICATION_STORE_PATH + ".tmp",
    });
  },

  // Helper function: promise will be resolved once file exists and/or is loaded.
  ensureLoaded() {
    if (!this.loaded) {
      return this.load();
    }
    return Promise.resolve();
  },

  receiveMessage(message) {
    if (DEBUG) {
      debug("Received message:" + message.name);
    }

    // sendAsyncMessage can fail if the child process exits during a
    // notification storage operation, so always wrap it in a try/catch.
    function returnMessage(name, data) {
      try {
        message.target.sendAsyncMessage(name, data);
      } catch (e) {
        if (DEBUG) {
          debug("Return message failed, " + name);
        }
      }
    }

    switch (message.name) {
      case "Notification:GetAll":
        this.queueTask("getall", message.data)
          .then(function(notifications) {
            returnMessage("Notification:GetAll:Return:OK", {
              requestID: message.data.requestID,
              origin: message.data.origin,
              notifications,
            });
          })
          .catch(function(error) {
            returnMessage("Notification:GetAll:Return:KO", {
              requestID: message.data.requestID,
              origin: message.data.origin,
              errorMsg: error,
            });
          });
        break;

      case "Notification:Save":
        this.queueTask("save", message.data)
          .then(function() {
            returnMessage("Notification:Save:Return:OK", {
              requestID: message.data.requestID,
            });
          })
          .catch(function(error) {
            returnMessage("Notification:Save:Return:KO", {
              requestID: message.data.requestID,
              errorMsg: error,
            });
          });
        break;

      case "Notification:Delete":
        this.queueTask("delete", message.data)
          .then(function() {
            returnMessage("Notification:Delete:Return:OK", {
              requestID: message.data.requestID,
            });
          })
          .catch(function(error) {
            returnMessage("Notification:Delete:Return:KO", {
              requestID: message.data.requestID,
              errorMsg: error,
            });
          });
        break;

      default:
        if (DEBUG) {
          debug("Invalid message name" + message.name);
        }
    }
  },

  // We need to make sure any read/write operations are atomic,
  // so use a queue to run each operation sequentially.
  queueTask(operation, data) {
    if (DEBUG) {
      debug("Queueing task: " + operation);
    }

    var defer = {};

    this.tasks.push({
      operation,
      data,
      defer,
    });

    var promise = new Promise(function(resolve, reject) {
      defer.resolve = resolve;
      defer.reject = reject;
    });

    // Only run immediately if we aren't currently running another task.
    if (!this.runningTask) {
      if (DEBUG) {
        debug("Task queue was not running, starting now...");
      }
      this.runNextTask();
    }

    return promise;
  },

  runNextTask() {
    if (this.tasks.length === 0) {
      if (DEBUG) {
        debug("No more tasks to run, queue depleted");
      }
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

          default:
            return Promise.reject(
              new Error(`Found a task with unknown operation ${task.operation}`)
            );
        }
      })
      .then(payload => {
        if (DEBUG) {
          debug("Finishing task: " + this.runningTask.operation);
        }
        this.runningTask.defer.resolve(payload);
      })
      .catch(err => {
        if (DEBUG) {
          debug(
            "Error while running " + this.runningTask.operation + ": " + err
          );
        }
        this.runningTask.defer.reject(err);
      })
      .then(() => {
        this.runNextTask();
      });
  },

  taskGetAll(data) {
    if (DEBUG) {
      debug("Task, getting all");
    }
    var origin = data.origin;
    var notifications = [];
    // Grab only the notifications for specified origin.
    if (this.notifications[origin]) {
      if (data.tag) {
        let n;
        if ((n = this.byTag[origin][data.tag])) {
          notifications.push(n);
        }
      } else {
        for (var i in this.notifications[origin]) {
          notifications.push(this.notifications[origin][i]);
        }
      }
    }
    return Promise.resolve(notifications);
  },

  taskSave(data) {
    if (DEBUG) {
      debug("Task, saving");
    }
    var origin = data.origin;
    var notification = data.notification;
    if (!this.notifications[origin]) {
      this.notifications[origin] = Object.create(null);
      this.byTag[origin] = Object.create(null);
    }

    // We might have existing notification with this tag,
    // if so we need to remove it before saving the new one.
    if (notification.tag) {
      var oldNotification = this.byTag[origin][notification.tag];
      if (oldNotification) {
        delete this.notifications[origin][oldNotification.id];
      }
      this.byTag[origin][notification.tag] = notification;
    }

    this.notifications[origin][notification.id] = notification;
    return this.save();
  },

  taskDelete(data) {
    if (DEBUG) {
      debug("Task, deleting");
    }
    var origin = data.origin;
    var id = data.id;
    if (!this.notifications[origin]) {
      if (DEBUG) {
        debug("No notifications found for origin: " + origin);
      }
      return Promise.resolve();
    }

    // Make sure we can find the notification to delete.
    var oldNotification = this.notifications[origin][id];
    if (!oldNotification) {
      if (DEBUG) {
        debug("No notification found with id: " + id);
      }
      return Promise.resolve();
    }

    if (oldNotification.tag) {
      delete this.byTag[origin][oldNotification.tag];
    }
    delete this.notifications[origin][id];
    return this.save();
  },
};

NotificationDB.init();
