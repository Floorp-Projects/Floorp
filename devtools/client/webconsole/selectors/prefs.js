/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

function getAllPrefs(state) {
  return state.prefs;
}

function getLogLimit(state) {
  return state.prefs.logLimit;
}

module.exports = {
  getAllPrefs,
  getLogLimit,
};
