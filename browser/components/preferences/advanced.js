# -*- indent-tabs-mode: nil; js-indent-level: 4 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Load DownloadUtils module for convertByteUnits
Components.utils.import("resource://gre/modules/DownloadUtils.jsm");
Components.utils.import("resource://gre/modules/ctypes.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/LoadContextInfo.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

var gAdvancedPane = {
  _inited: false,

  /**
   * Brings the appropriate tab to the front and initializes various bits of UI.
   */
  init: function ()
  {
    this._inited = true;
    var advancedPrefs = document.getElementById("advancedPrefs");

    var extraArgs = window.arguments[1];
    if (extraArgs && extraArgs["advancedTab"]){
      advancedPrefs.selectedTab = document.getElementById(extraArgs["advancedTab"]);
    } else {
      var preference = document.getElementById("browser.preferences.advanced.selectedTabIndex");
      if (preference.value !== null)
        advancedPrefs.selectedIndex = preference.value;
    }

#ifdef MOZ_UPDATER
    this.updateReadPrefs();
#endif
    this.updateOfflineApps();
#ifdef MOZ_CRASHREPORTER
    this.initSubmitCrashes();
#endif
    this.initTelemetry();
#ifdef MOZ_SERVICES_HEALTHREPORT
    this.initSubmitHealthReport();
#endif

    this.updateActualCacheSize();
    this.updateActualAppCacheSize();

    // Notify observers that the UI is now ready
    Services.obs.notifyObservers(window, "advanced-pane-loaded", null);
  },

  /**
   * Stores the identity of the current tab in preferences so that the selected
   * tab can be persisted between openings of the preferences window.
   */
  tabSelectionChanged: function ()
  {
    if (!this._inited)
      return;
    var advancedPrefs = document.getElementById("advancedPrefs");
    var preference = document.getElementById("browser.preferences.advanced.selectedTabIndex");
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
  readCheckSpelling: function ()
  {
    var pref = document.getElementById("layout.spellcheckDefault");
    this._storedSpellCheck = pref.value;

    return (pref.value != 0);
  },

  /**
   * Returns the value of the spellchecking preference represented by UI,
   * preserving the preference's "hidden" value if the preference is
   * unchanged and represents a value not strictly allowed in UI.
   */
  writeCheckSpelling: function ()
  {
    var checkbox = document.getElementById("checkSpelling");
    return checkbox.checked ? (this._storedSpellCheck == 2 ? 2 : 1) : 0;
  },

  /**
   * security.OCSP.enabled is an integer value for legacy reasons.
   * A value of 1 means OCSP is enabled. Any other value means it is disabled.
   */
  readEnableOCSP: function ()
  {
    var preference = document.getElementById("security.OCSP.enabled");
    // This is the case if the preference is the default value.
    if (preference.value === undefined) {
      return true;
    }
    return preference.value == 1;
  },

  /**
   * See documentation for readEnableOCSP.
   */
  writeEnableOCSP: function ()
  {
    var checkbox = document.getElementById("enableOCSP");
    return checkbox.checked ? 1 : 0;
  },

  /**
   * When the user toggles the layers.acceleration.disabled pref,
   * sync its new value to the gfx.direct2d.disabled pref too.
   */
  updateHardwareAcceleration: function()
  {
#ifdef XP_WIN
    var fromPref = document.getElementById("layers.acceleration.disabled");
    var toPref = document.getElementById("gfx.direct2d.disabled");
    toPref.value = fromPref.value;
#endif
  },

  // DATA CHOICES TAB

  /**
   * opening links behind a modal dialog is poor form. Work around flawed text-link handling here.
   */
  openTextLink: function (evt) {
    let where = Services.prefs.getBoolPref("browser.preferences.instantApply") ? "tab" : "window";
    openUILinkIn(evt.target.getAttribute("href"), where);
    evt.preventDefault();
  },

  /**
   * Set up or hide the Learn More links for various data collection options
   */
  _setupLearnMoreLink: function (pref, element) {
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
  initSubmitCrashes: function ()
  {
    var checkbox = document.getElementById("submitCrashesBox");
    try {
      var cr = Components.classes["@mozilla.org/toolkit/crash-reporter;1"].
               getService(Components.interfaces.nsICrashReporter);
      checkbox.checked = cr.submitReports;
    } catch (e) {
      checkbox.style.display = "none";
    }
    this._setupLearnMoreLink("toolkit.crashreporter.infoURL", "crashReporterLearnMore");
  },

  /**
   *
   */
  updateSubmitCrashes: function ()
  {
    var checkbox = document.getElementById("submitCrashesBox");
    try {
      var cr = Components.classes["@mozilla.org/toolkit/crash-reporter;1"].
               getService(Components.interfaces.nsICrashReporter);
      cr.submitReports = checkbox.checked;
    } catch (e) { }
  },


  /**
   * The preference/checkbox is configured in XUL.
   *
   * In all cases, set up the Learn More link sanely
   */
  initTelemetry: function ()
  {
#ifdef MOZ_TELEMETRY_REPORTING
    this._setupLearnMoreLink("toolkit.telemetry.infoURL", "telemetryLearnMore");
#endif
  },

#ifdef MOZ_SERVICES_HEALTHREPORT
  /**
   * Initialize the health report service reference and checkbox.
   */
  initSubmitHealthReport: function () {
    this._setupLearnMoreLink("datareporting.healthreport.infoURL", "FHRLearnMore");

    let policy = Components.classes["@mozilla.org/datareporting/service;1"]
                                   .getService(Components.interfaces.nsISupports)
                                   .wrappedJSObject
                                   .policy;

    let checkbox = document.getElementById("submitHealthReportBox");

    if (!policy || policy.healthReportUploadLocked) {
      checkbox.setAttribute("disabled", "true");
      return;
    }

    checkbox.checked = policy.healthReportUploadEnabled;
  },

  /**
   * Update the health report policy acceptance with state from checkbox.
   */
  updateSubmitHealthReport: function () {
    let policy = Components.classes["@mozilla.org/datareporting/service;1"]
                                   .getService(Components.interfaces.nsISupports)
                                   .wrappedJSObject
                                   .policy;

    if (!policy) {
      return;
    }

    let checkbox = document.getElementById("submitHealthReportBox");
    policy.recordHealthReportUploadEnabled(checkbox.checked,
                                           "Checkbox from preferences pane");
  },
#endif

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
  showConnections: function ()
  {
    document.documentElement.openSubDialog("chrome://browser/content/preferences/connection.xul",
                                           "", null);
  },

  // Retrieves the amount of space currently used by disk cache
  updateActualCacheSize: function ()
  {
    var actualSizeLabel = document.getElementById("actualDiskCacheSize");
    var prefStrBundle = document.getElementById("bundlePreferences");

    // Needs to root the observer since cache service keeps only a weak reference.
    this.observer = {
      onNetworkCacheDiskConsumption: function(consumption) {
        var size = DownloadUtils.convertByteUnits(consumption);
        actualSizeLabel.value = prefStrBundle.getFormattedString("actualDiskCacheSize", size);
      },

      QueryInterface: XPCOMUtils.generateQI([
        Components.interfaces.nsICacheStorageConsumptionObserver,
        Components.interfaces.nsISupportsWeakReference
      ])
    };

    actualSizeLabel.value = prefStrBundle.getString("actualDiskCacheSizeCalculated");

    var cacheService =
      Components.classes["@mozilla.org/netwerk/cache-storage-service;1"]
                .getService(Components.interfaces.nsICacheStorageService);
    cacheService.asyncGetDiskConsumption(this.observer);
  },

  // Retrieves the amount of space currently used by offline cache
  updateActualAppCacheSize: function ()
  {
    var visitor = {
      onCacheStorageInfo: function (aEntryCount, aConsumption, aCapacity, aDiskDirectory)
      {
        var actualSizeLabel = document.getElementById("actualAppCacheSize");
        var sizeStrings = DownloadUtils.convertByteUnits(aConsumption);
        var prefStrBundle = document.getElementById("bundlePreferences");
        var sizeStr = prefStrBundle.getFormattedString("actualAppCacheSize", sizeStrings);
        actualSizeLabel.value = sizeStr;
      }
    };

    var cacheService =
      Components.classes["@mozilla.org/netwerk/cache-storage-service;1"]
                .getService(Components.interfaces.nsICacheStorageService);
    var storage = cacheService.appCacheStorage(LoadContextInfo.default, null);
    storage.asyncVisitStorage(visitor, false);
  },

  updateCacheSizeUI: function (smartSizeEnabled)
  {
    document.getElementById("useCacheBefore").disabled = smartSizeEnabled;
    document.getElementById("cacheSize").disabled = smartSizeEnabled;
    document.getElementById("useCacheAfter").disabled = smartSizeEnabled;
  },

  readSmartSizeEnabled: function ()
  {
    // The smart_size.enabled preference element is inverted="true", so its
    // value is the opposite of the actual pref value
    var disabled = document.getElementById("browser.cache.disk.smart_size.enabled").value;
    this.updateCacheSizeUI(!disabled);
  },

  /**
   * Converts the cache size from units of KB to units of MB and returns that
   * value.
   */
  readCacheSize: function ()
  {
    var preference = document.getElementById("browser.cache.disk.capacity");
    return preference.value / 1024;
  },

  /**
   * Converts the cache size as specified in UI (in MB) to KB and returns that
   * value.
   */
  writeCacheSize: function ()
  {
    var cacheSize = document.getElementById("cacheSize");
    var intValue = parseInt(cacheSize.value, 10);
    return isNaN(intValue) ? 0 : intValue * 1024;
  },

  /**
   * Clears the cache.
   */
  clearCache: function ()
  {
    var cache = Components.classes["@mozilla.org/netwerk/cache-storage-service;1"]
                                 .getService(Components.interfaces.nsICacheStorageService);
    try {
      cache.clear();
    } catch(ex) {}
    this.updateActualCacheSize();
  },

  /**
   * Clears the application cache.
   */
  clearOfflineAppCache: function ()
  {
    Components.utils.import("resource:///modules/offlineAppCache.jsm");
    OfflineAppCacheHelper.clear();

    this.updateActualAppCacheSize();
    this.updateOfflineApps();
  },

  readOfflineNotify: function()
  {
    var pref = document.getElementById("browser.offline-apps.notify");
    var button = document.getElementById("offlineNotifyExceptions");
    button.disabled = !pref.value;
    return pref.value;
  },

  showOfflineExceptions: function()
  {
    var bundlePreferences = document.getElementById("bundlePreferences");
    var params = { blockVisible     : false,
                   sessionVisible   : false,
                   allowVisible     : false,
                   prefilledHost    : "",
                   permissionType   : "offline-app",
                   manageCapability : Components.interfaces.nsIPermissionManager.DENY_ACTION,
                   windowTitle      : bundlePreferences.getString("offlinepermissionstitle"),
                   introText        : bundlePreferences.getString("offlinepermissionstext") };
    document.documentElement.openWindow("Browser:Permissions",
                                        "chrome://browser/content/preferences/permissions.xul",
                                        "", params);
  },

  // XXX: duplicated in browser.js
  _getOfflineAppUsage: function (host, groups)
  {
    var cacheService = Components.classes["@mozilla.org/network/application-cache-service;1"].
                       getService(Components.interfaces.nsIApplicationCacheService);
    if (!groups)
      groups = cacheService.getGroups();

    var ios = Components.classes["@mozilla.org/network/io-service;1"].
              getService(Components.interfaces.nsIIOService);

    var usage = 0;
    for (var i = 0; i < groups.length; i++) {
      var uri = ios.newURI(groups[i], null, null);
      if (uri.asciiHost == host) {
        var cache = cacheService.getActiveCache(groups[i]);
        usage += cache.usage;
      }
    }

    return usage;
  },

  /**
   * Updates the list of offline applications
   */
  updateOfflineApps: function ()
  {
    var pm = Components.classes["@mozilla.org/permissionmanager;1"]
                       .getService(Components.interfaces.nsIPermissionManager);

    var list = document.getElementById("offlineAppsList");
    while (list.firstChild) {
      list.removeChild(list.firstChild);
    }

    var cacheService = Components.classes["@mozilla.org/network/application-cache-service;1"].
                       getService(Components.interfaces.nsIApplicationCacheService);
    var groups = cacheService.getGroups();

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
        row.setAttribute("host", perm.host);
        var converted = DownloadUtils.
                        convertByteUnits(this._getOfflineAppUsage(perm.host, groups));
        row.setAttribute("usage",
                         bundle.getFormattedString("offlineAppUsage",
                                                   converted));
        list.appendChild(row);
      }
    }
  },

  offlineAppSelected: function()
  {
    var removeButton = document.getElementById("offlineAppsListRemove");
    var list = document.getElementById("offlineAppsList");
    if (list.selectedItem) {
      removeButton.setAttribute("disabled", "false");
    } else {
      removeButton.setAttribute("disabled", "true");
    }
  },

  removeOfflineApp: function()
  {
    var list = document.getElementById("offlineAppsList");
    var item = list.selectedItem;
    var host = item.getAttribute("host");

    var prompts = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                            .getService(Components.interfaces.nsIPromptService);
    var flags = prompts.BUTTON_TITLE_IS_STRING * prompts.BUTTON_POS_0 +
                prompts.BUTTON_TITLE_CANCEL * prompts.BUTTON_POS_1;

    var bundle = document.getElementById("bundlePreferences");
    var title = bundle.getString("offlineAppRemoveTitle");
    var prompt = bundle.getFormattedString("offlineAppRemovePrompt", [host]);
    var confirm = bundle.getString("offlineAppRemoveConfirm");
    var result = prompts.confirmEx(window, title, prompt, flags, confirm,
                                   null, null, null, {});
    if (result != 0)
      return;

    // clear offline cache entries
    var cacheService = Components.classes["@mozilla.org/network/application-cache-service;1"].
                       getService(Components.interfaces.nsIApplicationCacheService);
    var ios = Components.classes["@mozilla.org/network/io-service;1"].
              getService(Components.interfaces.nsIIOService);
    var groups = cacheService.getGroups();
    for (var i = 0; i < groups.length; i++) {
        var uri = ios.newURI(groups[i], null, null);
        if (uri.asciiHost == host) {
            var cache = cacheService.getActiveCache(groups[i]);
            cache.discard();
        }
    }

    // remove the permission
    var pm = Components.classes["@mozilla.org/permissionmanager;1"]
                       .getService(Components.interfaces.nsIPermissionManager);
    pm.remove(host, "offline-app",
              Components.interfaces.nsIPermissionManager.ALLOW_ACTION);
    pm.remove(host, "offline-app",
              Components.interfaces.nsIOfflineCacheUpdateService.ALLOW_NO_WARN);

    list.removeChild(item);
    gAdvancedPane.offlineAppSelected();
    this.updateActualAppCacheSize();
  },

  // UPDATE TAB

  /*
   * Preferences:
   *
   * app.update.enabled
   * - true if updates to the application are enabled, false otherwise
   * extensions.update.enabled
   * - true if updates to extensions and themes are enabled, false otherwise
   * browser.search.update
   * - true if updates to search engines are enabled, false otherwise
   * app.update.auto
   * - true if updates should be automatically downloaded and installed,
   *   possibly with a warning if incompatible extensions are installed (see
   *   app.update.mode); false if the user should be asked what he wants to do
   *   when an update is available
   * app.update.mode
   * - an integer:
   *     0    do not warn if an update will disable extensions or themes
   *     1    warn if an update will disable extensions or themes
   *     2    warn if an update will disable extensions or themes *or* if the
   *          update is a major update
   */

#ifdef MOZ_UPDATER
  /**
   * Selects the item of the radiogroup, and sets the warnIncompatible checkbox
   * based on the pref values and locked states.
   *
   * UI state matrix for update preference conditions
   *
   * UI Components:                              Preferences
   * Radiogroup                                  i   = app.update.enabled
   * Warn before disabling extensions checkbox   ii  = app.update.auto
   *                                             iii = app.update.mode
   *
   * Disabled states:
   * Element           pref  value  locked  disabled
   * radiogroup        i     t/f    f       false
   *                   i     t/f    *t*     *true*
   *                   ii    t/f    f       false
   *                   ii    t/f    *t*     *true*
   *                   iii   0/1/2  t/f     false
   * warnIncompatible  i     t      f       false
   *                   i     t      *t*     *true*
   *                   i     *f*    t/f     *true*
   *                   ii    t      f       false
   *                   ii    t      *t*     *true*
   *                   ii    *f*    t/f     *true*
   *                   iii   0/1/2  f       false
   *                   iii   0/1/2  *t*     *true*
   */
  updateReadPrefs: function ()
  {
    var enabledPref = document.getElementById("app.update.enabled");
    var autoPref = document.getElementById("app.update.auto");
#ifdef XP_WIN
#ifdef MOZ_METRO
    var metroEnabledPref = document.getElementById("app.update.metro.enabled");
#endif
#endif
    var radiogroup = document.getElementById("updateRadioGroup");

    if (!enabledPref.value)   // Don't care for autoPref.value in this case.
      radiogroup.value="manual";    // 3. Never check for updates.
#ifdef XP_WIN
#ifdef MOZ_METRO
    // enabledPref.value && autoPref.value && metroEnabledPref.value
    else if (metroEnabledPref.value && this._showingWin8Prefs)
      radiogroup.value="autoMetro"; // 0. Automatically install updates for both Metro and Desktop
#endif
#endif
    else if (autoPref.value)  // enabledPref.value && autoPref.value
      radiogroup.value="auto";      // 1. Automatically install updates for Desktop only
    else                      // enabledPref.value && !autoPref.value
      radiogroup.value="checkOnly"; // 2. Check, but let me choose

    var canCheck = Components.classes["@mozilla.org/updates/update-service;1"].
                     getService(Components.interfaces.nsIApplicationUpdateService).
                     canCheckForUpdates;
    // canCheck is false if the enabledPref is false and locked,
    // or the binary platform or OS version is not known.
    // A locked pref is sufficient to disable the radiogroup.
    radiogroup.disabled = !canCheck || enabledPref.locked || autoPref.locked;

    var modePref = document.getElementById("app.update.mode");
    var warnIncompatible = document.getElementById("warnIncompatible");
    // the warnIncompatible checkbox value is set by readAddonWarn
    warnIncompatible.disabled = radiogroup.disabled || modePref.locked ||
                                !enabledPref.value || !autoPref.value;
#ifdef XP_WIN
#ifdef MOZ_METRO
    if (this._showingWin8Prefs) {
      warnIncompatible.disabled |= metroEnabledPref.value;
      warnIncompatible.checked |= metroEnabledPref.value;
    }
#endif
#endif

#ifdef MOZ_MAINTENANCE_SERVICE
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
    } catch(e) {
    }
    if (installed != 1) {
      document.getElementById("useService").hidden = true;
    }
    try {
      const DRIVE_FIXED = 3;
      const LPCWSTR = ctypes.jschar.ptr;
      const UINT = ctypes.uint32_t;
      let kernel32 = ctypes.open("kernel32");
      let GetDriveType = kernel32.declare("GetDriveTypeW", ctypes.default_abi, UINT, LPCWSTR);
      var UpdatesDir = Components.classes["@mozilla.org/updates/update-service;1"].
                       getService(Components.interfaces.nsIApplicationUpdateService);
      let rootPath = UpdatesDir.getUpdatesDirectory();
      while (rootPath.parent != null) {
        rootPath = rootPath.parent;
      }
      if (GetDriveType(rootPath.path) != DRIVE_FIXED) {
        document.getElementById("useService").hidden = true;
      }
      kernel32.close();
    } catch(e) {
    }
#endif
  },

  /**
   * Sets the pref values based on the selected item of the radiogroup,
   * and sets the disabled state of the warnIncompatible checkbox accordingly.
   */
  updateWritePrefs: function ()
  {
    var enabledPref = document.getElementById("app.update.enabled");
    var autoPref = document.getElementById("app.update.auto");
    var modePref = document.getElementById("app.update.mode");
#ifdef XP_WIN
#ifdef MOZ_METRO
    var metroEnabledPref = document.getElementById("app.update.metro.enabled");
    // Initialize the pref to false only if we're showing the option
    if (this._showingWin8Prefs) {
      metroEnabledPref.value = false;
    }
#endif
#endif
    var radiogroup = document.getElementById("updateRadioGroup");
    switch (radiogroup.value) {
      case "auto":      // 1. Automatically install updates for Desktop only
        enabledPref.value = true;
        autoPref.value = true;
        break;
#ifdef XP_WIN
#ifdef MOZ_METRO
      case "autoMetro": // 0. Automatically install updates for both Metro and Desktop
        enabledPref.value = true;
        autoPref.value = true;
        metroEnabledPref.value = true;
        modePref.value = 1;
        break;
#endif
#endif
      case "checkOnly": // 2. Check, but let me choose
        enabledPref.value = true;
        autoPref.value = false;
        break;
      case "manual":    // 3. Never check for updates.
        enabledPref.value = false;
        autoPref.value = false;
    }

    var warnIncompatible = document.getElementById("warnIncompatible");
    warnIncompatible.disabled = enabledPref.locked || !enabledPref.value ||
                                autoPref.locked || !autoPref.value ||
                                modePref.locked;

#ifdef XP_WIN
#ifdef MOZ_METRO
    if (this._showingWin8Prefs) {
      warnIncompatible.disabled |= metroEnabledPref.value;
      warnIncompatible.checked |= metroEnabledPref.value;
    }
#endif
#endif
  },

  /**
   * Stores the value of the app.update.mode preference, which is a tristate
   * integer preference.  We store the value here so that we can properly
   * restore the preference value if the UI reflecting the preference value
   * is in a state which can represent either of two integer values (as
   * opposed to only one possible value in the other UI state).
   */
  _modePreference: -1,

  /**
   * Reads the app.update.mode preference and converts its value into a
   * true/false value for use in determining whether the "Warn me if this will
   * disable extensions or themes" checkbox is checked.  We also save the value
   * of the preference so that the preference value can be properly restored if
   * the user's preferences cannot adequately be expressed by a single checkbox.
   *
   * app.update.mode          Checkbox State    Meaning
   * 0                        Unchecked         Do not warn
   * 1                        Checked           Warn if there are incompatibilities
   * 2                        Checked           Warn if there are incompatibilities,
   *                                            or the update is major.
   */
  readAddonWarn: function ()
  {
    var preference = document.getElementById("app.update.mode");
    var warn = preference.value != 0;
    gAdvancedPane._modePreference = warn ? preference.value : 1;
    return warn;
  },

  /**
   * Converts the state of the "Warn me if this will disable extensions or
   * themes" checkbox into the integer preference which represents it,
   * returning that value.
   */
  writeAddonWarn: function ()
  {
    var warnIncompatible = document.getElementById("warnIncompatible");
    return !warnIncompatible.checked ? 0 : gAdvancedPane._modePreference;
  },

  /**
   * Displays the history of installed updates.
   */
  showUpdates: function ()
  {
    var prompter = Components.classes["@mozilla.org/updates/update-prompt;1"]
                             .createInstance(Components.interfaces.nsIUpdatePrompt);
    prompter.showUpdateHistory(window);
  },
#endif

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
  showCertificates: function ()
  {
    document.documentElement.openWindow("mozilla:certmanager",
                                        "chrome://pippki/content/certManager.xul",
                                        "", null);
  },

  /**
   * Displays a dialog from which the user can manage his security devices.
   */
  showSecurityDevices: function ()
  {
    document.documentElement.openWindow("mozilla:devicemanager",
                                        "chrome://pippki/content/device_manager.xul",
                                        "", null);
  }
};
