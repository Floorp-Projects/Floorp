/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A worker dedicated to handle I/O for Session Store.
 */

"use strict";

importScripts("resource://gre/modules/osfile.jsm");

let PromiseWorker = require("resource://gre/modules/workers/PromiseWorker.js");

let File = OS.File;
let Encoder = new TextEncoder();
let Decoder = new TextDecoder();

let worker = new PromiseWorker.AbstractWorker();
worker.dispatch = function(method, args = []) {
  return Agent[method](...args);
};
worker.postMessage = function(result, ...transfers) {
  self.postMessage(result, ...transfers);
};
worker.close = function() {
  self.close();
};

self.addEventListener("message", msg => worker.handleMessage(msg));

// The various possible states

/**
 * We just started (we haven't written anything to disk yet) from
 * `Paths.clean`. The backup directory may not exist.
 */
const STATE_CLEAN = "clean";
/**
 * We know that `Paths.recovery` is good, either because we just read
 * it (we haven't written anything to disk yet) or because have
 * already written once to `Paths.recovery` during this session.
 * `Paths.clean` is absent or invalid. The backup directory exists.
 */
const STATE_RECOVERY = "recovery";
/**
 * We just started from `Paths.recoverBackupy` (we haven't written
 * anything to disk yet). Both `Paths.clean` and `Paths.recovery` are
 * absent or invalid. The backup directory exists.
 */
const STATE_RECOVERY_BACKUP = "recoveryBackup";
/**
 * We just started from `Paths.upgradeBackup` (we haven't written
 * anything to disk yet). Both `Paths.clean`, `Paths.recovery` and
 * `Paths.recoveryBackup` are absent or invalid. The backup directory
 * exists.
 */
const STATE_UPGRADE_BACKUP = "upgradeBackup";
/**
 * We just started without a valid session store file (we haven't
 * written anything to disk yet). The backup directory may not exist.
 */
const STATE_EMPTY = "empty";

