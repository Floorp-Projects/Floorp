/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    prefix: "NotificationDB",
    maxLogLevelPref: "dom.webnotifications.loglevel",
  });
});

const NOTIFICATION_STORE_DIR = PathUtils.profileDir;
const NOTIFICATION_STORE_PATH = PathUtils.join(
  NOTIFICATION_STORE_DIR,
  "notificationstore.json"
);

export class NotificationDB {
  // Ensure we won't call init() while xpcom-shutdown is performed
  #shutdownInProgress = false;

  // A promise that resolves once the ongoing task queue has been drained.
  // The value will be reset when the queue starts again.
  #queueDrainedPromise = null;
  #queueDrainedPromiseResolve = null;

  #byTag = Object.create(null);
  #notifications = Object.create(null);
  #loaded = false;
  #tasks = [];
  #runningTask = null;

  storageQualifier() {
    return "Notification";
  }

  prefixStorageQualifier(message) {
    return `${this.storageQualifier()}:${message}`;
  }

  formatMessageType(message) {
    return this.prefixStorageQualifier(message);
  }

  supportedMessageTypes() {
    return [
      this.formatMessageType("Save"),
      this.formatMessageType("Delete"),
      this.formatMessageType("GetAll"),
    ];
  }

  constructor() {
    if (this.#shutdownInProgress) {
      return;
    }

    this.#notifications = Object.create(null);
    this.#byTag = Object.create(null);
    this.#loaded = false;

    this.#tasks = []; // read/write operation queue
    this.#runningTask = null;

    Services.obs.addObserver(this, "xpcom-shutdown");
    this.registerListeners();

    // This assumes that nothing will queue a new task at profile-change-teardown phase,
    // potentially replacing the #queueDrainedPromise if there was no existing task run.
    lazy.AsyncShutdown.profileChangeTeardown.addBlocker(
      "NotificationDB: Need to make sure that all notification messages are processed",
      () => this.#queueDrainedPromise
    );
  }

  registerListeners() {
    for (let message of this.supportedMessageTypes()) {
      Services.ppmm.addMessageListener(message, this);
    }
  }

  unregisterListeners() {
    for (let message of this.supportedMessageTypes()) {
      Services.ppmm.removeMessageListener(message, this);
    }
  }

