/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const EXPORTED_SYMBOLS = ["QuickSuggestTestUtils"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  CONTEXTUAL_SERVICES_PING_TYPES:
    "resource:///modules/PartnerLinkAttribution.jsm",
  ExperimentAPI: "resource://nimbus/ExperimentAPI.jsm",
  ExperimentFakes: "resource://testing-common/NimbusTestUtils.jsm",
  ExperimentManager: "resource://nimbus/lib/ExperimentManager.jsm",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
  PartnerLinkAttribution: "resource:///modules/PartnerLinkAttribution.jsm",
  Services: "resource://gre/modules/Services.jsm",
  sinon: "resource://testing-common/Sinon.jsm",
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.jsm",
  TestUtils: "resource://testing-common/TestUtils.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
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

const LEARN_MORE_URL =
  Services.urlFormatter.formatURLPref("app.support.baseURL") +
  "firefox-suggest";

const SCALARS = {
  IMPRESSION: "contextual.services.quicksuggest.impression",
  CLICK: "contextual.services.quicksuggest.click",
  HELP: "contextual.services.quicksuggest.help",
};

const TELEMETRY_EVENT_CATEGORY = "contextservices.quicksuggest";

const UPDATE_TOPIC = "firefox-suggest-update";

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

  get SCALARS() {
    return SCALARS;
  }

  get TELEMETRY_EVENT_CATEGORY() {
    return TELEMETRY_EVENT_CATEGORY;
  }

  get UPDATE_TOPIC() {
    return UPDATE_TOPIC;
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
   * @returns {function}
   *   A cleanup function. You only need to call this function if you're in a
   *   browser chrome test and you did not also call `init`. You can ignore it
   *   otherwise.
   */
  async ensureQuickSuggestInit(results = null) {
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
      UrlbarQuickSuggest._addResults(results);
    }

    return cleanup;
  }

  /**
   * If you call UrlbarPrefs.updateFirefoxSuggestScenario() from an xpcshell
   * test, you must call this first to intialize the Nimbus urlbar feature.
   */
  async initNimbusFeature() {
    this.info?.("initNimbusFeature awaiting ExperimentManager.onStartup");
    await ExperimentManager.onStartup();

    this.info?.("initNimbusFeature awaiting ExperimentAPI.ready");
    await ExperimentAPI.ready();

    this.info?.("initNimbusFeature awaiting ExperimentFakes.enrollWithRollout");
    let doCleanup = await ExperimentFakes.enrollWithRollout({
      featureId: NimbusFeatures.urlbar.featureId,
      value: { enabled: true },
    });

    this.info?.("initNimbusFeature done");

    this.registerCleanupFunction(doCleanup);
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
   *   The expected URL.
   * @param {object} window
   * @param {number} [index]
   *   The expected index of the quick suggest result. Pass -1 to use the index
   *   of the last result.
   * @param {boolean} [isSponsored]
   *   Whether the result is expected to be sponsored.
   * @returns {result}
   *   The quick suggest result.
   */
  async assertIsQuickSuggest({
    url,
    window,
    index = -1,
    isSponsored = true,
  } = {}) {
    if (index < 0) {
      index = UrlbarTestUtils.getResultCount(window) - 1;
      this.Assert.greater(
        index,
        -1,
        "Sanity check: Result count should be > 0"
      );
    }

    let details = await UrlbarTestUtils.getDetailsOfResultAt(window, index);
    this.Assert.equal(details.type, UrlbarUtils.RESULT_TYPE.URL);
    this.Assert.equal(details.isSponsored, isSponsored, "Result isSponsored");
    this.Assert.equal(details.url, url, "Result URL");
    this.Assert.equal(
      details.displayed.action,
      isSponsored ? "Sponsored" : "",
      "Result action text"
    );

    let helpButton = details.element.row._elements.get("helpButton");
    this.Assert.ok(helpButton, "The help button should be present");
    this.Assert.equal(
      details.result.payload.helpUrl,
      LEARN_MORE_URL,
      "Result helpURL"
    );

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
    for (let scalarName of Object.values(SCALARS)) {
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
   * Asserts that a custom telemetry impression ping was sent with the expected
   * payload.
   *
   * @param {number} index
   *   The expected zero-based index of the quick suggest result.
   * @param {string} search_query
   *   The search string that triggered the quick suggest result.
   * @param {object} spy
   *   A `sinon.spy` object. See `createTelemetryPingSpy`.
   * @param {string} [advertiser]
   *   The expected advertiser in the ping.
   * @param {number} [block_id]
   *   The expected block_id in the ping.
   * @param {string} [matched_keywords]
   *   The expected matched_keywords in the ping.
   * @param {string} [reporting_url]
   *   The expected reporting_url in the ping.
   * @param {string} [scenario]
   *   The quick suggest scenario, one of: "history", "offline", "online"
   */
  assertImpressionPing({
    index,
    spy,
    advertiser = "test-advertiser",
    block_id = 1,
    search_query = undefined,
    matched_keywords = search_query,
    reporting_url = "http://impression.reporting.test.com/",
    scenario = "offline",
  }) {
    // Find the call for `QS_IMPRESSION`.
    let calls = spy.getCalls().filter(call => {
      let endpoint = call.args[1];
      return endpoint.includes(CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION);
    });
    this.Assert.equal(calls.length, 1, "Sent one impression ping");

    let payload = calls[0].args[0];

    // Check payload properties that should match exactly.
    let expectedPayload = {
      advertiser,
      block_id,
      matched_keywords,
      position: index + 1,
      reporting_url,
      scenario,
      search_query,
    };
    let actualPayload = {};
    for (let key of Object.keys(expectedPayload)) {
      actualPayload[key] = payload[key];
    }
    this.Assert.deepEqual(actualPayload, expectedPayload, "Payload is correct");

    // Check payload properties that don't need to match exactly.
    this.Assert.ok(!!payload.context_id, "Should set the context_id");
  }

  /**
   * Asserts no custom telemetry impression ping was sent.
   *
   * @param {object} spy
   *   A `sinon.spy` object. See `createTelemetryPingSpy`.
   */
  assertNoImpressionPing(spy) {
    // Find the call for `QS_IMPRESSION`.
    let calls = spy.getCalls().filter(call => {
      let endpoint = call.args[1];
      return endpoint.includes(CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION);
    });
    this.Assert.equal(calls.length, 0, "Did not send an impression ping");
  }

  /**
   * Asserts that a custom telemetry click ping was sent with the expected
   * payload.
   *
   * @param {number} index
   *   The expected zero-based index of the quick suggest result.
   * @param {object} spy
   *   A `sinon.spy` object. See `createTelemetryPingSpy`.
   * @param {string} [advertiser]
   *   The expected advertiser in the ping.
   * @param {number} [block_id]
   *   The expected block_id in the ping.
   * @param {string} [reporting_url]
   *   The expected reporting_url in the ping.
   * @param {string} [scenario]
   *   The quick suggest scenario, one of: "history", "offline", "online"
   */
  assertClickPing({
    index,
    spy,
    advertiser = "test-advertiser",
    block_id = 1,
    reporting_url = "http://click.reporting.test.com/",
    scenario = "offline",
  }) {
    // Find the call for `QS_SELECTION`.
    let calls = spy.getCalls().filter(call => {
      let endpoint = call.args[1];
      return endpoint.includes(CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION);
    });
    this.Assert.equal(calls.length, 1, "Sent one click ping");

    let payload = calls[0].args[0];

    // Check payload properties that should match exactly.
    let expectedPayload = {
      advertiser,
      block_id,
      position: index + 1,
      reporting_url,
      scenario,
    };
    let actualPayload = {};
    for (let key of Object.keys(expectedPayload)) {
      actualPayload[key] = payload[key];
    }
    this.Assert.deepEqual(actualPayload, expectedPayload, "Payload is correct");

    // Check payload properties that don't need to match exactly.
    this.Assert.ok(!!payload.context_id, "Should set the context_id");
  }

  /**
   * Asserts no custom telemetry click ping was sent.
   *
   * @param {object} spy
   *   A `sinon.spy` object. See `createTelemetryPingSpy`.
   */
  assertNoClickPing(spy) {
    // Find the call for `QS_SELECTION`.
    let calls = spy.getCalls().filter(call => {
      let endpoint = call.args[1];
      return endpoint.includes(CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION);
    });
    this.Assert.equal(calls.length, 0, "Did not send a click ping");
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
    let template = "%YYYYMMDDHH%";

    // Parse the timestamp strings from each payload property and save them in
    // `urls[key].timestamp`.
    urls = { ...urls };
    for (let [key, url] of Object.entries(urls)) {
      let index = url.indexOf(template);
      this.Assert.ok(
        index >= 0,
        `Timestamp template ${template} is in URL ${url} for key ${key}`
      );
      let value = result.payload[key];
      this.Assert.ok(value, "Key is in result payload: " + key);
      let timestamp = value.substring(index, index + template.length - 2);

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

    // This notification signals that pref updates due to enrollment are done.
    let updatePromise = TestUtils.topicObserved(
      QuickSuggestTestUtils.UPDATE_TOPIC
    );

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
      let unenrollUpdatePromise = TestUtils.topicObserved(
        QuickSuggestTestUtils.UPDATE_TOPIC
      );
      this.info?.("Awaiting experiment cleanup");
      await doExperimentCleanup();
      this.info?.("Awaiting update after unenrolling in experiment");
      await unenrollUpdatePromise;
    };
  }
}

var QuickSuggestTestUtils = new QSTestUtils();
