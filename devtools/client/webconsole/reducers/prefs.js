/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  WARNING_GROUPS_TOGGLE,
} = require("devtools/client/webconsole/constants");

const PrefState = overrides =>
  Object.freeze(
    Object.assign(
      {
        logLimit: 1000,
        sidebarToggle: false,
        jstermCodeMirror: false,
        groupWarnings: false,
        historyCount: 50,
        editor: false,
      },
      overrides
    )
  );

function prefs(state = PrefState(), action) {
  if (action.type === WARNING_GROUPS_TOGGLE) {
    return {
      ...state,
      groupWarnings: action.value,
    };
  }
  return state;
}

exports.PrefState = PrefState;
exports.prefs = prefs;
