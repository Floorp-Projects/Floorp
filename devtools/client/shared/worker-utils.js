/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

class WorkerDispatcher {
  #msgId = 1;
  #worker = null;
  // Map of message ids -> promise resolution functions, for dispatching worker responses
  #pendingCalls = new Map();
  #url = "";

  constructor(url) {
    this.#url = url;
  }

  start() {
    // When running in debugger jest test, we don't have access to ChromeWorker
    if (typeof ChromeWorker == "function") {
      this.#worker = new ChromeWorker(this.#url);
    } else {
      this.#worker = new Worker(this.#url);
    }
    this.#worker.onerror = err => {
      console.error(`Error in worker ${this.#url}`, err.message);
    };
    this.#worker.addEventListener("message", this.#onMessage);
  }

  stop() {
    if (!this.#worker) {
      return;
    }

    this.#worker.removeEventListener("message", this.#onMessage);
    this.#worker.terminate();
    this.#worker = null;
    this.#pendingCalls.clear();
  }

  task(method, { queue = false } = {}) {
    const calls = [];
    const push = args => {
      return new Promise((resolve, reject) => {
        if (queue && calls.length === 0) {
          Promise.resolve().then(flush);
        }

        calls.push({ args, resolve, reject });

        if (!queue) {
          flush();
        }
      });
    };

    const flush = () => {
      const items = calls.slice();
      calls.length = 0;

      if (!this.#worker) {
        this.start();
      }

      const id = this.#msgId++;
      this.#worker.postMessage({
        id,
        method,
        calls: items.map(item => item.args),
      });

      this.#pendingCalls.set(id, items);
    };

    return (...args) => push(args);
  }

  invoke(method, ...args) {
    return this.task(method)(...args);
  }

  #onMessage = ({ data: result }) => {
    const items = this.#pendingCalls.get(result.id);
    this.#pendingCalls.delete(result.id);
    if (!items) {
      return;
    }

    if (!this.#worker) {
      return;
    }

    result.results.forEach((resultData, i) => {
      const { resolve, reject } = items[i];

      if (resultData.error) {
        const err = new Error(resultData.message);
        err.metadata = resultData.metadata;
        reject(err);
      } else {
        resolve(resultData.response);
      }
    });
  };
}

function workerHandler(publicInterface) {
  return function (msg) {
    const { id, method, calls } = msg.data;

    Promise.all(
      calls.map(args => {
        try {
          const response = publicInterface[method].apply(undefined, args);
          if (response instanceof Promise) {
            return response.then(
              val => ({ response: val }),
              err => asErrorMessage(err)
            );
          }
          return { response };
        } catch (error) {
          return asErrorMessage(error);
        }
      })
    ).then(results => {
      globalThis.postMessage({ id, results });
    });
  };
}

function asErrorMessage(error) {
  if (typeof error === "object" && error && "message" in error) {
    // Error can't be sent via postMessage, so be sure to convert to
    // string.
    return {
      error: true,
      message: error.message,
      metadata: error.metadata,
    };
  }

  return {
    error: true,
    message: error == null ? error : error.toString(),
    metadata: undefined,
  };
}

// Might be loaded within a worker thread where `module` isn't available.
if (typeof module !== "undefined") {
  module.exports = {
    WorkerDispatcher,
    workerHandler,
  };
}
