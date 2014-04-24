/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DEBUG = false;
function debug(s) { dump("-*- NotificationStorage.js: " + s + "\n"); }

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const NOTIFICATIONSTORAGE_CID = "{37f819b0-0b5c-11e3-8ffd-0800200c9a66}";
const NOTIFICATIONSTORAGE_CONTRACTID = "@mozilla.org/notificationStorage;1";

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");


function NotificationStorage() {
  // cache objects
  this._notifications = {};
  this._byTag = {};
  this._cached = false;

  this._requests = {};
  this._requestCount = 0;

  // Register for message listeners.
  cpmm.addMessageListener("Notification:GetAll:Return:OK", this);
}

NotificationStorage.prototype = {

  put: function(origin, id, title, dir, lang, body, tag, icon) {
    if (DEBUG) { debug("PUT: " + id + ": " + title); }
    var notification = {
      id: id,
      title: title,
      dir: dir,
      lang: lang,
      body: body,
      tag: tag,
      icon: icon,
      origin: origin
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

    cpmm.sendAsyncMessage("Notification:Save", {
      origin: origin,
      notification: notification
    });
  },

  get: function(origin, tag, callback) {
    if (DEBUG) { debug("GET: " + origin + " " + tag); }
    if (this._cached) {
      this._fetchFromCache(origin, tag, callback);
    } else {
      this._fetchFromDB(origin, tag, callback);
    }
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

    cpmm.sendAsyncMessage("Notification:Delete", {
      origin: origin,
      id: id
    });
  },

  receiveMessage: function(message) {
    switch (message.name) {
      case "Notification:GetAll:Return:OK":
        var request = this._requests[message.data.requestID];
        delete this._requests[message.data.requestID];
        this._populateCache(message.data.notifications);
        this._fetchFromCache(request.origin, request.tag, request.callback);
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
    cpmm.sendAsyncMessage("Notification:GetAll", {
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
    notifications.forEach(function(notification) {
      try {
        callback.handle(notification.id,
                        notification.title,
                        notification.dir,
                        notification.lang,
                        notification.body,
                        notification.tag,
                        notification.icon);
      } catch (e) {
        if (DEBUG) { debug("Error calling callback handle: " + e); }
      }
    });
    try {
      callback.done();
    } catch (e) {
      if (DEBUG) { debug("Error calling callback done: " + e); }
    }
  },

  _populateCache: function(notifications) {
    notifications.forEach(function(notification) {
      this._notifications[notification.id] = notification;
      if (notification.tag && notification.origin) {
        let tag = notification.tag;
        let origin = notification.origin;
        if (!this._byTag[origin]) {
          this._byTag[origin] = {};
        }
        this._byTag[origin][tag] = notification;
      }
    }.bind(this));
    this._cached = true;
  },

  classID : Components.ID(NOTIFICATIONSTORAGE_CID),
  contractID : NOTIFICATIONSTORAGE_CONTRACTID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsINotificationStorage,
                                         Ci.nsIMessageListener]),
};


this.NSGetFactory = XPCOMUtils.generateNSGetFactory([NotificationStorage]);
