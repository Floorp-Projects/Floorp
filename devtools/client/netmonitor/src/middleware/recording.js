/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TOGGLE_RECORDING,
} = require("../constants");

/**
 * Start/stop HTTP traffic recording.
 */
function recordingMiddleware(store) {
  return next => action => {
    const res = next(action);
    if (action.type === TOGGLE_RECORDING) {
      // TODO connect/disconnect the backend.
    }
    return res;
  };
}

module.exports = recordingMiddleware;
