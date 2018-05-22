/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getFormatStr } = require("./l10n");

// If total duration for all animations is eqaul to or less than
// TIME_FORMAT_MAX_DURATION_IN_MS, the text which expresses time is in milliseconds,
// and seconds otherwise. Use in formatTime function.
const TIME_FORMAT_MAX_DURATION_IN_MS = 4000;

/**
 * TimeScale object holds the total duration, start time and end time information for all
 * animations which should be displayed, and is used to calculate the displayed area for
 * each animation.
 */
class TimeScale {
  constructor(animations) {
    if (!animations.every(animation => animation.state.createdTime)) {
      // Backward compatibility for createdTime.
      return this._initializeWithoutCreatedTime(animations);
    }

    let animationsCurrentTime = -Number.MAX_VALUE;
    let minStartTime = Infinity;
    let maxEndTime = 0;

    for (const animation of animations) {
      const {
        createdTime,
        currentTime,
        delay,
        duration,
        endDelay = 0,
        iterationCount,
        playbackRate,
      } = animation.state;

      const toRate = v => v / playbackRate;
      const startTime = createdTime + toRate(Math.min(delay, 0));
      const endTime = createdTime +
                      toRate(delay +
                             duration * (iterationCount || 1) +
                             Math.max(endDelay, 0));
      minStartTime = Math.min(minStartTime, startTime);
      maxEndTime = Math.max(maxEndTime, endTime);
      animationsCurrentTime =
        Math.max(animationsCurrentTime, createdTime + toRate(currentTime));
    }

    this.minStartTime = minStartTime;
    this.maxEndTime = maxEndTime;
    this.currentTime = animationsCurrentTime;
  }

  /**
   * Same as the constructor but doesn't use the animation's createdTime property
   * which has only been added in FF62, for backward compatbility reasons.
   *
   * @param {Array} animations
   */
  _initializeWithoutCreatedTime(animations) {
    this.minStartTime = Infinity;
    this.maxEndTime = 0;
    this.documentCurrentTime = 0;

    for (const animation of animations) {
      const {
        delay,
        documentCurrentTime,
        duration,
        endDelay = 0,
        iterationCount,
        playbackRate,
        previousStartTime = 0,
      } = animation.state;

      const toRate = v => v / playbackRate;
      const minZero = v => Math.max(v, 0);
      const rateRelativeDuration =
        toRate(duration * (!iterationCount ? 1 : iterationCount));
      // Negative-delayed animations have their startTimes set such that we would
      // be displaying the delay outside the time window if we didn't take it into
      // account here.
      const relevantDelay = delay < 0 ? toRate(delay) : 0;
      const startTime = toRate(minZero(delay)) +
                        rateRelativeDuration +
                        endDelay;
      this.minStartTime = Math.min(
        this.minStartTime,
        previousStartTime +
        relevantDelay +
        Math.min(startTime, 0)
      );
      const length = toRate(delay) + rateRelativeDuration + toRate(minZero(endDelay));
      const endTime = previousStartTime + length;
      this.maxEndTime = Math.max(this.maxEndTime, endTime);

      this.documentCurrentTime = Math.max(this.documentCurrentTime, documentCurrentTime);
    }
  }

  /**
   * Convert a distance in % to a time, in the current time scale.
   *
   * @param {Number} distance
   * @return {Number}
   */
  distanceToTime(distance) {
    return this.minStartTime + (this.getDuration() * distance / 100);
  }

  /**
   * Convert a distance in % to a time, in the current time scale.
   * The time will be relative to the current minimum start time.
   *
   * @param {Number} distance
   * @return {Number}
   */
  distanceToRelativeTime(distance) {
    const time = this.distanceToTime(distance);
    return time - this.minStartTime;
  }

  /**
   * Depending on the time scale, format the given time as milliseconds or
   * seconds.
   *
   * @param {Number} time
   * @return {String} The formatted time string.
   */
  formatTime(time) {
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
}

module.exports = TimeScale;
