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

  onLogin: function (data) {
    log("Received: 'login'. Data:" + JSON.stringify(data));
    this.injectData("message", { status: "login" });
  },

  onCreate: function (data) {
    log("Received: 'create'. Data:" + JSON.stringify(data));
    this.injectData("message", { status: "create" });
  },

  onVerified: function (data) {
    log("Received: 'verified'. Data:" + JSON.stringify(data));
    this.injectData("message", { status: "verified" });
  },

  handleRemoteCommand: function (evt) {
    log('command: ' + evt.detail.command);
    let data = evt.detail.data;

    switch (evt.detail.command) {
      case "create":
        this.onCreate(data);
        break;
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

