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
# The Original Code is the Browser Search Service.
#
# The Initial Developer of the Original Code is
# Giorgio Maone
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Giorgio Maone <g.maone@informaction.com>
#   Seth Spitzer <sspitzer@mozilla.com>
#   Asaf Romano <mano@mozilla.com>
#   Marco Bonardo <mak77@bonardo.net>
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

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/distribution.js");

const PREF_EM_NEW_ADDONS_LIST = "extensions.newAddons";

// Check to see if bookmarks need backing up once per
// day on 1 hour idle.
const BOOKMARKS_ARCHIVE_IDLE_TIME = 60 * 60;

// Backup bookmarks once every 24 hours.
const BOOKMARKS_ARCHIVE_INTERVAL = 86400 * 1000;

// Factory object
const BrowserGlueServiceFactory = {
  _instance: null,
  createInstance: function (outer, iid) 
  {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return this._instance == null ?
      this._instance = new BrowserGlue() : this._instance;
  }
};

// Constructor

function BrowserGlue() {
  this._init();
}

BrowserGlue.prototype = {
  get _prefs() {
    var prefs = Cc["@mozilla.org/preferences-service;1"].
                getService(Ci.nsIPrefBranch);
    this.__defineGetter__("_prefs", function() prefs);
    return this._prefs;
  },

  get _bundleService() {
    var bundleService = Cc["@mozilla.org/intl/stringbundle;1"].
                        getService(Ci.nsIStringBundleService);
    this.__defineGetter__("_bundleService", function() bundleService);
    return this._bundleService;
  },

  get _placesBundle() {
    var strings = this._bundleService.
                       createBundle("chrome://browser/locale/places/places.properties");
    this.__defineGetter__("_placesBundle", function() strings);
    return this._placesBundle;
  },

  get _idleService() {
    var idleSvc = Cc["@mozilla.org/widget/idleservice;1"].
                    getService(Ci.nsIIdleService);
    this.__defineGetter__("_idleService", function() idleSvc);
    return this._idleService;
  },

  get _observerService() {
    const obssvc = Cc['@mozilla.org/observer-service;1'].
                   getService(Ci.nsIObserverService);
    this.__defineGetter__("_observerService", function() obssvc);
    return this._observerService;
  },

  _saveSession: false,

  _setPrefToSaveSession: function()
  {
    this._prefs.setBoolPref("browser.sessionstore.resume_session_once", true);
  },

  // nsIObserver implementation 
  observe: function(subject, topic, data) 
  {
    switch(topic) {
      case "xpcom-shutdown":
        this._dispose();
        break;
      case "quit-application": 
        this._onProfileShutdown();
        break;
      case "prefservice:after-app-defaults":
        this._onAppDefaults();
        break;
      case "final-ui-startup":
        this._onProfileStartup();
        break;
      case "sessionstore-windows-restored":
        this._onBrowserStartup();
        break;
      case "browser:purge-session-history":
        // reset the console service's error buffer
        const cs = Cc["@mozilla.org/consoleservice;1"].
                   getService(Ci.nsIConsoleService);
        cs.logStringMessage(null); // clear the console (in case it's open)
        cs.reset();
        break;
      case "quit-application-requested":
        this._onQuitRequest(subject, data);
        break;
      case "quit-application-granted":
        if (this._saveSession) {
          this._setPrefToSaveSession();
        }
        break;
      case "session-save":
        this._setPrefToSaveSession();
        subject.QueryInterface(Ci.nsISupportsPRBool);
        subject.data = true;
        break;
      case "places-init-complete":
        this._initPlaces();
        this._observerService.removeObserver(this, "places-init-complete");
        // no longer needed, since history was initialized completely.
        this._observerService.removeObserver(this, "places-database-locked");
        break;
      case "places-database-locked":
        this._isPlacesDatabaseLocked = true;
        // stop observing, so further attempts to load history service
        // do not show the prompt.
        this._observerService.removeObserver(this, "places-database-locked");
        break;
      case "idle":
        if (this._idleService.idleTime > BOOKMARKS_ARCHIVE_IDLE_TIME * 1000) {
          // Back up bookmarks.
          this._archiveBookmarks();
        }
        break;
    }
  }, 

  // initialization (called on application startup) 
  _init: function() 
  {
    // observer registration
    const osvr = this._observerService;
    osvr.addObserver(this, "quit-application", false);
    osvr.addObserver(this, "xpcom-shutdown", false);
    osvr.addObserver(this, "prefservice:after-app-defaults", false);
    osvr.addObserver(this, "final-ui-startup", false);
    osvr.addObserver(this, "sessionstore-windows-restored", false);
    osvr.addObserver(this, "browser:purge-session-history", false);
    osvr.addObserver(this, "quit-application-requested", false);
    osvr.addObserver(this, "quit-application-granted", false);
    osvr.addObserver(this, "session-save", false);
    osvr.addObserver(this, "places-init-complete", false);
    osvr.addObserver(this, "places-database-locked", false);
  },

  // cleanup (called on application shutdown)
  _dispose: function() 
  {
    // observer removal 
    const osvr = this._observerService;
    osvr.removeObserver(this, "quit-application");
    osvr.removeObserver(this, "xpcom-shutdown");
    osvr.removeObserver(this, "prefservice:after-app-defaults");
    osvr.removeObserver(this, "final-ui-startup");
    osvr.removeObserver(this, "sessionstore-windows-restored");
    osvr.removeObserver(this, "browser:purge-session-history");
    osvr.removeObserver(this, "quit-application-requested");
    osvr.removeObserver(this, "quit-application-granted");
    osvr.removeObserver(this, "session-save");
  },

  _onAppDefaults: function()
  {
    // apply distribution customizations (prefs)
    // other customizations are applied in _onProfileStartup()
    var distro = new DistributionCustomizer();
    distro.applyPrefDefaults();
  },

  // profile startup handler (contains profile initialization routines)
  _onProfileStartup: function() 
  {
    this.Sanitizer.onStartup();
    // check if we're in safe mode
    var app = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULAppInfo).
              QueryInterface(Ci.nsIXULRuntime);
    if (app.inSafeMode) {
      var ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
               getService(Ci.nsIWindowWatcher);
      ww.openWindow(null, "chrome://browser/content/safeMode.xul", 
                    "_blank", "chrome,centerscreen,modal,resizable=no", null);
    }

    // apply distribution customizations
    // prefs are applied in _onAppDefaults()
    var distro = new DistributionCustomizer();
    distro.applyCustomizations();

    // handle any UI migration
    this._migrateUI();

    this._observerService.notifyObservers(null, "browser-ui-startup-complete", "");
  },

  // profile shutdown handler (contains profile cleanup routines)
  _onProfileShutdown: function() 
  {
    this._shutdownPlaces();
    this._idleService.removeIdleObserver(this, BOOKMARKS_ARCHIVE_IDLE_TIME);
    this.Sanitizer.onShutdown();
  },

  // Browser startup complete. All initial windows have opened.
  _onBrowserStartup: function()
  {
    // Show about:rights notification, if needed.
    if (this._shouldShowRights())
      this._showRightsNotification();

    // If new add-ons were installed during startup open the add-ons manager.
    if (this._prefs.prefHasUserValue(PREF_EM_NEW_ADDONS_LIST)) {
      var args = Cc["@mozilla.org/supports-array;1"].
                 createInstance(Ci.nsISupportsArray);
      var str = Cc["@mozilla.org/supports-string;1"].
                createInstance(Ci.nsISupportsString);
      str.data = "";
      args.AppendElement(str);
      var str = Cc["@mozilla.org/supports-string;1"].
                createInstance(Ci.nsISupportsString);
      str.data = this._prefs.getCharPref(PREF_EM_NEW_ADDONS_LIST);
      args.AppendElement(str);
      const EMURL = "chrome://mozapps/content/extensions/extensions.xul";
      const EMFEATURES = "chrome,menubar,extra-chrome,toolbar,dialog=no,resizable";
      var ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
               getService(Ci.nsIWindowWatcher);
      ww.openWindow(null, EMURL, "_blank", EMFEATURES, args);
      this._prefs.clearUserPref(PREF_EM_NEW_ADDONS_LIST);
    }

    // Load the "more info" page for a locked places.sqlite
    // This property is set earlier in the startup process:
    // nsPlacesDBFlush loads after profile-after-change and initializes
    // the history service, which sends out places-database-locked
    // which sets this property.
    if (this._isPlacesDatabaseLocked) {
      this._showPlacesLockedNotificationBox();
    }
  },

  _onQuitRequest: function(aCancelQuit, aQuitType)
  {
    // If user has already dismissed quit request, then do nothing
    if ((aCancelQuit instanceof Ci.nsISupportsPRBool) && aCancelQuit.data)
      return;

    var wm = Cc["@mozilla.org/appshell/window-mediator;1"].
             getService(Ci.nsIWindowMediator);

    var windowcount = 0;
    var pagecount = 0;
    var browserEnum = wm.getEnumerator("navigator:browser");
    while (browserEnum.hasMoreElements()) {
      windowcount++;

      var browser = browserEnum.getNext();
      var tabbrowser = browser.document.getElementById("content");
      if (tabbrowser)
        pagecount += tabbrowser.browsers.length;
    }

    this._saveSession = false;
    if (pagecount < 2)
      return;

    if (aQuitType != "restart")
      aQuitType = "quit";

    var showPrompt = true;
    try {
      // browser.warnOnQuit is a hidden global boolean to override all quit prompts
      // browser.warnOnRestart specifically covers app-initiated restarts where we restart the app
      // browser.tabs.warnOnClose is the global "warn when closing multiple tabs" pref

      var sessionWillBeSaved = this._prefs.getIntPref("browser.startup.page") == 3 ||
                               this._prefs.getBoolPref("browser.sessionstore.resume_session_once");
      if (sessionWillBeSaved || !this._prefs.getBoolPref("browser.warnOnQuit"))
        showPrompt = false;
      else if (aQuitType == "restart")
        showPrompt = this._prefs.getBoolPref("browser.warnOnRestart");
      else
        showPrompt = this._prefs.getBoolPref("browser.tabs.warnOnClose");
    } catch (ex) {}

    if (!showPrompt)
      return false;

    var buttonChoice = 0;
    var quitBundle = this._bundleService.createBundle("chrome://browser/locale/quitDialog.properties");
    var brandBundle = this._bundleService.createBundle("chrome://branding/locale/brand.properties");

    var appName = brandBundle.GetStringFromName("brandShortName");
    var quitDialogTitle = quitBundle.formatStringFromName(aQuitType + "DialogTitle",
                                                          [appName], 1);

    var message;
    if (aQuitType == "restart")
      message = quitBundle.formatStringFromName("messageRestart",
                                                [appName], 1);
    else if (windowcount == 1)
      message = quitBundle.formatStringFromName("messageNoWindows",
                                                [appName], 1);
    else
      message = quitBundle.formatStringFromName("message",
                                                [appName], 1);

    var promptService = Cc["@mozilla.org/embedcomp/prompt-service;1"].
                        getService(Ci.nsIPromptService);

    var flags = promptService.BUTTON_TITLE_IS_STRING * promptService.BUTTON_POS_0 +
                promptService.BUTTON_TITLE_IS_STRING * promptService.BUTTON_POS_1 +
                promptService.BUTTON_POS_0_DEFAULT;

    var neverAsk = {value:false};
    var button0Title, button2Title;
    var button1Title = quitBundle.GetStringFromName("cancelTitle");
    var neverAskText = quitBundle.GetStringFromName("neverAsk");

    if (aQuitType == "restart")
      button0Title = quitBundle.GetStringFromName("restartTitle");
    else {
      flags += promptService.BUTTON_TITLE_IS_STRING * promptService.BUTTON_POS_2;
      button0Title = quitBundle.GetStringFromName("saveTitle");
      button2Title = quitBundle.GetStringFromName("quitTitle");
    }

    buttonChoice = promptService.confirmEx(null, quitDialogTitle, message,
                                 flags, button0Title, button1Title, button2Title,
                                 neverAskText, neverAsk);

    switch (buttonChoice) {
    case 2: // Quit
      if (neverAsk.value)
        this._prefs.setBoolPref("browser.tabs.warnOnClose", false);
      break;
    case 1: // Cancel
      aCancelQuit.QueryInterface(Ci.nsISupportsPRBool);
      aCancelQuit.data = true;
      break;
    case 0: // Save & Quit
      this._saveSession = true;
      if (neverAsk.value) {
        if (aQuitType == "restart")
          this._prefs.setBoolPref("browser.warnOnRestart", false);
        else {
          // always save state when shutting down
          this._prefs.setIntPref("browser.startup.page", 3);
        }
      }
      break;
    }
  },

  /*
   * _shouldShowRights - Determines if the user should be shown the
   * about:rights notification. The notification should *not* be shown if
   * we've already shown the current version, or if the override pref says to
   * never show it. The notification *should* be shown if it's never been seen
   * before, if a newer version is available, or if the override pref says to
   * always show it.
   */
  _shouldShowRights : function () {
    // Look for an unconditional override pref. If set, do what it says.
    // (true --> never show, false --> always show)
    try {
      return !this._prefs.getBoolPref("browser.rights.override");
    } catch (e) { }
    // Ditto, for the legacy EULA pref.
    try {
      return !this._prefs.getBoolPref("browser.EULA.override");
    } catch (e) { }

#ifndef OFFICIAL_BUILD
    // Non-official builds shouldn't shouldn't show the notification.
    return false;
#endif

    // Look to see if the user has seen the current version or not.
    var currentVersion = this._prefs.getIntPref("browser.rights.version");
    try {
      return !this._prefs.getBoolPref("browser.rights." + currentVersion + ".shown");
    } catch (e) { }

    // Legacy: If the user accepted a EULA, we won't annoy them with the
    // equivalent about:rights page until the version changes.
    try {
      return !this._prefs.getBoolPref("browser.EULA." + currentVersion + ".accepted");
    } catch (e) { }

    // We haven't shown the notification before, so do so now.
    return true;
  },

  _showRightsNotification : function () {
    // Stick the notification onto the selected tab of the active browser window.
    var win = this.getMostRecentBrowserWindow();
    var browser = win.gBrowser; // for closure in notification bar callback
    var notifyBox = browser.getNotificationBox();

    var brandBundle  = this._bundleService.createBundle("chrome://branding/locale/brand.properties");
    var rightsBundle = this._bundleService.createBundle("chrome://browser/locale/aboutRights.properties");

    var buttonLabel     = rightsBundle.GetStringFromName("buttonLabel");
    var buttonAccessKey = rightsBundle.GetStringFromName("buttonAccessKey");
    var productName     = brandBundle.GetStringFromName("brandFullName");
    var notifyText      = rightsBundle.formatStringFromName("notifyText", [productName], 1);
    
    var buttons = [
                    {
                      label:     buttonLabel,
                      accessKey: buttonAccessKey,
                      popup:     null,
                      callback: function(aNotificationBar, aButton) {
                        browser.selectedTab = browser.addTab("about:rights");
                      }
                    }
                  ];

    // Set pref to indicate we've shown the notification.
    var currentVersion = this._prefs.getIntPref("browser.rights.version");
    this._prefs.setBoolPref("browser.rights." + currentVersion + ".shown", true);

    var box = notifyBox.appendNotification(notifyText, "about-rights", null, notifyBox.PRIORITY_INFO_LOW, buttons);
    box.persistence = 3; // arbitrary number, just so bar sticks around for a bit
  },

  // returns the (cached) Sanitizer constructor
  get Sanitizer() 
  {
    if(typeof(Sanitizer) != "function") { // we should dynamically load the script
      Cc["@mozilla.org/moz/jssubscript-loader;1"].
      getService(Ci.mozIJSSubScriptLoader).
      loadSubScript("chrome://browser/content/sanitize.js", null);
    }
    return Sanitizer;
  },

  /**
   * Initialize Places
   * - imports the bookmarks html file if bookmarks database is empty, try to
   *   restore bookmarks from a JSON backup if the backend indicates that the
   *   database was corrupt.
   *
   * These prefs can be set up by the frontend:
   *
   * WARNING: setting these preferences to true will overwite existing bookmarks
   *
   * - browser.places.importBookmarksHTML
   *   Set to true will import the bookmarks.html file from the profile folder.
   * - browser.places.smartBookmarksVersion
   *   Set during HTML import to indicate that Smart Bookmarks were created.
   *   Set to -1 to disable Smart Bookmarks creation.
   *   Set to 0 to restore current Smart Bookmarks.
   * - browser.bookmarks.restore_default_bookmarks
   *   Set to true by safe-mode dialog to indicate we must restore default
   *   bookmarks.
   */
  _initPlaces: function bg__initPlaces() {
    // We must instantiate the history service since it will tell us if we
    // need to import or restore bookmarks due to first-run, corruption or
    // forced migration (due to a major schema change).
    var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                  getService(Ci.nsINavHistoryService);

    // If the database is corrupt or has been newly created we should
    // import bookmarks.
    var databaseStatus = histsvc.databaseStatus;
    var importBookmarks = databaseStatus == histsvc.DATABASE_STATUS_CREATE ||
                          databaseStatus == histsvc.DATABASE_STATUS_CORRUPT;

    // Check if user or an extension has required to import bookmarks.html
    var importBookmarksHTML = false;
    try {
      importBookmarksHTML =
        this._prefs.getBoolPref("browser.places.importBookmarksHTML");
      if (importBookmarksHTML)
        importBookmarks = true;
    } catch(ex) {}

    // Check if Safe Mode or the user has required to restore bookmarks from
    // default profile's bookmarks.html
    var restoreDefaultBookmarks = false;
    try {
      restoreDefaultBookmarks =
        this._prefs.getBoolPref("browser.bookmarks.restore_default_bookmarks");
      if (restoreDefaultBookmarks) {
        // Ensure that we already have a bookmarks backup for today
        this._archiveBookmarks();
        importBookmarks = true;
      }
    } catch(ex) {}

    // If the user did not require to restore default bookmarks, or import
    // from bookmarks.html, we will try to restore from JSON
    if (importBookmarks && !restoreDefaultBookmarks && !importBookmarksHTML) {
      // get latest JSON backup
      Cu.import("resource://gre/modules/utils.js");
      var bookmarksBackupFile = PlacesUtils.getMostRecentBackup();
      if (bookmarksBackupFile && bookmarksBackupFile.leafName.match("\.json$")) {
        // restore from JSON backup
        PlacesUtils.restoreBookmarksFromJSONFile(bookmarksBackupFile);
        importBookmarks = false;
      }
      else {
        // We have created a new database but we don't have any backup available
        importBookmarks = true;
        var dirService = Cc["@mozilla.org/file/directory_service;1"].
                         getService(Ci.nsIProperties);
        var bookmarksHTMLFile = dirService.get("BMarks", Ci.nsILocalFile);
        if (bookmarksHTMLFile.exists()) {
          // If bookmarks.html is available in current profile import it...
          importBookmarksHTML = true;
        }
        else {
          // ...otherwise we will restore defaults
          restoreDefaultBookmarks = true;
        }
      }
    }

    if (!importBookmarks) {
      // Call it here for Fx3 profiles created before the Places folder
      // has been added, otherwise it's called during import.
      this.ensurePlacesDefaultQueriesInitialized();
    }
    else {
      // ensurePlacesDefaultQueriesInitialized() is called by import.
      this._prefs.setIntPref("browser.places.smartBookmarksVersion", 0);

      // Get bookmarks.html file location
      var dirService = Cc["@mozilla.org/file/directory_service;1"].
                       getService(Ci.nsIProperties);

      var bookmarksFile = null;
      if (restoreDefaultBookmarks) {
        // User wants to restore bookmarks.html file from default profile folder
        bookmarksFile = dirService.get("profDef", Ci.nsILocalFile);
        bookmarksFile.append("bookmarks.html");
      }
      else
        bookmarksFile = dirService.get("BMarks", Ci.nsILocalFile);

      if (bookmarksFile.exists()) {
        // import the file
        try {
          var importer = Cc["@mozilla.org/browser/places/import-export-service;1"].
                         getService(Ci.nsIPlacesImportExportService);
          importer.importHTMLFromFile(bookmarksFile, true /* overwrite existing */);
        } catch (err) {
          // Report the error, but ignore it.
          Cu.reportError("Bookmarks.html file could be corrupt. " + err);
        }
      }
      else
        Cu.reportError("Unable to find bookmarks.html file.");

      // Reset preferences, so we won't try to import again at next run
      if (importBookmarksHTML)
        this._prefs.setBoolPref("browser.places.importBookmarksHTML", false);
      if (restoreDefaultBookmarks)
        this._prefs.setBoolPref("browser.bookmarks.restore_default_bookmarks",
                                false);
    }

    // Initialize bookmark archiving on idle.
    // Once a day, either on idle or shutdown, bookmarks are backed up.
    this._idleService.addIdleObserver(this, BOOKMARKS_ARCHIVE_IDLE_TIME);
  },

  /**
   * Places shut-down tasks
   * - back up and archive bookmarks
   * - export bookmarks as HTML, if so configured
   *
   * Note: quit-application-granted notification is received twice
   *       so replace this method with a no-op when first called.
   */
  _shutdownPlaces: function bg__shutdownPlaces() {
    // Backup and archive Places bookmarks.
    this._archiveBookmarks();

    // Backup bookmarks to bookmarks.html to support apps that depend
    // on the legacy format.
    var autoExportHTML = false;
    try {
      autoExportHTML = this._prefs.getBoolPref("browser.bookmarks.autoExportHTML");
    } catch(ex) {
      Components.utils.reportError(ex);
    }

    if (autoExportHTML) {
      Cc["@mozilla.org/browser/places/import-export-service;1"].
        getService(Ci.nsIPlacesImportExportService).
        backupBookmarksFile();
    }
  },

  /**
   * Back up and archive bookmarks
   */
  _archiveBookmarks: function nsBrowserGlue__archiveBookmarks() {
    Cu.import("resource://gre/modules/utils.js");

    var lastBackup = PlacesUtils.getMostRecentBackup();

    // Backup bookmarks if there aren't any backups or 
    // they haven't been backed up in the last 24 hrs.
    if (!lastBackup ||
        Date.now() - lastBackup.lastModifiedTime > BOOKMARKS_ARCHIVE_INTERVAL) {
      var maxBackups = 5;
      try {
        maxBackups = this._prefs.getIntPref("browser.bookmarks.max_backups");
      } catch(ex) {}

      PlacesUtils.archiveBookmarksFile(maxBackups, false /* don't force */);
    }
  },

  /**
   * Show the notificationBox for a locked places database.
   */
  _showPlacesLockedNotificationBox: function nsBrowserGlue__showPlacesLockedNotificationBox() {
    var brandBundle  = this._bundleService.createBundle("chrome://branding/locale/brand.properties");
    var applicationName = brandBundle.GetStringFromName("brandShortName");
    var title = this._placesBundle.GetStringFromName("lockPrompt.title");
    var text = this._placesBundle.formatStringFromName("lockPrompt.text", [applicationName], 1);
    var buttonText = this._placesBundle.GetStringFromName("lockPromptInfoButton.label");
    var accessKey = this._placesBundle.GetStringFromName("lockPromptInfoButton.accessKey");

    var helpTopic = "places-locked";
    var url = Cc["@mozilla.org/toolkit/URLFormatterService;1"].
              getService(Components.interfaces.nsIURLFormatter).
              formatURLPref("app.support.baseURL");
    url += helpTopic;

    var browser = this.getMostRecentBrowserWindow().gBrowser;

    var buttons = [
                    {
                      label:     buttonText,
                      accessKey: accessKey,
                      popup:     null,
                      callback:  function(aNotificationBar, aButton) {
                        browser.selectedTab = browser.addTab(url);
                      }
                    }
                  ];

    var notifyBox = browser.getNotificationBox();
    var box = notifyBox.appendNotification(text, title, null,
                                           notifyBox.PRIORITY_CRITICAL_MEDIUM,
                                           buttons);
    box.persistence = -1; // Until user closes it
  },

  _migrateUI: function bg__migrateUI() {
    var migration = 0;
    try {
      migration = this._prefs.getIntPref("browser.migration.version");
    } catch(ex) {}

    if (migration == 0) {
      // this code should always migrate pre-FF3 profiles to the current UI state

      // grab the localstore.rdf and make changes needed for new UI
      this._rdf = Cc["@mozilla.org/rdf/rdf-service;1"].getService(Ci.nsIRDFService);
      this._dataSource = this._rdf.GetDataSource("rdf:local-store");
      this._dirty = false;

      let currentsetResource = this._rdf.GetResource("currentset");
      let toolbars = ["nav-bar", "toolbar-menubar", "PersonalToolbar"];
      for (let i = 0; i < toolbars.length; i++) {
        let toolbar = this._rdf.GetResource("chrome://browser/content/browser.xul#" + toolbars[i]);
        let currentset = this._getPersist(toolbar, currentsetResource);
        if (!currentset) {
          // toolbar isn't customized
          if (i == 0)
            // new button is in the defaultset, nothing to migrate
            break;
          continue;
        }
        if (/(?:^|,)unified-back-forward-button(?:$|,)/.test(currentset))
          // new button is already there, nothing to migrate
          break;
        if (/(?:^|,)back-button(?:$|,)/.test(currentset)) {
          let newset = currentset.replace(/(^|,)back-button($|,)/,
                                          "$1unified-back-forward-button,back-button$2")
          this._setPersist(toolbar, currentsetResource, newset);
          // done migrating
          break;
        }
      }

      // force the RDF to be saved
      if (this._dirty)
        this._dataSource.QueryInterface(Ci.nsIRDFRemoteDataSource).Flush();

      // free up the RDF service
      this._rdf = null;
      this._dataSource = null;

      // update the migration version
      this._prefs.setIntPref("browser.migration.version", 1);
    }
  },

  _getPersist: function bg__getPersist(aSource, aProperty) {
    var target = this._dataSource.GetTarget(aSource, aProperty, true);
    if (target instanceof Ci.nsIRDFLiteral)
      return target.Value;
    return null;
  },

  _setPersist: function bg__setPersist(aSource, aProperty, aTarget) {
    this._dirty = true;
    try {
      var oldTarget = this._dataSource.GetTarget(aSource, aProperty, true);
      if (oldTarget) {
        if (aTarget)
          this._dataSource.Change(aSource, aProperty, oldTarget, this._rdf.GetLiteral(aTarget));
        else
          this._dataSource.Unassert(aSource, aProperty, oldTarget);
      }
      else {
        this._dataSource.Assert(aSource, aProperty, this._rdf.GetLiteral(aTarget), true);
      }
    }
    catch(ex) {}
  },

  // ------------------------------
  // public nsIBrowserGlue members
  // ------------------------------
  
  sanitize: function(aParentWindow) 
  {
    this.Sanitizer.sanitize(aParentWindow);
  },

  ensurePlacesDefaultQueriesInitialized: function() {
    const SMART_BOOKMARKS_VERSION = 1;
    const SMART_BOOKMARKS_ANNO = "Places/SmartBookmark";
    const SMART_BOOKMARKS_PREF = "browser.places.smartBookmarksVersion";

    // XXX should this be a pref?  see bug #399268
    const MAX_RESULTS = 10;

    // get current smart bookmarks version
    // By default, if the pref is not set up, we must create Smart Bookmarks
    var smartBookmarksCurrentVersion = 0;
    try {
      smartBookmarksCurrentVersion = this._prefs.getIntPref(SMART_BOOKMARKS_PREF);
    } catch(ex) {}

    // bail out if we don't have to create or update Smart Bookmarks
    if (smartBookmarksCurrentVersion == -1 ||
        smartBookmarksCurrentVersion >= SMART_BOOKMARKS_VERSION)
      return;

    var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
                getService(Ci.nsINavBookmarksService);
    var annosvc = Cc["@mozilla.org/browser/annotation-service;1"].
                  getService(Ci.nsIAnnotationService);
    var strings = this._placesBundle;

    var callback = {
      _placesBundle: strings,

      _uri: function(aSpec) {
        return Cc["@mozilla.org/network/io-service;1"].
               getService(Ci.nsIIOService).
               newURI(aSpec, null, null);
      },

      runBatched: function() {
        var smartBookmarks = [];
        var bookmarksMenuIndex = 0;
        var bookmarksToolbarIndex = 0;

        // MOST VISITED
        var smart = {queryId: "MostVisited", // don't change this
                     itemId: null,
                     title: this._placesBundle.GetStringFromName("mostVisitedTitle"),
                     uri: this._uri("place:queryType=" +
                                    Ci.nsINavHistoryQueryOptions.QUERY_TYPE_HISTORY +
                                    "&sort=" +
                                    Ci.nsINavHistoryQueryOptions.SORT_BY_VISITCOUNT_DESCENDING +
                                    "&maxResults=" + MAX_RESULTS),
                     parent: bmsvc.toolbarFolder,
                     position: bookmarksToolbarIndex++};
        smartBookmarks.push(smart);

        // RECENTLY BOOKMARKED
        smart = {queryId: "RecentlyBookmarked", // don't change this
                 itemId: null,
                 title: this._placesBundle.GetStringFromName("recentlyBookmarkedTitle"),
                 uri: this._uri("place:folder=BOOKMARKS_MENU" +
                                "&folder=UNFILED_BOOKMARKS" +
                                "&folder=TOOLBAR" +
                                "&queryType=" +
                                Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS +
                                "&sort=" +
                                Ci.nsINavHistoryQueryOptions.SORT_BY_DATEADDED_DESCENDING +
                                "&excludeItemIfParentHasAnnotation=livemark%2FfeedURI" +
                                "&maxResults=" + MAX_RESULTS +
                                "&excludeQueries=1"),
                 parent: bmsvc.bookmarksMenuFolder,
                 position: bookmarksMenuIndex++};
        smartBookmarks.push(smart);

        // RECENT TAGS
        smart = {queryId: "RecentTags", // don't change this
                 itemId: null,
                 title: this._placesBundle.GetStringFromName("recentTagsTitle"),
                 uri: this._uri("place:"+
                    "type=" +
                    Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAG_QUERY +
                    "&sort=" +
                    Ci.nsINavHistoryQueryOptions.SORT_BY_LASTMODIFIED_DESCENDING +
                    "&maxResults=" + MAX_RESULTS),
                 parent: bmsvc.bookmarksMenuFolder,
                 position: bookmarksMenuIndex++};
        smartBookmarks.push(smart);

        var smartBookmarkItemIds = annosvc.getItemsWithAnnotation(SMART_BOOKMARKS_ANNO, {});
        // set current itemId, parent and position if Smart Bookmark exists
        for each(var itemId in smartBookmarkItemIds) {
          var queryId = annosvc.getItemAnnotation(itemId, SMART_BOOKMARKS_ANNO);
          for (var i = 0; i < smartBookmarks.length; i++){
            if (smartBookmarks[i].queryId == queryId) {
              smartBookmarks[i].itemId = itemId;
              smartBookmarks[i].parent = bmsvc.getFolderIdForItem(itemId);
              smartBookmarks[i].position = bmsvc.getItemIndex(itemId);
              // remove current item, since it will be replaced
              bmsvc.removeItem(itemId);
              break;
            }
            // We don't remove old Smart Bookmarks because user could still
            // find them useful, or could have personalized them.
            // Instead we remove the Smart Bookmark annotation.
            if (i == smartBookmarks.length - 1)
              annosvc.removeItemAnnotation(itemId, SMART_BOOKMARKS_ANNO);
          }
        }

        // create smart bookmarks
        for each(var smartBookmark in smartBookmarks) {
          smartBookmark.itemId = bmsvc.insertBookmark(smartBookmark.parent,
                                                      smartBookmark.uri,
                                                      smartBookmark.position,
                                                      smartBookmark.title);
          annosvc.setItemAnnotation(smartBookmark.itemId,
                                    SMART_BOOKMARKS_ANNO, smartBookmark.queryId,
                                    0, annosvc.EXPIRE_NEVER);
        }
        
        // If we are creating all Smart Bookmarks from ground up, add a
        // separator below them in the bookmarks menu.
        if (smartBookmarkItemIds.length == 0)
          bmsvc.insertSeparator(bmsvc.bookmarksMenuFolder, bookmarksMenuIndex);
      }
    };

    try {
      bmsvc.runInBatchMode(callback, null);
    }
    catch(ex) {
      Components.utils.reportError(ex);
    }
    finally {
      this._prefs.setIntPref(SMART_BOOKMARKS_PREF, SMART_BOOKMARKS_VERSION);
      this._prefs.QueryInterface(Ci.nsIPrefService).savePrefFile(null);
    }
  },

