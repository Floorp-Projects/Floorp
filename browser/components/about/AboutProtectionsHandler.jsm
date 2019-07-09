/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutProtectionsHandler"];

const { RemotePages } = ChromeUtils.import(
  "resource://gre/modules/remotepagemanager/RemotePageManagerParent.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var AboutProtectionsHandler = {
  _inited: false,
  _topics: [
    "openContentBlockingPreferences",
    "OpenAboutLogins",
    "OpenSyncPreferences",
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
      case "openContentBlockingPreferences":
        win.openPreferences("privacy-trackingprotection", {
          origin: "about-protections",
        });
        break;
      case "OpenAboutLogins":
        win.openTrustedLinkIn("about:logins", "tab");
        break;
      case "OpenSyncPreferences":
        win.openTrustedLinkIn("about:preferences#sync", "tab");
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
