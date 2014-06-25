/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Interface to a dedicated thread handling I/O
 */

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

Cu.import("resource://gre/modules/osfile/_PromiseWorker.jsm", this);
Cu.import("resource://gre/modules/osfile.jsm", this);

this.EXPORTED_SYMBOLS = ["SessionWorker"];

this.SessionWorker = (function () {
  let worker = new PromiseWorker("resource:///modules/sessionstore/SessionWorker.js",
    OS.Shared.LOG.bind("SessionWorker"));
  return {
    post: function post(...args) {
      let promise = worker.post.apply(worker, args);
      return promise.then(
        null,
        function onError(error) {
          // Decode any serialized error
          if (error instanceof PromiseWorker.WorkerError) {
            throw OS.File.Error.fromMsg(error.data);
          }
          // Extract something meaningful from ErrorEvent
          if (error instanceof ErrorEvent) {
            throw new Error(error.message, error.filename, error.lineno);
          }
          throw error;
        }
      );
    }
  };
})();
