/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["SessionStore"];

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

const STATE_STOPPED = 0;
const STATE_RUNNING = 1;
const STATE_QUITTING = -1;

const STATE_STOPPED_STR = "stopped";
const STATE_RUNNING_STR = "running";

const TAB_STATE_NEEDS_RESTORE = 1;
const TAB_STATE_RESTORING = 2;

const NOTIFY_WINDOWS_RESTORED = "sessionstore-windows-restored";
const NOTIFY_BROWSER_STATE_RESTORED = "sessionstore-browser-state-restored";

// Maximum number of tabs to restore simultaneously. Previously controlled by
// the browser.sessionstore.max_concurrent_tabs pref.
const MAX_CONCURRENT_TAB_RESTORES = 3;

// global notifications observed
const OBSERVING = [
  "domwindowopened", "domwindowclosed",
  "quit-application-requested", "quit-application-granted",
  "browser-lastwindow-close-granted",
  "quit-application", "browser:purge-session-history",
  "browser:purge-domain-data"
];

// XUL Window properties to (re)store
// Restored in restoreDimensions()
const WINDOW_ATTRIBUTES = ["width", "height", "screenX", "screenY", "sizemode"];

// Hideable window features to (re)store
// Restored in restoreWindowFeatures()
const WINDOW_HIDEABLE_FEATURES = [
  "menubar", "toolbar", "locationbar", "personalbar", "statusbar", "scrollbars"
];

const MESSAGES = [
  // The content script tells us that its form data (or that of one of its
  // subframes) might have changed. This can be the contents or values of
  // standard form fields or of ContentEditables.
  "SessionStore:input",

  // The content script has received a pageshow event. This happens when a
  // page is loaded from bfcache without any network activity, i.e. when
  // clicking the back or forward button.
  "SessionStore:pageshow",

  // The content script has received a MozStorageChanged event dealing
  // with a change in the contents of the sessionStorage.
  "SessionStore:MozStorageChanged",

  // The content script tells us that a new page just started loading in a
  // browser.
  "SessionStore:loadStart"
];

// These are tab events that we listen to.
const TAB_EVENTS = [
  "TabOpen", "TabClose", "TabSelect", "TabShow", "TabHide", "TabPinned",
  "TabUnpinned"
];

// The number of milliseconds in a day
const MS_PER_DAY = 1000.0 * 60.0 * 60.0 * 24.0;

#ifndef XP_WIN
#define BROKEN_WM_Z_ORDER
#endif

Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/TelemetryTimestamps.jsm", this);
Cu.import("resource://gre/modules/TelemetryStopwatch.jsm", this);
Cu.import("resource://gre/modules/osfile.jsm", this);
Cu.import("resource://gre/modules/PrivateBrowsingUtils.jsm", this);
Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);

XPCOMUtils.defineLazyServiceGetter(this, "gSessionStartup",
  "@mozilla.org/browser/sessionstartup;1", "nsISessionStartup");
XPCOMUtils.defineLazyServiceGetter(this, "gScreenManager",
  "@mozilla.org/gfx/screenmanager;1", "nsIScreenManager");

/**
 * Get nsIURI from string
 * @param string
 * @returns nsIURI
 */
function makeURI(aString) {
  return Services.io.newURI(aString, null, null);
}

XPCOMUtils.defineLazyModuleGetter(this, "ScratchpadManager",
  "resource:///modules/devtools/scratchpad-manager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DocShellCapabilities",
  "resource:///modules/sessionstore/DocShellCapabilities.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DocumentUtils",
  "resource:///modules/sessionstore/DocumentUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Messenger",
  "resource:///modules/sessionstore/Messenger.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivacyLevel",
  "resource:///modules/sessionstore/PrivacyLevel.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SessionSaver",
  "resource:///modules/sessionstore/SessionSaver.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SessionStorage",
  "resource:///modules/sessionstore/SessionStorage.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SessionCookies",
  "resource:///modules/sessionstore/SessionCookies.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SessionHistory",
  "resource:///modules/sessionstore/SessionHistory.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "_SessionFile",
  "resource:///modules/sessionstore/_SessionFile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TabStateCache",
  "resource:///modules/sessionstore/TabStateCache.jsm");

#ifdef MOZ_CRASHREPORTER
XPCOMUtils.defineLazyServiceGetter(this, "CrashReporter",
  "@mozilla.org/xre/app-info;1", "nsICrashReporter");
#endif

/**
 * |true| if we are in debug mode, |false| otherwise.
 * Debug mode is controlled by preference browser.sessionstore.debug
 */
let gDebuggingEnabled = false;
function debug(aMsg) {
  if (gDebuggingEnabled) {
    aMsg = ("SessionStore: " + aMsg).replace(/\S{80}/g, "$&\n");
    Services.console.logStringMessage(aMsg);
  }
}

this.SessionStore = {
  get promiseInitialized() {
    return SessionStoreInternal.promiseInitialized;
  },

  get canRestoreLastSession() {
    return SessionStoreInternal.canRestoreLastSession;
  },

  set canRestoreLastSession(val) {
    SessionStoreInternal.canRestoreLastSession = val;
  },

  init: function ss_init(aWindow) {
    SessionStoreInternal.init(aWindow);
  },

  getBrowserState: function ss_getBrowserState() {
    return SessionStoreInternal.getBrowserState();
  },

  setBrowserState: function ss_setBrowserState(aState) {
    SessionStoreInternal.setBrowserState(aState);
  },

  getWindowState: function ss_getWindowState(aWindow) {
    return SessionStoreInternal.getWindowState(aWindow);
  },

  setWindowState: function ss_setWindowState(aWindow, aState, aOverwrite) {
    SessionStoreInternal.setWindowState(aWindow, aState, aOverwrite);
  },

  getTabState: function ss_getTabState(aTab) {
    return SessionStoreInternal.getTabState(aTab);
  },

  setTabState: function ss_setTabState(aTab, aState) {
    SessionStoreInternal.setTabState(aTab, aState);
  },

  duplicateTab: function ss_duplicateTab(aWindow, aTab, aDelta = 0) {
    return SessionStoreInternal.duplicateTab(aWindow, aTab, aDelta);
  },

  getNumberOfTabsClosedLast: function ss_getNumberOfTabsClosedLast(aWindow) {
    return SessionStoreInternal.getNumberOfTabsClosedLast(aWindow);
  },

  setNumberOfTabsClosedLast: function ss_setNumberOfTabsClosedLast(aWindow, aNumber) {
    return SessionStoreInternal.setNumberOfTabsClosedLast(aWindow, aNumber);
  },

  getClosedTabCount: function ss_getClosedTabCount(aWindow) {
    return SessionStoreInternal.getClosedTabCount(aWindow);
  },

  getClosedTabData: function ss_getClosedTabDataAt(aWindow) {
    return SessionStoreInternal.getClosedTabData(aWindow);
  },

  undoCloseTab: function ss_undoCloseTab(aWindow, aIndex) {
    return SessionStoreInternal.undoCloseTab(aWindow, aIndex);
  },

  forgetClosedTab: function ss_forgetClosedTab(aWindow, aIndex) {
    return SessionStoreInternal.forgetClosedTab(aWindow, aIndex);
  },

  getClosedWindowCount: function ss_getClosedWindowCount() {
    return SessionStoreInternal.getClosedWindowCount();
  },

  getClosedWindowData: function ss_getClosedWindowData() {
    return SessionStoreInternal.getClosedWindowData();
  },

  undoCloseWindow: function ss_undoCloseWindow(aIndex) {
    return SessionStoreInternal.undoCloseWindow(aIndex);
  },

  forgetClosedWindow: function ss_forgetClosedWindow(aIndex) {
    return SessionStoreInternal.forgetClosedWindow(aIndex);
  },

  getWindowValue: function ss_getWindowValue(aWindow, aKey) {
    return SessionStoreInternal.getWindowValue(aWindow, aKey);
  },

  setWindowValue: function ss_setWindowValue(aWindow, aKey, aStringValue) {
    SessionStoreInternal.setWindowValue(aWindow, aKey, aStringValue);
  },

  deleteWindowValue: function ss_deleteWindowValue(aWindow, aKey) {
    SessionStoreInternal.deleteWindowValue(aWindow, aKey);
  },

  getTabValue: function ss_getTabValue(aTab, aKey) {
    return SessionStoreInternal.getTabValue(aTab, aKey);
  },

  setTabValue: function ss_setTabValue(aTab, aKey, aStringValue) {
    SessionStoreInternal.setTabValue(aTab, aKey, aStringValue);
  },

  deleteTabValue: function ss_deleteTabValue(aTab, aKey) {
    SessionStoreInternal.deleteTabValue(aTab, aKey);
  },

  persistTabAttribute: function ss_persistTabAttribute(aName) {
    SessionStoreInternal.persistTabAttribute(aName);
  },

  restoreLastSession: function ss_restoreLastSession() {
    SessionStoreInternal.restoreLastSession();
  },

  getCurrentState: function (aUpdateAll) {
    return SessionStoreInternal.getCurrentState(aUpdateAll);
  },

  fillTabCachesAsynchronously: function () {
    return SessionStoreInternal.fillTabCachesAsynchronously();
  },

  /**
   * Backstage pass to implementation details, used for testing purpose.
   * Controlled by preference "browser.sessionstore.testmode".
   */
  get _internal() {
    if (Services.prefs.getBoolPref("browser.sessionstore.debug")) {
      return SessionStoreInternal;
    }
    return undefined;
  },
};

// Freeze the SessionStore object. We don't want anyone to modify it.
Object.freeze(SessionStore);

