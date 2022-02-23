/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Threads reducer
 * @module reducers/threads
 */

import { sortBy } from "lodash";
import { createSelector } from "reselect";

export function initialThreadsState() {
  return {
    threads: [],
    isWebExtension: false,
  };
}

export default function update(state = initialThreadsState(), action) {
  switch (action.type) {
    case "CONNECT":
      return {
        ...state,
        isWebExtension: action.isWebExtension,
      };
    case "INSERT_THREAD":
      return {
        ...state,
        threads: [...state.threads, action.newThread],
      };

    case "REMOVE_THREAD":
      const { oldThread } = action;
      return {
        ...state,
        threads: state.threads.filter(
          thread => oldThread.actor != thread.actor
        ),
      };
    case "UPDATE_SERVICE_WORKER_STATUS":
      const { thread, status } = action;
      return {
        ...state,
        threads: state.threads.map(t => {
          if (t.actor == thread) {
            return { ...t, serviceWorkerStatus: status };
          }
          return t;
        }),
      };

    default:
      return state;
  }
}

export const getWorkerCount = state => getThreads(state).length;

export function getWorkerByThread(state, thread) {
  return getThreads(state).find(worker => worker.actor == thread);
}

function isMainThread(thread) {
  return thread.isTopLevel;
}

export function getMainThread(state) {
  return state.threads.threads.find(isMainThread);
}

export function getDebuggeeUrl(state) {
  return getMainThread(state)?.url || "";
}

export const getThreads = createSelector(
  state => state.threads.threads,
  threads => threads.filter(thread => !isMainThread(thread))
);

export const getAllThreads = createSelector(
  getMainThread,
  getThreads,
  (mainThread, threads) =>
    [mainThread, ...sortBy(threads, thread => thread.name)].filter(Boolean)
);

export function getThread(state, threadActor) {
  return getAllThreads(state).find(thread => thread.actor === threadActor);
}

// checks if a path begins with a thread actor
// e.g "server1.conn0.child1/workerTarget22/context1/dbg-workers.glitch.me"
export function startsWithThreadActor(state, path) {
  const threadActors = getAllThreads(state).map(t => t.actor);
  const match = path.match(new RegExp(`(${threadActors.join("|")})\/(.*)`));
  return match?.[1];
}
