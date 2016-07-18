/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const constants = require("devtools/client/webconsole/new-console-output/constants");
const {Filters} = require("devtools/client/webconsole/new-console-output/store");

function filters(state = new Filters(), action) {
  switch (action.type) {
    case constants.SEVERITY_FILTER:
      let {filter, toggled} = action;
      return state.set(filter, toggled);
    case constants.FILTERS_CLEAR:
      return new Filters();
    case constants.MESSAGES_SEARCH:
      let {searchText} = action;
      return state.set("searchText", searchText);
  }

  return state;
}

exports.filters = filters;
