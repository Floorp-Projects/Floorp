/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Session Storage and Restoration
 *
 * Overview
 * This service reads user's session file at startup, and makes a determination
 * as to whether the session should be restored. It will restore the session
 * under the circumstances described below.  If the auto-start Private Browsing
 * mode is active, however, the session is never restored.
 *
 * Crash Detection
 * The CrashMonitor is used to check if the final session state was successfully
 * written at shutdown of the last session. If we did not reach
 * 'sessionstore-final-state-write-complete', then it's assumed that the browser
 * has previously crashed and we should restore the session.
 *
 * Forced Restarts
 * In the event that a restart is required due to application update or extension
 * installation, set the browser.sessionstore.resume_session_once pref to true,
 * and the session will be restored the next time the browser starts.
 *
 * Always Resume
 * This service will always resume the session if the integer pref
 * browser.startup.page is set to 3.
 */

var EXPORTED_SYMBOLS = ["SessionStartup"];

/* :::::::: Constants and Helpers ::::::::::::::: */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "SessionFile",
  "resource:///modules/sessionstore/SessionFile.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "StartupPerformance",
  "resource:///modules/sessionstore/StartupPerformance.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "CrashMonitor",
  "resource://gre/modules/CrashMonitor.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

const STATE_RUNNING_STR = "running";

const TYPE_NO_SESSION = 0;
const TYPE_RECOVER_SESSION = 1;
const TYPE_RESUME_SESSION = 2;
const TYPE_DEFER_SESSION = 3;

// 'browser.startup.page' preference value to resume the previous session.
const BROWSER_STARTUP_RESUME_SESSION = 3;

function warning(msg, exception) {
  let consoleMsg = Cc["@mozilla.org/scripterror;1"].createInstance(
    Ci.nsIScriptError
  );
  consoleMsg.init(
    msg,
    exception.fileName,
    null,
    exception.lineNumber,
    0,
    Ci.nsIScriptError.warningFlag,
    "component javascript"
  );
  Services.console.logMessage(consoleMsg);
}

var gOnceInitializedDeferred = (function() {
  let deferred = {};

  deferred.promise = new Promise((resolve, reject) => {
    deferred.resolve = resolve;
    deferred.reject = reject;
  });

  return deferred;
})();

/* :::::::: The Service ::::::::::::::: */

