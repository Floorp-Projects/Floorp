/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const uuidgen = require("sdk/util/uuid").uuid;
const {
  entries, toObject, reportException, executeSoon
} = require("devtools/shared/DevToolsUtils");
const PROMISE = exports.PROMISE = "@@dispatch/promise";

function promiseMiddleware ({ dispatch, getState }) {
  return next => action => {
    if (!(PROMISE in action)) {
      return next(action);
    }

    const promise = action[PROMISE];
    const seqId = uuidgen().toString();

    // Create a new action that doesn't have the promise field and has
    // the `seqId` field that represents the sequence id
    action = Object.assign(
      toObject(entries(action).filter(pair => pair[0] !== PROMISE)), { seqId }
    );

    dispatch(Object.assign({}, action, { status: "start" }));

    promise.then(value => {
      executeSoon(() => {
        dispatch(Object.assign({}, action, {
          status: "done",
          value: value
        }));
      });
    }).catch(error => {
      executeSoon(() => {
        dispatch(Object.assign({}, action, {
          status: "error",
          error
        }));
      });
      reportException(`@@redux/middleware/promise#${action.type}`, error);
    });

    // Return the promise so action creators can still compose if they
    // want to.
    return promise;
  };
}

exports.promise = promiseMiddleware;
