/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ["SnapshotMonitor"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  clearTimeout: "resource://gre/modules/Timer.jsm",
  DomainGroupBuilder: "resource:///modules/DomainGroupBuilder.jsm",
  PinnedGroupBuilder: "resource:///modules/PinnedGroupBuilder.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
  Snapshots: "resource:///modules/Snapshots.jsm",
});

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "SNAPSHOT_ADDED_TIMER_DELAY",
  "browser.places.snapshots.monitorDelayAdded",
  5000
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "SNAPSHOT_REMOVED_TIMER_DELAY",
  "browser.places.snapshots.monitorDelayRemoved",
  1000
);

// Expiration days for automatic and user managed snapshots.
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "SNAPSHOT_EXPIRE_DAYS",
  "browser.places.snapshots.expiration.days",
  210
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "SNAPSHOT_USERMANAGED_EXPIRE_DAYS",
  "browser.places.snapshots.expiration.userManaged.days",
  420
);
// We expire on the next idle after a snapshot was added or removed, and
// idle-daily, but we don't want to expire too often or rarely.
// Thus we define both a mininum and maximum time in the session among which
// we'll expire chunks of snapshots.
const EXPIRE_EVERY_MIN_MS = 60 * 60000; // 1 Hour.
const EXPIRE_EVERY_MAX_MS = 120 * 60000; // 2 Hours.
// The number of snapshots to expire at once.
const EXPIRE_CHUNK_SIZE = 10;

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
  #addedItems = new Set();

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
   * The time of the last snapshots expiration.
   */
  #lastExpirationTime = 0;
  /**
   * How many snapshots to expire per chunk.
   */
  #expirationChunkSize = EXPIRE_CHUNK_SIZE;

  /**
   * Internal getter to get the builders used.
   *
   * @returns {object[]}
   */
  get #groupBuilders() {
    if (this.testGroupBuilders) {
      return this.testGroupBuilders;
    }
    return [DomainGroupBuilder, PinnedGroupBuilder];
  }

  /**
   * Returns a list of builder names which have a single group which is always
   * displayed regardless of the minimum snapshot count for snapshot groups.
   *
   * @returns {string[]}
   */
  get skipMinimumSizeBuilders() {
    let names = [];
    for (let builder of this.#groupBuilders) {
      let name = builder.skipMinimumSize;
      if (name) {
        names.push(builder.name);
      }
    }
    return names;
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
      let snapshots = await Snapshots.query({ limit: -1 });
      for (let builder of this.#groupBuilders) {
        await builder.rebuild(snapshots);
      }
    } else {
      for (let builder of this.#groupBuilders) {
        await builder.update({
          addedItems: this.#addedItems,
          removedUrls: this.#removedUrls,
        });
      }
    }

    this.#addedItems.clear();
    this.#removedUrls.clear();
  }

  /**
   * Triggers expiration of a chunk of snapshots.
   * We differentiate snapshots depending on whether they are user managed:
   *  1. manually created by the user
   *  2. part of a group
   *     TODO: evaluate whether we want to consider user managed only snapshots
   *           that are part of a user curated group, rather than any group.
   * User managed snapshots will expire if their last interaction is older than
   * browser.snapshots.expiration.userManaged.days, while others will expire
   * after browser.snapshots.expiration.days.
   * Snapshots that have a tombstone (removed_at is set) should not be expired.
   *
   * @param {boolean} onIdle
   *   Whether this is running on idle. When it's false expiration is
   *   rescheduled for the next idle.
   */
  async #expireSnapshotsChunk(onIdle = false) {
    let now = Date.now();
    if (now - this.#lastExpirationTime < EXPIRE_EVERY_MIN_MS) {
      return;
    }
    let instance = (this._expireInstance = {});
    let skip = false;
    if (!onIdle) {
      // Wait for the next idle.
      skip = await new Promise(resolve =>
        ChromeUtils.idleDispatch(deadLine => {
          // Skip if we couldn't find an idle, unless we're over max waiting time.
          resolve(
            deadLine.didTimeout &&
              now - this.#lastExpirationTime < EXPIRE_EVERY_MAX_MS
          );
        })
      );
    }
    if (skip || instance != this._expireInstance) {
      return;
    }

    this.#lastExpirationTime = now;
    let urls = (
      await Snapshots.query({
        includeUserPersisted: false,
        includeTombstones: false,
        includeSnapshotsInUserManagedGroups: false,
        lastInteractionBefore: now - SNAPSHOT_EXPIRE_DAYS * 86400000,
        limit: this.#expirationChunkSize,
      })
    ).map(s => s.url);
    if (instance != this._expireInstance) {
      return;
    }

    if (urls.length < this.#expirationChunkSize) {
      // If we couldn't find enough automatic snapshots, check if there's any
      // user managed ones we can expire.
      urls.push(
        ...(
          await Snapshots.query({
            includeUserPersisted: true,
            includeTombstones: false,
            lastInteractionBefore:
              now - SNAPSHOT_USERMANAGED_EXPIRE_DAYS * 86400000,
            limit: this.#expirationChunkSize - urls.length,
          })
        ).map(s => s.url)
      );
    }
    if (instance != this._expireInstance) {
      return;
    }

    await Snapshots.delete(
      [...new Set(urls)],
      Snapshots.REMOVED_REASON.EXPIRED
    );
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
    this.#timer = setTimeout(() => {
      this.#expireSnapshotsChunk().catch(console.error);
      this.#triggerBuilders().catch(console.error);
    }, timeout);
  }

  /**
   * observe function for nsIObserver. This is async so that we can call it in
   * tests and know that the triggerBuilders for idle-daily has finished.
   *
   * @param {object} subject
   * @param {string} topic
   * @param {nsISupports} data
   */
  async observe(subject, topic, data) {
    switch (topic) {
      case "places-snapshots-added":
        this.#onSnapshotAdded(JSON.parse(data));
        break;
      case "places-snapshots-deleted":
        this.#onSnapshotRemoved(JSON.parse(data));
        break;
      case "idle-daily":
        await this.#expireSnapshotsChunk(true);
        await this.#triggerBuilders(true);
        break;
      case "test-trigger-builders":
        await this.#triggerBuilders(true);
        break;
      case "test-expiration":
        this.#lastExpirationTime =
          subject.lastExpirationTime || this.#lastExpirationTime;
        this.#expirationChunkSize =
          subject.expirationChunkSize || this.#expirationChunkSize;
        await this.#expireSnapshotsChunk(subject.onIdle);
        break;
    }
  }

  /**
   * Handles snapshots being added - adds to the internal list and sets the
   * timer.
   *
   * @param {object[]} items
   *   An array of items that have been added.
   * @param {string} items.url
   *   The url of the item.
   * @param {number} items.userPersisted
   *   The userPersisted state of the item.
   */
  #onSnapshotAdded(items) {
    for (let item of items) {
      this.#addedItems.add(item);
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
