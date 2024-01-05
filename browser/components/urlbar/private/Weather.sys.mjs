/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BaseFeature } from "resource:///modules/urlbar/private/BaseFeature.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
  MerinoClient: "resource:///modules/MerinoClient.sys.mjs",
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
});

const FETCH_DELAY_AFTER_COMING_ONLINE_MS = 3000; // 3s
const FETCH_INTERVAL_MS = 30 * 60 * 1000; // 30 minutes
const MERINO_PROVIDER = "accuweather";
const MERINO_TIMEOUT_MS = 5000; // 5s

const HISTOGRAM_LATENCY = "FX_URLBAR_MERINO_LATENCY_WEATHER_MS";
const HISTOGRAM_RESPONSE = "FX_URLBAR_MERINO_RESPONSE_WEATHER";

const NOTIFICATIONS = {
  CAPTIVE_PORTAL_LOGIN: "captive-portal-login-success",
  LINK_STATUS_CHANGED: "network:link-status-changed",
  OFFLINE_STATUS_CHANGED: "network:offline-status-changed",
  WAKE: "wake_notification",
};

/**
 * A feature that periodically fetches weather suggestions from Merino.
 */
export class Weather extends BaseFeature {
  get shouldEnable() {
    // The feature itself is enabled by setting these prefs regardless of
    // whether any config is defined. This is necessary to allow the feature to
    // sync the config from remote settings and Nimbus. Suggestion fetches will
    // not start until the config has been either synced from remote settings or
    // set by Nimbus.
    return (
      lazy.UrlbarPrefs.get("weatherFeatureGate") &&
      lazy.UrlbarPrefs.get("suggest.weather")
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
   *   The set of keywords that should trigger the weather suggestion. This will
   *   be null when no config is defined.
   */
  get keywords() {
    return this.#keywords;
  }

  /**
   * @returns {number}
   *   The minimum prefix length of a weather keyword the user must type to
   *   trigger the suggestion. Note that the strings returned from `keywords`
   *   already take this into account. The min length is determined from the
   *   first config source below whose value is non-zero. If no source has a
   *   non-zero value, zero will be returned, and `this.keywords` will contain
   *   only full keywords.
   *
   *   1. The `weather.minKeywordLength` pref, which is set when the user
   *      increments the min length
   *   2. `weatherKeywordsMinimumLength` in Nimbus
   *   3. `min_keyword_length` in remote settings
   */
  get minKeywordLength() {
    let minLength =
      lazy.UrlbarPrefs.get("weather.minKeywordLength") ||
      lazy.UrlbarPrefs.get("weatherKeywordsMinimumLength") ||
      this.#rsData?.min_keyword_length ||
      0;
    return Math.max(minLength, 0);
  }

  /**
   * @returns {boolean}
   *   Weather the min keyword length can be incremented. A cap on the min
   *   length can be set in remote settings and Nimbus.
   */
  get canIncrementMinKeywordLength() {
    let cap =
      lazy.UrlbarPrefs.get("weatherKeywordsMinimumLengthCap") ||
      this.#rsData?.min_keyword_length_cap ||
      0;
    return !cap || this.minKeywordLength < cap;
  }

  update() {
    let wasEnabled = this.isEnabled;
    super.update();

    // This method is called by `QuickSuggest` in a
    // `NimbusFeatures.urlbar.onUpdate()` callback, when a change occurs to a
    // Nimbus variable or to a pref that's a fallback for a Nimbus variable. A
    // config-related variable or pref may have changed, so update it, but only
    // if the feature was already enabled because if it wasn't, `enable(true)`
    // was just called, which calls `#init()`, which calls `#updateConfig()`.
    if (wasEnabled && this.isEnabled) {
      this.#updateConfig();
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
   * Increments the minimum prefix length of a weather keyword the user must
   * type to trigger the suggestion, if possible. A cap on the min length can be
   * set in remote settings and Nimbus, and if the cap has been reached, the
   * length is not incremented.
   */
  incrementMinKeywordLength() {
    if (this.canIncrementMinKeywordLength) {
      lazy.UrlbarPrefs.set(
        "weather.minKeywordLength",
        this.minKeywordLength + 1
      );
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
      this.#waitForFetchesDeferred = Promise.withResolvers();
    }
    return this.#waitForFetchesDeferred.promise;
  }

  async onRemoteSettingsSync(rs) {
    this.logger.debug("Loading weather config from remote settings");
    let records = await rs.get({ filters: { type: "weather" } });
    if (!this.isEnabled) {
      return;
    }

    this.logger.debug("Got weather records: " + JSON.stringify(records));
    this.#rsData = records?.[0]?.weather;
    this.#updateConfig();
  }

  get #vpnDetected() {
    if (lazy.UrlbarPrefs.get("weather.ignoreVPN")) {
      return false;
    }

    let linkService =
      this._test_linkService ||
      Cc["@mozilla.org/network/network-link-service;1"].getService(
        Ci.nsINetworkLinkService
      );

    // `platformDNSIndications` throws `NS_ERROR_NOT_IMPLEMENTED` on all
    // platforms except Windows, so we can't detect a VPN on any other platform.
    try {
      return (
        linkService.platformDNSIndications &
        Ci.nsINetworkLinkService.VPN_DETECTED
      );
    } catch (e) {}
    return false;
  }

  #init() {
    // On feature init, we only update the config and listen for changes that
    // affect the config. Suggestion fetches will not start until a config has
    // been either synced from remote settings or set by Nimbus.
    this.#updateConfig();
    lazy.UrlbarPrefs.addObserver(this);
    lazy.QuickSuggest.jsBackend.register(this);
  }

  #uninit() {
    this.#stopFetching();
    lazy.QuickSuggest.jsBackend.unregister(this);
    lazy.UrlbarPrefs.removeObserver(this);
    this.#keywords = null;
  }

  #startFetching() {
    if (this.#merino) {
      this.logger.debug("Suggestion fetching already started");
      return;
    }

    this.logger.debug("Starting suggestion fetching");

    this.#merino = new lazy.MerinoClient(this.constructor.name);
    this.#fetch();
    for (let notif of Object.values(NOTIFICATIONS)) {
      Services.obs.addObserver(this, notif);
    }
  }

