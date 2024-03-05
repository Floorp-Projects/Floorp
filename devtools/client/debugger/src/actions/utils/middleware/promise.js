/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { executeSoon } from "../../../utils/DevToolsUtils";

import { pending, rejected, fulfilled } from "../../../utils/async-value";
export function asyncActionAsValue(action) {
  if (action.status === "start") {
    return pending();
  }
  if (action.status === "error") {
    return rejected(action.error);
  }
  return fulfilled(action.value);
}

let seqIdVal = 1;

function seqIdGen() {
  return seqIdVal++;
}

function promiseMiddleware({ dispatch }) {
  return next => action => {
    if (!(PROMISE in action)) {
      return next(action);
    }

    const seqId = seqIdGen().toString();
    const { [PROMISE]: promiseInst, ...originalActionProperties } = action;

    // Create a new action that doesn't have the promise field and has
    // the `seqId` field that represents the sequence id
    action = { ...originalActionProperties, seqId };

    dispatch({ ...action, status: "start" });

    // Return the promise so action creators can still compose if they
    // want to.
    return Promise.resolve(promiseInst)
      .finally(() => new Promise(resolve => executeSoon(resolve)))
      .then(
        value => {
          dispatch({ ...action, status: "done", value });
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
