/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  getAllFilters,
} = require("devtools/client/webconsole/selectors/filters");

const {
  FILTER_TEXT_SET,
  FILTER_TOGGLE,
  FILTERS_CLEAR,
  PREFS,
  FILTERS,
} = require("devtools/client/webconsole/constants");

function filterTextSet(text) {
  return {
    type: FILTER_TEXT_SET,
    text,
  };
}

function filterToggle(filter) {
  return ({ dispatch, getState, prefsService }) => {
    dispatch({
      type: FILTER_TOGGLE,
      filter,
    });
    const filterState = getAllFilters(getState());
    prefsService.setBoolPref(
      PREFS.FILTER[filter.toUpperCase()],
      filterState[filter]
    );
  };
}

function filtersClear() {
  return ({ dispatch, getState, prefsService }) => {
    dispatch({
      type: FILTERS_CLEAR,
    });

    const filterState = getAllFilters(getState());
    for (const filter in filterState) {
      if (filter !== FILTERS.TEXT) {
        prefsService.clearUserPref(PREFS.FILTER[filter.toUpperCase()]);
      }
    }
  };
}

module.exports = {
  filterTextSet,
  filterToggle,
  filtersClear,
};
