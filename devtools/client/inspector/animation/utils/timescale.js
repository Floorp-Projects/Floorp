/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  getFormatStr,
} = require("resource://devtools/client/inspector/animation/utils/l10n.js");

// If total duration for all animations is eqaul to or less than
// TIME_FORMAT_MAX_DURATION_IN_MS, the text which expresses time is in milliseconds,
// and seconds otherwise. Use in formatTime function.
const TIME_FORMAT_MAX_DURATION_IN_MS = 4000;

/**
 * TimeScale object holds the total duration, start time and end time and zero position
 * time information for all animations which should be displayed, and is used to calculate
 * the displayed area for each animation.
 */
class TimeScale {
  constructor(animations) {
    let resultCurrentTime = -Number.MAX_VALUE;
    let resultMinStartTime = Infinity;
    let resultMaxEndTime = 0;
    let resultZeroPositionTime = 0;

    for (const animation of animations) {
      const {
        currentTime,
        currentTimeAtCreated,
        delay,
        endTime,
        startTimeAtCreated,
      } = animation.state.absoluteValues;
      let { startTime } = animation.state.absoluteValues;

      const negativeDelay = Math.min(delay, 0);
      let zeroPositionTime = 0;

      // To shift the zero position time is the following two patterns.
      //  * Animation has negative current time which is smaller than negative delay.
      //  * Animation has negative delay.
      // Furthermore, we should override the zero position time if we will need to
      // expand the duration due to this negative current time or negative delay of
      // this target animation.
      if (currentTimeAtCreated < negativeDelay) {
        startTime = startTimeAtCreated;
        zeroPositionTime = Math.abs(currentTimeAtCreated);
      } else if (negativeDelay < 0) {
        zeroPositionTime = Math.abs(negativeDelay);
      }

      if (startTime < resultMinStartTime) {
        resultMinStartTime = startTime;
        // Override the previous calculated zero position only if the duration will be
        // expanded.
        resultZeroPositionTime = zeroPositionTime;
      } else {
        resultZeroPositionTime = Math.max(
          resultZeroPositionTime,
          zeroPositionTime
        );
      }

      resultMaxEndTime = Math.max(resultMaxEndTime, endTime);
      resultCurrentTime = Math.max(resultCurrentTime, currentTime);
    }

    this.minStartTime = resultMinStartTime;
    this.maxEndTime = resultMaxEndTime;
    this.currentTime = resultCurrentTime;
    this.zeroPositionTime = resultZeroPositionTime;
  }

  /**
   *  Convert a distance in % to a time, in the current time scale. The time
   *  will be relative to the zero position time.
   *  i.e., If zeroPositionTime will be negative and specified time is shorter
   *  than the absolute value of zero position time, relative time will be
   *  negative time.
   *
   * @param {Number} distance
   * @return {Number}
   */
  distanceToRelativeTime(distance) {
    return (this.getDuration() * distance) / 100 - this.zeroPositionTime;
  }

  /**
   * Depending on the time scale, format the given time as milliseconds or
   * seconds.
   *
   * @param {Number} time
   * @return {String} The formatted time string.
   */
  formatTime(time) {
    // Ignore negative zero
    if (Math.abs(time) < 1 / 1000) {
      time = 0.0;
    }

    // Format in milliseconds if the total duration is short enough.
    if (this.getDuration() <= TIME_FORMAT_MAX_DURATION_IN_MS) {
      return getFormatStr("timeline.timeGraduationLabel", time.toFixed(0));
    }

    // Otherwise format in seconds.
    return getFormatStr("player.timeLabel", (time / 1000).toFixed(1));
  }

  /**
   * Return entire animations duration.
   *
   * @return {Number} duration
   */
  getDuration() {
    return this.maxEndTime - this.minStartTime;
  }

  /**
   * Return current time of this time scale represents.
   *
   * @return {Number}
   */
  getCurrentTime() {
    return this.currentTime - this.minStartTime;
  }

  /**
   * Return end time of given animation.
   * This time does not include playbackRate and cratedTime.
   * Also, if the animation has infinite iterations, this returns Infinity.
   *
   * @param {Object} animation
   * @return {Numbber} end time
   */
  getEndTime({ state }) {
    return state.iterationCount
      ? state.delay + state.duration * state.iterationCount + state.endDelay
      : Infinity;
  }
}

module.exports = TimeScale;
