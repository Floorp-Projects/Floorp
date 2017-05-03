/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["SelfSupportBackend"];

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "HiddenFrame",
  "resource://gre/modules/HiddenFrame.jsm");

// Enables or disables the Self Support.
const PREF_ENABLED = "browser.selfsupport.enabled";
// Url to open in the Self Support browser, in the urlFormatter service format.
const PREF_URL = "browser.selfsupport.url";
// Unified Telemetry status.
const PREF_TELEMETRY_UNIFIED = "toolkit.telemetry.unified";
// UITour status.
const PREF_UITOUR_ENABLED = "browser.uitour.enabled";

// Controls the interval at which the self support page tries to reload in case of
// errors.
const RETRY_INTERVAL_MS = 30000;
// Maximum number of SelfSupport page load attempts in case of failure.
const MAX_RETRIES = 5;
// The delay after which to load the self-support, at startup.
const STARTUP_DELAY_MS = 5000;

const LOGGER_NAME = "Browser.SelfSupportBackend";
const PREF_BRANCH_LOG = "browser.selfsupport.log.";
const PREF_LOG_LEVEL = PREF_BRANCH_LOG + "level";
const PREF_LOG_DUMP = PREF_BRANCH_LOG + "dump";

const HTML_NS = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

const UITOUR_FRAME_SCRIPT = "chrome://browser/content/content-UITour.js";

// Whether the FHR/Telemetry unification features are enabled.
// Changing this pref requires a restart.
const IS_UNIFIED_TELEMETRY = Services.prefs.getBoolPref(PREF_TELEMETRY_UNIFIED, false);

var gLogAppenderDump = null;

this.SelfSupportBackend = Object.freeze({
  init() {
    SelfSupportBackendInternal.init();
  },

  uninit() {
    SelfSupportBackendInternal.uninit();
  },
});

var SelfSupportBackendInternal = {
  // The browser element that will load the SelfSupport page.
  _browser: null,
  // The Id of the timer triggering delayed SelfSupport page load.
  _delayedLoadTimerId: null,
  // The HiddenFrame holding the _browser element.
  _frame: null,
  _log: null,
  _progressListener: null,

  // Whether we're invited to let test code talk to our frame.
  _testing: false,

  // Whether self-support is enabled, and we want to continue lazy UI
  // startup after the session has been restored.
  _lazyStartupEnabled: false,

  /**
   * Initializes the self support backend.
   */
  init() {
    this._configureLogging();

    this._log.trace("init");

    Services.prefs.addObserver(PREF_BRANCH_LOG, this);

    // Only allow to use SelfSupport if Unified Telemetry is enabled.
    let reportingEnabled = IS_UNIFIED_TELEMETRY;
    if (!reportingEnabled) {
      this._log.config("init - Disabling SelfSupport because FHR and Unified Telemetry are disabled.");
      return;
    }

    // Make sure UITour is enabled.
    let uiTourEnabled = Services.prefs.getBoolPref(PREF_UITOUR_ENABLED, false);
    if (!uiTourEnabled) {
      this._log.config("init - Disabling SelfSupport because UITour is disabled.");
      return;
    }

    // Check the preferences to see if we want this to be active.
    if (!Services.prefs.getBoolPref(PREF_ENABLED, true)) {
      this._log.config("init - SelfSupport is disabled.");
      return;
    }

    this._lazyStartupEnabled = true;
  },

  /**
   * Shut down the self support backend, if active.
   */
  uninit() {
    if (!this._log) {
      // We haven't been initialized yet, so just return.
      return;
    }

    this._log.trace("uninit");

    Services.prefs.removeObserver(PREF_BRANCH_LOG, this);

    // Cancel delayed loading, if still active, when shutting down.
    clearTimeout(this._delayedLoadTimerId);

    // Dispose of the hidden browser.
    if (this._browser !== null) {
      if (this._browser.contentWindow) {
        this._browser.contentWindow.removeEventListener("DOMWindowClose", this, true);
      }

      if (this._progressListener) {
        this._browser.removeProgressListener(this._progressListener);
        this._progressListener.destroy();
        this._progressListener = null;
      }

      this._browser.remove();
      this._browser = null;
    }

    if (this._frame) {
      this._frame.destroy();
      this._frame = null;
    }
    if (this._testing) {
      Services.obs.notifyObservers(this._browser, "self-support-browser-destroyed");
    }
  },

  /**
   * Handle notifications. Once all windows are created, we wait a little bit more
   * since tabs might still be loading. Then, we open the self support.
   */
  // Observers are added in nsBrowserGlue.js
  observe(aSubject, aTopic, aData) {
    this._log.trace("observe - Topic " + aTopic);

    if (aTopic === "sessionstore-windows-restored") {
      if (this._lazyStartupEnabled) {
        this._delayedLoadTimerId = setTimeout(this._loadSelfSupport.bind(this), STARTUP_DELAY_MS);
        this._lazyStartupEnabled = false;
      }
    } else if (aTopic === "nsPref:changed") {
      this._configureLogging();
    }
  },

  /**
   * Configure the logger based on the preferences.
   */
  _configureLogging() {
    if (!this._log) {
      this._log = Log.repository.getLogger(LOGGER_NAME);

      // Log messages need to go to the browser console.
      let consoleAppender = new Log.ConsoleAppender(new Log.BasicFormatter());
      this._log.addAppender(consoleAppender);
    }

    // Make sure the logger keeps up with the logging level preference.
    this._log.level = Log.Level[Services.prefs.getStringPref(PREF_LOG_LEVEL, "Warn")];

    // If enabled in the preferences, add a dump appender.
    let logDumping = Services.prefs.getBoolPref(PREF_LOG_DUMP, false);
    if (logDumping != !!gLogAppenderDump) {
      if (logDumping) {
        gLogAppenderDump = new Log.DumpAppender(new Log.BasicFormatter());
        this._log.addAppender(gLogAppenderDump);
      } else {
        this._log.removeAppender(gLogAppenderDump);
        gLogAppenderDump = null;
      }
    }
  },

  /**
   * Create an hidden frame to host our |browser|, then load the SelfSupport page in it.
   * @param aURL The URL to load in the browser.
   */
  _makeHiddenBrowser(aURL) {
    this._frame = new HiddenFrame();
    return this._frame.get().then(aFrame => {
      let doc = aFrame.document;

      this._browser = doc.createElementNS(XUL_NS, "browser");
      this._browser.setAttribute("type", "content");
      this._browser.setAttribute("disableglobalhistory", "true");
      this._browser.setAttribute("src", aURL);

      if (this._testing) {
        Services.obs.notifyObservers(this._browser, "self-support-browser-created");
      }
      doc.documentElement.appendChild(this._browser);
    });
  },

  handleEvent(aEvent) {
    this._log.trace("handleEvent - aEvent.type " + aEvent.type + ", Trusted " + aEvent.isTrusted);

    if (aEvent.type === "DOMWindowClose") {
      let window = this._browser.contentDocument.defaultView;
      let target = aEvent.target;

      if (target == window) {
        // preventDefault stops the default window.close(). We need to do that to prevent
        // Services.appShell.hiddenDOMWindow from being destroyed.
        aEvent.preventDefault();

        this.uninit();
      }
    }
  },

  /**
   * Called when the self support page correctly loads.
   */
  _pageSuccessCallback() {
    this._log.debug("_pageSuccessCallback - Page correctly loaded.");
    this._browser.removeProgressListener(this._progressListener);
    this._progressListener.destroy();
    this._progressListener = null;

    // Allow SelfSupportBackend to catch |window.close()| issued by the content.
    this._browser.contentWindow.addEventListener("DOMWindowClose", this, true);
  },

  /**
   * Called when the self support page fails to load.
   */
  _pageLoadErrorCallback() {
    this._log.info("_pageLoadErrorCallback - Too many failed load attempts. Giving up.");
    this.uninit();
  },

  /**
   * Create a browser and attach it to an hidden window. The browser will contain the
   * self support page and attempt to load the page content. If loading fails, try again
   * after an interval.
   */
  _loadSelfSupport() {
    // Fetch the Self Support URL from the preferences.
    let unformattedURL = Services.prefs.getStringPref(PREF_URL, "");
    let url = Services.urlFormatter.formatURL(unformattedURL);
    if (!url.startsWith("https:")) {
      this._log.error("_loadSelfSupport - Non HTTPS URL provided: " + url);
      return;
    }

    this._log.config("_loadSelfSupport - URL " + url);

    // Create the hidden browser.
    this._makeHiddenBrowser(url).then(() => {
      // Load UITour frame script.
      this._browser.messageManager.loadFrameScript(UITOUR_FRAME_SCRIPT, true);

      // We need to watch for load errors as well and, in case, try to reload
      // the self support page.
      const webFlags = Ci.nsIWebProgress.NOTIFY_STATE_WINDOW |
                       Ci.nsIWebProgress.NOTIFY_STATE_REQUEST |
                       Ci.nsIWebProgress.NOTIFY_LOCATION;

      this._progressListener = new ProgressListener(() => this._pageLoadErrorCallback(),
                                                    () => this._pageSuccessCallback());

      this._browser.addProgressListener(this._progressListener, webFlags);
    });
  }
};

