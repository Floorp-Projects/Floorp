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
import { validateContext } from "../utils/context";

import {
  getContext,
  getAllThreads,
  getThreads,
  getSourceActorsForThread,
} from "../selectors";

async function addThreads(
  { dispatch, client, getState }: ThunkArgs,
  addedThreads: ThreadList
) {
  const cx = getContext(getState());
  dispatch(({ type: "INSERT_THREADS", cx, threads: addedThreads }: Action));

  // Fetch the sources and install breakpoints on any new workers.
  await Promise.all(
    addedThreads.map(async thread => {
      try {
        const sources = await client.fetchThreadSources(thread.actor);
        validateContext(getState(), cx);
        await dispatch(newGeneratedSources(sources));
      } catch (e) {
        // NOTE: This fails quietly because it is pretty easy for sources to
        // throw during the fetch if their thread shuts down,
        // which would cause test failures.
        console.error(e);
      }
    })
  );
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
    const { client, getState } = args;
    const cx = getContext(getState());
    const thread = await client.attachThread(targetFront);
    validateContext(getState(), cx);
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
    validateContext(getState(), cx);

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
