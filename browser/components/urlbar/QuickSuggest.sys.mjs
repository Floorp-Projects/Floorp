/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

import {
  TaskQueue,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarQuickSuggest: "resource:///modules/UrlbarQuickSuggest.sys.mjs",
  clearInterval: "resource://gre/modules/Timer.sys.mjs",
  setInterval: "resource://gre/modules/Timer.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
});

const TIMESTAMP_TEMPLATE = "%YYYYMMDDHH%";
const TIMESTAMP_LENGTH = 10;
const TIMESTAMP_REGEXP = /^\d{10}$/;

const IMPRESSION_COUNTERS_RESET_INTERVAL_MS = 60 * 60 * 1000; // 1 hour

const TELEMETRY_EVENT_CATEGORY = "contextservices.quicksuggest";

// This object maps impression stats object keys to their corresponding keys in
// the `extra` object of impression cap telemetry events. The main reason this
// is necessary is because the keys of the `extra` object are limited to 15
// characters in length, which some stats object keys exceed. It also forces us
// to be deliberate about keys we add to the `extra` object, since the `extra`
// object is limited to 10 keys.
let TELEMETRY_IMPRESSION_CAP_EXTRA_KEYS = {
  // stats object key -> `extra` telemetry event object key
  intervalSeconds: "intervalSeconds",
  startDateMs: "startDate",
  count: "count",
  maxCount: "maxCount",
  impressionDateMs: "impressionDate",
};

/**
 * This class manages the quick suggest feature (a.k.a Firefox Suggest) and has
 * related helpers.
 */
class _QuickSuggest {
  constructor() {
    lazy.UrlbarQuickSuggest.on("config-set", () =>
      this._validateImpressionStats()
    );

    this._updateFeatureState();
    lazy.NimbusFeatures.urlbar.onUpdate(() => this._updateFeatureState());

    lazy.UrlbarPrefs.addObserver(this);

    // Periodically record impression counters reset telemetry.
    this._setImpressionCountersResetInterval();

    // On shutdown, record any final impression counters reset telemetry.
    lazy.AsyncShutdown.profileChangeTeardown.addBlocker(
      "QuickSuggest: Record impression counters reset telemetry",
      () => this._resetElapsedImpressionCounters()
    );
  }

  /**
   * @returns {string} The name of the quick suggest telemetry event category.
   */
  get TELEMETRY_EVENT_CATEGORY() {
    return TELEMETRY_EVENT_CATEGORY;
  }

  /**
   * @returns {string} The timestamp template string used in quick suggest URLs.
   */
  get TIMESTAMP_TEMPLATE() {
    return TIMESTAMP_TEMPLATE;
  }

  /**
   * @returns {number} The length of the timestamp in quick suggest URLs.
   */
  get TIMESTAMP_LENGTH() {
    return TIMESTAMP_LENGTH;
  }

  get logger() {
    if (!this._logger) {
      this._logger = UrlbarUtils.getLogger({ prefix: "QuickSuggest" });
    }
    return this._logger;
  }

  /**
   * Blocks a suggestion.
   *
   * @param {string} originalUrl
   *   The suggestion's original URL with its unreplaced timestamp template.
   */
  async blockSuggestion(originalUrl) {
    this.logger.debug(`Queueing blockSuggestion: ${originalUrl}`);
    await this._blockTaskQueue.queue(async () => {
      this.logger.info(`Blocking suggestion: ${originalUrl}`);
      let digest = await this._getDigest(originalUrl);
      this.logger.debug(`Got digest for '${originalUrl}': ${digest}`);
      this._blockedDigests.add(digest);
      let json = JSON.stringify([...this._blockedDigests]);
      this._updatingBlockedDigests = true;
      try {
        lazy.UrlbarPrefs.set("quicksuggest.blockedDigests", json);
      } finally {
        this._updatingBlockedDigests = false;
      }
      this.logger.debug(`All blocked suggestions: ${json}`);
    });
  }

