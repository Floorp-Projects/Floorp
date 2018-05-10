"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.pauseOnExceptions = pauseOnExceptions;

var _promise = require("../utils/middleware/promise");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 *
 * @memberof actions/pause
 * @static
 */
function pauseOnExceptions(shouldPauseOnExceptions, shouldPauseOnCaughtExceptions) {
  return ({
    dispatch,
    client
  }) => {
    return dispatch({
      type: "PAUSE_ON_EXCEPTIONS",
      shouldPauseOnExceptions,
      shouldPauseOnCaughtExceptions,
      [_promise.PROMISE]: client.pauseOnExceptions(shouldPauseOnExceptions, shouldPauseOnCaughtExceptions)
    });
  };
}