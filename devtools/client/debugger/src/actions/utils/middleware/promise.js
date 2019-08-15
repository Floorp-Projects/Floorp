/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { fromPairs, toPairs } from "lodash";
import { executeSoon } from "../../../utils/DevToolsUtils";

import type { ThunkArgs } from "../../types";

type BasePromiseAction = {|
  +"@@dispatch/promise": Promise<mixed>,
|};

export type StartPromiseAction = {|
  ...BasePromiseAction,
  +status: "start",
|};

export type DonePromiseAction = {|
  ...BasePromiseAction,
  +status: "done",
  +value: any,
|};

export type ErrorPromiseAction = {|
  ...BasePromiseAction,
  +status: "error",
  +error: any,
|};

export type PromiseAction<Action, Value = any> =
  // | {| ...Action, "@@dispatch/promise": Promise<Object> |}
  | {|
      ...BasePromiseAction,
      ...Action,
      +status: "start",
      value: void,
    |}
  | {|
      ...BasePromiseAction,
      ...Action,
      +status: "done",
      +value: Value,
    |}
  | {|
      ...BasePromiseAction,
      ...Action,
      +status: "error",
      +error?: any,
      value: void,
    |};

let seqIdVal = 1;

function seqIdGen() {
  return seqIdVal++;
}

function filterAction(action: Object): Object {
  return fromPairs(toPairs(action).filter(pair => pair[0] !== PROMISE));
}

function promiseMiddleware({
  dispatch,
  getState,
}: ThunkArgs): Function | Promise<mixed> {
  return (next: Function) => (action: Object) => {
    if (!(PROMISE in action)) {
      return next(action);
    }

    const promiseInst = action[PROMISE];
    const seqId = seqIdGen().toString();

    // Create a new action that doesn't have the promise field and has
    // the `seqId` field that represents the sequence id
    action = { ...filterAction(action), seqId };

    dispatch({ ...action, status: "start" });

    // Return the promise so action creators can still compose if they
    // want to.
    return Promise.resolve(promiseInst)
      .finally(() => new Promise(resolve => executeSoon(resolve)))
      .then(
        value => {
          dispatch({ ...action, status: "done", value: value });
          return value;
        },
        error => {
          dispatch({
            ...action,
            status: "error",
            error: error.message || error,
          });
          return Promise.reject(error);
        }
      );
  };
}

export const PROMISE = "@@dispatch/promise";
export { promiseMiddleware as promise };
