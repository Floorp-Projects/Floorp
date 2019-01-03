/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Debuggee reducer
 * @module reducers/debuggee
 */

import { List } from "immutable";
import type { Record } from "../utils/makeRecord";
import type { Worker } from "../types";
import type { Action } from "../actions/types";
import makeRecord from "../utils/makeRecord";
import { getMainThread } from "./pause";

export type WorkersList = List<Worker>;

type DebuggeeState = {
  workers: WorkersList
};

export const createDebuggeeState: () => Record<DebuggeeState> = makeRecord({
  workers: List()
});

export default function debuggee(
  state: Record<DebuggeeState> = createDebuggeeState(),
  action: Action
): Record<DebuggeeState> {
  switch (action.type) {
    case "SET_WORKERS":
      return state.set("workers", List(action.workers));
    default:
      return state;
  }
}

export const getWorkers = (state: OuterState) => state.debuggee.workers;

export const getWorkerDisplayName = (state: OuterState, thread: string) => {
  let index = 1;
  for (const { actor } of state.debuggee.workers) {
    if (actor == thread) {
      return `Worker #${index}`;
    }
    index++;
  }
  return "";
};

export const isValidThread = (state: OuterState, thread: string) => {
  if (thread == getMainThread((state: any))) {
    return true;
  }
  for (const { actor } of state.debuggee.workers) {
    if (actor == thread) {
      return true;
    }
  }
  return false;
};

type OuterState = { debuggee: DebuggeeState };
