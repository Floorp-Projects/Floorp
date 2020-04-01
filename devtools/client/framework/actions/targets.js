/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
"use strict";

const TARGET_TYPES = {
  MAIN_TARGET: "mainTarget",
  CONTENT_PROCESS: "contentProcess",
  WORKER: "worker",
};

function registerTarget(targetFront) {
  const target = {
    actorID: targetFront.actorID,
    url: targetFront.url,
    type: getTargetType(targetFront),
    name: targetFront.name,
    serviceWorkerStatus: targetFront.debuggerServiceWorkerStatus,
    _targetFront: targetFront,
  };
  return { type: "REGISTER_TARGET", target };
}

function clearTarget(targetFront) {
  return { type: "CLEAR_TARGET", targetFront };
}

/**
 *
 * @param {String} targetActorID: The actorID of the target we want to select.
 */
function selectTarget(targetActorID) {
  return function(dispatch) {
    dispatch({ type: "SELECT_TARGET", targetActorID });
  };
}

function getTargetType(target) {
  if (target.isWorkerTarget) {
    return TARGET_TYPES.WORKER;
  }

  if (target.isContentProcess) {
    return TARGET_TYPES.CONTENT_PROCESS;
  }

  return TARGET_TYPES.MAIN_TARGET;
}

module.exports = {
  registerTarget,
  clearTarget,
  selectTarget,
  TARGET_TYPES,
};
