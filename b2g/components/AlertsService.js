/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cu = Components.utils;
const Cc = Components.classes;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "cpmm", function() {
  return Cc["@mozilla.org/childprocessmessagemanager;1"]
           .getService(Ci.nsIMessageSender);
});

// -----------------------------------------------------------------------
// Alerts Service
// -----------------------------------------------------------------------

function AlertsService() {
  cpmm.addMessageListener("app-notification-return", this);
  this._id = 0;
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
                                                        aName) {
    let browser = Services.wm.getMostRecentWindow("navigator:browser");
    browser.AlertsHelper.showAlertNotification(aImageUrl, aTitle, aText,
                                               aTextClickable, aCookie,
                                               aAlertListener, aName);
  },

  // nsIAppNotificationService
  _listeners: [],

  receiveMessage: function receiveMessage(aMessage) {
    let data = aMessage.data;
    if (aMessage.name !== "app-notification-return" ||
        !this._listeners[data.id]) {
      return;
    }

    let obs = this._listeners[data.id];
    let topic = data.type == "desktop-notification-click" ? "alertclickcallback"
                                                          : "alertfinished";
    obs.observe(null, topic, null);

    // we're done with this notification
    if (topic === "alertfinished")
      delete this._listeners[data.id];
  },

  // This method is called in the content process, so we remote the call
  // to shell.js
  showAppNotification: function showAppNotification(aImageURL,
                                                    aTitle,
                                                    aText,
                                                    aTextClickable,
                                                    aManifestURL,
                                                    aAlertListener) {
    let id = "app-notif" + this._id++;
    this._listeners[id] = aAlertListener;
    cpmm.sendAsyncMessage("app-notification-send", {
      imageURL: aImageURL,
      title: aTitle,
      text: aText,
      textClickable: aTextClickable,
      manifestURL: aManifestURL,
      id: id
    });
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([AlertsService]);
