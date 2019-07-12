/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutProtectionsHandler"];
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { RemotePages } = ChromeUtils.import(
  "resource://gre/modules/remotepagemanager/RemotePageManagerParent.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(
  this,
  "TrackingDBService",
  "@mozilla.org/tracking-db-service;1",
  "nsITrackingDBService"
);

let idToTextMap = new Map([
  [Ci.nsITrackingDBService.TRACKERS_ID, "tracker"],
  [Ci.nsITrackingDBService.TRACKING_COOKIES_ID, "cookie"],
  [Ci.nsITrackingDBService.CRYPTOMINERS_ID, "cryptominer"],
  [Ci.nsITrackingDBService.FINGERPRINTERS_ID, "fingerprinter"],
]);

var AboutProtectionsHandler = {
  _inited: false,
  _topics: [
    "OpenAboutLogins",
    "OpenContentBlockingPreferences",
    "OpenSyncPreferences",
    "FetchContentBlockingEvents",
    "FetchUserLoginsData",
  ],

  init() {
    this.receiveMessage = this.receiveMessage.bind(this);
    this.pageListener = new RemotePages("about:protections");
    for (let topic of this._topics) {
      this.pageListener.addMessageListener(topic, this.receiveMessage);
    }
    this._inited = true;
  },

  uninit() {
    if (!this._inited) {
      return;
    }
    for (let topic of this._topics) {
      this.pageListener.removeMessageListener(topic, this.receiveMessage);
    }
    this.pageListener.destroy();
  },

  /**
   * Retrieves login data for the user.
   *
   * @return {{ isLoggedIn: Boolean,
   *            numberOfLogins: Number,
   *            numberOfSyncedDevices: Number }}
   *         The login data.
   */
  getLoginData() {
    const logins = Services.logins.countLogins("", "", "");

    const isLoggedIn = logins > 0;
    return {
      isLoggedIn,
      numberOfLogins: logins,
      numberOfSyncedDevices: 0,
    };
  },

  receiveMessage(aMessage) {
    let win = aMessage.target.browser.ownerGlobal;
    switch (aMessage.name) {
      case "OpenAboutLogins":
        win.openTrustedLinkIn("about:logins", "tab");
        break;
      case "OpenContentBlockingPreferences":
        win.openPreferences("privacy-trackingprotection", {
          origin: "about-protections",
        });
        break;
      case "OpenSyncPreferences":
        win.openTrustedLinkIn("about:preferences#sync", "tab");
        break;
      case "FetchContentBlockingEvents":
        TrackingDBService.getEventsByDateRange(
          aMessage.data.from,
          aMessage.data.to
        ).then(results => {
          let dataToSend = {};
          let largest = 0;
          for (let result of results) {
            let count = result.getResultByName("count");
            let type = result.getResultByName("type");
            let timestamp = result.getResultByName("timestamp");
            dataToSend[timestamp] = dataToSend[timestamp] || { total: 0 };
            dataToSend[timestamp][idToTextMap.get(type)] = count;
            dataToSend[timestamp].total += count;
            // Record the largest amount of tracking events found per day,
            // to create the tallest column on the graph and compare other days to.
            if (largest < dataToSend[timestamp].total) {
              largest = dataToSend[timestamp].total;
            }
          }
          dataToSend.largest = largest;
          if (aMessage.target.browser) {
            aMessage.target.sendAsyncMessage(
              "SendContentBlockingRecords",
              dataToSend
            );
          }
        });
        break;
      case "FetchUserLoginsData":
        aMessage.target.sendAsyncMessage(
          "SendUserLoginsData",
          this.getLoginData()
        );
        break;
    }
  },
};
