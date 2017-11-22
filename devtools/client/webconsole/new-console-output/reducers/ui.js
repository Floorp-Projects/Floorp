/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  FILTER_BAR_TOGGLE,
  INITIALIZE,
  PERSIST_TOGGLE,
  SELECT_NETWORK_MESSAGE_TAB,
  SIDEBAR_TOGGLE,
  TIMESTAMPS_TOGGLE,
} = require("devtools/client/webconsole/new-console-output/constants");
const Immutable = require("devtools/client/shared/vendor/immutable");

const {
  PANELS,
} = require("devtools/client/netmonitor/src/constants");

const UiState = Immutable.Record({
  filterBarVisible: false,
  initialized: false,
  networkMessageActiveTabId: PANELS.HEADERS,
  persistLogs: false,
  sidebarVisible: false,
  timestampsVisible: true,
});

function ui(state = new UiState(), action) {
  switch (action.type) {
    case FILTER_BAR_TOGGLE:
      return state.set("filterBarVisible", !state.filterBarVisible);
    case PERSIST_TOGGLE:
      return state.set("persistLogs", !state.persistLogs);
    case TIMESTAMPS_TOGGLE:
      return state.set("timestampsVisible", action.visible);
    case SELECT_NETWORK_MESSAGE_TAB:
      return state.set("networkMessageActiveTabId", action.id);
    case SIDEBAR_TOGGLE:
      return state.set("sidebarVisible", !state.sidebarVisible);
    case INITIALIZE:
      return state.set("initialized", true);
  }

  return state;
}

module.exports = {
  UiState,
  ui,
};
