/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const I = require("devtools/client/shared/vendor/immutable");
const {
  TOGGLE_FILTER,
  ENABLE_FILTER_ONLY,
} = require("../constants");

const FiltersTypes = I.Record({
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
  types: new FiltersTypes({ all: true }),
});

function toggleFilter(state, action) {
  let { filter } = action;
  let newState;

  // Ignore unknown filter type
  if (!state.has(filter)) {
    return state;
  }
  if (filter === "all") {
    return new FiltersTypes({ all: true });
  }

  newState = state.withMutations(types => {
    types.set("all", false);
    types.set(filter, !state.get(filter));
  });

  if (!newState.includes(true)) {
    newState = new FiltersTypes({ all: true });
  }

  return newState;
}

function enableFilterOnly(state, action) {
  let { filter } = action;

  // Ignore unknown filter type
  if (!state.has(filter)) {
    return state;
  }

  return new FiltersTypes({ [filter]: true });
}

function filters(state = new Filters(), action) {
  let types;
  switch (action.type) {
    case TOGGLE_FILTER:
      types = toggleFilter(state.types, action);
      return state.set("types", types);
    case ENABLE_FILTER_ONLY:
      types = enableFilterOnly(state.types, action);
      return state.set("types", types);
    default:
      return state;
  }
}

module.exports = filters;
