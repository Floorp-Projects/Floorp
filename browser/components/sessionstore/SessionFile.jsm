/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["SessionFile"];

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

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/osfile.jsm");
ChromeUtils.import("resource://gre/modules/AsyncShutdown.jsm");

ChromeUtils.defineModuleGetter(this, "RunState",
  "resource:///modules/sessionstore/RunState.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "Telemetry",
  "@mozilla.org/base/telemetry;1", "nsITelemetry");
XPCOMUtils.defineLazyServiceGetter(this, "sessionStartup",
  "@mozilla.org/browser/sessionstartup;1", "nsISessionStartup");
ChromeUtils.defineModuleGetter(this, "SessionWorker",
  "resource:///modules/sessionstore/SessionWorker.jsm");
ChromeUtils.defineModuleGetter(this, "SessionStore",
  "resource:///modules/sessionstore/SessionStore.jsm");

const PREF_UPGRADE_BACKUP = "browser.sessionstore.upgradeBackup.latestBuildID";
const PREF_MAX_UPGRADE_BACKUPS = "browser.sessionstore.upgradeBackup.maxUpgradeBackups";

const PREF_MAX_SERIALIZE_BACK = "browser.sessionstore.max_serialize_back";
const PREF_MAX_SERIALIZE_FWD = "browser.sessionstore.max_serialize_forward";

XPCOMUtils.defineLazyPreferenceGetter(this, "kMaxWriteFailures",
  "browser.sessionstore.max_write_failures", 5);

var SessionFile = {
  /**
   * Read the contents of the session file, asynchronously.
   */
  read() {
    return SessionFileInternal.read();
  },
  /**
   * Write the contents of the session file, asynchronously.
   */
  write(aData) {
    return SessionFileInternal.write(aData);
  },
  /**
   * Wipe the contents of the session file, asynchronously.
   */
  wipe() {
    return SessionFileInternal.wipe();
  },

  /**
   * Return the paths to the files used to store, backup, etc.
   * the state of the file.
   */
  get Paths() {
    return SessionFileInternal.Paths;
  },

  get MaxWriteFailures() {
    return kMaxWriteFailures;
  }
};

Object.freeze(SessionFile);

var Path = OS.Path;
var profileDir = OS.Constants.Path.profileDir;

