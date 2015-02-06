/* -*- mode: js; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

this.EXPORTED_SYMBOLS = ["ProcessHangMonitor"];

Cu.import("resource://gre/modules/Services.jsm");

/**
 * This JSM is responsible for observing content process hang reports
 * and asking the user what to do about them. See nsIHangReport for
 * the platform interface.
 */

/**
 * If a hang hasn't been reported for more than 10 seconds, assume the
 * content process has gotten unstuck (and hide the hang notification).
 */
const HANG_EXPIRATION_TIME = 10000;

let ProcessHangMonitor = {
  /**
   * Collection of hang reports that haven't expired or been dismissed
   * by the user. The keys are nsIHangReports and values keys are
   * timers. Each time the hang is reported, the timer is refreshed so
   * it expires after HANG_EXPIRATION_TIME.
   */
  _activeReports: new Map(),

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
   * Kill the plugin process causing the hang being reported for the
   * selected browser in |win|.
   */
  terminatePlugin: function(win) {
    this.handleUserInput(win, report => report.terminatePlugin());
  },

  /**
   * Kill the content process causing the hang being reported for the selected
   * browser in |win|.
   */
  terminateProcess: function(win) {
    this.handleUserInput(win, report => report.terminateProcess());
  },

  /**
   * Update the "Options" pop-up menu for the hang notification
   * associated with the selected browser in |win|. The menu should
   * display only options that are relevant to the given report.
   */
  refreshMenu: function(win) {
    let report = this.findReport(win.gBrowser.selectedBrowser);
    if (!report) {
      return;
    }

    function setVisible(id, visible) {
      let item = win.document.getElementById(id);
      item.hidden = !visible;
    }

    if (report.hangType == report.SLOW_SCRIPT) {
      setVisible("processHangTerminateScript", true);
      setVisible("processHangDebugScript", true);
      setVisible("processHangTerminatePlugin", false);
    } else if (report.hangType == report.PLUGIN_HANG) {
      setVisible("processHangTerminateScript", false);
      setVisible("processHangDebugScript", false);
      setVisible("processHangTerminatePlugin", true);
    }
  },

  /**
   * If there is a hang report associated with the selected browser in
   * |win|, invoke |func| on that report and stop notifying the user
   * about it.
   */
  handleUserInput: function(win, func) {
    let report = this.findReport(win.gBrowser.selectedBrowser);
    if (!report) {
      return;
    }
    this.removeReport(report);

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
   * Find any active hang reports for the given <browser> element.
   */
  findReport: function(browser) {
    let frameLoader = browser.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader;
    for (let [report, timer] of this._activeReports) {
      if (report.isReportForBrowser(frameLoader)) {
        return report;
      }
    }
    return null;
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
    let report = this.findReport(win.gBrowser.selectedBrowser);

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
    let appName = brandBundle.getString("brandShortName");
    let message = bundle.getFormattedString(
      "processHang.message",
      [appName]);

    let buttons = [{
      label: bundle.getString("processHang.button.label"),
      accessKey: bundle.getString("processHang.button.accessKey"),
      popup: "processHangOptions",
      callback: null,
    }];

    nb.appendNotification(message, "process-hang",
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
    // If this hang was already reported, then reset the timer for it.
    if (this._activeReports.has(report)) {
      let timer = this._activeReports.get(report);
      timer.cancel();
      timer.initWithCallback(this, HANG_EXPIRATION_TIME, timer.TYPE_ONE_SHOT);
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
    timer.initWithCallback(this, HANG_EXPIRATION_TIME, timer.TYPE_ONE_SHOT);

    this._activeReports.set(report, timer);
    this.updateWindows();
  },

  /**
   * Dismiss a hang report because the user closed the notification
   * for it or the report expired.
   */
  removeReport: function(report) {
    this._activeReports.delete(report);
    this.updateWindows();
  },

  /**
   * Callback for when HANG_EXPIRATION_TIME has elapsed.
   */
  notify: function(timer) {
    for (let [otherReport, otherTimer] of this._activeReports) {
      if (otherTimer === timer) {
        this.removeReport(otherReport);
        break;
      }
    }
  },
};