let Agent = {
  // Boolean that tells whether we already made a
  // call to write(). We will only attempt to move
  // sessionstore.js to sessionstore.bak on the
  // first write.
  hasWrittenState: false,

  // Path to the files used by the SessionWorker
  Paths: null,

  /**
   * The current state of the worker, as one of the following strings:
   * - "permanent", once the first write has been completed;
   * - "empty", before the first write has been completed,
   *   if we have started without any sessionstore;
   * - one of "clean", "recovery", "recoveryBackup", "cleanBackup",
   *   "upgradeBackup", before the first write has been completed, if
   *   we have started by loading the corresponding file.
   */
  state: null,

  /**
   * Initialize (or reinitialize) the worker
   *
   * @param {string} origin Which of sessionstore.js or its backups
   *   was used. One of the `STATE_*` constants defined above.
   * @param {object} paths The paths at which to find the various files.
   */
  init: function (origin, paths) {
    if (!(origin in paths || origin == STATE_EMPTY)) {
      throw new TypeError("Invalid origin: " + origin);
    }
    this.state = origin;
    this.Paths = paths;
    this.upgradeBackupNeeded = paths.nextUpgradeBackup != paths.upgradeBackup;
    return {result: true};
  },

  /**
   * Write the session to disk.
   * Write the session to disk, performing any necessary backup
   * along the way.
   *
   * @param {string} stateString The state to write to disk.
   * @param {object} options
   *  - performShutdownCleanup If |true|, we should
   *    perform shutdown-time cleanup to ensure that private data
   *    is not left lying around;
   *  - isFinalWrite If |true|, write to Paths.clean instead of
   *    Paths.recovery
   */
  write: function (stateString, options = {}) {
    let exn;
    let telemetry = {};

    let data = Encoder.encode(stateString);
    let startWriteMs, stopWriteMs;

    try {

      if (this.state == STATE_CLEAN || this.state == STATE_EMPTY) {
        // The backups directory may not exist yet. In all other cases,
        // we have either already read from or already written to this
        // directory, so we are satisfied that it exists.
        File.makeDir(this.Paths.backups);
      }

      if (this.state == STATE_CLEAN) {
        // Move $Path.clean out of the way, to avoid any ambiguity as
        // to which file is more recent.
        File.move(this.Paths.clean, this.Paths.cleanBackup);
      }

      startWriteMs = Date.now();

      if (options.isFinalWrite) {
        // We are shutting down. At this stage, we know that
        // $Paths.clean is either absent or corrupted. If it was
        // originally present and valid, it has been moved to
        // $Paths.cleanBackup a long time ago. We can therefore write
        // with the guarantees that we erase no important data.
        File.writeAtomic(this.Paths.clean, data, {
          tmpPath: this.Paths.clean + ".tmp"
        });
      } else if (this.state == STATE_RECOVERY) {
        // At this stage, either $Paths.recovery was written >= 15
        // seconds ago during this session or we have just started
        // from $Paths.recovery left from the previous session. Either
        // way, $Paths.recovery is good. We can move $Path.backup to
        // $Path.recoveryBackup without erasing a good file with a bad
        // file.
        File.writeAtomic(this.Paths.recovery, data, {
          tmpPath: this.Paths.recovery + ".tmp",
          backupTo: this.Paths.recoveryBackup
        });
      } else {
        // In other cases, either $Path.recovery is not necessary, or
        // it doesn't exist or it has been corrupted. Regardless,
        // don't backup $Path.recovery.
        File.writeAtomic(this.Paths.recovery, data, {
          tmpPath: this.Paths.recovery + ".tmp"
        });
      }

      stopWriteMs = Date.now();

    } catch (ex) {
      // Don't throw immediately
      exn = exn || ex;
    }

    // If necessary, perform an upgrade backup
    let upgradeBackupComplete = false;
    if (this.upgradeBackupNeeded
      && (this.state == STATE_CLEAN || this.state == STATE_UPGRADE_BACKUP)) {
      try {
        // If we loaded from `clean`, the file has since then been renamed to `cleanBackup`.
        let path = this.state == STATE_CLEAN ? this.Paths.cleanBackup : this.Paths.upgradeBackup;
        File.copy(path, this.Paths.nextUpgradeBackup);
        this.upgradeBackupNeeded = false;
        upgradeBackupComplete = true;
      } catch (ex) {
        // Don't throw immediately
        exn = exn || ex;
      }
    }

    if (options.performShutdownCleanup && !exn) {

      // During shutdown, if auto-restore is disabled, we need to
      // remove possibly sensitive data that has been stored purely
      // for crash recovery. Note that this slightly decreases our
      // ability to recover from OS-level/hardware-level issue.

      // If an exception was raised, we assume that we still need
      // these files.
      File.remove(this.Paths.recoveryBackup);
      File.remove(this.Paths.recovery);
    }

    this.state = STATE_RECOVERY;

    if (exn) {
      throw exn;
    }

    return {
      result: {
        upgradeBackup: upgradeBackupComplete
      },
      telemetry: {
        FX_SESSION_RESTORE_WRITE_FILE_MS: stopWriteMs - startWriteMs,
        FX_SESSION_RESTORE_FILE_SIZE_BYTES: data.byteLength,
      }
    };
  },

  /**
   * Extract all sorts of useful statistics from a state string,
   * for use with Telemetry.
   *
   * @return {object}
   */
  gatherTelemetry: function (stateString) {
    return Statistics.collect(stateString);
  },

  /**
   * Wipes all files holding session data from disk.
   */
  wipe: function () {

    // Don't stop immediately in case of error.
    let exn = null;

    // Erase main session state file
    try {
      File.remove(this.Paths.clean);
    } catch (ex) {
      // Don't stop immediately.
      exn = exn || ex;
    }

    // Wipe the Session Restore directory
    try {
      this._wipeFromDir(this.Paths.backups, null);
    } catch (ex) {
      exn = exn || ex;
    }

    try {
      File.removeDir(this.Paths.backups);
    } catch (ex) {
      exn = exn || ex;
    }

    // Wipe legacy Ression Restore files from the profile directory
    try {
      this._wipeFromDir(OS.Constants.Path.profileDir, "sessionstore.bak");
    } catch (ex) {
      exn = exn || ex;
    }


    this.state = STATE_EMPTY;
    if (exn) {
      throw exn;
    }

    return { result: true };
  },

  /**
   * Wipe a number of files from a directory.
   *
   * @param {string} path The directory.
   * @param {string|null} prefix If provided, only remove files whose
   * name starts with a specific prefix.
   */
  _wipeFromDir: function(path, prefix) {
    // Sanity check
    if (typeof prefix == "undefined" || prefix == "") {
      throw new TypeError();
    }

    let exn = null;

    let iterator = new File.DirectoryIterator(path);
    if (!iterator.exists()) {
      return;
    }
    for (let entry in iterator) {
      if (entry.isDir) {
        continue;
      }
      if (!prefix || entry.name.startsWith(prefix)) {
        try {
          File.remove(entry.path);
        } catch (ex) {
          // Don't stop immediately
          exn = exn || ex;
        }
      }
    }

    if (exn) {
      throw exn;
    }
  },
};

