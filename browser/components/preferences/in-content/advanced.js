/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from preferences.js */

// Load DownloadUtils module for convertByteUnits
Components.utils.import("resource://gre/modules/DownloadUtils.jsm");
Components.utils.import("resource://gre/modules/LoadContextInfo.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const PREF_UPLOAD_ENABLED = "datareporting.healthreport.uploadEnabled";

var gAdvancedPane = {
  _inited: false,

  /**
   * Brings the appropriate tab to the front and initializes various bits of UI.
   */
  init() {
    function setEventListener(aId, aEventType, aCallback) {
      document.getElementById(aId)
              .addEventListener(aEventType, aCallback.bind(gAdvancedPane));
    }

    this._inited = true;

    if (AppConstants.MOZ_UPDATER) {
      let onUnload = function() {
        window.removeEventListener("unload", onUnload);
        Services.prefs.removeObserver("app.update.", this);
      }.bind(this);
      window.addEventListener("unload", onUnload);
      Services.prefs.addObserver("app.update.", this, false);
      this.updateReadPrefs();
    }
    if (AppConstants.MOZ_CRASHREPORTER) {
      this.initSubmitCrashes();
    }
    this.initTelemetry();
    if (AppConstants.MOZ_TELEMETRY_REPORTING) {
      this.initSubmitHealthReport();
    }
    this.updateOnScreenKeyboardVisibility();

    setEventListener("layers.acceleration.disabled", "change",
                     gAdvancedPane.updateHardwareAcceleration);
    if (AppConstants.MOZ_TELEMETRY_REPORTING) {
      setEventListener("submitHealthReportBox", "command",
                       gAdvancedPane.updateSubmitHealthReport);
    }

    if (AppConstants.MOZ_UPDATER) {
      setEventListener("updateRadioGroup", "command",
                       gAdvancedPane.updateWritePrefs);
      setEventListener("showUpdateHistory", "command",
                       gAdvancedPane.showUpdates);
    }
  },


  // GENERAL TAB

  /*
   * Preferences:
   *
   * accessibility.browsewithcaret
   * - true enables keyboard navigation and selection within web pages using a
   *   visible caret, false uses normal keyboard navigation with no caret
   * accessibility.typeaheadfind
   * - when set to true, typing outside text areas and input boxes will
   *   automatically start searching for what's typed within the current
   *   document; when set to false, no search action happens
   * ui.osk.enabled
   * - when set to true, subject to other conditions, we may sometimes invoke
   *   an on-screen keyboard when a text input is focused.
   *   (Currently Windows-only, and depending on prefs, may be Windows-8-only)
   * general.autoScroll
   * - when set to true, clicking the scroll wheel on the mouse activates a
   *   mouse mode where moving the mouse down scrolls the document downward with
   *   speed correlated with the distance of the cursor from the original
   *   position at which the click occurred (and likewise with movement upward);
   *   if false, this behavior is disabled
   * general.smoothScroll
   * - set to true to enable finer page scrolling than line-by-line on page-up,
   *   page-down, and other such page movements
   * layout.spellcheckDefault
   * - an integer:
   *     0  disables spellchecking
   *     1  enables spellchecking, but only for multiline text fields
   *     2  enables spellchecking for all text fields
   */

  /**
   * Stores the original value of the spellchecking preference to enable proper
   * restoration if unchanged (since we're mapping a tristate onto a checkbox).
   */
  _storedSpellCheck: 0,

  /**
   * Returns true if any spellchecking is enabled and false otherwise, caching
   * the current value to enable proper pref restoration if the checkbox is
   * never changed.
   */
  readCheckSpelling() {
    var pref = document.getElementById("layout.spellcheckDefault");
    this._storedSpellCheck = pref.value;

    return (pref.value != 0);
  },

  /**
   * Returns the value of the spellchecking preference represented by UI,
   * preserving the preference's "hidden" value if the preference is
   * unchanged and represents a value not strictly allowed in UI.
   */
  writeCheckSpelling() {
    var checkbox = document.getElementById("checkSpelling");
    if (checkbox.checked) {
      if (this._storedSpellCheck == 2) {
        return 2;
      }
      return 1;
    }
    return 0;
  },


  /**
   * When the user toggles the layers.acceleration.disabled pref,
   * sync its new value to the gfx.direct2d.disabled pref too.
   */
  updateHardwareAcceleration() {
    if (AppConstants.platform == "win") {
      var fromPref = document.getElementById("layers.acceleration.disabled");
      var toPref = document.getElementById("gfx.direct2d.disabled");
      toPref.value = fromPref.value;
    }
  },

  // DATA CHOICES TAB

  /**
   * Set up or hide the Learn More links for various data collection options
   */
  _setupLearnMoreLink(pref, element) {
    // set up the Learn More link with the correct URL
    let url = Services.prefs.getCharPref(pref);
    let el = document.getElementById(element);

    if (url) {
      el.setAttribute("href", url);
    } else {
      el.setAttribute("hidden", "true");
    }
  },

  /**
   *
   */
  initSubmitCrashes() {
    this._setupLearnMoreLink("toolkit.crashreporter.infoURL",
                             "crashReporterLearnMore");
  },

  /**
   * The preference/checkbox is configured in XUL.
   *
   * In all cases, set up the Learn More link sanely.
   */
  initTelemetry() {
    if (AppConstants.MOZ_TELEMETRY_REPORTING) {
      this._setupLearnMoreLink("toolkit.telemetry.infoURL", "telemetryLearnMore");
    }
  },

  /**
   * Set the status of the telemetry controls based on the input argument.
   * @param {Boolean} aEnabled False disables the controls, true enables them.
   */
  setTelemetrySectionEnabled(aEnabled) {
    if (AppConstants.MOZ_TELEMETRY_REPORTING) {
      // If FHR is disabled, additional data sharing should be disabled as well.
      let disabled = !aEnabled;
      document.getElementById("submitTelemetryBox").disabled = disabled;
      if (disabled) {
        // If we disable FHR, untick the telemetry checkbox.
        Services.prefs.setBoolPref("toolkit.telemetry.enabled", false);
      }
      document.getElementById("telemetryDataDesc").disabled = disabled;
    }
  },

  /**
   * Initialize the health report service reference and checkbox.
   */
  initSubmitHealthReport() {
    if (AppConstants.MOZ_TELEMETRY_REPORTING) {
      this._setupLearnMoreLink("datareporting.healthreport.infoURL", "FHRLearnMore");

      let checkbox = document.getElementById("submitHealthReportBox");

      if (Services.prefs.prefIsLocked(PREF_UPLOAD_ENABLED)) {
        checkbox.setAttribute("disabled", "true");
        return;
      }

      checkbox.checked = Services.prefs.getBoolPref(PREF_UPLOAD_ENABLED);
      this.setTelemetrySectionEnabled(checkbox.checked);
    }
  },

  /**
   * Update the health report preference with state from checkbox.
   */
  updateSubmitHealthReport() {
    if (AppConstants.MOZ_TELEMETRY_REPORTING) {
      let checkbox = document.getElementById("submitHealthReportBox");
      Services.prefs.setBoolPref(PREF_UPLOAD_ENABLED, checkbox.checked);
      this.setTelemetrySectionEnabled(checkbox.checked);
    }
  },

  updateOnScreenKeyboardVisibility() {
    if (AppConstants.platform == "win") {
      let minVersion = Services.prefs.getBoolPref("ui.osk.require_win10") ? 10 : 6.2;
      if (Services.vc.compare(Services.sysinfo.getProperty("version"), minVersion) >= 0) {
        document.getElementById("useOnScreenKeyboard").hidden = false;
      }
    }
  },

  /*
   * Preferences:
   *
   * app.update.enabled
   * - true if updates to the application are enabled, false otherwise
   * app.update.auto
   * - true if updates should be automatically downloaded and installed and
   * false if the user should be asked what he wants to do when an update is
   * available
   * extensions.update.enabled
   * - true if updates to extensions and themes are enabled, false otherwise
   * browser.search.update
   * - true if updates to search engines are enabled, false otherwise
   */

  /**
   * Selects the item of the radiogroup based on the pref values and locked
   * states.
   *
   * UI state matrix for update preference conditions
   *
   * UI Components:                              Preferences
   * Radiogroup                                  i   = app.update.enabled
   *                                             ii  = app.update.auto
   *
   * Disabled states:
   * Element           pref  value  locked  disabled
   * radiogroup        i     t/f    f       false
   *                   i     t/f    *t*     *true*
   *                   ii    t/f    f       false
   *                   ii    t/f    *t*     *true*
   */
  updateReadPrefs() {
    if (AppConstants.MOZ_UPDATER) {
      var enabledPref = document.getElementById("app.update.enabled");
      var autoPref = document.getElementById("app.update.auto");
      var radiogroup = document.getElementById("updateRadioGroup");

      if (!enabledPref.value)   // Don't care for autoPref.value in this case.
        radiogroup.value = "manual";    // 3. Never check for updates.
      else if (autoPref.value)  // enabledPref.value && autoPref.value
        radiogroup.value = "auto";      // 1. Automatically install updates
      else                      // enabledPref.value && !autoPref.value
        radiogroup.value = "checkOnly"; // 2. Check, but let me choose

      var canCheck = Components.classes["@mozilla.org/updates/update-service;1"].
                       getService(Components.interfaces.nsIApplicationUpdateService).
                       canCheckForUpdates;
      // canCheck is false if the enabledPref is false and locked,
      // or the binary platform or OS version is not known.
      // A locked pref is sufficient to disable the radiogroup.
      radiogroup.disabled = !canCheck || enabledPref.locked || autoPref.locked;

      if (AppConstants.MOZ_MAINTENANCE_SERVICE) {
        // Check to see if the maintenance service is installed.
        // If it is don't show the preference at all.
        var installed;
        try {
          var wrk = Components.classes["@mozilla.org/windows-registry-key;1"]
                    .createInstance(Components.interfaces.nsIWindowsRegKey);
          wrk.open(wrk.ROOT_KEY_LOCAL_MACHINE,
                   "SOFTWARE\\Mozilla\\MaintenanceService",
                   wrk.ACCESS_READ | wrk.WOW64_64);
          installed = wrk.readIntValue("Installed");
          wrk.close();
        } catch (e) {
        }
        if (installed != 1) {
          document.getElementById("useService").hidden = true;
        }
      }
    }
  },

  /**
   * Sets the pref values based on the selected item of the radiogroup.
   */
  updateWritePrefs() {
    if (AppConstants.MOZ_UPDATER) {
      var enabledPref = document.getElementById("app.update.enabled");
      var autoPref = document.getElementById("app.update.auto");
      var radiogroup = document.getElementById("updateRadioGroup");
      switch (radiogroup.value) {
        case "auto":      // 1. Automatically install updates for Desktop only
          enabledPref.value = true;
          autoPref.value = true;
          break;
        case "checkOnly": // 2. Check, but let me choose
          enabledPref.value = true;
          autoPref.value = false;
          break;
        case "manual":    // 3. Never check for updates.
          enabledPref.value = false;
          autoPref.value = false;
      }
    }
  },

  /**
   * Displays the history of installed updates.
   */
  showUpdates() {
    gSubDialog.open("chrome://mozapps/content/update/history.xul");
  },

  // ENCRYPTION TAB

  /*
   * Preferences:
   *
   * security.default_personal_cert
   * - a string:
   *     "Select Automatically"   select a certificate automatically when a site
   *                              requests one
   *     "Ask Every Time"         present a dialog to the user so he can select
   *                              the certificate to use on a site which
   *                              requests one
   */


  observe(aSubject, aTopic, aData) {
    if (AppConstants.MOZ_UPDATER) {
      switch (aTopic) {
        case "nsPref:changed":
          this.updateReadPrefs();
          break;
      }
    }
  },
};
