/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Cu.import("resource://services-common/log4moz.js");

/**
 * Represents an info bar that shows a data submission notification.
 */
function DataNotificationInfoBar() {
  let log4moz = Cu.import("resource://services-common/log4moz.js", {}).Log4Moz;
  this._log = log4moz.repository.getLogger("Services.DataReporting.InfoBar");

  this._notificationBox = null;
}

DataNotificationInfoBar.prototype = {
  _OBSERVERS: [
    "datareporting:notify-data-policy:request",
    "datareporting:notify-data-policy:close",
  ],

  _DATA_REPORTING_NOTIFICATION: "data-reporting",

  init: function() {
    window.addEventListener("unload", function onUnload() {
      window.removeEventListener("unload", onUnload, false);

      for (let o of this._OBSERVERS) {
        Services.obs.removeObserver(this, o);
      }
    }.bind(this), false);

    for (let o of this._OBSERVERS) {
      Services.obs.addObserver(this, o, true);
    }
  },

  _ensureNotificationBox: function () {
    if (this._notificationBox) {
      return;
    }

    let nb = document.createElementNS(
      "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
      "notificationbox"
    );
    nb.id = "data-notification-notify-bar";
    nb.setAttribute("flex", "1");

    let bottombox = document.getElementById("browser-bottombox");
    bottombox.insertBefore(nb, bottombox.firstChild);

    this._notificationBox = nb;
  },

  _getDataReportingNotification: function (name=this._DATA_REPORTING_NOTIFICATION) {
    if (!this._notificationBox) {
      return undefined;
    }
    return this._notificationBox.getNotificationWithValue(name);
  },

  _displayDataPolicyInfoBar: function (request) {
    this._ensureNotificationBox();

    if (this._getDataReportingNotification()) {
      return;
    }

    let policy = Cc["@mozilla.org/datareporting/service;1"]
                   .getService(Ci.nsISupports)
                   .wrappedJSObject
                   .policy;

    let brandBundle = document.getElementById("bundle_brand");
    let appName = brandBundle.getString("brandShortName");
    let vendorName = brandBundle.getString("vendorShortName");

    let message = gNavigatorBundle.getFormattedString(
      "dataReportingNotification.message",
      [appName, vendorName]);

    let actionTaken = false;

    let buttons = [{
      label: gNavigatorBundle.getString("dataReportingNotification.button.label"),
      accessKey: gNavigatorBundle.getString("dataReportingNotification.button.accessKey"),
      popup: null,
      callback: function () {
        // Clicking the button to go to the preferences tab constitutes
        // acceptance of the data upload policy for Firefox Health Report.
        // This will ensure the checkbox is checked. The user has the option of
        // unchecking it.
        request.onUserAccept("info-bar-button-pressed");
        actionTaken = true;
        window.openAdvancedPreferences("dataChoicesTab");
      },
    }];

    this._log.info("Creating data reporting policy notification.");
    let notification = this._notificationBox.appendNotification(
      message,
      this._DATA_REPORTING_NOTIFICATION,
      null,
      this._notificationBox.PRIORITY_INFO_HIGH,
      buttons,
      function onEvent(event) {
        if (event == "removed") {
          if (!actionTaken) {
            request.onUserAccept("info-bar-dismissed");
          }

          Services.obs.notifyObservers(null, "datareporting:notify-data-policy:close", null);
        }
      }.bind(this)
    );

    // Keep open until user closes it.
    notification.persistence = -1;

    // Tell the notification request we have displayed the notification.
    request.onUserNotifyComplete();
  },

  _clearPolicyNotification: function () {
    let notification = this._getDataReportingNotification();
    if (notification) {
      notification.close();
    }
  },

  onNotifyDataPolicy: function (request) {
    try {
      this._displayDataPolicyInfoBar(request);
    } catch (ex) {
      request.onUserNotifyFailed(ex);
    }
  },

  observe: function(subject, topic, data) {
    switch (topic) {
      case "datareporting:notify-data-policy:request":
        this.onNotifyDataPolicy(subject.wrappedJSObject.object);
        break;

      case "datareporting:notify-data-policy:close":
        this._clearPolicyNotification();
        break;

      default:
    }
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
  ]),
};

