/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const constants = require("resource://devtools/client/webconsole/constants.js");

const FilterState = overrides =>
  Object.freeze(cloneState(constants.DEFAULT_FILTERS_VALUES, overrides));

function filters(state = FilterState(), action) {
  switch (action.type) {
    case constants.FILTER_TOGGLE:
      const { filter } = action;
      const active = !state[filter];
      return cloneState(state, { [filter]: active });
    case constants.FILTERS_CLEAR:
      return FilterState();
    case constants.FILTER_TEXT_SET:
      const { text } = action;
      return cloneState(state, { [constants.FILTERS.TEXT]: text });
  }

  return state;
}

function cloneState(state, overrides) {
  return Object.assign({}, state, overrides);
}

exports.FilterState = FilterState;
exports.filters = filters;