  observe(aSubject, aTopic) {
    lazy.console.debug(`Topic: ${aTopic}`);
    if (aTopic == "xpcom-shutdown") {
      this.#shutdownInProgress = true;
      Services.obs.removeObserver(this, "xpcom-shutdown");
      this.unregisterListeners();
    }
  }

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
        lazy.console.debug(
          `Origin ${origin} is not linked to an app manifest, deleting.`
        );
        delete result[origin];
      }
    }

    return result;
  }

  // Attempt to read notification file, if it's not there we will create it.
  load() {
    var promise = IOUtils.readUTF8(NOTIFICATION_STORE_PATH);
    return promise.then(
      data => {
        if (data.length) {
          // Preprocessing phase intends to cleanly separate any migration-related
          // tasks.
          this.#notifications = this.filterNonAppNotifications(
            JSON.parse(data)
          );
        }

        // populate the list of notifications by tag
        if (this.#notifications) {
          for (var origin in this.#notifications) {
            this.#byTag[origin] = Object.create(null);
            for (var id in this.#notifications[origin]) {
              var curNotification = this.#notifications[origin][id];
              if (curNotification.tag) {
                this.#byTag[origin][curNotification.tag] = curNotification;
              }
            }
          }
        }

        this.#loaded = true;
      },

      // If read failed, we assume we have no notifications to load.
      () => {
        this.#loaded = true;
        return this.createStore();
      }
    );
  }

  // Creates the notification directory.
  createStore() {
    var promise = IOUtils.makeDirectory(NOTIFICATION_STORE_DIR, {
      ignoreExisting: true,
    });
    return promise.then(this.createFile.bind(this));
  }

  // Creates the notification file once the directory is created.
  createFile() {
    return IOUtils.writeUTF8(NOTIFICATION_STORE_PATH, "", {
      tmpPath: NOTIFICATION_STORE_PATH + ".tmp",
    });
  }

  // Save current notifications to the file.
  save() {
    var data = JSON.stringify(this.#notifications);
    return IOUtils.writeUTF8(NOTIFICATION_STORE_PATH, data, {
      tmpPath: NOTIFICATION_STORE_PATH + ".tmp",
    });
  }

  // Helper function: promise will be resolved once file exists and/or is loaded.
  #ensureLoaded() {
    if (!this.#loaded) {
      return this.load();
    }
    return Promise.resolve();
  }

  receiveMessage(message) {
    lazy.console.debug(`Received message: ${message.name}`);

    // sendAsyncMessage can fail if the child process exits during a
    // notification storage operation, so always wrap it in a try/catch.
    function returnMessage(name, data) {
      try {
        message.target.sendAsyncMessage(name, data);
      } catch (e) {
        lazy.console.debug(`Return message failed, ${name}`);
      }
    }

    switch (message.name) {
      case this.formatMessageType("GetAll"):
        this.queueTask("getall", message.data)
          .then(notifications => {
            returnMessage(this.formatMessageType("GetAll:Return:OK"), {
              requestID: message.data.requestID,
              origin: message.data.origin,
              notifications,
            });
          })
          .catch(error => {
            returnMessage(this.formatMessageType("GetAll:Return:KO"), {
              requestID: message.data.requestID,
              origin: message.data.origin,
              errorMsg: error,
            });
          });
        break;

      case this.formatMessageType("Save"):
        this.queueTask("save", message.data)
          .then(() => {
            returnMessage(this.formatMessageType("Save:Return:OK"), {
              requestID: message.data.requestID,
            });
          })
          .catch(error => {
            returnMessage(this.formatMessageType("Save:Return:KO"), {
              requestID: message.data.requestID,
              errorMsg: error,
            });
          });
        break;

      case this.formatMessageType("Delete"):
        this.queueTask("delete", message.data)
          .then(() => {
            returnMessage(this.formatMessageType("Delete:Return:OK"), {
              requestID: message.data.requestID,
            });
          })
          .catch(error => {
            returnMessage(this.formatMessageType("Delete:Return:KO"), {
              requestID: message.data.requestID,
              errorMsg: error,
            });
          });
        break;

      default:
        lazy.console.debug(`Invalid message name ${message.name}`);
    }
  }

  // We need to make sure any read/write operations are atomic,
  // so use a queue to run each operation sequentially.
  queueTask(operation, data) {
    lazy.console.debug(`Queueing task: ${operation}`);

    var defer = {};

    this.#tasks.push({
      operation,
      data,
      defer,
    });

    var promise = new Promise((resolve, reject) => {
      defer.resolve = resolve;
      defer.reject = reject;
    });

    // Only run immediately if we aren't currently running another task.
    if (!this.#runningTask) {
      lazy.console.debug("Task queue was not running, starting now...");
      this.runNextTask();
      this.#queueDrainedPromise = new Promise(resolve => {
        this.#queueDrainedPromiseResolve = resolve;
      });
    }

    return promise;
  }

  runNextTask() {
    if (this.#tasks.length === 0) {
      lazy.console.debug("No more tasks to run, queue depleted");
      this.#runningTask = null;
      if (this.#queueDrainedPromiseResolve) {
        this.#queueDrainedPromiseResolve();
      } else {
        lazy.console.debug(
          "#queueDrainedPromiseResolve was null somehow, no promise to resolve"
        );
      }
      return;
    }
    this.#runningTask = this.#tasks.shift();

    // Always make sure we are loaded before performing any read/write tasks.
    this.#ensureLoaded()
      .then(() => {
        var task = this.#runningTask;

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
        lazy.console.debug(`Finishing task: ${this.#runningTask.operation}`);
        this.#runningTask.defer.resolve(payload);
      })
      .catch(err => {
        lazy.console.debug(
          `Error while running ${this.#runningTask.operation}: ${err}`
        );
        this.#runningTask.defer.reject(err);
      })
      .then(() => {
        this.runNextTask();
      });
  }

  taskGetAll(data) {
    lazy.console.debug("Task, getting all");
    var origin = data.origin;
    var notifications = [];
    // Grab only the notifications for specified origin.
    if (this.#notifications[origin]) {
      if (data.tag) {
        let n;
        if ((n = this.#byTag[origin][data.tag])) {
          notifications.push(n);
        }
      } else {
        for (var i in this.#notifications[origin]) {
          notifications.push(this.#notifications[origin][i]);
        }
      }
    }
    return Promise.resolve(notifications);
  }

  taskSave(data) {
    lazy.console.debug("Task, saving");
    var origin = data.origin;
    var notification = data.notification;
    if (!this.#notifications[origin]) {
      this.#notifications[origin] = Object.create(null);
      this.#byTag[origin] = Object.create(null);
    }

    // We might have existing notification with this tag,
    // if so we need to remove it before saving the new one.
    if (notification.tag) {
      var oldNotification = this.#byTag[origin][notification.tag];
      if (oldNotification) {
        delete this.#notifications[origin][oldNotification.id];
      }
      this.#byTag[origin][notification.tag] = notification;
    }

    this.#notifications[origin][notification.id] = notification;
    return this.save();
  }

  taskDelete(data) {
    lazy.console.debug("Task, deleting");
    var origin = data.origin;
    var id = data.id;
    if (!this.#notifications[origin]) {
      lazy.console.debug(`No notifications found for origin: ${origin}`);
      return Promise.resolve();
    }

    // Make sure we can find the notification to delete.
    var oldNotification = this.#notifications[origin][id];
    if (!oldNotification) {
      lazy.console.debug(`No notification found with id: ${id}`);
      return Promise.resolve();
    }

    if (oldNotification.tag) {
      delete this.#byTag[origin][oldNotification.tag];
    }
    delete this.#notifications[origin][id];
    return this.save();
  }
}

new NotificationDB();
