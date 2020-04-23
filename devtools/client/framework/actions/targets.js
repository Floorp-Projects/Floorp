/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
"use strict";

function registerTarget(targetFront, targetList) {
  const target = {
    actorID: targetFront.actorID,
    isMainTarget: targetFront === targetList?.targetFront,
    name: targetFront.name,
    type: targetList?.getTargetType(targetFront),
    url: targetFront.url,
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

module.exports = {
  registerTarget,
  unregisterTarget,
  selectTarget,
};
