/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from preferences.js */
/* import-globals-from ../../../../toolkit/mozapps/preferences/fontbuilder.js */

Components.utils.import("resource://gre/modules/Downloads.jsm");
Components.utils.import("resource://gre/modules/FileUtils.jsm");
Components.utils.import("resource://gre/modules/Task.jsm");
Components.utils.import("resource:///modules/ShellService.jsm");
Components.utils.import("resource:///modules/TransientPrefs.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");

if (AppConstants.E10S_TESTING_ONLY) {
  XPCOMUtils.defineLazyModuleGetter(this, "UpdateUtils",
                                    "resource://gre/modules/UpdateUtils.jsm");
}

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");

if (AppConstants.MOZ_DEV_EDITION) {
  XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts",
                                    "resource://gre/modules/FxAccounts.jsm");
}

const ENGINE_FLAVOR = "text/x-moz-search-engine";

var gEngineView = null;

var gMainPane = {
  /**
   * Initialize autocomplete to ensure prefs are in sync.
   */
  _initAutocomplete() {
    Components.classes["@mozilla.org/autocomplete/search;1?name=unifiedcomplete"]
              .getService(Components.interfaces.mozIPlacesAutoComplete);
  },

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
        window.setInterval(this.updateSetDefaultBrowser.bind(this), 1000);
      }
    }

    gEngineView = new EngineView(new EngineStore());
    document.getElementById("engineList").view = gEngineView;
    this.buildDefaultEngineDropDown();

    let addEnginesLink = document.getElementById("addEngines");
    let searchEnginesURL = Services.wm.getMostRecentWindow("navigator:browser")
                                      .BrowserSearch.searchEnginesURL;
    addEnginesLink.setAttribute("href", searchEnginesURL);

    window.addEventListener("click", this);
    window.addEventListener("command", this);
    window.addEventListener("dragstart", this);
    window.addEventListener("keypress", this);
    window.addEventListener("select", this);
    window.addEventListener("blur", this, true);

    Services.obs.addObserver(this, "browser-search-engine-modified", false);
    window.addEventListener("unload", () => {
      Services.obs.removeObserver(this, "browser-search-engine-modified");
    });

    this._initAutocomplete();

    let suggestsPref =
      document.getElementById("browser.search.suggest.enabled");
    suggestsPref.addEventListener("change", () => {
      this.updateSuggestsCheckbox();
    });
    this.updateSuggestsCheckbox();

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

    // Initializes the fonts dropdowns displayed in this pane.
    this._rebuildFonts();

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

    // Notify observers that the UI is now ready
    Components.classes["@mozilla.org/observer-service;1"]
              .getService(Components.interfaces.nsIObserverService)
              .notifyObservers(window, "main-pane-loaded", null);
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

  updateSuggestsCheckbox() {
    let suggestsPref =
      document.getElementById("browser.search.suggest.enabled");
    let permanentPB =
      Services.prefs.getBoolPref("browser.privatebrowsing.autostart");
    let urlbarSuggests = document.getElementById("urlBarSuggestion");
    urlbarSuggests.disabled = !suggestsPref.value || permanentPB;

    let urlbarSuggestsPref =
      document.getElementById("browser.urlbar.suggest.searches");
    urlbarSuggests.checked = urlbarSuggestsPref.value;
    if (urlbarSuggests.disabled) {
      urlbarSuggests.checked = false;
    }

    let permanentPBLabel =
      document.getElementById("urlBarSuggestionPermanentPBLabel");
    permanentPBLabel.hidden = urlbarSuggests.hidden || !permanentPB;
  },

  buildDefaultEngineDropDown() {
    // This is called each time something affects the list of engines.
    let list = document.getElementById("defaultEngine");
    // Set selection to the current default engine.
    let currentEngine = Services.search.currentEngine.name;

    // If the current engine isn't in the list any more, select the first item.
    let engines = gEngineView._engineStore._engines;
    if (!engines.some(e => e.name == currentEngine))
      currentEngine = engines[0].name;

    // Now clean-up and rebuild the list.
    list.removeAllItems();
    gEngineView._engineStore._engines.forEach(e => {
      let item = list.appendItem(e.name);
      item.setAttribute("class", "menuitem-iconic searchengine-menuitem menuitem-with-favicon");
      if (e.iconURI) {
        item.setAttribute("image", e.iconURI.spec);
      }
      item.engine = e;
      if (e.name == currentEngine)
        list.selectedItem = item;
    });
  },

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "click":
        if (aEvent.target.id != "engineChildren" &&
            !aEvent.target.classList.contains("searchEngineAction")) {
          let engineList = document.getElementById("engineList");
          // We don't want to toggle off selection while editing keyword
          // so proceed only when the input field is hidden.
          // We need to check that engineList.view is defined here
          // because the "click" event listener is on <window> and the
          // view might have been destroyed if the pane has been navigated
          // away from.
          if (engineList.inputField.hidden && engineList.view) {
            let selection = engineList.view.selection;
            if (selection.count > 0) {
              selection.toggleSelect(selection.currentIndex);
            }
            engineList.blur();
          }
        }
        break;
      case "command":
        switch (aEvent.target.id) {
          case "":
            if (aEvent.target.parentNode &&
                aEvent.target.parentNode.parentNode &&
                aEvent.target.parentNode.parentNode.id == "defaultEngine") {
              gMainPane.setDefaultEngine();
            }
            break;
          case "restoreDefaultSearchEngines":
            gMainPane.onRestoreDefaults();
            break;
          case "removeEngineButton":
            Services.search.removeEngine(gEngineView.selectedEngine.originalEngine);
            break;
        }
        break;
      case "dragstart":
        if (aEvent.target.id == "engineChildren") {
          onDragEngineStart(aEvent);
        }
        break;
      case "keypress":
        if (aEvent.target.id == "engineList") {
          gMainPane.onTreeKeyPress(aEvent);
        }
        break;
      case "select":
        if (aEvent.target.id == "engineList") {
          gMainPane.onTreeSelect();
        }
        break;
      case "blur":
        if (aEvent.target.id == "engineList" &&
            aEvent.target.inputField == document.getBindingParent(aEvent.originalTarget)) {
          gMainPane.onInputBlur();
        }
        break;
    }
  },

  observe(aEngine, aTopic, aVerb) {
    if (aTopic == "browser-search-engine-modified") {
      aEngine.QueryInterface(Components.interfaces.nsISearchEngine);
      switch (aVerb) {
      case "engine-added":
        gEngineView._engineStore.addEngine(aEngine);
        gEngineView.rowCountChanged(gEngineView.lastIndex, 1);
        gMainPane.buildDefaultEngineDropDown();
        break;
      case "engine-changed":
        gEngineView._engineStore.reloadIcons();
        gEngineView.invalidate();
        break;
      case "engine-removed":
        gMainPane.remove(aEngine);
        break;
      case "engine-current":
        // If the user is going through the drop down using up/down keys, the
        // dropdown may still be open (eg. on Windows) when engine-current is
        // fired, so rebuilding the list unconditionally would get in the way.
        let selectedEngine =
          document.getElementById("defaultEngine").selectedItem.engine;
        if (selectedEngine.name != aEngine.name)
          gMainPane.buildDefaultEngineDropDown();
        break;
      case "engine-default":
        // Not relevant
        break;
      }
    }
  },

  onInputBlur(aEvent) {
    let tree = document.getElementById("engineList");
    if (!tree.hasAttribute("editing"))
      return;

    // Accept input unless discarded.
    let accept = aEvent.charCode != KeyEvent.DOM_VK_ESCAPE;
    tree.stopEditing(accept);
  },

  onTreeSelect() {
    document.getElementById("removeEngineButton").disabled =
      !gEngineView.isEngineSelectedAndRemovable();
  },

  onTreeKeyPress(aEvent) {
    let index = gEngineView.selectedIndex;
    let tree = document.getElementById("engineList");
    if (tree.hasAttribute("editing"))
      return;

    if (aEvent.charCode == KeyEvent.DOM_VK_SPACE) {
      // Space toggles the checkbox.
      let newValue = !gEngineView._engineStore.engines[index].shown;
      gEngineView.setCellValue(index, tree.columns.getFirstColumn(),
                               newValue.toString());
      // Prevent page from scrolling on the space key.
      aEvent.preventDefault();
    } else {
      let isMac = Services.appinfo.OS == "Darwin";
      if ((isMac && aEvent.keyCode == KeyEvent.DOM_VK_RETURN) ||
          (!isMac && aEvent.keyCode == KeyEvent.DOM_VK_F2)) {
        tree.startEditing(index, tree.columns.getLastColumn());
      } else if (aEvent.keyCode == KeyEvent.DOM_VK_DELETE ||
                 (isMac && aEvent.shiftKey &&
                  aEvent.keyCode == KeyEvent.DOM_VK_BACK_SPACE &&
                  gEngineView.isEngineSelectedAndRemovable())) {
        // Delete and Shift+Backspace (Mac) removes selected engine.
        Services.search.removeEngine(gEngineView.selectedEngine.originalEngine);
     }
    }
  },

  onRestoreDefaults() {
    let num = gEngineView._engineStore.restoreDefaultEngines();
    gEngineView.rowCountChanged(0, num);
    gEngineView.invalidate();
  },

  showRestoreDefaults(aEnable) {
    document.getElementById("restoreDefaultSearchEngines").disabled = !aEnable;
  },

  remove(aEngine) {
    let index = gEngineView._engineStore.removeEngine(aEngine);
    gEngineView.rowCountChanged(index, -1);
    gEngineView.invalidate();
    gEngineView.selection.select(Math.min(index, gEngineView.lastIndex));
    gEngineView.ensureRowIsVisible(gEngineView.currentIndex);
    document.getElementById("engineList").focus();
  },

  editKeyword: Task.async(function* (aEngine, aNewKeyword) {
    let keyword = aNewKeyword.trim();
    if (keyword) {
      let eduplicate = false;
      let dupName = "";

      // Check for duplicates in Places keywords.
      let bduplicate = !!(yield PlacesUtils.keywords.fetch(keyword));

      // Check for duplicates in changes we haven't committed yet
      let engines = gEngineView._engineStore.engines;
      for (let engine of engines) {
        if (engine.alias == keyword &&
            engine.name != aEngine.name) {
          eduplicate = true;
          dupName = engine.name;
          break;
        }
      }

      // Notify the user if they have chosen an existing engine/bookmark keyword
      if (eduplicate || bduplicate) {
        let strings = document.getElementById("engineManagerBundle");
        let dtitle = strings.getString("duplicateTitle");
        let bmsg = strings.getString("duplicateBookmarkMsg");
        let emsg = strings.getFormattedString("duplicateEngineMsg", [dupName]);

        Services.prompt.alert(window, dtitle, eduplicate ? emsg : bmsg);
        return false;
      }
    }

    gEngineView._engineStore.changeEngine(aEngine, "alias", keyword);
    gEngineView.invalidate();
    return true;
  }),

  saveOneClickEnginesList() {
    let hiddenList = [];
    for (let engine of gEngineView._engineStore.engines) {
      if (!engine.shown)
        hiddenList.push(engine.name);
    }
    document.getElementById("browser.search.hiddenOneOffs").value =
      hiddenList.join(",");
  },

  setDefaultEngine() {
    Services.search.currentEngine =
      document.getElementById("defaultEngine").selectedItem.engine;
  }
};

