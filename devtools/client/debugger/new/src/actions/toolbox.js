/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

const { isDevelopment } = require("devtools-environment");

import type { ThunkArgs } from "./types";
import type { Worker, Grip } from "../types";

/**
 * @memberof actions/toolbox
 * @static
 */
export function openLink(url: string) {
  return async function({ openLink: openLinkCommand }: ThunkArgs) {
    if (isDevelopment()) {
      const win = window.open(url, "_blank");
      win.focus();
    } else {
      openLinkCommand(url);
    }
  };
}

export function openWorkerToolbox(worker: Worker) {
  return async function({
    getState,
    openWorkerToolbox: openWorkerToolboxCommand
  }: ThunkArgs) {
    if (isDevelopment()) {
      alert(worker.url);
    } else {
      openWorkerToolboxCommand(worker);
    }
  };
}

export function evaluateInConsole(inputString: string) {
  return async ({ openConsoleAndEvaluate }: ThunkArgs) => {
    if (isDevelopment()) {
      alert(`console.log: ${inputString}`);
    } else {
      return openConsoleAndEvaluate(inputString);
    }
  };
}

export function openElementInInspectorCommand(grip: Grip) {
  return async ({ openElementInInspector }: ThunkArgs) => {
    if (isDevelopment()) {
      alert(`Opening node in Inspector: ${grip.class}`);
    } else {
      return openElementInInspector(grip);
    }
  };
}