  /**
   * Gets whether a suggestion is blocked.
   *
   * @param {string} originalUrl
   *   The suggestion's original URL with its unreplaced timestamp template.
   * @returns {boolean}
   *   Whether the suggestion is blocked.
   */
  async isSuggestionBlocked(originalUrl) {
    this.logger.debug(`Queueing isSuggestionBlocked: ${originalUrl}`);
    return this._blockTaskQueue.queue(async () => {
      this.logger.info(`Getting blocked status: ${originalUrl}`);
      let digest = await this._getDigest(originalUrl);
      this.logger.debug(`Got digest for '${originalUrl}': ${digest}`);
      let isBlocked = this._blockedDigests.has(digest);
      this.logger.info(`Blocked status for '${originalUrl}': ${isBlocked}`);
      return isBlocked;
    });
  }

  /**
   * Unblocks all suggestions.
   */
  async clearBlockedSuggestions() {
    this.logger.debug(`Queueing clearBlockedSuggestions`);
    await this._blockTaskQueue.queue(() => {
      this.logger.info(`Clearing all blocked suggestions`);
      this._blockedDigests.clear();
      lazy.UrlbarPrefs.clear("quicksuggest.blockedDigests");
    });
  }

  /**
   * Called when a urlbar pref changes.
   *
   * @param {string} pref
   *   The name of the pref relative to `browser.urlbar`.
   */
  onPrefChanged(pref) {
    switch (pref) {
      case "quicksuggest.blockedDigests":
        if (!this._updatingBlockedDigests) {
          this.logger.info(
            "browser.urlbar.quicksuggest.blockedDigests changed"
          );
          this._loadBlockedDigests();
        }
        break;
      case "quicksuggest.impressionCaps.stats":
        if (!this._updatingImpressionStats) {
          this.logger.info(
            "browser.urlbar.quicksuggest.impressionCaps.stats changed"
          );
          this._loadImpressionStats();
        }
        break;
      case "quicksuggest.dataCollection.enabled":
        if (!lazy.UrlbarPrefs.updatingFirefoxSuggestScenario) {
          Services.telemetry.recordEvent(
            TELEMETRY_EVENT_CATEGORY,
            "data_collect_toggled",
            lazy.UrlbarPrefs.get(pref) ? "enabled" : "disabled"
          );
        }
        break;
      case "suggest.quicksuggest.nonsponsored":
        if (!lazy.UrlbarPrefs.updatingFirefoxSuggestScenario) {
          Services.telemetry.recordEvent(
            TELEMETRY_EVENT_CATEGORY,
            "enable_toggled",
            lazy.UrlbarPrefs.get(pref) ? "enabled" : "disabled"
          );
        }
        break;
      case "suggest.quicksuggest.sponsored":
        if (!lazy.UrlbarPrefs.updatingFirefoxSuggestScenario) {
          Services.telemetry.recordEvent(
            TELEMETRY_EVENT_CATEGORY,
            "sponsored_toggled",
            lazy.UrlbarPrefs.get(pref) ? "enabled" : "disabled"
          );
        }
        break;
    }
  }

  /**
   * Returns whether a given URL and quick suggest's URL are equivalent. URLs
   * are equivalent if they are identical except for substrings that replaced
   * templates in the original suggestion URL.
   *
   * For example, a suggestion URL from the backing suggestions source might
   * include a timestamp template "%YYYYMMDDHH%" like this:
   *
   *   http://example.com/foo?bar=%YYYYMMDDHH%
   *
   * When a quick suggest result is created from this suggestion URL, it's
   * created with a URL that is a copy of the suggestion URL but with the
   * template replaced with a real timestamp value, like this:
   *
   *   http://example.com/foo?bar=2021111610
   *
   * All URLs created from this single suggestion URL are considered equivalent
   * regardless of their real timestamp values.
   *
   * @param {string} url
   *   The URL to check.
   * @param {UrlbarResult} result
   *   The quick suggest result. Will compare {@link url} to `result.payload.url`
   * @returns {boolean}
   *   Whether `url` is equivalent to `result.payload.url`.
   */
  isURLEquivalentToResultURL(url, result) {
    // If the URLs aren't the same length, they can't be equivalent.
    let resultURL = result.payload.url;
    if (resultURL.length != url.length) {
      return false;
    }

    // If the result URL doesn't have a timestamp, then do a straight string
    // comparison.
    let { urlTimestampIndex } = result.payload;
    if (typeof urlTimestampIndex != "number" || urlTimestampIndex < 0) {
      return resultURL == url;
    }

    // Compare the first parts of the strings before the timestamps.
    if (
      resultURL.substring(0, urlTimestampIndex) !=
      url.substring(0, urlTimestampIndex)
    ) {
      return false;
    }

    // Compare the second parts of the strings after the timestamps.
    let remainderIndex = urlTimestampIndex + TIMESTAMP_LENGTH;
    if (resultURL.substring(remainderIndex) != url.substring(remainderIndex)) {
      return false;
    }

    // Test the timestamp against the regexp.
    let maybeTimestamp = url.substring(
      urlTimestampIndex,
      urlTimestampIndex + TIMESTAMP_LENGTH
    );
    return TIMESTAMP_REGEXP.test(maybeTimestamp);
  }

