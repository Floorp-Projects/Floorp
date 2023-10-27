/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-disable mozilla/valid-lazy */

import {
  CONTEXTUAL_SERVICES_PING_TYPES,
  PartnerLinkAttribution,
} from "resource:///modules/PartnerLinkAttribution.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ExperimentAPI: "resource://nimbus/ExperimentAPI.sys.mjs",
  ExperimentFakes: "resource://testing-common/NimbusTestUtils.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  RemoteSettingsConfig: "resource://gre/modules/RustRemoteSettings.sys.mjs",
  RemoteSettingsServer:
    "resource://testing-common/RemoteSettingsServer.sys.mjs",
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
  SuggestBackendRust:
    "resource:///modules/urlbar/private/SuggestBackendRust.sys.mjs",
  Suggestion: "resource://gre/modules/RustSuggest.sys.mjs",
  SuggestionProvider: "resource://gre/modules/RustSuggest.sys.mjs",
  SuggestStore: "resource://gre/modules/RustSuggest.sys.mjs",
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.sys.mjs",
  TestUtils: "resource://testing-common/TestUtils.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarProviderQuickSuggest:
    "resource:///modules/UrlbarProviderQuickSuggest.sys.mjs",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
});

let gTestScope;

// Test utils singletons need special handling. Since they are uninitialized in
// cleanup functions, they must be re-initialized on each new test. That does
// not happen automatically inside system modules like this one because system
// module lifetimes are the app's lifetime, unlike individual browser chrome and
// xpcshell tests.
Object.defineProperty(lazy, "UrlbarTestUtils", {
  get: () => {
    if (!lazy._UrlbarTestUtils) {
      const { UrlbarTestUtils: module } = ChromeUtils.importESModule(
        "resource://testing-common/UrlbarTestUtils.sys.mjs"
      );
      module.init(gTestScope);
      gTestScope.registerCleanupFunction(() => {
        // Make sure the utils are re-initialized during the next test.
        lazy._UrlbarTestUtils = null;
      });
      lazy._UrlbarTestUtils = module;
    }
    return lazy._UrlbarTestUtils;
  },
});

// Test utils singletons need special handling. Since they are uninitialized in
// cleanup functions, they must be re-initialized on each new test. That does
// not happen automatically inside system modules like this one because system
// module lifetimes are the app's lifetime, unlike individual browser chrome and
// xpcshell tests.
Object.defineProperty(lazy, "MerinoTestUtils", {
  get: () => {
    if (!lazy._MerinoTestUtils) {
      const { MerinoTestUtils: module } = ChromeUtils.importESModule(
        "resource://testing-common/MerinoTestUtils.sys.mjs"
      );
      module.init(gTestScope);
      gTestScope.registerCleanupFunction(() => {
        // Make sure the utils are re-initialized during the next test.
        lazy._MerinoTestUtils = null;
      });
      lazy._MerinoTestUtils = module;
    }
    return lazy._MerinoTestUtils;
  },
});

const DEFAULT_CONFIG = {};

const DEFAULT_PING_PAYLOADS = {
  [CONTEXTUAL_SERVICES_PING_TYPES.QS_BLOCK]: {
    advertiser: "testadvertiser",
    block_id: 1,
    context_id: () => actual => !!actual,
    iab_category: "22 - Shopping",
    improve_suggest_experience_checked: false,
    match_type: "firefox-suggest",
    request_id: null,
    source: "remote-settings",
  },
  [CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION]: {
    advertiser: "testadvertiser",
    block_id: 1,
    context_id: () => actual => !!actual,
    improve_suggest_experience_checked: false,
    match_type: "firefox-suggest",
    reporting_url: "https://example.com/click",
    request_id: null,
    source: "remote-settings",
  },
  [CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION]: {
    advertiser: "testadvertiser",
    block_id: 1,
    context_id: () => actual => !!actual,
    improve_suggest_experience_checked: false,
    is_clicked: false,
    match_type: "firefox-suggest",
    reporting_url: "https://example.com/impression",
    request_id: null,
    source: "remote-settings",
  },
};

// The following properties and methods are copied from the test scope to the
// test utils object so they can be easily accessed. Be careful about assuming a
// particular property will be defined because depending on the scope -- browser
// test or xpcshell test -- some may not be.
const TEST_SCOPE_PROPERTIES = [
  "Assert",
  "EventUtils",
  "info",
  "registerCleanupFunction",
];

