/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from preferences.js */
/* import-globals-from ../../../../toolkit/mozapps/preferences/fontbuilder.js */
/* import-globals-from ../../../base/content/aboutDialog-appUpdater.js */

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/Downloads.jsm");
Components.utils.import("resource://gre/modules/FileUtils.jsm");
Components.utils.import("resource:///modules/ShellService.jsm");
Components.utils.import("resource:///modules/TransientPrefs.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/AppConstants.jsm");

// Constants & Enumeration Values
const TYPE_MAYBE_FEED = "application/vnd.mozilla.maybe.feed";
const TYPE_MAYBE_VIDEO_FEED = "application/vnd.mozilla.maybe.video.feed";
const TYPE_MAYBE_AUDIO_FEED = "application/vnd.mozilla.maybe.audio.feed";
const TYPE_PDF = "application/pdf";

const PREF_PDFJS_DISABLED = "pdfjs.disabled";
const TOPIC_PDFJS_HANDLER_CHANGED = "pdfjs:handlerChanged";

const PREF_DISABLED_PLUGIN_TYPES = "plugin.disable_full_page_plugin_for_types";

// Preferences that affect which entries to show in the list.
const PREF_SHOW_PLUGINS_IN_LIST = "browser.download.show_plugins_in_list";
const PREF_HIDE_PLUGINS_WITHOUT_EXTENSIONS =
  "browser.download.hide_plugins_without_extensions";

/*
 * Preferences where we store handling information about the feed type.
 *
 * browser.feeds.handler
 * - "bookmarks", "reader" (clarified further using the .default preference),
 *   or "ask" -- indicates the default handler being used to process feeds;
 *   "bookmarks" is obsolete; to specify that the handler is bookmarks,
 *   set browser.feeds.handler.default to "bookmarks";
 *
 * browser.feeds.handler.default
 * - "bookmarks", "client" or "web" -- indicates the chosen feed reader used
 *   to display feeds, either transiently (i.e., when the "use as default"
 *   checkbox is unchecked, corresponds to when browser.feeds.handler=="ask")
 *   or more permanently (i.e., the item displayed in the dropdown in Feeds
 *   preferences)
 *
 * browser.feeds.handler.webservice
 * - the URL of the currently selected web service used to read feeds
 *
 * browser.feeds.handlers.application
 * - nsILocalFile, stores the current client-side feed reading app if one has
 *   been chosen
 */
const PREF_FEED_SELECTED_APP    = "browser.feeds.handlers.application";
const PREF_FEED_SELECTED_WEB    = "browser.feeds.handlers.webservice";
const PREF_FEED_SELECTED_ACTION = "browser.feeds.handler";
const PREF_FEED_SELECTED_READER = "browser.feeds.handler.default";

const PREF_VIDEO_FEED_SELECTED_APP    = "browser.videoFeeds.handlers.application";
const PREF_VIDEO_FEED_SELECTED_WEB    = "browser.videoFeeds.handlers.webservice";
const PREF_VIDEO_FEED_SELECTED_ACTION = "browser.videoFeeds.handler";
const PREF_VIDEO_FEED_SELECTED_READER = "browser.videoFeeds.handler.default";

const PREF_AUDIO_FEED_SELECTED_APP    = "browser.audioFeeds.handlers.application";
const PREF_AUDIO_FEED_SELECTED_WEB    = "browser.audioFeeds.handlers.webservice";
const PREF_AUDIO_FEED_SELECTED_ACTION = "browser.audioFeeds.handler";
const PREF_AUDIO_FEED_SELECTED_READER = "browser.audioFeeds.handler.default";

// The nsHandlerInfoAction enumeration values in nsIHandlerInfo identify
// the actions the application can take with content of various types.
// But since nsIHandlerInfo doesn't support plugins, there's no value
// identifying the "use plugin" action, so we use this constant instead.
const kActionUsePlugin = 5;

const ICON_URL_APP = AppConstants.platform == "linux" ?
                     "moz-icon://dummy.exe?size=16" :
                     "chrome://browser/skin/preferences/application.png";

// For CSS. Can be one of "ask", "save", "plugin" or "feed". If absent, the icon URL
// was set by us to a custom handler icon and CSS should not try to override it.
const APP_ICON_ATTR_NAME = "appHandlerIcon";

XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");

if (AppConstants.E10S_TESTING_ONLY) {
  XPCOMUtils.defineLazyModuleGetter(this, "UpdateUtils",
                                    "resource://gre/modules/UpdateUtils.jsm");
}

if (AppConstants.MOZ_DEV_EDITION) {
  XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts",
                                    "resource://gre/modules/FxAccounts.jsm");
}