#ifdef XP_UNIX
#ifndef XP_MACOSX
#define BROKEN_WM_Z_ORDER
#endif
#endif
#ifdef XP_OS2
#define BROKEN_WM_Z_ORDER
#endif

  // this returns the most recent non-popup browser window
  getMostRecentBrowserWindow : function ()
  {
    var wm = Cc["@mozilla.org/appshell/window-mediator;1"].
             getService(Components.interfaces.nsIWindowMediator);

#ifdef BROKEN_WM_Z_ORDER
    var win = wm.getMostRecentWindow("navigator:browser", true);

    // if we're lucky, this isn't a popup, and we can just return this
    if (win && win.document.documentElement.getAttribute("chromehidden")) {
      win = null;
      var windowList = wm.getEnumerator("navigator:browser", true);
      // this is oldest to newest, so this gets a bit ugly
      while (windowList.hasMoreElements()) {
        var nextWin = windowList.getNext();
        if (!nextWin.document.documentElement.getAttribute("chromehidden"))
          win = nextWin;
      }
    }
#else
    var windowList = wm.getZOrderDOMWindowEnumerator("navigator:browser", true);
    if (!windowList.hasMoreElements())
      return null;

    var win = windowList.getNext();
    while (win.document.documentElement.getAttribute("chromehidden")) {
      if (!windowList.hasMoreElements())
        return null;

      win = windowList.getNext();
    }
#endif

    return win;
  },


  // for XPCOM
  classDescription: "Firefox Browser Glue Service",
  classID:          Components.ID("{eab9012e-5f74-4cbc-b2b5-a590235513cc}"),
  contractID:       "@mozilla.org/browser/browserglue;1",

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsIBrowserGlue]),

  // redefine the default factory for XPCOMUtils
  _xpcom_factory: BrowserGlueServiceFactory,

  // get this contractID registered for certain categories via XPCOMUtils
  _xpcom_categories: [
    // make BrowserGlue a startup observer
    { category: "app-startup", service: true }
  ]
}

