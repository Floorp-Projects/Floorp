/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

this.EXPORTED_SYMBOLS = [ "TabCrashHandler",
                          "PluginCrashReporter",
                          "UnsubmittedCrashHandler" ];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "CrashSubmit",
  "resource://gre/modules/CrashSubmit.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
  "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RemotePages",
  "resource://gre/modules/RemotePageManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SessionStore",
  "resource:///modules/sessionstore/SessionStore.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RecentWindow",
  "resource:///modules/RecentWindow.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PluralForm",
  "resource://gre/modules/PluralForm.jsm");

XPCOMUtils.defineLazyGetter(this, "gNavigatorBundle", function() {
  const url = "chrome://browser/locale/browser.properties";
  return Services.strings.createBundle(url);
});

// We don't process crash reports older than 28 days, so don't bother
// submitting them
const PENDING_CRASH_REPORT_DAYS = 28;
const DAY = 24 * 60 * 60 * 1000; // milliseconds
const DAYS_TO_SUPPRESS = 30;

this.TabCrashHandler = {
  _crashedTabCount: 0,

  get prefs() {
    delete this.prefs;
    return this.prefs = Services.prefs.getBranch("browser.tabs.crashReporting.");
  },

  init: function () {
    if (this.initialized)
      return;
    this.initialized = true;

    if (AppConstants.MOZ_CRASHREPORTER) {
      Services.obs.addObserver(this, "ipc:content-shutdown", false);
      Services.obs.addObserver(this, "oop-frameloader-crashed", false);

      this.childMap = new Map();
      this.browserMap = new WeakMap();
    }

    this.pageListener = new RemotePages("about:tabcrashed");
    // LOAD_BACKGROUND pages don't fire load events, so the about:tabcrashed
    // content will fire up its own message when its initial scripts have
    // finished running.
    this.pageListener.addMessageListener("Load", this.receiveMessage.bind(this));
    this.pageListener.addMessageListener("RemotePage:Unload", this.receiveMessage.bind(this));
    this.pageListener.addMessageListener("closeTab", this.receiveMessage.bind(this));
    this.pageListener.addMessageListener("restoreTab", this.receiveMessage.bind(this));
    this.pageListener.addMessageListener("restoreAll", this.receiveMessage.bind(this));
  },

  observe: function (aSubject, aTopic, aData) {
    switch (aTopic) {
      case "ipc:content-shutdown":
        aSubject.QueryInterface(Ci.nsIPropertyBag2);

        if (!aSubject.get("abnormal"))
          return;

        this.childMap.set(aSubject.get("childID"), aSubject.get("dumpID"));
        break;

      case "oop-frameloader-crashed":
        aSubject.QueryInterface(Ci.nsIFrameLoader);

        let browser = aSubject.ownerElement;
        if (!browser)
          return;

        this.browserMap.set(browser.permanentKey, aSubject.childID);
        break;
    }
  },

  receiveMessage: function(message) {
    let browser = message.target.browser;
    let gBrowser = browser.ownerGlobal.gBrowser;
    let tab = gBrowser.getTabForBrowser(browser);

    switch (message.name) {
      case "Load": {
        this.onAboutTabCrashedLoad(message);
        break;
      }

      case "RemotePage:Unload": {
        this.onAboutTabCrashedUnload(message);
        break;
      }

      case "closeTab": {
        this.maybeSendCrashReport(message);
        gBrowser.removeTab(tab, { animate: true });
        break;
      }

      case "restoreTab": {
        this.maybeSendCrashReport(message);
        SessionStore.reviveCrashedTab(tab);
        break;
      }

      case "restoreAll": {
        this.maybeSendCrashReport(message);
        SessionStore.reviveAllCrashedTabs();
        break;
      }
    }
  },

  /**
   * Submits a crash report from about:tabcrashed, if the crash
   * reporter is enabled and a crash report can be found.
   *
   * @param aBrowser
   *        The <xul:browser> that the report was sent from.
   * @param aFormData
   *        An Object with the following properties:
   *
   *        includeURL (bool):
   *          Whether to include the URL that the user was on
   *          in the crashed tab before the crash occurred.
   *        URL (String)
   *          The URL that the user was on in the crashed tab
   *          before the crash occurred.
   *        emailMe (bool):
   *          Whether or not to include the user's email address
   *          in the crash report.
   *        email (String):
   *          The email address of the user.
   *        comments (String):
   *          Any additional comments from the user.
   *
   *        Note that it is expected that all properties are set,
   *        even if they are empty.
   */
  maybeSendCrashReport(message) {
    if (!AppConstants.MOZ_CRASHREPORTER)
      return;

    let browser = message.target.browser;

    let childID = this.browserMap.get(browser.permanentKey);
    let dumpID = this.childMap.get(childID);
    if (!dumpID)
      return

    if (!message.data.sendReport) {
      Services.telemetry.getHistogramById("FX_CONTENT_CRASH_NOT_SUBMITTED").add(1);
      this.prefs.setBoolPref("sendReport", false);
      return;
    }

    let {
      includeURL,
      comments,
      email,
      emailMe,
      URL,
    } = message.data;

    let extraExtraKeyVals = {
      "Comments": comments,
      "Email": email,
      "URL": URL,
    };

    // For the entries in extraExtraKeyVals, we only want to submit the
    // extra data values where they are not the empty string.
    for (let key in extraExtraKeyVals) {
      let val = extraExtraKeyVals[key].trim();
      if (!val) {
        delete extraExtraKeyVals[key];
      }
    }

    // URL is special, since it's already been written to extra data by
    // default. In order to make sure we don't send it, we overwrite it
    // with the empty string.
    if (!includeURL) {
      extraExtraKeyVals["URL"] = "";
    }

    CrashSubmit.submit(dumpID, {
      recordSubmission: true,
      extraExtraKeyVals,
    }).then(null, Cu.reportError);

    this.prefs.setBoolPref("sendReport", true);
    this.prefs.setBoolPref("includeURL", includeURL);
    this.prefs.setBoolPref("emailMe", emailMe);
    if (emailMe) {
      this.prefs.setCharPref("email", email);
    } else {
      this.prefs.setCharPref("email", "");
    }

    this.childMap.set(childID, null); // Avoid resubmission.
    this.removeSubmitCheckboxesForSameCrash(childID);
  },

  removeSubmitCheckboxesForSameCrash: function(childID) {
    let enumerator = Services.wm.getEnumerator("navigator:browser");
    while (enumerator.hasMoreElements()) {
      let window = enumerator.getNext();
      if (!window.gMultiProcessBrowser)
        continue;

      for (let browser of window.gBrowser.browsers) {
        if (browser.isRemoteBrowser)
          continue;

        let doc = browser.contentDocument;
        if (!doc.documentURI.startsWith("about:tabcrashed"))
          continue;

        if (this.browserMap.get(browser.permanentKey) == childID) {
          this.browserMap.delete(browser.permanentKey);
          let ports = this.pageListener.portsForBrowser(browser);
          if (ports.length) {
            // For about:tabcrashed, we don't expect subframes. We can
            // assume sending to the first port is sufficient.
            ports[0].sendAsyncMessage("CrashReportSent");
          }
        }
      }
    }
  },

  onAboutTabCrashedLoad: function (message) {
    this._crashedTabCount++;

    // Broadcast to all about:tabcrashed pages a count of
    // how many about:tabcrashed pages exist, so that they
    // can decide whether or not to display the "Restore All
    // Crashed Tabs" button.
    this.pageListener.sendAsyncMessage("UpdateCount", {
      count: this._crashedTabCount,
    });

    let browser = message.target.browser;

    let dumpID = this.getDumpID(browser);
    if (!dumpID) {
      // Make sure to only count once even if there are multiple windows
      // that will all show about:tabcrashed.
      if (this._crashedTabCount == 1) {
        Services.telemetry.getHistogramById("FX_CONTENT_CRASH_DUMP_UNAVAILABLE").add(1);
      }

      message.target.sendAsyncMessage("SetCrashReportAvailable", {
        hasReport: false,
      });
      return;
    }

    let sendReport = this.prefs.getBoolPref("sendReport");
    let includeURL = this.prefs.getBoolPref("includeURL");
    let emailMe = this.prefs.getBoolPref("emailMe");

    let data = { hasReport: true, sendReport, includeURL, emailMe };
    if (emailMe) {
      data.email = this.prefs.getCharPref("email", "");
    }

    // Make sure to only count once even if there are multiple windows
    // that will all show about:tabcrashed.
    if (this._crashedTabCount == 1) {
      Services.telemetry.getHistogramById("FX_CONTENT_CRASH_PRESENTED").add(1);
    }

    message.target.sendAsyncMessage("SetCrashReportAvailable", data);
  },

  onAboutTabCrashedUnload(message) {
    if (!this._crashedTabCount) {
      Cu.reportError("Can not decrement crashed tab count to below 0");
      return;
    }
    this._crashedTabCount--;

    // Broadcast to all about:tabcrashed pages a count of
    // how many about:tabcrashed pages exist, so that they
    // can decide whether or not to display the "Restore All
    // Crashed Tabs" button.
    this.pageListener.sendAsyncMessage("UpdateCount", {
      count: this._crashedTabCount,
    });

    let browser = message.target.browser;
    let childID = this.browserMap.get(browser.permanentKey);

    // Make sure to only count once even if there are multiple windows
    // that will all show about:tabcrashed.
    if (this._crashedTabCount == 0 && childID) {
      Services.telemetry.getHistogramById("FX_CONTENT_CRASH_NOT_SUBMITTED").add(1);
    }
},

  /**
   * For some <xul:browser>, return a crash report dump ID for that browser
   * if we have been informed of one. Otherwise, return null.
   *
   * @param browser (<xul:browser)
   *        The browser to try to get the dump ID for
   * @returns dumpID (String)
   */
  getDumpID(browser) {
    if (!this.childMap) {
      return null;
    }

    return this.childMap.get(this.browserMap.get(browser.permanentKey));
  },
}

