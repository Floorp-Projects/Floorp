/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { clearDocuments } from "../utils/editor";
import sourceQueue from "../utils/source-queue";
import { getSourceList } from "../reducers/sources";
import { waitForMs } from "../utils/utils";

import { newGeneratedSources } from "./sources";
import { updateWorkers } from "./debuggee";

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
    await dispatch(updateWorkers());
    dispatch(
      ({
        type: "CONNECT",
        mainThread: { url, actor, type: -1, name: "" },
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
  return async function({ dispatch, getState, client, panel }: ThunkArgs) {
    // this time out is used to wait for sources. If we have 0 sources,
    // it is likely that the sources are being loaded from the bfcache,
    // and we should make an explicit request to the server to load them.
    await waitForMs(100);
    if (getSourceList(getState()).length == 0) {
      const sources = await client.fetchSources();
      dispatch(newGeneratedSources(sources));
    }
    panel.emit("reloaded");
  };
}
