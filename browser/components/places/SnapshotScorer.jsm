/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ["SnapshotScorer"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Snapshots: "resource:///modules/Snapshots.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logConsole", function() {
  return console.createInstance({
    prefix: "SnapshotSelector",
    maxLogLevel: Services.prefs.getBoolPref(
      "browser.snapshots.scorer.log",
      false
    )
      ? "Debug"
      : "Warn",
  });
});

/**
 * The snapshot scorer receives sets of snapshots and scores them based on the
 * expected relevancy to the user. This order is subsequently used to display
 * the candidates.
 */
const SnapshotScorer = new (class SnapshotScorer {
  /**
   * @type {Map}
   *   A map of function suffixes to relevancy points. The suffixes are prefixed
   *   with `_score`. Each function will be called in turn to obtain the score
   *   for that item with the result multiplied by the relevancy points.
   *   This map is filled from the `browser.snapshots.score.` preferences.
   */
  #RELEVANCY_POINTS = new Map();

  /**
   * @type {Date|null}
   *   Used to override the current date for tests.
   */
  #dateOverride = null;

  constructor() {
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "snapshotThreshold",
      "browser.places.snapshots.threshold",
      4
    );

    let branch = Services.prefs.getBranch("browser.snapshots.score.");
    for (let name of branch.getChildList("")) {
      this.#RELEVANCY_POINTS.set(name, branch.getIntPref(name, 0));
    }
  }

  /**
   * Combines groups of snapshots into one group, and scoring their relevance.
   * If snapshots are present in multiple groups, the snapshot with the highest
   * score is used.
   * A snapshot score must meet the `snapshotThreshold` to be included in the
   * results.
   *
   * @param {Set} currentSessionUrls
   *    A set of urls that are in the current session.
   * @param {Snapshot[]} snapshotGroups
   *    One or more arrays of snapshot groups to combine.
   * @returns {Snapshot[]}
   *    The combined snapshot array in descending order of relevancy.
   */
  combineAndScore(currentSessionUrls, ...snapshotGroups) {
    let combined = new Map();
    let currentDate = this.#dateOverride ?? Date.now();
    for (let group of snapshotGroups) {
      for (let snapshot of group) {
        let existing = combined.get(snapshot.url);
        let score = this.#score(snapshot, currentDate, currentSessionUrls);
        logConsole.debug("Scored", score, "for", snapshot.url);
        if (existing) {
          if (score > existing.relevancyScore) {
            snapshot.relevancyScore = score;
            combined.set(snapshot.url, snapshot);
          }
        } else if (score >= this.snapshotThreshold) {
          snapshot.relevancyScore = score;
          combined.set(snapshot.url, snapshot);
        }
      }
    }

    return [...combined.values()].sort(
      (a, b) => b.relevancyScore - a.relevancyScore
    );
  }

  /**
   * Test-only. Overrides the time used in the scoring algorithm with a
   * specific time which allows for deterministic tests.
   *
   * @param {number} date
   *   Epoch time to set the date to.
   */
  overrideCurrentTimeForTests(date) {
    this.#dateOverride = date;
  }

  /**
   * Scores a snapshot based on its relevancy.
   *
   * @param {Snapshot} snapshot
   *   The snapshot to score.
   * @param {number} currentDate
   *   The current time in milliseconds from the epoch.
   * @param {Set} currentSessionUrls
   *   The urls of the current session.
   * @returns {number}
   *   The relevancy score for the snapshot.
   */
  #score(snapshot, currentDate, currentSessionUrls) {
    let points = 0;
    for (let [item, value] of this.#RELEVANCY_POINTS.entries()) {
      let fnName = `_score${item}`;
      if (!(fnName in this)) {
        console.error("Could not find function", fnName, "in SnapshotScorer");
        continue;
      }
      points += this[fnName](snapshot, currentSessionUrls) * value;
    }

    let timeAgo = currentDate - snapshot.lastInteractionAt;
    timeAgo = timeAgo / (24 * 60 * 60 * 1000);

    return points * Math.exp(timeAgo / -7);
  }

  /**
   * Calculates points based on how many times the snapshot has been visited.
   *
   * @param {Snapshot} snapshot
   * @returns {number}
   */
  _scoreVisit(snapshot) {
    // Protect against cases where a bookmark was created without a visit.
    if (snapshot.visitCount == 0) {
      return 0;
    }
    return 2 - 1 / snapshot.visitCount;
  }

  /**
   * Calculates points based on if the snapshot has already been visited in
   * the current session.
   *
   * @param {Snapshot} snapshot
   * @param {Set} currentSessionUrls
   * @returns {number}
   */
  _scoreCurrentSession(snapshot, currentSessionUrls) {
    return currentSessionUrls.has(snapshot.url) ? 1 : 0;
  }

  /**
   * Not currently used.
   *
   * @param {Snapshot} snapshot
   * @returns {number}
   */
  _scoreInNavigation(snapshot) {
    // In Navigation is not currently implemented.
    return 0;
  }

  /**
   * Calculates points based on if the snapshot has been visited within a
   * certain time period of another website.
   *
   * @param {Snapshot} snapshot
   * @returns {number}
   */
  _scoreIsOverlappingVisit(snapshot) {
    return snapshot.overlappingVisitScore ?? 0;
  }

  /**
   * Calculates points based on if the user persisted the snapshot.
   *
   * @param {Snapshot} snapshot
   * @returns {number}
   */
  _scoreIsUserPersisted(snapshot) {
    return snapshot.userPersisted != Snapshots.USER_PERSISTED.NO ? 1 : 0;
  }

  /**
   * Calculates points based on if the user removed the snapshot.
   *
   * @param {Snapshot} snapshot
   * @returns {number}
   */
  _scoreIsUsedRemoved(snapshot) {
    return snapshot.removedAt ? 1 : 0;
  }
})();
