/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["_SessionFile"];

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

XPCOMUtils.defineLazyModuleGetter(this, "TelemetryStopwatch",
  "resource://gre/modules/TelemetryStopwatch.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "Telemetry",
  "@mozilla.org/base/telemetry;1", "nsITelemetry");
XPCOMUtils.defineLazyModuleGetter(this, "Deprecated",
  "resource://gre/modules/Deprecated.jsm");

this._SessionFile = {
  /**
   * Read the contents of the session file, asynchronously.
   */
  read: function () {
    return SessionFileInternal.read();
  },
  /**
   * Read the contents of the session file, synchronously.
   */
  syncRead: function () {
    Deprecated.warning(
      "syncRead is deprecated and will be removed in a future version",
      "https://bugzilla.mozilla.org/show_bug.cgi?id=532150")
    return SessionFileInternal.syncRead();
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
    return SessionFileInternal.writeLoadStateOnceAfterStartup(aLoadState);
  },
  /**
   * Create a backup copy, asynchronously.
   */
  moveToBackupPath: function () {
    return SessionFileInternal.moveToBackupPath();
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
    return SessionFileInternal.wipe();
  }
};

Object.freeze(_SessionFile);

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
        Cu.reportError("Uncaught asynchronous error: " + reason + " at\n" + reason.stack);
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
   * Utility function to safely read a file synchronously.
   * @param aPath
   *        A path to read the file from.
   * @returns string if successful, undefined otherwise.
   */
  readAuxSync: function (aPath) {
    let text;
    try {
      let file = new FileUtils.File(aPath);
      let chan = NetUtil.newChannel(file);
      let stream = chan.open();
      text = NetUtil.readInputStreamToString(stream, stream.available(),
        {charset: "utf-8"});
    } catch (e if e.result == Components.results.NS_ERROR_FILE_NOT_FOUND) {
      // Ignore exceptions about non-existent files.
    } catch (ex) {
      // Any other error.
      Cu.reportError(ex);
    } finally {
      return text;
    }
  },

  /**
   * Read the sessionstore file synchronously.
   *
   * This function is meant to serve as a fallback in case of race
   * between a synchronous usage of the API and asynchronous
   * initialization.
   *
   * In case if sessionstore.js file does not exist or is corrupted (something
   * happened between backup and write), attempt to read the sessionstore.bak
   * instead.
   */
  syncRead: function () {
    // Start measuring the duration of the synchronous read.
    TelemetryStopwatch.start("FX_SESSION_RESTORE_SYNC_READ_FILE_MS");
    // First read the sessionstore.js.
    let text = this.readAuxSync(this.path);
    if (typeof text === "undefined") {
      // If sessionstore.js does not exist or is corrupted, read sessionstore.bak.
      text = this.readAuxSync(this.backupPath);
    }
    // Finish the telemetry probe and return an empty string.
    TelemetryStopwatch.finish("FX_SESSION_RESTORE_SYNC_READ_FILE_MS");
    text = text || "";

    // The worker needs to know the initial state read from
    // disk so that writeLoadStateOnceAfterStartup() works.
    SessionWorker.post("setInitialState", [text]);
    return text;
  },

  read: function () {
    return SessionWorker.post("read").then(msg => msg.ok);
  },

  write: function (aData) {
    let refObj = {};
    return TaskUtils.spawn(function task() {
      TelemetryStopwatch.start("FX_SESSION_RESTORE_WRITE_FILE_MS", refObj);
      TelemetryStopwatch.start("FX_SESSION_RESTORE_WRITE_FILE_LONGEST_OP_MS", refObj);

      try {
        let promise = SessionWorker.post("write", [aData]);
        // At this point, we measure how long we stop the main thread
        TelemetryStopwatch.finish("FX_SESSION_RESTORE_WRITE_FILE_LONGEST_OP_MS", refObj);

        // Now wait for the result and measure how long we had to wait for the result
        yield promise;
        TelemetryStopwatch.finish("FX_SESSION_RESTORE_WRITE_FILE_MS", refObj);
      } catch (ex) {
        TelemetryStopwatch.cancel("FX_SESSION_RESTORE_WRITE_FILE_LONGEST_OP_MS", refObj);
        TelemetryStopwatch.cancel("FX_SESSION_RESTORE_WRITE_FILE_MS", refObj);
        Cu.reportError("Could not write session state file " + this.path
                       + ": " + ex);
      }
    }.bind(this));
  },

  writeLoadStateOnceAfterStartup: function (aLoadState) {
    return SessionWorker.post("writeLoadStateOnceAfterStartup", [aLoadState]);
  },

  moveToBackupPath: function () {
    return SessionWorker.post("moveToBackupPath");
  },

  createBackupCopy: function (ext) {
    return SessionWorker.post("createBackupCopy", [ext]);
  },

  removeBackupCopy: function (ext) {
    return SessionWorker.post("removeBackupCopy", [ext]);
  },

  wipe: function () {
    return SessionWorker.post("wipe");
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
