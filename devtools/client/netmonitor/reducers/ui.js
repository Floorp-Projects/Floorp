/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const I = require("devtools/client/shared/vendor/immutable");
const {
  OPEN_SIDEBAR,
  TOGGLE_SIDEBAR,
} = require("../constants");

const Sidebar = I.Record({
  open: false,
});

const UI = I.Record({
  sidebar: new Sidebar(),
});

function openSidebar(state, action) {
  return state.setIn(["sidebar", "open"], action.open);
}

function toggleSidebar(state, action) {
  return state.setIn(["sidebar", "open"], !state.sidebar.open);
}

function ui(state = new UI(), action) {
  switch (action.type) {
    case OPEN_SIDEBAR:
      return openSidebar(state, action);
    case TOGGLE_SIDEBAR:
      return toggleSidebar(state, action);
    default:
      return state;
  }
}

module.exports = ui;
