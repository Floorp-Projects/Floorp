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
  text: ""
});

function filters(state = new FilterState(), action) {
  switch (action.type) {
    case constants.FILTER_TOGGLE:
      const {filter} = action;
      const active = !state.get(filter);
      return state.set(filter, active);
    case constants.FILTERS_CLEAR:
      return new FilterState();
    case constants.FILTER_TEXT_SET:
      let {text} = action;
      return state.set("text", text);
  }

  return state;
}

exports.FilterState = FilterState;
exports.filters = filters;
