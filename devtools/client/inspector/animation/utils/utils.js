/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Check the equality timing effects from given animations.
 *
 * @param {Array} animations.
 * @param {Array} same to avobe.
 * @return {Boolean} true: same timing effects
 */
function isAllTimingEffectEqual(animationsA, animationsB) {
  if (animationsA.length !== animationsB.length) {
    return false;
  }

  for (let i = 0; i < animationsA.length; i++) {
    if (!isTimingEffectEqual(animationsA[i].state, animationsB[i].state)) {
      return false;
    }
  }
  return true;
}

/**
 * Check the equality given states as effect timing.
 *
 * @param {Object} state of animation.
 * @param {Object} same to avobe.
 * @return {Boolean} true: same effect timing
 */
function isTimingEffectEqual(stateA, stateB) {
  return stateA.delay === stateB.delay &&
         stateA.direction === stateB.direction &&
         stateA.duration === stateB.duration &&
         stateA.easing === stateB.easing &&
         stateA.endDelay === stateB.endDelay &&
         stateA.fill === stateB.fill &&
         stateA.iterationCount === stateB.iterationCount &&
         stateA.iterationStart === stateB.iterationStart;
}

module.exports.isAllTimingEffectEqual = isAllTimingEffectEqual;
module.exports.isTimingEffectEqual = isTimingEffectEqual;