var gMainPane = {
  // The set of types the app knows how to handle.  A hash of HandlerInfoWrapper
  // objects, indexed by type.
  _handledTypes: {},

  // The list of types we can show, sorted by the sort column/direction.
  // An array of HandlerInfoWrapper objects.  We build this list when we first
  // load the data and then rebuild it when users change a pref that affects
  // what types we can show or change the sort column/direction.
  // Note: this isn't necessarily the list of types we *will* show; if the user
  // provides a filter string, we'll only show the subset of types in this list
  // that match that string.
  _visibleTypes: [],

  // A count of the number of times each visible type description appears.
  // We use these counts to determine whether or not to annotate descriptions
  // with their types to distinguish duplicate descriptions from each other.
  // A hash of integer counts, indexed by string description.
  _visibleTypeDescriptionCount: {},


  // Convenience & Performance Shortcuts

  // These get defined by init().
  _brandShortName: null,
  _prefsBundle: null,
  _list: null,
  _filter: null,

  _prefSvc: Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefBranch),

  _mimeSvc: Cc["@mozilla.org/mime;1"].
            getService(Ci.nsIMIMEService),

  _helperAppSvc: Cc["@mozilla.org/uriloader/external-helper-app-service;1"].
                 getService(Ci.nsIExternalHelperAppService),

  _handlerSvc: Cc["@mozilla.org/uriloader/handler-service;1"].
               getService(Ci.nsIHandlerService),

  _ioSvc: Cc["@mozilla.org/network/io-service;1"].
          getService(Ci.nsIIOService),

  /**
   * Initialization of this.
   */
  init() {
    function setEventListener(aId, aEventType, aCallback) {
      document.getElementById(aId)
              .addEventListener(aEventType, aCallback.bind(gMainPane));
    }

    if (AppConstants.HAVE_SHELL_SERVICE) {
      this.updateSetDefaultBrowser();
      if (AppConstants.platform == "win") {
        // In Windows 8 we launch the control panel since it's the only
        // way to get all file type association prefs. So we don't know
        // when the user will select the default.  We refresh here periodically
        // in case the default changes. On other Windows OS's defaults can also
        // be set while the prefs are open.
        let win = Services.wm.getMostRecentWindow("navigator:browser");

        let pollForDefaultBrowser = () => {
          let uri = win.gBrowser.currentURI.spec;

          if ((uri == "about:preferences" || uri == "about:preferences#general") &&
              document.visibilityState == "visible") {
            this.updateSetDefaultBrowser();
          }

          // approximately a "requestIdleInterval"
          window.setTimeout(() => {
            window.requestIdleCallback(pollForDefaultBrowser);
          }, 1000);
        };

        window.setTimeout(() => {
          window.requestIdleCallback(pollForDefaultBrowser);
        }, 1000);
      }
    }

    this.initBrowserContainers();
    this.buildContentProcessCountMenuList();

    let performanceSettingsLink = document.getElementById("performanceSettingsLearnMore");
    let performanceSettingsUrl = Services.urlFormatter.formatURLPref("app.support.baseURL") + "performance";
    performanceSettingsLink.setAttribute("href", performanceSettingsUrl);

    this.updateDefaultPerformanceSettingsPref();

    let defaultPerformancePref =
      document.getElementById("browser.preferences.defaultPerformanceSettings.enabled");
    defaultPerformancePref.addEventListener("change", () => {
      this.updatePerformanceSettingsBox();
    });
    this.updatePerformanceSettingsBox();

    // set up the "use current page" label-changing listener
    this._updateUseCurrentButton();
    window.addEventListener("focus", this._updateUseCurrentButton.bind(this));

    this.updateBrowserStartupLastSession();

    if (AppConstants.platform == "win") {
      // Functionality for "Show tabs in taskbar" on Windows 7 and up.
      try {
        let sysInfo = Cc["@mozilla.org/system-info;1"].
                      getService(Ci.nsIPropertyBag2);
        let ver = parseFloat(sysInfo.getProperty("version"));
        let showTabsInTaskbar = document.getElementById("showTabsInTaskbar");
        showTabsInTaskbar.hidden = ver < 6.1;
      } catch (ex) {}
    }

    // The "closing multiple tabs" and "opening multiple tabs might slow down
    // &brandShortName;" warnings provide options for not showing these
    // warnings again. When the user disabled them, we provide checkboxes to
    // re-enable the warnings.
    if (!TransientPrefs.prefShouldBeVisible("browser.tabs.warnOnClose"))
      document.getElementById("warnCloseMultiple").hidden = true;
    if (!TransientPrefs.prefShouldBeVisible("browser.tabs.warnOnOpen"))
      document.getElementById("warnOpenMany").hidden = true;

    setEventListener("browser.privatebrowsing.autostart", "change",
                     gMainPane.updateBrowserStartupLastSession);
    if (AppConstants.HAVE_SHELL_SERVICE) {
      setEventListener("setDefaultButton", "command",
                       gMainPane.setDefaultBrowser);
    }
    setEventListener("useCurrent", "command",
                     gMainPane.setHomePageToCurrent);
    setEventListener("useBookmark", "command",
                     gMainPane.setHomePageToBookmark);
    setEventListener("restoreDefaultHomePage", "command",
                     gMainPane.restoreDefaultHomePage);
    setEventListener("chooseLanguage", "command",
      gMainPane.showLanguages);
    setEventListener("translationAttributionImage", "click",
      gMainPane.openTranslationProviderAttribution);
    setEventListener("translateButton", "command",
      gMainPane.showTranslationExceptions);
    setEventListener("font.language.group", "change",
      gMainPane._rebuildFonts);
    setEventListener("advancedFonts", "command",
      gMainPane.configureFonts);
    setEventListener("colors", "command",
      gMainPane.configureColors);
    setEventListener("layers.acceleration.disabled", "change",
      gMainPane.updateHardwareAcceleration);
    setEventListener("connectionSettings", "command",
      gMainPane.showConnections);
    setEventListener("browserContainersCheckbox", "command",
      gMainPane.checkBrowserContainers);
    setEventListener("browserContainersSettings", "command",
      gMainPane.showContainerSettings);

    // Initializes the fonts dropdowns displayed in this pane.
    this._rebuildFonts();

    this.updateOnScreenKeyboardVisibility();

    // Show translation preferences if we may:
    const prefName = "browser.translation.ui.show";
    if (Services.prefs.getBoolPref(prefName)) {
      let row = document.getElementById("translationBox");
      row.removeAttribute("hidden");
      // Showing attribution only for Bing Translator.
      Components.utils.import("resource:///modules/translation/Translation.jsm");
      if (Translation.translationEngine == "bing") {
        document.getElementById("bingAttribution").removeAttribute("hidden");
      }
    }

    if (AppConstants.E10S_TESTING_ONLY) {
      setEventListener("e10sAutoStart", "command",
                       gMainPane.enableE10SChange);
      let e10sCheckbox = document.getElementById("e10sAutoStart");

      let e10sPref = document.getElementById("browser.tabs.remote.autostart");
      let e10sTempPref = document.getElementById("e10sTempPref");
      let e10sForceEnable = document.getElementById("e10sForceEnable");

      let preffedOn = e10sPref.value || e10sTempPref.value || e10sForceEnable.value;

      if (preffedOn) {
        // The checkbox is checked if e10s is preffed on and enabled.
        e10sCheckbox.checked = Services.appinfo.browserTabsRemoteAutostart;

        // but if it's force disabled, then the checkbox is disabled.
        e10sCheckbox.disabled = !Services.appinfo.browserTabsRemoteAutostart;
      }
    }

    if (AppConstants.MOZ_DEV_EDITION) {
      let uAppData = OS.Constants.Path.userApplicationDataDir;
      let ignoreSeparateProfile = OS.Path.join(uAppData, "ignore-dev-edition-profile");

      setEventListener("separateProfileMode", "command", gMainPane.separateProfileModeChange);
      let separateProfileModeCheckbox = document.getElementById("separateProfileMode");
      setEventListener("getStarted", "click", gMainPane.onGetStarted);

      OS.File.stat(ignoreSeparateProfile).then(() => separateProfileModeCheckbox.checked = false,
                                               () => separateProfileModeCheckbox.checked = true);

      fxAccounts.getSignedInUser().then(data => {
        document.getElementById("getStarted").selectedIndex = data ? 1 : 0;
      })
      .catch(Cu.reportError);
    }

    // Initialize the Firefox Updates section.
    let version = AppConstants.MOZ_APP_VERSION_DISPLAY;

    // Include the build ID if this is an "a#" (nightly) build
    if (/a\d+$/.test(version)) {
      let buildID = Services.appinfo.appBuildID;
      let year = buildID.slice(0, 4);
      let month = buildID.slice(4, 6);
      let day = buildID.slice(6, 8);
      version += ` (${year}-${month}-${day})`;
    }

    // Append "(32-bit)" or "(64-bit)" build architecture to the version number:
    let bundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");
    let archResource = Services.appinfo.is64Bit
                       ? "aboutDialog.architecture.sixtyFourBit"
                       : "aboutDialog.architecture.thirtyTwoBit";
    let arch = bundle.GetStringFromName(archResource);
    version += ` (${arch})`;

    document.getElementById("version").textContent = version;

    // Show a release notes link if we have a URL.
    let relNotesLink = document.getElementById("releasenotes");
    let relNotesPrefType = Services.prefs.getPrefType("app.releaseNotesURL");
    if (relNotesPrefType != Services.prefs.PREF_INVALID) {
      let relNotesURL = Services.urlFormatter.formatURLPref("app.releaseNotesURL");
      if (relNotesURL != "about:blank") {
        relNotesLink.href = relNotesURL;
        relNotesLink.hidden = false;
      }
    }

    let distroId = Services.prefs.getCharPref("distribution.id", "");
    if (distroId) {
      let distroVersion = Services.prefs.getCharPref("distribution.version");

      let distroIdField = document.getElementById("distributionId");
      distroIdField.value = distroId + " - " + distroVersion;
      distroIdField.hidden = false;

      let distroAbout = Services.prefs.getStringPref("distribution.about", "");
      if (distroAbout) {
        let distroField = document.getElementById("distribution");
        distroField.value = distroAbout;
        distroField.hidden = false;
      }
    }

    if (AppConstants.MOZ_UPDATER) {
      gAppUpdater = new appUpdater();
      let onUnload = () => {
        window.removeEventListener("unload", onUnload);
        Services.prefs.removeObserver("app.update.", this);
      };
      window.addEventListener("unload", onUnload);
      Services.prefs.addObserver("app.update.", this);
      this.updateReadPrefs();
      setEventListener("updateRadioGroup", "command",
                       gMainPane.updateWritePrefs);
      setEventListener("showUpdateHistory", "command",
                       gMainPane.showUpdates);
    }

    // Initilize Application section.
    // Initialize shortcuts to some commonly accessed elements & values.
    this._brandShortName =
      document.getElementById("bundleBrand").getString("brandShortName");
    this._prefsBundle = document.getElementById("bundlePreferences");
    this._list = document.getElementById("handlersView");
    this._filter = document.getElementById("filter");

    // Observe preferences that influence what we display so we can rebuild
    // the view when they change.
    this._prefSvc.addObserver(PREF_SHOW_PLUGINS_IN_LIST, this);
    this._prefSvc.addObserver(PREF_HIDE_PLUGINS_WITHOUT_EXTENSIONS, this);
    this._prefSvc.addObserver(PREF_FEED_SELECTED_APP, this);
    this._prefSvc.addObserver(PREF_FEED_SELECTED_WEB, this);
    this._prefSvc.addObserver(PREF_FEED_SELECTED_ACTION, this);
    this._prefSvc.addObserver(PREF_FEED_SELECTED_READER, this);

    this._prefSvc.addObserver(PREF_VIDEO_FEED_SELECTED_APP, this);
    this._prefSvc.addObserver(PREF_VIDEO_FEED_SELECTED_WEB, this);
    this._prefSvc.addObserver(PREF_VIDEO_FEED_SELECTED_ACTION, this);
    this._prefSvc.addObserver(PREF_VIDEO_FEED_SELECTED_READER, this);

    this._prefSvc.addObserver(PREF_AUDIO_FEED_SELECTED_APP, this);
    this._prefSvc.addObserver(PREF_AUDIO_FEED_SELECTED_WEB, this);
    this._prefSvc.addObserver(PREF_AUDIO_FEED_SELECTED_ACTION, this);
    this._prefSvc.addObserver(PREF_AUDIO_FEED_SELECTED_READER, this);

    setEventListener("focusSearch1", "command", gMainPane.focusFilterBox);
    setEventListener("focusSearch2", "command", gMainPane.focusFilterBox);
    setEventListener("filter", "command", gMainPane.filter);
    setEventListener("handlersView", "select",
      gMainPane.onSelectionChanged);
    setEventListener("typeColumn", "click", gMainPane.sort);
    setEventListener("actionColumn", "click", gMainPane.sort);
    setEventListener("chooseFolder", "command", gMainPane.chooseFolder);
    setEventListener("browser.download.dir", "change", gMainPane.displayDownloadDirPref);

    // Listen for window unload so we can remove our preference observers.
    window.addEventListener("unload", this);

    // Figure out how we should be sorting the list.  We persist sort settings
    // across sessions, so we can't assume the default sort column/direction.
    // XXX should we be using the XUL sort service instead?
    if (document.getElementById("actionColumn").hasAttribute("sortDirection")) {
      this._sortColumn = document.getElementById("actionColumn");
      // The typeColumn element always has a sortDirection attribute,
      // either because it was persisted or because the default value
      // from the xul file was used.  If we are sorting on the other
      // column, we should remove it.
      document.getElementById("typeColumn").removeAttribute("sortDirection");
    } else
      this._sortColumn = document.getElementById("typeColumn");

    // Load the data and build the list of handlers.
    // By doing this in a timeout, we let the preferences dialog resize itself
    // to an appropriate size before we add a bunch of items to the list.
    // Otherwise, if there are many items, and the Applications prefpane
    // is the one that gets displayed when the user first opens the dialog,
    // the dialog might stretch too much in an attempt to fit them all in.
    // XXX Shouldn't we perhaps just set a max-height on the richlistbox?
    var _delayedPaneLoad = function(self) {
      self._loadData();
      self._rebuildVisibleTypes();
      self._sortVisibleTypes();
      self._rebuildView();
    }
    setTimeout(_delayedPaneLoad, 0, this);

    let browserBundle = document.getElementById("browserBundle");
    appendSearchKeywords("browserContainersSettings", [
      browserBundle.getString("userContextPersonal.label"),
      browserBundle.getString("userContextWork.label"),
      browserBundle.getString("userContextBanking.label"),
      browserBundle.getString("userContextShopping.label"),
    ]);

    // Notify observers that the UI is now ready
    Components.classes["@mozilla.org/observer-service;1"]
              .getService(Components.interfaces.nsIObserverService)
              .notifyObservers(window, "main-pane-loaded");
  },

  /**
   * Show the Containers UI depending on the privacy.userContext.ui.enabled pref.
   */
  initBrowserContainers() {
    if (!Services.prefs.getBoolPref("privacy.userContext.ui.enabled")) {
      // The browserContainersGroup element has its own internal padding that
      // is visible even if the browserContainersbox is visible, so hide the whole
      // groupbox if the feature is disabled to prevent a gap in the preferences.
      document.getElementById("browserContainersbox").setAttribute("data-hidden-from-search", "true");
      return;
    }

    let link = document.getElementById("browserContainersLearnMore");
    link.href = Services.urlFormatter.formatURLPref("app.support.baseURL") + "containers";

    document.getElementById("browserContainersbox").hidden = false;

    document.getElementById("browserContainersCheckbox").checked =
      Services.prefs.getBoolPref("privacy.userContext.enabled");
  },

  isE10SEnabled() {
    let e10sEnabled;
    try {
      let e10sStatus = Components.classes["@mozilla.org/supports-PRUint64;1"]
                         .createInstance(Ci.nsISupportsPRUint64);
      let appinfo = Services.appinfo.QueryInterface(Ci.nsIObserver);
      appinfo.observe(e10sStatus, "getE10SBlocked", "");
      e10sEnabled = e10sStatus.data < 2;
    } catch (e) {
      e10sEnabled = false;
    }

    return e10sEnabled;
  },

  enableE10SChange() {
    if (AppConstants.E10S_TESTING_ONLY) {
      let e10sCheckbox = document.getElementById("e10sAutoStart");
      let e10sPref = document.getElementById("browser.tabs.remote.autostart");
      let e10sTempPref = document.getElementById("e10sTempPref");

      let prefsToChange;
      if (e10sCheckbox.checked) {
        // Enabling e10s autostart
        prefsToChange = [e10sPref];
      } else {
        // Disabling e10s autostart
        prefsToChange = [e10sPref];
        if (e10sTempPref.value) {
         prefsToChange.push(e10sTempPref);
        }
      }

      let buttonIndex = confirmRestartPrompt(e10sCheckbox.checked, 0,
                                             true, false);
      if (buttonIndex == CONFIRM_RESTART_PROMPT_RESTART_NOW) {
        for (let prefToChange of prefsToChange) {
          prefToChange.value = e10sCheckbox.checked;
        }

        Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart);
      }

      // Revert the checkbox in case we didn't quit
      e10sCheckbox.checked = e10sPref.value || e10sTempPref.value;
    }
  },

  separateProfileModeChange() {
    if (AppConstants.MOZ_DEV_EDITION) {
      function quitApp() {
        Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestartNotSameProfile);
      }
      function revertCheckbox(error) {
        separateProfileModeCheckbox.checked = !separateProfileModeCheckbox.checked;
        if (error) {
          Cu.reportError("Failed to toggle separate profile mode: " + error);
        }
      }
      function createOrRemoveSpecialDevEditionFile(onSuccess) {
        let uAppData = OS.Constants.Path.userApplicationDataDir;
        let ignoreSeparateProfile = OS.Path.join(uAppData, "ignore-dev-edition-profile");

        if (separateProfileModeCheckbox.checked) {
          OS.File.remove(ignoreSeparateProfile).then(onSuccess, revertCheckbox);
        } else {
          OS.File.writeAtomic(ignoreSeparateProfile, new Uint8Array()).then(onSuccess, revertCheckbox);
        }
      }

      let separateProfileModeCheckbox = document.getElementById("separateProfileMode");
      let button_index = confirmRestartPrompt(separateProfileModeCheckbox.checked,
                                              0, false, true);
      switch (button_index) {
        case CONFIRM_RESTART_PROMPT_CANCEL:
          revertCheckbox();
          return;
        case CONFIRM_RESTART_PROMPT_RESTART_NOW:
          const Cc = Components.classes, Ci = Components.interfaces;
          let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"]
                             .createInstance(Ci.nsISupportsPRBool);
          Services.obs.notifyObservers(cancelQuit, "quit-application-requested",
                                        "restart");
          if (!cancelQuit.data) {
            createOrRemoveSpecialDevEditionFile(quitApp);
            return;
          }

          // Revert the checkbox in case we didn't quit
          revertCheckbox();
          return;
        case CONFIRM_RESTART_PROMPT_RESTART_LATER:
          createOrRemoveSpecialDevEditionFile();
      }
    }
  },

  onGetStarted(aEvent) {
    if (AppConstants.MOZ_DEV_EDITION) {
      const Cc = Components.classes, Ci = Components.interfaces;
      let wm = Cc["@mozilla.org/appshell/window-mediator;1"]
                  .getService(Ci.nsIWindowMediator);
      let win = wm.getMostRecentWindow("navigator:browser");

      fxAccounts.getSignedInUser().then(data => {
        if (win) {
          if (data) {
            // We have a user, open Sync preferences in the same tab
            win.openUILinkIn("about:preferences#sync", "current");
            return;
          }
          let accountsTab = win.gBrowser.addTab("about:accounts?action=signin&entrypoint=dev-edition-setup");
          win.gBrowser.selectedTab = accountsTab;
        }
      });
    }
  },

  // HOME PAGE

  /*
   * Preferences:
   *
   * browser.startup.homepage
   * - the user's home page, as a string; if the home page is a set of tabs,
   *   this will be those URLs separated by the pipe character "|"
   * browser.startup.page
   * - what page(s) to show when the user starts the application, as an integer:
   *
   *     0: a blank page
   *     1: the home page (as set by the browser.startup.homepage pref)
   *     2: the last page the user visited (DEPRECATED)
   *     3: windows and tabs from the last session (a.k.a. session restore)
   *
   *   The deprecated option is not exposed in UI; however, if the user has it
   *   selected and doesn't change the UI for this preference, the deprecated
   *   option is preserved.
   */

  syncFromHomePref() {
    let homePref = document.getElementById("browser.startup.homepage");

    // If the pref is set to about:home or about:newtab, set the value to ""
    // to show the placeholder text (about:home title) rather than
    // exposing those URLs to users.
    let defaultBranch = Services.prefs.getDefaultBranch("");
    let defaultValue = defaultBranch.getComplexValue("browser.startup.homepage",
                                                     Ci.nsIPrefLocalizedString).data;
    let currentValue = homePref.value.toLowerCase();
    if (currentValue == "about:home" ||
        (currentValue == defaultValue && currentValue == "about:newtab")) {
      return "";
    }

    // If the pref is actually "", show about:blank.  The actual home page
    // loading code treats them the same, and we don't want the placeholder text
    // to be shown.
    if (homePref.value == "")
      return "about:blank";

    // Otherwise, show the actual pref value.
    return undefined;
  },

  syncToHomePref(value) {
    // If the value is "", use about:home.
    if (value == "")
      return "about:home";

    // Otherwise, use the actual textbox value.
    return undefined;
  },

  /**
   * Sets the home page to the current displayed page (or frontmost tab, if the
   * most recent browser window contains multiple tabs), updating preference
   * window UI to reflect this.
   */
  setHomePageToCurrent() {
    let homePage = document.getElementById("browser.startup.homepage");
    let tabs = this._getTabsForHomePage();
    function getTabURI(t) {
      return t.linkedBrowser.currentURI.spec;
    }

    // FIXME Bug 244192: using dangerous "|" joiner!
    if (tabs.length)
      homePage.value = tabs.map(getTabURI).join("|");
  },

  /**
   * Displays a dialog in which the user can select a bookmark to use as home
   * page.  If the user selects a bookmark, that bookmark's name is displayed in
   * UI and the bookmark's address is stored to the home page preference.
   */
  setHomePageToBookmark() {
    var rv = { urls: null, names: null };
    gSubDialog.open("chrome://browser/content/preferences/selectBookmark.xul",
                    "resizable=yes, modal=yes", rv,
                    this._setHomePageToBookmarkClosed.bind(this, rv));
  },

  _setHomePageToBookmarkClosed(rv, aEvent) {
    if (aEvent.detail.button != "accept")
      return;
    if (rv.urls && rv.names) {
      var homePage = document.getElementById("browser.startup.homepage");

      // XXX still using dangerous "|" joiner!
      homePage.value = rv.urls.join("|");
    }
  },

  /**
   * Switches the "Use Current Page" button between its singular and plural
   * forms.
   */
  _updateUseCurrentButton() {
    let useCurrent = document.getElementById("useCurrent");


    let tabs = this._getTabsForHomePage();

    if (tabs.length > 1)
      useCurrent.label = useCurrent.getAttribute("label2");
    else
      useCurrent.label = useCurrent.getAttribute("label1");

    // In this case, the button's disabled state is set by preferences.xml.
    let prefName = "pref.browser.homepage.disable_button.current_page";
    if (document.getElementById(prefName).locked)
      return;

    useCurrent.disabled = !tabs.length
  },

  _getTabsForHomePage() {
    var win;
    var tabs = [];

    const Cc = Components.classes, Ci = Components.interfaces;
    var wm = Cc["@mozilla.org/appshell/window-mediator;1"]
                .getService(Ci.nsIWindowMediator);
    win = wm.getMostRecentWindow("navigator:browser");

    if (win && win.document.documentElement
                  .getAttribute("windowtype") == "navigator:browser") {
      // We should only include visible & non-pinned tabs

      tabs = win.gBrowser.visibleTabs.slice(win.gBrowser._numPinnedTabs);
      tabs = tabs.filter(this.isNotAboutPreferences);
    }

    return tabs;
  },

  /**
   * Check to see if a tab is not about:preferences
   */
  isNotAboutPreferences(aElement, aIndex, aArray) {
    return !aElement.linkedBrowser.currentURI.spec.startsWith("about:preferences");
  },

  /**
   * Restores the default home page as the user's home page.
   */
  restoreDefaultHomePage() {
    var homePage = document.getElementById("browser.startup.homepage");
    homePage.value = homePage.defaultValue;
  },

  /**
   * Utility function to enable/disable the button specified by aButtonID based
   * on the value of the Boolean preference specified by aPreferenceID.
   */
  updateButtons(aButtonID, aPreferenceID) {
    var button = document.getElementById(aButtonID);
    var preference = document.getElementById(aPreferenceID);
    button.disabled = preference.value != true;
    return undefined;
  },

  /**
   * Hide/show the "Show my windows and tabs from last time" option based
   * on the value of the browser.privatebrowsing.autostart pref.
   */
  updateBrowserStartupLastSession() {
    let pbAutoStartPref = document.getElementById("browser.privatebrowsing.autostart");
    let startupPref = document.getElementById("browser.startup.page");
    let menu = document.getElementById("browserStartupPage");
    let option = document.getElementById("browserStartupLastSession");
    if (pbAutoStartPref.value) {
      option.setAttribute("disabled", "true");
      if (option.selected) {
        menu.selectedItem = document.getElementById("browserStartupHomePage");
      }
    } else {
      option.removeAttribute("disabled");
      startupPref.updateElements(); // select the correct index in the startup menulist
    }
  },

  // TABS

  /*
   * Preferences:
   *
   * browser.link.open_newwindow - int
   *   Determines where links targeting new windows should open.
   *   Values:
   *     1 - Open in the current window or tab.
   *     2 - Open in a new window.
   *     3 - Open in a new tab in the most recent window.
   * browser.tabs.loadInBackground - bool
   *   True - Whether browser should switch to a new tab opened from a link.
   * browser.tabs.warnOnClose - bool
   *   True - If when closing a window with multiple tabs the user is warned and
   *          allowed to cancel the action, false to just close the window.
   * browser.tabs.warnOnOpen - bool
   *   True - Whether the user should be warned when trying to open a lot of
   *          tabs at once (e.g. a large folder of bookmarks), allowing to
   *          cancel the action.
   * browser.taskbar.previews.enable - bool
   *   True - Tabs are to be shown in Windows 7 taskbar.
   *   False - Only the window is to be shown in Windows 7 taskbar.
   */

  /**
   * Determines where a link which opens a new window will open.
   *
   * @returns |true| if such links should be opened in new tabs
   */
  readLinkTarget() {
    var openNewWindow = document.getElementById("browser.link.open_newwindow");
    return openNewWindow.value != 2;
  },

  /**
   * Determines where a link which opens a new window will open.
   *
   * @returns 2 if such links should be opened in new windows,
   *          3 if such links should be opened in new tabs
   */
  writeLinkTarget() {
    var linkTargeting = document.getElementById("linkTargeting");
    return linkTargeting.checked ? 3 : 2;
  },
  /*
   * Preferences:
   *
   * browser.shell.checkDefault
   * - true if a default-browser check (and prompt to make it so if necessary)
   *   occurs at startup, false otherwise
   */

  /**
   * Show button for setting browser as default browser or information that
   * browser is already the default browser.
   */
  updateSetDefaultBrowser() {
    if (AppConstants.HAVE_SHELL_SERVICE) {
      let shellSvc = getShellService();
      let defaultBrowserBox = document.getElementById("defaultBrowserBox");
      if (!shellSvc) {
        defaultBrowserBox.hidden = true;
        return;
      }
      let setDefaultPane = document.getElementById("setDefaultPane");
      let isDefault = shellSvc.isDefaultBrowser(false, true);
      setDefaultPane.selectedIndex = isDefault ? 1 : 0;
      let alwaysCheck = document.getElementById("alwaysCheckDefault");
      alwaysCheck.disabled = alwaysCheck.disabled ||
                             isDefault && alwaysCheck.checked;
    }
  },

  /**
   * Set browser as the operating system default browser.
   */
  setDefaultBrowser() {
    if (AppConstants.HAVE_SHELL_SERVICE) {
      let alwaysCheckPref = document.getElementById("browser.shell.checkDefaultBrowser");
      alwaysCheckPref.value = true;

      let shellSvc = getShellService();
      if (!shellSvc)
        return;
      try {
        shellSvc.setDefaultBrowser(true, false);
      } catch (ex) {
        Cu.reportError(ex);
        return;
      }

      let selectedIndex = shellSvc.isDefaultBrowser(false, true) ? 1 : 0;
      document.getElementById("setDefaultPane").selectedIndex = selectedIndex;
    }
  },

  /**
   * Shows a dialog in which the preferred language for web content may be set.
   */
  showLanguages() {
    gSubDialog.open("chrome://browser/content/preferences/languages.xul");
  },

  /**
   * Displays the translation exceptions dialog where specific site and language
   * translation preferences can be set.
   */
  showTranslationExceptions() {
    gSubDialog.open("chrome://browser/content/preferences/translation.xul");
  },

  openTranslationProviderAttribution() {
    Components.utils.import("resource:///modules/translation/Translation.jsm");
    Translation.openProviderAttribution();
  },

  /**
   * Displays the fonts dialog, where web page font names and sizes can be
   * configured.
   */
  configureFonts() {
    gSubDialog.open("chrome://browser/content/preferences/fonts.xul", "resizable=no");
  },

  /**
   * Displays the colors dialog, where default web page/link/etc. colors can be
   * configured.
   */
  configureColors() {
    gSubDialog.open("chrome://browser/content/preferences/colors.xul", "resizable=no");
  },

  // NETWORK
  /**
   * Displays a dialog in which proxy settings may be changed.
   */
  showConnections() {
    gSubDialog.open("chrome://browser/content/preferences/connection.xul");
  },

  checkBrowserContainers(event) {
    let checkbox = document.getElementById("browserContainersCheckbox");
    if (checkbox.checked) {
      Services.prefs.setBoolPref("privacy.userContext.enabled", true);
      return;
    }

    let count = ContextualIdentityService.countContainerTabs();
    if (count == 0) {
      Services.prefs.setBoolPref("privacy.userContext.enabled", false);
      return;
    }

    let bundlePreferences = document.getElementById("bundlePreferences");

    let title = bundlePreferences.getString("disableContainersAlertTitle");
    let message = PluralForm.get(count, bundlePreferences.getString("disableContainersMsg"))
                            .replace("#S", count)
    let okButton = PluralForm.get(count, bundlePreferences.getString("disableContainersOkButton"))
                             .replace("#S", count)
    let cancelButton = bundlePreferences.getString("disableContainersButton2");

    let buttonFlags = (Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_0) +
                      (Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_1);

    let rv = Services.prompt.confirmEx(window, title, message, buttonFlags,
                                       okButton, cancelButton, null, null, {});
    if (rv == 0) {
      ContextualIdentityService.closeContainerTabs();
      Services.prefs.setBoolPref("privacy.userContext.enabled", false);
      return;
    }

    checkbox.checked = true;
  },

  /**
   * Displays container panel for customising and adding containers.
   */
  showContainerSettings() {
    gotoPref("containers");
  },

  /**
   * ui.osk.enabled
   * - when set to true, subject to other conditions, we may sometimes invoke
   *   an on-screen keyboard when a text input is focused.
   *   (Currently Windows-only, and depending on prefs, may be Windows-8-only)
   */
  updateOnScreenKeyboardVisibility() {
    if (AppConstants.platform == "win") {
      let minVersion = Services.prefs.getBoolPref("ui.osk.require_win10") ? 10 : 6.2;
      if (Services.vc.compare(Services.sysinfo.getProperty("version"), minVersion) >= 0) {
        document.getElementById("useOnScreenKeyboard").hidden = false;
      }
    }
  },

  updateHardwareAcceleration() {
    // Placeholder for restart on change
  },

  // FONTS

  /**
   * Populates the default font list in UI.
   */
  _rebuildFonts() {
    var preferences = document.getElementById("mainPreferences");
    // Ensure preferences are "visible" to ensure bindings work.
    preferences.hidden = false;
    // Force flush:
    preferences.clientHeight;
    var langGroupPref = document.getElementById("font.language.group");
    this._selectDefaultLanguageGroup(langGroupPref.value,
                                     this._readDefaultFontTypeForLanguage(langGroupPref.value) == "serif");
  },

  /**
   * Returns the type of the current default font for the language denoted by
   * aLanguageGroup.
   */
  _readDefaultFontTypeForLanguage(aLanguageGroup) {
    const kDefaultFontType = "font.default.%LANG%";
    var defaultFontTypePref = kDefaultFontType.replace(/%LANG%/, aLanguageGroup);
    var preference = document.getElementById(defaultFontTypePref);
    if (!preference) {
      preference = document.createElement("preference");
      preference.id = defaultFontTypePref;
      preference.setAttribute("name", defaultFontTypePref);
      preference.setAttribute("type", "string");
      preference.setAttribute("onchange", "gMainPane._rebuildFonts();");
      document.getElementById("mainPreferences").appendChild(preference);
    }
    return preference.value;
  },

  _selectDefaultLanguageGroup(aLanguageGroup, aIsSerif) {
    const kFontNameFmtSerif         = "font.name.serif.%LANG%";
    const kFontNameFmtSansSerif     = "font.name.sans-serif.%LANG%";
    const kFontNameListFmtSerif     = "font.name-list.serif.%LANG%";
    const kFontNameListFmtSansSerif = "font.name-list.sans-serif.%LANG%";
    const kFontSizeFmtVariable      = "font.size.variable.%LANG%";

    var preferences = document.getElementById("mainPreferences");
    var prefs = [{ format: aIsSerif ? kFontNameFmtSerif : kFontNameFmtSansSerif,
                   type: "fontname",
                   element: "defaultFont",
                   fonttype: aIsSerif ? "serif" : "sans-serif" },
                 { format: aIsSerif ? kFontNameListFmtSerif : kFontNameListFmtSansSerif,
                   type: "unichar",
                   element: null,
                   fonttype: aIsSerif ? "serif" : "sans-serif" },
                 { format: kFontSizeFmtVariable,
                   type: "int",
                   element: "defaultFontSize",
                   fonttype: null }];
    for (var i = 0; i < prefs.length; ++i) {
      var preference = document.getElementById(prefs[i].format.replace(/%LANG%/, aLanguageGroup));
      if (!preference) {
        preference = document.createElement("preference");
        var name = prefs[i].format.replace(/%LANG%/, aLanguageGroup);
        preference.id = name;
        preference.setAttribute("name", name);
        preference.setAttribute("type", prefs[i].type);
        preferences.appendChild(preference);
      }

      if (!prefs[i].element)
        continue;

      var element = document.getElementById(prefs[i].element);
      if (element) {
        element.setAttribute("preference", preference.id);

        if (prefs[i].fonttype)
          FontBuilder.buildFontList(aLanguageGroup, prefs[i].fonttype, element);

        preference.setElementValue(element);
      }
    }
  },

  /**
   * Stores the original value of the spellchecking preference to enable proper
   * restoration if unchanged (since we're mapping a tristate onto a checkbox).
   */
  _storedSpellCheck: 0,

  /**
   * Returns true if any spellchecking is enabled and false otherwise, caching
   * the current value to enable proper pref restoration if the checkbox is
   * never changed.
   *
   * layout.spellcheckDefault
   * - an integer:
   *     0  disables spellchecking
   *     1  enables spellchecking, but only for multiline text fields
   *     2  enables spellchecking for all text fields
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

  updateDefaultPerformanceSettingsPref() {
    let defaultPerformancePref =
      document.getElementById("browser.preferences.defaultPerformanceSettings.enabled");
    let processCountPref = document.getElementById("dom.ipc.processCount");
    let accelerationPref = document.getElementById("layers.acceleration.disabled");
    if (processCountPref.value != processCountPref.defaultValue ||
        accelerationPref.value != accelerationPref.defaultValue) {
      defaultPerformancePref.value = false;
    }
  },

  updatePerformanceSettingsBox() {
    let defaultPerformancePref =
      document.getElementById("browser.preferences.defaultPerformanceSettings.enabled");
    let performanceSettings = document.getElementById("performanceSettings");
    if (defaultPerformancePref.value) {
      let processCountPref = document.getElementById("dom.ipc.processCount");
      let accelerationPref = document.getElementById("layers.acceleration.disabled");
      processCountPref.value = processCountPref.defaultValue;
      accelerationPref.value = accelerationPref.defaultValue;
      performanceSettings.hidden = true;
    } else {
      performanceSettings.hidden = false;
    }
  },

  buildContentProcessCountMenuList() {
    if (gMainPane.isE10SEnabled()) {
      let processCountPref = document.getElementById("dom.ipc.processCount");
      let bundlePreferences = document.getElementById("bundlePreferences");
      let label = bundlePreferences.getFormattedString("defaultContentProcessCount",
        [processCountPref.defaultValue]);
      let contentProcessCount =
        document.querySelector(`#contentProcessCount > menupopup >
                                menuitem[value="${processCountPref.defaultValue}"]`);
      contentProcessCount.label = label;

      document.getElementById("limitContentProcess").disabled = false;
      document.getElementById("contentProcessCount").disabled = false;
      document.getElementById("contentProcessCountEnabledDescription").hidden = false;
      document.getElementById("contentProcessCountDisabledDescription").hidden = true;
    } else {
      document.getElementById("limitContentProcess").disabled = true;
      document.getElementById("contentProcessCount").disabled = true;
      document.getElementById("contentProcessCountEnabledDescription").hidden = true;
      document.getElementById("contentProcessCountDisabledDescription").hidden = false;
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

  destroy() {
    window.removeEventListener("unload", this);
    this._prefSvc.removeObserver(PREF_SHOW_PLUGINS_IN_LIST, this);
    this._prefSvc.removeObserver(PREF_HIDE_PLUGINS_WITHOUT_EXTENSIONS, this);
    this._prefSvc.removeObserver(PREF_FEED_SELECTED_APP, this);
    this._prefSvc.removeObserver(PREF_FEED_SELECTED_WEB, this);
    this._prefSvc.removeObserver(PREF_FEED_SELECTED_ACTION, this);
    this._prefSvc.removeObserver(PREF_FEED_SELECTED_READER, this);

    this._prefSvc.removeObserver(PREF_VIDEO_FEED_SELECTED_APP, this);
    this._prefSvc.removeObserver(PREF_VIDEO_FEED_SELECTED_WEB, this);
    this._prefSvc.removeObserver(PREF_VIDEO_FEED_SELECTED_ACTION, this);
    this._prefSvc.removeObserver(PREF_VIDEO_FEED_SELECTED_READER, this);

    this._prefSvc.removeObserver(PREF_AUDIO_FEED_SELECTED_APP, this);
    this._prefSvc.removeObserver(PREF_AUDIO_FEED_SELECTED_WEB, this);
    this._prefSvc.removeObserver(PREF_AUDIO_FEED_SELECTED_ACTION, this);
    this._prefSvc.removeObserver(PREF_AUDIO_FEED_SELECTED_READER, this);
  },


  // nsISupports

  QueryInterface(aIID) {
    if (aIID.equals(Ci.nsIObserver) ||
        aIID.equals(Ci.nsIDOMEventListener ||
        aIID.equals(Ci.nsISupports)))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  },


  // nsIObserver

  observe(aSubject, aTopic, aData) {
    if (aTopic == "nsPref:changed") {
      // Rebuild the list when there are changes to preferences that influence
      // whether or not to show certain entries in the list.
      if (!this._storingAction) {
        // These two prefs alter the list of visible types, so we have to rebuild
        // that list when they change.
        if (aData == PREF_SHOW_PLUGINS_IN_LIST ||
            aData == PREF_HIDE_PLUGINS_WITHOUT_EXTENSIONS) {
          this._rebuildVisibleTypes();
          this._sortVisibleTypes();
        }

        // All the prefs we observe can affect what we display, so we rebuild
        // the view when any of them changes.
        this._rebuildView();
      }
      if (AppConstants.MOZ_UPDATER) {
        this.updateReadPrefs();
      }
    }
  },


  // nsIDOMEventListener

  handleEvent(aEvent) {
    if (aEvent.type == "unload") {
      this.destroy();
    }
  },


  // Composed Model Construction

  _loadData() {
    this._loadFeedHandler();
    this._loadInternalHandlers();
    this._loadPluginHandlers();
    this._loadApplicationHandlers();
  },

  _loadFeedHandler() {
    this._handledTypes[TYPE_MAYBE_FEED] = feedHandlerInfo;
    feedHandlerInfo.handledOnlyByPlugin = false;

    this._handledTypes[TYPE_MAYBE_VIDEO_FEED] = videoFeedHandlerInfo;
    videoFeedHandlerInfo.handledOnlyByPlugin = false;

    this._handledTypes[TYPE_MAYBE_AUDIO_FEED] = audioFeedHandlerInfo;
    audioFeedHandlerInfo.handledOnlyByPlugin = false;
  },

  /**
   * Load higher level internal handlers so they can be turned on/off in the
   * applications menu.
   */
  _loadInternalHandlers() {
    var internalHandlers = [pdfHandlerInfo];
    for (let internalHandler of internalHandlers) {
      if (internalHandler.enabled) {
        this._handledTypes[internalHandler.type] = internalHandler;
      }
    }
  },

  /**
   * Load the set of handlers defined by plugins.
   *
   * Note: if there's more than one plugin for a given MIME type, we assume
   * the last one is the one that the application will use.  That may not be
   * correct, but it's how we've been doing it for years.
   *
   * Perhaps we should instead query navigator.mimeTypes for the set of types
   * supported by the application and then get the plugin from each MIME type's
   * enabledPlugin property.  But if there's a plugin for a type, we need
   * to know about it even if it isn't enabled, since we're going to give
   * the user an option to enable it.
   *
   * Also note that enabledPlugin does not get updated when
   * plugin.disable_full_page_plugin_for_types changes, so even if we could use
   * enabledPlugin to get the plugin that would be used, we'd still need to
   * check the pref ourselves to find out if it's enabled.
   */
  _loadPluginHandlers() {
    "use strict";

    let mimeTypes = navigator.mimeTypes;

    for (let mimeType of mimeTypes) {
      let handlerInfoWrapper;
      if (mimeType.type in this._handledTypes) {
        handlerInfoWrapper = this._handledTypes[mimeType.type];
      } else {
        let wrappedHandlerInfo =
              this._mimeSvc.getFromTypeAndExtension(mimeType.type, null);
        handlerInfoWrapper = new HandlerInfoWrapper(mimeType.type, wrappedHandlerInfo);
        handlerInfoWrapper.handledOnlyByPlugin = true;
        this._handledTypes[mimeType.type] = handlerInfoWrapper;
      }
      handlerInfoWrapper.pluginName = mimeType.enabledPlugin.name;
    }
  },

  /**
   * Load the set of handlers defined by the application datastore.
   */
  _loadApplicationHandlers() {
    var wrappedHandlerInfos = this._handlerSvc.enumerate();
    while (wrappedHandlerInfos.hasMoreElements()) {
      let wrappedHandlerInfo =
        wrappedHandlerInfos.getNext().QueryInterface(Ci.nsIHandlerInfo);
      let type = wrappedHandlerInfo.type;

      let handlerInfoWrapper;
      if (type in this._handledTypes)
        handlerInfoWrapper = this._handledTypes[type];
      else {
        handlerInfoWrapper = new HandlerInfoWrapper(type, wrappedHandlerInfo);
        this._handledTypes[type] = handlerInfoWrapper;
      }

      handlerInfoWrapper.handledOnlyByPlugin = false;
    }
  },


  // View Construction

  _rebuildVisibleTypes() {
    // Reset the list of visible types and the visible type description counts.
    this._visibleTypes = [];
    this._visibleTypeDescriptionCount = {};

    // Get the preferences that help determine what types to show.
    var showPlugins = this._prefSvc.getBoolPref(PREF_SHOW_PLUGINS_IN_LIST);
    var hidePluginsWithoutExtensions =
      this._prefSvc.getBoolPref(PREF_HIDE_PLUGINS_WITHOUT_EXTENSIONS);

    for (let type in this._handledTypes) {
      let handlerInfo = this._handledTypes[type];

      // Hide plugins without associated extensions if so prefed so we don't
      // show a whole bunch of obscure types handled by plugins on Mac.
      // Note: though protocol types don't have extensions, we still show them;
      // the pref is only meant to be applied to MIME types, since plugins are
      // only associated with MIME types.
      // FIXME: should we also check the "suffixes" property of the plugin?
      // Filed as bug 395135.
      if (hidePluginsWithoutExtensions && handlerInfo.handledOnlyByPlugin &&
          handlerInfo.wrappedHandlerInfo instanceof Ci.nsIMIMEInfo &&
          !handlerInfo.primaryExtension)
        continue;

      // Hide types handled only by plugins if so prefed.
      if (handlerInfo.handledOnlyByPlugin && !showPlugins)
        continue;

      // We couldn't find any reason to exclude the type, so include it.
      this._visibleTypes.push(handlerInfo);

      if (handlerInfo.description in this._visibleTypeDescriptionCount)
        this._visibleTypeDescriptionCount[handlerInfo.description]++;
      else
        this._visibleTypeDescriptionCount[handlerInfo.description] = 1;
    }
  },

  _rebuildView() {
    // Clear the list of entries.
    while (this._list.childNodes.length > 1)
      this._list.removeChild(this._list.lastChild);

    var visibleTypes = this._visibleTypes;

    // If the user is filtering the list, then only show matching types.
    if (this._filter.value)
      visibleTypes = visibleTypes.filter(this._matchesFilter, this);

    for (let visibleType of visibleTypes) {
      let item = document.createElement("richlistitem");
      item.setAttribute("type", visibleType.type);
      item.setAttribute("typeDescription", this._describeType(visibleType));
      if (visibleType.smallIcon)
        item.setAttribute("typeIcon", visibleType.smallIcon);
      item.setAttribute("actionDescription",
                        this._describePreferredAction(visibleType));

      if (!this._setIconClassForPreferredAction(visibleType, item)) {
        item.setAttribute("actionIcon",
                          this._getIconURLForPreferredAction(visibleType));
      }

      this._list.appendChild(item);
    }

    this._selectLastSelectedType();
  },

  _matchesFilter(aType) {
    var filterValue = this._filter.value.toLowerCase();
    return this._describeType(aType).toLowerCase().indexOf(filterValue) != -1 ||
           this._describePreferredAction(aType).toLowerCase().indexOf(filterValue) != -1;
  },

  /**
   * Describe, in a human-readable fashion, the type represented by the given
   * handler info object.  Normally this is just the description provided by
   * the info object, but if more than one object presents the same description,
   * then we annotate the duplicate descriptions with the type itself to help
   * users distinguish between those types.
   *
   * @param aHandlerInfo {nsIHandlerInfo} the type being described
   * @returns {string} a description of the type
   */
  _describeType(aHandlerInfo) {
    if (this._visibleTypeDescriptionCount[aHandlerInfo.description] > 1)
      return this._prefsBundle.getFormattedString("typeDescriptionWithType",
                                                  [aHandlerInfo.description,
                                                   aHandlerInfo.type]);

    return aHandlerInfo.description;
  },

  /**
   * Describe, in a human-readable fashion, the preferred action to take on
   * the type represented by the given handler info object.
   *
   * XXX Should this be part of the HandlerInfoWrapper interface?  It would
   * violate the separation of model and view, but it might make more sense
   * nonetheless (f.e. it would make sortTypes easier).
   *
   * @param aHandlerInfo {nsIHandlerInfo} the type whose preferred action
   *                                      is being described
   * @returns {string} a description of the action
   */
  _describePreferredAction(aHandlerInfo) {
    // alwaysAskBeforeHandling overrides the preferred action, so if that flag
    // is set, then describe that behavior instead.  For most types, this is
    // the "alwaysAsk" string, but for the feed type we show something special.
    if (aHandlerInfo.alwaysAskBeforeHandling) {
      if (isFeedType(aHandlerInfo.type))
        return this._prefsBundle.getFormattedString("previewInApp",
                                                    [this._brandShortName]);
      return this._prefsBundle.getString("alwaysAsk");
    }

    switch (aHandlerInfo.preferredAction) {
      case Ci.nsIHandlerInfo.saveToDisk:
        return this._prefsBundle.getString("saveFile");

      case Ci.nsIHandlerInfo.useHelperApp:
        var preferredApp = aHandlerInfo.preferredApplicationHandler;
        var name;
        if (preferredApp instanceof Ci.nsILocalHandlerApp)
          name = getFileDisplayName(preferredApp.executable);
        else
          name = preferredApp.name;
        return this._prefsBundle.getFormattedString("useApp", [name]);

      case Ci.nsIHandlerInfo.handleInternally:
        // For the feed type, handleInternally means live bookmarks.
        if (isFeedType(aHandlerInfo.type)) {
          return this._prefsBundle.getFormattedString("addLiveBookmarksInApp",
                                                      [this._brandShortName]);
        }

        if (aHandlerInfo instanceof InternalHandlerInfoWrapper) {
          return this._prefsBundle.getFormattedString("previewInApp",
                                                      [this._brandShortName]);
        }

        // For other types, handleInternally looks like either useHelperApp
        // or useSystemDefault depending on whether or not there's a preferred
        // handler app.
        if (this.isValidHandlerApp(aHandlerInfo.preferredApplicationHandler))
          return aHandlerInfo.preferredApplicationHandler.name;

        return aHandlerInfo.defaultDescription;

        // XXX Why don't we say the app will handle the type internally?
        // Is it because the app can't actually do that?  But if that's true,
        // then why would a preferredAction ever get set to this value
        // in the first place?

      case Ci.nsIHandlerInfo.useSystemDefault:
        return this._prefsBundle.getFormattedString("useDefault",
                                                    [aHandlerInfo.defaultDescription]);

      case kActionUsePlugin:
        return this._prefsBundle.getFormattedString("usePluginIn",
                                                    [aHandlerInfo.pluginName,
                                                     this._brandShortName]);
      default:
        throw new Error(`Unexpected preferredAction: ${aHandlerInfo.preferredAction}`);
    }
  },

  _selectLastSelectedType() {
    // If the list is disabled by the pref.downloads.disable_button.edit_actions
    // preference being locked, then don't select the type, as that would cause
    // it to appear selected, with a different background and an actions menu
    // that makes it seem like you can choose an action for the type.
    if (this._list.disabled)
      return;

    var lastSelectedType = this._list.getAttribute("lastSelectedType");
    if (!lastSelectedType)
      return;

    var item = this._list.getElementsByAttribute("type", lastSelectedType)[0];
    if (!item)
      return;

    this._list.selectedItem = item;
  },

  /**
   * Whether or not the given handler app is valid.
   *
   * @param aHandlerApp {nsIHandlerApp} the handler app in question
   *
   * @returns {boolean} whether or not it's valid
   */
  isValidHandlerApp(aHandlerApp) {
    if (!aHandlerApp)
      return false;

    if (aHandlerApp instanceof Ci.nsILocalHandlerApp)
      return this._isValidHandlerExecutable(aHandlerApp.executable);

    if (aHandlerApp instanceof Ci.nsIWebHandlerApp)
      return aHandlerApp.uriTemplate;

    if (aHandlerApp instanceof Ci.nsIWebContentHandlerInfo)
      return aHandlerApp.uri;

    return false;
  },

  _isValidHandlerExecutable(aExecutable) {
    let leafName;
    if (AppConstants.platform == "win") {
      leafName = `${AppConstants.MOZ_APP_NAME}.exe`;
    } else if (AppConstants.platform == "macosx") {
      leafName = AppConstants.MOZ_MACBUNDLE_NAME;
    } else {
      leafName = `${AppConstants.MOZ_APP_NAME}-bin`;
    }
    return aExecutable &&
           aExecutable.exists() &&
           aExecutable.isExecutable() &&
// XXXben - we need to compare this with the running instance executable
//          just don't know how to do that via script...
// XXXmano TBD: can probably add this to nsIShellService
           aExecutable.leafName != leafName;
  },

  /**
   * Rebuild the actions menu for the selected entry.  Gets called by
   * the richlistitem constructor when an entry in the list gets selected.
   */
  rebuildActionsMenu() {
    var typeItem = this._list.selectedItem;
    var handlerInfo = this._handledTypes[typeItem.type];
    var menu =
      document.getAnonymousElementByAttribute(typeItem, "class", "actionsMenu");
    var menuPopup = menu.menupopup;

    // Clear out existing items.
    while (menuPopup.hasChildNodes())
      menuPopup.removeChild(menuPopup.lastChild);

    let internalMenuItem;
    // Add the "Preview in Firefox" option for optional internal handlers.
    if (handlerInfo instanceof InternalHandlerInfoWrapper) {
      internalMenuItem = document.createElement("menuitem");
      internalMenuItem.setAttribute("action", Ci.nsIHandlerInfo.handleInternally);
      let label = this._prefsBundle.getFormattedString("previewInApp",
                                                       [this._brandShortName]);
      internalMenuItem.setAttribute("label", label);
      internalMenuItem.setAttribute("tooltiptext", label);
      internalMenuItem.setAttribute(APP_ICON_ATTR_NAME, "ask");
      menuPopup.appendChild(internalMenuItem);
    }

    {
      var askMenuItem = document.createElement("menuitem");
      askMenuItem.setAttribute("action", Ci.nsIHandlerInfo.alwaysAsk);
      let label;
      if (isFeedType(handlerInfo.type))
        label = this._prefsBundle.getFormattedString("previewInApp",
                                                     [this._brandShortName]);
      else
        label = this._prefsBundle.getString("alwaysAsk");
      askMenuItem.setAttribute("label", label);
      askMenuItem.setAttribute("tooltiptext", label);
      askMenuItem.setAttribute(APP_ICON_ATTR_NAME, "ask");
      menuPopup.appendChild(askMenuItem);
    }

    // Create a menu item for saving to disk.
    // Note: this option isn't available to protocol types, since we don't know
    // what it means to save a URL having a certain scheme to disk, nor is it
    // available to feeds, since the feed code doesn't implement the capability.
    if ((handlerInfo.wrappedHandlerInfo instanceof Ci.nsIMIMEInfo) &&
        !isFeedType(handlerInfo.type)) {
      var saveMenuItem = document.createElement("menuitem");
      saveMenuItem.setAttribute("action", Ci.nsIHandlerInfo.saveToDisk);
      let label = this._prefsBundle.getString("saveFile");
      saveMenuItem.setAttribute("label", label);
      saveMenuItem.setAttribute("tooltiptext", label);
      saveMenuItem.setAttribute(APP_ICON_ATTR_NAME, "save");
      menuPopup.appendChild(saveMenuItem);
    }

    // If this is the feed type, add a Live Bookmarks item.
    if (isFeedType(handlerInfo.type)) {
      internalMenuItem = document.createElement("menuitem");
      internalMenuItem.setAttribute("action", Ci.nsIHandlerInfo.handleInternally);
      let label = this._prefsBundle.getFormattedString("addLiveBookmarksInApp",
                                                       [this._brandShortName]);
      internalMenuItem.setAttribute("label", label);
      internalMenuItem.setAttribute("tooltiptext", label);
      internalMenuItem.setAttribute(APP_ICON_ATTR_NAME, "feed");
      menuPopup.appendChild(internalMenuItem);
    }

    // Add a separator to distinguish these items from the helper app items
    // that follow them.
    let menuseparator = document.createElement("menuseparator");
    menuPopup.appendChild(menuseparator);

    // Create a menu item for the OS default application, if any.
    if (handlerInfo.hasDefaultHandler) {
      var defaultMenuItem = document.createElement("menuitem");
      defaultMenuItem.setAttribute("action", Ci.nsIHandlerInfo.useSystemDefault);
      let label = this._prefsBundle.getFormattedString("useDefault",
                                                       [handlerInfo.defaultDescription]);
      defaultMenuItem.setAttribute("label", label);
      defaultMenuItem.setAttribute("tooltiptext", handlerInfo.defaultDescription);
      defaultMenuItem.setAttribute("image", this._getIconURLForSystemDefault(handlerInfo));

      menuPopup.appendChild(defaultMenuItem);
    }

    // Create menu items for possible handlers.
    let preferredApp = handlerInfo.preferredApplicationHandler;
    let possibleApps = handlerInfo.possibleApplicationHandlers.enumerate();
    var possibleAppMenuItems = [];
    while (possibleApps.hasMoreElements()) {
      let possibleApp = possibleApps.getNext();
      if (!this.isValidHandlerApp(possibleApp))
        continue;

      let menuItem = document.createElement("menuitem");
      menuItem.setAttribute("action", Ci.nsIHandlerInfo.useHelperApp);
      let label;
      if (possibleApp instanceof Ci.nsILocalHandlerApp)
        label = getFileDisplayName(possibleApp.executable);
      else
        label = possibleApp.name;
      label = this._prefsBundle.getFormattedString("useApp", [label]);
      menuItem.setAttribute("label", label);
      menuItem.setAttribute("tooltiptext", label);
      menuItem.setAttribute("image", this._getIconURLForHandlerApp(possibleApp));

      // Attach the handler app object to the menu item so we can use it
      // to make changes to the datastore when the user selects the item.
      menuItem.handlerApp = possibleApp;

      menuPopup.appendChild(menuItem);
      possibleAppMenuItems.push(menuItem);
    }

    // Create a menu item for the plugin.
    if (handlerInfo.pluginName) {
      var pluginMenuItem = document.createElement("menuitem");
      pluginMenuItem.setAttribute("action", kActionUsePlugin);
      let label = this._prefsBundle.getFormattedString("usePluginIn",
                                                       [handlerInfo.pluginName,
                                                        this._brandShortName]);
      pluginMenuItem.setAttribute("label", label);
      pluginMenuItem.setAttribute("tooltiptext", label);
      pluginMenuItem.setAttribute(APP_ICON_ATTR_NAME, "plugin");
      menuPopup.appendChild(pluginMenuItem);
    }

    // Create a menu item for selecting a local application.
    let canOpenWithOtherApp = true;
    if (AppConstants.platform == "win") {
      // On Windows, selecting an application to open another application
      // would be meaningless so we special case executables.
      let executableType = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService)
                                                    .getTypeFromExtension("exe");
      canOpenWithOtherApp = handlerInfo.type != executableType;
    }
    if (canOpenWithOtherApp) {
      let menuItem = document.createElement("menuitem");
      menuItem.className = "choose-app-item";
      menuItem.addEventListener("command", function(e) {
        gMainPane.chooseApp(e);
      });
      let label = this._prefsBundle.getString("useOtherApp");
      menuItem.setAttribute("label", label);
      menuItem.setAttribute("tooltiptext", label);
      menuPopup.appendChild(menuItem);
    }

    // Create a menu item for managing applications.
    if (possibleAppMenuItems.length) {
      let menuItem = document.createElement("menuseparator");
      menuPopup.appendChild(menuItem);
      menuItem = document.createElement("menuitem");
      menuItem.className = "manage-app-item";
      menuItem.addEventListener("command", function(e) {
        gMainPane.manageApp(e);
      });
      menuItem.setAttribute("label", this._prefsBundle.getString("manageApp"));
      menuPopup.appendChild(menuItem);
    }

    // Select the item corresponding to the preferred action.  If the always
    // ask flag is set, it overrides the preferred action.  Otherwise we pick
    // the item identified by the preferred action (when the preferred action
    // is to use a helper app, we have to pick the specific helper app item).
    if (handlerInfo.alwaysAskBeforeHandling)
      menu.selectedItem = askMenuItem;
    else switch (handlerInfo.preferredAction) {
      case Ci.nsIHandlerInfo.handleInternally:
        if (internalMenuItem) {
          menu.selectedItem = internalMenuItem;
        } else {
          Cu.reportError("No menu item defined to set!")
        }
        break;
      case Ci.nsIHandlerInfo.useSystemDefault:
        menu.selectedItem = defaultMenuItem;
        break;
      case Ci.nsIHandlerInfo.useHelperApp:
        if (preferredApp)
          menu.selectedItem =
            possibleAppMenuItems.filter(v => v.handlerApp.equals(preferredApp))[0];
        break;
      case kActionUsePlugin:
        menu.selectedItem = pluginMenuItem;
        break;
      case Ci.nsIHandlerInfo.saveToDisk:
        menu.selectedItem = saveMenuItem;
        break;
    }
  },


  // Sorting & Filtering

  _sortColumn: null,

  /**
   * Sort the list when the user clicks on a column header.
   */
  sort(event) {
    var column = event.target;

    // If the user clicked on a new sort column, remove the direction indicator
    // from the old column.
    if (this._sortColumn && this._sortColumn != column)
      this._sortColumn.removeAttribute("sortDirection");

    this._sortColumn = column;

    // Set (or switch) the sort direction indicator.
    if (column.getAttribute("sortDirection") == "ascending")
      column.setAttribute("sortDirection", "descending");
    else
      column.setAttribute("sortDirection", "ascending");

    this._sortVisibleTypes();
    this._rebuildView();
  },

  /**
   * Sort the list of visible types by the current sort column/direction.
   */
  _sortVisibleTypes() {
    if (!this._sortColumn)
      return;

    var t = this;

    function sortByType(a, b) {
      return t._describeType(a).toLowerCase().
             localeCompare(t._describeType(b).toLowerCase());
    }

    function sortByAction(a, b) {
      return t._describePreferredAction(a).toLowerCase().
             localeCompare(t._describePreferredAction(b).toLowerCase());
    }

    switch (this._sortColumn.getAttribute("value")) {
      case "type":
        this._visibleTypes.sort(sortByType);
        break;
      case "action":
        this._visibleTypes.sort(sortByAction);
        break;
    }

    if (this._sortColumn.getAttribute("sortDirection") == "descending")
      this._visibleTypes.reverse();
  },

  /**
   * Filter the list when the user enters a filter term into the filter field.
   */
  filter() {
    this._rebuildView();
  },

  focusFilterBox() {
    this._filter.focus();
    this._filter.select();
  },


  // Changes

  onSelectAction(aActionItem) {
    this._storingAction = true;

    try {
      this._storeAction(aActionItem);
    } finally {
      this._storingAction = false;
    }
  },

  _storeAction(aActionItem) {
    var typeItem = this._list.selectedItem;
    var handlerInfo = this._handledTypes[typeItem.type];

    let action = parseInt(aActionItem.getAttribute("action"));

    // Set the plugin state if we're enabling or disabling a plugin.
    if (action == kActionUsePlugin)
      handlerInfo.enablePluginType();
    else if (handlerInfo.pluginName && !handlerInfo.isDisabledPluginType)
      handlerInfo.disablePluginType();

    // Set the preferred application handler.
    // We leave the existing preferred app in the list when we set
    // the preferred action to something other than useHelperApp so that
    // legacy datastores that don't have the preferred app in the list
    // of possible apps still include the preferred app in the list of apps
    // the user can choose to handle the type.
    if (action == Ci.nsIHandlerInfo.useHelperApp)
      handlerInfo.preferredApplicationHandler = aActionItem.handlerApp;

    // Set the "always ask" flag.
    if (action == Ci.nsIHandlerInfo.alwaysAsk)
      handlerInfo.alwaysAskBeforeHandling = true;
    else
      handlerInfo.alwaysAskBeforeHandling = false;

    // Set the preferred action.
    handlerInfo.preferredAction = action;

    handlerInfo.store();

    // Make sure the handler info object is flagged to indicate that there is
    // now some user configuration for the type.
    handlerInfo.handledOnlyByPlugin = false;

    // Update the action label and image to reflect the new preferred action.
    typeItem.setAttribute("actionDescription",
                          this._describePreferredAction(handlerInfo));
    if (!this._setIconClassForPreferredAction(handlerInfo, typeItem)) {
      typeItem.setAttribute("actionIcon",
                            this._getIconURLForPreferredAction(handlerInfo));
    }
  },

  manageApp(aEvent) {
    // Don't let the normal "on select action" handler get this event,
    // as we handle it specially ourselves.
    aEvent.stopPropagation();

    var typeItem = this._list.selectedItem;
    var handlerInfo = this._handledTypes[typeItem.type];

    let onComplete = () => {
      // Rebuild the actions menu so that we revert to the previous selection,
      // or "Always ask" if the previous default application has been removed
      this.rebuildActionsMenu();

      // update the richlistitem too. Will be visible when selecting another row
      typeItem.setAttribute("actionDescription",
                            this._describePreferredAction(handlerInfo));
      if (!this._setIconClassForPreferredAction(handlerInfo, typeItem)) {
        typeItem.setAttribute("actionIcon",
                              this._getIconURLForPreferredAction(handlerInfo));
      }
    };

    gSubDialog.open("chrome://browser/content/preferences/applicationManager.xul",
                    "resizable=no", handlerInfo, onComplete);

  },

  chooseApp(aEvent) {
    // Don't let the normal "on select action" handler get this event,
    // as we handle it specially ourselves.
    aEvent.stopPropagation();

    var handlerApp;
    let chooseAppCallback = aHandlerApp => {
      // Rebuild the actions menu whether the user picked an app or canceled.
      // If they picked an app, we want to add the app to the menu and select it.
      // If they canceled, we want to go back to their previous selection.
      this.rebuildActionsMenu();

      // If the user picked a new app from the menu, select it.
      if (aHandlerApp) {
        let typeItem = this._list.selectedItem;
        let actionsMenu =
          document.getAnonymousElementByAttribute(typeItem, "class", "actionsMenu");
        let menuItems = actionsMenu.menupopup.childNodes;
        for (let i = 0; i < menuItems.length; i++) {
          let menuItem = menuItems[i];
          if (menuItem.handlerApp && menuItem.handlerApp.equals(aHandlerApp)) {
            actionsMenu.selectedIndex = i;
            this.onSelectAction(menuItem);
            break;
          }
        }
      }
    };

    if (AppConstants.platform == "win") {
      var params = {};
      var handlerInfo = this._handledTypes[this._list.selectedItem.type];

      if (isFeedType(handlerInfo.type)) {
        // MIME info will be null, create a temp object.
        params.mimeInfo = this._mimeSvc.getFromTypeAndExtension(handlerInfo.type,
                                                   handlerInfo.primaryExtension);
      } else {
        params.mimeInfo = handlerInfo.wrappedHandlerInfo;
      }

      params.title         = this._prefsBundle.getString("fpTitleChooseApp");
      params.description   = handlerInfo.description;
      params.filename      = null;
      params.handlerApp    = null;

      let onAppSelected = () => {
        if (this.isValidHandlerApp(params.handlerApp)) {
          handlerApp = params.handlerApp;

          // Add the app to the type's list of possible handlers.
          handlerInfo.addPossibleApplicationHandler(handlerApp);
        }

        chooseAppCallback(handlerApp);
      };

      gSubDialog.open("chrome://global/content/appPicker.xul",
                      null, params, onAppSelected);
    } else {
      let winTitle = this._prefsBundle.getString("fpTitleChooseApp");
      let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
      let fpCallback = aResult => {
        if (aResult == Ci.nsIFilePicker.returnOK && fp.file &&
            this._isValidHandlerExecutable(fp.file)) {
          handlerApp = Cc["@mozilla.org/uriloader/local-handler-app;1"].
                       createInstance(Ci.nsILocalHandlerApp);
          handlerApp.name = getFileDisplayName(fp.file);
          handlerApp.executable = fp.file;

          // Add the app to the type's list of possible handlers.
          let handler = this._handledTypes[this._list.selectedItem.type];
          handler.addPossibleApplicationHandler(handlerApp);

          chooseAppCallback(handlerApp);
        }
      };

      // Prompt the user to pick an app.  If they pick one, and it's a valid
      // selection, then add it to the list of possible handlers.
      fp.init(window, winTitle, Ci.nsIFilePicker.modeOpen);
      fp.appendFilters(Ci.nsIFilePicker.filterApps);
      fp.open(fpCallback);
    }
  },

  // Mark which item in the list was last selected so we can reselect it
  // when we rebuild the list or when the user returns to the prefpane.
  onSelectionChanged() {
    if (this._list.selectedItem)
      this._list.setAttribute("lastSelectedType",
                              this._list.selectedItem.getAttribute("type"));
  },

  _setIconClassForPreferredAction(aHandlerInfo, aElement) {
    // If this returns true, the attribute that CSS sniffs for was set to something
    // so you shouldn't manually set an icon URI.
    // This removes the existing actionIcon attribute if any, even if returning false.
    aElement.removeAttribute("actionIcon");

    if (aHandlerInfo.alwaysAskBeforeHandling) {
      aElement.setAttribute(APP_ICON_ATTR_NAME, "ask");
      return true;
    }

    switch (aHandlerInfo.preferredAction) {
      case Ci.nsIHandlerInfo.saveToDisk:
        aElement.setAttribute(APP_ICON_ATTR_NAME, "save");
        return true;

      case Ci.nsIHandlerInfo.handleInternally:
        if (isFeedType(aHandlerInfo.type)) {
          aElement.setAttribute(APP_ICON_ATTR_NAME, "feed");
          return true;
        } else if (aHandlerInfo instanceof InternalHandlerInfoWrapper) {
          aElement.setAttribute(APP_ICON_ATTR_NAME, "ask");
          return true;
        }
        break;

      case kActionUsePlugin:
        aElement.setAttribute(APP_ICON_ATTR_NAME, "plugin");
        return true;
    }
    aElement.removeAttribute(APP_ICON_ATTR_NAME);
    return false;
  },

  _getIconURLForPreferredAction(aHandlerInfo) {
    switch (aHandlerInfo.preferredAction) {
      case Ci.nsIHandlerInfo.useSystemDefault:
        return this._getIconURLForSystemDefault(aHandlerInfo);

      case Ci.nsIHandlerInfo.useHelperApp:
        let preferredApp = aHandlerInfo.preferredApplicationHandler;
        if (this.isValidHandlerApp(preferredApp))
          return this._getIconURLForHandlerApp(preferredApp);
        // Explicit fall-through

      // This should never happen, but if preferredAction is set to some weird
      // value, then fall back to the generic application icon.
      default:
        return ICON_URL_APP;
    }
  },

  _getIconURLForHandlerApp(aHandlerApp) {
    if (aHandlerApp instanceof Ci.nsILocalHandlerApp)
      return this._getIconURLForFile(aHandlerApp.executable);

    if (aHandlerApp instanceof Ci.nsIWebHandlerApp)
      return this._getIconURLForWebApp(aHandlerApp.uriTemplate);

    if (aHandlerApp instanceof Ci.nsIWebContentHandlerInfo)
      return this._getIconURLForWebApp(aHandlerApp.uri)

    // We know nothing about other kinds of handler apps.
    return "";
  },

  _getIconURLForFile(aFile) {
    var fph = this._ioSvc.getProtocolHandler("file").
              QueryInterface(Ci.nsIFileProtocolHandler);
    var urlSpec = fph.getURLSpecFromFile(aFile);

    return "moz-icon://" + urlSpec + "?size=16";
  },

  _getIconURLForWebApp(aWebAppURITemplate) {
    var uri = this._ioSvc.newURI(aWebAppURITemplate);

    // Unfortunately we can't use the favicon service to get the favicon,
    // because the service looks in the annotations table for a record with
    // the exact URL we give it, and users won't have such records for URLs
    // they don't visit, and users won't visit the web app's URL template,
    // they'll only visit URLs derived from that template (i.e. with %s
    // in the template replaced by the URL of the content being handled).

    if (/^https?$/.test(uri.scheme) && this._prefSvc.getBoolPref("browser.chrome.favicons"))
      return uri.prePath + "/favicon.ico";

    return "";
  },

  _getIconURLForSystemDefault(aHandlerInfo) {
    // Handler info objects for MIME types on some OSes implement a property bag
    // interface from which we can get an icon for the default app, so if we're
    // dealing with a MIME type on one of those OSes, then try to get the icon.
    if ("wrappedHandlerInfo" in aHandlerInfo) {
      let wrappedHandlerInfo = aHandlerInfo.wrappedHandlerInfo;

      if (wrappedHandlerInfo instanceof Ci.nsIMIMEInfo &&
          wrappedHandlerInfo instanceof Ci.nsIPropertyBag) {
        try {
          let url = wrappedHandlerInfo.getProperty("defaultApplicationIconURL");
          if (url)
            return url + "?size=16";
        } catch (ex) {}
      }
    }

    // If this isn't a MIME type object on an OS that supports retrieving
    // the icon, or if we couldn't retrieve the icon for some other reason,
    // then use a generic icon.
    return ICON_URL_APP;
  },

  // DOWNLOADS

  /*
   * Preferences:
   *
   * browser.download.useDownloadDir - bool
   *   True - Save files directly to the folder configured via the
   *   browser.download.folderList preference.
   *   False - Always ask the user where to save a file and default to
   *   browser.download.lastDir when displaying a folder picker dialog.
   * browser.download.dir - local file handle
   *   A local folder the user may have selected for downloaded files to be
   *   saved. Migration of other browser settings may also set this path.
   *   This folder is enabled when folderList equals 2.
   * browser.download.lastDir - local file handle
   *   May contain the last folder path accessed when the user browsed
   *   via the file save-as dialog. (see contentAreaUtils.js)
   * browser.download.folderList - int
   *   Indicates the location users wish to save downloaded files too.
   *   It is also used to display special file labels when the default
   *   download location is either the Desktop or the Downloads folder.
   *   Values:
   *     0 - The desktop is the default download location.
   *     1 - The system's downloads folder is the default download location.
   *     2 - The default download location is elsewhere as specified in
   *         browser.download.dir.
   * browser.download.downloadDir
   *   deprecated.
   * browser.download.defaultFolder
   *   deprecated.
   */

  /**
   * Enables/disables the folder field and Browse button based on whether a
   * default download directory is being used.
   */
  readUseDownloadDir() {
    var downloadFolder = document.getElementById("downloadFolder");
    var chooseFolder = document.getElementById("chooseFolder");
    var preference = document.getElementById("browser.download.useDownloadDir");
    downloadFolder.disabled = !preference.value || preference.locked;
    chooseFolder.disabled = !preference.value || preference.locked;

    // don't override the preference's value in UI
    return undefined;
  },

  /**
   * Displays a file picker in which the user can choose the location where
   * downloads are automatically saved, updating preferences and UI in
   * response to the choice, if one is made.
   */
  chooseFolder() {
    return this.chooseFolderTask().catch(Components.utils.reportError);
  },
  async chooseFolderTask() {
    let bundlePreferences = document.getElementById("bundlePreferences");
    let title = bundlePreferences.getString("chooseDownloadFolderTitle");
    let folderListPref = document.getElementById("browser.download.folderList");
    let currentDirPref = await this._indexToFolder(folderListPref.value);
    let defDownloads = await this._indexToFolder(1);
    let fp = Components.classes["@mozilla.org/filepicker;1"].
             createInstance(Components.interfaces.nsIFilePicker);

    fp.init(window, title, Components.interfaces.nsIFilePicker.modeGetFolder);
    fp.appendFilters(Components.interfaces.nsIFilePicker.filterAll);
    // First try to open what's currently configured
    if (currentDirPref && currentDirPref.exists()) {
      fp.displayDirectory = currentDirPref;
    } else if (defDownloads && defDownloads.exists()) {
      // Try the system's download dir
      fp.displayDirectory = defDownloads;
    } else {
      // Fall back to Desktop
      fp.displayDirectory = await this._indexToFolder(0);
    }

    let result = await new Promise(resolve => fp.open(resolve));
    if (result != Components.interfaces.nsIFilePicker.returnOK) {
      return;
    }

    let downloadDirPref = document.getElementById("browser.download.dir");
    downloadDirPref.value = fp.file;
    folderListPref.value = await this._folderToIndex(fp.file);
    // Note, the real prefs will not be updated yet, so dnld manager's
    // userDownloadsDirectory may not return the right folder after
    // this code executes. displayDownloadDirPref will be called on
    // the assignment above to update the UI.
  },

  /**
   * Initializes the download folder display settings based on the user's
   * preferences.
   */
  displayDownloadDirPref() {
    this.displayDownloadDirPrefTask().catch(Components.utils.reportError);

    // don't override the preference's value in UI
    return undefined;
  },

  async displayDownloadDirPrefTask() {
    var folderListPref = document.getElementById("browser.download.folderList");
    var bundlePreferences = document.getElementById("bundlePreferences");
    var downloadFolder = document.getElementById("downloadFolder");
    var currentDirPref = document.getElementById("browser.download.dir");

    // Used in defining the correct path to the folder icon.
    var ios = Components.classes["@mozilla.org/network/io-service;1"]
                        .getService(Components.interfaces.nsIIOService);
    var fph = ios.getProtocolHandler("file")
                 .QueryInterface(Components.interfaces.nsIFileProtocolHandler);
    var iconUrlSpec;

    // Display a 'pretty' label or the path in the UI.
    if (folderListPref.value == 2) {
      // Custom path selected and is configured
      downloadFolder.label = this._getDisplayNameOfFile(currentDirPref.value);
      iconUrlSpec = fph.getURLSpecFromFile(currentDirPref.value);
    } else if (folderListPref.value == 1) {
      // 'Downloads'
      downloadFolder.label = bundlePreferences.getString("downloadsFolderName");
      iconUrlSpec = fph.getURLSpecFromFile(await this._indexToFolder(1));
    } else {
      // 'Desktop'
      downloadFolder.label = bundlePreferences.getString("desktopFolderName");
      iconUrlSpec = fph.getURLSpecFromFile(await this._getDownloadsFolder("Desktop"));
    }
    downloadFolder.image = "moz-icon://" + iconUrlSpec + "?size=16";
  },

  /**
   * Returns the textual path of a folder in readable form.
   */
  _getDisplayNameOfFile(aFolder) {
    // TODO: would like to add support for 'Downloads on Macintosh HD'
    //       for OS X users.
    return aFolder ? aFolder.path : "";
  },

  /**
   * Returns the Downloads folder.  If aFolder is "Desktop", then the Downloads
   * folder returned is the desktop folder; otherwise, it is a folder whose name
   * indicates that it is a download folder and whose path is as determined by
   * the XPCOM directory service via the download manager's attribute
   * defaultDownloadsDirectory.
   *
   * @throws if aFolder is not "Desktop" or "Downloads"
   */
  async _getDownloadsFolder(aFolder) {
    switch (aFolder) {
      case "Desktop":
        var fileLoc = Components.classes["@mozilla.org/file/directory_service;1"]
                                    .getService(Components.interfaces.nsIProperties);
        return fileLoc.get("Desk", Components.interfaces.nsILocalFile);
      case "Downloads":
        let downloadsDir = await Downloads.getSystemDownloadsDirectory();
        return new FileUtils.File(downloadsDir);
    }
    throw "ASSERTION FAILED: folder type should be 'Desktop' or 'Downloads'";
  },

  /**
   * Determines the type of the given folder.
   *
   * @param   aFolder
   *          the folder whose type is to be determined
   * @returns integer
   *          0 if aFolder is the Desktop or is unspecified,
   *          1 if aFolder is the Downloads folder,
   *          2 otherwise
   */
  async _folderToIndex(aFolder) {
    if (!aFolder || aFolder.equals(await this._getDownloadsFolder("Desktop")))
      return 0;
    else if (aFolder.equals(await this._getDownloadsFolder("Downloads")))
      return 1;
    return 2;
  },

  /**
   * Converts an integer into the corresponding folder.
   *
   * @param   aIndex
   *          an integer
   * @returns the Desktop folder if aIndex == 0,
   *          the Downloads folder if aIndex == 1,
   *          the folder stored in browser.download.dir
   */
  async _indexToFolder(aIndex) {
    switch (aIndex) {
      case 0:
        return await this._getDownloadsFolder("Desktop");
      case 1:
        return await this._getDownloadsFolder("Downloads");
    }
    var currentDirPref = document.getElementById("browser.download.dir");
    return currentDirPref.value;
  }
};