/**
 * Test utils for quick suggest.
 */
class _QuickSuggestTestUtils {
  /**
   * Initializes the utils.
   *
   * @param {object} scope
   *   The global JS scope where tests are being run. This allows the instance
   *   to access test helpers like `Assert` that are available in the scope.
   */
  init(scope) {
    if (!scope) {
      throw new Error("QuickSuggestTestUtils() must be called with a scope");
    }
    gTestScope = scope;
    for (let p of TEST_SCOPE_PROPERTIES) {
      this[p] = scope[p];
    }
    // If you add other properties to `this`, null them in `uninit()`.

    Services.telemetry.clearScalars();

    scope.registerCleanupFunction?.(() => this.uninit());
  }

  /**
   * Uninitializes the utils. If they were created with a test scope that
   * defines `registerCleanupFunction()`, you don't need to call this yourself
   * because it will automatically be called as a cleanup function. Otherwise
   * you'll need to call this.
   */
  uninit() {
    gTestScope = null;
    for (let p of TEST_SCOPE_PROPERTIES) {
      this[p] = null;
    }
    Services.telemetry.clearScalars();
  }

  get DEFAULT_CONFIG() {
    // Return a clone so callers can modify it.
    return Cu.cloneInto(DEFAULT_CONFIG, this);
  }

