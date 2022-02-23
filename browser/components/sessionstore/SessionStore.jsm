/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["SessionStore", "_LastSession"];

// Current version of the format used by Session Restore.
const FORMAT_VERSION = 1;

const TAB_CUSTOM_VALUES = new WeakMap();
const TAB_LAZY_STATES = new WeakMap();
const TAB_STATE_NEEDS_RESTORE = 1;
const TAB_STATE_RESTORING = 2;
const TAB_STATE_FOR_BROWSER = new WeakMap();
const WINDOW_RESTORE_IDS = new WeakMap();
const WINDOW_RESTORE_ZINDICES = new WeakMap();
const WINDOW_SHOWING_PROMISES = new Map();
const WINDOW_FLUSHING_PROMISES = new Map();

// A new window has just been restored. At this stage, tabs are generally
// not restored.
const NOTIFY_SINGLE_WINDOW_RESTORED = "sessionstore-single-window-restored";
const NOTIFY_WINDOWS_RESTORED = "sessionstore-windows-restored";
const NOTIFY_BROWSER_STATE_RESTORED = "sessionstore-browser-state-restored";
const NOTIFY_LAST_SESSION_CLEARED = "sessionstore-last-session-cleared";
const NOTIFY_RESTORING_ON_STARTUP = "sessionstore-restoring-on-startup";
const NOTIFY_INITIATING_MANUAL_RESTORE =
  "sessionstore-initiating-manual-restore";
const NOTIFY_CLOSED_OBJECTS_CHANGED = "sessionstore-closed-objects-changed";

const NOTIFY_TAB_RESTORED = "sessionstore-debug-tab-restored"; // WARNING: debug-only
const NOTIFY_DOMWINDOWCLOSED_HANDLED =
  "sessionstore-debug-domwindowclosed-handled"; // WARNING: debug-only

const NOTIFY_BROWSER_SHUTDOWN_FLUSH = "sessionstore-browser-shutdown-flush";

// Maximum number of tabs to restore simultaneously. Previously controlled by
// the browser.sessionstore.max_concurrent_tabs pref.
const MAX_CONCURRENT_TAB_RESTORES = 3;

// Amount (in CSS px) by which we allow window edges to be off-screen
// when restoring a window, before we override the saved position to
// pull the window back within the available screen area.
const SCREEN_EDGE_SLOP = 8;

// global notifications observed
const OBSERVING = [
  "browser-window-before-show",
  "domwindowclosed",
  "quit-application-granted",
  "browser-lastwindow-close-granted",
  "quit-application",
  "browser:purge-session-history",
  "browser:purge-session-history-for-domain",
  "idle-daily",
  "clear-origin-attributes-data",
  "browsing-context-did-set-embedder",
  "browsing-context-discarded",
  "browser-shutdown-tabstate-updated",
];

// XUL Window properties to (re)store
// Restored in restoreDimensions()
const WINDOW_ATTRIBUTES = ["width", "height", "screenX", "screenY", "sizemode"];

// Hideable window features to (re)store
// Restored in restoreWindowFeatures()
const WINDOW_HIDEABLE_FEATURES = [
  "menubar",
  "toolbar",
  "locationbar",
  "personalbar",
  "statusbar",
  "scrollbars",
];

// Messages that will be received via the Frame Message Manager.
const MESSAGES = [
  // The content script sends us data that has been invalidated and needs to
  // be saved to disk.
  "SessionStore:update",

  // The restoreHistory code has run. This is a good time to run SSTabRestoring.
  "SessionStore:restoreHistoryComplete",

  // The load for the restoring tab has begun. We update the URL bar at this
  // time; if we did it before, the load would overwrite it.
  "SessionStore:restoreTabContentStarted",

  // All network loads for a restoring tab are done, so we should
  // consider restoring another tab in the queue. The document has
  // been restored, and forms have been filled. We trigger
  // SSTabRestored at this time.
  "SessionStore:restoreTabContentComplete",

  // The content script encountered an error.
  "SessionStore:error",
];

// The list of messages we accept from <xul:browser>s that have no tab
// assigned, or whose windows have gone away. Those are for example the
// ones that preload about:newtab pages, or from browsers where the window
// has just been closed.
const NOTAB_MESSAGES = new Set([
  // For a description see above.
  "SessionStore:update",

  // For a description see above.
  "SessionStore:error",
]);

// The list of messages we accept without an "epoch" parameter.
// See getCurrentEpoch() and friends to find out what an "epoch" is.
const NOEPOCH_MESSAGES = new Set([
  // For a description see above.
  "SessionStore:error",
]);

// The list of messages we want to receive even during the short period after a
// frame has been removed from the DOM and before its frame script has finished
// unloading.
const CLOSED_MESSAGES = new Set([
  // For a description see above.
  "SessionStore:update",

  // For a description see above.
  "SessionStore:error",
]);

// These are tab events that we listen to.
const TAB_EVENTS = [
  "TabOpen",
  "TabBrowserInserted",
  "TabClose",
  "TabSelect",
  "TabShow",
  "TabHide",
  "TabPinned",
  "TabUnpinned",
];

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

/**
 * When calling restoreTabContent, we can supply a reason why
 * the content is being restored. These are those reasons.
 */
const RESTORE_TAB_CONTENT_REASON = {
  /**
   * SET_STATE:
   * We're restoring this tab's content because we're setting
   * state inside this browser tab, probably because the user
   * has asked us to restore a tab (or window, or entire session).
   */
  SET_STATE: 0,
  /**
   * NAVIGATE_AND_RESTORE:
   * We're restoring this tab's content because a navigation caused
   * us to do a remoteness-flip.
   */
  NAVIGATE_AND_RESTORE: 1,
};

// 'browser.startup.page' preference value to resume the previous session.
const BROWSER_STARTUP_RESUME_SESSION = 3;

// Used by SessionHistoryListener.
const kNoIndex = Number.MAX_SAFE_INTEGER;
const kLastIndex = Number.MAX_SAFE_INTEGER - 1;

const { PrivateBrowsingUtils } = ChromeUtils.import(
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { TelemetryTimestamps } = ChromeUtils.import(
  "resource://gre/modules/TelemetryTimestamps.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "SessionHistory",
  "resource://gre/modules/sessionstore/SessionHistory.jsm"
);

XPCOMUtils.defineLazyServiceGetters(this, {
  gScreenManager: ["@mozilla.org/gfx/screenmanager;1", "nsIScreenManager"],
});

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  DevToolsShim: "chrome://devtools-startup/content/DevToolsShim.jsm",
  E10SUtils: "resource://gre/modules/E10SUtils.jsm",
  GlobalState: "resource:///modules/sessionstore/GlobalState.jsm",
  HomePage: "resource:///modules/HomePage.jsm",
  PrivacyFilter: "resource://gre/modules/sessionstore/PrivacyFilter.jsm",
  PromiseUtils: "resource://gre/modules/PromiseUtils.jsm",
  RunState: "resource:///modules/sessionstore/RunState.jsm",
  SessionCookies: "resource:///modules/sessionstore/SessionCookies.jsm",
  SessionFile: "resource:///modules/sessionstore/SessionFile.jsm",
  SessionSaver: "resource:///modules/sessionstore/SessionSaver.jsm",
  SessionStartup: "resource:///modules/sessionstore/SessionStartup.jsm",
  TabAttributes: "resource:///modules/sessionstore/TabAttributes.jsm",
  TabCrashHandler: "resource:///modules/ContentCrashHandlers.jsm",
  TabState: "resource:///modules/sessionstore/TabState.jsm",
  TabStateCache: "resource:///modules/sessionstore/TabStateCache.jsm",
  TabStateFlusher: "resource:///modules/sessionstore/TabStateFlusher.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
});

/**
 * |true| if we are in debug mode, |false| otherwise.
 * Debug mode is controlled by preference browser.sessionstore.debug
 */
var gDebuggingEnabled = false;

/**
 * A global value to tell that fingerprinting resistance is enabled or not.
 * If it's enabled, the session restore won't restore the window's size and
 * size mode.
 * This value is controlled by preference privacy.resistFingerprinting.
 */
var gResistFingerprintingEnabled = false;

