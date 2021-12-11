/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

function WorkerDispatcher() {
  this.msgId = 1;
  this.worker = null;
}

WorkerDispatcher.prototype = {
  start(url, win) {
    this.worker = new win.Worker(url);
    this.worker.onerror = () => {
      console.error(`Error in worker ${url}`);
    };
  },

  stop() {
    if (!this.worker) {
      return;
    }

    this.worker.terminate();
    this.worker = null;
  },

  task(method, { queue = false } = {}) {
    const calls = [];
    const push = args => {
      return new Promise((resolve, reject) => {
        if (queue && calls.length === 0) {
          Promise.resolve().then(flush);
        }

        calls.push([args, resolve, reject]);

        if (!queue) {
          flush();
        }
      });
    };

    const flush = () => {
      const items = calls.slice();
      calls.length = 0;

      if (!this.worker) {
        return;
      }

      const id = this.msgId++;
      this.worker.postMessage({
        id,
        method,
        calls: items.map(item => item[0]),
      });

      const listener = ({ data: result }) => {
        if (result.id !== id) {
          return;
        }

        if (!this.worker) {
          return;
        }

        this.worker.removeEventListener("message", listener);

        result.results.forEach((resultData, i) => {
          const [, resolve, reject] = items[i];

          if (resultData.error) {
            reject(resultData.error);
          } else {
            resolve(resultData.response);
          }
        });
      };

      this.worker.addEventListener("message", listener);
    };

    return (...args) => push(args);
  },

  invoke(method, ...args) {
    return this.task(method)(...args);
  },
};

function workerHandler(publicInterface) {
  return function(msg) {
    const { id, method, calls } = msg.data;

    Promise.all(
      calls.map(args => {
        try {
          const response = publicInterface[method].apply(undefined, args);
          if (response instanceof Promise) {
            return response.then(
              val => ({ response: val }),
              // Error can't be sent via postMessage, so be sure to
              // convert to string.
              err => ({ error: err.toString() })
            );
          }
          return { response };
        } catch (error) {
          // Error can't be sent via postMessage, so be sure to convert to
          // string.
          return { error: error.toString() };
        }
      })
    ).then(results => {
      self.postMessage({ id, results });
    });
  };
}

// Might be loaded within a worker thread where `module` isn't available.
if (typeof module !== "undefined") {
  module.exports = {
    WorkerDispatcher,
    workerHandler,
  };
}