  /**
   * Some suggestion properties like `url` and `click_url` include template
   * substrings that must be replaced with real values. This method replaces
   * templates with appropriate values in place.
   *
   * @param {object} suggestion
   *   A suggestion object fetched from remote settings or Merino.
   */
  replaceSuggestionTemplates(suggestion) {
    let now = new Date();
    let timestampParts = [
      now.getFullYear(),
      now.getMonth() + 1,
      now.getDate(),
      now.getHours(),
    ];
    let timestamp = timestampParts
      .map(n => n.toString().padStart(2, "0"))
      .join("");
    for (let key of ["url", "click_url"]) {
      let value = suggestion[key];
      if (!value) {
        continue;
      }

      let timestampIndex = value.indexOf(TIMESTAMP_TEMPLATE);
      if (timestampIndex >= 0) {
        if (key == "url") {
          suggestion.urlTimestampIndex = timestampIndex;
        }
        // We could use replace() here but we need the timestamp index for
        // `suggestion.urlTimestampIndex`, and since we already have that, avoid
        // another O(n) substring search and manually replace the template with
        // the timestamp.
        suggestion[key] =
          value.substring(0, timestampIndex) +
          timestamp +
          value.substring(timestampIndex + TIMESTAMP_TEMPLATE.length);
      }
    }
  }

  /**
   * Increments the user's impression stats counters for the given type of
   * suggestion. This should be called only when a suggestion impression is
   * recorded.
   *
   * @param {string} type
   *   The suggestion type, one of: "sponsored", "nonsponsored"
   */
  updateImpressionStats(type) {
    this.logger.info("Starting impression stats update");
    this.logger.debug(
      JSON.stringify({
        type,
        currentStats: this._impressionStats,
        impression_caps: lazy.UrlbarQuickSuggest.config.impression_caps,
      })
    );

    // Don't bother recording anything if caps are disabled.
    let isSponsored = type == "sponsored";
    if (
      (isSponsored &&
        !lazy.UrlbarPrefs.get("quickSuggestImpressionCapsSponsoredEnabled")) ||
      (!isSponsored &&
        !lazy.UrlbarPrefs.get("quickSuggestImpressionCapsNonSponsoredEnabled"))
    ) {
      this.logger.info("Impression caps disabled, skipping update");
      return;
    }

    // Get the user's impression stats. Since stats are synced from caps, if the
    // stats don't exist then the caps don't exist, and don't bother recording
    // anything in that case.
    let stats = this._impressionStats[type];
    if (!stats) {
      this.logger.info("Impression caps undefined, skipping update");
      return;
    }

    // Increment counters.
    for (let stat of stats) {
      stat.count++;
      stat.impressionDateMs = Date.now();

      // Record a telemetry event for each newly hit cap.
      if (stat.count == stat.maxCount) {
        this.logger.info(`'${type}' impression cap hit`);
        this.logger.debug(JSON.stringify({ type, hitStat: stat }));
        this._recordImpressionCapEvent({
          stat,
          eventType: "hit",
          suggestionType: type,
        });
      }
    }

    // Save the stats.
    this._updatingImpressionStats = true;
    try {
      lazy.UrlbarPrefs.set(
        "quicksuggest.impressionCaps.stats",
        JSON.stringify(this._impressionStats)
      );
    } finally {
      this._updatingImpressionStats = false;
    }

    this.logger.info("Finished impression stats update");
    this.logger.debug(JSON.stringify({ newStats: this._impressionStats }));
  }