var SessionStore = {
  get promiseInitialized() {
    return SessionStoreInternal.promiseInitialized;
  },

  get promiseAllWindowsRestored() {
    return SessionStoreInternal.promiseAllWindowsRestored;
  },

  get canRestoreLastSession() {
    return SessionStoreInternal.canRestoreLastSession;
  },

  set canRestoreLastSession(val) {
    SessionStoreInternal.canRestoreLastSession = val;
  },

  get lastClosedObjectType() {
    return SessionStoreInternal.lastClosedObjectType;
  },

  get willAutoRestore() {
    return SessionStoreInternal.willAutoRestore;
  },

  init: function ss_init() {
    SessionStoreInternal.init();
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

  // Return whether a tab is restoring.
  isTabRestoring(aTab) {
    return TAB_STATE_FOR_BROWSER.has(aTab.linkedBrowser);
  },

  getInternalObjectState(obj) {
    return SessionStoreInternal.getInternalObjectState(obj);
  },

  duplicateTab: function ss_duplicateTab(
    aWindow,
    aTab,
    aDelta = 0,
    aRestoreImmediately = true,
    aOptions = {}
  ) {
    return SessionStoreInternal.duplicateTab(
      aWindow,
      aTab,
      aDelta,
      aRestoreImmediately,
      aOptions
    );
  },

  getLastClosedTabCount(aWindow) {
    return SessionStoreInternal.getLastClosedTabCount(aWindow);
  },

  resetLastClosedTabCount(aWindow) {
    SessionStoreInternal.resetLastClosedTabCount(aWindow);
  },

  getClosedTabCount: function ss_getClosedTabCount(aWindow) {
    return SessionStoreInternal.getClosedTabCount(aWindow);
  },

  getClosedTabData: function ss_getClosedTabData(aWindow) {
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

  maybeDontSaveTabs(aWindow) {
    SessionStoreInternal.maybeDontSaveTabs(aWindow);
  },

  undoCloseWindow: function ss_undoCloseWindow(aIndex) {
    return SessionStoreInternal.undoCloseWindow(aIndex);
  },

  forgetClosedWindow: function ss_forgetClosedWindow(aIndex) {
    return SessionStoreInternal.forgetClosedWindow(aIndex);
  },

  getCustomWindowValue(aWindow, aKey) {
    return SessionStoreInternal.getCustomWindowValue(aWindow, aKey);
  },

  setCustomWindowValue(aWindow, aKey, aStringValue) {
    SessionStoreInternal.setCustomWindowValue(aWindow, aKey, aStringValue);
  },

  deleteCustomWindowValue(aWindow, aKey) {
    SessionStoreInternal.deleteCustomWindowValue(aWindow, aKey);
  },

  getCustomTabValue(aTab, aKey) {
    return SessionStoreInternal.getCustomTabValue(aTab, aKey);
  },

  setCustomTabValue(aTab, aKey, aStringValue) {
    SessionStoreInternal.setCustomTabValue(aTab, aKey, aStringValue);
  },

  deleteCustomTabValue(aTab, aKey) {
    SessionStoreInternal.deleteCustomTabValue(aTab, aKey);
  },

  getLazyTabValue(aTab, aKey) {
    return SessionStoreInternal.getLazyTabValue(aTab, aKey);
  },

  getCustomGlobalValue(aKey) {
    return SessionStoreInternal.getCustomGlobalValue(aKey);
  },

  setCustomGlobalValue(aKey, aStringValue) {
    SessionStoreInternal.setCustomGlobalValue(aKey, aStringValue);
  },

  deleteCustomGlobalValue(aKey) {
    SessionStoreInternal.deleteCustomGlobalValue(aKey);
  },

  persistTabAttribute: function ss_persistTabAttribute(aName) {
    SessionStoreInternal.persistTabAttribute(aName);
  },

  restoreLastSession: function ss_restoreLastSession() {
    SessionStoreInternal.restoreLastSession();
  },

  speculativeConnectOnTabHover(tab) {
    SessionStoreInternal.speculativeConnectOnTabHover(tab);
  },

  getCurrentState(aUpdateAll) {
    return SessionStoreInternal.getCurrentState(aUpdateAll);
  },

  reviveCrashedTab(aTab) {
    return SessionStoreInternal.reviveCrashedTab(aTab);
  },

  reviveAllCrashedTabs() {
    return SessionStoreInternal.reviveAllCrashedTabs();
  },

  updateSessionStoreFromTablistener(
    aBrowser,
    aBrowsingContext,
    aPermanentKey,
    aData
  ) {
    return SessionStoreInternal.updateSessionStoreFromTablistener(
      aBrowser,
      aBrowsingContext,
      aPermanentKey,
      aData
    );
  },

  getSessionHistory(tab, updatedCallback) {
    return SessionStoreInternal.getSessionHistory(tab, updatedCallback);
  },

  undoCloseById(aClosedId, aIncludePrivate) {
    return SessionStoreInternal.undoCloseById(aClosedId, aIncludePrivate);
  },

  resetBrowserToLazyState(tab) {
    return SessionStoreInternal.resetBrowserToLazyState(tab);
  },

  maybeExitCrashedState(browser) {
    SessionStoreInternal.maybeExitCrashedState(browser);
  },

  isBrowserInCrashedSet(browser) {
    return SessionStoreInternal.isBrowserInCrashedSet(browser);
  },

  /**
   * Ensures that session store has registered and started tracking a given window.
   * @param window
   *        Window reference
   */
  ensureInitialized(window) {
    if (SessionStoreInternal._sessionInitialized && !window.__SSi) {
      /*
        We need to check that __SSi is not defined on the window so that if
        onLoad function is in the middle of executing we don't enter the function
        again and try to redeclare the ContentSessionStore script.
       */
      SessionStoreInternal.onLoad(window);
    }
  },

  getCurrentEpoch(browser) {
    return SessionStoreInternal.getCurrentEpoch(browser.permanentKey);
  },

  /**
   * Determines whether the passed version number is compatible with
   * the current version number of the SessionStore.
   *
   * @param version The format and version of the file, as an array, e.g.
   * ["sessionrestore", 1]
   */
  isFormatVersionCompatible(version) {
    if (!version) {
      return false;
    }
    if (!Array.isArray(version)) {
      // Improper format.
      return false;
    }
    if (version[0] != "sessionrestore") {
      // Not a Session Restore file.
      return false;
    }
    let number = Number.parseFloat(version[1]);
    if (Number.isNaN(number)) {
      return false;
    }
    return number <= FORMAT_VERSION;
  },

  /**
   * Filters out not worth-saving tabs from a given browser state object.
   *
   * @param aState (object)
   *        The browser state for which we remove worth-saving tabs.
   *        The given object will be modified.
   */
  keepOnlyWorthSavingTabs(aState) {
    let closedWindowShouldRestore = null;
    for (let i = aState.windows.length - 1; i >= 0; i--) {
      let win = aState.windows[i];
      for (let j = win.tabs.length - 1; j >= 0; j--) {
        let tab = win.tabs[j];
        if (!SessionStoreInternal._shouldSaveTab(tab)) {
          win.tabs.splice(j, 1);
          if (win.selected > j) {
            win.selected--;
          }
        }
      }

      // If it's the last window (and no closedWindow that will restore), keep the window state with no tabs.
      if (
        !win.tabs.length &&
        (aState.windows.length > 1 ||
          closedWindowShouldRestore ||
          (closedWindowShouldRestore == null &&
            (closedWindowShouldRestore = aState._closedWindows.some(
              w => w._shouldRestore
            ))))
      ) {
        aState.windows.splice(i, 1);
        if (aState.selectedWindow > i) {
          aState.selectedWindow--;
        }
      }
    }
  },

  /**
   * Prepares to change the remoteness of the given browser, by ensuring that
   * the local instance of session history is up-to-date.
   */
  async prepareToChangeRemoteness(aTab) {
    await SessionStoreInternal.prepareToChangeRemoteness(aTab);
  },

  finishTabRemotenessChange(aTab, aSwitchId) {
    SessionStoreInternal.finishTabRemotenessChange(aTab, aSwitchId);
  },
};

// Freeze the SessionStore object. We don't want anyone to modify it.
Object.freeze(SessionStore);

var SessionStoreInternal = {
  QueryInterface: ChromeUtils.generateQI([
    "nsIObserver",
    "nsISupportsWeakReference",
  ]),

  _globalState: new GlobalState(),

  // A counter to be used to generate a unique ID for each closed tab or window.
  _nextClosedId: 0,

  // During the initial restore and setBrowserState calls tracks the number of
  // windows yet to be restored
  _restoreCount: -1,

  // For each <browser> element, records the SHistoryListener.
  _browserSHistoryListener: new WeakMap(),

  // Tracks the various listeners that are used throughout the restore.
  _restoreListeners: new WeakMap(),

  // Records the promise created in _restoreHistory, which is used to track
  // the completion of the first phase of the restore.
  _tabStateRestorePromises: new WeakMap(),

  // The history data needed to be restored in the parent.
  _tabStateToRestore: new WeakMap(),

  // For each <browser> element, records the current epoch.
  _browserEpochs: new WeakMap(),

  // Any browsers that fires the oop-browser-crashed event gets stored in
  // here - that way we know which browsers to ignore messages from (until
  // they get restored).
  _crashedBrowsers: new WeakSet(),

  // A map (xul:browser -> FrameLoader) that maps a browser to the last
  // associated frameLoader we heard about.
  _lastKnownFrameLoader: new WeakMap(),

  // A map (xul:browser -> object) that maps a browser associated with a
  // recently closed tab to all its necessary state information we need to
  // properly handle final update message.
  _closedTabs: new WeakMap(),

  // A map (xul:browser -> object) that maps a browser associated with a
  // recently closed tab due to a window closure to the tab state information
  // that is being stored in _closedWindows for that tab.
  _closedWindowTabs: new WeakMap(),

  // A set of window data that has the potential to be saved in the _closedWindows
  // array for the session. We will remove window data from this set whenever
  // forgetClosedWindow is called for the window, or when session history is
  // purged, so that we don't accidentally save that data after the flush has
  // completed. Closed tabs use a more complicated mechanism for this particular
  // problem. When forgetClosedTab is called, the browser is removed from the
  // _closedTabs map, so its data is not recorded. In the purge history case,
  // the closedTabs array per window is overwritten so that once the flush is
  // complete, the tab would only ever add itself to an array that SessionStore
  // no longer cares about. Bug 1230636 has been filed to make the tab case
  // work more like the window case, which is more explicit, and easier to
  // reason about.
  _saveableClosedWindowData: new WeakSet(),

  // whether a setBrowserState call is in progress
  _browserSetState: false,

  // time in milliseconds when the session was started (saved across sessions),
  // defaults to now if no session was restored or timestamp doesn't exist
  _sessionStartTime: Date.now(),

  // states for all currently opened windows
  _windows: {},

  // counter for creating unique window IDs
  _nextWindowID: 0,

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

  _log: null,

  // When starting Firefox with a single private window, this is the place
  // where we keep the session we actually wanted to restore in case the user
  // decides to later open a non-private window as well.
  _deferredInitialState: null,

  // Keeps track of whether a notification needs to be sent that closed objects have changed.
  _closedObjectsChanged: false,

  // A promise resolved once initialization is complete
  _deferredInitialized: (function() {
    let deferred = {};

    deferred.promise = new Promise((resolve, reject) => {
      deferred.resolve = resolve;
      deferred.reject = reject;
    });

    return deferred;
  })(),

  // Whether session has been initialized
  _sessionInitialized: false,

  // A promise resolved once all windows are restored.
  _deferredAllWindowsRestored: (function() {
    let deferred = {};

    deferred.promise = new Promise((resolve, reject) => {
      deferred.resolve = resolve;
      deferred.reject = reject;
    });

    return deferred;
  })(),

  get promiseAllWindowsRestored() {
    return this._deferredAllWindowsRestored.promise;
  },

  // Promise that is resolved when we're ready to initialize
  // and restore the session.
  _promiseReadyForInitialization: null,

  // Keep busy state counters per window.
  _windowBusyStates: new WeakMap(),

  /**
   * A promise fulfilled once initialization is complete.
   */
  get promiseInitialized() {
    return this._deferredInitialized.promise;
  },

  get canRestoreLastSession() {
    return LastSession.canRestore;
  },

  set canRestoreLastSession(val) {
    // Cheat a bit; only allow false.
    if (!val) {
      LastSession.clear();
    }
  },

  /**
   * Returns a string describing the last closed object, either "tab" or "window".
   *
   * This was added to support the sessions.restore WebExtensions API.
   */
  get lastClosedObjectType() {
    if (this._closedWindows.length) {
      // Since there are closed windows, we need to check if there's a closed tab
      // in one of the currently open windows that was closed after the
      // last-closed window.
      let tabTimestamps = [];
      for (let window of Services.wm.getEnumerator("navigator:browser")) {
        let windowState = this._windows[window.__SSi];
        if (windowState && windowState._closedTabs[0]) {
          tabTimestamps.push(windowState._closedTabs[0].closedAt);
        }
      }
      if (
        !tabTimestamps.length ||
        tabTimestamps.sort((a, b) => b - a)[0] < this._closedWindows[0].closedAt
      ) {
        return "window";
      }
    }
    return "tab";
  },

  /**
   * Returns a boolean that determines whether the session will be automatically
   * restored upon the _next_ startup or a restart.
   */
  get willAutoRestore() {
    return (
      !PrivateBrowsingUtils.permanentPrivateBrowsing &&
      (Services.prefs.getBoolPref("browser.sessionstore.resume_session_once") ||
        Services.prefs.getIntPref("browser.startup.page") ==
          BROWSER_STARTUP_RESUME_SESSION)
    );
  },

  /**
   * Initialize the sessionstore service.
   */
  init() {
    if (this._initialized) {
      throw new Error("SessionStore.init() must only be called once!");
    }

    TelemetryTimestamps.add("sessionRestoreInitialized");
    OBSERVING.forEach(function(aTopic) {
      Services.obs.addObserver(this, aTopic, true);
    }, this);

    this._initPrefs();
    this._initialized = true;

    Services.telemetry
      .getHistogramById("FX_SESSION_RESTORE_PRIVACY_LEVEL")
      .add(Services.prefs.getIntPref("browser.sessionstore.privacy_level"));
  },

  /**
   * Initialize the session using the state provided by SessionStartup
   */
  initSession() {
    TelemetryStopwatch.start("FX_SESSION_RESTORE_STARTUP_INIT_SESSION_MS");
    let state;
    let ss = SessionStartup;

    if (ss.willRestore() || ss.sessionType == ss.DEFER_SESSION) {
      state = ss.state;
    }

    if (state) {
      try {
        // If we're doing a DEFERRED session, then we want to pull pinned tabs
        // out so they can be restored.
        if (ss.sessionType == ss.DEFER_SESSION) {
          let [iniState, remainingState] = this._prepDataForDeferredRestore(
            state
          );
          // If we have a iniState with windows, that means that we have windows
          // with app tabs to restore.
          if (iniState.windows.length) {
            // Move cookies over from the remaining state so that they're
            // restored right away, and pinned tabs will load correctly.
            iniState.cookies = remainingState.cookies;
            delete remainingState.cookies;
            state = iniState;
          } else {
            state = null;
          }

          if (remainingState.windows.length) {
            LastSession.setState(remainingState);
          }
          Services.telemetry.keyedScalarAdd(
            "browser.engagement.sessionrestore_interstitial",
            "deferred_restore",
            1
          );
        } else {
          // Get the last deferred session in case the user still wants to
          // restore it
          LastSession.setState(state.lastSessionState);

          let restoreAsCrashed = ss.willRestoreAsCrashed();
          if (restoreAsCrashed) {
            this._recentCrashes =
              ((state.session && state.session.recentCrashes) || 0) + 1;

            // _needsRestorePage will record sessionrestore_interstitial,
            // including the specific reason we decided we needed to show
            // about:sessionrestore, if that's what we do.
            if (this._needsRestorePage(state, this._recentCrashes)) {
              // replace the crashed session with a restore-page-only session
              let url = "about:sessionrestore";
              let formdata = { id: { sessionData: state }, url };
              let entry = {
                url,
                triggeringPrincipal_base64:
                  E10SUtils.SERIALIZED_SYSTEMPRINCIPAL,
              };
              state = { windows: [{ tabs: [{ entries: [entry], formdata }] }] };
            } else if (
              this._hasSingleTabWithURL(state.windows, "about:welcomeback")
            ) {
              Services.telemetry.keyedScalarAdd(
                "browser.engagement.sessionrestore_interstitial",
                "shown_only_about_welcomeback",
                1
              );
              // On a single about:welcomeback URL that crashed, replace about:welcomeback
              // with about:sessionrestore, to make clear to the user that we crashed.
              state.windows[0].tabs[0].entries[0].url = "about:sessionrestore";
              state.windows[0].tabs[0].entries[0].triggeringPrincipal_base64 =
                E10SUtils.SERIALIZED_SYSTEMPRINCIPAL;
            } else {
              restoreAsCrashed = false;
            }
          }

          // If we didn't use about:sessionrestore, record that:
          if (!restoreAsCrashed) {
            Services.telemetry.keyedScalarAdd(
              "browser.engagement.sessionrestore_interstitial",
              "autorestore",
              1
            );
          }

          // Update the session start time using the restored session state.
          this._updateSessionStartTime(state);

          // Make sure that at least the first window doesn't have anything hidden.
          delete state.windows[0].hidden;
          // Since nothing is hidden in the first window, it cannot be a popup.
          delete state.windows[0].isPopup;
          // We don't want to minimize and then open a window at startup.
          if (state.windows[0].sizemode == "minimized") {
            state.windows[0].sizemode = "normal";
          }

          // clear any lastSessionWindowID attributes since those don't matter
          // during normal restore
          state.windows.forEach(function(aWindow) {
            delete aWindow.__lastSessionWindowID;
          });
        }
      } catch (ex) {
        this._log.error("The session file is invalid: " + ex);
      }
    }

    // at this point, we've as good as resumed the session, so we can
    // clear the resume_session_once flag, if it's set
    if (
      !RunState.isQuitting &&
      this._prefBranch.getBoolPref("sessionstore.resume_session_once")
    ) {
      this._prefBranch.setBoolPref("sessionstore.resume_session_once", false);
    }

    TelemetryStopwatch.finish("FX_SESSION_RESTORE_STARTUP_INIT_SESSION_MS");
    return state;
  },

  _initPrefs() {
    this._prefBranch = Services.prefs.getBranch("browser.");

    gDebuggingEnabled = this._prefBranch.getBoolPref("sessionstore.debug");

    Services.prefs.addObserver("browser.sessionstore.debug", () => {
      gDebuggingEnabled = this._prefBranch.getBoolPref("sessionstore.debug");
    });

    this._log = console.createInstance({
      prefix: "SessionStore",
      maxLogLevel: gDebuggingEnabled ? "Debug" : "Warn",
    });

    this._max_tabs_undo = this._prefBranch.getIntPref(
      "sessionstore.max_tabs_undo"
    );
    this._prefBranch.addObserver("sessionstore.max_tabs_undo", this, true);

    this._max_windows_undo = this._prefBranch.getIntPref(
      "sessionstore.max_windows_undo"
    );
    this._prefBranch.addObserver("sessionstore.max_windows_undo", this, true);

    this._restore_on_demand = this._prefBranch.getBoolPref(
      "sessionstore.restore_on_demand"
    );
    this._prefBranch.addObserver("sessionstore.restore_on_demand", this, true);

    gResistFingerprintingEnabled = Services.prefs.getBoolPref(
      "privacy.resistFingerprinting"
    );
    Services.prefs.addObserver("privacy.resistFingerprinting", this);

    this._shistoryInParent = Services.appinfo.sessionHistoryInParent;
  },

  /**
   * Called on application shutdown, after notifications:
   * quit-application-granted, quit-application
   */
  _uninit: function ssi_uninit() {
    if (!this._initialized) {
      throw new Error("SessionStore is not initialized.");
    }

    // Prepare to close the session file and write the last state.
    RunState.setClosing();

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
      case "browser-window-before-show": // catch new windows
        this.onBeforeBrowserWindowShown(aSubject);
        break;
      case "domwindowclosed": // catch closed windows
        this.onClose(aSubject).then(() => {
          this._notifyOfClosedObjectsChange();
        });
        if (gDebuggingEnabled) {
          Services.obs.notifyObservers(null, NOTIFY_DOMWINDOWCLOSED_HANDLED);
        }
        break;
      case "quit-application-granted":
        let syncShutdown = aData == "syncShutdown";
        this.onQuitApplicationGranted(syncShutdown);
        break;
      case "browser-lastwindow-close-granted":
        this.onLastWindowCloseGranted();
        break;
      case "quit-application":
        this.onQuitApplication(aData);
        break;
      case "browser:purge-session-history": // catch sanitization
        this.onPurgeSessionHistory();
        this._notifyOfClosedObjectsChange();
        break;
      case "browser:purge-session-history-for-domain":
        this.onPurgeDomainData(aData);
        this._notifyOfClosedObjectsChange();
        break;
      case "nsPref:changed": // catch pref changes
        this.onPrefChange(aData);
        this._notifyOfClosedObjectsChange();
        break;
      case "idle-daily":
        this.onIdleDaily();
        this._notifyOfClosedObjectsChange();
        break;
      case "clear-origin-attributes-data":
        let userContextId = 0;
        try {
          userContextId = JSON.parse(aData).userContextId;
        } catch (e) {}
        if (userContextId) {
          this._forgetTabsWithUserContextId(userContextId);
        }
        break;
      case "browsing-context-did-set-embedder":
        if (Services.appinfo.sessionHistoryInParent) {
          if (
            aSubject &&
            aSubject === aSubject.top &&
            aSubject.isContent &&
            aSubject.embedderElement &&
            aSubject.embedderElement.permanentKey
          ) {
            let permanentKey = aSubject.embedderElement.permanentKey;
            this._browserSHistoryListener.get(permanentKey)?.unregister();
            this.getOrCreateSHistoryListener(permanentKey, aSubject, true);
          }
        }
        break;
      case "browsing-context-discarded":
        if (Services.appinfo.sessionHistoryInParent) {
          let permanentKey = aSubject?.embedderElement?.permanentKey;
          if (permanentKey) {
            this._browserSHistoryListener.get(permanentKey)?.unregister();
          }
        }
        break;
      case "browser-shutdown-tabstate-updated":
        if (Services.appinfo.sessionHistoryInParent) {
          // Non-SHIP code calls this when the frame script is unloaded.
          this.onFinalTabStateUpdateComplete(aSubject);
        }
        break;
    }
  },

  getOrCreateSHistoryListener(
    permanentKey,
    browsingContext,
    collectImmediately = false
  ) {
    class SHistoryListener {
      constructor() {
        this.QueryInterface = ChromeUtils.generateQI([
          "nsISHistoryListener",
          "nsISupportsWeakReference",
        ]);

        this._browserId = browsingContext.browserId;
        this._fromIndex = kNoIndex;
      }

      unregister() {
        let bc = BrowsingContext.getCurrentTopByBrowserId(this._browserId);
        bc?.sessionHistory?.removeSHistoryListener(this);
        SessionStoreInternal._browserSHistoryListener.delete(permanentKey);
      }

      collect(
        permanentKey, // eslint-disable-line no-shadow
        browsingContext, // eslint-disable-line no-shadow
        { collectFull = true, writeToCache = false }
      ) {
        // Don't bother doing anything if we haven't seen any navigations.
        if (!collectFull && this._fromIndex === kNoIndex) {
          return null;
        }

        let fromIndex = collectFull ? -1 : this._fromIndex;
        this._fromIndex = kNoIndex;

        let historychange = SessionHistory.collectFromParent(
          browsingContext.currentURI?.spec,
          true, // Bug 1704574
          browsingContext.sessionHistory,
          fromIndex
        );

        if (writeToCache) {
          let win =
            browsingContext.embedderElement?.ownerGlobal ||
            browsingContext.currentWindowGlobal?.browsingContext?.window;

          SessionStoreInternal.onTabStateUpdate(permanentKey, win, {
            data: { historychange },
          });
        }

        return historychange;
      }

      collectFrom(index) {
        if (this._fromIndex <= index) {
          // If we already know that we need to update history from index N we
          // can ignore any changes that happened with an element with index
          // larger than N.
          //
          // Note: initially we use kNoIndex which is MAX_SAFE_INTEGER which
          // means we don't ignore anything here, and in case of navigation in
          // the history back and forth cases we use kLastIndex which ignores
          // only the subsequent navigations, but not any new elements added.
          return;
        }

        let bc = BrowsingContext.getCurrentTopByBrowserId(this._browserId);
        if (bc?.embedderElement?.frameLoader) {
          this._fromIndex = index;

          // Queue a tab state update on the |browser.sessionstore.interval|
          // timer. We'll call this.collect() when we receive the update.
          bc.embedderElement.frameLoader.requestSHistoryUpdate();
        }
      }

      OnHistoryNewEntry(newURI, oldIndex) {
        // We use oldIndex - 1 to collect the current entry as well. This makes
        // sure to collect any changes that were made to the entry while the
        // document was active.
        this.collectFrom(oldIndex == -1 ? oldIndex : oldIndex - 1);
      }
      OnHistoryGotoIndex() {
        this.collectFrom(kLastIndex);
      }
      OnHistoryPurge() {
        this.collectFrom(-1);
      }
      OnHistoryReload() {
        this.collectFrom(-1);
        return true;
      }
      OnHistoryReplaceEntry() {
        this.collectFrom(-1);
      }
    }

    if (!Services.appinfo.sessionHistoryInParent) {
      throw new Error("This function should only be used with SHIP");
    }

    if (!permanentKey || browsingContext !== browsingContext.top) {
      return null;
    }

    let sessionHistory = browsingContext.sessionHistory;
    if (!sessionHistory) {
      return null;
    }

    let listener = this._browserSHistoryListener.get(permanentKey);
    if (listener) {
      return listener;
    }

    listener = new SHistoryListener();
    sessionHistory.addSHistoryListener(listener);
    this._browserSHistoryListener.set(permanentKey, listener);

    let isAboutBlank = browsingContext.currentURI?.spec === "about:blank";

    if (collectImmediately && (!isAboutBlank || sessionHistory.count !== 0)) {
      listener.collect(permanentKey, browsingContext, { writeToCache: true });
    }

    return listener;
  },

  onTabStateUpdate(permanentKey, win, update) {
    // Ignore messages from <browser> elements that have crashed
    // and not yet been revived.
    if (this._crashedBrowsers.has(permanentKey)) {
      return;
    }

    TabState.update(permanentKey, update);
    this.saveStateDelayed(win);

    // Handle any updates sent by the child after the tab was closed. This
    // might be the final update as sent by the "unload" handler but also
    // any async update message that was sent before the child unloaded.
    let closedTab = this._closedTabs.get(permanentKey);
    if (closedTab) {
      // Update the closed tab's state. This will be reflected in its
      // window's list of closed tabs as that refers to the same object.
      TabState.copyFromCache(permanentKey, closedTab.tabData.state);
    }
  },

  onFinalTabStateUpdateComplete(browser) {
    let permanentKey = browser.permanentKey;
    if (
      this._closedTabs.has(permanentKey) &&
      !this._crashedBrowsers.has(permanentKey)
    ) {
      let { winData, closedTabs, tabData } = this._closedTabs.get(permanentKey);

      // We expect no further updates.
      this._closedTabs.delete(permanentKey);

      // The tab state no longer needs this reference.
      delete tabData.permanentKey;

      // Determine whether the tab state is worth saving.
      let shouldSave = this._shouldSaveTabState(tabData.state);
      let index = closedTabs.indexOf(tabData);

      if (shouldSave && index == -1) {
        // If the tab state is worth saving and we didn't push it onto
        // the list of closed tabs when it was closed (because we deemed
        // the state not worth saving) then add it to the window's list
        // of closed tabs now.
        this.saveClosedTabData(winData, closedTabs, tabData);
      } else if (!shouldSave && index > -1) {
        // Remove from the list of closed tabs. The update messages sent
        // after the tab was closed changed enough state so that we no
        // longer consider its data interesting enough to keep around.
        this.removeClosedTabData(winData, closedTabs, index);
      }
    }

    // If this the final message we need to resolve all pending flush
    // requests for the given browser as they might have been sent too
    // late and will never respond. If they have been sent shortly after
    // switching a browser's remoteness there isn't too much data to skip.
    TabStateFlusher.resolveAll(browser);

    this._browserSHistoryListener.get(permanentKey)?.unregister();
    this._restoreListeners.get(permanentKey)?.unregister();

    Services.obs.notifyObservers(browser, NOTIFY_BROWSER_SHUTDOWN_FLUSH);
  },

  updateSessionStoreFromTablistener(
    browser,
    browsingContext,
    permanentKey,
    update
  ) {
    permanentKey = browser?.permanentKey ?? permanentKey;
    if (!permanentKey) {
      return;
    }

    // Ignore sessionStore update from previous epochs
    if (!this.isCurrentEpoch(permanentKey, update.epoch)) {
      return;
    }

    if (browsingContext.isReplaced) {
      return;
    }

    if (Services.appinfo.sessionHistoryInParent) {
      let listener = this.getOrCreateSHistoryListener(
        permanentKey,
        browsingContext
      );

      if (listener) {
        let historychange = listener.collect(permanentKey, browsingContext, {
          collectFull: !!update.sHistoryNeeded,
          writeToCache: false,
        });

        if (historychange) {
          update.data.historychange = historychange;
        }
      }
    }

    let win =
      browser?.ownerGlobal ??
      browsingContext.currentWindowGlobal?.browsingContext?.window;

    this.onTabStateUpdate(permanentKey, win, update);
  },

  /**
   * This method handles incoming messages sent by the session store content
   * script via the Frame Message Manager or Parent Process Message Manager,
   * and thus enables communication with OOP tabs.
   */
  receiveMessage(aMessage) {
    // If we got here, that means we're dealing with a frame message
    // manager message, so the target will be a <xul:browser>.
    var browser = aMessage.target;
    let win = browser.ownerGlobal;
    let tab = win ? win.gBrowser.getTabForBrowser(browser) : null;

    // Ensure we receive only specific messages from <xul:browser>s that
    // have no tab or window assigned, e.g. the ones that preload
    // about:newtab pages, or windows that have closed.
    if (!tab && !NOTAB_MESSAGES.has(aMessage.name)) {
      throw new Error(
        `received unexpected message '${aMessage.name}' ` +
          `from a browser that has no tab or window`
      );
    }

    let data = aMessage.data || {};
    let hasEpoch = data.hasOwnProperty("epoch");

    // Most messages sent by frame scripts require to pass an epoch.
    if (!hasEpoch && !NOEPOCH_MESSAGES.has(aMessage.name)) {
      throw new Error(`received message '${aMessage.name}' without an epoch`);
    }

    // Ignore messages from previous epochs.
    if (hasEpoch && !this.isCurrentEpoch(browser.permanentKey, data.epoch)) {
      return;
    }

    switch (aMessage.name) {
      case "SessionStore:update":
        // |browser.frameLoader| might be empty if the browser was already
        // destroyed and its tab removed. In that case we still have the last
        // frameLoader we know about to compare.
        let frameLoader =
          browser.frameLoader ||
          this._lastKnownFrameLoader.get(browser.permanentKey);

        // If the message isn't targeting the latest frameLoader discard it.
        if (frameLoader != aMessage.targetFrameLoader) {
          return;
        }

        this.onTabStateUpdate(browser.permanentKey, browser.ownerGlobal, data);

        if (data.isFinal) {
          this.onFinalTabStateUpdateComplete(browser);
        } else if (data.flushID) {
          // This is an update kicked off by an async flush request. Notify the
          // TabStateFlusher so that it can finish the request and notify its
          // consumer that's waiting for the flush to be done.
          TabStateFlusher.resolve(browser, data.flushID);
        }

        break;
      case "SessionStore:restoreHistoryComplete":
        this._restoreHistoryComplete(browser, data);
        break;
      case "SessionStore:restoreTabContentStarted":
        this._restoreTabContentStarted(browser, data);
        break;
      case "SessionStore:restoreTabContentComplete":
        this._restoreTabContentComplete(browser, data);
        break;
      case "SessionStore:error":
        TabStateFlusher.resolveAll(
          browser,
          false,
          "Received error from the content process"
        );
        break;
      default:
        throw new Error(`received unknown message '${aMessage.name}'`);
    }
  },

  /* ........ Window Event Handlers .............. */

  /**
   * Implement EventListener for handling various window and tab events
   */
  handleEvent: function ssi_handleEvent(aEvent) {
    let win = aEvent.currentTarget.ownerGlobal;
    let target = aEvent.originalTarget;
    switch (aEvent.type) {
      case "TabOpen":
        this.onTabAdd(win);
        break;
      case "TabBrowserInserted":
        this.onTabBrowserInserted(win, target);
        break;
      case "TabClose":
        // `adoptedBy` will be set if the tab was closed because it is being
        // moved to a new window.
        if (!aEvent.detail.adoptedBy) {
          this.onTabClose(win, target);
        }
        this.onTabRemove(win, target);
        this._notifyOfClosedObjectsChange();
        break;
      case "TabSelect":
        this.onTabSelect(win);
        break;
      case "TabShow":
        this.onTabShow(win, target);
        break;
      case "TabHide":
        this.onTabHide(win, target);
        break;
      case "TabPinned":
      case "TabUnpinned":
      case "SwapDocShells":
        this.saveStateDelayed(win);
        break;
      case "oop-browser-crashed":
      case "oop-browser-buildid-mismatch":
        if (aEvent.isTopFrame) {
          this.onBrowserCrashed(target);
        }
        break;
      case "XULFrameLoaderCreated":
        if (
          target.namespaceURI == XUL_NS &&
          target.localName == "browser" &&
          target.frameLoader &&
          target.permanentKey
        ) {
          this._lastKnownFrameLoader.set(
            target.permanentKey,
            target.frameLoader
          );
          this.resetEpoch(target.permanentKey, target.frameLoader);
        }
        break;
      default:
        throw new Error(`unhandled event ${aEvent.type}?`);
    }
    this._clearRestoringWindows();
  },

  /**
   * Generate a unique window identifier
   * @return string
   *         A unique string to identify a window
   */
  _generateWindowID: function ssi_generateWindowID() {
    return "window" + this._nextWindowID++;
  },

  /**
   * Registers and tracks a given window.
   *
   * @param aWindow
   *        Window reference
   */
  onLoad(aWindow) {
    // return if window has already been initialized
    if (aWindow && aWindow.__SSi && this._windows[aWindow.__SSi]) {
      return;
    }

    // ignore windows opened while shutting down
    if (RunState.isQuitting) {
      return;
    }

    // Assign the window a unique identifier we can use to reference
    // internal data about the window.
    aWindow.__SSi = this._generateWindowID();

    let mm = aWindow.getGroupMessageManager("browsers");
    MESSAGES.forEach(msg => {
      let listenWhenClosed = CLOSED_MESSAGES.has(msg);
      mm.addMessageListener(msg, this, listenWhenClosed);
    });

    // Load the frame script after registering listeners.
    if (!Services.appinfo.sessionHistoryInParent) {
      mm.loadFrameScript(
        "chrome://browser/content/content-sessionStore.js",
        true,
        true
      );
    }

    // and create its data object
    this._windows[aWindow.__SSi] = {
      tabs: [],
      selected: 0,
      _closedTabs: [],
      _lastClosedTabGroupCount: -1,
      busy: false,
    };

    if (PrivateBrowsingUtils.isWindowPrivate(aWindow)) {
      this._windows[aWindow.__SSi].isPrivate = true;
    }
    if (!this._isWindowLoaded(aWindow)) {
      this._windows[aWindow.__SSi]._restoring = true;
    }
    if (!aWindow.toolbar.visible) {
      this._windows[aWindow.__SSi].isPopup = true;
    }

    let tabbrowser = aWindow.gBrowser;

    // add tab change listeners to all already existing tabs
    for (let i = 0; i < tabbrowser.tabs.length; i++) {
      this.onTabBrowserInserted(aWindow, tabbrowser.tabs[i]);
    }
    // notification of tab add/remove/selection/show/hide
    TAB_EVENTS.forEach(function(aEvent) {
      tabbrowser.tabContainer.addEventListener(aEvent, this, true);
    }, this);

    // Keep track of a browser's latest frameLoader.
    aWindow.gBrowser.addEventListener("XULFrameLoaderCreated", this);
  },

  /**
   * Initializes a given window.
   *
   * Windows are registered as soon as they are created but we need to wait for
   * the session file to load, and the initial window's delayed startup to
   * finish before initializing a window, i.e. restoring data into it.
   *
   * @param aWindow
   *        Window reference
   * @param aInitialState
   *        The initial state to be loaded after startup (optional)
   */
  initializeWindow(aWindow, aInitialState = null) {
    let isPrivateWindow = PrivateBrowsingUtils.isWindowPrivate(aWindow);

    // perform additional initialization when the first window is loading
    if (RunState.isStopped) {
      RunState.setRunning();

      // restore a crashed session resp. resume the last session if requested
      if (aInitialState) {
        // Don't write to disk right after startup. Set the last time we wrote
        // to disk to NOW() to enforce a full interval before the next write.
        SessionSaver.updateLastSaveTime();

        if (isPrivateWindow) {
          // We're starting with a single private window. Save the state we
          // actually wanted to restore so that we can do it later in case
          // the user opens another, non-private window.
          this._deferredInitialState = SessionStartup.state;

          // Nothing to restore now, notify observers things are complete.
          Services.obs.notifyObservers(null, NOTIFY_WINDOWS_RESTORED);
          Services.obs.notifyObservers(
            null,
            "sessionstore-one-or-no-tab-restored"
          );
          this._deferredAllWindowsRestored.resolve();
        } else {
          TelemetryTimestamps.add("sessionRestoreRestoring");
          this._restoreCount = aInitialState.windows
            ? aInitialState.windows.length
            : 0;

          // global data must be restored before restoreWindow is called so that
          // it happens before observers are notified
          this._globalState.setFromState(aInitialState);

          // Restore session cookies before loading any tabs.
          SessionCookies.restore(aInitialState.cookies || []);

          let overwrite = this._isCmdLineEmpty(aWindow, aInitialState);
          let options = { firstWindow: true, overwriteTabs: overwrite };
          this.restoreWindows(aWindow, aInitialState, options);
        }
      } else {
        // Nothing to restore, notify observers things are complete.
        Services.obs.notifyObservers(null, NOTIFY_WINDOWS_RESTORED);
        Services.obs.notifyObservers(
          null,
          "sessionstore-one-or-no-tab-restored"
        );
        this._deferredAllWindowsRestored.resolve();
      }
      // this window was opened by _openWindowWithState
    } else if (!this._isWindowLoaded(aWindow)) {
      // We want to restore windows after all windows have opened (since bug
      // 1034036), so bail out here.
      return;
      // The user opened another, non-private window after starting up with
      // a single private one. Let's restore the session we actually wanted to
      // restore at startup.
    } else if (
      this._deferredInitialState &&
      !isPrivateWindow &&
      aWindow.toolbar.visible
    ) {
      // global data must be restored before restoreWindow is called so that
      // it happens before observers are notified
      this._globalState.setFromState(this._deferredInitialState);

      this._restoreCount = this._deferredInitialState.windows
        ? this._deferredInitialState.windows.length
        : 0;
      this.restoreWindows(aWindow, this._deferredInitialState, {
        firstWindow: true,
      });
      this._deferredInitialState = null;
    } else if (
      this._restoreLastWindow &&
      aWindow.toolbar.visible &&
      this._closedWindows.length &&
      !isPrivateWindow
    ) {
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
        if (
          AppConstants.platform == "macosx" ||
          !SessionStartup.willRestore()
        ) {
          // We want to split the window up into pinned tabs and unpinned tabs.
          // Pinned tabs should be restored. If there are any remaining tabs,
          // they should be added back to _closedWindows.
          // We'll cheat a little bit and reuse _prepDataForDeferredRestore
          // even though it wasn't built exactly for this.
          let [
            appTabsState,
            normalTabsState,
          ] = this._prepDataForDeferredRestore({
            windows: [closedWindowState],
          });

          // These are our pinned tabs, which we should restore
          if (appTabsState.windows.length) {
            newWindowState = appTabsState.windows[0];
            delete newWindowState.__lastSessionWindowID;
          }

          // In case there were no unpinned tabs, remove the window from _closedWindows
          if (!normalTabsState.windows.length) {
            this._removeClosedWindow(closedWindowIndex);
            // Or update _closedWindows with the modified state
          } else {
            delete normalTabsState.windows[0].__lastSessionWindowID;
            this._closedWindows[closedWindowIndex] = normalTabsState.windows[0];
          }
        } else {
          // If we're just restoring the window, make sure it gets removed from
          // _closedWindows.
          this._removeClosedWindow(closedWindowIndex);
          newWindowState = closedWindowState;
          delete newWindowState.hidden;
        }

        if (newWindowState) {
          // Ensure that the window state isn't hidden
          this._restoreCount = 1;
          let state = { windows: [newWindowState] };
          let options = { overwriteTabs: this._isCmdLineEmpty(aWindow, state) };
          this.restoreWindow(aWindow, newWindowState, options);
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
  },

  /**
   * Called right before a new browser window is shown.
   * @param aWindow
   *        Window reference
   */
  onBeforeBrowserWindowShown(aWindow) {
    // Register the window.
    this.onLoad(aWindow);

    // Some are waiting for this window to be shown, which is now, so let's resolve
    // the deferred operation.
    let deferred = WINDOW_SHOWING_PROMISES.get(aWindow);
    if (deferred) {
      deferred.resolve(aWindow);
      WINDOW_SHOWING_PROMISES.delete(aWindow);
    }

    // Just call initializeWindow() directly if we're initialized already.
    if (this._sessionInitialized) {
      this.initializeWindow(aWindow);
      return;
    }

    // The very first window that is opened creates a promise that is then
    // re-used by all subsequent windows. The promise will be used to tell
    // when we're ready for initialization.
    if (!this._promiseReadyForInitialization) {
      // Wait for the given window's delayed startup to be finished.
      let promise = new Promise(resolve => {
        Services.obs.addObserver(function obs(subject, topic) {
          if (aWindow == subject) {
            Services.obs.removeObserver(obs, topic);
            resolve();
          }
        }, "browser-delayed-startup-finished");
      });

      // We are ready for initialization as soon as the session file has been
      // read from disk and the initial window's delayed startup has finished.
      this._promiseReadyForInitialization = Promise.all([
        promise,
        SessionStartup.onceInitialized,
      ]);
    }

    // We can't call this.onLoad since initialization
    // hasn't completed, so we'll wait until it is done.
    // Even if additional windows are opened and wait
    // for initialization as well, the first opened
    // window should execute first, and this.onLoad
    // will be called with the initialState.
    this._promiseReadyForInitialization
      .then(() => {
        if (aWindow.closed) {
          return;
        }

        if (this._sessionInitialized) {
          this.initializeWindow(aWindow);
        } else {
          let initialState = this.initSession();
          this._sessionInitialized = true;

          if (initialState) {
            Services.obs.notifyObservers(null, NOTIFY_RESTORING_ON_STARTUP);
          }
          TelemetryStopwatch.start(
            "FX_SESSION_RESTORE_STARTUP_ONLOAD_INITIAL_WINDOW_MS"
          );
          this.initializeWindow(aWindow, initialState);
          TelemetryStopwatch.finish(
            "FX_SESSION_RESTORE_STARTUP_ONLOAD_INITIAL_WINDOW_MS"
          );

          // Let everyone know we're done.
          this._deferredInitialized.resolve();
        }
      })
      .catch(console.error);
  },

  /**
   * On window close...
   * - remove event listeners from tabs
   * - save all window data
   * @param aWindow
   *        Window reference
   *
   * @returns a Promise
   */
  onClose: function ssi_onClose(aWindow) {
    let completionPromise = Promise.resolve();
    // this window was about to be restored - conserve its original data, if any
    let isFullyLoaded = this._isWindowLoaded(aWindow);
    if (!isFullyLoaded) {
      if (!aWindow.__SSi) {
        aWindow.__SSi = this._generateWindowID();
      }

      let restoreID = WINDOW_RESTORE_IDS.get(aWindow);
      this._windows[aWindow.__SSi] = this._statesToRestore[
        restoreID
      ].windows[0];
      delete this._statesToRestore[restoreID];
      WINDOW_RESTORE_IDS.delete(aWindow);
    }

    // ignore windows not tracked by SessionStore
    if (!aWindow.__SSi || !this._windows[aWindow.__SSi]) {
      return completionPromise;
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

    let browsers = Array.from(tabbrowser.browsers);

    TAB_EVENTS.forEach(function(aEvent) {
      tabbrowser.tabContainer.removeEventListener(aEvent, this, true);
    }, this);

    aWindow.gBrowser.removeEventListener("XULFrameLoaderCreated", this);

    let winData = this._windows[aWindow.__SSi];

    // Collect window data only when *not* closed during shutdown.
    if (RunState.isRunning) {
      // Grab the most recent window data. The tab data will be updated
      // once we finish flushing all of the messages from the tabs.
      let tabMap = this._collectWindowData(aWindow);

      for (let [tab, tabData] of tabMap) {
        let permanentKey = tab.linkedBrowser.permanentKey;
        this._closedWindowTabs.set(permanentKey, tabData);
        if (aWindow._dontSaveTabs && !tabData.isPrivate) {
          // Close remaining tabs.
          tab._closedInGroup = true;
          this.maybeSaveClosedTab(aWindow, tab, tabData);
        }
      }

      if (aWindow._dontSaveTabs) {
        winData.tabs.splice(0, winData.tabs.length);
        winData.selected = -1;
      }

      if (isFullyLoaded) {
        winData.title =
          tabbrowser.selectedBrowser.contentTitle ||
          tabbrowser.selectedTab.label;
      }

      if (AppConstants.platform != "macosx") {
        // Until we decide otherwise elsewhere, this window is part of a series
        // of closing windows to quit.
        winData._shouldRestore = true;
      }

      // Store the window's close date to figure out when each individual tab
      // was closed. This timestamp should allow re-arranging data based on how
      // recently something was closed.
      winData.closedAt = Date.now();

      // we don't want to save the busy state
      delete winData.busy;

      // When closing windows one after the other until Firefox quits, we
      // will move those closed in series back to the "open windows" bucket
      // before writing to disk. If however there is only a single window
      // with tabs we deem not worth saving then we might end up with a
      // random closed or even a pop-up window re-opened. To prevent that
      // we explicitly allow saving an "empty" window state.
      let isLastWindow = this.isLastRestorableWindow();

      // clear this window from the list, since it has definitely been closed.
      delete this._windows[aWindow.__SSi];

      // This window has the potential to be saved in the _closedWindows
      // array (maybeSaveClosedWindows gets the final call on that).
      this._saveableClosedWindowData.add(winData);

      // Now we have to figure out if this window is worth saving in the _closedWindows
      // Object.
      //
      // We're about to flush the tabs from this window, but it's possible that we
      // might never hear back from the content process(es) in time before the user
      // chooses to restore the closed window. So we do the following:
      //
      // 1) Use the tab state cache to determine synchronously if the window is
      //    worth stashing in _closedWindows.
      // 2) Flush the window.
      // 3) When the flush is complete, revisit our decision to store the window
      //    in _closedWindows, and add/remove as necessary.
      if (!winData.isPrivate) {
        // Remove any open private tabs the window may contain.
        PrivacyFilter.filterPrivateTabs(winData);
        this.maybeSaveClosedWindow(winData, isLastWindow);
      }

      completionPromise = TabStateFlusher.flushWindow(aWindow).then(() => {
        // At this point, aWindow is closed! You should probably not try to
        // access any DOM elements from aWindow within this callback unless
        // you're holding on to them in the closure.

        WINDOW_FLUSHING_PROMISES.delete(aWindow);

        for (let browser of browsers) {
          if (this._closedWindowTabs.has(browser.permanentKey)) {
            let tabData = this._closedWindowTabs.get(browser.permanentKey);
            TabState.copyFromCache(browser.permanentKey, tabData);
            this._closedWindowTabs.delete(browser.permanentKey);
          }
        }

        // Save non-private windows if they have at
        // least one saveable tab or are the last window.
        if (!winData.isPrivate) {
          // It's possible that a tab switched its privacy state at some point
          // before our flush, so we need to filter again.
          PrivacyFilter.filterPrivateTabs(winData);
          this.maybeSaveClosedWindow(winData, isLastWindow);
        }

        // Update the tabs data now that we've got the most
        // recent information.
        this.cleanUpWindow(aWindow, winData, browsers);

        // save the state without this window to disk
        this.saveStateDelayed();
      });

      // Here we might override a flush already in flight, but that's fine
      // because `completionPromise` will always resolve after the old flush
      // resolves.
      WINDOW_FLUSHING_PROMISES.set(aWindow, completionPromise);
    } else {
      this.cleanUpWindow(aWindow, winData, browsers);
    }

    for (let i = 0; i < tabbrowser.tabs.length; i++) {
      this.onTabRemove(aWindow, tabbrowser.tabs[i], true);
    }

    return completionPromise;
  },

  /**
   * Clean up the message listeners on a window that has finally
   * gone away. Call this once you're sure you don't want to hear
   * from any of this windows tabs from here forward.
   *
   * @param aWindow
   *        The browser window we're cleaning up.
   * @param winData
   *        The data for the window that we should hold in the
   *        DyingWindowCache in case anybody is still holding a
   *        reference to it.
   */
  cleanUpWindow(aWindow, winData, browsers) {
    // Any leftover TabStateFlusher Promises need to be resolved now,
    // since we're about to remove the message listeners.
    for (let browser of browsers) {
      TabStateFlusher.resolveAll(browser);
    }

    // Cache the window state until it is completely gone.
    DyingWindowCache.set(aWindow, winData);

    let mm = aWindow.getGroupMessageManager("browsers");
    MESSAGES.forEach(msg => mm.removeMessageListener(msg, this));

    this._saveableClosedWindowData.delete(winData);
    delete aWindow.__SSi;
  },

  /**
   * Decides whether or not a closed window should be put into the
   * _closedWindows Object. This might be called multiple times per
   * window, and will do the right thing of moving the window data
   * in or out of _closedWindows if the winData indicates that our
   * need for saving it has changed.
   *
   * @param winData
   *        The data for the closed window that we might save.
   * @param isLastWindow
   *        Whether or not the window being closed is the last
   *        browser window. Callers of this function should pass
   *        in the value of SessionStoreInternal.atLastWindow for
   *        this argument, and pass in the same value if they happen
   *        to call this method again asynchronously (for example, after
   *        a window flush).
   */
  maybeSaveClosedWindow(winData, isLastWindow) {
    // Make sure SessionStore is still running, and make sure that we
    // haven't chosen to forget this window.
    if (RunState.isRunning && this._saveableClosedWindowData.has(winData)) {
      // Determine whether the window has any tabs worth saving.
      let hasSaveableTabs = winData.tabs.some(this._shouldSaveTabState);

      // Note that we might already have this window stored in
      // _closedWindows from a previous call to this function.
      let winIndex = this._closedWindows.indexOf(winData);
      let alreadyStored = winIndex != -1;
      let shouldStore = hasSaveableTabs || isLastWindow;

      if (shouldStore && !alreadyStored) {
        let index = this._closedWindows.findIndex(win => {
          return win.closedAt < winData.closedAt;
        });

        // If we found no tab closed before our
        // tab then just append it to the list.
        if (index == -1) {
          index = this._closedWindows.length;
        }

        // About to save the closed window, add a unique ID.
        winData.closedId = this._nextClosedId++;

        // Insert tabData at the right position.
        this._closedWindows.splice(index, 0, winData);
        this._capClosedWindows();
        this._closedObjectsChanged = true;
        // The first time we close a window, ensure it can be restored from the
        // hidden window.
        if (
          AppConstants.platform == "macosx" &&
          this._closedWindows.length == 1
        ) {
          // Fake a popupshowing event so shortcuts work:
          let window = Services.appShell.hiddenDOMWindow;
          let historyMenu = window.document.getElementById("history-menu");
          let evt = new window.CustomEvent("popupshowing", { bubbles: true });
          historyMenu.menupopup.dispatchEvent(evt);
        }
      } else if (!shouldStore && alreadyStored) {
        this._removeClosedWindow(winIndex);
      }
    }
  },

  /**
   * On quit application granted
   */
  onQuitApplicationGranted: function ssi_onQuitApplicationGranted(
    syncShutdown = false
  ) {
    // Collect an initial snapshot of window data before we do the flush.
    let index = 0;
    for (let window of this._orderedBrowserWindows) {
      this._collectWindowData(window);
      this._windows[window.__SSi].zIndex = ++index;
    }

    // Now add an AsyncShutdown blocker that'll spin the event loop
    // until the windows have all been flushed.

    // This progress object will track the state of async window flushing
    // and will help us debug things that go wrong with our AsyncShutdown
    // blocker.
    let progress = { total: -1, current: -1 };

    // We're going down! Switch state so that we treat closing windows and
    // tabs correctly.
    RunState.setQuitting();

    if (!syncShutdown) {
      // We've got some time to shut down, so let's do this properly that there
      // will be a complete session available upon next startup.
      // To prevent a blocker from taking longer than the DELAY_CRASH_MS limit
      // (which will cause a crash) of AsyncShutdown whilst flushing all windows,
      // we resolve the Promise blocker once:
      // 1. the flush duration exceeds 10 seconds before DELAY_CRASH_MS, or
      // 2. 'oop-frameloader-crashed', or
      // 3. 'ipc:content-shutdown' is observed.
      AsyncShutdown.quitApplicationGranted.addBlocker(
        "SessionStore: flushing all windows",
        () => {
          // Set up the list of promises that will signal a complete sessionstore
          // shutdown: either all data is saved, or we crashed or the message IPC
          // channel went away in the meantime.
          let promises = [this.flushAllWindowsAsync(progress)];

          const observeTopic = topic => {
            let deferred = PromiseUtils.defer();
            const observer = subject => {
              // Skip abort on ipc:content-shutdown if not abnormal/crashed
              subject.QueryInterface(Ci.nsIPropertyBag2);
              if (
                !(topic == "ipc:content-shutdown" && !subject.get("abnormal"))
              ) {
                deferred.resolve();
              }
            };
            const cleanup = () => {
              try {
                Services.obs.removeObserver(observer, topic);
              } catch (ex) {
                Cu.reportError(
                  "SessionStore: exception whilst flushing all windows: " + ex
                );
              }
            };
            Services.obs.addObserver(observer, topic);
            deferred.promise.then(cleanup, cleanup);
            return deferred;
          };

          // Build a list of deferred executions that require cleanup once the
          // Promise race is won.
          // Ensure that the timer fires earlier than the AsyncShutdown crash timer.
          let waitTimeMaxMs = Math.max(0, AsyncShutdown.DELAY_CRASH_MS - 10000);
          let defers = [
            this.looseTimer(waitTimeMaxMs),

            // FIXME: We should not be aborting *all* flushes when a single
            // content process crashes here.
            observeTopic("oop-frameloader-crashed"),
            observeTopic("ipc:content-shutdown"),
          ];
          // Add these monitors to the list of Promises to start the race.
          promises.push(...defers.map(deferred => deferred.promise));

          return Promise.race(promises).then(() => {
            // When a Promise won the race, make sure we clean up the running
            // monitors.
            defers.forEach(deferred => deferred.reject());
          });
        },
        () => progress
      );
    } else {
      // We have to shut down NOW, which means we only get to save whatever
      // we already had cached.
    }
  },

  /**
   * An async Task that iterates all open browser windows and flushes
   * any outstanding messages from their tabs. This will also close
   * all of the currently open windows while we wait for the flushes
   * to complete.
   *
   * @param progress (Object)
   *        Optional progress object that will be updated as async
   *        window flushing progresses. flushAllWindowsSync will
   *        write to the following properties:
   *
   *        total (int):
   *          The total number of windows to be flushed.
   *        current (int):
   *          The current window that we're waiting for a flush on.
   *
   * @return Promise
   */
  async flushAllWindowsAsync(progress = {}) {
    let windowPromises = new Map(WINDOW_FLUSHING_PROMISES);
    WINDOW_FLUSHING_PROMISES.clear();

    // We collect flush promises and close each window immediately so that
    // the user can't start changing any window state while we're waiting
    // for the flushes to finish.
    for (let window of this._browserWindows) {
      windowPromises.set(window, TabStateFlusher.flushWindow(window));

      // We have to wait for these messages to come up from
      // each window and each browser. In the meantime, hide
      // the windows to improve perceived shutdown speed.
      let baseWin = window.docShell.treeOwner.QueryInterface(Ci.nsIBaseWindow);
      baseWin.visibility = false;
    }

    progress.total = windowPromises.size;
    progress.current = 0;

    // We'll iterate through the Promise array, yielding each one, so as to
    // provide useful progress information to AsyncShutdown.
    for (let [win, promise] of windowPromises) {
      await promise;

      // We may have already stopped tracking this window in onClose, which is
      // fine as we would've collected window data there as well.
      if (win.__SSi && this._windows[win.__SSi]) {
        this._collectWindowData(win);
      }

      progress.current++;
    }

    // We must cache this because _getTopWindow will always
    // return null by the time quit-application occurs.
    var activeWindow = this._getTopWindow();
    if (activeWindow) {
      this.activeWindowSSiCache = activeWindow.__SSi || "";
    }
    DirtyWindows.clear();
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
    if (aData == "restart" || aData == "os-restart") {
      if (!PrivateBrowsingUtils.permanentPrivateBrowsing) {
        if (
          aData == "os-restart" &&
          !this._prefBranch.getBoolPref("sessionstore.resume_session_once")
        ) {
          this._prefBranch.setBoolPref(
            "sessionstore.resuming_after_os_restart",
            true
          );
        }
        this._prefBranch.setBoolPref("sessionstore.resume_session_once", true);
      }

      // The browser:purge-session-history notification fires after the
      // quit-application notification so unregister the
      // browser:purge-session-history notification to prevent clearing
      // session data on disk on a restart.  It is also unnecessary to
      // perform any other sanitization processing on a restart as the
      // browser is about to exit anyway.
      Services.obs.removeObserver(this, "browser:purge-session-history");
    }

    if (aData != "restart") {
      // Throw away the previous session on shutdown without notification
      LastSession.clear(true);
    }

    this._uninit();
  },

  /**
   * On purge of session history
   */
  onPurgeSessionHistory: function ssi_onPurgeSessionHistory() {
    SessionFile.wipe();
    // If the browser is shutting down, simply return after clearing the
    // session data on disk as this notification fires after the
    // quit-application notification so the browser is about to exit.
    if (RunState.isQuitting) {
      return;
    }
    LastSession.clear();

    let openWindows = {};
    // Collect open windows.
    for (let window of this._browserWindows) {
      openWindows[window.__SSi] = true;
    }

    // also clear all data about closed tabs and windows
    for (let ix in this._windows) {
      if (ix in openWindows) {
        if (this._windows[ix]._closedTabs.length) {
          this._windows[ix]._closedTabs = [];
          this._closedObjectsChanged = true;
        }
      } else {
        delete this._windows[ix];
      }
    }
    // also clear all data about closed windows
    if (this._closedWindows.length) {
      this._closedWindows = [];
      this._closedObjectsChanged = true;
    }
    // give the tabbrowsers a chance to clear their histories first
    var win = this._getTopWindow();
    if (win) {
      win.setTimeout(() => SessionSaver.run(), 0);
    } else if (RunState.isRunning) {
      SessionSaver.run();
    }

    this._clearRestoringWindows();
    this._saveableClosedWindowData = new WeakSet();
  },

  /**
   * On purge of domain data
   * @param {string} aDomain
   *        The domain we want to purge data for
   */
  onPurgeDomainData: function ssi_onPurgeDomainData(aDomain) {
    // does a session history entry contain a url for the given domain?
    function containsDomain(aEntry) {
      let host;
      try {
        host = Services.io.newURI(aEntry.url).host;
      } catch (e) {
        // The given URL probably doesn't have a host.
      }
      if (host && Services.eTLD.hasRootDomain(host, aDomain)) {
        return true;
      }
      return aEntry.children && aEntry.children.some(containsDomain, this);
    }
    // remove all closed tabs containing a reference to the given domain
    for (let ix in this._windows) {
      let closedTabs = this._windows[ix]._closedTabs;
      for (let i = closedTabs.length - 1; i >= 0; i--) {
        if (closedTabs[i].state.entries.some(containsDomain, this)) {
          closedTabs.splice(i, 1);
          this._closedObjectsChanged = true;
        }
      }
    }
    // remove all open & closed tabs containing a reference to the given
    // domain in closed windows
    for (let ix = this._closedWindows.length - 1; ix >= 0; ix--) {
      let closedTabs = this._closedWindows[ix]._closedTabs;
      let openTabs = this._closedWindows[ix].tabs;
      let openTabCount = openTabs.length;
      for (let i = closedTabs.length - 1; i >= 0; i--) {
        if (closedTabs[i].state.entries.some(containsDomain, this)) {
          closedTabs.splice(i, 1);
        }
      }
      for (let j = openTabs.length - 1; j >= 0; j--) {
        if (openTabs[j].entries.some(containsDomain, this)) {
          openTabs.splice(j, 1);
          if (this._closedWindows[ix].selected > j) {
            this._closedWindows[ix].selected--;
          }
        }
      }
      if (!openTabs.length) {
        this._closedWindows.splice(ix, 1);
      } else if (openTabs.length != openTabCount) {
        // Adjust the window's title if we removed an open tab
        let selectedTab = openTabs[this._closedWindows[ix].selected - 1];
        // some duplication from restoreHistory - make sure we get the correct title
        let activeIndex = (selectedTab.index || selectedTab.entries.length) - 1;
        if (activeIndex >= selectedTab.entries.length) {
          activeIndex = selectedTab.entries.length - 1;
        }
        this._closedWindows[ix].title = selectedTab.entries[activeIndex].title;
      }
    }

    if (RunState.isRunning) {
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
        this._max_tabs_undo = this._prefBranch.getIntPref(
          "sessionstore.max_tabs_undo"
        );
        for (let ix in this._windows) {
          if (this._windows[ix]._closedTabs.length > this._max_tabs_undo) {
            this._windows[ix]._closedTabs.splice(
              this._max_tabs_undo,
              this._windows[ix]._closedTabs.length
            );
            this._closedObjectsChanged = true;
          }
        }
        break;
      case "sessionstore.max_windows_undo":
        this._max_windows_undo = this._prefBranch.getIntPref(
          "sessionstore.max_windows_undo"
        );
        this._capClosedWindows();
        break;
      case "privacy.resistFingerprinting":
        gResistFingerprintingEnabled = Services.prefs.getBoolPref(
          "privacy.resistFingerprinting"
        );
        break;
      case "sessionstore.restore_on_demand":
        this._restore_on_demand = this._prefBranch.getBoolPref(
          "sessionstore.restore_on_demand"
        );
        break;
    }
  },

  /**
   * save state when new tab is added
   * @param aWindow
   *        Window reference
   */
  onTabAdd: function ssi_onTabAdd(aWindow) {
    this.saveStateDelayed(aWindow);
  },

  /**
   * set up listeners for a new tab
   * @param aWindow
   *        Window reference
   * @param aTab
   *        Tab reference
   */
  onTabBrowserInserted: function ssi_onTabBrowserInserted(aWindow, aTab) {
    let browser = aTab.linkedBrowser;
    browser.addEventListener("SwapDocShells", this);
    browser.addEventListener("oop-browser-crashed", this);
    browser.addEventListener("oop-browser-buildid-mismatch", this);

    if (browser.frameLoader) {
      this._lastKnownFrameLoader.set(browser.permanentKey, browser.frameLoader);
    }

    // Only restore if browser has been lazy.
    if (
      TAB_LAZY_STATES.has(aTab) &&
      !TAB_STATE_FOR_BROWSER.has(browser) &&
      TabStateCache.get(browser.permanentKey)
    ) {
      let tabState = TabState.clone(aTab, TAB_CUSTOM_VALUES.get(aTab));
      this.restoreTab(aTab, tabState);
    }

    // The browser has been inserted now, so lazy data is no longer relevant.
    TAB_LAZY_STATES.delete(aTab);
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
    this.cleanUpRemovedBrowser(aTab);

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
    let tabState = TabState.collect(aTab, TAB_CUSTOM_VALUES.get(aTab));

    // Store closed-tab data for undo.
    this.maybeSaveClosedTab(aWindow, aTab, tabState);
  },

  /**
   * Save a closed tab if needed.
   * @param aWindow
   *        Window reference.
   * @param aTab
   *        Tab reference.
   * @param tabState
   *        Tab state.
   */
  maybeSaveClosedTab(aWindow, aTab, tabState) {
    // Don't save private tabs
    let isPrivateWindow = PrivateBrowsingUtils.isWindowPrivate(aWindow);
    if (!isPrivateWindow && tabState.isPrivate) {
      return;
    }

    let permanentKey = aTab.linkedBrowser.permanentKey;

    let tabData = {
      permanentKey,
      state: tabState,
      title: aTab.label,
      image: aWindow.gBrowser.getIcon(aTab),
      pos: aTab._tPos,
      closedAt: Date.now(),
      closedInGroup: aTab._closedInGroup,
    };

    let winData = this._windows[aWindow.__SSi];
    let closedTabs = winData._closedTabs;

    // Determine whether the tab contains any information worth saving. Note
    // that there might be pending state changes queued in the child that
    // didn't reach the parent yet. If a tab is emptied before closing then we
    // might still remove it from the list of closed tabs later.
    if (this._shouldSaveTabState(tabState)) {
      // Save the tab state, for now. We might push a valid tab out
      // of the list but those cases should be extremely rare and
      // do probably never occur when using the browser normally.
      // (Tests or add-ons might do weird things though.)
      this.saveClosedTabData(winData, closedTabs, tabData);
    }

    // Remember the closed tab to properly handle any last updates included in
    // the final "update" message sent by the frame script's unload handler.
    this._closedTabs.set(permanentKey, { winData, closedTabs, tabData });
  },

  /**
   * Remove listeners which were added when browser was inserted and reset restoring state.
   * Also re-instate lazy data and basically revert tab to its lazy browser state.
   * @param aTab
   *        Tab reference
   */
  resetBrowserToLazyState(aTab) {
    let browser = aTab.linkedBrowser;
    // Browser is already lazy so don't do anything.
    if (!browser.isConnected) {
      return;
    }

    this.cleanUpRemovedBrowser(aTab);

    aTab.setAttribute("pending", "true");

    this._lastKnownFrameLoader.delete(browser.permanentKey);
    this._crashedBrowsers.delete(browser.permanentKey);
    aTab.removeAttribute("crashed");

    let { userTypedValue = null, userTypedClear = 0 } = browser;

    let cacheState = TabStateCache.get(browser.permanentKey);

    // cache the userTypedValue either if the there is no cache state at all
    // (e.g. if it was already discarded before we got to cache its state) or
    // or it may have been created but not including a userTypedValue (e.g.
    // for a private tab we will cache `isPrivate: true` as soon as the tab
    // is opened).
    //
    // In both cases we want to be sure that we are caching the userTypedValue
    // if the browser element has one, otherwise the lazy tab will not be
    // restored with the expected url once activated again (e.g. See Bug 1724205).
    if (userTypedValue && !cacheState?.userTypedValue) {
      // Discard was likely called before state can be cached.  Update
      // the persistent tab state cache with browser information so a
      // restore will be successful.  This information is necessary for
      // restoreTabContent in ContentRestore.jsm to work properly.
      TabStateCache.update(browser.permanentKey, {
        userTypedValue,
        userTypedClear: 1,
      });
    }

    TAB_LAZY_STATES.set(aTab, {
      url: browser.currentURI.spec,
      title: aTab.label,
      userTypedValue,
      userTypedClear,
    });
  },

  /**
   * Check if we are dealing with a crashed browser. If so, then the corresponding
   * crashed tab was revived by navigating to a different page. Remove the browser
   * from the list of crashed browsers to stop ignoring its messages.
   * @param aBrowser
   *        Browser reference
   */
  maybeExitCrashedState(aBrowser) {
    let uri = aBrowser.documentURI;
    if (uri?.spec?.startsWith("about:tabcrashed")) {
      this._crashedBrowsers.delete(aBrowser.permanentKey);
    }
  },

  /**
   * A debugging-only function to check if a browser is in _crashedBrowsers.
   * @param aBrowser
   *        Browser reference
   */
  isBrowserInCrashedSet(aBrowser) {
    if (gDebuggingEnabled) {
      return this._crashedBrowsers.has(aBrowser.permanentKey);
    }
    throw new Error(
      "SessionStore.isBrowserInCrashedSet() should only be called in debug mode!"
    );
  },

  /**
   * When a tab is removed or suspended, remove listeners and reset restoring state.
   * @param aBrowser
   *        Browser reference
   */
  cleanUpRemovedBrowser(aTab) {
    let browser = aTab.linkedBrowser;

    browser.removeEventListener("SwapDocShells", this);
    browser.removeEventListener("oop-browser-crashed", this);
    browser.removeEventListener("oop-browser-buildid-mismatch", this);

    // If this tab was in the middle of restoring or still needs to be restored,
    // we need to reset that state. If the tab was restoring, we will attempt to
    // restore the next tab.
    let previousState = TAB_STATE_FOR_BROWSER.get(browser);
    if (previousState) {
      this._resetTabRestoringState(aTab);
      if (previousState == TAB_STATE_RESTORING) {
        this.restoreNextTab();
      }
    }
  },

  /**
   * Insert a given |tabData| object into the list of |closedTabs|. We will
   * determine the right insertion point based on the .closedAt properties of
   * all tabs already in the list. The list will be truncated to contain a
   * maximum of |this._max_tabs_undo| entries.
   *
   * @param winData (object)
   *        The data of the window.
   * @param tabData (object)
   *        The tabData to be inserted.
   * @param closedTabs (array)
   *        The list of closed tabs for a window.
   */
  saveClosedTabData(winData, closedTabs, tabData) {
    // Find the index of the first tab in the list
    // of closed tabs that was closed before our tab.
    let index = closedTabs.findIndex(tab => {
      return tab.closedAt < tabData.closedAt;
    });

    // If we found no tab closed before our
    // tab then just append it to the list.
    if (index == -1) {
      index = closedTabs.length;
    }

    // About to save the closed tab, add a unique ID.
    tabData.closedId = this._nextClosedId++;

    // Insert tabData at the right position.
    closedTabs.splice(index, 0, tabData);
    this._closedObjectsChanged = true;

    if (tabData.closedInGroup) {
      if (winData._lastClosedTabGroupCount < this._max_tabs_undo) {
        if (winData._lastClosedTabGroupCount < 0) {
          winData._lastClosedTabGroupCount = 1;
        } else {
          winData._lastClosedTabGroupCount++;
        }
      }
    } else {
      winData._lastClosedTabGroupCount = -1;
    }

    // Truncate the list of closed tabs, if needed.
    if (closedTabs.length > this._max_tabs_undo) {
      closedTabs.splice(this._max_tabs_undo, closedTabs.length);
    }
  },

  /**
   * Remove the closed tab data at |index| from the list of |closedTabs|. If
   * the tab's final message is still pending we will simply discard it when
   * it arrives so that the tab doesn't reappear in the list.
   *
   * @param winData (object)
   *        The data of the window.
   * @param index (uint)
   *        The index of the tab to remove.
   * @param closedTabs (array)
   *        The list of closed tabs for a window.
   */
  removeClosedTabData(winData, closedTabs, index) {
    // Remove the given index from the list.
    let [closedTab] = closedTabs.splice(index, 1);
    this._closedObjectsChanged = true;

    // If the tab is part of the last closed group,
    // we need to deduct the tab from the count.
    if (index < winData._lastClosedTabGroupCount) {
      winData._lastClosedTabGroupCount--;
    }

    // If the closed tab's state still has a .permanentKey property then we
    // haven't seen its final update message yet. Remove it from the map of
    // closed tabs so that we will simply discard its last messages and will
    // not add it back to the list of closed tabs again.
    if (closedTab.permanentKey) {
      this._closedTabs.delete(closedTab.permanentKey);
      this._closedWindowTabs.delete(closedTab.permanentKey);
      delete closedTab.permanentKey;
    }

    return closedTab;
  },

  /**
   * When a tab is selected, save session data
   * @param aWindow
   *        Window reference
   */
  onTabSelect: function ssi_onTabSelect(aWindow) {
    if (RunState.isRunning) {
      this._windows[aWindow.__SSi].selected =
        aWindow.gBrowser.tabContainer.selectedIndex;

      let tab = aWindow.gBrowser.selectedTab;
      let browser = tab.linkedBrowser;

      if (TAB_STATE_FOR_BROWSER.get(browser) == TAB_STATE_NEEDS_RESTORE) {
        // If BROWSER_STATE is still available for the browser and it is
        // If __SS_restoreState is still on the browser and it is
        // TAB_STATE_NEEDS_RESTORE, then we haven't restored this tab yet.
        //
        // It's possible that this tab was recently revived, and that
        // we've deferred showing the tab crashed page for it (if the
        // tab crashed in the background). If so, we need to re-enter
        // the crashed state, since we'll be showing the tab crashed
        // page.
        if (TabCrashHandler.willShowCrashedTab(browser)) {
          this.enterCrashedState(browser);
        } else {
          this.restoreTabContent(tab);
        }
      }
    }
  },

  onTabShow: function ssi_onTabShow(aWindow, aTab) {
    // If the tab hasn't been restored yet, move it into the right bucket
    if (
      TAB_STATE_FOR_BROWSER.get(aTab.linkedBrowser) == TAB_STATE_NEEDS_RESTORE
    ) {
      TabRestoreQueue.hiddenToVisible(aTab);

      // let's kick off tab restoration again to ensure this tab gets restored
      // with "restore_hidden_tabs" == false (now that it has become visible)
      this.restoreNextTab();
    }

    // Default delay of 2 seconds gives enough time to catch multiple TabShow
    // events. This used to be due to changing groups in 'tab groups'. We
    // might be able to get rid of this now?
    this.saveStateDelayed(aWindow);
  },

  onTabHide: function ssi_onTabHide(aWindow, aTab) {
    // If the tab hasn't been restored yet, move it into the right bucket
    if (
      TAB_STATE_FOR_BROWSER.get(aTab.linkedBrowser) == TAB_STATE_NEEDS_RESTORE
    ) {
      TabRestoreQueue.visibleToHidden(aTab);
    }

    // Default delay of 2 seconds gives enough time to catch multiple TabHide
    // events. This used to be due to changing groups in 'tab groups'. We
    // might be able to get rid of this now?
    this.saveStateDelayed(aWindow);
  },

  /**
   * Handler for the event that is fired when a <xul:browser> crashes.
   *
   * @param aWindow
   *        The window that the crashed browser belongs to.
   * @param aBrowser
   *        The <xul:browser> that is now in the crashed state.
   */
  onBrowserCrashed(aBrowser) {
    this.enterCrashedState(aBrowser);
    // The browser crashed so we might never receive flush responses.
    // Resolve all pending flush requests for the crashed browser.
    TabStateFlusher.resolveAll(aBrowser);
  },

  /**
   * Called when a browser is showing or is about to show the tab
   * crashed page. This method causes SessionStore to ignore the
   * tab until it's restored.
   *
   * @param browser
   *        The <xul:browser> that is about to show the crashed page.
   */
  enterCrashedState(browser) {
    this._crashedBrowsers.add(browser.permanentKey);

    let win = browser.ownerGlobal;

    // If we hadn't yet restored, or were still in the midst of
    // restoring this browser at the time of the crash, we need
    // to reset its state so that we can try to restore it again
    // when the user revives the tab from the crash.
    if (TAB_STATE_FOR_BROWSER.has(browser)) {
      let tab = win.gBrowser.getTabForBrowser(browser);
      if (tab) {
        this._resetLocalTabRestoringState(tab);
      }
    }
  },

  // Clean up data that has been closed a long time ago.
  // Do not reschedule a save. This will wait for the next regular
  // save.
  onIdleDaily() {
    // Remove old closed windows
    this._cleanupOldData([this._closedWindows]);

    // Remove closed tabs of closed windows
    this._cleanupOldData(
      this._closedWindows.map(winData => winData._closedTabs)
    );

    // Remove closed tabs of open windows
    this._cleanupOldData(
      Object.keys(this._windows).map(key => this._windows[key]._closedTabs)
    );

    this._notifyOfClosedObjectsChange();
  },

  // Remove "old" data from an array
  _cleanupOldData(targets) {
    const TIME_TO_LIVE = this._prefBranch.getIntPref(
      "sessionstore.cleanup.forget_closed_after"
    );
    const now = Date.now();

    for (let array of targets) {
      for (let i = array.length - 1; i >= 0; --i) {
        let data = array[i];
        // Make sure that we have a timestamp to tell us when the target
        // has been closed. If we don't have a timestamp, default to a
        // safe timestamp: just now.
        data.closedAt = data.closedAt || now;
        if (now - data.closedAt > TIME_TO_LIVE) {
          array.splice(i, 1);
          this._closedObjectsChanged = true;
        }
      }
    }
  },

  /* ........ nsISessionStore API .............. */

  getBrowserState: function ssi_getBrowserState() {
    let state = this.getCurrentState();

    // Don't include the last session state in getBrowserState().
    delete state.lastSessionState;

    // Don't include any deferred initial state.
    delete state.deferredInitialState;

    return JSON.stringify(state);
  },

  setBrowserState: function ssi_setBrowserState(aState) {
    this._handleClosedWindows();

    try {
      var state = JSON.parse(aState);
    } catch (ex) {
      /* invalid state object - don't restore anything */
    }
    if (!state) {
      throw Components.Exception(
        "Invalid state string: not JSON",
        Cr.NS_ERROR_INVALID_ARG
      );
    }
    if (!state.windows) {
      throw Components.Exception("No windows", Cr.NS_ERROR_INVALID_ARG);
    }

    this._browserSetState = true;

    // Make sure the priority queue is emptied out
    this._resetRestoringState();

    var window = this._getTopWindow();
    if (!window) {
      this._restoreCount = 1;
      this._openWindowWithState(state);
      return;
    }

    // close all other browser windows
    for (let otherWin of this._browserWindows) {
      if (otherWin != window) {
        otherWin.close();
        this.onClose(otherWin);
      }
    }

    // make sure closed window data isn't kept
    if (this._closedWindows.length) {
      this._closedWindows = [];
      this._closedObjectsChanged = true;
    }

    // determine how many windows are meant to be restored
    this._restoreCount = state.windows ? state.windows.length : 0;

    // global data must be restored before restoreWindow is called so that
    // it happens before observers are notified
    this._globalState.setFromState(state);

    // Restore session cookies.
    SessionCookies.restore(state.cookies || []);

    // restore to the given state
    this.restoreWindows(window, state, { overwriteTabs: true });

    // Notify of changes to closed objects.
    this._notifyOfClosedObjectsChange();
  },

  getWindowState: function ssi_getWindowState(aWindow) {
    if ("__SSi" in aWindow) {
      return Cu.cloneInto(this._getWindowState(aWindow), {});
    }

    if (DyingWindowCache.has(aWindow)) {
      let data = DyingWindowCache.get(aWindow);
      return Cu.cloneInto({ windows: [data] }, {});
    }

    throw Components.Exception(
      "Window is not tracked",
      Cr.NS_ERROR_INVALID_ARG
    );
  },

  setWindowState: function ssi_setWindowState(aWindow, aState, aOverwrite) {
    if (!aWindow.__SSi) {
      throw Components.Exception(
        "Window is not tracked",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    this.restoreWindows(aWindow, aState, { overwriteTabs: aOverwrite });

    // Notify of changes to closed objects.
    this._notifyOfClosedObjectsChange();
  },

  getTabState: function ssi_getTabState(aTab) {
    if (!aTab || !aTab.ownerGlobal) {
      throw Components.Exception("Need a valid tab", Cr.NS_ERROR_INVALID_ARG);
    }
    if (!aTab.ownerGlobal.__SSi) {
      throw Components.Exception(
        "Default view is not tracked",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    let tabState = TabState.collect(aTab, TAB_CUSTOM_VALUES.get(aTab));

    return JSON.stringify(tabState);
  },

  setTabState(aTab, aState) {
    // Remove the tab state from the cache.
    // Note that we cannot simply replace the contents of the cache
    // as |aState| can be an incomplete state that will be completed
    // by |restoreTabs|.
    let tabState = aState;
    if (typeof tabState == "string") {
      tabState = JSON.parse(aState);
    }
    if (!tabState) {
      throw Components.Exception(
        "Invalid state string: not JSON",
        Cr.NS_ERROR_INVALID_ARG
      );
    }
    if (typeof tabState != "object") {
      throw Components.Exception("Not an object", Cr.NS_ERROR_INVALID_ARG);
    }
    if (!("entries" in tabState)) {
      throw Components.Exception(
        "Invalid state object: no entries",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    let window = aTab.ownerGlobal;
    if (!window || !("__SSi" in window)) {
      throw Components.Exception(
        "Window is not tracked",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    if (TAB_STATE_FOR_BROWSER.has(aTab.linkedBrowser)) {
      this._resetTabRestoringState(aTab);
    }

    this.restoreTab(aTab, tabState);

    // Notify of changes to closed objects.
    this._notifyOfClosedObjectsChange();
  },

  getInternalObjectState(obj) {
    if (obj.__SSi) {
      return this._windows[obj.__SSi];
    }
    return obj.loadURI
      ? TAB_STATE_FOR_BROWSER.get(obj)
      : TAB_CUSTOM_VALUES.get(obj);
  },

  duplicateTab: function ssi_duplicateTab(
    aWindow,
    aTab,
    aDelta = 0,
    aRestoreImmediately = true,
    { inBackground, index } = {}
  ) {
    if (!aTab || !aTab.ownerGlobal) {
      throw Components.Exception("Need a valid tab", Cr.NS_ERROR_INVALID_ARG);
    }
    if (!aTab.ownerGlobal.__SSi) {
      throw Components.Exception(
        "Default view is not tracked",
        Cr.NS_ERROR_INVALID_ARG
      );
    }
    if (!aWindow.gBrowser) {
      throw Components.Exception(
        "Invalid window object: no gBrowser",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    // Create a new tab.
    let userContextId = aTab.getAttribute("usercontextid");

    let tabOptions = {
      userContextId,
      index,
      ...(aTab == aWindow.gBrowser.selectedTab
        ? { relatedToCurrent: true, ownerTab: aTab }
        : {}),
      skipLoad: true,
    };
    let newTab = aWindow.gBrowser.addTrustedTab(null, tabOptions);

    // Start the throbber to pretend we're doing something while actually
    // waiting for data from the frame script. This throbber is disabled
    // if the URI is a local about: URI.
    let uriObj = aTab.linkedBrowser.currentURI;
    if (!uriObj || (uriObj && !uriObj.schemeIs("about"))) {
      newTab.setAttribute("busy", "true");
    }

    // Hack to ensure that the about:home, about:newtab, and about:welcome
    // favicon is loaded instantaneously, to avoid flickering and improve
    // perceived performance.
    aWindow.gBrowser.setDefaultIcon(newTab, uriObj);

    // Collect state before flushing.
    let tabState = TabState.collect(aTab, TAB_CUSTOM_VALUES.get(aTab));

    // Flush to get the latest tab state to duplicate.
    let browser = aTab.linkedBrowser;
    TabStateFlusher.flush(browser).then(() => {
      // The new tab might have been closed in the meantime.
      if (newTab.closing || !newTab.linkedBrowser) {
        return;
      }

      let window = newTab.ownerGlobal;

      // The tab or its window might be gone.
      if (!window || !window.__SSi) {
        return;
      }

      // Update state with flushed data. We can't use TabState.clone() here as
      // the tab to duplicate may have already been closed. In that case we
      // only have access to the <xul:browser>.
      let options = { includePrivateData: true };
      TabState.copyFromCache(browser.permanentKey, tabState, options);

      tabState.index += aDelta;
      tabState.index = Math.max(
        1,
        Math.min(tabState.index, tabState.entries.length)
      );
      tabState.pinned = false;

      if (inBackground === false) {
        aWindow.gBrowser.selectedTab = newTab;
      }

      // Restore the state into the new tab.
      this.restoreTab(newTab, tabState, {
        restoreImmediately: aRestoreImmediately,
      });
    });

    return newTab;
  },

  getLastClosedTabCount(aWindow) {
    if ("__SSi" in aWindow) {
      return Math.min(
        Math.max(this._windows[aWindow.__SSi]._lastClosedTabGroupCount, 1),
        this.getClosedTabCount(aWindow)
      );
    }

    throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
  },

  resetLastClosedTabCount(aWindow) {
    if ("__SSi" in aWindow) {
      this._windows[aWindow.__SSi]._lastClosedTabGroupCount = -1;
    } else {
      throw (Components.returnCode = Cr.NS_ERROR_INVALID_ARG);
    }
  },

  getClosedTabCount: function ssi_getClosedTabCount(aWindow) {
    if ("__SSi" in aWindow) {
      return this._windows[aWindow.__SSi]._closedTabs.length;
    }

    if (!DyingWindowCache.has(aWindow)) {
      throw Components.Exception(
        "Window is not tracked",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    return DyingWindowCache.get(aWindow)._closedTabs.length;
  },

  getClosedTabData: function ssi_getClosedTabData(aWindow) {
    if ("__SSi" in aWindow) {
      return Cu.cloneInto(this._windows[aWindow.__SSi]._closedTabs, {});
    }

    if (!DyingWindowCache.has(aWindow)) {
      throw Components.Exception(
        "Window is not tracked",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    let data = DyingWindowCache.get(aWindow);
    return Cu.cloneInto(data._closedTabs, {});
  },

  undoCloseTab: function ssi_undoCloseTab(aWindow, aIndex) {
    if (!aWindow.__SSi) {
      throw Components.Exception(
        "Window is not tracked",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    let winData = this._windows[aWindow.__SSi];

    // default to the most-recently closed tab
    aIndex = aIndex || 0;
    if (!(aIndex in winData._closedTabs)) {
      throw Components.Exception(
        "Invalid index: not in the closed tabs",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    // fetch the data of closed tab, while removing it from the array
    let { state, pos } = this.removeClosedTabData(
      winData,
      winData._closedTabs,
      aIndex
    );

    // create a new tab
    let tabbrowser = aWindow.gBrowser;
    let tab = (tabbrowser.selectedTab = tabbrowser.addTrustedTab(null, {
      index: pos,
      pinned: state.pinned,
      userContextId: state.userContextId,
    }));

    // restore tab content
    this.restoreTab(tab, state);

    // Notify of changes to closed objects.
    this._notifyOfClosedObjectsChange();

    return tab;
  },

  forgetClosedTab: function ssi_forgetClosedTab(aWindow, aIndex) {
    if (!aWindow.__SSi) {
      throw Components.Exception(
        "Window is not tracked",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    let winData = this._windows[aWindow.__SSi];

    // default to the most-recently closed tab
    aIndex = aIndex || 0;
    if (!(aIndex in winData._closedTabs)) {
      throw Components.Exception(
        "Invalid index: not in the closed tabs",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    // remove closed tab from the array
    this.removeClosedTabData(winData, winData._closedTabs, aIndex);

    // Notify of changes to closed objects.
    this._notifyOfClosedObjectsChange();
  },

  getClosedWindowCount: function ssi_getClosedWindowCount() {
    return this._closedWindows.length;
  },

  getClosedWindowData: function ssi_getClosedWindowData() {
    return Cu.cloneInto(this._closedWindows, {});
  },

  maybeDontSaveTabs(aWindow) {
    if (this.willAutoRestore && this.isLastRestorableWindow()) {
      aWindow._dontSaveTabs = true;
    }
  },

  isLastRestorableWindow() {
    return (
      Object.values(this._windows).filter(winData => !winData.isPrivate)
        .length == 1 &&
      !this._closedWindows.some(win => win._shouldRestore || false)
    );
  },

  undoCloseWindow: function ssi_undoCloseWindow(aIndex) {
    if (!(aIndex in this._closedWindows)) {
      throw Components.Exception(
        "Invalid index: not in the closed windows",
        Cr.NS_ERROR_INVALID_ARG
      );
    }
    // reopen the window
    let state = { windows: this._removeClosedWindow(aIndex) };
    delete state.windows[0].closedAt; // Window is now open.

    let window = this._openWindowWithState(state);
    this.windowToFocus = window;
    WINDOW_SHOWING_PROMISES.get(window).promise.then(win =>
      this.restoreWindows(win, state, { overwriteTabs: true })
    );

    // Notify of changes to closed objects.
    this._notifyOfClosedObjectsChange();

    return window;
  },

  forgetClosedWindow: function ssi_forgetClosedWindow(aIndex) {
    // default to the most-recently closed window
    aIndex = aIndex || 0;
    if (!(aIndex in this._closedWindows)) {
      throw Components.Exception(
        "Invalid index: not in the closed windows",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    // remove closed window from the array
    let winData = this._closedWindows[aIndex];
    this._removeClosedWindow(aIndex);
    this._saveableClosedWindowData.delete(winData);

    // Notify of changes to closed objects.
    this._notifyOfClosedObjectsChange();
  },

  getCustomWindowValue(aWindow, aKey) {
    if ("__SSi" in aWindow) {
      let data = this._windows[aWindow.__SSi].extData || {};
      return data[aKey] || "";
    }

    if (DyingWindowCache.has(aWindow)) {
      let data = DyingWindowCache.get(aWindow).extData || {};
      return data[aKey] || "";
    }

    throw Components.Exception(
      "Window is not tracked",
      Cr.NS_ERROR_INVALID_ARG
    );
  },

  setCustomWindowValue(aWindow, aKey, aStringValue) {
    if (typeof aStringValue != "string") {
      throw new TypeError("setCustomWindowValue only accepts string values");
    }

    if (!("__SSi" in aWindow)) {
      throw Components.Exception(
        "Window is not tracked",
        Cr.NS_ERROR_INVALID_ARG
      );
    }
    if (!this._windows[aWindow.__SSi].extData) {
      this._windows[aWindow.__SSi].extData = {};
    }
    this._windows[aWindow.__SSi].extData[aKey] = aStringValue;
    this.saveStateDelayed(aWindow);
  },

  deleteCustomWindowValue(aWindow, aKey) {
    if (
      aWindow.__SSi &&
      this._windows[aWindow.__SSi].extData &&
      this._windows[aWindow.__SSi].extData[aKey]
    ) {
      delete this._windows[aWindow.__SSi].extData[aKey];
    }
    this.saveStateDelayed(aWindow);
  },

  getCustomTabValue(aTab, aKey) {
    return (TAB_CUSTOM_VALUES.get(aTab) || {})[aKey] || "";
  },

  setCustomTabValue(aTab, aKey, aStringValue) {
    if (typeof aStringValue != "string") {
      throw new TypeError("setCustomTabValue only accepts string values");
    }

    // If the tab hasn't been restored, then set the data there, otherwise we
    // could lose newly added data.
    if (!TAB_CUSTOM_VALUES.has(aTab)) {
      TAB_CUSTOM_VALUES.set(aTab, {});
    }

    TAB_CUSTOM_VALUES.get(aTab)[aKey] = aStringValue;
    this.saveStateDelayed(aTab.ownerGlobal);
  },

  deleteCustomTabValue(aTab, aKey) {
    let state = TAB_CUSTOM_VALUES.get(aTab);
    if (state && aKey in state) {
      delete state[aKey];
      this.saveStateDelayed(aTab.ownerGlobal);
    }
  },

  /**
   * Retrieves data specific to lazy-browser tabs.  If tab is not lazy,
   * will return undefined.
   *
   * @param aTab (xul:tab)
   *        The tabbrowser-tab the data is for.
   * @param aKey (string)
   *        The key which maps to the desired data.
   */
  getLazyTabValue(aTab, aKey) {
    return (TAB_LAZY_STATES.get(aTab) || {})[aKey];
  },

  getCustomGlobalValue(aKey) {
    return this._globalState.get(aKey);
  },

  setCustomGlobalValue(aKey, aStringValue) {
    if (typeof aStringValue != "string") {
      throw new TypeError("setCustomGlobalValue only accepts string values");
    }

    this._globalState.set(aKey, aStringValue);
    this.saveStateDelayed();
  },

  deleteCustomGlobalValue(aKey) {
    this._globalState.delete(aKey);
    this.saveStateDelayed();
  },

  persistTabAttribute: function ssi_persistTabAttribute(aName) {
    if (TabAttributes.persist(aName)) {
      this.saveStateDelayed();
    }
  },

  /**
   * Undoes the closing of a tab or window which corresponds
   * to the closedId passed in.
   *
   * @param aClosedId
   *        The closedId of the tab or window
   * @param aIncludePrivate
   *        Whether to restore private tabs or windows
   *
   * @returns a tab or window object
   */
  undoCloseById(aClosedId, aIncludePrivate = true) {
    // Check for a window first.
    for (let i = 0, l = this._closedWindows.length; i < l; i++) {
      if (this._closedWindows[i].closedId == aClosedId) {
        return this.undoCloseWindow(i);
      }
    }

    // Check for a tab.
    for (let window of Services.wm.getEnumerator("navigator:browser")) {
      if (!aIncludePrivate && PrivateBrowsingUtils.isWindowPrivate(window)) {
        continue;
      }
      let windowState = this._windows[window.__SSi];
      if (windowState) {
        for (let j = 0, l = windowState._closedTabs.length; j < l; j++) {
          if (windowState._closedTabs[j].closedId == aClosedId) {
            return this.undoCloseTab(window, j);
          }
        }
      }
    }

    // Neither a tab nor a window was found, return undefined and let the caller decide what to do about it.
    return undefined;
  },

  /**
   * Updates the label and icon for a <xul:tab> using the data from
   * tabData.
   *
   * @param tab
   *        The <xul:tab> to update.
   * @param tabData (optional)
   *        The tabData to use to update the tab. If the argument is
   *        not supplied, the data will be retrieved from the cache.
   */
  updateTabLabelAndIcon(tab, tabData = null) {
    if (tab.hasAttribute("customizemode")) {
      return;
    }

    let browser = tab.linkedBrowser;
    let win = browser.ownerGlobal;

    if (!tabData) {
      tabData = TabState.collect(tab, TAB_CUSTOM_VALUES.get(tab));
      if (!tabData) {
        throw new Error("tabData not found for given tab");
      }
    }

    let activePageData = tabData.entries[tabData.index - 1] || null;

    // If the page has a title, set it.
    if (activePageData) {
      if (activePageData.title && activePageData.title != activePageData.url) {
        win.gBrowser.setInitialTabTitle(tab, activePageData.title, {
          isContentTitle: true,
        });
      } else {
        win.gBrowser.setInitialTabTitle(tab, activePageData.url);
      }
    }

    // Restore the tab icon.
    if ("image" in tabData) {
      // We know that about:blank is safe to load in any remote type. Since
      // SessionStore is triggered with about:blank, there must be a process
      // flip. We will ignore the first about:blank load to prevent resetting the
      // favicon that we have set earlier to avoid flickering and improve
      // perceived performance.
      if (
        !activePageData ||
        (activePageData && activePageData.url != "about:blank")
      ) {
        win.gBrowser.setIcon(
          tab,
          tabData.image,
          undefined,
          tabData.iconLoadingPrincipal
        );
      }
      TabStateCache.update(browser.permanentKey, {
        image: null,
        iconLoadingPrincipal: null,
      });
    }
  },

  // This method deletes all the closedTabs matching userContextId.
  _forgetTabsWithUserContextId(userContextId) {
    for (let window of Services.wm.getEnumerator("navigator:browser")) {
      let windowState = this._windows[window.__SSi];
      if (windowState) {
        // In order to remove the tabs in the correct order, we store the
        // indexes, into an array, then we revert the array and remove closed
        // data from the last one going backward.
        let indexes = [];
        windowState._closedTabs.forEach((closedTab, index) => {
          if (closedTab.state.userContextId == userContextId) {
            indexes.push(index);
          }
        });

        for (let index of indexes.reverse()) {
          this.removeClosedTabData(windowState, windowState._closedTabs, index);
        }
      }
    }

    // Notify of changes to closed objects.
    this._notifyOfClosedObjectsChange();
  },

  /**
   * Restores the session state stored in LastSession. This will attempt
   * to merge data into the current session. If a window was opened at startup
   * with pinned tab(s), then the remaining data from the previous session for
   * that window will be opened into that window. Otherwise new windows will
   * be opened.
   */
  restoreLastSession: function ssi_restoreLastSession() {
    // Use the public getter since it also checks PB mode
    if (!this.canRestoreLastSession) {
      throw Components.Exception("Last session can not be restored");
    }

    Services.obs.notifyObservers(null, NOTIFY_INITIATING_MANUAL_RESTORE);

    // First collect each window with its id...
    let windows = {};
    for (let window of this._browserWindows) {
      if (window.__SS_lastSessionWindowID) {
        windows[window.__SS_lastSessionWindowID] = window;
      }
    }

    let lastSessionState = LastSession.getState();

    // This shouldn't ever be the case...
    if (!lastSessionState.windows.length) {
      throw Components.Exception(
        "lastSessionState has no windows",
        Cr.NS_ERROR_UNEXPECTED
      );
    }

    // We're technically doing a restore, so set things up so we send the
    // notification when we're done. We want to send "sessionstore-browser-state-restored".
    this._restoreCount = lastSessionState.windows.length;
    this._browserSetState = true;

    // We want to re-use the last opened window instead of opening a new one in
    // the case where it's "empty" and not associated with a window in the session.
    // We will do more processing via _prepWindowToRestoreInto if we need to use
    // the lastWindow.
    let lastWindow = this._getTopWindow();
    let canUseLastWindow = lastWindow && !lastWindow.__SS_lastSessionWindowID;

    // global data must be restored before restoreWindow is called so that
    // it happens before observers are notified
    this._globalState.setFromState(lastSessionState);

    let openWindows = [];
    let windowsToOpen = [];

    // Restore session cookies.
    SessionCookies.restore(lastSessionState.cookies || []);

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

      let [canUseWindow, canOverwriteTabs] = this._prepWindowToRestoreInto(
        windowToUse
      );

      // If there's a window already open that we can restore into, use that
      if (canUseWindow) {
        // Since we're not overwriting existing tabs, we want to merge _closedTabs,
        // putting existing ones first. Then make sure we're respecting the max pref.
        if (winState._closedTabs && winState._closedTabs.length) {
          let curWinState = this._windows[windowToUse.__SSi];
          curWinState._closedTabs = curWinState._closedTabs.concat(
            winState._closedTabs
          );
          curWinState._closedTabs.splice(
            this._max_tabs_undo,
            curWinState._closedTabs.length
          );
        }

        // XXXzpao This is going to merge extData together (taking what was in
        //        winState over what is in the window already.
        // We don't restore window right away, just store its data.
        // Later, these windows will be restored with newly opened windows.
        this._updateWindowRestoreState(windowToUse, {
          windows: [winState],
          options: { overwriteTabs: canOverwriteTabs },
        });
        openWindows.push(windowToUse);
      } else {
        windowsToOpen.push(winState);
      }
    }

    // Actually restore windows in reversed z-order.
    this._openWindows({ windows: windowsToOpen }).then(openedWindows =>
      this._restoreWindowsInReversedZOrder(openWindows.concat(openedWindows))
    );

    // Merge closed windows from this session with ones from last session
    if (lastSessionState._closedWindows) {
      this._closedWindows = this._closedWindows.concat(
        lastSessionState._closedWindows
      );
      this._capClosedWindows();
      this._closedObjectsChanged = true;
    }

    DevToolsShim.restoreDevToolsSession(lastSessionState);

    // Set data that persists between sessions
    this._recentCrashes =
      (lastSessionState.session && lastSessionState.session.recentCrashes) || 0;

    // Update the session start time using the restored session state.
    this._updateSessionStartTime(lastSessionState);

    LastSession.clear();

    // Notify of changes to closed objects.
    this._notifyOfClosedObjectsChange();
  },

  /**
   * Revive a crashed tab and restore its state from before it crashed.
   *
   * @param aTab
   *        A <xul:tab> linked to a crashed browser. This is a no-op if the
   *        browser hasn't actually crashed, or is not associated with a tab.
   *        This function will also throw if the browser happens to be remote.
   */
  reviveCrashedTab(aTab) {
    if (!aTab) {
      throw new Error(
        "SessionStore.reviveCrashedTab expected a tab, but got null."
      );
    }

    let browser = aTab.linkedBrowser;
    if (!this._crashedBrowsers.has(browser.permanentKey)) {
      return;
    }

    // Sanity check - the browser to be revived should not be remote
    // at this point.
    if (browser.isRemoteBrowser) {
      throw new Error(
        "SessionStore.reviveCrashedTab: " +
          "Somehow a crashed browser is still remote."
      );
    }

    // We put the browser at about:blank in case the user is
    // restoring tabs on demand. This way, the user won't see
    // a flash of the about:tabcrashed page after selecting
    // the revived tab.
    aTab.removeAttribute("crashed");
    browser.loadURI("about:blank", {
      triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal({
        userContextId: aTab.userContextId,
      }),
      remoteTypeOverride: E10SUtils.NOT_REMOTE,
    });

    let data = TabState.collect(aTab, TAB_CUSTOM_VALUES.get(aTab));
    this.restoreTab(aTab, data, {
      forceOnDemand: true,
    });
  },

  /**
   * Revive all crashed tabs and reset the crashed tabs count to 0.
   */
  reviveAllCrashedTabs() {
    for (let window of Services.wm.getEnumerator("navigator:browser")) {
      for (let tab of window.gBrowser.tabs) {
        this.reviveCrashedTab(tab);
      }
    }
  },

  /**
   * Retrieves the latest session history information for a tab. The cached data
   * is returned immediately, but a callback may be provided that supplies
   * up-to-date data when or if it is available. The callback is passed a single
   * argument with data in the same format as the return value.
   *
   * @param tab tab to retrieve the session history for
   * @param updatedCallback function to call with updated data as the single argument
   * @returns a object containing 'index' specifying the current index, and an
   * array 'entries' containing an object for each history item.
   */
  getSessionHistory(tab, updatedCallback) {
    if (updatedCallback) {
      TabStateFlusher.flush(tab.linkedBrowser).then(() => {
        let sessionHistory = this.getSessionHistory(tab);
        if (sessionHistory) {
          updatedCallback(sessionHistory);
        }
      });
    }

    // Don't continue if the tab was closed before TabStateFlusher.flush resolves.
    if (tab.linkedBrowser) {
      let tabState = TabState.collect(tab, TAB_CUSTOM_VALUES.get(tab));
      return { index: tabState.index - 1, entries: tabState.entries };
    }
    return null;
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
    if (!aWindow) {
      return [false, false];
    }

    // We might be able to overwrite the existing tabs instead of just adding
    // the previous session's tabs to the end. This will be set if possible.
    let canOverwriteTabs = false;

    // Look at the open tabs in comparison to home pages. If all the tabs are
    // home pages then we'll end up overwriting all of them. Otherwise we'll
    // just close the tabs that match home pages. Tabs with the about:blank
    // URI will always be overwritten.
    let homePages = ["about:blank"];
    let removableTabs = [];
    let tabbrowser = aWindow.gBrowser;
    let startupPref = this._prefBranch.getIntPref("startup.page");
    if (startupPref == 1) {
      homePages = homePages.concat(HomePage.get(aWindow).split("|"));
    }

    for (let i = tabbrowser._numPinnedTabs; i < tabbrowser.tabs.length; i++) {
      let tab = tabbrowser.tabs[i];
      if (homePages.includes(tab.linkedBrowser.currentURI.spec)) {
        removableTabs.push(tab);
      }
    }

    if (tabbrowser.tabs.length == removableTabs.length) {
      canOverwriteTabs = true;
    } else {
      // If we're not overwriting all of the tabs, then close the home tabs.
      for (let i = removableTabs.length - 1; i >= 0; i--) {
        tabbrowser.removeTab(removableTabs.pop(), { animate: false });
      }
    }

    return [true, canOverwriteTabs];
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

    if (winData.sizemode != "minimized") {
      winData.sizemodeBeforeMinimized = winData.sizemode;
    }

    var hidden = WINDOW_HIDEABLE_FEATURES.filter(function(aItem) {
      return aWindow[aItem] && !aWindow[aItem].visible;
    });
    if (hidden.length) {
      winData.hidden = hidden.join(",");
    } else if (winData.hidden) {
      delete winData.hidden;
    }

    let sidebarBox = aWindow.document.getElementById("sidebar-box");
    let sidebar = sidebarBox.getAttribute("sidebarcommand");
    if (sidebar && sidebarBox.getAttribute("checked") == "true") {
      winData.sidebar = sidebar;
    } else if (winData.sidebar) {
      delete winData.sidebar;
    }
    let workspaceID = aWindow.getWorkspaceID();
    if (workspaceID) {
      winData.workspaceID = workspaceID;
    }
  },

  /**
   * gather session data as object
   * @param aUpdateAll
   *        Bool update all windows
   * @returns object
   */
  getCurrentState(aUpdateAll) {
    this._handleClosedWindows().then(() => {
      this._notifyOfClosedObjectsChange();
    });

    var activeWindow = this._getTopWindow();

    TelemetryStopwatch.start("FX_SESSION_RESTORE_COLLECT_ALL_WINDOWS_DATA_MS");
    if (RunState.isRunning) {
      // update the data for all windows with activities since the last save operation.
      let index = 0;
      for (let window of this._orderedBrowserWindows) {
        if (!this._isWindowLoaded(window)) {
          // window data is still in _statesToRestore
          continue;
        }
        if (aUpdateAll || DirtyWindows.has(window) || window == activeWindow) {
          this._collectWindowData(window);
        } else {
          // always update the window features (whose change alone never triggers a save operation)
          this._updateWindowFeatures(window);
        }
        this._windows[window.__SSi].zIndex = ++index;
      }
      DirtyWindows.clear();
    }
    TelemetryStopwatch.finish("FX_SESSION_RESTORE_COLLECT_ALL_WINDOWS_DATA_MS");

    // An array that at the end will hold all current window data.
    var total = [];
    // The ids of all windows contained in 'total' in the same order.
    var ids = [];
    // The number of window that are _not_ popups.
    var nonPopupCount = 0;
    var ix;

    // collect the data for all windows
    for (ix in this._windows) {
      if (this._windows[ix]._restoring) {
        // window data is still in _statesToRestore
        continue;
      }
      total.push(this._windows[ix]);
      ids.push(ix);
      if (!this._windows[ix].isPopup) {
        nonPopupCount++;
      }
    }

    // collect the data for all windows yet to be restored
    for (ix in this._statesToRestore) {
      for (let winData of this._statesToRestore[ix].windows) {
        total.push(winData);
        if (!winData.isPopup) {
          nonPopupCount++;
        }
      }
    }

    // shallow copy this._closedWindows to preserve current state
    let lastClosedWindowsCopy = this._closedWindows.slice();

    if (AppConstants.platform != "macosx") {
      // If no non-popup browser window remains open, return the state of the last
      // closed window(s). We only want to do this when we're actually "ending"
      // the session.
      // XXXzpao We should do this for _restoreLastWindow == true, but that has
      //        its own check for popups. c.f. bug 597619
      if (
        nonPopupCount == 0 &&
        !!lastClosedWindowsCopy.length &&
        RunState.isQuitting
      ) {
        // prepend the last non-popup browser window, so that if the user loads more tabs
        // at startup we don't accidentally add them to a popup window
        do {
          total.unshift(lastClosedWindowsCopy.shift());
        } while (total[0].isPopup && lastClosedWindowsCopy.length);
      }
    }

    if (activeWindow) {
      this.activeWindowSSiCache = activeWindow.__SSi || "";
    }
    ix = ids.indexOf(this.activeWindowSSiCache);
    // We don't want to restore focus to a minimized window or a window which had all its
    // tabs stripped out (doesn't exist).
    if (ix != -1 && total[ix] && total[ix].sizemode == "minimized") {
      ix = -1;
    }

    let session = {
      lastUpdate: Date.now(),
      startTime: this._sessionStartTime,
      recentCrashes: this._recentCrashes,
    };

    let state = {
      version: ["sessionrestore", FORMAT_VERSION],
      windows: total,
      selectedWindow: ix + 1,
      _closedWindows: lastClosedWindowsCopy,
      session,
      global: this._globalState.getState(),
    };

    // Collect and store session cookies.
    state.cookies = SessionCookies.collect();

    DevToolsShim.saveDevToolsSession(state);

    // Persist the last session if we deferred restoring it
    if (LastSession.canRestore) {
      state.lastSessionState = LastSession.getState();
    }

    // If we were called by the SessionSaver and started with only a private
    // window we want to pass the deferred initial state to not lose the
    // previous session.
    if (this._deferredInitialState) {
      state.deferredInitialState = this._deferredInitialState;
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
    if (!this._isWindowLoaded(aWindow)) {
      return this._statesToRestore[WINDOW_RESTORE_IDS.get(aWindow)];
    }

    if (RunState.isRunning) {
      this._collectWindowData(aWindow);
    }

    return { windows: [this._windows[aWindow.__SSi]] };
  },

  /**
   * Gathers data about a window and its tabs, and updates its
   * entry in this._windows.
   *
   * @param aWindow
   *        Window references.
   * @returns a Map mapping the browser tabs from aWindow to the tab
   *          entry that was put into the window data in this._windows.
   */
  _collectWindowData: function ssi_collectWindowData(aWindow) {
    let tabMap = new Map();

    if (!this._isWindowLoaded(aWindow)) {
      return tabMap;
    }

    let tabbrowser = aWindow.gBrowser;
    let tabs = tabbrowser.tabs;
    let winData = this._windows[aWindow.__SSi];
    let tabsData = (winData.tabs = []);

    // update the internal state data for this window
    for (let tab of tabs) {
      let tabData = TabState.collect(tab, TAB_CUSTOM_VALUES.get(tab));
      tabMap.set(tab, tabData);
      tabsData.push(tabData);
    }
    winData.selected = tabbrowser.tabbox.selectedIndex + 1;

    this._updateWindowFeatures(aWindow);

    // Make sure we keep __SS_lastSessionWindowID around for cases like entering
    // or leaving PB mode.
    if (aWindow.__SS_lastSessionWindowID) {
      this._windows[aWindow.__SSi].__lastSessionWindowID =
        aWindow.__SS_lastSessionWindowID;
    }

    DirtyWindows.remove(aWindow);
    return tabMap;
  },

  /* ........ Restoring Functionality .............. */

  /**
   * Open windows with data
   *
   * @param root
   *        Windows data
   * @returns a promise resolved when all windows have been opened
   */
  _openWindows(root) {
    let windowsOpened = [];
    for (let winData of root.windows) {
      if (!winData || !winData.tabs || !winData.tabs[0]) {
        continue;
      }
      windowsOpened.push(this._openWindowWithState({ windows: [winData] }));
    }
    let windowOpenedPromises = [];
    for (const openedWindow of windowsOpened) {
      let deferred = WINDOW_SHOWING_PROMISES.get(openedWindow);
      windowOpenedPromises.push(deferred.promise);
    }
    return Promise.all(windowOpenedPromises);
  },

  /**
   * restore features to a single window
   * @param aWindow
   *        Window reference to the window to use for restoration
   * @param winData
   *        JS object
   * @param aOptions
   *        {overwriteTabs: true} to overwrite existing tabs w/ new ones
   *        {firstWindow: true} if this is the first non-private window we're
   *                            restoring in this session, that might open an
   *                            external link as well
   */
  restoreWindow: function ssi_restoreWindow(aWindow, winData, aOptions = {}) {
    let overwriteTabs = aOptions && aOptions.overwriteTabs;
    let firstWindow = aOptions && aOptions.firstWindow;

    // initialize window if necessary
    if (aWindow && (!aWindow.__SSi || !this._windows[aWindow.__SSi])) {
      this.onLoad(aWindow);
    }

    TelemetryStopwatch.start("FX_SESSION_RESTORE_RESTORE_WINDOW_MS");

    // We're not returning from this before we end up calling restoreTabs
    // for this window, so make sure we send the SSWindowStateBusy event.
    this._sendWindowRestoringNotification(aWindow);
    this._setWindowStateBusy(aWindow);

    if (winData.workspaceID) {
      aWindow.moveToWorkspace(winData.workspaceID);
    }

    if (!winData.tabs) {
      winData.tabs = [];
      // don't restore a single blank tab when we've had an external
      // URL passed in for loading at startup (cf. bug 357419)
    } else if (
      firstWindow &&
      !overwriteTabs &&
      winData.tabs.length == 1 &&
      (!winData.tabs[0].entries || !winData.tabs[0].entries.length)
    ) {
      winData.tabs = [];
    }

    // See SessionStoreInternal.restoreTabs for a description of what
    // selectTab represents.
    let selectTab = 0;
    if (overwriteTabs) {
      selectTab = parseInt(winData.selected || 1, 10);
      selectTab = Math.max(selectTab, 1);
      selectTab = Math.min(selectTab, winData.tabs.length);
    }

    let tabbrowser = aWindow.gBrowser;

    // disable smooth scrolling while adding, moving, removing and selecting tabs
    let arrowScrollbox = tabbrowser.tabContainer.arrowScrollbox;
    let smoothScroll = arrowScrollbox.smoothScroll;
    arrowScrollbox.smoothScroll = false;

    // We need to keep track of the initially open tabs so that they
    // can be moved to the end of the restored tabs.
    let initialTabs;
    if (!overwriteTabs && firstWindow) {
      initialTabs = Array.from(tabbrowser.tabs);
    }

    // Get rid of tabs that aren't needed anymore.
    if (overwriteTabs) {
      for (let i = tabbrowser.browsers.length - 1; i >= 0; i--) {
        if (!tabbrowser.tabs[i].selected) {
          tabbrowser.removeTab(tabbrowser.tabs[i]);
        }
      }
    }

    let restoreTabsLazily =
      this._prefBranch.getBoolPref("sessionstore.restore_tabs_lazily") &&
      this._restore_on_demand;

    if (winData.tabs.length) {
      var tabs = tabbrowser.addMultipleTabs(
        restoreTabsLazily,
        selectTab,
        winData.tabs
      );
    }

    // Move the originally open tabs to the end.
    if (initialTabs) {
      let endPosition = tabbrowser.tabs.length - 1;
      for (let i = 0; i < initialTabs.length; i++) {
        tabbrowser.unpinTab(initialTabs[i]);
        tabbrowser.moveTabTo(initialTabs[i], endPosition);
      }
    }

    // We want to correlate the window with data from the last session, so
    // assign another id if we have one. Otherwise clear so we don't do
    // anything with it.
    delete aWindow.__SS_lastSessionWindowID;
    if (winData.__lastSessionWindowID) {
      aWindow.__SS_lastSessionWindowID = winData.__lastSessionWindowID;
    }

    if (overwriteTabs) {
      delete this._windows[aWindow.__SSi].extData;
    }

    // Restore cookies from legacy sessions, i.e. before bug 912717.
    SessionCookies.restore(winData.cookies || []);

    if (winData.extData) {
      if (!this._windows[aWindow.__SSi].extData) {
        this._windows[aWindow.__SSi].extData = {};
      }
      for (var key in winData.extData) {
        this._windows[aWindow.__SSi].extData[key] = winData.extData[key];
      }
    }

    let newClosedTabsData = winData._closedTabs || [];
    let newLastClosedTabGroupCount = winData._lastClosedTabGroupCount || -1;

    if (overwriteTabs || firstWindow) {
      // Overwrite existing closed tabs data when overwriteTabs=true
      // or we're the first window to be restored.
      this._windows[aWindow.__SSi]._closedTabs = newClosedTabsData;
    } else if (this._max_tabs_undo > 0) {
      // If we merge tabs, we also want to merge closed tabs data. We'll assume
      // the restored tabs were closed more recently and append the current list
      // of closed tabs to the new one...
      newClosedTabsData = newClosedTabsData.concat(
        this._windows[aWindow.__SSi]._closedTabs
      );

      // ... and make sure that we don't exceed the max number of closed tabs
      // we can restore.
      this._windows[aWindow.__SSi]._closedTabs = newClosedTabsData.slice(
        0,
        this._max_tabs_undo
      );
    }
    // Because newClosedTabsData are put in first, we need to
    // copy also the _lastClosedTabGroupCount.
    this._windows[
      aWindow.__SSi
    ]._lastClosedTabGroupCount = newLastClosedTabGroupCount;

    if (!this._isWindowLoaded(aWindow)) {
      // from now on, the data will come from the actual window
      delete this._statesToRestore[WINDOW_RESTORE_IDS.get(aWindow)];
      WINDOW_RESTORE_IDS.delete(aWindow);
      delete this._windows[aWindow.__SSi]._restoring;
    }

    // Restore tabs, if any.
    if (winData.tabs.length) {
      this.restoreTabs(aWindow, tabs, winData.tabs, selectTab);
    }

    // set smoothScroll back to the original value
    arrowScrollbox.smoothScroll = smoothScroll;

    TelemetryStopwatch.finish("FX_SESSION_RESTORE_RESTORE_WINDOW_MS");

    this._setWindowStateReady(aWindow);

    this._sendWindowRestoredNotification(aWindow);

    Services.obs.notifyObservers(aWindow, NOTIFY_SINGLE_WINDOW_RESTORED);

    this._sendRestoreCompletedNotifications();
  },

  /**
   * Prepare connection to host beforehand.
   *
   * @param tab
   *        Tab we are loading from.
   * @param url
   *        URL of a host.
   * @returns a flag indicates whether a connection has been made
   */
  prepareConnectionToHost(tab, url) {
    if (!url.startsWith("about:")) {
      let principal = Services.scriptSecurityManager.createNullPrincipal({
        userContextId: tab.userContextId,
      });
      let sc = Services.io.QueryInterface(Ci.nsISpeculativeConnect);
      let uri = Services.io.newURI(url);
      try {
        sc.speculativeConnect(uri, principal, null);
        return true;
      } catch (error) {
        // Can't setup speculative connection for this url.
        Cu.reportError(error);
        return false;
      }
    }
    return false;
  },

  /**
   * Make a connection to a host when users hover mouse on a tab.
   * This will also set a flag in the tab to prevent us from speculatively
   * connecting a second time.
   *
   * @param tab
   *        a tab to speculatively connect on mouse hover.
   */
  speculativeConnectOnTabHover(tab) {
    let tabState = TAB_LAZY_STATES.get(tab);
    if (tabState && !tabState.connectionPrepared) {
      let url = this.getLazyTabValue(tab, "url");
      let prepared = this.prepareConnectionToHost(tab, url);
      // This is used to test if a connection has been made beforehand.
      if (gDebuggingEnabled) {
        tab.__test_connection_prepared = prepared;
        tab.__test_connection_url = url;
      }
      // A flag indicate that we've prepared a connection for this tab and
      // if is called again, we shouldn't prepare another connection.
      tabState.connectionPrepared = true;
    }
  },

  /**
   * This function will restore window features and then retore window data.
   *
   * @param windows
   *        ordered array of windows to restore
   */
  _restoreWindowsFeaturesAndTabs(windows) {
    // First, we restore window features, so that when users start interacting
    // with a window, we don't steal the window focus.
    for (let window of windows) {
      let state = this._statesToRestore[WINDOW_RESTORE_IDS.get(window)];
      this.restoreWindowFeatures(window, state.windows[0]);
    }

    // Then we restore data into windows.
    for (let window of windows) {
      let state = this._statesToRestore[WINDOW_RESTORE_IDS.get(window)];
      this.restoreWindow(
        window,
        state.windows[0],
        state.options || { overwriteTabs: true }
      );
      WINDOW_RESTORE_ZINDICES.delete(window);
    }
  },

  /**
   * This function will restore window in reversed z-index, so that users will
   * be presented with most recently used window first.
   *
   * @param windows
   *        unordered array of windows to restore
   */
  _restoreWindowsInReversedZOrder(windows) {
    windows.sort(
      (a, b) =>
        (WINDOW_RESTORE_ZINDICES.get(a) || 0) -
        (WINDOW_RESTORE_ZINDICES.get(b) || 0)
    );

    this.windowToFocus = windows[0];
    this._restoreWindowsFeaturesAndTabs(windows);
  },

  /**
   * Restore multiple windows using the provided state.
   * @param aWindow
   *        Window reference to the first window to use for restoration.
   *        Additionally required windows will be opened.
   * @param aState
   *        JS object or JSON string
   * @param aOptions
   *        {overwriteTabs: true} to overwrite existing tabs w/ new ones
   *        {firstWindow: true} if this is the first non-private window we're
   *                            restoring in this session, that might open an
   *                            external link as well
   */
  restoreWindows: function ssi_restoreWindows(aWindow, aState, aOptions = {}) {
    // initialize window if necessary
    if (aWindow && (!aWindow.__SSi || !this._windows[aWindow.__SSi])) {
      this.onLoad(aWindow);
    }

    let root;
    try {
      root = typeof aState == "string" ? JSON.parse(aState) : aState;
    } catch (ex) {
      // invalid state object - don't restore anything
      this._log.error(ex);
      this._sendRestoreCompletedNotifications();
      return;
    }

    // Restore closed windows if any.
    if (root._closedWindows) {
      this._closedWindows = root._closedWindows;
      this._closedObjectsChanged = true;
    }

    // We're done here if there are no windows.
    if (!root.windows || !root.windows.length) {
      this._sendRestoreCompletedNotifications();
      return;
    }

    let firstWindowData = root.windows.splice(0, 1);
    // Store the restore state and restore option of the current window,
    // so that the window can be restored in reversed z-order.
    this._updateWindowRestoreState(aWindow, {
      windows: firstWindowData,
      options: aOptions,
    });

    // Begin the restoration: First open all windows in creation order. After all
    // windows have opened, we restore states to windows in reversed z-order.
    this._openWindows(root).then(windows => {
      // We want to add current window to opened window, so that this window will be
      // restored in reversed z-order. (We add the window to first position, in case
      // no z-indices are found, that window will be restored first.)
      windows.unshift(aWindow);

      this._restoreWindowsInReversedZOrder(windows);
    });

    DevToolsShim.restoreDevToolsSession(aState);
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
   *        Index of the tab to select. This is a 1-based index where "1"
   *        indicates the first tab should be selected, and "0" indicates that
   *        the currently selected tab will not be changed.
   */
  restoreTabs(aWindow, aTabs, aTabData, aSelectTab) {
    var tabbrowser = aWindow.gBrowser;

    let numTabsToRestore = aTabs.length;
    let numTabsInWindow = tabbrowser.tabs.length;
    let tabsDataArray = this._windows[aWindow.__SSi].tabs;

    // Update the window state in case we shut down without being notified.
    // Individual tab states will be taken care of by restoreTab() below.
    if (numTabsInWindow == numTabsToRestore) {
      // Remove all previous tab data.
      tabsDataArray.length = 0;
    } else {
      // Remove all previous tab data except tabs that should not be overriden.
      tabsDataArray.splice(numTabsInWindow - numTabsToRestore);
    }

    // Let the tab data array have the right number of slots.
    tabsDataArray.length = numTabsInWindow;

    if (aSelectTab > 0 && aSelectTab <= aTabs.length) {
      // Update the window state in case we shut down without being notified.
      this._windows[aWindow.__SSi].selected = aSelectTab;
    }

    // If we restore the selected tab, make sure it goes first.
    let selectedIndex = aTabs.indexOf(tabbrowser.selectedTab);
    if (selectedIndex > -1) {
      this.restoreTab(tabbrowser.selectedTab, aTabData[selectedIndex]);
    }

    // Restore all tabs.
    for (let t = 0; t < aTabs.length; t++) {
      if (t != selectedIndex) {
        this.restoreTab(aTabs[t], aTabData[t]);
      }
    }
  },

  // Restores the given tab state for a given tab.
  restoreTab(tab, tabData, options = {}) {
    let browser = tab.linkedBrowser;

    if (TAB_STATE_FOR_BROWSER.has(browser)) {
      Cu.reportError("Must reset tab before calling restoreTab.");
      return;
    }

    let loadArguments = options.loadArguments;
    let window = tab.ownerGlobal;
    let tabbrowser = window.gBrowser;
    let forceOnDemand = options.forceOnDemand;
    let isRemotenessUpdate = options.isRemotenessUpdate;

    let willRestoreImmediately =
      options.restoreImmediately || tabbrowser.selectedBrowser == browser;

    let isBrowserInserted = browser.isConnected;

    // Increase the busy state counter before modifying the tab.
    this._setWindowStateBusy(window);

    // It's important to set the window state to dirty so that
    // we collect their data for the first time when saving state.
    DirtyWindows.add(window);

    // In case we didn't collect/receive data for any tabs yet we'll have to
    // fill the array with at least empty tabData objects until |_tPos| or
    // we'll end up with |null| entries.
    for (let otherTab of Array.prototype.slice.call(
      tabbrowser.tabs,
      0,
      tab._tPos
    )) {
      let emptyState = { entries: [], lastAccessed: otherTab.lastAccessed };
      this._windows[window.__SSi].tabs.push(emptyState);
    }

    // Update the tab state in case we shut down without being notified.
    this._windows[window.__SSi].tabs[tab._tPos] = tabData;

    // Prepare the tab so that it can be properly restored.  We'll also attach
    // a copy of the tab's data in case we close it before it's been restored.
    // Anything that dispatches an event to external consumers must happen at
    // the end of this method, to make sure that the tab/browser object is in a
    // reliable and consistent state.

    if (tabData.lastAccessed) {
      tab.updateLastAccessed(tabData.lastAccessed);
    }

    if ("attributes" in tabData) {
      // Ensure that we persist tab attributes restored from previous sessions.
      Object.keys(tabData.attributes).forEach(a => TabAttributes.persist(a));
    }

    if (!tabData.entries) {
      tabData.entries = [];
    }
    if (tabData.extData) {
      TAB_CUSTOM_VALUES.set(tab, Cu.cloneInto(tabData.extData, {}));
    } else {
      TAB_CUSTOM_VALUES.delete(tab);
    }

    // Tab is now open.
    delete tabData.closedAt;

    // Ensure the index is in bounds.
    let activeIndex = (tabData.index || tabData.entries.length) - 1;
    activeIndex = Math.min(activeIndex, tabData.entries.length - 1);
    activeIndex = Math.max(activeIndex, 0);

    // Save the index in case we updated it above.
    tabData.index = activeIndex + 1;

    tab.setAttribute("pending", "true");

    // If we're restoring this tab, it certainly shouldn't be in
    // the ignored set anymore.
    this._crashedBrowsers.delete(browser.permanentKey);

    // If we're in the midst of performing a process flip, then we must
    // have initiated a navigation. This means that these userTyped*
    // values are now out of date.
    if (
      options.restoreContentReason ==
      RESTORE_TAB_CONTENT_REASON.NAVIGATE_AND_RESTORE
    ) {
      delete tabData.userTypedValue;
      delete tabData.userTypedClear;
    }

    // Update the persistent tab state cache with |tabData| information.
    TabStateCache.update(browser.permanentKey, {
      // NOTE: Copy the entries array shallowly, so as to not screw with the
      // original tabData's history when getting history updates.
      history: { entries: [...tabData.entries], index: tabData.index },
      scroll: tabData.scroll || null,
      storage: tabData.storage || null,
      formdata: tabData.formdata || null,
      disallow: tabData.disallow || null,
      userContextId: tabData.userContextId || 0,

      // This information is only needed until the tab has finished restoring.
      // When that's done it will be removed from the cache and we always
      // collect it in TabState._collectBaseTabData().
      image: tabData.image || "",
      iconLoadingPrincipal: tabData.iconLoadingPrincipal || null,
      searchMode: tabData.searchMode || null,
      userTypedValue: tabData.userTypedValue || "",
      userTypedClear: tabData.userTypedClear || 0,
    });

    // Restore tab attributes.
    if ("attributes" in tabData) {
      TabAttributes.set(tab, tabData.attributes);
    }

    if (isBrowserInserted) {
      // Start a new epoch to discard all frame script messages relating to a
      // previous epoch. All async messages that are still on their way to chrome
      // will be ignored and don't override any tab data set when restoring.
      let epoch = this.startNextEpoch(browser.permanentKey);

      // Ensure that the tab will get properly restored in the event the tab
      // crashes while restoring.  But don't set this on lazy browsers as
      // restoreTab will get called again when the browser is instantiated.
      TAB_STATE_FOR_BROWSER.set(browser, TAB_STATE_NEEDS_RESTORE);

      this._sendRestoreHistory(browser, {
        tabData,
        epoch,
        loadArguments,
        isRemotenessUpdate,
      });

      // This could cause us to ignore MAX_CONCURRENT_TAB_RESTORES a bit, but
      // it ensures each window will have its selected tab loaded.
      if (willRestoreImmediately) {
        this.restoreTabContent(tab, options);
      } else if (!forceOnDemand) {
        TabRestoreQueue.add(tab);
        // Check if a tab is in queue and will be restored
        // after the currently loading tabs. If so, prepare
        // a connection to host to speed up page loading.
        if (TabRestoreQueue.willRestoreSoon(tab)) {
          if (activeIndex in tabData.entries) {
            let url = tabData.entries[activeIndex].url;
            let prepared = this.prepareConnectionToHost(tab, url);
            if (gDebuggingEnabled) {
              tab.__test_connection_prepared = prepared;
              tab.__test_connection_url = url;
            }
          }
        }
        this.restoreNextTab();
      }
    } else {
      // TAB_LAZY_STATES holds data for lazy-browser tabs to proxy for
      // data unobtainable from the unbound browser.  This only applies to lazy
      // browsers and will be removed once the browser is inserted in the document.
      // This must preceed `updateTabLabelAndIcon` call for required data to be present.
      let url = "about:blank";
      let title = "";

      if (activeIndex in tabData.entries) {
        url = tabData.entries[activeIndex].url;
        title = tabData.entries[activeIndex].title || url;
      }
      TAB_LAZY_STATES.set(tab, {
        url,
        title,
        userTypedValue: tabData.userTypedValue || "",
        userTypedClear: tabData.userTypedClear || 0,
      });
    }

    // Most of tabData has been restored, now continue with restoring
    // attributes that may trigger external events.

    if (tabData.pinned) {
      tabbrowser.pinTab(tab);
    } else {
      tabbrowser.unpinTab(tab);
    }

    if (tabData.hidden) {
      tabbrowser.hideTab(tab);
    } else {
      tabbrowser.showTab(tab);
    }

    if (!!tabData.muted != browser.audioMuted) {
      tab.toggleMuteAudio(tabData.muteReason);
    }

    if (tab.hasAttribute("customizemode")) {
      window.gCustomizeMode.setTab(tab);
    }

    // Update tab label and icon to show something
    // while we wait for the messages to be processed.
    this.updateTabLabelAndIcon(tab, tabData);

    // Decrease the busy state counter after we're done.
    this._setWindowStateReady(window);
  },

  /**
   * Kicks off restoring the given tab.
   *
   * @param aTab
   *        the tab to restore
   * @param aOptions
   *        optional arguments used when performing process switch during load
   */
  restoreTabContent(aTab, aOptions = {}) {
    let loadArguments = aOptions.loadArguments;
    if (aTab.hasAttribute("customizemode") && !loadArguments) {
      return;
    }

    let browser = aTab.linkedBrowser;
    let window = aTab.ownerGlobal;
    let tabbrowser = window.gBrowser;
    let tabData = TabState.clone(aTab, TAB_CUSTOM_VALUES.get(aTab));
    let activeIndex = tabData.index - 1;
    let activePageData = tabData.entries[activeIndex] || null;
    let uri = activePageData ? activePageData.url || null : null;

    this.markTabAsRestoring(aTab);

    let isRemotenessUpdate = aOptions.isRemotenessUpdate;
    let explicitlyUpdateRemoteness = !Services.appinfo.sessionHistoryInParent;
    // If we aren't already updating the browser's remoteness, check if it's
    // necessary.
    if (explicitlyUpdateRemoteness && !isRemotenessUpdate) {
      isRemotenessUpdate = tabbrowser.updateBrowserRemotenessByURL(
        browser,
        uri
      );

      if (isRemotenessUpdate) {
        // We updated the remoteness, so we need to send the history down again.
        //
        // Start a new epoch to discard all frame script messages relating to a
        // previous epoch. All async messages that are still on their way to chrome
        // will be ignored and don't override any tab data set when restoring.
        let epoch = this.startNextEpoch(browser.permanentKey);

        this._sendRestoreHistory(browser, {
          tabData,
          epoch,
          loadArguments,
          isRemotenessUpdate,
        });
      }
    }

    this._sendRestoreTabContent(browser, {
      loadArguments,
      isRemotenessUpdate,
      reason:
        aOptions.restoreContentReason || RESTORE_TAB_CONTENT_REASON.SET_STATE,
    });

    // Focus the tab's content area, unless the restore is for a new tab URL or
    // was triggered by a DocumentChannel process switch.
    if (
      aTab.selected &&
      !window.isBlankPageURL(uri) &&
      !aOptions.isRemotenessUpdate
    ) {
      browser.focus();
    }
  },

  /**
   * Marks a given pending tab as restoring.
   *
   * @param aTab
   *        the pending tab to mark as restoring
   */
  markTabAsRestoring(aTab) {
    let browser = aTab.linkedBrowser;
    if (TAB_STATE_FOR_BROWSER.get(browser) != TAB_STATE_NEEDS_RESTORE) {
      throw new Error("Given tab is not pending.");
    }

    // Make sure that this tab is removed from the priority queue.
    TabRestoreQueue.remove(aTab);

    // Increase our internal count.
    this._tabsRestoringCount++;

    // Set this tab's state to restoring
    TAB_STATE_FOR_BROWSER.set(browser, TAB_STATE_RESTORING);
    aTab.removeAttribute("pending");
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
    if (RunState.isQuitting) {
      return;
    }

    // Don't exceed the maximum number of concurrent tab restores.
    if (this._tabsRestoringCount >= MAX_CONCURRENT_TAB_RESTORES) {
      return;
    }

    let tab = TabRestoreQueue.shift();
    if (tab) {
      this.restoreTabContent(tab);
    }
  },

  /**
   * Restore visibility and dimension features to a window
   * @param aWindow
   *        Window reference
   * @param aWinData
   *        Object containing session data for the window
   */
  restoreWindowFeatures: function ssi_restoreWindowFeatures(aWindow, aWinData) {
    var hidden = aWinData.hidden ? aWinData.hidden.split(",") : [];
    WINDOW_HIDEABLE_FEATURES.forEach(function(aItem) {
      aWindow[aItem].visible = !hidden.includes(aItem);
    });

    if (aWinData.isPopup) {
      this._windows[aWindow.__SSi].isPopup = true;
      if (aWindow.gURLBar) {
        aWindow.gURLBar.readOnly = true;
      }
    } else {
      delete this._windows[aWindow.__SSi].isPopup;
      if (aWindow.gURLBar) {
        aWindow.gURLBar.readOnly = false;
      }
    }

    aWindow.setTimeout(() => {
      this.restoreDimensions(
        aWindow,
        +(aWinData.width || 0),
        +(aWinData.height || 0),
        "screenX" in aWinData ? +aWinData.screenX : NaN,
        "screenY" in aWinData ? +aWinData.screenY : NaN,
        aWinData.sizemode || "",
        aWinData.sizemodeBeforeMinimized || "",
        aWinData.sidebar || ""
      );
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
   * @param aSizeModeBeforeMinimized
   *        Window size mode before window got minimized (eg: maximized)
   * @param aSidebar
   *        Sidebar command
   */
  restoreDimensions: function ssi_restoreDimensions(
    aWindow,
    aWidth,
    aHeight,
    aLeft,
    aTop,
    aSizeMode,
    aSizeModeBeforeMinimized,
    aSidebar
  ) {
    var win = aWindow;
    var _this = this;
    function win_(aName) {
      return _this._getWindowDimension(win, aName);
    }

    const dwu = win.windowUtils;
    // find available space on the screen where this window is being placed
    let screen = gScreenManager.screenForRect(aLeft, aTop, aWidth, aHeight);
    if (screen) {
      let screenLeft = {},
        screenTop = {},
        screenWidth = {},
        screenHeight = {};
      screen.GetAvailRectDisplayPix(
        screenLeft,
        screenTop,
        screenWidth,
        screenHeight
      );
      // screenX/Y are based on the origin of the screen's desktop-pixel coordinate space
      let screenLeftCss = screenLeft.value;
      let screenTopCss = screenTop.value;
      // convert screen's device pixel dimensions to CSS px dimensions
      screen.GetAvailRect(screenLeft, screenTop, screenWidth, screenHeight);
      let cssToDevScale = screen.defaultCSSScaleFactor;
      let screenRightCss = screenLeftCss + screenWidth.value / cssToDevScale;
      let screenBottomCss = screenTopCss + screenHeight.value / cssToDevScale;

      // Pull the window within the screen's bounds (allowing a little slop
      // for windows that may be deliberately placed with their border off-screen
      // as when Win10 "snaps" a window to the left/right edge -- bug 1276516).
      // First, ensure the left edge is large enough...
      if (aLeft < screenLeftCss - SCREEN_EDGE_SLOP) {
        aLeft = screenLeftCss;
      }
      // Then check the resulting right edge, and reduce it if necessary.
      let right = aLeft + aWidth;
      if (right > screenRightCss + SCREEN_EDGE_SLOP) {
        right = screenRightCss;
        // See if we can move the left edge leftwards to maintain width.
        if (aLeft > screenLeftCss) {
          aLeft = Math.max(right - aWidth, screenLeftCss);
        }
      }
      // Finally, update aWidth to account for the adjusted left and right edges.
      aWidth = right - aLeft;

      // And do the same in the vertical dimension.
      if (aTop < screenTopCss - SCREEN_EDGE_SLOP) {
        aTop = screenTopCss;
      }
      let bottom = aTop + aHeight;
      if (bottom > screenBottomCss + SCREEN_EDGE_SLOP) {
        bottom = screenBottomCss;
        if (aTop > screenTopCss) {
          aTop = Math.max(bottom - aHeight, screenTopCss);
        }
      }
      aHeight = bottom - aTop;
    }

    // Suppress animations.
    dwu.suppressAnimation(true);

    // We want to make sure users will get their animations back in case an exception is thrown.
    try {
      // only modify those aspects which aren't correct yet
      if (
        !isNaN(aLeft) &&
        !isNaN(aTop) &&
        (aLeft != win_("screenX") || aTop != win_("screenY"))
      ) {
        aWindow.moveTo(aLeft, aTop);
      }
      if (
        aWidth &&
        aHeight &&
        (aWidth != win_("width") || aHeight != win_("height")) &&
        !gResistFingerprintingEnabled
      ) {
        // Don't resize the window if it's currently maximized and we would
        // maximize it again shortly after.
        if (aSizeMode != "maximized" || win_("sizemode") != "maximized") {
          aWindow.resizeTo(aWidth, aHeight);
        }
      }
      this._windows[
        aWindow.__SSi
      ].sizemodeBeforeMinimized = aSizeModeBeforeMinimized;
      if (
        aSizeMode &&
        win_("sizemode") != aSizeMode &&
        !gResistFingerprintingEnabled
      ) {
        switch (aSizeMode) {
          case "maximized":
            aWindow.maximize();
            break;
          case "minimized":
            if (aSizeModeBeforeMinimized == "maximized") {
              aWindow.maximize();
            }
            aWindow.minimize();
            break;
          case "normal":
            aWindow.restore();
            break;
        }
      }
      let sidebarBox = aWindow.document.getElementById("sidebar-box");
      if (
        aSidebar &&
        (sidebarBox.getAttribute("sidebarcommand") != aSidebar ||
          !sidebarBox.getAttribute("checked"))
      ) {
        aWindow.SidebarUI.showInitially(aSidebar);
      }
      // since resizing/moving a window brings it to the foreground,
      // we might want to re-focus the last focused window
      if (this.windowToFocus) {
        this.windowToFocus.focus();
      }
    } finally {
      // Enable animations.
      dwu.suppressAnimation(false);
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
  saveStateDelayed(aWindow = null) {
    if (aWindow) {
      DirtyWindows.add(aWindow);
    }

    SessionSaver.runDelayed();
  },

  /* ........ Auxiliary Functions .............. */

  /**
   * Remove a closed window from the list of closed windows and indicate that
   * the change should be notified.
   *
   * @param index
   *        The index of the window in this._closedWindows.
   *
   * @returns Array of closed windows.
   */
  _removeClosedWindow(index) {
    let windows = this._closedWindows.splice(index, 1);
    this._closedObjectsChanged = true;
    return windows;
  },

  /**
   * Notifies observers that the list of closed tabs and/or windows has changed.
   * Waits a tick to allow SessionStorage a chance to register the change.
   */
  _notifyOfClosedObjectsChange() {
    if (!this._closedObjectsChanged) {
      return;
    }
    this._closedObjectsChanged = false;
    setTimeout(() => {
      Services.obs.notifyObservers(null, NOTIFY_CLOSED_OBJECTS_CHANGED);
    }, 0);
  },

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
    }
  },

  /**
   * Iterator that yields all currently opened browser windows.
   * (Might miss the most recent one.)
   * This list is in focus order, but may include minimized windows
   * before non-minimized windows.
   */
  _browserWindows: {
    *[Symbol.iterator]() {
      for (let window of BrowserWindowTracker.orderedWindows) {
        if (window.__SSi && !window.closed) {
          yield window;
        }
      }
    },
  },

  /**
   * Iterator that yields all currently opened browser windows,
   * with minimized windows last.
   * (Might miss the most recent window.)
   */
  _orderedBrowserWindows: {
    *[Symbol.iterator]() {
      let windows = BrowserWindowTracker.orderedWindows;
      windows.sort((a, b) => {
        if (
          a.windowState == a.STATE_MINIMIZED &&
          b.windowState != b.STATE_MINIMIZED
        ) {
          return 1;
        }
        if (
          a.windowState != a.STATE_MINIMIZED &&
          b.windowState == b.STATE_MINIMIZED
        ) {
          return -1;
        }
        return 0;
      });
      for (let window of windows) {
        if (window.__SSi && !window.closed) {
          yield window;
        }
      }
    },
  },

  /**
   * Returns most recent window
   * @returns Window reference
   */
  _getTopWindow: function ssi_getTopWindow() {
    return BrowserWindowTracker.getTopWindow({ allowPopups: true });
  },

  /**
   * Calls onClose for windows that are determined to be closed but aren't
   * destroyed yet, which would otherwise cause getBrowserState and
   * setBrowserState to treat them as open windows.
   */
  _handleClosedWindows: function ssi_handleClosedWindows() {
    let promises = [];
    for (let window of Services.wm.getEnumerator("navigator:browser")) {
      if (window.closed) {
        promises.push(this.onClose(window));
      }
    }
    return Promise.all(promises);
  },

  /**
   * Store a restore state of a window to this._statesToRestore. The window
   * will be given an id that can be used to get the restore state from
   * this._statesToRestore.
   *
   * @param window
   *        a reference to a window that has a state to restore
   * @param state
   *        an object containing session data
   */
  _updateWindowRestoreState(window, state) {
    // Store z-index, so that windows can be restored in reversed z-order.
    if ("zIndex" in state.windows[0]) {
      WINDOW_RESTORE_ZINDICES.set(window, state.windows[0].zIndex);
    }
    do {
      var ID = "window" + Math.random();
    } while (ID in this._statesToRestore);
    WINDOW_RESTORE_IDS.set(window, ID);
    this._statesToRestore[ID] = state;
  },

  /**
   * open a new browser window for a given session state
   * called when restoring a multi-window session
   * @param aState
   *        Object containing session data
   */
  _openWindowWithState: function ssi_openWindowWithState(aState) {
    var argString = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString
    );
    argString.data = "";

    // Build feature string
    let features = "chrome,dialog=no,suppressanimation,all";
    let winState = aState.windows[0];
    WINDOW_ATTRIBUTES.forEach(function(aFeature) {
      // Use !isNaN as an easy way to ignore sizemode and check for numbers
      if (aFeature in winState && !isNaN(winState[aFeature])) {
        features += "," + aFeature + "=" + winState[aFeature];
      }
    });

    if (winState.isPrivate) {
      features += ",private";
    }

    var window = Services.ww.openWindow(
      null,
      AppConstants.BROWSER_CHROME_URL,
      "_blank",
      features,
      argString
    );

    this._updateWindowRestoreState(window, aState);
    WINDOW_SHOWING_PROMISES.set(window, PromiseUtils.defer());

    return window;
  },

  /**
   * whether the user wants to load any other page at startup
   * (except the homepage) - needed for determining whether to overwrite the current tabs
   * C.f.: nsBrowserContentHandler's defaultArgs implementation.
   * @returns bool
   */
  _isCmdLineEmpty: function ssi_isCmdLineEmpty(aWindow, aState) {
    var pinnedOnly =
      aState.windows &&
      aState.windows.every(win => win.tabs.every(tab => tab.pinned));

    let hasFirstArgument = aWindow.arguments && aWindow.arguments[0];
    if (!pinnedOnly) {
      let defaultArgs = Cc["@mozilla.org/browser/clh;1"].getService(
        Ci.nsIBrowserHandler
      ).defaultArgs;
      if (
        aWindow.arguments &&
        aWindow.arguments[0] &&
        aWindow.arguments[0] == defaultArgs
      ) {
        hasFirstArgument = false;
      }
    }

    return !hasFirstArgument;
  },

  /**
   * on popup windows, the AppWindow's attributes seem not to be set correctly
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

    // We want to persist the size / position in normal state, so that
    // we can restore to them even if the window is currently maximized
    // or minimized. However, attributes on window object only reflect
    // the current state of the window, so when it isn't in the normal
    // sizemode, their values aren't what we want the window to restore
    // to. In that case, try to read from the attributes of the root
    // element first instead.
    if (aWindow.windowState != aWindow.STATE_NORMAL) {
      let docElem = aWindow.document.documentElement;
      let attr = parseInt(docElem.getAttribute(aAttribute), 10);
      if (attr) {
        if (aAttribute != "width" && aAttribute != "height") {
          return attr;
        }
        // Width and height attribute report the inner size, but we want
        // to store the outer size, so add the difference.
        let appWin = aWindow.docShell.treeOwner
          .QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIAppWindow);
        let diff =
          aAttribute == "width"
            ? appWin.outerToInnerWidthDifferenceInCSSPixels
            : appWin.outerToInnerHeightDifferenceInCSSPixels;
        return attr + diff;
      }
    }

    switch (aAttribute) {
      case "width":
        return aWindow.outerWidth;
      case "height":
        return aWindow.outerHeight;
      default:
        return aAttribute in aWindow ? aWindow[aAttribute] : "";
    }
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
    if (!winData || !winData.length) {
      return false;
    }

    // don't wrap a single about:sessionrestore page
    if (
      this._hasSingleTabWithURL(winData, "about:sessionrestore") ||
      this._hasSingleTabWithURL(winData, "about:welcomeback")
    ) {
      return false;
    }

    // don't automatically restore in Safe Mode
    if (Services.appinfo.inSafeMode) {
      return true;
    }

    let max_resumed_crashes = this._prefBranch.getIntPref(
      "sessionstore.max_resumed_crashes"
    );
    let sessionAge =
      aState.session &&
      aState.session.lastUpdate &&
      Date.now() - aState.session.lastUpdate;

    let decision =
      max_resumed_crashes != -1 &&
      (aRecentCrashes > max_resumed_crashes ||
        (sessionAge && sessionAge >= SIX_HOURS_IN_MS));
    if (decision) {
      let key;
      if (aRecentCrashes > max_resumed_crashes) {
        if (sessionAge && sessionAge >= SIX_HOURS_IN_MS) {
          key = "shown_many_crashes_old_session";
        } else {
          key = "shown_many_crashes";
        }
      } else {
        key = "shown_old_session";
      }
      Services.telemetry.keyedScalarAdd(
        "browser.engagement.sessionrestore_interstitial",
        key,
        1
      );
    }
    return decision;
  },

  /**
   * @param aWinData is the set of windows in session state
   * @param aURL is the single URL we're looking for
   * @returns whether the window data contains only the single URL passed
   */
  _hasSingleTabWithURL(aWinData, aURL) {
    if (
      aWinData &&
      aWinData.length == 1 &&
      aWinData[0].tabs &&
      aWinData[0].tabs.length == 1 &&
      aWinData[0].tabs[0].entries &&
      aWinData[0].tabs[0].entries.length == 1
    ) {
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
    return (
      aTabState.entries.length &&
      !(
        aTabState.entries.length == 1 &&
        (aTabState.entries[0].url == "about:blank" ||
          aTabState.entries[0].url == "about:newtab" ||
          aTabState.entries[0].url == "about:printpreview" ||
          aTabState.entries[0].url == "about:privatebrowsing") &&
        !aTabState.userTypedValue
      )
    );
  },

  /**
   * Determine if the tab state we're passed is something we should keep to be
   * reopened at session restore. This is used when we are saving the current
   * session state to disk. This method is very similar to _shouldSaveTabState,
   * however, "about:blank" and "about:newtab" tabs will still be saved to disk.
   *
   * @param aTabState
   *        The current tab state
   * @returns boolean
   */
  _shouldSaveTab: function ssi_shouldSaveTab(aTabState) {
    // If the tab has one of the following transient about: history entry, no
    // userTypedValue, and no customizemode attribute, then we don't actually
    // want to write this tab's data to disk.
    return (
      aTabState.userTypedValue ||
      (aTabState.attributes && aTabState.attributes.customizemode == "true") ||
      (aTabState.entries.length &&
        !(
          aTabState.entries[0].url == "about:printpreview" ||
          aTabState.entries[0].url == "about:privatebrowsing"
        ))
    );
  },

  /**
   * This is going to take a state as provided at startup (via
   * SessionStartup.state) and split it into 2 parts. The first part
   * (defaultState) will be a state that should still be restored at startup,
   * while the second part (state) is a state that should be saved for later.
   * defaultState will be comprised of windows with only pinned tabs, extracted
   * from state. It will also contain window position information.
   *
   * defaultState will be restored at startup. state will be passed into
   * LastSession and will be kept in case the user explicitly wants
   * to restore the previous session (publicly exposed as restoreLastSession).
   *
   * @param state
   *        The state, presumably from SessionStartup.state
   * @returns [defaultState, state]
   */
  _prepDataForDeferredRestore: function ssi_prepDataForDeferredRestore(state) {
    // Make sure that we don't modify the global state as provided by
    // SessionStartup.state.
    state = Cu.cloneInto(state, {});

    let defaultState = { windows: [], selectedWindow: 1 };

    state.selectedWindow = state.selectedWindow || 1;

    // Look at each window, remove pinned tabs, adjust selectedindex,
    // remove window if necessary.
    for (let wIndex = 0; wIndex < state.windows.length; ) {
      let window = state.windows[wIndex];
      window.selected = window.selected || 1;
      // We're going to put the state of the window into this object
      let pinnedWindowState = { tabs: [] };
      for (let tIndex = 0; tIndex < window.tabs.length; ) {
        if (window.tabs[tIndex].pinned) {
          // Adjust window.selected
          if (tIndex + 1 < window.selected) {
            window.selected -= 1;
          } else if (tIndex + 1 == window.selected) {
            pinnedWindowState.selected = pinnedWindowState.tabs.length + 1;
          }
          // + 1 because the tab isn't actually in the array yet

          // Now add the pinned tab to our window
          pinnedWindowState.tabs = pinnedWindowState.tabs.concat(
            window.tabs.splice(tIndex, 1)
          );
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
        window.__lastSessionWindowID = pinnedWindowState.__lastSessionWindowID =
          "" + Date.now() + Math.random();

        // Actually add this window to our defaultState
        defaultState.windows.push(pinnedWindowState);
        // Remove the window from the state if it doesn't have any tabs
        if (!window.tabs.length) {
          if (wIndex + 1 <= state.selectedWindow) {
            state.selectedWindow -= 1;
          } else if (wIndex + 1 == state.selectedWindow) {
            defaultState.selectedIndex = defaultState.windows.length + 1;
          }

          state.windows.splice(wIndex, 1);
          // We don't want to increment wIndex here.
          continue;
        }
      }
      wIndex++;
    }

    return [defaultState, state];
  },

  _sendRestoreCompletedNotifications: function ssi_sendRestoreCompletedNotifications() {
    // not all windows restored, yet
    if (this._restoreCount > 1) {
      this._restoreCount--;
      return;
    }

    // observers were already notified
    if (this._restoreCount == -1) {
      return;
    }

    // This was the last window restored at startup, notify observers.
    if (!this._browserSetState) {
      Services.obs.notifyObservers(null, NOTIFY_WINDOWS_RESTORED);
      this._deferredAllWindowsRestored.resolve();
    } else {
      // _browserSetState is used only by tests, and it uses an alternate
      // notification in order not to retrigger startup observers that
      // are listening for NOTIFY_WINDOWS_RESTORED.
      Services.obs.notifyObservers(null, NOTIFY_BROWSER_STATE_RESTORED);
    }

    this._browserSetState = false;
    this._restoreCount = -1;
  },

  /**
   * Set the given window's busy state
   * @param aWindow the window
   * @param aValue the window's busy state
   */
  _setWindowStateBusyValue: function ssi_changeWindowStateBusyValue(
    aWindow,
    aValue
  ) {
    this._windows[aWindow.__SSi].busy = aValue;

    // Keep the to-be-restored state in sync because that is returned by
    // getWindowState() as long as the window isn't loaded, yet.
    if (!this._isWindowLoaded(aWindow)) {
      let stateToRestore = this._statesToRestore[
        WINDOW_RESTORE_IDS.get(aWindow)
      ].windows[0];
      stateToRestore.busy = aValue;
    }
  },

  /**
   * Set the given window's state to 'not busy'.
   * @param aWindow the window
   */
  _setWindowStateReady: function ssi_setWindowStateReady(aWindow) {
    let newCount = (this._windowBusyStates.get(aWindow) || 0) - 1;
    if (newCount < 0) {
      throw new Error("Invalid window busy state (less than zero).");
    }
    this._windowBusyStates.set(aWindow, newCount);

    if (newCount == 0) {
      this._setWindowStateBusyValue(aWindow, false);
      this._sendWindowStateEvent(aWindow, "Ready");
    }
  },

  /**
   * Set the given window's state to 'busy'.
   * @param aWindow the window
   */
  _setWindowStateBusy: function ssi_setWindowStateBusy(aWindow) {
    let newCount = (this._windowBusyStates.get(aWindow) || 0) + 1;
    this._windowBusyStates.set(aWindow, newCount);

    if (newCount == 1) {
      this._setWindowStateBusyValue(aWindow, true);
      this._sendWindowStateEvent(aWindow, "Busy");
    }
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
   * Dispatch the SSWindowRestoring event for the given window.
   * @param aWindow
   *        The window which is going to be restored
   */
  _sendWindowRestoringNotification(aWindow) {
    let event = aWindow.document.createEvent("Events");
    event.initEvent("SSWindowRestoring", true, false);
    aWindow.dispatchEvent(event);
  },

  /**
   * Dispatch the SSWindowRestored event for the given window.
   * @param aWindow
   *        The window which has been restored
   */
  _sendWindowRestoredNotification(aWindow) {
    let event = aWindow.document.createEvent("Events");
    event.initEvent("SSWindowRestored", true, false);
    aWindow.dispatchEvent(event);
  },

  /**
   * Dispatch the SSTabRestored event for the given tab.
   * @param aTab
   *        The tab which has been restored
   * @param aIsRemotenessUpdate
   *        True if this tab was restored due to flip from running from
   *        out-of-main-process to in-main-process or vice-versa.
   */
  _sendTabRestoredNotification(aTab, aIsRemotenessUpdate) {
    let event = aTab.ownerDocument.createEvent("CustomEvent");
    event.initCustomEvent("SSTabRestored", true, false, {
      isRemotenessUpdate: aIsRemotenessUpdate,
    });
    aTab.dispatchEvent(event);
  },

  /**
   * @param aWindow
   *        Window reference
   * @returns whether this window's data is still cached in _statesToRestore
   *          because it's not fully loaded yet
   */
  _isWindowLoaded: function ssi_isWindowLoaded(aWindow) {
    return !WINDOW_RESTORE_IDS.has(aWindow);
  },

  /**
   * Resize this._closedWindows to the value of the pref, except in the case
   * where we don't have any non-popup windows on Windows and Linux. Then we must
   * resize such that we have at least one non-popup window.
   */
  _capClosedWindows: function ssi_capClosedWindows() {
    if (this._closedWindows.length <= this._max_windows_undo) {
      return;
    }
    let spliceTo = this._max_windows_undo;
    if (AppConstants.platform != "macosx") {
      let normalWindowIndex = 0;
      // try to find a non-popup window in this._closedWindows
      while (
        normalWindowIndex < this._closedWindows.length &&
        !!this._closedWindows[normalWindowIndex].isPopup
      ) {
        normalWindowIndex++;
      }
      if (normalWindowIndex >= this._max_windows_undo) {
        spliceTo = normalWindowIndex + 1;
      }
    }
    if (spliceTo < this._closedWindows.length) {
      this._closedWindows.splice(spliceTo, this._closedWindows.length);
      this._closedObjectsChanged = true;
    }
  },

  /**
   * Clears the set of windows that are "resurrected" before writing to disk to
   * make closing windows one after the other until shutdown work as expected.
   *
   * This function should only be called when we are sure that there has been
   * a user action that indicates the browser is actively being used and all
   * windows that have been closed before are not part of a series of closing
   * windows.
   */
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
  _resetLocalTabRestoringState(aTab) {
    let browser = aTab.linkedBrowser;

    // Keep the tab's previous state for later in this method
    let previousState = TAB_STATE_FOR_BROWSER.get(browser);

    if (!previousState) {
      Cu.reportError("Given tab is not restoring.");
      return;
    }

    // The browser is no longer in any sort of restoring state.
    TAB_STATE_FOR_BROWSER.delete(browser);

    if (Services.appinfo.sessionHistoryInParent) {
      this._restoreListeners.get(browser.permanentKey)?.unregister();
      browser.browsingContext.clearRestoreState();
    }

    aTab.removeAttribute("pending");

    if (previousState == TAB_STATE_RESTORING) {
      if (this._tabsRestoringCount) {
        this._tabsRestoringCount--;
      }
    } else if (previousState == TAB_STATE_NEEDS_RESTORE) {
      // Make sure that the tab is removed from the list of tabs to restore.
      // Again, this is normally done in restoreTabContent, but that isn't being called
      // for this tab.
      TabRestoreQueue.remove(aTab);
    }
  },

  _resetTabRestoringState(tab) {
    let browser = tab.linkedBrowser;

    if (!TAB_STATE_FOR_BROWSER.has(browser)) {
      Cu.reportError("Given tab is not restoring.");
      return;
    }

    if (!Services.appinfo.sessionHistoryInParent) {
      browser.messageManager.sendAsyncMessage("SessionStore:resetRestore", {});
    }
    this._resetLocalTabRestoringState(tab);
  },

  /**
   * Each fresh tab starts out with epoch=0. This function can be used to
   * start a next epoch by incrementing the current value. It will enables us
   * to ignore stale messages sent from previous epochs. The function returns
   * the new epoch ID for the given |browser|.
   */
  startNextEpoch(permanentKey) {
    let next = this.getCurrentEpoch(permanentKey) + 1;
    this._browserEpochs.set(permanentKey, next);
    return next;
  },

  /**
   * Returns the current epoch for the given <browser>. If we haven't assigned
   * a new epoch this will default to zero for new tabs.
   */
  getCurrentEpoch(permanentKey) {
    return this._browserEpochs.get(permanentKey) || 0;
  },

  /**
   * Each time a <browser> element is restored, we increment its "epoch". To
   * check if a message from content-sessionStore.js is out of date, we can
   * compare the epoch received with the message to the <browser> element's
   * epoch. This function does that, and returns true if |epoch| is up-to-date
   * with respect to |browser|.
   */
  isCurrentEpoch(permanentKey, epoch) {
    return this.getCurrentEpoch(permanentKey) == epoch;
  },

  /**
   * Resets the epoch for a given <browser>. We need to this every time we
   * receive a hint that a new docShell has been loaded into the browser as
   * the frame script starts out with epoch=0.
   */
  resetEpoch(permanentKey, frameLoader = null) {
    this._browserEpochs.delete(permanentKey);
    if (frameLoader) {
      frameLoader.requestEpochUpdate(0);
    }
  },

  /**
   * Countdown for a given duration, skipping beats if the computer is too busy,
   * sleeping or otherwise unavailable.
   *
   * @param {number} delay An approximate delay to wait in milliseconds (rounded
   * up to the closest second).
   *
   * @return Promise
   */
  looseTimer(delay) {
    let DELAY_BEAT = 1000;
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    let beats = Math.ceil(delay / DELAY_BEAT);
    let deferred = PromiseUtils.defer();
    timer.initWithCallback(
      function() {
        if (beats <= 0) {
          deferred.resolve();
        }
        --beats;
      },
      DELAY_BEAT,
      Ci.nsITimer.TYPE_REPEATING_PRECISE_CAN_SKIP
    );
    // Ensure that the timer is both canceled once we are done with it
    // and not garbage-collected until then.
    deferred.promise.then(
      () => timer.cancel(),
      () => timer.cancel()
    );
    return deferred;
  },

  /**
   * Builds a single nsISessionStoreRestoreData tree for the provided |formdata|
   * and |scroll| trees.
   */
  buildRestoreData(formdata, scroll) {
    function addFormEntries(root, fields, isXpath) {
      for (let [key, value] of Object.entries(fields)) {
        switch (typeof value) {
          case "string":
            root.addTextField(isXpath, key, value);
            break;
          case "boolean":
            root.addCheckbox(isXpath, key, value);
            break;
          case "object": {
            if (value === null) {
              break;
            }
            if (
              value.hasOwnProperty("type") &&
              value.hasOwnProperty("fileList")
            ) {
              root.addFileList(isXpath, key, value.type, value.fileList);
              break;
            }
            if (
              value.hasOwnProperty("selectedIndex") &&
              value.hasOwnProperty("value")
            ) {
              root.addSingleSelect(
                isXpath,
                key,
                value.selectedIndex,
                value.value
              );
              break;
            }
            if (
              key === "sessionData" &&
              ["about:sessionrestore", "about:welcomeback"].includes(
                formdata.url
              )
            ) {
              root.addTextField(isXpath, key, JSON.stringify(value));
              break;
            }
            if (Array.isArray(value)) {
              root.addMultipleSelect(isXpath, key, value);
              break;
            }
          }
        }
      }
    }

    let root = SessionStoreUtils.constructSessionStoreRestoreData();
    if (scroll?.hasOwnProperty("scroll")) {
      root.scroll = scroll.scroll;
    }
    if (formdata?.hasOwnProperty("url")) {
      root.url = formdata.url;
      if (formdata.hasOwnProperty("innerHTML")) {
        // eslint-disable-next-line no-unsanitized/property
        root.innerHTML = formdata.innerHTML;
      }
      if (formdata.hasOwnProperty("xpath")) {
        addFormEntries(root, formdata.xpath, /* isXpath */ true);
      }
      if (formdata.hasOwnProperty("id")) {
        addFormEntries(root, formdata.id, /* isXpath */ false);
      }
    }
    let childrenLength = Math.max(
      scroll?.children?.length || 0,
      formdata?.children?.length || 0
    );
    for (let i = 0; i < childrenLength; i++) {
      root.addChild(
        this.buildRestoreData(formdata?.children?.[i], scroll?.children?.[i]),
        i
      );
    }
    return root;
  },

  _waitForStateStop(browser, expectedURL = null) {
    const deferred = PromiseUtils.defer();

    const listener = {
      unregister(reject = true) {
        if (reject) {
          deferred.reject();
        }

        SessionStoreInternal._restoreListeners.delete(browser.permanentKey);

        try {
          browser.removeProgressListener(
            this,
            Ci.nsIWebProgress.NOTIFY_STATE_WINDOW
          );
        } catch {} // May have already gotten rid of the browser's webProgress.
      },

      onStateChange(webProgress, request, stateFlags, status) {
        if (
          webProgress.isTopLevel &&
          stateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW &&
          stateFlags & Ci.nsIWebProgressListener.STATE_STOP
        ) {
          // FIXME: We sometimes see spurious STATE_STOP events for about:blank
          // loads, so we have to account for that here.
          let aboutBlankOK = !expectedURL || expectedURL === "about:blank";
          let url = request.QueryInterface(Ci.nsIChannel).originalURI.spec;
          if (url !== "about:blank" || aboutBlankOK) {
            this.unregister(false);
            deferred.resolve();
          }
        }
      },

      QueryInterface: ChromeUtils.generateQI([
        "nsIWebProgressListener",
        "nsISupportsWeakReference",
      ]),
    };

    this._restoreListeners.get(browser.permanentKey)?.unregister();
    this._restoreListeners.set(browser.permanentKey, listener);

    browser.addProgressListener(
      listener,
      Ci.nsIWebProgress.NOTIFY_STATE_WINDOW
    );

    return deferred.promise;
  },

  _listenForNavigations(browser, callbacks) {
    const listener = {
      unregister() {
        browser.browsingContext?.sessionHistory?.removeSHistoryListener(this);

        try {
          browser.removeProgressListener(
            this,
            Ci.nsIWebProgress.NOTIFY_STATE_WINDOW
          );
        } catch {} // May have already gotten rid of the browser's webProgress.

        SessionStoreInternal._restoreListeners.delete(browser.permanentKey);
      },

      OnHistoryReload() {
        this.unregister();
        return callbacks.onHistoryReload();
      },

      // TODO(kashav): ContentRestore.jsm handles OnHistoryNewEntry separately,
      // so we should eventually support that here as well.
      OnHistoryNewEntry() {},
      OnHistoryGotoIndex() {},
      OnHistoryPurge() {},
      OnHistoryReplaceEntry() {},

      onStateChange(webProgress, request, stateFlags, status) {
        if (
          webProgress.isTopLevel &&
          stateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW &&
          stateFlags & Ci.nsIWebProgressListener.STATE_START
        ) {
          this.unregister();
          callbacks.onStartRequest();
        }
      },

      QueryInterface: ChromeUtils.generateQI([
        "nsISHistoryListener",
        "nsIWebProgressListener",
        "nsISupportsWeakReference",
      ]),
    };

    this._restoreListeners.get(browser.permanentKey)?.unregister();
    this._restoreListeners.set(browser.permanentKey, listener);

    browser.browsingContext?.sessionHistory?.addSHistoryListener(listener);

    browser.addProgressListener(
      listener,
      Ci.nsIWebProgress.NOTIFY_STATE_WINDOW
    );
  },

  /**
   * This mirrors ContentRestore.restoreHistory() for parent process session
   * history restores.
   */
  _restoreHistory(browser, data) {
    if (!Services.appinfo.sessionHistoryInParent) {
      throw new Error("This function should only be used with SHIP");
    }

    this._tabStateToRestore.set(browser.permanentKey, data);

    // In case about:blank isn't done yet.
    // XXX(kashav): Does this actually accomplish anything? Can we remove?
    browser.stop();

    SessionHistory.restoreFromParent(
      browser.browsingContext.sessionHistory,
      data.tabData
    );

    let url = data.tabData?.entries[data.tabData.index - 1]?.url;
    let disallow = data.tabData?.disallow;

    let promise = SessionStoreUtils.restoreDocShellState(
      browser.browsingContext,
      url,
      disallow
    );
    this._tabStateRestorePromises.set(browser.permanentKey, promise);

    const onResolve = () => {
      if (TAB_STATE_FOR_BROWSER.get(browser) !== TAB_STATE_RESTORING) {
        this._listenForNavigations(browser, {
          // The history entry was reloaded before we began restoring tab
          // content, just proceed as we would normally.
          onHistoryReload: () => {
            this._restoreTabContent(browser);
            return false;
          },

          // Some foreign code, like an extension, loaded a new URI on the
          // browser. We no longer want to restore saved tab data, but may
          // still have browser state that needs to be restored.
          onStartRequest: () => {
            this._tabStateToRestore.delete(browser.permanentKey);
            this._restoreTabContent(browser);
          },
        });
      }

      this._tabStateRestorePromises.delete(browser.permanentKey);

      this._restoreHistoryComplete(browser, data);
    };

    promise.then(onResolve).catch(() => {});
  },

  /**
   * Either load the saved typed value or restore the active history entry.
   * If neither is possible, just load an empty document.
   */
  _restoreTabEntry(browser, tabData) {
    let url = "about:blank";
    let loadFlags = Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_HISTORY;

    if (tabData.userTypedValue && tabData.userTypedClear) {
      url = tabData.userTypedValue;
      loadFlags = Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
    } else if (tabData.entries.length) {
      return SessionStoreUtils.initializeRestore(
        browser.browsingContext,
        this.buildRestoreData(tabData.formdata, tabData.scroll)
      );
    }

    let loadPromise = this._waitForStateStop(browser, url);

    browser.browsingContext.loadURI(url, {
      loadFlags,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });

    return loadPromise;
  },

  /**
   * This mirrors ContentRestore.restoreTabContent() for parent process session
   * history restores.
   */
  _restoreTabContent(browser, options = {}) {
    if (!Services.appinfo.sessionHistoryInParent) {
      throw new Error("This function should only be used with SHIP");
    }

    this._restoreListeners.get(browser.permanentKey)?.unregister();

    this._restoreTabContentStarted(browser, options);

    let state = this._tabStateToRestore.get(browser.permanentKey);
    this._tabStateToRestore.delete(browser.permanentKey);

    let promises = [this._tabStateRestorePromises.get(browser.permanentKey)];

    if (state) {
      promises.push(this._restoreTabEntry(browser, state.tabData));
    } else {
      // The browser started another load, so we decided to not restore
      // saved tab data. We should still wait for that new load to finish
      // before proceeding.
      promises.push(this._waitForStateStop(browser));
    }

    Promise.allSettled(promises).then(() => {
      this._restoreTabContentComplete(browser, options);
    });
  },

  _sendRestoreTabContent(browser, options) {
    if (Services.appinfo.sessionHistoryInParent) {
      this._restoreTabContent(browser, options);
    } else {
      browser.messageManager.sendAsyncMessage(
        "SessionStore:restoreTabContent",
        options
      );
    }
  },

  _restoreHistoryComplete(browser, data) {
    let win = browser.ownerGlobal;
    let tab = win?.gBrowser.getTabForBrowser(browser);
    if (!tab) {
      return;
    }

    // Notify the tabbrowser that the tab chrome has been restored.
    let tabData = TabState.collect(tab, TAB_CUSTOM_VALUES.get(tab));

    // wall-paper fix for bug 439675: make sure that the URL to be loaded
    // is always visible in the address bar if no other value is present
    let activePageData = tabData.entries[tabData.index - 1] || null;
    let uri = activePageData ? activePageData.url || null : null;
    // NB: we won't set initial URIs (about:home, about:newtab, etc.) here
    // because their load will not normally trigger a location bar clearing
    // when they finish loading (to avoid race conditions where we then
    // clear user input instead), so we shouldn't set them here either.
    // They also don't fall under the issues in bug 439675 where user input
    // needs to be preserved if the load doesn't succeed.
    // We also don't do this for remoteness updates, where it should not
    // be necessary.
    if (
      !browser.userTypedValue &&
      uri &&
      !data.isRemotenessUpdate &&
      !win.gInitialPages.includes(uri)
    ) {
      browser.userTypedValue = uri;
    }

    // Update tab label and icon again after the tab history was updated.
    this.updateTabLabelAndIcon(tab, tabData);

    let event = win.document.createEvent("Events");
    event.initEvent("SSTabRestoring", true, false);
    tab.dispatchEvent(event);
  },

  _restoreTabContentStarted(browser, data) {
    let win = browser.ownerGlobal;
    let tab = win?.gBrowser.getTabForBrowser(browser);
    if (!tab) {
      return;
    }

    let initiatedBySessionStore =
      TAB_STATE_FOR_BROWSER.get(browser) != TAB_STATE_NEEDS_RESTORE;
    let isNavigateAndRestore =
      data.reason == RESTORE_TAB_CONTENT_REASON.NAVIGATE_AND_RESTORE;

    // We need to be careful when restoring the urlbar's search mode because
    // we race a call to gURLBar.setURI due to the location change.  setURI
    // will exit search mode and set gURLBar.value to the restored URL,
    // clobbering any search mode and userTypedValue we restore here.  If
    // this is a typical restore -- restoring on startup or restoring a
    // closed tab for example -- then we need to restore search mode after
    // that setURI call, and so we wait until restoreTabContentComplete, at
    // which point setURI will have been called.  If this is not a typical
    // restore -- it was not initiated by session store or it's due to a
    // remoteness change -- then we do not want to restore search mode at
    // all, and so we remove it from the tab state cache.  In particular, if
    // the restore is due to a remoteness change, then the user is loading a
    // new URL and the current search mode should not be carried over to it.
    let cacheState = TabStateCache.get(browser.permanentKey);
    if (cacheState.searchMode) {
      if (!initiatedBySessionStore || isNavigateAndRestore) {
        TabStateCache.update(browser.permanentKey, {
          searchMode: null,
          userTypedValue: null,
        });
      }
      return;
    }

    if (!initiatedBySessionStore) {
      // If a load not initiated by sessionstore was started in a
      // previously pending tab. Mark the tab as no longer pending.
      this.markTabAsRestoring(tab);
    } else if (!isNavigateAndRestore) {
      // If the user was typing into the URL bar when we crashed, but hadn't hit
      // enter yet, then we just need to write that value to the URL bar without
      // loading anything. This must happen after the load, as the load will clear
      // userTypedValue.
      //
      // Note that we only want to do that if we're restoring state for reasons
      // _other_ than a navigateAndRestore remoteness-flip, as such a flip
      // implies that the user was navigating.
      let tabData = TabState.collect(tab, TAB_CUSTOM_VALUES.get(tab));
      if (
        tabData.userTypedValue &&
        !tabData.userTypedClear &&
        !browser.userTypedValue
      ) {
        browser.userTypedValue = tabData.userTypedValue;
        if (tab.selected) {
          win.gURLBar.setURI();
        }
      }

      // Remove state we don't need any longer.
      TabStateCache.update(browser.permanentKey, {
        userTypedValue: null,
        userTypedClear: null,
      });
    }
  },

  _restoreTabContentComplete(browser, data) {
    let win = browser.ownerGlobal;
    let tab = browser.ownerGlobal?.gBrowser.getTabForBrowser(browser);
    if (!tab) {
      return;
    }
    // Restore search mode and its search string in userTypedValue, if
    // appropriate.
    let cacheState = TabStateCache.get(browser.permanentKey);
    if (cacheState.searchMode) {
      win.gURLBar.setSearchMode(cacheState.searchMode, browser);
      browser.userTypedValue = cacheState.userTypedValue;
      if (tab.selected) {
        win.gURLBar.setURI();
      }
      TabStateCache.update(browser.permanentKey, {
        searchMode: null,
        userTypedValue: null,
      });
    }

    // This callback is used exclusively by tests that want to
    // monitor the progress of network loads.
    if (gDebuggingEnabled) {
      Services.obs.notifyObservers(browser, NOTIFY_TAB_RESTORED);
    }

    SessionStoreInternal._resetLocalTabRestoringState(tab);
    SessionStoreInternal.restoreNextTab();

    this._sendTabRestoredNotification(tab, data.isRemotenessUpdate);

    Services.obs.notifyObservers(null, "sessionstore-one-or-no-tab-restored");
  },

  /**
   * Send the "SessionStore:restoreHistory" message to content, triggering a
   * content restore. This method is intended to be used internally by
   * SessionStore, as it also ensures that permissions are avaliable in the
   * content process before triggering the history restore in the content
   * process.
   *
   * @param browser The browser to transmit the permissions for
   * @param options The options data to send to content.
   */
  _sendRestoreHistory(browser, options) {
    if (options.tabData.storage) {
      SessionStoreUtils.restoreSessionStorageFromParent(
        browser.browsingContext,
        options.tabData.storage
      );
      delete options.tabData.storage;
    }

    if (Services.appinfo.sessionHistoryInParent) {
      this._restoreHistory(browser, options);
    } else {
      browser.messageManager.sendAsyncMessage(
        "SessionStore:restoreHistory",
        options
      );
    }

    if (browser && browser.frameLoader) {
      browser.frameLoader.requestEpochUpdate(options.epoch);
    }
  },

  // Flush out session history state so that it can be used to restore the state
  // into a new process in `finishTabRemotenessChange`.
  //
  // NOTE: This codepath is temporary while the Fission Session History rewrite
  // is in process, and will be removed & replaced once that rewrite is
  // complete. (bug 1645062)
  async prepareToChangeRemoteness(aBrowser) {
    aBrowser.messageManager.sendAsyncMessage(
      "SessionStore:prepareForProcessChange"
    );
    await TabStateFlusher.flush(aBrowser);
  },

  // Handle finishing the remoteness change for a tab by restoring session
  // history state into it, and resuming the ongoing network load.
  //
  // NOTE: This codepath is temporary while the Fission Session History rewrite
  // is in process, and will be removed & replaced once that rewrite is
  // complete. (bug 1645062)
  finishTabRemotenessChange(aTab, aSwitchId) {
    let window = aTab.ownerGlobal;
    if (!window || !window.__SSi || window.closed) {
      return;
    }

    let tabState = TabState.clone(aTab, TAB_CUSTOM_VALUES.get(aTab));
    let options = {
      restoreImmediately: true,
      restoreContentReason: RESTORE_TAB_CONTENT_REASON.NAVIGATE_AND_RESTORE,
      isRemotenessUpdate: true,
      loadArguments: {
        redirectLoadSwitchId: aSwitchId,
        // As we're resuming a load which has been redirected from another
        // process, record the history index which is currently being requested.
        // It has to be offset by 1 to get back to native history indices from
        // SessionStore history indicies.
        redirectHistoryIndex: tabState.requestedIndex - 1,
      },
    };

    // Need to reset restoring tabs.
    if (TAB_STATE_FOR_BROWSER.has(aTab.linkedBrowser)) {
      this._resetLocalTabRestoringState(aTab);
    }

    // Restore the state into the tab.
    this.restoreTab(aTab, tabState, options);
  },
};

/**
 * Priority queue that keeps track of a list of tabs to restore and returns
 * the tab we should restore next, based on priority rules. We decide between
 * pinned, visible and hidden tabs in that and FIFO order. Hidden tabs are only
 * restored with restore_hidden_tabs=true.
 */
var TabRestoreQueue = {
  // The separate buckets used to store tabs.
  tabs: { priority: [], visible: [], hidden: [] },

  // Preferences used by the TabRestoreQueue to determine which tabs
  // are restored automatically and which tabs will be on-demand.
  prefs: {
    // Lazy getter that returns whether tabs are restored on demand.
    get restoreOnDemand() {
      let updateValue = () => {
        let value = Services.prefs.getBoolPref(PREF);
        let definition = { value, configurable: true };
        Object.defineProperty(this, "restoreOnDemand", definition);
        return value;
      };

      const PREF = "browser.sessionstore.restore_on_demand";
      Services.prefs.addObserver(PREF, updateValue);
      return updateValue();
    },

    // Lazy getter that returns whether pinned tabs are restored on demand.
    get restorePinnedTabsOnDemand() {
      let updateValue = () => {
        let value = Services.prefs.getBoolPref(PREF);
        let definition = { value, configurable: true };
        Object.defineProperty(this, "restorePinnedTabsOnDemand", definition);
        return value;
      };

      const PREF = "browser.sessionstore.restore_pinned_tabs_on_demand";
      Services.prefs.addObserver(PREF, updateValue);
      return updateValue();
    },

    // Lazy getter that returns whether we should restore hidden tabs.
    get restoreHiddenTabs() {
      let updateValue = () => {
        let value = Services.prefs.getBoolPref(PREF);
        let definition = { value, configurable: true };
        Object.defineProperty(this, "restoreHiddenTabs", definition);
        return value;
      };

      const PREF = "browser.sessionstore.restore_hidden_tabs";
      Services.prefs.addObserver(PREF, updateValue);
      return updateValue();
    },
  },

  // Resets the queue and removes all tabs.
  reset() {
    this.tabs = { priority: [], visible: [], hidden: [] };
  },

  // Adds a tab to the queue and determines its priority bucket.
  add(tab) {
    let { priority, hidden, visible } = this.tabs;

    if (tab.pinned) {
      priority.push(tab);
    } else if (tab.hidden) {
      hidden.push(tab);
    } else {
      visible.push(tab);
    }
  },

  // Removes a given tab from the queue, if it's in there.
  remove(tab) {
    let { priority, hidden, visible } = this.tabs;

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
  shift() {
    let set;
    let { priority, hidden, visible } = this.tabs;

    let { restoreOnDemand, restorePinnedTabsOnDemand } = this.prefs;
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
  hiddenToVisible(tab) {
    let { hidden, visible } = this.tabs;
    let index = hidden.indexOf(tab);

    if (index > -1) {
      hidden.splice(index, 1);
      visible.push(tab);
    }
  },

  // Moves a given tab from the 'visible' to the 'hidden' bucket.
  visibleToHidden(tab) {
    let { visible, hidden } = this.tabs;
    let index = visible.indexOf(tab);

    if (index > -1) {
      visible.splice(index, 1);
      hidden.push(tab);
    }
  },

  /**
   * Returns true if the passed tab is in one of the sets that we're
   * restoring content in automatically.
   *
   * @param tab (<xul:tab>)
   *        The tab to check
   * @returns bool
   */
  willRestoreSoon(tab) {
    let { priority, hidden, visible } = this.tabs;
    let {
      restoreOnDemand,
      restorePinnedTabsOnDemand,
      restoreHiddenTabs,
    } = this.prefs;
    let restorePinned = !(restoreOnDemand && restorePinnedTabsOnDemand);
    let candidateSet = [];

    if (restorePinned && priority.length) {
      candidateSet.push(...priority);
    }

    if (!restoreOnDemand) {
      if (visible.length) {
        candidateSet.push(...visible);
      }

      if (restoreHiddenTabs && hidden.length) {
        candidateSet.push(...hidden);
      }
    }

    return candidateSet.indexOf(tab) > -1;
  },
};

// A map storing a closed window's state data until it goes aways (is GC'ed).
// This ensures that API clients can still read (but not write) states of
// windows they still hold a reference to but we don't.
var DyingWindowCache = {
  _data: new WeakMap(),

  has(window) {
    return this._data.has(window);
  },

  get(window) {
    return this._data.get(window);
  },

  set(window, data) {
    this._data.set(window, data);
  },

  remove(window) {
    this._data.delete(window);
  },
};

// A weak set of dirty windows. We use it to determine which windows we need to
// recollect data for when getCurrentState() is called.
var DirtyWindows = {
  _data: new WeakMap(),

  has(window) {
    return this._data.has(window);
  },

  add(window) {
    return this._data.set(window, true);
  },

  remove(window) {
    this._data.delete(window);
  },

  clear(window) {
    this._data = new WeakMap();
  },
};

// The state from the previous session (after restoring pinned tabs). This
// state is persisted and passed through to the next session during an app
// restart to make the third party add-on warning not trash the deferred
// session
var LastSession = {
  _state: null,

  get canRestore() {
    return !!this._state;
  },

  getState() {
    return this._state;
  },

  setState(state) {
    this._state = state;
  },

  clear(silent = false) {
    if (this._state) {
      this._state = null;
      if (!silent) {
        Services.obs.notifyObservers(null, NOTIFY_LAST_SESSION_CLEARED);
      }
    }
  },
};

// Exposed for tests
const _LastSession = LastSession;
