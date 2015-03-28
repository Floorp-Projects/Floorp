/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

this.EXPORTED_SYMBOLS = [ "TabCrashReporter", "PluginCrashReporter" ];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "CrashSubmit",
  "resource://gre/modules/CrashSubmit.jsm");

this.TabCrashReporter = {
  init: function () {
    if (this.initialized)
      return;
    this.initialized = true;

    Services.obs.addObserver(this, "ipc:content-shutdown", false);
    Services.obs.addObserver(this, "oop-frameloader-crashed", false);

    this.childMap = new Map();
    this.browserMap = new WeakMap();
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

        this.browserMap.set(browser, aSubject.childID);
        break;
    }
  },

  submitCrashReport: function (aBrowser) {
    let childID = this.browserMap.get(aBrowser);
    let dumpID = this.childMap.get(childID);
    if (!dumpID)
      return

    if (CrashSubmit.submit(dumpID, { recordSubmission: true })) {
      this.childMap.set(childID, null); // Avoid resubmission.
      this.removeSubmitCheckboxesForSameCrash(childID);
    }
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

        if (this.browserMap.get(browser) == childID) {
          this.browserMap.delete(browser);
          browser.contentDocument.documentElement.classList.remove("crashDumpAvailable");
          browser.contentDocument.documentElement.classList.add("crashDumpSubmitted");
        }
      }
    }
  },

  onAboutTabCrashedLoad: function (aBrowser) {
    if (!this.childMap)
      return;

    let dumpID = this.childMap.get(this.browserMap.get(aBrowser));
    if (!dumpID)
      return;

    aBrowser.contentDocument.documentElement.classList.add("crashDumpAvailable");
  }
}

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
  },

  observe(subject, topic, data) {
    if (topic != "plugin-crashed") {
      return;
    }

    let propertyBag = subject;
    if (!(propertyBag instanceof Ci.nsIPropertyBag2) ||
        !(propertyBag instanceof Ci.nsIWritablePropertyBag2) ||
        !propertyBag.hasKey("runID") ||
        !propertyBag.hasKey("pluginName")) {
      Cu.reportError("PluginCrashReporter can not read plugin information.");
      return;
    }

    let runID = propertyBag.getPropertyAsUint32("runID");
    let pluginDumpID = propertyBag.getPropertyAsAString("pluginDumpID");
    let browserDumpID = propertyBag.getPropertyAsAString("browserDumpID");
    if (pluginDumpID) {
      this.crashReports.set(runID, { pluginDumpID, browserDumpID });
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

  /**
   * Deprecated mechanism for sending crash reports for GMPs. This
   * should be removed when bug 1146955 is fixed.
   */
  submitGMPCrashReport(pluginDumpID, browserDumpID) {
    CrashSubmit.submit(pluginDumpID, { recordSubmission: true });
    if (browserDumpID)
      CrashSubmit.submit(browserDumpID);
  },
};
