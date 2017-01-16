/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ENABLE_REQUEST_FILTER_TYPE_ONLY,
  TOGGLE_REQUEST_FILTER_TYPE,
  SET_REQUEST_FILTER_TEXT,
} = require("../constants");

/**
 * Toggle an existing filter type state.
 * If type 'all' is specified, all the other filter types are set to false.
 * Available filter types are defined in filters reducer.
 *
 * @param {string} filter - A filter type is going to be updated
 */
function toggleRequestFilterType(filter) {
  return {
    type: TOGGLE_REQUEST_FILTER_TYPE,
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
function enableRequestFilterTypeOnly(filter) {
  return {
    type: ENABLE_REQUEST_FILTER_TYPE_ONLY,
    filter,
  };
}

/**
 * Set filter text in toolbar.
 *
 * @param {string} text - A filter text is going to be set
 */
function setRequestFilterText(text) {
  return {
    type: SET_REQUEST_FILTER_TEXT,
    text,
  };
}

module.exports = {
  enableRequestFilterTypeOnly,
  toggleRequestFilterType,
  setRequestFilterText,
};
