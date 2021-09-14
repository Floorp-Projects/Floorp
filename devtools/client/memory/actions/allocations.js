/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  actions,
  ALLOCATION_RECORDING_OPTIONS,
} = require("devtools/client/memory/constants");

exports.toggleRecordingAllocationStacks = function(commands) {
  return async function({ dispatch, getState }) {
    dispatch({ type: actions.TOGGLE_RECORD_ALLOCATION_STACKS_START });

    const { targetConfigurationCommand } = commands;
    // @backward-compat { version 94 } Starts supporting "recordAllocations" configuration in order to better support SSTS
    // Could only be dropped once we support targetConfiguration for all toolboxes (either we drop the Browser content toolbox, or support the content process in watcher actor)
    // Once Fx93 support is removed, we can replace this check with `commands.targetCommand.hasTargetWatcherSupport()` to check if the watcher is supported.
    if (targetConfigurationCommand.supports("recordAllocations")) {
      await targetConfigurationCommand.updateConfiguration({
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
