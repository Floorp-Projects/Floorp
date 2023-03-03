/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BaseFeature } from "resource:///modules/urlbar/private/BaseFeature.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  clearInterval: "resource://gre/modules/Timer.sys.mjs",
  MerinoClient: "resource:///modules/MerinoClient.sys.mjs",
  PromiseUtils: "resource://gre/modules/PromiseUtils.sys.mjs",
  setInterval: "resource://gre/modules/Timer.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
});

const FETCH_INTERVAL_MS = 30 * 60 * 1000; // 30 minutes
const MERINO_PROVIDER = "accuweather";
const MERINO_TIMEOUT_MS = 5000; // 5s

const HISTOGRAM_LATENCY = "FX_URLBAR_MERINO_LATENCY_WEATHER_MS";
const HISTOGRAM_RESPONSE = "FX_URLBAR_MERINO_RESPONSE_WEATHER";

/**
 * A feature that periodically fetches weather suggestions from Merino.
 */
export class Weather extends BaseFeature {
  get shouldEnable() {
    return (
      lazy.UrlbarPrefs.get("weatherFeatureGate") &&
      lazy.UrlbarPrefs.get("suggest.weather") &&
      lazy.UrlbarPrefs.get("merinoEnabled")
    );
  }

  get enablingPreferences() {
    return ["suggest.weather"];
  }

  /**
   * @returns {object}
   *   The last weather suggestion fetched from Merino or null if none.
   */
  get suggestion() {
    return this.#suggestion;
  }

  /**
   * @returns {Set}
   *   The set of keywords that should trigger the weather suggestion. Null if
   *   the suggestion should be shown on zero prefix (empty search string).
   */
  get keywords() {
    return this.#keywords;
  }

  update() {
    super.update();
    if (this.isEnabled) {
      this.#updateKeywords();
    }
  }

  enable(enabled) {
    if (enabled) {
      this.#init();
    } else {
      this.#uninit();
    }
  }

  /**
   * Returns a promise that resolves when all pending fetches finish, if there
   * are pending fetches. If there aren't, the promise resolves when all pending
   * fetches starting with the next fetch finish.
   *
   * @returns {Promise}
   */
  waitForFetches() {
    if (!this.#waitForFetchesDeferred) {
      this.#waitForFetchesDeferred = lazy.PromiseUtils.defer();
    }
    return this.#waitForFetchesDeferred.promise;
  }

  #init() {
    this.#merino = new lazy.MerinoClient(this.constructor.name);

    this.#fetchInterval = lazy.setInterval(
      () => this.#fetch(),
      this.#fetchIntervalMs
    );

    // `#fetch()` is async but there's no need to await it here.
    this.#fetch();

    this.#updateKeywords();
  }

  #uninit() {
    this.#merino = null;
    this.#suggestion = null;
    lazy.clearInterval(this.#fetchInterval);
    this.#fetchInterval = 0;
    this.#keywords = null;
  }

  async #fetch() {
    this.logger.info("Fetching suggestion");

    // This `Weather` instance may be uninitialized while awaiting the fetch or
    // even uninitialized and re-initialized a number of times. Discard the
    // fetched suggestion if the `#merino` after the fetch isn't the same as the
    // one before.
    let merino = this.#merino;

    let suggestions;
    this.#pendingFetchCount++;
    try {
      suggestions = await merino.fetch({
        query: "",
        providers: [MERINO_PROVIDER],
        timeoutMs: this.#timeoutMs,
        extraLatencyHistogram: HISTOGRAM_LATENCY,
        extraResponseHistogram: HISTOGRAM_RESPONSE,
      });
    } finally {
      this.#pendingFetchCount--;
    }

    // Reset the Merino client's session so different fetches use different
    // sessions. A single session is intended to represent a single user
    // engagement in the urlbar, which this is not. Practically this isn't
    // necessary since the client automatically resets the session on a timer
    // whose period is much shorter than our fetch period, but there's no reason
    // to keep it ticking in the meantime.
    merino.resetSession();

    if (merino != this.#merino) {
      this.logger.info("Fetch canceled, discarding fetched suggestion, if any");
    } else {
      let suggestion = suggestions?.[0];
      if (!suggestion) {
        this.logger.info("No suggestion received");
        this.#suggestion = null;
      } else {
        this.logger.info("Got suggestion");
        this.logger.debug(JSON.stringify({ suggestion }));
        this.#suggestion = { ...suggestion, source: "merino" };
      }
    }

    if (!this.#pendingFetchCount) {
      this.#waitForFetchesDeferred?.resolve();
      this.#waitForFetchesDeferred = null;
    }
  }

  #updateKeywords() {
    let fullKeywords = lazy.UrlbarPrefs.get("weatherKeywords");
    let minLength = lazy.UrlbarPrefs.get("weatherKeywordsMinimumLength");
    if (!fullKeywords || !minLength) {
      this.#keywords = null;
      return;
    }

    this.#keywords = new Set();

    // Create keywords that are prefixes of the full keywords starting at the
    // specified minimum length.
    for (let full of fullKeywords) {
      this.#keywords.add(full);
      for (let i = minLength; i < full.length; i++) {
        this.#keywords.add(full.substring(0, i));
      }
    }
  }

  get _test_merino() {
    return this.#merino;
  }

  get _test_fetchInterval() {
    return this.#fetchInterval;
  }

  get _test_pendingFetchCount() {
    return this.#pendingFetchCount;
  }

  async _test_fetch() {
    await this.#fetch();
  }

  _test_setFetchIntervalMs(ms) {
    this.#fetchIntervalMs = ms < 0 ? FETCH_INTERVAL_MS : ms;
  }

  _test_setTimeoutMs(ms) {
    this.#timeoutMs = ms < 0 ? MERINO_TIMEOUT_MS : ms;
  }

  #merino = null;
  #suggestion = null;
  #fetchInterval = 0;
  #fetchIntervalMs = FETCH_INTERVAL_MS;
  #timeoutMs = MERINO_TIMEOUT_MS;
  #waitForFetchesDeferred = null;
  #pendingFetchCount = 0;
  #keywords = null;
}
