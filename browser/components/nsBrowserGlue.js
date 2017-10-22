/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;
const Cu = Components.utils;

const XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "WindowsUIUtils", "@mozilla.org/windows-ui-utils;1", "nsIWindowsUIUtils");
XPCOMUtils.defineLazyGetter(this, "WeaveService", () =>
  Cc["@mozilla.org/weave/service;1"].getService().wrappedJSObject
);

// lazy module getters

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AppMenuNotifications: "resource://gre/modules/AppMenuNotifications.jsm",
  AsyncPrefs: "resource://gre/modules/AsyncPrefs.jsm",
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  AutoCompletePopup: "resource://gre/modules/AutoCompletePopup.jsm",
  BookmarkHTMLUtils: "resource://gre/modules/BookmarkHTMLUtils.jsm",
  BookmarkJSONUtils: "resource://gre/modules/BookmarkJSONUtils.jsm",
  BrowserUITelemetry: "resource:///modules/BrowserUITelemetry.jsm",
  BrowserUsageTelemetry: "resource:///modules/BrowserUsageTelemetry.jsm",
  ContentClick: "resource:///modules/ContentClick.jsm",
  ContextualIdentityService: "resource://gre/modules/ContextualIdentityService.jsm",
  DateTimePickerHelper: "resource://gre/modules/DateTimePickerHelper.jsm",
  DirectoryLinksProvider: "resource:///modules/DirectoryLinksProvider.jsm",
  ExtensionsUI: "resource:///modules/ExtensionsUI.jsm",
  Feeds: "resource:///modules/Feeds.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  FileSource: "resource://gre/modules/L10nRegistry.jsm",
  FormValidationHandler: "resource:///modules/FormValidationHandler.jsm",
  Integration: "resource://gre/modules/Integration.jsm",
  L10nRegistry: "resource://gre/modules/L10nRegistry.jsm",
  LightweightThemeManager: "resource://gre/modules/LightweightThemeManager.jsm",
  LoginHelper: "resource://gre/modules/LoginHelper.jsm",
  LoginManagerParent: "resource://gre/modules/LoginManagerParent.jsm",
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  NewTabUtils: "resource://gre/modules/NewTabUtils.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  PageActions: "resource:///modules/PageActions.jsm",
  PageThumbs: "resource://gre/modules/PageThumbs.jsm",
  PdfJs: "resource://pdf.js/PdfJs.jsm",
  PermissionUI: "resource:///modules/PermissionUI.jsm",
  PingCentre: "resource:///modules/PingCentre.jsm",
  PlacesBackups: "resource://gre/modules/PlacesBackups.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  PluralForm: "resource://gre/modules/PluralForm.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  ProcessHangMonitor: "resource:///modules/ProcessHangMonitor.jsm",
  ReaderParent: "resource:///modules/ReaderParent.jsm",
  RecentWindow: "resource:///modules/RecentWindow.jsm",
  RemotePrompt: "resource:///modules/RemotePrompt.jsm",
  SafeBrowsing: "resource://gre/modules/SafeBrowsing.jsm",
  SessionStore: "resource:///modules/sessionstore/SessionStore.jsm",
  ShellService: "resource:///modules/ShellService.jsm",
  SimpleServiceDiscovery: "resource://gre/modules/SimpleServiceDiscovery.jsm",
  TabCrashHandler: "resource:///modules/ContentCrashHandlers.jsm",
  UIState: "resource://services-sync/UIState.jsm",
  UITour: "resource:///modules/UITour.jsm",
  WebChannel: "resource://gre/modules/WebChannel.jsm",
  WindowsRegistry: "resource://gre/modules/WindowsRegistry.jsm",
});

/* global AboutHome:false, ContentPrefServiceParent:false, ContentSearch:false,
          UpdateListener:false, webrtcUI:false */

/**
 * IF YOU ADD OR REMOVE FROM THIS LIST, PLEASE UPDATE THE LIST ABOVE AS WELL.
 * XXX Bug 1325373 is for making eslint detect these automatically.
 */

let initializedModules = {};

[
  ["AboutHome", "resource:///modules/AboutHome.jsm", "init"],
  ["ContentPrefServiceParent", "resource://gre/modules/ContentPrefServiceParent.jsm", "alwaysInit"],
  ["ContentSearch", "resource:///modules/ContentSearch.jsm", "init"],
  ["UpdateListener", "resource://gre/modules/UpdateListener.jsm", "init"],
  ["webrtcUI", "resource:///modules/webrtcUI.jsm", "init"],
].forEach(([name, resource, init]) => {
  XPCOMUtils.defineLazyGetter(this, name, () => {
    Cu.import(resource, initializedModules);
    initializedModules[name][init]();
    return initializedModules[name];
  });
});

if (AppConstants.MOZ_CRASHREPORTER) {
  XPCOMUtils.defineLazyModuleGetters(this, {
    PluginCrashReporter: "resource:///modules/ContentCrashHandlers.jsm",
    UnsubmittedCrashHandler: "resource:///modules/ContentCrashHandlers.jsm",
    CrashSubmit: "resource://gre/modules/CrashSubmit.jsm",
  });
}

XPCOMUtils.defineLazyGetter(this, "gBrandBundle", function() {
  return Services.strings.createBundle("chrome://branding/locale/brand.properties");
});

XPCOMUtils.defineLazyGetter(this, "gBrowserBundle", function() {
  return Services.strings.createBundle("chrome://browser/locale/browser.properties");
});

const global = this;

const listeners = {
  observers: {
    "update-staged": ["UpdateListener"],
    "update-downloaded": ["UpdateListener"],
    "update-available": ["UpdateListener"],
    "update-error": ["UpdateListener"],
  },

  ppmm: {
    // PLEASE KEEP THIS LIST IN SYNC WITH THE LISTENERS ADDED IN ContentPrefServiceParent.init
    "ContentPrefs:FunctionCall": ["ContentPrefServiceParent"],
    "ContentPrefs:AddObserverForName": ["ContentPrefServiceParent"],
    "ContentPrefs:RemoveObserverForName": ["ContentPrefServiceParent"],
    // PLEASE KEEP THIS LIST IN SYNC WITH THE LISTENERS ADDED IN ContentPrefServiceParent.init

    // PLEASE KEEP THIS LIST IN SYNC WITH THE LISTENERS ADDED IN AsyncPrefs.init
    "AsyncPrefs:SetPref": ["AsyncPrefs"],
    "AsyncPrefs:ResetPref": ["AsyncPrefs"],
    // PLEASE KEEP THIS LIST IN SYNC WITH THE LISTENERS ADDED IN AsyncPrefs.init

    "FeedConverter:addLiveBookmark": ["Feeds"],
    "WCCR:setAutoHandler": ["Feeds"],
    "webrtc:UpdateGlobalIndicators": ["webrtcUI"],
    "webrtc:UpdatingIndicators": ["webrtcUI"],
  },

  mm: {
    "AboutHome:MaybeShowMigrateMessage": ["AboutHome"],
    "AboutHome:RequestUpdate": ["AboutHome"],
    "Content:Click": ["ContentClick"],
    "ContentSearch": ["ContentSearch"],
    "FormValidation:ShowPopup": ["FormValidationHandler"],
    "FormValidation:HidePopup": ["FormValidationHandler"],
    "Prompt:Open": ["RemotePrompt"],
    "Reader:ArticleGet": ["ReaderParent"],
    "Reader:FaviconRequest": ["ReaderParent"],
    "Reader:UpdateReaderButton": ["ReaderParent"],
    // PLEASE KEEP THIS LIST IN SYNC WITH THE MOBILE LISTENERS IN BrowserCLH.js
    "RemoteLogins:findLogins": ["LoginManagerParent"],
    "RemoteLogins:findRecipes": ["LoginManagerParent"],
    "RemoteLogins:onFormSubmit": ["LoginManagerParent"],
    "RemoteLogins:autoCompleteLogins": ["LoginManagerParent"],
    "RemoteLogins:removeLogin": ["LoginManagerParent"],
    "RemoteLogins:insecureLoginFormPresent": ["LoginManagerParent"],
    // PLEASE KEEP THIS LIST IN SYNC WITH THE MOBILE LISTENERS IN BrowserCLH.js
    "WCCR:registerProtocolHandler": ["Feeds"],
    "WCCR:registerContentHandler": ["Feeds"],
    "rtcpeer:CancelRequest": ["webrtcUI"],
    "rtcpeer:Request": ["webrtcUI"],
    "webrtc:CancelRequest": ["webrtcUI"],
    "webrtc:Request": ["webrtcUI"],
    "webrtc:StopRecording": ["webrtcUI"],
    "webrtc:UpdateBrowserIndicators": ["webrtcUI"],
  },

  observe(subject, topic, data) {
    for (let module of this.observers[topic]) {
      try {
        global[module].observe(subject, topic, data);
      } catch (e) {
        Cu.reportError(e);
      }
    }
  },

  receiveMessage(modules, data) {
    let val;
    for (let module of modules[data.name]) {
      try {
        val = global[module].receiveMessage(data) || val;
      } catch (e) {
        Cu.reportError(e);
      }
    }
    return val;
  },

  init() {
    for (let observer of Object.keys(this.observers)) {
      Services.obs.addObserver(this, observer);
    }

    let receiveMessageMM = this.receiveMessage.bind(this, this.mm);
    for (let message of Object.keys(this.mm)) {
      Services.mm.addMessageListener(message, receiveMessageMM);
    }

    let receiveMessagePPMM = this.receiveMessage.bind(this, this.ppmm);
    for (let message of Object.keys(this.ppmm)) {
      Services.ppmm.addMessageListener(message, receiveMessagePPMM);
    }
  }
};

// Seconds of idle before trying to create a bookmarks backup.
const BOOKMARKS_BACKUP_IDLE_TIME_SEC = 8 * 60;
// Minimum interval between backups.  We try to not create more than one backup
// per interval.
const BOOKMARKS_BACKUP_MIN_INTERVAL_DAYS = 1;
// Maximum interval between backups.  If the last backup is older than these
// days we will try to create a new one more aggressively.
const BOOKMARKS_BACKUP_MAX_INTERVAL_DAYS = 3;
// Seconds of idle time before the late idle tasks will be scheduled.
const LATE_TASKS_IDLE_TIME_SEC = 20;
// Time after we stop tracking startup crashes.
const STARTUP_CRASHES_END_DELAY_MS = 30 * 1000;

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

  XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts", "resource://gre/modules/FxAccounts.jsm");
  XPCOMUtils.defineLazyServiceGetter(this, "AlertsService", "@mozilla.org/alerts-service;1", "nsIAlertsService");

  this._init();
}

/*
 * OS X has the concept of zero-window sessions and therefore ignores the
 * browser-lastwindow-close-* topics.
 */
const OBSERVE_LASTWINDOW_CLOSE_TOPICS = AppConstants.platform != "macosx";

