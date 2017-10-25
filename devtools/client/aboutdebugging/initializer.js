/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
/* globals DebuggerClient, DebuggerServer, Telemetry */

"use strict";

const { loader } = Components.utils.import(
  "resource://devtools/shared/Loader.jsm", {});
const { BrowserLoader } = Components.utils.import(
  "resource://devtools/client/shared/browser-loader.js", {});
const { Services } = Components.utils.import(
  "resource://gre/modules/Services.jsm", {});

loader.lazyRequireGetter(this, "DebuggerClient",
  "devtools/shared/client/debugger-client", true);
loader.lazyRequireGetter(this, "DebuggerServer",
  "devtools/server/main", true);
loader.lazyRequireGetter(this, "Telemetry",
  "devtools/client/shared/telemetry");

const { require } = BrowserLoader({
  baseURI: "resource://devtools/client/aboutdebugging/",
  window
});

const { createFactory } = require("devtools/client/shared/vendor/react");
const { render, unmountComponentAtNode } = require("devtools/client/shared/vendor/react-dom");

const AboutDebuggingApp = createFactory(require("./components/Aboutdebugging"));

var AboutDebugging = {
  init() {
    if (!Services.prefs.getBoolPref("devtools.enabled", true)) {
      // If DevTools are disabled, navigate to about:devtools.
      window.location = "about:devtools?reason=AboutDebugging";
      return;
    }

    if (!DebuggerServer.initialized) {
      DebuggerServer.init();
    }
    DebuggerServer.allowChromeProcess = true;
    // We want a full featured server for about:debugging. Especially the
    // "browser actors" like addons.
    DebuggerServer.registerActors({ root: true, browser: true, tab: true });

    this.client = new DebuggerClient(DebuggerServer.connectPipe());

    this.client.connect().then(() => {
      let client = this.client;
      let telemetry = new Telemetry();

      render(AboutDebuggingApp({ client, telemetry }),
        document.querySelector("#body"));
    });
  },

  destroy() {
    unmountComponentAtNode(document.querySelector("#body"));

    if (this.client) {
      this.client.close();
      this.client = null;
    }
  },
};

window.addEventListener("DOMContentLoaded", function () {
  AboutDebugging.init();
}, {once: true});

window.addEventListener("unload", function () {
  AboutDebugging.destroy();
}, {once: true});
