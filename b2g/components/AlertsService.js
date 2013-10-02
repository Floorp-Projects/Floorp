/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cu = Components.utils;
const Cc = Components.classes;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gSystemMessenger",
                                   "@mozilla.org/system-message-internal;1",
                                   "nsISystemMessagesInternal");

XPCOMUtils.defineLazyServiceGetter(this, "uuidGenerator",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

XPCOMUtils.defineLazyGetter(this, "cpmm", function() {
  return Cc["@mozilla.org/childprocessmessagemanager;1"]
           .getService(Ci.nsIMessageSender);
});

function debug(str) {
  dump("=*= AlertsService.js : " + str + "\n");
}

// -----------------------------------------------------------------------
// Alerts Service
// -----------------------------------------------------------------------

function AlertsService() {
  cpmm.addMessageListener("app-notification-return", this);
}

AlertsService.prototype = {
  classID: Components.ID("{fe33c107-82a4-41d6-8c64-5353267e04c9}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAlertsService,
                                         Ci.nsIAppNotificationService]),

  // nsIAlertsService
  showAlertNotification: function showAlertNotification(aImageUrl,
                                                        aTitle,
                                                        aText,
                                                        aTextClickable,
                                                        aCookie,
                                                        aAlertListener,
                                                        aName,
                                                        aBidi,
                                                        aLang) {
    let browser = Services.wm.getMostRecentWindow("navigator:browser");
    browser.AlertsHelper.showAlertNotification(aImageUrl, aTitle, aText,
                                               aTextClickable, aCookie,
                                               aAlertListener, aName, aBidi,
                                               aLang);
  },

  closeAlert: function(aName) {
    let browser = Services.wm.getMostRecentWindow("navigator:browser");
    browser.AlertsHelper.closeAlert(aName);
  },

  // nsIAppNotificationService
  showAppNotification: function showAppNotification(aImageURL,
                                                    aTitle,
                                                    aText,
                                                    aAlertListener,
                                                    aDetails) {
    let uid = (aDetails.id == "") ?
          "app-notif-" + uuidGenerator.generateUUID() : aDetails.id;

    this._listeners[uid] = {
      observer: aAlertListener,
      title: aTitle,
      text: aText,
      manifestURL: aDetails.manifestURL,
      imageURL: aImageURL
    };

    cpmm.sendAsyncMessage("app-notification-send", {
      imageURL: aImageURL,
      title: aTitle,
      text: aText,
      uid: uid,
      details: aDetails
    });
  },

  // AlertsService.js custom implementation
  _listeners: [],

  receiveMessage: function receiveMessage(aMessage) {
    let data = aMessage.data;
    let listener = this._listeners[data.uid];
    if (aMessage.name !== "app-notification-return" || !listener) {
      return;
    }

    let topic = data.topic;

    try {
      listener.observer.observe(null, topic, null);
    } catch (e) {
      // It seems like there is no callbacks anymore, forward the click on
      // notification via a system message containing the title/text/icon of
      // the notification so the app get a change to react.
      if (data.target) {
        gSystemMessenger.sendMessage("notification", {
            title: listener.title,
            body: listener.text,
            imageURL: listener.imageURL
          },
          Services.io.newURI(data.target, null, null),
          Services.io.newURI(listener.manifestURL, null, null));
      }
    }

    // we're done with this notification
    if (topic === "alertfinished") {
      delete this._listeners[data.uid];
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([AlertsService]);