// Utilities

function getFileDisplayName(file) {
  if (AppConstants.platform == "win") {
    if (file instanceof Ci.nsILocalFileWin) {
      try {
        return file.getVersionInfoField("FileDescription");
      } catch (e) {}
    }
  }
  if (AppConstants.platform == "macosx") {
    if (file instanceof Ci.nsILocalFileMac) {
      try {
        return file.bundleDisplayName;
      } catch (e) {}
    }
  }
  return file.leafName;
}

function getLocalHandlerApp(aFile) {
  var localHandlerApp = Cc["@mozilla.org/uriloader/local-handler-app;1"].
                        createInstance(Ci.nsILocalHandlerApp);
  localHandlerApp.name = getFileDisplayName(aFile);
  localHandlerApp.executable = aFile;

  return localHandlerApp;
}

/**
 * An enumeration of items in a JS array.
 *
 * FIXME: use ArrayConverter once it lands (bug 380839).
 *
 * @constructor
 */
function ArrayEnumerator(aItems) {
  this._index = 0;
  this._contents = aItems;
}

ArrayEnumerator.prototype = {
  _index: 0,

  hasMoreElements() {
    return this._index < this._contents.length;
  },

  getNext() {
    return this._contents[this._index++];
  }
};

function isFeedType(t) {
  return t == TYPE_MAYBE_FEED || t == TYPE_MAYBE_VIDEO_FEED || t == TYPE_MAYBE_AUDIO_FEED;
}