/**
 * This component is responsible for scanning the pending
 * crash report directory for reports, and (if enabled), to
 * prompt the user to submit those reports. It might also
 * submit those reports automatically without prompting if
 * the user has opted in.
 */
this.UnsubmittedCrashHandler = {
  get prefs() {
    delete this.prefs;
    return this.prefs =
      Services.prefs.getBranch("browser.crashReports.unsubmittedCheck.");
  },

  get enabled() {
    return this.prefs.getBoolPref("enabled");
  },

  // showingNotification is set to true once a notification
  // is successfully shown, and then set back to false if
  // the notification is dismissed by an action by the user.
  showingNotification: false,
  // suppressed is true if we've determined that we've shown
  // the notification too many times across too many days without
  // user interaction, so we're suppressing the notification for
  // some number of days. See the documentation for
  // shouldShowPendingSubmissionsNotification().
  suppressed: false,

  init() {
    if (this.initialized) {
      return;
    }

    this.initialized = true;

    // UnsubmittedCrashHandler can be initialized but still be disabled.
    // This is intentional, as this makes simulating UnsubmittedCrashHandler's
    // reactions to browser startup and shutdown easier in test automation.
    //
    // UnsubmittedCrashHandler, when initialized but not enabled, is inert.
    if (this.enabled) {
      if (this.prefs.prefHasUserValue("suppressUntilDate")) {
        if (this.prefs.getCharPref("suppressUntilDate") > this.dateString()) {
          // We'll be suppressing any notifications until after suppressedDate,
          // so there's no need to do anything more.
          this.suppressed = true;
          return;
        }

        // We're done suppressing, so we don't need this pref anymore.
        this.prefs.clearUserPref("suppressUntilDate");
      }

      Services.obs.addObserver(this, "browser-delayed-startup-finished",
                               false);
      Services.obs.addObserver(this, "profile-before-change",
                               false);
    }
  },

  uninit() {
    if (!this.initialized) {
      return;
    }

    this.initialized = false;

    if (!this.enabled) {
      return;
    }

    if (this.suppressed) {
      this.suppressed = false;
      // No need to do any more clean-up, since we were suppressed.
      return;
    }

    if (this.showingNotification) {
      this.prefs.setBoolPref("shutdownWhileShowing", true);
      this.showingNotification = false;
    }

    try {
      Services.obs.removeObserver(this, "browser-delayed-startup-finished");
    } catch (e) {
      // The browser-delayed-startup-finished observer might have already
      // fired and removed itself, so if this fails, it's okay.
      if (e.result != Cr.NS_ERROR_FAILURE) {
        throw e;
      }
    }

    Services.obs.removeObserver(this, "profile-before-change");
  },

  observe(subject, topic, data) {
    switch (topic) {
      case "browser-delayed-startup-finished": {
        Services.obs.removeObserver(this, topic);
        this.checkForUnsubmittedCrashReports();
        break;
      }
      case "profile-before-change": {
        this.uninit();
        break;
      }
    }
  },

  /**
   * Scans the profile directory for unsubmitted crash reports
   * within the past PENDING_CRASH_REPORT_DAYS days. If it
   * finds any, it will, if necessary, attempt to open a notification
   * bar to prompt the user to submit them.
   *
   * @returns Promise
   *          Resolves with the <xul:notification> after it tries to
   *          show a notification on the most recent browser window.
   *          If a notification cannot be shown, will resolve with null.
   */
  checkForUnsubmittedCrashReports: Task.async(function*() {
    let dateLimit = new Date();
    dateLimit.setDate(dateLimit.getDate() - PENDING_CRASH_REPORT_DAYS);

    let reportIDs = [];
    try {
      reportIDs = yield CrashSubmit.pendingIDsAsync(dateLimit);
    } catch (e) {
      Cu.reportError(e);
      return null;
    }

    if (reportIDs.length) {
      if (CrashNotificationBar.autoSubmit) {
        CrashNotificationBar.submitReports(reportIDs);
      } else if (this.shouldShowPendingSubmissionsNotification()) {
        return this.showPendingSubmissionsNotification(reportIDs);
      }
    }
    return null;
  }),

  /**
   * Returns true if the notification should be shown.
   * shouldShowPendingSubmissionsNotification makes this decision
   * by looking at whether or not the user has seen the notification
   * over several days without ever interacting with it. If this occurs
   * too many times, we suppress the notification for DAYS_TO_SUPPRESS
   * days.
   *
   * @returns bool
   */
  shouldShowPendingSubmissionsNotification() {
    if (!this.prefs.prefHasUserValue("shutdownWhileShowing")) {
      return true;
    }

    let shutdownWhileShowing = this.prefs.getBoolPref("shutdownWhileShowing");
    this.prefs.clearUserPref("shutdownWhileShowing");

    if (!this.prefs.prefHasUserValue("lastShownDate")) {
      // This isn't expected, but we're being defensive here. We'll
      // opt for showing the notification in this case.
      return true;
    }

    let lastShownDate = this.prefs.getCharPref("lastShownDate");
    if (this.dateString() > lastShownDate && shutdownWhileShowing) {
      // We're on a newer day then when we last showed the
      // notification without closing it. We don't want to do
      // this too many times, so we'll decrement a counter for
      // this situation. Too many of these, and we'll assume the
      // user doesn't know or care about unsubmitted notifications,
      // and we'll suppress the notification for a while.
      let chances = this.prefs.getIntPref("chancesUntilSuppress");
      if (--chances < 0) {
        // We're out of chances!
        this.prefs.clearUserPref("chancesUntilSuppress");
        // We'll suppress for DAYS_TO_SUPPRESS days.
        let suppressUntil =
          this.dateString(new Date(Date.now() + (DAY * DAYS_TO_SUPPRESS)));
        this.prefs.setCharPref("suppressUntilDate", suppressUntil);
        return false;
      }
      this.prefs.setIntPref("chancesUntilSuppress", chances);
    }

    return true;
  },

  /**
   * Given an array of unsubmitted crash report IDs, try to open
   * up a notification asking the user to submit them.
   *
   * @param reportIDs (Array<string>)
   *        The Array of report IDs to offer the user to send.
   * @returns The <xul:notification> if one is shown. null otherwise.
   */
  showPendingSubmissionsNotification(reportIDs) {
    let count = reportIDs.length;
    if (!count) {
      return null;
    }

    let messageTemplate =
      gNavigatorBundle.GetStringFromName("pendingCrashReports2.label");

    let message = PluralForm.get(count, messageTemplate).replace("#1", count);

    let notification = CrashNotificationBar.show({
      notificationID: "pending-crash-reports",
      message,
      reportIDs,
      onAction: () => {
        this.showingNotification = false;
      },
    });

    if (notification) {
      this.showingNotification = true;
      this.prefs.setCharPref("lastShownDate", this.dateString());
    }

    return notification;
  },

  /**
   * Returns a string representation of a Date in the format
   * YYYYMMDD.
   *
   * @param someDate (Date, optional)
   *        The Date to convert to the string. If not provided,
   *        defaults to today's date.
   * @returns String
   */
  dateString(someDate = new Date()) {
    return someDate.toLocaleFormat("%Y%m%d");
  },
};

