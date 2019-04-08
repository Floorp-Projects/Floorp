/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { Action, ThunkArgs } from "./types";

import { getContext } from "../selectors";

export function updateWorkers() {
  return async function({ dispatch, getState, client }: ThunkArgs) {
    const cx = getContext(getState());
    const workers = await client.fetchWorkers();
    const mainThread = client.getMainThread();
    dispatch(({ type: "SET_WORKERS", cx, workers, mainThread }: Action));
  };
}
