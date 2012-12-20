/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Session Storage and Restoration
 *
 * Overview
 * This service reads user's session file at startup, and makes a determination
 * as to whether the session should be restored. It will restore the session
 * under the circumstances described below.  If the auto-start Private Browsing
 * mode is active, however, the session is never restored.
 *
 * Crash Detection
 * The session file stores a session.state property, that
 * indicates whether the browser is currently running. When the browser shuts
 * down, the field is changed to "stopped". At startup, this field is read, and
 * if its value is "running", then it's assumed that the browser had previously
 * crashed, or at the very least that something bad happened, and that we should
 * restore the session.
 *
 * Forced Restarts
 * In the event that a restart is required due to application update or extension
 * installation, set the browser.sessionstore.resume_session_once pref to true,
 * and the session will be restored the next time the browser starts.
 *
 * Always Resume
 * This service will always resume the session if the integer pref
 * browser.startup.page is set to 3.
 */

/* :::::::: Constants and Helpers ::::::::::::::: */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/TelemetryStopwatch.jsm");
Cu.import("resource://gre/modules/PrivateBrowsingUtils.jsm");
Cu.import("resource://gre/modules/commonjs/promise/core.js");

XPCOMUtils.defineLazyModuleGetter(this, "_SessionFile",
  "resource:///modules/sessionstore/_SessionFile.jsm");

const STATE_RUNNING_STR = "running";

function debug(aMsg) {
  aMsg = ("SessionStartup: " + aMsg).replace(/\S{80}/g, "$&\n");
  Services.console.logStringMessage(aMsg);
}

let gOnceInitializedDeferred = Promise.defer();

/* :::::::: The Service ::::::::::::::: */

function SessionStartup() {
}