  #stopFetching() {
    if (!this.#merino) {
      this.logger.debug("Suggestion fetching already stopped");
      return;
    }

    this.logger.debug("Stopping suggestion fetching");

    for (let notif of Object.values(NOTIFICATIONS)) {
      Services.obs.removeObserver(this, notif);
    }
    lazy.clearTimeout(this.#fetchTimer);
    this.#merino = null;
    this.#suggestion = null;
    this.#fetchTimer = 0;
  }

  async #fetch() {
    this.logger.info("Fetching suggestion");

    if (this.#vpnDetected) {
      // A VPN is detected, so Merino will not be able to accurately determine
      // the user's location. Set the suggestion to null. We treat this as if
      // the network is offline (see below). When the VPN is disconnected, a
      // `network:link-status-changed` notification will be sent, triggering a
      // new fetch.
      this.logger.info("VPN detected, not fetching");
      this.#suggestion = null;
      if (!this.#pendingFetchCount) {
        this.#waitForFetchesDeferred?.resolve();
        this.#waitForFetchesDeferred = null;
      }
      return;
    }

    // This `Weather` instance may be uninitialized while awaiting the fetch or
    // even uninitialized and re-initialized a number of times. Multiple fetches
    // may also happen at once. Ignore the fetch below if `#merino` changes or
    // another fetch happens in the meantime.
    let merino = this.#merino;
    let instance = (this.#fetchInstance = {});

    this.#restartFetchTimer();
    this.#lastFetchTimeMs = Date.now();
    this.#pendingFetchCount++;

    let suggestions;
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

    if (merino != this.#merino || instance != this.#fetchInstance) {
      this.logger.info("Fetch finished but is out of date, ignoring");
    } else {
      let suggestion = suggestions?.[0];
      if (!suggestion) {
        // No suggestion was received. The network may be offline or there may
        // be some other problem. Set the suggestion to null: Better to show
        // nothing than outdated weather information. When the network comes
        // back online, one or more network notifications will be sent,
        // triggering a new fetch.
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

  #restartFetchTimer(ms = this.#fetchIntervalMs) {
    this.logger.debug(
      "Restarting fetch timer: " +
        JSON.stringify({ ms, fetchIntervalMs: this.#fetchIntervalMs })
    );

    lazy.clearTimeout(this.#fetchTimer);
    this.#fetchTimer = lazy.setTimeout(() => {
      this.logger.debug("Fetch timer fired");
      this.#fetch();
    }, ms);
    this._test_fetchTimerMs = ms;
  }

  #onMaybeCameOnline() {
    this.logger.debug("Maybe came online");

    // If the suggestion is null, we were offline the last time we tried to
    // fetch, at the start of the current fetch period. Otherwise the suggestion
    // was fetched successfully at the start of the current fetch period and is
    // therefore still fresh.
    if (!this.suggestion) {
      // Multiple notifications can occur at once when the network comes online,
      // and we don't want to do separate fetches for each. Start the timer with
      // a small timeout. If another notification happens in the meantime, we'll
      // start it again.
      this.#restartFetchTimer(this.#fetchDelayAfterComingOnlineMs);
    }
  }

  #onWake() {
    // Calculate the elapsed time between the last fetch and now, and the
    // remaining interval in the current fetch period.
    let elapsedMs = Date.now() - this.#lastFetchTimeMs;
    let remainingIntervalMs = this.#fetchIntervalMs - elapsedMs;
    this.logger.debug(
      "Wake: " +
        JSON.stringify({
          elapsedMs,
          remainingIntervalMs,
          fetchIntervalMs: this.#fetchIntervalMs,
        })
    );

    // Regardless of the elapsed time, we need to restart the fetch timer
    // because it didn't tick while the computer was asleep. If the elapsed time
    // >= the fetch interval, the remaining interval will be negative and we
    // need to fetch now, but do it after a brief delay in case other
    // notifications occur soon when the network comes online. If the elapsed
    // time < the fetch interval, the suggestion is still fresh so there's no
    // need to fetch. Just restart the timer with the remaining interval.
    if (remainingIntervalMs <= 0) {
      remainingIntervalMs = this.#fetchDelayAfterComingOnlineMs;
    }
    this.#restartFetchTimer(remainingIntervalMs);
  }

  #updateConfig() {
    this.logger.debug("Starting config update");

    // Get the full keywords, preferring Nimbus over remote settings.
    let fullKeywords =
      lazy.UrlbarPrefs.get("weatherKeywords") ?? this.#rsData?.keywords;
    if (!fullKeywords) {
      this.logger.debug("No keywords defined, stopping suggestion fetching");
      this.#keywords = null;
      this.#stopFetching();
      return;
    }

    let minLength = this.minKeywordLength;
    this.logger.debug(
      "Updating keywords: " + JSON.stringify({ fullKeywords, minLength })
    );

    if (!minLength) {
      this.logger.debug("Min length is undefined or zero, using full keywords");
      this.#keywords = new Set(fullKeywords);
    } else {
      // Create keywords that are prefixes of the full keywords starting at the
      // specified minimum length.
      this.#keywords = new Set();
      for (let full of fullKeywords) {
        for (let i = minLength; i <= full.length; i++) {
          this.#keywords.add(full.substring(0, i));
        }
      }
    }

    this.#startFetching();
  }

