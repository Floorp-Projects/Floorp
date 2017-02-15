/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ENABLE_REQUEST_FILTER_TYPE_ONLY,
  TOGGLE_REQUEST_FILTER_TYPE,
} = require("../constants");
const { Prefs } = require("../prefs");
const { getActiveFilters } = require("../selectors/index");

/**
  * Whenever the User clicks on a filter in the network monitor, save the new
  * filters for future tabs
  */
function prefsMiddleware(store) {
  return next => action => {
    const res = next(action);
    if (action.type === ENABLE_REQUEST_FILTER_TYPE_ONLY ||
        action.type === TOGGLE_REQUEST_FILTER_TYPE) {
      Prefs.filters = getActiveFilters(store.getState());
    }
    return res;
  };
}

module.exports = prefsMiddleware;
