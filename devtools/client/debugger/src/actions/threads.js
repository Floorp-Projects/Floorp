/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { differenceBy } from "lodash";
import type { Target } from "../client/firefox/types";
import type { Thread, ThreadList, ActorId } from "../types";
import type { Action, ThunkArgs } from "./types";
import { removeSourceActors } from "./source-actors";
import { newGeneratedSources } from "./sources";

import {
  getContext,
  getAllThreads,
  getThreads,
  getSourceActorsForThread,
} from "../selectors";

function addThreads(
  { dispatch, client, getState }: ThunkArgs,
  addedThreads: ThreadList
) {
  const cx = getContext(getState());
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

function removeThreads(
  { dispatch, client, getState }: ThunkArgs,
  removedThreads: ThreadList
) {
  const cx = getContext(getState());
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

export function addTarget(targetFront: Target) {
  return async function(args: ThunkArgs) {
    const { client } = args;
    const thread = await client.attachThread(targetFront);
    return addThreads(args, [thread]);
  };
}
export function removeTarget(targetFront: Target) {
  return async function(args: ThunkArgs) {
    const { getState } = args;
    const currentThreads = getThreads(getState());
    const { actorID } = targetFront.threadFront;
    const thread: void | Thread = currentThreads.find(t => t.actor == actorID);
    if (thread) {
      return removeThreads(args, [thread]);
    }
  };
}

export function updateThreads() {
  return async function(args: ThunkArgs) {
    const { dispatch, getState, client } = args;
    const cx = getContext(getState());
    const threads = await client.fetchThreads();

    const currentThreads = getThreads(getState());

    const addedThreads = differenceBy(threads, currentThreads, t => t.actor);
    const removedThreads = differenceBy(currentThreads, threads, t => t.actor);
    if (removedThreads.length > 0) {
      removeThreads(args, removedThreads);
    }
    if (addedThreads.length > 0) {
      await addThreads(args, addedThreads);
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