BrowserGlue.prototype = {
  _saveSession: false,
  _migrationImportsDefaultBookmarks: false,
  _placesBrowserInitComplete: false,

  _setPrefToSaveSession: function BG__setPrefToSaveSession(aForce) {
    if (!this._saveSession && !aForce)
      return;

    Services.prefs.setBoolPref("browser.sessionstore.resume_session_once", true);

    // This method can be called via [NSApplication terminate:] on Mac, which
    // ends up causing prefs not to be flushed to disk, so we need to do that
    // explicitly here. See bug 497652.
    Services.prefs.savePrefFile(null);
  },

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

  /**
   * Lazily initialize PingCentre
   */
  get pingCentre() {
    const MAIN_TOPIC_ID = "main";
    Object.defineProperty(this, "pingCentre", {
      value: new PingCentre({ topic: MAIN_TOPIC_ID })
    });
    return this.pingCentre;
  },

  _sendMainPingCentrePing() {
    const ACTIVITY_STREAM_ENABLED_PREF = "browser.newtabpage.activity-stream.enabled";
    const ACTIVITY_STREAM_ID = "activity-stream";
    let asEnabled = Services.prefs.getBoolPref(ACTIVITY_STREAM_ENABLED_PREF, false);

    const payload = {
      event: "AS_ENABLED",
      value: asEnabled
    };
    const options = {filter: ACTIVITY_STREAM_ID};
    this.pingCentre.sendPing(payload, options);
  },

  // nsIObserver implementation
  observe: function BG_observe(subject, topic, data) {
    switch (topic) {
      case "notifications-open-settings":
        this._openPreferences("privacy", { origin: "notifOpenSettings" });
        break;
      case "prefservice:after-app-defaults":
        this._onAppDefaults();
        break;
      case "final-ui-startup":
        this._beforeUIStartup();
        break;
      case "browser-delayed-startup-finished":
        this._onFirstWindowLoaded(subject);
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
      case "restart-in-safe-mode":
        this._onSafeModeRestart();
        break;
      case "quit-application-requested":
        this._onQuitRequest(subject, data);
        break;
      case "quit-application-granted":
        this._onQuitApplicationGranted();
        break;
      case "browser-lastwindow-close-requested":
        if (OBSERVE_LASTWINDOW_CLOSE_TOPICS) {
          // The application is not actually quitting, but the last full browser
          // window is about to be closed.
          this._onQuitRequest(subject, "lastwindow");
        }
        break;
      case "browser-lastwindow-close-granted":
        if (OBSERVE_LASTWINDOW_CLOSE_TOPICS) {
          this._setPrefToSaveSession();
        }
        break;
      case "weave:service:ready":
        this._setSyncAutoconnectDelay();
        break;
      case "fxaccounts:onverified":
        this._showSyncStartedDoorhanger();
        break;
      case "fxaccounts:device_connected":
        this._onDeviceConnected(data);
        break;
      case "fxaccounts:verify_login":
        this._onVerifyLoginNotification(JSON.parse(data));
        break;
      case "fxaccounts:device_disconnected":
        data = JSON.parse(data);
        if (data.isLocalDevice) {
          this._onDeviceDisconnected();
        }
        break;
      case "weave:engine:clients:display-uris":
        this._onDisplaySyncURIs(subject);
        break;
      case "session-save":
        this._setPrefToSaveSession(true);
        subject.QueryInterface(Ci.nsISupportsPRBool);
        subject.data = true;
        break;
      case "places-init-complete":
        Services.obs.removeObserver(this, "places-init-complete");
        if (!this._migrationImportsDefaultBookmarks)
          this._initPlaces(false);
        break;
      case "idle":
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
        } else if (data == "force-ui-migration") {
          this._migrateUI();
        } else if (data == "force-distribution-customization") {
          this._distributionCustomizer.applyPrefDefaults();
          this._distributionCustomizer.applyCustomizations();
          // To apply distribution bookmarks use "places-init-complete".
        } else if (data == "force-places-init") {
          this._initPlaces(false);
        } else if (data == "smart-bookmarks-init") {
          this.ensurePlacesDefaultQueriesInitialized().then(() => {
            Services.obs.notifyObservers(null, "test-smart-bookmarks-done");
          });
        } else if (data == "mock-fxaccounts") {
          Object.defineProperty(this, "fxAccounts", {
            value: subject.wrappedJSObject
          });
        } else if (data == "mock-alerts-service") {
          Object.defineProperty(this, "AlertsService", {
            value: subject.wrappedJSObject
          });
        } else if (data == "places-browser-init-complete") {
          if (this._placesBrowserInitComplete) {
            Services.obs.notifyObservers(null, "places-browser-init-complete");
          }
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
          let win = RecentWindow.getMostRecentBrowserWindow();
          if (win) {
            data = JSON.parse(data);
            let where = win.whereToOpenLink(data);
            // Preserve legacy behavior of non-modifier left-clicks
            // opening in a new selected tab.
            if (where == "current") {
              where = "tab";
            }
            win.openUILinkIn(data.href, where);
            linkHandled.data = true;
          }
        }
        break;
      case "profile-before-change":
         // Any component depending on Places should be finalized in
         // _onPlacesShutdown.  Any component that doesn't need to act after
         // the UI has gone should be finalized in _onQuitApplicationGranted.
        this._dispose();
        break;
      case "keyword-search":
        // This notification is broadcast by the docshell when it "fixes up" a
        // URI that it's been asked to load into a keyword search.
        let engine = null;
        try {
          engine = subject.QueryInterface(Ci.nsISearchEngine);
        } catch (ex) {
          Cu.reportError(ex);
        }
        let win = RecentWindow.getMostRecentBrowserWindow();
        win.BrowserSearch.recordSearchInTelemetry(engine, "urlbar");
        break;
      case "browser-search-engine-modified":
        // Ensure we cleanup the hiddenOneOffs pref when removing
        // an engine, and that newly added engines are visible.
        if (data == "engine-added" || data == "engine-removed") {
          let engineName = subject.QueryInterface(Ci.nsISearchEngine).name;
          let pref = Services.prefs.getStringPref("browser.search.hiddenOneOffs");
          let hiddenList = pref ? pref.split(",") : [];
          hiddenList = hiddenList.filter(x => x !== engineName);
          Services.prefs.setStringPref("browser.search.hiddenOneOffs", hiddenList.join(","));
        }
        break;
      case "flash-plugin-hang":
        this._handleFlashHang();
        break;
      case "xpi-signature-changed":
        let disabledAddons = JSON.parse(data).disabled;
        AddonManager.getAddonsByIDs(disabledAddons, (addons) => {
          for (let addon of addons) {
            if (addon.type != "experiment") {
              this._notifyUnsignedAddonsDisabled();
              break;
            }
          }
        });
        break;
      case "test-initialize-sanitizer":
        this._sanitizer.onStartup();
        break;
      case "sync-ui-state:update":
        this._updateFxaBadges();
        break;
      case "handlersvc-store-initialized":
        // Initialize PdfJs when running in-process and remote. This only
        // happens once since PdfJs registers global hooks. If the PdfJs
        // extension is installed the init method below will be overridden
        // leaving initialization to the extension.
        // parent only: configure default prefs, set up pref observers, register
        // pdf content handler, and initializes parent side message manager
        // shim for privileged api access.
        PdfJs.init(true);
        break;
      case "shield-init-complete":
        this._sendMainPingCentrePing();
        break;
    }
  },

  // initialization (called on application startup)
  _init: function BG__init() {
    let os = Services.obs;
    os.addObserver(this, "notifications-open-settings");
    os.addObserver(this, "prefservice:after-app-defaults");
    os.addObserver(this, "final-ui-startup");
    os.addObserver(this, "browser-delayed-startup-finished");
    os.addObserver(this, "sessionstore-windows-restored");
    os.addObserver(this, "browser:purge-session-history");
    os.addObserver(this, "quit-application-requested");
    os.addObserver(this, "quit-application-granted");
    if (OBSERVE_LASTWINDOW_CLOSE_TOPICS) {
      os.addObserver(this, "browser-lastwindow-close-requested");
      os.addObserver(this, "browser-lastwindow-close-granted");
    }
    os.addObserver(this, "weave:service:ready");
    os.addObserver(this, "fxaccounts:onverified");
    os.addObserver(this, "fxaccounts:device_connected");
    os.addObserver(this, "fxaccounts:verify_login");
    os.addObserver(this, "fxaccounts:device_disconnected");
    os.addObserver(this, "weave:engine:clients:display-uris");
    os.addObserver(this, "session-save");
    os.addObserver(this, "places-init-complete");
    os.addObserver(this, "distribution-customization-complete");
    os.addObserver(this, "handle-xul-text-link");
    os.addObserver(this, "profile-before-change");
    os.addObserver(this, "keyword-search");
    os.addObserver(this, "browser-search-engine-modified");
    os.addObserver(this, "restart-in-safe-mode");
    os.addObserver(this, "flash-plugin-hang");
    os.addObserver(this, "xpi-signature-changed");
    os.addObserver(this, "sync-ui-state:update");
    os.addObserver(this, "handlersvc-store-initialized");
    os.addObserver(this, "shield-init-complete");

    this._flashHangCount = 0;
    this._firstWindowReady = new Promise(resolve => this._firstWindowLoaded = resolve);
    if (AppConstants.platform == "win") {
      JawsScreenReaderVersionCheck.init();
    }
  },

  // cleanup (called on application shutdown)
  _dispose: function BG__dispose() {
    let os = Services.obs;
    os.removeObserver(this, "notifications-open-settings");
    os.removeObserver(this, "prefservice:after-app-defaults");
    os.removeObserver(this, "final-ui-startup");
    os.removeObserver(this, "sessionstore-windows-restored");
    os.removeObserver(this, "browser:purge-session-history");
    os.removeObserver(this, "quit-application-requested");
    os.removeObserver(this, "quit-application-granted");
    os.removeObserver(this, "restart-in-safe-mode");
    if (OBSERVE_LASTWINDOW_CLOSE_TOPICS) {
      os.removeObserver(this, "browser-lastwindow-close-requested");
      os.removeObserver(this, "browser-lastwindow-close-granted");
    }
    os.removeObserver(this, "weave:service:ready");
    os.removeObserver(this, "fxaccounts:onverified");
    os.removeObserver(this, "fxaccounts:device_connected");
    os.removeObserver(this, "fxaccounts:verify_login");
    os.removeObserver(this, "fxaccounts:device_disconnected");
    os.removeObserver(this, "weave:engine:clients:display-uris");
    os.removeObserver(this, "session-save");
    if (this._bookmarksBackupIdleTime) {
      this._idleService.removeIdleObserver(this, this._bookmarksBackupIdleTime);
      delete this._bookmarksBackupIdleTime;
    }
    if (this._lateTasksIdleObserver) {
      this._idleService.removeIdleObserver(this._lateTasksIdleObserver, LATE_TASKS_IDLE_TIME_SEC);
      delete this._lateTasksIdleObserver;
    }
    if (this._gmpInstallManager) {
      this._gmpInstallManager.uninit();
      delete this._gmpInstallManager;
    }
    try {
      os.removeObserver(this, "places-init-complete");
    } catch (ex) { /* Could have been removed already */ }
    os.removeObserver(this, "handle-xul-text-link");
    os.removeObserver(this, "profile-before-change");
    os.removeObserver(this, "keyword-search");
    os.removeObserver(this, "browser-search-engine-modified");
    os.removeObserver(this, "flash-plugin-hang");
    os.removeObserver(this, "xpi-signature-changed");
    os.removeObserver(this, "sync-ui-state:update");
    os.removeObserver(this, "shield-init-complete");
  },

  _onAppDefaults: function BG__onAppDefaults() {
    // apply distribution customizations (prefs)
    // other customizations are applied in _beforeUIStartup()
    this._distributionCustomizer.applyPrefDefaults();
  },

  // runs on startup, before the first command line handler is invoked
  // (i.e. before the first window is opened)
  _beforeUIStartup: function BG__beforeUIStartup() {
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

    listeners.init();

    SessionStore.init();

    if (AppConstants.INSTALL_COMPACT_THEMES) {
      let vendorShortName = gBrandBundle.GetStringFromName("vendorShortName");

      LightweightThemeManager.addBuiltInTheme({
        id: "firefox-compact-light@mozilla.org",
        name: gBrowserBundle.GetStringFromName("lightTheme.name"),
        description: gBrowserBundle.GetStringFromName("lightTheme.description"),
        headerURL: "resource:///chrome/browser/content/browser/defaultthemes/compact.header.png",
        iconURL: "resource:///chrome/browser/content/browser/defaultthemes/light.icon.svg",
        textcolor: "black",
        accentcolor: "white",
        author: vendorShortName,
      });
      LightweightThemeManager.addBuiltInTheme({
        id: "firefox-compact-dark@mozilla.org",
        name: gBrowserBundle.GetStringFromName("darkTheme.name"),
        description: gBrowserBundle.GetStringFromName("darkTheme.description"),
        headerURL: "resource:///chrome/browser/content/browser/defaultthemes/compact.header.png",
        iconURL: "resource:///chrome/browser/content/browser/defaultthemes/dark.icon.svg",
        textcolor: "white",
        accentcolor: "black",
        author: vendorShortName,
      });
    }


    // Initialize the default l10n resource sources for L10nRegistry.
    const multilocalePath = "resource://gre/res/multilocale.json";
    L10nRegistry.bootstrap = fetch(multilocalePath).then(d => d.json()).then(({ locales }) => {
      const toolkitSource = new FileSource("toolkit", locales, "resource://gre/localization/{locale}/");
      L10nRegistry.registerSource(toolkitSource);
      const appSource = new FileSource("app", locales, "resource://app/localization/{locale}/");
      L10nRegistry.registerSource(appSource);
    }).catch(e => {
      Services.console.logStringMessage(`Could not load multilocale.json. Error: ${e}`);
    });

    Services.obs.notifyObservers(null, "browser-ui-startup-complete");
  },

  _checkForOldBuildUpdates() {
    // check for update if our build is old
    if (AppConstants.MOZ_UPDATER &&
        Services.prefs.getBoolPref("app.update.enabled") &&
        Services.prefs.getBoolPref("app.update.checkInstallTime")) {

      let buildID = Services.appinfo.appBuildID;
      let today = new Date().getTime();
      /* eslint-disable no-multi-spaces */
      let buildDate = new Date(buildID.slice(0, 4),     // year
                               buildID.slice(4, 6) - 1, // months are zero-based.
                               buildID.slice(6, 8),     // day
                               buildID.slice(8, 10),    // hour
                               buildID.slice(10, 12),   // min
                               buildID.slice(12, 14))   // ms
      .getTime();
      /* eslint-enable no-multi-spaces */

      const millisecondsIn24Hours = 86400000;
      let acceptableAge = Services.prefs.getIntPref("app.update.checkInstallTime.days") * millisecondsIn24Hours;

      if (buildDate + acceptableAge < today) {
        Cc["@mozilla.org/updates/update-service;1"].getService(Ci.nsIApplicationUpdateService).checkForBackgroundUpdates();
      }
    }
  },

  _onSafeModeRestart: function BG_onSafeModeRestart() {
    // prompt the user to confirm
    let strings = gBrowserBundle;
    let promptTitle = strings.GetStringFromName("safeModeRestartPromptTitle");
    let promptMessage = strings.GetStringFromName("safeModeRestartPromptMessage");
    let restartText = strings.GetStringFromName("safeModeRestartButton");
    let buttonFlags = (Services.prompt.BUTTON_POS_0 *
                       Services.prompt.BUTTON_TITLE_IS_STRING) +
                      (Services.prompt.BUTTON_POS_1 *
                       Services.prompt.BUTTON_TITLE_CANCEL) +
                      Services.prompt.BUTTON_POS_0_DEFAULT;

    let rv = Services.prompt.confirmEx(null, promptTitle, promptMessage,
                                       buttonFlags, restartText, null, null,
                                       null, {});
    if (rv != 0)
      return;

    let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"]
                       .createInstance(Ci.nsISupportsPRBool);
    Services.obs.notifyObservers(cancelQuit, "quit-application-requested", "restart");

    if (!cancelQuit.data) {
      Services.startup.restartInSafeMode(Ci.nsIAppStartup.eAttemptQuit);
    }
  },

  _trackSlowStartup() {
    if (Services.startup.interrupted ||
        Services.prefs.getBoolPref("browser.slowStartup.notificationDisabled"))
      return;

    let currentTime = Date.now() - Services.startup.getStartupInfo().process;
    let averageTime = 0;
    let samples = 0;
    try {
      averageTime = Services.prefs.getIntPref("browser.slowStartup.averageTime");
      samples = Services.prefs.getIntPref("browser.slowStartup.samples");
    } catch (e) { }

    let totalTime = (averageTime * samples) + currentTime;
    samples++;
    averageTime = totalTime / samples;

    if (samples >= Services.prefs.getIntPref("browser.slowStartup.maxSamples")) {
      if (averageTime > Services.prefs.getIntPref("browser.slowStartup.timeThreshold"))
        this._calculateProfileAgeInDays().then(this._showSlowStartupNotification, null);
      averageTime = 0;
      samples = 0;
    }

    Services.prefs.setIntPref("browser.slowStartup.averageTime", averageTime);
    Services.prefs.setIntPref("browser.slowStartup.samples", samples);
  },

  async _calculateProfileAgeInDays() {
    let ProfileAge = Cu.import("resource://gre/modules/ProfileAge.jsm", {}).ProfileAge;
    let profileAge = new ProfileAge(null, null);

    let creationDate = await profileAge.created;
    let resetDate = await profileAge.reset;

    // if the profile was reset, consider the
    // reset date for its age.
    let profileDate = resetDate || creationDate;

    const ONE_DAY = 24 * 60 * 60 * 1000;
    return (Date.now() - profileDate) / ONE_DAY;
  },

  _showSlowStartupNotification(profileAge) {
    if (profileAge < 90) // 3 months
      return;

    let win = RecentWindow.getMostRecentBrowserWindow();
    if (!win)
      return;

    let productName = gBrandBundle.GetStringFromName("brandFullName");
    let message = win.gNavigatorBundle.getFormattedString("slowStartup.message", [productName]);

    let buttons = [
      {
        label:     win.gNavigatorBundle.getString("slowStartup.helpButton.label"),
        accessKey: win.gNavigatorBundle.getString("slowStartup.helpButton.accesskey"),
        callback() {
          win.openUILinkIn("https://support.mozilla.org/kb/reset-firefox-easily-fix-most-problems", "tab");
        }
      },
      {
        label:     win.gNavigatorBundle.getString("slowStartup.disableNotificationButton.label"),
        accessKey: win.gNavigatorBundle.getString("slowStartup.disableNotificationButton.accesskey"),
        callback() {
          Services.prefs.setBoolPref("browser.slowStartup.notificationDisabled", true);
        }
      }
    ];

    let nb = win.document.getElementById("global-notificationbox");
    nb.appendNotification(message, "slow-startup",
                          "chrome://browser/skin/slowStartup-16.png",
                          nb.PRIORITY_INFO_LOW, buttons);
  },

  /**
   * Show a notification bar offering a reset.
   *
   * @param reason
   *        String of either "unused" or "uninstall", specifying the reason
   *        why a profile reset is offered.
   */
  _resetProfileNotification(reason) {
    let win = RecentWindow.getMostRecentBrowserWindow();
    if (!win)
      return;

    Cu.import("resource://gre/modules/ResetProfile.jsm");
    if (!ResetProfile.resetSupported())
      return;

    let productName = gBrandBundle.GetStringFromName("brandShortName");
    let resetBundle = Services.strings
                              .createBundle("chrome://global/locale/resetProfile.properties");

    let message;
    if (reason == "unused") {
      message = resetBundle.formatStringFromName("resetUnusedProfile.message", [productName], 1);
    } else if (reason == "uninstall") {
      message = resetBundle.formatStringFromName("resetUninstalled.message", [productName], 1);
    } else {
      throw new Error(`Unknown reason (${reason}) given to _resetProfileNotification.`);
    }
    let buttons = [
      {
        label:     resetBundle.formatStringFromName("refreshProfile.resetButton.label", [productName], 1),
        accessKey: resetBundle.GetStringFromName("refreshProfile.resetButton.accesskey"),
        callback() {
          ResetProfile.openConfirmationDialog(win);
        }
      },
    ];

    let nb = win.document.getElementById("global-notificationbox");
    nb.appendNotification(message, "reset-profile-notification",
                          "chrome://global/skin/icons/question-16.png",
                          nb.PRIORITY_INFO_LOW, buttons);
  },

  _notifyUnsignedAddonsDisabled() {
    let win = RecentWindow.getMostRecentBrowserWindow();
    if (!win)
      return;

    let message = win.gNavigatorBundle.getString("unsignedAddonsDisabled.message");
    let buttons = [
      {
        label:     win.gNavigatorBundle.getString("unsignedAddonsDisabled.learnMore.label"),
        accessKey: win.gNavigatorBundle.getString("unsignedAddonsDisabled.learnMore.accesskey"),
        callback() {
          win.BrowserOpenAddonsMgr("addons://list/extension?unsigned=true");
        }
      },
    ];

    let nb = win.document.getElementById("high-priority-global-notificationbox");
    nb.appendNotification(message, "unsigned-addons-disabled", "",
                          nb.PRIORITY_WARNING_MEDIUM, buttons);
  },

  _firstWindowTelemetry(aWindow) {
    let scaling = aWindow.devicePixelRatio * 100;
    try {
      Services.telemetry.getHistogramById("DISPLAY_SCALING").add(scaling);
    } catch (ex) {}
  },

  // the first browser window has finished initializing
  _onFirstWindowLoaded: function BG__onFirstWindowLoaded(aWindow) {
    // Set up listeners and, if PdfJs is enabled, register the PDF stream converter.
    // We delay all of the parent's initialization other than stream converter
    // registration, because it requires file IO from nsHandlerService-json.js
    Services.ppmm.loadProcessScript("resource://pdf.js/pdfjschildbootstrap.js", true);
    if (PdfJs.enabled) {
      PdfJs.ensureRegistered();
      Services.ppmm.loadProcessScript("resource://pdf.js/pdfjschildbootstrap-enabled.js", true);
    }

    TabCrashHandler.init();
    if (AppConstants.MOZ_CRASHREPORTER) {
      PluginCrashReporter.init();
    }

    ProcessHangMonitor.init();

    // A channel for "remote troubleshooting" code...
    let channel = new WebChannel("remote-troubleshooting", "remote-troubleshooting");
    channel.listen((id, data, target) => {
      if (data.command == "request") {
        let {Troubleshoot} = Cu.import("resource://gre/modules/Troubleshoot.jsm", {});
        Troubleshoot.snapshot(snapshotData => {
          // for privacy we remove crash IDs and all preferences (but bug 1091944
          // exists to expose prefs once we are confident of privacy implications)
          delete snapshotData.crashes;
          delete snapshotData.modifiedPreferences;
          channel.send(snapshotData, target);
        });
      }
    });

    this._trackSlowStartup();

    // Offer to reset a user's profile if it hasn't been used for 60 days.
    const OFFER_PROFILE_RESET_INTERVAL_MS = 60 * 24 * 60 * 60 * 1000;
    let lastUse = Services.appinfo.replacedLockTime;
    let disableResetPrompt = Services.prefs.getBoolPref("browser.disableResetPrompt", false);

    if (!disableResetPrompt && lastUse &&
        Date.now() - lastUse >= OFFER_PROFILE_RESET_INTERVAL_MS) {
      this._resetProfileNotification("unused");
    } else if (AppConstants.platform == "win" && !disableResetPrompt) {
      // Check if we were just re-installed and offer Firefox Reset
      let updateChannel;
      try {
        updateChannel = Cu.import("resource://gre/modules/UpdateUtils.jsm", {}).UpdateUtils.UpdateChannel;
      } catch (ex) {}
      if (updateChannel) {
        let uninstalledValue =
          WindowsRegistry.readRegKey(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                                     "Software\\Mozilla\\Firefox",
                                     `Uninstalled-${updateChannel}`);
        let removalSuccessful =
          WindowsRegistry.removeRegKey(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                                       "Software\\Mozilla\\Firefox",
                                       `Uninstalled-${updateChannel}`);
        if (removalSuccessful && uninstalledValue == "True") {
          this._resetProfileNotification("uninstall");
        }
      }
    }

    this._checkForOldBuildUpdates();

    AutoCompletePopup.init();
    DateTimePickerHelper.init();
    // Check if Sync is configured
    if (Services.prefs.prefHasUserValue("services.sync.username")) {
      WeaveService.init();
    }

    PageThumbs.init();

    DirectoryLinksProvider.init();
    NewTabUtils.init();
    NewTabUtils.links.addProvider(DirectoryLinksProvider);

    PageActions.init();

    this._firstWindowTelemetry(aWindow);
    this._firstWindowLoaded();

    // Set the default favicon size for UI views that use the page-icon protocol.
    PlacesUtils.favicons.setDefaultIconURIPreferredSize(16 * aWindow.devicePixelRatio);
  },

  _sendMediaTelemetry() {
    let win = Services.appShell.hiddenDOMWindow;
    let v = win.document.createElementNS("http://www.w3.org/1999/xhtml", "video");
    v.reportCanPlayTelemetry();
  },

  /**
   * Application shutdown handler.
   */
  _onQuitApplicationGranted() {
    // This pref must be set here because SessionStore will use its value
    // on quit-application.
    this._setPrefToSaveSession();

    // Call trackStartupCrashEnd here in case the delayed call on startup hasn't
    // yet occurred (see trackStartupCrashEnd caller in browser.js).
    try {
      Services.startup.trackStartupCrashEnd();
    } catch (e) {
      Cu.reportError("Could not end startup crash tracking in quit-application-granted: " + e);
    }

    if (this._bookmarksBackupIdleTime) {
      this._idleService.removeIdleObserver(this, this._bookmarksBackupIdleTime);
      delete this._bookmarksBackupIdleTime;
    }

    for (let mod of Object.values(initializedModules)) {
      if (mod.uninit) {
        mod.uninit();
      }
    }

    BrowserUsageTelemetry.uninit();
    // Only uninit PingCentre if the getter has initialized it
    if (Object.prototype.hasOwnProperty.call(this, "pingCentre")) {
      this.pingCentre.uninit();
    }

    PageThumbs.uninit();
    NewTabUtils.uninit();
    AutoCompletePopup.uninit();
    DateTimePickerHelper.uninit();
  },

  // All initial windows have opened.
  _onWindowsRestored: function BG__onWindowsRestored() {
    if (this._windowsWereRestored) {
      return;
    }
    this._windowsWereRestored = true;

    BrowserUsageTelemetry.init();
    BrowserUITelemetry.init();

    // Show update notification, if needed.
    if (Services.prefs.prefHasUserValue("app.update.postupdate"))
      this._showUpdateNotification();

    ExtensionsUI.init();

    let signingRequired;
    if (AppConstants.MOZ_REQUIRE_SIGNING) {
      signingRequired = true;
    } else {
      signingRequired = Services.prefs.getBoolPref("xpinstall.signatures.required");
    }

    if (signingRequired) {
      let disabledAddons = AddonManager.getStartupChanges(AddonManager.STARTUP_CHANGE_DISABLED);
      AddonManager.getAddonsByIDs(disabledAddons, (addons) => {
        for (let addon of addons) {
          if (addon.type == "experiment")
            continue;

          if (addon.signedState <= AddonManager.SIGNEDSTATE_MISSING) {
            this._notifyUnsignedAddonsDisabled();
            break;
          }
        }
      });
    }

    if (AppConstants.MOZ_CRASHREPORTER) {
      UnsubmittedCrashHandler.init();
    }

    this._sanitizer.onStartup();
    this._scheduleStartupIdleTasks();
    this._lateTasksIdleObserver = (idleService, topic, data) => {
      if (topic == "idle") {
        idleService.removeIdleObserver(this._lateTasksIdleObserver,
                                       LATE_TASKS_IDLE_TIME_SEC);
        delete this._lateTasksIdleObserver;
        this._scheduleArbitrarilyLateIdleTasks();
      }
    };
    this._idleService.addIdleObserver(
      this._lateTasksIdleObserver, LATE_TASKS_IDLE_TIME_SEC);
  },

  /**
   * Use this function as an entry point to schedule tasks that
   * need to run only once after startup, and can be scheduled
   * by using an idle callback.
   *
   * The functions scheduled here will fire from idle callbacks
   * once every window has finished being restored by session
   * restore, and it's guaranteed that they will run before
   * the equivalent per-window idle tasks
   * (from _schedulePerWindowIdleTasks in browser.js).
   *
   * If you have something that can wait even further than the
   * per-window initialization, please schedule them using
   * _scheduleArbitrarilyLateIdleTasks.
   * Don't be fooled by thinking that the use of the timeout parameter
   * will delay your function: it will just ensure that it potentially
   * happens _earlier_ than expected (when the timeout limit has been reached),
   * but it will not make it happen later (and out of order) compared
   * to the other ones scheduled together.
   */
  _scheduleStartupIdleTasks() {
    Services.tm.idleDispatchToMainThread(() => {
      ContextualIdentityService.load();
    });

    // Load the Login Manager data from disk off the main thread, some time
    // after startup.  If the data is required before this runs, for example
    // because a restored page contains a password field, it will be loaded on
    // the main thread, and this initialization request will be ignored.
    Services.tm.idleDispatchToMainThread(() => {
      try {
        Services.logins;
      } catch (ex) {
        Cu.reportError(ex);
      }
    }, 3000);

    // It's important that SafeBrowsing is initialized reasonably
    // early, so we use a maximum timeout for it.
    Services.tm.idleDispatchToMainThread(() => {
      SafeBrowsing.init();

      // Login reputation depends on the Safe Browsing API.
      if (Services.prefs.getBoolPref("browser.safebrowsing.passwords.enabled")) {
        Cc["@mozilla.org/reputationservice/login-reputation-service;1"]
        .getService(Ci.ILoginReputationService);
      }
    }, 5000);

    if (AppConstants.MOZ_CRASHREPORTER) {
      UnsubmittedCrashHandler.scheduleCheckForUnsubmittedCrashReports();
    }

    if (AppConstants.platform == "win") {
      Services.tm.idleDispatchToMainThread(() => {
        // For Windows 7, initialize the jump list module.
        const WINTASKBAR_CONTRACTID = "@mozilla.org/windows-taskbar;1";
        if (WINTASKBAR_CONTRACTID in Cc &&
            Cc[WINTASKBAR_CONTRACTID].getService(Ci.nsIWinTaskbar).available) {
          let temp = {};
          Cu.import("resource:///modules/WindowsJumpLists.jsm", temp);
          temp.WinTaskbarJumpList.startup();
        }
      });
    }

    if (AppConstants.MOZ_DEV_EDITION) {
      Services.tm.idleDispatchToMainThread(() => {
        this._createExtraDefaultProfile();
      });
    }

    Services.tm.idleDispatchToMainThread(() => {
      this._checkForDefaultBrowser();
    });

    Services.tm.idleDispatchToMainThread(() => {
      let {setTimeout} = Cu.import("resource://gre/modules/Timer.jsm", {});
      setTimeout(function() {
        Services.tm.idleDispatchToMainThread(Services.startup.trackStartupCrashEnd);
      }, STARTUP_CRASHES_END_DELAY_MS);
    });

    Services.tm.idleDispatchToMainThread(() => {
      let handlerService = Cc["@mozilla.org/uriloader/handler-service;1"].
                           getService(Ci.nsIHandlerService);
      handlerService.asyncInit();
    });

    if (AppConstants.platform == "win") {
      Services.tm.idleDispatchToMainThread(() => {
        JawsScreenReaderVersionCheck.onWindowsRestored();
      });
    }
  },

  /**
   * Use this function as an entry point to schedule tasks that need
   * to run once per session, at any arbitrary point in time.
   * This function will be called from an idle observer. Check the value of
   * LATE_TASKS_IDLE_TIME_SEC to see the current value for this idle
   * observer.
   *
   * Note: this function may never be called if the user is never idle for the
   * full length of the period of time specified. But given a reasonably low
   * value, this is unlikely.
   */
  _scheduleArbitrarilyLateIdleTasks() {
    Services.tm.idleDispatchToMainThread(() => {
      this._sendMediaTelemetry();
    });

    Services.tm.idleDispatchToMainThread(() => {
      // Telemetry for master-password - we do this after a delay as it
      // can cause IO if NSS/PSM has not already initialized.
      let tokenDB = Cc["@mozilla.org/security/pk11tokendb;1"]
                .getService(Ci.nsIPK11TokenDB);
      let token = tokenDB.getInternalKeyToken();
      let mpEnabled = token.hasPassword;
      if (mpEnabled) {
        Services.telemetry.getHistogramById("MASTER_PASSWORD_ENABLED").add(mpEnabled);
      }
    });

    Services.tm.idleDispatchToMainThread(() => {
      let obj = {};
      Cu.import("resource://gre/modules/GMPInstallManager.jsm", obj);
      this._gmpInstallManager = new obj.GMPInstallManager();
      // We don't really care about the results, if someone is interested they
      // can check the log.
      this._gmpInstallManager.simpleCheckAndInstall().catch(() => {});
    });
  },

  _createExtraDefaultProfile() {
    if (!AppConstants.MOZ_DEV_EDITION) {
      return;
    }
    // If Developer Edition is the only installed Firefox version and no other
    // profiles are present, create a second one for use by other versions.
    // This helps Firefox versions earlier than 35 avoid accidentally using the
    // unsuitable Developer Edition profile.
    let profileService = Cc["@mozilla.org/toolkit/profile-service;1"]
                         .getService(Ci.nsIToolkitProfileService);
    let profileCount = profileService.profileCount;
    if (profileCount == 1 && profileService.selectedProfile.name != "default") {
      let newProfile;
      try {
        newProfile = profileService.createProfile(null, "default");
        profileService.defaultProfile = newProfile;
        profileService.flush();
      } catch (e) {
        Cu.reportError("Could not create profile 'default': " + e);
      }
      if (newProfile) {
        // We don't want a default profile with Developer Edition settings, an
        // empty profile directory will do. The profile service of the other
        // Firefox will populate it with its own stuff.
        let newProfilePath = newProfile.rootDir.path;
        OS.File.removeDir(newProfilePath).then(() => {
          return OS.File.makeDir(newProfilePath);
        }).catch(e => {
          Cu.reportError("Could not empty profile 'default': " + e);
        });
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
    // 5. The browser will be restarted.
    //
    // Otherwise these are the conditions and the associated dialogs that will be shown:
    // 1. aQuitType == "lastwindow" or "quit" and browser.showQuitWarning == true
    //    - The quit dialog will be shown
    // 2. aQuitType == "lastwindow" && browser.tabs.warnOnClose == true
    //    - The "closing multiple tabs" dialog will be shown
    //
    // aQuitType == "lastwindow" is overloaded. "lastwindow" is used to indicate
    // "the last window is closing but we're not quitting (a non-browser window is open)"
    // and also "we're quitting by closing the last window".

    if (aQuitType == "restart")
      return;

    var windowcount = 0;
    var pagecount = 0;
    var browserEnum = Services.wm.getEnumerator("navigator:browser");
    let allWindowsPrivate = true;
    while (browserEnum.hasMoreElements()) {
      // XXXbz should we skip closed windows here?
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

    // browser.warnOnQuit is a hidden global boolean to override all quit prompts
    // browser.showQuitWarning specifically covers quitting
    // browser.tabs.warnOnClose is the global "warn when closing multiple tabs" pref

    var sessionWillBeRestored = Services.prefs.getIntPref("browser.startup.page") == 3 ||
                                Services.prefs.getBoolPref("browser.sessionstore.resume_session_once");
    if (sessionWillBeRestored || !Services.prefs.getBoolPref("browser.warnOnQuit"))
      return;

    let win = Services.wm.getMostRecentWindow("navigator:browser");

    // On last window close or quit && showQuitWarning, we want to show the
    // quit warning.
    if (!Services.prefs.getBoolPref("browser.showQuitWarning")) {
      if (aQuitType == "lastwindow") {
        // If aQuitType is "lastwindow" and we aren't showing the quit warning,
        // we should show the window closing warning instead. warnAboutClosing
        // tabs checks browser.tabs.warnOnClose and returns if it's ok to close
        // the window. It doesn't actually close the window.
        aCancelQuit.data =
          !win.gBrowser.warnAboutClosingTabs(win.gBrowser.closingTabsEnum.ALL);
      }
      return;
    }

    let prompt = Services.prompt;
    let quitBundle = Services.strings.createBundle("chrome://browser/locale/quitDialog.properties");
    let appName = gBrandBundle.GetStringFromName("brandShortName");
    let quitDialogTitle = quitBundle.formatStringFromName("quitDialogTitle",
                                                          [appName], 1);
    let neverAskText = quitBundle.GetStringFromName("neverAsk2");
    let neverAsk = {value: false};

    let choice;
    if (allWindowsPrivate) {
      let text = quitBundle.formatStringFromName("messagePrivate", [appName], 1);
      let flags = prompt.BUTTON_TITLE_IS_STRING * prompt.BUTTON_POS_0 +
                  prompt.BUTTON_TITLE_IS_STRING * prompt.BUTTON_POS_1 +
                  prompt.BUTTON_POS_0_DEFAULT;
      choice = prompt.confirmEx(win, quitDialogTitle, text, flags,
                                quitBundle.GetStringFromName("quitTitle"),
                                quitBundle.GetStringFromName("cancelTitle"),
                                null,
                                neverAskText, neverAsk);

      // The order of the buttons differs between the prompt.confirmEx calls
      // here so we need to fix this for proper handling below.
      if (choice == 0) {
        choice = 2;
      }
    } else {
      let text = quitBundle.formatStringFromName(
        windowcount == 1 ? "messageNoWindows" : "message", [appName], 1);
      let flags = prompt.BUTTON_TITLE_IS_STRING * prompt.BUTTON_POS_0 +
                  prompt.BUTTON_TITLE_IS_STRING * prompt.BUTTON_POS_1 +
                  prompt.BUTTON_TITLE_IS_STRING * prompt.BUTTON_POS_2 +
                  prompt.BUTTON_POS_0_DEFAULT;
      choice = prompt.confirmEx(win, quitDialogTitle, text, flags,
                                quitBundle.GetStringFromName("saveTitle"),
                                quitBundle.GetStringFromName("cancelTitle"),
                                quitBundle.GetStringFromName("quitTitle"),
                                neverAskText, neverAsk);
    }

    switch (choice) {
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
        // always save state when shutting down
        Services.prefs.setIntPref("browser.startup.page", 3);
      }
      break;
    }
  },

  _showUpdateNotification: function BG__showUpdateNotification() {
    Services.prefs.clearUserPref("app.update.postupdate");

    var um = Cc["@mozilla.org/updates/update-manager;1"].
             getService(Ci.nsIUpdateManager);
    try {
      // If the updates.xml file is deleted then getUpdateAt will throw.
      var update = um.getUpdateAt(0).QueryInterface(Ci.nsIPropertyBag);
    } catch (e) {
      // This should never happen.
      Cu.reportError("Unable to find update: " + e);
      return;
    }

    var actions = update.getProperty("actions");
    if (!actions || actions.indexOf("silent") != -1)
      return;

    var appName = gBrandBundle.GetStringFromName("brandShortName");

    function getNotifyString(aPropData) {
      var propValue = update.getProperty(aPropData.propName);
      if (!propValue) {
        if (aPropData.prefName)
          propValue = Services.urlFormatter.formatURLPref(aPropData.prefName);
        else if (aPropData.stringParams)
          propValue = gBrowserBundle.formatStringFromName(aPropData.stringName,
                                                          aPropData.stringParams,
                                                          aPropData.stringParams.length);
        else
          propValue = gBrowserBundle.GetStringFromName(aPropData.stringName);
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

      let win = RecentWindow.getMostRecentBrowserWindow();
      let notifyBox = win.document.getElementById("high-priority-global-notificationbox");

      let buttons = [
                      {
                        label,
                        accessKey: key,
                        popup:     null,
                        callback(aNotificationBar, aButton) {
                          win.openUILinkIn(url, "tab");
                        }
                      }
                    ];

      notifyBox.appendNotification(text, "post-update-notification",
                                   null, notifyBox.PRIORITY_INFO_LOW,
                                   buttons);
    }

    if (actions.indexOf("showAlert") == -1)
      return;

    let title = getNotifyString({propName: "alertTitle",
                                 stringName: "puAlertTitle",
                                 stringParams: [appName]});
    let text = getNotifyString({propName: "alertText",
                                stringName: "puAlertText",
                                stringParams: [appName]});
    let url = getNotifyString({propName: "alertURL",
                               prefName: "startup.homepage_override_url"});

    function clickCallback(subject, topic, data) {
      // This callback will be called twice but only once with this topic
      if (topic != "alertclickcallback")
        return;
      let win = RecentWindow.getMostRecentBrowserWindow();
      win.openUILinkIn(data, "tab");
    }

    try {
      // This will throw NS_ERROR_NOT_AVAILABLE if the notification cannot
      // be displayed per the idl.
      this.AlertsService.showAlertNotification(null, title, text,
                                          true, url, clickCallback);
    } catch (e) {
      Cu.reportError(e);
    }
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
    let dbStatus = PlacesUtils.history.databaseStatus;

    // Show a notification with a "more info" link for a locked places.sqlite.
    if (dbStatus == PlacesUtils.history.DATABASE_STATUS_LOCKED) {
      // Note: initPlaces should always happen when the first window is ready,
      // in any case, better safe than sorry.
      this._firstWindowReady.then(() => {
        this._showPlacesLockedNotificationBox();
        this._placesBrowserInitComplete = true;
        Services.obs.notifyObservers(null, "places-browser-init-complete");
      });
      return;
    }

    let importBookmarks = !aInitialMigrationPerformed &&
                          (dbStatus == PlacesUtils.history.DATABASE_STATUS_CREATE ||
                           dbStatus == PlacesUtils.history.DATABASE_STATUS_CORRUPT);

    // Check if user or an extension has required to import bookmarks.html
    let importBookmarksHTML = false;
    try {
      importBookmarksHTML =
        Services.prefs.getBoolPref("browser.places.importBookmarksHTML");
      if (importBookmarksHTML)
        importBookmarks = true;
    } catch (ex) {}

    // Support legacy bookmarks.html format for apps that depend on that format.
    let autoExportHTML = Services.prefs.getBoolPref("browser.bookmarks.autoExportHTML", false); // Do not export.
    if (autoExportHTML) {
      // Sqlite.jsm and Places shutdown happen at profile-before-change, thus,
      // to be on the safe side, this should run earlier.
      AsyncShutdown.profileChangeTeardown.addBlocker(
        "Places: export bookmarks.html",
        () => BookmarkHTMLUtils.exportToFile(BookmarkHTMLUtils.defaultPath));
    }

    (async () => {
      // Check if Safe Mode or the user has required to restore bookmarks from
      // default profile's bookmarks.html
      let restoreDefaultBookmarks = false;
      try {
        restoreDefaultBookmarks =
          Services.prefs.getBoolPref("browser.bookmarks.restore_default_bookmarks");
        if (restoreDefaultBookmarks) {
          // Ensure that we already have a bookmarks backup for today.
          await this._backupBookmarks();
          importBookmarks = true;
        }
      } catch (ex) {}

      // This may be reused later, check for "=== undefined" to see if it has
      // been populated already.
      let lastBackupFile;

      // If the user did not require to restore default bookmarks, or import
      // from bookmarks.html, we will try to restore from JSON
      if (importBookmarks && !restoreDefaultBookmarks && !importBookmarksHTML) {
        // get latest JSON backup
        lastBackupFile = await PlacesBackups.getMostRecentBackup();
        if (lastBackupFile) {
          // restore from JSON backup
          await BookmarkJSONUtils.importFromFile(lastBackupFile, true);
          importBookmarks = false;
        } else {
          // We have created a new database but we don't have any backup available
          importBookmarks = true;
          if (await OS.File.exists(BookmarkHTMLUtils.defaultPath)) {
            // If bookmarks.html is available in current profile import it...
            importBookmarksHTML = true;
          } else {
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
        try {
          await this._distributionCustomizer.applyBookmarks();
          await this.ensurePlacesDefaultQueriesInitialized();
        } catch (e) {
          Cu.reportError(e);
        }
      } else {
        // An import operation is about to run.
        // Don't try to recreate smart bookmarks if autoExportHTML is true or
        // smart bookmarks are disabled.
        let smartBookmarksVersion = Services.prefs.getIntPref("browser.places.smartBookmarksVersion", 0);
        if (!autoExportHTML && smartBookmarksVersion != -1)
          Services.prefs.setIntPref("browser.places.smartBookmarksVersion", 0);

        let bookmarksUrl = null;
        if (restoreDefaultBookmarks) {
          // User wants to restore bookmarks.html file from default profile folder
          bookmarksUrl = "chrome://browser/locale/bookmarks.html";
        } else if (await OS.File.exists(BookmarkHTMLUtils.defaultPath)) {
          bookmarksUrl = OS.Path.toFileURI(BookmarkHTMLUtils.defaultPath);
        }

        if (bookmarksUrl) {
          // Import from bookmarks.html file.
          try {
            await BookmarkHTMLUtils.importFromURL(bookmarksUrl, true);
          } catch (e) {
            Cu.reportError("Bookmarks.html file could be corrupt. " + e);
          }
          try {
            // Now apply distribution customized bookmarks.
            // This should always run after Places initialization.
            await this._distributionCustomizer.applyBookmarks();
            // Ensure that smart bookmarks are created once the operation is
            // complete.
            await this.ensurePlacesDefaultQueriesInitialized();
          } catch (e) {
            Cu.reportError(e);
          }

        } else {
          Cu.reportError(new Error("Unable to find bookmarks.html file."));
        }

        // Reset preferences, so we won't try to import again at next run
        if (importBookmarksHTML)
          Services.prefs.setBoolPref("browser.places.importBookmarksHTML", false);
        if (restoreDefaultBookmarks)
          Services.prefs.setBoolPref("browser.bookmarks.restore_default_bookmarks",
                                     false);
      }

      // Initialize bookmark archiving on idle.
      if (!this._bookmarksBackupIdleTime) {
        this._bookmarksBackupIdleTime = BOOKMARKS_BACKUP_IDLE_TIME_SEC;

        // If there is no backup, or the last bookmarks backup is too old, use
        // a more aggressive idle observer.
        if (lastBackupFile === undefined)
          lastBackupFile = await PlacesBackups.getMostRecentBackup();
        if (!lastBackupFile) {
            this._bookmarksBackupIdleTime /= 2;
        } else {
          let lastBackupTime = PlacesBackups.getDateForFile(lastBackupFile);
          let profileLastUse = Services.appinfo.replacedLockTime || Date.now();

          // If there is a backup after the last profile usage date it's fine,
          // regardless its age.  Otherwise check how old is the last
          // available backup compared to that session.
          if (profileLastUse > lastBackupTime) {
            let backupAge = Math.round((profileLastUse - lastBackupTime) / 86400000);
            // Report the age of the last available backup.
            try {
              Services.telemetry
                      .getHistogramById("PLACES_BACKUPS_DAYSFROMLAST")
                      .add(backupAge);
            } catch (ex) {
              Cu.reportError(new Error("Unable to report telemetry."));
            }

            if (backupAge > BOOKMARKS_BACKUP_MAX_INTERVAL_DAYS)
              this._bookmarksBackupIdleTime /= 2;
          }
        }
        this._idleService.addIdleObserver(this, this._bookmarksBackupIdleTime);
      }

    })().catch(ex => {
      Cu.reportError(ex);
    }).then(() => {
      // NB: deliberately after the catch so that we always do this, even if
      // we threw halfway through initializing in the Task above.
      this._placesBrowserInitComplete = true;
      Services.obs.notifyObservers(null, "places-browser-init-complete");
    });
  },

  /**
   * If a backup for today doesn't exist, this creates one.
   */
  _backupBookmarks: function BG__backupBookmarks() {
    return (async function() {
      let lastBackupFile = await PlacesBackups.getMostRecentBackup();
      // Should backup bookmarks if there are no backups or the maximum
      // interval between backups elapsed.
      if (!lastBackupFile ||
          new Date() - PlacesBackups.getDateForFile(lastBackupFile) > BOOKMARKS_BACKUP_MIN_INTERVAL_DAYS * 86400000) {
        let maxBackups = Services.prefs.getIntPref("browser.bookmarks.max_backups");
        await PlacesBackups.create(maxBackups);
      }
    })();
  },

  /**
   * Show the notificationBox for a locked places database.
   */
  _showPlacesLockedNotificationBox: function BG__showPlacesLockedNotificationBox() {
    var applicationName = gBrandBundle.GetStringFromName("brandShortName");
    var placesBundle = Services.strings.createBundle("chrome://browser/locale/places/places.properties");
    var title = placesBundle.GetStringFromName("lockPrompt.title");
    var text = placesBundle.formatStringFromName("lockPrompt.text", [applicationName], 1);
    var buttonText = placesBundle.GetStringFromName("lockPromptInfoButton.label");
    var accessKey = placesBundle.GetStringFromName("lockPromptInfoButton.accessKey");

    var helpTopic = "places-locked";
    var url = Services.urlFormatter.formatURLPref("app.support.baseURL");
    url += helpTopic;

    var win = RecentWindow.getMostRecentBrowserWindow();

    var buttons = [
                    {
                      label:     buttonText,
                      accessKey,
                      popup:     null,
                      callback(aNotificationBar, aButton) {
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

  _showSyncStartedDoorhanger() {
    let bundle = Services.strings.createBundle("chrome://browser/locale/accounts.properties");
    let productName = gBrandBundle.GetStringFromName("brandShortName");
    let title = bundle.GetStringFromName("syncStartNotification.title");
    let body = bundle.formatStringFromName("syncStartNotification.body2",
                                            [productName], 1);

    let clickCallback = (subject, topic, data) => {
      if (topic != "alertclickcallback")
        return;
      this._openPreferences("sync", { origin: "doorhanger" });
    };
    this.AlertsService.showAlertNotification(null, title, body, true, null, clickCallback);
  },

  // eslint-disable-next-line complexity
  _migrateUI: function BG__migrateUI() {
    const UI_VERSION = 57;
    const BROWSER_DOCURL = "chrome://browser/content/browser.xul";

    let currentUIVersion;
    if (Services.prefs.prefHasUserValue("browser.migration.version")) {
      currentUIVersion = Services.prefs.getIntPref("browser.migration.version");
    } else {
      // This is a new profile, nothing to migrate.
      Services.prefs.setIntPref("browser.migration.version", UI_VERSION);
      return;
    }

    if (currentUIVersion >= UI_VERSION)
      return;

    let xulStore = Cc["@mozilla.org/xul/xulstore;1"].getService(Ci.nsIXULStore);

    if (currentUIVersion < 14) {
      // DOM Storage doesn't specially handle about: pages anymore.
      let path = OS.Path.join(OS.Constants.Path.profileDir,
                              "chromeappsstore.sqlite");
      OS.File.remove(path);
    }

    if (currentUIVersion < 16) {
      xulStore.removeValue(BROWSER_DOCURL, "nav-bar", "collapsed");
    }

    // Insert the bookmarks-menu-button into the nav-bar if it isn't already
    // there.
    if (currentUIVersion < 17) {
      let currentset = xulStore.getValue(BROWSER_DOCURL, "nav-bar", "currentset");
      // Need to migrate only if toolbar is customized.
      if (currentset) {
        if (!currentset.includes("bookmarks-menu-button")) {
          // The button isn't in the nav-bar, so let's look for an appropriate
          // place to put it.
          if (currentset.includes("bookmarks-menu-button-container")) {
            currentset = currentset.replace(/(^|,)bookmarks-menu-button-container($|,)/,
                                            "$1bookmarks-menu-button$2");
          } else if (currentset.includes("downloads-button")) {
            currentset = currentset.replace(/(^|,)downloads-button($|,)/,
                                            "$1bookmarks-menu-button,downloads-button$2");
          } else if (currentset.includes("home-button")) {
            currentset = currentset.replace(/(^|,)home-button($|,)/,
                                            "$1bookmarks-menu-button,home-button$2");
          } else {
            // Just append.
            currentset = currentset.replace(/(^|,)window-controls($|,)/,
                                            "$1bookmarks-menu-button,window-controls$2");
          }
          xulStore.setValue(BROWSER_DOCURL, "nav-bar", "currentset", currentset);
        }
      }
    }

    if (currentUIVersion < 18) {
      // Remove iconsize and mode from all the toolbars
      let toolbars = ["navigator-toolbox", "nav-bar", "PersonalToolbar",
                      "TabsToolbar", "toolbar-menubar"];
      for (let resourceName of ["mode", "iconsize"]) {
        for (let toolbarId of toolbars) {
          xulStore.removeValue(BROWSER_DOCURL, toolbarId, resourceName);
        }
      }
    }

    if (currentUIVersion < 19) {
      let detector = null;
      try {
        detector = Services.prefs.getComplexValue("intl.charset.detector",
                                                  Ci.nsIPrefLocalizedString).data;
      } catch (ex) {}
      if (!(detector == "" ||
            detector == "ja_parallel_state_machine" ||
            detector == "ruprob" ||
            detector == "ukprob")) {
        // If the encoding detector pref value is not reachable from the UI,
        // reset to default (varies by localization).
        Services.prefs.clearUserPref("intl.charset.detector");
      }
    }

    if (currentUIVersion < 20) {
      // Remove persisted collapsed state from TabsToolbar.
      xulStore.removeValue(BROWSER_DOCURL, "TabsToolbar", "collapsed");
    }

    if (currentUIVersion < 23) {
      const kSelectedEnginePref = "browser.search.selectedEngine";
      if (Services.prefs.prefHasUserValue(kSelectedEnginePref)) {
        try {
          let name = Services.prefs.getComplexValue(kSelectedEnginePref,
                                                    Ci.nsIPrefLocalizedString).data;
          Services.search.currentEngine = Services.search.getEngineByName(name);
        } catch (ex) {}
      }
    }

    if (currentUIVersion < 24) {
      // Reset homepage pref for users who have it set to start.mozilla.org
      // or google.com/firefox.
      const HOMEPAGE_PREF = "browser.startup.homepage";
      if (Services.prefs.prefHasUserValue(HOMEPAGE_PREF)) {
        const DEFAULT =
          Services.prefs.getDefaultBranch(HOMEPAGE_PREF)
                        .getComplexValue("", Ci.nsIPrefLocalizedString).data;
        let value = Services.prefs.getStringPref(HOMEPAGE_PREF);
        let updated =
          value.replace(/https?:\/\/start\.mozilla\.org[^|]*/i, DEFAULT)
               .replace(/https?:\/\/(www\.)?google\.[a-z.]+\/firefox[^|]*/i,
                        DEFAULT);
        if (updated != value) {
          if (updated == DEFAULT) {
            Services.prefs.clearUserPref(HOMEPAGE_PREF);
          } else {
            Services.prefs.setStringPref(HOMEPAGE_PREF, updated);
          }
        }
      }
    }

    if (currentUIVersion < 25) {
      // Make sure the doNotTrack value conforms to the conversion from
      // three-state to two-state. (This reverts a setting of "please track me"
      // to the default "don't say anything").
      try {
        if (Services.prefs.getBoolPref("privacy.donottrackheader.enabled") &&
            Services.prefs.getIntPref("privacy.donottrackheader.value") != 1) {
          Services.prefs.clearUserPref("privacy.donottrackheader.enabled");
          Services.prefs.clearUserPref("privacy.donottrackheader.value");
        }
      } catch (ex) {}
    }

    if (currentUIVersion < 26) {
      // Refactor urlbar suggestion preferences to make it extendable and
      // allow new suggestion types (e.g: search suggestions).
      let types = ["history", "bookmark", "openpage"];
      let defaultBehavior = Services.prefs.getIntPref("browser.urlbar.default.behavior", 0);
      try {
        let autocompleteEnabled = Services.prefs.getBoolPref("browser.urlbar.autocomplete.enabled");
        if (!autocompleteEnabled) {
          defaultBehavior = -1;
        }
      } catch (ex) {}

      // If the default behavior is:
      //    -1  - all new "...suggest.*" preferences will be false
      //     0  - all new "...suggest.*" preferences will use the default values
      //   > 0  - all new "...suggest.*" preferences will be inherited
      for (let type of types) {
        let prefValue = defaultBehavior == 0;
        if (defaultBehavior > 0) {
          prefValue = !!(defaultBehavior & Ci.mozIPlacesAutoComplete["BEHAVIOR_" + type.toUpperCase()]);
        }
        Services.prefs.setBoolPref("browser.urlbar.suggest." + type, prefValue);
      }

      // Typed behavior will be used only for results from history.
      if (defaultBehavior != -1 &&
          !!(defaultBehavior & Ci.mozIPlacesAutoComplete.BEHAVIOR_TYPED)) {
        Services.prefs.setBoolPref("browser.urlbar.suggest.history.onlyTyped", true);
      }
    }

    if (currentUIVersion < 27) {
      // Fix up document color use:
      const kOldColorPref = "browser.display.use_document_colors";
      if (Services.prefs.prefHasUserValue(kOldColorPref) &&
          !Services.prefs.getBoolPref(kOldColorPref)) {
        Services.prefs.setIntPref("browser.display.document_color_use", 2);
      }
    }

    if (currentUIVersion < 29) {
      let group = null;
      try {
        group = Services.prefs.getComplexValue("font.language.group",
                                               Ci.nsIPrefLocalizedString);
      } catch (ex) {}
      if (group &&
          ["tr", "x-baltic", "x-central-euro"].some(g => g == group.data)) {
        // Latin groups were consolidated.
        group.data = "x-western";
        Services.prefs.setComplexValue("font.language.group",
                                       Ci.nsIPrefLocalizedString, group);
      }
    }

    if (currentUIVersion < 30) {
      // Convert old devedition theme pref to lightweight theme storage
      let lightweightThemeSelected = false;
      let selectedThemeID = null;
      try {
        lightweightThemeSelected = Services.prefs.prefHasUserValue("lightweightThemes.selectedThemeID");
        selectedThemeID = Services.prefs.getCharPref("lightweightThemes.selectedThemeID");
      } catch (e) {}

      let defaultThemeSelected = false;
      try {
         defaultThemeSelected = Services.prefs.getCharPref("general.skins.selectedSkin") == "classic/1.0";
      } catch (e) {}

      // If we are on the devedition channel, the devedition theme is on by
      // default.  But we need to handle the case where they didn't want it
      // applied, and unapply the theme.
      let userChoseToNotUseDeveditionTheme =
        !defaultThemeSelected ||
        (lightweightThemeSelected && selectedThemeID != "firefox-devedition@mozilla.org");

      if (userChoseToNotUseDeveditionTheme && selectedThemeID == "firefox-devedition@mozilla.org") {
        Services.prefs.setCharPref("lightweightThemes.selectedThemeID", "");
      }

      Services.prefs.clearUserPref("browser.devedition.showCustomizeButton");
    }

    if (currentUIVersion < 31) {
      xulStore.removeValue(BROWSER_DOCURL, "bookmarks-menu-button", "class");
      xulStore.removeValue(BROWSER_DOCURL, "home-button", "class");
    }

    if (currentUIVersion < 36) {
      xulStore.removeValue("chrome://passwordmgr/content/passwordManager.xul",
                           "passwordCol",
                           "hidden");
    }

    if (currentUIVersion < 37) {
      Services.prefs.clearUserPref("browser.sessionstore.restore_on_demand");
    }

    if (currentUIVersion < 38) {
      LoginHelper.removeLegacySignonFiles();
    }

    if (currentUIVersion < 39) {
      // Remove the 'defaultset' value for all the toolbars
      let toolbars = ["nav-bar", "PersonalToolbar",
                      "TabsToolbar", "toolbar-menubar"];
      for (let toolbarId of toolbars) {
        xulStore.removeValue(BROWSER_DOCURL, toolbarId, "defaultset");
      }
    }

    if (currentUIVersion < 40) {
      const kOldSafeBrowsingPref = "browser.safebrowsing.enabled";
      // Default value is set to true, a user pref means that the pref was
      // set to false.
      if (Services.prefs.prefHasUserValue(kOldSafeBrowsingPref) &&
          !Services.prefs.getBoolPref(kOldSafeBrowsingPref)) {
        Services.prefs.setBoolPref("browser.safebrowsing.phishing.enabled",
                                   false);
        // Should just remove support for the pref entirely, even if it's
        // only in about:config
        Services.prefs.clearUserPref(kOldSafeBrowsingPref);
      }
    }

    if (currentUIVersion < 41) {
      const Preferences = Cu.import("resource://gre/modules/Preferences.jsm", {}).Preferences;
      Preferences.resetBranch("loop.");
    }

    if (currentUIVersion < 42) {
      let backupFile = Services.dirsvc.get("ProfD", Ci.nsIFile);
      backupFile.append("tabgroups-session-backup.json");
      OS.File.remove(backupFile.path, {ignoreAbsent: true}).catch(ex => Cu.reportError(ex));
    }

    if (currentUIVersion < 43) {
      let currentTheme = Services.prefs.getCharPref("lightweightThemes.selectedThemeID", "");
      if (currentTheme == "firefox-devedition@mozilla.org") {
        let newTheme = Services.prefs.getCharPref("devtools.theme") == "dark" ?
          "firefox-compact-dark@mozilla.org" : "firefox-compact-light@mozilla.org";
        Services.prefs.setCharPref("lightweightThemes.selectedThemeID", newTheme);
      }
    }

    if (currentUIVersion < 44) {
      // Merge the various cosmetic animation prefs into one. If any were set to
      // disable animations, we'll disabled cosmetic animations entirely.
      let animate = Services.prefs.getBoolPref("browser.tabs.animate", true) &&
                    Services.prefs.getBoolPref("browser.fullscreen.animate", true) &&
                    !Services.prefs.getBoolPref("alerts.disableSlidingEffect", false);

      Services.prefs.setBoolPref("toolkit.cosmeticAnimations.enabled", animate);

      Services.prefs.clearUserPref("browser.tabs.animate");
      Services.prefs.clearUserPref("browser.fullscreen.animate");
      Services.prefs.clearUserPref("alerts.disableSlidingEffect");
    }

    if (currentUIVersion < 45) {
      const LEGACY_PREF = "browser.shell.skipDefaultBrowserCheck";
      if (Services.prefs.prefHasUserValue(LEGACY_PREF)) {
        Services.prefs.setBoolPref("browser.shell.didSkipDefaultBrowserCheckOnFirstRun",
                                   !Services.prefs.getBoolPref(LEGACY_PREF));
        Services.prefs.clearUserPref(LEGACY_PREF);
      }
    }

    // Version 46 has been replaced by 47
    if (currentUIVersion < 47) {
      // Search suggestions are now on by default.
      // For privacy reasons, we want to respect previously made user's choice
      // regarding the feature, so if it's known reflect that choice into the
      // current pref.
      // Note that in case of downgrade/upgrade we won't guarantee anything.
      try {
        if (Services.prefs.prefHasUserValue("browser.urlbar.searchSuggestionsChoice")) {
          Services.prefs.setBoolPref(
            "browser.urlbar.suggest.searches",
            Services.prefs.getBoolPref("browser.urlbar.searchSuggestionsChoice")
          );
        } else if (Services.prefs.getBoolPref("browser.urlbar.userMadeSearchSuggestionsChoice")) {
          // If the user made a choice but searchSuggestionsChoice is not set,
          // something went wrong in the upgrade path. For example, due to a
          // now fixed bug, some profilespicking "no" at the opt-in bar and
          // upgrading in the same session wouldn't mirror the pref.
          // Users could also lack the mirrored pref due to skipping one version.
          // In this case just fallback to the safest side and disable suggestions.
          Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
        }
      } catch (ex) {
        // A missing pref is not a fatal error.
      }
    }

    if (currentUIVersion < 49) {
      // Annotate that a user haven't seen any onboarding tour
      Services.prefs.setIntPref("browser.onboarding.seen-tourset-version", 0);
    }

    if (currentUIVersion < 50) {
      try {
        // Transform prefs related to old DevTools Console.
        // The following prefs might be missing when the old DevTools Console
        // front-end is removed.
        // See also: https://bugzilla.mozilla.org/show_bug.cgi?id=1381834
        if (Services.prefs.getBoolPref("devtools.webconsole.filter.networkinfo")) {
          Services.prefs.setBoolPref("devtools.webconsole.filter.net", true);
        }
        if (Services.prefs.getBoolPref("devtools.webconsole.filter.cssparser")) {
          Services.prefs.setBoolPref("devtools.webconsole.filter.css", true);
        }
      } catch (ex) {
        // It's ok if a pref is missing.
      }
    }

    if (currentUIVersion < 51) {
      // Switch to compact UI density if the user is using a formerly compact
      // dark or light theme.
      let currentTheme = Services.prefs.getCharPref("lightweightThemes.selectedThemeID", "");
      if (currentTheme == "firefox-compact-dark@mozilla.org" ||
          currentTheme == "firefox-compact-light@mozilla.org") {
        Services.prefs.setIntPref("browser.uidensity", 1);
      }
    }

    if (currentUIVersion < 52) {
      // Keep old devtools log persistence behavior after splitting netmonitor and
      // webconsole prefs (bug 1307881).
      if (Services.prefs.getBoolPref("devtools.webconsole.persistlog", false)) {
        Services.prefs.setBoolPref("devtools.netmonitor.persistlog", true);
      }
    }

    // Update user customizations that will interfere with the Safe Browsing V2
    // to V4 migration (bug 1395419).
    if (currentUIVersion < 53) {
      const MALWARE_PREF = "urlclassifier.malwareTable";
      if (Services.prefs.prefHasUserValue(MALWARE_PREF)) {
        let malwareList = Services.prefs.getCharPref(MALWARE_PREF);
        if (malwareList.indexOf("goog-malware-shavar") != -1) {
          malwareList.replace("goog-malware-shavar", "goog-malware-proto");
          Services.prefs.setCharPref(MALWARE_PREF, malwareList);
        }
      }
    }

    if (currentUIVersion < 54) {
      // Migrate browser.onboarding.hidden to browser.onboarding.state.
      if (Services.prefs.prefHasUserValue("browser.onboarding.hidden")) {
        let state = Services.prefs.getBoolPref("browser.onboarding.hidden") ? "watermark" : "default";
        Services.prefs.setStringPref("browser.onboarding.state", state);
        Services.prefs.clearUserPref("browser.onboarding.hidden");
      }
    }

    if (currentUIVersion < 55) {
      Services.prefs.clearUserPref("browser.customizemode.tip0.shown");
    }

    if (currentUIVersion < 56) {
      // Prior to the end of the Firefox 57 cycle, the sidebarcommand being present
      // or not was the only thing that distinguished whether the sidebar was open.
      // Now, the sidebarcommand always indicates the last opened sidebar, and we
      // correctly persist the checked attribute to indicate whether or not the
      // sidebar was open. We should set the checked attribute in case it wasn't:
      if (xulStore.getValue(BROWSER_DOCURL, "sidebar-box", "sidebarcommand")) {
        xulStore.setValue(BROWSER_DOCURL, "sidebar-box", "checked", "true");
      }
    }

    if (currentUIVersion < 57) {
      // Beginning Firefox 57, the theme accent color is shown as highlight
      // on top of tabs. This didn't look too good with the "A Web Browser Renaissance"
      // theme, so we're changing its accent color.
      let lwthemePrefs = Services.prefs.getBranch("lightweightThemes.");
      if (lwthemePrefs.prefHasUserValue("usedThemes")) {
        try {
          let usedThemes = lwthemePrefs.getStringPref("usedThemes");
          usedThemes = JSON.parse(usedThemes);
          let renaissanceTheme = usedThemes.find(theme => theme.id == "recommended-1");
          if (renaissanceTheme) {
            renaissanceTheme.accentcolor = "#834d29";
            lwthemePrefs.setStringPref("usedThemes", JSON.stringify(usedThemes));
          }
        } catch (e) { /* Don't panic if this pref isn't what we expect it to be. */ }
      }
    }

    // Update the migration version.
    Services.prefs.setIntPref("browser.migration.version", UI_VERSION);
  },

  _checkForDefaultBrowser() {
    // Perform default browser checking.
    if (!ShellService) {
      return;
    }

    let shouldCheck = AppConstants.DEBUG ? false :
                                           ShellService.shouldCheckDefaultBrowser;

    const skipDefaultBrowserCheck =
      Services.prefs.getBoolPref("browser.shell.skipDefaultBrowserCheckOnFirstRun") &&
      !Services.prefs.getBoolPref("browser.shell.didSkipDefaultBrowserCheckOnFirstRun");

    const usePromptLimit = !AppConstants.RELEASE_OR_BETA;
    let promptCount =
      usePromptLimit ? Services.prefs.getIntPref("browser.shell.defaultBrowserCheckCount") : 0;

    let willRecoverSession = false;
    try {
      let ss = Cc["@mozilla.org/browser/sessionstartup;1"].
               getService(Ci.nsISessionStartup);
      willRecoverSession =
        (ss.sessionType == Ci.nsISessionStartup.RECOVER_SESSION);
    } catch (ex) { /* never mind; suppose SessionStore is broken */ }

    // startup check, check all assoc
    let isDefault = false;
    let isDefaultError = false;
    try {
      isDefault = ShellService.isDefaultBrowser(true, false);
    } catch (ex) {
      isDefaultError = true;
    }

    if (isDefault) {
      let now = (Math.floor(Date.now() / 1000)).toString();
      Services.prefs.setCharPref("browser.shell.mostRecentDateSetAsDefault", now);
    }

    let willPrompt = shouldCheck && !isDefault && !willRecoverSession;

    // Skip the "Set Default Browser" check during first-run or after the
    // browser has been run a few times.
    if (willPrompt) {
      if (skipDefaultBrowserCheck) {
        Services.prefs.setBoolPref("browser.shell.didSkipDefaultBrowserCheckOnFirstRun", true);
        willPrompt = false;
      } else {
        promptCount++;
      }
      if (usePromptLimit && promptCount > 3) {
        willPrompt = false;
      }
    }

    if (usePromptLimit && willPrompt) {
      Services.prefs.setIntPref("browser.shell.defaultBrowserCheckCount", promptCount);
    }

    try {
      // Report default browser status on startup to telemetry
      // so we can track whether we are the default.
      Services.telemetry.getHistogramById("BROWSER_IS_USER_DEFAULT")
                        .add(isDefault);
      Services.telemetry.getHistogramById("BROWSER_IS_USER_DEFAULT_ERROR")
                        .add(isDefaultError);
      Services.telemetry.getHistogramById("BROWSER_SET_DEFAULT_ALWAYS_CHECK")
                        .add(shouldCheck);
      Services.telemetry.getHistogramById("BROWSER_SET_DEFAULT_DIALOG_PROMPT_RAWCOUNT")
                        .add(promptCount);
    } catch (ex) { /* Don't break the default prompt if telemetry is broken. */ }

    if (willPrompt) {
      DefaultBrowserCheck.prompt(RecentWindow.getMostRecentBrowserWindow());
    }
  },

  // ------------------------------
  // public nsIBrowserGlue members
  // ------------------------------

  sanitize: function BG_sanitize(aParentWindow) {
    this._sanitizer.sanitize(aParentWindow);
  },

  async ensurePlacesDefaultQueriesInitialized() {
    // This is the current smart bookmarks version, it must be increased every
    // time they change.
    // When adding a new smart bookmark below, its newInVersion property must
    // be set to the version it has been added in.  We will compare its value
    // to users' smartBookmarksVersion and add new smart bookmarks without
    // recreating old deleted ones.
    const SMART_BOOKMARKS_VERSION = 8;
    const SMART_BOOKMARKS_ANNO = "Places/SmartBookmark";
    const SMART_BOOKMARKS_PREF = "browser.places.smartBookmarksVersion";

    // TODO bug 399268: should this be a pref?
    const MAX_RESULTS = 10;

    // Get current smart bookmarks version.  If not set, create them.
    let smartBookmarksCurrentVersion = Services.prefs.getIntPref(SMART_BOOKMARKS_PREF, 0);

    // If version is current, or smart bookmarks are disabled, bail out.
    if (smartBookmarksCurrentVersion == -1 ||
        smartBookmarksCurrentVersion >= SMART_BOOKMARKS_VERSION) {
      return;
    }

    try {
      let menuIndex = 0;
      let toolbarIndex = 0;
      let bundle = Services.strings.createBundle("chrome://browser/locale/places/places.properties");
      let queryOptions = Ci.nsINavHistoryQueryOptions;

      let smartBookmarks = {
        MostVisited: {
          title: bundle.GetStringFromName("mostVisitedTitle"),
          url: "place:sort=" + queryOptions.SORT_BY_VISITCOUNT_DESCENDING +
                    "&maxResults=" + MAX_RESULTS,
          parentGuid: PlacesUtils.bookmarks.toolbarGuid,
          newInVersion: 1
        },
        RecentTags: {
          title: bundle.GetStringFromName("recentTagsTitle"),
          url: "place:type=" + queryOptions.RESULTS_AS_TAG_QUERY +
                    "&sort=" + queryOptions.SORT_BY_LASTMODIFIED_DESCENDING +
                    "&maxResults=" + MAX_RESULTS,
          parentGuid: PlacesUtils.bookmarks.menuGuid,
          newInVersion: 1
        },
      };

      // Set current guid, parentGuid and index of existing Smart Bookmarks.
      // We will use those to create a new version of the bookmark at the same
      // position.
      let smartBookmarkItemIds = PlacesUtils.annotations.getItemsWithAnnotation(SMART_BOOKMARKS_ANNO);
      for (let itemId of smartBookmarkItemIds) {
        let queryId = PlacesUtils.annotations.getItemAnnotation(itemId, SMART_BOOKMARKS_ANNO);
        if (queryId in smartBookmarks) {
          // Known smart bookmark.
          let smartBookmark = smartBookmarks[queryId];
          smartBookmark.guid = await PlacesUtils.promiseItemGuid(itemId);

          if (!smartBookmark.url) {
            await PlacesUtils.bookmarks.remove(smartBookmark.guid);
            continue;
          }

          let bm = await PlacesUtils.bookmarks.fetch(smartBookmark.guid);
          smartBookmark.parentGuid = bm.parentGuid;
          smartBookmark.index = bm.index;
        } else {
          // We don't remove old Smart Bookmarks because user could still
          // find them useful, or could have personalized them.
          // Instead we remove the Smart Bookmark annotation.
          PlacesUtils.annotations.removeItemAnnotation(itemId, SMART_BOOKMARKS_ANNO);
        }
      }

      for (let queryId of Object.keys(smartBookmarks)) {
        let smartBookmark = smartBookmarks[queryId];

        // We update or create only changed or new smart bookmarks.
        // Also we respect user choices, so we won't try to create a smart
        // bookmark if it has been removed.
        if (smartBookmarksCurrentVersion > 0 &&
            smartBookmark.newInVersion <= smartBookmarksCurrentVersion &&
            !smartBookmark.guid || !smartBookmark.url)
          continue;

        // Remove old version of the smart bookmark if it exists, since it
        // will be replaced in place.
        if (smartBookmark.guid) {
          await PlacesUtils.bookmarks.remove(smartBookmark.guid);
        }

        // Create the new smart bookmark and store its updated guid.
        if (!("index" in smartBookmark)) {
          if (smartBookmark.parentGuid == PlacesUtils.bookmarks.toolbarGuid)
            smartBookmark.index = toolbarIndex++;
          else if (smartBookmark.parentGuid == PlacesUtils.bookmarks.menuGuid)
            smartBookmark.index = menuIndex++;
        }
        smartBookmark = await PlacesUtils.bookmarks.insert(smartBookmark);
        let itemId = await PlacesUtils.promiseItemId(smartBookmark.guid);
        PlacesUtils.annotations.setItemAnnotation(itemId,
                                                  SMART_BOOKMARKS_ANNO,
                                                  queryId, 0,
                                                  PlacesUtils.annotations.EXPIRE_NEVER);
      }

      // If we are creating all Smart Bookmarks from ground up, add a
      // separator below them in the bookmarks menu.
      if (smartBookmarksCurrentVersion == 0 &&
          smartBookmarkItemIds.length == 0) {
        let bm = await PlacesUtils.bookmarks.fetch({ parentGuid: PlacesUtils.bookmarks.menuGuid,
                                                     index: menuIndex });
        // Don't add a separator if the menu was empty or there is one already.
        if (bm && bm.type != PlacesUtils.bookmarks.TYPE_SEPARATOR) {
          await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
                                               parentGuid: PlacesUtils.bookmarks.menuGuid,
                                               index: menuIndex });
        }
      }
    } catch (ex) {
      Cu.reportError(ex);
    } finally {
      Services.prefs.setIntPref(SMART_BOOKMARKS_PREF, SMART_BOOKMARKS_VERSION);
      Services.prefs.savePrefFile(null);
    }
  },

  /**
   * Open preferences even if there are no open windows.
   */
  _openPreferences(...args) {
    if (Services.appShell.hiddenDOMWindow.openPreferences) {
      Services.appShell.hiddenDOMWindow.openPreferences(...args);
      return;
    }

    let chromeWindow = RecentWindow.getMostRecentBrowserWindow();
    chromeWindow.openPreferences(...args);
  },

  _openURLInNewWindow(url) {
    let urlString = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
    urlString.data = url;
    return new Promise(resolve => {
      let win = Services.ww.openWindow(null, Services.prefs.getCharPref("browser.chromeURL"),
                                       "_blank", "chrome,all,dialog=no", urlString);
      win.addEventListener("load", () => { resolve(win); }, {once: true});
    });
  },

  /**
   * Called as an observer when Sync's "display URIs" notification is fired.
   *
   * We open the received URIs in background tabs.
   */
  async _onDisplaySyncURIs(data) {
    try {
      // The payload is wrapped weirdly because of how Sync does notifications.
      const URIs = data.wrappedJSObject.object;

      // win can be null, but it's ok, we'll assign it later in openTab()
      let win = RecentWindow.getMostRecentBrowserWindow({private: false});

      const openTab = async (URI) => {
        let tab;
        if (!win) {
          win = await this._openURLInNewWindow(URI.uri);
          let tabs = win.gBrowser.tabs;
          tab = tabs[tabs.length - 1];
        } else {
          tab = win.gBrowser.addTab(URI.uri);
        }
        tab.setAttribute("attention", true);
        return tab;
      };

      const firstTab = await openTab(URIs[0]);
      await Promise.all(URIs.slice(1).map(URI => openTab(URI)));

      let title, body;
      const deviceName = Weave.Service.clientsEngine.getClientName(URIs[0].clientId);
      const bundle = Services.strings.createBundle("chrome://browser/locale/accounts.properties");
      if (URIs.length == 1) {
        // Due to bug 1305895, tabs from iOS may not have device information, so
        // we have separate strings to handle those cases. (See Also
        // unnamedTabsArrivingNotificationNoDevice.body below)
        if (deviceName) {
          title = bundle.formatStringFromName("tabArrivingNotificationWithDevice.title", [deviceName], 1);
        } else {
          title = bundle.GetStringFromName("tabArrivingNotification.title");
        }
        // Use the page URL as the body. We strip the fragment and query to
        // reduce size, and also format it the same way that the url bar would.
        body = URIs[0].uri.replace(/[?#].*$/, "");
        if (win.gURLBar) {
          body = win.gURLBar.trimValue(body);
        }
      } else {
        title = bundle.GetStringFromName("multipleTabsArrivingNotification.title");
        const allSameDevice = URIs.every(URI => URI.clientId == URIs[0].clientId);
        const unknownDevice = allSameDevice && !deviceName;
        let tabArrivingBody;
        if (unknownDevice) {
          tabArrivingBody = "unnamedTabsArrivingNotificationNoDevice.body";
        } else if (allSameDevice) {
          tabArrivingBody = "unnamedTabsArrivingNotification2.body";
        } else {
          tabArrivingBody = "unnamedTabsArrivingNotificationMultiple2.body";
        }

        body = bundle.GetStringFromName(tabArrivingBody);
        body = PluralForm.get(URIs.length, body);
        body = body.replace("#1", URIs.length);
        body = body.replace("#2", deviceName);
      }

      const clickCallback = (obsSubject, obsTopic, obsData) => {
        if (obsTopic == "alertclickcallback") {
          win.gBrowser.selectedTab = firstTab;
        }
      };

      // Specify an icon because on Windows no icon is shown at the moment
      let imageURL;
      if (AppConstants.platform == "win") {
        imageURL = "chrome://branding/content/icon64.png";
      }
      this.AlertsService.showAlertNotification(imageURL, title, body, true, null, clickCallback);
    } catch (ex) {
      Cu.reportError("Error displaying tab(s) received by Sync: " + ex);
    }
  },

  async _onVerifyLoginNotification({body, title, url}) {
    let tab;
    let imageURL;
    if (AppConstants.platform == "win") {
      imageURL = "chrome://branding/content/icon64.png";
    }
    let win = RecentWindow.getMostRecentBrowserWindow({private: false});
    if (!win) {
      win = await this._openURLInNewWindow(url);
      let tabs = win.gBrowser.tabs;
      tab = tabs[tabs.length - 1];
    } else {
      tab = win.gBrowser.addTab(url);
    }
    tab.setAttribute("attention", true);
    let clickCallback = (subject, topic, data) => {
      if (topic != "alertclickcallback")
        return;
      win.gBrowser.selectedTab = tab;
    };

    try {
      this.AlertsService.showAlertNotification(imageURL, title, body, true, null, clickCallback);
    } catch (ex) {
      Cu.reportError("Error notifying of a verify login event: " + ex);
    }
  },

  _onDeviceConnected(deviceName) {
    let accountsBundle = Services.strings.createBundle(
      "chrome://browser/locale/accounts.properties"
    );
    let title = accountsBundle.GetStringFromName("deviceConnectedTitle");
    let body = accountsBundle.formatStringFromName("deviceConnectedBody" +
                                                   (deviceName ? "" : ".noDeviceName"),
                                                   [deviceName], 1);

    let clickCallback = async (subject, topic, data) => {
      if (topic != "alertclickcallback")
        return;
      let url = await this.fxAccounts.promiseAccountsManageDevicesURI("device-connected-notification");
      let win = RecentWindow.getMostRecentBrowserWindow({private: false});
      if (!win) {
        this._openURLInNewWindow(url);
      } else {
        win.gBrowser.addTab(url);
      }
    };

    try {
      this.AlertsService.showAlertNotification(null, title, body, true, null, clickCallback);
    } catch (ex) {
      Cu.reportError("Error notifying of a new Sync device: " + ex);
    }
  },

  _onDeviceDisconnected() {
    let bundle = Services.strings.createBundle("chrome://browser/locale/accounts.properties");
    let title = bundle.GetStringFromName("deviceDisconnectedNotification.title");
    let body = bundle.GetStringFromName("deviceDisconnectedNotification.body");

    let clickCallback = (subject, topic, data) => {
      if (topic != "alertclickcallback")
        return;
      this._openPreferences("sync", { origin: "devDisconnectedAlert"});
    };
    this.AlertsService.showAlertNotification(null, title, body, true, null, clickCallback);
  },

  _handleFlashHang() {
    ++this._flashHangCount;
    if (this._flashHangCount < 2) {
      return;
    }
    // protected mode only applies to win32
    if (Services.appinfo.XPCOMABI != "x86-msvc") {
      return;
    }

    if (Services.prefs.getBoolPref("dom.ipc.plugins.flash.disable-protected-mode")) {
      return;
    }
    if (!Services.prefs.getBoolPref("browser.flash-protected-mode-flip.enable")) {
      return;
    }
    if (Services.prefs.getBoolPref("browser.flash-protected-mode-flip.done")) {
      return;
    }
    Services.prefs.setBoolPref("dom.ipc.plugins.flash.disable-protected-mode", true);
    Services.prefs.setBoolPref("browser.flash-protected-mode-flip.done", true);

    let win = RecentWindow.getMostRecentBrowserWindow();
    if (!win) {
      return;
    }
    let productName = gBrandBundle.GetStringFromName("brandShortName");
    let message = win.gNavigatorBundle.
      getFormattedString("flashHang.message", [productName]);
    let buttons = [{
      label: win.gNavigatorBundle.getString("flashHang.helpButton.label"),
      accessKey: win.gNavigatorBundle.getString("flashHang.helpButton.accesskey"),
      callback() {
        win.openUILinkIn("https://support.mozilla.org/kb/flash-protected-mode-autodisabled", "tab");
      }
    }];
    let nb = win.document.getElementById("global-notificationbox");
    nb.appendNotification(message, "flash-hang", null,
                          nb.PRIORITY_INFO_MEDIUM, buttons);
  },

  _updateFxaBadges() {
    let state = UIState.get();
    if (state.status == UIState.STATUS_LOGIN_FAILED ||
        state.status == UIState.STATUS_NOT_VERIFIED) {
      AppMenuNotifications.showBadgeOnlyNotification("fxa-needs-authentication");
    } else {
      AppMenuNotifications.removeNotification("fxa-needs-authentication");
    }
  },

  // for XPCOM
  classID:          Components.ID("{eab9012e-5f74-4cbc-b2b5-a590235513cc}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsIBrowserGlue]),

  // redefine the default factory for XPCOMUtils
  _xpcom_factory: BrowserGlueServiceFactory,
};

/**
 * ContentPermissionIntegration is responsible for showing the user
 * simple permission prompts when content requests additional
 * capabilities.
 *
 * While there are some built-in permission prompts, createPermissionPrompt
 * can also be overridden by system add-ons or tests to provide new ones.
 *
 * This override ability is provided by Integration.jsm. See
 * PermissionUI.jsm for an example of how to provide a new prompt
 * from an add-on.
 */
const ContentPermissionIntegration = {
  /**
   * Creates a PermissionPrompt for a given permission type and
   * nsIContentPermissionRequest.
   *
   * @param {string} type
   *        The type of the permission request from content. This normally
   *        matches the "type" field of an nsIContentPermissionType, but it
   *        can be something else if the permission does not use the
   *        nsIContentPermissionRequest model. Note that this type might also
   *        be different from the permission key used in the permissions
   *        database.
   *        Example: "geolocation"
   * @param {nsIContentPermissionRequest} request
   *        The request for a permission from content.
   * @return {PermissionPrompt} (see PermissionUI.jsm),
   *         or undefined if the type cannot be handled.
   */
  createPermissionPrompt(type, request) {
    switch (type) {
      case "geolocation": {
        return new PermissionUI.GeolocationPermissionPrompt(request);
      }
      case "desktop-notification": {
        return new PermissionUI.DesktopNotificationPermissionPrompt(request);
      }
      case "persistent-storage": {
        if (Services.prefs.getBoolPref("browser.storageManager.enabled")) {
          return new PermissionUI.PersistentStoragePermissionPrompt(request);
        }
      }
    }
    return undefined;
  },
};

function ContentPermissionPrompt() {}

ContentPermissionPrompt.prototype = {
  classID:          Components.ID("{d8903bf6-68d5-4e97-bcd1-e4d3012f721a}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPermissionPrompt]),

  /**
   * This implementation of nsIContentPermissionPrompt.prompt ensures
   * that there's only one nsIContentPermissionType in the request,
   * and that it's of type nsIContentPermissionType. Failing to
   * satisfy either of these conditions will result in this method
   * throwing NS_ERRORs. If the combined ContentPermissionIntegration
   * cannot construct a prompt for this particular request, an
   * NS_ERROR_FAILURE will be thrown.
   *
   * Any time an error is thrown, the nsIContentPermissionRequest is
   * cancelled automatically.
   *
   * @param {nsIContentPermissionRequest} request
   *        The request that we're to show a prompt for.
   */
  prompt(request) {
    try {
      // Only allow exactly one permission request here.
      let types = request.types.QueryInterface(Ci.nsIArray);
      if (types.length != 1) {
        throw Components.Exception(
          "Expected an nsIContentPermissionRequest with only 1 type.",
          Cr.NS_ERROR_UNEXPECTED);
      }

      let type = types.queryElementAt(0, Ci.nsIContentPermissionType).type;
      let combinedIntegration =
        Integration.contentPermission.getCombined(ContentPermissionIntegration);

      let permissionPrompt =
        combinedIntegration.createPermissionPrompt(type, request);
      if (!permissionPrompt) {
        throw Components.Exception(
          `Failed to handle permission of type ${type}`,
          Cr.NS_ERROR_FAILURE);
      }

      permissionPrompt.prompt();
    } catch (ex) {
      Cu.reportError(ex);
      request.cancel();
      throw ex;
    }
  },
};

var DefaultBrowserCheck = {
  get OPTIONPOPUP() { return "defaultBrowserNotificationPopup"; },

  closePrompt(aNode) {
    if (this._notification) {
      this._notification.close();
    }
  },

  setAsDefault() {
    let claimAllTypes = true;
    let setAsDefaultError = false;
    if (AppConstants.platform == "win") {
      try {
        // In Windows 8+, the UI for selecting default protocol is much
        // nicer than the UI for setting file type associations. So we
        // only show the protocol association screen on Windows 8+.
        // Windows 8 is version 6.2.
        let version = Services.sysinfo.getProperty("version");
        claimAllTypes = (parseFloat(version) < 6.2);
      } catch (ex) { }
    }
    try {
      ShellService.setDefaultBrowser(claimAllTypes, false);
    } catch (ex) {
      setAsDefaultError = true;
      Cu.reportError(ex);
    }
    // Here BROWSER_IS_USER_DEFAULT and BROWSER_SET_USER_DEFAULT_ERROR appear
    // to be inverse of each other, but that is only because this function is
    // called when the browser is set as the default. During startup we record
    // the BROWSER_IS_USER_DEFAULT value without recording BROWSER_SET_USER_DEFAULT_ERROR.
    Services.telemetry.getHistogramById("BROWSER_IS_USER_DEFAULT")
                      .add(!setAsDefaultError);
    Services.telemetry.getHistogramById("BROWSER_SET_DEFAULT_ERROR")
                      .add(setAsDefaultError);
  },

  _createPopup(win, notNowStrings, neverStrings) {
    let doc = win.document;
    let popup = doc.createElement("menupopup");
    popup.id = this.OPTIONPOPUP;

    let notNowItem = doc.createElement("menuitem");
    notNowItem.id = "defaultBrowserNotNow";
    notNowItem.setAttribute("label", notNowStrings.label);
    notNowItem.setAttribute("accesskey", notNowStrings.accesskey);
    popup.appendChild(notNowItem);

    let neverItem = doc.createElement("menuitem");
    neverItem.id = "defaultBrowserNever";
    neverItem.setAttribute("label", neverStrings.label);
    neverItem.setAttribute("accesskey", neverStrings.accesskey);
    popup.appendChild(neverItem);

    popup.addEventListener("command", this);

    let popupset = doc.getElementById("mainPopupSet");
    popupset.appendChild(popup);
  },

  handleEvent(event) {
    if (event.type == "command") {
      if (event.target.id == "defaultBrowserNever") {
        ShellService.shouldCheckDefaultBrowser = false;
      }
      this.closePrompt();
    }
  },

  prompt(win) {
    let useNotificationBar = Services.prefs.getBoolPref("browser.defaultbrowser.notificationbar");

    let brandBundle = win.document.getElementById("bundle_brand");
    let brandShortName = brandBundle.getString("brandShortName");

    let shellBundle = win.document.getElementById("bundle_shell");
    let buttonPrefix = "setDefaultBrowser" + (useNotificationBar ? "" : "Alert");
    let yesButton = shellBundle.getFormattedString(buttonPrefix + "Confirm.label",
                                                   [brandShortName]);
    let notNowButton = shellBundle.getString(buttonPrefix + "NotNow.label");

    if (useNotificationBar) {
      let promptMessage = shellBundle.getFormattedString("setDefaultBrowserMessage2",
                                                         [brandShortName]);
      let optionsMessage = shellBundle.getString("setDefaultBrowserOptions.label");
      let optionsKey = shellBundle.getString("setDefaultBrowserOptions.accesskey");

      let neverLabel = shellBundle.getString("setDefaultBrowserNever.label");
      let neverKey = shellBundle.getString("setDefaultBrowserNever.accesskey");

      let yesButtonKey = shellBundle.getString("setDefaultBrowserConfirm.accesskey");
      let notNowButtonKey = shellBundle.getString("setDefaultBrowserNotNow.accesskey");

      let notificationBox = win.document.getElementById("high-priority-global-notificationbox");

      this._createPopup(win, {
        label: notNowButton,
        accesskey: notNowButtonKey
      }, {
        label: neverLabel,
        accesskey: neverKey
      });

      let buttons = [
        {
          label: yesButton,
          accessKey: yesButtonKey,
          callback: () => {
            this.setAsDefault();
            this.closePrompt();
          }
        },
        {
          label: optionsMessage,
          accessKey: optionsKey,
          popup: this.OPTIONPOPUP
        }
      ];

      let iconPixels = win.devicePixelRatio > 1 ? "32" : "16";
      let iconURL = "chrome://branding/content/icon" + iconPixels + ".png";
      const priority = notificationBox.PRIORITY_WARNING_HIGH;
      let callback = this._onNotificationEvent.bind(this);
      this._notification = notificationBox.appendNotification(promptMessage, "default-browser",
                                                              iconURL, priority, buttons,
                                                              callback);
    } else {
      // Modal prompt
      let promptTitle = shellBundle.getString("setDefaultBrowserTitle");
      let promptMessage = shellBundle.getFormattedString("setDefaultBrowserMessage",
                                                         [brandShortName]);
      let askLabel = shellBundle.getFormattedString("setDefaultBrowserDontAsk",
                                                    [brandShortName]);

      let ps = Services.prompt;
      let shouldAsk = { value: true };
      let buttonFlags = (ps.BUTTON_TITLE_IS_STRING * ps.BUTTON_POS_0) +
                        (ps.BUTTON_TITLE_IS_STRING * ps.BUTTON_POS_1) +
                        ps.BUTTON_POS_0_DEFAULT;
      let rv = ps.confirmEx(win, promptTitle, promptMessage, buttonFlags,
                            yesButton, notNowButton, null, askLabel, shouldAsk);
      if (rv == 0) {
        this.setAsDefault();
      } else if (!shouldAsk.value) {
        ShellService.shouldCheckDefaultBrowser = false;
      }

      try {
        let resultEnum = rv * 2 + shouldAsk.value;
        Services.telemetry.getHistogramById("BROWSER_SET_DEFAULT_RESULT")
                          .add(resultEnum);
      } catch (ex) { /* Don't break if Telemetry is acting up. */ }
    }
  },

  _onNotificationEvent(eventType) {
    if (eventType == "removed") {
      let doc = this._notification.ownerDocument;
      let popup = doc.getElementById(this.OPTIONPOPUP);
      popup.removeEventListener("command", this);
      popup.remove();
      delete this._notification;
    }
  },
};

/*
 * Prompts users who have an outdated JAWS screen reader informing
 * them they need to update JAWS or switch to esr. Can be removed
 * 12/31/2018.
 */
var JawsScreenReaderVersionCheck = {
  _prompted: false,

  init() {
    Services.obs.addObserver(this, "a11y-init-or-shutdown", true);
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver, Ci.nsISupportsWeakReference]),

  observe(subject, topic, data) {
    if (topic == "a11y-init-or-shutdown" && data == "1") {
      Services.tm.dispatchToMainThread(() => this._checkVersionAndPrompt());
    }
  },

  onWindowsRestored() {
    Services.tm.dispatchToMainThread(() => this._checkVersionAndPrompt());
  },

  _checkVersionAndPrompt() {
    // Make sure we only prompt for versions of JAWS we do not
    // support and never prompt if e10s is disabled.
    if (!Services.appinfo.shouldBlockIncompatJaws ||
        !Services.appinfo.browserTabsRemoteAutostart) {
      return;
    }

    let win = RecentWindow.getMostRecentBrowserWindow();
    if (!win || !win.gBrowser || !win.gBrowser.selectedBrowser) {
      Services.console.logStringMessage(
          "Content access support for older versions of JAWS is disabled " +
          "due to compatibility issues with this version of Firefox.");
      this._prompted = false;
      return;
    }

    // Only prompt once per session
    if (this._prompted) {
      return;
    }
    this._prompted = true;

    let browser = win.gBrowser.selectedBrowser;

    // Prompt JAWS users to let them know they need to update
    let promptMessage = win.gNavigatorBundle.getFormattedString(
                          "e10s.accessibilityNotice.jawsMessage",
                          [gBrandBundle.GetStringFromName("brandShortName")]
                        );
    let notification;
    // main option: an Ok button, keeps running with content accessibility disabled
    let mainAction = {
      label: win.gNavigatorBundle.getString("e10s.accessibilityNotice.acceptButton.label"),
      accessKey: win.gNavigatorBundle.getString("e10s.accessibilityNotice.acceptButton.accesskey"),
      callback() {
        // If the user invoked the button option remove the notification,
        // otherwise keep the alert icon around in the address bar.
        notification.remove();
      }
    };
    let options = {
      popupIconURL: "chrome://browser/skin/e10s-64@2x.png",
      persistWhileVisible: true,
      persistent: true,
      persistence: 100
    };

    notification =
      win.PopupNotifications.show(browser, "e10s_enabled_with_incompat_jaws",
                                  promptMessage, null, mainAction,
                                  null, options);
  },
};

var components = [BrowserGlue, ContentPermissionPrompt];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);

// Listen for UITour messages.
// Do it here instead of the UITour module itself so that the UITour module is lazy loaded
// when the first message is received.
var globalMM = Cc["@mozilla.org/globalmessagemanager;1"].getService(Ci.nsIMessageListenerManager);
globalMM.addMessageListener("UITour:onPageEvent", function(aMessage) {
  UITour.onPageEvent(aMessage, aMessage.data);
});
