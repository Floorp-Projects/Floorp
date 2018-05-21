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
 * Check whether or not the given list of animations has an iteration count of infinite.
 *
 * @param {Array} animations.
 * @return {Boolean} true if there is an animation in the  list of animations
 *                   whose animation iteration count is infinite.
 */
function hasAnimationIterationCountInfinite(animations) {
  return animations.some(({state}) => !state.iterationCount);
}

/**
 * Check wether the animations are running at least one.
 *
 * @param {Array} animations.
 * @return {Boolean} true: running
 */
function hasRunningAnimation(animations) {
  return animations.some(({state}) => state.playState === "running");
}

exports.findOptimalTimeInterval = findOptimalTimeInterval;
exports.hasAnimationIterationCountInfinite = hasAnimationIterationCountInfinite;
exports.hasRunningAnimation = hasRunningAnimation;
