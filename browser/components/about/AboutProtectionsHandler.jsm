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
  _topics: ["OpenContentBlockingPreferences", "FetchContentBlockingEvents"],

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

  receiveMessage(aMessage) {
    let win = aMessage.target.browser.ownerGlobal;
    switch (aMessage.name) {
      case "OpenContentBlockingPreferences":
        win.openPreferences("privacy-trackingprotection", {
          origin: "about-protections",
        });
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
    }
  },
};
