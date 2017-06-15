/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from preferences.js */

// Load DownloadUtils module for convertByteUnits
Components.utils.import("resource://gre/modules/DownloadUtils.jsm");
Components.utils.import("resource://gre/modules/LoadContextInfo.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SiteDataManager",
                                  "resource:///modules/SiteDataManager.jsm");

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
    var advancedPrefs = document.getElementById("advancedPrefs");

    var preference = document.getElementById("browser.preferences.advanced.selectedTabIndex");
    if (preference.value !== null)
        advancedPrefs.selectedIndex = preference.value;

    if (AppConstants.MOZ_UPDATER) {
      let onUnload = () => {
        window.removeEventListener("unload", onUnload);
        Services.prefs.removeObserver("app.update.", this);
      };
      window.addEventListener("unload", onUnload);
      Services.prefs.addObserver("app.update.", this);
      this.updateReadPrefs();
    }
    if (AppConstants.MOZ_CRASHREPORTER) {
      this.initSubmitCrashes();
    }
    this.initTelemetry();
    this.initSubmitHealthReport();
    this.updateOnScreenKeyboardVisibility();
    this.updateCacheSizeInputField();
    this.updateActualCacheSize();

    if (Services.prefs.getBoolPref("browser.storageManager.enabled")) {
      Services.obs.addObserver(this, "sitedatamanager:sites-updated");
      Services.obs.addObserver(this, "sitedatamanager:updating-sites");
      let unload = () => {
        window.removeEventListener("unload", unload);
        Services.obs.removeObserver(this, "sitedatamanager:sites-updated");
        Services.obs.removeObserver(this, "sitedatamanager:updating-sites");
      };
      window.addEventListener("unload", unload);
      SiteDataManager.updateSites();
      setEventListener("clearSiteDataButton", "command",
                       gAdvancedPane.clearSiteData);
      setEventListener("siteDataSettings", "command",
                       gAdvancedPane.showSiteDataSettings);

      let url = Services.urlFormatter.formatURLPref("app.support.baseURL") + "storage-permissions";
      document.getElementById("siteDataLearnMoreLink").setAttribute("href", url);
      let siteDataGroup = document.getElementById("siteDataGroup");
      siteDataGroup.hidden = false;
    }

    setEventListener("layers.acceleration.disabled", "change",
                     gAdvancedPane.updateHardwareAcceleration);
    setEventListener("advancedPrefs", "select",
                     gAdvancedPane.tabSelectionChanged);
    setEventListener("submitHealthReportBox", "command",
                     gAdvancedPane.updateSubmitHealthReport);

    setEventListener("connectionSettings", "command",
                     gAdvancedPane.showConnections);
    setEventListener("clearCacheButton", "command",
                     gAdvancedPane.clearCache);
    if (AppConstants.MOZ_UPDATER) {
      setEventListener("updateRadioGroup", "command",
                       gAdvancedPane.updateWritePrefs);
      setEventListener("showUpdateHistory", "command",
                       gAdvancedPane.showUpdates);
    }
    setEventListener("viewCertificatesButton", "command",
                     gAdvancedPane.showCertificates);
    setEventListener("viewSecurityDevicesButton", "command",
                     gAdvancedPane.showSecurityDevices);
    setEventListener("cacheSize", "change",
                     gAdvancedPane.updateCacheSizePref);

    if (Services.prefs.getBoolPref("browser.preferences.offlineGroup.enabled")) {
      this.updateOfflineApps();
      this.updateActualAppCacheSize();
      setEventListener("offlineNotifyExceptions", "command",
                      gAdvancedPane.showOfflineExceptions);
      setEventListener("offlineAppsList", "select",
                      gAdvancedPane.offlineAppSelected);
      setEventListener("offlineAppsListRemove", "command",
                      gAdvancedPane.removeOfflineApp);
      setEventListener("clearOfflineAppCacheButton", "command",
                      gAdvancedPane.clearOfflineAppCache);
      let bundlePrefs = document.getElementById("bundlePreferences");
      document.getElementById("offlineAppsList")
              .style.height = bundlePrefs.getString("offlineAppsList.height");
      let offlineGroup = document.getElementById("offlineGroup");
      offlineGroup.hidden = false;
    }

    if (AppConstants.MOZ_WIDGET_GTK) {
      // GTK tabbox' allow the scroll wheel to change the selected tab,
      // but we don't want this behavior for the in-content preferences.
      let tabsElement = document.getElementById("tabsElement");
      tabsElement.addEventListener("DOMMouseScroll", event => {
        event.stopPropagation();
      }, true);
    }
  },

  /**
   * Stores the identity of the current tab in preferences so that the selected
   * tab can be persisted between openings of the preferences window.
   */
  tabSelectionChanged() {
    if (!this._inited)
      return;
    var advancedPrefs = document.getElementById("advancedPrefs");
    var preference = document.getElementById("browser.preferences.advanced.selectedTabIndex");

    // tabSelectionChanged gets called twice due to the selectedIndex being set
    // by both the selectedItem and selectedPanel callstacks. This guard is used
    // to prevent double-counting in Telemetry.
    if (preference.valueFromPreferences != advancedPrefs.selectedIndex) {
      Services.telemetry
              .getHistogramById("FX_PREFERENCES_CATEGORY_OPENED")
              .add(telemetryBucketForCategory("advanced"));
    }

    preference.valueFromPreferences = advancedPrefs.selectedIndex;
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
    var preference = document.getElementById("security.OCSP.enabled");
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

  updateHardwareAcceleration() {
    // Placeholder for restart
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
    this._setupLearnMoreLink("toolkit.telemetry.infoURL", "telemetryLearnMore");
    // If we're not sending any Telemetry, disable the telemetry upload checkbox as well.
    if (!AppConstants.MOZ_TELEMETRY_REPORTING) {
      document.getElementById("submitTelemetryBox").setAttribute("disabled", "true");
    }
  },

  /**
   * Set the status of the telemetry controls based on the input argument.
   * @param {Boolean} aEnabled False disables the controls, true enables them.
   */
  setTelemetrySectionEnabled(aEnabled) {
    // If FHR is disabled, additional data sharing should be disabled as well.
    let disabled = !aEnabled;
    document.getElementById("submitTelemetryBox").disabled = disabled;
    if (disabled) {
      // If we disable FHR, untick the telemetry checkbox.
      Services.prefs.setBoolPref("toolkit.telemetry.enabled", false);
    }
    document.getElementById("telemetryDataDesc").disabled = disabled;
  },

  /**
   * Initialize the health report service reference and checkbox.
   */
  initSubmitHealthReport() {
    this._setupLearnMoreLink("datareporting.healthreport.infoURL", "FHRLearnMore");

    let checkbox = document.getElementById("submitHealthReportBox");

    // Telemetry is only sending data if MOZ_TELEMETRY_REPORTING is defined.
    // We still want to display the preferences panel if that's not the case, but
    // we want it to be disabled and unchecked.
    if (Services.prefs.prefIsLocked(PREF_UPLOAD_ENABLED) ||
        !AppConstants.MOZ_TELEMETRY_REPORTING) {
      checkbox.setAttribute("disabled", "true");
      return;
    }

    checkbox.checked = Services.prefs.getBoolPref(PREF_UPLOAD_ENABLED) &&
                       AppConstants.MOZ_TELEMETRY_REPORTING;
    this.setTelemetrySectionEnabled(checkbox.checked);
  },

  /**
   * Update the health report preference with state from checkbox.
   */
  updateSubmitHealthReport() {
    let checkbox = document.getElementById("submitHealthReportBox");
    Services.prefs.setBoolPref(PREF_UPLOAD_ENABLED, checkbox.checked);
    this.setTelemetrySectionEnabled(checkbox.checked);
  },

  updateOnScreenKeyboardVisibility() {
    if (AppConstants.platform == "win") {
      let minVersion = Services.prefs.getBoolPref("ui.osk.require_win10") ? 10 : 6.2;
      if (Services.vc.compare(Services.sysinfo.getProperty("version"), minVersion) >= 0) {
        document.getElementById("useOnScreenKeyboard").hidden = false;
      }
    }
  },

  // NETWORK TAB

  /*
   * Preferences:
   *
   * browser.cache.disk.capacity
   * - the size of the browser cache in KB
   * - Only used if browser.cache.disk.smart_size.enabled is disabled
   */

  /**
   * Displays a dialog in which proxy settings may be changed.
   */
  showConnections() {
    gSubDialog.open("chrome://browser/content/preferences/connection.xul");
  },

  showSiteDataSettings() {
    gSubDialog.open("chrome://browser/content/preferences/siteDataSettings.xul");
  },

  toggleSiteData(shouldShow) {
    let clearButton = document.getElementById("clearSiteDataButton");
    let settingsButton = document.getElementById("siteDataSettings");
    clearButton.disabled = !shouldShow;
    settingsButton.disabled = !shouldShow;
  },

  updateTotalDataSizeLabel(usage) {
    let prefStrBundle = document.getElementById("bundlePreferences");
    let totalSiteDataSizeLabel = document.getElementById("totalSiteDataSize");
    if (usage < 0) {
      totalSiteDataSizeLabel.textContent = prefStrBundle.getString("loadingSiteDataSize");
    } else {
      let size = DownloadUtils.convertByteUnits(usage);
      totalSiteDataSizeLabel.textContent = prefStrBundle.getFormattedString("totalSiteDataSize", size);
    }
  },

  // Retrieves the amount of space currently used by disk cache
  updateActualCacheSize() {
    var actualSizeLabel = document.getElementById("actualDiskCacheSize");
    var prefStrBundle = document.getElementById("bundlePreferences");

    // Needs to root the observer since cache service keeps only a weak reference.
    this.observer = {
      onNetworkCacheDiskConsumption(consumption) {
        var size = DownloadUtils.convertByteUnits(consumption);
        // The XBL binding for the string bundle may have been destroyed if
        // the page was closed before this callback was executed.
        if (!prefStrBundle.getFormattedString) {
          return;
        }
        actualSizeLabel.value = prefStrBundle.getFormattedString("actualDiskCacheSize", size);
      },

      QueryInterface: XPCOMUtils.generateQI([
        Components.interfaces.nsICacheStorageConsumptionObserver,
        Components.interfaces.nsISupportsWeakReference
      ])
    };

    actualSizeLabel.value = prefStrBundle.getString("actualDiskCacheSizeCalculated");

    try {
      var cacheService =
        Components.classes["@mozilla.org/netwerk/cache-storage-service;1"]
                  .getService(Components.interfaces.nsICacheStorageService);
      cacheService.asyncGetDiskConsumption(this.observer);
    } catch (e) {}
  },

  updateCacheSizeUI(smartSizeEnabled) {
    document.getElementById("useCacheBefore").disabled = smartSizeEnabled;
    document.getElementById("cacheSize").disabled = smartSizeEnabled;
    document.getElementById("useCacheAfter").disabled = smartSizeEnabled;
  },

  readSmartSizeEnabled() {
    // The smart_size.enabled preference element is inverted="true", so its
    // value is the opposite of the actual pref value
    var disabled = document.getElementById("browser.cache.disk.smart_size.enabled").value;
    this.updateCacheSizeUI(!disabled);
  },

  /**
   * Converts the cache size from units of KB to units of MB and stores it in
   * the textbox element.
   */
  updateCacheSizeInputField() {
    let cacheSizeElem = document.getElementById("cacheSize");
    let cachePref = document.getElementById("browser.cache.disk.capacity");
    cacheSizeElem.value = cachePref.value / 1024;
    if (cachePref.locked)
      cacheSizeElem.disabled = true;
  },

  /**
   * Updates the cache size preference once user enters a new value.
   * We intentionally do not set preference="browser.cache.disk.capacity"
   * onto the textbox directly, as that would update the pref at each keypress
   * not only after the final value is entered.
   */
  updateCacheSizePref() {
    let cacheSizeElem = document.getElementById("cacheSize");
    let cachePref = document.getElementById("browser.cache.disk.capacity");
    // Converts the cache size as specified in UI (in MB) to KB.
    let intValue = parseInt(cacheSizeElem.value, 10);
    cachePref.value = isNaN(intValue) ? 0 : intValue * 1024;
  },

  /**
   * Clears the cache.
   */
  clearCache() {
    try {
      var cache = Components.classes["@mozilla.org/netwerk/cache-storage-service;1"]
                            .getService(Components.interfaces.nsICacheStorageService);
      cache.clear();
    } catch (ex) {}
    this.updateActualCacheSize();
  },

  clearSiteData() {
    let flags =
      Services.prompt.BUTTON_TITLE_IS_STRING * Services.prompt.BUTTON_POS_0 +
      Services.prompt.BUTTON_TITLE_CANCEL * Services.prompt.BUTTON_POS_1 +
      Services.prompt.BUTTON_POS_0_DEFAULT;
    let prefStrBundle = document.getElementById("bundlePreferences");
    let title = prefStrBundle.getString("clearSiteDataPromptTitle");
    let text = prefStrBundle.getString("clearSiteDataPromptText");
    let btn0Label = prefStrBundle.getString("clearSiteDataNow");

    let result = Services.prompt.confirmEx(
      window, title, text, flags, btn0Label, null, null, null, {});
    if (result == 0) {
      SiteDataManager.removeAll();
    }
  },

  // Methods for Offline Apps(Appcache)

  /**
   * Clears the application cache.
   */
  clearOfflineAppCache() {
    Components.utils.import("resource:///modules/offlineAppCache.jsm");
    OfflineAppCacheHelper.clear();

    this.updateActualAppCacheSize();
    this.updateOfflineApps();
  },

  // Retrieves the amount of space currently used by offline cache
  updateActualAppCacheSize() {
    var visitor = {
      onCacheStorageInfo(aEntryCount, aConsumption, aCapacity, aDiskDirectory) {
        var actualSizeLabel = document.getElementById("actualAppCacheSize");
        var sizeStrings = DownloadUtils.convertByteUnits(aConsumption);
        var prefStrBundle = document.getElementById("bundlePreferences");
        // The XBL binding for the string bundle may have been destroyed if
        // the page was closed before this callback was executed.
        if (!prefStrBundle.getFormattedString) {
          return;
        }
        var sizeStr = prefStrBundle.getFormattedString("actualAppCacheSize", sizeStrings);
        actualSizeLabel.value = sizeStr;
      }
    };

    try {
      var cacheService =
        Components.classes["@mozilla.org/netwerk/cache-storage-service;1"]
                  .getService(Components.interfaces.nsICacheStorageService);
      var storage = cacheService.appCacheStorage(LoadContextInfo.default, null);
      storage.asyncVisitStorage(visitor, false);
    } catch (e) {}
  },

  readOfflineNotify() {
    var pref = document.getElementById("browser.offline-apps.notify");
    var button = document.getElementById("offlineNotifyExceptions");
    button.disabled = !pref.value;
    return pref.value;
  },

  showOfflineExceptions() {
    var bundlePreferences = document.getElementById("bundlePreferences");
    var params = { blockVisible: false,
                   sessionVisible: false,
                   allowVisible: false,
                   prefilledHost: "",
                   permissionType: "offline-app",
                   manageCapability: Components.interfaces.nsIPermissionManager.DENY_ACTION,
                   windowTitle: bundlePreferences.getString("offlinepermissionstitle"),
                   introText: bundlePreferences.getString("offlinepermissionstext") };
    gSubDialog.open("chrome://browser/content/preferences/permissions.xul",
                    null, params);
  },

  // XXX: duplicated in browser.js
  _getOfflineAppUsage(perm, groups) {
    let cacheService = Cc["@mozilla.org/network/application-cache-service;1"].
                       getService(Ci.nsIApplicationCacheService);
    if (!groups) {
      try {
        groups = cacheService.getGroups();
      } catch (ex) {
        return 0;
      }
    }

    let usage = 0;
    for (let group of groups) {
      let uri = Services.io.newURI(group);
      if (perm.matchesURI(uri, true)) {
        let cache = cacheService.getActiveCache(group);
        usage += cache.usage;
      }
    }

    return usage;
  },

  /**
   * Updates the list of offline applications
   */
  updateOfflineApps() {
    var pm = Components.classes["@mozilla.org/permissionmanager;1"]
                       .getService(Components.interfaces.nsIPermissionManager);

    var list = document.getElementById("offlineAppsList");
    while (list.firstChild) {
      list.firstChild.remove();
    }

    var groups;
    try {
      var cacheService = Components.classes["@mozilla.org/network/application-cache-service;1"].
                         getService(Components.interfaces.nsIApplicationCacheService);
      groups = cacheService.getGroups();
    } catch (e) {
      return;
    }

    var bundle = document.getElementById("bundlePreferences");

    var enumerator = pm.enumerator;
    while (enumerator.hasMoreElements()) {
      var perm = enumerator.getNext().QueryInterface(Components.interfaces.nsIPermission);
      if (perm.type == "offline-app" &&
          perm.capability != Components.interfaces.nsIPermissionManager.DEFAULT_ACTION &&
          perm.capability != Components.interfaces.nsIPermissionManager.DENY_ACTION) {
        var row = document.createElement("listitem");
        row.id = "";
        row.className = "offlineapp";
        row.setAttribute("origin", perm.principal.origin);
        var converted = DownloadUtils.
                        convertByteUnits(this._getOfflineAppUsage(perm, groups));
        row.setAttribute("usage",
                         bundle.getFormattedString("offlineAppUsage",
                                                   converted));
        list.appendChild(row);
      }
    }
  },

  offlineAppSelected() {
    var removeButton = document.getElementById("offlineAppsListRemove");
    var list = document.getElementById("offlineAppsList");
    if (list.selectedItem) {
      removeButton.setAttribute("disabled", "false");
    } else {
      removeButton.setAttribute("disabled", "true");
    }
  },

  removeOfflineApp() {
    var list = document.getElementById("offlineAppsList");
    var item = list.selectedItem;
    var origin = item.getAttribute("origin");
    var principal = Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(origin);

    var prompts = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                            .getService(Components.interfaces.nsIPromptService);
    var flags = prompts.BUTTON_TITLE_IS_STRING * prompts.BUTTON_POS_0 +
                prompts.BUTTON_TITLE_CANCEL * prompts.BUTTON_POS_1;

    var bundle = document.getElementById("bundlePreferences");
    var title = bundle.getString("offlineAppRemoveTitle");
    var prompt = bundle.getFormattedString("offlineAppRemovePrompt", [principal.URI.prePath]);
    var confirm = bundle.getString("offlineAppRemoveConfirm");
    var result = prompts.confirmEx(window, title, prompt, flags, confirm,
                                   null, null, null, {});
    if (result != 0)
      return;

    // get the permission
    var pm = Components.classes["@mozilla.org/permissionmanager;1"]
                       .getService(Components.interfaces.nsIPermissionManager);
    var perm = pm.getPermissionObject(principal, "offline-app", true);
    if (perm) {
      // clear offline cache entries
      try {
        var cacheService = Components.classes["@mozilla.org/network/application-cache-service;1"].
                           getService(Components.interfaces.nsIApplicationCacheService);
        var groups = cacheService.getGroups();
        for (var i = 0; i < groups.length; i++) {
          var uri = Services.io.newURI(groups[i]);
          if (perm.matchesURI(uri, true)) {
            var cache = cacheService.getActiveCache(groups[i]);
            cache.discard();
          }
        }
      } catch (e) {}

      pm.removePermission(perm);
    }
    list.removeChild(item);
    gAdvancedPane.offlineAppSelected();
    this.updateActualAppCacheSize();
  },
  // Methods for Offline Apps(Appcache) end

  // UPDATE TAB

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

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "nsPref:changed":
        this.updateReadPrefs();
        break;

      case "sitedatamanager:updating-sites":
        // While updating, we want to disable this section and display loading message until updated
        this.toggleSiteData(false);
        this.updateTotalDataSizeLabel(-1);
        break;

      case "sitedatamanager:sites-updated":
        this.toggleSiteData(true);
        SiteDataManager.getTotalUsage()
          .then(this.updateTotalDataSizeLabel.bind(this));
        break;
    }
  },
};
