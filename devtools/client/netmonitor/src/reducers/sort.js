/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { SORT_BY } = require("../constants");

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
      if (action.sortType == state.type) {
        state.ascending = !state.ascending;
      } else {
        state.type = action.sortType;
        state.ascending = true;
      }
      return state;
    }
    default:
      return state;
  }
}

module.exports = {
  Sort,
  sortReducer
};