let SessionStoreInternal = {
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIDOMEventListener,
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference
  ]),

  // set default load state
  _loadState: STATE_STOPPED,

  // During the initial restore and setBrowserState calls tracks the number of
  // windows yet to be restored
  _restoreCount: -1,

  // whether a setBrowserState call is in progress
  _browserSetState: false,

  // time in milliseconds when the session was started (saved across sessions),
  // defaults to now if no session was restored or timestamp doesn't exist
  _sessionStartTime: Date.now(),

  // states for all currently opened windows
  _windows: {},

  // states for all recently closed windows
  _closedWindows: [],

  // collection of session states yet to be restored
  _statesToRestore: {},

  // counts the number of crashes since the last clean start
  _recentCrashes: 0,

  // whether the last window was closed and should be restored
  _restoreLastWindow: false,

  // number of tabs currently restoring
  _tabsRestoringCount: 0,

  // The state from the previous session (after restoring pinned tabs). This
  // state is persisted and passed through to the next session during an app
  // restart to make the third party add-on warning not trash the deferred
  // session
  _lastSessionState: null,

  // When starting Firefox with a single private window, this is the place
  // where we keep the session we actually wanted to restore in case the user
  // decides to later open a non-private window as well.
  _deferredInitialState: null,

  // A promise resolved once initialization is complete
  _deferredInitialized: Promise.defer(),

  // Whether session has been initialized
  _sessionInitialized: false,

  // True if session store is disabled by multi-process browsing.
  // See bug 516755.
  _disabledForMultiProcess: false,

  /**
   * A promise fulfilled once initialization is complete.
   */
  get promiseInitialized() {
    return this._deferredInitialized.promise;
  },

  /* ........ Public Getters .............. */
  get canRestoreLastSession() {
    return this._lastSessionState;
  },

  set canRestoreLastSession(val) {
    // Cheat a bit; only allow false.
    if (val)
      return;
    this._lastSessionState = null;
  },

  /**
   * Initialize the sessionstore service.
   */
  init: function (aWindow) {
    if (this._initialized) {
      throw new Error("SessionStore.init() must only be called once!");
    }

    if (!aWindow) {
      throw new Error("SessionStore.init() must be called with a valid window.");
    }

    this._disabledForMultiProcess = Services.prefs.getBoolPref("browser.tabs.remote");
    if (this._disabledForMultiProcess) {
      this._deferredInitialized.resolve();
      return;
    }

    TelemetryTimestamps.add("sessionRestoreInitialized");
    OBSERVING.forEach(function(aTopic) {
      Services.obs.addObserver(this, aTopic, true);
    }, this);

    this._initPrefs();
    this._initialized = true;

    // Wait until nsISessionStartup has finished reading the session data.
    gSessionStartup.onceInitialized.then(() => {
      // Parse session data and start restoring.
      let initialState = this.initSession();

      // Start tracking the given (initial) browser window.
      if (!aWindow.closed) {
        this.onLoad(aWindow, initialState);
      }

      // Let everyone know we're done.
      this._deferredInitialized.resolve();
    }, Cu.reportError);
  },

  initSession: function ssi_initSession() {
    let state;
    let ss = gSessionStartup;

    try {
      if (ss.doRestore() ||
          ss.sessionType == Ci.nsISessionStartup.DEFER_SESSION)
        state = ss.state;
    }
    catch(ex) { dump(ex + "\n"); } // no state to restore, which is ok

    if (state) {
      try {
        // If we're doing a DEFERRED session, then we want to pull pinned tabs
        // out so they can be restored.
        if (ss.sessionType == Ci.nsISessionStartup.DEFER_SESSION) {
          let [iniState, remainingState] = this._prepDataForDeferredRestore(state);
          // If we have a iniState with windows, that means that we have windows
          // with app tabs to restore.
          if (iniState.windows.length)
            state = iniState;
          else
            state = null;
          if (remainingState.windows.length)
            this._lastSessionState = remainingState;
        }
        else {
          // Get the last deferred session in case the user still wants to
          // restore it
          this._lastSessionState = state.lastSessionState;

          let lastSessionCrashed =
            state.session && state.session.state &&
            state.session.state == STATE_RUNNING_STR;
          if (lastSessionCrashed) {
            this._recentCrashes = (state.session &&
                                   state.session.recentCrashes || 0) + 1;

            if (this._needsRestorePage(state, this._recentCrashes)) {
              // replace the crashed session with a restore-page-only session
              let pageData = {
                url: "about:sessionrestore",
                formdata: {
                  id: { "sessionData": state },
                  xpath: {}
                }
              };
              state = { windows: [{ tabs: [{ entries: [pageData] }] }] };
            } else if (this._hasSingleTabWithURL(state.windows,
                                                 "about:welcomeback")) {
              // On a single about:welcomeback URL that crashed, replace about:welcomeback
              // with about:sessionrestore, to make clear to the user that we crashed.
              state.windows[0].tabs[0].entries[0].url = "about:sessionrestore";
            }
          }

          // Update the session start time using the restored session state.
          this._updateSessionStartTime(state);

          // make sure that at least the first window doesn't have anything hidden
          delete state.windows[0].hidden;
          // Since nothing is hidden in the first window, it cannot be a popup
          delete state.windows[0].isPopup;
          // We don't want to minimize and then open a window at startup.
          if (state.windows[0].sizemode == "minimized")
            state.windows[0].sizemode = "normal";
          // clear any lastSessionWindowID attributes since those don't matter
          // during normal restore
          state.windows.forEach(function(aWindow) {
            delete aWindow.__lastSessionWindowID;
          });
        }
      }
      catch (ex) { debug("The session file is invalid: " + ex); }
    }

    // at this point, we've as good as resumed the session, so we can
    // clear the resume_session_once flag, if it's set
    if (this._loadState != STATE_QUITTING &&
        this._prefBranch.getBoolPref("sessionstore.resume_session_once"))
      this._prefBranch.setBoolPref("sessionstore.resume_session_once", false);

    this._performUpgradeBackup();
    this._sessionInitialized = true;

    return state;
  },

  /**
   * If this is the first time we launc this build of Firefox,
   * backup sessionstore.js.
   */
  _performUpgradeBackup: function ssi_performUpgradeBackup() {
    // Perform upgrade backup, if necessary
    const PREF_UPGRADE = "sessionstore.upgradeBackup.latestBuildID";

    let buildID = Services.appinfo.platformBuildID;
    let latestBackup = this._prefBranch.getCharPref(PREF_UPGRADE);
    if (latestBackup == buildID) {
      return Promise.resolve();
    }
    return Task.spawn(function task() {
      try {
        // Perform background backup
        yield _SessionFile.createBackupCopy("-" + buildID);

        this._prefBranch.setCharPref(PREF_UPGRADE, buildID);

        // In case of success, remove previous backup.
        yield _SessionFile.removeBackupCopy("-" + latestBackup);
      } catch (ex) {
        debug("Could not perform upgrade backup " + ex);
        debug(ex.stack);
      }
    }.bind(this));
  },

  _initPrefs : function() {
    this._prefBranch = Services.prefs.getBranch("browser.");

    gDebuggingEnabled = this._prefBranch.getBoolPref("sessionstore.debug");

    Services.prefs.addObserver("browser.sessionstore.debug", () => {
      gDebuggingEnabled = this._prefBranch.getBoolPref("sessionstore.debug");
    }, false);

    XPCOMUtils.defineLazyGetter(this, "_max_tabs_undo", function () {
      this._prefBranch.addObserver("sessionstore.max_tabs_undo", this, true);
      return this._prefBranch.getIntPref("sessionstore.max_tabs_undo");
    });

    XPCOMUtils.defineLazyGetter(this, "_max_windows_undo", function () {
      this._prefBranch.addObserver("sessionstore.max_windows_undo", this, true);
      return this._prefBranch.getIntPref("sessionstore.max_windows_undo");
    });
  },

  /**
   * Called on application shutdown, after notifications:
   * quit-application-granted, quit-application
   */
  _uninit: function ssi_uninit() {
    if (!this._initialized) {
      throw new Error("SessionStore is not initialized.");
    }

    // save all data for session resuming
    if (this._sessionInitialized) {
      SessionSaver.run();
    }

    // clear out priority queue in case it's still holding refs
    TabRestoreQueue.reset();

    // Make sure to cancel pending saves.
    SessionSaver.cancel();
  },

  /**
   * Handle notifications
   */
  observe: function ssi_observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "domwindowopened": // catch new windows
        this.onOpen(aSubject);
        break;
      case "domwindowclosed": // catch closed windows
        this.onClose(aSubject);
        break;
      case "quit-application-requested":
        this.onQuitApplicationRequested();
        break;
      case "quit-application-granted":
        this.onQuitApplicationGranted();
        break;
      case "browser-lastwindow-close-granted":
        this.onLastWindowCloseGranted();
        break;
      case "quit-application":
        this.onQuitApplication(aData);
        break;
      case "browser:purge-session-history": // catch sanitization
        this.onPurgeSessionHistory();
        break;
      case "browser:purge-domain-data":
        this.onPurgeDomainData(aData);
        break;
      case "nsPref:changed": // catch pref changes
        this.onPrefChange(aData);
        break;
      case "timer-callback": // timer call back for delayed saving
        this.onTimerCallback();
        break;
    }
  },

  /**
   * This method handles incoming messages sent by the session store content
   * script and thus enables communication with OOP tabs.
   */
  receiveMessage: function ssi_receiveMessage(aMessage) {
    var browser = aMessage.target;
    var win = browser.ownerDocument.defaultView;

    switch (aMessage.name) {
      case "SessionStore:pageshow":
        this.onTabLoad(win, browser);
        break;
      case "SessionStore:input":
        this.onTabInput(win, browser);
        break;
      case "SessionStore:MozStorageChanged":
        TabStateCache.delete(browser);
        this.saveStateDelayed(win);
        break;
      case "SessionStore:loadStart":
        TabStateCache.delete(browser);
        break;
      default:
        debug("received unknown message '" + aMessage.name + "'");
        break;
    }

    this._clearRestoringWindows();
  },

  /* ........ Window Event Handlers .............. */

  /**
   * Implement nsIDOMEventListener for handling various window and tab events
   */
  handleEvent: function ssi_handleEvent(aEvent) {
    if (this._disabledForMultiProcess)
      return;

    var win = aEvent.currentTarget.ownerDocument.defaultView;
    switch (aEvent.type) {
      case "load":
        // If __SS_restore_data is set, then we need to restore the document
        // (form data, scrolling, etc.). This will only happen when a tab is
        // first restored.
        let browser = aEvent.currentTarget;
        TabStateCache.delete(browser);
        if (browser.__SS_restore_data)
          this.restoreDocument(win, browser, aEvent);
        this.onTabLoad(win, browser);
        break;
      case "TabOpen":
        this.onTabAdd(win, aEvent.originalTarget);
        break;
      case "TabClose":
        // aEvent.detail determines if the tab was closed by moving to a different window
        if (!aEvent.detail)
          this.onTabClose(win, aEvent.originalTarget);
        this.onTabRemove(win, aEvent.originalTarget);
        break;
      case "TabSelect":
        this.onTabSelect(win);
        break;
      case "TabShow":
        this.onTabShow(win, aEvent.originalTarget);
        break;
      case "TabHide":
        this.onTabHide(win, aEvent.originalTarget);
        break;
      case "TabPinned":
        // If possible, update cached data without having to invalidate it
        TabStateCache.updateField(aEvent.originalTarget, "pinned", true);
        this.saveStateDelayed(win);
        break;
      case "TabUnpinned":
        // If possible, update cached data without having to invalidate it
        TabStateCache.updateField(aEvent.originalTarget, "pinned", false);
        this.saveStateDelayed(win);
        break;
    }
    this._clearRestoringWindows();
  },

  /**
   * If it's the first window load since app start...
   * - determine if we're reloading after a crash or a forced-restart
   * - restore window state
   * - restart downloads
   * Set up event listeners for this window's tabs
   * @param aWindow
   *        Window reference
   * @param aInitialState
   *        The initial state to be loaded after startup (optional)
   */
  onLoad: function ssi_onLoad(aWindow, aInitialState = null) {
    // return if window has already been initialized
    if (aWindow && aWindow.__SSi && this._windows[aWindow.__SSi])
      return;

    // ignore non-browser windows and windows opened while shutting down
    if (aWindow.document.documentElement.getAttribute("windowtype") != "navigator:browser" ||
        this._loadState == STATE_QUITTING)
      return;

    // assign it a unique identifier (timestamp)
    aWindow.__SSi = "window" + Date.now();

    // and create its data object
    this._windows[aWindow.__SSi] = { tabs: [], selected: 0, _closedTabs: [], busy: false };

    let isPrivateWindow = false;
    if (PrivateBrowsingUtils.isWindowPrivate(aWindow))
      this._windows[aWindow.__SSi].isPrivate = isPrivateWindow = true;
    if (!this._isWindowLoaded(aWindow))
      this._windows[aWindow.__SSi]._restoring = true;
    if (!aWindow.toolbar.visible)
      this._windows[aWindow.__SSi].isPopup = true;

    // perform additional initialization when the first window is loading
    if (this._loadState == STATE_STOPPED) {
      this._loadState = STATE_RUNNING;
      SessionSaver.updateLastSaveTime();

      // restore a crashed session resp. resume the last session if requested
      if (aInitialState) {
        if (isPrivateWindow) {
          // We're starting with a single private window. Save the state we
          // actually wanted to restore so that we can do it later in case
          // the user opens another, non-private window.
          this._deferredInitialState = aInitialState;

          // Nothing to restore now, notify observers things are complete.
          Services.obs.notifyObservers(null, NOTIFY_WINDOWS_RESTORED, "");
        } else {
          TelemetryTimestamps.add("sessionRestoreRestoring");
          this._restoreCount = aInitialState.windows ? aInitialState.windows.length : 0;

          let overwrite = this._isCmdLineEmpty(aWindow, aInitialState);
          let options = {firstWindow: true, overwriteTabs: overwrite};
          this.restoreWindow(aWindow, aInitialState, options);

          // _loadState changed from "stopped" to "running". Save the session's
          // load state immediately so that crashes happening during startup
          // are correctly counted.
          _SessionFile.writeLoadStateOnceAfterStartup(STATE_RUNNING_STR);
        }
      }
      else {
        // Nothing to restore, notify observers things are complete.
        Services.obs.notifyObservers(null, NOTIFY_WINDOWS_RESTORED, "");

        // The next delayed save request should execute immediately.
        SessionSaver.clearLastSaveTime();
      }
    }
    // this window was opened by _openWindowWithState
    else if (!this._isWindowLoaded(aWindow)) {
      let state = this._statesToRestore[aWindow.__SS_restoreID];
      let options = {overwriteTabs: true, isFollowUp: state.windows.length == 1};
      this.restoreWindow(aWindow, state, options);
    }
    // The user opened another, non-private window after starting up with
    // a single private one. Let's restore the session we actually wanted to
    // restore at startup.
    else if (this._deferredInitialState && !isPrivateWindow &&
             aWindow.toolbar.visible) {

      this._restoreCount = this._deferredInitialState.windows ?
        this._deferredInitialState.windows.length : 0;
      this.restoreWindow(aWindow, this._deferredInitialState, {firstWindow: true});
      this._deferredInitialState = null;
    }
    else if (this._restoreLastWindow && aWindow.toolbar.visible &&
             this._closedWindows.length && !isPrivateWindow) {

      // default to the most-recently closed window
      // don't use popup windows
      let closedWindowState = null;
      let closedWindowIndex;
      for (let i = 0; i < this._closedWindows.length; i++) {
        // Take the first non-popup, point our object at it, and break out.
        if (!this._closedWindows[i].isPopup) {
          closedWindowState = this._closedWindows[i];
          closedWindowIndex = i;
          break;
        }
      }

      if (closedWindowState) {
        let newWindowState;
#ifndef XP_MACOSX
        if (!this._doResumeSession()) {
#endif
          // We want to split the window up into pinned tabs and unpinned tabs.
          // Pinned tabs should be restored. If there are any remaining tabs,
          // they should be added back to _closedWindows.
          // We'll cheat a little bit and reuse _prepDataForDeferredRestore
          // even though it wasn't built exactly for this.
          let [appTabsState, normalTabsState] =
            this._prepDataForDeferredRestore({ windows: [closedWindowState] });

          // These are our pinned tabs, which we should restore
          if (appTabsState.windows.length) {
            newWindowState = appTabsState.windows[0];
            delete newWindowState.__lastSessionWindowID;
          }

          // In case there were no unpinned tabs, remove the window from _closedWindows
          if (!normalTabsState.windows.length) {
            this._closedWindows.splice(closedWindowIndex, 1);
          }
          // Or update _closedWindows with the modified state
          else {
            delete normalTabsState.windows[0].__lastSessionWindowID;
            this._closedWindows[closedWindowIndex] = normalTabsState.windows[0];
          }
#ifndef XP_MACOSX
        }
        else {
          // If we're just restoring the window, make sure it gets removed from
          // _closedWindows.
          this._closedWindows.splice(closedWindowIndex, 1);
          newWindowState = closedWindowState;
          delete newWindowState.hidden;
        }
#endif
        if (newWindowState) {
          // Ensure that the window state isn't hidden
          this._restoreCount = 1;
          let state = { windows: [newWindowState] };
          let options = {overwriteTabs: this._isCmdLineEmpty(aWindow, state)};
          this.restoreWindow(aWindow, state, options);
        }
      }
      // we actually restored the session just now.
      this._prefBranch.setBoolPref("sessionstore.resume_session_once", false);
    }
    if (this._restoreLastWindow && aWindow.toolbar.visible) {
      // always reset (if not a popup window)
      // we don't want to restore a window directly after, for example,
      // undoCloseWindow was executed.
      this._restoreLastWindow = false;
    }

    var tabbrowser = aWindow.gBrowser;

    // add tab change listeners to all already existing tabs
    for (let i = 0; i < tabbrowser.tabs.length; i++) {
      this.onTabAdd(aWindow, tabbrowser.tabs[i], true);
    }
    // notification of tab add/remove/selection/show/hide
    TAB_EVENTS.forEach(function(aEvent) {
      tabbrowser.tabContainer.addEventListener(aEvent, this, true);
    }, this);
  },

  /**
   * On window open
   * @param aWindow
   *        Window reference
   */
  onOpen: function ssi_onOpen(aWindow) {
    let onload = () => {
      aWindow.removeEventListener("load", onload);
      this.onLoad(aWindow);
    };

    aWindow.addEventListener("load", onload);
  },

  /**
   * On window close...
   * - remove event listeners from tabs
   * - save all window data
   * @param aWindow
   *        Window reference
   */
  onClose: function ssi_onClose(aWindow) {
    // this window was about to be restored - conserve its original data, if any
    let isFullyLoaded = this._isWindowLoaded(aWindow);
    if (!isFullyLoaded) {
      if (!aWindow.__SSi)
        aWindow.__SSi = "window" + Date.now();
      this._windows[aWindow.__SSi] = this._statesToRestore[aWindow.__SS_restoreID];
      delete this._statesToRestore[aWindow.__SS_restoreID];
      delete aWindow.__SS_restoreID;
    }

    // ignore windows not tracked by SessionStore
    if (!aWindow.__SSi || !this._windows[aWindow.__SSi]) {
      return;
    }

    // notify that the session store will stop tracking this window so that
    // extensions can store any data about this window in session store before
    // that's not possible anymore
    let event = aWindow.document.createEvent("Events");
    event.initEvent("SSWindowClosing", true, false);
    aWindow.dispatchEvent(event);

    if (this.windowToFocus && this.windowToFocus == aWindow) {
      delete this.windowToFocus;
    }

    var tabbrowser = aWindow.gBrowser;

    TAB_EVENTS.forEach(function(aEvent) {
      tabbrowser.tabContainer.removeEventListener(aEvent, this, true);
    }, this);

    // remove the progress listener for this window
    tabbrowser.removeTabsProgressListener(gRestoreTabsProgressListener);

    let winData = this._windows[aWindow.__SSi];
    if (this._loadState == STATE_RUNNING) { // window not closed during a regular shut-down
      // update all window data for a last time
      this._collectWindowData(aWindow);

      if (isFullyLoaded) {
        winData.title = aWindow.content.document.title || tabbrowser.selectedTab.label;
        winData.title = this._replaceLoadingTitle(winData.title, tabbrowser,
                                                  tabbrowser.selectedTab);
        SessionCookies.update([winData]);
      }

#ifndef XP_MACOSX
      // Until we decide otherwise elsewhere, this window is part of a series
      // of closing windows to quit.
      winData._shouldRestore = true;
#endif

      // Save the window if it has multiple tabs or a single saveable tab and
      // it's not private.
      if (!winData.isPrivate && (winData.tabs.length > 1 ||
          (winData.tabs.length == 1 && this._shouldSaveTabState(winData.tabs[0])))) {
        // we don't want to save the busy state
        delete winData.busy;

        this._closedWindows.unshift(winData);
        this._capClosedWindows();
      }

      // clear this window from the list
      delete this._windows[aWindow.__SSi];

      // save the state without this window to disk
      this.saveStateDelayed();
    }

    for (let i = 0; i < tabbrowser.tabs.length; i++) {
      this.onTabRemove(aWindow, tabbrowser.tabs[i], true);
    }

    // Cache the window state until it is completely gone.
    DyingWindowCache.set(aWindow, winData);

    delete aWindow.__SSi;
  },

  /**
   * On quit application requested
   */
  onQuitApplicationRequested: function ssi_onQuitApplicationRequested() {
    // get a current snapshot of all windows
    this._forEachBrowserWindow(function(aWindow) {
      this._collectWindowData(aWindow);
    });
    // we must cache this because _getMostRecentBrowserWindow will always
    // return null by the time quit-application occurs
    var activeWindow = this._getMostRecentBrowserWindow();
    if (activeWindow)
      this.activeWindowSSiCache = activeWindow.__SSi || "";
    DirtyWindows.clear();
  },

  /**
   * On quit application granted
   */
  onQuitApplicationGranted: function ssi_onQuitApplicationGranted() {
    // freeze the data at what we've got (ignoring closing windows)
    this._loadState = STATE_QUITTING;
  },

  /**
   * On last browser window close
   */
  onLastWindowCloseGranted: function ssi_onLastWindowCloseGranted() {
    // last browser window is quitting.
    // remember to restore the last window when another browser window is opened
    // do not account for pref(resume_session_once) at this point, as it might be
    // set by another observer getting this notice after us
    this._restoreLastWindow = true;
  },

  /**
   * On quitting application
   * @param aData
   *        String type of quitting
   */
  onQuitApplication: function ssi_onQuitApplication(aData) {
    if (aData == "restart") {
      this._prefBranch.setBoolPref("sessionstore.resume_session_once", true);
      // The browser:purge-session-history notification fires after the
      // quit-application notification so unregister the
      // browser:purge-session-history notification to prevent clearing
      // session data on disk on a restart.  It is also unnecessary to
      // perform any other sanitization processing on a restart as the
      // browser is about to exit anyway.
      Services.obs.removeObserver(this, "browser:purge-session-history");
    }

    if (aData != "restart") {
      // Throw away the previous session on shutdown
      this._lastSessionState = null;
    }

    this._loadState = STATE_QUITTING; // just to be sure
    this._uninit();
  },

  /**
   * On purge of session history
   */
  onPurgeSessionHistory: function ssi_onPurgeSessionHistory() {
    _SessionFile.wipe();
    // If the browser is shutting down, simply return after clearing the
    // session data on disk as this notification fires after the
    // quit-application notification so the browser is about to exit.
    if (this._loadState == STATE_QUITTING)
      return;
    this._lastSessionState = null;
    let openWindows = {};
    this._forEachBrowserWindow(function(aWindow) {
      Array.forEach(aWindow.gBrowser.tabs, function(aTab) {
        TabStateCache.delete(aTab);
        delete aTab.linkedBrowser.__SS_data;
        delete aTab.linkedBrowser.__SS_tabStillLoading;
        if (aTab.linkedBrowser.__SS_restoreState)
          this._resetTabRestoringState(aTab);
      }, this);
      openWindows[aWindow.__SSi] = true;
    });
    // also clear all data about closed tabs and windows
    for (let ix in this._windows) {
      if (ix in openWindows) {
        this._windows[ix]._closedTabs = [];
      } else {
        delete this._windows[ix];
      }
    }
    // also clear all data about closed windows
    this._closedWindows = [];
    // give the tabbrowsers a chance to clear their histories first
    var win = this._getMostRecentBrowserWindow();
    if (win) {
      win.setTimeout(() => SessionSaver.run(), 0);
    } else if (this._loadState == STATE_RUNNING) {
      SessionSaver.run();
    }

    this._clearRestoringWindows();
  },

  /**
   * On purge of domain data
   * @param aData
   *        String domain data
   */
  onPurgeDomainData: function ssi_onPurgeDomainData(aData) {
    // does a session history entry contain a url for the given domain?
    function containsDomain(aEntry) {
      try {
        if (makeURI(aEntry.url).host.hasRootDomain(aData)) {
          return true;
        }
      }
      catch (ex) { /* url had no host at all */ }
      return aEntry.children && aEntry.children.some(containsDomain, this);
    }
    // remove all closed tabs containing a reference to the given domain
    for (let ix in this._windows) {
      let closedTabs = this._windows[ix]._closedTabs;
      for (let i = closedTabs.length - 1; i >= 0; i--) {
        if (closedTabs[i].state.entries.some(containsDomain, this))
          closedTabs.splice(i, 1);
      }
    }
    // remove all open & closed tabs containing a reference to the given
    // domain in closed windows
    for (let ix = this._closedWindows.length - 1; ix >= 0; ix--) {
      let closedTabs = this._closedWindows[ix]._closedTabs;
      let openTabs = this._closedWindows[ix].tabs;
      let openTabCount = openTabs.length;
      for (let i = closedTabs.length - 1; i >= 0; i--)
        if (closedTabs[i].state.entries.some(containsDomain, this))
          closedTabs.splice(i, 1);
      for (let j = openTabs.length - 1; j >= 0; j--) {
        if (openTabs[j].entries.some(containsDomain, this)) {
          openTabs.splice(j, 1);
          if (this._closedWindows[ix].selected > j)
            this._closedWindows[ix].selected--;
        }
      }
      if (openTabs.length == 0) {
        this._closedWindows.splice(ix, 1);
      }
      else if (openTabs.length != openTabCount) {
        // Adjust the window's title if we removed an open tab
        let selectedTab = openTabs[this._closedWindows[ix].selected - 1];
        // some duplication from restoreHistory - make sure we get the correct title
        let activeIndex = (selectedTab.index || selectedTab.entries.length) - 1;
        if (activeIndex >= selectedTab.entries.length)
          activeIndex = selectedTab.entries.length - 1;
        this._closedWindows[ix].title = selectedTab.entries[activeIndex].title;
      }
    }

    if (this._loadState == STATE_RUNNING) {
      SessionSaver.run();
    }

    this._clearRestoringWindows();
  },

  /**
   * On preference change
   * @param aData
   *        String preference changed
   */
  onPrefChange: function ssi_onPrefChange(aData) {
    switch (aData) {
      // if the user decreases the max number of closed tabs they want
      // preserved update our internal states to match that max
      case "sessionstore.max_tabs_undo":
        this._max_tabs_undo = this._prefBranch.getIntPref("sessionstore.max_tabs_undo");
        for (let ix in this._windows) {
          this._windows[ix]._closedTabs.splice(this._max_tabs_undo, this._windows[ix]._closedTabs.length);
        }
        break;
      case "sessionstore.max_windows_undo":
        this._max_windows_undo = this._prefBranch.getIntPref("sessionstore.max_windows_undo");
        this._capClosedWindows();
        break;
    }
  },

  /**
   * set up listeners for a new tab
   * @param aWindow
   *        Window reference
   * @param aTab
   *        Tab reference
   * @param aNoNotification
   *        bool Do not save state if we're updating an existing tab
   */
  onTabAdd: function ssi_onTabAdd(aWindow, aTab, aNoNotification) {
    let browser = aTab.linkedBrowser;
    browser.addEventListener("load", this, true);

    let mm = browser.messageManager;
    MESSAGES.forEach(msg => mm.addMessageListener(msg, this));

    if (!aNoNotification) {
      this.saveStateDelayed(aWindow);
    }

    this._updateCrashReportURL(aWindow);
  },

  /**
   * remove listeners for a tab
   * @param aWindow
   *        Window reference
   * @param aTab
   *        Tab reference
   * @param aNoNotification
   *        bool Do not save state if we're updating an existing tab
   */
  onTabRemove: function ssi_onTabRemove(aWindow, aTab, aNoNotification) {
    let browser = aTab.linkedBrowser;
    browser.removeEventListener("load", this, true);

    let mm = browser.messageManager;
    MESSAGES.forEach(msg => mm.removeMessageListener(msg, this));

    delete browser.__SS_data;
    delete browser.__SS_tabStillLoading;

    // If this tab was in the middle of restoring or still needs to be restored,
    // we need to reset that state. If the tab was restoring, we will attempt to
    // restore the next tab.
    let previousState = browser.__SS_restoreState;
    if (previousState) {
      this._resetTabRestoringState(aTab);
      if (previousState == TAB_STATE_RESTORING)
        this.restoreNextTab();
    }

    if (!aNoNotification) {
      this.saveStateDelayed(aWindow);
    }
  },

  /**
   * When a tab closes, collect its properties
   * @param aWindow
   *        Window reference
   * @param aTab
   *        Tab reference
   */
  onTabClose: function ssi_onTabClose(aWindow, aTab) {
    // notify the tabbrowser that the tab state will be retrieved for the last time
    // (so that extension authors can easily set data on soon-to-be-closed tabs)
    var event = aWindow.document.createEvent("Events");
    event.initEvent("SSTabClosing", true, false);
    aTab.dispatchEvent(event);

    // don't update our internal state if we don't have to
    if (this._max_tabs_undo == 0) {
      return;
    }

    // Get the latest data for this tab (generally, from the cache)
    let tabState = TabState.collectSync(aTab);

    // store closed-tab data for undo
    if (this._shouldSaveTabState(tabState)) {
      let tabTitle = aTab.label;
      let tabbrowser = aWindow.gBrowser;
      tabTitle = this._replaceLoadingTitle(tabTitle, tabbrowser, aTab);

      this._windows[aWindow.__SSi]._closedTabs.unshift({
        state: tabState,
        title: tabTitle,
        image: tabbrowser.getIcon(aTab),
        pos: aTab._tPos
      });
      var length = this._windows[aWindow.__SSi]._closedTabs.length;
      if (length > this._max_tabs_undo)
        this._windows[aWindow.__SSi]._closedTabs.splice(this._max_tabs_undo, length - this._max_tabs_undo);
    }
  },

  /**
   * When a tab loads, invalidate its cached state, trigger async save.
   *
   * @param aWindow
   *        Window reference
   * @param aBrowser
   *        Browser reference
   */
  onTabLoad: function ssi_onTabLoad(aWindow, aBrowser) {
    // react on "load" and solitary "pageshow" events (the first "pageshow"
    // following "load" is too late for deleting the data caches)
    // It's possible to get a load event after calling stop on a browser (when
    // overwriting tabs). We want to return early if the tab hasn't been restored yet.
    if (aBrowser.__SS_restoreState &&
        aBrowser.__SS_restoreState == TAB_STATE_NEEDS_RESTORE) {
      return;
    }

    TabStateCache.delete(aBrowser);

    delete aBrowser.__SS_data;
    delete aBrowser.__SS_tabStillLoading;
    this.saveStateDelayed(aWindow);

    // attempt to update the current URL we send in a crash report
    this._updateCrashReportURL(aWindow);
  },

  /**
   * Called when a browser sends the "input" notification
   * @param aWindow
   *        Window reference
   * @param aBrowser
   *        Browser reference
   */
  onTabInput: function ssi_onTabInput(aWindow, aBrowser) {
    TabStateCache.delete(aBrowser);
    this.saveStateDelayed(aWindow);
  },

  /**
   * When a tab is selected, save session data
   * @param aWindow
   *        Window reference
   */
  onTabSelect: function ssi_onTabSelect(aWindow) {
    if (this._loadState == STATE_RUNNING) {
      this._windows[aWindow.__SSi].selected = aWindow.gBrowser.tabContainer.selectedIndex;

      let tab = aWindow.gBrowser.selectedTab;
      // If __SS_restoreState is still on the browser and it is
      // TAB_STATE_NEEDS_RESTORE, then then we haven't restored
      // this tab yet. Explicitly call restoreTab to kick off the restore.
      if (tab.linkedBrowser.__SS_restoreState &&
          tab.linkedBrowser.__SS_restoreState == TAB_STATE_NEEDS_RESTORE)
        this.restoreTab(tab);

      // attempt to update the current URL we send in a crash report
      this._updateCrashReportURL(aWindow);
    }
  },

  onTabShow: function ssi_onTabShow(aWindow, aTab) {
    // If the tab hasn't been restored yet, move it into the right bucket
    if (aTab.linkedBrowser.__SS_restoreState &&
        aTab.linkedBrowser.__SS_restoreState == TAB_STATE_NEEDS_RESTORE) {
      TabRestoreQueue.hiddenToVisible(aTab);

      // let's kick off tab restoration again to ensure this tab gets restored
      // with "restore_hidden_tabs" == false (now that it has become visible)
      this.restoreNextTab();
    }

    // If possible, update cached data without having to invalidate it
    TabStateCache.updateField(aTab, "hidden", false);

    // Default delay of 2 seconds gives enough time to catch multiple TabShow
    // events due to changing groups in Panorama.
    this.saveStateDelayed(aWindow);
  },

  onTabHide: function ssi_onTabHide(aWindow, aTab) {
    // If the tab hasn't been restored yet, move it into the right bucket
    if (aTab.linkedBrowser.__SS_restoreState &&
        aTab.linkedBrowser.__SS_restoreState == TAB_STATE_NEEDS_RESTORE) {
      TabRestoreQueue.visibleToHidden(aTab);
    }

    // If possible, update cached data without having to invalidate it
    TabStateCache.updateField(aTab, "hidden", true);

    // Default delay of 2 seconds gives enough time to catch multiple TabHide
    // events due to changing groups in Panorama.
    this.saveStateDelayed(aWindow);
  },

  /* ........ nsISessionStore API .............. */

  getBrowserState: function ssi_getBrowserState() {
    let state = this.getCurrentState();

    // Don't include the last session state in getBrowserState().
    delete state.lastSessionState;

    return this._toJSONString(state);
  },

  setBrowserState: function ssi_setBrowserState(aState) {
    this._handleClosedWindows();

    try {
      var state = JSON.parse(aState);
    }
    catch (ex) { /* invalid state object - don't restore anything */ }
    if (!state || !state.windows)
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);

    this._browserSetState = true;

    // Make sure the priority queue is emptied out
    this._resetRestoringState();

    var window = this._getMostRecentBrowserWindow();
    if (!window) {
      this._restoreCount = 1;
      this._openWindowWithState(state);
      return;
    }

    // close all other browser windows
    this._forEachBrowserWindow(function(aWindow) {
      if (aWindow != window) {
        aWindow.close();
        this.onClose(aWindow);
      }
    });

    // make sure closed window data isn't kept
    this._closedWindows = [];

    // determine how many windows are meant to be restored
    this._restoreCount = state.windows ? state.windows.length : 0;

    // restore to the given state
    this.restoreWindow(window, state, {overwriteTabs: true});
  },

  getWindowState: function ssi_getWindowState(aWindow) {
    if ("__SSi" in aWindow) {
      return this._toJSONString(this._getWindowState(aWindow));
    }

    if (DyingWindowCache.has(aWindow)) {
      let data = DyingWindowCache.get(aWindow);
      return this._toJSONString({ windows: [data] });
    }

    throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
  },

  setWindowState: function ssi_setWindowState(aWindow, aState, aOverwrite) {
    if (!aWindow.__SSi)
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);

    this.restoreWindow(aWindow, aState, {overwriteTabs: aOverwrite});
  },

  getTabState: function ssi_getTabState(aTab) {
    if (!aTab.ownerDocument || !aTab.ownerDocument.defaultView.__SSi)
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);

    let tabState = TabState.collectSync(aTab);

    return this._toJSONString(tabState);
  },

  setTabState: function ssi_setTabState(aTab, aState) {
    // Remove the tab state from the cache.
    // Note that we cannot simply replace the contents of the cache
    // as |aState| can be an incomplete state that will be completed
    // by |restoreHistoryPrecursor|.
    let tabState = JSON.parse(aState);
    if (!tabState) {
      debug("Empty state argument");
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
    }
    if (typeof tabState != "object") {
      debug("State argument does not represent an object");
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
    }
    if (!("entries" in tabState)) {
      debug("State argument must contain field 'entries'");
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
    }
    if (!aTab.ownerDocument) {
      debug("Tab argument must have an owner document");
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
    }

    let window = aTab.ownerDocument.defaultView;
    if (!("__SSi" in window)) {
      debug("Default view of ownerDocument must have a unique identifier");
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
    }

    if (aTab.linkedBrowser.__SS_restoreState) {
      this._resetTabRestoringState(aTab);
    }

    TabStateCache.delete(aTab);
    this._setWindowStateBusy(window);
    this.restoreHistoryPrecursor(window, [aTab], [tabState], 0, 0, 0);
  },

  duplicateTab: function ssi_duplicateTab(aWindow, aTab, aDelta = 0) {
    if (!aTab.ownerDocument || !aTab.ownerDocument.defaultView.__SSi ||
        !aWindow.getBrowser)
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);

    // Duplicate the tab state
    let tabState = TabState.clone(aTab);

    tabState.index += aDelta;
    tabState.index = Math.max(1, Math.min(tabState.index, tabState.entries.length));
    tabState.pinned = false;

    this._setWindowStateBusy(aWindow);
    let newTab = aTab == aWindow.gBrowser.selectedTab ?
      aWindow.gBrowser.addTab(null, {relatedToCurrent: true, ownerTab: aTab}) :
      aWindow.gBrowser.addTab();

    this.restoreHistoryPrecursor(aWindow, [newTab], [tabState], 0, 0, 0,
                                 true /* Load this tab right away. */);

    return newTab;
  },

  setNumberOfTabsClosedLast: function ssi_setNumberOfTabsClosedLast(aWindow, aNumber) {
    if (this._disabledForMultiProcess)
      return;

    if ("__SSi" in aWindow) {
      return NumberOfTabsClosedLastPerWindow.set(aWindow, aNumber);
    }

    throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
  },

  /* Used to undo batch tab-close operations. Defaults to 1. */
  getNumberOfTabsClosedLast: function ssi_getNumberOfTabsClosedLast(aWindow) {
    if (this._disabledForMultiProcess)
      return 0;

    if ("__SSi" in aWindow) {
      // Blank tabs cannot be undo-closed, so the number returned by
      // the NumberOfTabsClosedLastPerWindow can be greater than the
      // return value of getClosedTabCount. We won't restore blank
      // tabs, so we return the minimum of these two values.
      return Math.min(NumberOfTabsClosedLastPerWindow.get(aWindow) || 1,
                      this.getClosedTabCount(aWindow));
    }

    throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
  },

  getClosedTabCount: function ssi_getClosedTabCount(aWindow) {
    if ("__SSi" in aWindow) {
      return this._windows[aWindow.__SSi]._closedTabs.length;
    }

    if (DyingWindowCache.has(aWindow)) {
      return DyingWindowCache.get(aWindow)._closedTabs.length;
    }

    throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
  },

  getClosedTabData: function ssi_getClosedTabDataAt(aWindow) {
    if ("__SSi" in aWindow) {
      return this._toJSONString(this._windows[aWindow.__SSi]._closedTabs);
    }

    if (DyingWindowCache.has(aWindow)) {
      let data = DyingWindowCache.get(aWindow);
      return this._toJSONString(data._closedTabs);
    }

    throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
  },

  undoCloseTab: function ssi_undoCloseTab(aWindow, aIndex) {
    if (!aWindow.__SSi)
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);

    var closedTabs = this._windows[aWindow.__SSi]._closedTabs;

    // default to the most-recently closed tab
    aIndex = aIndex || 0;
    if (!(aIndex in closedTabs))
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);

    // fetch the data of closed tab, while removing it from the array
    let closedTab = closedTabs.splice(aIndex, 1).shift();
    let closedTabState = closedTab.state;

    this._setWindowStateBusy(aWindow);
    // create a new tab
    let tabbrowser = aWindow.gBrowser;
    let tab = tabbrowser.addTab();

    // restore tab content
    this.restoreHistoryPrecursor(aWindow, [tab], [closedTabState], 1, 0, 0);

    // restore the tab's position
    tabbrowser.moveTabTo(tab, closedTab.pos);

    // focus the tab's content area (bug 342432)
    tab.linkedBrowser.focus();

    return tab;
  },

  forgetClosedTab: function ssi_forgetClosedTab(aWindow, aIndex) {
    if (!aWindow.__SSi)
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);

    var closedTabs = this._windows[aWindow.__SSi]._closedTabs;

    // default to the most-recently closed tab
    aIndex = aIndex || 0;
    if (!(aIndex in closedTabs))
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);

    // remove closed tab from the array
    closedTabs.splice(aIndex, 1);
  },

  getClosedWindowCount: function ssi_getClosedWindowCount() {
    return this._closedWindows.length;
  },

  getClosedWindowData: function ssi_getClosedWindowData() {
    return this._toJSONString(this._closedWindows);
  },

  undoCloseWindow: function ssi_undoCloseWindow(aIndex) {
    if (!(aIndex in this._closedWindows))
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);

    // reopen the window
    let state = { windows: this._closedWindows.splice(aIndex, 1) };
    let window = this._openWindowWithState(state);
    this.windowToFocus = window;
    return window;
  },

  forgetClosedWindow: function ssi_forgetClosedWindow(aIndex) {
    // default to the most-recently closed window
    aIndex = aIndex || 0;
    if (!(aIndex in this._closedWindows))
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);

    // remove closed window from the array
    this._closedWindows.splice(aIndex, 1);
  },

  getWindowValue: function ssi_getWindowValue(aWindow, aKey) {
    if (this._disabledForMultiProcess)
      return "";

    if ("__SSi" in aWindow) {
      var data = this._windows[aWindow.__SSi].extData || {};
      return data[aKey] || "";
    }

    if (DyingWindowCache.has(aWindow)) {
      let data = DyingWindowCache.get(aWindow).extData || {};
      return data[aKey] || "";
    }

    throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
  },

  setWindowValue: function ssi_setWindowValue(aWindow, aKey, aStringValue) {
    if (aWindow.__SSi) {
      if (!this._windows[aWindow.__SSi].extData) {
        this._windows[aWindow.__SSi].extData = {};
      }
      this._windows[aWindow.__SSi].extData[aKey] = aStringValue;
      this.saveStateDelayed(aWindow);
    }
    else {
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
    }
  },

  deleteWindowValue: function ssi_deleteWindowValue(aWindow, aKey) {
    if (aWindow.__SSi && this._windows[aWindow.__SSi].extData &&
        this._windows[aWindow.__SSi].extData[aKey])
      delete this._windows[aWindow.__SSi].extData[aKey];
    this.saveStateDelayed(aWindow);
  },

  getTabValue: function ssi_getTabValue(aTab, aKey) {
    let data = {};
    if (aTab.__SS_extdata) {
      data = aTab.__SS_extdata;
    }
    else if (aTab.linkedBrowser.__SS_data && aTab.linkedBrowser.__SS_data.extData) {
      // If the tab hasn't been fully restored, get the data from the to-be-restored data
      data = aTab.linkedBrowser.__SS_data.extData;
    }
    return data[aKey] || "";
  },

  setTabValue: function ssi_setTabValue(aTab, aKey, aStringValue) {
    // If the tab hasn't been restored, then set the data there, otherwise we
    // could lose newly added data.
    let saveTo;
    if (aTab.__SS_extdata) {
      saveTo = aTab.__SS_extdata;
    }
    else if (aTab.linkedBrowser.__SS_data && aTab.linkedBrowser.__SS_data.extData) {
      saveTo = aTab.linkedBrowser.__SS_data.extData;
    }
    else {
      aTab.__SS_extdata = {};
      saveTo = aTab.__SS_extdata;
    }

    saveTo[aKey] = aStringValue;
    TabStateCache.updateField(aTab, "extData", saveTo);
    this.saveStateDelayed(aTab.ownerDocument.defaultView);
  },

  deleteTabValue: function ssi_deleteTabValue(aTab, aKey) {
    // We want to make sure that if data is accessed early, we attempt to delete
    // that data from __SS_data as well. Otherwise we'll throw in cases where
    // data can be set or read.
    let deleteFrom;
    if (aTab.__SS_extdata) {
      deleteFrom = aTab.__SS_extdata;
    }
    else if (aTab.linkedBrowser.__SS_data && aTab.linkedBrowser.__SS_data.extData) {
      deleteFrom = aTab.linkedBrowser.__SS_data.extData;
    }

    if (deleteFrom && aKey in deleteFrom) {
      delete deleteFrom[aKey];

      // Keep the extData object only if it is not empty, to save
      // a little disk space when serializing the tab state later.
      if (Object.keys(deleteFrom).length) {
        TabStateCache.updateField(aTab, "extData", deleteFrom);
      } else {
        TabStateCache.removeField(aTab, "extData");
      }

      this.saveStateDelayed(aTab.ownerDocument.defaultView);
    }
  },

  persistTabAttribute: function ssi_persistTabAttribute(aName) {
    if (TabAttributes.persist(aName)) {
      TabStateCache.clear();
      this.saveStateDelayed();
    }
  },

  /**
   * Restores the session state stored in _lastSessionState. This will attempt
   * to merge data into the current session. If a window was opened at startup
   * with pinned tab(s), then the remaining data from the previous session for
   * that window will be opened into that winddow. Otherwise new windows will
   * be opened.
   */
  restoreLastSession: function ssi_restoreLastSession() {
    // Use the public getter since it also checks PB mode
    if (!this.canRestoreLastSession)
      throw (Components.returnCode = Cr.NS_ERROR_FAILURE);

    // First collect each window with its id...
    let windows = {};
    this._forEachBrowserWindow(function(aWindow) {
      if (aWindow.__SS_lastSessionWindowID)
        windows[aWindow.__SS_lastSessionWindowID] = aWindow;
    });

    let lastSessionState = this._lastSessionState;

    // This shouldn't ever be the case...
    if (!lastSessionState.windows.length)
      throw (Components.returnCode = Cr.NS_ERROR_UNEXPECTED);

    // We're technically doing a restore, so set things up so we send the
    // notification when we're done. We want to send "sessionstore-browser-state-restored".
    this._restoreCount = lastSessionState.windows.length;
    this._browserSetState = true;

    // We want to re-use the last opened window instead of opening a new one in
    // the case where it's "empty" and not associated with a window in the session.
    // We will do more processing via _prepWindowToRestoreInto if we need to use
    // the lastWindow.
    let lastWindow = this._getMostRecentBrowserWindow();
    let canUseLastWindow = lastWindow &&
                           !lastWindow.__SS_lastSessionWindowID;

    // Restore into windows or open new ones as needed.
    for (let i = 0; i < lastSessionState.windows.length; i++) {
      let winState = lastSessionState.windows[i];
      let lastSessionWindowID = winState.__lastSessionWindowID;
      // delete lastSessionWindowID so we don't add that to the window again
      delete winState.__lastSessionWindowID;

      // See if we can use an open window. First try one that is associated with
      // the state we're trying to restore and then fallback to the last selected
      // window.
      let windowToUse = windows[lastSessionWindowID];
      if (!windowToUse && canUseLastWindow) {
        windowToUse = lastWindow;
        canUseLastWindow = false;
      }

      let [canUseWindow, canOverwriteTabs] = this._prepWindowToRestoreInto(windowToUse);

      // If there's a window already open that we can restore into, use that
      if (canUseWindow) {
        // Since we're not overwriting existing tabs, we want to merge _closedTabs,
        // putting existing ones first. Then make sure we're respecting the max pref.
        if (winState._closedTabs && winState._closedTabs.length) {
          let curWinState = this._windows[windowToUse.__SSi];
          curWinState._closedTabs = curWinState._closedTabs.concat(winState._closedTabs);
          curWinState._closedTabs.splice(this._prefBranch.getIntPref("sessionstore.max_tabs_undo"), curWinState._closedTabs.length);
        }

        // Restore into that window - pretend it's a followup since we'll already
        // have a focused window.
        //XXXzpao This is going to merge extData together (taking what was in
        //        winState over what is in the window already. The hack we have
        //        in _preWindowToRestoreInto will prevent most (all?) Panorama
        //        weirdness but we will still merge other extData.
        //        Bug 588217 should make this go away by merging the group data.
        let options = {overwriteTabs: canOverwriteTabs, isFollowUp: true};
        this.restoreWindow(windowToUse, { windows: [winState] }, options);
      }
      else {
        this._openWindowWithState({ windows: [winState] });
      }
    }

    // Merge closed windows from this session with ones from last session
    if (lastSessionState._closedWindows) {
      this._closedWindows = this._closedWindows.concat(lastSessionState._closedWindows);
      this._capClosedWindows();
    }

    if (lastSessionState.scratchpads) {
      ScratchpadManager.restoreSession(lastSessionState.scratchpads);
    }

    // Set data that persists between sessions
    this._recentCrashes = lastSessionState.session &&
                          lastSessionState.session.recentCrashes || 0;

    // Update the session start time using the restored session state.
    this._updateSessionStartTime(lastSessionState);

    this._lastSessionState = null;
  },

  /**
   * See if aWindow is usable for use when restoring a previous session via
   * restoreLastSession. If usable, prepare it for use.
   *
   * @param aWindow
   *        the window to inspect & prepare
   * @returns [canUseWindow, canOverwriteTabs]
   *          canUseWindow: can the window be used to restore into
   *          canOverwriteTabs: all of the current tabs are home pages and we
   *                            can overwrite them
   */
  _prepWindowToRestoreInto: function ssi_prepWindowToRestoreInto(aWindow) {
    if (!aWindow)
      return [false, false];

    // We might be able to overwrite the existing tabs instead of just adding
    // the previous session's tabs to the end. This will be set if possible.
    let canOverwriteTabs = false;

    // Step 1 of processing:
    // Inspect extData for Panorama identifiers. If found, then we want to
    // inspect further. If there is a single group, then we can use this
    // window. If there are multiple groups then we won't use this window.
    let groupsData = this.getWindowValue(aWindow, "tabview-groups");
    if (groupsData) {
      groupsData = JSON.parse(groupsData);

      // If there are multiple groups, we don't want to use this window.
      if (groupsData.totalNumber > 1)
        return [false, false];
    }

    // Step 2 of processing:
    // If we're still here, then the window is usable. Look at the open tabs in
    // comparison to home pages. If all the tabs are home pages then we'll end
    // up overwriting all of them. Otherwise we'll just close the tabs that
    // match home pages. Tabs with the about:blank URI will always be
    // overwritten.
    let homePages = ["about:blank"];
    let removableTabs = [];
    let tabbrowser = aWindow.gBrowser;
    let normalTabsLen = tabbrowser.tabs.length - tabbrowser._numPinnedTabs;
    let startupPref = this._prefBranch.getIntPref("startup.page");
    if (startupPref == 1)
      homePages = homePages.concat(aWindow.gHomeButton.getHomePage().split("|"));

    for (let i = tabbrowser._numPinnedTabs; i < tabbrowser.tabs.length; i++) {
      let tab = tabbrowser.tabs[i];
      if (homePages.indexOf(tab.linkedBrowser.currentURI.spec) != -1) {
        removableTabs.push(tab);
      }
    }

    if (tabbrowser.tabs.length == removableTabs.length) {
      canOverwriteTabs = true;
    }
    else {
      // If we're not overwriting all of the tabs, then close the home tabs.
      for (let i = removableTabs.length - 1; i >= 0; i--) {
        tabbrowser.removeTab(removableTabs.pop(), { animate: false });
      }
    }

    return [true, canOverwriteTabs];
  },

  /* ........ Async Data Collection .............. */

  /**
   * Kicks off asynchronous data collection for all tabs that do not have any
   * cached data. The returned promise will only notify that the tab collection
   * has been finished without resolving to any data. The tab collection for a
   * a few or all tabs might have failed or timed out. By calling
   * fillTabCachesAsynchronously() and waiting for the promise to be resolved
   * before calling getCurrentState(), callers ensure that most of the data
   * should have been collected asynchronously, without blocking the main
   * thread.
   *
   * @return {Promise} the promise that is fulfilled when the tab data is ready
   */
  fillTabCachesAsynchronously: function () {
    let countdown = 0;
    let deferred = Promise.defer();
    let activeWindow = this._getMostRecentBrowserWindow();

    // The callback that will be called when a promise has been resolved
    // successfully, i.e. the tab data has been collected.
    function done() {
      if (--countdown === 0) {
        deferred.resolve();
      }
    }

    // The callback that will be called when a promise is rejected, i.e. we
    // we couldn't collect the tab data because of a script error or a timeout.
    function fail(reason) {
      debug("Failed collecting tab data asynchronously: " + reason);
      done();
    }

    this._forEachBrowserWindow(win => {
      if (!this._isWindowLoaded(win)) {
        // Bail out if the window hasn't even loaded, yet.
        return;
      }

      if (!DirtyWindows.has(win) && win != activeWindow) {
        // Bail out if the window is not dirty and inactive.
        return;
      }

      for (let tab of win.gBrowser.tabs) {
        if (!TabStateCache.has(tab)) {
          countdown++;
          TabState.collect(tab).then(done, fail);
        }
      }
    });

    // If no dirty tabs were found, return a resolved
    // promise because there is nothing to do here.
    if (countdown == 0) {
      return Promise.resolve();
    }

    return deferred.promise;
  },

  /* ........ Saving Functionality .............. */

  /**
   * Store window dimensions, visibility, sidebar
   * @param aWindow
   *        Window reference
   */
  _updateWindowFeatures: function ssi_updateWindowFeatures(aWindow) {
    var winData = this._windows[aWindow.__SSi];

    WINDOW_ATTRIBUTES.forEach(function(aAttr) {
      winData[aAttr] = this._getWindowDimension(aWindow, aAttr);
    }, this);

    var hidden = WINDOW_HIDEABLE_FEATURES.filter(function(aItem) {
      return aWindow[aItem] && !aWindow[aItem].visible;
    });
    if (hidden.length != 0)
      winData.hidden = hidden.join(",");
    else if (winData.hidden)
      delete winData.hidden;

    var sidebar = aWindow.document.getElementById("sidebar-box").getAttribute("sidebarcommand");
    if (sidebar)
      winData.sidebar = sidebar;
    else if (winData.sidebar)
      delete winData.sidebar;
  },

  /**
   * gather session data as object
   * @param aUpdateAll
   *        Bool update all windows
   * @returns object
   */
  getCurrentState: function (aUpdateAll) {
    this._handleClosedWindows();

    var activeWindow = this._getMostRecentBrowserWindow();

    if (this._loadState == STATE_RUNNING) {
      // update the data for all windows with activities since the last save operation
      this._forEachBrowserWindow(function(aWindow) {
        if (!this._isWindowLoaded(aWindow)) // window data is still in _statesToRestore
          return;
        if (aUpdateAll || DirtyWindows.has(aWindow) || aWindow == activeWindow) {
          this._collectWindowData(aWindow);
        }
        else { // always update the window features (whose change alone never triggers a save operation)
          this._updateWindowFeatures(aWindow);
        }
      });
      DirtyWindows.clear();
    }

    // An array that at the end will hold all current window data.
    var total = [];
    // The ids of all windows contained in 'total' in the same order.
    var ids = [];
    // The number of window that are _not_ popups.
    var nonPopupCount = 0;
    var ix;

    // collect the data for all windows
    for (ix in this._windows) {
      if (this._windows[ix]._restoring) // window data is still in _statesToRestore
        continue;
      total.push(this._windows[ix]);
      ids.push(ix);
      if (!this._windows[ix].isPopup)
        nonPopupCount++;
    }
    SessionCookies.update(total);

    // collect the data for all windows yet to be restored
    for (ix in this._statesToRestore) {
      for each (let winData in this._statesToRestore[ix].windows) {
        total.push(winData);
        if (!winData.isPopup)
          nonPopupCount++;
      }
    }

    // shallow copy this._closedWindows to preserve current state
    let lastClosedWindowsCopy = this._closedWindows.slice();

#ifndef XP_MACOSX
    // If no non-popup browser window remains open, return the state of the last
    // closed window(s). We only want to do this when we're actually "ending"
    // the session.
    //XXXzpao We should do this for _restoreLastWindow == true, but that has
    //        its own check for popups. c.f. bug 597619
    if (nonPopupCount == 0 && lastClosedWindowsCopy.length > 0 &&
        this._loadState == STATE_QUITTING) {
      // prepend the last non-popup browser window, so that if the user loads more tabs
      // at startup we don't accidentally add them to a popup window
      do {
        total.unshift(lastClosedWindowsCopy.shift())
      } while (total[0].isPopup && lastClosedWindowsCopy.length > 0)
    }
#endif

    if (activeWindow) {
      this.activeWindowSSiCache = activeWindow.__SSi || "";
    }
    ix = ids.indexOf(this.activeWindowSSiCache);
    // We don't want to restore focus to a minimized window or a window which had all its
    // tabs stripped out (doesn't exist).
    if (ix != -1 && total[ix] && total[ix].sizemode == "minimized")
      ix = -1;

    let session = {
      state: this._loadState == STATE_RUNNING ? STATE_RUNNING_STR : STATE_STOPPED_STR,
      lastUpdate: Date.now(),
      startTime: this._sessionStartTime,
      recentCrashes: this._recentCrashes
    };

    // get open Scratchpad window states too
    var scratchpads = ScratchpadManager.getSessionState();

    let state = {
      windows: total,
      selectedWindow: ix + 1,
      _closedWindows: lastClosedWindowsCopy,
      session: session,
      scratchpads: scratchpads
    };

    // Persist the last session if we deferred restoring it
    if (this._lastSessionState) {
      state.lastSessionState = this._lastSessionState;
    }

    return state;
  },

  /**
   * serialize session data for a window
   * @param aWindow
   *        Window reference
   * @returns string
   */
  _getWindowState: function ssi_getWindowState(aWindow) {
    if (!this._isWindowLoaded(aWindow))
      return this._statesToRestore[aWindow.__SS_restoreID];

    if (this._loadState == STATE_RUNNING) {
      this._collectWindowData(aWindow);
    }

    let windows = [this._windows[aWindow.__SSi]];
    SessionCookies.update(windows);

    return { windows: windows };
  },

  _collectWindowData: function ssi_collectWindowData(aWindow) {
    if (!this._isWindowLoaded(aWindow))
      return;

    let tabbrowser = aWindow.gBrowser;
    let tabs = tabbrowser.tabs;
    let winData = this._windows[aWindow.__SSi];
    let tabsData = winData.tabs = [];

    // update the internal state data for this window
    for (let tab of tabs) {
      tabsData.push(TabState.collectSync(tab));
    }
    winData.selected = tabbrowser.mTabBox.selectedIndex + 1;

    this._updateWindowFeatures(aWindow);

    // Make sure we keep __SS_lastSessionWindowID around for cases like entering
    // or leaving PB mode.
    if (aWindow.__SS_lastSessionWindowID)
      this._windows[aWindow.__SSi].__lastSessionWindowID =
        aWindow.__SS_lastSessionWindowID;

    DirtyWindows.remove(aWindow);
  },

  /* ........ Restoring Functionality .............. */

  /**
   * restore features to a single window
   * @param aWindow
   *        Window reference
   * @param aState
   *        JS object or its eval'able source
   * @param aOptions
   *        {overwriteTabs: true} to overwrite existing tabs w/ new ones
   *        {isFollowUp: true} if this is not the restoration of the 1st window
   *        {firstWindow: true} if this is the first non-private window we're
   *                            restoring in this session, that might open an
   *                            external link as well
   */
  restoreWindow: function ssi_restoreWindow(aWindow, aState, aOptions = {}) {
    let overwriteTabs = aOptions && aOptions.overwriteTabs;
    let isFollowUp = aOptions && aOptions.isFollowUp;
    let firstWindow = aOptions && aOptions.firstWindow;

    if (isFollowUp) {
      this.windowToFocus = aWindow;
    }
    // initialize window if necessary
    if (aWindow && (!aWindow.__SSi || !this._windows[aWindow.__SSi]))
      this.onLoad(aWindow);

    try {
      var root = typeof aState == "string" ? JSON.parse(aState) : aState;
      if (!root.windows[0]) {
        this._sendRestoreCompletedNotifications();
        return; // nothing to restore
      }
    }
    catch (ex) { // invalid state object - don't restore anything
      debug(ex);
      this._sendRestoreCompletedNotifications();
      return;
    }

    TelemetryStopwatch.start("FX_SESSION_RESTORE_RESTORE_WINDOW_MS");

    // We're not returning from this before we end up calling restoreHistoryPrecursor
    // for this window, so make sure we send the SSWindowStateBusy event.
    this._setWindowStateBusy(aWindow);

    if (root._closedWindows)
      this._closedWindows = root._closedWindows;

    var winData;
    if (!root.selectedWindow || root.selectedWindow > root.windows.length) {
      root.selectedWindow = 0;
    }

    // open new windows for all further window entries of a multi-window session
    // (unless they don't contain any tab data)
    for (var w = 1; w < root.windows.length; w++) {
      winData = root.windows[w];
      if (winData && winData.tabs && winData.tabs[0]) {
        var window = this._openWindowWithState({ windows: [winData] });
        if (w == root.selectedWindow - 1) {
          this.windowToFocus = window;
        }
      }
    }
    winData = root.windows[0];
    if (!winData.tabs) {
      winData.tabs = [];
    }
    // don't restore a single blank tab when we've had an external
    // URL passed in for loading at startup (cf. bug 357419)
    else if (firstWindow && !overwriteTabs && winData.tabs.length == 1 &&
             (!winData.tabs[0].entries || winData.tabs[0].entries.length == 0)) {
      winData.tabs = [];
    }

    var tabbrowser = aWindow.gBrowser;
    var openTabCount = overwriteTabs ? tabbrowser.browsers.length : -1;
    var newTabCount = winData.tabs.length;
    var tabs = [];

    // disable smooth scrolling while adding, moving, removing and selecting tabs
    var tabstrip = tabbrowser.tabContainer.mTabstrip;
    var smoothScroll = tabstrip.smoothScroll;
    tabstrip.smoothScroll = false;

    // unpin all tabs to ensure they are not reordered in the next loop
    if (overwriteTabs) {
      for (let t = tabbrowser._numPinnedTabs - 1; t > -1; t--)
        tabbrowser.unpinTab(tabbrowser.tabs[t]);
    }

    // We need to keep track of the initially open tabs so that they
    // can be moved to the end of the restored tabs.
    let initialTabs = [];
    if (!overwriteTabs && firstWindow) {
      initialTabs = Array.slice(tabbrowser.tabs);
    }

    // make sure that the selected tab won't be closed in order to
    // prevent unnecessary flickering
    if (overwriteTabs && tabbrowser.selectedTab._tPos >= newTabCount)
      tabbrowser.moveTabTo(tabbrowser.selectedTab, newTabCount - 1);

    let numVisibleTabs = 0;

    for (var t = 0; t < newTabCount; t++) {
      tabs.push(t < openTabCount ?
                tabbrowser.tabs[t] :
                tabbrowser.addTab("about:blank", {skipAnimation: true}));

      if (winData.tabs[t].pinned)
        tabbrowser.pinTab(tabs[t]);

      if (winData.tabs[t].hidden) {
        tabbrowser.hideTab(tabs[t]);
      }
      else {
        tabbrowser.showTab(tabs[t]);
        numVisibleTabs++;
      }
    }

    if (!overwriteTabs && firstWindow) {
      // Move the originally open tabs to the end
      let endPosition = tabbrowser.tabs.length - 1;
      for (let i = 0; i < initialTabs.length; i++) {
        tabbrowser.moveTabTo(initialTabs[i], endPosition);
      }
    }

    // if all tabs to be restored are hidden, make the first one visible
    if (!numVisibleTabs && winData.tabs.length) {
      winData.tabs[0].hidden = false;
      tabbrowser.showTab(tabs[0]);
    }

    // If overwriting tabs, we want to reset each tab's "restoring" state. Since
    // we're overwriting those tabs, they should no longer be restoring. The
    // tabs will be rebuilt and marked if they need to be restored after loading
    // state (in restoreHistoryPrecursor).
    // We also want to invalidate any cached information on the tab state.
    if (overwriteTabs) {
      for (let i = 0; i < tabbrowser.tabs.length; i++) {
        let tab = tabbrowser.tabs[i];
        TabStateCache.delete(tab);
        if (tabbrowser.browsers[i].__SS_restoreState)
          this._resetTabRestoringState(tab);
      }
    }

    // We want to set up a counter on the window that indicates how many tabs
    // in this window are unrestored. This will be used in restoreNextTab to
    // determine if gRestoreTabsProgressListener should be removed from the window.
    // If we aren't overwriting existing tabs, then we want to add to the existing
    // count in case there are still tabs restoring.
    if (!aWindow.__SS_tabsToRestore)
      aWindow.__SS_tabsToRestore = 0;
    if (overwriteTabs)
      aWindow.__SS_tabsToRestore = newTabCount;
    else
      aWindow.__SS_tabsToRestore += newTabCount;

    // We want to correlate the window with data from the last session, so
    // assign another id if we have one. Otherwise clear so we don't do
    // anything with it.
    delete aWindow.__SS_lastSessionWindowID;
    if (winData.__lastSessionWindowID)
      aWindow.__SS_lastSessionWindowID = winData.__lastSessionWindowID;

    // when overwriting tabs, remove all superflous ones
    if (overwriteTabs && newTabCount < openTabCount) {
      Array.slice(tabbrowser.tabs, newTabCount, openTabCount)
           .forEach(tabbrowser.removeTab, tabbrowser);
    }

    if (overwriteTabs) {
      this.restoreWindowFeatures(aWindow, winData);
      delete this._windows[aWindow.__SSi].extData;
    }
    if (winData.cookies) {
      this.restoreCookies(winData.cookies);
    }
    if (winData.extData) {
      if (!this._windows[aWindow.__SSi].extData) {
        this._windows[aWindow.__SSi].extData = {};
      }
      for (var key in winData.extData) {
        this._windows[aWindow.__SSi].extData[key] = winData.extData[key];
      }
    }
    if (overwriteTabs || firstWindow) {
      this._windows[aWindow.__SSi]._closedTabs = winData._closedTabs || [];
    }

    this.restoreHistoryPrecursor(aWindow, tabs, winData.tabs,
      (overwriteTabs ? (parseInt(winData.selected) || 1) : 0), 0, 0);

    if (aState.scratchpads) {
      ScratchpadManager.restoreSession(aState.scratchpads);
    }

    // set smoothScroll back to the original value
    tabstrip.smoothScroll = smoothScroll;

    TelemetryStopwatch.finish("FX_SESSION_RESTORE_RESTORE_WINDOW_MS");

    this._sendRestoreCompletedNotifications();
  },

  /**
   * Sets the tabs restoring order with the following priority:
   * Selected tab, pinned tabs, optimized visible tabs, other visible tabs and
   * hidden tabs.
   * @param aTabBrowser
   *        Tab browser object
   * @param aTabs
   *        Array of tab references
   * @param aTabData
   *        Array of tab data
   * @param aSelectedTab
   *        Index of selected tab (1 is first tab, 0 no selected tab)
   */
  _setTabsRestoringOrder : function ssi__setTabsRestoringOrder(
    aTabBrowser, aTabs, aTabData, aSelectedTab) {

    // Store the selected tab. Need to substract one to get the index in aTabs.
    let selectedTab;
    if (aSelectedTab > 0 && aTabs[aSelectedTab - 1]) {
      selectedTab = aTabs[aSelectedTab - 1];
    }

    // Store the pinned tabs and hidden tabs.
    let pinnedTabs = [];
    let pinnedTabsData = [];
    let hiddenTabs = [];
    let hiddenTabsData = [];
    if (aTabs.length > 1) {
      for (let t = aTabs.length - 1; t >= 0; t--) {
        if (aTabData[t].pinned) {
          pinnedTabs.unshift(aTabs.splice(t, 1)[0]);
          pinnedTabsData.unshift(aTabData.splice(t, 1)[0]);
        } else if (aTabData[t].hidden) {
          hiddenTabs.unshift(aTabs.splice(t, 1)[0]);
          hiddenTabsData.unshift(aTabData.splice(t, 1)[0]);
        }
      }
    }

    // Optimize the visible tabs only if there is a selected tab.
    if (selectedTab) {
      let selectedTabIndex = aTabs.indexOf(selectedTab);
      if (selectedTabIndex > 0) {
        let scrollSize = aTabBrowser.tabContainer.mTabstrip.scrollClientSize;
        let tabWidth = aTabs[0].getBoundingClientRect().width;
        let maxVisibleTabs = Math.ceil(scrollSize / tabWidth);
        if (maxVisibleTabs < aTabs.length) {
          let firstVisibleTab = 0;
          let nonVisibleTabsCount = aTabs.length - maxVisibleTabs;
          if (nonVisibleTabsCount >= selectedTabIndex) {
            // Selected tab is leftmost since we scroll to it when possible.
            firstVisibleTab = selectedTabIndex;
          } else {
            // Selected tab is rightmost or no more room to scroll right.
            firstVisibleTab = nonVisibleTabsCount;
          }
          aTabs = aTabs.splice(firstVisibleTab, maxVisibleTabs).concat(aTabs);
          aTabData =
            aTabData.splice(firstVisibleTab, maxVisibleTabs).concat(aTabData);
        }
      }
    }

    // Merge the stored tabs in order.
    aTabs = pinnedTabs.concat(aTabs, hiddenTabs);
    aTabData = pinnedTabsData.concat(aTabData, hiddenTabsData);

    // Load the selected tab to the first position and select it.
    if (selectedTab) {
      let selectedTabIndex = aTabs.indexOf(selectedTab);
      if (selectedTabIndex > 0) {
        aTabs = aTabs.splice(selectedTabIndex, 1).concat(aTabs);
        aTabData = aTabData.splice(selectedTabIndex, 1).concat(aTabData);
      }
      aTabBrowser.selectedTab = selectedTab;
    }

    return [aTabs, aTabData];
  },
  
  /**
   * Manage history restoration for a window
   * @param aWindow
   *        Window to restore the tabs into
   * @param aTabs
   *        Array of tab references
   * @param aTabData
   *        Array of tab data
   * @param aSelectTab
   *        Index of selected tab
   * @param aIx
   *        Index of the next tab to check readyness for
   * @param aCount
   *        Counter for number of times delaying b/c browser or history aren't ready
   * @param aRestoreImmediately
   *        Flag to indicate whether the given set of tabs aTabs should be
   *        restored/loaded immediately even if restore_on_demand = true
   */
  restoreHistoryPrecursor:
    function ssi_restoreHistoryPrecursor(aWindow, aTabs, aTabData, aSelectTab,
                                         aIx, aCount, aRestoreImmediately = false) {

    var tabbrowser = aWindow.gBrowser;

    // make sure that all browsers and their histories are available
    // - if one's not, resume this check in 100ms (repeat at most 10 times)
    for (var t = aIx; t < aTabs.length; t++) {
      try {
        if (!tabbrowser.getBrowserForTab(aTabs[t]).webNavigation.sessionHistory) {
          throw new Error();
        }
      }
      catch (ex) { // in case browser or history aren't ready yet
        if (aCount < 10) {
          var restoreHistoryFunc = function(self) {
            self.restoreHistoryPrecursor(aWindow, aTabs, aTabData, aSelectTab,
                                         aIx, aCount + 1, aRestoreImmediately);
          };
          aWindow.setTimeout(restoreHistoryFunc, 100, this);
          return;
        }
      }
    }

    if (!this._isWindowLoaded(aWindow)) {
      // from now on, the data will come from the actual window
      delete this._statesToRestore[aWindow.__SS_restoreID];
      delete aWindow.__SS_restoreID;
      delete this._windows[aWindow.__SSi]._restoring;
    }

    // It's important to set the window state to dirty so that
    // we collect their data for the first time when saving state.
    DirtyWindows.add(aWindow);

    // Set the state to restore as the window's current state. Normally, this
    // will just be overridden the next time we collect state but we need this
    // as a fallback should Firefox be shutdown early without notifying us
    // beforehand.
    this._windows[aWindow.__SSi].tabs = aTabData.slice();
    this._windows[aWindow.__SSi].selected = aSelectTab;

    if (aTabs.length == 0) {
      // this is normally done in restoreHistory() but as we're returning early
      // here we need to take care of it.
      this._setWindowStateReady(aWindow);
      return;
    }

    // Sets the tabs restoring order. 
    [aTabs, aTabData] =
      this._setTabsRestoringOrder(tabbrowser, aTabs, aTabData, aSelectTab);

    // Prepare the tabs so that they can be properly restored. We'll pin/unpin
    // and show/hide tabs as necessary. We'll also set the labels, user typed
    // value, and attach a copy of the tab's data in case we close it before
    // it's been restored.
    for (t = 0; t < aTabs.length; t++) {
      let tab = aTabs[t];
      let browser = tabbrowser.getBrowserForTab(tab);
      let tabData = aTabData[t];

      if (tabData.pinned)
        tabbrowser.pinTab(tab);
      else
        tabbrowser.unpinTab(tab);

      if (tabData.hidden)
        tabbrowser.hideTab(tab);
      else
        tabbrowser.showTab(tab);

      if ("attributes" in tabData) {
        // Ensure that we persist tab attributes restored from previous sessions.
        Object.keys(tabData.attributes).forEach(a => TabAttributes.persist(a));
      }

      browser.__SS_tabStillLoading = true;

      // keep the data around to prevent dataloss in case
      // a tab gets closed before it's been properly restored
      browser.__SS_data = tabData;
      browser.__SS_restoreState = TAB_STATE_NEEDS_RESTORE;
      browser.setAttribute("pending", "true");
      tab.setAttribute("pending", "true");

      // Make sure that set/getTabValue will set/read the correct data by
      // wiping out any current value in tab.__SS_extdata.
      delete tab.__SS_extdata;

      if (!tabData.entries || tabData.entries.length == 0) {
        // make sure to blank out this tab's content
        // (just purging the tab's history won't be enough)
        browser.loadURIWithFlags("about:blank",
                                 Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_HISTORY,
                                 null, null, null);
        continue;
      }

      browser.stop(); // in case about:blank isn't done yet

      // wall-paper fix for bug 439675: make sure that the URL to be loaded
      // is always visible in the address bar
      let activeIndex = (tabData.index || tabData.entries.length) - 1;
      let activePageData = tabData.entries[activeIndex] || null;
      let uri = activePageData ? activePageData.url || null : null;
      browser.userTypedValue = uri;

      // Also make sure currentURI is set so that switch-to-tab works before
      // the tab is restored. We'll reset this to about:blank when we try to
      // restore the tab to ensure that docshell doeesn't get confused.
      if (uri) {
        browser.docShell.setCurrentURI(makeURI(uri));
      }

      // If the page has a title, set it.
      if (activePageData) {
        if (activePageData.title) {
          tab.label = activePageData.title;
          tab.crop = "end";
        } else if (activePageData.url != "about:blank") {
          tab.label = activePageData.url;
          tab.crop = "center";
        }
      }
    }

    // helper hashes for ensuring unique frame IDs and unique document
    // identifiers.
    var idMap = { used: {} };
    var docIdentMap = {};
    this.restoreHistory(aWindow, aTabs, aTabData, idMap, docIdentMap,
                        aRestoreImmediately);
  },

  /**
   * Restore history for a window
   * @param aWindow
   *        Window reference
   * @param aTabs
   *        Array of tab references
   * @param aTabData
   *        Array of tab data
   * @param aIdMap
   *        Hash for ensuring unique frame IDs
   * @param aRestoreImmediately
   *        Flag to indicate whether the given set of tabs aTabs should be
   *        restored/loaded immediately even if restore_on_demand = true
   */
  restoreHistory:
    function ssi_restoreHistory(aWindow, aTabs, aTabData, aIdMap, aDocIdentMap,
                                aRestoreImmediately)
  {
    // if the tab got removed before being completely restored, then skip it
    while (aTabs.length > 0 && !(this._canRestoreTabHistory(aTabs[0]))) {
      aTabs.shift();
      aTabData.shift();
    }
    if (aTabs.length == 0) {
      // At this point we're essentially ready for consumers to read/write data
      // via the sessionstore API so we'll send the SSWindowStateReady event.
      this._setWindowStateReady(aWindow);
      return; // no more tabs to restore
    }

    var tab = aTabs.shift();
    var tabData = aTabData.shift();
    var browser = aWindow.gBrowser.getBrowserForTab(tab);
    var history = browser.webNavigation.sessionHistory;

    if (history.count > 0) {
      history.PurgeHistory(history.count);
    }
    history.QueryInterface(Ci.nsISHistoryInternal);

    browser.__SS_shistoryListener = new SessionStoreSHistoryListener(tab);
    history.addSHistoryListener(browser.__SS_shistoryListener);

    if (!tabData.entries) {
      tabData.entries = [];
    }
    if (tabData.extData) {
      tab.__SS_extdata = {};
      for (let key in tabData.extData)
        tab.__SS_extdata[key] = tabData.extData[key];
    }
    else
      delete tab.__SS_extdata;

    for (var i = 0; i < tabData.entries.length; i++) {
      //XXXzpao Wallpaper patch for bug 514751
      if (!tabData.entries[i].url)
        continue;
      history.addEntry(this._deserializeHistoryEntry(tabData.entries[i],
                                                     aIdMap, aDocIdentMap), true);
    }

    // make sure to reset the capabilities and attributes, in case this tab gets reused
    let disallow = new Set(tabData.disallow && tabData.disallow.split(","));
    DocShellCapabilities.restore(browser.docShell, disallow);

    // Restore tab attributes.
    if ("attributes" in tabData) {
      TabAttributes.set(tab, tabData.attributes);
    }

    // Restore the tab icon.
    if ("image" in tabData) {
      aWindow.gBrowser.setIcon(tab, tabData.image);
    }

    if (tabData.storage && browser.docShell instanceof Ci.nsIDocShell)
      SessionStorage.deserialize(browser.docShell, tabData.storage);

    // notify the tabbrowser that the tab chrome has been restored
    var event = aWindow.document.createEvent("Events");
    event.initEvent("SSTabRestoring", true, false);
    tab.dispatchEvent(event);

    // Restore the history in the next tab
    aWindow.setTimeout(() => {
      if (!aWindow.closed) {
        this.restoreHistory(aWindow, aTabs, aTabData, aIdMap, aDocIdentMap,
                            aRestoreImmediately);
      }
    }, 0);

    // This could cause us to ignore MAX_CONCURRENT_TAB_RESTORES a bit, but
    // it ensures each window will have its selected tab loaded.
    if (aRestoreImmediately || aWindow.gBrowser.selectedBrowser == browser) {
      this.restoreTab(tab);
    }
    else {
      TabRestoreQueue.add(tab);
      this.restoreNextTab();
    }
  },

  /**
   * Restores the specified tab. If the tab can't be restored (eg, no history or
   * calling gotoIndex fails), then state changes will be rolled back.
   * This method will check if gTabsProgressListener is attached to the tab's
   * window, ensuring that we don't get caught without one.
   * This method removes the session history listener right before starting to
   * attempt a load. This will prevent cases of "stuck" listeners.
   * If this method returns false, then it is up to the caller to decide what to
   * do. In the common case (restoreNextTab), we will want to then attempt to
   * restore the next tab. In the other case (selecting the tab, reloading the
   * tab), the caller doesn't actually want to do anything if no page is loaded.
   *
   * @param aTab
   *        the tab to restore
   *
   * @returns true/false indicating whether or not a load actually happened
   */
  restoreTab: function ssi_restoreTab(aTab) {
    let window = aTab.ownerDocument.defaultView;
    let browser = aTab.linkedBrowser;
    let tabData = browser.__SS_data;

    // There are cases within where we haven't actually started a load. In that
    // that case we'll reset state changes we made and return false to the caller
    // can handle appropriately.
    let didStartLoad = false;

    // Make sure that the tabs progress listener is attached to this window
    this._ensureTabsProgressListener(window);

    // Make sure that this tab is removed from the priority queue.
    TabRestoreQueue.remove(aTab);

    // Increase our internal count.
    this._tabsRestoringCount++;

    // Set this tab's state to restoring
    browser.__SS_restoreState = TAB_STATE_RESTORING;
    browser.removeAttribute("pending");
    aTab.removeAttribute("pending");

    // Remove the history listener, since we no longer need it once we start restoring
    this._removeSHistoryListener(aTab);

    let activeIndex = (tabData.index || tabData.entries.length) - 1;
    if (activeIndex >= tabData.entries.length)
      activeIndex = tabData.entries.length - 1;
    // Reset currentURI.  This creates a new session history entry with a new
    // doc identifier, so we need to explicitly save and restore the old doc
    // identifier (corresponding to the SHEntry at activeIndex) below.
    browser.webNavigation.setCurrentURI(makeURI("about:blank"));
    // Attach data that will be restored on "load" event, after tab is restored.
    if (activeIndex > -1) {
      // restore those aspects of the currently active documents which are not
      // preserved in the plain history entries (mainly scroll state and text data)
      browser.__SS_restore_data = tabData.entries[activeIndex] || {};
      browser.__SS_restore_pageStyle = tabData.pageStyle || "";
      browser.__SS_restore_tab = aTab;
      didStartLoad = true;
      try {
        // In order to work around certain issues in session history, we need to
        // force session history to update its internal index and call reload
        // instead of gotoIndex. See bug 597315.
        browser.webNavigation.sessionHistory.getEntryAtIndex(activeIndex, true);
        browser.webNavigation.sessionHistory.reloadCurrentEntry();
      }
      catch (ex) {
        // ignore page load errors
        didStartLoad = false;
      }
    }

    // Handle userTypedValue. Setting userTypedValue seems to update gURLbar
    // as needed. Calling loadURI will cancel form filling in restoreDocument
    if (tabData.userTypedValue) {
      browser.userTypedValue = tabData.userTypedValue;
      if (tabData.userTypedClear) {
        // Make it so that we'll enter restoreDocument on page load. We will
        // fire SSTabRestored from there. We don't have any form data to restore
        // so we can just set the URL to null.
        browser.__SS_restore_data = { url: null };
        browser.__SS_restore_tab = aTab;
        if (didStartLoad)
          browser.stop();
        didStartLoad = true;
        browser.loadURIWithFlags(tabData.userTypedValue,
                                 Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP);
      }
    }

    // If we didn't start a load, then we won't reset this tab through the usual
    // channel (via the progress listener), so reset the tab ourselves. We will
    // also send SSTabRestored since this tab has technically been restored.
    if (!didStartLoad) {
      this._sendTabRestoredNotification(aTab);
      this._resetTabRestoringState(aTab);
    }

    return didStartLoad;
  },

  /**
   * This _attempts_ to restore the next available tab. If the restore fails,
   * then we will attempt the next one.
   * There are conditions where this won't do anything:
   *   if we're in the process of quitting
   *   if there are no tabs to restore
   *   if we have already reached the limit for number of tabs to restore
   */
  restoreNextTab: function ssi_restoreNextTab() {
    // If we call in here while quitting, we don't actually want to do anything
    if (this._loadState == STATE_QUITTING)
      return;

    // Don't exceed the maximum number of concurrent tab restores.
    if (this._tabsRestoringCount >= MAX_CONCURRENT_TAB_RESTORES)
      return;

    let tab = TabRestoreQueue.shift();
    if (tab) {
      let didStartLoad = this.restoreTab(tab);
      // If we don't start a load in the restored tab (eg, no entries) then we
      // want to attempt to restore the next tab.
      if (!didStartLoad)
        this.restoreNextTab();
    }
  },

  /**
   * expands serialized history data into a session-history-entry instance
   * @param aEntry
   *        Object containing serialized history data for a URL
   * @param aIdMap
   *        Hash for ensuring unique frame IDs
   * @returns nsISHEntry
   */
  _deserializeHistoryEntry:
    function ssi_deserializeHistoryEntry(aEntry, aIdMap, aDocIdentMap) {

    var shEntry = Cc["@mozilla.org/browser/session-history-entry;1"].
                  createInstance(Ci.nsISHEntry);

    shEntry.setURI(makeURI(aEntry.url));
    shEntry.setTitle(aEntry.title || aEntry.url);
    if (aEntry.subframe)
      shEntry.setIsSubFrame(aEntry.subframe || false);
    shEntry.loadType = Ci.nsIDocShellLoadInfo.loadHistory;
    if (aEntry.contentType)
      shEntry.contentType = aEntry.contentType;
    if (aEntry.referrer)
      shEntry.referrerURI = makeURI(aEntry.referrer);
    if (aEntry.isSrcdocEntry)
      shEntry.srcdocData = aEntry.srcdocData;

    if (aEntry.cacheKey) {
      var cacheKey = Cc["@mozilla.org/supports-PRUint32;1"].
                     createInstance(Ci.nsISupportsPRUint32);
      cacheKey.data = aEntry.cacheKey;
      shEntry.cacheKey = cacheKey;
    }

    if (aEntry.ID) {
      // get a new unique ID for this frame (since the one from the last
      // start might already be in use)
      var id = aIdMap[aEntry.ID] || 0;
      if (!id) {
        for (id = Date.now(); id in aIdMap.used; id++);
        aIdMap[aEntry.ID] = id;
        aIdMap.used[id] = true;
      }
      shEntry.ID = id;
    }

    if (aEntry.docshellID)
      shEntry.docshellID = aEntry.docshellID;

    if (aEntry.structuredCloneState && aEntry.structuredCloneVersion) {
      shEntry.stateData =
        Cc["@mozilla.org/docshell/structured-clone-container;1"].
        createInstance(Ci.nsIStructuredCloneContainer);

      shEntry.stateData.initFromBase64(aEntry.structuredCloneState,
                                       aEntry.structuredCloneVersion);
    }

    if (aEntry.scroll) {
      var scrollPos = (aEntry.scroll || "0,0").split(",");
      scrollPos = [parseInt(scrollPos[0]) || 0, parseInt(scrollPos[1]) || 0];
      shEntry.setScrollPosition(scrollPos[0], scrollPos[1]);
    }

    if (aEntry.postdata_b64) {
      var postdata = atob(aEntry.postdata_b64);
      var stream = Cc["@mozilla.org/io/string-input-stream;1"].
                   createInstance(Ci.nsIStringInputStream);
      stream.setData(postdata, postdata.length);
      shEntry.postData = stream;
    }

    let childDocIdents = {};
    if (aEntry.docIdentifier) {
      // If we have a serialized document identifier, try to find an SHEntry
      // which matches that doc identifier and adopt that SHEntry's
      // BFCacheEntry.  If we don't find a match, insert shEntry as the match
      // for the document identifier.
      let matchingEntry = aDocIdentMap[aEntry.docIdentifier];
      if (!matchingEntry) {
        matchingEntry = {shEntry: shEntry, childDocIdents: childDocIdents};
        aDocIdentMap[aEntry.docIdentifier] = matchingEntry;
      }
      else {
        shEntry.adoptBFCacheEntry(matchingEntry.shEntry);
        childDocIdents = matchingEntry.childDocIdents;
      }
    }

    if (aEntry.owner_b64) {
      var ownerInput = Cc["@mozilla.org/io/string-input-stream;1"].
                       createInstance(Ci.nsIStringInputStream);
      var binaryData = atob(aEntry.owner_b64);
      ownerInput.setData(binaryData, binaryData.length);
      var binaryStream = Cc["@mozilla.org/binaryinputstream;1"].
                         createInstance(Ci.nsIObjectInputStream);
      binaryStream.setInputStream(ownerInput);
      try { // Catch possible deserialization exceptions
        shEntry.owner = binaryStream.readObject(true);
      } catch (ex) { debug(ex); }
    }

    if (aEntry.children && shEntry instanceof Ci.nsISHContainer) {
      for (var i = 0; i < aEntry.children.length; i++) {
        //XXXzpao Wallpaper patch for bug 514751
        if (!aEntry.children[i].url)
          continue;

        // We're getting sessionrestore.js files with a cycle in the
        // doc-identifier graph, likely due to bug 698656.  (That is, we have
        // an entry where doc identifier A is an ancestor of doc identifier B,
        // and another entry where doc identifier B is an ancestor of A.)
        //
        // If we were to respect these doc identifiers, we'd create a cycle in
        // the SHEntries themselves, which causes the docshell to loop forever
        // when it looks for the root SHEntry.
        //
        // So as a hack to fix this, we restrict the scope of a doc identifier
        // to be a node's siblings and cousins, and pass childDocIdents, not
        // aDocIdents, to _deserializeHistoryEntry.  That is, we say that two
        // SHEntries with the same doc identifier have the same document iff
        // they have the same parent or their parents have the same document.

        shEntry.AddChild(this._deserializeHistoryEntry(aEntry.children[i], aIdMap,
                                                       childDocIdents), i);
      }
    }

    return shEntry;
  },

  /**
   * Restore properties to a loaded document
   */
  restoreDocument: function ssi_restoreDocument(aWindow, aBrowser, aEvent) {
    // wait for the top frame to be loaded completely
    if (!aEvent || !aEvent.originalTarget || !aEvent.originalTarget.defaultView ||
        aEvent.originalTarget.defaultView != aEvent.originalTarget.defaultView.top) {
      return;
    }

    // always call this before injecting content into a document!
    function hasExpectedURL(aDocument, aURL)
      !aURL || aURL.replace(/#.*/, "") == aDocument.location.href.replace(/#.*/, "");

    let selectedPageStyle = aBrowser.__SS_restore_pageStyle;
    function restoreTextDataAndScrolling(aContent, aData, aPrefix) {
      if (aData.formdata && hasExpectedURL(aContent.document, aData.url)) {
        let formdata = aData.formdata;

        // handle backwards compatibility
        // this is a migration from pre-firefox 15. cf. bug 742051
        if (!("xpath" in formdata || "id" in formdata)) {
          formdata = { xpath: {}, id: {} };

          for each (let [key, value] in Iterator(aData.formdata)) {
            if (key.charAt(0) == "#") {
              formdata.id[key.slice(1)] = value;
            } else {
              formdata.xpath[key] = value;
            }
          }
        }

        // for about:sessionrestore we saved the field as JSON to avoid
        // nested instances causing humongous sessionstore.js files.
        // cf. bug 467409
        if ((aData.url == "about:sessionrestore" || aData.url == "about:welcomeback") &&
            "sessionData" in formdata.id &&
            typeof formdata.id["sessionData"] == "object") {
          formdata.id["sessionData"] =
            JSON.stringify(formdata.id["sessionData"]);
        }

        // update the formdata
        aData.formdata = formdata;
        // merge the formdata
        DocumentUtils.mergeFormData(aContent.document, formdata);
      }

      if (aData.innerHTML) {
        aWindow.setTimeout(function() {
          if (aContent.document.designMode == "on" &&
              hasExpectedURL(aContent.document, aData.url) &&
              aContent.document.body) {
            aContent.document.body.innerHTML = aData.innerHTML;
          }
        }, 0);
      }
      var match;
      if (aData.scroll && (match = /(\d+),(\d+)/.exec(aData.scroll)) != null) {
        aContent.scrollTo(match[1], match[2]);
      }
      Array.forEach(aContent.document.styleSheets, function(aSS) {
        aSS.disabled = aSS.title && aSS.title != selectedPageStyle;
      });
      for (var i = 0; i < aContent.frames.length; i++) {
        if (aData.children && aData.children[i] &&
          hasExpectedURL(aContent.document, aData.url)) {
          restoreTextDataAndScrolling(aContent.frames[i], aData.children[i], aPrefix + i + "|");
        }
      }
    }

    // don't restore text data and scrolling state if the user has navigated
    // away before the loading completed (except for in-page navigation)
    if (hasExpectedURL(aEvent.originalTarget, aBrowser.__SS_restore_data.url)) {
      var content = aEvent.originalTarget.defaultView;
      restoreTextDataAndScrolling(content, aBrowser.__SS_restore_data, "");
      aBrowser.markupDocumentViewer.authorStyleDisabled = selectedPageStyle == "_nostyle";
    }

    // notify the tabbrowser that this document has been completely restored
    this._sendTabRestoredNotification(aBrowser.__SS_restore_tab);

    delete aBrowser.__SS_restore_data;
    delete aBrowser.__SS_restore_pageStyle;
    delete aBrowser.__SS_restore_tab;
  },

  /**
   * Restore visibility and dimension features to a window
   * @param aWindow
   *        Window reference
   * @param aWinData
   *        Object containing session data for the window
   */
  restoreWindowFeatures: function ssi_restoreWindowFeatures(aWindow, aWinData) {
    var hidden = (aWinData.hidden)?aWinData.hidden.split(","):[];
    WINDOW_HIDEABLE_FEATURES.forEach(function(aItem) {
      aWindow[aItem].visible = hidden.indexOf(aItem) == -1;
    });

    if (aWinData.isPopup) {
      this._windows[aWindow.__SSi].isPopup = true;
      if (aWindow.gURLBar) {
        aWindow.gURLBar.readOnly = true;
        aWindow.gURLBar.setAttribute("enablehistory", "false");
      }
    }
    else {
      delete this._windows[aWindow.__SSi].isPopup;
      if (aWindow.gURLBar) {
        aWindow.gURLBar.readOnly = false;
        aWindow.gURLBar.setAttribute("enablehistory", "true");
      }
    }

    var _this = this;
    aWindow.setTimeout(function() {
      _this.restoreDimensions.apply(_this, [aWindow,
        +aWinData.width || 0,
        +aWinData.height || 0,
        "screenX" in aWinData ? +aWinData.screenX : NaN,
        "screenY" in aWinData ? +aWinData.screenY : NaN,
        aWinData.sizemode || "", aWinData.sidebar || ""]);
    }, 0);
  },

  /**
   * Restore a window's dimensions
   * @param aWidth
   *        Window width
   * @param aHeight
   *        Window height
   * @param aLeft
   *        Window left
   * @param aTop
   *        Window top
   * @param aSizeMode
   *        Window size mode (eg: maximized)
   * @param aSidebar
   *        Sidebar command
   */
  restoreDimensions: function ssi_restoreDimensions(aWindow, aWidth, aHeight, aLeft, aTop, aSizeMode, aSidebar) {
    var win = aWindow;
    var _this = this;
    function win_(aName) { return _this._getWindowDimension(win, aName); }

    // find available space on the screen where this window is being placed
    let screen = gScreenManager.screenForRect(aLeft, aTop, aWidth, aHeight);
    if (screen) {
      let screenLeft = {}, screenTop = {}, screenWidth = {}, screenHeight = {};
      screen.GetAvailRectDisplayPix(screenLeft, screenTop, screenWidth, screenHeight);
      // constrain the dimensions to the actual space available
      if (aWidth > screenWidth.value) {
        aWidth = screenWidth.value;
      }
      if (aHeight > screenHeight.value) {
        aHeight = screenHeight.value;
      }
      // and then pull the window within the screen's bounds
      if (aLeft < screenLeft.value) {
        aLeft = screenLeft.value;
      } else if (aLeft + aWidth > screenLeft.value + screenWidth.value) {
        aLeft = screenLeft.value + screenWidth.value - aWidth;
      }
      if (aTop < screenTop.value) {
        aTop = screenTop.value;
      } else if (aTop + aHeight > screenTop.value + screenHeight.value) {
        aTop = screenTop.value + screenHeight.value - aHeight;
      }
    }

    // only modify those aspects which aren't correct yet
    if (aWidth && aHeight && (aWidth != win_("width") || aHeight != win_("height"))) {
      aWindow.resizeTo(aWidth, aHeight);
    }
    if (!isNaN(aLeft) && !isNaN(aTop) && (aLeft != win_("screenX") || aTop != win_("screenY"))) {
      aWindow.moveTo(aLeft, aTop);
    }
    if (aSizeMode && win_("sizemode") != aSizeMode)
    {
      switch (aSizeMode)
      {
      case "maximized":
        aWindow.maximize();
        break;
      case "minimized":
        aWindow.minimize();
        break;
      case "normal":
        aWindow.restore();
        break;
      }
    }
    var sidebar = aWindow.document.getElementById("sidebar-box");
    if (sidebar.getAttribute("sidebarcommand") != aSidebar) {
      aWindow.toggleSidebar(aSidebar);
    }
    // since resizing/moving a window brings it to the foreground,
    // we might want to re-focus the last focused window
    if (this.windowToFocus) {
      this.windowToFocus.focus();
    }
  },

  /**
   * Restores cookies
   * @param aCookies
   *        Array of cookie objects
   */
  restoreCookies: function ssi_restoreCookies(aCookies) {
    // MAX_EXPIRY should be 2^63-1, but JavaScript can't handle that precision
    var MAX_EXPIRY = Math.pow(2, 62);
    for (let i = 0; i < aCookies.length; i++) {
      var cookie = aCookies[i];
      try {
        Services.cookies.add(cookie.host, cookie.path || "", cookie.name || "",
                             cookie.value, !!cookie.secure, !!cookie.httponly, true,
                             "expiry" in cookie ? cookie.expiry : MAX_EXPIRY);
      }
      catch (ex) { Cu.reportError(ex); } // don't let a single cookie stop recovering
    }
  },

  /* ........ Disk Access .............. */

  /**
   * Save the current session state to disk, after a delay.
   *
   * @param aWindow (optional)
   *        Will mark the given window as dirty so that we will recollect its
   *        data before we start writing.
   */
  saveStateDelayed: function (aWindow = null) {
    if (aWindow) {
      DirtyWindows.add(aWindow);
    }

    SessionSaver.runDelayed();
  },

  /* ........ Auxiliary Functions .............. */

  /**
   * Update the session start time and send a telemetry measurement
   * for the number of days elapsed since the session was started.
   *
   * @param state
   *        The session state.
   */
  _updateSessionStartTime: function ssi_updateSessionStartTime(state) {
    // Attempt to load the session start time from the session state
    if (state.session && state.session.startTime) {
      this._sessionStartTime = state.session.startTime;

      // ms to days
      let sessionLength = (Date.now() - this._sessionStartTime) / MS_PER_DAY;

      if (sessionLength > 0) {
        // Submit the session length telemetry measurement
        Services.telemetry.getHistogramById("FX_SESSION_RESTORE_SESSION_LENGTH").add(sessionLength);
      }
    }
  },

  /**
   * call a callback for all currently opened browser windows
   * (might miss the most recent one)
   * @param aFunc
   *        Callback each window is passed to
   */
  _forEachBrowserWindow: function ssi_forEachBrowserWindow(aFunc) {
    var windowsEnum = Services.wm.getEnumerator("navigator:browser");

    while (windowsEnum.hasMoreElements()) {
      var window = windowsEnum.getNext();
      if (window.__SSi && !window.closed) {
        aFunc.call(this, window);
      }
    }
  },

  /**
   * Returns most recent window
   * @returns Window reference
   */
  _getMostRecentBrowserWindow: function ssi_getMostRecentBrowserWindow() {
    var win = Services.wm.getMostRecentWindow("navigator:browser");
    if (!win)
      return null;
    if (!win.closed)
      return win;

#ifdef BROKEN_WM_Z_ORDER
    win = null;
    var windowsEnum = Services.wm.getEnumerator("navigator:browser");
    // this is oldest to newest, so this gets a bit ugly
    while (windowsEnum.hasMoreElements()) {
      let nextWin = windowsEnum.getNext();
      if (!nextWin.closed)
        win = nextWin;
    }
    return win;
#else
    var windowsEnum =
      Services.wm.getZOrderDOMWindowEnumerator("navigator:browser", true);
    while (windowsEnum.hasMoreElements()) {
      win = windowsEnum.getNext();
      if (!win.closed)
        return win;
    }
    return null;
#endif
  },

  /**
   * Calls onClose for windows that are determined to be closed but aren't
   * destroyed yet, which would otherwise cause getBrowserState and
   * setBrowserState to treat them as open windows.
   */
  _handleClosedWindows: function ssi_handleClosedWindows() {
    var windowsEnum = Services.wm.getEnumerator("navigator:browser");

    while (windowsEnum.hasMoreElements()) {
      var window = windowsEnum.getNext();
      if (window.closed) {
        this.onClose(window);
      }
    }
  },

  /**
   * open a new browser window for a given session state
   * called when restoring a multi-window session
   * @param aState
   *        Object containing session data
   */
  _openWindowWithState: function ssi_openWindowWithState(aState) {
    var argString = Cc["@mozilla.org/supports-string;1"].
                    createInstance(Ci.nsISupportsString);
    argString.data = "";

    // Build feature string
    let features = "chrome,dialog=no,macsuppressanimation,all";
    let winState = aState.windows[0];
    WINDOW_ATTRIBUTES.forEach(function(aFeature) {
      // Use !isNaN as an easy way to ignore sizemode and check for numbers
      if (aFeature in winState && !isNaN(winState[aFeature]))
        features += "," + aFeature + "=" + winState[aFeature];
    });

    if (winState.isPrivate) {
      features += ",private";
    }

    var window =
      Services.ww.openWindow(null, this._prefBranch.getCharPref("chromeURL"),
                             "_blank", features, argString);

    do {
      var ID = "window" + Math.random();
    } while (ID in this._statesToRestore);
    this._statesToRestore[(window.__SS_restoreID = ID)] = aState;

    return window;
  },

  /**
   * Gets the tab for the given browser. This should be marginally better
   * than using tabbrowser's getTabForContentWindow. This assumes the browser
   * is the linkedBrowser of a tab, not a dangling browser.
   *
   * @param aBrowser
   *        The browser from which to get the tab.
   */
  _getTabForBrowser: function ssi_getTabForBrowser(aBrowser) {
    let window = aBrowser.ownerDocument.defaultView;
    for (let i = 0; i < window.gBrowser.tabs.length; i++) {
      let tab = window.gBrowser.tabs[i];
      if (tab.linkedBrowser == aBrowser)
        return tab;
    }
    return undefined;
  },

  /**
   * Whether or not to resume session, if not recovering from a crash.
   * @returns bool
   */
  _doResumeSession: function ssi_doResumeSession() {
    return this._prefBranch.getIntPref("startup.page") == 3 ||
           this._prefBranch.getBoolPref("sessionstore.resume_session_once");
  },

  /**
   * whether the user wants to load any other page at startup
   * (except the homepage) - needed for determining whether to overwrite the current tabs
   * C.f.: nsBrowserContentHandler's defaultArgs implementation.
   * @returns bool
   */
  _isCmdLineEmpty: function ssi_isCmdLineEmpty(aWindow, aState) {
    var pinnedOnly = aState.windows &&
                     aState.windows.every(function (win)
                       win.tabs.every(function (tab) tab.pinned));

    let hasFirstArgument = aWindow.arguments && aWindow.arguments[0];
    if (!pinnedOnly) {
      let defaultArgs = Cc["@mozilla.org/browser/clh;1"].
                        getService(Ci.nsIBrowserHandler).defaultArgs;
      if (aWindow.arguments &&
          aWindow.arguments[0] &&
          aWindow.arguments[0] == defaultArgs)
        hasFirstArgument = false;
    }

    return !hasFirstArgument;
  },

  /**
   * on popup windows, the XULWindow's attributes seem not to be set correctly
   * we use thus JSDOMWindow attributes for sizemode and normal window attributes
   * (and hope for reasonable values when maximized/minimized - since then
   * outerWidth/outerHeight aren't the dimensions of the restored window)
   * @param aWindow
   *        Window reference
   * @param aAttribute
   *        String sizemode | width | height | other window attribute
   * @returns string
   */
  _getWindowDimension: function ssi_getWindowDimension(aWindow, aAttribute) {
    if (aAttribute == "sizemode") {
      switch (aWindow.windowState) {
      case aWindow.STATE_FULLSCREEN:
      case aWindow.STATE_MAXIMIZED:
        return "maximized";
      case aWindow.STATE_MINIMIZED:
        return "minimized";
      default:
        return "normal";
      }
    }

    var dimension;
    switch (aAttribute) {
    case "width":
      dimension = aWindow.outerWidth;
      break;
    case "height":
      dimension = aWindow.outerHeight;
      break;
    default:
      dimension = aAttribute in aWindow ? aWindow[aAttribute] : "";
      break;
    }

    if (aWindow.windowState == aWindow.STATE_NORMAL) {
      return dimension;
    }
    return aWindow.document.documentElement.getAttribute(aAttribute) || dimension;
  },

  /**
   * Get nsIURI from string
   * @param string
   * @returns nsIURI
   */
  _getURIFromString: function ssi_getURIFromString(aString) {
    return Services.io.newURI(aString, null, null);
  },

  /**
   * Annotate a breakpad crash report with the currently selected tab's URL.
   */
  _updateCrashReportURL: function ssi_updateCrashReportURL(aWindow) {
#ifdef MOZ_CRASHREPORTER
    try {
      var currentURI = aWindow.gBrowser.currentURI.clone();
      // if the current URI contains a username/password, remove it
      try {
        currentURI.userPass = "";
      }
      catch (ex) { } // ignore failures on about: URIs

      CrashReporter.annotateCrashReport("URL", currentURI.spec);
    }
    catch (ex) {
      // don't make noise when crashreporter is built but not enabled
      if (ex.result != Components.results.NS_ERROR_NOT_INITIALIZED)
        debug(ex);
    }
#endif
  },

  /**
   * @param aState is a session state
   * @param aRecentCrashes is the number of consecutive crashes
   * @returns whether a restore page will be needed for the session state
   */
  _needsRestorePage: function ssi_needsRestorePage(aState, aRecentCrashes) {
    const SIX_HOURS_IN_MS = 6 * 60 * 60 * 1000;

    // don't display the page when there's nothing to restore
    let winData = aState.windows || null;
    if (!winData || winData.length == 0)
      return false;

    // don't wrap a single about:sessionrestore page
    if (this._hasSingleTabWithURL(winData, "about:sessionrestore") ||
        this._hasSingleTabWithURL(winData, "about:welcomeback")) {
      return false;
    }

    // don't automatically restore in Safe Mode
    if (Services.appinfo.inSafeMode)
      return true;

    let max_resumed_crashes =
      this._prefBranch.getIntPref("sessionstore.max_resumed_crashes");
    let sessionAge = aState.session && aState.session.lastUpdate &&
                     (Date.now() - aState.session.lastUpdate);

    return max_resumed_crashes != -1 &&
           (aRecentCrashes > max_resumed_crashes ||
            sessionAge && sessionAge >= SIX_HOURS_IN_MS);
  },

  /**
   * @param aWinData is the set of windows in session state
   * @param aURL is the single URL we're looking for
   * @returns whether the window data contains only the single URL passed
   */
  _hasSingleTabWithURL: function(aWinData, aURL) {
    if (aWinData &&
        aWinData.length == 1 &&
        aWinData[0].tabs &&
        aWinData[0].tabs.length == 1 &&
        aWinData[0].tabs[0].entries &&
        aWinData[0].tabs[0].entries.length == 1) {
      return aURL == aWinData[0].tabs[0].entries[0].url;
    }
    return false;
  },

  /**
   * Determine if the tab state we're passed is something we should save. This
   * is used when closing a tab or closing a window with a single tab
   *
   * @param aTabState
   *        The current tab state
   * @returns boolean
   */
  _shouldSaveTabState: function ssi_shouldSaveTabState(aTabState) {
    // If the tab has only a transient about: history entry, no other
    // session history, and no userTypedValue, then we don't actually want to
    // store this tab's data.
    return aTabState.entries.length &&
           !(aTabState.entries.length == 1 &&
                (aTabState.entries[0].url == "about:blank" ||
                 aTabState.entries[0].url == "about:newtab") &&
                 !aTabState.userTypedValue);
  },

  /**
   * Determine if we can restore history into this tab.
   * This will be false when a tab has been removed (usually between
   * restoreHistoryPrecursor && restoreHistory) or if the tab is still marked
   * as loading.
   *
   * @param aTab
   * @returns boolean
   */
  _canRestoreTabHistory: function ssi_canRestoreTabHistory(aTab) {
    return aTab.parentNode && aTab.linkedBrowser &&
           aTab.linkedBrowser.__SS_tabStillLoading;
  },

  /**
   * This is going to take a state as provided at startup (via
   * nsISessionStartup.state) and split it into 2 parts. The first part
   * (defaultState) will be a state that should still be restored at startup,
   * while the second part (state) is a state that should be saved for later.
   * defaultState will be comprised of windows with only pinned tabs, extracted
   * from state. It will contain the cookies that go along with the history
   * entries in those tabs. It will also contain window position information.
   *
   * defaultState will be restored at startup. state will be placed into
   * this._lastSessionState and will be kept in case the user explicitly wants
   * to restore the previous session (publicly exposed as restoreLastSession).
   *
   * @param state
   *        The state, presumably from nsISessionStartup.state
   * @returns [defaultState, state]
   */
  _prepDataForDeferredRestore: function ssi_prepDataForDeferredRestore(state) {
    let defaultState = { windows: [], selectedWindow: 1 };

    state.selectedWindow = state.selectedWindow || 1;

    // Look at each window, remove pinned tabs, adjust selectedindex,
    // remove window if necessary.
    for (let wIndex = 0; wIndex < state.windows.length;) {
      let window = state.windows[wIndex];
      window.selected = window.selected || 1;
      // We're going to put the state of the window into this object
      let pinnedWindowState = { tabs: [], cookies: []};
      for (let tIndex = 0; tIndex < window.tabs.length;) {
        if (window.tabs[tIndex].pinned) {
          // Adjust window.selected
          if (tIndex + 1 < window.selected)
            window.selected -= 1;
          else if (tIndex + 1 == window.selected)
            pinnedWindowState.selected = pinnedWindowState.tabs.length + 2;
            // + 2 because the tab isn't actually in the array yet

          // Now add the pinned tab to our window
          pinnedWindowState.tabs =
            pinnedWindowState.tabs.concat(window.tabs.splice(tIndex, 1));
          // We don't want to increment tIndex here.
          continue;
        }
        tIndex++;
      }

      // At this point the window in the state object has been modified (or not)
      // We want to build the rest of this new window object if we have pinnedTabs.
      if (pinnedWindowState.tabs.length) {
        // First get the other attributes off the window
        WINDOW_ATTRIBUTES.forEach(function(attr) {
          if (attr in window) {
            pinnedWindowState[attr] = window[attr];
            delete window[attr];
          }
        });
        // We're just copying position data into the pinned window.
        // Not copying over:
        // - _closedTabs
        // - extData
        // - isPopup
        // - hidden

        // Assign a unique ID to correlate the window to be opened with the
        // remaining data
        window.__lastSessionWindowID = pinnedWindowState.__lastSessionWindowID
                                     = "" + Date.now() + Math.random();

        // Extract the cookies that belong with each pinned tab
        this._splitCookiesFromWindow(window, pinnedWindowState);

        // Actually add this window to our defaultState
        defaultState.windows.push(pinnedWindowState);
        // Remove the window from the state if it doesn't have any tabs
        if (!window.tabs.length) {
          if (wIndex + 1 <= state.selectedWindow)
            state.selectedWindow -= 1;
          else if (wIndex + 1 == state.selectedWindow)
            defaultState.selectedIndex = defaultState.windows.length + 1;

          state.windows.splice(wIndex, 1);
          // We don't want to increment wIndex here.
          continue;
        }


      }
      wIndex++;
    }

    return [defaultState, state];
  },

  /**
   * Splits out the cookies from aWinState into aTargetWinState based on the
   * tabs that are in aTargetWinState.
   * This alters the state of aWinState and aTargetWinState.
   */
  _splitCookiesFromWindow:
    function ssi_splitCookiesFromWindow(aWinState, aTargetWinState) {
    if (!aWinState.cookies || !aWinState.cookies.length)
      return;

    // Get the hosts for history entries in aTargetWinState
    let cookieHosts = SessionCookies.getHostsForWindow(aTargetWinState);

    // By creating a regex we reduce overhead and there is only one loop pass
    // through either array (cookieHosts and aWinState.cookies).
    let hosts = Object.keys(cookieHosts).join("|").replace("\\.", "\\.", "g");
    // If we don't actually have any hosts, then we don't want to do anything.
    if (!hosts.length)
      return;
    let cookieRegex = new RegExp(".*(" + hosts + ")");
    for (let cIndex = 0; cIndex < aWinState.cookies.length;) {
      if (cookieRegex.test(aWinState.cookies[cIndex].host)) {
        aTargetWinState.cookies =
          aTargetWinState.cookies.concat(aWinState.cookies.splice(cIndex, 1));
        continue;
      }
      cIndex++;
    }
  },

  /**
   * Converts a JavaScript object into a JSON string
   * (see http://www.json.org/ for more information).
   *
   * The inverse operation consists of JSON.parse(JSON_string).
   *
   * @param aJSObject is the object to be converted
   * @returns the object's JSON representation
   */
  _toJSONString: function ssi_toJSONString(aJSObject) {
    return JSON.stringify(aJSObject);
  },

  _sendRestoreCompletedNotifications: function ssi_sendRestoreCompletedNotifications() {
    // not all windows restored, yet
    if (this._restoreCount > 1) {
      this._restoreCount--;
      return;
    }

    // observers were already notified
    if (this._restoreCount == -1)
      return;

    // This was the last window restored at startup, notify observers.
    Services.obs.notifyObservers(null,
      this._browserSetState ? NOTIFY_BROWSER_STATE_RESTORED : NOTIFY_WINDOWS_RESTORED,
      "");

    this._browserSetState = false;
    this._restoreCount = -1;
  },

   /**
   * Set the given window's busy state
   * @param aWindow the window
   * @param aValue the window's busy state
   */
  _setWindowStateBusyValue:
    function ssi_changeWindowStateBusyValue(aWindow, aValue) {

    this._windows[aWindow.__SSi].busy = aValue;

    // Keep the to-be-restored state in sync because that is returned by
    // getWindowState() as long as the window isn't loaded, yet.
    if (!this._isWindowLoaded(aWindow)) {
      let stateToRestore = this._statesToRestore[aWindow.__SS_restoreID].windows[0];
      stateToRestore.busy = aValue;
    }
  },

  /**
   * Set the given window's state to 'not busy'.
   * @param aWindow the window
   */
  _setWindowStateReady: function ssi_setWindowStateReady(aWindow) {
    this._setWindowStateBusyValue(aWindow, false);
    this._sendWindowStateEvent(aWindow, "Ready");
  },

  /**
   * Set the given window's state to 'busy'.
   * @param aWindow the window
   */
  _setWindowStateBusy: function ssi_setWindowStateBusy(aWindow) {
    this._setWindowStateBusyValue(aWindow, true);
    this._sendWindowStateEvent(aWindow, "Busy");
  },

  /**
   * Dispatch an SSWindowState_____ event for the given window.
   * @param aWindow the window
   * @param aType the type of event, SSWindowState will be prepended to this string
   */
  _sendWindowStateEvent: function ssi_sendWindowStateEvent(aWindow, aType) {
    let event = aWindow.document.createEvent("Events");
    event.initEvent("SSWindowState" + aType, true, false);
    aWindow.dispatchEvent(event);
  },

  /**
   * Dispatch the SSTabRestored event for the given tab.
   * @param aTab the which has been restored
   */
  _sendTabRestoredNotification: function ssi_sendTabRestoredNotification(aTab) {
      let event = aTab.ownerDocument.createEvent("Events");
      event.initEvent("SSTabRestored", true, false);
      aTab.dispatchEvent(event);
  },

  /**
   * @param aWindow
   *        Window reference
   * @returns whether this window's data is still cached in _statesToRestore
   *          because it's not fully loaded yet
   */
  _isWindowLoaded: function ssi_isWindowLoaded(aWindow) {
    return !aWindow.__SS_restoreID;
  },

  /**
   * Replace "Loading..." with the tab label (with minimal side-effects)
   * @param aString is the string the title is stored in
   * @param aTabbrowser is a tabbrowser object, containing aTab
   * @param aTab is the tab whose title we're updating & using
   *
   * @returns aString that has been updated with the new title
   */
  _replaceLoadingTitle : function ssi_replaceLoadingTitle(aString, aTabbrowser, aTab) {
    if (aString == aTabbrowser.mStringBundle.getString("tabs.connecting")) {
      aTabbrowser.setTabTitle(aTab);
      [aString, aTab.label] = [aTab.label, aString];
    }
    return aString;
  },

  /**
   * Resize this._closedWindows to the value of the pref, except in the case
   * where we don't have any non-popup windows on Windows and Linux. Then we must
   * resize such that we have at least one non-popup window.
   */
  _capClosedWindows : function ssi_capClosedWindows() {
    if (this._closedWindows.length <= this._max_windows_undo)
      return;
    let spliceTo = this._max_windows_undo;
#ifndef XP_MACOSX
    let normalWindowIndex = 0;
    // try to find a non-popup window in this._closedWindows
    while (normalWindowIndex < this._closedWindows.length &&
           !!this._closedWindows[normalWindowIndex].isPopup)
      normalWindowIndex++;
    if (normalWindowIndex >= this._max_windows_undo)
      spliceTo = normalWindowIndex + 1;
#endif
    this._closedWindows.splice(spliceTo, this._closedWindows.length);
  },

  _clearRestoringWindows: function ssi_clearRestoringWindows() {
    for (let i = 0; i < this._closedWindows.length; i++) {
      delete this._closedWindows[i]._shouldRestore;
    }
  },

  /**
   * Reset state to prepare for a new session state to be restored.
   */
  _resetRestoringState: function ssi_initRestoringState() {
    TabRestoreQueue.reset();
    this._tabsRestoringCount = 0;
  },

  /**
   * Reset the restoring state for a particular tab. This will be called when
   * removing a tab or when a tab needs to be reset (it's being overwritten).
   *
   * @param aTab
   *        The tab that will be "reset"
   */
  _resetTabRestoringState: function ssi_resetTabRestoringState(aTab) {
    let window = aTab.ownerDocument.defaultView;
    let browser = aTab.linkedBrowser;

    // Keep the tab's previous state for later in this method
    let previousState = browser.__SS_restoreState;

    // The browser is no longer in any sort of restoring state.
    delete browser.__SS_restoreState;

    aTab.removeAttribute("pending");
    browser.removeAttribute("pending");

    // We want to decrement window.__SS_tabsToRestore here so that we always
    // decrement it AFTER a tab is done restoring or when a tab gets "reset".
    window.__SS_tabsToRestore--;

    // Remove the progress listener if we should.
    this._removeTabsProgressListener(window);

    if (previousState == TAB_STATE_RESTORING) {
      if (this._tabsRestoringCount)
        this._tabsRestoringCount--;
    }
    else if (previousState == TAB_STATE_NEEDS_RESTORE) {
      // Make sure the session history listener is removed. This is normally
      // done in restoreTab, but this tab is being removed before that gets called.
      this._removeSHistoryListener(aTab);

      // Make sure that the tab is removed from the list of tabs to restore.
      // Again, this is normally done in restoreTab, but that isn't being called
      // for this tab.
      TabRestoreQueue.remove(aTab);
    }
  },

  /**
   * Add the tabs progress listener to the window if it isn't already
   *
   * @param aWindow
   *        The window to add our progress listener to
   */
  _ensureTabsProgressListener: function ssi_ensureTabsProgressListener(aWindow) {
    let tabbrowser = aWindow.gBrowser;
    if (tabbrowser.mTabsProgressListeners.indexOf(gRestoreTabsProgressListener) == -1)
      tabbrowser.addTabsProgressListener(gRestoreTabsProgressListener);
  },

  /**
   * Attempt to remove the tabs progress listener from the window.
   *
   * @param aWindow
   *        The window from which to remove our progress listener from
   */
  _removeTabsProgressListener: function ssi_removeTabsProgressListener(aWindow) {
    // If there are no tabs left to restore (or restoring) in this window, then
    // we can safely remove the progress listener from this window.
    if (!aWindow.__SS_tabsToRestore)
      aWindow.gBrowser.removeTabsProgressListener(gRestoreTabsProgressListener);
  },

  /**
   * Remove the session history listener from the tab's browser if there is one.
   *
   * @param aTab
   *        The tab who's browser to remove the listener
   */
  _removeSHistoryListener: function ssi_removeSHistoryListener(aTab) {
    let browser = aTab.linkedBrowser;
    if (browser.__SS_shistoryListener) {
      browser.webNavigation.sessionHistory.
                            removeSHistoryListener(browser.__SS_shistoryListener);
      delete browser.__SS_shistoryListener;
    }
  }
};

