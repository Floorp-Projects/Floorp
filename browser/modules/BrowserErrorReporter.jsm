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

const CONTEXT_LINES = 5;
const ERROR_PREFIX_RE = /^[^\W]+:/m;
const PREF_ENABLED = "browser.chrome.errorReporter.enabled";
const PREF_LOG_LEVEL = "browser.chrome.errorReporter.logLevel";
const PREF_PROJECT_ID = "browser.chrome.errorReporter.projectId";
const PREF_PUBLIC_KEY = "browser.chrome.errorReporter.publicKey";
const PREF_SAMPLE_RATE = "browser.chrome.errorReporter.sampleRate";
const PREF_SUBMIT_URL = "browser.chrome.errorReporter.submitUrl";
const SDK_NAME = "firefox-error-reporter";
const SDK_VERSION = "1.0.0";

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
 * The outgoing requests are designed to be compatible with Sentry. See
 * https://docs.sentry.io/clientdev/ for details on the data format that Sentry
 * expects.
 *
 * Errors may contain PII, such as in messages or local file paths in stack
 * traces; see bug 1426482 for privacy review and server-side mitigation.
 */
class BrowserErrorReporter {
  constructor(fetchMethod = this._defaultFetch) {
    // A fake fetch is passed by the tests to avoid network connections
    this.fetch = fetchMethod;

    // Values that don't change between error reports.
    this.requestBodyTemplate = {
      logger: "javascript",
      platform: "javascript",
      release: Services.appinfo.version,
      environment: UpdateUtils.getUpdateChannel(false),
      tags: {
        appBuildID: Services.appinfo.appBuildID,
        changeset: AppConstants.SOURCE_REVISION_URL,
      },
      sdk: {
        name: SDK_NAME,
        version: SDK_VERSION,
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

    const frames = [];
    let frame = message.stack;
    // Avoid an infinite loop by limiting traces to 100 frames.
    while (frame && frames.length < 100) {
      frames.push(await this.normalizeStackFrame(frame));
      frame = frame.parent;
    }
    // Frames are sent in order from oldest to newest.
    frames.reverse();

    const requestBody = Object.assign({}, this.requestBodyTemplate, {
      timestamp: new Date().toISOString().slice(0, -1), // Remove trailing "Z"
      project: Services.prefs.getCharPref(PREF_PROJECT_ID),
      exception: {
        values: [
          {
            type: errorName,
            value: errorMessage,
            module: message.sourceName,
            stacktrace: {
              frames,
            }
          },
        ],
      },
      culprit: message.sourceName,
    });

    const url = new URL(Services.prefs.getCharPref(PREF_SUBMIT_URL));
    url.searchParams.set("sentry_client", `${SDK_NAME}/${SDK_VERSION}`);
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

  async normalizeStackFrame(frame) {
    const normalizedFrame = {
      function: frame.functionDisplayName,
      module: frame.source,
      lineno: frame.line,
      colno: frame.column,
    };

    try {
      const response = await fetch(frame.source);
      const sourceCode = await response.text();
      const sourceLines = sourceCode.split(/\r?\n/);
      // HTML pages and some inline event handlers have 0 as their line number
      let lineIndex = Math.max(frame.line - 1, 0);

      // XBL line numbers are off by one, and pretty much every XML file with JS
      // in it is an XBL file.
      if (frame.source.endsWith(".xml") && lineIndex > 0) {
        lineIndex--;
      }

      normalizedFrame.context_line = sourceLines[lineIndex];
      normalizedFrame.pre_context = sourceLines.slice(
        Math.max(lineIndex - CONTEXT_LINES, 0),
        lineIndex,
      );
      normalizedFrame.post_context = sourceLines.slice(
        lineIndex + 1,
        Math.min(lineIndex + 1 + CONTEXT_LINES, sourceLines.length),
      );
    } catch (err) {
      // Could be a fetch issue, could be a line index issue. Not much we can
      // do to recover in either case.
    }

    return normalizedFrame;
  }

  async _defaultFetch(...args) {
    // Do not make network requests while running in automation
    if (Cu.isInAutomation) {
      return null;
    }

    return fetch(...args);
  }
}