// HandlerInfoWrapper

/**
 * This object wraps nsIHandlerInfo with some additional functionality
 * the Applications prefpane needs to display and allow modification of
 * the list of handled types.
 *
 * We create an instance of this wrapper for each entry we might display
 * in the prefpane, and we compose the instances from various sources,
 * including plugins and the handler service.
 *
 * We don't implement all the original nsIHandlerInfo functionality,
 * just the stuff that the prefpane needs.
 *
 * In theory, all of the custom functionality in this wrapper should get
 * pushed down into nsIHandlerInfo eventually.
 */
function HandlerInfoWrapper(aType, aHandlerInfo) {
  this._type = aType;
  this.wrappedHandlerInfo = aHandlerInfo;
}

HandlerInfoWrapper.prototype = {
  // The wrapped nsIHandlerInfo object.  In general, this object is private,
  // but there are a couple cases where callers access it directly for things
  // we haven't (yet?) implemented, so we make it a public property.
  wrappedHandlerInfo: null,


  // Convenience Utils

  _handlerSvc: Cc["@mozilla.org/uriloader/handler-service;1"].
               getService(Ci.nsIHandlerService),

  _prefSvc: Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefBranch),

  _categoryMgr: Cc["@mozilla.org/categorymanager;1"].
                getService(Ci.nsICategoryManager),

  element(aID) {
    return document.getElementById(aID);
  },


  // nsIHandlerInfo

  // The MIME type or protocol scheme.
  _type: null,
  get type() {
    return this._type;
  },

  get description() {
    if (this.wrappedHandlerInfo.description)
      return this.wrappedHandlerInfo.description;

    if (this.primaryExtension) {
      var extension = this.primaryExtension.toUpperCase();
      return this.element("bundlePreferences").getFormattedString("fileEnding",
                                                                  [extension]);
    }

    return this.type;
  },

  get preferredApplicationHandler() {
    return this.wrappedHandlerInfo.preferredApplicationHandler;
  },

  set preferredApplicationHandler(aNewValue) {
    this.wrappedHandlerInfo.preferredApplicationHandler = aNewValue;

    // Make sure the preferred handler is in the set of possible handlers.
    if (aNewValue)
      this.addPossibleApplicationHandler(aNewValue)
  },

  get possibleApplicationHandlers() {
    return this.wrappedHandlerInfo.possibleApplicationHandlers;
  },

  addPossibleApplicationHandler(aNewHandler) {
    var possibleApps = this.possibleApplicationHandlers.enumerate();
    while (possibleApps.hasMoreElements()) {
      if (possibleApps.getNext().equals(aNewHandler))
        return;
    }
    this.possibleApplicationHandlers.appendElement(aNewHandler);
  },

  removePossibleApplicationHandler(aHandler) {
    var defaultApp = this.preferredApplicationHandler;
    if (defaultApp && aHandler.equals(defaultApp)) {
      // If the app we remove was the default app, we must make sure
      // it won't be used anymore
      this.alwaysAskBeforeHandling = true;
      this.preferredApplicationHandler = null;
    }

    var handlers = this.possibleApplicationHandlers;
    for (var i = 0; i < handlers.length; ++i) {
      var handler = handlers.queryElementAt(i, Ci.nsIHandlerApp);
      if (handler.equals(aHandler)) {
        handlers.removeElementAt(i);
        break;
      }
    }
  },

  get hasDefaultHandler() {
    return this.wrappedHandlerInfo.hasDefaultHandler;
  },

  get defaultDescription() {
    return this.wrappedHandlerInfo.defaultDescription;
  },

  // What to do with content of this type.
  get preferredAction() {
    // If we have an enabled plugin, then the action is to use that plugin.
    if (this.pluginName && !this.isDisabledPluginType)
      return kActionUsePlugin;

    // If the action is to use a helper app, but we don't have a preferred
    // handler app, then switch to using the system default, if any; otherwise
    // fall back to saving to disk, which is the default action in nsMIMEInfo.
    // Note: "save to disk" is an invalid value for protocol info objects,
    // but the alwaysAskBeforeHandling getter will detect that situation
    // and always return true in that case to override this invalid value.
    if (this.wrappedHandlerInfo.preferredAction == Ci.nsIHandlerInfo.useHelperApp &&
        !gMainPane.isValidHandlerApp(this.preferredApplicationHandler)) {
      if (this.wrappedHandlerInfo.hasDefaultHandler)
        return Ci.nsIHandlerInfo.useSystemDefault;
      return Ci.nsIHandlerInfo.saveToDisk;
    }

    return this.wrappedHandlerInfo.preferredAction;
  },

  set preferredAction(aNewValue) {
    // If the action is to use the plugin,
    // we must set the preferred action to "save to disk".
    // But only if it's not currently the preferred action.
    if ((aNewValue == kActionUsePlugin) &&
        (this.preferredAction != Ci.nsIHandlerInfo.saveToDisk)) {
      aNewValue = Ci.nsIHandlerInfo.saveToDisk;
    }

    // We don't modify the preferred action if the new action is to use a plugin
    // because handler info objects don't understand our custom "use plugin"
    // value.  Also, leaving it untouched means that we can automatically revert
    // to the old setting if the user ever removes the plugin.

    if (aNewValue != kActionUsePlugin)
      this.wrappedHandlerInfo.preferredAction = aNewValue;
  },

  get alwaysAskBeforeHandling() {
    // If this type is handled only by a plugin, we can't trust the value
    // in the handler info object, since it'll be a default based on the absence
    // of any user configuration, and the default in that case is to always ask,
    // even though we never ask for content handled by a plugin, so special case
    // plugin-handled types by returning false here.
    if (this.pluginName && this.handledOnlyByPlugin)
      return false;

    // If this is a protocol type and the preferred action is "save to disk",
    // which is invalid for such types, then return true here to override that
    // action.  This could happen when the preferred action is to use a helper
    // app, but the preferredApplicationHandler is invalid, and there isn't
    // a default handler, so the preferredAction getter returns save to disk
    // instead.
    if (!(this.wrappedHandlerInfo instanceof Ci.nsIMIMEInfo) &&
        this.preferredAction == Ci.nsIHandlerInfo.saveToDisk)
      return true;

    return this.wrappedHandlerInfo.alwaysAskBeforeHandling;
  },

  set alwaysAskBeforeHandling(aNewValue) {
    this.wrappedHandlerInfo.alwaysAskBeforeHandling = aNewValue;
  },


  // nsIMIMEInfo

  // The primary file extension associated with this type, if any.
  //
  // XXX Plugin objects contain an array of MimeType objects with "suffixes"
  // properties; if this object has an associated plugin, shouldn't we check
  // those properties for an extension?
  get primaryExtension() {
    try {
      if (this.wrappedHandlerInfo instanceof Ci.nsIMIMEInfo &&
          this.wrappedHandlerInfo.primaryExtension)
        return this.wrappedHandlerInfo.primaryExtension
    } catch (ex) {}

    return null;
  },


  // Plugin Handling

  // A plugin that can handle this type, if any.
  //
  // Note: just because we have one doesn't mean it *will* handle the type.
  // That depends on whether or not the type is in the list of types for which
  // plugin handling is disabled.
  plugin: null,

  // Whether or not this type is only handled by a plugin or is also handled
  // by some user-configured action as specified in the handler info object.
  //
  // Note: we can't just check if there's a handler info object for this type,
  // because OS and user configuration is mixed up in the handler info object,
  // so we always need to retrieve it for the OS info and can't tell whether
  // it represents only OS-default information or user-configured information.
  //
  // FIXME: once handler info records are broken up into OS-provided records
  // and user-configured records, stop using this boolean flag and simply
  // check for the presence of a user-configured record to determine whether
  // or not this type is only handled by a plugin.  Filed as bug 395142.
  handledOnlyByPlugin: undefined,

  get isDisabledPluginType() {
    return this._getDisabledPluginTypes().indexOf(this.type) != -1;
  },

  _getDisabledPluginTypes() {
    var types = "";

    if (this._prefSvc.prefHasUserValue(PREF_DISABLED_PLUGIN_TYPES))
      types = this._prefSvc.getCharPref(PREF_DISABLED_PLUGIN_TYPES);

    // Only split if the string isn't empty so we don't end up with an array
    // containing a single empty string.
    if (types != "")
      return types.split(",");

    return [];
  },

  disablePluginType() {
    var disabledPluginTypes = this._getDisabledPluginTypes();

    if (disabledPluginTypes.indexOf(this.type) == -1)
      disabledPluginTypes.push(this.type);

    this._prefSvc.setCharPref(PREF_DISABLED_PLUGIN_TYPES,
                              disabledPluginTypes.join(","));

    // Update the category manager so existing browser windows update.
    this._categoryMgr.deleteCategoryEntry("Gecko-Content-Viewers",
                                          this.type,
                                          false);
  },

  enablePluginType() {
    var disabledPluginTypes = this._getDisabledPluginTypes();

    var type = this.type;
    disabledPluginTypes = disabledPluginTypes.filter(v => v != type);

    this._prefSvc.setCharPref(PREF_DISABLED_PLUGIN_TYPES,
                              disabledPluginTypes.join(","));

    // Update the category manager so existing browser windows update.
    this._categoryMgr.
      addCategoryEntry("Gecko-Content-Viewers",
                       this.type,
                       "@mozilla.org/content/plugin/document-loader-factory;1",
                       false,
                       true);
  },


  // Storage

  store() {
    this._handlerSvc.store(this.wrappedHandlerInfo);
  },


  // Icons

  get smallIcon() {
    return this._getIcon(16);
  },

  _getIcon(aSize) {
    if (this.primaryExtension)
      return "moz-icon://goat." + this.primaryExtension + "?size=" + aSize;

    if (this.wrappedHandlerInfo instanceof Ci.nsIMIMEInfo)
      return "moz-icon://goat?size=" + aSize + "&contentType=" + this.type;

    // FIXME: consider returning some generic icon when we can't get a URL for
    // one (for example in the case of protocol schemes).  Filed as bug 395141.
    return null;
  }

};