/**
 * Priority queue that keeps track of a list of tabs to restore and returns
 * the tab we should restore next, based on priority rules. We decide between
 * pinned, visible and hidden tabs in that and FIFO order. Hidden tabs are only
 * restored with restore_hidden_tabs=true.
 */
let TabRestoreQueue = {
  // The separate buckets used to store tabs.
  tabs: {priority: [], visible: [], hidden: []},

  // Preferences used by the TabRestoreQueue to determine which tabs
  // are restored automatically and which tabs will be on-demand.
  prefs: {
    // Lazy getter that returns whether tabs are restored on demand.
    get restoreOnDemand() {
      let updateValue = () => {
        let value = Services.prefs.getBoolPref(PREF);
        let definition = {value: value, configurable: true};
        Object.defineProperty(this, "restoreOnDemand", definition);
        return value;
      }

      const PREF = "browser.sessionstore.restore_on_demand";
      Services.prefs.addObserver(PREF, updateValue, false);
      return updateValue();
    },

    // Lazy getter that returns whether pinned tabs are restored on demand.
    get restorePinnedTabsOnDemand() {
      let updateValue = () => {
        let value = Services.prefs.getBoolPref(PREF);
        let definition = {value: value, configurable: true};
        Object.defineProperty(this, "restorePinnedTabsOnDemand", definition);
        return value;
      }

      const PREF = "browser.sessionstore.restore_pinned_tabs_on_demand";
      Services.prefs.addObserver(PREF, updateValue, false);
      return updateValue();
    },

    // Lazy getter that returns whether we should restore hidden tabs.
    get restoreHiddenTabs() {
      let updateValue = () => {
        let value = Services.prefs.getBoolPref(PREF);
        let definition = {value: value, configurable: true};
        Object.defineProperty(this, "restoreHiddenTabs", definition);
        return value;
      }

      const PREF = "browser.sessionstore.restore_hidden_tabs";
      Services.prefs.addObserver(PREF, updateValue, false);
      return updateValue();
    }
  },

  // Resets the queue and removes all tabs.
  reset: function () {
    this.tabs = {priority: [], visible: [], hidden: []};
  },

  // Adds a tab to the queue and determines its priority bucket.
  add: function (tab) {
    let {priority, hidden, visible} = this.tabs;

    if (tab.pinned) {
      priority.push(tab);
    } else if (tab.hidden) {
      hidden.push(tab);
    } else {
      visible.push(tab);
    }
  },

  // Removes a given tab from the queue, if it's in there.
  remove: function (tab) {
    let {priority, hidden, visible} = this.tabs;

    // We'll always check priority first since we don't
    // have an indicator if a tab will be there or not.
    let set = priority;
    let index = set.indexOf(tab);

    if (index == -1) {
      set = tab.hidden ? hidden : visible;
      index = set.indexOf(tab);
    }

    if (index > -1) {
      set.splice(index, 1);
    }
  },

  // Returns and removes the tab with the highest priority.
  shift: function () {
    let set;
    let {priority, hidden, visible} = this.tabs;

    let {restoreOnDemand, restorePinnedTabsOnDemand} = this.prefs;
    let restorePinned = !(restoreOnDemand && restorePinnedTabsOnDemand);
    if (restorePinned && priority.length) {
      set = priority;
    } else if (!restoreOnDemand) {
      if (visible.length) {
        set = visible;
      } else if (this.prefs.restoreHiddenTabs && hidden.length) {
        set = hidden;
      }
    }

    return set && set.shift();
  },

  // Moves a given tab from the 'hidden' to the 'visible' bucket.
  hiddenToVisible: function (tab) {
    let {hidden, visible} = this.tabs;
    let index = hidden.indexOf(tab);

    if (index > -1) {
      hidden.splice(index, 1);
      visible.push(tab);
    } else {
      throw new Error("restore queue: hidden tab not found");
    }
  },

  // Moves a given tab from the 'visible' to the 'hidden' bucket.
  visibleToHidden: function (tab) {
    let {visible, hidden} = this.tabs;
    let index = visible.indexOf(tab);

    if (index > -1) {
      visible.splice(index, 1);
      hidden.push(tab);
    } else {
      throw new Error("restore queue: visible tab not found");
    }
  }
};

