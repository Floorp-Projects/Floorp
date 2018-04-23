/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DEBUG = false;
function debug(s) { dump("-*- NotificationStorage.js: " + s + "\n"); }

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const NOTIFICATIONSTORAGE_CID = "{37f819b0-0b5c-11e3-8ffd-0800200c9a66}";
const NOTIFICATIONSTORAGE_CONTRACTID = "@mozilla.org/notificationStorage;1";

ChromeUtils.defineModuleGetter(this, "Services",
                               "resource://gre/modules/Services.jsm");

const kMessageNotificationGetAllOk = "Notification:GetAll:Return:OK";
const kMessageNotificationGetAllKo = "Notification:GetAll:Return:KO";
const kMessageNotificationSaveKo   = "Notification:Save:Return:KO";
const kMessageNotificationDeleteKo = "Notification:Delete:Return:KO";

const kMessages = [
  kMessageNotificationGetAllOk,
  kMessageNotificationGetAllKo,
  kMessageNotificationSaveKo,
  kMessageNotificationDeleteKo
];

function NotificationStorage() {
  // cache objects
  this._notifications = {};
  this._byTag = {};
  this._cached = false;

  this._requests = {};
  this._requestCount = 0;

  Services.obs.addObserver(this, "xpcom-shutdown");

  // Register for message listeners.
  this.registerListeners();
}

NotificationStorage.prototype = {

  registerListeners: function() {
    for (let message of kMessages) {
      Services.cpmm.addMessageListener(message, this);
    }
  },

  unregisterListeners: function() {
    for (let message of kMessages) {
      Services.cpmm.removeMessageListener(message, this);
    }
  },

  observe: function(aSubject, aTopic, aData) {
    if (DEBUG) debug("Topic: " + aTopic);
    if (aTopic === "xpcom-shutdown") {
      Services.obs.removeObserver(this, "xpcom-shutdown");
      this.unregisterListeners();
    }
  },

  put: function(origin, id, title, dir, lang, body, tag, icon, alertName,
                data, behavior, serviceWorkerRegistrationScope) {
    if (DEBUG) { debug("PUT: " + origin + " " + id + ": " + title); }
    var notification = {
      id: id,
      title: title,
      dir: dir,
      lang: lang,
      body: body,
      tag: tag,
      icon: icon,
      alertName: alertName,
      timestamp: new Date().getTime(),
      origin: origin,
      data: data,
      mozbehavior: behavior,
      serviceWorkerRegistrationScope: serviceWorkerRegistrationScope,
    };

    this._notifications[id] = notification;
    if (tag) {
      if (!this._byTag[origin]) {
        this._byTag[origin] = {};
      }

      // We might have existing notification with this tag,
      // if so we need to remove it from our cache.
      if (this._byTag[origin][tag]) {
        var oldNotification = this._byTag[origin][tag];
        delete this._notifications[oldNotification.id];
      }

      this._byTag[origin][tag] = notification;
    };

    if (serviceWorkerRegistrationScope) {
      Services.cpmm.sendAsyncMessage("Notification:Save", {
        origin: origin,
        notification: notification
      });
    }
  },

  get: function(origin, tag, callback) {
    if (DEBUG) { debug("GET: " + origin + " " + tag); }
    if (this._cached) {
      this._fetchFromCache(origin, tag, callback);
    } else {
      this._fetchFromDB(origin, tag, callback);
    }
  },

  getByID: function(origin, id, callback) {
    if (DEBUG) { debug("GETBYID: " + origin + " " + id); }
    var GetByIDProxyCallback = function(id, originalCallback) {
      this.searchID = id;
      this.originalCallback = originalCallback;
      var self = this;
      this.handle = function(id, title, dir, lang, body, tag, icon, data, behavior, serviceWorkerRegistrationScope) {
        if (id == this.searchID) {
          self.originalCallback.handle(id, title, dir, lang, body, tag, icon, data, behavior, serviceWorkerRegistrationScope);
        }
      };
      this.done = function() {
        self.originalCallback.done();
      };
    };

    return this.get(origin, "", new GetByIDProxyCallback(id, callback));
  },

  delete: function(origin, id) {
    if (DEBUG) { debug("DELETE: " + id); }
    var notification = this._notifications[id];
    if (notification) {
      if (notification.tag) {
        delete this._byTag[origin][notification.tag];
      }
      delete this._notifications[id];
    }

    Services.cpmm.sendAsyncMessage("Notification:Delete", {
      origin: origin,
      id: id
    });
  },

  receiveMessage: function(message) {
    var request = this._requests[message.data.requestID];

    switch (message.name) {
      case kMessageNotificationGetAllOk:
        delete this._requests[message.data.requestID];
        this._populateCache(message.data.notifications);
        this._fetchFromCache(request.origin, request.tag, request.callback);
        break;

      case kMessageNotificationGetAllKo:
        delete this._requests[message.data.requestID];
        try {
          request.callback.done();
        } catch (e) {
          debug("Error calling callback done: " + e);
        }
      case kMessageNotificationSaveKo:
      case kMessageNotificationDeleteKo:
        if (DEBUG) {
          debug("Error received when treating: '" + message.name +
                "': " + message.data.errorMsg);
        }
        break;

      default:
        if (DEBUG) debug("Unrecognized message: " + message.name);
        break;
    }
  },

  _fetchFromDB: function(origin, tag, callback) {
    var request = {
      origin: origin,
      tag: tag,
      callback: callback
    };
    var requestID = this._requestCount++;
    this._requests[requestID] = request;
    Services.cpmm.sendAsyncMessage("Notification:GetAll", {
      origin: origin,
      requestID: requestID
    });
  },

  _fetchFromCache: function(origin, tag, callback) {
    var notifications = [];
    // If a tag was specified and we have a notification
    // with this tag, return that. If no tag was specified
    // simple return all stored notifications.
    if (tag && this._byTag[origin] && this._byTag[origin][tag]) {
      notifications.push(this._byTag[origin][tag]);
    } else if (!tag) {
      for (var id in this._notifications) {
        if (this._notifications[id].origin === origin) {
          notifications.push(this._notifications[id]);
        }
      }
    }

    // Pass each notification back separately.
    // The callback is called asynchronously to match the behaviour when
    // fetching from the database.
    notifications.forEach(function(notification) {
      try {
        Services.tm.dispatchToMainThread(
          callback.handle.bind(callback,
                               notification.id,
                               notification.title,
                               notification.dir,
                               notification.lang,
                               notification.body,
                               notification.tag,
                               notification.icon,
                               notification.data,
                               notification.mozbehavior,
                               notification.serviceWorkerRegistrationScope));
      } catch (e) {
        if (DEBUG) { debug("Error calling callback handle: " + e); }
      }
    });
    try {
      Services.tm.dispatchToMainThread(callback.done);
    } catch (e) {
      if (DEBUG) { debug("Error calling callback done: " + e); }
    }
  },

  _populateCache: function(notifications) {
    notifications.forEach(notification => {
      this._notifications[notification.id] = notification;
      if (notification.tag && notification.origin) {
        let tag = notification.tag;
        let origin = notification.origin;
        if (!this._byTag[origin]) {
          this._byTag[origin] = {};
        }
        this._byTag[origin][tag] = notification;
      }
    });
    this._cached = true;
  },

  classID : Components.ID(NOTIFICATIONSTORAGE_CID),
  contractID : NOTIFICATIONSTORAGE_CONTRACTID,
  QueryInterface: ChromeUtils.generateQI([Ci.nsINotificationStorage]),
};


this.NSGetFactory = XPCOMUtils.generateNSGetFactory([NotificationStorage]);
