/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getAllFilters } = require("devtools/client/webconsole/selectors/filters");

const {
  FILTER_TEXT_SET,
  FILTER_TOGGLE,
  FILTERS_CLEAR,
  DEFAULT_FILTERS_RESET,
  PREFS,
  FILTERS,
  DEFAULT_FILTERS,
} = require("devtools/client/webconsole/constants");

function filterTextSet(text) {
  return {
    type: FILTER_TEXT_SET,
    text
  };
}

function filterToggle(filter) {
  return (dispatch, getState, {prefsService}) => {
    dispatch({
      type: FILTER_TOGGLE,
      filter,
    });
    const filterState = getAllFilters(getState());
    prefsService.setBoolPref(PREFS.FILTER[filter.toUpperCase()], filterState[filter]);
  };
}

function filtersClear() {
  return (dispatch, getState, {prefsService}) => {
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

/**
 * Set the default filters to their original values.
 * This is different than filtersClear where we reset
 * all the filters to their original values. Here we want
 * to keep non-default filters the user might have set.
 */
function defaultFiltersReset() {
  return (dispatch, getState, {prefsService}) => {
    // Get the state before dispatching so the action does not alter prefs reset.
    const filterState = getAllFilters(getState());

    dispatch({
      type: DEFAULT_FILTERS_RESET,
    });

    DEFAULT_FILTERS.forEach(filter => {
      if (filterState[filter] === false) {
        prefsService.clearUserPref(PREFS.FILTER[filter.toUpperCase()]);
      }
    });
  };
}

module.exports = {
  filterTextSet,
  filterToggle,
  filtersClear,
  defaultFiltersReset,
};
