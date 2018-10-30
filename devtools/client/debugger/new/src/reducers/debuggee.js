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

export type WorkersList = List<Worker>;

type DebuggeeState = {
  workers: WorkersList
};

export const createDebuggeeState = makeRecord(
  ({
    workers: List()
  }: DebuggeeState)
);

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

type OuterState = { debuggee: DebuggeeState };

export function getWorker(state: OuterState, url: string) {
  return getWorkers(state).find(value => url);
}
