/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const DEBUG = false;
function debug(s) {
  dump("-*- NotificationStorage.js: " + s + "\n");
}

const kMessageNotificationGetAllOk = "Notification:GetAll:Return:OK";
const kMessageNotificationGetAllKo = "Notification:GetAll:Return:KO";
const kMessageNotificationSaveKo = "Notification:Save:Return:KO";
const kMessageNotificationDeleteKo = "Notification:Delete:Return:KO";

const kMessages = [
  kMessageNotificationGetAllOk,
  kMessageNotificationGetAllKo,
  kMessageNotificationSaveKo,
  kMessageNotificationDeleteKo,
];

export class NotificationStorage {
  #requests = {};
  #requestCount = 0;

  constructor() {
    Services.obs.addObserver(this, "xpcom-shutdown");

    // Register for message listeners.
    this.registerListeners();
  }

  registerListeners() {
    for (let message of kMessages) {
      Services.cpmm.addMessageListener(message, this);
    }
  }

  unregisterListeners() {
    for (let message of kMessages) {
      Services.cpmm.removeMessageListener(message, this);
    }
  }

  observe(aSubject, aTopic) {
    if (DEBUG) {
      debug("Topic: " + aTopic);
    }
    if (aTopic === "xpcom-shutdown") {
      Services.obs.removeObserver(this, "xpcom-shutdown");
      this.unregisterListeners();
    }
  }

  put(
    origin,
    id,
    title,
    dir,
    lang,
    body,
    tag,
    icon,
    alertName,
    data,
    behavior,
    serviceWorkerRegistrationScope
  ) {
    if (DEBUG) {
      debug("PUT: " + origin + " " + id + ": " + title);
    }
    var notification = {
      id,
      title,
      dir,
      lang,
      body,
      tag,
      icon,
      alertName,
      timestamp: new Date().getTime(),
      origin,
      data,
      mozbehavior: behavior,
      serviceWorkerRegistrationScope,
    };

    Services.cpmm.sendAsyncMessage("Notification:Save", {
      origin,
      notification,
    });
  }

  get(origin, tag, callback) {
    if (DEBUG) {
      debug("GET: " + origin + " " + tag);
    }
    this.#fetchFromDB(origin, tag, callback);
  }

  delete(origin, id) {
    if (DEBUG) {
      debug("DELETE: " + id);
    }
    Services.cpmm.sendAsyncMessage("Notification:Delete", {
      origin,
      id,
    });
  }

  receiveMessage(message) {
    var request = this.#requests[message.data.requestID];

    switch (message.name) {
      case kMessageNotificationGetAllOk:
        delete this.#requests[message.data.requestID];
        this.#returnNotifications(message.data.notifications, request.callback);
        break;

      case kMessageNotificationGetAllKo:
        delete this.#requests[message.data.requestID];
        try {
          request.callback.done();
        } catch (e) {
          debug("Error calling callback done: " + e);
        }
        break;
      case kMessageNotificationSaveKo:
      case kMessageNotificationDeleteKo:
        if (DEBUG) {
          debug(
            "Error received when treating: '" +
              message.name +
              "': " +
              message.data.errorMsg
          );
        }
        break;

      default:
        if (DEBUG) {
          debug("Unrecognized message: " + message.name);
        }
        break;
    }
  }

  #fetchFromDB(origin, tag, callback) {
    var request = {
      origin,
      tag,
      callback,
    };
    var requestID = this.#requestCount++;
    this.#requests[requestID] = request;
    Services.cpmm.sendAsyncMessage("Notification:GetAll", {
      origin,
      tag,
      requestID,
    });
  }

  #returnNotifications(notifications, callback) {
    // Pass each notification back separately.
    // The callback is called asynchronously to match the behaviour when
    // fetching from the database.
    notifications.forEach(function (notification) {
      try {
        Services.tm.dispatchToMainThread(
          callback.handle.bind(
            callback,
            notification.id,
            notification.title,
            notification.dir,
            notification.lang,
            notification.body,
            notification.tag,
            notification.icon,
            notification.data,
            notification.mozbehavior,
            notification.serviceWorkerRegistrationScope
          )
        );
      } catch (e) {
        if (DEBUG) {
          debug("Error calling callback handle: " + e);
        }
      }
    });
    try {
      Services.tm.dispatchToMainThread(callback.done);
    } catch (e) {
      if (DEBUG) {
        debug("Error calling callback done: " + e);
      }
    }
  }

  QueryInterface = ChromeUtils.generateQI(["nsINotificationStorage"]);
}
