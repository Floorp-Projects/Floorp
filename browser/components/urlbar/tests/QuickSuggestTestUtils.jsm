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
  PartnerLinkAttribution: "resource:///modules/PartnerLinkAttribution.jsm",
  Services: "resource://gre/modules/Services.jsm",
  sinon: "resource://testing-common/Sinon.jsm",
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.jsm",
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
class TestUtils {
  get LEARN_MORE_URL() {
    return LEARN_MORE_URL;
  }

  get SCALARS() {
    return SCALARS;
  }

  get TELEMETRY_EVENT_CATEGORY() {
    return TELEMETRY_EVENT_CATEGORY;
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
   * @param {array} [data]
   *   Array of quick suggest data objects. If not given, then this function
   *   won't set up any mock data.
   * @returns {function}
   *   A cleanup function. You only need to call this function if you're in a
   *   browser chrome test and you did not also call `init`. You can ignore it
   *   otherwise.
   */
  async ensureQuickSuggestInit(data = null) {
    this.info?.("Awaiting UrlbarQuickSuggest.init");
    await UrlbarQuickSuggest.init();
    this.info?.("Done awaiting UrlbarQuickSuggest.init");
    let sandbox = sinon.createSandbox();
    sandbox.stub(UrlbarQuickSuggest, "_ensureAttachmentsDownloadedHelper");
    let cleanup = () => sandbox.restore();
    this.registerCleanupFunction?.(cleanup);
    if (data) {
      this.info?.("Awaiting UrlbarQuickSuggest._processSuggestionsJSON");
      await UrlbarQuickSuggest._processSuggestionsJSON(data);
      this.info?.("Done awaiting UrlbarQuickSuggest._processSuggestionsJSON");
    }
    return cleanup;
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
    search_query,
    spy,
    advertiser = "test-advertiser",
    block_id = 1,
    matched_keywords = search_query,
    reporting_url = "http://impression.reporting.test.com/",
    scenario = "offline",
  }) {
    this.Assert.ok(spy.calledOnce, "Should send a custom impression ping");

    let [payload, endpoint] = spy.firstCall.args;

    this.Assert.ok(
      endpoint.includes(CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION),
      "Should set the endpoint for QuickSuggest impression"
    );

    // Check payload properties that should match exactly.
    let expectedPayload = {
      advertiser,
      block_id,
      matched_keywords: scenario == "online" ? matched_keywords : "",
      position: index + 1,
      reporting_url,
      scenario,
      search_query: scenario == "online" ? search_query : "",
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
    this.Assert.ok(spy.notCalled, "Should not send a custom impression");
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
    // Impression is sent first, followed by the click
    this.Assert.ok(spy.calledTwice, "Should send a custom impression ping");

    let [payload, endpoint] = spy.secondCall.args;

    this.Assert.ok(
      endpoint.includes(CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION),
      "Should set the endpoint for QuickSuggest click"
    );

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
    // Only called once for the impression
    this.Assert.ok(spy.calledOnce, "Should not send a custom impression");
  }
}

var QuickSuggestTestUtils = new TestUtils();
