/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FxAccounts.jsm");

function log(msg) {
  //dump("FXA: " + msg + "\n");
};

function error(msg) {
  console.log("Firefox Account Error: " + msg + "\n");
};

let wrapper = {
  iframe: null,

  init: function () {
    let iframe = document.getElementById("remote");
    this.iframe = iframe;
    iframe.addEventListener("load", this);

    try {
      iframe.src = fxAccounts.getAccountsURI();
    } catch (e) {
      error("Couldn't init Firefox Account wrapper: " + e.message);
    }
  },

  handleEvent: function (evt) {
    switch (evt.type) {
      case "load":
        this.iframe.contentWindow.addEventListener("FirefoxAccountsCommand", this);
        this.iframe.removeEventListener("load", this);
        break;
      case "FirefoxAccountsCommand":
        this.handleRemoteCommand(evt);
        break;
    }
  },

  /**
   * onLogin handler receives user credentials from the jelly after a
   * sucessful login and stores it in the fxaccounts service
   *
   * @param accountData the user's account data and credentials
   */
  onLogin: function (accountData) {
    log("Received: 'login'. Data:" + JSON.stringify(accountData));

    fxAccounts.setSignedInUser(accountData).then(
      () => this.injectData("message", { status: "login" }),
      (err) => this.injectData("message", { status: "error", error: err })
    );
  },

  /**
   * onVerified handler receives user credentials from the jelly after
   * sucessful account creation and email verification
   * and stores it in the fxaccounts service
   *
   * @param accountData the user's account data and credentials
   */
  onVerified: function (accountData) {
    log("Received: 'verified'. Data:" + JSON.stringify(accountData));

    fxAccounts.setSignedInUser(accountData).then(
      () => this.injectData("message", { status: "verified" }),
      (err) => this.injectData("message", { status: "error", error: err })
    );
  },

  handleRemoteCommand: function (evt) {
    log('command: ' + evt.detail.command);
    let data = evt.detail.data;

    switch (evt.detail.command) {
      case "login":
        this.onLogin(data);
        break;
      case "verified":
        this.onVerified(data);
        break;
      default:
        log("Unexpected remote command received: " + evt.detail.command + ". Ignoring command.");
        break;
    }
  },

  injectData: function (type, content) {
    let authUrl;
    try {
      authUrl = fxAccounts.getAccountsURI();
    } catch (e) {
      error("Couldn't inject data: " + e.message);
      return;
    }
    let data = {
      type: type,
      content: content
    };
    this.iframe.contentWindow.postMessage(data, authUrl);
  },
};

wrapper.init();