var SessionFileInternal = {
  Paths: Object.freeze({
    // The path to the latest version of sessionstore written during a clean
    // shutdown. After startup, it is renamed `cleanBackup`.
    clean: Path.join(profileDir, "sessionstore.jsonlz4"),

    // The path at which we store the previous version of `clean`. Updated
    // whenever we successfully load from `clean`.
    cleanBackup: Path.join(profileDir, "sessionstore-backups", "previous.jsonlz4"),

    // The directory containing all sessionstore backups.
    backups: Path.join(profileDir, "sessionstore-backups"),

    // The path to the latest version of the sessionstore written
    // during runtime. Generally, this file contains more
    // privacy-sensitive information than |clean|, and this file is
    // therefore removed during clean shutdown. This file is designed to protect
    // against crashes / sudden shutdown.
    recovery: Path.join(profileDir, "sessionstore-backups", "recovery.jsonlz4"),

    // The path to the previous version of the sessionstore written
    // during runtime (e.g. 15 seconds before recovery). In case of a
    // clean shutdown, this file is removed.  Generally, this file
    // contains more privacy-sensitive information than |clean|, and
    // this file is therefore removed during clean shutdown.  This
    // file is designed to protect against crashes that are nasty
    // enough to corrupt |recovery|.
    recoveryBackup: Path.join(profileDir, "sessionstore-backups", "recovery.baklz4"),

    // The path to a backup created during an upgrade of Firefox.
    // Having this backup protects the user essentially from bugs in
    // Firefox or add-ons, especially for users of Nightly. This file
    // does not contain any information more sensitive than |clean|.
    upgradeBackupPrefix: Path.join(profileDir, "sessionstore-backups", "upgrade.jsonlz4-"),

    // The path to the backup of the version of the session store used
    // during the latest upgrade of Firefox. During load/recovery,
    // this file should be used if both |path|, |backupPath| and
    // |latestStartPath| are absent/incorrect.  May be "" if no
    // upgrade backup has ever been performed. This file does not
    // contain any information more sensitive than |clean|.
    get upgradeBackup() {
      let latestBackupID = SessionFileInternal.latestUpgradeBackupID;
      if (!latestBackupID) {
        return "";
      }
      return this.upgradeBackupPrefix + latestBackupID;
    },

    // The path to a backup created during an upgrade of Firefox.
    // Having this backup protects the user essentially from bugs in
    // Firefox, especially for users of Nightly.
    get nextUpgradeBackup() {
      return this.upgradeBackupPrefix + Services.appinfo.platformBuildID;
    },

    /**
     * The order in which to search for a valid sessionstore file.
     */
    get loadOrder() {
      // If `clean` exists and has been written without corruption during
      // the latest shutdown, we need to use it.
      //
      // Otherwise, `recovery` and `recoveryBackup` represent the most
      // recent state of the session store.
      //
      // Finally, if nothing works, fall back to the last known state
      // that can be loaded (`cleanBackup`) or, if available, to the
      // backup performed during the latest upgrade.
      let order = ["clean",
                   "recovery",
                   "recoveryBackup",
                   "cleanBackup"];
      if (SessionFileInternal.latestUpgradeBackupID) {
        // We have an upgradeBackup
        order.push("upgradeBackup");
      }
      return order;
    },
  }),

  // Number of attempted calls to `write`.
  // Note that we may have _attempts > _successes + _failures,
  // if attempts never complete.
  // Used for error reporting.
  _attempts: 0,

  // Number of successful calls to `write`.
  // Used for error reporting.
  _successes: 0,

  // Number of failed calls to `write`.
  // Used for error reporting.
  _failures: 0,

  // Object that keeps statistics that should help us make informed decisions
  // about the current status of the worker.
  _workerHealth: {
    failures: 0
  },

  // `true` once we have started initialization of the worker.
  _initializationStarted: false,

  // A string that will be set to the session file name part that was read from
  // disk. It will be available _after_ a session file read() is done.
  _readOrigin: null,

  // `true` if the old, uncompressed, file format was used to read from disk, as
  // a fallback mechanism.
  _usingOldExtension: false,

  // The ID of the latest version of Gecko for which we have an upgrade backup
  // or |undefined| if no upgrade backup was ever written.
  get latestUpgradeBackupID() {
    try {
      return Services.prefs.getCharPref(PREF_UPGRADE_BACKUP);
    } catch (ex) {
      return undefined;
    }
  },

  async _readInternal(useOldExtension) {
    let result;
    let noFilesFound = true;
    this._usingOldExtension = useOldExtension;

    // Attempt to load by order of priority from the various backups
    for (let key of this.Paths.loadOrder) {
      let corrupted = false;
      let exists = true;
      try {
        let path;
        let startMs = Date.now();

        let options = {encoding: "utf-8"};
        if (useOldExtension) {
          path = this.Paths[key]
                     .replace("jsonlz4", "js")
                     .replace("baklz4", "bak");
        } else {
          path = this.Paths[key];
          options.compression = "lz4";
        }
        let source = await OS.File.read(path, options);
        let parsed = JSON.parse(source);

        if (!SessionStore.isFormatVersionCompatible(parsed.version || ["sessionrestore", 0] /* fallback for old versions*/)) {
          // Skip sessionstore files that we don't understand.
          Cu.reportError("Cannot extract data from Session Restore file " + path +
            ". Wrong format/version: " + JSON.stringify(parsed.version) + ".");
          continue;
        }
        result = {
          origin: key,
          source,
          parsed,
          useOldExtension
        };
        Telemetry.getHistogramById("FX_SESSION_RESTORE_CORRUPT_FILE").
          add(false);
        Telemetry.getHistogramById("FX_SESSION_RESTORE_READ_FILE_MS").
          add(Date.now() - startMs);
        break;
      } catch (ex) {
          if (ex instanceof OS.File.Error && ex.becauseNoSuchFile) {
            exists = false;
          } else if (ex instanceof OS.File.Error) {
            // The file might be inaccessible due to wrong permissions
            // or similar failures. We'll just count it as "corrupted".
            console.error("Could not read session file ", ex, ex.stack);
            corrupted = true;
          } else if (ex instanceof SyntaxError) {
            console.error("Corrupt session file (invalid JSON found) ", ex, ex.stack);
            // File is corrupted, try next file
            corrupted = true;
          }
      } finally {
        if (exists) {
          noFilesFound = false;
          Telemetry.getHistogramById("FX_SESSION_RESTORE_CORRUPT_FILE").
            add(corrupted);
        }
      }
    }
    return {result, noFilesFound};
  },

  // Find the correct session file, read it and setup the worker.
  async read() {
    // Load session files with lz4 compression.
    let {result, noFilesFound} = await this._readInternal(false);
    if (!result) {
      // No result? Probably because of migration, let's
      // load uncompressed session files.
      let r = await this._readInternal(true);
      result = r.result;
    }

    // All files are corrupted if files found but none could deliver a result.
    let allCorrupt = !noFilesFound && !result;
    Telemetry.getHistogramById("FX_SESSION_RESTORE_ALL_FILES_CORRUPT").
      add(allCorrupt);

    if (!result) {
      // If everything fails, start with an empty session.
      result = {
        origin: "empty",
        source: "",
        parsed: null,
        useOldExtension: false
      };
    }
    this._readOrigin = result.origin;

    result.noFilesFound = noFilesFound;

    // Initialize the worker (in the background) to let it handle backups and also
    // as a workaround for bug 964531.
    this._initWorker();

    return result;
  },

  // Initialize the worker in the background.
  // Since this called _before_ any other messages are posted to the worker (see
  // `_postToWorker()`), we know that this initialization process will be completed
  // on time.
  // Thus, effectively, this blocks callees on its completion.
  // In case of a worker crash/ shutdown during its initialization phase,
  // `_checkWorkerHealth()` will detect it and flip the `_initializationStarted`
  // property back to `false`. This means that we'll respawn the worker upon the
  // next request, followed by the initialization sequence here. In other words;
  // exactly the same procedure as when the worker crashed/ shut down 'regularly'.
  //
  // This will never throw an error.
  _initWorker() {
    return new Promise(resolve => {
      if (this._initializationStarted) {
        resolve();
        return;
      }

      if (!this._readOrigin) {
        throw new Error("_initWorker called too early! Please read the session file from disk first.");
      }

      this._initializationStarted = true;
      SessionWorker.post("init", [this._readOrigin, this._usingOldExtension, this.Paths, {
        maxUpgradeBackups: Services.prefs.getIntPref(PREF_MAX_UPGRADE_BACKUPS, 3),
        maxSerializeBack: Services.prefs.getIntPref(PREF_MAX_SERIALIZE_BACK, 10),
        maxSerializeForward: Services.prefs.getIntPref(PREF_MAX_SERIALIZE_FWD, -1)
      }]).catch(err => {
        // Ensure that we report errors but that they do not stop us.
        Promise.reject(err);
      }).then(resolve);
    });
  },

  // Post a message to the worker, making sure that it has been initialized first.
  async _postToWorker(...args) {
    await this._initWorker();
    return SessionWorker.post(...args);
  },

  /**
   * For good measure, terminate the worker when we've had over `kMaxWriteFailures`
   * amount of failures to deal with. This will spawn a fresh worker upon the next
   * write.
   * This also resets the `_workerHealth` stats.
   */
  _checkWorkerHealth() {
    if (this._workerHealth.failures >= kMaxWriteFailures) {
      SessionWorker.terminate();
      // Flag as not-initialized, to ensure that the worker state init is performed
      // upon the next request.
      this._initializationStarted = false;
      // Reset the counter and report to telemetry.
      this._workerHealth.failures = 0;
      Telemetry.scalarAdd("browser.session.restore.worker_restart_count", 1);
    }
  },

  write(aData) {
    if (RunState.isClosed) {
      return Promise.reject(new Error("SessionFile is closed"));
    }

    let isFinalWrite = false;
    if (RunState.isClosing) {
      // If shutdown has started, we will want to stop receiving
      // write instructions.
      isFinalWrite = true;
      RunState.setClosed();
    }

    let performShutdownCleanup = isFinalWrite &&
      !sessionStartup.isAutomaticRestoreEnabled();

    this._attempts++;
    let options = {isFinalWrite, performShutdownCleanup};
    let promise = this._postToWorker("write", [aData, options]);

    // Wait until the write is done.
    promise = promise.then(msg => {
      // Record how long the write took.
      this._recordTelemetry(msg.telemetry);
      this._successes++;
      if (msg.result.upgradeBackup) {
        // We have just completed a backup-on-upgrade, store the information
        // in preferences.
        Services.prefs.setCharPref(PREF_UPGRADE_BACKUP,
          Services.appinfo.platformBuildID);
      }
    }, err => {
      // Catch and report any errors.
      console.error("Could not write session state file ", err, err.stack);
      this._failures++;
      this._workerHealth.failures++;
      // By not doing anything special here we ensure that |promise| cannot
      // be rejected anymore. The shutdown/cleanup code at the end of the
      // function will thus always be executed.
    });

    // Ensure that we can write sessionstore.js cleanly before the profile
    // becomes unaccessible.
    AsyncShutdown.profileBeforeChange.addBlocker(
      "SessionFile: Finish writing Session Restore data",
      promise,
      {
        fetchState: () => ({
          options,
          attempts: this._attempts,
          successes: this._successes,
          failures: this._failures,
        })
      });

    // This code will always be executed because |promise| can't fail anymore.
    // We ensured that by having a reject handler that reports the failure but
    // doesn't forward the rejection.
    return promise.then(() => {
      // Remove the blocker, no matter if writing failed or not.
      AsyncShutdown.profileBeforeChange.removeBlocker(promise);

      if (isFinalWrite) {
        Services.obs.notifyObservers(null, "sessionstore-final-state-write-complete");
      } else {
        this._checkWorkerHealth();
      }
    });
  },

  wipe() {
    return this._postToWorker("wipe").then(() => {
      // After a wipe, we need to make sure to re-initialize upon the next read(),
      // because the state variables as sent to the worker have changed.
      this._initializationStarted = false;
    });
  },

  _recordTelemetry(telemetry) {
    for (let id of Object.keys(telemetry)) {
      let value = telemetry[id];
      let samples = [];
      if (Array.isArray(value)) {
        samples.push(...value);
      } else {
        samples.push(value);
      }
      let histogram = Telemetry.getHistogramById(id);
      for (let sample of samples) {
        histogram.add(sample);
      }
    }
  }
};
