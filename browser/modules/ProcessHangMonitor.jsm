/* -*- mode: js; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

this.EXPORTED_SYMBOLS = ["ProcessHangMonitor"];

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/Services.jsm");

/**
 * This JSM is responsible for observing content process hang reports
 * and asking the user what to do about them. See nsIHangReport for
 * the platform interface.
 */

var ProcessHangMonitor = {
  /**
   * If a hang hasn't been reported for more than 10 seconds, assume the
   * content process has gotten unstuck (and hide the hang notification).
   */
  get HANG_EXPIRATION_TIME() {
    try {
      return Services.prefs.getIntPref("browser.hangNotification.expiration");
    } catch (ex) {
      return 10000;
    }
  },

  /**
   * This timeout is the wait period applied after a user selects "Wait" in
   * an existing notification.
   */
  get WAIT_EXPIRATION_TIME() {
    try {
      return Services.prefs.getIntPref("browser.hangNotification.waitPeriod");
    } catch (ex) {
      return 10000;
    }
  },

  /**
   * Collection of hang reports that haven't expired or been dismissed
   * by the user. The keys are nsIHangReports and values keys are
   * timers. Each time the hang is reported, the timer is refreshed so
   * it expires after HANG_EXPIRATION_TIME.
   */
  _activeReports: new Map(),

  /**
   * Collection of hang reports that have been suppressed for a
   * short period of time.
   */
  _pausedReports: new Map(),

  /**
   * Initialize hang reporting. Called once in the parent process.
   */
  init: function() {
    Services.obs.addObserver(this, "process-hang-report", false);
    Services.obs.addObserver(this, "xpcom-shutdown", false);
    Services.ww.registerNotification(this);
  },

  /**
   * Terminate JavaScript associated with the hang being reported for
   * the selected browser in |win|.
   */
  terminateScript: function(win) {
    this.handleUserInput(win, report => report.terminateScript());
  },

  /**
   * Start devtools debugger for JavaScript associated with the hang
   * being reported for the selected browser in |win|.
   */
  debugScript: function(win) {
    this.handleUserInput(win, report => {
      function callback() {
        report.endStartingDebugger();
      }

      report.beginStartingDebugger();

      let svc = Cc["@mozilla.org/dom/slow-script-debug;1"].getService(Ci.nsISlowScriptDebug);
      let handler = svc.remoteActivationHandler;
      handler.handleSlowScriptDebug(report.scriptBrowser, callback);
    });
  },

  /**
   * Terminate the plugin process associated with a hang being reported
   * for the selected browser in |win|. Will attempt to generate a combined
   * crash report for all processes.
   */
  terminatePlugin: function(win) {
    this.handleUserInput(win, report => report.terminatePlugin());
  },

  /**
   * Dismiss the browser notification and invoke an appropriate action based on
   * the hang type.
   */
  stopIt: function (win) {
    let report = this.findActiveReport(win.gBrowser.selectedBrowser);
    if (!report) {
      return;
    }

    switch (report.hangType) {
      case report.SLOW_SCRIPT:
        this.terminateScript(win);
        break;
      case report.PLUGIN_HANG:
        this.terminatePlugin(win);
        break;
    }
  },

  /**
   * Dismiss the notification, clear the report from the active list and set up
   * a new timer to track a wait period during which we won't notify.
   */
  waitLonger: function(win) {
    let report = this.findActiveReport(win.gBrowser.selectedBrowser);
    if (!report) {
      return;
    }
    // Remove the report from the active list and cancel its timer.
    this.removeActiveReport(report);

    // NOTE, we didn't call userCanceled on nsIHangReport here. This insures
    // we don't repeatedly generate and cache crash report data for this hang
    // in the process hang reporter. It already has one report for the browser
    // process we want it hold onto.

    // Create a new wait timer with notify callback
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.initWithCallback(() => {
      for (let [stashedReport, otherTimer] of this._pausedReports) {
        if (otherTimer === timer) {
          this.removePausedReport(stashedReport);

          // Create a new notification display timeout timer
          let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
          timer.initWithCallback(this, this.HANG_EXPIRATION_TIME, timer.TYPE_ONE_SHOT);

          // Store the timer in the active reports map. If we receive a new
          // observer notification for this hang, we'll redisplay the browser
          // notification in reportHang below. If we do not receive a new
          // observer, timer will take care fo cleaning up resources associated
          // with this hang. The observer for active hangs fires about once
          // a second.
          this._activeReports.set(report, timer);
          break;
        }
      }
    }, this.WAIT_EXPIRATION_TIME, timer.TYPE_ONE_SHOT);

    this._pausedReports.set(report, timer);

    // remove the browser notification associated with this hang
    this.updateWindows();
  },

  /**
   * If there is a hang report associated with the selected browser in
   * |win|, invoke |func| on that report and stop notifying the user
   * about it.
   */
  handleUserInput: function(win, func) {
    let report = this.findActiveReport(win.gBrowser.selectedBrowser);
    if (!report) {
      return null;
    }
    this.removeActiveReport(report);

    return func(report);
  },

  observe: function(subject, topic, data) {
    switch (topic) {
      case "xpcom-shutdown":
        Services.obs.removeObserver(this, "xpcom-shutdown");
        Services.obs.removeObserver(this, "process-hang-report");
        Services.ww.unregisterNotification(this);
        break;

      case "process-hang-report":
        this.reportHang(subject.QueryInterface(Ci.nsIHangReport));
        break;

      case "domwindowopened":
        // Install event listeners on the new window in case one of
        // its tabs is already hung.
        let win = subject.QueryInterface(Ci.nsIDOMWindow);
        let listener = (ev) => {
          win.removeEventListener("load", listener, true);
          this.updateWindows();
        };
        win.addEventListener("load", listener, true);
        break;
    }
  },

  /**
   * Find a active hang report for the given <browser> element.
   */
  findActiveReport: function(browser) {
    let frameLoader = browser.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader;
    for (let [report, timer] of this._activeReports) {
      if (report.isReportForBrowser(frameLoader)) {
        return report;
      }
    }
    return null;
  },

  /**
   * Find a paused hang report for the given <browser> element.
   */
  findPausedReport: function(browser) {
    let frameLoader = browser.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader;
    for (let [report, timer] of this._pausedReports) {
      if (report.isReportForBrowser(frameLoader)) {
        return report;
      }
    }
    return null;
  },

  /**
   * Remove an active hang report from the active list and cancel the timer
   * associated with it.
   */
  removeActiveReport: function(report) {
    let timer = this._activeReports.get(report);
    if (timer) {
      timer.cancel();
    }
    this._activeReports.delete(report);
    this.updateWindows();
  },

  /**
   * Remove a paused hang report from the paused list and cancel the timer
   * associated with it.
   */
  removePausedReport: function(report) {
    let timer = this._pausedReports.get(report);
    if (timer) {
      timer.cancel();
    }
    this._pausedReports.delete(report);
  },

  /**
   * Iterate over all XUL windows and ensure that the proper hang
   * reports are shown for each one. Also install event handlers in
   * each window to watch for events that would cause a different hang
   * report to be displayed.
   */
  updateWindows: function() {
    let e = Services.wm.getEnumerator("navigator:browser");
    while (e.hasMoreElements()) {
      let win = e.getNext();

      this.updateWindow(win);

      // Only listen for these events if there are active hang reports.
      if (this._activeReports.size) {
        this.trackWindow(win);
      } else {
        this.untrackWindow(win);
      }
    }
  },

  /**
   * If there is a hang report for the current tab in |win|, display it.
   */
  updateWindow: function(win) {
    let report = this.findActiveReport(win.gBrowser.selectedBrowser);

    if (report) {
      this.showNotification(win, report);
    } else {
      this.hideNotification(win);
    }
  },

  /**
   * Show the notification for a hang.
   */
  showNotification: function(win, report) {
    let nb = win.document.getElementById("high-priority-global-notificationbox");
    let notification = nb.getNotificationWithValue("process-hang");
    if (notification) {
      return;
    }

    let bundle = win.gNavigatorBundle;
    let brandBundle = win.document.getElementById("bundle_brand");

    let buttons = [{
        label: bundle.getString("processHang.button_stop.label"),
        accessKey: bundle.getString("processHang.button_stop.accessKey"),
        callback: function() {
          ProcessHangMonitor.stopIt(win);
        }
      },
      {
        label: bundle.getString("processHang.button_wait.label"),
        accessKey: bundle.getString("processHang.button_wait.accessKey"),
        callback: function() {
          ProcessHangMonitor.waitLonger(win);
        }
      }];

    if (AppConstants.MOZ_DEV_EDITION && report.hangType == report.SLOW_SCRIPT) {
      buttons.push({
        label: bundle.getString("processHang.button_debug.label"),
        accessKey: bundle.getString("processHang.button_debug.accessKey"),
        callback: function() {
          ProcessHangMonitor.debugScript(win);
        }
      });
    }

    nb.appendNotification(bundle.getString("processHang.label"),
                          "process-hang",
                          "chrome://browser/content/aboutRobots-icon.png",
                          nb.PRIORITY_WARNING_HIGH, buttons);
  },

  /**
   * Ensure that no hang notifications are visible in |win|.
   */
  hideNotification: function(win) {
    let nb = win.document.getElementById("high-priority-global-notificationbox");
    let notification = nb.getNotificationWithValue("process-hang");
    if (notification) {
      nb.removeNotification(notification);
    }
  },

  /**
   * Install event handlers on |win| to watch for events that would
   * cause a different hang report to be displayed.
   */
  trackWindow: function(win) {
    win.gBrowser.tabContainer.addEventListener("TabSelect", this, true);
    win.gBrowser.tabContainer.addEventListener("TabRemotenessChange", this, true);
  },

  untrackWindow: function(win) {
    win.gBrowser.tabContainer.removeEventListener("TabSelect", this, true);
    win.gBrowser.tabContainer.removeEventListener("TabRemotenessChange", this, true);
  },

  handleEvent: function(event) {
    let win = event.target.ownerDocument.defaultView;

    // If a new tab is selected or if a tab changes remoteness, then
    // we may need to show or hide a hang notification.

    if (event.type == "TabSelect" || event.type == "TabRemotenessChange") {
      this.updateWindow(win);
    }
  },

  /**
   * Handle a potentially new hang report. If it hasn't been seen
   * before, show a notification for it in all open XUL windows.
   */
  reportHang: function(report) {
    // If this hang was already reported reset the timer for it.
    if (this._activeReports.has(report)) {
      let timer = this._activeReports.get(report);
      timer.cancel();
      timer.initWithCallback(this, this.HANG_EXPIRATION_TIME, timer.TYPE_ONE_SHOT);
      // if this report is in active but doesn't have a notification associated
      // with it, display a notification.
      this.updateWindows();
      return;
    }

    // If this hang was already reported and paused by the user ignore it.
    if (this._pausedReports.has(report)) {
      return;
    }

    // On e10s this counts slow-script/hanged-plugin notice only once.
    // This code is not reached on non-e10s.
    if (report.hangType == report.SLOW_SCRIPT) {
      // On non-e10s, SLOW_SCRIPT_NOTICE_COUNT is probed at nsGlobalWindow.cpp
      Services.telemetry.getHistogramById("SLOW_SCRIPT_NOTICE_COUNT").add();
    } else if (report.hangType == report.PLUGIN_HANG) {
      // On non-e10s we have sufficient plugin telemetry probes,
      // so PLUGIN_HANG_NOTICE_COUNT is only probed on e10s.
      Services.telemetry.getHistogramById("PLUGIN_HANG_NOTICE_COUNT").add();
    }

    // Otherwise create a new timer and display the report.
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.initWithCallback(this, this.HANG_EXPIRATION_TIME, timer.TYPE_ONE_SHOT);

    this._activeReports.set(report, timer);
    this.updateWindows();
  },

  /**
   * Callback for when HANG_EXPIRATION_TIME has elapsed.
   */
  notify: function(timer) {
    for (let [otherReport, otherTimer] of this._activeReports) {
      if (otherTimer === timer) {
        this.removeActiveReport(otherReport);
        otherReport.userCanceled();
        break;
      }
    }
  },
};
