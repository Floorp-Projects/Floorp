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
ChromeUtils.defineModuleGetter(
  this,
  "fxAccounts",
  "resource://gre/modules/FxAccounts.jsm"
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
  _topics: [
    // Opening about:* pages
    "OpenAboutLogins",
    "OpenContentBlockingPreferences",
    "OpenSyncPreferences",
    // Fetching data
    "FetchContentBlockingEvents",
    "FetchMonitorData",
    "FetchUserLoginsData",
    // Getting prefs
    "GetEnabledPrefs",
  ],
  _prefs: {
    LockwiseCard: "browser.contentblocking.report.lockwise.enabled",
    MonitorCard: "browser.contentblocking.report.monitor.enabled",
  },

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
   * @return {{ hasFxa: Boolean,
   *            numLogins: Number,
   *            numSyncedDevices: Number }}
   *         The login data.
   */
  async getLoginData() {
    let syncedDevices = [];
    const hasFxa = await fxAccounts.accountStatus();

    if (hasFxa) {
      syncedDevices = await fxAccounts.getDeviceList();
    }

    return {
      hasFxa,
      numLogins: Services.logins.countLogins("", "", ""),
      numSyncedDevices: syncedDevices.length,
    };
  },

  /**
   * Retrieves monitor data for the user.
   *
   * @return {{ monitoredEmails: Number,
   *            numBreaches: Number,
   *            passwords: Number,
   *            error: Boolean }}
   *         Monitor data.
   */
  async getMonitorData() {
    // TODO: Fetch real data for endpoints in Bug 1559424.
    return {
      monitoredEmails: 1,
      numBreaches: 11,
      passwords: 8,
      lockwisePasswords: 2,
      error: false,
    };
  },

  /**
   * Sends a response from message target.
   *
   * @param {Object}  target
   *        The message target.
   * @param {String}  message
   *        The topic of the message to send.
   * @param {Object}  payload
   *        The payload of the message to send.
   */
  sendMessage(target, message, payload) {
    // Make sure the target's browser is available before sending.
    if (target.browser) {
      target.sendAsyncMessage(message, payload);
    }
  },

  async receiveMessage(aMessage) {
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
        let sumEvents = await TrackingDBService.sumAllEvents();
        let earliestDate = await TrackingDBService.getEarliestRecordedDate();
        let eventsByDate = await TrackingDBService.getEventsByDateRange(
          aMessage.data.from,
          aMessage.data.to
        );
        let dataToSend = {};
        let largest = 0;

        for (let result of eventsByDate) {
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
          dataToSend.largest = largest;
        }
        dataToSend.earliestDate = earliestDate;
        dataToSend.sumEvents = sumEvents;
        this.sendMessage(
          aMessage.target,
          "SendContentBlockingRecords",
          dataToSend
        );
        break;
      case "FetchMonitorData":
        this.sendMessage(
          aMessage.target,
          "SendMonitorData",
          await this.getMonitorData()
        );
        break;
      case "FetchUserLoginsData":
        this.sendMessage(
          aMessage.target,
          "SendUserLoginsData",
          await this.getLoginData()
        );
        break;
      case "GetEnabledPrefs":
        const prefs = Object.keys(this._prefs);

        // Get all the enabled prefs and send separate messages depending on their names.
        for (let name of prefs) {
          const message = `SendEnabled${name}Pref`;
          const isEnabled = Services.prefs.getBoolPref(this._prefs[name]);
          this.sendMessage(aMessage.target, message, {
            isEnabled,
          });
        }
        break;
    }
  },
};
