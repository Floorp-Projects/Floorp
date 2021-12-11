/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { actions, censusDisplays } = require("devtools/client/memory/constants");
const DEFAULT_CENSUS_DISPLAY = censusDisplays.coarseType;

const handlers = Object.create(null);

handlers[actions.SET_CENSUS_DISPLAY] = function(_, { display }) {
  return display;
};

module.exports = function(state = DEFAULT_CENSUS_DISPLAY, action) {
  const handle = handlers[action.type];
  if (handle) {
    return handle(state, action);
  }
  return state;
};
