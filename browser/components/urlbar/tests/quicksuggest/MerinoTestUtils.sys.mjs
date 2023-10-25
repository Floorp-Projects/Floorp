/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PromiseUtils: "resource://gre/modules/PromiseUtils.sys.mjs",
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
});

import { HttpServer } from "resource://testing-common/httpd.sys.mjs";

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

const SEARCH_PARAMS = {
  CLIENT_VARIANTS: "client_variants",
  PROVIDERS: "providers",
  QUERY: "q",
  SEQUENCE_NUMBER: "seq",
  SESSION_ID: "sid",
};

const REQUIRED_SEARCH_PARAMS = [
  SEARCH_PARAMS.QUERY,
  SEARCH_PARAMS.SEQUENCE_NUMBER,
  SEARCH_PARAMS.SESSION_ID,
];

// We set the client timeout to a large value to avoid intermittent failures in
// CI, especially TV tests, where the Merino fetch unexpectedly doesn't finish
// before the default timeout.
const CLIENT_TIMEOUT_MS = 2000;

const HISTOGRAM_LATENCY = "FX_URLBAR_MERINO_LATENCY_MS";
const HISTOGRAM_RESPONSE = "FX_URLBAR_MERINO_RESPONSE";

// Maps from string labels of the `FX_URLBAR_MERINO_RESPONSE` histogram to their
// numeric values.
const RESPONSE_HISTOGRAM_VALUES = {
  success: 0,
  timeout: 1,
  network_error: 2,
  http_error: 3,
  no_suggestion: 4,
};

const WEATHER_KEYWORD = "weather";

const WEATHER_RS_DATA = {
  keywords: [WEATHER_KEYWORD],
};

const WEATHER_SUGGESTION = {
  title: "Weather for San Francisco",
  url: "https://example.com/weather",
  provider: "accuweather",
  is_sponsored: false,
  score: 0.2,
  icon: null,
  city_name: "San Francisco",
  current_conditions: {
    url: "https://example.com/weather-current-conditions",
    summary: "Mostly cloudy",
    icon_id: 6,
    temperature: { c: 15.5, f: 60.0 },
  },
  forecast: {
    url: "https://example.com/weather-forecast",
    summary: "Pleasant Saturday",
    high: { c: 21.1, f: 70.0 },
    low: { c: 13.9, f: 57.0 },
  },
};

// We set the weather suggestion fetch interval to an absurdly large value so it
// absolutely will not fire during tests.
const WEATHER_FETCH_INTERVAL_MS = 24 * 60 * 60 * 1000; // 24 hours

/**
 * Test utils for Merino.
 */
