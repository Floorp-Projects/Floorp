/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getAllFilters } = require("devtools/client/webconsole/new-console-output/selectors/filters");
const Services = require("Services");

const {
  FILTER_TEXT_SET,
  FILTER_TOGGLE,
  FILTERS_CLEAR,
  PREFS,
} = require("devtools/client/webconsole/new-console-output/constants");

function filterTextSet(text) {
  return {
    type: FILTER_TEXT_SET,
    text
  };
}

function filterToggle(filter) {
  return (dispatch, getState) => {
    dispatch({
      type: FILTER_TOGGLE,
      filter,
    });
    const filterState = getAllFilters(getState());
    Services.prefs.setBoolPref(PREFS.FILTER[filter.toUpperCase()],
      filterState.get(filter));
  };
}

function filtersClear() {
  return (dispatch, getState) => {
    dispatch({
      type: FILTERS_CLEAR,
    });

    const filterState = getAllFilters(getState());
    for (let filter in filterState) {
      Services.prefs.clearUserPref(PREFS.FILTER[filter.toUpperCase()]);
    }
  };
}

module.exports = {
  filterTextSet,
  filterToggle,
  filtersClear
};
