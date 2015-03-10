/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Promise = require("sdk/core/promise");

/**
 * Wraps an event handler that responds to an actor request and triggers
 * them in the order they arrive. This is necessary because some handlers are async and
 * introduce race conditions. Example (bug 1141261), connect two nodes, and we have to wait
 * for both nodes to be created (another async handler), but the disconnect handler resolves faster
 * as it only has to wait for one node, and none of these timings are reliable.
 *
 * So queue them up here and execute them in order when the previous handler completes.
 *
 * Usage:
 *
 * var q = new Queue();
 *
 * let handler = q.addHandler(handler);
 * gFront.on("event", handler);
 */

function Queue () {
  this._messages = [];
  this._processing = false;
  this._currentProcess = null;
  this._process = this._process.bind(this);
  this._asyncHandler = this._asyncHandler.bind(this);
}
exports.Queue = Queue;

/**
 * Wrap a function that returns a new function
 * that executes the original in accordance with the queue.
 */
Queue.prototype.addHandler = function (fn) {
  return (...args) => {
    this._messages.push([fn, ...args]);
    if (!this._processing) {
      this._process();
    }
  }
};

Queue.prototype._process = function () {
  if (this._messages.length === 0) {
    this._processing = false;
    return;
  }

  this._processing = true;

  let [fn, ...args] = this._messages.shift();
  let result = fn.apply(null, args);
  if (result && result.then) {
    // Store the current process if its async, so we
    // can wait for it to finish if we clear.
    this._currentProcess = result.then(this._asyncHandler, this._asyncHandler);
  } else {
    this._process();
  }
};

/**
 * Used to wrap up an async message completion.
 */
Queue.prototype._asyncHandler = function () {
  this._currentProcess = null;
  this._process();
};

/**
 * Return the number of in-flight messages.
 */
Queue.prototype.getMessageCount = function () {
  return this._messages.length;
};

/**
 * Clear out all remaining messages. Returns a promise if there's
 * an async message currently being processed that resolves upon
 * the message completion.
 */
Queue.prototype.clear = function () {
  this._messages.length = 0;
  this._processing = false;

  // If currently waiting for the last async message to finish,
  // wait for it, then clear out the messages.
  if (this._currentProcess) {
    return this._currentProcess;
  }
  return Promise.resolve();
};
