"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.promise = exports.PROMISE = undefined;

var _lodash = require("devtools/client/shared/vendor/lodash");

var _DevToolsUtils = require("../../../utils/DevToolsUtils");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
let seqIdVal = 1;

function seqIdGen() {
  return seqIdVal++;
}

function filterAction(action) {
  return (0, _lodash.fromPairs)((0, _lodash.toPairs)(action).filter(pair => pair[0] !== PROMISE));
}

function promiseMiddleware({
  dispatch,
  getState
}) {
  return next => action => {
    if (!(PROMISE in action)) {
      return next(action);
    }

    const promiseInst = action[PROMISE];
    const seqId = seqIdGen().toString(); // Create a new action that doesn't have the promise field and has
    // the `seqId` field that represents the sequence id

    action = { ...filterAction(action),
      seqId
    };
    dispatch({ ...action,
      status: "start"
    }); // Return the promise so action creators can still compose if they
    // want to.

    return new Promise((resolve, reject) => {
      promiseInst.then(value => {
        (0, _DevToolsUtils.executeSoon)(() => {
          dispatch({ ...action,
            status: "done",
            value: value
          });
          resolve(value);
        });
      }, error => {
        (0, _DevToolsUtils.executeSoon)(() => {
          dispatch({ ...action,
            status: "error",
            error: error.message || error
          });
          reject(error);
        });
      });
    });
  };
}

const PROMISE = exports.PROMISE = "@@dispatch/promise";
exports.promise = promiseMiddleware;