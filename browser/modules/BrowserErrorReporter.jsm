/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/Timer.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "Log", "resource://gre/modules/Log.jsm");
ChromeUtils.defineModuleGetter(this, "UpdateUtils", "resource://gre/modules/UpdateUtils.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["fetch", "URL"]);

var EXPORTED_SYMBOLS = ["BrowserErrorReporter"];

const CONTEXT_LINES = 5;
const ERROR_PREFIX_RE = /^[^\W]+:/m;
const PREF_ENABLED = "browser.chrome.errorReporter.enabled";
const PREF_LOG_LEVEL = "browser.chrome.errorReporter.logLevel";
const PREF_PROJECT_ID = "browser.chrome.errorReporter.projectId";
const PREF_PUBLIC_KEY = "browser.chrome.errorReporter.publicKey";
const PREF_SAMPLE_RATE = "browser.chrome.errorReporter.sampleRate";
const PREF_SUBMIT_URL = "browser.chrome.errorReporter.submitUrl";
const RECENT_BUILD_AGE = 1000 * 60 * 60 * 24 * 7; // 7 days
const SDK_NAME = "firefox-error-reporter";
const SDK_VERSION = "1.0.0";
const TELEMETRY_ERROR_COLLECTED = "browser.errors.collected_count";
const TELEMETRY_ERROR_COLLECTED_FILENAME = "browser.errors.collected_count_by_filename";
const TELEMETRY_ERROR_COLLECTED_STACK = "browser.errors.collected_with_stack_count";
const TELEMETRY_ERROR_REPORTED = "browser.errors.reported_success_count";
const TELEMETRY_ERROR_REPORTED_FAIL = "browser.errors.reported_failure_count";
const TELEMETRY_ERROR_SAMPLE_RATE = "browser.errors.sample_rate";


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

const PLATFORM_NAMES = {
  linux: "Linux",
  win: "Windows",
  macosx: "macOS",
  android: "Android",
};

// Filename URI regexes that we are okay with reporting to Telemetry. URIs not
// matching these patterns may contain local file paths.
const TELEMETRY_REPORTED_PATTERNS = new Set([
  /^resource:\/\/(?:\/|gre|devtools)/,
  /^chrome:\/\/(?:global|browser|devtools)/,
]);

// Mapping of regexes to sample rates; if the regex matches the module an error
// is thrown from, the matching sample rate is used instead of the default.
// In case of a conflict, the first matching rate by insertion order is used.
const MODULE_SAMPLE_RATES = new Map([
  [/^(?:chrome|resource):\/\/devtools/, 1],
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
  /**
   * Generate a Date object corresponding to the date in the appBuildId.
   */
  static getAppBuildIdDate() {
    const appBuildId = Services.appinfo.appBuildID;
    const buildYear = Number.parseInt(appBuildId.slice(0, 4));
    // Date constructor uses 0-indexed months
    const buildMonth = Number.parseInt(appBuildId.slice(4, 6)) - 1;
    const buildDay = Number.parseInt(appBuildId.slice(6, 8));
    return new Date(buildYear, buildMonth, buildDay);
  }

  constructor(options = {}) {
    // Test arguments for mocks and changing behavior
    this.fetch = options.fetch || defaultFetch;
    this.now = options.now || null;
    this.chromeOnly = options.chromeOnly !== undefined ? options.chromeOnly : true;
    this.registerListener = (
      options.registerListener || (() => Services.console.registerListener(this))
    );
    this.unregisterListener = (
      options.unregisterListener || (() => Services.console.unregisterListener(this))
    );

    XPCOMUtils.defineLazyGetter(this, "appBuildIdDate", BrowserErrorReporter.getAppBuildIdDate);

    // Values that don't change between error reports.
    this.requestBodyTemplate = {
      logger: "javascript",
      platform: "javascript",
      release: Services.appinfo.appBuildID,
      environment: UpdateUtils.getUpdateChannel(false),
      contexts: {
        os: {
          name: PLATFORM_NAMES[AppConstants.platform],
          version: (
            Cc["@mozilla.org/network/protocol;1?name=http"]
            .getService(Ci.nsIHttpProtocolHandler)
            .oscpu
          ),
        },
        browser: {
          name: "Firefox",
          version: Services.appinfo.version,
        },
      },
      tags: {
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
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "sampleRatePref",
      PREF_SAMPLE_RATE,
      "0.0",
      this.handleSampleRatePrefChanged.bind(this),
    );

    // Prefix mappings for the mangleFilePaths transform.
    this.manglePrefixes = options.manglePrefixes || {
      greDir: Services.dirsvc.get("GreD", Ci.nsIFile),
      profileDir: Services.dirsvc.get("ProfD", Ci.nsIFile),
    };
    // File paths are encoded by nsIURI, so let's do the same for the prefixes
    // we're comparing them to.
    for (const [name, prefixFile] of Object.entries(this.manglePrefixes)) {
      let filePath = Services.io.newFileURI(prefixFile).filePath;

      // filePath might not have a trailing slash in some cases
      if (!filePath.endsWith("/")) {
        filePath += "/";
      }

      this.manglePrefixes[name] = filePath;
    }
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
      this.registerListener();

      // Processing already-logged messages in case any errors occurred before
      // startup.
      for (const message of Services.console.getMessageArray()) {
        this.observe(message);
      }
    }
  }

  uninit() {
    try {
      this.unregisterListener();
    } catch (err) {} // It probably wasn't registered.
  }

  handleEnabledPrefChanged(prefName, previousValue, newValue) {
    if (newValue) {
      this.registerListener();
    } else {
      try {
        this.unregisterListener();
      } catch (err) {} // It probably wasn't registered.
    }
  }

  handleSampleRatePrefChanged(prefName, previousValue, newValue) {
    Services.telemetry.scalarSet(TELEMETRY_ERROR_SAMPLE_RATE, newValue);
  }

  errorCollectedFilenameKey(filename) {
    for (const pattern of TELEMETRY_REPORTED_PATTERNS) {
      if (filename.match(pattern)) {
        return filename;
      }
    }

    // WebExtensions get grouped separately from other errors
    if (filename.startsWith("moz-extension://")) {
        return "MOZEXTENSION";
    }

    return "FILTERED";
  }

  isRecentBuild() {
    // The local clock is not reliable, but this method doesn't need to be
    // perfect.
    const now = this.now || new Date();
    return (now - this.appBuildIdDate) <= RECENT_BUILD_AGE;
  }

  observe(message) {
    if (message instanceof Ci.nsIScriptError) {
      ChromeUtils.idleDispatch(() => this.handleMessage(message));
    }
  }

  async handleMessage(message) {
    const isWarning = message.flags & message.warningFlag;
    const isFromChrome = REPORTED_CATEGORIES.has(message.category);
    if ((this.chromeOnly && !isFromChrome) || isWarning) {
      return;
    }

    // Record that we collected an error prior to applying the sample rate
    Services.telemetry.scalarAdd(TELEMETRY_ERROR_COLLECTED, 1);
    if (message.stack) {
      Services.telemetry.scalarAdd(TELEMETRY_ERROR_COLLECTED_STACK, 1);
    }
    if (message.sourceName) {
      const key = this.errorCollectedFilenameKey(message.sourceName);
      Services.telemetry.keyedScalarAdd(TELEMETRY_ERROR_COLLECTED_FILENAME, key.slice(0, 69), 1);
    }

    // We do not collect errors on non-Nightly channels, just telemetry.
    // Also, old builds should not send errors to Sentry
    if (!AppConstants.NIGHTLY_BUILD || !this.isRecentBuild()) {
      return;
    }

    // Sample the amount of errors we send out
    let sampleRate = Number.parseFloat(this.sampleRatePref);
    for (const [regex, rate] of MODULE_SAMPLE_RATES) {
      if (message.sourceName.match(regex)) {
        sampleRate = rate;
        break;
      }
    }
    if (!Number.isFinite(sampleRate) || (Math.random() >= sampleRate)) {
      return;
    }

    const exceptionValue = {};
    const requestBody = {
      ...this.requestBodyTemplate,
      timestamp: new Date().toISOString().slice(0, -1), // Remove trailing "Z"
      project: Services.prefs.getCharPref(PREF_PROJECT_ID),
      exception: {
        values: [exceptionValue],
      },
    };

    const transforms = [
      addErrorMessage,
      addStacktrace,
      addModule,
      mangleExtensionUrls,
      this.mangleFilePaths.bind(this),
      tagExtensionErrors,
    ];
    for (const transform of transforms) {
      await transform(message, exceptionValue, requestBody);
    }

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
      Services.telemetry.scalarAdd(TELEMETRY_ERROR_REPORTED, 1);
      this.logger.debug(`Sent error "${message.errorMessage}" successfully.`);
    } catch (error) {
      Services.telemetry.scalarAdd(TELEMETRY_ERROR_REPORTED_FAIL, 1);
      this.logger.warn(`Failed to send error "${message.errorMessage}": ${error}`);
    }
  }

  /**
   * Alters file: and jar: paths to remove leading file paths that may contain
   * user-identifying or platform-specific paths.
   *
   * prefixes is a mapping of replacementName -> filePath, where filePath is a
   * path on the filesystem that should be replaced, and replacementName is the
   * text that will replace it.
   */
  mangleFilePaths(message, exceptionValue) {
    exceptionValue.module = this._transformFilePath(exceptionValue.module);
    for (const frame of exceptionValue.stacktrace.frames) {
      frame.module = this._transformFilePath(frame.module);
    }
  }

  _transformFilePath(path) {
    try {
      const uri = Services.io.newURI(path);
      if (uri.schemeIs("jar")) {
        return uri.filePath;
      }
      if (uri.schemeIs("file")) {
        for (const [name, prefix] of Object.entries(this.manglePrefixes)) {
          if (uri.filePath.startsWith(prefix)) {
            return uri.filePath.replace(prefix, `[${name}]/`);
          }
        }

        return "[UNKNOWN_LOCAL_FILEPATH]";
      }
    } catch (err) {}

    return path;
  }
}

function defaultFetch(...args) {
  // Do not make network requests while running in automation
  if (Cu.isInAutomation) {
    return null;
  }

  return fetch(...args);
}

function addErrorMessage(message, exceptionValue) {
  // Parse the error type from the message if present (e.g. "TypeError: Whoops").
  let errorMessage = message.errorMessage;
  let errorName = "Error";
  if (message.errorMessage.match(ERROR_PREFIX_RE)) {
    const parts = message.errorMessage.split(":");
    errorName = parts[0];
    errorMessage = parts.slice(1).join(":").trim();
  }

  exceptionValue.type = errorName;
  exceptionValue.value = errorMessage;
}

async function addStacktrace(message, exceptionValue) {
  const frames = [];
  let frame = message.stack;
  // Avoid an infinite loop by limiting traces to 100 frames.
  while (frame && frames.length < 100) {
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

    frames.push(normalizedFrame);
    frame = frame.parent;
  }
  // Frames are sent in order from oldest to newest.
  frames.reverse();

  exceptionValue.stacktrace = {frames};
}

function addModule(message, exceptionValue) {
  exceptionValue.module = message.sourceName;
}

function mangleExtensionUrls(message, exceptionValue) {
  const extensions = new Map();
  for (let extension of WebExtensionPolicy.getActiveExtensions()) {
    extensions.set(extension.mozExtensionHostname, extension);
  }

  // Replaces any instances of moz-extension:// URLs with internal UUIDs to use
  // the add-on ID instead.
  function mangleExtURL(string, anchored = true) {
    if (!string) {
      return string;
    }

    const re = new RegExp(`${anchored ? "^" : ""}moz-extension://([^/]+)/`, "g");
    return string.replace(re, (m0, m1) => {
      const id = extensions.has(m1) ? extensions.get(m1).id : m1;
      return `moz-extension://${id}/`;
    });
  }

  exceptionValue.value = mangleExtURL(exceptionValue.value, false);
  exceptionValue.module = mangleExtURL(exceptionValue.module);
  for (const frame of exceptionValue.stacktrace.frames) {
    frame.module = mangleExtURL(frame.module);
  }
}

function tagExtensionErrors(message, exceptionValue, requestBody) {
  requestBody.tags.isExtensionError = !!(
      exceptionValue.module && exceptionValue.module.startsWith("moz-extension://")
  );
}
