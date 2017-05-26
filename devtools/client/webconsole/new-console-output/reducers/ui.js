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
  autoscroll: true,
  timestampsVisible: true,
});

function ui(state = new UiState(), action) {
  // Autoscroll should be set for all action types. If the last action was not message
  // add, then turn it off. This prevents us from scrolling after someone toggles a
  // filter, or to the bottom of the attachment when an expandable message at the bottom
  // of the list is expanded. It does depend on the MESSAGE_ADD action being the last in
  // its batch, though.
  // It also depends on REMOVED_MESSAGES_CLEAR action being sent after MESSAGE_ADD
  // if number of messages reached the maximum limit.
  let autoscroll = action.type == MESSAGE_ADD || action.type == REMOVED_MESSAGES_CLEAR;
  state = state.set("autoscroll", autoscroll);

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
