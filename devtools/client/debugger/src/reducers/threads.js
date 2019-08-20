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
import type { Thread, ThreadList } from "../types";
import type { Action } from "../actions/types";

export type ThreadsState = {
  threads: ThreadList,
  mainThread: Thread,
  isWebExtension: boolean,
};

export function initialThreadsState(): ThreadsState {
  return {
    threads: [],
    mainThread: { actor: "", url: "", type: -1, name: "" },
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
        mainThread: { ...action.mainThread, name: L10N.getStr("mainThread") },
        isWebExtension: action.isWebExtension,
      };
    case "INSERT_THREADS":
      return {
        ...state,
        threads: [...state.threads, ...action.threads],
      };

    case "REMOVE_THREADS":
      const { threads } = action;
      return {
        ...state,
        threads: state.threads.filter(w => !threads.includes(w.actor)),
      };
    case "NAVIGATE":
      return {
        ...initialThreadsState(),
        mainThread: action.mainThread,
      };
    default:
      return state;
  }
}

export const getThreads = (state: OuterState) => state.threads.threads;

export const getWorkerCount = (state: OuterState) => getThreads(state).length;

export function getWorkerByThread(state: OuterState, thread: string) {
  return getThreads(state).find(worker => worker.actor == thread);
}

export function getMainThread(state: OuterState): Thread {
  return state.threads.mainThread;
}

export function getDebuggeeUrl(state: OuterState): string {
  return getMainThread(state).url;
}

export const getAllThreads: Selector<Thread[]> = createSelector(
  getMainThread,
  getThreads,
  (mainThread, threads) => [
    mainThread,
    ...sortBy(threads, thread => thread.name),
  ]
);

// checks if a path begins with a thread actor
// e.g "server1.conn0.child1/workerTarget22/context1/dbg-workers.glitch.me"
export function startsWithThreadActor(state: State, path: string) {
  const threadActors = getAllThreads(state).map(t => t.actor);

  const match = path.match(new RegExp(`(${threadActors.join("|")})\/(.*)`));
  return match && match[1];
}

type OuterState = { threads: ThreadsState };
