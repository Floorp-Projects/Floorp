/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutNetErrorHandler"];

const {RemotePages} = ChromeUtils.import("resource://gre/modules/remotepagemanager/RemotePageManagerParent.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

var AboutNetErrorHandler = {
  _inited: false,

  init() {
    this._boundReceiveMessage = this.receiveMessage.bind(this);
    this.pageListener = new RemotePages(["about:certerror", "about:neterror"]);
    this.pageListener.addMessageListener("Browser:OpenCaptivePortalPage", this._boundReceiveMessage);
    this._inited = true;

    Services.obs.addObserver(this, "captive-portal-login-abort");
    Services.obs.addObserver(this, "captive-portal-login-success");
  },

  uninit() {
    if (!this._inited) {
      return;
    }

    this.pageListener.removeMessageListener("Browser:OpenCaptivePortalPage", this._boundReceiveMessage);
    this.pageListener.destroy();

    Services.obs.removeObserver(this, "captive-portal-login-abort");
    Services.obs.removeObserver(this, "captive-portal-login-success");
  },

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "captive-portal-login-abort":
      case "captive-portal-login-success":
        // Send a message to the content when a captive portal is freed
        // so that error pages can refresh themselves.
        this.pageListener.sendAsyncMessage("AboutNetErrorCaptivePortalFreed");
      break;
    }
  },

  receiveMessage(msg) {
    switch (msg.name) {
      case "Browser:OpenCaptivePortalPage":
        Services.obs.notifyObservers(null, "ensure-captive-portal-tab");
      break;
    }
  },
};
