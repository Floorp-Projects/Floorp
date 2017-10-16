/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TOGGLE_RECORDING,
} = require("../constants");

const {
  getRecordingState,
} = require("../selectors/index");

/**
 * Start/stop HTTP traffic recording.
 *
 * The UI state of the toolbar toggle button is stored in UI
 * reducer and the backend connection is managed here in the
 * middleware.
 */
function recordingMiddleware(connector) {
  return store => next => action => {
    const res = next(action);

    // Pause/resume HTTP monitoring according to
    // the user action.
    if (action.type === TOGGLE_RECORDING) {
      let recording = getRecordingState(store.getState());
      if (recording) {
        connector.resume();
      } else {
        connector.pause();
      }
    }

    return res;
  };
}

module.exports = recordingMiddleware;
