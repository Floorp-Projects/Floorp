/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const PrefState = (overrides) => Object.freeze(Object.assign({
  logLimit: 1000,
  sidebarToggle: false,
  jstermCodeMirror: false,
  groupWarnings: false,
  filterContentMessages: false,
  historyCount: 50,
}, overrides));

function prefs(state = PrefState(), action) {
  return state;
}

exports.PrefState = PrefState;
exports.prefs = prefs;
