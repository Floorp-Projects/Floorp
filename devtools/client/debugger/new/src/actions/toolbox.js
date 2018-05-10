"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.openLink = openLink;
exports.openWorkerToolbox = openWorkerToolbox;
exports.evaluateInConsole = evaluateInConsole;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const {
  isDevelopment
} = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-environment"];

const {
  getSelectedFrameId
} = require("../selectors/index");

/**
 * @memberof actions/toolbox
 * @static
 */
function openLink(url) {
  return async function ({
    openLink: openLinkCommand
  }) {
    if (isDevelopment()) {
      const win = window.open(url, "_blank");
      win.focus();
    } else {
      openLinkCommand(url);
    }
  };
}

function openWorkerToolbox(worker) {
  return async function ({
    getState,
    openWorkerToolbox: openWorkerToolboxCommand
  }) {
    if (isDevelopment()) {
      alert(worker.url);
    } else {
      openWorkerToolboxCommand(worker);
    }
  };
}

function evaluateInConsole(inputString) {
  return async ({
    client,
    getState
  }) => {
    const frameId = getSelectedFrameId(getState());
    client.evaluate(`console.log("${inputString}"); console.log(${inputString})`, {
      frameId
    });
  };
}