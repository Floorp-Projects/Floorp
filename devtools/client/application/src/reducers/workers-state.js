/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_WORKERS,
} = require("../constants");

function WorkersState() {
  return {
    // Array of all service workers
    list: [],
  };
}

function workersReducer(state = WorkersState(), action) {
  switch (action.type) {
    case UPDATE_WORKERS: {
      const { workers } = action;
      return { list: workers };
    }

    default:
      return state;
  }
}

module.exports = {
  WorkersState,
  workersReducer,
};
