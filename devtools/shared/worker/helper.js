/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env amd */

"use strict";

(function(root, factory) {
  if (typeof define === "function" && define.amd) {
    define(factory);
  } else if (typeof exports === "object") {
    module.exports = factory();
  } else {
    root.workerHelper = factory();
  }
})(this, function() {
  /**
   * This file is to only be included by ChromeWorkers. This exposes
   * a `createTask` function to workers to register tasks for communication
   * back to `devtools/shared/worker`.
   *
   * Tasks can be send their responses via a return value, either a primitive
   * or a promise.
   *
   * createTask(self, "average", function (data) {
   *   return data.reduce((sum, val) => sum + val, 0) / data.length;
   * });
   *
   * createTask(self, "average", function (data) {
   *   return new Promise((resolve, reject) => {
   *     resolve(data.reduce((sum, val) => sum + val, 0) / data.length);
   *   });
   * });
   *
   *
   * Errors:
   *
   * Returning an Error value, or if the returned promise is rejected, this
   * propagates to the DevToolsWorker as a rejected promise. If an error is
   * thrown in a synchronous function, that error is also propagated.
   */

  /**
   * Takes a worker's `self` object, a task name, and a function to
   * be called when that task is called. The task is called with the
   * passed in data as the first argument
   *
   * @param {object} self
   * @param {string} name
   * @param {function} fn
   */
  function createTask(self, name, fn) {
    // Store a hash of task name to function on the Worker
    if (!self._tasks) {
      self._tasks = {};
    }

    // Create the onmessage handler if not yet created.
    if (!self.onmessage) {
      self.onmessage = createHandler(self);
    }

    // Store the task on the worker.
    self._tasks[name] = fn;
  }

  /**
   * Creates the `self.onmessage` handler for a Worker.
   *
   * @param {object} self
   * @return {function}
   */
  function createHandler(self) {
    return function(e) {
      const { id, task, data } = e.data;
      const taskFn = self._tasks[task];

      if (!taskFn) {
        self.postMessage({ id, error: `Task "${task}" not found in worker.` });
        return;
      }

      try {
        handleResponse(taskFn(data));
      } catch (ex) {
        handleError(ex);
      }

      function handleResponse(response) {
        // If a promise
        if (response && typeof response.then === "function") {
          response.then(
            val => self.postMessage({ id, response: val }),
            handleError
          );
        } else if (response instanceof Error) {
          // If an error object
          handleError(response);
        } else {
          // If anything else
          self.postMessage({ id, response });
        }
      }

      function handleError(error = "Error") {
        try {
          // First, try and structured clone the error across directly.
          self.postMessage({ id, error });
        } catch (x) {
          // We could not clone whatever error value was given. Do our best to
          // stringify it.
          let errorString = `Error while performing task "${task}": `;

          try {
            errorString += error.toString();
          } catch (ex) {
            errorString += "<could not stringify error>";
          }

          if ("stack" in error) {
            try {
              errorString += "\n" + error.stack;
            } catch (err) {
              // Do nothing
            }
          }

          self.postMessage({ id, error: errorString });
        }
      }
    };
  }

  return { createTask: createTask };
});