// A map storing a closed window's state data until it goes aways (is GC'ed).
// This ensures that API clients can still read (but not write) states of
// windows they still hold a reference to but we don't.
let DyingWindowCache = {
  _data: new WeakMap(),

  has: function (window) {
    return this._data.has(window);
  },

  get: function (window) {
    return this._data.get(window);
  },

  set: function (window, data) {
    this._data.set(window, data);
  },

  remove: function (window) {
    this._data.delete(window);
  }
};

// A weak set of dirty windows. We use it to determine which windows we need to
// recollect data for when getCurrentState() is called.
let DirtyWindows = {
  _data: new WeakMap(),

  has: function (window) {
    return this._data.has(window);
  },

  add: function (window) {
    return this._data.set(window, true);
  },

  remove: function (window) {
    this._data.delete(window);
  },

  clear: function (window) {
    this._data.clear();
  }
};

// A map storing the number of tabs last closed per windoow. This only
// stores the most recent tab-close operation, and is used to undo
// batch tab-closing operations.
let NumberOfTabsClosedLastPerWindow = new WeakMap();

// A set of tab attributes to persist. We will read a given list of tab
// attributes when collecting tab data and will re-set those attributes when
// the given tab data is restored to a new tab.
let TabAttributes = {
  _attrs: new Set(),

  // We never want to directly read or write those attributes.
  // 'image' should not be accessed directly but handled by using the
  //         gBrowser.getIcon()/setIcon() methods.
  // 'pending' is used internal by sessionstore and managed accordingly.
  _skipAttrs: new Set(["image", "pending"]),

  persist: function (name) {
    if (this._attrs.has(name) || this._skipAttrs.has(name)) {
      return false;
    }

    this._attrs.add(name);
    return true;
  },

  get: function (tab) {
    let data = {};

    for (let name of this._attrs) {
      if (tab.hasAttribute(name)) {
        data[name] = tab.getAttribute(name);
      }
    }

    return data;
  },

  set: function (tab, data = {}) {
    // Clear attributes.
    for (let name of this._attrs) {
      tab.removeAttribute(name);
    }

    // Set attributes.
    for (let name in data) {
      tab.setAttribute(name, data[name]);
    }
  }
};