this.CrashNotificationBar = {
  /**
   * Attempts to show a notification bar to the user in the most
   * recent browser window asking them to submit some crash report
   * IDs. If a notification cannot be shown (for example, there
   * is no browser window), this method exits silently.
   *
   * The notification will allow the user to submit their crash
   * reports. If the user dismissed the notification, the crash
   * reports will be marked to be ignored (though they can
   * still be manually submitted via about:crashes).
   *
   * @param JS Object
   *        An Object with the following properties:
   *
   *        notificationID (string)
   *          The ID for the notification to be opened.
   *
   *        message (string)
   *          The message to be displayed in the notification.
   *
   *        reportIDs (Array<string>)
   *          The array of report IDs to offer to the user.
   *
   *        onAction (function, optional)
   *          A callback to fire once the user performs an
   *          action on the notification bar (this includes
   *          dismissing the notification).
   *
   * @returns The <xul:notification> if one is shown. null otherwise.
   */
  show({ notificationID, message, reportIDs, onAction }) {
    let chromeWin = RecentWindow.getMostRecentBrowserWindow();
    if (!chromeWin) {
      // Can't show a notification in this case. We'll hopefully
      // get another opportunity to have the user submit their
      // crash reports later.
      return null;
    }

    let nb =  chromeWin.document.getElementById("global-notificationbox");
    let notification = nb.getNotificationWithValue(notificationID);
    if (notification) {
      return null;
    }

    let buttons = [{
      label: gNavigatorBundle.GetStringFromName("pendingCrashReports.send"),
      callback: () => {
        this.submitReports(reportIDs);
        if (onAction) {
          onAction();
        }
      },
    },
    {
      label: gNavigatorBundle.GetStringFromName("pendingCrashReports.alwaysSend"),
      callback: () => {
        this.autoSubmit = true;
        this.submitReports(reportIDs);
        if (onAction) {
          onAction();
        }
      },
    },
    {
      label: gNavigatorBundle.GetStringFromName("pendingCrashReports.viewAll"),
      callback: function() {
        chromeWin.openUILinkIn("about:crashes", "tab");
        return true;
      },
    }];

    let eventCallback = (eventType) => {
      if (eventType == "dismissed") {
        // The user intentionally dismissed the notification,
        // which we interpret as meaning that they don't care
        // to submit the reports. We'll ignore these particular
        // reports going forward.
        reportIDs.forEach(function(reportID) {
          CrashSubmit.ignore(reportID);
        });
        if (onAction) {
          onAction();
        }
      }
    };

    return nb.appendNotification(message, notificationID,
                                 "chrome://browser/skin/tab-crashed.svg",
                                 nb.PRIORITY_INFO_HIGH, buttons,
                                 eventCallback);
  },

  get autoSubmit() {
    return Services.prefs
                   .getBoolPref("browser.crashReports.unsubmittedCheck.autoSubmit");
  },

  set autoSubmit(val) {
    Services.prefs.setBoolPref("browser.crashReports.unsubmittedCheck.autoSubmit",
                               val);
  },

  /**
   * Attempt to submit reports to the crash report server. Each
   * report will have the "SubmittedFromInfobar" extra key set
   * to true.
   *
   * @param reportIDs (Array<string>)
   *        The array of reportIDs to submit.
   */
  submitReports(reportIDs) {
    for (let reportID of reportIDs) {
      CrashSubmit.submit(reportID, {
        extraExtraKeyVals: {
          "SubmittedFromInfobar": true,
        },
      });
    }
  },
};