  /**
   * Returns a non-null value if an impression cap has been reached for the
   * given suggestion type and null otherwise. This method can therefore be used
   * to tell whether a cap has been reached for a given type. The actual return
   * value an object describing the impression stats that caused the cap to be
   * reached.
   *
   * @param {string} type
   *   The suggestion type, one of: "sponsored", "nonsponsored"
   * @returns {object}
   *   An impression stats object or null.
   */
  impressionCapHitStats(type) {
    this._resetElapsedImpressionCounters();
    let stats = this._impressionStats[type];
    if (stats) {
      let hitStats = stats.filter(s => s.maxCount <= s.count);
      if (hitStats.length) {
        return hitStats;
      }
    }
    return null;
  }

  /**
   * Records the Nimbus exposure event if it hasn't already been recorded during
   * the app session. This method actually queues the recording on idle because
   * it's potentially an expensive operation.
   */
  ensureExposureEventRecorded() {
    // `recordExposureEvent()` makes sure only one event is recorded per app
    // session even if it's called many times, but since it may be expensive, we
    // also keep `_recordedExposureEvent`.
    if (!this._recordedExposureEvent) {
      this._recordedExposureEvent = true;
      Services.tm.idleDispatchToMainThread(() =>
        lazy.NimbusFeatures.urlbar.recordExposureEvent({ once: true })
      );
    }
  }

  /**
   * Loads and validates impression stats.
   */
  _loadImpressionStats() {
    let json = lazy.UrlbarPrefs.get("quicksuggest.impressionCaps.stats");
    if (!json) {
      this._impressionStats = {};
    } else {
      try {
        this._impressionStats = JSON.parse(
          json,
          // Infinity, which is the `intervalSeconds` for the lifetime cap, is
          // stringified as `null` in the JSON, so convert it back to Infinity.
          (key, value) =>
            key == "intervalSeconds" && value === null ? Infinity : value
        );
      } catch (error) {}
    }
    this._validateImpressionStats();
  }

  /**
   * Validates impression stats, which includes two things:
   *
   * - Type checks stats and discards any that are invalid. We do this because
   *   stats are stored in prefs where anyone can modify them.
   * - Syncs stats with impression caps so that there is one stats object
   *   corresponding to each impression cap. See the `_impressionStats` comment
   *   for more info.
   */
  _validateImpressionStats() {
    let { impression_caps } = lazy.UrlbarQuickSuggest.config;

    this.logger.info("Validating impression stats");
    this.logger.debug(
      JSON.stringify({
        impression_caps,
        currentStats: this._impressionStats,
      })
    );

    if (!this._impressionStats || typeof this._impressionStats != "object") {
      this._impressionStats = {};
    }

    for (let [type, cap] of Object.entries(impression_caps || {})) {
      // Build a map from interval seconds to max counts in the caps.
      let maxCapCounts = (cap.custom || []).reduce(
        (map, { interval_s, max_count }) => {
          map.set(interval_s, max_count);
          return map;
        },
        new Map()
      );
      if (typeof cap.lifetime == "number") {
        maxCapCounts.set(Infinity, cap.lifetime);
      }

      let stats = this._impressionStats[type];
      if (!Array.isArray(stats)) {
        stats = [];
        this._impressionStats[type] = stats;
      }

      // Validate existing stats:
      //
      // * Discard stats with invalid properties.
      // * Collect and remove stats with intervals that aren't in the caps. This
      //   should only happen when caps are changed or removed.
      // * For stats with intervals that are in the caps:
      //   * Keep track of the max `stat.count` across all stats so we can
      //     update the lifetime stat below.
      //   * Set `stat.maxCount` to the max count in the corresponding cap.
      let orphanStats = [];
      let maxCountInStats = 0;
      for (let i = 0; i < stats.length; ) {
        let stat = stats[i];
        if (
          typeof stat.intervalSeconds != "number" ||
          typeof stat.startDateMs != "number" ||
          typeof stat.count != "number" ||
          typeof stat.maxCount != "number" ||
          typeof stat.impressionDateMs != "number"
        ) {
          stats.splice(i, 1);
        } else {
          maxCountInStats = Math.max(maxCountInStats, stat.count);
          let maxCount = maxCapCounts.get(stat.intervalSeconds);
          if (maxCount === undefined) {
            stats.splice(i, 1);
            orphanStats.push(stat);
          } else {
            stat.maxCount = maxCount;
            i++;
          }
        }
      }

      // Create stats for caps that don't already have corresponding stats.
      for (let [intervalSeconds, maxCount] of maxCapCounts.entries()) {
        if (!stats.some(s => s.intervalSeconds == intervalSeconds)) {
          stats.push({
            maxCount,
            intervalSeconds,
            startDateMs: Date.now(),
            count: 0,
            impressionDateMs: 0,
          });
        }
      }

      // Merge orphaned stats into other ones if possible. For each orphan, if
      // its interval is no bigger than an existing stat's interval, then the
      // orphan's count can contribute to the existing stat's count, so merge
      // the two.
      for (let orphan of orphanStats) {
        for (let stat of stats) {
          if (orphan.intervalSeconds <= stat.intervalSeconds) {
            stat.count = Math.max(stat.count, orphan.count);
            stat.startDateMs = Math.min(stat.startDateMs, orphan.startDateMs);
            stat.impressionDateMs = Math.max(
              stat.impressionDateMs,
              orphan.impressionDateMs
            );
          }
        }
      }

      // If the lifetime stat exists, make its count the max count found above.
      // This is only necessary when the lifetime cap wasn't present before, but
      // it doesn't hurt to always do it.
      let lifetimeStat = stats.find(s => s.intervalSeconds == Infinity);
      if (lifetimeStat) {
        lifetimeStat.count = maxCountInStats;
      }

      // Sort the stats by interval ascending. This isn't necessary except that
      // it guarantees an ordering for tests.
      stats.sort((a, b) => a.intervalSeconds - b.intervalSeconds);
    }

    this.logger.debug(JSON.stringify({ newStats: this._impressionStats }));
  }

