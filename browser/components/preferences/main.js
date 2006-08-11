# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Firefox Preferences System.
#
# The Initial Developer of the Original Code is
# Jeff Walden <jwalden+code@mit.edu>.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben Goodger <ben@mozilla.org>
#   Asaf Romano <mozilla.mano@sent.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

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
    window.addEventListener("focus", this._updateUseCurrentButton, false);
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
   * Sets the home page to the current displayed page (or frontmost tab, if the
   * most recent browser window contains multiple tabs), updating preference
   * window UI to reflect this.
   */
  setHomePageToCurrent: function ()
  {
    var win;
    if (document.documentElement.instantApply) {
      // If we're in instant-apply mode, use the most recent browser window
      var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                         .getService(Components.interfaces.nsIWindowMediator);
      win = wm.getMostRecentWindow("navigator:browser");
    }
    else
      win = window.opener;

    if (win) {
      var homePage = document.getElementById("browser.startup.homepage");
      var browser = win.document.getElementById("content");

      var newVal = browser.browsers[0].currentURI.spec;
      if (browser.browsers.length > 1) {
        // XXX using dangerous "|" joiner!
        for (var i = 1; i < browser.browsers.length; i++)
          newVal += "|" + browser.browsers[i].currentURI.spec;
      }

      homePage.value = newVal;
    }
  },

  /**
   * Displays a dialog in which the user can select a bookmark to use as home
   * page.  If the user selects a bookmark, that bookmark's name is displayed in
   * UI and the bookmark's address is stored to the home page preference.
   */
  setHomePageToBookmark: function ()
  {
    var rv = { urls: null, names: null };
#ifdef MOZ_PLACES
    document.documentElement.openSubDialog("chrome://browser/content/preferences/selectBookmark.xul",
#else
    document.documentElement.openSubDialog("chrome://browser/content/bookmarks/selectBookmark.xul",
#endif
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
    var useCurrent = document.getElementById("useCurrent");

    var win;
    if (document.documentElement.instantApply) {
      const Cc = Components.classes, Ci = Components.interfaces;
      // If we're in instant-apply mode, use the most recent browser window
      var wm = Cc["@mozilla.org/appshell/window-mediator;1"]
                 .getService(Ci.nsIWindowMediator);
      win = wm.getMostRecentWindow("navigator:browser");
    }
    else
      win = window.opener;

    if (win && win.document.documentElement
                  .getAttribute("windowtype") == "navigator:browser") {
      useCurrent.disabled = false;

      var tabbrowser = win.document.getElementById("content");
      if (tabbrowser.browsers.length > 1)
        useCurrent.label = useCurrent.getAttribute("label2");
      else
        useCurrent.label = useCurrent.getAttribute("label1");
    }
    else
      useCurrent.disabled = true;
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
   * browser.download.showWhenStarting
   *   true if the Download Manager should be opened when a download is started,
   *   false if it shouldn't be opened
   * browser.download.closeWhenDone
   *   true if the Download Manager should be closed when all downloads
   *   complete, false if it shouldn't be closed
   * browser.download.useDownloadDir
   *   true if downloads are saved to a default location with no UI shown, false
   *   if the user should always be asked where to save files
   * browser.download.dir
   *   the last directory to which a download was saved
   * browser.download.downloadDir
   *   the current default download location
   * browser.download.folderList
   *   0 if the desktop is the default download location,
   *   1 if the downloads folder is the default download location,
   *   2 if the default download location is elsewhere;
   *   used to display special UI when the default location is the Desktop or
   *   the Downloads folder in Download Manager UI and in the file field in
   *   preferences
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
    var fp = Components.classes["@mozilla.org/filepicker;1"]
                       .createInstance(nsIFilePicker);
    var bundlePreferences = document.getElementById("bundlePreferences");
    var title = bundlePreferences.getString("chooseDownloadFolderTitle");
    fp.init(window, title, nsIFilePicker.modeGetFolder);

    const nsILocalFile = Components.interfaces.nsILocalFile;
    var customDirPref = document.getElementById("browser.download.dir");
    if (customDirPref.value)
      fp.displayDirectory = customDirPref.value;
    fp.appendFilters(nsIFilePicker.filterAll);
    if (fp.show() == nsIFilePicker.returnOK) {
      var file = fp.file.QueryInterface(nsILocalFile);
      var currentDirPref = document.getElementById("browser.download.downloadDir");
      customDirPref.value = currentDirPref.value = file;
      var folderListPref = document.getElementById("browser.download.folderList");
      folderListPref.value = this._folderToIndex(file);
    }
  },

  /**
   * Initializes the download folder widget based on the folder as stored in
   * preferences.
   */
  readDownloadDirPref: function ()
  {
    var folderListPref = document.getElementById("browser.download.folderList");
    var bundlePreferences = document.getElementById("bundlePreferences");
    var downloadFolder = document.getElementById("downloadFolder");

    var customDirPref = document.getElementById("browser.download.dir");
    var customIndex = customDirPref.value ? this._folderToIndex(customDirPref.value) : 0;
    if (folderListPref.value == 0 || customIndex == 0)
      downloadFolder.label = bundlePreferences.getString("desktopFolderName");
    else if (folderListPref.value == 1 || customIndex == 1)
      downloadFolder.label = bundlePreferences.getString("myDownloadsFolderName");
    else
      downloadFolder.label = this._getDisplayNameOfFile(customDirPref.value);

    var ios = Components.classes["@mozilla.org/network/io-service;1"]
                        .getService(Components.interfaces.nsIIOService);
    var fph = ios.getProtocolHandler("file")
                 .QueryInterface(Components.interfaces.nsIFileProtocolHandler);
    var currentDirPref = document.getElementById("browser.download.downloadDir");
    var downloadDir = currentDirPref.value || this._indexToFolder(folderListPref.value);
    var urlspec = fph.getURLSpecFromFile(downloadDir);
    downloadFolder.image = "moz-icon://" + urlspec + "?size=16";

    // don't override the preference's value in UI
    return undefined;
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
   * Returns the Downloads folder.  If aFolder is "Desktop", then the Downloads
   * folder returned is the desktop folder; otherwise, it is a folder whose name
   * indicates that it is a download folder and whose path is as determined by
   * the XPCOM directory service from aFolder.
   *
   * @throws if aFolder is not "Desktop" or "Downloads"
   */
  _getDownloadsFolder: function (aFolder)
  {
    var fileLocator = Components.classes["@mozilla.org/file/directory_service;1"]
                                .getService(Components.interfaces.nsIProperties);
    var dir = fileLocator.get(this._getSpecialFolderKey(aFolder),
                              Components.interfaces.nsILocalFile);
    if (aFolder != "Desktop")
      dir.append("My Downloads"); // XXX l12y!

    return dir;
  },

  /**
   * Gets the platform-specific key to be fed to the directory service for the
   * given special folder.
   *
   * @param   aFolder
   *          either of the strings "Desktop" or "Downloads"
   * @returns the platform-specific key for the location, which may be used with
   *          the XPCOM directory service
   */
  _getSpecialFolderKey: function (aFolderType)
  {
    if (aFolderType == "Desktop")
      return "Desk";

    if (aFolderType == "Downloads")
#ifdef XP_WIN
      return "Pers";
#else
#ifdef XP_MACOSX
      return "UsrDocs";
#else
      return "Home";
#endif
#endif

      throw "ASSERTION FAILED: folder type should be 'Desktop' or 'Downloads'";
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
   * Converts an integer into the corresponding folder.
   *
   * @param   aIndex
   *          an integer
   * @returns the Desktop folder if aIndex == 0,
   *          the Downloads folder if aIndex == 1,
   *          the folder stored in browser.download.dir otherwise
   */
  _indexToFolder: function (aIndex)
  {
    switch (aIndex) {
      case 0:
        return this._getDownloadsFolder("Desktop");
      case 1:
        return this._getDownloadsFolder("Downloads");
    }

    var customDirPref = document.getElementById("browser.download.dir");
    return customDirPref.value;
  },

  /**
   * Returns the value for the browser.download.folderList preference determined
   * from the current value of browser.download.downloadDir.
   */
  writeFolderList: function ()
  {
    var currentDirPref = document.getElementById("browser.download.downloadDir");
    return this._folderToIndex(currentDirPref.value);
  }

#ifdef HAVE_SHELL_SERVICE
  ,

  // SYSTEM DEFAULTS

  /*
   * Preferences:
   *
   * browser.shell.checkDefault
   * - true if a default-browser check (and prompt to make it so if necessary)
   *   occurs at startup, false otherwise
   */

  /**
   * Checks whether the browser is currently registered with the operating
   * system as the default browser.  If the browser is not currently the
   * default browser, the user is given the option of making it the default;
   * otherwise, the user is informed that this browser already is the browser.
   */
  checkNow: function ()
  {
    var shellSvc = Components.classes["@mozilla.org/browser/shell-service;1"]
                             .getService(Components.interfaces.nsIShellService);
    var brandBundle = document.getElementById("bundleBrand");
    var shellBundle = document.getElementById("bundleShell");
    var brandShortName = brandBundle.getString("brandShortName");
    var promptTitle = shellBundle.getString("setDefaultBrowserTitle");
    var promptMessage;
    const IPS = Components.interfaces.nsIPromptService;
    var psvc = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                         .getService(IPS);
    if (!shellSvc.isDefaultBrowser(false)) {
      promptMessage = shellBundle.getFormattedString("setDefaultBrowserMessage", 
                                                     [brandShortName]);
      var rv = psvc.confirmEx(window, promptTitle, promptMessage, 
                              (IPS.BUTTON_TITLE_YES * IPS.BUTTON_POS_0) + 
                              (IPS.BUTTON_TITLE_NO * IPS.BUTTON_POS_1),
                              null, null, null, null, { });
      if (rv == 0)
        shellSvc.setDefaultBrowser(true, false);
    }
    else {
      promptMessage = shellBundle.getFormattedString("alreadyDefaultBrowser",
                                                     [brandShortName]);
      psvc.alert(window, promptTitle, promptMessage);
    }
  }
#endif
};
