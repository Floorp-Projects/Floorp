/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { actions, ALLOCATION_RECORDING_OPTIONS } = require("../constants");

exports.toggleRecordingAllocationStacks = function (front) {
  return function* (dispatch, getState) {
    dispatch({ type: actions.TOGGLE_RECORD_ALLOCATION_STACKS_START });

    if (getState().recordingAllocationStacks) {
      yield front.stopRecordingAllocations();
    } else {
      yield front.startRecordingAllocations(ALLOCATION_RECORDING_OPTIONS);
    }

    dispatch({ type: actions.TOGGLE_RECORD_ALLOCATION_STACKS_END });
  };
};
