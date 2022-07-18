/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
"use strict";

function getToolboxTargets(state) {
  return state.targets;
}

function getSelectedTarget(state) {
  return state.selected;
}

function getLastTargetRefresh(state) {
  return state.lastTargetRefresh;
}

exports.getToolboxTargets = getToolboxTargets;
exports.getSelectedTarget = getSelectedTarget;
exports.getLastTargetRefresh = getLastTargetRefresh;
