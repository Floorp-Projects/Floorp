# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadsCommon",
                                  "resource:///modules/DownloadsCommon.jsm");

var gMainPane = {
  _pane: null,

  /**
   * Initialization of this.
   */
  init: function ()
  {
    this._pane = document.getElementById("paneMain");

    // set up the "use current page" label-changing listener
    this._updateUseCurrentButton();
    window.addEventListener("focus", this._updateUseCurrentButton.bind(this), false);

    this.updateBrowserStartupLastSession();
    this.startupPagePrefChanged();

    this.setupDownloadsWindowOptions();

    // Notify observers that the UI is now ready
    Components.classes["@mozilla.org/observer-service;1"]
              .getService(Components.interfaces.nsIObserverService)
              .notifyObservers(window, "main-pane-loaded", null);
  },

  setupDownloadsWindowOptions: function ()
  {
    var showWhenDownloading = document.getElementById("showWhenDownloading");
    var closeWhenDone = document.getElementById("closeWhenDone");

    // These radio-buttons should not be visible if we have enabled the Downloads Panel.
    let shouldHide = !DownloadsCommon.useToolkitUI;
    showWhenDownloading.hidden = shouldHide;
    closeWhenDone.hidden = shouldHide;
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

  /**
   * Enables/Disables the restore on demand checkbox.
   */
  startupPagePrefChanged: function ()
  {
    let startupPref = document.getElementById("browser.startup.page");
    let restoreOnDemandPref = document.getElementById("browser.sessionstore.restore_on_demand");
    restoreOnDemandPref.disabled = startupPref.value != 3;
  },

  syncFromHomePref: function ()
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

  syncToHomePref: function (value)
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
  setHomePageToCurrent: function ()
  {
    let homePage = document.getElementById("browser.startup.homepage");
    let tabs = this._getTabsForHomePage();
    function getTabURI(t) t.linkedBrowser.currentURI.spec;

    // FIXME Bug 244192: using dangerous "|" joiner!
    if (tabs.length)
      homePage.value = tabs.map(getTabURI).join("|");
  },

  /**
   * Displays a dialog in which the user can select a bookmark to use as home
   * page.  If the user selects a bookmark, that bookmark's name is displayed in
   * UI and the bookmark's address is stored to the home page preference.
   */
  setHomePageToBookmark: function ()
  {
    var rv = { urls: null, names: null };
    document.documentElement.openSubDialog("chrome://browser/content/preferences/selectBookmark.xul",
                                           "resizable", rv);  
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
  _updateUseCurrentButton: function () {
    let useCurrent = document.getElementById("useCurrent");

    let tabs = this._getTabsForHomePage();
    if (tabs.length > 1)
      useCurrent.label = useCurrent.getAttribute("label2");
    else
      useCurrent.label = useCurrent.getAttribute("label1");

    // In this case, the button's disabled state is set by preferences.xml.
    if (document.getElementById
        ("pref.browser.homepage.disable_button.current_page").locked)
      return;

    useCurrent.disabled = !tabs.length
  },

  _getTabsForHomePage: function ()
  {
    var win;
    var tabs = [];
    if (document.documentElement.instantApply) {
      const Cc = Components.classes, Ci = Components.interfaces;
      // If we're in instant-apply mode, use the most recent browser window
      var wm = Cc["@mozilla.org/appshell/window-mediator;1"]
                 .getService(Ci.nsIWindowMediator);
      win = wm.getMostRecentWindow("navigator:browser");
    }
    else {
      win = window.opener;
    }

    if (win && win.document.documentElement
                  .getAttribute("windowtype") == "navigator:browser") {
      // We should only include visible & non-pinned tabs
      tabs = win.gBrowser.visibleTabs.slice(win.gBrowser._numPinnedTabs);
    }

    return tabs;
  },

  /**
   * Restores the default home page as the user's home page.
   */
  restoreDefaultHomePage: function ()
  {
    var homePage = document.getElementById("browser.startup.homepage");
    homePage.value = homePage.defaultValue;
  },

  // DOWNLOADS

  /*
   * Preferences:
   * 
   * browser.download.showWhenStarting - bool
   *   True if the Download Manager should be opened when a download is
   *   started, false if it shouldn't be opened.
   * browser.download.closeWhenDone - bool
   *   True if the Download Manager should be closed when all downloads
   *   complete, false if it should be left open.
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
   * Updates preferences which depend upon the value of the preference which
   * determines whether the Downloads manager is opened at the start of a
   * download.
   */
  readShowDownloadsWhenStarting: function ()
  {
    this.showDownloadsWhenStartingPrefChanged();

    // don't override the preference's value in UI
    return undefined;
  },

  /**
   * Enables or disables the "close Downloads manager when downloads finished"
   * preference element, consequently updating the associated UI.
   */
  showDownloadsWhenStartingPrefChanged: function ()
  {
    var showWhenStartingPref = document.getElementById("browser.download.manager.showWhenStarting");
    var closeWhenDonePref = document.getElementById("browser.download.manager.closeWhenDone");
    closeWhenDonePref.disabled = !showWhenStartingPref.value;
  },

  /**
   * Enables/disables the folder field and Browse button based on whether a
   * default download directory is being used.
   */
  readUseDownloadDir: function ()
  {
    var downloadFolder = document.getElementById("downloadFolder");
    var chooseFolder = document.getElementById("chooseFolder");
    var preference = document.getElementById("browser.download.useDownloadDir");
    downloadFolder.disabled = !preference.value;
    chooseFolder.disabled = !preference.value;

    // don't override the preference's value in UI
    return undefined;
  },
  
  /**
   * Displays a file picker in which the user can choose the location where
   * downloads are automatically saved, updating preferences and UI in
   * response to the choice, if one is made.
   */
  chooseFolder: function ()
  {
    const nsIFilePicker = Components.interfaces.nsIFilePicker;
    const nsILocalFile = Components.interfaces.nsILocalFile;

    var fp = Components.classes["@mozilla.org/filepicker;1"]
                       .createInstance(nsIFilePicker);
    var bundlePreferences = document.getElementById("bundlePreferences");
    var title = bundlePreferences.getString("chooseDownloadFolderTitle");
    fp.init(window, title, nsIFilePicker.modeGetFolder);
    fp.appendFilters(nsIFilePicker.filterAll);

    var folderListPref = document.getElementById("browser.download.folderList");
    var currentDirPref = this._indexToFolder(folderListPref.value); // file
    var defDownloads = this._indexToFolder(1); // file

    // First try to open what's currently configured
    if (currentDirPref && currentDirPref.exists()) {
      fp.displayDirectory = currentDirPref;
    } // Try the system's download dir
    else if (defDownloads && defDownloads.exists()) {
      fp.displayDirectory = defDownloads;
    } // Fall back to Desktop
    else {
      fp.displayDirectory = this._indexToFolder(0);
    }

    if (fp.show() == nsIFilePicker.returnOK) {
      var file = fp.file.QueryInterface(nsILocalFile);
      var currentDirPref = document.getElementById("browser.download.dir");
      currentDirPref.value = file;
      var folderListPref = document.getElementById("browser.download.folderList");
      folderListPref.value = this._folderToIndex(file);
      // Note, the real prefs will not be updated yet, so dnld manager's
      // userDownloadsDirectory may not return the right folder after
      // this code executes. displayDownloadDirPref will be called on
      // the assignment above to update the UI.
    }
  },

  /**
   * Initializes the download folder display settings based on the user's 
   * preferences.
   */
  displayDownloadDirPref: function ()
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
      iconUrlSpec = fph.getURLSpecFromFile(this._indexToFolder(1));
    } else {
      // 'Desktop'
      downloadFolder.label = bundlePreferences.getString("desktopFolderName");
      iconUrlSpec = fph.getURLSpecFromFile(this._getDownloadsFolder("Desktop"));
    }
    downloadFolder.image = "moz-icon://" + iconUrlSpec + "?size=16";
    
    // don't override the preference's value in UI
    return undefined;
  },

  /**
   * Returns the textual path of a folder in readable form.
   */
  _getDisplayNameOfFile: function (aFolder)
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
  _getDownloadsFolder: function (aFolder)
  {
    switch (aFolder) {
      case "Desktop":
        var fileLoc = Components.classes["@mozilla.org/file/directory_service;1"]
                                    .getService(Components.interfaces.nsIProperties);
        return fileLoc.get("Desk", Components.interfaces.nsILocalFile);
      break;
      case "Downloads":
        var dnldMgr = Components.classes["@mozilla.org/download-manager;1"]
                                .getService(Components.interfaces.nsIDownloadManager);
        return dnldMgr.defaultDownloadsDirectory;
      break;
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
  _folderToIndex: function (aFolder)
  {
    if (!aFolder || aFolder.equals(this._getDownloadsFolder("Desktop")))
      return 0;
    else if (aFolder.equals(this._getDownloadsFolder("Downloads")))
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
  _indexToFolder: function (aIndex)
  {
    switch (aIndex) {
      case 0:
        return this._getDownloadsFolder("Desktop");
      case 1:
        return this._getDownloadsFolder("Downloads");
    }
    var currentDirPref = document.getElementById("browser.download.dir");
    return currentDirPref.value;
  },

  /**
   * Returns the value for the browser.download.folderList preference.
   */
  getFolderListPref: function ()
  {
    var folderListPref = document.getElementById("browser.download.folderList");
    switch (folderListPref.value) {
      case 0: // Desktop
      case 1: // Downloads
        return folderListPref.value;
      break;
      case 2: // Custom
        var currentDirPref = document.getElementById("browser.download.dir");
        if (currentDirPref.value) {
          // Resolve to a known location if possible. We are writing out
          // to prefs on this call, so now would be a good time to do it.
          return this._folderToIndex(currentDirPref.value);
        }
        return 0;
      break;
    }
  },

  /**
   * Displays the Add-ons Manager.
   */
  showAddonsMgr: function ()
  {
    openUILinkIn("about:addons", "window");
  },

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
  }
};
