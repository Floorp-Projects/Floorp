/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
/* global DebuggerClient, DebuggerServer */

"use strict";

const { loader } = Components.utils.import(
  "resource://devtools/shared/Loader.jsm", {});

loader.lazyRequireGetter(this, "DebuggerClient",
  "devtools/shared/client/main", true);
loader.lazyRequireGetter(this, "DebuggerServer",
  "devtools/server/main", true);
loader.lazyRequireGetter(this, "Telemetry",
  "devtools/client/shared/telemetry");

const { BrowserLoader } = Components.utils.import(
  "resource://devtools/client/shared/browser-loader.js", {});
const { require } =
  BrowserLoader("resource://devtools/client/aboutdebugging/", window);

const React = require("devtools/client/shared/vendor/react");
const { AboutDebuggingApp } = require("./components/aboutdebugging");

var AboutDebugging = {
  init() {
    if (!DebuggerServer.initialized) {
      DebuggerServer.init();
      DebuggerServer.addBrowserActors();
    }
    DebuggerServer.allowChromeProcess = true;
    this.client = new DebuggerClient(DebuggerServer.connectPipe());

    this.client.connect().then(() => {
      let client = this.client;
      let telemetry = new Telemetry();

      let app = React.createElement(AboutDebuggingApp, { client, telemetry });
      React.render(app, document.querySelector("#body"));
    });
  },

  destroy() {
    React.unmountComponentAtNode(document.querySelector("#body"));

    this.client.close();
    this.client = null;
  },
};

window.addEventListener("DOMContentLoaded", function load() {
  window.removeEventListener("DOMContentLoaded", load);
  AboutDebugging.init();
});

window.addEventListener("unload", function unload() {
  window.removeEventListener("unload", unload);
  AboutDebugging.destroy();
});
