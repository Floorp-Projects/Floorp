/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { clearDocuments } from "../utils/editor";
import sourceQueue from "../utils/source-queue";

import { updateThreads } from "./threads";

import { clearWasmStates } from "../utils/wasm";
import { getMainThread } from "../selectors";
import type { Action, ThunkArgs } from "./types";

/**
 * Redux actions for the navigation state
 * @module actions/navigation
 */

/**
 * @memberof actions/navigation
 * @static
 */
export function willNavigate(event: Object) {
  return async function({
    dispatch,
    getState,
    client,
    sourceMaps,
    parser,
  }: ThunkArgs) {
    sourceQueue.clear();
    sourceMaps.clearSourceMaps();
    clearWasmStates();
    clearDocuments();
    parser.clear();
    client.detachWorkers();
    const thread = getMainThread(getState());

    dispatch({
      type: "NAVIGATE",
      mainThread: { ...thread, url: event.url },
    });
  };
}

export function connect(
  url: string,
  actor: string,
  canRewind: boolean,
  isWebExtension: boolean
) {
  return async function({ dispatch }: ThunkArgs) {
    await dispatch(updateThreads());
    dispatch(
      ({
        type: "CONNECT",
        mainThread: {
          url,
          actor,
          type: "main-thread",
          name: L10N.getStr("mainThread"),
        },
        canRewind,
        isWebExtension,
      }: Action)
    );
  };
}

/**
 * @memberof actions/navigation
 * @static
 */
export function navigated() {
  return async function({ panel }: ThunkArgs) {
    panel.emit("reloaded");
  };
}