var SessionStartup = {
  NO_SESSION: TYPE_NO_SESSION,
  RECOVER_SESSION: TYPE_RECOVER_SESSION,
  RESUME_SESSION: TYPE_RESUME_SESSION,
  DEFER_SESSION: TYPE_DEFER_SESSION,

  // The state to restore at startup.
  _initialState: null,
  _sessionType: null,
  _initialized: false,

  // Stores whether the previous session crashed.
  _previousSessionCrashed: null,

  _resumeSessionEnabled: null,

  /* ........ Global Event Handlers .............. */

  /**
   * Initialize the component
   */
  init() {
    Services.obs.notifyObservers(null, "sessionstore-init-started");
    StartupPerformance.init();

    // do not need to initialize anything in auto-started private browsing sessions
    if (PrivateBrowsingUtils.permanentPrivateBrowsing) {
      this._initialized = true;
      gOnceInitializedDeferred.resolve();
      return;
    }

    if (
      Services.prefs.getBoolPref(
        "browser.sessionstore.resuming_after_os_restart"
      )
    ) {
      if (!Services.appinfo.restartedByOS) {
        // We had set resume_session_once in order to resume after an OS restart,
        // but we aren't automatically started by the OS (or else appinfo.restartedByOS
        // would have been set). Therefore we should clear resume_session_once
        // to avoid forcing a resume for a normal startup.
        Services.prefs.setBoolPref(
          "browser.sessionstore.resume_session_once",
          false
        );
      }
      Services.prefs.setBoolPref(
        "browser.sessionstore.resuming_after_os_restart",
        false
      );
    }

    SessionFile.read().then(this._onSessionFileRead.bind(this), console.error);
  },

  // Wrap a string as a nsISupports.
  _createSupportsString(data) {
    let string = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString
    );
    string.data = data;
    return string;
  },

  /**
   * Complete initialization once the Session File has been read.
   *
   * @param source The Session State string read from disk.
   * @param parsed The object obtained by parsing |source| as JSON.
   */
  _onSessionFileRead({ source, parsed, noFilesFound }) {
    this._initialized = true;

    // Let observers modify the state before it is used
    let supportsStateString = this._createSupportsString(source);
    Services.obs.notifyObservers(
      supportsStateString,
      "sessionstore-state-read"
    );
    let stateString = supportsStateString.data;

    if (stateString != source) {
      // The session has been modified by an add-on, reparse.
      try {
        this._initialState = JSON.parse(stateString);
      } catch (ex) {
        // That's not very good, an add-on has rewritten the initial
        // state to something that won't parse.
        warning("Observer rewrote the state to something that won't parse", ex);
      }
    } else {
      // No need to reparse
      this._initialState = parsed;
    }

    if (this._initialState == null) {
      // No valid session found.
      this._sessionType = this.NO_SESSION;
      Services.obs.notifyObservers(null, "sessionstore-state-finalized");
      gOnceInitializedDeferred.resolve();
      return;
    }

    let initialState = this._initialState;
    Services.tm.idleDispatchToMainThread(() => {
      let pinnedTabCount = initialState.windows.reduce((winAcc, win) => {
        return (
          winAcc +
          win.tabs.reduce((tabAcc, tab) => {
            return tabAcc + (tab.pinned ? 1 : 0);
          }, 0)
        );
      }, 0);
      Services.telemetry.scalarSetMaximum(
        "browser.engagement.max_concurrent_tab_pinned_count",
        pinnedTabCount
      );
    }, 60000);

    // If this is a normal restore then throw away any previous session.
    if (!this.isAutomaticRestoreEnabled() && this._initialState) {
      delete this._initialState.lastSessionState;
    }

    CrashMonitor.previousCheckpoints.then(checkpoints => {
      if (checkpoints) {
        // If the previous session finished writing the final state, we'll
        // assume there was no crash.
        this._previousSessionCrashed = !checkpoints[
          "sessionstore-final-state-write-complete"
        ];
      } else if (noFilesFound) {
        // If the Crash Monitor could not load a checkpoints file it will
        // provide null. This could occur on the first run after updating to
        // a version including the Crash Monitor, or if the checkpoints file
        // was removed, or on first startup with this profile, or after Firefox Reset.

        // There was no checkpoints file and no sessionstore.js or its backups,
        // so we will assume that this was a fresh profile.
        this._previousSessionCrashed = false;
      } else {
        // If this is the first run after an update, sessionstore.js should
        // still contain the session.state flag to indicate if the session
        // crashed. If it is not present, we will assume this was not the first
        // run after update and the checkpoints file was somehow corrupted or
        // removed by a crash.
        //
        // If the session.state flag is present, we will fallback to using it
        // for crash detection - If the last write of sessionstore.js had it
        // set to "running", we crashed.
        let stateFlagPresent =
          this._initialState.session && this._initialState.session.state;

        this._previousSessionCrashed =
          !stateFlagPresent ||
          this._initialState.session.state == STATE_RUNNING_STR;
      }

      // Report shutdown success via telemetry. Shortcoming here are
      // being-killed-by-OS-shutdown-logic, shutdown freezing after
      // session restore was written, etc.
      Services.telemetry
        .getHistogramById("SHUTDOWN_OK")
        .add(!this._previousSessionCrashed);

      Services.obs.addObserver(this, "sessionstore-windows-restored", true);

      if (this.sessionType == this.NO_SESSION) {
        this._initialState = null; // Reset the state.
      } else {
        Services.obs.addObserver(this, "browser:purge-session-history", true);
      }

      // We're ready. Notify everyone else.
      Services.obs.notifyObservers(null, "sessionstore-state-finalized");

      gOnceInitializedDeferred.resolve();
    });
  },

  /**
   * Handle notifications
   */
  observe(subject, topic, data) {
    switch (topic) {
      case "sessionstore-windows-restored":
        Services.obs.removeObserver(this, "sessionstore-windows-restored");
        // Free _initialState after nsSessionStore is done with it.
        this._initialState = null;
        this._didRestore = true;
        break;
      case "browser:purge-session-history":
        Services.obs.removeObserver(this, "browser:purge-session-history");
        // Reset all state on sanitization.
        this._sessionType = this.NO_SESSION;
        break;
    }
  },

  /* ........ Public API ................*/

  get onceInitialized() {
    return gOnceInitializedDeferred.promise;
  },

  /**
   * Get the session state as a jsval
   */
  get state() {
    return this._initialState;
  },

  /**
   * Determines whether automatic session restoration is enabled for this
   * launch of the browser. This does not include crash restoration. In
   * particular, if session restore is configured to restore only in case of
   * crash, this method returns false.
   * @returns bool
   */
  isAutomaticRestoreEnabled() {
    if (this._resumeSessionEnabled === null) {
      this._resumeSessionEnabled =
        !PrivateBrowsingUtils.permanentPrivateBrowsing &&
        (Services.prefs.getBoolPref(
          "browser.sessionstore.resume_session_once"
        ) ||
          Services.prefs.getIntPref("browser.startup.page") ==
            BROWSER_STARTUP_RESUME_SESSION);
    }

    return this._resumeSessionEnabled;
  },

  /**
   * Determines whether there is a pending session restore.
   * @returns bool
   */
  willRestore() {
    return (
      this.sessionType == this.RECOVER_SESSION ||
      this.sessionType == this.RESUME_SESSION
    );
  },

  /**
   * Determines whether there is a pending session restore and if that will refer
   * back to a crash.
   * @returns bool
   */
  willRestoreAsCrashed() {
    return this.sessionType == this.RECOVER_SESSION;
  },

  /**
   * Returns a boolean or a promise that resolves to a boolean, indicating
   * whether we will restore a session that ends up replacing the homepage.
   * True guarantees that we'll restore a session; false means that we
   * /probably/ won't do so.
   * The browser uses this to avoid unnecessarily loading the homepage when
   * restoring a session.
   */
  get willOverrideHomepage() {
    // If the session file hasn't been read yet and resuming the session isn't
    // enabled via prefs, go ahead and load the homepage. We may still replace
    // it when recovering from a crash, which we'll only know after reading the
    // session file, but waiting for that would delay loading the homepage in
    // the non-crash case.
    if (!this._initialState && !this.isAutomaticRestoreEnabled()) {
      return false;
    }
    // If we've already restored the session, we won't override again.
    if (this._didRestore) {
      return false;
    }

    return new Promise(resolve => {
      this.onceInitialized.then(() => {
        // If there are valid windows with not only pinned tabs, signal that we
        // will override the default homepage by restoring a session.
        resolve(
          this.willRestore() &&
            this._initialState &&
            this._initialState.windows &&
            this._initialState.windows.some(w => w.tabs.some(t => !t.pinned))
        );
      });
    });
  },

  /**
   * Get the type of pending session store, if any.
   */
  get sessionType() {
    if (this._sessionType === null) {
      let resumeFromCrash = Services.prefs.getBoolPref(
        "browser.sessionstore.resume_from_crash"
      );
      // Set the startup type.
      if (this.isAutomaticRestoreEnabled()) {
        this._sessionType = this.RESUME_SESSION;
      } else if (this._previousSessionCrashed && resumeFromCrash) {
        this._sessionType = this.RECOVER_SESSION;
      } else if (this._initialState) {
        this._sessionType = this.DEFER_SESSION;
      } else {
        this._sessionType = this.NO_SESSION;
      }
    }

    return this._sessionType;
  },

  /**
   * Get whether the previous session crashed.
   */
  get previousSessionCrashed() {
    return this._previousSessionCrashed;
  },

  resetForTest() {
    this._resumeSessionEnabled = null;
    this._sessionType = null;
  },

  QueryInterface: ChromeUtils.generateQI([
    "nsIObserver",
    "nsISupportsWeakReference",
  ]),
};
