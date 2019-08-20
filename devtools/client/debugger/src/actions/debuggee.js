/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { differenceBy } from "lodash";
import type { Action, ThunkArgs } from "./types";
import { removeSourceActors } from "./source-actors";

import { getContext, getWorkers, getSourceActorsForThread } from "../selectors";

export function updateWorkers() {
  return async function({ dispatch, getState, client }: ThunkArgs) {
    const cx = getContext(getState());
    const workers = await client.fetchWorkers();

    const currentWorkers = getWorkers(getState());

    const addedWorkers = differenceBy(workers, currentWorkers, w => w.actor);
    const removedWorkers = differenceBy(currentWorkers, workers, w => w.actor);
    if (removedWorkers.length > 0) {
      const sourceActors = getSourceActorsForThread(
        getState(),
        removedWorkers.map(w => w.actor)
      );
      dispatch(removeSourceActors(sourceActors));
      dispatch(
        ({
          type: "REMOVE_WORKERS",
          cx,
          workers: removedWorkers.map(w => w.actor),
        }: Action)
      );
    }
    if (addedWorkers.length > 0) {
      dispatch(({ type: "INSERT_WORKERS", cx, workers: addedWorkers }: Action));
    }
  };
}
