/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DEBUG = false;

function debug(s) {
  dump("-*- ChromeNotifications.js: " + s + "\n");
}

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

XPCOMUtils.defineLazyServiceGetter(this, "appNotifier",
                                   "@mozilla.org/system-alerts-service;1",
                                   "nsIAppNotificationService");

const CHROMENOTIFICATIONS_CID = "{74f94093-8b37-497e-824f-c3b250a911da}";
const CHROMENOTIFICATIONS_CONTRACTID = "@mozilla.org/mozChromeNotifications;1";

function ChromeNotifications() {
  this.innerWindowID = null;
  this.resendCallback = null;
}

ChromeNotifications.prototype = {

  init: function(aWindow) {
    let util = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIDOMWindowUtils);
    this.innerWindowID = util.currentInnerWindowID;
    Services.obs.addObserver(this, "inner-window-destroyed", false);
    cpmm.addMessageListener("Notification:GetAllCrossOrigin:Return:OK", this);
  },

  performResend: function(notifications) {
    let resentNotifications = 0;

    notifications.forEach(function(notification) {
      appNotifier.showAppNotification(
        notification.icon,
        notification.title,
        notification.body,
        null,
        {
          manifestURL: notification.origin,
          id: notification.alertName,
          dir: notification.dir,
          lang: notification.lang,
          tag: notification.tag,
          dbId: notification.id,
          timestamp: notification.timestamp,
          data: notification.data
        }
      );
      resentNotifications++;
    });

    try {
      this.resendCallback && this.resendCallback(resentNotifications);
    } catch (ex) {
      if (DEBUG) debug("Content sent exception: " + ex);
    }
  },

  mozResendAllNotifications: function(resendCallback) {
    this.resendCallback = resendCallback;
    cpmm.sendAsyncMessage("Notification:GetAllCrossOrigin", {});
  },

  receiveMessage: function(message) {
    switch (message.name) {
      case "Notification:GetAllCrossOrigin:Return:OK":
        this.performResend(message.data.notifications);
        break;

      default:
        if (DEBUG) { debug("Unrecognized message: " + message.name); }
        break;
    }
  },

  observe: function(aSubject, aTopic, aData) {
    if (DEBUG) debug("Topic: " + aTopic);
    if (aTopic == "inner-window-destroyed") {
      let wId = aSubject.QueryInterface(Ci.nsISupportsPRUint64).data;
      if (wId != this.innerWindowID) {
        return;
      }
      Services.obs.removeObserver(this, "inner-window-destroyed");
      cpmm.removeMessageListener("Notification:GetAllCrossOrigin:Return:OK", this);
    }
  },

  classID : Components.ID(CHROMENOTIFICATIONS_CID),
  contractID : CHROMENOTIFICATIONS_CONTRACTID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIChromeNotifications,
                                         Ci.nsIDOMGlobalPropertyInitializer,
                                         Ci.nsIObserver,
                                         Ci.nsIMessageListener]),
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ChromeNotifications]);
