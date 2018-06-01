/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
/* globals Telemetry */

"use strict";

const { loader } = ChromeUtils.import(
  "resource://devtools/shared/Loader.jsm", {});
const { BrowserLoader } = ChromeUtils.import(
  "resource://devtools/client/shared/browser-loader.js", {});
const { Services } = ChromeUtils.import(
  "resource://gre/modules/Services.jsm", {});

loader.lazyRequireGetter(this, "Telemetry",
  "devtools/client/shared/telemetry");

const { require } = BrowserLoader({
  baseURI: "resource://devtools/client/aboutdebugging/",
  window
});

const { createFactory } = require("devtools/client/shared/vendor/react");
const { render, unmountComponentAtNode } = require("devtools/client/shared/vendor/react-dom");
const EventEmitter = require("devtools/shared/event-emitter");

const AboutDebuggingApp = createFactory(require("./components/Aboutdebugging"));
const { createClient } = require("./modules/connect");

var AboutDebugging = {
  async init() {
    if (!Services.prefs.getBoolPref("devtools.enabled", true)) {
      // If DevTools are disabled, navigate to about:devtools.
      window.location = "about:devtools?reason=AboutDebugging";
      return;
    }

    const {connect, client} = await createClient();

    this.client = client;
    await this.client.connect();

    const telemetry = new Telemetry();

    render(AboutDebuggingApp({ client, connect, telemetry }),
      document.querySelector("#body"));
  },

  destroy() {
    unmountComponentAtNode(document.querySelector("#body"));

    if (this.client) {
      this.client.close();
      this.client = null;
    }
  },
};

// Used to track async requests in tests.  See bug 1444424 for better ideas.
EventEmitter.decorate(AboutDebugging);

window.addEventListener("DOMContentLoaded", function() {
  AboutDebugging.init();
}, {once: true});

window.addEventListener("unload", function() {
  AboutDebugging.destroy();
}, {once: true});
