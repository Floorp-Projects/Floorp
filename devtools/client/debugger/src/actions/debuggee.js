/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { differenceBy } from "lodash";
import type { Action, ThunkArgs } from "./types";
import { removeSourceActors } from "./source-actors";

import { getContext, getThreads, getSourceActorsForThread } from "../selectors";

export function updateThreads() {
  return async function({ dispatch, getState, client }: ThunkArgs) {
    const cx = getContext(getState());
    const threads = await client.fetchThreads();

    const currentThreads = getThreads(getState());

    const addedThreads = differenceBy(threads, currentThreads, w => w.actor);
    const removedThreads = differenceBy(currentThreads, threads, w => w.actor);
    if (removedThreads.length > 0) {
      const sourceActors = getSourceActorsForThread(
        getState(),
        removedThreads.map(w => w.actor)
      );
      dispatch(removeSourceActors(sourceActors));
      dispatch(
        ({
          type: "REMOVE_THREADS",
          cx,
          threads: removedThreads.map(w => w.actor),
        }: Action)
      );
    }
    if (addedThreads.length > 0) {
      dispatch(({ type: "INSERT_THREADS", cx, threads: addedThreads }: Action));
    }
  };
}
