/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { ThunkArgs } from "./types";
import type { Worker, Grip } from "../types";

/**
 * @memberof actions/toolbox
 * @static
 */
export function openLink(url: string) {
  return async function({ panel }: ThunkArgs) {
    return panel.openLink(url);
  };
}

export function openWorkerToolbox(worker: Worker) {
  return async function({ getState, panel }: ThunkArgs) {
    return panel.openWorkerToolbox(worker);
  };
}

export function evaluateInConsole(inputString: string) {
  return async ({ panel }: ThunkArgs) => {
    return panel.openConsoleAndEvaluate(inputString);
  };
}

export function openElementInInspectorCommand(grip: Grip) {
  return async ({ panel }: ThunkArgs) => {
    return panel.openElementInInspector(grip);
  };
}

export function highlightDomElement(grip: Grip) {
  return async ({ panel }: ThunkArgs) => {
    return panel.highlightDomElement(grip);
  };
}

export function unHighlightDomElement(grip: Grip) {
  return async ({ panel }: ThunkArgs) => {
    return panel.unHighlightDomElement(grip);
  };
}
