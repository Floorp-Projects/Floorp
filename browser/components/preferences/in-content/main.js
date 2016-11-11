/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

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

var gMainPane = {
  /**
   * Initialization of this.
   */
  init: function()
  {
    function setEventListener(aId, aEventType, aCallback)
    {
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

    // set up the "use current page" label-changing listener
    this._updateUseCurrentButton();
    window.addEventListener("focus", this._updateUseCurrentButton.bind(this), false);

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
    setEventListener("browser.download.dir", "change",
                     gMainPane.displayDownloadDirPref);
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
    setEventListener("chooseFolder", "command",
                     gMainPane.chooseFolder);

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
    }

    // Notify observers that the UI is now ready
    Components.classes["@mozilla.org/observer-service;1"]
              .getService(Components.interfaces.nsIObserverService)
              .notifyObservers(window, "main-pane-loaded", null);
  },

  enableE10SChange: function()
  {
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

        Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit |  Ci.nsIAppStartup.eRestart);
      }

      // Revert the checkbox in case we didn't quit
      e10sCheckbox.checked = e10sPref.value || e10sTempPref.value;
    }
  },

  separateProfileModeChange: function()
  {
    if (AppConstants.MOZ_DEV_EDITION) {
      function quitApp() {
        Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit |  Ci.nsIAppStartup.eRestartNotSameProfile);
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
          return;
      }
    }
  },

  onGetStarted: function(aEvent) {
    if (AppConstants.MOZ_DEV_EDITION) {
      const Cc = Components.classes, Ci = Components.interfaces;
      let wm = Cc["@mozilla.org/appshell/window-mediator;1"]
                  .getService(Ci.nsIWindowMediator);
      let win = wm.getMostRecentWindow("navigator:browser");

      if (win) {
        let accountsTab = win.gBrowser.addTab("about:accounts?action=signin&entrypoint=dev-edition-setup");
        win.gBrowser.selectedTab = accountsTab;
      }
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

  syncFromHomePref: function()
  {
    let homePref = document.getElementById("browser.startup.homepage");

    // If the pref is set to about:home, set the value to "" to show the
    // placeholder text (about:home title).
    if (homePref.value.toLowerCase() == "about:home")
      return "";

    // If the pref is actually "", show about:blank.  The actual home page
    // loading code treats them the same, and we don't want the placeholder text
    // to be shown.
    if (homePref.value == "")
      return "about:blank";

    // Otherwise, show the actual pref value.
    return undefined;
  },

  syncToHomePref: function(value)
  {
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
  setHomePageToCurrent: function()
  {
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
  setHomePageToBookmark: function()
  {
    var rv = { urls: null, names: null };
    gSubDialog.open("chrome://browser/content/preferences/selectBookmark.xul",
                    "resizable=yes, modal=yes", rv,
                    this._setHomePageToBookmarkClosed.bind(this, rv));
  },

  _setHomePageToBookmarkClosed: function(rv, aEvent) {
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
  _updateUseCurrentButton: function() {
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

  _getTabsForHomePage: function()
  {
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
  isNotAboutPreferences: function(aElement, aIndex, aArray)
  {
    return !aElement.linkedBrowser.currentURI.spec.startsWith("about:preferences");
  },

  /**
   * Restores the default home page as the user's home page.
   */
  restoreDefaultHomePage: function()
  {
    var homePage = document.getElementById("browser.startup.homepage");
    homePage.value = homePage.defaultValue;
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
  readUseDownloadDir: function()
  {
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
  chooseFolder()
  {
    return this.chooseFolderTask().catch(Components.utils.reportError);
  },
  chooseFolderTask: Task.async(function* ()
  {
    let bundlePreferences = document.getElementById("bundlePreferences");
    let title = bundlePreferences.getString("chooseDownloadFolderTitle");
    let folderListPref = document.getElementById("browser.download.folderList");
    let currentDirPref = yield this._indexToFolder(folderListPref.value);
    let defDownloads = yield this._indexToFolder(1);
    let fp = Components.classes["@mozilla.org/filepicker;1"].
             createInstance(Components.interfaces.nsIFilePicker);

    fp.init(window, title, Components.interfaces.nsIFilePicker.modeGetFolder);
    fp.appendFilters(Components.interfaces.nsIFilePicker.filterAll);
    // First try to open what's currently configured
    if (currentDirPref && currentDirPref.exists()) {
      fp.displayDirectory = currentDirPref;
    } // Try the system's download dir
    else if (defDownloads && defDownloads.exists()) {
      fp.displayDirectory = defDownloads;
    } // Fall back to Desktop
    else {
      fp.displayDirectory = yield this._indexToFolder(0);
    }

    let result = yield new Promise(resolve => fp.open(resolve));
    if (result != Components.interfaces.nsIFilePicker.returnOK) {
      return;
    }

    let downloadDirPref = document.getElementById("browser.download.dir");
    downloadDirPref.value = fp.file;
    folderListPref.value = yield this._folderToIndex(fp.file);
    // Note, the real prefs will not be updated yet, so dnld manager's
    // userDownloadsDirectory may not return the right folder after
    // this code executes. displayDownloadDirPref will be called on
    // the assignment above to update the UI.
  }),

  /**
   * Initializes the download folder display settings based on the user's
   * preferences.
   */
  displayDownloadDirPref()
  {
    this.displayDownloadDirPrefTask().catch(Components.utils.reportError);

    // don't override the preference's value in UI
    return undefined;
  },

  displayDownloadDirPrefTask: Task.async(function* ()
  {
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
      // In 1.5, this pointed to a folder we created called 'My Downloads'
      // and was available as an option in the 1.5 drop down. On XP this
      // was in My Documents, on OSX it was in User Docs. In 2.0, we did
      // away with the drop down option, although the special label was
      // still supported for the folder if it existed. Because it was
      // not exposed it was rarely used.
      // With 3.0, a new desktop folder - 'Downloads' was introduced for
      // platforms and versions that don't support a default system downloads
      // folder. See nsDownloadManager for details.
      downloadFolder.label = bundlePreferences.getString("downloadsFolderName");
      iconUrlSpec = fph.getURLSpecFromFile(yield this._indexToFolder(1));
    } else {
      // 'Desktop'
      downloadFolder.label = bundlePreferences.getString("desktopFolderName");
      iconUrlSpec = fph.getURLSpecFromFile(yield this._getDownloadsFolder("Desktop"));
    }
    downloadFolder.image = "moz-icon://" + iconUrlSpec + "?size=16";
  }),

  /**
   * Returns the textual path of a folder in readable form.
   */
  _getDisplayNameOfFile: function(aFolder)
  {
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
  _getDownloadsFolder: Task.async(function* (aFolder)
  {
    switch (aFolder) {
      case "Desktop":
        var fileLoc = Components.classes["@mozilla.org/file/directory_service;1"]
                                    .getService(Components.interfaces.nsIProperties);
        return fileLoc.get("Desk", Components.interfaces.nsILocalFile);
      case "Downloads":
        let downloadsDir = yield Downloads.getSystemDownloadsDirectory();
        return new FileUtils.File(downloadsDir);
    }
    throw "ASSERTION FAILED: folder type should be 'Desktop' or 'Downloads'";
  }),

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
  _folderToIndex: Task.async(function* (aFolder)
  {
    if (!aFolder || aFolder.equals(yield this._getDownloadsFolder("Desktop")))
      return 0;
    else if (aFolder.equals(yield this._getDownloadsFolder("Downloads")))
      return 1;
    return 2;
  }),

  /**
   * Converts an integer into the corresponding folder.
   *
   * @param   aIndex
   *          an integer
   * @returns the Desktop folder if aIndex == 0,
   *          the Downloads folder if aIndex == 1,
   *          the folder stored in browser.download.dir
   */
  _indexToFolder: Task.async(function* (aIndex)
  {
    switch (aIndex) {
      case 0:
        return yield this._getDownloadsFolder("Desktop");
      case 1:
        return yield this._getDownloadsFolder("Downloads");
    }
    var currentDirPref = document.getElementById("browser.download.dir");
    return currentDirPref.value;
  }),

  /**
   * Hide/show the "Show my windows and tabs from last time" option based
   * on the value of the browser.privatebrowsing.autostart pref.
   */
  updateBrowserStartupLastSession: function()
  {
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
  readLinkTarget: function() {
    var openNewWindow = document.getElementById("browser.link.open_newwindow");
    return openNewWindow.value != 2;
  },

  /**
   * Determines where a link which opens a new window will open.
   *
   * @returns 2 if such links should be opened in new windows,
   *          3 if such links should be opened in new tabs
   */
  writeLinkTarget: function() {
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
  updateSetDefaultBrowser: function()
  {
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
  setDefaultBrowser: function()
  {
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
};