/**
 * A progress listener object which notifies of page load error and load success
 * through callbacks. When the page fails to load, the progress listener tries to
 * reload it up to MAX_RETRIES times. The page is not loaded again immediately, but
 * after a timeout.
 *
 * @param aLoadErrorCallback Called when a page failed to load MAX_RETRIES times.
 * @param aLoadSuccessCallback Called when a page correctly loads.
 */
function ProgressListener(aLoadErrorCallback, aLoadSuccessCallback) {
  this._loadErrorCallback = aLoadErrorCallback;
  this._loadSuccessCallback = aLoadSuccessCallback;
  // The number of page loads attempted.
  this._loadAttempts = 0;
  this._log = Log.repository.getLogger(LOGGER_NAME);
  // The Id of the timer which triggers page load again in case of errors.
  this._reloadTimerId = null;
}

ProgressListener.prototype = {
  onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
    if (aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_ERROR_PAGE) {
      this._log.warn("onLocationChange - There was a problem fetching the SelfSupport URL (attempt " +
                     this._loadAttempts + ").");

      // Increase the number of attempts and bail out if we failed too many times.
      this._loadAttempts++;
      if (this._loadAttempts > MAX_RETRIES) {
        this._loadErrorCallback();
        return;
      }

      // Reload the page after the retry interval expires. The interval is multiplied
      // by the number of attempted loads, so that it takes a bit more to try to reload
      // when frequently failing.
      this._reloadTimerId = setTimeout(() => {
        this._log.debug("onLocationChange - Reloading SelfSupport URL in the hidden browser.");
        aWebProgress.DOMWindow.location.reload();
      }, RETRY_INTERVAL_MS * this._loadAttempts);
    }
  },

  onStateChange(aWebProgress, aRequest, aFlags, aStatus) {
    if (aFlags & Ci.nsIWebProgressListener.STATE_STOP &&
        aFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK &&
        aFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW &&
        Components.isSuccessCode(aStatus)) {
      this._loadSuccessCallback();
    }
  },

  destroy() {
    // Make sure we don't try to reload self support when shutting down.
    clearTimeout(this._reloadTimerId);
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference]),
};
