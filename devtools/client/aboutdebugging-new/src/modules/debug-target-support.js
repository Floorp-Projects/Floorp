/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DEBUG_TARGETS, DEBUG_TARGET_PANE, RUNTIMES } = require("../constants");

function isSupportedDebugTarget(runtimeType, debugTargetType) {
  if (runtimeType === RUNTIMES.THIS_FIREFOX) {
    return true;
  }

  return debugTargetType === DEBUG_TARGETS.TAB;
}
exports.isSupportedDebugTarget = isSupportedDebugTarget;

function isSupportedDebugTargetPane(runtimeType, debugTargetPaneKey) {
  if (runtimeType === RUNTIMES.THIS_FIREFOX) {
    return true;
  }

  return debugTargetPaneKey === DEBUG_TARGET_PANE.TAB;
}
exports.isSupportedDebugTargetPane = isSupportedDebugTargetPane;