  /**
   * Sets up local remote settings and Merino servers, registers test
   * suggestions, and initializes Suggest.
   *
   * @param {object} options
   *   Options object
   * @param {Array} options.remoteSettingsRecords
   *   Array of remote settings records. Each item in this array should be a
   *   realistic remote settings record with some exceptions, e.g.,
   *   `record.attachment`, if defined, should be the attachment itself and not
   *   its metadata. For details see `RemoteSettingsServer.addRecords()`.
   * @param {Array} options.merinoSuggestions
   *   Array of Merino suggestion objects. If given, this function will start
   *   the mock Merino server and set `quicksuggest.dataCollection.enabled` to
   *   true so that `UrlbarProviderQuickSuggest` will fetch suggestions from it.
   *   Otherwise Merino will not serve suggestions, but you can still set up
   *   Merino without using this function by using `MerinoTestUtils` directly.
   * @param {object} options.config
   *   The Suggest configuration object. This should not be the full remote
   *   settings record; only pass the object that should be set to the nested
   *   `configuration` object inside the record.
   * @param {Array} options.prefs
   *   An array of Suggest-related prefs to set. This is useful because setting
   *   some prefs, like feature gates, can cause Suggest to sync from remote
   *   settings; this function will set them, wait for sync to finish, and clear
   *   them when the cleanup function is called. Each item in this array should
   *   itself be a two-element array `[prefName, prefValue]` similar to the
   *   `set` array passed to `SpecialPowers.pushPrefEnv()`, except here pref
   *   names are relative to `browser.urlbar`.
   * @returns {Function}
   *   An async cleanup function. This function is automatically registered as a
   *   cleanup function, so you only need to call it if your test needs to clean
   *   up Suggest before it ends, for example if you have a small number of
   *   tasks that need Suggest and it's not enabled throughout your test. The
   *   cleanup function is idempotent so there's no harm in calling it more than
   *   once. Be sure to `await` it.
   */
  async ensureQuickSuggestInit({
    remoteSettingsRecords = [],
    merinoSuggestions = null,
    config = DEFAULT_CONFIG,
    prefs = [],
  } = {}) {
    prefs.push(["quicksuggest.enabled", true]);

    // Set up the local remote settings server. We do this even for tests that
    // aren't specifically related to remote settings so Suggest doesn't hit the
    // real remote settings server during testing.
    this.#log(
      "ensureQuickSuggestInit",
      "Started, preparing remote settings server"
    );
    if (!this.#remoteSettingsServer) {
      this.#remoteSettingsServer = new lazy.RemoteSettingsServer();
    }
    await this.#remoteSettingsServer.setRecords({
      collection: "quicksuggest",
      records: [
        ...remoteSettingsRecords,
        { type: "configuration", configuration: config },
      ],
    });
    this.#log("ensureQuickSuggestInit", "Starting remote settings server");
    await this.#remoteSettingsServer.start();

    // Get the cached `RemoteSettings` client used by the JS backend and tell it
    // to ignore signatures and to always force sync. Otherwise it won't sync if
    // the previous sync was recent enough, which is incompatible with testing.
    let rs = lazy.RemoteSettings("quicksuggest");
    let { get, verifySignature } = rs;
    rs.verifySignature = false;
    rs.get = opts => get.call(rs, { forceSync: true, ...opts });
    this.#restoreRemoteSettings = () => {
      rs.verifySignature = verifySignature;
      rs.get = get;
    };

    // Tell the Rust backend to use the local remote setting server.
    lazy.SuggestBackendRust._test_remoteSettingsConfig =
      new lazy.RemoteSettingsConfig(
        this.#remoteSettingsServer.url.toString(),
        "main",
        "quicksuggest"
      );

    // Finally, init Suggest and set prefs. Do this after setting up remote
    // settings because the current backend will immediately try to sync.
    this.#log(
      "ensureQuickSuggestInit",
      "Calling QuickSuggest.init() and setting prefs"
    );
    lazy.QuickSuggest.init();
    for (let [name, value] of prefs) {
      lazy.UrlbarPrefs.set(name, value);
    }

    // Wait for the current backend to finish syncing.
    await this.forceSync();

    // Set up Merino. This can happen any time relative to Suggest init.
    if (merinoSuggestions) {
      this.#log("ensureQuickSuggestInit", "Setting up Merino server");
      await lazy.MerinoTestUtils.server.start();
      lazy.MerinoTestUtils.server.response.body.suggestions = merinoSuggestions;
      lazy.UrlbarPrefs.set("quicksuggest.dataCollection.enabled", true);
      this.#log("ensureQuickSuggestInit", "Done setting up Merino server");
    }

    let cleanupCalled = false;
    let cleanup = async () => {
      if (!cleanupCalled) {
        cleanupCalled = true;
        await this.#uninitQuickSuggest(prefs, !!merinoSuggestions);
      }
    };
    this.registerCleanupFunction?.(cleanup);

    this.#log("ensureQuickSuggestInit", "Done");
    return cleanup;
  }

  async #uninitQuickSuggest(prefs, clearDataCollectionEnabled) {
    this.#log("#uninitQuickSuggest", "Started");

    // Reset prefs, which can cause the current backend to start syncing. Wait
    // for it to finish.
    for (let [name] of prefs) {
      lazy.UrlbarPrefs.clear(name);
    }
    await this.forceSync();

    this.#log("#uninitQuickSuggest", "Stopping remote settings server");
    await this.#remoteSettingsServer.stop();
    this.#restoreRemoteSettings();

    if (clearDataCollectionEnabled) {
      lazy.UrlbarPrefs.clear("quicksuggest.dataCollection.enabled");
    }

    this.#log("#uninitQuickSuggest", "Done");
  }

  /**
   * Removes all records from the local remote settings server and adds a new
   * batch of records.
   *
   * @param {Array} records
   *   Array of remote settings records. See `ensureQuickSuggestInit()`.
   * @param {object} options
   *   Options object.
   * @param {boolean} options.forceSync
   *   Whether to force Suggest to sync after updating the records.
   */
  async setRemoteSettingsRecords(records, { forceSync = true } = {}) {
    this.#log("setRemoteSettingsRecords", "Started");
    await this.#remoteSettingsServer.setRecords({
      collection: "quicksuggest",
      records,
    });
    if (forceSync) {
      this.#log("setRemoteSettingsRecords", "Forcing sync");
      await this.forceSync();
    }
    this.#log("setRemoteSettingsRecords", "Done");
  }

  /**
   * Sets the quick suggest configuration. You should call this again with
   * `DEFAULT_CONFIG` before your test finishes. See also `withConfig()`.
   *
   * @param {object} config
   *   The quick suggest configuration object. This should not be the full
   *   remote settings record; only pass the object that should be set to the
   *   `configuration` nested object inside the record.
   */
  async setConfig(config) {
    this.#log("setConfig", "Started");
    let type = "configuration";
    this.#remoteSettingsServer.removeRecords({ type });
    await this.#remoteSettingsServer.addRecords({
      collection: "quicksuggest",
      records: [{ type, configuration: config }],
    });
    this.#log("setConfig", "Forcing sync");
    await this.forceSync();
    this.#log("setConfig", "Done");
  }

  /**
   * Forces Suggest to sync with remote settings. This can be used to ensure
   * Suggest has finished all sync activity.
   */
  async forceSync() {
    this.#log("forceSync", "Started");
    if (lazy.QuickSuggest.rustBackend.isEnabled) {
      this.#log("forceSync", "Syncing Rust backend");
      await lazy.QuickSuggest.rustBackend._test_ingest();
      this.#log("forceSync", "Done syncing Rust backend");
    }
    if (lazy.QuickSuggest.jsBackend.isEnabled) {
      this.#log("forceSync", "Syncing JS backend");
      await lazy.QuickSuggest.jsBackend._test_syncAll();
      this.#log("forceSync", "Done syncing JS backend");
    }
    this.#log("forceSync", "Done");
  }

  /**
   * Sets the quick suggest configuration, calls your callback, and restores the
   * previous configuration.
   *
   * @param {object} options
   *   The options object.
   * @param {object} options.config
   *   The configuration that should be used with the callback
   * @param {Function} options.callback
   *   Will be called with the configuration applied
   *
   * @see {@link setConfig}
   */
  async withConfig({ config, callback }) {
    let original = lazy.QuickSuggest.jsBackend.config;
    await this.setConfig(config);
    await callback();
    await this.setConfig(original);
  }

  /**
   * Sets the Firefox Suggest scenario and waits for prefs to be updated.
   *
   * @param {string} scenario
   *   Pass falsey to reset the scenario to the default.
   */
  async setScenario(scenario) {
    // If we try to set the scenario before a previous update has finished,
    // `updateFirefoxSuggestScenario` will bail, so wait.
    await this.waitForScenarioUpdated();
    await lazy.UrlbarPrefs.updateFirefoxSuggestScenario({ scenario });
  }

  /**
   * Waits for any prior scenario update to finish.
   */
  async waitForScenarioUpdated() {
    await lazy.TestUtils.waitForCondition(
      () => !lazy.UrlbarPrefs.updatingFirefoxSuggestScenario,
      "Waiting for updatingFirefoxSuggestScenario to be false"
    );
  }

  /**
   * Asserts a result is a quick suggest result.
   *
   * @param {object} [options]
   *   The options object.
   * @param {string} options.url
   *   The expected URL. At least one of `url` and `originalUrl` must be given.
   * @param {string} options.originalUrl
   *   The expected original URL (the URL with an unreplaced timestamp
   *   template). At least one of `url` and `originalUrl` must be given.
   * @param {object} options.window
   *   The window that should be used for this assertion
   * @param {number} [options.index]
   *   The expected index of the quick suggest result. Pass -1 to use the index
   *   of the last result.
   * @param {boolean} [options.isSponsored]
   *   Whether the result is expected to be sponsored.
   * @param {boolean} [options.isBestMatch]
   *   Whether the result is expected to be a best match.
   * @returns {result}
   *   The quick suggest result.
   */
  async assertIsQuickSuggest({
    url,
    originalUrl,
    window,
    index = -1,
    isSponsored = true,
    isBestMatch = false,
  } = {}) {
    this.Assert.ok(
      url || originalUrl,
      "At least one of url and originalUrl is specified"
    );

    if (index < 0) {
      let resultCount = lazy.UrlbarTestUtils.getResultCount(window);
      if (isBestMatch) {
        index = 1;
        this.Assert.greater(
          resultCount,
          1,
          "Sanity check: Result count should be > 1"
        );
      } else {
        index = resultCount - 1;
        this.Assert.greater(
          resultCount,
          0,
          "Sanity check: Result count should be > 0"
        );
      }
    }

    let details = await lazy.UrlbarTestUtils.getDetailsOfResultAt(
      window,
      index
    );
    let { result } = details;

    this.#log(
      "assertIsQuickSuggest",
      `Checking actual result at index ${index}: ` + JSON.stringify(result)
    );

    this.Assert.equal(
      result.providerName,
      "UrlbarProviderQuickSuggest",
      "Result provider name is UrlbarProviderQuickSuggest"
    );
    this.Assert.equal(details.type, lazy.UrlbarUtils.RESULT_TYPE.URL);
    this.Assert.equal(details.isSponsored, isSponsored, "Result isSponsored");
    if (url) {
      this.Assert.equal(details.url, url, "Result URL");
    }
    if (originalUrl) {
      this.Assert.equal(
        result.payload.originalUrl,
        originalUrl,
        "Result original URL"
      );
    }

    this.Assert.equal(!!result.isBestMatch, isBestMatch, "Result isBestMatch");

    let { row } = details.element;

    let sponsoredElement = row._elements.get("description");
    if (isSponsored || isBestMatch) {
      this.Assert.ok(sponsoredElement, "Result sponsored label element exists");
      this.Assert.equal(
        sponsoredElement.textContent,
        isSponsored ? "Sponsored" : "",
        "Result sponsored label"
      );
    } else {
      this.Assert.ok(
        !sponsoredElement,
        "Result sponsored label element should not exist"
      );
    }

    this.Assert.equal(
      result.payload.helpUrl,
      lazy.QuickSuggest.HELP_URL,
      "Result helpURL"
    );

    this.Assert.ok(
      row._buttons.get("menu"),
      "The menu button should be present"
    );

    return details;
  }

  /**
   * Asserts a result is not a quick suggest result.
   *
   * @param {object} window
   *   The window that should be used for this assertion
   * @param {number} index
   *   The index of the result.
   */
  async assertIsNotQuickSuggest(window, index) {
    let details = await lazy.UrlbarTestUtils.getDetailsOfResultAt(
      window,
      index
    );
    this.Assert.notEqual(
      details.result.providerName,
      "UrlbarProviderQuickSuggest",
      `Result at index ${index} is not provided by UrlbarProviderQuickSuggest`
    );
  }

  /**
   * Asserts that none of the results are quick suggest results.
   *
   * @param {object} window
   *   The window that should be used for this assertion
   */
  async assertNoQuickSuggestResults(window) {
    for (let i = 0; i < lazy.UrlbarTestUtils.getResultCount(window); i++) {
      await this.assertIsNotQuickSuggest(window, i);
    }
  }

  /**
   * Checks the values of all the quick suggest telemetry keyed scalars and,
   * if provided, other non-quick-suggest keyed scalars. Scalar values are all
   * assumed to be 1.
   *
   * @param {object} expectedKeysByScalarName
   *   Maps scalar names to keys that are expected to be recorded. The value for
   *   each key is assumed to be 1. If you expect a scalar to be incremented,
   *   include it in this object; otherwise, don't include it.
   */
  assertScalars(expectedKeysByScalarName) {
    let scalars = lazy.TelemetryTestUtils.getProcessScalars(
      "parent",
      true,
      true
    );

    // Check all quick suggest scalars.
    expectedKeysByScalarName = { ...expectedKeysByScalarName };
    for (let scalarName of Object.values(
      lazy.UrlbarProviderQuickSuggest.TELEMETRY_SCALARS
    )) {
      if (scalarName in expectedKeysByScalarName) {
        lazy.TelemetryTestUtils.assertKeyedScalar(
          scalars,
          scalarName,
          expectedKeysByScalarName[scalarName],
          1
        );
        delete expectedKeysByScalarName[scalarName];
      } else {
        this.Assert.ok(
          !(scalarName in scalars),
          "Scalar should not be present: " + scalarName
        );
      }
    }

    // Check any other remaining scalars that were passed in.
    for (let [scalarName, key] of Object.entries(expectedKeysByScalarName)) {
      lazy.TelemetryTestUtils.assertKeyedScalar(scalars, scalarName, key, 1);
    }
  }

  /**
   * Checks quick suggest telemetry events. This is the same as
   * `TelemetryTestUtils.assertEvents()` except it filters in only quick suggest
   * events by default. If you are expecting events that are not in the quick
   * suggest category, use `TelemetryTestUtils.assertEvents()` directly or pass
   * in a filter override for `category`.
   *
   * @param {Array} expectedEvents
   *   List of expected telemetry events.
   * @param {object} filterOverrides
   *   Extra properties to set in the filter object.
   * @param {object} options
   *   The options object to pass to `TelemetryTestUtils.assertEvents()`.
   */
  assertEvents(expectedEvents, filterOverrides = {}, options = undefined) {
    lazy.TelemetryTestUtils.assertEvents(
      expectedEvents,
      {
        category: lazy.QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        ...filterOverrides,
      },
      options
    );
  }

  /**
   * Creates a `sinon.sandbox` and `sinon.spy` that can be used to instrument
   * the quick suggest custom telemetry pings. If `init` was called with a test
   * scope where `registerCleanupFunction` is defined, the sandbox will
   * automically be restored at the end of the test.
   *
   * @returns {object}
   *   An object: { sandbox, spy, spyCleanup }
   *   `spyCleanup` is a cleanup function that should be called if you're in a
   *   browser chrome test and you did not also call `init`, or if you need to
   *   remove the spy before the test ends for some other reason. You can ignore
   *   it otherwise.
   */
  createTelemetryPingSpy() {
    let sandbox = lazy.sinon.createSandbox();
    let spy = sandbox.spy(
      PartnerLinkAttribution._pingCentre,
      "sendStructuredIngestionPing"
    );
    let spyCleanup = () => sandbox.restore();
    this.registerCleanupFunction?.(spyCleanup);
    return { sandbox, spy, spyCleanup };
  }

  /**
   * Asserts that custom telemetry pings are recorded in the order they appear
   * in the given `pings` array and that no other pings are recorded.
   *
   * @param {object} spy
   *   A `sinon.spy` object. See `createTelemetryPingSpy()`. This method resets
   *   the spy before returning.
   * @param {Array} pings
   *   The expected pings in the order they are expected to be recorded. Each
   *   item in this array should be an object: `{ type, payload }`
   *
   *   {string} type
   *     The ping's expected type, one of the `CONTEXTUAL_SERVICES_PING_TYPES`
   *     values.
   *   {object} payload
   *     The ping's expected payload. For convenience, you can leave out
   *     properties whose values are expected to be the default values defined
   *     in `DEFAULT_PING_PAYLOADS`.
   */
  assertPings(spy, pings) {
    let calls = spy.getCalls();
    this.Assert.equal(
      calls.length,
      pings.length,
      "Expected number of ping calls"
    );

    for (let i = 0; i < pings.length; i++) {
      let ping = pings[i];
      this.#log(
        "assertPings",
        `Checking ping at index ${i}, expected is: ` + JSON.stringify(ping)
      );

      // Add default properties to the expected payload for any that aren't
      // already defined.
      let { type, payload } = ping;
      let defaultPayload = DEFAULT_PING_PAYLOADS[type];
      this.Assert.ok(
        defaultPayload,
        `Sanity check: Default payload exists for type: ${type}`
      );
      payload = { ...defaultPayload, ...payload };

      // Check the endpoint URL.
      let call = calls[i];
      let endpointURL = call.args[1];
      this.Assert.ok(
        endpointURL.includes(type),
        `Endpoint URL corresponds to the expected ping type: ${type}`
      );

      // Check the payload.
      let actualPayload = call.args[0];
      this.#assertPingPayload(actualPayload, payload);
    }

    spy.resetHistory();
  }

  /**
   * Helper for checking contextual services ping payloads.
   *
   * @param {object} actualPayload
   *   The actual payload in the ping.
   * @param {object} expectedPayload
   *   An object describing the expected payload. Non-function values in this
   *   object are checked for equality against the corresponding actual payload
   *   values. Function values are called and passed the corresponding actual
   *   values and should return true if the actual values are correct.
   */
  #assertPingPayload(actualPayload, expectedPayload) {
    this.#log(
      "#assertPingPayload",
      "Checking ping payload. Actual: " +
        JSON.stringify(actualPayload) +
        " -- Expected (excluding function properties): " +
        JSON.stringify(expectedPayload)
    );

    this.Assert.equal(
      Object.entries(actualPayload).length,
      Object.entries(expectedPayload).length,
      "Payload has expected number of properties"
    );

    for (let [key, expectedValue] of Object.entries(expectedPayload)) {
      let actualValue = actualPayload[key];
      if (typeof expectedValue == "function") {
        this.Assert.ok(expectedValue(actualValue), "Payload property: " + key);
      } else {
        this.Assert.equal(
          actualValue,
          expectedValue,
          "Payload property: " + key
        );
      }
    }
  }

  /**
   * Asserts that URLs in a result's payload have the timestamp template
   * substring replaced with real timestamps.
   *
   * @param {UrlbarResult} result The results to check
   * @param {object} urls
   *   An object that contains the expected payload properties with template
   *   substrings. For example:
   *   ```js
   *   {
   *     url: "http://example.com/foo-%YYYYMMDDHH%",
   *     sponsoredClickUrl: "http://example.com/bar-%YYYYMMDDHH%",
   *   }
   *   ```
   */
  assertTimestampsReplaced(result, urls) {
    let { TIMESTAMP_TEMPLATE, TIMESTAMP_LENGTH } = lazy.QuickSuggest;

    // Parse the timestamp strings from each payload property and save them in
    // `urls[key].timestamp`.
    urls = { ...urls };
    for (let [key, url] of Object.entries(urls)) {
      let index = url.indexOf(TIMESTAMP_TEMPLATE);
      this.Assert.ok(
        index >= 0,
        `Timestamp template ${TIMESTAMP_TEMPLATE} is in URL ${url} for key ${key}`
      );
      let value = result.payload[key];
      this.Assert.ok(value, "Key is in result payload: " + key);
      let timestamp = value.substring(index, index + TIMESTAMP_LENGTH);

      // Set `urls[key]` to an object that's helpful in the logged info message
      // below.
      urls[key] = { url, value, timestamp };
    }

    this.#log(
      "assertTimestampsReplaced",
      "Parsed timestamps: " + JSON.stringify(urls)
    );

    // Make a set of unique timestamp strings. There should only be one.
    let { timestamp } = Object.values(urls)[0];
    this.Assert.deepEqual(
      [...new Set(Object.values(urls).map(o => o.timestamp))],
      [timestamp],
      "There's only one unique timestamp string"
    );

    // Parse the parts of the timestamp string.
    let year = timestamp.slice(0, -6);
    let month = timestamp.slice(-6, -4);
    let day = timestamp.slice(-4, -2);
    let hour = timestamp.slice(-2);
    let date = new Date(year, month - 1, day, hour);

    // The timestamp should be no more than two hours in the past. Typically it
    // will be the same as the current hour, but since its resolution is in
    // terms of hours and it's possible the test may have crossed over into a
    // new hour as it was running, allow for the previous hour.
    this.Assert.less(
      Date.now() - 2 * 60 * 60 * 1000,
      date.getTime(),
      "Timestamp is within the past two hours"
    );
  }

  /**
   * Calls a callback while enrolled in a mock Nimbus experiment. The experiment
   * is automatically unenrolled and cleaned up after the callback returns.
   *
   * @param {object} options
   *   Options for the mock experiment.
   * @param {Function} options.callback
   *   The callback to call while enrolled in the mock experiment.
   * @param {object} options.options
   *   See {@link enrollExperiment}.
   */
  async withExperiment({ callback, ...options }) {
    let doExperimentCleanup = await this.enrollExperiment(options);
    await callback();
    await doExperimentCleanup();
  }

  /**
   * Enrolls in a mock Nimbus experiment.
   *
   * @param {object} options
   *   Options for the mock experiment.
   * @param {object} [options.valueOverrides]
   *   Values for feature variables.
   * @returns {Promise<Function>}
   *   The experiment cleanup function (async).
   */
  async enrollExperiment({ valueOverrides = {} }) {
    this.#log("enrollExperiment", "Awaiting ExperimentAPI.ready");
    await lazy.ExperimentAPI.ready();

    // Wait for any prior scenario updates to finish. If updates are ongoing,
    // UrlbarPrefs will ignore the Nimbus update when the experiment is
    // installed. This shouldn't be a problem in practice because in reality
    // scenario updates are triggered only on app startup and Nimbus
    // enrollments, but tests can trigger lots of updates back to back.
    await this.waitForScenarioUpdated();

    let doExperimentCleanup =
      await lazy.ExperimentFakes.enrollWithFeatureConfig({
        enabled: true,
        featureId: "urlbar",
        value: valueOverrides,
      });

    // Wait for the pref updates triggered by the experiment enrollment.
    this.#log(
      "enrollExperiment",
      "Awaiting update after enrolling in experiment"
    );
    await this.waitForScenarioUpdated();

    return async () => {
      this.#log("enrollExperiment.cleanup", "Awaiting experiment cleanup");
      await doExperimentCleanup();

      // The same pref updates will be triggered by unenrollment, so wait for
      // them again.
      this.#log(
        "enrollExperiment.cleanup",
        "Awaiting update after unenrolling in experiment"
      );
      await this.waitForScenarioUpdated();
    };
  }

  /**
   * Clears the Nimbus exposure event.
   */
  async clearExposureEvent() {
    // Exposure event recording is queued to the idle thread, so wait for idle
    // before we start so any events from previous tasks will have been recorded
    // and won't interfere with this task.
    await new Promise(resolve => Services.tm.idleDispatchToMainThread(resolve));

    Services.telemetry.clearEvents();
    lazy.NimbusFeatures.urlbar._didSendExposureEvent = false;
    lazy.QuickSuggest._recordedExposureEvent = false;
  }

  /**
   * Asserts the Nimbus exposure event is recorded or not as expected.
   *
   * @param {boolean} expectedRecorded
   *   Whether the event is expected to be recorded.
   */
  async assertExposureEvent(expectedRecorded) {
    this.Assert.equal(
      lazy.QuickSuggest._recordedExposureEvent,
      expectedRecorded,
      "_recordedExposureEvent is correct"
    );

    let filter = {
      category: "normandy",
      method: "expose",
      object: "nimbus_experiment",
    };

    let expectedEvents = [];
    if (expectedRecorded) {
      expectedEvents.push({
        ...filter,
        extra: {
          branchSlug: "control",
          featureId: "urlbar",
        },
      });
    }

    // The event recording is queued to the idle thread when the search starts,
    // so likewise queue the assert to idle instead of doing it immediately.
    await new Promise(resolve => {
      Services.tm.idleDispatchToMainThread(() => {
        lazy.TelemetryTestUtils.assertEvents(expectedEvents, filter);
        resolve();
      });
    });
  }

  /**
   * Sets the app's locales, calls your callback, and resets locales.
   *
   * @param {Array} locales
   *   An array of locale strings. The entire array will be set as the available
   *   locales, and the first locale in the array will be set as the requested
   *   locale.
   * @param {Function} callback
   *  The callback to be called with the {@link locales} set. This function can
   *  be async.
   */
  async withLocales(locales, callback) {
    let promiseChanges = async desiredLocales => {
      this.#log(
        "withLocales",
        "Changing locales from " +
          JSON.stringify(Services.locale.requestedLocales) +
          " to " +
          JSON.stringify(desiredLocales)
      );

      if (desiredLocales[0] == Services.locale.requestedLocales[0]) {
        // Nothing happens when the locale doesn't actually change.
        return;
      }

      this.#log("withLocales", "Waiting for intl:requested-locales-changed");
      await lazy.TestUtils.topicObserved("intl:requested-locales-changed");
      this.#log("withLocales", "Observed intl:requested-locales-changed");

      // Wait for the search service to reload engines. Otherwise tests can fail
      // in strange ways due to internal search service state during shutdown.
      // It won't always reload engines but it's hard to tell in advance when it
      // won't, so also set a timeout.
      this.#log("withLocales", "Waiting for TOPIC_SEARCH_SERVICE");
      await Promise.race([
        lazy.TestUtils.topicObserved(
          lazy.SearchUtils.TOPIC_SEARCH_SERVICE,
          (subject, data) => {
            this.#log(
              "withLocales",
              "Observed TOPIC_SEARCH_SERVICE with data: " + data
            );
            return data == "engines-reloaded";
          }
        ),
        new Promise(resolve => {
          lazy.setTimeout(() => {
            this.#log(
              "withLocales",
              "Timed out waiting for TOPIC_SEARCH_SERVICE"
            );
            resolve();
          }, 2000);
        }),
      ]);

      this.#log("withLocales", "Done waiting for locale changes");
    };

    let available = Services.locale.availableLocales;
    let requested = Services.locale.requestedLocales;

    let newRequested = locales.slice(0, 1);
    let promise = promiseChanges(newRequested);
    Services.locale.availableLocales = locales;
    Services.locale.requestedLocales = newRequested;
    await promise;

    this.Assert.equal(
      Services.locale.appLocaleAsBCP47,
      locales[0],
      "App locale is now " + locales[0]
    );

    await callback();

    promise = promiseChanges(requested);
    Services.locale.availableLocales = available;
    Services.locale.requestedLocales = requested;
    await promise;
  }

  #log(fnName, msg) {
    this.info?.(`QuickSuggestTestUtils.${fnName} ${msg}`);
  }

  #remoteSettingsServer;
  #restoreRemoteSettings;
}

export var QuickSuggestTestUtils = new _QuickSuggestTestUtils();
