/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Immutable = require("devtools/client/shared/vendor/immutable");
const constants = require("devtools/client/webconsole/new-console-output/constants");

const FilterState = Immutable.Record({
  error: true,
  warn: true,
  info: true,
  log: true,
  searchText: ""
});

function filters(state = new FilterState(), action) {
  switch (action.type) {
    case constants.SEVERITY_FILTER:
      let {filter, toggled} = action;
      return state.set(filter, toggled);
    case constants.FILTERS_CLEAR:
      return new FilterState();
    case constants.MESSAGES_SEARCH:
      let {searchText} = action;
      return state.set("searchText", searchText);
  }

  return state;
}

exports.FilterState = FilterState;
exports.filters = filters;
