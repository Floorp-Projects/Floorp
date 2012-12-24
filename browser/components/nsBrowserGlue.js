#filter substitution
# -*- indent-tabs-mode: nil -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;
const Cu = Components.utils;

const XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/SignInToWebsite.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "UserAgentOverrides",
                                  "resource://gre/modules/UserAgentOverrides.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "BookmarkHTMLUtils",
                                  "resource://gre/modules/BookmarkHTMLUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "webappsUI",
                                  "resource:///modules/webappsUI.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PageThumbs",
                                  "resource:///modules/PageThumbs.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NewTabUtils",
                                  "resource:///modules/NewTabUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "BrowserNewTabPreloader",
                                  "resource:///modules/BrowserNewTabPreloader.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PdfJs",
                                  "resource://pdf.js/PdfJs.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "webrtcUI",
                                  "resource:///modules/webrtcUI.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "KeywordURLResetPrompter",
                                  "resource:///modules/KeywordURLResetPrompter.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "RecentWindow",
                                  "resource:///modules/RecentWindow.jsm");

const PREF_PLUGINS_NOTIFYUSER = "plugins.update.notifyUser";
const PREF_PLUGINS_UPDATEURL  = "plugins.update.url";

// We try to backup bookmarks at idle times, to avoid doing that at shutdown.
// Number of idle seconds before trying to backup bookmarks.  15 minutes.
const BOOKMARKS_BACKUP_IDLE_TIME = 15 * 60;
// Minimum interval in milliseconds between backups.
const BOOKMARKS_BACKUP_INTERVAL = 86400 * 1000;
// Maximum number of backups to create.  Old ones will be purged.
const BOOKMARKS_BACKUP_MAX_BACKUPS = 10;

// Factory object
const BrowserGlueServiceFactory = {
  _instance: null,
  createInstance: function BGSF_createInstance(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return this._instance == null ?
      this._instance = new BrowserGlue() : this._instance;
  }
};

// Constructor

function BrowserGlue() {
  XPCOMUtils.defineLazyServiceGetter(this, "_idleService",
                                     "@mozilla.org/widget/idleservice;1",
                                     "nsIIdleService");

  XPCOMUtils.defineLazyGetter(this, "_distributionCustomizer", function() {
                                Cu.import("resource:///modules/distribution.js");
                                return new DistributionCustomizer();
                              });

  XPCOMUtils.defineLazyGetter(this, "_sanitizer",
    function() {
      let sanitizerScope = {};
      Services.scriptloader.loadSubScript("chrome://browser/content/sanitize.js", sanitizerScope);
      return sanitizerScope.Sanitizer;
    });

  this._init();
}

#ifndef XP_MACOSX
# OS X has the concept of zero-window sessions and therefore ignores the
# browser-lastwindow-close-* topics.
#define OBSERVE_LASTWINDOW_CLOSE_TOPICS 1
#endif

