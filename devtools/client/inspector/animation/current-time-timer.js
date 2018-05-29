/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * In animation inspector, the scrubber and the progress bar moves along the current time
 * of animation. However, the processing which sync with actual animations is heavy since
 * we have to communication by the actor. The role of this class is to make the pseudo
 * current time in animation inspector to proceed.
 */
class CurrentTimeTimer {
  /**
   * Constructor.
   *
   * @param {Object} timeScale
   * @param {Bool} shouldStopAfterEndTime
   *               If need to stop the timer after animation end time, set true.
   * @param {window} win
   *                 Be used for requestAnimationFrame and performance.
   * @param {Function} onUpdated
   *                   Listener function to get updating.
   *                   This function is called with 2 parameters.
   *                   1st: current time
   *                   2nd: if shouldStopAfterEndTime is true and
   *                        the current time is over the end time, true is given.
   */
  constructor(timeScale, shouldStopAfterEndTime, win, onUpdated) {
    this.baseCurrentTime = timeScale.getCurrentTime();
    this.endTime = timeScale.getDuration();
    this.timerStartTime = win.performance.now();

    this.shouldStopAfterEndTime = shouldStopAfterEndTime;
    this.onUpdated = onUpdated;
    this.win = win;
    this.next = this.next.bind(this);
  }

  destroy() {
    this.stop();
    this.onUpdated = null;
    this.win = null;
  }

  /**
   * Proceed the pseudo current time.
   */
  next() {
    if (this.doStop) {
      return;
    }

    const currentTime =
      this.baseCurrentTime + this.win.performance.now() - this.timerStartTime;

    if (this.endTime < currentTime && this.shouldStopAfterEndTime) {
      this.onUpdated(this.endTime, true);
      return;
    }

    this.onUpdated(currentTime);
    this.win.requestAnimationFrame(this.next);
  }

  start() {
    this.next();
  }

  stop() {
    this.doStop = true;
  }
}

module.exports = CurrentTimeTimer;
