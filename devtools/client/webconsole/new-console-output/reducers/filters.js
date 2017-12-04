/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const constants = require("devtools/client/webconsole/new-console-output/constants");

const FilterState = (overrides) => Object.freeze(
  cloneState(constants.DEFAULT_FILTERS_VALUES, overrides)
);

function filters(state = FilterState(), action) {
  switch (action.type) {
    case constants.FILTER_TOGGLE:
      const {filter} = action;
      const active = !state[filter];
      return cloneState(state, {[filter]: active});
    case constants.FILTERS_CLEAR:
      return FilterState();
    case constants.DEFAULT_FILTERS_RESET:
      const newState = cloneState(state);
      constants.DEFAULT_FILTERS.forEach(filterName => {
        newState[filterName] = constants.DEFAULT_FILTERS_VALUES[filterName];
      });
      return newState;
    case constants.FILTER_TEXT_SET:
      const {text} = action;
      return cloneState(state, {[constants.FILTERS.TEXT]: text});
  }

  return state;
}

function cloneState(state, overrides) {
  return Object.assign({}, state, overrides);
}

exports.FilterState = FilterState;
exports.filters = filters;
