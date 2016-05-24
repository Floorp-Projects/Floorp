/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
loader.lazyRequireGetter(this, "defer",
  "promise", true);

/**
 * @constructor Poller
 * Takes a function that is to be called on an interval,
 * and can be turned on and off via methods to execute `fn` on the interval
 * specified during `on`. If `fn` returns a promise, the polling waits for
 * that promise to resolve before waiting the interval to call again.
 *
 * Specify the `wait` duration between polling here, and optionally
 * an `immediate` boolean, indicating whether the function should be called
 * immediately when toggling on.
 *
 * @param {function} fn
 * @param {number} wait
 * @param {boolean?} immediate
 */
function Poller(fn, wait, immediate) {
  this._fn = fn;
  this._wait = wait;
  this._immediate = immediate;
  this._poll = this._poll.bind(this);
  this._preparePoll = this._preparePoll.bind(this);
}
exports.Poller = Poller;

/**
 * Returns a boolean indicating whether or not poller
 * is polling.
 *
 * @return {boolean}
 */
Poller.prototype.isPolling = function pollerIsPolling() {
  return !!this._timer;
};

/**
 * Turns polling on.
 *
 * @return {Poller}
 */
Poller.prototype.on = function pollerOn() {
  if (this._destroyed) {
    throw Error("Poller cannot be turned on after destruction.");
  }
  if (this._timer) {
    this.off();
  }
  this._immediate ? this._poll() : this._preparePoll();
  return this;
};

/**
 * Turns off polling. Returns a promise that resolves when
 * the last outstanding `fn` call finishes if it's an async function.
 *
 * @return {Promise}
 */
Poller.prototype.off = function pollerOff() {
  let { resolve, promise } = defer();
  if (this._timer) {
    clearTimeout(this._timer);
    this._timer = null;
  }

  // Settle an inflight poll call before resolving
  // if using a promise-backed poll function
  if (this._inflight) {
    this._inflight.then(resolve);
  } else {
    resolve();
  }
  return promise;
};

/**
 * Turns off polling and removes the reference to the poller function.
 * Resolves when the last outstanding `fn` call finishes if it's an async
 * function.
 */
Poller.prototype.destroy = function pollerDestroy() {
  return this.off().then(() => {
    this._destroyed = true;
    this._fn = null;
  });
};

Poller.prototype._preparePoll = function pollerPrepare() {
  this._timer = setTimeout(this._poll, this._wait);
};

Poller.prototype._poll = function pollerPoll() {
  let response = this._fn();
  if (response && typeof response.then === "function") {
    // Store the most recent in-flight polling
    // call so we can clean it up when disabling
    this._inflight = response;
    response.then(() => {
      // Only queue up the next call if poller was not turned off
      // while this async poll call was in flight.
      if (this._timer) {
        this._preparePoll();
      }
    });
  } else {
    this._preparePoll();
  }
};
