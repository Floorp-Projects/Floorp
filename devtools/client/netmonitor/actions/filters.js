/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  TOGGLE_FILTER,
  ENABLE_FILTER_ONLY,
} = require("../constants");

/**
 * Toggle an existing filter type state.
 * If type 'all' is specified, all the other filter types are set to false.
 * Available filter types are defined in filters reducer.
 *
 * @param {string} filter - A filter type is going to be updated
 */
function toggleFilter(filter) {
  return {
    type: TOGGLE_FILTER,
    filter,
  };
}

/**
 * Enable filter type exclusively.
 * Except filter type is set to true, all the other filter types are set
 * to false.
 * Available filter types are defined in filters reducer.
 *
 * @param {string} filter - A filter type is going to be updated
 */
function enableFilterOnly(filter) {
  return {
    type: ENABLE_FILTER_ONLY,
    filter,
  };
}

module.exports = {
  toggleFilter,
  enableFilterOnly,
};
