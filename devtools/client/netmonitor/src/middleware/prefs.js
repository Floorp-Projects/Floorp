/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const {
  ENABLE_REQUEST_FILTER_TYPE_ONLY,
  RESET_COLUMNS,
  TOGGLE_COLUMN,
  TOGGLE_REQUEST_FILTER_TYPE,
  ENABLE_PERSISTENT_LOGS,
  DISABLE_BROWSER_CACHE,
} = require("../constants");

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
        let filters = store.getState().filters.requestFilterTypes
          .entrySeq().toArray()
          .filter(([type, check]) => check)
          .map(([type, check]) => type);
        Services.prefs.setCharPref(
          "devtools.netmonitor.filters", JSON.stringify(filters));
        break;
      case ENABLE_PERSISTENT_LOGS:
        Services.prefs.setBoolPref(
          "devtools.netmonitor.persistlog", store.getState().ui.persistentLogsEnabled);
        break;
      case DISABLE_BROWSER_CACHE:
        Services.prefs.setBoolPref(
          "devtools.cache.disabled", store.getState().ui.browserCacheDisabled);
        break;
      case TOGGLE_COLUMN:
      case RESET_COLUMNS:
        let visibleColumns = [];
        let columns = store.getState().ui.columns;
        for (let column in columns) {
          if (columns[column]) {
            visibleColumns.push(column);
          }
        }
        Services.prefs.setCharPref(
          "devtools.netmonitor.visibleColumns", JSON.stringify(visibleColumns));
        break;
    }
    return res;
  };
}

module.exports = prefsMiddleware;
