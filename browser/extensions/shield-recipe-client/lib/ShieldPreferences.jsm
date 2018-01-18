/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(
  this, "AppConstants", "resource://gre/modules/AppConstants.jsm"
);
XPCOMUtils.defineLazyModuleGetter(
  this, "AddonStudies", "resource://shield-recipe-client/lib/AddonStudies.jsm"
);
XPCOMUtils.defineLazyModuleGetter(
  this, "CleanupManager", "resource://shield-recipe-client/lib/CleanupManager.jsm"
);

this.EXPORTED_SYMBOLS = ["ShieldPreferences"];

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID = "nsPref:changed"; // from modules/libpref/nsIPrefBranch.idl
const FHR_UPLOAD_ENABLED_PREF = "datareporting.healthreport.uploadEnabled";
const OPT_OUT_STUDIES_ENABLED_PREF = "app.shield.optoutstudies.enabled";

/**
 * Handles Shield-specific preferences, including their UI.
 */
this.ShieldPreferences = {
  init() {
    // If the FHR pref was disabled since our last run, disable opt-out as well.
    if (!Services.prefs.getBoolPref(FHR_UPLOAD_ENABLED_PREF)) {
      Services.prefs.setBoolPref(OPT_OUT_STUDIES_ENABLED_PREF, false);
    }

    // Watch for changes to the FHR pref
    Services.prefs.addObserver(FHR_UPLOAD_ENABLED_PREF, this);
    CleanupManager.addCleanupHandler(() => {
      Services.prefs.removeObserver(FHR_UPLOAD_ENABLED_PREF, this);
    });

    // Watch for changes to the Opt-out pref
    Services.prefs.addObserver(OPT_OUT_STUDIES_ENABLED_PREF, this);
    CleanupManager.addCleanupHandler(() => {
      Services.prefs.removeObserver(OPT_OUT_STUDIES_ENABLED_PREF, this);
    });

    // Disabled outside of en-* locales temporarily (bug 1377192).
    // Disabled when MOZ_DATA_REPORTING is false since the FHR UI is also hidden
    // when data reporting is false.
    if (AppConstants.MOZ_DATA_REPORTING && Services.locale.getAppLocaleAsLangTag().startsWith("en")) {
      Services.obs.addObserver(this, "privacy-pane-loaded");
      CleanupManager.addCleanupHandler(() => {
        Services.obs.removeObserver(this, "privacy-pane-loaded");
      });
    }
  },

  observe(subject, topic, data) {
    switch (topic) {
      // Add the opt-out-study checkbox to the Privacy preferences when it is shown.
      case "privacy-pane-loaded":
        this.injectOptOutStudyCheckbox(subject.document);
        break;
      case NS_PREFBRANCH_PREFCHANGE_TOPIC_ID:
        this.observePrefChange(data);
        break;
    }
  },

  async observePrefChange(prefName) {
    let prefValue;
    switch (prefName) {
      // If the FHR pref changes, set the opt-out-study pref to the value it is changing to.
      case FHR_UPLOAD_ENABLED_PREF:
        prefValue = Services.prefs.getBoolPref(FHR_UPLOAD_ENABLED_PREF);
        Services.prefs.setBoolPref(OPT_OUT_STUDIES_ENABLED_PREF, prefValue);
        break;

      // If the opt-out pref changes to be false, disable all current studies.
      case OPT_OUT_STUDIES_ENABLED_PREF:
        prefValue = Services.prefs.getBoolPref(OPT_OUT_STUDIES_ENABLED_PREF);
        if (!prefValue) {
          for (const study of await AddonStudies.getAll()) {
            if (study.active) {
              await AddonStudies.stop(study.recipeId);
            }
          }
        }
        break;
    }
  },

  /**
   * Injects the opt-out-study preference checkbox into about:preferences and
   * handles events coming from the UI for it.
   */
  injectOptOutStudyCheckbox(doc) {
    const container = doc.createElementNS(XUL_NS, "vbox");
    container.classList.add("indent");

    const hContainer = doc.createElementNS(XUL_NS, "hbox");
    hContainer.setAttribute("align", "center");
    container.appendChild(hContainer);

    const checkbox = doc.createElementNS(XUL_NS, "checkbox");
    checkbox.setAttribute("id", "optOutStudiesEnabled");
    checkbox.setAttribute("class", "tail-with-learn-more");
    checkbox.setAttribute("label", "Allow Firefox to install and run studies");
    checkbox.setAttribute("preference", OPT_OUT_STUDIES_ENABLED_PREF);
    checkbox.setAttribute("disabled", Services.prefs.prefIsLocked(FHR_UPLOAD_ENABLED_PREF) ||
      !AppConstants.MOZ_TELEMETRY_REPORTING);
    hContainer.appendChild(checkbox);

    const viewStudies = doc.createElementNS(XUL_NS, "label");
    viewStudies.setAttribute("id", "viewShieldStudies");
    viewStudies.setAttribute("href", "about:studies");
    viewStudies.setAttribute("useoriginprincipal", true);
    viewStudies.textContent = "View Firefox Studies";
    viewStudies.classList.add("learnMore", "text-link");
    hContainer.appendChild(viewStudies);

    const optOutPref = doc.createElementNS(XUL_NS, "preference");
    optOutPref.setAttribute("id", OPT_OUT_STUDIES_ENABLED_PREF);
    optOutPref.setAttribute("name", OPT_OUT_STUDIES_ENABLED_PREF);
    optOutPref.setAttribute("type", "bool");

    // Preference instances for prefs that we need to monitor while the page is open.
    doc.defaultView.Preferences.add({ id: OPT_OUT_STUDIES_ENABLED_PREF, type: "bool" });

    // Weirdly, FHR doesn't have a <preference> element on the page, so we create it.
    const fhrPref = doc.defaultView.Preferences.add({ id: FHR_UPLOAD_ENABLED_PREF, type: "bool" });
    fhrPref.on("change", function(event) {
      // Avoid reference to the document directly, to avoid leaks.
      const eventTargetCheckbox = event.target.ownerDocument.getElementById("optOutStudiesEnabled");
      eventTargetCheckbox.disabled = !Services.prefs.getBoolPref(FHR_UPLOAD_ENABLED_PREF);
    });

    // Actually inject the elements we've created.
    const parent = doc.getElementById("submitHealthReportBox").closest("description");
    parent.appendChild(container);
  },
};
