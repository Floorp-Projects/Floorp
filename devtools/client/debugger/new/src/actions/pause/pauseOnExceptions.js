"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.pauseOnExceptions = pauseOnExceptions;

var _promise = require("../utils/middleware/promise");

var _telemetry = require("../../utils/telemetry");

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
    /* eslint-disable camelcase */
    (0, _telemetry.recordEvent)("pause_on_exceptions", {
      exceptions: shouldPauseOnExceptions,
      // There's no "n" in the key below (#1463117)
      caught_exceptio: shouldPauseOnCaughtExceptions
    });
    /* eslint-enable camelcase */

    return dispatch({
      type: "PAUSE_ON_EXCEPTIONS",
      shouldPauseOnExceptions,
      shouldPauseOnCaughtExceptions,
      [_promise.PROMISE]: client.pauseOnExceptions(shouldPauseOnExceptions, shouldPauseOnCaughtExceptions)
    });
  };
}