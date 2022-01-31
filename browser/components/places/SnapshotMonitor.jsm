/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ["SnapshotMonitor"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  clearTimeout: "resource://gre/modules/Timer.jsm",
  Services: "resource://gre/modules/Services.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
});

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "SNAPSHOT_ADDED_TIMER_DELAY",
  "browser.places.snapshot.monitorDelayAdded",
  5000
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "SNAPSHOT_REMOVED_TIMER_DELAY",
  "browser.places.snapshot.monitorDelayRemoved",
  1000
);

/**
 * Monitors changes in snapshots (additions, deletions, etc) and triggers
 * the snapshot group builders to run as necessary.
 */
const SnapshotMonitor = new (class SnapshotMonitor {
  /**
   * @type {number}
   */
  #addedTimerDelay = SNAPSHOT_ADDED_TIMER_DELAY;
  /**
   * @type {number}
   */
  #removedTimerDelay = SNAPSHOT_REMOVED_TIMER_DELAY;

  /**
   * The set of urls that have been added since the last builder update.
   *
   * @type {Set<string>}
   */
  #addedUrls = new Set();

  /**
   * The set of urls that have been removed since the last builder update.
   *
   * @type {Set<string>}
   */
  #removedUrls = new Set();

  /**
   * The current timer, if any, which triggers the next builder update.
   *
   * @type {number}
   */
  #timer = null;

  /**
   * The time the current timer is targetted to. Used to work out if the timer
   * needs respositioning.
   *
   * @type {Set<string>}
   */
  #currentTargetTime = null;

  /**
   * Test-only. Used to specify one or more builders to use instead of the
   * built-in group builders.
   *
   * @type {object[]}
   */
  testGroupBuilders = null;

  /**
   * Internal getter to get the builders used.
   *
   * @returns {object[]}
   */
  get #groupBuilders() {
    if (this.testGroupBuilders) {
      return this.testGroupBuilders;
    }
    return [];
  }

  /**
   * Performs initialization to add observers.
   */
  init() {
    // Only enable if interactions are enabled.
    if (
      !Services.prefs.getBoolPref("browser.places.interactions.enabled", false)
    ) {
      return;
    }
    Services.obs.addObserver(this, "places-snapshots-added");
    Services.obs.addObserver(this, "places-snapshots-deleted");
    Services.obs.addObserver(this, "idle-daily");
  }

  /**
   * Test-only function used to override the delay values to provide shorter
   * delays for tests.
   */
  setTimerDelaysForTests({
    added = SNAPSHOT_ADDED_TIMER_DELAY,
    removed = SNAPSHOT_REMOVED_TIMER_DELAY,
  } = {}) {
    this.#addedTimerDelay = added;
    this.#removedTimerDelay = removed;
  }

  /**
   * Triggers the individual group builders to either rebuild or update
   * their groups.
   *
   * @param {boolean} rebuild
   *   Set to true to force a full rebuild of the groups.
   */
  async #triggerBuilders(rebuild = false) {
    if (this.#timer) {
      clearTimeout(this.#timer);
    }
    this.#timer = null;
    this.#currentTargetTime = null;

    if (rebuild) {
      for (let builder of this.#groupBuilders) {
        await builder.rebuild();
      }
    } else {
      for (let builder of this.#groupBuilders) {
        await builder.update({
          addedUrls: this.#addedUrls,
          removedUrls: this.#removedUrls,
        });
      }
    }

    this.#addedUrls.clear();
    this.#removedUrls.clear();
  }

  /**
   * Sets a timer ensuring that if the new timeout would occur sooner than the
   * current target time, the timer is changed to the sooner time.
   *
   * @param {number} timeout
   *   The timeout in milliseconds to use.
   */
  #setTimer(timeout) {
    let targetTime = Date.now() + timeout;
    if (this.#currentTargetTime && targetTime >= this.#currentTargetTime) {
      // If the new time is in the future, there's no need to reset the timer.
      return;
    }

    if (this.#timer) {
      clearTimeout(this.#timer);
    }

    this.#currentTargetTime = targetTime;
    this.#timer = setTimeout(
      () => this.#triggerBuilders().catch(console.error),
      timeout
    );
  }

  observe(subject, topic, data) {
    if (topic == "places-snapshots-added") {
      this.#onSnapshotAdded(JSON.parse(data));
    } else if (topic == "places-snapshots-deleted") {
      this.#onSnapshotRemoved(JSON.parse(data));
    } else if (topic == "idle-daily") {
      this.#triggerBuilders(true);
    }
  }

  /**
   * Handles snapshots being added - adds to the internal list and sets the
   * timer.
   *
   * @param {string[]} urls
   *   An array of snapshot urls that have been added.
   */
  #onSnapshotAdded(urls) {
    for (let url of urls) {
      this.#addedUrls.add(url);
    }
    this.#setTimer(this.#addedTimerDelay);
  }

  /**
   * Handles snapshots being removed - adds to the internal list and sets the
   * timer.
   *
   * @param {string[]} urls
   *   An array of snapshot urls that have been removed.
   */
  #onSnapshotRemoved(urls) {
    for (let url of urls) {
      this.#removedUrls.add(url);
    }
    this.#setTimer(this.#removedTimerDelay);
  }
})();
