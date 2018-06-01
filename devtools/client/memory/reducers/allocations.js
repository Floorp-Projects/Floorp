/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { assert } = require("devtools/shared/DevToolsUtils");
const { actions } = require("../constants");

const handlers = Object.create(null);

handlers[actions.TOGGLE_RECORD_ALLOCATION_STACKS_START] = function(state, action) {
  assert(!state.togglingInProgress,
         "Changing recording state must not be reentrant.");

  return {
    recording: !state.recording,
    togglingInProgress: true,
  };
};

handlers[actions.TOGGLE_RECORD_ALLOCATION_STACKS_END] = function(state, action) {
  assert(state.togglingInProgress,
         "Should not complete changing recording state if we weren't changing "
         + "recording state already.");

  return {
    recording: state.recording,
    togglingInProgress: false,
  };
};

const DEFAULT_ALLOCATIONS_STATE = {
  recording: false,
  togglingInProgress: false
};

module.exports = function(state = DEFAULT_ALLOCATIONS_STATE, action) {
  const handle = handlers[action.type];
  if (handle) {
    return handle(state, action);
  }
  return state;
};
