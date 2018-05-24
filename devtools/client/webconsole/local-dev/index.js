/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /* eslint-env browser */

"use strict";

const React = require("react");
const ReactDOM = require("react-dom");
const { EventEmitter } = require("devtools-modules");
const { Services: { appinfo, pref } } = require("devtools-modules");
const { bootstrap } = require("devtools-launchpad");

EventEmitter.decorate(window);

require("../../themes/widgets.css");
require("../../themes/webconsole.css");
require("../../themes/components-frame.css");
require("../../themes/light-theme.css");
require("../../shared/components/reps/reps.css");
require("../../shared/components/tabs/Tabs.css");
require("../../shared/components/tabs/TabBar.css");
require("../../netmonitor/src/assets/styles/httpi.css");

pref("devtools.debugger.remote-timeout", 10000);
pref("devtools.hud.loglimit", 10000);
pref("devtools.webconsole.filter.error", true);
pref("devtools.webconsole.filter.warn", true);
pref("devtools.webconsole.filter.info", true);
pref("devtools.webconsole.filter.log", true);
pref("devtools.webconsole.filter.debug", true);
pref("devtools.webconsole.filter.css", false);
pref("devtools.webconsole.filter.net", false);
pref("devtools.webconsole.filter.netxhr", false);
pref("devtools.webconsole.ui.filterbar", false);
pref("devtools.webconsole.inputHistoryCount", 50);
pref("devtools.webconsole.persistlog", false);
pref("devtools.webconsole.timestampMessages", false);
pref("devtools.webconsole.autoMultiline", true);
pref("devtools.webconsole.sidebarToggle", true);

const WebConsoleOutputWrapper = require("../webconsole-output-wrapper");
const WebConsoleFrame = require("../webconsole-frame").WebConsoleFrame;

// Copied from netmonitor/index.js:
window.addEventListener("DOMContentLoaded", () => {
  for (let link of document.head.querySelectorAll("link")) {
    link.href = link.href.replace(/(resource|chrome)\:\/\//, "/");
  }

  if (appinfo.OS === "Darwin") {
    document.documentElement.setAttribute("platform", "mac");
  } else if (appinfo.OS === "Linux") {
    document.documentElement.setAttribute("platform", "linux");
  } else {
    document.documentElement.setAttribute("platform", "win");
  }
});

let consoleFrame;
function onConnect(connection) {
  // If we are on the main dashboard don't render the component
  if (!connection || !connection.tabConnection || !connection.tabConnection.tabTarget) {
    return;
  }

  // Replicate the DOM that the root component lives within
  document.querySelector("#mount").innerHTML = `
    <div id="app-wrapper" class="theme-body">
      <div id="output-container" role="document" aria-live="polite" />
    </div>
  `;

  // Stub out properties that are received from hudservice
  const owner = {
    iframeWindow: window,
    chromeWindow: window,
    hudId: "hud_0",
    getDebuggerFrames: () => { },
    getInspectorSelection: () => { },
    target: connection.tabConnection.tabTarget,
    _browserConsole: false,
    WebConsoleOutputWrapper,
  };
  consoleFrame = new WebConsoleFrame(owner);
  consoleFrame.init().then(function() {
    console.log("WebConsoleFrame initialized");
  });
}

// This is just a hack until the local dev environment includes jsterm
window.evaluateJS = function(input) {
  consoleFrame.webConsoleClient.evaluateJSAsync(`${input}`, function(r) {
    consoleFrame.consoleOutput.dispatchMessageAdd(r);
  }, {});
};

document.documentElement.classList.add("theme-light");
bootstrap(React, ReactDOM).then(onConnect);