// Feed Handler Info

/**
 * This object implements nsIHandlerInfo for the feed types.  It's a separate
 * object because we currently store handling information for the feed type
 * in a set of preferences rather than the nsIHandlerService-managed datastore.
 *
 * This object inherits from HandlerInfoWrapper in order to get functionality
 * that isn't special to the feed type.
 *
 * XXX Should we inherit from HandlerInfoWrapper?  After all, we override
 * most of that wrapper's properties and methods, and we have to dance around
 * the fact that the wrapper expects to have a wrappedHandlerInfo, which we
 * don't provide.
 */

function FeedHandlerInfo(aMIMEType) {
  HandlerInfoWrapper.call(this, aMIMEType, null);
}

FeedHandlerInfo.prototype = {
  __proto__: HandlerInfoWrapper.prototype,

  // Convenience Utils

  _converterSvc:
    Cc["@mozilla.org/embeddor.implemented/web-content-handler-registrar;1"].
    getService(Ci.nsIWebContentConverterService),

  _shellSvc: AppConstants.HAVE_SHELL_SERVICE ? getShellService() : null,

  // nsIHandlerInfo

  get description() {
    return this.element("bundlePreferences").getString(this._appPrefLabel);
  },

  get preferredApplicationHandler() {
    switch (this.element(this._prefSelectedReader).value) {
      case "client":
        var file = this.element(this._prefSelectedApp).value;
        if (file)
          return getLocalHandlerApp(file);

        return null;

      case "web":
        var uri = this.element(this._prefSelectedWeb).value;
        if (!uri)
          return null;
        return this._converterSvc.getWebContentHandlerByURI(this.type, uri);

      case "bookmarks":
      default:
        // When the pref is set to bookmarks, we handle feeds internally,
        // we don't forward them to a local or web handler app, so there is
        // no preferred handler.
        return null;
    }
  },

  set preferredApplicationHandler(aNewValue) {
    if (aNewValue instanceof Ci.nsILocalHandlerApp) {
      this.element(this._prefSelectedApp).value = aNewValue.executable;
      this.element(this._prefSelectedReader).value = "client";
    } else if (aNewValue instanceof Ci.nsIWebContentHandlerInfo) {
      this.element(this._prefSelectedWeb).value = aNewValue.uri;
      this.element(this._prefSelectedReader).value = "web";
      // Make the web handler be the new "auto handler" for feeds.
      // Note: we don't have to unregister the auto handler when the user picks
      // a non-web handler (local app, Live Bookmarks, etc.) because the service
      // only uses the "auto handler" when the selected reader is a web handler.
      // We also don't have to unregister it when the user turns on "always ask"
      // (i.e. preview in browser), since that also overrides the auto handler.
      this._converterSvc.setAutoHandler(this.type, aNewValue);
    }
  },

  _possibleApplicationHandlers: null,

  get possibleApplicationHandlers() {
    if (this._possibleApplicationHandlers)
      return this._possibleApplicationHandlers;

    // A minimal implementation of nsIMutableArray.  It only supports the two
    // methods its callers invoke, namely appendElement and nsIArray::enumerate.
    this._possibleApplicationHandlers = {
      _inner: [],
      _removed: [],

      QueryInterface(aIID) {
        if (aIID.equals(Ci.nsIMutableArray) ||
            aIID.equals(Ci.nsIArray) ||
            aIID.equals(Ci.nsISupports))
          return this;

        throw Cr.NS_ERROR_NO_INTERFACE;
      },

      get length() {
        return this._inner.length;
      },

      enumerate() {
        return new ArrayEnumerator(this._inner);
      },

      appendElement(aHandlerApp, aWeak) {
        this._inner.push(aHandlerApp);
      },

      removeElementAt(aIndex) {
        this._removed.push(this._inner[aIndex]);
        this._inner.splice(aIndex, 1);
      },

      queryElementAt(aIndex, aInterface) {
        return this._inner[aIndex].QueryInterface(aInterface);
      }
    };

    // Add the selected local app if it's different from the OS default handler.
    // Unlike for other types, we can store only one local app at a time for the
    // feed type, since we store it in a preference that historically stores
    // only a single path.  But we display all the local apps the user chooses
    // while the prefpane is open, only dropping the list when the user closes
    // the prefpane, for maximum usability and consistency with other types.
    var preferredAppFile = this.element(this._prefSelectedApp).value;
    if (preferredAppFile) {
      let preferredApp = getLocalHandlerApp(preferredAppFile);
      let defaultApp = this._defaultApplicationHandler;
      if (!defaultApp || !defaultApp.equals(preferredApp))
        this._possibleApplicationHandlers.appendElement(preferredApp);
    }

    // Add the registered web handlers.  There can be any number of these.
    var webHandlers = this._converterSvc.getContentHandlers(this.type);
    for (let webHandler of webHandlers)
      this._possibleApplicationHandlers.appendElement(webHandler);

    return this._possibleApplicationHandlers;
  },

  __defaultApplicationHandler: undefined,
  get _defaultApplicationHandler() {
    if (typeof this.__defaultApplicationHandler != "undefined")
      return this.__defaultApplicationHandler;

    var defaultFeedReader = null;
    if (AppConstants.HAVE_SHELL_SERVICE) {
      try {
        defaultFeedReader = this._shellSvc.defaultFeedReader;
      } catch (ex) {
        // no default reader or _shellSvc is null
      }
    }

    if (defaultFeedReader) {
      let handlerApp = Cc["@mozilla.org/uriloader/local-handler-app;1"].
                       createInstance(Ci.nsIHandlerApp);
      handlerApp.name = getFileDisplayName(defaultFeedReader);
      handlerApp.QueryInterface(Ci.nsILocalHandlerApp);
      handlerApp.executable = defaultFeedReader;

      this.__defaultApplicationHandler = handlerApp;
    } else {
      this.__defaultApplicationHandler = null;
    }

    return this.__defaultApplicationHandler;
  },

  get hasDefaultHandler() {
    if (AppConstants.HAVE_SHELL_SERVICE) {
      try {
        if (this._shellSvc.defaultFeedReader)
          return true;
      } catch (ex) {
        // no default reader or _shellSvc is null
      }
    }

    return false;
  },

  get defaultDescription() {
    if (this.hasDefaultHandler)
      return this._defaultApplicationHandler.name;

    // Should we instead return null?
    return "";
  },

  // What to do with content of this type.
  get preferredAction() {
    switch (this.element(this._prefSelectedAction).value) {

      case "bookmarks":
        return Ci.nsIHandlerInfo.handleInternally;

      case "reader": {
        let preferredApp = this.preferredApplicationHandler;
        let defaultApp = this._defaultApplicationHandler;

        // If we have a valid preferred app, return useSystemDefault if it's
        // the default app; otherwise return useHelperApp.
        if (gMainPane.isValidHandlerApp(preferredApp)) {
          if (defaultApp && defaultApp.equals(preferredApp))
            return Ci.nsIHandlerInfo.useSystemDefault;

          return Ci.nsIHandlerInfo.useHelperApp;
        }

        // The pref is set to "reader", but we don't have a valid preferred app.
        // What do we do now?  Not sure this is the best option (perhaps we
        // should direct the user to the default app, if any), but for now let's
        // direct the user to live bookmarks.
        return Ci.nsIHandlerInfo.handleInternally;
      }

      // If the action is "ask", then alwaysAskBeforeHandling will override
      // the action, so it doesn't matter what we say it is, it just has to be
      // something that doesn't cause the controller to hide the type.
      case "ask":
      default:
        return Ci.nsIHandlerInfo.handleInternally;
    }
  },

  set preferredAction(aNewValue) {
    switch (aNewValue) {

      case Ci.nsIHandlerInfo.handleInternally:
        this.element(this._prefSelectedReader).value = "bookmarks";
        break;

      case Ci.nsIHandlerInfo.useHelperApp:
        this.element(this._prefSelectedAction).value = "reader";
        // The controller has already set preferredApplicationHandler
        // to the new helper app.
        break;

      case Ci.nsIHandlerInfo.useSystemDefault:
        this.element(this._prefSelectedAction).value = "reader";
        this.preferredApplicationHandler = this._defaultApplicationHandler;
        break;
    }
  },

  get alwaysAskBeforeHandling() {
    return this.element(this._prefSelectedAction).value == "ask";
  },

  set alwaysAskBeforeHandling(aNewValue) {
    if (aNewValue == true)
      this.element(this._prefSelectedAction).value = "ask";
    else
      this.element(this._prefSelectedAction).value = "reader";
  },

  // Whether or not we are currently storing the action selected by the user.
  // We use this to suppress notification-triggered updates to the list when
  // we make changes that may spawn such updates, specifically when we change
  // the action for the feed type, which results in feed preference updates,
  // which spawn "pref changed" notifications that would otherwise cause us
  // to rebuild the view unnecessarily.
  _storingAction: false,


  // nsIMIMEInfo

  get primaryExtension() {
    return "xml";
  },


  // Storage

  // Changes to the preferred action and handler take effect immediately
  // (we write them out to the preferences right as they happen),
  // so we when the controller calls store() after modifying the handlers,
  // the only thing we need to store is the removal of possible handlers
  // XXX Should we hold off on making the changes until this method gets called?
  store() {
    for (let app of this._possibleApplicationHandlers._removed) {
      if (app instanceof Ci.nsILocalHandlerApp) {
        let pref = this.element(PREF_FEED_SELECTED_APP);
        var preferredAppFile = pref.value;
        if (preferredAppFile) {
          let preferredApp = getLocalHandlerApp(preferredAppFile);
          if (app.equals(preferredApp))
            pref.reset();
        }
      } else {
        app.QueryInterface(Ci.nsIWebContentHandlerInfo);
        this._converterSvc.removeContentHandler(app.contentType, app.uri);
      }
    }
    this._possibleApplicationHandlers._removed = [];
  },


  // Icons

  get smallIcon() {
    return this._smallIcon;
  }

};