function onDragEngineStart(event) {
  var selectedIndex = gEngineView.selectedIndex;
  var tree = document.getElementById("engineList");
  var row = { }, col = { }, child = { };
  tree.treeBoxObject.getCellAt(event.clientX, event.clientY, row, col, child);
  if (selectedIndex >= 0 && !gEngineView.isCheckBox(row.value, col.value)) {
    event.dataTransfer.setData(ENGINE_FLAVOR, selectedIndex.toString());
    event.dataTransfer.effectAllowed = "move";
  }
}


function EngineStore() {
  let pref = document.getElementById("browser.search.hiddenOneOffs").value;
  this.hiddenList = pref ? pref.split(",") : [];

  this._engines = Services.search.getVisibleEngines().map(this._cloneEngine, this);
  this._defaultEngines = Services.search.getDefaultEngines().map(this._cloneEngine, this);

  // check if we need to disable the restore defaults button
  var someHidden = this._defaultEngines.some(e => e.hidden);
  gMainPane.showRestoreDefaults(someHidden);
}
EngineStore.prototype = {
  _engines: null,
  _defaultEngines: null,

  get engines() {
    return this._engines;
  },
  set engines(val) {
    this._engines = val;
    return val;
  },

  _getIndexForEngine(aEngine) {
    return this._engines.indexOf(aEngine);
  },

  _getEngineByName(aName) {
    return this._engines.find(engine => engine.name == aName);
  },

  _cloneEngine(aEngine) {
    var clonedObj = {};
    for (var i in aEngine)
      clonedObj[i] = aEngine[i];
    clonedObj.originalEngine = aEngine;
    clonedObj.shown = this.hiddenList.indexOf(clonedObj.name) == -1;
    return clonedObj;
  },

  // Callback for Array's some(). A thisObj must be passed to some()
  _isSameEngine(aEngineClone) {
    return aEngineClone.originalEngine == this.originalEngine;
  },

  addEngine(aEngine) {
    this._engines.push(this._cloneEngine(aEngine));
  },

  moveEngine(aEngine, aNewIndex) {
    if (aNewIndex < 0 || aNewIndex > this._engines.length - 1)
      throw new Error("ES_moveEngine: invalid aNewIndex!");
    var index = this._getIndexForEngine(aEngine);
    if (index == -1)
      throw new Error("ES_moveEngine: invalid engine?");

    if (index == aNewIndex)
      return; // nothing to do

    // Move the engine in our internal store
    var removedEngine = this._engines.splice(index, 1)[0];
    this._engines.splice(aNewIndex, 0, removedEngine);

    Services.search.moveEngine(aEngine.originalEngine, aNewIndex);
  },

  removeEngine(aEngine) {
    if (this._engines.length == 1) {
      throw new Error("Cannot remove last engine!");
    }

    let engineName = aEngine.name;
    let index = this._engines.findIndex(element => element.name == engineName);

    if (index == -1)
      throw new Error("invalid engine?");

    let removedEngine = this._engines.splice(index, 1)[0];

    if (this._defaultEngines.some(this._isSameEngine, removedEngine))
      gMainPane.showRestoreDefaults(true);
    gMainPane.buildDefaultEngineDropDown();
    return index;
  },

  restoreDefaultEngines() {
    var added = 0;

    for (var i = 0; i < this._defaultEngines.length; ++i) {
      var e = this._defaultEngines[i];

      // If the engine is already in the list, just move it.
      if (this._engines.some(this._isSameEngine, e)) {
        this.moveEngine(this._getEngineByName(e.name), i);
      } else {
        // Otherwise, add it back to our internal store

        // The search service removes the alias when an engine is hidden,
        // so clear any alias we may have cached before unhiding the engine.
        e.alias = "";

        this._engines.splice(i, 0, e);
        let engine = e.originalEngine;
        engine.hidden = false;
        Services.search.moveEngine(engine, i);
        added++;
      }
    }
    Services.search.resetToOriginalDefaultEngine();
    gMainPane.showRestoreDefaults(false);
    gMainPane.buildDefaultEngineDropDown();
    return added;
  },

  changeEngine(aEngine, aProp, aNewValue) {
    var index = this._getIndexForEngine(aEngine);
    if (index == -1)
      throw new Error("invalid engine?");

    this._engines[index][aProp] = aNewValue;
    aEngine.originalEngine[aProp] = aNewValue;
  },

  reloadIcons() {
    this._engines.forEach(function(e) {
      e.uri = e.originalEngine.uri;
    });
  }
};

