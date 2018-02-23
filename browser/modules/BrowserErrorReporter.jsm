/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/Timer.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "Log", "resource://gre/modules/Log.jsm");
ChromeUtils.defineModuleGetter(this, "UpdateUtils", "resource://gre/modules/UpdateUtils.jsm");

Cu.importGlobalProperties(["fetch", "URL"]);

var EXPORTED_SYMBOLS = ["BrowserErrorReporter"];

const ERROR_PREFIX_RE = /^[^\W]+:/m;
const PREF_ENABLED = "browser.chrome.errorReporter.enabled";
const PREF_LOG_LEVEL = "browser.chrome.errorReporter.logLevel";
const PREF_PROJECT_ID = "browser.chrome.errorReporter.projectId";
const PREF_PUBLIC_KEY = "browser.chrome.errorReporter.publicKey";
const PREF_SAMPLE_RATE = "browser.chrome.errorReporter.sampleRate";
const PREF_SUBMIT_URL = "browser.chrome.errorReporter.submitUrl";

// https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XPCOM/Reference/Interface/nsIScriptError#Categories
const REPORTED_CATEGORIES = new Set([
  "XPConnect JavaScript",
  "component javascript",
  "chrome javascript",
  "chrome registration",
  "XBL",
  "XBL Prototype Handler",
  "XBL Content Sink",
  "xbl javascript",
  "FrameConstructor",
]);

/**
 * Collects nsIScriptError messages logged to the browser console and reports
 * them to a remotely-hosted error collection service.
 *
 * This is a PROTOTYPE; it will be removed in the future and potentially
 * replaced with a more robust implementation. It is meant to only collect
 * errors from Nightly (and local builds if enabled for development purposes)
 * and has not been reviewed for use outside of Nightly.
 *
 * The outgoing requests are designed to be compatible with version 7 of Sentry.
 */
class BrowserErrorReporter {
  constructor(fetchMethod = this._defaultFetch) {
    // A fake fetch is passed by the tests to avoid network connections
    this.fetch = fetchMethod;

    // Values that don't change between error reports.
    this.requestBodyTemplate = {
      request: {
        headers: {
          "User-Agent": Services.appShell.hiddenDOMWindow.navigator.userAgent,
        },
      },
      logger: "javascript",
      platform: "javascript",
      release: Services.appinfo.version,
      environment: UpdateUtils.getUpdateChannel(false),
      tags: {
        appBuildID: Services.appinfo.appBuildID,
        changeset: AppConstants.SOURCE_REVISION_URL,
      },
    };

    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "collectionEnabled",
      PREF_ENABLED,
      false,
      this.handleEnabledPrefChanged.bind(this),
    );
  }

  /**
   * Lazily-created logger
   */
  get logger() {
    const logger = Log.repository.getLogger("BrowserErrorReporter");
    logger.addAppender(new Log.ConsoleAppender(new Log.BasicFormatter()));
    logger.manageLevelFromPref(PREF_LOG_LEVEL);

    Object.defineProperty(this, "logger", {value: logger});
    return this.logger;
  }

  init() {
    if (this.collectionEnabled) {
      Services.console.registerListener(this);

      // Processing already-logged messages in case any errors occurred before
      // startup.
      for (const message of Services.console.getMessageArray()) {
        this.observe(message);
      }
    }
  }

  uninit() {
    try {
      Services.console.unregisterListener(this);
    } catch (err) {} // It probably wasn't registered.
  }

  handleEnabledPrefChanged(prefName, previousValue, newValue) {
    if (newValue) {
      Services.console.registerListener(this);
    } else {
      try {
        Services.console.unregisterListener(this);
      } catch (err) {} // It probably wasn't registered.
    }
  }

  async observe(message) {
    try {
      message.QueryInterface(Ci.nsIScriptError);
    } catch (err) {
      return; // Not an error
    }

    const isWarning = message.flags & message.warningFlag;
    const isFromChrome = REPORTED_CATEGORIES.has(message.category);
    if (!isFromChrome || isWarning) {
      return;
    }

    // Sample the amount of errors we send out
    const sampleRate = Number.parseFloat(Services.prefs.getCharPref(PREF_SAMPLE_RATE));
    if (!Number.isFinite(sampleRate) || (Math.random() >= sampleRate)) {
      return;
    }

    // Parse the error type from the message if present (e.g. "TypeError: Whoops").
    let errorMessage = message.errorMessage;
    let errorName = "Error";
    if (message.errorMessage.match(ERROR_PREFIX_RE)) {
      const parts = message.errorMessage.split(":");
      errorName = parts[0];
      errorMessage = parts.slice(1).join(":").trim();
    }

    // Pull and normalize stacktrace frames from the message
    const frames = [];
    let frame = message.stack;

    // Avoid an infinite loop by limiting traces to 100 frames.
    while (frame && frames.length < 100) {
      frames.push({
        function: frame.functionDisplayName,
        filename: frame.source,
        lineno: frame.line,
        colno: frame.column,
        in_app: true,
      });
      frame = frame.parent;
    }

    // Sentry-compatible request body copied from an example generated by Raven.js 3.22.1.
    const requestBody = Object.assign({}, this.requestBodyTemplate, {
      project: Services.prefs.getCharPref(PREF_PROJECT_ID),
      exception: {
        values: [
          {
            type: errorName,
            // Error messages may contain PII; see bug 1426482 for privacy
            // review and server-side mitigation.
            value: errorMessage,
            stacktrace: {
              frames,
            }
          },
        ],
      },
      culprit: message.sourceName,
    });
    requestBody.request.url = message.sourceName;

    const url = new URL(Services.prefs.getCharPref(PREF_SUBMIT_URL));
    url.searchParams.set("sentry_client", "firefox-error-reporter/1.0.0");
    url.searchParams.set("sentry_version", "7");
    url.searchParams.set("sentry_key", Services.prefs.getCharPref(PREF_PUBLIC_KEY));

    try {
      await this.fetch(url, {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
          "Accept": "application/json",
        },
        // Sentry throws an auth error without a referrer specified.
        referrer: "https://fake.mozilla.org",
        body: JSON.stringify(requestBody)
      });
      this.logger.debug("Sent error successfully.");
    } catch (error) {
      this.logger.warn(`Failed to send error: ${error}`);
    }
  }

  async _defaultFetch(...args) {
    // Do not make network requests while running in automation
    if (Cu.isInAutomation) {
      return null;
    }

    return fetch(...args);
  }
}
