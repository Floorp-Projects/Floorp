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
    switch(topic) {
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
