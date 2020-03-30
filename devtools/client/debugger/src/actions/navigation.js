/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { clearDocuments } from "../utils/editor";
import sourceQueue from "../utils/source-queue";

import { updateThreads } from "./threads";
import { evaluateExpressions } from "./expressions";

import { clearWasmStates } from "../utils/wasm";
import { getMainThread, getThreadContext } from "../selectors";
import type { Action, ThunkArgs } from "./types";
import type { ActorId, URL } from "../types";

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
    const thread = getMainThread(getState());

    dispatch({
      type: "NAVIGATE",
      mainThread: { ...thread, url: event.url },
    });
  };
}

export function connect(
  url: URL,
  actor: ActorId,
  traits: Object,
  isWebExtension: boolean
) {
  return async function({ dispatch, getState }: ThunkArgs) {
    await dispatch(updateThreads());
    await dispatch(
      ({
        type: "CONNECT",
        mainThread: {
          url,
          actor,
          type: "mainThread",
          name: L10N.getStr("mainThread"),
        },
        traits,
        isWebExtension,
      }: Action)
    );

    const cx = getThreadContext(getState());
    dispatch(evaluateExpressions(cx));
  };
}

/**
 * @memberof actions/navigation
 * @static
 */
export function navigated() {
  return async function({ dispatch, panel }: ThunkArgs) {
    await dispatch(updateThreads());
    panel.emit("reloaded");
  };
}
