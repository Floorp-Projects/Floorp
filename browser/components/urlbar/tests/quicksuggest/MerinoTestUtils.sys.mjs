/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PromiseUtils: "resource://gre/modules/PromiseUtils.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.jsm",
});

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

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
const CLIENT_TIMEOUT_MS = 1000;

const HISTOGRAM_LATENCY = "FX_URLBAR_MERINO_LATENCY_MS";
const HISTOGRAM_RESPONSE = "FX_URLBAR_MERINO_RESPONSE";

// Maps from string labels of the `FX_URLBAR_MERINO_RESPONSE` histogram to their
// numeric values.
const RESPONSE_HISTOGRAM_VALUES = {
  success: 0,
  timeout: 1,
  network_error: 2,
  http_error: 3,
};

/**
 * Test utils for Merino.
 */
export class MerinoTestUtils {
  /**
   * @param {object} scope
   *   The global JS scope where tests are being run. This allows the instance
   *   to access test helpers like `Assert` that are available in the scope.
   */
  constructor(scope) {
    if (!scope) {
      throw new Error("MerinoTestUtils() must be called with a scope");
    }
    for (let p of TEST_SCOPE_PROPERTIES) {
      this[p] = scope[p];
    }
    // If you add other properties to `this`, null them in `uninit()`.

    this.#server = new MockMerinoServer(scope);
    lazy.UrlbarPrefs.set("merino.timeoutMs", CLIENT_TIMEOUT_MS);

    scope.registerCleanupFunction?.(() => this.uninit());
  }

  /**
   * Uninitializes the utils. If they were created with a test scope that
   * defines `registerCleanupFunction()`, you don't need to call this yourself
   * because it will automatically be called as a cleanup function. Otherwise
   * you'll need to call this.
   */
  uninit() {
    for (let p of TEST_SCOPE_PROPERTIES) {
      this[p] = null;
    }

    this.#server.uninit();
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
   * @returns {object}
   *   An object of histograms: `{ latency, response }`
   */
  getAndClearHistograms() {
    return {
      latency: lazy.TelemetryTestUtils.getAndClearHistogram(HISTOGRAM_LATENCY),
      response: lazy.TelemetryTestUtils.getAndClearHistogram(
        HISTOGRAM_RESPONSE
      ),
    };
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
    // Check the response histogram.
    if (response) {
      this.Assert.ok(
        RESPONSE_HISTOGRAM_VALUES.hasOwnProperty(response),
        "Sanity check: Expected response is valid: " + response
      );
      lazy.TelemetryTestUtils.assertHistogram(
        histograms.response,
        RESPONSE_HISTOGRAM_VALUES[response],
        1
      );
    } else {
      this.Assert.strictEqual(
        histograms.response.snapshot().sum,
        0,
        "Response histogram not updated"
      );
    }

    // Check the latency histogram.
    if (latencyRecorded) {
      // There should be a single value across all buckets.
      this.Assert.deepEqual(
        Object.values(histograms.latency.snapshot().values).filter(v => v > 0),
        [1],
        "Latency histogram updated"
      );
    } else {
      this.Assert.strictEqual(
        histograms.latency.snapshot().sum,
        0,
        "Latency histogram not updated"
      );
    }

    // Check the latency stopwatch.
    this.Assert.equal(
      TelemetryStopwatch.running(
        HISTOGRAM_LATENCY,
        client._test_latencyStopwatchInstance
      ),
      latencyStopwatchRunning,
      "Latency stopwatch running as expected"
    );

    // Clear histograms.
    for (let histogram of Object.values(histograms)) {
      histogram.clear();
    }
  }

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
            full_keyword: "full_keyword",
            title: "title",
            url: "url",
            icon: null,
            impression_url: "impression_url",
            click_url: "click_url",
            block_id: 1,
            advertiser: "advertiser",
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