  /**
   * Resets the counters of impression stats whose intervals have elapased.
   */
  _resetElapsedImpressionCounters() {
    this.logger.info("Checking for elapsed impression cap intervals");
    this.logger.debug(
      JSON.stringify({
        currentStats: this._impressionStats,
        impression_caps: lazy.UrlbarQuickSuggest.config.impression_caps,
      })
    );

    let now = Date.now();
    for (let [type, stats] of Object.entries(this._impressionStats)) {
      for (let stat of stats) {
        let elapsedMs = now - stat.startDateMs;
        let intervalMs = 1000 * stat.intervalSeconds;
        let elapsedIntervalCount = Math.floor(elapsedMs / intervalMs);
        if (elapsedIntervalCount) {
          // At least one interval period elapsed for the stat, so reset it. We
          // may also need to record a telemetry event for the reset.
          this.logger.info(
            `Resetting impression counter for interval ${stat.intervalSeconds}s`
          );
          this.logger.debug(
            JSON.stringify({ type, stat, elapsedMs, elapsedIntervalCount })
          );

          let newStartDateMs =
            stat.startDateMs + elapsedIntervalCount * intervalMs;

          // Compute the portion of `elapsedIntervalCount` that happened after
          // startup. This will be the interval count we report in the telemetry
          // event. By design we don't report intervals that elapsed while the
          // app wasn't running. For example, if the user stopped using Firefox
          // for a year, we don't want to report a year's worth of intervals.
          //
          // First, compute the count of intervals that elapsed before startup.
          // This is the same arithmetic used above except here it's based on
          // the startup date instead of `now`. Keep in mind that startup may be
          // before the stat's start date. Then subtract that count from
          // `elapsedIntervalCount` to get the portion after startup.
          let startupDateMs = this._getStartupDateMs();
          let elapsedIntervalCountBeforeStartup = Math.floor(
            Math.max(0, startupDateMs - stat.startDateMs) / intervalMs
          );
          let elapsedIntervalCountAfterStartup =
            elapsedIntervalCount - elapsedIntervalCountBeforeStartup;

          if (elapsedIntervalCountAfterStartup) {
            this._recordImpressionCapEvent({
              eventType: "reset",
              suggestionType: type,
              eventDateMs: newStartDateMs,
              eventCount: elapsedIntervalCountAfterStartup,
              stat: {
                ...stat,
                startDateMs:
                  stat.startDateMs +
                  elapsedIntervalCountBeforeStartup * intervalMs,
              },
            });
          }

          // Reset the stat.
          stat.startDateMs = newStartDateMs;
          stat.count = 0;
        }
      }
    }

    this.logger.debug(JSON.stringify({ newStats: this._impressionStats }));
  }

