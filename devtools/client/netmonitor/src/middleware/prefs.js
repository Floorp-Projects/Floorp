/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ENABLE_REQUEST_FILTER_TYPE_ONLY,
  RESET_COLUMNS,
  TOGGLE_COLUMN,
  TOGGLE_REQUEST_FILTER_TYPE,
} = require("../constants");
const { Prefs } = require("../utils/prefs");
const { getRequestFilterTypes } = require("../selectors/index");

/**
  * Update the relevant prefs when:
  *   - a column has been toggled
  *   - a filter type has been set
  */
function prefsMiddleware(store) {
  return next => action => {
    const res = next(action);
    switch (action.type) {
      case ENABLE_REQUEST_FILTER_TYPE_ONLY:
      case TOGGLE_REQUEST_FILTER_TYPE:
        Prefs.filters = getRequestFilterTypes(store.getState())
          .filter(([type, check]) => check)
          .map(([type, check]) => type);
        break;

      case TOGGLE_COLUMN:
        Prefs.hiddenColumns = [...store.getState().ui.columns]
          .filter(([column, shown]) => !shown)
          .map(([column, shown]) => column);
        break;

      case RESET_COLUMNS:
        Prefs.hiddenColumns = [];
        break;
    }
    return res;
  };
}

module.exports = prefsMiddleware;
