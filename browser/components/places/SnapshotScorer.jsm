/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ["SnapshotScorer"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  Snapshots: "resource:///modules/Snapshots.jsm",
  FilterAdult: "resource://activity-stream/lib/FilterAdult.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "logConsole", function() {
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
 * @typedef {object} Recommendation
 *   A snapshot recommendation with an associated score.
 * @property {Snapshot} snapshot
 *   The recommended snapshot.
 * @property {number} score
 *   The score for this snapshot.
 * @property {string | undefined} source
 *   The source that provided the largest score for this snapshot.
 */

/**
 * @typedef {object} RecommendationGroup
 *   A set of recommendations with an associated weight to apply to their scores.
 * @property {string} source
 *   The source of the group.
 * @property {Recommendation[]} recommendations
 *   The recommended snapshot.
 * @property {number} weight
 *   The weight for the scores in these recommendations.
 */

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
   * Takes a list of recommendations with source specific scores and
   * de-duplicates and generates a final scored list of recommendations.
   * If recommendations are present from multiple sources the scores from each
   * source are added.
   * A recommendations's final score must meet the `snapshotThreshold` to be
   * included in the results.
   *
   * @param {SelectionContext} selectionContext
   *    The selection context to score against. See SnapshotSelector.#context.
   * @param {RecommendationGroup[]} recommendationGroups
   *    A list of recommendations with a weight to apply.
   * @returns {Recommendation[]}
   *    The combined snapshot recommendations in descending order of score.
   */
  combineAndScore(selectionContext, ...recommendationGroups) {
    /**
     * @typedef {object} SnapshotScore
     *   A currently known score for a snapshot.
     * @property {Snapshot} snapshot
     *   The snapshot.
     * @property {number} snapshotScore
     *   The score generated from this snapshot.
     * @property {Map<string, number>} sourceScore
     *   The score from the sources of the recommendation.
     */

    /**
     * Maintains the current score for each seen snapshot.
     * @type {Map<string, SnapshotScore>}
     */
    let combined = new Map();

    let currentDate = this.#dateOverride ?? Date.now();
    let currentSessionUrls = selectionContext.getCurrentSessionUrls();

    for (let { source, recommendations, weight } of recommendationGroups) {
      for (let { snapshot, score } of recommendations) {
        if (
          selectionContext.filterAdult &&
          lazy.FilterAdult.isAdultUrl(snapshot.url)
        ) {
          continue;
        }

        let currentScore = combined.get(snapshot.url);
        if (currentScore) {
          // We've already generated the snapshot specific score, update the
          // source specific scores.
          currentScore.sourceScore.set(source, score * weight);
        } else {
          currentScore = {
            snapshot,
            snapshotScore: this.#score(
              snapshot,
              currentDate,
              currentSessionUrls
            ),
            sourceScore: new Map([[source, score * weight]]),
          };

          combined.set(snapshot.url, currentScore);
        }
      }
    }

    let recommendations = [];
    for (let currentScore of combined.values()) {
      let recommendation = {
        snapshot: currentScore.snapshot,
        score: currentScore.snapshotScore,
        source: undefined,
      };

      // Add up all the source scores and identify the largest.
      let maxScore = null;
      let source = null;
      for (let [id, score] of currentScore.sourceScore) {
        recommendation.score += score;

        if (maxScore === null || maxScore < score) {
          maxScore = score;
          source = id;
        }
      }

      recommendation.source = source;

      lazy.logConsole.debug(
        `Scored ${recommendation.score} for ${recommendation.snapshot.url} from source ${recommendation.source}`
      );

      if (recommendation.score >= this.snapshotThreshold) {
        recommendations.push(recommendation);
      }
    }

    return this.dedupeSnapshots(recommendations).sort(
      (a, b) => b.score - a.score
    );
  }

  /**
   * De-dupes snapshots based on matching titles and query parameters.
   *
   * @param {Recommendation[]} recommendations
   *   An array of snapshots recommendations to de-dupe.
   * @returns {Recommendation[]}
   *   A deduped array.
   */
  dedupeSnapshots(recommendations) {
    // A map that uses the url plus the title as the key. The values are
    // objects with a hasSearch property, and an optional score property.
    let matchingMap = new Map();
    let result = [];

    // First build a map of urls and titles mapping to snapshots data.
    for (let recommendation of recommendations) {
      let url;
      try {
        url = new URL(recommendation.snapshot.url);
      } catch (ex) {
        // If we can't analyse the URL, we simply add the snapshot to the
        // results regardless.
        result.push(recommendation);
        continue;
      }
      let newRecommendation = { ...recommendation, hasSearch: !!url.search };
      url.search = recommendation.snapshot.title;
      let key = url.href;
      let existing = matchingMap.get(key);
      if (existing) {
        // If the new recommendation doesn't have a search query, then
        // set that as the preferred option.
        if (!newRecommendation.hasSearch) {
          matchingMap.set(key, newRecommendation);
          continue;
        }

        // If the existing match does not have a search query, simply continue
        // as it is the preferred option.
        if (!existing.hasSearch) {
          continue;
        }

        // If we have scores, select the best one from the highest score.
        if ("score" in newRecommendation) {
          if (newRecommendation.score > existing.score) {
            matchingMap.set(key, newRecommendation);
          }
          continue;
        }

        // Otherwise, work out the best one to pick based on the most recently
        // visited.
        if (
          newRecommendation.snapshot.lastInteractionAt.getTime() >
          existing.snapshot.lastInteractionAt.getTime()
        ) {
          matchingMap.set(key, newRecommendation);
        }
      } else {
        matchingMap.set(key, newRecommendation);
      }
    }

    return [...result, ...matchingMap.values()];
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
   * Calculates points based on if the user persisted the snapshot.
   *
   * @param {Snapshot} snapshot
   * @returns {number}
   */
  _scoreIsUserPersisted(snapshot) {
    return snapshot.userPersisted != lazy.Snapshots.USER_PERSISTED.NO ? 1 : 0;
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