// This is used to help meter the number of restoring tabs. This is the control
// point for telling the next tab to restore. It gets attached to each gBrowser
// via gBrowser.addTabsProgressListener
let gRestoreTabsProgressListener = {
  onStateChange: function(aBrowser, aWebProgress, aRequest, aStateFlags, aStatus) {
    // Ignore state changes on browsers that we've already restored and state
    // changes that aren't applicable.
    if (aBrowser.__SS_restoreState &&
        aBrowser.__SS_restoreState == TAB_STATE_RESTORING &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW) {
      // We need to reset the tab before starting the next restore.
      let tab = SessionStoreInternal._getTabForBrowser(aBrowser);
      SessionStoreInternal._resetTabRestoringState(tab);
      SessionStoreInternal.restoreNextTab();
    }
  }
};

// A SessionStoreSHistoryListener will be attached to each browser before it is
// restored. We need to catch reloads that occur before the tab is restored
// because otherwise, docShell will reload an old URI (usually about:blank).
function SessionStoreSHistoryListener(aTab) {
  this.tab = aTab;
}
SessionStoreSHistoryListener.prototype = {
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsISHistoryListener,
    Ci.nsISupportsWeakReference
  ]),
  browser: null,
// The following events (with the exception of OnHistoryPurge)
// accompany either a "load" or a "pageshow" which will in turn cause
// invalidations.
  OnHistoryNewEntry: function(aNewURI) {

  },
  OnHistoryGoBack: function(aBackURI) {
    return true;
  },
  OnHistoryGoForward: function(aForwardURI) {
    return true;
  },
  OnHistoryGotoIndex: function(aIndex, aGotoURI) {
    return true;
  },
  OnHistoryPurge: function(aNumEntries) {
    TabStateCache.delete(this.tab);
    return true;
  },
  OnHistoryReload: function(aReloadURI, aReloadFlags) {
    // On reload, we want to make sure that session history loads the right
    // URI. In order to do that, we will juet call restoreTab. That will remove
    // the history listener and load the right URI.
    SessionStoreInternal.restoreTab(this.tab);
    // Returning false will stop the load that docshell is attempting.
    return false;
  }
}

