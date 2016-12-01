/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { BATCH_ACTIONS, BATCH_ENABLE, BATCH_RESET } = require("../constants");

// ms
const REQUESTS_REFRESH_RATE = 50;

/**
 * Middleware that watches for actions with a "batch = true" value in their meta field.
 * These actions are queued and dispatched as one batch after a timeout.
 * Special actions that are handled by this middleware:
 * - BATCH_ENABLE can be used to enable and disable the batching.
 * - BATCH_RESET discards the actions that are currently in the queue.
 */
function batchingMiddleware(store) {
  return next => {
    let queuedActions = [];
    let enabled = true;
    let flushTask = null;

    return action => {
      if (action.type === BATCH_ENABLE) {
        return setEnabled(action.enabled);
      }

      if (action.type === BATCH_RESET) {
        return resetQueue();
      }

      if (action.meta && action.meta.batch) {
        if (!enabled) {
          next(action);
          return Promise.resolve();
        }

        queuedActions.push(action);

        if (!flushTask) {
          flushTask = new DelayedTask(flushActions, REQUESTS_REFRESH_RATE);
        }

        return flushTask.promise;
      }

      return next(action);
    };

    function setEnabled(value) {
      enabled = value;

      // If disabling the batching, flush the actions that have been queued so far
      if (!enabled && flushTask) {
        flushTask.runNow();
      }
    }

    function resetQueue() {
      queuedActions = [];

      if (flushTask) {
        flushTask.cancel();
        flushTask = null;
      }
    }

    function flushActions() {
      const actions = queuedActions;
      queuedActions = [];

      next({
        type: BATCH_ACTIONS,
        actions,
      });

      flushTask = null;
    }
  };
}

/**
 * Create a delayed task that calls the specified task function after a delay.
 */
function DelayedTask(taskFn, delay) {
  this._promise = new Promise((resolve, reject) => {
    this.runTask = (cancel) => {
      if (cancel) {
        reject("Task cancelled");
      } else {
        taskFn();
        resolve();
      }
      this.runTask = null;
    };
    this.timeout = setTimeout(this.runTask, delay);
  }).catch(console.error);
}

DelayedTask.prototype = {
  /**
   * Return a promise that is resolved after the task is performed or canceled.
   */
  get promise() {
    return this._promise;
  },

  /**
   * Cancel the execution of the task.
   */
  cancel() {
    clearTimeout(this.timeout);
    if (this.runTask) {
      this.runTask(true);
    }
  },

  /**
   * Execute the scheduled task immediately, without waiting for the timeout.
   * Resolves the promise correctly.
   */
  runNow() {
    clearTimeout(this.timeout);
    if (this.runTask) {
      this.runTask();
    }
  }
};

module.exports = batchingMiddleware;
