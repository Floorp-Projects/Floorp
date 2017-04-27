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

require("../../themes/new-webconsole.css");
require("../../shared/components/reps/reps.css");

pref("devtools.debugger.remote-timeout", 10000);
pref("devtools.hud.loglimit", 1000);
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

const NewConsoleOutputWrapper = require("../new-console-output/new-console-output-wrapper");
const NewWebConsoleFrame = require("../new-webconsole").NewWebConsoleFrame;

// Replicate the DOM that the root component lives within
const el = document.createElement("div");
el.style.flex = "1";
el.innerHTML = `
  <div id="app-wrapper" class="theme-body">
    <div id="output-container" role="document" aria-live="polite" />
  </div>
`;
document.querySelector("#mount").appendChild(el);

document.documentElement.classList.add("theme-light");

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

  // Stub out properties that are received from hudservice
  const owner = {
    iframeWindow: window,
    chromeWindow: window,
    hudId: "hud_0",
    target: connection.tabConnection.tabTarget,
    _browserConsole: false,
    NewConsoleOutputWrapper,
  };
  consoleFrame = new NewWebConsoleFrame(owner);
  consoleFrame.init().then(function () {
    console.log("NewWebConsoleFrame initialized");
  });
}

// This is just a hack until the local dev environment includes jsterm
window.evaluateJS = function (input) {
  consoleFrame.webConsoleClient.evaluateJSAsync(`${input}`, function (r) {
    consoleFrame.newConsoleOutput.dispatchMessageAdd(r);
  }, {});
};

bootstrap(React, ReactDOM, el).then(onConnect);