SessionStartup.prototype = {

  // the state to restore at startup
  _initialState: null,
  _sessionType: Ci.nsISessionStartup.NO_SESSION,
  _initialized: false,

/* ........ Global Event Handlers .............. */

  /**
   * Initialize the component
   */
  init: function sss_init() {
    debug("init starting");
    // do not need to initialize anything in auto-started private browsing sessions
    let pbs = Cc["@mozilla.org/privatebrowsing;1"].
              getService(Ci.nsIPrivateBrowsingService);
    if (PrivateBrowsingUtils.permanentPrivateBrowsing ||
        pbs.lastChangedByCommandLine)
      return;
    // Session state is unknown until we read the file.
    this._sessionType = null;
    _SessionFile.read().then(
      this._onSessionFileRead.bind(this)
    );
    debug("init launched");
  },

  // Wrap a string as a nsISupports
  _createSupportsString: function ssfi_createSupportsString(aData) {
    let string = Cc["@mozilla.org/supports-string;1"]
                   .createInstance(Ci.nsISupportsString);
    string.data = aData;
    return string;
  },

  _onSessionFileRead: function sss_onSessionFileRead(aStateString) {
    debug("onSessionFileRead ");
    if (this._initialized) {
      debug("onSessionFileRead: Initialization is already complete");
      // Initialization is complete, nothing else to do
      return;
    }
    try {
      this._initialized = true;

      // Let observers modify the state before it is used
      let supportsStateString = this._createSupportsString(aStateString);
      Services.obs.notifyObservers(supportsStateString, "sessionstore-state-read", "");
      aStateString = supportsStateString.data;

      // No valid session found.
      if (!aStateString) {
        this._sessionType = Ci.nsISessionStartup.NO_SESSION;
        return;
      }

      // parse the session state into a JS object
      // remove unneeded braces (added for compatibility with Firefox 2.0 and 3.0)
      if (aStateString.charAt(0) == '(')
        aStateString = aStateString.slice(1, -1);
      let corruptFile = false;
      try {
        this._initialState = JSON.parse(aStateString);
      }
      catch (ex) {
        debug("The session file contained un-parse-able JSON: " + ex);
        // This is not valid JSON, but this might still be valid JavaScript,
        // as used in FF2/FF3, so we need to eval.
        // evalInSandbox will throw if aStateString is not parse-able.
        try {
          var s = new Cu.Sandbox("about:blank", {sandboxName: 'nsSessionStartup'});
          this._initialState = Cu.evalInSandbox("(" + aStateString + ")", s);
        } catch(ex) {
          debug("The session file contained un-eval-able JSON: " + ex);
          corruptFile = true;
        }
      }
      Services.telemetry.getHistogramById("FX_SESSION_RESTORE_CORRUPT_FILE").add(corruptFile);

      let doResumeSessionOnce = Services.prefs.getBoolPref("browser.sessionstore.resume_session_once");
      let doResumeSession = doResumeSessionOnce ||
            Services.prefs.getIntPref("browser.startup.page") == 3;

      // If this is a normal restore then throw away any previous session
      if (!doResumeSessionOnce)
        delete this._initialState.lastSessionState;

      let resumeFromCrash = Services.prefs.getBoolPref("browser.sessionstore.resume_from_crash");
      let lastSessionCrashed =
        this._initialState && this._initialState.session &&
        this._initialState.session.state &&
        this._initialState.session.state == STATE_RUNNING_STR;

      // Report shutdown success via telemetry. Shortcoming here are
      // being-killed-by-OS-shutdown-logic, shutdown freezing after
      // session restore was written, etc.
      Services.telemetry.getHistogramById("SHUTDOWN_OK").add(!lastSessionCrashed);

      // set the startup type
      if (lastSessionCrashed && resumeFromCrash)
        this._sessionType = Ci.nsISessionStartup.RECOVER_SESSION;
      else if (!lastSessionCrashed && doResumeSession)
        this._sessionType = Ci.nsISessionStartup.RESUME_SESSION;
      else if (this._initialState)
        this._sessionType = Ci.nsISessionStartup.DEFER_SESSION;
      else
        this._initialState = null; // reset the state

      // wait for the first browser window to open
      // Don't reset the initial window's default args (i.e. the home page(s))
      // if all stored tabs are pinned.
      if (this.doRestore() &&
          (!this._initialState.windows ||
           !this._initialState.windows.every(function (win)
             win.tabs.every(function (tab) tab.pinned))))
        Services.obs.addObserver(this, "domwindowopened", true);

      Services.obs.addObserver(this, "sessionstore-windows-restored", true);

      if (this._sessionType != Ci.nsISessionStartup.NO_SESSION)
        Services.obs.addObserver(this, "browser:purge-session-history", true);

    } finally {
      // We're ready. Notify everyone else.
      Services.obs.notifyObservers(null, "sessionstore-state-finalized", "");
      gOnceInitializedDeferred.resolve();
    }
  },

  /**
   * Handle notifications
   */
  observe: function sss_observe(aSubject, aTopic, aData) {
    switch (aTopic) {
    case "app-startup":
      Services.obs.addObserver(this, "final-ui-startup", true);
      Services.obs.addObserver(this, "quit-application", true);
      break;
    case "final-ui-startup":
      Services.obs.removeObserver(this, "final-ui-startup");
      Services.obs.removeObserver(this, "quit-application");
      this.init();
      break;
    case "quit-application":
      // no reason for initializing at this point (cf. bug 409115)
      Services.obs.removeObserver(this, "final-ui-startup");
      Services.obs.removeObserver(this, "quit-application");
      if (this._sessionType != Ci.nsISessionStartup.NO_SESSION)
        Services.obs.removeObserver(this, "browser:purge-session-history");
      break;
    case "domwindowopened":
      var window = aSubject;
      var self = this;
      window.addEventListener("load", function() {
        self._onWindowOpened(window);
        window.removeEventListener("load", arguments.callee, false);
      }, false);
      break;
    case "sessionstore-windows-restored":
      Services.obs.removeObserver(this, "sessionstore-windows-restored");
      // free _initialState after nsSessionStore is done with it
      this._initialState = null;
      break;
    case "browser:purge-session-history":
      Services.obs.removeObserver(this, "browser:purge-session-history");
      // reset all state on sanitization
      this._sessionType = Ci.nsISessionStartup.NO_SESSION;
      break;
    }
  },

  /**
   * Removes the default arguments from the first browser window
   * (and removes the "domwindowopened" observer afterwards).
   */
  _onWindowOpened: function sss_onWindowOpened(aWindow) {
    var wType = aWindow.document.documentElement.getAttribute("windowtype");
    if (wType != "navigator:browser")
      return;

    /**
     * Note: this relies on the fact that nsBrowserContentHandler will return
     * a different value the first time its getter is called after an update,
     * due to its needHomePageOverride() logic. We don't want to remove the
     * default arguments in the update case, since they include the "What's
     * New" page.
     *
     * Since we're garanteed to be at least the second caller of defaultArgs
     * (nsBrowserContentHandler calls it to determine which arguments to pass
     * at startup), we know that if the window's arguments don't match the
     * current defaultArguments, we're either in the update case, or we're
     * launching a non-default browser window, so we shouldn't remove the
     * window's arguments.
     */
    var defaultArgs = Cc["@mozilla.org/browser/clh;1"].
                      getService(Ci.nsIBrowserHandler).defaultArgs;
    if (aWindow.arguments && aWindow.arguments[0] &&
        aWindow.arguments[0] == defaultArgs)
      aWindow.arguments[0] = null;

    try {
      Services.obs.removeObserver(this, "domwindowopened");
    } catch (e) {
      // This might throw if we're removing the observer multiple times,
      // but this is safe to ignore.
    }
  },

/* ........ Public API ................*/

  get onceInitialized() {
    return gOnceInitializedDeferred.promise;
  },

  /**
   * Get the session state as a jsval
   */
  get state() {
    this._ensureInitialized();
    return this._initialState;
  },

  /**
   * Determine whether there is a pending session restore.
   * @returns bool
   */
  doRestore: function sss_doRestore() {
    this._ensureInitialized();
    return this._sessionType == Ci.nsISessionStartup.RECOVER_SESSION ||
           this._sessionType == Ci.nsISessionStartup.RESUME_SESSION;
  },

  /**
   * Get the type of pending session store, if any.
   */
  get sessionType() {
    this._ensureInitialized();
    return this._sessionType;
  },

  // Ensure that initialization is complete.
  // If initialization is not complete yet, fall back to a synchronous
  // initialization and kill ongoing asynchronous initialization
  _ensureInitialized: function sss__ensureInitialized() {
    try {
      debug("_ensureInitialized: " + this._initialState);
      if (this._initialized) {
        // Initialization is complete, nothing else to do
        return;
      }
      let contents = _SessionFile.syncRead();
      this._onSessionFileRead(contents);
    } catch(ex) {
      debug("ensureInitialized: could not read session " + ex + ", " + ex.stack);
      throw ex;
    }
  },

  /* ........ QueryInterface .............. */
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIObserver,
                                          Ci.nsISupportsWeakReference,
                                          Ci.nsISessionStartup]),
  classID:          Components.ID("{ec7a6c20-e081-11da-8ad9-0800200c9a66}"),
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SessionStartup]);
