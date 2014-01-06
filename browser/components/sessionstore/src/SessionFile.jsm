/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["SessionFile"];

/**
 * Implementation of all the disk I/O required by the session store.
 * This is a private API, meant to be used only by the session store.
 * It will change. Do not use it for any other purpose.
 *
 * Note that this module implicitly depends on one of two things:
 * 1. either the asynchronous file I/O system enqueues its requests
 *   and never attempts to simultaneously execute two I/O requests on
 *   the files used by this module from two distinct threads; or
 * 2. the clients of this API are well-behaved and do not place
 *   concurrent requests to the files used by this module.
 *
 * Otherwise, we could encounter bugs, especially under Windows,
 *   e.g. if a request attempts to write sessionstore.js while
 *   another attempts to copy that file.
 *
 * This implementation uses OS.File, which guarantees property 1.
 */

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/osfile/_PromiseWorker.jsm", this);
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/AsyncShutdown.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "console",
  "resource://gre/modules/devtools/Console.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryStopwatch",
  "resource://gre/modules/TelemetryStopwatch.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "Telemetry",
  "@mozilla.org/base/telemetry;1", "nsITelemetry");

this.SessionFile = {
  /**
   * Read the contents of the session file, asynchronously.
   */
  read: function () {
    return SessionFileInternal.read();
  },
  /**
   * Write the contents of the session file, asynchronously.
   */
  write: function (aData) {
    return SessionFileInternal.write(aData);
  },
  /**
   * Writes the initial state to disk again only to change the session's load
   * state. This must only be called once, it will throw an error otherwise.
   */
  writeLoadStateOnceAfterStartup: function (aLoadState) {
    SessionFileInternal.writeLoadStateOnceAfterStartup(aLoadState);
  },
  /**
   * Create a backup copy, asynchronously.
   * This is designed to perform backup on upgrade.
   */
  createBackupCopy: function (ext) {
    return SessionFileInternal.createBackupCopy(ext);
  },
  /**
   * Remove a backup copy, asynchronously.
   * This is designed to clean up a backup on upgrade.
   */
  removeBackupCopy: function (ext) {
    return SessionFileInternal.removeBackupCopy(ext);
  },
  /**
   * Wipe the contents of the session file, asynchronously.
   */
  wipe: function () {
    SessionFileInternal.wipe();
  }
};

Object.freeze(SessionFile);

/**
 * Utilities for dealing with promises and Task.jsm
 */
const TaskUtils = {
  /**
   * Add logging to a promise.
   *
   * @param {Promise} promise
   * @return {Promise} A promise behaving as |promise|, but with additional
   * logging in case of uncaught error.
   */
  captureErrors: function captureErrors(promise) {
    return promise.then(
      null,
      function onError(reason) {
        console.error("Uncaught asynchronous error", reason, "at", reason.stack);
        throw reason;
      }
    );
  },
  /**
   * Spawn a new Task from a generator.
   *
   * This function behaves as |Task.spawn|, with the exception that it
   * adds logging in case of uncaught error. For more information, see
   * the documentation of |Task.jsm|.
   *
   * @param {generator} gen Some generator.
   * @return {Promise} A promise built from |gen|, with the same semantics
   * as |Task.spawn(gen)|.
   */
  spawn: function spawn(gen) {
    return this.captureErrors(Task.spawn(gen));
  }
};

let SessionFileInternal = {
  /**
   * The path to sessionstore.js
   */
  path: OS.Path.join(OS.Constants.Path.profileDir, "sessionstore.js"),

  /**
   * The path to sessionstore.bak
   */
  backupPath: OS.Path.join(OS.Constants.Path.profileDir, "sessionstore.bak"),

  /**
   * The promise returned by the latest call to |write|.
   * We use it to ensure that AsyncShutdown.profileBeforeChange cannot
   * interrupt a call to |write|.
   */
  _latestWrite: null,

  /**
   * |true| once we have decided to stop receiving write instructiosn
   */
  _isClosed: false,

  read: function () {
    return SessionWorker.post("read").then(msg => {
      this._recordTelemetry(msg.telemetry);
      return msg.ok;
    });
  },

  write: function (aData) {
    if (this._isClosed) {
      return Promise.reject(new Error("SessionFile is closed"));
    }
    let refObj = {};

    let isFinalWrite = false;
    if (Services.startup.shuttingDown) {
      // If shutdown has started, we will want to stop receiving
      // write instructions.
      isFinalWrite = this._isClosed = true;
    }

    return this._latestWrite = TaskUtils.spawn(function task() {
      TelemetryStopwatch.start("FX_SESSION_RESTORE_WRITE_FILE_LONGEST_OP_MS", refObj);

      try {
        let promise = SessionWorker.post("write", [aData]);
        // At this point, we measure how long we stop the main thread
        TelemetryStopwatch.finish("FX_SESSION_RESTORE_WRITE_FILE_LONGEST_OP_MS", refObj);

        // Now wait for the result and record how long the write took
        let msg = yield promise;
        this._recordTelemetry(msg.telemetry);
      } catch (ex) {
        TelemetryStopwatch.cancel("FX_SESSION_RESTORE_WRITE_FILE_LONGEST_OP_MS", refObj);
        console.error("Could not write session state file ", this.path, ex);
      }

      if (isFinalWrite) {
        Services.obs.notifyObservers(null, "sessionstore-final-state-write-complete", "");
      }
    }.bind(this));
  },

  writeLoadStateOnceAfterStartup: function (aLoadState) {
    SessionWorker.post("writeLoadStateOnceAfterStartup", [aLoadState]).then(msg => {
      this._recordTelemetry(msg.telemetry);
      return msg;
    }, console.error);
  },

  createBackupCopy: function (ext) {
    return SessionWorker.post("createBackupCopy", [ext]);
  },

  removeBackupCopy: function (ext) {
    return SessionWorker.post("removeBackupCopy", [ext]);
  },

  wipe: function () {
    SessionWorker.post("wipe");
  },

  _recordTelemetry: function(telemetry) {
    for (let histogramId in telemetry){
      Telemetry.getHistogramById(histogramId).add(telemetry[histogramId]);
    }
  }
};

// Interface to a dedicated thread handling I/O
let SessionWorker = (function () {
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
          } else {
            throw error;
          }
        }
      );
    }
  };
})();

// Ensure that we can write sessionstore.js cleanly before the profile
// becomes unaccessible.
AsyncShutdown.profileBeforeChange.addBlocker(
  "SessionFile: Finish writing the latest sessionstore.js",
  function() {
    return SessionFile._latestWrite;
  });
