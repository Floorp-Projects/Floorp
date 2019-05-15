/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  BATCH_ACTIONS,
  BATCH_ENABLE,
  BATCH_RESET,
  BATCH_FLUSH,
} = require("../constants");

/**
 * Process multiple actions at once as part of one dispatch, and produce only one
 * state update at the end. This action is not processed by any reducer, but by a
 * special store enhancer.
 */
function batchActions(actions) {
  return {
    type: BATCH_ACTIONS,
    actions,
  };
}

function batchEnable(enabled) {
  return {
    type: BATCH_ENABLE,
    enabled,
  };
}

function batchReset() {
  return {
    type: BATCH_RESET,
  };
}

function batchFlush() {
  return {
    type: BATCH_FLUSH,
  };
}

module.exports = {
  batchActions,
  batchEnable,
  batchReset,
  batchFlush,
};
