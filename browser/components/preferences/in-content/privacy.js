/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from extensionControlled.js */
/* import-globals-from preferences.js */

var { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "DownloadUtils",
  "resource://gre/modules/DownloadUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "LoginHelper",
  "resource://gre/modules/LoginHelper.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "SiteDataManager",
  "resource:///modules/SiteDataManager.jsm"
);

var { PrivateBrowsingUtils } = ChromeUtils.import(
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

const PREF_UPLOAD_ENABLED = "datareporting.healthreport.uploadEnabled";

const TRACKING_PROTECTION_KEY = "websites.trackingProtectionMode";
const TRACKING_PROTECTION_PREFS = [
  "privacy.trackingprotection.enabled",
  "privacy.trackingprotection.pbmode.enabled",
];
const CONTENT_BLOCKING_PREFS = [
  "privacy.trackingprotection.enabled",
  "privacy.trackingprotection.pbmode.enabled",
  "network.cookie.cookieBehavior",
  "privacy.trackingprotection.fingerprinting.enabled",
  "privacy.trackingprotection.cryptomining.enabled",
];

const PREF_OPT_OUT_STUDIES_ENABLED = "app.shield.optoutstudies.enabled";
const PREF_NORMANDY_ENABLED = "app.normandy.enabled";

const PREF_ADDON_RECOMMENDATIONS_ENABLED = "browser.discovery.enabled";

const PREF_PASSWORD_GENERATION_AVAILABLE = "signon.generation.available";

XPCOMUtils.defineLazyGetter(this, "AlertsServiceDND", function() {
  try {
    let alertsService = Cc["@mozilla.org/alerts-service;1"]
      .getService(Ci.nsIAlertsService)
      .QueryInterface(Ci.nsIAlertsDoNotDisturb);
    // This will throw if manualDoNotDisturb isn't implemented.
    alertsService.manualDoNotDisturb;
    return alertsService;
  } catch (ex) {
    return undefined;
  }
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "listManager",
  "@mozilla.org/url-classifier/listmanager;1",
  "nsIUrlListManager"
);

Preferences.addAll([
  // Content blocking / Tracking Protection
  { id: "privacy.trackingprotection.enabled", type: "bool" },
  { id: "privacy.trackingprotection.pbmode.enabled", type: "bool" },
  { id: "privacy.trackingprotection.fingerprinting.enabled", type: "bool" },
  { id: "privacy.trackingprotection.cryptomining.enabled", type: "bool" },

  // Social tracking
  { id: "privacy.trackingprotection.socialtracking.enabled", type: "bool" },
  { id: "privacy.socialtracking.block_cookies.enabled", type: "bool" },

  // Tracker list
  { id: "urlclassifier.trackingTable", type: "string" },

  // Button prefs
  { id: "pref.privacy.disable_button.cookie_exceptions", type: "bool" },
  { id: "pref.privacy.disable_button.view_cookies", type: "bool" },
  { id: "pref.privacy.disable_button.change_blocklist", type: "bool" },
  {
    id: "pref.privacy.disable_button.tracking_protection_exceptions",
    type: "bool",
  },

  // Location Bar
  { id: "browser.urlbar.suggest.bookmark", type: "bool" },
  { id: "browser.urlbar.suggest.history", type: "bool" },
  { id: "browser.urlbar.suggest.openpage", type: "bool" },

  // History
  { id: "places.history.enabled", type: "bool" },
  { id: "browser.formfill.enable", type: "bool" },
  { id: "privacy.history.custom", type: "bool" },
  // Cookies
  { id: "network.cookie.cookieBehavior", type: "int" },
  { id: "network.cookie.lifetimePolicy", type: "int" },
  { id: "network.cookie.blockFutureCookies", type: "bool" },
  // Content blocking category
  { id: "browser.contentblocking.category", type: "string" },
  { id: "browser.contentblocking.features.strict", type: "string" },

  // Clear Private Data
  { id: "privacy.sanitize.sanitizeOnShutdown", type: "bool" },
  { id: "privacy.sanitize.timeSpan", type: "int" },
  // Do not track
  { id: "privacy.donottrackheader.enabled", type: "bool" },

  // Media
  { id: "media.autoplay.default", type: "int" },

  // Popups
  { id: "dom.disable_open_during_load", type: "bool" },
  // Passwords
  { id: "signon.rememberSignons", type: "bool" },
  { id: "signon.generation.enabled", type: "bool" },
  { id: "signon.autofillForms", type: "bool" },

  // Buttons
  { id: "pref.privacy.disable_button.view_passwords", type: "bool" },
  { id: "pref.privacy.disable_button.view_passwords_exceptions", type: "bool" },

  /* Certificates tab
   * security.default_personal_cert
   *   - a string:
   *       "Select Automatically"   select a certificate automatically when a site
   *                                requests one
   *       "Ask Every Time"         present a dialog to the user so he can select
   *                                the certificate to use on a site which
   *                                requests one
   */
  { id: "security.default_personal_cert", type: "string" },

  { id: "security.disable_button.openCertManager", type: "bool" },

  { id: "security.disable_button.openDeviceManager", type: "bool" },

  { id: "security.OCSP.enabled", type: "int" },

  // Add-ons, malware, phishing
  { id: "xpinstall.whitelist.required", type: "bool" },

  { id: "browser.safebrowsing.malware.enabled", type: "bool" },
  { id: "browser.safebrowsing.phishing.enabled", type: "bool" },

  { id: "browser.safebrowsing.downloads.enabled", type: "bool" },

  { id: "urlclassifier.malwareTable", type: "string" },

  {
    id: "browser.safebrowsing.downloads.remote.block_potentially_unwanted",
    type: "bool",
  },
  { id: "browser.safebrowsing.downloads.remote.block_uncommon", type: "bool" },
]);

// Study opt out
if (AppConstants.MOZ_DATA_REPORTING) {
  Preferences.addAll([
    // Preference instances for prefs that we need to monitor while the page is open.
    { id: PREF_OPT_OUT_STUDIES_ENABLED, type: "bool" },
    { id: PREF_ADDON_RECOMMENDATIONS_ENABLED, type: "bool" },
    { id: PREF_UPLOAD_ENABLED, type: "bool" },
  ]);
}

// Data Choices tab
if (AppConstants.MOZ_CRASHREPORTER) {
  Preferences.add({
    id: "browser.crashReports.unsubmittedCheck.autoSubmit2",
    type: "bool",
  });
}

function setEventListener(aId, aEventType, aCallback) {
  document
    .getElementById(aId)
    .addEventListener(aEventType, aCallback.bind(gPrivacyPane));
}

function setSyncFromPrefListener(aId, aCallback) {
  Preferences.addSyncFromPrefListener(document.getElementById(aId), aCallback);
}

function setSyncToPrefListener(aId, aCallback) {
  Preferences.addSyncToPrefListener(document.getElementById(aId), aCallback);
}

function dataCollectionCheckboxHandler({
  checkbox,
  pref,
  matchPref = () => true,
  isDisabled = () => false,
}) {
  function updateCheckbox() {
    let collectionEnabled = Services.prefs.getBoolPref(
      PREF_UPLOAD_ENABLED,
      false
    );

    if (collectionEnabled && matchPref()) {
      if (Services.prefs.getBoolPref(pref, false)) {
        checkbox.setAttribute("checked", "true");
      } else {
        checkbox.removeAttribute("checked");
      }
      checkbox.setAttribute("preference", pref);
    } else {
      checkbox.removeAttribute("preference");
      checkbox.removeAttribute("checked");
    }

    // We can't use checkbox.disabled here because the XBL binding may not be present,
    // in which case setting the property won't work properly.
    if (
      !collectionEnabled ||
      Services.prefs.prefIsLocked(pref) ||
      isDisabled()
    ) {
      checkbox.setAttribute("disabled", "true");
    } else {
      checkbox.removeAttribute("disabled");
    }
  }

  Preferences.get(PREF_UPLOAD_ENABLED).on("change", updateCheckbox);
  updateCheckbox();
}

// Sets the "Learn how" SUMO link in the Strict/Custom options of Content Blocking.
function addCustomBlockingLearnMore() {
  let links = document.querySelectorAll(".contentBlockWarningLink");
  let contentBlockingWarningUrl =
    Services.urlFormatter.formatURLPref("app.support.baseURL") +
    "turn-off-etp-desktop";
  for (let link of links) {
    link.setAttribute("href", contentBlockingWarningUrl);
  }
}

var gPrivacyPane = {
  _pane: null,

  /**
   * Whether the prompt to restart Firefox should appear when changing the autostart pref.
   */
  _shouldPromptForRestart: true,

  /**
   * Update the tracking protection UI to deal with extension control.
   */
  _updateTrackingProtectionUI() {
    let cBPrefisLocked = CONTENT_BLOCKING_PREFS.some(pref =>
      Services.prefs.prefIsLocked(pref)
    );
    let tPPrefisLocked = TRACKING_PROTECTION_PREFS.some(pref =>
      Services.prefs.prefIsLocked(pref)
    );

    function setInputsDisabledState(isControlled) {
      let tpDisabled = tPPrefisLocked || isControlled;
      let disabled = cBPrefisLocked || isControlled;
      let tpCheckbox = document.getElementById(
        "contentBlockingTrackingProtectionCheckbox"
      );
      // Only enable the TP menu if Detect All Trackers is enabled.
      document.getElementById("trackingProtectionMenu").disabled =
        tpDisabled || !tpCheckbox.checked;
      tpCheckbox.disabled = tpDisabled;

      document.getElementById("standardRadio").disabled = disabled;
      document.getElementById("strictRadio").disabled = disabled;
      document
        .getElementById("contentBlockingOptionStrict")
        .classList.toggle("disabled", disabled);
      document
        .getElementById("contentBlockingOptionStandard")
        .classList.toggle("disabled", disabled);
      let arrowButtons = document.querySelectorAll("button.arrowhead");
      for (let button of arrowButtons) {
        button.disabled = disabled;
      }

      // Notify observers that the TP UI has been updated.
      // This is needed since our tests need to be notified about the
      // trackingProtectionMenu element getting disabled/enabled at the right time.
      Services.obs.notifyObservers(window, "privacy-pane-tp-ui-updated");
    }

    let policy = Services.policies.getActivePolicies();
    if (
      policy &&
      ((policy.EnableTrackingProtection &&
        policy.EnableTrackingProtection.Locked) ||
        (policy.Cookies && policy.Cookies.Locked))
    ) {
      setInputsDisabledState(true);
    }
    if (tPPrefisLocked) {
      // An extension can't control this setting if either pref is locked.
      hideControllingExtension(TRACKING_PROTECTION_KEY);
      setInputsDisabledState(false);
    } else {
      handleControllingExtension(
        PREF_SETTING_TYPE,
        TRACKING_PROTECTION_KEY
      ).then(setInputsDisabledState);
    }
  },

  /**
   * Hide the "Change Block List" link for trackers/tracking content in the
   * custom Content Blocking/ETP panel. By default, it will not be visible.
   */
  _showCustomBlockList() {
    let prefValue = Services.prefs.getBoolPref(
      "browser.contentblocking.customBlockList.preferences.ui.enabled"
    );
    if (!prefValue) {
      document.getElementById("changeBlockListLink").style.display = "none";
    } else {
      setEventListener("changeBlockListLink", "click", this.showBlockLists);
    }
  },

  /**
   * Set up handlers for showing and hiding controlling extension info
   * for tracking protection.
   */
  _initTrackingProtectionExtensionControl() {
    setEventListener(
      "contentBlockingDisableTrackingProtectionExtension",
      "command",
      makeDisableControllingExtension(
        PREF_SETTING_TYPE,
        TRACKING_PROTECTION_KEY
      )
    );

    let trackingProtectionObserver = {
      observe(subject, topic, data) {
        gPrivacyPane._updateTrackingProtectionUI();
      },
    };

    for (let pref of TRACKING_PROTECTION_PREFS) {
      Services.prefs.addObserver(pref, trackingProtectionObserver);
    }
    window.addEventListener("unload", () => {
      for (let pref of TRACKING_PROTECTION_PREFS) {
        Services.prefs.removeObserver(pref, trackingProtectionObserver);
      }
    });
  },

  /**
   * Initialize autocomplete to ensure prefs are in sync.
   */
  _initAutocomplete() {
    Cc["@mozilla.org/autocomplete/search;1?name=unifiedcomplete"].getService(
      Ci.mozIPlacesAutoComplete
    );
  },

  /**
   * Sets up the UI for the number of days of history to keep, and updates the
   * label of the "Clear Now..." button.
   */
  init() {
    this._updateSanitizeSettingsButton();
    this.initializeHistoryMode();
    this.updateHistoryModePane();
    this.updatePrivacyMicroControls();
    this.initAutoStartPrivateBrowsingReverter();
    this._initAutocomplete();

    /* Initialize Content Blocking */
    this.initContentBlocking();

    this._showCustomBlockList();
    this.trackingProtectionReadPrefs();
    this.networkCookieBehaviorReadPrefs();
    this._initTrackingProtectionExtensionControl();

    Services.telemetry.setEventRecordingEnabled("pwmgr", true);

    Preferences.get("privacy.trackingprotection.enabled").on(
      "change",
      gPrivacyPane.trackingProtectionReadPrefs.bind(gPrivacyPane)
    );
    Preferences.get("privacy.trackingprotection.pbmode.enabled").on(
      "change",
      gPrivacyPane.trackingProtectionReadPrefs.bind(gPrivacyPane)
    );

    // Watch all of the prefs that the new Cookies & Site Data UI depends on
    Preferences.get("network.cookie.cookieBehavior").on(
      "change",
      gPrivacyPane.networkCookieBehaviorReadPrefs.bind(gPrivacyPane)
    );
    Preferences.get("network.cookie.lifetimePolicy").on(
      "change",
      gPrivacyPane.networkCookieBehaviorReadPrefs.bind(gPrivacyPane)
    );
    Preferences.get("browser.privatebrowsing.autostart").on(
      "change",
      gPrivacyPane.networkCookieBehaviorReadPrefs.bind(gPrivacyPane)
    );

    setEventListener(
      "trackingProtectionExceptions",
      "command",
      gPrivacyPane.showTrackingProtectionExceptions
    );

    Preferences.get("privacy.sanitize.sanitizeOnShutdown").on(
      "change",
      gPrivacyPane._updateSanitizeSettingsButton.bind(gPrivacyPane)
    );
    Preferences.get("browser.privatebrowsing.autostart").on(
      "change",
      gPrivacyPane.updatePrivacyMicroControls.bind(gPrivacyPane)
    );
    setEventListener("historyMode", "command", function() {
      gPrivacyPane.updateHistoryModePane();
      gPrivacyPane.updateHistoryModePrefs();
      gPrivacyPane.updatePrivacyMicroControls();
      gPrivacyPane.updateAutostart();
    });
    setEventListener("clearHistoryButton", "command", function() {
      let historyMode = document.getElementById("historyMode");
      // Select "everything" in the clear history dialog if the
      // user has set their history mode to never remember history.
      gPrivacyPane.clearPrivateDataNow(historyMode.value == "dontremember");
    });
    setEventListener("openSearchEnginePreferences", "click", function(event) {
      if (event.button == 0) {
        gotoPref("search");
      }
      return false;
    });
    setEventListener(
      "privateBrowsingAutoStart",
      "command",
      gPrivacyPane.updateAutostart
    );
    setEventListener(
      "cookieExceptions",
      "command",
      gPrivacyPane.showCookieExceptions
    );
    setEventListener(
      "clearDataSettings",
      "command",
      gPrivacyPane.showClearPrivateDataSettings
    );
    setEventListener(
      "passwordExceptions",
      "command",
      gPrivacyPane.showPasswordExceptions
    );
    setEventListener(
      "useMasterPassword",
      "command",
      gPrivacyPane.updateMasterPasswordButton
    );
    setEventListener(
      "changeMasterPassword",
      "command",
      gPrivacyPane.changeMasterPassword
    );
    setEventListener("showPasswords", "command", gPrivacyPane.showPasswords);
    setEventListener(
      "addonExceptions",
      "command",
      gPrivacyPane.showAddonExceptions
    );
    setEventListener(
      "viewCertificatesButton",
      "command",
      gPrivacyPane.showCertificates
    );
    setEventListener(
      "viewSecurityDevicesButton",
      "command",
      gPrivacyPane.showSecurityDevices
    );

    this._pane = document.getElementById("panePrivacy");

    this._initPasswordGenerationUI();
    this._initMasterPasswordUI();

    this._initSafeBrowsing();

    setEventListener(
      "autoplaySettingsButton",
      "command",
      gPrivacyPane.showAutoplayMediaExceptions
    );
    setEventListener(
      "notificationSettingsButton",
      "command",
      gPrivacyPane.showNotificationExceptions
    );
    setEventListener(
      "locationSettingsButton",
      "command",
      gPrivacyPane.showLocationExceptions
    );
    setEventListener(
      "cameraSettingsButton",
      "command",
      gPrivacyPane.showCameraExceptions
    );
    setEventListener(
      "microphoneSettingsButton",
      "command",
      gPrivacyPane.showMicrophoneExceptions
    );
    setEventListener(
      "popupPolicyButton",
      "command",
      gPrivacyPane.showPopupExceptions
    );
    setEventListener(
      "notificationsDoNotDisturb",
      "command",
      gPrivacyPane.toggleDoNotDisturbNotifications
    );

    setSyncFromPrefListener("contentBlockingBlockCookiesCheckbox", () =>
      this.readBlockCookies()
    );
    setSyncToPrefListener("contentBlockingBlockCookiesCheckbox", () =>
      this.writeBlockCookies()
    );
    setSyncFromPrefListener("blockCookiesMenu", () =>
      this.readBlockCookiesFrom()
    );
    setSyncToPrefListener("blockCookiesMenu", () =>
      this.writeBlockCookiesFrom()
    );
    setSyncFromPrefListener("deleteOnClose", () => this.readDeleteOnClose());
    setSyncToPrefListener("deleteOnClose", () => this.writeDeleteOnClose());
    setSyncFromPrefListener("savePasswords", () => this.readSavePasswords());

    let microControlHandler = el =>
      this.ensurePrivacyMicroControlUncheckedWhenDisabled(el);
    setSyncFromPrefListener("rememberHistory", microControlHandler);
    setSyncFromPrefListener("rememberForms", microControlHandler);
    setSyncFromPrefListener("alwaysClear", microControlHandler);

    setSyncFromPrefListener("popupPolicy", () =>
      this.updateButtons("popupPolicyButton", "dom.disable_open_during_load")
    );
    setSyncFromPrefListener("warnAddonInstall", () =>
      this.readWarnAddonInstall()
    );
    setSyncFromPrefListener("enableOCSP", () => this.readEnableOCSP());
    setSyncToPrefListener("enableOCSP", () => this.writeEnableOCSP());

    if (AlertsServiceDND) {
      let notificationsDoNotDisturbBox = document.getElementById(
        "notificationsDoNotDisturbBox"
      );
      notificationsDoNotDisturbBox.removeAttribute("hidden");
      let checkbox = document.getElementById("notificationsDoNotDisturb");
      document.l10n.setAttributes(checkbox, "permissions-notification-pause");
      if (AlertsServiceDND.manualDoNotDisturb) {
        let notificationsDoNotDisturb = document.getElementById(
          "notificationsDoNotDisturb"
        );
        notificationsDoNotDisturb.setAttribute("checked", true);
      }
    }

    this.initSiteDataControls();
    setEventListener(
      "clearSiteDataButton",
      "command",
      gPrivacyPane.clearSiteData
    );
    setEventListener(
      "siteDataSettings",
      "command",
      gPrivacyPane.showSiteDataSettings
    );
    let url =
      Services.urlFormatter.formatURLPref("app.support.baseURL") +
      "storage-permissions";
    document.getElementById("siteDataLearnMoreLink").setAttribute("href", url);

    let notificationInfoURL =
      Services.urlFormatter.formatURLPref("app.support.baseURL") + "push";
    document
      .getElementById("notificationPermissionsLearnMore")
      .setAttribute("href", notificationInfoURL);

    if (AppConstants.MOZ_DATA_REPORTING) {
      this.initDataCollection();
      if (AppConstants.MOZ_CRASHREPORTER) {
        this.initSubmitCrashes();
      }
      this.initSubmitHealthReport();
      setEventListener(
        "submitHealthReportBox",
        "command",
        gPrivacyPane.updateSubmitHealthReport
      );
      if (AppConstants.MOZ_NORMANDY) {
        this.initOptOutStudyCheckbox();
      }
      this.initAddonRecommendationsCheckbox();
    }
    this._initA11yState();
    let signonBundle = document.getElementById("signonBundle");
    let pkiBundle = document.getElementById("pkiBundle");
    appendSearchKeywords("showPasswords", [
      signonBundle.getString("loginsDescriptionAll2"),
    ]);
    appendSearchKeywords("viewSecurityDevicesButton", [
      pkiBundle.getString("enable_fips"),
    ]);

    if (!PrivateBrowsingUtils.enabled) {
      document.getElementById("privateBrowsingAutoStart").hidden = true;
      document.querySelector("menuitem[value='dontremember']").hidden = true;
    }

    // Notify observers that the UI is now ready
    Services.obs.notifyObservers(window, "privacy-pane-loaded");
  },

  initSiteDataControls() {
    Services.obs.addObserver(this, "sitedatamanager:sites-updated");
    Services.obs.addObserver(this, "sitedatamanager:updating-sites");
    let unload = () => {
      window.removeEventListener("unload", unload);
      Services.obs.removeObserver(this, "sitedatamanager:sites-updated");
      Services.obs.removeObserver(this, "sitedatamanager:updating-sites");
    };
    window.addEventListener("unload", unload);
    SiteDataManager.updateSites();
  },

  // CONTENT BLOCKING

  /**
   * Initializes the content blocking section.
   */
  initContentBlocking() {
    setEventListener("changeBlockListLink", "click", this.showBlockLists);
    setEventListener(
      "contentBlockingTrackingProtectionCheckbox",
      "command",
      this.trackingProtectionWritePrefs
    );
    setEventListener(
      "contentBlockingTrackingProtectionCheckbox",
      "command",
      this._updateTrackingProtectionUI
    );
    setEventListener(
      "contentBlockingCryptominersCheckbox",
      "command",
      this.updateCryptominingLists
    );
    setEventListener(
      "contentBlockingFingerprintersCheckbox",
      "command",
      this.updateFingerprintingLists
    );
    setEventListener(
      "trackingProtectionMenu",
      "command",
      this.trackingProtectionWritePrefs
    );
    setEventListener("standardArrow", "command", this.toggleExpansion);
    setEventListener("strictArrow", "command", this.toggleExpansion);
    setEventListener("customArrow", "command", this.toggleExpansion);

    Preferences.get("network.cookie.cookieBehavior").on(
      "change",
      gPrivacyPane.readBlockCookies.bind(gPrivacyPane)
    );
    Preferences.get("browser.contentblocking.category").on(
      "change",
      gPrivacyPane.highlightCBCategory
    );

    // If any relevant content blocking pref changes, show a warning that the changes will
    // not be implemented until they refresh their tabs.
    for (let pref of CONTENT_BLOCKING_PREFS) {
      Preferences.get(pref).on("change", gPrivacyPane.maybeNotifyUserToReload);
      // If the value changes, run populateCategoryContents, since that change might have been
      // triggered by a default value changing in the standard category.
      Preferences.get(pref).on("change", gPrivacyPane.populateCategoryContents);
    }
    Preferences.get("urlclassifier.trackingTable").on(
      "change",
      gPrivacyPane.maybeNotifyUserToReload
    );
    for (let button of document.querySelectorAll(".reload-tabs-button")) {
      button.addEventListener("command", gPrivacyPane.reloadAllOtherTabs);
    }

    let cryptoMinersOption = document.getElementById(
      "contentBlockingCryptominersOption"
    );
    let fingerprintersOption = document.getElementById(
      "contentBlockingFingerprintersOption"
    );

    cryptoMinersOption.hidden = !Services.prefs.getBoolPref(
      "browser.contentblocking.cryptomining.preferences.ui.enabled"
    );
    fingerprintersOption.hidden = !Services.prefs.getBoolPref(
      "browser.contentblocking.fingerprinting.preferences.ui.enabled"
    );

    Preferences.get("browser.contentblocking.features.strict").on(
      "change",
      this.populateCategoryContents
    );
    this.populateCategoryContents();
    this.highlightCBCategory();
    this.readBlockCookies();

    let link = document.getElementById("contentBlockingLearnMore");
    let contentBlockingUrl =
      Services.urlFormatter.formatURLPref("app.support.baseURL") +
      "enhanced-tracking-protection";
    link.setAttribute("href", contentBlockingUrl);

    // Toggles the text "Cross-site and social media trackers" based on the
    // social tracking pref. If the pref is false, the text reads
    // "Cross-site trackers".
    const STP_COOKIES_PREF = "privacy.socialtracking.block_cookies.enabled";
    if (Services.prefs.getBoolPref(STP_COOKIES_PREF)) {
      let contentBlockOptionSocialMedia = document.getElementById(
        "blockCookiesSocialMedia"
      );
      document.l10n.setAttributes(
        contentBlockOptionSocialMedia,
        "sitedata-option-block-cross-site-and-social-media-trackers"
      );
    }

    addCustomBlockingLearnMore();
  },

  populateCategoryContents() {
    for (let type of ["strict", "standard"]) {
      let rulesArray = [];
      let selector;
      if (type == "strict") {
        selector = "#contentBlockingOptionStrict";
        rulesArray = Services.prefs
          .getStringPref("browser.contentblocking.features.strict")
          .split(",");
      } else {
        selector = "#contentBlockingOptionStandard";
        // In standard show/hide UI items based on the default values of the relevant prefs.
        let defaults = Services.prefs.getDefaultBranch("");

        let cookieBehavior = defaults.getIntPref(
          "network.cookie.cookieBehavior"
        );
        switch (cookieBehavior) {
          case Ci.nsICookieService.BEHAVIOR_ACCEPT:
            rulesArray.push("cookieBehavior0");
            break;
          case Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN:
            rulesArray.push("cookieBehavior1");
            break;
          case Ci.nsICookieService.BEHAVIOR_REJECT:
            rulesArray.push("cookieBehavior2");
            break;
          case Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN:
            rulesArray.push("cookieBehavior3");
            break;
          case Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER:
            rulesArray.push("cookieBehavior4");
            break;
        }
        rulesArray.push(
          defaults.getBoolPref(
            "privacy.trackingprotection.cryptomining.enabled"
          )
            ? "cm"
            : "-cm"
        );
        rulesArray.push(
          defaults.getBoolPref(
            "privacy.trackingprotection.fingerprinting.enabled"
          )
            ? "fp"
            : "-fp"
        );
        rulesArray.push(
          Services.prefs.getBoolPref(
            "privacy.socialtracking.block_cookies.enabled"
          )
            ? "stp"
            : "-stp"
        );
        rulesArray.push(
          defaults.getBoolPref("privacy.trackingprotection.enabled")
            ? "tp"
            : "-tp"
        );
        rulesArray.push(
          defaults.getBoolPref("privacy.trackingprotection.pbmode.enabled")
            ? "tpPrivate"
            : "-tpPrivate"
        );
      }

      // Hide all cookie options first, until we learn which one should be showing.
      document.querySelector(selector + " .all-cookies-option").hidden = true;
      document.querySelector(
        selector + " .unvisited-cookies-option"
      ).hidden = true;
      document.querySelector(
        selector + " .third-party-tracking-cookies-option"
      ).hidden = true;
      document.querySelector(
        selector + " .all-third-party-cookies-option"
      ).hidden = true;
      document.querySelector(selector + " .social-media-option").hidden = true;

      for (let item of rulesArray) {
        // Note "cookieBehavior0", will result in no UI changes, so is not listed here.
        switch (item) {
          case "tp":
            document.querySelector(
              selector + " .trackers-option"
            ).hidden = false;
            break;
          case "-tp":
            document.querySelector(
              selector + " .trackers-option"
            ).hidden = true;
            break;
          case "tpPrivate":
            document.querySelector(
              selector + " .pb-trackers-option"
            ).hidden = false;
            break;
          case "-tpPrivate":
            document.querySelector(
              selector + " .pb-trackers-option"
            ).hidden = true;
            break;
          case "fp":
            document.querySelector(
              selector + " .fingerprinters-option"
            ).hidden = false;
            break;
          case "-fp":
            document.querySelector(
              selector + " .fingerprinters-option"
            ).hidden = true;
            break;
          case "cm":
            document.querySelector(
              selector + " .cryptominers-option"
            ).hidden = false;
            break;
          case "-cm":
            document.querySelector(
              selector + " .cryptominers-option"
            ).hidden = true;
            break;
          case "stp":
            // Store social tracking cookies pref
            const STP_COOKIES_PREF =
              "privacy.socialtracking.block_cookies.enabled";

            if (Services.prefs.getBoolPref(STP_COOKIES_PREF)) {
              document.querySelector(
                selector + " .social-media-option"
              ).hidden = false;
            }
            break;
          case "-stp":
            // Store social tracking cookies pref
            document.querySelector(
              selector + " .social-media-option"
            ).hidden = true;
            break;
          case "cookieBehavior1":
            document.querySelector(
              selector + " .all-third-party-cookies-option"
            ).hidden = false;
            break;
          case "cookieBehavior2":
            document.querySelector(
              selector + " .all-cookies-option"
            ).hidden = false;
            break;
          case "cookieBehavior3":
            document.querySelector(
              selector + " .unvisited-cookies-option"
            ).hidden = false;
            break;
          case "cookieBehavior4":
            document.querySelector(
              selector + " .third-party-tracking-cookies-option"
            ).hidden = false;
            break;
          case "cookieBehavior5":
            // No UI support for this cookie policy yet
            break;
        }
      }
      // Hide the "tracking protection in private browsing" list item
      // if the "tracking protection enabled in all windows" list item is showing.
      if (!document.querySelector(selector + " .trackers-option").hidden) {
        document.querySelector(selector + " .pb-trackers-option").hidden = true;
      }
    }
  },

  highlightCBCategory() {
    let value = Preferences.get("browser.contentblocking.category").value;
    let standardEl = document.getElementById("contentBlockingOptionStandard");
    let strictEl = document.getElementById("contentBlockingOptionStrict");
    let customEl = document.getElementById("contentBlockingOptionCustom");
    standardEl.classList.remove("selected");
    strictEl.classList.remove("selected");
    customEl.classList.remove("selected");

    switch (value) {
      case "strict":
        strictEl.classList.add("selected");
        break;
      case "custom":
        customEl.classList.add("selected");
        break;
      case "standard":
      /* fall through */
      default:
        standardEl.classList.add("selected");
        break;
    }
  },

  updateCryptominingLists() {
    let listPrefs = [
      "urlclassifier.features.cryptomining.blacklistTables",
      "urlclassifier.features.cryptomining.whitelistTables",
    ];

    let listValue = listPrefs
      .map(l => Services.prefs.getStringPref(l))
      .join(",");
    listManager.forceUpdates(listValue);
  },

  updateFingerprintingLists() {
    let listPrefs = [
      "urlclassifier.features.fingerprinting.blacklistTables",
      "urlclassifier.features.fingerprinting.whitelistTables",
    ];

    let listValue = listPrefs
      .map(l => Services.prefs.getStringPref(l))
      .join(",");
    listManager.forceUpdates(listValue);
  },

  // TRACKING PROTECTION MODE

  /**
   * Selects the right item of the Tracking Protection menulist and checkbox.
   */
  trackingProtectionReadPrefs() {
    let enabledPref = Preferences.get("privacy.trackingprotection.enabled");
    let pbmPref = Preferences.get("privacy.trackingprotection.pbmode.enabled");
    let tpMenu = document.getElementById("trackingProtectionMenu");
    let tpCheckbox = document.getElementById(
      "contentBlockingTrackingProtectionCheckbox"
    );

    this._updateTrackingProtectionUI();

    // Global enable takes precedence over enabled in Private Browsing.
    if (enabledPref.value) {
      tpMenu.value = "always";
      tpCheckbox.checked = true;
    } else if (pbmPref.value) {
      tpMenu.value = "private";
      tpCheckbox.checked = true;
    } else {
      tpMenu.value = "never";
      tpCheckbox.checked = false;
    }
  },

  /**
   * Selects the right items of the new Cookies & Site Data UI.
   */
  networkCookieBehaviorReadPrefs() {
    let behavior = Preferences.get("network.cookie.cookieBehavior").value;
    let blockCookiesMenu = document.getElementById("blockCookiesMenu");
    let deleteOnCloseCheckbox = document.getElementById("deleteOnClose");
    let deleteOnCloseNote = document.getElementById("deleteOnCloseNote");

    let blockCookies = behavior != Ci.nsICookieService.BEHAVIOR_ACCEPT;
    let cookieBehaviorLocked = Services.prefs.prefIsLocked(
      "network.cookie.cookieBehavior"
    );
    let blockCookiesControlsDisabled = !blockCookies || cookieBehaviorLocked;
    blockCookiesMenu.disabled = blockCookiesControlsDisabled;

    let completelyBlockCookies =
      behavior == Ci.nsICookieService.BEHAVIOR_REJECT;
    let privateBrowsing = Preferences.get("browser.privatebrowsing.autostart")
      .value;
    let cookieExpirationLocked = Services.prefs.prefIsLocked(
      "network.cookie.lifetimePolicy"
    );
    deleteOnCloseCheckbox.disabled =
      privateBrowsing || completelyBlockCookies || cookieExpirationLocked;
    deleteOnCloseNote.hidden = !privateBrowsing;

    switch (behavior) {
      case Ci.nsICookieService.BEHAVIOR_ACCEPT:
        break;
      case Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN:
        blockCookiesMenu.value = "all-third-parties";
        break;
      case Ci.nsICookieService.BEHAVIOR_REJECT:
        blockCookiesMenu.value = "always";
        break;
      case Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN:
        blockCookiesMenu.value = "unvisited";
        break;
      case Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER:
        blockCookiesMenu.value = "trackers";
        break;
    }
  },

  /**
   * Sets the pref values based on the selected item of the radiogroup.
   */
  trackingProtectionWritePrefs() {
    let enabledPref = Preferences.get("privacy.trackingprotection.enabled");
    let pbmPref = Preferences.get("privacy.trackingprotection.pbmode.enabled");
    let stpPref = Preferences.get(
      "privacy.trackingprotection.socialtracking.enabled"
    );
    let stpCookiePref = Preferences.get(
      "privacy.socialtracking.block_cookies.enabled"
    );
    let tpMenu = document.getElementById("trackingProtectionMenu");
    let tpCheckbox = document.getElementById(
      "contentBlockingTrackingProtectionCheckbox"
    );

    let value;
    if (tpCheckbox.checked) {
      if (tpMenu.value == "never") {
        tpMenu.value = "private";
      }
      value = tpMenu.value;
    } else {
      tpMenu.value = "never";
      value = "never";
    }

    switch (value) {
      case "always":
        enabledPref.value = true;
        pbmPref.value = true;
        if (stpCookiePref.value) {
          stpPref.value = true;
        }
        break;
      case "private":
        enabledPref.value = false;
        pbmPref.value = true;
        if (stpCookiePref.value) {
          stpPref.value = false;
        }
        break;
      case "never":
        enabledPref.value = false;
        pbmPref.value = false;
        if (stpCookiePref.value) {
          stpPref.value = false;
        }
        break;
    }
  },

  toggleExpansion(e) {
    let carat = e.target;
    carat.classList.toggle("up");
    carat.closest(".content-blocking-category").classList.toggle("expanded");
    carat.setAttribute(
      "aria-expanded",
      carat.getAttribute("aria-expanded") === "false"
    );
  },

  // HISTORY MODE

  /**
   * The list of preferences which affect the initial history mode settings.
   * If the auto start private browsing mode pref is active, the initial
   * history mode would be set to "Don't remember anything".
   * If ALL of these preferences are set to the values that correspond
   * to keeping some part of history, and the auto-start
   * private browsing mode is not active, the initial history mode would be
   * set to "Remember everything".
   * Otherwise, the initial history mode would be set to "Custom".
   *
   * Extensions adding their own preferences can set values here if needed.
   */
  prefsForKeepingHistory: {
    "places.history.enabled": true, // History is enabled
    "browser.formfill.enable": true, // Form information is saved
    "privacy.sanitize.sanitizeOnShutdown": false, // Private date is NOT cleared on shutdown
  },

  /**
   * The list of control IDs which are dependent on the auto-start private
   * browsing setting, such that in "Custom" mode they would be disabled if
   * the auto-start private browsing checkbox is checked, and enabled otherwise.
   *
   * Extensions adding their own controls can append their IDs to this array if needed.
   */
  dependentControls: [
    "rememberHistory",
    "rememberForms",
    "alwaysClear",
    "clearDataSettings",
  ],

  /**
   * Check whether preferences values are set to keep history
   *
   * @param aPrefs an array of pref names to check for
   * @returns boolean true if all of the prefs are set to keep history,
   *                  false otherwise
   */
  _checkHistoryValues(aPrefs) {
    for (let pref of Object.keys(aPrefs)) {
      if (Preferences.get(pref).value != aPrefs[pref]) {
        return false;
      }
    }
    return true;
  },

  /**
   * Initialize the history mode menulist based on the privacy preferences
   */
  initializeHistoryMode() {
    let mode;
    let getVal = aPref => Preferences.get(aPref).value;

    if (getVal("privacy.history.custom")) {
      mode = "custom";
    } else if (this._checkHistoryValues(this.prefsForKeepingHistory)) {
      if (getVal("browser.privatebrowsing.autostart")) {
        mode = "dontremember";
      } else {
        mode = "remember";
      }
    } else {
      mode = "custom";
    }

    document.getElementById("historyMode").value = mode;
  },

  /**
   * Update the selected pane based on the history mode menulist
   */
  updateHistoryModePane() {
    let selectedIndex = -1;
    switch (document.getElementById("historyMode").value) {
      case "remember":
        selectedIndex = 0;
        break;
      case "dontremember":
        selectedIndex = 1;
        break;
      case "custom":
        selectedIndex = 2;
        break;
    }
    document.getElementById("historyPane").selectedIndex = selectedIndex;
    Preferences.get("privacy.history.custom").value = selectedIndex == 2;
  },

  /**
   * Update the private browsing auto-start pref and the history mode
   * micro-management prefs based on the history mode menulist
   */
  updateHistoryModePrefs() {
    let pref = Preferences.get("browser.privatebrowsing.autostart");
    switch (document.getElementById("historyMode").value) {
      case "remember":
        if (pref.value) {
          pref.value = false;
        }

        // select the remember history option if needed
        Preferences.get("places.history.enabled").value = true;

        // select the remember forms history option
        Preferences.get("browser.formfill.enable").value = true;

        // select the clear on close option
        Preferences.get("privacy.sanitize.sanitizeOnShutdown").value = false;
        break;
      case "dontremember":
        if (!pref.value) {
          pref.value = true;
        }
        break;
    }
  },

  /**
   * Update the privacy micro-management controls based on the
   * value of the private browsing auto-start preference.
   */
  updatePrivacyMicroControls() {
    // Check the "Delete cookies when Firefox is closed" checkbox and disable the setting
    // when we're in auto private mode (or reset it back otherwise).
    document.getElementById("deleteOnClose").checked = this.readDeleteOnClose();

    let clearDataSettings = document.getElementById("clearDataSettings");

    if (document.getElementById("historyMode").value == "custom") {
      let disabled = Preferences.get("browser.privatebrowsing.autostart").value;
      this.dependentControls.forEach(aElement => {
        let control = document.getElementById(aElement);
        let preferenceId = control.getAttribute("preference");
        if (!preferenceId) {
          let dependentControlId = control.getAttribute("control");
          if (dependentControlId) {
            let dependentControl = document.getElementById(dependentControlId);
            preferenceId = dependentControl.getAttribute("preference");
          }
        }

        let preference = preferenceId ? Preferences.get(preferenceId) : {};
        control.disabled = disabled || preference.locked;
        if (control != clearDataSettings) {
          this.ensurePrivacyMicroControlUncheckedWhenDisabled(control);
        }
      });

      clearDataSettings.removeAttribute("hidden");

      if (!disabled) {
        // adjust the Settings button for sanitizeOnShutdown
        this._updateSanitizeSettingsButton();
      }
    } else {
      clearDataSettings.setAttribute("hidden", "true");
    }
  },

  ensurePrivacyMicroControlUncheckedWhenDisabled(el) {
    if (Preferences.get("browser.privatebrowsing.autostart").value) {
      // Set checked to false when called from updatePrivacyMicroControls
      el.checked = false;
      // return false for the onsyncfrompreference case:
      return false;
    }
    return undefined; // tell preferencesBindings to assign the 'right' value.
  },

  // CLEAR PRIVATE DATA

  /*
   * Preferences:
   *
   * privacy.sanitize.sanitizeOnShutdown
   * - true if the user's private data is cleared on startup according to the
   *   Clear Private Data settings, false otherwise
   */

  /**
   * Displays the Clear Private Data settings dialog.
   */
  showClearPrivateDataSettings() {
    gSubDialog.open(
      "chrome://browser/content/preferences/sanitize.xul",
      "resizable=no"
    );
  },

  /**
   * Displays a dialog from which individual parts of private data may be
   * cleared.
   */
  clearPrivateDataNow(aClearEverything) {
    var ts = Preferences.get("privacy.sanitize.timeSpan");
    var timeSpanOrig = ts.value;

    if (aClearEverything) {
      ts.value = 0;
    }

    gSubDialog.open(
      "chrome://browser/content/sanitize.xul",
      "resizable=no",
      null,
      () => {
        // reset the timeSpan pref
        if (aClearEverything) {
          ts.value = timeSpanOrig;
        }

        Services.obs.notifyObservers(null, "clear-private-data");
      }
    );
  },

  /**
   * Enables or disables the "Settings..." button depending
   * on the privacy.sanitize.sanitizeOnShutdown preference value
   */
  _updateSanitizeSettingsButton() {
    var settingsButton = document.getElementById("clearDataSettings");
    var sanitizeOnShutdownPref = Preferences.get(
      "privacy.sanitize.sanitizeOnShutdown"
    );

    settingsButton.disabled = !sanitizeOnShutdownPref.value;
  },

  toggleDoNotDisturbNotifications(event) {
    AlertsServiceDND.manualDoNotDisturb = event.target.checked;
  },

  // PRIVATE BROWSING

  /**
   * Initialize the starting state for the auto-start private browsing mode pref reverter.
   */
  initAutoStartPrivateBrowsingReverter() {
    // We determine the mode in initializeHistoryMode, which is guaranteed to have been
    // called before now, so this is up-to-date.
    let mode = document.getElementById("historyMode");
    this._lastMode = mode.selectedIndex;
    // The value of the autostart pref, on the other hand, is gotten from Preferences,
    // which updates the DOM asynchronously, so we can't rely on the DOM. Get it directly
    // from the prefs.
    this._lastCheckState = Preferences.get(
      "browser.privatebrowsing.autostart"
    ).value;
  },

  _lastMode: null,
  _lastCheckState: null,
  async updateAutostart() {
    let mode = document.getElementById("historyMode");
    let autoStart = document.getElementById("privateBrowsingAutoStart");
    let pref = Preferences.get("browser.privatebrowsing.autostart");
    if (
      (mode.value == "custom" && this._lastCheckState == autoStart.checked) ||
      (mode.value == "remember" && !this._lastCheckState) ||
      (mode.value == "dontremember" && this._lastCheckState)
    ) {
      // These are all no-op changes, so we don't need to prompt.
      this._lastMode = mode.selectedIndex;
      this._lastCheckState = autoStart.hasAttribute("checked");
      return;
    }

    if (!this._shouldPromptForRestart) {
      // We're performing a revert. Just let it happen.
      return;
    }

    let buttonIndex = await confirmRestartPrompt(
      autoStart.checked,
      1,
      true,
      false
    );
    if (buttonIndex == CONFIRM_RESTART_PROMPT_RESTART_NOW) {
      pref.value = autoStart.hasAttribute("checked");
      Services.startup.quit(
        Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart
      );
      return;
    }

    this._shouldPromptForRestart = false;

    if (this._lastCheckState) {
      autoStart.checked = "checked";
    } else {
      autoStart.removeAttribute("checked");
    }
    pref.value = autoStart.hasAttribute("checked");
    mode.selectedIndex = this._lastMode;
    mode.doCommand();

    this._shouldPromptForRestart = true;
  },

  /**
   * Displays fine-grained, per-site preferences for tracking protection.
   */
  showTrackingProtectionExceptions() {
    let params = {
      permissionType: "trackingprotection",
      hideStatusColumn: true,
    };
    gSubDialog.open(
      "chrome://browser/content/preferences/permissions.xul",
      null,
      params
    );
  },

  /**
   * Displays the available block lists for tracking protection.
   */
  showBlockLists() {
    gSubDialog.open(
      "chrome://browser/content/preferences/blocklists.xul",
      null
    );
  },

  // COOKIES AND SITE DATA

  /*
   * Preferences:
   *
   * network.cookie.cookieBehavior
   * - determines how the browser should handle cookies:
   *     0   means enable all cookies
   *     1   means reject all third party cookies
   *     2   means disable all cookies
   *     3   means reject third party cookies unless at least one is already set for the eTLD
   *         see netwerk/cookie/src/nsCookieService.cpp for details
   * network.cookie.lifetimePolicy
   * - determines how long cookies are stored:
   *     0   means keep cookies until they expire
   *     2   means keep cookies until the browser is closed
   */

  readDeleteOnClose() {
    let privateBrowsing = Preferences.get("browser.privatebrowsing.autostart")
      .value;
    if (privateBrowsing) {
      return true;
    }

    let lifetimePolicy = Preferences.get("network.cookie.lifetimePolicy").value;
    return lifetimePolicy == Ci.nsICookieService.ACCEPT_SESSION;
  },

  writeDeleteOnClose() {
    let checkbox = document.getElementById("deleteOnClose");
    return checkbox.checked
      ? Ci.nsICookieService.ACCEPT_SESSION
      : Ci.nsICookieService.ACCEPT_NORMALLY;
  },

  /**
   * Reads the network.cookie.cookieBehavior preference value and
   * enables/disables the "blockCookiesMenu" menulist accordingly.
   */
  readBlockCookies() {
    let pref = Preferences.get("network.cookie.cookieBehavior");
    let bcControl = document.getElementById("blockCookiesMenu");
    bcControl.disabled = pref.value == Ci.nsICookieService.BEHAVIOR_ACCEPT;
  },

  /**
   * Updates the "accept third party cookies" menu based on whether the
   * "contentBlockingBlockCookiesCheckbox" checkbox is checked.
   */
  writeBlockCookies() {
    let block = document.getElementById("contentBlockingBlockCookiesCheckbox");
    let blockCookiesMenu = document.getElementById("blockCookiesMenu");

    if (block.checked) {
      // Automatically select 'third-party trackers' as the default.
      blockCookiesMenu.selectedIndex = 0;
      return this.writeBlockCookiesFrom();
    }
    return Ci.nsICookieService.BEHAVIOR_ACCEPT;
  },

  readBlockCookiesFrom() {
    let pref = Preferences.get("network.cookie.cookieBehavior");
    switch (pref.value) {
      case Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN:
        return "all-third-parties";
      case Ci.nsICookieService.BEHAVIOR_REJECT:
        return "always";
      case Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN:
        return "unvisited";
      case Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER:
        return "trackers";
      default:
        return undefined;
    }
  },

  writeBlockCookiesFrom() {
    let block = document.getElementById("blockCookiesMenu").selectedItem;
    switch (block.value) {
      case "trackers":
        return Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER;
      case "unvisited":
        return Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN;
      case "always":
        return Ci.nsICookieService.BEHAVIOR_REJECT;
      case "all-third-parties":
        return Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN;
      default:
        return undefined;
    }
  },

  /**
   * Discard the browsers of all tabs in all windows. Pinned tabs, as
   * well as tabs for which discarding doesn't succeed (e.g. selected
   * tabs, tabs with beforeunload listeners), are reloaded.
   */
  reloadAllOtherTabs() {
    let ourTab = BrowserWindowTracker.getTopWindow().gBrowser.selectedTab;
    BrowserWindowTracker.orderedWindows.forEach(win => {
      let otherGBrowser = win.gBrowser;
      for (let tab of otherGBrowser.tabs) {
        if (tab == ourTab) {
          // Don't reload our preferences tab.
          continue;
        }

        if (tab.pinned || !otherGBrowser.discardBrowser(tab)) {
          otherGBrowser.reloadTab(tab);
        }
      }
    });

    for (let notification of document.querySelectorAll(".reload-tabs")) {
      notification.hidden = true;
    }
  },

  /**
   * If there are more tabs than just the preferences tab, show a warning to the user that
   * they need to reload their tabs to apply the setting.
   */
  maybeNotifyUserToReload() {
    let shouldShow = false;
    if (window.BrowserWindowTracker.orderedWindows.length > 1) {
      shouldShow = true;
    } else {
      let tabbrowser = window.BrowserWindowTracker.getTopWindow().gBrowser;
      if (tabbrowser.tabs.length > 1) {
        shouldShow = true;
      }
    }
    if (shouldShow) {
      for (let notification of document.querySelectorAll(".reload-tabs")) {
        notification.hidden = false;
      }
    }
  },

  /**
   * Displays fine-grained, per-site preferences for cookies.
   */
  showCookieExceptions() {
    var params = {
      blockVisible: true,
      sessionVisible: true,
      allowVisible: true,
      prefilledHost: "",
      permissionType: "cookie",
    };
    gSubDialog.open(
      "chrome://browser/content/preferences/permissions.xul",
      null,
      params
    );
  },

  showSiteDataSettings() {
    gSubDialog.open(
      "chrome://browser/content/preferences/siteDataSettings.xul"
    );
  },

  toggleSiteData(shouldShow) {
    let clearButton = document.getElementById("clearSiteDataButton");
    let settingsButton = document.getElementById("siteDataSettings");
    clearButton.disabled = !shouldShow;
    settingsButton.disabled = !shouldShow;
  },

  showSiteDataLoading() {
    let totalSiteDataSizeLabel = document.getElementById("totalSiteDataSize");
    document.l10n.setAttributes(
      totalSiteDataSizeLabel,
      "sitedata-total-size-calculating"
    );
  },

  updateTotalDataSizeLabel(siteDataUsage) {
    SiteDataManager.getCacheSize().then(function(cacheUsage) {
      let totalSiteDataSizeLabel = document.getElementById("totalSiteDataSize");
      let totalUsage = siteDataUsage + cacheUsage;
      let [value, unit] = DownloadUtils.convertByteUnits(totalUsage);
      document.l10n.setAttributes(
        totalSiteDataSizeLabel,
        "sitedata-total-size",
        {
          value,
          unit,
        }
      );
    });
  },

  clearSiteData() {
    gSubDialog.open("chrome://browser/content/preferences/clearSiteData.xul");
  },

  // GEOLOCATION

  /**
   * Displays the location exceptions dialog where specific site location
   * preferences can be set.
   */
  showLocationExceptions() {
    let params = { permissionType: "geo" };

    gSubDialog.open(
      "chrome://browser/content/preferences/sitePermissions.xul",
      "resizable=yes",
      params
    );
  },

  // CAMERA

  /**
   * Displays the camera exceptions dialog where specific site camera
   * preferences can be set.
   */
  showCameraExceptions() {
    let params = { permissionType: "camera" };

    gSubDialog.open(
      "chrome://browser/content/preferences/sitePermissions.xul",
      "resizable=yes",
      params
    );
  },

  // MICROPHONE

  /**
   * Displays the microphone exceptions dialog where specific site microphone
   * preferences can be set.
   */
  showMicrophoneExceptions() {
    let params = { permissionType: "microphone" };

    gSubDialog.open(
      "chrome://browser/content/preferences/sitePermissions.xul",
      "resizable=yes",
      params
    );
  },

  // NOTIFICATIONS

  /**
   * Displays the notifications exceptions dialog where specific site notification
   * preferences can be set.
   */
  showNotificationExceptions() {
    let params = { permissionType: "desktop-notification" };

    gSubDialog.open(
      "chrome://browser/content/preferences/sitePermissions.xul",
      "resizable=yes",
      params
    );
  },

  // MEDIA

  showAutoplayMediaExceptions() {
    var params = { permissionType: "autoplay-media" };

    gSubDialog.open(
      "chrome://browser/content/preferences/sitePermissions.xul",
      "resizable=yes",
      params
    );
  },

  // POP-UPS

  /**
   * Displays the popup exceptions dialog where specific site popup preferences
   * can be set.
   */
  showPopupExceptions() {
    var params = {
      blockVisible: false,
      sessionVisible: false,
      allowVisible: true,
      prefilledHost: "",
      permissionType: "popup",
    };

    gSubDialog.open(
      "chrome://browser/content/preferences/permissions.xul",
      "resizable=yes",
      params
    );
  },

  // UTILITY FUNCTIONS

  /**
   * Utility function to enable/disable the button specified by aButtonID based
   * on the value of the Boolean preference specified by aPreferenceID.
   */
  updateButtons(aButtonID, aPreferenceID) {
    var button = document.getElementById(aButtonID);
    var preference = Preferences.get(aPreferenceID);
    button.disabled = !preference.value || preference.locked;
    return undefined;
  },

  // BEGIN UI CODE

  /*
   * Preferences:
   *
   * dom.disable_open_during_load
   * - true if popups are blocked by default, false otherwise
   */

  // POP-UPS

  /**
   * Displays a dialog in which the user can view and modify the list of sites
   * where passwords are never saved.
   */
  showPasswordExceptions() {
    var params = {
      blockVisible: true,
      sessionVisible: false,
      allowVisible: false,
      hideStatusColumn: true,
      prefilledHost: "",
      permissionType: "login-saving",
    };

    gSubDialog.open(
      "chrome://browser/content/preferences/permissions.xul",
      null,
      params
    );
  },

  /**
   * Initializes master password UI: the "use master password" checkbox, selects
   * the master password button to show, and enables/disables it as necessary.
   * The master password is controlled by various bits of NSS functionality, so
   * the UI for it can't be controlled by the normal preference bindings.
   */
  _initMasterPasswordUI() {
    var noMP = !LoginHelper.isMasterPasswordSet();

    var button = document.getElementById("changeMasterPassword");
    button.disabled = noMP;

    var checkbox = document.getElementById("useMasterPassword");
    checkbox.checked = !noMP;
    checkbox.disabled =
      noMP && !Services.policies.isAllowed("createMasterPassword");
  },

  /**
   * Enables/disables the master password button depending on the state of the
   * "use master password" checkbox, and prompts for master password removal if
   * one is set.
   */
  updateMasterPasswordButton() {
    var checkbox = document.getElementById("useMasterPassword");
    var button = document.getElementById("changeMasterPassword");
    button.disabled = !checkbox.checked;

    // unchecking the checkbox should try to immediately remove the master
    // password, because it's impossible to non-destructively remove the master
    // password used to encrypt all the passwords without providing it (by
    // design), and it would be extremely odd to pop up that dialog when the
    // user closes the prefwindow and saves his settings
    if (!checkbox.checked) {
      this._removeMasterPassword();
    } else {
      this.changeMasterPassword();
    }

    this._initMasterPasswordUI();
  },

  /**
   * Displays the "remove master password" dialog to allow the user to remove
   * the current master password.  When the dialog is dismissed, master password
   * UI is automatically updated.
   */
  _removeMasterPassword() {
    var secmodDB = Cc["@mozilla.org/security/pkcs11moduledb;1"].getService(
      Ci.nsIPKCS11ModuleDB
    );
    if (secmodDB.isFIPSEnabled) {
      var bundle = document.getElementById("bundlePreferences");
      Services.prompt.alert(
        window,
        bundle.getString("pw_change_failed_title"),
        bundle.getString("pw_change2empty_in_fips_mode")
      );
      this._initMasterPasswordUI();
    } else {
      gSubDialog.open(
        "chrome://mozapps/content/preferences/removemp.xul",
        null,
        null,
        this._initMasterPasswordUI.bind(this)
      );
    }
  },

  /**
   * Displays a dialog in which the master password may be changed.
   */
  changeMasterPassword() {
    gSubDialog.open(
      "chrome://mozapps/content/preferences/changemp.xul",
      "resizable=no",
      null,
      this._initMasterPasswordUI.bind(this)
    );
  },

  /**
   * Set up the initial state for the password generation UI.
   * It will be hidden unless the .available pref is true
   */
  _initPasswordGenerationUI() {
    // we don't watch the .available pref for runtime changes
    let prefValue = Services.prefs.getBoolPref(
      PREF_PASSWORD_GENERATION_AVAILABLE,
      false
    );
    document.getElementById("generatePasswordsBox").hidden = !prefValue;
  },

  /**
   * Shows the sites where the user has saved passwords and the associated login
   * information.
   */
  showPasswords() {
    if (LoginHelper.managementURI) {
      window.docShell.messageManager.sendAsyncMessage(
        "PasswordManager:OpenPreferences",
        {
          entryPoint: "preferences",
        }
      );
      return;
    }
    Services.telemetry.recordEvent("pwmgr", "open_management", "preferences");
    gSubDialog.open("chrome://passwordmgr/content/passwordManager.xul");
  },

  /**
   * Enables/disables dependent controls related to password saving
   * When password saving is not enabled, we need to also disable the password generation checkbox
   * The Exceptions button is used to configure sites where
   * passwords are never saved. When browser is set to start in Private
   * Browsing mode, the "Remember passwords" UI is useless, so we disable it.
   */
  readSavePasswords() {
    var pref = Preferences.get("signon.rememberSignons");
    var excepts = document.getElementById("passwordExceptions");
    var generatePasswords = document.getElementById("generatePasswords");
    var autofillCheckbox = document.getElementById("passwordAutofillCheckbox");

    if (PrivateBrowsingUtils.permanentPrivateBrowsing) {
      document.getElementById("savePasswords").disabled = true;
      excepts.disabled = true;
      generatePasswords.disabled = true;
      autofillCheckbox.disabled = true;
      return false;
    }
    excepts.disabled = !pref.value;
    generatePasswords.disabled = !pref.value;
    autofillCheckbox.disabled = !pref.value;

    // don't override pref value in UI
    return undefined;
  },

  /**
   * Enables/disables the add-ons Exceptions button depending on whether
   * or not add-on installation warnings are displayed.
   */
  readWarnAddonInstall() {
    var warn = Preferences.get("xpinstall.whitelist.required");
    var exceptions = document.getElementById("addonExceptions");

    exceptions.disabled = !warn.value;

    // don't override the preference value
    return undefined;
  },

  _initSafeBrowsing() {
    let enableSafeBrowsing = document.getElementById("enableSafeBrowsing");
    let blockDownloads = document.getElementById("blockDownloads");
    let blockUncommonUnwanted = document.getElementById(
      "blockUncommonUnwanted"
    );

    let safeBrowsingPhishingPref = Preferences.get(
      "browser.safebrowsing.phishing.enabled"
    );
    let safeBrowsingMalwarePref = Preferences.get(
      "browser.safebrowsing.malware.enabled"
    );

    let blockDownloadsPref = Preferences.get(
      "browser.safebrowsing.downloads.enabled"
    );
    let malwareTable = Preferences.get("urlclassifier.malwareTable");

    let blockUnwantedPref = Preferences.get(
      "browser.safebrowsing.downloads.remote.block_potentially_unwanted"
    );
    let blockUncommonPref = Preferences.get(
      "browser.safebrowsing.downloads.remote.block_uncommon"
    );

    let learnMoreLink = document.getElementById("enableSafeBrowsingLearnMore");
    let phishingUrl =
      Services.urlFormatter.formatURLPref("app.support.baseURL") +
      "phishing-malware";
    learnMoreLink.setAttribute("href", phishingUrl);

    enableSafeBrowsing.addEventListener("command", function() {
      safeBrowsingPhishingPref.value = enableSafeBrowsing.checked;
      safeBrowsingMalwarePref.value = enableSafeBrowsing.checked;

      if (enableSafeBrowsing.checked) {
        if (blockDownloads) {
          blockDownloads.removeAttribute("disabled");
          if (blockDownloads.checked) {
            blockUncommonUnwanted.removeAttribute("disabled");
          }
        } else {
          blockUncommonUnwanted.removeAttribute("disabled");
        }
      } else {
        if (blockDownloads) {
          blockDownloads.setAttribute("disabled", "true");
        }
        blockUncommonUnwanted.setAttribute("disabled", "true");
      }
    });

    if (blockDownloads) {
      blockDownloads.addEventListener("command", function() {
        blockDownloadsPref.value = blockDownloads.checked;
        if (blockDownloads.checked) {
          blockUncommonUnwanted.removeAttribute("disabled");
        } else {
          blockUncommonUnwanted.setAttribute("disabled", "true");
        }
      });
    }

    blockUncommonUnwanted.addEventListener("command", function() {
      blockUnwantedPref.value = blockUncommonUnwanted.checked;
      blockUncommonPref.value = blockUncommonUnwanted.checked;

      let malware = malwareTable.value
        .split(",")
        .filter(
          x =>
            x !== "goog-unwanted-proto" &&
            x !== "goog-unwanted-shavar" &&
            x !== "test-unwanted-simple"
        );

      if (blockUncommonUnwanted.checked) {
        if (malware.includes("goog-malware-shavar")) {
          malware.push("goog-unwanted-shavar");
        } else {
          malware.push("goog-unwanted-proto");
        }

        malware.push("test-unwanted-simple");
      }

      // sort alphabetically to keep the pref consistent
      malware.sort();

      malwareTable.value = malware.join(",");

      // Force an update after changing the malware table.
      listManager.forceUpdates(malwareTable.value);
    });

    // set initial values

    enableSafeBrowsing.checked =
      safeBrowsingPhishingPref.value && safeBrowsingMalwarePref.value;
    if (!enableSafeBrowsing.checked) {
      if (blockDownloads) {
        blockDownloads.setAttribute("disabled", "true");
      }

      blockUncommonUnwanted.setAttribute("disabled", "true");
    }

    if (blockDownloads) {
      blockDownloads.checked = blockDownloadsPref.value;
      if (!blockDownloadsPref.value) {
        blockUncommonUnwanted.setAttribute("disabled", "true");
      }
    }

    blockUncommonUnwanted.checked =
      blockUnwantedPref.value && blockUncommonPref.value;
  },

  /**
   * Displays the exceptions lists for add-on installation warnings.
   */
  showAddonExceptions() {
    var params = this._addonParams;

    gSubDialog.open(
      "chrome://browser/content/preferences/permissions.xul",
      null,
      params
    );
  },

  /**
   * Parameters for the add-on install permissions dialog.
   */
  _addonParams: {
    blockVisible: false,
    sessionVisible: false,
    allowVisible: true,
    prefilledHost: "",
    permissionType: "install",
  },

  /**
   * readEnableOCSP is used by the preferences UI to determine whether or not
   * the checkbox for OCSP fetching should be checked (it returns true if it
   * should be checked and false otherwise). The about:config preference
   * "security.OCSP.enabled" is an integer rather than a boolean, so it can't be
   * directly mapped from {true,false} to {checked,unchecked}. The possible
   * values for "security.OCSP.enabled" are:
   * 0: fetching is disabled
   * 1: fetch for all certificates
   * 2: fetch only for EV certificates
   * Hence, if "security.OCSP.enabled" is non-zero, the checkbox should be
   * checked. Otherwise, it should be unchecked.
   */
  readEnableOCSP() {
    var preference = Preferences.get("security.OCSP.enabled");
    // This is the case if the preference is the default value.
    if (preference.value === undefined) {
      return true;
    }
    return preference.value != 0;
  },

  /**
   * writeEnableOCSP is used by the preferences UI to map the checked/unchecked
   * state of the OCSP fetching checkbox to the value that the preference
   * "security.OCSP.enabled" should be set to (it returns that value). See the
   * readEnableOCSP documentation for more background. We unfortunately don't
   * have enough information to map from {true,false} to all possible values for
   * "security.OCSP.enabled", but a reasonable alternative is to map from
   * {true,false} to {<the default value>,0}. That is, if the box is checked,
   * "security.OCSP.enabled" will be set to whatever default it should be, given
   * the platform and channel. If the box is unchecked, the preference will be
   * set to 0. Obviously this won't work if the default is 0, so we will have to
   * revisit this if we ever set it to 0.
   */
  writeEnableOCSP() {
    var checkbox = document.getElementById("enableOCSP");
    var defaults = Services.prefs.getDefaultBranch(null);
    var defaultValue = defaults.getIntPref("security.OCSP.enabled");
    return checkbox.checked ? defaultValue : 0;
  },

  /**
   * Displays the user's certificates and associated options.
   */
  showCertificates() {
    gSubDialog.open("chrome://pippki/content/certManager.xul");
  },

  /**
   * Displays a dialog from which the user can manage his security devices.
   */
  showSecurityDevices() {
    gSubDialog.open("chrome://pippki/content/device_manager.xul");
  },

  initDataCollection() {
    this._setupLearnMoreLink(
      "toolkit.datacollection.infoURL",
      "dataCollectionPrivacyNotice"
    );
  },

  initSubmitCrashes() {
    this._setupLearnMoreLink(
      "toolkit.crashreporter.infoURL",
      "crashReporterLearnMore"
    );
  },

  /**
   * Set up or hide the Learn More links for various data collection options
   */
  _setupLearnMoreLink(pref, element) {
    // set up the Learn More link with the correct URL
    let url = Services.urlFormatter.formatURLPref(pref);
    let el = document.getElementById(element);

    if (url) {
      el.setAttribute("href", url);
    } else {
      el.setAttribute("hidden", "true");
    }
  },

  /**
   * Initialize the health report service reference and checkbox.
   */
  initSubmitHealthReport() {
    this._setupLearnMoreLink(
      "datareporting.healthreport.infoURL",
      "FHRLearnMore"
    );

    let checkbox = document.getElementById("submitHealthReportBox");

    // Telemetry is only sending data if MOZ_TELEMETRY_REPORTING is defined.
    // We still want to display the preferences panel if that's not the case, but
    // we want it to be disabled and unchecked.
    if (
      Services.prefs.prefIsLocked(PREF_UPLOAD_ENABLED) ||
      !AppConstants.MOZ_TELEMETRY_REPORTING
    ) {
      checkbox.setAttribute("disabled", "true");
      return;
    }

    checkbox.checked =
      Services.prefs.getBoolPref(PREF_UPLOAD_ENABLED) &&
      AppConstants.MOZ_TELEMETRY_REPORTING;
  },

  /**
   * Update the health report preference with state from checkbox.
   */
  updateSubmitHealthReport() {
    let checkbox = document.getElementById("submitHealthReportBox");
    Services.prefs.setBoolPref(PREF_UPLOAD_ENABLED, checkbox.checked);
  },

  /**
   * Initialize the opt-out-study preference checkbox into about:preferences and
   * handles events coming from the UI for it.
   */
  initOptOutStudyCheckbox(doc) {
    // The checkbox should be disabled if any of the below are true. This
    // prevents the user from changing the value in the box.
    //
    // * the policy forbids shield
    // * Normandy is disabled
    //
    // The checkbox should match the value of the preference only if all of
    // these are true. Otherwise, the checkbox should remain unchecked. This
    // is because in these situations, Shield studies are always disabled, and
    // so showing a checkbox would be confusing.
    //
    // * the policy allows Shield
    // * Normandy is enabled

    const allowedByPolicy = Services.policies.isAllowed("Shield");
    const checkbox = document.getElementById("optOutStudiesEnabled");

    if (
      allowedByPolicy &&
      Services.prefs.getBoolPref(PREF_NORMANDY_ENABLED, false)
    ) {
      if (Services.prefs.getBoolPref(PREF_OPT_OUT_STUDIES_ENABLED, false)) {
        checkbox.setAttribute("checked", "true");
      } else {
        checkbox.removeAttribute("checked");
      }
      checkbox.setAttribute("preference", PREF_OPT_OUT_STUDIES_ENABLED);
      checkbox.removeAttribute("disabled");
    } else {
      checkbox.removeAttribute("preference");
      checkbox.removeAttribute("checked");
      checkbox.setAttribute("disabled", "true");
    }
  },

  initAddonRecommendationsCheckbox() {
    // Setup the learn more link.
    const url =
      Services.urlFormatter.formatURLPref("app.support.baseURL") +
      "personalized-addons";
    document
      .getElementById("addonRecommendationLearnMore")
      .setAttribute("href", url);

    // Setup the checkbox.
    dataCollectionCheckboxHandler({
      checkbox: document.getElementById("addonRecommendationEnabled"),
      pref: PREF_ADDON_RECOMMENDATIONS_ENABLED,
    });
  },

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "sitedatamanager:updating-sites":
        // While updating, we want to disable this section and display loading message until updated
        this.toggleSiteData(false);
        this.showSiteDataLoading();
        break;

      case "sitedatamanager:sites-updated":
        this.toggleSiteData(true);
        SiteDataManager.getTotalUsage().then(
          this.updateTotalDataSizeLabel.bind(this)
        );
        break;
    }
  },

  // Accessibility checkbox helpers
  _initA11yState() {
    this._initA11yString();
    let checkbox = document.getElementById("a11yPrivacyCheckbox");
    switch (Services.prefs.getIntPref("accessibility.force_disabled")) {
      case 1: // access blocked
        checkbox.checked = true;
        break;
      case -1: // a11y is forced on for testing
      case 0: // access allowed
        checkbox.checked = false;
        break;
    }
  },

  _initA11yString() {
    let a11yLearnMoreLink = Services.urlFormatter.formatURLPref(
      "accessibility.support.url"
    );
    document
      .getElementById("a11yLearnMoreLink")
      .setAttribute("href", a11yLearnMoreLink);
  },

  async updateA11yPrefs(checked) {
    let buttonIndex = await confirmRestartPrompt(checked, 0, true, false);
    if (buttonIndex == CONFIRM_RESTART_PROMPT_RESTART_NOW) {
      Services.prefs.setIntPref(
        "accessibility.force_disabled",
        checked ? 1 : 0
      );
      Services.telemetry.scalarSet(
        "preferences.prevent_accessibility_services",
        true
      );
      Services.startup.quit(
        Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart
      );
    }

    // Revert the checkbox in case we didn't quit
    document.getElementById("a11yPrivacyCheckbox").checked = !checked;
  },
};
