/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["StartupPerformance"];

const { utils: Cu, classes: Cc, interfaces: Ci } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);

XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "console",
  "resource://gre/modules/Console.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "setTimeout",
  "resource://gre/modules/Timer.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "clearTimeout",
  "resource://gre/modules/Timer.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");

const COLLECT_RESULTS_AFTER_MS = 10000;

const OBSERVED_TOPICS = ["sessionstore-restoring-on-startup", "sessionstore-initiating-manual-restore"];

this.StartupPerformance = {
  /**
   * Once we have finished restoring initial tabs, we broadcast on this topic.
   */
  RESTORED_TOPIC: "sessionstore-finished-restoring-initial-tabs",

  // Instant at which we have started restoration (notification "sessionstore-restoring-on-startup")
  _startTimeStamp: null,

  // Latest instant at which we have finished restoring a tab (DOM event "SSTabRestored")
  _latestRestoredTimeStamp: null,

  // A promise resolved once we have finished restoring all the startup tabs.
  _promiseFinished: null,

  // Function `resolve()` for `_promiseFinished`.
  _resolveFinished: null,

  // A timer
  _deadlineTimer: null,

  // `true` once the timer has fired
  _hasFired: false,

  // `true` once we are restored
  _isRestored: false,

  // Statistics on the session we need to restore.
  _totalNumberOfEagerTabs: 0,
  _totalNumberOfTabs: 0,
  _totalNumberOfWindows: 0,

  init() {
    for (let topic of OBSERVED_TOPICS) {
      Services.obs.addObserver(this, topic);
    }
  },

  /**
   * Return the timestamp at which we finished restoring the latest tab.
   *
   * This information is not really interesting until we have finished restoring
   * tabs.
   */
  get latestRestoredTimeStamp() {
    return this._latestRestoredTimeStamp;
  },

  /**
   * `true` once we have finished restoring startup tabs.
   */
  get isRestored() {
    return this._isRestored;
  },

  // Called when restoration starts.
  // Record the start timestamp, setup the timer and `this._promiseFinished`.
  // Behavior is unspecified if there was already an ongoing measure.
  _onRestorationStarts(isAutoRestore) {
    this._latestRestoredTimeStamp = this._startTimeStamp = Date.now();
    this._totalNumberOfEagerTabs = 0;
    this._totalNumberOfTabs = 0;
    this._totalNumberOfWindows = 0;

    // While we may restore several sessions in a single run of the browser,
    // that's a very unusual case, and not really worth measuring, so let's
    // stop listening for further restorations.

    for (let topic of OBSERVED_TOPICS) {
      Services.obs.removeObserver(this, topic);
    }

    Services.obs.addObserver(this, "sessionstore-single-window-restored");
    this._promiseFinished = new Promise(resolve => {
      this._resolveFinished = resolve;
    });
    this._promiseFinished.then(() => {
      try {
        this._isRestored = true;
        Services.obs.notifyObservers(null, this.RESTORED_TOPIC, "");

        if (this._latestRestoredTimeStamp == this._startTimeStamp) {
          // Apparently, we haven't restored any tab.
          return;
        }

        // Once we are done restoring tabs, update Telemetry.
        let histogramName = isAutoRestore ?
          "FX_SESSION_RESTORE_AUTO_RESTORE_DURATION_UNTIL_EAGER_TABS_RESTORED_MS" :
          "FX_SESSION_RESTORE_MANUAL_RESTORE_DURATION_UNTIL_EAGER_TABS_RESTORED_MS";
        let histogram = Services.telemetry.getHistogramById(histogramName);
        let delta = this._latestRestoredTimeStamp - this._startTimeStamp;
        histogram.add(delta);

        Services.telemetry.getHistogramById("FX_SESSION_RESTORE_NUMBER_OF_EAGER_TABS_RESTORED").add(this._totalNumberOfEagerTabs);
        Services.telemetry.getHistogramById("FX_SESSION_RESTORE_NUMBER_OF_TABS_RESTORED").add(this._totalNumberOfTabs);
        Services.telemetry.getHistogramById("FX_SESSION_RESTORE_NUMBER_OF_WINDOWS_RESTORED").add(this._totalNumberOfWindows);

        // Reset
        this._startTimeStamp = null;
     } catch (ex) {
        console.error("StartupPerformance: error after resolving promise", ex);
      }
    });
  },

  _startTimer() {
    if (this._hasFired) {
      return;
    }
    if (this._deadlineTimer) {
      clearTimeout(this._deadlineTimer);
    }
    this._deadlineTimer = setTimeout(() => {
      try {
        this._resolveFinished();
      } catch (ex) {
        console.error("StartupPerformance: Error in timeout handler", ex);
      } finally {
        // Clean up.
        this._deadlineTimer = null;
        this._hasFired = true;
        this._resolveFinished = null;
        Services.obs.removeObserver(this, "sessionstore-single-window-restored");
      }
    }, COLLECT_RESULTS_AFTER_MS);
  },

  observe(subject, topic, details) {
    try {
      switch (topic) {
        case "sessionstore-restoring-on-startup":
          this._onRestorationStarts(true);
          break;
        case "sessionstore-initiating-manual-restore":
          this._onRestorationStarts(false);
          break;
        case "sessionstore-single-window-restored": {
          // Session Restore has just opened a window with (initially empty) tabs.
          // Some of these tabs will be restored eagerly, while others will be
          // restored on demand. The process becomes usable only when all windows
          // have finished restored their eager tabs.
          //
          // While it would be possible to track the restoration of each tab
          // from within SessionRestore to determine exactly when the process
          // becomes usable, experience shows that this is too invasive. Rather,
          // we employ the following heuristic:
          // - we maintain a timer of `COLLECT_RESULTS_AFTER_MS` that we expect
          //   will be triggered only once all tabs have been restored;
          // - whenever we restore a new window (hence a bunch of eager tabs),
          //   we postpone the timer to ensure that the new eager tabs have
          //   `COLLECT_RESULTS_AFTER_MS` to be restored;
          // - whenever a tab is restored, we update
          //   `this._latestRestoredTimeStamp`;
          // - after `COLLECT_RESULTS_AFTER_MS`, we collect the final version
          //   of `this._latestRestoredTimeStamp`, and use it to determine the
          //   entire duration of the collection.
          //
          // Note that this heuristic may be inaccurate if a user clicks
          // immediately on a restore-on-demand tab before the end of
          // `COLLECT_RESULTS_AFTER_MS`. We assume that this will not
          // affect too much the results.
          //
          // Reset the delay, to give the tabs a little (more) time to restore.
          this._startTimer();

          this._totalNumberOfWindows += 1;

          // Observe the restoration of all tabs. We assume that all tabs of this
          // window will have been restored before `COLLECT_RESULTS_AFTER_MS`.
          // The last call to `observer` will let us determine how long it took
          // to reach that point.
          let win = subject;

          let observer = (event) => {
            // We don't care about tab restorations that are due to
            // a browser flipping from out-of-main-process to in-main-process
            // or vice-versa. We only care about restorations that are due
            // to the user switching to a lazily restored tab, or for tabs
            // that are restoring eagerly.
            if (!event.detail.isRemotenessUpdate) {
              this._latestRestoredTimeStamp = Date.now();
              this._totalNumberOfEagerTabs += 1;
            }
          };
          win.gBrowser.tabContainer.addEventListener("SSTabRestored", observer);
          this._totalNumberOfTabs += win.gBrowser.tabContainer.itemCount;

          // Once we have finished collecting the results, clean up the observers.
          this._promiseFinished.then(() => {
            if (!win.gBrowser.tabContainer) {
              // May be undefined during shutdown and/or some tests.
              return;
            }
            win.gBrowser.tabContainer.removeEventListener("SSTabRestored", observer);
          });
        }
        break;
        default:
          throw new Error(`Unexpected topic ${topic}`);
      }
    } catch (ex) {
      console.error("StartupPerformance error", ex, ex.stack);
      throw ex;
    }
  }
};