var feedHandlerInfo = {
  __proto__: new FeedHandlerInfo(TYPE_MAYBE_FEED),
  _prefSelectedApp: PREF_FEED_SELECTED_APP,
  _prefSelectedWeb: PREF_FEED_SELECTED_WEB,
  _prefSelectedAction: PREF_FEED_SELECTED_ACTION,
  _prefSelectedReader: PREF_FEED_SELECTED_READER,
  _smallIcon: "chrome://browser/skin/feeds/feedIcon16.png",
  _appPrefLabel: "webFeed"
}

var videoFeedHandlerInfo = {
  __proto__: new FeedHandlerInfo(TYPE_MAYBE_VIDEO_FEED),
  _prefSelectedApp: PREF_VIDEO_FEED_SELECTED_APP,
  _prefSelectedWeb: PREF_VIDEO_FEED_SELECTED_WEB,
  _prefSelectedAction: PREF_VIDEO_FEED_SELECTED_ACTION,
  _prefSelectedReader: PREF_VIDEO_FEED_SELECTED_READER,
  _smallIcon: "chrome://browser/skin/feeds/videoFeedIcon16.png",
  _appPrefLabel: "videoPodcastFeed"
}

var audioFeedHandlerInfo = {
  __proto__: new FeedHandlerInfo(TYPE_MAYBE_AUDIO_FEED),
  _prefSelectedApp: PREF_AUDIO_FEED_SELECTED_APP,
  _prefSelectedWeb: PREF_AUDIO_FEED_SELECTED_WEB,
  _prefSelectedAction: PREF_AUDIO_FEED_SELECTED_ACTION,
  _prefSelectedReader: PREF_AUDIO_FEED_SELECTED_READER,
  _smallIcon: "chrome://browser/skin/feeds/audioFeedIcon16.png",
  _appPrefLabel: "audioPodcastFeed"
}

/**
 * InternalHandlerInfoWrapper provides a basic mechanism to create an internal
 * mime type handler that can be enabled/disabled in the applications preference
 * menu.
 */
function InternalHandlerInfoWrapper(aMIMEType) {
  var mimeSvc = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
  var handlerInfo = mimeSvc.getFromTypeAndExtension(aMIMEType, null);

  HandlerInfoWrapper.call(this, aMIMEType, handlerInfo);
}

InternalHandlerInfoWrapper.prototype = {
  __proto__: HandlerInfoWrapper.prototype,

  // Override store so we so we can notify any code listening for registration
  // or unregistration of this handler.
  store() {
    HandlerInfoWrapper.prototype.store.call(this);
    Services.obs.notifyObservers(null, this._handlerChanged);
  },

  get enabled() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  get description() {
    return this.element("bundlePreferences").getString(this._appPrefLabel);
  }
};

var pdfHandlerInfo = {
  __proto__: new InternalHandlerInfoWrapper(TYPE_PDF),
  _handlerChanged: TOPIC_PDFJS_HANDLER_CHANGED,
  _appPrefLabel: "portableDocumentFormat",
  get enabled() {
    return !Services.prefs.getBoolPref(PREF_PDFJS_DISABLED);
  },
};
