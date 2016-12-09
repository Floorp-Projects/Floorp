/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const I = require("devtools/client/shared/vendor/immutable");
const {
  TOGGLE_FILTER_TYPE,
  ENABLE_FILTER_TYPE_ONLY,
  SET_FILTER_TEXT,
} = require("../constants");

const FilterTypes = I.Record({
  all: false,
  html: false,
  css: false,
  js: false,
  xhr: false,
  fonts: false,
  images: false,
  media: false,
  flash: false,
  ws: false,
  other: false,
});

const Filters = I.Record({
  types: new FilterTypes({ all: true }),
  url: "",
});

function toggleFilterType(state, action) {
  let { filter } = action;
  let newState;

  // Ignore unknown filter type
  if (!state.has(filter)) {
    return state;
  }
  if (filter === "all") {
    return new FilterTypes({ all: true });
  }

  newState = state.withMutations(types => {
    types.set("all", false);
    types.set(filter, !state.get(filter));
  });

  if (!newState.includes(true)) {
    newState = new FilterTypes({ all: true });
  }

  return newState;
}

function enableFilterTypeOnly(state, action) {
  let { filter } = action;

  // Ignore unknown filter type
  if (!state.has(filter)) {
    return state;
  }

  return new FilterTypes({ [filter]: true });
}

function filters(state = new Filters(), action) {
  switch (action.type) {
    case TOGGLE_FILTER_TYPE:
      return state.set("types", toggleFilterType(state.types, action));
    case ENABLE_FILTER_TYPE_ONLY:
      return state.set("types", enableFilterTypeOnly(state.types, action));
    case SET_FILTER_TEXT:
      return state.set("url", action.url);
    default:
      return state;
  }
}

module.exports = filters;
