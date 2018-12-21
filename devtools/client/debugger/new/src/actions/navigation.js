/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { clearDocuments } from "../utils/editor";
import sourceQueue from "../utils/source-queue";
import { getSources } from "../reducers/sources";
import { waitForMs } from "../utils/utils";

import { newSources } from "./sources";
import { updateWorkers } from "./debuggee";

import {
  clearASTs,
  clearSymbols,
  clearScopes,
  clearSources
} from "../workers/parser";

import { clearWasmStates } from "../utils/wasm";

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
  return async function({ dispatch, getState, client, sourceMaps }: ThunkArgs) {
    await sourceMaps.clearSourceMaps();
    clearWasmStates();
    clearDocuments();
    clearSymbols();
    clearASTs();
    clearScopes();
    clearSources();
    dispatch(navigate(event.url));
  };
}

export function navigate(url: string): Action {
  sourceQueue.clear();

  return {
    type: "NAVIGATE",
    url
  };
}

export function connect(url: string, thread: string, canRewind: boolean) {
  return async function({ dispatch }: ThunkArgs) {
    await dispatch(updateWorkers());
    dispatch(({ type: "CONNECT", url, thread, canRewind }: Action));
  };
}

/**
 * @memberof actions/navigation
 * @static
 */
export function navigated() {
  return async function({ dispatch, getState, client }: ThunkArgs) {
    await waitForMs(100);
    if (Object.keys(getSources(getState())).length == 0) {
      const sources = await client.fetchSources();
      dispatch(newSources(sources));
    }
  };
}
