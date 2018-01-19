/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// The maximum number of times we can loop before we find the optimal time interval in the
// timeline graph.
const OPTIMAL_TIME_INTERVAL_MAX_ITERS = 100;
// Time graduations should be multiple of one of these number.
const OPTIMAL_TIME_INTERVAL_MULTIPLES = [1, 2.5, 5];

/**
 * Find the optimal interval between time graduations in the animation timeline
 * graph based on a minimum time interval.
 *
 * @param {Number} minTimeInterval
 *                 Minimum time in ms in one interval
 * @return {Number} The optimal interval time in ms
 */
function findOptimalTimeInterval(minTimeInterval) {
  if (!minTimeInterval) {
    return 0;
  }

  let numIters = 0;
  let multiplier = 1;
  let interval;

  while (true) {
    for (let i = 0; i < OPTIMAL_TIME_INTERVAL_MULTIPLES.length; i++) {
      interval = OPTIMAL_TIME_INTERVAL_MULTIPLES[i] * multiplier;

      if (minTimeInterval <= interval) {
        return interval;
      }
    }

    if (++numIters > OPTIMAL_TIME_INTERVAL_MAX_ITERS) {
      return interval;
    }

    multiplier *= 10;
  }
}

/**
 * Check the equality of the given animations.
 *
 * @param {Array} animations.
 * @param {Array} same to above.
 * @return {Boolean} true: same animations
 */
function isAllAnimationEqual(animationsA, animationsB) {
  if (animationsA.length !== animationsB.length) {
    return false;
  }

  for (let i = 0; i < animationsA.length; i++) {
    const animationA = animationsA[i];
    const animationB = animationsB[i];

    if (animationA.actorID !== animationB.actorID ||
        !isTimingEffectEqual(animationsA[i].state, animationsB[i].state)) {
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

exports.findOptimalTimeInterval = findOptimalTimeInterval;
exports.isAllAnimationEqual = isAllAnimationEqual;
exports.isTimingEffectEqual = isTimingEffectEqual;
