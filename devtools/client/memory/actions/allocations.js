/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  actions,
  ALLOCATION_RECORDING_OPTIONS,
} = require("resource://devtools/client/memory/constants.js");

exports.toggleRecordingAllocationStacks = function (commands) {
  return async function ({ dispatch, getState }) {
    dispatch({ type: actions.TOGGLE_RECORD_ALLOCATION_STACKS_START });

    if (commands.targetCommand.hasTargetWatcherSupport()) {
      await commands.targetConfigurationCommand.updateConfiguration({
        recordAllocations: getState().recordingAllocationStacks
          ? null
          : ALLOCATION_RECORDING_OPTIONS,
      });
    } else {
      const front = await commands.targetCommand.targetFront.getFront("memory");
      if (getState().recordingAllocationStacks) {
        await front.stopRecordingAllocations();
      } else {
        await front.startRecordingAllocations(ALLOCATION_RECORDING_OPTIONS);
      }
    }

    dispatch({ type: actions.TOGGLE_RECORD_ALLOCATION_STACKS_END });
  };
};
