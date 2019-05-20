/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Debuggee reducer
 * @module reducers/debuggee
 */

import { sortBy } from "lodash";
import { createSelector } from "reselect";

import { getDisplayName } from "../utils/workers";

import type { Selector } from "./types";
import type { MainThread, WorkerList, Thread } from "../types";
import type { Action } from "../actions/types";

export type DebuggeeState = {
  workers: WorkerList,
  mainThread: MainThread,
  isWebExtension: boolean,
};

export function initialDebuggeeState(): DebuggeeState {
  return {
    workers: [],
    mainThread: { actor: "", url: "", type: -1, name: "" },
    isWebExtension: false,
  };
}

export default function debuggee(
  state: DebuggeeState = initialDebuggeeState(),
  action: Action
): DebuggeeState {
  switch (action.type) {
    case "CONNECT":
      return {
        ...state,
        mainThread: { ...action.mainThread, name: L10N.getStr("mainThread") },
        isWebExtension: action.isWebExtension,
      };
    case "INSERT_WORKERS":
      return insertWorkers(state, action.workers);
    case "REMOVE_WORKERS":
      const { workers } = action;
      return {
        ...state,
        workers: state.workers.filter(w => !workers.includes(w.actor)),
      };
    case "NAVIGATE":
      return {
        ...initialDebuggeeState(),
        mainThread: action.mainThread,
      };
    default:
      return state;
  }
}

function insertWorkers(state, workers) {
  const formatedWorkers = workers.map(worker => ({
    ...worker,
    name: getDisplayName(worker),
  }));

  return {
    ...state,
    workers: [...state.workers, ...formatedWorkers],
  };
}

export const getWorkers = (state: OuterState) => state.debuggee.workers;

export const getWorkerCount = (state: OuterState) => getWorkers(state).length;

export function getWorkerByThread(state: OuterState, thread: string) {
  return getWorkers(state).find(worker => worker.actor == thread);
}

export function getMainThread(state: OuterState): MainThread {
  return state.debuggee.mainThread;
}

export function getDebuggeeUrl(state: OuterState): string {
  return getMainThread(state).url;
}

export const getThreads: Selector<Thread[]> = createSelector(
  getMainThread,
  getWorkers,
  (mainThread, workers) => [mainThread, ...sortBy(workers, getDisplayName)]
);

type OuterState = { debuggee: DebuggeeState };
