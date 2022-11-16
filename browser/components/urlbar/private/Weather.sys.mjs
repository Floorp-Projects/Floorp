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

  enable(enabled) {
    if (enabled) {
      this.#init();
    } else {
      this.#uninit();
    }
  }

  /**
   * Returns a promise that's resolved when the next fetch finishes.
   *
   * @returns {Promise}
   */
  waitForNextFetch() {
    if (!this.#nextFetchDeferred) {
      this.#nextFetchDeferred = lazy.PromiseUtils.defer();
    }
    return this.#nextFetchDeferred.promise;
  }

  #init() {
    this.#merino = new lazy.MerinoClient(this.constructor.name);

    this.#fetchInterval = lazy.setInterval(
      () => this.#fetch(),
      this.#fetchIntervalMs
    );

    // `#fetch()` is async but there's no need to await it here.
    this.#fetch();
  }

  #uninit() {
    this.#merino = null;
    this.#suggestion = null;
    lazy.clearInterval(this.#fetchInterval);
    this.#fetchInterval = 0;
  }

  async #fetch() {
    this.logger.info("Fetching suggestion");

    let suggestions = await this.#merino.fetch({
      query: "",
      providers: [MERINO_PROVIDER],
      timeoutMs: MERINO_TIMEOUT_MS,
    });

    let suggestion = suggestions?.[0];
    if (!suggestion) {
      this.logger.info("No suggestion received");
      this.#suggestion = null;
    } else {
      this.logger.info("Got suggestion");
      this.logger.debug(JSON.stringify({ suggestion }));
      this.#suggestion = { ...suggestion, source: "merino" };
    }

    this.#nextFetchDeferred?.resolve();
    this.#nextFetchDeferred = null;
  }

  get _test_merino() {
    return this.#merino;
  }

  get _test_fetchInterval() {
    return this.#fetchInterval;
  }

  async _test_fetch() {
    await this.#fetch();
  }

  _test_setFetchIntervalMs(ms) {
    this.#fetchIntervalMs = ms < 0 ? FETCH_INTERVAL_MS : ms;
  }

  #merino = null;
  #suggestion = null;
  #fetchInterval = 0;
  #fetchIntervalMs = FETCH_INTERVAL_MS;
  #nextFetchDeferred = null;
}
