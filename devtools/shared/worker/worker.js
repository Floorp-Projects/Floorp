/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ChromeWorker */

(function(factory) {
  if (this.module && module.id.includes("worker")) {
    // require
    const { Cc, Ci, Cu, ChromeWorker } = require("chrome");
    const dumpn = require("devtools/shared/DevToolsUtils").dumpn;
    factory.call(
      this,
      require,
      exports,
      module,
      { Cc, Ci, Cu },
      ChromeWorker,
      dumpn
    );
  } else {
    // Cu.import
    const { require } = ChromeUtils.import(
      "resource://devtools/shared/Loader.jsm"
    );
    this.isWorker = false;
    this.console = console;
    factory.call(
      this,
      require,
      this,
      { exports: this },
      { Cc, Ci, Cu },
      ChromeWorker,
      null
    );
    this.EXPORTED_SYMBOLS = ["DevToolsWorker", "workerify"];
  }
}.call(this, function(
  require,
  exports,
  module,
  { Ci, Cc },
  ChromeWorker,
  dumpn
) {
  let MESSAGE_COUNTER = 0;

  /**
   * Creates a wrapper around a ChromeWorker, providing easy
   * communication to offload demanding tasks. The corresponding URL
   * must implement the interface provided by `devtools/shared/worker/helper`.
   *
   * @see `./devtools/client/shared/widgets/GraphsWorker.js`
   *
   * @param {string} url
   *        The URL of the worker.
   * @param Object opts
   *        An option with the following optional fields:
   *        - name: a name that will be printed with logs
   *        - verbose: log incoming and outgoing messages
   */
  function DevToolsWorker(url, opts) {
    opts = opts || {};
    this._worker = new ChromeWorker(url);
    this._verbose = opts.verbose;
    this._name = opts.name;

    this._worker.addEventListener("error", this.onError);
  }
  exports.DevToolsWorker = DevToolsWorker;

  /**
   * Performs the given task in a chrome worker, passing in data.
   * Returns a promise that resolves when the task is completed, resulting in
   * the return value of the task.
   *
   * @param {string} task
   *        The name of the task to execute in the worker.
   * @param {any} data
   *        Data to be passed into the task implemented by the worker.
   * @param {undefined|Array} transfer
   *        Optional array of transferable objects to transfer ownership of.
   * @return {Promise}
   */
  DevToolsWorker.prototype.performTask = function(task, data, transfer) {
    if (this._destroyed) {
      return Promise.reject(
        "Cannot call performTask on a destroyed DevToolsWorker"
      );
    }
    const worker = this._worker;
    const id = ++MESSAGE_COUNTER;
    const payload = { task, id, data };

    if (this._verbose && dumpn) {
      dumpn(
        "Sending message to worker" +
          (this._name ? " (" + this._name + ")" : "") +
          ": " +
          JSON.stringify(payload, null, 2)
      );
    }
    worker.postMessage(payload, transfer);

    return new Promise((resolve, reject) => {
      const listener = ({ data: result }) => {
        if (this._verbose && dumpn) {
          dumpn(
            "Received message from worker" +
              (this._name ? " (" + this._name + ")" : "") +
              ": " +
              JSON.stringify(result, null, 2)
          );
        }

        if (result.id !== id) {
          return;
        }
        worker.removeEventListener("message", listener);
        if (result.error) {
          reject(result.error);
        } else {
          resolve(result.response);
        }
      };

      worker.addEventListener("message", listener);
    });
  };

  /**
   * Terminates the underlying worker. Use when no longer needing the worker.
   */
  DevToolsWorker.prototype.destroy = function() {
    this._worker.terminate();
    this._worker = null;
    this._destroyed = true;
  };

  DevToolsWorker.prototype.onError = function({ message, filename, lineno }) {
    dump(new Error(message + " @ " + filename + ":" + lineno) + "\n");
  };

  /**
   * Takes a function and returns a Worker-wrapped version of the same function.
   * Returns a promise upon resolution.
   * @see `./devtools/shared/shared/tests/browser/browser_devtools-worker-03.js
   *
   * ⚠ This should only be used for tests or A/B testing performance ⚠
   *
   * The original function must:
   *
   * Be a pure function, that is, not use any variables not declared within the
   * function, or its arguments.
   *
   * Return a value or a promise.
   *
   * Note any state change in the worker will not affect the callee's context.
   *
   * @param {function} fn
   * @return {function}
   */
  function workerify(fn) {
    console.warn(
      "`workerify` should only be used in tests or measuring performance. " +
        "This creates an object URL on the browser window, and should not be " +
        "used in production."
    );
    // Fetch modules here as we don't want to include it normally.
    const Services = require("Services");
    const { URL, Blob } = Services.wm.getMostRecentWindow("navigator:browser");
    const stringifiedFn = createWorkerString(fn);
    const blob = new Blob([stringifiedFn]);
    const url = URL.createObjectURL(blob);
    const worker = new DevToolsWorker(url);

    const wrapperFn = (data, transfer) =>
      worker.performTask("workerifiedTask", data, transfer);

    wrapperFn.destroy = function() {
      URL.revokeObjectURL(url);
      worker.destroy();
    };

    return wrapperFn;
  }
  exports.workerify = workerify;

  /**
   * Takes a function, and stringifies it, attaching the worker-helper.js
   * boilerplate hooks.
   */
  function createWorkerString(fn) {
    return `importScripts("resource://gre/modules/workers/require.js");
            const { createTask } = require("resource://devtools/shared/worker/helper.js");
            createTask(self, "workerifiedTask", ${fn.toString()});`;
  }
}));
