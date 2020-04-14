/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
"use strict";

const TARGET_TYPES = {
  MAIN_TARGET: "mainTarget",
  FRAME: "frame",
  CONTENT_PROCESS: "contentProcess",
  WORKER: "worker",
};

function registerTarget(targetFront, targetList) {
  const target = {
    actorID: targetFront.actorID,
    url: targetFront.url,
    type: getTargetType(targetFront, targetList),
    name: targetFront.name,
    serviceWorkerStatus: targetFront.debuggerServiceWorkerStatus,
    _targetFront: targetFront,
  };
  return { type: "REGISTER_TARGET", target };
}

function unregisterTarget(targetFront) {
  return { type: "UNREGISTER_TARGET", targetFront };
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

function getTargetType(target, targetList) {
  if (target.isWorkerTarget) {
    return TARGET_TYPES.WORKER;
  }

  if (target.isContentProcess) {
    return TARGET_TYPES.CONTENT_PROCESS;
  }

  if (targetList?.targetFront === target) {
    return TARGET_TYPES.MAIN_TARGET;
  }

  return TARGET_TYPES.FRAME;
}

module.exports = {
  registerTarget,
  unregisterTarget,
  selectTarget,
  TARGET_TYPES,
};
