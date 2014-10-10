/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["SessionSaver"];

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/Timer.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/TelemetryStopwatch.jsm", this);

XPCOMUtils.defineLazyModuleGetter(this, "console",
  "resource://gre/modules/devtools/Console.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivacyFilter",
  "resource:///modules/sessionstore/PrivacyFilter.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RevivableWindows",
  "resource:///modules/sessionstore/RevivableWindows.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SessionStore",
  "resource:///modules/sessionstore/SessionStore.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SessionFile",
  "resource:///modules/sessionstore/SessionFile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");

// Minimal interval between two save operations (in milliseconds).
XPCOMUtils.defineLazyGetter(this, "gInterval", function () {
  const PREF = "browser.sessionstore.interval";

  // Observer that updates the cached value when the preference changes.
  Services.prefs.addObserver(PREF, () => {
    this.gInterval = Services.prefs.getIntPref(PREF);

    // Cancel any pending runs and call runDelayed() with
    // zero to apply the newly configured interval.
    SessionSaverInternal.cancel();
    SessionSaverInternal.runDelayed(0);
  }, false);

  return Services.prefs.getIntPref(PREF);
});

// Notify observers about a given topic with a given subject.
function notify(subject, topic) {
  Services.obs.notifyObservers(subject, topic, "");
}

// TelemetryStopwatch helper functions.
function stopWatch(method) {
  return function (...histograms) {
    for (let hist of histograms) {
      TelemetryStopwatch[method]("FX_SESSION_RESTORE_" + hist);
    }
  };
}

let stopWatchStart = stopWatch("start");
let stopWatchCancel = stopWatch("cancel");
let stopWatchFinish = stopWatch("finish");

/**
 * The external API implemented by the SessionSaver module.
 */
this.SessionSaver = Object.freeze({
  /**
   * Immediately saves the current session to disk.
   */
  run: function () {
    return SessionSaverInternal.run();
  },

  /**
   * Saves the current session to disk delayed by a given amount of time. Should
   * another delayed run be scheduled already, we will ignore the given delay
   * and state saving may occur a little earlier.
   */
  runDelayed: function () {
    SessionSaverInternal.runDelayed();
  },

  /**
   * Sets the last save time to the current time. This will cause us to wait for
   * at least the configured interval when runDelayed() is called next.
   */
  updateLastSaveTime: function () {
    SessionSaverInternal.updateLastSaveTime();
  },

  /**
   * Sets the last save time to zero. This will cause us to
   * immediately save the next time runDelayed() is called.
   */
  clearLastSaveTime: function () {
    SessionSaverInternal.clearLastSaveTime();
  },

  /**
   * Cancels all pending session saves.
   */
  cancel: function () {
    SessionSaverInternal.cancel();
  }
});

/**
 * The internal API.
 */
