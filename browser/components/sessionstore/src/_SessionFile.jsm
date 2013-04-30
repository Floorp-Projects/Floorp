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
Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js");

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

// An encoder to UTF-8.
XPCOMUtils.defineLazyGetter(this, "gEncoder", function () {
  return new TextEncoder();
});
// A decoder.
XPCOMUtils.defineLazyGetter(this, "gDecoder", function () {
  return new TextDecoder();
});

this._SessionFile = {
  /**
   * A promise fulfilled once initialization (either synchronous or
   * asynchronous) is complete.
   */
  promiseInitialized: function SessionFile_initialized() {
    return SessionFileInternal.promiseInitialized;
  },
  /**
   * Read the contents of the session file, asynchronously.
   */
  read: function SessionFile_read() {
    return SessionFileInternal.read();
  },
  /**
   * Read the contents of the session file, synchronously.
   */
  syncRead: function SessionFile_syncRead() {
    return SessionFileInternal.syncRead();
  },
  /**
   * Write the contents of the session file, asynchronously.
   */
  write: function SessionFile_write(aData) {
    return SessionFileInternal.write(aData);
  },
  /**
   * Create a backup copy, asynchronously.
   */
  createBackupCopy: function SessionFile_createBackupCopy() {
    return SessionFileInternal.createBackupCopy();
  },
  /**
   * Wipe the contents of the session file, asynchronously.
   */
  wipe: function SessionFile_wipe() {
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
   * A promise fulfilled once initialization is complete
   */
  promiseInitialized: Promise.defer(),

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
  readAuxSync: function ssfi_readAuxSync(aPath) {
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
  syncRead: function ssfi_syncRead() {
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
    return text || "";
  },

  /**
   * Utility function to safely read a file asynchronously.
   * @param aPath
   *        A path to read the file from.
   * @param aReadOptions
   *        Read operation options.
   *        |outExecutionDuration| option will be reused and can be
   *        incrementally updated by the worker process.
   * @returns string if successful, undefined otherwise.
   */
  readAux: function ssfi_readAux(aPath, aReadOptions) {
    let self = this;
    return TaskUtils.spawn(function () {
      let text;
      try {
        let bytes = yield OS.File.read(aPath, undefined, aReadOptions);
        text = gDecoder.decode(bytes);
        // If the file is read successfully, add a telemetry probe based on
        // the updated duration value of the |outExecutionDuration| option.
        let histogram = Telemetry.getHistogramById(
          "FX_SESSION_RESTORE_READ_FILE_MS");
        histogram.add(aReadOptions.outExecutionDuration);
      } catch (ex if self._isNoSuchFile(ex)) {
        // Ignore exceptions about non-existent files.
      } catch (ex) {
        Cu.reportError(ex);
      }
      throw new Task.Result(text);
    });
  },

  /**
   * Read the sessionstore file asynchronously.
   *
   * In case sessionstore.js file does not exist or is corrupted (something
   * happened between backup and write), attempt to read the sessionstore.bak
   * instead.
   */
  read: function ssfi_read() {
    let self = this;
    return TaskUtils.spawn(function task() {
      // Specify |outExecutionDuration| option to hold the combined duration of
      // the asynchronous reads off the main thread (of both sessionstore.js and
      // sessionstore.bak, if necessary). If sessionstore.js does not exist or
      // is corrupted, |outExecutionDuration| will register the time it took to
      // attempt to read the file. It will then be subsequently incremented by
      // the read time of sessionsore.bak.
      let readOptions = {
        outExecutionDuration: null
      };
      // First read the sessionstore.js.
      let text = yield self.readAux(self.path, readOptions);
      if (typeof text === "undefined") {
        // If sessionstore.js does not exist or is corrupted, read the
        // sessionstore.bak.
        text = yield self.readAux(self.backupPath, readOptions);
      }
      // Return either the content of the sessionstore.bak if it was read
      // successfully or an empty string otherwise.
      throw new Task.Result(text || "");
    });
  },

  write: function ssfi_write(aData) {
    let refObj = {};
    let self = this;
    return TaskUtils.spawn(function task() {
      TelemetryStopwatch.start("FX_SESSION_RESTORE_WRITE_FILE_MS", refObj);
      TelemetryStopwatch.start("FX_SESSION_RESTORE_WRITE_FILE_LONGEST_OP_MS", refObj);

      let bytes = gEncoder.encode(aData);

      try {
        let promise = OS.File.writeAtomic(self.path, bytes, {tmpPath: self.path + ".tmp"});
        // At this point, we measure how long we stop the main thread
        TelemetryStopwatch.finish("FX_SESSION_RESTORE_WRITE_FILE_LONGEST_OP_MS", refObj);

        // Now wait for the result and measure how long we had to wait for the result
        yield promise;
        TelemetryStopwatch.finish("FX_SESSION_RESTORE_WRITE_FILE_MS", refObj);
      } catch (ex) {
        TelemetryStopwatch.cancel("FX_SESSION_RESTORE_WRITE_FILE_LONGEST_OP_MS", refObj);
        TelemetryStopwatch.cancel("FX_SESSION_RESTORE_WRITE_FILE_MS", refObj);
        Cu.reportError("Could not write session state file " + self.path
                       + ": " + aReason);
      }
    });
  },

  createBackupCopy: function ssfi_createBackupCopy() {
    let backupCopyOptions = {
      outExecutionDuration: null
    };
    let self = this;
    return TaskUtils.spawn(function task() {
      try {
        yield OS.File.move(self.path, self.backupPath, backupCopyOptions);
        Telemetry.getHistogramById("FX_SESSION_RESTORE_BACKUP_FILE_MS").add(
          backupCopyOptions.outExecutionDuration);
      } catch (ex if self._isNoSuchFile(ex)) {
        // Ignore exceptions about non-existent files.
      } catch (ex) {
        Cu.reportError("Could not backup session state file: " + ex);
        throw ex;
      }
    });
  },

  wipe: function ssfi_wipe() {
    let self = this;
    return TaskUtils.spawn(function task() {
      try {
        yield OS.File.remove(self.path);
      } catch (ex if self._isNoSuchFile(ex)) {
        // Ignore exceptions about non-existent files.
      } catch (ex) {
        Cu.reportError("Could not remove session state file: " + ex);
        throw ex;
      }

      try {
        yield OS.File.remove(self.backupPath);
      } catch (ex if self._isNoSuchFile(ex)) {
        // Ignore exceptions about non-existent files.
      } catch (ex) {
        Cu.reportError("Could not remove session state backup file: " + ex);
        throw ex;
      }
    });
  },

  _isNoSuchFile: function ssfi_isNoSuchFile(aReason) {
    return aReason instanceof OS.File.Error && aReason.becauseNoSuchFile;
  }
};