this.PluginCrashReporter = {
  /**
   * Makes the PluginCrashReporter ready to hear about and
   * submit crash reports.
   */
  init() {
    if (this.initialized) {
      return;
    }

    this.initialized = true;
    this.crashReports = new Map();

    Services.obs.addObserver(this, "plugin-crashed", false);
    Services.obs.addObserver(this, "gmp-plugin-crash", false);
    Services.obs.addObserver(this, "profile-after-change", false);
  },

  uninit() {
    Services.obs.removeObserver(this, "plugin-crashed", false);
    Services.obs.removeObserver(this, "gmp-plugin-crash", false);
    Services.obs.removeObserver(this, "profile-after-change", false);
    this.initialized = false;
  },

  observe(subject, topic, data) {
    switch (topic) {
      case "plugin-crashed": {
        let propertyBag = subject;
        if (!(propertyBag instanceof Ci.nsIPropertyBag2) ||
            !(propertyBag instanceof Ci.nsIWritablePropertyBag2) ||
            !propertyBag.hasKey("runID") ||
            !propertyBag.hasKey("pluginDumpID")) {
          Cu.reportError("PluginCrashReporter can not read plugin information.");
          return;
        }

        let runID = propertyBag.getPropertyAsUint32("runID");
        let pluginDumpID = propertyBag.getPropertyAsAString("pluginDumpID");
        let browserDumpID = propertyBag.getPropertyAsAString("browserDumpID");
        if (pluginDumpID) {
          this.crashReports.set(runID, { pluginDumpID, browserDumpID });
        }
        break;
      }
      case "gmp-plugin-crash": {
        let propertyBag = subject;
        if (!(propertyBag instanceof Ci.nsIWritablePropertyBag2) ||
            !propertyBag.hasKey("pluginID") ||
            !propertyBag.hasKey("pluginDumpID") ||
            !propertyBag.hasKey("pluginName")) {
          Cu.reportError("PluginCrashReporter can not read plugin information.");
          return;
        }

        let pluginID = propertyBag.getPropertyAsUint32("pluginID");
        let pluginDumpID = propertyBag.getPropertyAsAString("pluginDumpID");
        if (pluginDumpID) {
          this.crashReports.set(pluginID, { pluginDumpID });
        }

        // Only the parent process gets the gmp-plugin-crash observer
        // notification, so we need to inform any content processes that
        // the GMP has crashed.
        if (Cc["@mozilla.org/parentprocessmessagemanager;1"]) {
          let pluginName = propertyBag.getPropertyAsAString("pluginName");
          let mm = Cc["@mozilla.org/parentprocessmessagemanager;1"]
            .getService(Ci.nsIMessageListenerManager);
          mm.broadcastAsyncMessage("gmp-plugin-crash",
                                   { pluginName, pluginID });
        }
        break;
      }
      case "profile-after-change":
        this.uninit();
        break;
    }
  },

  /**
   * Submit a crash report for a crashed NPAPI plugin.
   *
   * @param runID
   *        The runID of the plugin that crashed. A run ID is a unique
   *        identifier for a particular run of a plugin process - and is
   *        analogous to a process ID (though it is managed by Gecko instead
   *        of the operating system).
   * @param keyVals
   *        An object whose key-value pairs will be merged
   *        with the ".extra" file submitted with the report.
   *        The properties of htis object will override properties
   *        of the same name in the .extra file.
   */
  submitCrashReport(runID, keyVals) {
    if (!this.crashReports.has(runID)) {
      Cu.reportError(`Could not find plugin dump IDs for run ID ${runID}.` +
                     `It is possible that a report was already submitted.`);
      return;
    }

    keyVals = keyVals || {};
    let { pluginDumpID, browserDumpID } = this.crashReports.get(runID);

    let submissionPromise = CrashSubmit.submit(pluginDumpID, {
      recordSubmission: true,
      extraExtraKeyVals: keyVals,
    });

    if (browserDumpID)
      CrashSubmit.submit(browserDumpID);

    this.broadcastState(runID, "submitting");

    submissionPromise.then(() => {
      this.broadcastState(runID, "success");
    }, () => {
      this.broadcastState(runID, "failed");
    });

    this.crashReports.delete(runID);
  },

  broadcastState(runID, state) {
    let enumerator = Services.wm.getEnumerator("navigator:browser");
    while (enumerator.hasMoreElements()) {
      let window = enumerator.getNext();
      let mm = window.messageManager;
      mm.broadcastAsyncMessage("BrowserPlugins:CrashReportSubmitted",
                               { runID, state });
    }
  },

  hasCrashReport(runID) {
    return this.crashReports.has(runID);
  },
};
