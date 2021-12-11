/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  SORT_BY,
  RESET_COLUMNS,
} = require("devtools/client/netmonitor/src/constants");

function Sort() {
  return {
    // null means: sort by "waterfall", but don't highlight the table header
    type: null,
    ascending: true,
  };
}

function sortReducer(state = new Sort(), action) {
  switch (action.type) {
    case SORT_BY: {
      state = { ...state };
      if (action.sortType != null && action.sortType == state.type) {
        state.ascending = !state.ascending;
      } else {
        state.type = action.sortType;
        state.ascending = true;
      }
      return state;
    }

    case RESET_COLUMNS: {
      state = { ...state };
      state.type = null;
      state.ascending = true;
      return state;
    }

    default:
      return state;
  }
}

module.exports = {
  Sort,
  sortReducer,
};
