/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Threads reducer
 * @module reducers/threads
 */

import { sortBy } from "lodash";
import { createSelector } from "reselect";

import type { Selector, State } from "./types";
import type { Thread, ThreadList, Worker } from "../types";
import type { Action } from "../actions/types";

export type ThreadsState = {
  threads: ThreadList,
  traits: Object,
  isWebExtension: boolean,
};

export function initialThreadsState(): ThreadsState {
  return {
    threads: [],
    traits: {},
    isWebExtension: false,
  };
}

export default function update(
  state: ThreadsState = initialThreadsState(),
  action: Action
): ThreadsState {
  switch (action.type) {
    case "CONNECT":
      return {
        ...state,
        traits: action.traits,
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

export const getWorkerCount = (state: State) => getThreads(state).length;

export function getWorkerByThread(state: State, thread: string): ?Worker {
  return getThreads(state).find(worker => worker.actor == thread);
}

function isMainThread(thread: Thread) {
  return thread.type === "mainThread";
}

export function getMainThread(state: State): ?Thread {
  return state.threads.threads.find(isMainThread);
}

export function getDebuggeeUrl(state: State): string {
  return getMainThread(state)?.url || "";
}

export const getThreads: Selector<Thread[]> = createSelector(
  state => state.threads.threads,
  threads => threads.filter(thread => !isMainThread(thread))
);

export const getAllThreads: Selector<Thread[]> = createSelector(
  getMainThread,
  getThreads,
  (mainThread, threads) =>
    [mainThread, ...sortBy(threads, thread => thread.name)].filter(Boolean)
);

export function getThread(state: State, threadActor: string) {
  return getAllThreads(state).find(thread => thread.actor === threadActor);
}

// checks if a path begins with a thread actor
// e.g "server1.conn0.child1/workerTarget22/context1/dbg-workers.glitch.me"
export function startsWithThreadActor(state: State, path: string): ?string {
  const threadActors = getAllThreads(state).map(t => t.actor);
  const match = path.match(new RegExp(`(${threadActors.join("|")})\/(.*)`));
  return match?.[1];
}
