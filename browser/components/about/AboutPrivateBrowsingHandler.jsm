/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutPrivateBrowsingHandler"];

ChromeUtils.import("resource://gre/modules/RemotePageManager.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

var AboutPrivateBrowsingHandler = {
  _topics: [
    "DontShowIntroPanelAgain",
    "OpenPrivateWindow",
  ],

  init() {
    this.pageListener = new RemotePages("about:privatebrowsing");
    for (let topic of this._topics) {
      this.pageListener.addMessageListener(topic, this.receiveMessage.bind(this));
    }
  },

  uninit() {
    for (let topic of this._topics) {
      this.pageListener.removeMessageListener(topic);
    }
    this.pageListener.destroy();
  },

  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "OpenPrivateWindow": {
        let win = aMessage.target.browser.ownerGlobal;
        win.OpenBrowserWindow({private: true});
        break;
      }
      case "DontShowIntroPanelAgain": {
        let win = aMessage.target.browser.ownerGlobal;
        win.TrackingProtection.dontShowIntroPanelAgain();
        break;
      }
    }
  },
};
