/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
/* global DebuggerClient, DebuggerServer, React */

"use strict";

const { loader } = Components.utils.import(
  "resource://devtools/shared/Loader.jsm", {});

loader.lazyRequireGetter(this, "DebuggerClient",
  "devtools/shared/client/main", true);
loader.lazyRequireGetter(this, "DebuggerServer",
  "devtools/server/main", true);
loader.lazyRequireGetter(this, "Telemetry",
  "devtools/client/shared/telemetry");
loader.lazyRequireGetter(this, "AboutDebuggingApp",
  "devtools/client/aboutdebugging/components/aboutdebugging", true);

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
      React.render(React.createElement(AboutDebuggingApp,
        { client, telemetry, window }), document.querySelector("#body"));
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