  onPrefChanged(pref) {
    if (pref == "weather.minKeywordLength") {
      this.#updateConfig();
    }
  }

  observe(subject, topic, data) {
    this.logger.debug(
      "Observed notification: " + JSON.stringify({ topic, data })
    );

    switch (topic) {
      case NOTIFICATIONS.CAPTIVE_PORTAL_LOGIN:
        this.#onMaybeCameOnline();
        break;
      case NOTIFICATIONS.LINK_STATUS_CHANGED:
        // This notificaton means the user's connection status changed. See
        // nsINetworkLinkService.
        if (data != "down") {
          this.#onMaybeCameOnline();
        }
        break;
      case NOTIFICATIONS.OFFLINE_STATUS_CHANGED:
        // This notificaton means the user toggled the "Work Offline" pref.
        // See nsIIOService.
        if (data != "offline") {
          this.#onMaybeCameOnline();
        }
        break;
      case NOTIFICATIONS.WAKE:
        this.#onWake();
        break;
    }
  }

  get _test_fetchDelayAfterComingOnlineMs() {
    return this.#fetchDelayAfterComingOnlineMs;
  }
  set _test_fetchDelayAfterComingOnlineMs(ms) {
    this.#fetchDelayAfterComingOnlineMs =
      ms < 0 ? FETCH_DELAY_AFTER_COMING_ONLINE_MS : ms;
  }

  get _test_fetchIntervalMs() {
    return this.#fetchIntervalMs;
  }
  set _test_fetchIntervalMs(ms) {
    this.#fetchIntervalMs = ms < 0 ? FETCH_INTERVAL_MS : ms;
  }

  get _test_fetchTimer() {
    return this.#fetchTimer;
  }

  get _test_lastFetchTimeMs() {
    return this.#lastFetchTimeMs;
  }

  get _test_merino() {
    return this.#merino;
  }

  get _test_pendingFetchCount() {
    return this.#pendingFetchCount;
  }

  async _test_fetch() {
    await this.#fetch();
  }

  _test_setSuggestionToNull() {
    this.#suggestion = null;
  }

  _test_setTimeoutMs(ms) {
    this.#timeoutMs = ms < 0 ? MERINO_TIMEOUT_MS : ms;
  }

  #fetchDelayAfterComingOnlineMs = FETCH_DELAY_AFTER_COMING_ONLINE_MS;
  #fetchInstance = null;
  #fetchIntervalMs = FETCH_INTERVAL_MS;
  #fetchTimer = 0;
  #keywords = null;
  #lastFetchTimeMs = 0;
  #merino = null;
  #pendingFetchCount = 0;
  #rsData = null;
  #suggestion = null;
  #timeoutMs = MERINO_TIMEOUT_MS;
  #waitForFetchesDeferred = null;
}
