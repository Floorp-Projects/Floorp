/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BaseFeature } from "resource:///modules/urlbar/private/BaseFeature.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.sys.mjs",
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  clearInterval: "resource://gre/modules/Timer.sys.mjs",
  setInterval: "resource://gre/modules/Timer.sys.mjs",
});

const IMPRESSION_COUNTERS_RESET_INTERVAL_MS = 60 * 60 * 1000; // 1 hour

// This object maps impression stats object keys to their corresponding keys in
// the `extra` object of impression cap telemetry events. The main reason this
// is necessary is because the keys of the `extra` object are limited to 15
// characters in length, which some stats object keys exceed. It also forces us
// to be deliberate about keys we add to the `extra` object, since the `extra`
// object is limited to 10 keys.
const TELEMETRY_IMPRESSION_CAP_EXTRA_KEYS = {
  // stats object key -> `extra` telemetry event object key
  intervalSeconds: "intervalSeconds",
  startDateMs: "startDate",
  count: "count",
  maxCount: "maxCount",
  impressionDateMs: "impressionDate",
};

/**
 * Impression caps and stats for quick suggest suggestions.
 */
export class ImpressionCaps extends BaseFeature {
  constructor() {
    super();
    lazy.UrlbarPrefs.addObserver(this);
  }

  get shouldEnable() {
    return (
      lazy.UrlbarPrefs.get("quickSuggestImpressionCapsSponsoredEnabled") ||
      lazy.UrlbarPrefs.get("quickSuggestImpressionCapsNonSponsoredEnabled")
    );
  }

  enable(enabled) {
    if (enabled) {
      this.#init();
    } else {
      this.#uninit();
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
  updateStats(type) {
    this.logger.info("Starting impression stats update");
    this.logger.debug(
      JSON.stringify({
        type,
        currentStats: this.#stats,
        impression_caps: lazy.QuickSuggest.jsBackend.config.impression_caps,
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
    let stats = this.#stats[type];
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
        this.#recordCapEvent({
          stat,
          eventType: "hit",
          suggestionType: type,
        });
      }
    }

    // Save the stats.
    this.#updatingStats = true;
    try {
      lazy.UrlbarPrefs.set(
        "quicksuggest.impressionCaps.stats",
        JSON.stringify(this.#stats)
      );
    } finally {
      this.#updatingStats = false;
    }

    this.logger.info("Finished impression stats update");
    this.logger.debug(JSON.stringify({ newStats: this.#stats }));
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
  getHitStats(type) {
    this.#resetElapsedCounters();
    let stats = this.#stats[type];
    if (stats) {
      let hitStats = stats.filter(s => s.maxCount <= s.count);
      if (hitStats.length) {
        return hitStats;
      }
    }
    return null;
  }

  /**
   * Called when a urlbar pref changes.
   *
   * @param {string} pref
   *   The name of the pref relative to `browser.urlbar`.
   */
  onPrefChanged(pref) {
    switch (pref) {
      case "quicksuggest.impressionCaps.stats":
        if (!this.#updatingStats) {
          this.logger.info(
            "browser.urlbar.quicksuggest.impressionCaps.stats changed"
          );
          this.#loadStats();
        }
        break;
    }
  }

  #init() {
    this.#loadStats();

    // Validate stats against any changes to the impression caps in the config.
    this._onConfigSet = () => this.#validateStats();
    lazy.QuickSuggest.jsBackend.emitter.on("config-set", this._onConfigSet);

    // Periodically record impression counters reset telemetry.
    this.#setCountersResetInterval();

    // On shutdown, record any final impression counters reset telemetry.
    this._shutdownBlocker = () => this.#resetElapsedCounters();
    lazy.AsyncShutdown.profileChangeTeardown.addBlocker(
      "QuickSuggest: Record impression counters reset telemetry",
      this._shutdownBlocker
    );
  }

  #uninit() {
    lazy.QuickSuggest.jsBackend.emitter.off("config-set", this._onConfigSet);
    this._onConfigSet = null;

    lazy.clearInterval(this._impressionCountersResetInterval);
    this._impressionCountersResetInterval = 0;

    lazy.AsyncShutdown.profileChangeTeardown.removeBlocker(
      this._shutdownBlocker
    );
    this._shutdownBlocker = null;
  }

  /**
   * Loads and validates impression stats.
   */
  #loadStats() {
    let json = lazy.UrlbarPrefs.get("quicksuggest.impressionCaps.stats");
    if (!json) {
      this.#stats = {};
    } else {
      try {
        this.#stats = JSON.parse(
          json,
          // Infinity, which is the `intervalSeconds` for the lifetime cap, is
          // stringified as `null` in the JSON, so convert it back to Infinity.
          (key, value) =>
            key == "intervalSeconds" && value === null ? Infinity : value
        );
      } catch (error) {}
    }
    this.#validateStats();
  }

  /**
   * Validates impression stats, which includes two things:
   *
   * - Type checks stats and discards any that are invalid. We do this because
   *   stats are stored in prefs where anyone can modify them.
   * - Syncs stats with impression caps so that there is one stats object
   *   corresponding to each impression cap. See the `#stats` comment for info.
   */
  #validateStats() {
    let { impression_caps } = lazy.QuickSuggest.jsBackend.config;

    this.logger.info("Validating impression stats");
    this.logger.debug(
      JSON.stringify({
        impression_caps,
        currentStats: this.#stats,
      })
    );

    if (!this.#stats || typeof this.#stats != "object") {
      this.#stats = {};
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

      let stats = this.#stats[type];
      if (!Array.isArray(stats)) {
        stats = [];
        this.#stats[type] = stats;
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

    this.logger.debug(JSON.stringify({ newStats: this.#stats }));
  }

  /**
   * Resets the counters of impression stats whose intervals have elapased.
   */
  #resetElapsedCounters() {
    this.logger.info("Checking for elapsed impression cap intervals");
    this.logger.debug(
      JSON.stringify({
        currentStats: this.#stats,
        impression_caps: lazy.QuickSuggest.jsBackend.config.impression_caps,
      })
    );

    let now = Date.now();
    for (let [type, stats] of Object.entries(this.#stats)) {
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
            this.#recordCapEvent({
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

    this.logger.debug(JSON.stringify({ newStats: this.#stats }));
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
  #recordCapEvent({
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
      lazy.QuickSuggest.TELEMETRY_EVENT_CATEGORY,
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
  #setCountersResetInterval(ms = IMPRESSION_COUNTERS_RESET_INTERVAL_MS) {
    if (this._impressionCountersResetInterval) {
      lazy.clearInterval(this._impressionCountersResetInterval);
    }
    this._impressionCountersResetInterval = lazy.setInterval(
      () => this.#resetElapsedCounters(),
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

  get _test_stats() {
    return this.#stats;
  }

  _test_reloadStats() {
    this.#stats = null;
    this.#loadStats();
  }

  _test_resetElapsedCounters() {
    this.#resetElapsedCounters();
  }

  _test_setCountersResetInterval(ms) {
    this.#setCountersResetInterval(ms);
  }

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
  // `#stats` is kept in sync with impression caps, and there is a one-to-one
  // relationship between stats objects and caps. A stats object's corresponding
  // cap is the one with the same suggestion type (sponsored or non-sponsored)
  // and interval. See `#validateStats()` for more.
  //
  // Impression caps are stored in the remote settings config. See
  // `SuggestBackendJs.config.impression_caps`.
  #stats = {};

  // Whether impression stats are currently being updated.
  #updatingStats = false;
}