function isNoSuchFileEx(aReason) {
  return aReason instanceof OS.File.Error && aReason.becauseNoSuchFile;
}

/**
 * Estimate the number of bytes that a data structure will use on disk
 * once serialized.
 */
function getByteLength(str) {
  return Encoder.encode(JSON.stringify(str)).byteLength;
}

/**
 * Tools for gathering statistics on a state string.
 */
let Statistics = {
  collect: function(stateString) {
    let start = Date.now();
    let TOTAL_PREFIX = "FX_SESSION_RESTORE_TOTAL_";
    let INDIVIDUAL_PREFIX = "FX_SESSION_RESTORE_INDIVIDUAL_";
    let SIZE_SUFFIX = "_SIZE_BYTES";

    let state = JSON.parse(stateString);

    // Gather all data
    let subsets = {};
    this.gatherSimpleData(state, subsets);
    this.gatherComplexData(state, subsets);

    // Extract telemetry
    let telemetry = {};
    for (let k of Object.keys(subsets)) {
      let obj = subsets[k];
      telemetry[TOTAL_PREFIX + k + SIZE_SUFFIX] = getByteLength(obj);

      if (Array.isArray(obj)) {
        let size = obj.map(getByteLength);
        telemetry[INDIVIDUAL_PREFIX + k + SIZE_SUFFIX] = size;
      }
    }

    let stop = Date.now();
    telemetry["FX_SESSION_RESTORE_EXTRACTING_STATISTICS_DURATION_MS"] = stop - start;
    return {
      telemetry: telemetry
    };
  },

  /**
   * Collect data that doesn't require a recursive walk through the
   * data structure.
   */
  gatherSimpleData: function(state, subsets) {
    // The subset of sessionstore.js dealing with open windows
    subsets.OPEN_WINDOWS = state.windows;

    // The subset of sessionstore.js dealing with closed windows
    subsets.CLOSED_WINDOWS = state._closedWindows;

    // The subset of sessionstore.js dealing with closed tabs
    // in open windows
    subsets.CLOSED_TABS_IN_OPEN_WINDOWS = [];

    // The subset of sessionstore.js dealing with cookies
    // in both open and closed windows
    subsets.COOKIES = [];

    for (let winData of state.windows) {
      let closedTabs = winData._closedTabs || [];
      subsets.CLOSED_TABS_IN_OPEN_WINDOWS.push(...closedTabs);

      let cookies = winData.cookies || [];
      subsets.COOKIES.push(...cookies);
    }

    for (let winData of state._closedWindows) {
      let cookies = winData.cookies || [];
      subsets.COOKIES.push(...cookies);
    }
  },

  /**
   * Walk through a data structure, recursively.
   *
   * @param {object} root The object from which to start walking.
   * @param {function(key, value)} cb Callback, called for each
   * item except the root. Returns |true| to walk the subtree rooted
   * at |value|, |false| otherwise   */
  walk: function(root, cb) {
    if (!root || typeof root !== "object") {
      return;
    }
    for (let k of Object.keys(root)) {
      let obj = root[k];
      let stepIn = cb(k, obj);
      if (stepIn) {
        this.walk(obj, cb);
      }
    }
  },

  /**
   * Collect data that requires walking through the data structure
   */
  gatherComplexData: function(state, subsets) {
    // The subset of sessionstore.js dealing with DOM storage
    subsets.DOM_STORAGE = [];
    // The subset of sessionstore.js storing form data
    subsets.FORMDATA = [];
    // The subset of sessionstore.js storing history
    subsets.HISTORY = [];


    this.walk(state, function(k, value) {
      let dest;
      switch (k) {
        case "entries":
          subsets.HISTORY.push(value);
          return true;
        case "storage":
          subsets.DOM_STORAGE.push(value);
          // Never visit storage, it's full of weird stuff
          return false;
        case "formdata":
          subsets.FORMDATA.push(value);
          // Never visit formdata, it's full of weird stuff
          return false;
        case "cookies": // Don't visit these places, they are full of weird stuff
        case "extData":
          return false;
        default:
          return true;
      }
    });

    return subsets;
  },

};
