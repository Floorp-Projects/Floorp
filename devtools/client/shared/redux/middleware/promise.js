/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

loader.lazyRequireGetter(
  this,
  "generateUUID",
  "devtools/shared/generate-uuid",
  true
);
loader.lazyRequireGetter(
  this,
  ["entries", "executeSoon", "toObject"],
  "devtools/shared/DevToolsUtils",
  true
);

const PROMISE = (exports.PROMISE = "@@dispatch/promise");

function promiseMiddleware({ dispatch, getState }) {
  return next => action => {
    if (!(PROMISE in action)) {
      return next(action);
    }
    // Return the promise so action creators can still compose if they
    // want to.
    return new Promise((resolve, reject) => {
      const promiseInst = action[PROMISE];
      const seqId = generateUUID().toString();

      // Create a new action that doesn't have the promise field and has
      // the `seqId` field that represents the sequence id
      action = Object.assign(
        toObject(entries(action).filter(pair => pair[0] !== PROMISE)),
        { seqId }
      );

      dispatch(Object.assign({}, action, { status: "start" }));

      promiseInst.then(
        value => {
          executeSoon(() => {
            dispatch(
              Object.assign({}, action, {
                status: "done",
                value,
              })
            );
            resolve(value);
          });
        },
        error => {
          executeSoon(() => {
            dispatch(
              Object.assign({}, action, {
                status: "error",
                error: error.message || error,
              })
            );
            reject(error);
          });
        }
      );
    });
  };
}

exports.promise = promiseMiddleware;