// see nsPrivateBrowsingService.js
String.prototype.hasRootDomain = function hasRootDomain(aDomain) {
  let index = this.indexOf(aDomain);
  if (index == -1)
    return false;

  if (this == aDomain)
    return true;

  let prevChar = this[index - 1];
  return (index == (this.length - aDomain.length)) &&
         (prevChar == "." || prevChar == "/");
};

function TabData(obj = null) {
  if (obj) {
    if (obj instanceof TabData) {
      // FIXME: Can we get rid of this?
      return obj;
    }
    for (let [key, value] in Iterator(obj)) {
      this[key] = value;
    }
  }
  return this;
}

/**
 * Module that contains tab state collection methods.
 */
let TabState = {
  // A map (xul:tab -> promise) that keeps track of tabs and
  // their promises when collecting tab data asynchronously.
  _pendingCollections: new WeakMap(),

  /**
   * Collect data related to a single tab, asynchronously.
   *
   * @param tab
   *        tabbrowser tab
   *
   * @returns {Promise} A promise that will resolve to a TabData instance.
   */
  collect: function (tab) {
    if (!tab) {
      throw new TypeError("Expecting a tab");
    }

    // Don't collect if we don't need to.
    if (TabStateCache.has(tab)) {
      return Promise.resolve(TabStateCache.get(tab));
    }

    // If the tab hasn't been restored yet, just return the data we
    // have saved for it.
    let browser = tab.linkedBrowser;
    if (!browser.currentURI || (browser.__SS_data && browser.__SS_tabStillLoading)) {
      let tabData = new TabData(this._collectBaseTabData(tab));
      return Promise.resolve(tabData);
    }

    let promise = Task.spawn(function task() {
      // Collected session history data asynchronously.
      let history = yield Messenger.send(tab, "SessionStore:collectSessionHistory");

      // Collected session storage data asynchronously.
      let storage = yield Messenger.send(tab, "SessionStore:collectSessionStorage");

      // Collect docShell capabilities asynchronously.
      let disallow = yield Messenger.send(tab, "SessionStore:collectDocShellCapabilities");

      // Collect basic tab data, without session history and storage.
      let options = {omitSessionHistory: true,
                     omitSessionStorage: true,
                     omitDocShellCapabilities: true};
      let tabData = this._collectBaseTabData(tab, options);

      // Apply collected data.
      tabData.entries = history.entries;
      tabData.index = history.index;

      if (Object.keys(storage).length) {
        tabData.storage = storage;
      }

      if (disallow.length > 0) {
	tabData.disallow = disallow.join(",");
      }

      // Save text and scroll data.
      this._updateTextAndScrollDataForTab(tab, tabData);

      // If we're still the latest async collection for the given tab and
      // the cache hasn't been filled by collect() in the meantime, let's
      // fill the cache with the data we received.
      if (this._pendingCollections.get(tab) == promise) {
        TabStateCache.set(tab, tabData);
        this._pendingCollections.delete(tab);
      }

      throw new Task.Result(tabData);
    }.bind(this));

    // Save the current promise as the latest asynchronous collection that is
    // running. This will be used to check whether the collected data is still
    // valid and will be used to fill the tab state cache.
    this._pendingCollections.set(tab, promise);

    return promise;
  },

  /**
   * Collect data related to a single tab, synchronously.
   *
   * @param tab
   *        tabbrowser tab
   *
   * @returns {TabData} An object with the data for this tab.  If the
   * tab has not been invalidated since the last call to
   * collectSync(aTab), the same object is returned.
   */
  collectSync: function (tab) {
    if (!tab) {
      throw new TypeError("Expecting a tab");
    }
    if (TabStateCache.has(tab)) {
      return TabStateCache.get(tab);
    }

    let tabData = this._collectBaseTabData(tab);
    if (this._updateTextAndScrollDataForTab(tab, tabData)) {
      TabStateCache.set(tab, tabData);
    }

    // Prevent all running asynchronous collections from filling the cache.
    // Every asynchronous data collection started before a collectSync() call
    // can't expect to retrieve different data than the sync call. That's why
    // we just fill the cache with the data collected from the sync call and
    // discard any data collected asynchronously.
    this._pendingCollections.delete(tab);

    return tabData;
  },

  /**
   * Collect data related to a single tab, including private data.
   * Use with caution.
   *
   * @param tab
   *        tabbrowser tab
   *
   * @returns {object} An object with the data for this tab. This data is never
   *                   cached, it will always be read from the tab and thus be
   *                   up-to-date.
   */
  clone: function (tab) {
    let options = { includePrivateData: true };
    let tabData = this._collectBaseTabData(tab, options);
    this._updateTextAndScrollDataForTab(tab, tabData, options);
    return tabData;
  },

  /**
   * Collects basic tab data for a given tab.
   *
   * @param tab
   *        tabbrowser tab
   * @param options
   *        An object that will be passed to session history and session
   *        storage data collection methods.
   *        {omitSessionHistory: true} to skip collecting session history data
   *        {omitSessionStorage: true} to skip collecting session storage data
   *        {omitDocShellCapabilities: true} to skip collecting docShell allow* attributes
   *
   *        The omit* options have been introduced to enable us collecting
   *        those parts of the tab data asynchronously. We will request basic
   *        tabData without the parts to omit and fill those holes later when
   *        the content script has responded.
   *
   * @returns {object} An object with the basic data for this tab.
   */
  _collectBaseTabData: function (tab, options = {}) {
    let tabData = {entries: [], lastAccessed: tab.lastAccessed };
    let browser = tab.linkedBrowser;
    if (!browser || !browser.currentURI) {
      // can happen when calling this function right after .addTab()
      return tabData;
    }
    if (browser.__SS_data && browser.__SS_tabStillLoading) {
      // use the data to be restored when the tab hasn't been completely loaded
      tabData = browser.__SS_data;
      if (tab.pinned)
        tabData.pinned = true;
      else
        delete tabData.pinned;
      tabData.hidden = tab.hidden;

      // If __SS_extdata is set then we'll use that since it might be newer.
      if (tab.__SS_extdata)
        tabData.extData = tab.__SS_extdata;
      // If it exists but is empty then a key was likely deleted. In that case just
      // delete extData.
      if (tabData.extData && !Object.keys(tabData.extData).length)
        delete tabData.extData;
      return tabData;
    }

    // Collection session history data.
    if (!options || !options.omitSessionHistory) {
      this._collectTabHistory(tab, tabData, options);
    }

    // If there is a userTypedValue set, then either the user has typed something
    // in the URL bar, or a new tab was opened with a URI to load. userTypedClear
    // is used to indicate whether the tab was in some sort of loading state with
    // userTypedValue.
    if (browser.userTypedValue) {
      tabData.userTypedValue = browser.userTypedValue;
      tabData.userTypedClear = browser.userTypedClear;
    } else {
      delete tabData.userTypedValue;
      delete tabData.userTypedClear;
    }

    if (tab.pinned)
      tabData.pinned = true;
    else
      delete tabData.pinned;
    tabData.hidden = tab.hidden;

    if (!options || !options.omitDocShellCapabilities) {
      let disallow = DocShellCapabilities.collect(browser.docShell);
      if (disallow.length > 0)
	tabData.disallow = disallow.join(",");
      else if (tabData.disallow)
	delete tabData.disallow;
    }

    // Save tab attributes.
    tabData.attributes = TabAttributes.get(tab);

    // Store the tab icon.
    let tabbrowser = tab.ownerDocument.defaultView.gBrowser;
    tabData.image = tabbrowser.getIcon(tab);

    if (tab.__SS_extdata)
      tabData.extData = tab.__SS_extdata;
    else if (tabData.extData)
      delete tabData.extData;

    // Collect DOMSessionStorage data.
    if (!options || !options.omitSessionStorage) {
      this._collectTabSessionStorage(tab, tabData, options);
    }

    return tabData;
  },

  /**
   * Collects session history data for a given tab.
   *
   * @param tab
   *        tabbrowser tab
   * @param tabData
   *        An object that the session history data will be added to.
   * @param options
   *        {includePrivateData: true} to always include private data
   */
  _collectTabHistory: function (tab, tabData, options = {}) {
    let includePrivateData = options && options.includePrivateData;
    let docShell = tab.linkedBrowser.docShell;

    if (docShell instanceof Ci.nsIDocShell) {
      let history = SessionHistory.read(docShell, includePrivateData);
      tabData.entries = history.entries;

      // For blank tabs without any history entries,
      // there will not be an 'index' property.
      if ("index" in history) {
        tabData.index = history.index;
      }
    }
  },

  /**
   * Collects session history data for a given tab.
   *
   * @param tab
   *        tabbrowser tab
   * @param tabData
   *        An object that the session storage data will be added to.
   * @param options
   *        {includePrivateData: true} to always include private data
   */
  _collectTabSessionStorage: function (tab, tabData, options = {}) {
    let includePrivateData = options && options.includePrivateData;
    let docShell = tab.linkedBrowser.docShell;

    if (docShell instanceof Ci.nsIDocShell) {
      let storageData = SessionStorage.serialize(docShell, includePrivateData)
      if (Object.keys(storageData).length) {
        tabData.storage = storageData;
      }
    }
  },

  /**
   * Go through all frames and store the current scroll positions
   * and innerHTML content of WYSIWYG editors
   *
   * @param tab
   *        tabbrowser tab
   * @param tabData
   *        tabData object to add the information to
   * @param options
   *        An optional object that may contain the following field:
   *        - includePrivateData: always return privacy sensitive data
   *          (use with care)
   * @return false if data should not be cached because the tab
   *        has not been fully initialized yet.
   */
  _updateTextAndScrollDataForTab: function (tab, tabData, options = null) {
    let includePrivateData = options && options.includePrivateData;
    let window = tab.ownerDocument.defaultView;
    let browser = tab.linkedBrowser;
    // we shouldn't update data for incompletely initialized tabs
    if (!browser.currentURI
        || (browser.__SS_data && browser.__SS_tabStillLoading)) {
      return false;
    }

    let tabIndex = (tabData.index || tabData.entries.length) - 1;
    // entry data needn't exist for tabs just initialized with an incomplete session state
    if (!tabData.entries[tabIndex]) {
      return false;
    }

    let selectedPageStyle = browser.markupDocumentViewer.authorStyleDisabled ? "_nostyle" :
                            this._getSelectedPageStyle(browser.contentWindow);
    if (selectedPageStyle)
      tabData.pageStyle = selectedPageStyle;
    else if (tabData.pageStyle)
      delete tabData.pageStyle;

    this._updateTextAndScrollDataForFrame(window, browser.contentWindow,
                                          tabData.entries[tabIndex],
                                          includePrivateData,
                                          !!tabData.pinned);
    if (browser.currentURI.spec == "about:config") {
      tabData.entries[tabIndex].formdata = {
        id: {
          "textbox": browser.contentDocument.getElementById("textbox").value
        },
        xpath: {}
      };
    }

    return true;
  },

  /**
   * Go through all subframes and store all form data, the current
   * scroll positions and innerHTML content of WYSIWYG editors.
   *
   * @param window
   *        Window reference
   * @param content
   *        frame reference
   * @param data
   *        part of a tabData object to add the information to
   * @param includePrivateData
   *        always return privacy sensitive data (use with care)
   * @param isPinned
   *        the tab is pinned and should be treated differently for privacy
   */
  _updateTextAndScrollDataForFrame:
    function (window, content, data, includePrivateData, isPinned) {

    for (let i = 0; i < content.frames.length; i++) {
      if (data.children && data.children[i])
        this._updateTextAndScrollDataForFrame(window, content.frames[i],
                                              data.children[i],
                                              includePrivateData, isPinned);
    }
    let href = (content.parent || content).document.location.href;
    let isHttps = makeURI(href).schemeIs("https");
    let topURL = content.top.document.location.href;
    let isAboutSR = topURL == "about:sessionrestore" || topURL == "about:welcomeback";
    if (includePrivateData || isAboutSR ||
        PrivacyLevel.canSave({isHttps: isHttps, isPinned: isPinned})) {
      let formData = DocumentUtils.getFormData(content.document);

      // We want to avoid saving data for about:sessionrestore as a string.
      // Since it's stored in the form as stringified JSON, stringifying further
      // causes an explosion of escape characters. cf. bug 467409
      if (formData && isAboutSR) {
        formData.id["sessionData"] = JSON.parse(formData.id["sessionData"]);
      }

      if (Object.keys(formData.id).length ||
          Object.keys(formData.xpath).length) {
        data.formdata = formData;
      } else if (data.formdata) {
        delete data.formdata;
      }

      // designMode is undefined e.g. for XUL documents (as about:config)
      if ((content.document.designMode || "") == "on" && content.document.body)
        data.innerHTML = content.document.body.innerHTML;
    }

    // get scroll position from nsIDOMWindowUtils, since it allows avoiding a
    // flush of layout
    let domWindowUtils = content.QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIDOMWindowUtils);
    let scrollX = {}, scrollY = {};
    domWindowUtils.getScrollXY(false, scrollX, scrollY);
    data.scroll = scrollX.value + "," + scrollY.value;
  },

  /**
   * determine the title of the currently enabled style sheet (if any)
   * and recurse through the frameset if necessary
   * @param   content is a frame reference
   * @returns the title style sheet determined to be enabled (empty string if none)
   */
  _getSelectedPageStyle: function (content) {
    const forScreen = /(?:^|,)\s*(?:all|screen)\s*(?:,|$)/i;
    for (let i = 0; i < content.document.styleSheets.length; i++) {
      let ss = content.document.styleSheets[i];
      let media = ss.media.mediaText;
      if (!ss.disabled && ss.title && (!media || forScreen.test(media)))
        return ss.title
    }
    for (let i = 0; i < content.frames.length; i++) {
      let selectedPageStyle = this._getSelectedPageStyle(content.frames[i]);
      if (selectedPageStyle)
        return selectedPageStyle;
    }
    return "";
  }
};


