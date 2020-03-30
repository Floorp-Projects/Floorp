/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { differenceBy } from "lodash";
import type { Action, ThunkArgs } from "./types";
import { removeSourceActors } from "./source-actors";
import { newGeneratedSources } from "./sources";

import {
  getContext,
  getAllThreads,
  getThreads,
  getSourceActorsForThread,
} from "../selectors";

import type { ActorId } from "../types";

export function updateThreads() {
  return async function({ dispatch, getState, client }: ThunkArgs) {
    const cx = getContext(getState());
    const threads = await client.fetchThreads();

    const currentThreads = getThreads(getState());

    const addedThreads = differenceBy(threads, currentThreads, t => t.actor);
    const removedThreads = differenceBy(currentThreads, threads, t => t.actor);
    if (removedThreads.length > 0) {
      const sourceActors = getSourceActorsForThread(
        getState(),
        removedThreads.map(t => t.actor)
      );
      dispatch(removeSourceActors(sourceActors));
      dispatch(
        ({
          type: "REMOVE_THREADS",
          cx,
          threads: removedThreads.map(t => t.actor),
        }: Action)
      );
    }
    if (addedThreads.length > 0) {
      dispatch(({ type: "INSERT_THREADS", cx, threads: addedThreads }: Action));

      // Fetch the sources and install breakpoints on any new workers.
      // NOTE: This runs in the background and fails quietly because it is
      // pretty easy for sources to throw during the fetch if their thread
      // shuts down, which would cause test failures.
      for (const thread of addedThreads) {
        client
          .fetchThreadSources(thread.actor)
          .then(sources => dispatch(newGeneratedSources(sources)))
          .catch(e => console.error(e));
      }
    }

    // Update the status of any service workers.
    for (const thread of currentThreads) {
      if (thread.serviceWorkerStatus) {
        for (const fetchedThread of threads) {
          if (
            fetchedThread.actor == thread.actor &&
            fetchedThread.serviceWorkerStatus != thread.serviceWorkerStatus
          ) {
            dispatch(
              ({
                type: "UPDATE_SERVICE_WORKER_STATUS",
                cx,
                thread: thread.actor,
                status: fetchedThread.serviceWorkerStatus,
              }: Action)
            );
          }
        }
      }
    }
  };
}

export function ensureHasThread(thread: ActorId) {
  return async function({ dispatch, getState, client }: ThunkArgs) {
    const currentThreads = getAllThreads(getState());
    if (!currentThreads.some(t => t.actor == thread)) {
      await dispatch(updateThreads());
    }
  };
}