function EngineView(aEngineStore) {
  this._engineStore = aEngineStore;
}
EngineView.prototype = {
  _engineStore: null,
  tree: null,

  get lastIndex() {
    return this.rowCount - 1;
  },
  get selectedIndex() {
    var seln = this.selection;
    if (seln.getRangeCount() > 0) {
      var min = {};
      seln.getRangeAt(0, min, {});
      return min.value;
    }
    return -1;
  },
  get selectedEngine() {
    return this._engineStore.engines[this.selectedIndex];
  },

  // Helpers
  rowCountChanged(index, count) {
    this.tree.rowCountChanged(index, count);
  },

  invalidate() {
    this.tree.invalidate();
  },

  ensureRowIsVisible(index) {
    this.tree.ensureRowIsVisible(index);
  },

  getSourceIndexFromDrag(dataTransfer) {
    return parseInt(dataTransfer.getData(ENGINE_FLAVOR));
  },

  isCheckBox(index, column) {
    return column.id == "engineShown";
  },

  isEngineSelectedAndRemovable() {
    return this.selectedIndex != -1 && this.lastIndex != 0;
  },

  // nsITreeView
  get rowCount() {
    return this._engineStore.engines.length;
  },

  getImageSrc(index, column) {
    if (column.id == "engineName") {
      if (this._engineStore.engines[index].iconURI)
        return this._engineStore.engines[index].iconURI.spec;

      if (window.devicePixelRatio > 1)
        return "chrome://browser/skin/search-engine-placeholder@2x.png";
      return "chrome://browser/skin/search-engine-placeholder.png";
    }

    return "";
  },

  getCellText(index, column) {
    if (column.id == "engineName")
      return this._engineStore.engines[index].name;
    else if (column.id == "engineKeyword")
      return this._engineStore.engines[index].alias;
    return "";
  },

  setTree(tree) {
    this.tree = tree;
  },

  canDrop(targetIndex, orientation, dataTransfer) {
    var sourceIndex = this.getSourceIndexFromDrag(dataTransfer);
    return (sourceIndex != -1 &&
            sourceIndex != targetIndex &&
            sourceIndex != targetIndex + orientation);
  },

  drop(dropIndex, orientation, dataTransfer) {
    var sourceIndex = this.getSourceIndexFromDrag(dataTransfer);
    var sourceEngine = this._engineStore.engines[sourceIndex];

    const nsITreeView = Components.interfaces.nsITreeView;
    if (dropIndex > sourceIndex) {
      if (orientation == nsITreeView.DROP_BEFORE)
        dropIndex--;
    } else if (orientation == nsITreeView.DROP_AFTER) {
      dropIndex++;
    }

    this._engineStore.moveEngine(sourceEngine, dropIndex);
    gMainPane.showRestoreDefaults(true);
    gMainPane.buildDefaultEngineDropDown();

    // Redraw, and adjust selection
    this.invalidate();
    this.selection.select(dropIndex);
  },

  selection: null,
  getRowProperties(index) { return ""; },
  getCellProperties(index, column) { return ""; },
  getColumnProperties(column) { return ""; },
  isContainer(index) { return false; },
  isContainerOpen(index) { return false; },
  isContainerEmpty(index) { return false; },
  isSeparator(index) { return false; },
  isSorted(index) { return false; },
  getParentIndex(index) { return -1; },
  hasNextSibling(parentIndex, index) { return false; },
  getLevel(index) { return 0; },
  getProgressMode(index, column) { },
  getCellValue(index, column) {
    if (column.id == "engineShown")
      return this._engineStore.engines[index].shown;
    return undefined;
  },
  toggleOpenState(index) { },
  cycleHeader(column) { },
  selectionChanged() { },
  cycleCell(row, column) { },
  isEditable(index, column) { return column.id != "engineName"; },
  isSelectable(index, column) { return false; },
  setCellValue(index, column, value) {
    if (column.id == "engineShown") {
      this._engineStore.engines[index].shown = value == "true";
      gEngineView.invalidate();
      gMainPane.saveOneClickEnginesList();
    }
  },
  setCellText(index, column, value) {
    if (column.id == "engineKeyword") {
      gMainPane.editKeyword(this._engineStore.engines[index], value)
                 .then(valid => {
        if (!valid)
          document.getElementById("engineList").startEditing(index, column);
      });
    }
  },
  performAction(action) { },
  performActionOnRow(action, index) { },
  performActionOnCell(action, index, column) { }
};