let SessionSaverInternal = {
  /**
   * The timeout ID referencing an active timer for a delayed save. When no
   * save is pending, this is null.
   */
  _timeoutID: null,

  /**
   * A timestamp that keeps track of when we saved the session last. We will
   * this to determine the correct interval between delayed saves to not deceed
   * the configured session write interval.
   */
  _lastSaveTime: 0,

  /**
   * Immediately saves the current session to disk.
   */
  run: function () {
    return this._saveState(true /* force-update all windows */);
  },

  /**
   * Saves the current session to disk delayed by a given amount of time. Should
   * another delayed run be scheduled already, we will ignore the given delay
   * and state saving may occur a little earlier.
   *
   * @param delay (optional)
   *        The minimum delay in milliseconds to wait for until we collect and
   *        save the current session.
   */
  runDelayed: function (delay = 2000) {
    // Bail out if there's a pending run.
    if (this._timeoutID) {
      return;
    }

    // Interval until the next disk operation is allowed.
    delay = Math.max(this._lastSaveTime + gInterval - Date.now(), delay, 0);

    // Schedule a state save.
    this._timeoutID = setTimeout(() => this._saveStateAsync(), delay);
  },

  /**
   * Sets the last save time to the current time. This will cause us to wait for
   * at least the configured interval when runDelayed() is called next.
   */
  updateLastSaveTime: function () {
    this._lastSaveTime = Date.now();
  },

  /**
   * Sets the last save time to zero. This will cause us to
   * immediately save the next time runDelayed() is called.
   */
  clearLastSaveTime: function () {
    this._lastSaveTime = 0;
  },

  /**
   * Cancels all pending session saves.
   */
  cancel: function () {
    clearTimeout(this._timeoutID);
    this._timeoutID = null;
  },

  /**
   * Saves the current session state. Collects data and writes to disk.
   *
   * @param forceUpdateAllWindows (optional)
   *        Forces us to recollect data for all windows and will bypass and
   *        update the corresponding caches.
   */
  _saveState: function (forceUpdateAllWindows = false) {
    // Cancel any pending timeouts.
    this.cancel();

    if (PrivateBrowsingUtils.permanentPrivateBrowsing) {
      // Don't save (or even collect) anything in permanent private
      // browsing mode

      this.updateLastSaveTime();
      return Promise.resolve();
    }

    stopWatchStart("COLLECT_DATA_MS", "COLLECT_DATA_LONGEST_OP_MS");
    let state = SessionStore.getCurrentState(forceUpdateAllWindows);
    PrivacyFilter.filterPrivateWindowsAndTabs(state);

    // Make sure that we keep the previous session if we started with a single
    // private window and no non-private windows have been opened, yet.
    if (state.deferredInitialState) {
      state.windows = state.deferredInitialState.windows || [];
      delete state.deferredInitialState;
    }

    // We want to revive closed windows that have been closed in succession
    // without any user action in between closing those. This happens here in
    // the SessionSaver because we only want to revive when saving to disk.
    // On Mac OS X this list will always be empty.
    let windowsToRevive = RevivableWindows.get();
    state.windows.unshift(...windowsToRevive);
    let revivedWindows = state._closedWindows.splice(0, windowsToRevive.length);
#ifdef DEBUG
    // Check that the windows to revive equal the windows
    // that we removed from the list of closed windows.
    let match = revivedWindows.every((win, idx) => {
      return win == windowsToRevive[windowsToRevive.length - 1 - idx];
    });

    if (!match) {
      throw new Error("SessionStore: revived windows didn't match closed windows");
    }
#endif DEBUG

    stopWatchFinish("COLLECT_DATA_MS", "COLLECT_DATA_LONGEST_OP_MS");
    return this._writeState(state);
  },

  /**
   * Saves the current session state. Collects data asynchronously and calls
   * _saveState() to collect data again (with a cache hit rate of hopefully
   * 100%) and write to disk afterwards.
   */
  _saveStateAsync: function () {
    // Allow scheduling delayed saves again.
    this._timeoutID = null;

    // Write to disk.
    this._saveState();
  },

  /**
   * Write the given state object to disk.
   */
  _writeState: function (state) {
    // Inform observers
    notify(null, "sessionstore-state-write");

    stopWatchStart("SERIALIZE_DATA_MS", "SERIALIZE_DATA_LONGEST_OP_MS", "WRITE_STATE_LONGEST_OP_MS");
    let data = JSON.stringify(state);
    stopWatchFinish("SERIALIZE_DATA_MS", "SERIALIZE_DATA_LONGEST_OP_MS");

    // We update the time stamp before writing so that we don't write again
    // too soon, if saving is requested before the write completes. Without
    // this update we may save repeatedly if actions cause a runDelayed
    // before writing has completed. See Bug 902280
    this.updateLastSaveTime();

    // Write (atomically) to a session file, using a tmp file. Once the session
    // file is successfully updated, save the time stamp of the last save and
    // notify the observers.
    stopWatchStart("SEND_SERIALIZED_STATE_LONGEST_OP_MS");
    let promise = SessionFile.write(data);
    stopWatchFinish("WRITE_STATE_LONGEST_OP_MS",
                    "SEND_SERIALIZED_STATE_LONGEST_OP_MS");
    promise = promise.then(() => {
      this.updateLastSaveTime();
      notify(null, "sessionstore-state-write-complete");
    }, console.error);

    return promise;
  },
};