class _MerinoTestUtils {
  /**
   * Initializes the utils.
   *
   * @param {object} scope
   *   The global JS scope where tests are being run. This allows the instance
   *   to access test helpers like `Assert` that are available in the scope.
   */
  init(scope) {
    if (!scope) {
      throw new Error("MerinoTestUtils.init() must be called with a scope");
    }

    this.#initDepth++;
    scope.info?.("MerinoTestUtils init: Depth is now " + this.#initDepth);

    for (let p of TEST_SCOPE_PROPERTIES) {
      this[p] = scope[p];
    }
    // If you add other properties to `this`, null them in `uninit()`.

    if (!this.#server) {
      this.#server = new MockMerinoServer(scope);
    }
    lazy.UrlbarPrefs.set("merino.timeoutMs", CLIENT_TIMEOUT_MS);
    scope.registerCleanupFunction?.(() => {
      scope.info?.("MerinoTestUtils cleanup function");
      this.uninit();
    });
  }

  /**
   * Uninitializes the utils. If they were created with a test scope that
   * defines `registerCleanupFunction()`, you don't need to call this yourself
   * because it will automatically be called as a cleanup function. Otherwise
   * you'll need to call this.
   */
  uninit() {
    this.#initDepth--;
    this.info?.("MerinoTestUtils uninit: Depth is now " + this.#initDepth);

    if (this.#initDepth) {
      this.info?.("MerinoTestUtils uninit: Bailing because depth > 0");
      return;
    }
    this.info?.("MerinoTestUtils uninit: Now uninitializing");

    for (let p of TEST_SCOPE_PROPERTIES) {
      this[p] = null;
    }
    this.#server.uninit();
    this.#server = null;
    lazy.UrlbarPrefs.clear("merino.timeoutMs");
  }

  /**
   * @returns {object}
   *   The names of URL search params.
   */
  get SEARCH_PARAMS() {
    return SEARCH_PARAMS;
  }

  /**
   * @returns {string}
   *   The weather keyword in `WEATHER_RS_DATA`. Can be used as a search string
   *   to match the weather suggestion.
   */
  get WEATHER_KEYWORD() {
    return WEATHER_KEYWORD;
  }

  /**
   * @returns {object}
   *   Default remote settings data that sets up `WEATHER_KEYWORD` as the
   *   keyword for the weather suggestion.
   */
  get WEATHER_RS_DATA() {
    return { ...WEATHER_RS_DATA };
  }

  /**
   * @returns {object}
   *   A mock weather suggestion.
   */
  get WEATHER_SUGGESTION() {
    return WEATHER_SUGGESTION;
  }

  /**
   * @returns {MockMerinoServer}
   *   The mock Merino server. The server isn't started until its `start()`
   *   method is called.
   */
  get server() {
    return this.#server;
  }

  /**
   * Clears the Merino-related histograms and returns them.
   *
   * @param {object} options
   *   Options
   * @param {string} options.extraLatency
   *   The name of another latency histogram you expect to be updated.
   * @param {string} options.extraResponse
   *   The name of another response histogram you expect to be updated.
   * @returns {object}
   *   An object of histograms: `{ latency, response }`
   *   `latency` and `response` are both arrays of Histogram objects.
   */
  getAndClearHistograms({
    extraLatency = undefined,
    extraResponse = undefined,
  } = {}) {
    let histograms = {
      latency: [
        lazy.TelemetryTestUtils.getAndClearHistogram(HISTOGRAM_LATENCY),
      ],
      response: [
        lazy.TelemetryTestUtils.getAndClearHistogram(HISTOGRAM_RESPONSE),
      ],
    };
    if (extraLatency) {
      histograms.latency.push(
        lazy.TelemetryTestUtils.getAndClearHistogram(extraLatency)
      );
    }
    if (extraResponse) {
      histograms.response.push(
        lazy.TelemetryTestUtils.getAndClearHistogram(extraResponse)
      );
    }
    return histograms;
  }

  /**
   * Asserts the Merino-related histograms are updated as expected. Clears the
   * histograms before returning.
   *
   * @param {object} options
   *   Options object
   * @param {MerinoClient} options.client
   *   The relevant `MerinoClient` instance. This is used to check the latency
   *   stopwatch.
   * @param {object} options.histograms
   *   The histograms object returned from `getAndClearHistograms()`.
   * @param {string} options.response
   *   The expected string label for the `response` histogram. If the histogram
   *   should not be recorded, pass null.
   * @param {boolean} options.latencyRecorded
   *   Whether the latency histogram is expected to contain a value.
   * @param {boolean} options.latencyStopwatchRunning
   *   Whether the latency stopwatch is expected to be running.
   */
  checkAndClearHistograms({
    client,
    histograms,
    response,
    latencyRecorded,
    latencyStopwatchRunning = false,
  }) {
    // Check the response histograms.
    if (response) {
      this.Assert.ok(
        RESPONSE_HISTOGRAM_VALUES.hasOwnProperty(response),
        "Sanity check: Expected response is valid: " + response
      );
      for (let histogram of histograms.response) {
        lazy.TelemetryTestUtils.assertHistogram(
          histogram,
          RESPONSE_HISTOGRAM_VALUES[response],
          1
        );
      }
    } else {
      for (let histogram of histograms.response) {
        this.Assert.strictEqual(
          histogram.snapshot().sum,
          0,
          "Response histogram not updated: " + histogram.name()
        );
      }
    }

    // Check the latency histograms.
    if (latencyRecorded) {
      // There should be a single value across all buckets.
      for (let histogram of histograms.latency) {
        this.Assert.deepEqual(
          Object.values(histogram.snapshot().values).filter(v => v > 0),
          [1],
          "Latency histogram updated: " + histogram.name()
        );
      }
    } else {
      for (let histogram of histograms.latency) {
        this.Assert.strictEqual(
          histogram.snapshot().sum,
          0,
          "Latency histogram not updated: " + histogram.name()
        );
      }
    }

    // Check the latency stopwatch.
    if (!client) {
      this.Assert.ok(
        !latencyStopwatchRunning,
        "Client is null, latency stopwatch should not be expected to be running"
      );
    } else {
      this.Assert.equal(
        TelemetryStopwatch.running(
          HISTOGRAM_LATENCY,
          client._test_latencyStopwatchInstance
        ),
        latencyStopwatchRunning,
        "Latency stopwatch running as expected"
      );
    }

    // Clear histograms.
    for (let histogramArray of Object.values(histograms)) {
      for (let histogram of histogramArray) {
        histogram.clear();
      }
    }
  }

  /**
   * Initializes the quick suggest weather feature and mock Merino server.
   */
  async initWeather() {
    await this.server.start();
    this.server.response.body.suggestions = [WEATHER_SUGGESTION];

    lazy.QuickSuggest.weather._test_fetchIntervalMs = WEATHER_FETCH_INTERVAL_MS;

    // Enabling weather will trigger a fetch. Wait for it to finish so the
    // suggestion is ready when this function returns.
    let fetchPromise = lazy.QuickSuggest.weather.waitForFetches();
    lazy.UrlbarPrefs.set("weather.featureGate", true);
    lazy.UrlbarPrefs.set("suggest.weather", true);
    await fetchPromise;

    this.Assert.equal(
      lazy.QuickSuggest.weather._test_pendingFetchCount,
      0,
      "No pending fetches after awaiting initial fetch"
    );

    this.registerCleanupFunction?.(async () => {
      lazy.UrlbarPrefs.clear("weather.featureGate");
      lazy.UrlbarPrefs.clear("suggest.weather");
      lazy.QuickSuggest.weather._test_fetchIntervalMs = -1;
    });
  }

  #initDepth = 0;
  #server = null;
}

/**
 * A mock Merino server with useful helper methods.
 */
class MockMerinoServer {
  /**
   * Until `start()` is called the server isn't started and `this.url` is null.
   *
   * @param {object} scope
   *   The global JS scope where tests are being run. This allows the instance
   *   to access test helpers like `Assert` that are available in the scope.
   */
  constructor(scope) {
    scope.info?.("MockMerinoServer constructor");

    for (let p of TEST_SCOPE_PROPERTIES) {
      this[p] = scope[p];
    }

    let path = "/merino";
    this.#httpServer = new HttpServer();
    this.#httpServer.registerPathHandler(path, (req, resp) =>
      this.#handleRequest(req, resp)
    );
    this.#baseURL = new URL("http://localhost/");
    this.#baseURL.pathname = path;

    this.reset();
  }

  /**
   * Uninitializes the server.
   */
  uninit() {
    this.info?.("MockMerinoServer uninit");
    for (let p of TEST_SCOPE_PROPERTIES) {
      this[p] = null;
    }
  }

  /**
   * @returns {nsIHttpServer}
   *   The underlying HTTP server.
   */
  get httpServer() {
    return this.#httpServer;
  }

  /**
   * @returns {URL}
   *   The server's endpoint URL or null if the server isn't running.
   */
  get url() {
    return this.#url;
  }

  /**
   * @returns {Array}
   *   Array of received nsIHttpRequest objects. Requests are continually
   *   collected, and the list can be cleared with `reset()`.
   */
  get requests() {
    return this.#requests;
  }

  /**
   * @returns {object}
   *   An object that describes the response that the server will return. Can be
   *   modified or set to a different object to change the response. Can be
   *   reset to the default reponse by calling `reset()`. For details see
   *   `makeDefaultResponse()` and `#handleRequest()`. In summary:
   *
   *   {
   *     status,
   *     contentType,
   *     delay,
   *     body: {
   *       request_id,
   *       suggestions,
   *     },
   *   }
   */
  get response() {
    return this.#response;
  }
  set response(value) {
    this.#response = value;
  }

  /**
   * Starts the server and sets `this.url`. If the server was created with a
   * test scope that defines `registerCleanupFunction()`, you don't need to call
   * `stop()` yourself because it will automatically be called as a cleanup
   * function. Otherwise you'll need to call `stop()`.
   */
  async start() {
    if (this.#url) {
      return;
    }

    this.info("MockMerinoServer starting");

    this.#httpServer.start(-1);
    this.#url = new URL(this.#baseURL);
    this.#url.port = this.#httpServer.identity.primaryPort;

    this._originalEndpointURL = lazy.UrlbarPrefs.get("merino.endpointURL");
    lazy.UrlbarPrefs.set("merino.endpointURL", this.#url.toString());

    this.registerCleanupFunction?.(() => this.stop());

    // Wait for the server to actually start serving. In TV tests, where the
    // server is created over and over again, sometimes it doesn't seem to be
    // ready after being recreated even after `#httpServer.start()` is called.
    this.info("MockMerinoServer waiting to start serving...");
    this.reset();
    let suggestion;
    while (!suggestion) {
      let response = await fetch(this.#url);
      let body = await response?.json();
      suggestion = body?.suggestions?.[0];
    }
    this.reset();
    this.info("MockMerinoServer is now serving");
  }

  /**
   * Stops the server and cleans up other state.
   */
  async stop() {
    if (!this.#url) {
      return;
    }

    // `uninit()` may have already been called by this point and removed
    // `this.info()`, so don't assume it's defined.
    this.info?.("MockMerinoServer stopping");

    // Cancel delayed-response timers and resolve their promises. Otherwise, if
    // a test awaits this method before finishing, it will hang until the timers
    // fire and allow the server to send the responses.
    this.#cancelDelayedResponses();

    await this.#httpServer.stop();
    this.#url = null;
    lazy.UrlbarPrefs.set("merino.endpointURL", this._originalEndpointURL);

    this.info?.("MockMerinoServer is now stopped");
  }

  /**
   * Returns a new object that describes the default response the server will
   * return.
   *
   * @returns {object}
   */
  makeDefaultResponse() {
    return {
      status: 200,
      contentType: "application/json",
      body: {
        request_id: "request_id",
        suggestions: [
          {
            provider: "adm",
            full_keyword: "full_keyword",
            title: "title",
            url: "http://example.com/amp",
            icon: null,
            impression_url: "http://example.com/amp-impression",
            click_url: "http://example.com/amp-click",
            block_id: 1,
            advertiser: "amp",
            is_sponsored: true,
            score: 1,
          },
        ],
      },
    };
  }

  /**
   * Clears the received requests and sets the response to the default.
   */
  reset() {
    this.#requests = [];
    this.response = this.makeDefaultResponse();
    this.#cancelDelayedResponses();
  }

  /**
   * Asserts a given list of requests has been received. Clears the list of
   * received requests before returning.
   *
   * @param {Array} expected
   *   The expected requests. Each item should be an object: `{ params }`
   */
  checkAndClearRequests(expected) {
    let actual = this.requests.map(req => {
      let params = new URLSearchParams(req.queryString);
      return { params: Object.fromEntries(params) };
    });

    this.info("Checking requests");
    this.info("actual: " + JSON.stringify(actual));
    this.info("expect: " + JSON.stringify(expected));

    // Check the request count.
    this.Assert.equal(actual.length, expected.length, "Expected request count");
    if (actual.length != expected.length) {
      return;
    }

    // Check each request.
    for (let i = 0; i < actual.length; i++) {
      let a = actual[i];
      let e = expected[i];
      this.info("Checking requests at index " + i);
      this.info("actual: " + JSON.stringify(a));
      this.info("expect: " + JSON.stringify(e));

      // Check required search params.
      for (let p of REQUIRED_SEARCH_PARAMS) {
        this.Assert.ok(
          a.params.hasOwnProperty(p),
          "Required param is present in actual request: " + p
        );
        if (p != SEARCH_PARAMS.SESSION_ID) {
          this.Assert.ok(
            e.params.hasOwnProperty(p),
            "Required param is present in expected request: " + p
          );
        }
      }

      // If the expected request doesn't include a session ID, then:
      if (!e.params.hasOwnProperty(SEARCH_PARAMS.SESSION_ID)) {
        if (e.params[SEARCH_PARAMS.SEQUENCE_NUMBER] == 0 || i == 0) {
          // If its sequence number is zero, then copy the actual request's
          // sequence number to the expected request. As a convenience, do the
          // same if this is the first request.
          e.params[SEARCH_PARAMS.SESSION_ID] =
            a.params[SEARCH_PARAMS.SESSION_ID];
        } else {
          // Otherwise this is not the first request in the session and
          // therefore the session ID should be the same as the ID in the
          // previous expected request.
          e.params[SEARCH_PARAMS.SESSION_ID] =
            expected[i - 1].params[SEARCH_PARAMS.SESSION_ID];
        }
      }

      this.Assert.deepEqual(a, e, "Expected request at index " + i);

      let actualSessionID = a.params[SEARCH_PARAMS.SESSION_ID];
      this.Assert.ok(actualSessionID, "Session ID exists");
      this.Assert.ok(
        /^[0-9a-f]{8}-([0-9a-f]{4}-){3}[0-9a-f]{12}$/i.test(actualSessionID),
        "Session ID is a UUID"
      );
    }

    this.#requests = [];
  }

  /**
   * Temporarily creates the conditions for a network error. Any Merino fetches
   * that occur during the callback will fail with a network error.
   *
   * @param {Function} callback
   *   Callback function.
   */
  async withNetworkError(callback) {
    // Set the endpoint to a valid, unreachable URL.
    let originalURL = lazy.UrlbarPrefs.get("merino.endpointURL");
    lazy.UrlbarPrefs.set(
      "merino.endpointURL",
      "http://localhost/valid-but-unreachable-url"
    );

    // Set the timeout high enough that the network error exception will happen
    // first. On Mac and Linux the fetch naturally times out fairly quickly but
    // on Windows it seems to take 5s, so set our artificial timeout to 10s.
    let originalTimeout = lazy.UrlbarPrefs.get("merino.timeoutMs");
    lazy.UrlbarPrefs.set("merino.timeoutMs", 10000);

    await callback();

    lazy.UrlbarPrefs.set("merino.endpointURL", originalURL);
    lazy.UrlbarPrefs.set("merino.timeoutMs", originalTimeout);
  }

  /**
   * Returns a promise that will resolve when the next request is received.
   *
   * @returns {Promise}
   */
  waitForNextRequest() {
    if (!this.#nextRequestDeferred) {
      this.#nextRequestDeferred = lazy.PromiseUtils.defer();
    }
    return this.#nextRequestDeferred.promise;
  }

  /**
   * nsIHttpServer request handler.
   *
   * @param {nsIHttpRequest} httpRequest
   *   Request.
   * @param {nsIHttpResponse} httpResponse
   *   Response.
   */
  #handleRequest(httpRequest, httpResponse) {
    this.info(
      "MockMerinoServer received request with query string: " +
        JSON.stringify(httpRequest.queryString)
    );
    this.info(
      "MockMerinoServer replying with response: " +
        JSON.stringify(this.response)
    );

    // Add the request to the list of received requests.
    this.#requests.push(httpRequest);

    // Resolve promises waiting on the next request.
    this.#nextRequestDeferred?.resolve();
    this.#nextRequestDeferred = null;

    // Now set up and finish the response.
    httpResponse.processAsync();

    let { response } = this;

    let finishResponse = () => {
      let status = response.status || 200;
      httpResponse.setStatusLine("", status, status);

      let contentType = response.contentType || "application/json";
      httpResponse.setHeader("Content-Type", contentType, false);

      if (typeof response.body == "string") {
        httpResponse.write(response.body);
      } else if (response.body) {
        httpResponse.write(JSON.stringify(response.body));
      }

      httpResponse.finish();
    };

    if (typeof response.delay != "number") {
      finishResponse();
      return;
    }

    // Set up a timer to wait until the delay elapses. Since we called
    // `httpResponse.processAsync()`, we need to be careful to always finish the
    // response, even if the timer is canceled. Otherwise the server will hang
    // when we try to stop it at the end of the test. When an `nsITimer` is
    // canceled, its callback is *not* called. Therefore we set up a race
    // between the timer's callback and a deferred promise. If the timer is
    // canceled, resolving the deferred promise will resolve the race, and the
    // response can then be finished.

    let delayedResponseID = this.#nextDelayedResponseID++;
    this.info(
      "MockMerinoServer delaying response: " +
        JSON.stringify({ delayedResponseID, delay: response.delay })
    );

    let deferred = lazy.PromiseUtils.defer();
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    let record = { timer, resolve: deferred.resolve };
    this.#delayedResponseRecords.add(record);

    // Don't await this promise.
    Promise.race([
      deferred.promise,
      new Promise(resolve => {
        timer.initWithCallback(
          resolve,
          response.delay,
          Ci.nsITimer.TYPE_ONE_SHOT
        );
      }),
    ]).then(() => {
      this.info(
        "MockMerinoServer done delaying response: " +
          JSON.stringify({ delayedResponseID })
      );
      deferred.resolve();
      this.#delayedResponseRecords.delete(record);
      finishResponse();
    });
  }

  /**
   * Cancels the timers for delayed responses and resolves their promises.
   */
  #cancelDelayedResponses() {
    for (let { timer, resolve } of this.#delayedResponseRecords) {
      timer.cancel();
      resolve();
    }
    this.#delayedResponseRecords.clear();
  }

  #httpServer = null;
  #url = null;
  #baseURL = null;
  #response = null;
  #requests = [];
  #nextRequestDeferred = null;
  #nextDelayedResponseID = 0;
  #delayedResponseRecords = new Set();
}

export var MerinoTestUtils = new _MerinoTestUtils();
