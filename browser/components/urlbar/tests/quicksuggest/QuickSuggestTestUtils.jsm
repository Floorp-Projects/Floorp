/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const EXPORTED_SYMBOLS = ["QuickSuggestTestUtils"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  CONTEXTUAL_SERVICES_PING_TYPES:
    "resource:///modules/PartnerLinkAttribution.jsm",
  ExperimentAPI: "resource://nimbus/ExperimentAPI.jsm",
  ExperimentFakes: "resource://testing-common/NimbusTestUtils.jsm",
  ExperimentManager: "resource://nimbus/lib/ExperimentManager.jsm",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
  PartnerLinkAttribution: "resource:///modules/PartnerLinkAttribution.jsm",
  sinon: "resource://testing-common/Sinon.jsm",
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.jsm",
  TestUtils: "resource://testing-common/TestUtils.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProviderQuickSuggest:
    "resource:///modules/UrlbarProviderQuickSuggest.jsm",
  UrlbarQuickSuggest: "resource:///modules/UrlbarQuickSuggest.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

XPCOMUtils.defineLazyGetter(this, "UrlbarTestUtils", () => {
  const { UrlbarTestUtils: module } = ChromeUtils.import(
    "resource://testing-common/UrlbarTestUtils.jsm"
  );
  module.init(QuickSuggestTestUtils._testScope);
  return module;
});

const DEFAULT_CONFIG = {
  best_match: {
    blocked_suggestion_ids: [],
    min_search_string_length: 4,
  },
};

const DEFAULT_PING_PAYLOADS = {
  [CONTEXTUAL_SERVICES_PING_TYPES.QS_BLOCK]: {
    advertiser: "testadvertiser",
    block_id: 1,
    context_id: () => actual => !!actual,
    iab_category: "22 - Shopping",
    improve_suggest_experience_checked: false,
    match_type: "firefox-suggest",
    request_id: null,
  },
  [CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION]: {
    advertiser: "testadvertiser",
    block_id: 1,
    context_id: () => actual => !!actual,
    improve_suggest_experience_checked: false,
    match_type: "firefox-suggest",
    reporting_url: "http://example.com/click",
    request_id: null,
  },
  [CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION]: {
    advertiser: "testadvertiser",
    block_id: 1,
    context_id: () => actual => !!actual,
    improve_suggest_experience_checked: false,
    is_clicked: false,
    match_type: "firefox-suggest",
    reporting_url: "http://example.com/impression",
    request_id: null,
  },
};

const LEARN_MORE_URL =
  Services.urlFormatter.formatURLPref("app.support.baseURL") +
  "firefox-suggest";

const TELEMETRY_EVENT_CATEGORY = "contextservices.quicksuggest";

const UPDATE_TOPIC = "firefox-suggest-update";
const UPDATE_SKIPPED_TOPIC = "firefox-suggest-update-skipped";

// On `init`, the following properties and methods are copied from the test
// scope to the `TestUtils` object so they can be easily accessed. Be careful
// about assuming a particular property will be defined because depending on the
// scope -- browser test or xpcshell test -- some may not be.
const TEST_SCOPE_PROPERTIES = [
  "Assert",
  "EventUtils",
  "info",
  "registerCleanupFunction",
];

/**
 * Test utils for quick suggest.
 */
class QSTestUtils {
  get LEARN_MORE_URL() {
    return LEARN_MORE_URL;
  }

  get BEST_MATCH_LEARN_MORE_URL() {
    return UrlbarProviderQuickSuggest.bestMatchHelpUrl;
  }

  get SCALARS() {
    return UrlbarProviderQuickSuggest.TELEMETRY_SCALARS;
  }

  get TELEMETRY_EVENT_CATEGORY() {
    return TELEMETRY_EVENT_CATEGORY;
  }

  get UPDATE_TOPIC() {
    return UPDATE_TOPIC;
  }

  get UPDATE_SKIPPED_TOPIC() {
    return UPDATE_SKIPPED_TOPIC;
  }

  get DEFAULT_CONFIG() {
    // Return a clone so callers can modify it.
    return Cu.cloneInto(DEFAULT_CONFIG, this);
  }

  /**
   * Call to init the utils. This allows this instance to access test helpers
   * available in the test's scope like Assert.
   *
   * @param {object} scope
   *   The global scope where tests are being run.
   */
  init(scope) {
    if (!scope) {
      throw new Error(
        "QuickSuggestTestUtils.init() must be called with a scope"
      );
    }
    this._testScope = scope;
    for (let p of TEST_SCOPE_PROPERTIES) {
      this[p] = scope[p];
    }
    // If you add other properties to `this`, null them in uninit().

    Services.telemetry.clearScalars();
  }

  /**
   * Tests that call `init` should call this function in their cleanup callback,
   * or else their scope will affect subsequent tests. This is usually only
   * required for tests outside browser/components/urlbar.
   */
  uninit() {
    this._testScope = null;
    for (let p of TEST_SCOPE_PROPERTIES) {
      this[p] = null;
    }
    Services.telemetry.clearScalars();
  }

  /**
   * Waits for quick suggest initialization to finish, ensures its data will not
   * be updated again during the test, and also optionally sets it up with mock
   * data.
   *
   * @param {array} [results]
   *   Array of quick suggest result objects. If not given, then this function
   *   won't set up any mock data.
   * @param {object} [config]
   *   Configuration object.
   * @returns {function}
   *   A cleanup function. You only need to call this function if you're in a
   *   browser chrome test and you did not also call `init`. You can ignore it
   *   otherwise.
   */
  async ensureQuickSuggestInit(results = null, config = DEFAULT_CONFIG) {
    this.info?.(
      "ensureQuickSuggestInit awaiting UrlbarQuickSuggest.readyPromise"
    );
    await UrlbarQuickSuggest.readyPromise;
    this.info?.(
      "ensureQuickSuggestInit done awaiting UrlbarQuickSuggest.readyPromise"
    );

    // Stub _queueSettingsSync() so any actual remote settings syncs that happen
    // during the test are ignored.
    let sandbox = sinon.createSandbox();
    sandbox.stub(UrlbarQuickSuggest, "_queueSettingsSync");
    let cleanup = () => sandbox.restore();
    this.registerCleanupFunction?.(cleanup);

    if (results) {
      UrlbarQuickSuggest._resultsByKeyword.clear();
      await UrlbarQuickSuggest._addResults(results);
    }
    if (config) {
      this.setConfig(config);
    }

    return cleanup;
  }

  /**
   * Enrolls in a mock Nimbus rollout.
   *
   * If you call UrlbarPrefs.updateFirefoxSuggestScenario() from an xpcshell
   * test, you must call this first to intialize the Nimbus urlbar feature.
   *
   * @param {object} value
   *   Define any desired Nimbus variables in this object.
   * @returns {function}
   *   A cleanup function that will remove the mock rollout.
   */
  async initNimbusFeature(value = {}) {
    this.info?.("initNimbusFeature awaiting ExperimentManager.onStartup");
    await ExperimentManager.onStartup();

    this.info?.("initNimbusFeature awaiting ExperimentAPI.ready");
    await ExperimentAPI.ready();

    this.info?.("initNimbusFeature awaiting ExperimentFakes.enrollWithRollout");
    let doCleanup = await ExperimentFakes.enrollWithRollout({
      featureId: NimbusFeatures.urlbar.featureId,
      value: { enabled: true, ...value },
    });

    this.info?.("initNimbusFeature done");

    this.registerCleanupFunction?.(() => {
      // If `doCleanup()` has already been called (i.e., by the caller), it will
      // throw an error here.
      try {
        doCleanup();
      } catch (error) {}
    });

    return doCleanup;
  }

  /**
   * Sets the quick suggest configuration. You should call this again with
   * `DEFAULT_CONFIG` before your test finishes. See also `withConfig()`.
   *
   * @param {object} config
   */
  setConfig(config) {
    UrlbarQuickSuggest._setConfig(config);
  }

  /**
   * Sets the quick suggest configuration, calls your callback, and restores the
   * default configuration.
   *
   * @param {object} config
   * @param {function} callback
   */
  async withConfig({ config, callback }) {
    this.setConfig(config);
    await callback();
    this.setConfig(DEFAULT_CONFIG);
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
    await UrlbarPrefs.updateFirefoxSuggestScenario({ scenario });
  }

  /**
   * Waits for any prior scenario update to finish.
   */
  async waitForScenarioUpdated() {
    await TestUtils.waitForCondition(
      () => !UrlbarPrefs.updatingFirefoxSuggestScenario,
      "Waiting for updatingFirefoxSuggestScenario to be false"
    );
  }

  /**
   * Asserts a result is a quick suggest result.
   *
   * @param {string} url
   *   The expected URL. At least one of `url` and `originalUrl` must be given.
   * @param {string} originalUrl
   *   The expected original URL (the URL with an unreplaced timestamp
   *   template). At least one of `url` and `originalUrl` must be given.
   * @param {object} window
   * @param {number} [index]
   *   The expected index of the quick suggest result. Pass -1 to use the index
   *   of the last result.
   * @param {boolean} [isSponsored]
   *   Whether the result is expected to be sponsored.
   * @param {boolean} [isBestMatch]
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
      let resultCount = UrlbarTestUtils.getResultCount(window);
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

    let details = await UrlbarTestUtils.getDetailsOfResultAt(window, index);
    let { result } = details;

    this.info?.(
      `Checking actual result at index ${index}: ` + JSON.stringify(result)
    );

    this.Assert.equal(
      result.providerName,
      "UrlbarProviderQuickSuggest",
      "Result provider name is UrlbarProviderQuickSuggest"
    );
    this.Assert.equal(details.type, UrlbarUtils.RESULT_TYPE.URL);
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

    let sponsoredElement = isBestMatch
      ? row._elements.get("bottom")
      : row._elements.get("action");
    this.Assert.ok(sponsoredElement, "Result sponsored label element exists");
    this.Assert.equal(
      sponsoredElement.textContent,
      isSponsored ? "Sponsored" : "",
      "Result sponsored label"
    );

    let helpButton = row._buttons.get("help");
    this.Assert.ok(helpButton, "The help button should be present");
    this.Assert.equal(result.payload.helpUrl, LEARN_MORE_URL, "Result helpURL");

    let blockButton = row._buttons.get("block");
    if (!isBestMatch) {
      this.Assert.equal(
        !!blockButton,
        UrlbarPrefs.get("quickSuggestBlockingEnabled"),
        "The block button is present iff quick suggest blocking is enabled"
      );
    } else {
      this.Assert.equal(
        !!blockButton,
        UrlbarPrefs.get("bestMatchBlockingEnabled"),
        "The block button is present iff best match blocking is enabled"
      );
    }

    return details;
  }

  /**
   * Asserts a result is not a quick suggest result.
   *
   * @param {object} window
   * @param {number} index
   *   The index of the result.
   */
  async assertIsNotQuickSuggest(window, index) {
    let details = await UrlbarTestUtils.getDetailsOfResultAt(window, index);
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
   */
  async assertNoQuickSuggestResults(window) {
    for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
      await this.assertIsNotQuickSuggest(window, i);
    }
  }

  /**
   * Checks the values of all the quick suggest telemetry scalars.
   *
   * @param {object} expectedIndexesByScalarName
   *   Maps scalar names to the expected 1-based indexes of results. If you
   *   expect a scalar to be incremented, then include it in this object. If you
   *   expect a scalar not to be incremented, don't include it.
   */
  assertScalars(expectedIndexesByScalarName) {
    let scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
    for (let scalarName of Object.values(this.SCALARS)) {
      if (scalarName in expectedIndexesByScalarName) {
        TelemetryTestUtils.assertKeyedScalar(
          scalars,
          scalarName,
          expectedIndexesByScalarName[scalarName],
          1
        );
      } else {
        this.Assert.ok(
          !(scalarName in scalars),
          "Scalar should not be present: " + scalarName
        );
      }
    }
  }

  /**
   * Checks quick suggest telemetry events. This is the same as
   * `TelemetryTestUtils.assertEvents()` except it filters in only quick suggest
   * events by default. If you are expecting events that are not in the quick
   * suggest category, use `TelemetryTestUtils.assertEvents()` directly or pass
   * in a filter override for `category`.
   *
   * @param {array} expectedEvents
   *   List of expected telemetry events.
   * @param {object} filterOverrides
   *   Extra properties to set in the filter object.
   * @param {object} options
   *   The options object to pass to `TelemetryTestUtils.assertEvents()`.
   */
  assertEvents(expectedEvents, filterOverrides = {}, options = undefined) {
    TelemetryTestUtils.assertEvents(
      expectedEvents,
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
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
    let sandbox = sinon.createSandbox();
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
   * @param {array} pings
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
      this.info?.(
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
      this._assertPingPayload(actualPayload, payload);
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
  _assertPingPayload(actualPayload, expectedPayload) {
    this.info?.(
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
   * @param {UrlbarResult} result
   * @param {object} urls
   *   An object that contains the expected payload properties with template
   *   substrings. For example:
   *
   *   {
   *     url: "http://example.com/foo-%YYYYMMDDHH%",
   *     sponsoredClickUrl: "http://example.com/bar-%YYYYMMDDHH%",
   *   }
   */
  assertTimestampsReplaced(result, urls) {
    let { TIMESTAMP_TEMPLATE, TIMESTAMP_LENGTH } = UrlbarProviderQuickSuggest;

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

    this.info?.("Parsed timestamps: " + JSON.stringify(urls));

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
   * @param {function} callback
   * @param {object} options
   *   See enrollExperiment().
   */
  async withExperiment({ callback, ...options }) {
    let doExperimentCleanup = await this.enrollExperiment(options);
    await callback();
    await doExperimentCleanup();
  }

  /**
   * Enrolls in a mock Nimbus experiment.
   *
   * @param {object} [valueOverrides]
   *   Values for feature variables.
   * @returns {function}
   *   The experiment cleanup function (async).
   */
  async enrollExperiment({ valueOverrides = {} }) {
    this.info?.("Awaiting ExperimentAPI.ready");
    await ExperimentAPI.ready();

    // Wait for any prior scenario updates to finish. If updates are ongoing,
    // UrlbarPrefs will ignore the Nimbus update when the experiment is
    // installed. This shouldn't be a problem in practice because in reality
    // scenario updates are triggered only on app startup and Nimbus
    // enrollments, but tests can trigger lots of updates back to back.
    await this.waitForScenarioUpdated();

    // These notifications signal either that pref updates due to enrollment are
    // done or that updates weren't necessary.
    let updatePromise = Promise.race([
      TestUtils.topicObserved(QuickSuggestTestUtils.UPDATE_TOPIC),
      TestUtils.topicObserved(QuickSuggestTestUtils.UPDATE_SKIPPED_TOPIC),
    ]);

    let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
      enabled: true,
      featureId: "urlbar",
      value: valueOverrides,
    });

    // Wait for the pref updates triggered by the experiment enrollment.
    this.info?.("Awaiting update after enrolling in experiment");
    await updatePromise;

    return async () => {
      // The same pref updates will be triggered by unenrollment, so wait for
      // them again.
      let unenrollUpdatePromise = Promise.race([
        TestUtils.topicObserved(QuickSuggestTestUtils.UPDATE_TOPIC),
        TestUtils.topicObserved(QuickSuggestTestUtils.UPDATE_SKIPPED_TOPIC),
      ]);

      this.info?.("Awaiting experiment cleanup");
      await doExperimentCleanup();
      this.info?.("Awaiting update after unenrolling in experiment");
      await unenrollUpdatePromise;
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
    NimbusFeatures.urlbar._didSendExposureEvent = false;
    UrlbarQuickSuggest._recordedExposureEvent = false;
  }

  /**
   * Asserts the Nimbus exposure event is recorded or not as expected.
   *
   * @param {boolean} expectedRecorded
   *   Whether the event is expected to be recorded.
   * @param {string} [branchSlug]
   *   If the event is expected to be recorded, then this should be the name of
   *   the experiment branch for which it was recorded.
   */
  async assertExposureEvent(expectedRecorded) {
    this.Assert.equal(
      UrlbarQuickSuggest._recordedExposureEvent,
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
        TelemetryTestUtils.assertEvents(expectedEvents, filter);
        resolve();
      });
    });
  }
}

var QuickSuggestTestUtils = new QSTestUtils();
