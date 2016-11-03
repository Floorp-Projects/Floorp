/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const I = require("devtools/client/shared/vendor/immutable");
const {
  DISABLE_TOGGLE_BUTTON,
  SHOW_SIDEBAR,
  TOGGLE_SIDEBAR,
} = require("../constants");

const SidebarState = I.Record({
  toggleButtonDisabled: true,
  visible: false,
});

function disableToggleButton(state, action) {
  return state.set("toggleButtonDisabled", action.disabled);
}

function showSidebar(state, action) {
  return state.set("visible", action.visible);
}

function toggleSidebar(state, action) {
  return state.set("visible", !state.visible);
}

function sidebar(state = new SidebarState(), action) {
  switch (action.type) {
    case DISABLE_TOGGLE_BUTTON:
      return disableToggleButton(state, action);
    case SHOW_SIDEBAR:
      return showSidebar(state, action);
    case TOGGLE_SIDEBAR:
      return toggleSidebar(state, action);
    default:
      return state;
  }
}

module.exports = sidebar;
