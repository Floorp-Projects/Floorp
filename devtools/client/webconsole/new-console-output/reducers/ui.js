/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  FILTER_BAR_TOGGLE,
  MESSAGE_ADD,
  REMOVED_MESSAGES_CLEAR,
  TIMESTAMPS_TOGGLE
} = require("devtools/client/webconsole/new-console-output/constants");
const Immutable = require("devtools/client/shared/vendor/immutable");

const UiState = Immutable.Record({
  filterBarVisible: false,
  filteredMessageVisible: false,
  timestampsVisible: true,
});

function ui(state = new UiState(), action) {
  switch (action.type) {
    case FILTER_BAR_TOGGLE:
      return state.set("filterBarVisible", !state.filterBarVisible);
    case TIMESTAMPS_TOGGLE:
      return state.set("timestampsVisible", action.visible);
  }

  return state;
}

module.exports = {
  UiState,
  ui,
};