function GeolocationPrompt() {}

GeolocationPrompt.prototype = {
  classDescription: "Geolocation Prompting Component",
  classID:          Components.ID("{C6E8C44D-9F39-4AF7-BCC0-76E38A8310F5}"),
  contractID:       "@mozilla.org/geolocation/prompt;1",

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIGeolocationPrompt]),
 
  prompt: function(request) {

    function getChromeWindow(aWindow) {
      var chromeWin = aWindow 
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIWebNavigation)
        .QueryInterface(Ci.nsIDocShellTreeItem)
        .rootTreeItem
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindow)
        .QueryInterface(Ci.nsIDOMChromeWindow);
      return chromeWin;
    }

    var requestingWindow = request.requestingWindow.top;
    var tabbrowser = getChromeWindow(requestingWindow).wrappedJSObject.gBrowser;
    var browser = tabbrowser.getBrowserForDocument(requestingWindow.document);
    var notificationBox = tabbrowser.getNotificationBox(browser);

    var notification = notificationBox.getNotificationWithValue("geolocation");
    if (!notification) {
      var bundleService = Cc["@mozilla.org/intl/stringbundle;1"].getService(Ci.nsIStringBundleService);
      var browserBundle = bundleService.createBundle("chrome://browser/locale/browser.properties");

      var buttons = [{
        label: browserBundle.GetStringFromName("geolocation.exactLocation"),
        accessKey: browserBundle.GetStringFromName("geolocation.exactLocationKey"),
        callback: function() request.allow() ,
        },
        {
        label: browserBundle.GetStringFromName("geolocation.nothingLocation"),
        accessKey: browserBundle.GetStringFromName("geolocation.nothingLocationKey"),
        callback: function() request.cancel() ,
        }];
      
      var message = browserBundle.formatStringFromName("geolocation.requestMessage",
                                                       [request.requestingURI.spec], 1);      
      notificationBox.appendNotification(message,
                                         "geolocation",
                                         "chrome://browser/skin/Info.png",
                                         notificationBox.PRIORITY_INFO_HIGH,
                                         buttons);
    }
  },
};


//module initialization
function NSGetModule(aCompMgr, aFileSpec) {
  return XPCOMUtils.generateModule([BrowserGlue, GeolocationPrompt]);
}