  /**
   * Records an impression cap telemetry event.
   *
   * @param {object} options
   *   Options object
   * @param {"hit" | "reset"} options.eventType
   *   One of: "hit", "reset"
   * @param {string} options.suggestionType
   *   One of: "sponsored", "nonsponsored"
   * @param {object} options.stat
   *   The stats object whose max count was hit or whose counter was reset.
   * @param {number} options.eventCount
   *   The number of intervals that elapsed since the last event.
   * @param {number} options.eventDateMs
   *   The `eventDate` that should be recorded in the event's `extra` object.
   *   We include this in `extra` even though events are timestamped because
   *   "reset" events are batched during periods where the user doesn't perform
   *   any searches and therefore impression counters are not reset.
   */
  _recordImpressionCapEvent({
    eventType,
    suggestionType,
    stat,
    eventCount = 1,
    eventDateMs = Date.now(),
  }) {
    // All `extra` object values must be strings.
    let extra = {
      type: suggestionType,
      eventDate: String(eventDateMs),
      eventCount: String(eventCount),
    };
    for (let [statKey, value] of Object.entries(stat)) {
      let extraKey = TELEMETRY_IMPRESSION_CAP_EXTRA_KEYS[statKey];
      if (!extraKey) {
        throw new Error("Unrecognized stats object key: " + statKey);
      }
      extra[extraKey] = String(value);
    }
    Services.telemetry.recordEvent(
      TELEMETRY_EVENT_CATEGORY,
      "impression_cap",
      eventType,
      "",
      extra
    );
  }

  /**
   * Creates a repeating timer that resets impression counters and records
   * related telemetry. Since counters are also reset when suggestions are
   * triggered, the only point of this is to make sure we record reset telemetry
   * events in a timely manner during periods when suggestions aren't triggered.
   *
   * @param {number} ms
   *   The number of milliseconds in the interval.
   */
  _setImpressionCountersResetInterval(
    ms = IMPRESSION_COUNTERS_RESET_INTERVAL_MS
  ) {
    if (this._impressionCountersResetInterval) {
      lazy.clearInterval(this._impressionCountersResetInterval);
    }
    this._impressionCountersResetInterval = lazy.setInterval(
      () => this._resetElapsedImpressionCounters(),
      ms
    );
  }

  /**
   * Gets the timestamp of app startup in ms since Unix epoch. This is only
   * defined as its own method so tests can override it to simulate arbitrary
   * startups.
   *
   * @returns {number}
   *   Startup timestamp in ms since Unix epoch.
   */
  _getStartupDateMs() {
    return Services.startup.getStartupInfo().process.getTime();
  }

  /**
   * Loads blocked suggestion digests from the pref into `_blockedDigests`.
   */
  async _loadBlockedDigests() {
    this.logger.debug(`Queueing _loadBlockedDigests`);
    await this._blockTaskQueue.queue(() => {
      this.logger.info(`Loading blocked suggestion digests`);
      let json = lazy.UrlbarPrefs.get("quicksuggest.blockedDigests");
      this.logger.debug(
        `browser.urlbar.quicksuggest.blockedDigests value: ${json}`
      );
      if (!json) {
        this.logger.info(`There are no blocked suggestion digests`);
        this._blockedDigests.clear();
      } else {
        try {
          this._blockedDigests = new Set(JSON.parse(json));
          this.logger.info(`Successfully loaded blocked suggestion digests`);
        } catch (error) {
          this.logger.error(
            `Error loading blocked suggestion digests: ${error}`
          );
        }
      }
    });
  }

  /**
   * Returns the SHA-1 digest of a string as a 40-character hex-encoded string.
   *
   * @param {string} string
   *   The string to convert to SHA-1
   * @returns {string}
   *   The hex-encoded digest of the given string.
   */
  async _getDigest(string) {
    let stringArray = new TextEncoder().encode(string);
    let hashBuffer = await crypto.subtle.digest("SHA-1", stringArray);
    let hashArray = new Uint8Array(hashBuffer);
    return Array.from(hashArray, b => b.toString(16).padStart(2, "0")).join("");
  }

