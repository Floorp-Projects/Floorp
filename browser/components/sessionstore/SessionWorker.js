/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A worker dedicated to handle I/O for Session Store.
 */

"use strict";

importScripts("resource://gre/modules/osfile.jsm");

var PromiseWorker = require("resource://gre/modules/workers/PromiseWorker.js");

var File = OS.File;
var Encoder = new TextEncoder();
var Decoder = new TextDecoder();

var worker = new PromiseWorker.AbstractWorker();
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

var Agent = {
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
   * Number of old upgrade backups that are being kept
   */
  maxUpgradeBackups: null,

  /**
   * Initialize (or reinitialize) the worker
   *
   * @param {string} origin Which of sessionstore.js or its backups
   *   was used. One of the `STATE_*` constants defined above.
   * @param {object} paths The paths at which to find the various files.
   * @param {object} prefs The preferences the worker needs to known.
   */
  init(origin, paths, prefs = {}) {
    if (!(origin in paths || origin == STATE_EMPTY)) {
      throw new TypeError("Invalid origin: " + origin);
    }

    // Check that all required preference values were passed.
    for (let pref of ["maxUpgradeBackups", "maxSerializeBack", "maxSerializeForward"]) {
      if (!prefs.hasOwnProperty(pref)) {
        throw new TypeError(`Missing preference value for ${pref}`);
      }
    }

    this.state = origin;
    this.Paths = paths;
    this.maxUpgradeBackups = prefs.maxUpgradeBackups;
    this.maxSerializeBack = prefs.maxSerializeBack;
    this.maxSerializeForward = prefs.maxSerializeForward;
    this.upgradeBackupNeeded = paths.nextUpgradeBackup != paths.upgradeBackup;
    return {result: true};
  },

  /**
   * Write the session to disk.
   * Write the session to disk, performing any necessary backup
   * along the way.
   *
   * @param {object} state The state to write to disk.
   * @param {object} options
   *  - performShutdownCleanup If |true|, we should
   *    perform shutdown-time cleanup to ensure that private data
   *    is not left lying around;
   *  - isFinalWrite If |true|, write to Paths.clean instead of
   *    Paths.recovery
   */
  write: function (state, options = {}) {
    let exn;
    let telemetry = {};

    // Cap the number of backward and forward shistory entries on shutdown.
    if (options.isFinalWrite) {
      for (let window of state.windows) {
        for (let tab of window.tabs) {
          let lower = 0;
          let upper = tab.entries.length;

          if (this.maxSerializeBack > -1) {
            lower = Math.max(lower, tab.index - this.maxSerializeBack - 1);
          }
          if (this.maxSerializeForward > -1) {
            upper = Math.min(upper, tab.index + this.maxSerializeForward);
          }

          tab.entries = tab.entries.slice(lower, upper);
          tab.index -= lower;
        }
      }
    }

    let stateString = JSON.stringify(state);
    let data = Encoder.encode(stateString);

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

      let startWriteMs = Date.now();

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

      telemetry.FX_SESSION_RESTORE_WRITE_FILE_MS = Date.now() - startWriteMs;
      telemetry.FX_SESSION_RESTORE_FILE_SIZE_BYTES = data.byteLength;

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

      // Find all backups
      let iterator;
      let backups = [];  // array that will contain the paths to all upgrade backup
      let upgradeBackupPrefix = this.Paths.upgradeBackupPrefix;  // access for forEach callback

      try {
        iterator = new File.DirectoryIterator(this.Paths.backups);
        iterator.forEach(function (file) {
          if (file.path.startsWith(upgradeBackupPrefix)) {
            backups.push(file.path);
          }
        }, this);
      } catch (ex) {
          // Don't throw immediately
          exn = exn || ex;
      } finally {
        if (iterator) {
          iterator.close();
        }
      }

      // If too many backups exist, delete them
      if (backups.length > this.maxUpgradeBackups) {
        // Use alphanumerical sort since dates are in YYYYMMDDHHMMSS format
        backups.sort().forEach((file, i) => {
          // remove backup file if it is among the first (n-maxUpgradeBackups) files
          if (i < backups.length - this.maxUpgradeBackups) {
            File.remove(file);
          }
        });
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
      telemetry: telemetry,
    };
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
    try {
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
    } finally {
      iterator.close();
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
