/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    prefix: "NotificationStorage",
    maxLogLevelPref: "dom.webnotifications.loglevel",
  });
});

const kMessageGetAllOk = "GetAll:Return:OK";
const kMessageGetAllKo = "GetAll:Return:KO";
const kMessageSaveKo = "Save:Return:KO";
const kMessageDeleteKo = "Delete:Return:KO";

export class NotificationStorage {
  #requests = {};
  #requestCount = 0;

  constructor() {
    Services.obs.addObserver(this, "xpcom-shutdown");

    // Register for message listeners.
    this.registerListeners();
  }

  storageQualifier() {
    return "Notification";
  }

  prefixStorageQualifier(message) {
    return `${this.storageQualifier()}:${message}`;
  }

  formatMessageType(message) {
    return this.prefixStorageQualifier(message);
  }

  supportedMessages() {
    return [
      this.formatMessageType(kMessageGetAllOk),
      this.formatMessageType(kMessageGetAllKo),
      this.formatMessageType(kMessageSaveKo),
      this.formatMessageType(kMessageDeleteKo),
    ];
  }

  registerListeners() {
    for (let message of this.supportedMessages()) {
      Services.cpmm.addMessageListener(message, this);
    }
  }

  unregisterListeners() {
    for (let message of this.supportedMessages()) {
      Services.cpmm.removeMessageListener(message, this);
    }
  }

  observe(aSubject, aTopic) {
    lazy.console.debug(`Topic: ${aTopic}`);
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
    lazy.console.debug(`PUT: ${origin} ${id}: ${title}`);
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

    Services.cpmm.sendAsyncMessage(this.formatMessageType("Save"), {
      origin,
      notification,
    });
  }

  get(origin, tag, callback) {
    lazy.console.debug(`GET: ${origin} ${tag}`);
    this.#fetchFromDB(origin, tag, callback);
  }

  delete(origin, id) {
    lazy.console.debug(`DELETE: ${id}`);
    Services.cpmm.sendAsyncMessage(this.formatMessageType("Delete"), {
      origin,
      id,
    });
  }

  receiveMessage(message) {
    var request = this.#requests[message.data.requestID];

    switch (message.name) {
      case this.formatMessageType(kMessageGetAllOk):
        delete this.#requests[message.data.requestID];
        this.#returnNotifications(message.data.notifications, request.callback);
        break;

      case this.formatMessageType(kMessageGetAllKo):
        delete this.#requests[message.data.requestID];
        try {
          request.callback.done();
        } catch (e) {
          lazy.console.debug(`Error calling callback done: ${e}`);
        }
        break;
      case this.formatMessageType(kMessageSaveKo):
      case this.formatMessageType(kMessageDeleteKo):
        lazy.console.debug(
          `Error received when treating: '${message.name}': ${message.data.errorMsg}`
        );
        break;

      default:
        lazy.console.debug(`Unrecognized message: ${message.name}`);
        break;
    }
  }

  #getUniqueRequestID() {
    // This assumes the count will never go above MAX_SAFE_INTEGER, as
    // notifications are not supposed to happen that frequently.
    this.#requestCount += 1;
    return this.#requestCount;
  }

  #fetchFromDB(origin, tag, callback) {
    var request = {
      origin,
      tag,
      callback,
    };
    var requestID = this.#getUniqueRequestID();
    this.#requests[requestID] = request;
    Services.cpmm.sendAsyncMessage(this.formatMessageType("GetAll"), {
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
        lazy.console.debug(`Error calling callback handle: ${e}`);
      }
    });
    try {
      Services.tm.dispatchToMainThread(callback.done);
    } catch (e) {
      lazy.console.debug(`Error calling callback done: ${e}`);
    }
  }

  QueryInterface = ChromeUtils.generateQI(["nsINotificationStorage"]);
}

export class MemoryNotificationStorage extends NotificationStorage {
  storageQualifier() {
    return "MemoryNotification";
  }
}