  /**
   * Updates state based on the `browser.urlbar.quicksuggest.enabled` pref.
   */
  _updateFeatureState() {
    let enabled = lazy.UrlbarPrefs.get("quickSuggestEnabled");
    if (enabled == this._quickSuggestEnabled) {
      // This method is a Nimbus `onUpdate()` callback, which means it's called
      // each time any pref is changed that is a fallback for a Nimbus variable.
      // We have many such prefs. The point of this method is to set up and tear
      // down state when quick suggest's enabled status changes, so ignore
      // updates that do not modify `quickSuggestEnabled`.
      return;
    }

    this._quickSuggestEnabled = enabled;
    this.logger.info("Updating feature state, feature enabled: " + enabled);

    Services.telemetry.setEventRecordingEnabled(
      TELEMETRY_EVENT_CATEGORY,
      enabled
    );
    if (enabled) {
      this._loadImpressionStats();
      this._loadBlockedDigests();
    }
  }

  // The most recently cached value of `UrlbarPrefs.get("quickSuggestEnabled")`.
  // The purpose of this property is only to detect changes in the feature's
  // enabled status. To determine the current status, call
  // `UrlbarPrefs.get("quickSuggestEnabled")` directly instead.
  _quickSuggestEnabled = false;

  // An object that keeps track of impression stats per sponsored and
  // non-sponsored suggestion types. It looks like this:
  //
  //   { sponsored: statsArray, nonsponsored: statsArray }
  //
  // The `statsArray` values are arrays of stats objects, one per impression
  // cap, which look like this:
  //
  //   { intervalSeconds, startDateMs, count, maxCount, impressionDateMs }
  //
  //   {number} intervalSeconds
  //     The number of seconds in the corresponding cap's time interval.
  //   {number} startDateMs
  //     The timestamp at which the current interval period started and the
  //     object's `count` was reset to zero. This is a value returned from
  //     `Date.now()`.  When the current date/time advances past `startDateMs +
  //     1000 * intervalSeconds`, a new interval period will start and `count`
  //     will be reset to zero.
  //   {number} count
  //     The number of impressions during the current interval period.
  //   {number} maxCount
  //     The maximum number of impressions allowed during an interval period.
  //     This value is the same as the `max_count` value in the corresponding
  //     cap. It's stored in the stats object for convenience.
  //   {number} impressionDateMs
  //     The timestamp of the most recent impression, i.e., when `count` was
  //     last incremented.
  //
  // There are two types of impression caps: interval and lifetime. Interval
  // caps are periodically reset, and lifetime caps are never reset. For stats
  // objects corresponding to interval caps, `intervalSeconds` will be the
  // `interval_s` value of the cap. For stats objects corresponding to lifetime
  // caps, `intervalSeconds` will be `Infinity`.
  //
  // `_impressionStats` is kept in sync with impression caps, and there is a
  // one-to-one relationship between stats objects and caps. A stats object's
  // corresponding cap is the one with the same suggestion type (sponsored or
  // non-sponsored) and interval. See `_validateImpressionStats()` for more.
  //
  // Impression caps are stored in the remote settings config. See
  // `UrlbarQuickSuggest.confg.impression_caps`.
  _impressionStats = {};

  // Whether impression stats are currently being updated.
  _updatingImpressionStats = false;

  // Set of digests of the original URLs of blocked suggestions. A suggestion's
  // "original URL" is its URL straight from the source with an unreplaced
  // timestamp template. For details on the digests, see `_getDigest()`.
  //
  // The only reason we use URL digests is that suggestions currently do not
  // have persistent IDs. We could use the URLs themselves but SHA-1 digests are
  // only 40 chars long, so they save a little space. This is also consistent
  // with how blocked tiles on the newtab page are stored, but they use MD5. We
  // do *not* store digests for any security or obfuscation reason.
  //
  // This value is serialized as a JSON'ed array to the
  // `browser.urlbar.quicksuggest.blockedDigests` pref.
  _blockedDigests = new Set();

  // Used to serialize access to blocked suggestions. This is only necessary
  // because getting a suggestion's URL digest is async.
  _blockTaskQueue = new TaskQueue();

  // Whether blocked digests are currently being updated.
  _updatingBlockedDigests = false;
}

export const QuickSuggest = new _QuickSuggest();
