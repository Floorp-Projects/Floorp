/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

this.EXPORTED_SYMBOLS = [ "TabCrashReporter", "PluginCrashReporter" ];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "CrashSubmit",
  "resource://gre/modules/CrashSubmit.jsm");

this.TabCrashReporter = {
  get prefs() {
    delete this.prefs;
    return this.prefs = Services.prefs.getBranch("browser.tabs.crashReporting.");
  },

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

        this.browserMap.set(browser.permanentKey, aSubject.childID);
        break;
    }
  },

  /**
   * Submits a crash report from about:tabcrashed
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
  submitCrashReport: function (aBrowser, aFormData) {
    let childID = this.browserMap.get(aBrowser.permanentKey);
    let dumpID = this.childMap.get(childID);
    if (!dumpID)
      return

    CrashSubmit.submit(dumpID, {
      recordSubmission: true,
      extraExtraKeyVals: {
        Comments: aFormData.comments,
        Email: aFormData.email,
        URL: aFormData.URL,
      },
    }).then(null, Cu.reportError);

    this.prefs.setBoolPref("sendReport", true);
    this.prefs.setBoolPref("includeURL", aFormData.includeURL);
    this.prefs.setBoolPref("emailMe", aFormData.emailMe);
    if (aFormData.emailMe) {
      this.prefs.setCharPref("email", aFormData.email);
    } else {
      this.prefs.setCharPref("email", "");
    }

    this.childMap.set(childID, null); // Avoid resubmission.
    this.removeSubmitCheckboxesForSameCrash(childID);
  },

  dontSubmitCrashReport: function() {
    this.prefs.setBoolPref("sendReport", false);
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
          browser.contentDocument.documentElement.classList.remove("crashDumpAvailable");
          browser.contentDocument.documentElement.classList.add("crashDumpSubmitted");
        }
      }
    }
  },

  onAboutTabCrashedLoad: function (aBrowser, aParams) {
    // If there was only one tab open that crashed, do not show the "restore all tabs" button
    if (aParams.crashedTabCount == 1) {
      this.hideRestoreAllButton(aBrowser);
    }

    if (!this.childMap)
      return;

    let dumpID = this.childMap.get(this.browserMap.get(aBrowser.permanentKey));
    if (!dumpID)
      return;

    let doc = aBrowser.contentDocument;

    doc.documentElement.classList.add("crashDumpAvailable");

    let sendReport = this.prefs.getBoolPref("sendReport");
    doc.getElementById("sendReport").checked = sendReport;

    let includeURL = this.prefs.getBoolPref("includeURL");
    doc.getElementById("includeURL").checked = includeURL;

    let emailMe = this.prefs.getBoolPref("emailMe");
    doc.getElementById("emailMe").checked = emailMe;

    if (emailMe) {
      let email = this.prefs.getCharPref("email", "");
      doc.getElementById("email").value = email;
    }
  },

  hideRestoreAllButton: function (aBrowser) {
    aBrowser.contentDocument.getElementById("restoreAll").setAttribute("hidden", true);
    aBrowser.contentDocument.getElementById("restoreTab").setAttribute("class", "primary");
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