BrowserGlue.prototype = {
  _saveSession: false,
  _isIdleObserver: false,
  _isPlacesInitObserver: false,
  _isPlacesLockedObserver: false,
  _isPlacesShutdownObserver: false,
  _isPlacesDatabaseLocked: false,
  _migrationImportsDefaultBookmarks: false,

  _setPrefToSaveSession: function BG__setPrefToSaveSession(aForce) {
    if (!this._saveSession && !aForce)
      return;

    Services.prefs.setBoolPref("browser.sessionstore.resume_session_once", true);

    // This method can be called via [NSApplication terminate:] on Mac, which
    // ends up causing prefs not to be flushed to disk, so we need to do that
    // explicitly here. See bug 497652.
    Services.prefs.savePrefFile(null);
  },

#ifdef MOZ_SERVICES_SYNC
  _setSyncAutoconnectDelay: function BG__setSyncAutoconnectDelay() {
    // Assume that a non-zero value for services.sync.autoconnectDelay should override
    if (Services.prefs.prefHasUserValue("services.sync.autoconnectDelay")) {
      let prefDelay = Services.prefs.getIntPref("services.sync.autoconnectDelay");

      if (prefDelay > 0)
        return;
    }

    // delays are in seconds
    const MAX_DELAY = 300;
    let delay = 3;
    let browserEnum = Services.wm.getEnumerator("navigator:browser");
    while (browserEnum.hasMoreElements()) {
      delay += browserEnum.getNext().gBrowser.tabs.length;
    }
    delay = delay <= MAX_DELAY ? delay : MAX_DELAY;

    Cu.import("resource://services-sync/main.js");
    Weave.Service.scheduler.delayedAutoConnect(delay);
  },
#endif

  // nsIObserver implementation 
  observe: function BG_observe(subject, topic, data) {
    switch (topic) {
      case "xpcom-shutdown":
        this._dispose();
        break;
      case "prefservice:after-app-defaults":
        this._onAppDefaults();
        break;
      case "final-ui-startup":
        this._onProfileStartup();
        break;
      case "browser-delayed-startup-finished":
        this._onFirstWindowLoaded();
        Services.obs.removeObserver(this, "browser-delayed-startup-finished");
        break;
      case "sessionstore-windows-restored":
        this._onWindowsRestored();
        break;
      case "browser:purge-session-history":
        // reset the console service's error buffer
        Services.console.logStringMessage(null); // clear the console (in case it's open)
        Services.console.reset();
        break;
      case "quit-application-requested":
        this._onQuitRequest(subject, data);
        break;
      case "quit-application-granted":
        // This pref must be set here because SessionStore will use its value
        // on quit-application.
        this._setPrefToSaveSession();
        try {
          let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].
                           getService(Ci.nsIAppStartup);
          appStartup.trackStartupCrashEnd();
        } catch (e) {
          Cu.reportError("Could not end startup crash tracking in quit-application-granted: " + e);
        }
        break;
#ifdef OBSERVE_LASTWINDOW_CLOSE_TOPICS
      case "browser-lastwindow-close-requested":
        // The application is not actually quitting, but the last full browser
        // window is about to be closed.
        this._onQuitRequest(subject, "lastwindow");
        break;
      case "browser-lastwindow-close-granted":
        this._setPrefToSaveSession();
        break;
#endif
#ifdef MOZ_SERVICES_SYNC
      case "weave:service:ready":
        this._setSyncAutoconnectDelay();
        break;
      case "weave:engine:clients:display-uri":
        this._onDisplaySyncURI(subject);
        break;
#endif
      case "session-save":
        this._setPrefToSaveSession(true);
        subject.QueryInterface(Ci.nsISupportsPRBool);
        subject.data = true;
        break;
      case "places-init-complete":
        if (!this._migrationImportsDefaultBookmarks)
          this._initPlaces(false);

        Services.obs.removeObserver(this, "places-init-complete");
        this._isPlacesInitObserver = false;
        // no longer needed, since history was initialized completely.
        Services.obs.removeObserver(this, "places-database-locked");
        this._isPlacesLockedObserver = false;
        break;
      case "places-database-locked":
        this._isPlacesDatabaseLocked = true;
        // Stop observing, so further attempts to load history service
        // will not show the prompt.
        Services.obs.removeObserver(this, "places-database-locked");
        this._isPlacesLockedObserver = false;
        break;
      case "places-shutdown":
        if (this._isPlacesShutdownObserver) {
          Services.obs.removeObserver(this, "places-shutdown");
          this._isPlacesShutdownObserver = false;
        }
        // places-shutdown is fired when the profile is about to disappear.
        this._onProfileShutdown();
        break;
      case "idle":
        if (this._idleService.idleTime > BOOKMARKS_BACKUP_IDLE_TIME * 1000)
          this._backupBookmarks();
        break;
      case "distribution-customization-complete":
        Services.obs.removeObserver(this, "distribution-customization-complete");
        // Customization has finished, we don't need the customizer anymore.
        delete this._distributionCustomizer;
        break;
      case "browser-glue-test": // used by tests
        if (data == "post-update-notification") {
          if (Services.prefs.prefHasUserValue("app.update.postupdate"))
            this._showUpdateNotification();
        }
        else if (data == "force-ui-migration") {
          this._migrateUI();
        }
        else if (data == "force-distribution-customization") {
          this._distributionCustomizer.applyPrefDefaults();
          this._distributionCustomizer.applyCustomizations();
          // To apply distribution bookmarks use "places-init-complete".
        }
        else if (data == "force-places-init") {
          this._initPlaces(false);
        }
        break;
      case "defaultURIFixup-using-keyword-pref":
        if (KeywordURLResetPrompter.shouldPrompt) {
          let keywordURI = subject.QueryInterface(Ci.nsIURI);
          KeywordURLResetPrompter.prompt(this.getMostRecentBrowserWindow(),
                                         keywordURI);
        }
        break;
      case "initial-migration-will-import-default-bookmarks":
        this._migrationImportsDefaultBookmarks = true;
        break;
      case "initial-migration-did-import-default-bookmarks":
        this._initPlaces(true);
        break;
      case "handle-xul-text-link":
        let linkHandled = subject.QueryInterface(Ci.nsISupportsPRBool);
        if (!linkHandled.data) {
          let win = this.getMostRecentBrowserWindow();
          if (win) {
            win.openUILinkIn(data, "tab");
            linkHandled.data = true;
          }
        }
        break;
    }
  }, 

  // initialization (called on application startup) 
  _init: function BG__init() {
    let os = Services.obs;
    os.addObserver(this, "xpcom-shutdown", false);
    os.addObserver(this, "prefservice:after-app-defaults", false);
    os.addObserver(this, "final-ui-startup", false);
    os.addObserver(this, "browser-delayed-startup-finished", false);
    os.addObserver(this, "sessionstore-windows-restored", false);
    os.addObserver(this, "browser:purge-session-history", false);
    os.addObserver(this, "quit-application-requested", false);
    os.addObserver(this, "quit-application-granted", false);
#ifdef OBSERVE_LASTWINDOW_CLOSE_TOPICS
    os.addObserver(this, "browser-lastwindow-close-requested", false);
    os.addObserver(this, "browser-lastwindow-close-granted", false);
#endif
#ifdef MOZ_SERVICES_SYNC
    os.addObserver(this, "weave:service:ready", false);
    os.addObserver(this, "weave:engine:clients:display-uri", false);
#endif
    os.addObserver(this, "session-save", false);
    os.addObserver(this, "places-init-complete", false);
    this._isPlacesInitObserver = true;
    os.addObserver(this, "places-database-locked", false);
    this._isPlacesLockedObserver = true;
    os.addObserver(this, "distribution-customization-complete", false);
    os.addObserver(this, "places-shutdown", false);
    this._isPlacesShutdownObserver = true;
    os.addObserver(this, "defaultURIFixup-using-keyword-pref", false);
    os.addObserver(this, "handle-xul-text-link", false);
  },

  // cleanup (called on application shutdown)
  _dispose: function BG__dispose() {
    let os = Services.obs;
    os.removeObserver(this, "xpcom-shutdown");
    os.removeObserver(this, "prefservice:after-app-defaults");
    os.removeObserver(this, "final-ui-startup");
    os.removeObserver(this, "sessionstore-windows-restored");
    os.removeObserver(this, "browser:purge-session-history");
    os.removeObserver(this, "quit-application-requested");
    os.removeObserver(this, "quit-application-granted");
#ifdef OBSERVE_LASTWINDOW_CLOSE_TOPICS
    os.removeObserver(this, "browser-lastwindow-close-requested");
    os.removeObserver(this, "browser-lastwindow-close-granted");
#endif
#ifdef MOZ_SERVICES_SYNC
    os.removeObserver(this, "weave:service:ready", false);
    os.removeObserver(this, "weave:engine:clients:display-uri", false);
#endif
    os.removeObserver(this, "session-save");
    if (this._isIdleObserver)
      this._idleService.removeIdleObserver(this, BOOKMARKS_BACKUP_IDLE_TIME);
    if (this._isPlacesInitObserver)
      os.removeObserver(this, "places-init-complete");
    if (this._isPlacesLockedObserver)
      os.removeObserver(this, "places-database-locked");
    if (this._isPlacesShutdownObserver)
      os.removeObserver(this, "places-shutdown");
    os.removeObserver(this, "defaultURIFixup-using-keyword-pref");
    os.removeObserver(this, "handle-xul-text-link");
    UserAgentOverrides.uninit();
    webappsUI.uninit();
    SignInToWebsiteUX.uninit();
    webrtcUI.uninit();
  },

  _onAppDefaults: function BG__onAppDefaults() {
    // apply distribution customizations (prefs)
    // other customizations are applied in _onProfileStartup()
    this._distributionCustomizer.applyPrefDefaults();
  },

  // profile startup handler (contains profile initialization routines)
  _onProfileStartup: function BG__onProfileStartup() {
    this._sanitizer.onStartup();
    // check if we're in safe mode
    if (Services.appinfo.inSafeMode) {
      Services.ww.openWindow(null, "chrome://browser/content/safeMode.xul", 
                             "_blank", "chrome,centerscreen,modal,resizable=no", null);
    }

    // apply distribution customizations
    // prefs are applied in _onAppDefaults()
    this._distributionCustomizer.applyCustomizations();

    // handle any UI migration
    this._migrateUI();

    this._setUpUserAgentOverrides();

    webappsUI.init();
    PageThumbs.init();
    NewTabUtils.init();
    BrowserNewTabPreloader.init();
    SignInToWebsiteUX.init();
    PdfJs.init();
    webrtcUI.init();

    Services.obs.notifyObservers(null, "browser-ui-startup-complete", "");
  },

  _setUpUserAgentOverrides: function BG__setUpUserAgentOverrides() {
    UserAgentOverrides.init();

    if (Services.prefs.getBoolPref("general.useragent.complexOverride.moodle")) {
      UserAgentOverrides.addComplexOverride(function (aHttpChannel, aOriginalUA) {
        let cookies;
        try {
          cookies = aHttpChannel.getRequestHeader("Cookie");
        } catch (e) { /* no cookie sent */ }
        if (cookies && cookies.indexOf("MoodleSession") > -1)
          return aOriginalUA.replace(/Gecko\/[^ ]*/, "Gecko/20100101");
        return null;
      });
    }
  },

  // the first browser window has finished initializing
  _onFirstWindowLoaded: function BG__onFirstWindowLoaded() {
#ifdef XP_WIN
    // For windows seven, initialize the jump list module.
    const WINTASKBAR_CONTRACTID = "@mozilla.org/windows-taskbar;1";
    if (WINTASKBAR_CONTRACTID in Cc &&
        Cc[WINTASKBAR_CONTRACTID].getService(Ci.nsIWinTaskbar).available) {
      let temp = {};
      Cu.import("resource://gre/modules/WindowsJumpLists.jsm", temp);
      temp.WinTaskbarJumpList.startup();
    }
#endif
  },

  // profile shutdown handler (contains profile cleanup routines)
  _onProfileShutdown: function BG__onProfileShutdown() {
    this._shutdownPlaces();
    this._sanitizer.onShutdown();
    PageThumbs.uninit();
    BrowserNewTabPreloader.uninit();
  },

  // All initial windows have opened.
  _onWindowsRestored: function BG__onWindowsRestored() {
    // Show about:rights notification, if needed.
    if (this._shouldShowRights()) {
      this._showRightsNotification();
#ifdef MOZ_TELEMETRY_REPORTING
    } else {
      // Only show telemetry notification when about:rights notification is not shown.
      this._showTelemetryNotification();
#endif
    }

    // Show update notification, if needed.
    if (Services.prefs.prefHasUserValue("app.update.postupdate"))
      this._showUpdateNotification();

    // Load the "more info" page for a locked places.sqlite
    // This property is set earlier by places-database-locked topic.
    if (this._isPlacesDatabaseLocked) {
      this._showPlacesLockedNotificationBox();
    }

    // If there are plugins installed that are outdated, and the user hasn't
    // been warned about them yet, open the plugins update page.
    if (Services.prefs.getBoolPref(PREF_PLUGINS_NOTIFYUSER))
      this._showPluginUpdatePage();

    // For any add-ons that were installed disabled and can be enabled offer
    // them to the user.
    let changedIDs = AddonManager.getStartupChanges(AddonManager.STARTUP_CHANGE_INSTALLED);
    if (changedIDs.length > 0) {
      let win = this.getMostRecentBrowserWindow();
      AddonManager.getAddonsByIDs(changedIDs, function(aAddons) {
        aAddons.forEach(function(aAddon) {
          // If the add-on isn't user disabled or can't be enabled then skip it.
          if (!aAddon.userDisabled || !(aAddon.permissions & AddonManager.PERM_CAN_ENABLE))
            return;

          win.openUILinkIn("about:newaddon?id=" + aAddon.id, "tab");
        })
      });
    }

    let keywordURLUserSet = Services.prefs.prefHasUserValue("keyword.URL");
    Services.telemetry.getHistogramById("FX_KEYWORD_URL_USERSET").add(keywordURLUserSet);

    // Perform default browser checking.
    var shell;
    try {
      shell = Components.classes["@mozilla.org/browser/shell-service;1"]
        .getService(Components.interfaces.nsIShellService);
    } catch (e) { }
    if (shell) {
#ifdef DEBUG
      let shouldCheck = false;
#else
      let shouldCheck = shell.shouldCheckDefaultBrowser;
#endif
      let willRecoverSession = false;
      try {
        let ss = Cc["@mozilla.org/browser/sessionstartup;1"].
                 getService(Ci.nsISessionStartup);
        willRecoverSession =
          (ss.sessionType == Ci.nsISessionStartup.RECOVER_SESSION);
      }
      catch (ex) { /* never mind; suppose SessionStore is broken */ }
      if (shouldCheck &&
          !shell.isDefaultBrowser(true, false) &&
          !willRecoverSession) {
        Services.tm.mainThread.dispatch(function() {
          var win = this.getMostRecentBrowserWindow();
          var brandBundle = win.document.getElementById("bundle_brand");
          var shellBundle = win.document.getElementById("bundle_shell");
  
          var brandShortName = brandBundle.getString("brandShortName");
          var promptTitle = shellBundle.getString("setDefaultBrowserTitle");
          var promptMessage = shellBundle.getFormattedString("setDefaultBrowserMessage",
                                                             [brandShortName]);
          var checkboxLabel = shellBundle.getFormattedString("setDefaultBrowserDontAsk",
                                                             [brandShortName]);
          var checkEveryTime = { value: shouldCheck };
          var ps = Services.prompt;
          var rv = ps.confirmEx(win, promptTitle, promptMessage,
                                ps.STD_YES_NO_BUTTONS,
                                null, null, null, checkboxLabel, checkEveryTime);
          if (rv == 0) {
            var claimAllTypes = true;
#ifdef XP_WIN
            try {
              // In Windows 8, the UI for selecting default protocol is much
              // nicer than the UI for setting file type associations. So we
              // only show the protocol association screen on Windows 8.
              // Windows 8 is version 6.2.
              let version = Cc["@mozilla.org/system-info;1"]
                              .getService(Ci.nsIPropertyBag2)
                              .getProperty("version");
              claimAllTypes = (parseFloat(version) < 6.2);
            } catch (ex) { }
#endif
            shell.setDefaultBrowser(claimAllTypes, false);
          }
          shell.shouldCheckDefaultBrowser = checkEveryTime.value;
        }.bind(this), Ci.nsIThread.DISPATCH_NORMAL);
      }
    }
  },

  _onQuitRequest: function BG__onQuitRequest(aCancelQuit, aQuitType) {
    // If user has already dismissed quit request, then do nothing
    if ((aCancelQuit instanceof Ci.nsISupportsPRBool) && aCancelQuit.data)
      return;

    // There are several cases where we won't show a dialog here:
    // 1. There is only 1 tab open in 1 window
    // 2. The session will be restored at startup, indicated by
    //    browser.startup.page == 3 or browser.sessionstore.resume_session_once == true
    // 3. browser.warnOnQuit == false
    // 4. The browser is currently in Private Browsing mode
    //
    // Otherwise these are the conditions and the associated dialogs that will be shown:
    // 1. aQuitType == "lastwindow" or "quit" and browser.showQuitWarning == true
    //    - The quit dialog will be shown
    // 2. aQuitType == "restart" && browser.warnOnRestart == true
    //    - The restart dialog will be shown
    // 3. aQuitType == "lastwindow" && browser.tabs.warnOnClose == true
    //    - The "closing multiple tabs" dialog will be shown
    //
    // aQuitType == "lastwindow" is overloaded. "lastwindow" is used to indicate
    // "the last window is closing but we're not quitting (a non-browser window is open)"
    // and also "we're quitting by closing the last window".

    var windowcount = 0;
    var pagecount = 0;
    var browserEnum = Services.wm.getEnumerator("navigator:browser");
    let allWindowsPrivate = true;
    while (browserEnum.hasMoreElements()) {
      windowcount++;

      var browser = browserEnum.getNext();
      if (!PrivateBrowsingUtils.isWindowPrivate(browser))
        allWindowsPrivate = false;
      var tabbrowser = browser.document.getElementById("content");
      if (tabbrowser)
        pagecount += tabbrowser.browsers.length - tabbrowser._numPinnedTabs;
    }

    this._saveSession = false;
    if (pagecount < 2)
      return;

    if (!aQuitType)
      aQuitType = "quit";

    var showPrompt = false;
    var mostRecentBrowserWindow;

    // browser.warnOnQuit is a hidden global boolean to override all quit prompts
    // browser.showQuitWarning specifically covers quitting
    // browser.warnOnRestart specifically covers app-initiated restarts where we restart the app
    // browser.tabs.warnOnClose is the global "warn when closing multiple tabs" pref

    var sessionWillBeRestored = Services.prefs.getIntPref("browser.startup.page") == 3 ||
                                Services.prefs.getBoolPref("browser.sessionstore.resume_session_once");
    if (sessionWillBeRestored || !Services.prefs.getBoolPref("browser.warnOnQuit"))
      return;

    // On last window close or quit && showQuitWarning, we want to show the
    // quit warning.
    if (aQuitType != "restart" && Services.prefs.getBoolPref("browser.showQuitWarning")) {
      showPrompt = true;
    }
    else if (aQuitType == "restart" && Services.prefs.getBoolPref("browser.warnOnRestart")) {
      showPrompt = true;
    }
    else if (aQuitType == "lastwindow") {
      // If aQuitType is "lastwindow" and we aren't showing the quit warning,
      // we should show the window closing warning instead. warnAboutClosing
      // tabs checks browser.tabs.warnOnClose and returns if it's ok to close
      // the window. It doesn't actually close the window.
      mostRecentBrowserWindow = Services.wm.getMostRecentWindow("navigator:browser");
      aCancelQuit.data = !mostRecentBrowserWindow.gBrowser.warnAboutClosingTabs(true);
      return;
    }

    // Never show a prompt inside private browsing mode
    if (allWindowsPrivate)
      return;

    if (!showPrompt)
      return;

    var quitBundle = Services.strings.createBundle("chrome://browser/locale/quitDialog.properties");
    var brandBundle = Services.strings.createBundle("chrome://branding/locale/brand.properties");

    var appName = brandBundle.GetStringFromName("brandShortName");
    var quitTitleString = (aQuitType == "restart" ? "restart" : "quit") + "DialogTitle";
    var quitDialogTitle = quitBundle.formatStringFromName(quitTitleString, [appName], 1);

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

    var promptService = Services.prompt;

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

    // This wouldn't have been set above since we shouldn't be here for
    // aQuitType == "lastwindow"
    mostRecentBrowserWindow = Services.wm.getMostRecentWindow("navigator:browser");

    var buttonChoice =
      promptService.confirmEx(mostRecentBrowserWindow, quitDialogTitle, message,
                              flags, button0Title, button1Title, button2Title,
                              neverAskText, neverAsk);

    switch (buttonChoice) {
    case 2: // Quit
      if (neverAsk.value)
        Services.prefs.setBoolPref("browser.showQuitWarning", false);
      break;
    case 1: // Cancel
      aCancelQuit.QueryInterface(Ci.nsISupportsPRBool);
      aCancelQuit.data = true;
      break;
    case 0: // Save & Quit
      this._saveSession = true;
      if (neverAsk.value) {
        if (aQuitType == "restart")
          Services.prefs.setBoolPref("browser.warnOnRestart", false);
        else {
          // always save state when shutting down
          Services.prefs.setIntPref("browser.startup.page", 3);
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
  _shouldShowRights: function BG__shouldShowRights() {
    // Look for an unconditional override pref. If set, do what it says.
    // (true --> never show, false --> always show)
    try {
      return !Services.prefs.getBoolPref("browser.rights.override");
    } catch (e) { }
    // Ditto, for the legacy EULA pref.
    try {
      return !Services.prefs.getBoolPref("browser.EULA.override");
    } catch (e) { }

#ifndef OFFICIAL_BUILD
    // Non-official builds shouldn't shouldn't show the notification.
    return false;
#endif

    // Look to see if the user has seen the current version or not.
    var currentVersion = Services.prefs.getIntPref("browser.rights.version");
    try {
      return !Services.prefs.getBoolPref("browser.rights." + currentVersion + ".shown");
    } catch (e) { }

    // Legacy: If the user accepted a EULA, we won't annoy them with the
    // equivalent about:rights page until the version changes.
    try {
      return !Services.prefs.getBoolPref("browser.EULA." + currentVersion + ".accepted");
    } catch (e) { }

    // We haven't shown the notification before, so do so now.
    return true;
  },

  _showRightsNotification: function BG__showRightsNotification() {
    // Stick the notification onto the selected tab of the active browser window.
    var win = this.getMostRecentBrowserWindow();
    var notifyBox = win.gBrowser.getNotificationBox();

    var brandBundle  = Services.strings.createBundle("chrome://branding/locale/brand.properties");
    var rightsBundle = Services.strings.createBundle("chrome://global/locale/aboutRights.properties");

    var buttonLabel      = rightsBundle.GetStringFromName("buttonLabel");
    var buttonAccessKey  = rightsBundle.GetStringFromName("buttonAccessKey");
    var productName      = brandBundle.GetStringFromName("brandFullName");
    var notifyRightsText = rightsBundle.formatStringFromName("notifyRightsText", [productName], 1);

    var buttons = [
                    {
                      label:     buttonLabel,
                      accessKey: buttonAccessKey,
                      popup:     null,
                      callback: function(aNotificationBar, aButton) {
                        win.openUILinkIn("about:rights", "tab");
                      }
                    }
                  ];

    // Set pref to indicate we've shown the notification.
    var currentVersion = Services.prefs.getIntPref("browser.rights.version");
    Services.prefs.setBoolPref("browser.rights." + currentVersion + ".shown", true);

    var notification = notifyBox.appendNotification(notifyRightsText, "about-rights", null, notifyBox.PRIORITY_INFO_LOW, buttons);
    notification.persistence = -1; // Until user closes it
  },

  _showUpdateNotification: function BG__showUpdateNotification() {
    Services.prefs.clearUserPref("app.update.postupdate");

    var um = Cc["@mozilla.org/updates/update-manager;1"].
             getService(Ci.nsIUpdateManager);
    try {
      // If the updates.xml file is deleted then getUpdateAt will throw.
      var update = um.getUpdateAt(0).QueryInterface(Ci.nsIPropertyBag);
    }
    catch (e) {
      // This should never happen.
      Cu.reportError("Unable to find update: " + e);
      return;
    }

    var actions = update.getProperty("actions");
    if (!actions || actions.indexOf("silent") != -1)
      return;

    var formatter = Cc["@mozilla.org/toolkit/URLFormatterService;1"].
                    getService(Ci.nsIURLFormatter);
    var browserBundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");
    var brandBundle = Services.strings.createBundle("chrome://branding/locale/brand.properties");
    var appName = brandBundle.GetStringFromName("brandShortName");

    function getNotifyString(aPropData) {
      var propValue = update.getProperty(aPropData.propName);
      if (!propValue) {
        if (aPropData.prefName)
          propValue = formatter.formatURLPref(aPropData.prefName);
        else if (aPropData.stringParams)
          propValue = browserBundle.formatStringFromName(aPropData.stringName,
                                                         aPropData.stringParams,
                                                         aPropData.stringParams.length);
        else
          propValue = browserBundle.GetStringFromName(aPropData.stringName);
      }
      return propValue;
    }

    if (actions.indexOf("showNotification") != -1) {
      let text = getNotifyString({propName: "notificationText",
                                  stringName: "puNotifyText",
                                  stringParams: [appName]});
      let url = getNotifyString({propName: "notificationURL",
                                 prefName: "startup.homepage_override_url"});
      let label = getNotifyString({propName: "notificationButtonLabel",
                                   stringName: "pu.notifyButton.label"});
      let key = getNotifyString({propName: "notificationButtonAccessKey",
                                 stringName: "pu.notifyButton.accesskey"});

      let win = this.getMostRecentBrowserWindow();
      let notifyBox = win.gBrowser.getNotificationBox();

      let buttons = [
                      {
                        label:     label,
                        accessKey: key,
                        popup:     null,
                        callback: function(aNotificationBar, aButton) {
                          win.openUILinkIn(url, "tab");
                        }
                      }
                    ];

      let notification = notifyBox.appendNotification(text, "post-update-notification",
                                                      null, notifyBox.PRIORITY_INFO_LOW,
                                                      buttons);
      notification.persistence = -1; // Until user closes it
    }

    if (actions.indexOf("showAlert") == -1)
      return;

    let notifier;
    try {
      notifier = Cc["@mozilla.org/alerts-service;1"].
                 getService(Ci.nsIAlertsService);
    }
    catch (e) {
      // nsIAlertsService is not available for this platform
      return;
    }

    let title = getNotifyString({propName: "alertTitle",
                                 stringName: "puAlertTitle",
                                 stringParams: [appName]});
    let text = getNotifyString({propName: "alertText",
                                stringName: "puAlertText",
                                stringParams: [appName]});
    let url = getNotifyString({propName: "alertURL",
                               prefName: "startup.homepage_override_url"});

    var self = this;
    function clickCallback(subject, topic, data) {
      // This callback will be called twice but only once with this topic
      if (topic != "alertclickcallback")
        return;
      let win = self.getMostRecentBrowserWindow();
      win.openUILinkIn(data, "tab");
    }

    try {
      // This will throw NS_ERROR_NOT_AVAILABLE if the notification cannot
      // be displayed per the idl.
      notifier.showAlertNotification(null, title, text,
                                     true, url, clickCallback);
    }
    catch (e) {
    }
  },

#ifdef MOZ_TELEMETRY_REPORTING
  _showTelemetryNotification: function BG__showTelemetryNotification() {
#ifdef MOZ_TELEMETRY_ON_BY_DEFAULT
    const PREF_TELEMETRY_ENABLED = "toolkit.telemetry.enabledPreRelease";
    const PREF_TELEMETRY_DISPLAYED = "toolkit.telemetry.notifiedOptOut";
#else
    const PREF_TELEMETRY_ENABLED  = "toolkit.telemetry.enabled";
    const PREF_TELEMETRY_DISPLAYED = "toolkit.telemetry.prompted";
#endif
    const PREF_TELEMETRY_REJECTED  = "toolkit.telemetry.rejected";
    const PREF_TELEMETRY_INFOURL  = "toolkit.telemetry.infoURL";
    const PREF_TELEMETRY_SERVER_OWNER = "toolkit.telemetry.server_owner";
    // This is used to reprompt/renotify users when privacy message changes
    const TELEMETRY_DISPLAY_REV = @MOZ_TELEMETRY_DISPLAY_REV@;

    // Stick notifications onto the selected tab of the active browser window.
    var win = this.getMostRecentBrowserWindow();
    var tabbrowser = win.gBrowser;
    var notifyBox = tabbrowser.getNotificationBox();

    var browserBundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");
    var brandBundle = Services.strings.createBundle("chrome://branding/locale/brand.properties");
    var productName = brandBundle.GetStringFromName("brandFullName");
    var serverOwner = Services.prefs.getCharPref(PREF_TELEMETRY_SERVER_OWNER);

    function appendTelemetryNotification(message, buttons, hideclose) {
      let notification = notifyBox.appendNotification(message, "telemetry", null,
                                                      notifyBox.PRIORITY_INFO_LOW,
                                                      buttons);
      if (hideclose)
        notification.setAttribute("hideclose", hideclose);
      notification.persistence = -1;  // Until user closes it
      return notification;
    }

    function appendLearnMoreLink(notification) {
      let XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
      let link = notification.ownerDocument.createElementNS(XULNS, "label");
      link.className = "text-link telemetry-text-link";
      link.setAttribute("value", browserBundle.GetStringFromName("telemetryLinkLabel"));
      let description = notification.ownerDocument.getAnonymousElementByAttribute(notification, "anonid", "messageText");
      description.appendChild(link);
      return link;
    }

    /*
     * Display an opt-out notification when telemetry is enabled by default,
     * an opt-in prompt otherwise.
     *
     * But do not display this prompt/notification if:
     *
     * - The last accepted/refused policy (either by accepting the prompt or by
     *   manually flipping the telemetry preference) is already at version
     *   TELEMETRY_DISPLAY_REV.
     */
    var telemetryDisplayed;
    try {
      telemetryDisplayed = Services.prefs.getIntPref(PREF_TELEMETRY_DISPLAYED);
    } catch(e) {}
    if (telemetryDisplayed === TELEMETRY_DISPLAY_REV)
      return;

#ifdef MOZ_TELEMETRY_ON_BY_DEFAULT
    /*
     * Additionally, in opt-out builds, don't display the notification if:
     *
     * - Telemetry is disabled
     * - Telemetry was explicitly refused through the UI
     * - Opt-in telemetry was enabled and this is the first run with opt-out.
     */

    var telemetryEnabled = Services.prefs.getBoolPref(PREF_TELEMETRY_ENABLED);
    if (!telemetryEnabled)
      return;

    // If telemetry was explicitly refused through the UI,
    // also disable opt-out telemetry and bail out.
    var telemetryRejected = false;
    try {
      telemetryRejected = Services.prefs.getBoolPref(PREF_TELEMETRY_REJECTED);
    } catch(e) {}
    if (telemetryRejected) {
      Services.prefs.setBoolPref(PREF_TELEMETRY_ENABLED, false);
      Services.prefs.setIntPref(PREF_TELEMETRY_DISPLAYED, TELEMETRY_DISPLAY_REV);
      return;
    }

    // If opt-in telemetry was enabled and this is the first run with opt-out,
    // don't notify the user.
    var optInTelemetryEnabled = false;
    try {
      optInTelemetryEnabled = Services.prefs.getBoolPref("toolkit.telemetry.enabled");
    } catch(e) {}
    if (optInTelemetryEnabled && telemetryDisplayed === undefined) {
      Services.prefs.setBoolPref(PREF_TELEMETRY_REJECTED, false);
      Services.prefs.setIntPref(PREF_TELEMETRY_DISPLAYED, TELEMETRY_DISPLAY_REV);
      return;
    }

    var telemetryPrompt = browserBundle.formatStringFromName("telemetryOptOutPrompt",
                                                            [productName, serverOwner, productName], 3);
    var buttons = null;
    var hideCloseButton = false;
    function learnModeClickHandler() {
      // Open the learn more url in a new tab
      var url = Services.urlFormatter.formatURLPref("app.support.baseURL");
      url += "how-can-i-help-submitting-performance-data";
      tabbrowser.selectedTab = tabbrowser.addTab(url);
      // Remove the notification on which the user clicked
      notification.parentNode.removeNotification(notification, true);
    }
#else

    // Clear old prefs and reprompt
    Services.prefs.clearUserPref(PREF_TELEMETRY_ENABLED);
    Services.prefs.clearUserPref(PREF_TELEMETRY_REJECTED);

    var telemetryPrompt = browserBundle.formatStringFromName("telemetryOptInPrompt",
                                                            [productName, serverOwner], 2);
    var buttons = [
                    {
                      label:     browserBundle.GetStringFromName("telemetryYesButtonLabel2"),
                      accessKey: browserBundle.GetStringFromName("telemetryYesButtonAccessKey"),
                      popup:     null,
                      callback:  function(aNotificationBar, aButton) {
                        Services.prefs.setBoolPref(PREF_TELEMETRY_ENABLED, true);
                        Services.prefs.setBoolPref(PREF_TELEMETRY_REJECTED, false);
                      }
                    },
                    {
                      label:     browserBundle.GetStringFromName("telemetryNoButtonLabel"),
                      accessKey: browserBundle.GetStringFromName("telemetryNoButtonAccessKey"),
                      popup:     null,
                      callback:  function(aNotificationBar, aButton) {
                        Services.prefs.setBoolPref(PREF_TELEMETRY_REJECTED, true);
                      }
                    }
                  ];

    var hideCloseButton = true;
    function learnModeClickHandler() {
      // Open the learn more url in a new tab
      win.openUILinkIn(Services.prefs.getCharPref(PREF_TELEMETRY_INFOURL), "tab");
      // Remove the notification on which the user clicked
      notification.parentNode.removeNotification(notification, true);
      // Add a new notification to that tab, with no "Learn more" link
      notifyBox = tabbrowser.getNotificationBox();
      appendTelemetryNotification(telemetryPrompt, buttons, true);
    }
#endif

    // Set pref to indicate we've shown the notification.
    Services.prefs.setIntPref(PREF_TELEMETRY_DISPLAYED, TELEMETRY_DISPLAY_REV);

    var notification = appendTelemetryNotification(telemetryPrompt, buttons, hideCloseButton);
    var link = appendLearnMoreLink(notification);
    link.addEventListener('click', learnModeClickHandler, false);
  },
#endif

  _showPluginUpdatePage: function BG__showPluginUpdatePage() {
    Services.prefs.setBoolPref(PREF_PLUGINS_NOTIFYUSER, false);

    var formatter = Cc["@mozilla.org/toolkit/URLFormatterService;1"].
                    getService(Ci.nsIURLFormatter);
    var updateUrl = formatter.formatURLPref(PREF_PLUGINS_UPDATEURL);

    var win = this.getMostRecentBrowserWindow();
    win.openUILinkIn(updateUrl, "tab");
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
  _initPlaces: function BG__initPlaces(aInitialMigrationPerformed) {
    // We must instantiate the history service since it will tell us if we
    // need to import or restore bookmarks due to first-run, corruption or
    // forced migration (due to a major schema change).
    // If the database is corrupt or has been newly created we should
    // import bookmarks.
    var dbStatus = PlacesUtils.history.databaseStatus;
    var importBookmarks = !aInitialMigrationPerformed &&
                          (dbStatus == PlacesUtils.history.DATABASE_STATUS_CREATE ||
                           dbStatus == PlacesUtils.history.DATABASE_STATUS_CORRUPT);

    // Check if user or an extension has required to import bookmarks.html
    var importBookmarksHTML = false;
    try {
      importBookmarksHTML =
        Services.prefs.getBoolPref("browser.places.importBookmarksHTML");
      if (importBookmarksHTML)
        importBookmarks = true;
    } catch(ex) {}

    // Check if Safe Mode or the user has required to restore bookmarks from
    // default profile's bookmarks.html
    var restoreDefaultBookmarks = false;
    try {
      restoreDefaultBookmarks =
        Services.prefs.getBoolPref("browser.bookmarks.restore_default_bookmarks");
      if (restoreDefaultBookmarks) {
        // Ensure that we already have a bookmarks backup for today.
        this._backupBookmarks();
        importBookmarks = true;
      }
    } catch(ex) {}

    // If the user did not require to restore default bookmarks, or import
    // from bookmarks.html, we will try to restore from JSON
    if (importBookmarks && !restoreDefaultBookmarks && !importBookmarksHTML) {
      // get latest JSON backup
      var bookmarksBackupFile = PlacesUtils.backups.getMostRecent("json");
      if (bookmarksBackupFile) {
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

    // If bookmarks are not imported, then initialize smart bookmarks.  This
    // happens during a common startup.
    // Otherwise, if any kind of import runs, smart bookmarks creation should be
    // delayed till the import operations has finished.  Not doing so would
    // cause them to be overwritten by the newly imported bookmarks.
    if (!importBookmarks) {
      // Now apply distribution customized bookmarks.
      // This should always run after Places initialization.
      this._distributionCustomizer.applyBookmarks();
      this.ensurePlacesDefaultQueriesInitialized();
    }
    else {
      // An import operation is about to run.
      // Don't try to recreate smart bookmarks if autoExportHTML is true or
      // smart bookmarks are disabled.
      var autoExportHTML = false;
      try {
        autoExportHTML = Services.prefs.getBoolPref("browser.bookmarks.autoExportHTML");
      } catch(ex) {}
      var smartBookmarksVersion = 0;
      try {
        smartBookmarksVersion = Services.prefs.getIntPref("browser.places.smartBookmarksVersion");
      } catch(ex) {}
      if (!autoExportHTML && smartBookmarksVersion != -1)
        Services.prefs.setIntPref("browser.places.smartBookmarksVersion", 0);

      // Get bookmarks.html file location
      var dirService = Cc["@mozilla.org/file/directory_service;1"].
                       getService(Ci.nsIProperties);

      var bookmarksURI = null;
      if (restoreDefaultBookmarks) {
        // User wants to restore bookmarks.html file from default profile folder
        bookmarksURI = NetUtil.newURI("resource:///defaults/profile/bookmarks.html");
      }
      else {
        var bookmarksFile = dirService.get("BMarks", Ci.nsILocalFile);
        if (bookmarksFile.exists())
          bookmarksURI = NetUtil.newURI(bookmarksFile);
      }

      if (bookmarksURI) {
        // Import from bookmarks.html file.
        try {
          BookmarkHTMLUtils.importFromURL(bookmarksURI.spec, true).then(null,
            function onFailure() {
              Cu.reportError("Bookmarks.html file could be corrupt.");
            }
          ).then(
            function onComplete() {
              // Now apply distribution customized bookmarks.
              // This should always run after Places initialization.
              this._distributionCustomizer.applyBookmarks();
              // Ensure that smart bookmarks are created once the operation is
              // complete.
              this.ensurePlacesDefaultQueriesInitialized();
            }.bind(this)
          );
        } catch (err) {
          Cu.reportError("Bookmarks.html file could be corrupt. " + err);
        }
      }
      else {
        Cu.reportError("Unable to find bookmarks.html file.");
      }

      // Reset preferences, so we won't try to import again at next run
      if (importBookmarksHTML)
        Services.prefs.setBoolPref("browser.places.importBookmarksHTML", false);
      if (restoreDefaultBookmarks)
        Services.prefs.setBoolPref("browser.bookmarks.restore_default_bookmarks",
                                   false);
    }

    // Initialize bookmark archiving on idle.
    // Once a day, either on idle or shutdown, bookmarks are backed up.
    if (!this._isIdleObserver) {
      this._idleService.addIdleObserver(this, BOOKMARKS_BACKUP_IDLE_TIME);
      this._isIdleObserver = true;
    }
  },

  /**
   * Places shut-down tasks
   * - back up bookmarks if needed.
   * - export bookmarks as HTML, if so configured.
   *
   * Note: quit-application-granted notification is received twice
   *       so replace this method with a no-op when first called.
   */
  _shutdownPlaces: function BG__shutdownPlaces() {
    if (this._isIdleObserver) {
      this._idleService.removeIdleObserver(this, BOOKMARKS_BACKUP_IDLE_TIME);
      this._isIdleObserver = false;
    }
    this._backupBookmarks();

    // Backup bookmarks to bookmarks.html to support apps that depend
    // on the legacy format.
    try {
      // If this fails to get the preference value, we don't export.
      if (Services.prefs.getBoolPref("browser.bookmarks.autoExportHTML")) {
        // Exceptionally, since this is a non-default setting and HTML format is
        // discouraged in favor of the JSON backups, we spin the event loop on
        // shutdown, to wait for the export to finish.  We cannot safely spin
        // the event loop on shutdown until we include a watchdog to prevent
        // potential hangs (bug 518683).  The asynchronous shutdown operations
        // will then be handled by a shutdown service (bug 435058).
        let shutdownComplete = false;
        BookmarkHTMLUtils.exportToFile(FileUtils.getFile("BMarks", [])).then(
          function onSuccess() {
            shutdownComplete = true;
          },
          function onFailure() {
            // There is no point in reporting errors since we are shutting down.
            shutdownComplete = true;
          }
        );
        let thread = Services.tm.currentThread;
        while (!shutdownComplete) {
          thread.processNextEvent(true);
        }
      }
    } catch(ex) { /* Don't export */ }
  },

  /**
   * Backup bookmarks if needed.
   */
  _backupBookmarks: function BG__backupBookmarks() {
    let lastBackupFile = PlacesUtils.backups.getMostRecent();

    // Backup bookmarks if there are no backups or the maximum interval between
    // backups elapsed.
    if (!lastBackupFile ||
        new Date() - PlacesUtils.backups.getDateForFile(lastBackupFile) > BOOKMARKS_BACKUP_INTERVAL) {
      let maxBackups = BOOKMARKS_BACKUP_MAX_BACKUPS;
      try {
        maxBackups = Services.prefs.getIntPref("browser.bookmarks.max_backups");
      }
      catch(ex) { /* Use default. */ }

      PlacesUtils.backups.create(maxBackups); // Don't force creation.
    }
  },

  /**
   * Show the notificationBox for a locked places database.
   */
  _showPlacesLockedNotificationBox: function BG__showPlacesLockedNotificationBox() {
    var brandBundle  = Services.strings.createBundle("chrome://branding/locale/brand.properties");
    var applicationName = brandBundle.GetStringFromName("brandShortName");
    var placesBundle = Services.strings.createBundle("chrome://browser/locale/places/places.properties");
    var title = placesBundle.GetStringFromName("lockPrompt.title");
    var text = placesBundle.formatStringFromName("lockPrompt.text", [applicationName], 1);
    var buttonText = placesBundle.GetStringFromName("lockPromptInfoButton.label");
    var accessKey = placesBundle.GetStringFromName("lockPromptInfoButton.accessKey");

    var helpTopic = "places-locked";
    var url = Cc["@mozilla.org/toolkit/URLFormatterService;1"].
              getService(Components.interfaces.nsIURLFormatter).
              formatURLPref("app.support.baseURL");
    url += helpTopic;

    var win = this.getMostRecentBrowserWindow();

    var buttons = [
                    {
                      label:     buttonText,
                      accessKey: accessKey,
                      popup:     null,
                      callback:  function(aNotificationBar, aButton) {
                        win.openUILinkIn(url, "tab");
                      }
                    }
                  ];

    var notifyBox = win.gBrowser.getNotificationBox();
    var notification = notifyBox.appendNotification(text, title, null,
                                                    notifyBox.PRIORITY_CRITICAL_MEDIUM,
                                                    buttons);
    notification.persistence = -1; // Until user closes it
  },

  _migrateUI: function BG__migrateUI() {
    const UI_VERSION = 8;
    const BROWSER_DOCURL = "chrome://browser/content/browser.xul#";
    let currentUIVersion = 0;
    try {
      currentUIVersion = Services.prefs.getIntPref("browser.migration.version");
    } catch(ex) {}
    if (currentUIVersion >= UI_VERSION)
      return;

    this._rdf = Cc["@mozilla.org/rdf/rdf-service;1"].getService(Ci.nsIRDFService);
    this._dataSource = this._rdf.GetDataSource("rdf:local-store");
    this._dirty = false;

    if (currentUIVersion < 2) {
      // This code adds the customizable bookmarks button.
      let currentsetResource = this._rdf.GetResource("currentset");
      let toolbarResource = this._rdf.GetResource(BROWSER_DOCURL + "nav-bar");
      let currentset = this._getPersist(toolbarResource, currentsetResource);
      // Need to migrate only if toolbar is customized and the element is not found.
      if (currentset &&
          currentset.indexOf("bookmarks-menu-button-container") == -1) {
        currentset += ",bookmarks-menu-button-container";
        this._setPersist(toolbarResource, currentsetResource, currentset);
      }
    }

    if (currentUIVersion < 3) {
      // This code merges the reload/stop/go button into the url bar.
      let currentsetResource = this._rdf.GetResource("currentset");
      let toolbarResource = this._rdf.GetResource(BROWSER_DOCURL + "nav-bar");
      let currentset = this._getPersist(toolbarResource, currentsetResource);
      // Need to migrate only if toolbar is customized and all 3 elements are found.
      if (currentset &&
          currentset.indexOf("reload-button") != -1 &&
          currentset.indexOf("stop-button") != -1 &&
          currentset.indexOf("urlbar-container") != -1 &&
          currentset.indexOf("urlbar-container,reload-button,stop-button") == -1) {
        currentset = currentset.replace(/(^|,)reload-button($|,)/, "$1$2")
                               .replace(/(^|,)stop-button($|,)/, "$1$2")
                               .replace(/(^|,)urlbar-container($|,)/,
                                        "$1urlbar-container,reload-button,stop-button$2");
        this._setPersist(toolbarResource, currentsetResource, currentset);
      }
    }

    if (currentUIVersion < 4) {
      // This code moves the home button to the immediate left of the bookmarks menu button.
      let currentsetResource = this._rdf.GetResource("currentset");
      let toolbarResource = this._rdf.GetResource(BROWSER_DOCURL + "nav-bar");
      let currentset = this._getPersist(toolbarResource, currentsetResource);
      // Need to migrate only if toolbar is customized and the elements are found.
      if (currentset &&
          currentset.indexOf("home-button") != -1 &&
          currentset.indexOf("bookmarks-menu-button-container") != -1) {
        currentset = currentset.replace(/(^|,)home-button($|,)/, "$1$2")
                               .replace(/(^|,)bookmarks-menu-button-container($|,)/,
                                        "$1home-button,bookmarks-menu-button-container$2");
        this._setPersist(toolbarResource, currentsetResource, currentset);
      }
    }

    if (currentUIVersion < 5) {
      // This code uncollapses PersonalToolbar if its collapsed status is not
      // persisted, and user customized it or changed default bookmarks.
      let toolbarResource = this._rdf.GetResource(BROWSER_DOCURL + "PersonalToolbar");
      let collapsedResource = this._rdf.GetResource("collapsed");
      let collapsed = this._getPersist(toolbarResource, collapsedResource);
      // If the user does not have a persisted value for the toolbar's
      // "collapsed" attribute, try to determine whether it's customized.
      if (collapsed === null) {
        // We consider the toolbar customized if it has more than
        // 3 children, or if it has a persisted currentset value.
        let currentsetResource = this._rdf.GetResource("currentset");
        let toolbarIsCustomized = !!this._getPersist(toolbarResource,
                                                     currentsetResource);
        function getToolbarFolderCount() {
          let toolbarFolder =
            PlacesUtils.getFolderContents(PlacesUtils.toolbarFolderId).root;
          let toolbarChildCount = toolbarFolder.childCount;
          toolbarFolder.containerOpen = false;
          return toolbarChildCount;
        }

        if (toolbarIsCustomized || getToolbarFolderCount() > 3) {
          this._setPersist(toolbarResource, collapsedResource, "false");
        }
      }
    }

    if (currentUIVersion < 6) {
      // convert tabsontop attribute to pref
      let toolboxResource = this._rdf.GetResource(BROWSER_DOCURL + "navigator-toolbox");
      let tabsOnTopResource = this._rdf.GetResource("tabsontop");
      let tabsOnTopAttribute = this._getPersist(toolboxResource, tabsOnTopResource);
      if (tabsOnTopAttribute)
        Services.prefs.setBoolPref("browser.tabs.onTop", tabsOnTopAttribute == "true");
    }

    // This migration step is executed only if the Downloads Panel feature is
    // enabled.  By default, the feature is enabled only in the Nightly channel.
    // This means that, unless the preference that enables the feature is
    // changed manually, the Downloads button is added to the toolbar only if
    // migration happens while running a build from the Nightly channel.  This
    // migration code will be updated when the feature will be enabled on all
    // channels, see bug 748381 for details.
    if (currentUIVersion < 7 &&
        !Services.prefs.getBoolPref("browser.download.useToolkitUI")) {
      // This code adds the customizable downloads buttons.
      let currentsetResource = this._rdf.GetResource("currentset");
      let toolbarResource = this._rdf.GetResource(BROWSER_DOCURL + "nav-bar");
      let currentset = this._getPersist(toolbarResource, currentsetResource);

      // Since the Downloads button is located in the navigation bar by default,
      // migration needs to happen only if the toolbar was customized using a
      // previous UI version, and the button was not already placed on the
      // toolbar manually.
      if (currentset &&
          currentset.indexOf("downloads-button") == -1) {
        // The element is added either after the search bar or before the home
        // button. As a last resort, the element is added just before the
        // non-customizable window controls.
        if (currentset.indexOf("search-container") != -1) {
          currentset = currentset.replace(/(^|,)search-container($|,)/,
                                          "$1search-container,downloads-button$2")
        } else if (currentset.indexOf("home-button") != -1) {
          currentset = currentset.replace(/(^|,)home-button($|,)/,
                                          "$1downloads-button,home-button$2")
        } else {
          currentset = currentset.replace(/(^|,)window-controls($|,)/,
                                          "$1downloads-button,window-controls$2")
        }
        this._setPersist(toolbarResource, currentsetResource, currentset);
      }
    }

    if (currentUIVersion < 8) {
      // Reset homepage pref for users who have it set to google.com/firefox
      let uri = Services.prefs.getComplexValue("browser.startup.homepage",
                                               Ci.nsIPrefLocalizedString).data;
      if (uri && /^https?:\/\/(www\.)?google(\.\w{2,3}){1,2}\/firefox\/?$/.test(uri)) {
        Services.prefs.clearUserPref("browser.startup.homepage");
      }
    }

    if (this._dirty)
      this._dataSource.QueryInterface(Ci.nsIRDFRemoteDataSource).Flush();

    delete this._rdf;
    delete this._dataSource;

    // Update the migration version.
    Services.prefs.setIntPref("browser.migration.version", UI_VERSION);
  },

  _getPersist: function BG__getPersist(aSource, aProperty) {
    var target = this._dataSource.GetTarget(aSource, aProperty, true);
    if (target instanceof Ci.nsIRDFLiteral)
      return target.Value;
    return null;
  },

  _setPersist: function BG__setPersist(aSource, aProperty, aTarget) {
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

      // Add the entry to the persisted set for this document if it's not there.
      // This code is mostly borrowed from nsXULDocument::Persist.
      let docURL = aSource.ValueUTF8.split("#")[0];
      let docResource = this._rdf.GetResource(docURL);
      let persistResource = this._rdf.GetResource("http://home.netscape.com/NC-rdf#persist");
      if (!this._dataSource.HasAssertion(docResource, persistResource, aSource, true)) {
        this._dataSource.Assert(docResource, persistResource, aSource, true);
      }
    }
    catch(ex) {}
  },

  // ------------------------------
  // public nsIBrowserGlue members
  // ------------------------------

  sanitize: function BG_sanitize(aParentWindow) {
    this._sanitizer.sanitize(aParentWindow);
  },

  ensurePlacesDefaultQueriesInitialized:
  function BG_ensurePlacesDefaultQueriesInitialized() {
    // This is actual version of the smart bookmarks, must be increased every
    // time smart bookmarks change.
    // When adding a new smart bookmark below, its newInVersion property must
    // be set to the version it has been added in, we will compare its value
    // to users' smartBookmarksVersion and add new smart bookmarks without
    // recreating old deleted ones.
    const SMART_BOOKMARKS_VERSION = 4;
    const SMART_BOOKMARKS_ANNO = "Places/SmartBookmark";
    const SMART_BOOKMARKS_PREF = "browser.places.smartBookmarksVersion";

    // TODO bug 399268: should this be a pref?
    const MAX_RESULTS = 10;

    // Get current smart bookmarks version.  If not set, create them.
    let smartBookmarksCurrentVersion = 0;
    try {
      smartBookmarksCurrentVersion = Services.prefs.getIntPref(SMART_BOOKMARKS_PREF);
    } catch(ex) {}

    // If version is current or smart bookmarks are disabled, just bail out.
    if (smartBookmarksCurrentVersion == -1 ||
        smartBookmarksCurrentVersion >= SMART_BOOKMARKS_VERSION) {
      return;
    }

    let batch = {
      runBatched: function BG_EPDQI_runBatched() {
        let menuIndex = 0;
        let toolbarIndex = 0;
        let bundle = Services.strings.createBundle("chrome://browser/locale/places/places.properties");

        let smartBookmarks = {
          MostVisited: {
            title: bundle.GetStringFromName("mostVisitedTitle"),
            uri: NetUtil.newURI("place:sort=" +
                                Ci.nsINavHistoryQueryOptions.SORT_BY_VISITCOUNT_DESCENDING +
                                "&maxResults=" + MAX_RESULTS),
            parent: PlacesUtils.toolbarFolderId,
            position: toolbarIndex++,
            newInVersion: 1
          },
          RecentlyBookmarked: {
            title: bundle.GetStringFromName("recentlyBookmarkedTitle"),
            uri: NetUtil.newURI("place:folder=BOOKMARKS_MENU" +
                                "&folder=UNFILED_BOOKMARKS" +
                                "&folder=TOOLBAR" +
                                "&queryType=" +
                                Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS +
                                "&sort=" +
                                Ci.nsINavHistoryQueryOptions.SORT_BY_DATEADDED_DESCENDING +
                                "&maxResults=" + MAX_RESULTS +
                                "&excludeQueries=1"),
            parent: PlacesUtils.bookmarksMenuFolderId,
            position: menuIndex++,
            newInVersion: 1
          },
          RecentTags: {
            title: bundle.GetStringFromName("recentTagsTitle"),
            uri: NetUtil.newURI("place:"+
                                "type=" +
                                Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAG_QUERY +
                                "&sort=" +
                                Ci.nsINavHistoryQueryOptions.SORT_BY_LASTMODIFIED_DESCENDING +
                                "&maxResults=" + MAX_RESULTS),
            parent: PlacesUtils.bookmarksMenuFolderId,
            position: menuIndex++,
            newInVersion: 1
          }
        };

        // Set current itemId, parent and position if Smart Bookmark exists,
        // we will use these informations to create the new version at the same
        // position.
        let smartBookmarkItemIds = PlacesUtils.annotations.getItemsWithAnnotation(SMART_BOOKMARKS_ANNO);
        smartBookmarkItemIds.forEach(function (itemId) {
          let queryId = PlacesUtils.annotations.getItemAnnotation(itemId, SMART_BOOKMARKS_ANNO);
          if (queryId in smartBookmarks) {
            let smartBookmark = smartBookmarks[queryId];
            smartBookmark.itemId = itemId;
            smartBookmark.parent = PlacesUtils.bookmarks.getFolderIdForItem(itemId);
            smartBookmark.position = PlacesUtils.bookmarks.getItemIndex(itemId);
          }
          else {
            // We don't remove old Smart Bookmarks because user could still
            // find them useful, or could have personalized them.
            // Instead we remove the Smart Bookmark annotation.
            PlacesUtils.annotations.removeItemAnnotation(itemId, SMART_BOOKMARKS_ANNO);
          }
        });

        for (let queryId in smartBookmarks) {
          let smartBookmark = smartBookmarks[queryId];

          // We update or create only changed or new smart bookmarks.
          // Also we respect user choices, so we won't try to create a smart
          // bookmark if it has been removed.
          if (smartBookmarksCurrentVersion > 0 &&
              smartBookmark.newInVersion <= smartBookmarksCurrentVersion &&
              !smartBookmark.itemId)
            continue;

          // Remove old version of the smart bookmark if it exists, since it
          // will be replaced in place.
          if (smartBookmark.itemId) {
            PlacesUtils.bookmarks.removeItem(smartBookmark.itemId);
          }

          // Create the new smart bookmark and store its updated itemId.
          smartBookmark.itemId =
            PlacesUtils.bookmarks.insertBookmark(smartBookmark.parent,
                                                 smartBookmark.uri,
                                                 smartBookmark.position,
                                                 smartBookmark.title);
          PlacesUtils.annotations.setItemAnnotation(smartBookmark.itemId,
                                                    SMART_BOOKMARKS_ANNO,
                                                    queryId, 0,
                                                    PlacesUtils.annotations.EXPIRE_NEVER);
        }

        // If we are creating all Smart Bookmarks from ground up, add a
        // separator below them in the bookmarks menu.
        if (smartBookmarksCurrentVersion == 0 &&
            smartBookmarkItemIds.length == 0) {
          let id = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.bookmarksMenuFolderId,
                                                        menuIndex);
          // Don't add a separator if the menu was empty or there is one already.
          if (id != -1 &&
              PlacesUtils.bookmarks.getItemType(id) != PlacesUtils.bookmarks.TYPE_SEPARATOR) {
            PlacesUtils.bookmarks.insertSeparator(PlacesUtils.bookmarksMenuFolderId,
                                                  menuIndex);
          }
        }
      }
    };

    try {
      PlacesUtils.bookmarks.runInBatchMode(batch, null);
    }
    catch(ex) {
      Components.utils.reportError(ex);
    }
    finally {
      Services.prefs.setIntPref(SMART_BOOKMARKS_PREF, SMART_BOOKMARKS_VERSION);
      Services.prefs.savePrefFile(null);
    }
  },

  // this returns the most recent non-popup browser window
  getMostRecentBrowserWindow: function BG_getMostRecentBrowserWindow() {
    return RecentWindow.getMostRecentBrowserWindow();
  },

#ifdef MOZ_SERVICES_SYNC
  /**
   * Called as an observer when Sync's "display URI" notification is fired.
   *
   * We open the received URI in a background tab.
   *
   * Eventually, this will likely be replaced by a more robust tab syncing
   * feature. This functionality is considered somewhat evil by UX because it
   * opens a new tab automatically without any prompting. However, it is a
   * lesser evil than sending a tab to a specific device (from e.g. Fennec)
   * and having nothing happen on the receiving end.
   */
  _onDisplaySyncURI: function _onDisplaySyncURI(data) {
    try {
      let tabbrowser = this.getMostRecentBrowserWindow().gBrowser;

      // The payload is wrapped weirdly because of how Sync does notifications.
      tabbrowser.addTab(data.wrappedJSObject.object.uri);
    } catch (ex) {
      Cu.reportError("Error displaying tab received by Sync: " + ex);
    }
  },
#endif

  // for XPCOM
  classID:          Components.ID("{eab9012e-5f74-4cbc-b2b5-a590235513cc}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsIBrowserGlue]),

  // redefine the default factory for XPCOMUtils
  _xpcom_factory: BrowserGlueServiceFactory,
}

function ContentPermissionPrompt() {}

ContentPermissionPrompt.prototype = {
  classID:          Components.ID("{d8903bf6-68d5-4e97-bcd1-e4d3012f721a}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPermissionPrompt]),

  prompt: function CPP_prompt(request) {

    if (request.type != "geolocation") {
        return;
    }

    var requestingPrincipal = request.principal;
    var requestingURI = requestingPrincipal.URI;

    // Ignore requests from non-nsIStandardURLs
    if (!(requestingURI instanceof Ci.nsIStandardURL))
      return;

    var result = Services.perms.testExactPermissionFromPrincipal(requestingPrincipal, "geo");

    if (result == Ci.nsIPermissionManager.ALLOW_ACTION) {
      request.allow();
      return;
    }

    if (result == Ci.nsIPermissionManager.DENY_ACTION) {
      request.cancel();
      return;
    }

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

    var browserBundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");
    let secHistogram = Components.classes["@mozilla.org/base/telemetry;1"].
                                  getService(Ci.nsITelemetry).
                                  getHistogramById("SECURITY_UI");

    var mainAction = {
      label: browserBundle.GetStringFromName("geolocation.shareLocation"),
      accessKey: browserBundle.GetStringFromName("geolocation.shareLocation.accesskey"),
      callback: function() {
        secHistogram.add(Ci.nsISecurityUITelemetry.WARNING_GEOLOCATION_REQUEST_SHARE_LOCATION);
        request.allow();
      },
    };

    var message;
    var secondaryActions = [];
    var requestingWindow = request.window.top;
    var chromeWin = getChromeWindow(requestingWindow).wrappedJSObject;

    // Different message/options if it is a local file
    if (requestingURI.schemeIs("file")) {
      message = browserBundle.formatStringFromName("geolocation.shareWithFile",
                                                   [requestingURI.path], 1);
    } else {
      message = browserBundle.formatStringFromName("geolocation.shareWithSite",
                                                   [requestingURI.host], 1);

      // Don't offer to "always/never share" in PB mode
      if (!PrivateBrowsingUtils.isWindowPrivate(chromeWin)) {
        secondaryActions.push({
          label: browserBundle.GetStringFromName("geolocation.alwaysShareLocation"),
          accessKey: browserBundle.GetStringFromName("geolocation.alwaysShareLocation.accesskey"),
          callback: function () {
            Services.perms.addFromPrincipal(requestingPrincipal, "geo", Ci.nsIPermissionManager.ALLOW_ACTION);
            secHistogram.add(Ci.nsISecurityUITelemetry.WARNING_GEOLOCATION_REQUEST_ALWAYS_SHARE);
            request.allow();
          }
        });
        secondaryActions.push({
          label: browserBundle.GetStringFromName("geolocation.neverShareLocation"),
          accessKey: browserBundle.GetStringFromName("geolocation.neverShareLocation.accesskey"),
          callback: function () {
            Services.perms.addFromPrincipal(requestingPrincipal, "geo", Ci.nsIPermissionManager.DENY_ACTION);
            secHistogram.add(Ci.nsISecurityUITelemetry.WARNING_GEOLOCATION_REQUEST_NEVER_SHARE);
            request.cancel();
          }
        });
      }
    }

    var link = chromeWin.document.getElementById("geolocation-learnmore-link");
    link.value = browserBundle.GetStringFromName("geolocation.learnMore");
    link.href = Services.urlFormatter.formatURLPref("browser.geolocation.warning.infoURL");

    var browser = chromeWin.gBrowser.getBrowserForDocument(requestingWindow.document);

    secHistogram.add(Ci.nsISecurityUITelemetry.WARNING_GEOLOCATION_REQUEST);
    chromeWin.PopupNotifications.show(browser, "geolocation", message, "geo-notification-icon",
                                      mainAction, secondaryActions);
  }
};

var components = [BrowserGlue, ContentPermissionPrompt];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
