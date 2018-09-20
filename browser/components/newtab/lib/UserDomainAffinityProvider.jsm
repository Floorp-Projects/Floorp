/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");

const DEFAULT_TIME_SEGMENTS = [
  {"id": "hour", "startTime": 3600, "endTime": 0, "weightPosition": 1},
  {"id": "day", "startTime": 86400, "endTime": 3600, "weightPosition": 0.75},
  {"id": "week", "startTime": 604800, "endTime": 86400, "weightPosition": 0.5},
  {"id": "weekPlus", "startTime": 0, "endTime": 604800, "weightPosition": 0.25},
  {"id": "alltime", "startTime": 0, "endTime": 0, "weightPosition": 0.25},
];

const DEFAULT_PARAMETER_SETS = {
  "linear-frequency": {
    "recencyFactor": 0.4,
    "frequencyFactor": 0.5,
    "combinedDomainFactor": 0.5,
    "perfectFrequencyVisits": 10,
    "perfectCombinedDomainScore": 2,
    "multiDomainBoost": 0.1,
    "itemScoreFactor": 0,
  },
};

const DEFAULT_MAX_HISTORY_QUERY_RESULTS = 1000;

function merge(...args) {
  return Object.assign.apply(this, args);
}

/**
 * Provides functionality to personalize content recommendations by calculating
 * user domain affinity scores. These scores are used to calculate relevance
 * scores for items/recs/stories that have domain affinities.
 *
 * The algorithm works as follows:
 *
 * - The recommendation endpoint returns a settings object containing
 * timeSegments and parametersets.
 *
 * - For every time segment we calculate the corresponding domain visit counts,
 * yielding result objects of the following structure: {"mozilla.org": 12,
 * "mozilla.com": 34} (see UserDomainAffinityProvider#queryVisits)
 *
 * - These visit counts are transformed to domain affinity scores for all
 * provided parameter sets: {"mozilla.org": {"paramSet1": 0.8,
 * "paramSet2": 0.9}, "mozilla.org": {"paramSet1": 1, "paramSet2": 0.9}}
 * (see UserDomainAffinityProvider#calculateScoresForParameterSets)
 *
 * - The parameter sets provide factors for weighting which allows for
 * flexible targeting. The functionality to calculate final scores can
 * be seen in UserDomainAffinityProvider#calculateScores
 *
 * - The user domain affinity scores are summed up across all time segments
 * see UserDomainAffinityProvider#calculateAllUserDomainAffinityScores
 *
 * - An item's domain affinities are matched to the user's domain affinity
 * scores by calculating an item relevance score
 * (see UserDomainAffinityProvider#calculateItemRelevanceScore)
 *
 * - The item relevance scores are used to sort items (see TopStoriesFeed for
 * more details)
 *
 * - The data structure was chosen to allow for fast cache lookups during
 * relevance score calculation. While user domain affinities are calculated
 * infrequently (i.e. only once a day), the item relevance score (potentially)
 * needs to be calculated every time the feed updates. Therefore allowing cache
 * lookups of scores[domain][parameterSet] is beneficial
 */
this.UserDomainAffinityProvider = class UserDomainAffinityProvider {
  constructor(
    timeSegments = DEFAULT_TIME_SEGMENTS,
    parameterSets = DEFAULT_PARAMETER_SETS,
    maxHistoryQueryResults = DEFAULT_MAX_HISTORY_QUERY_RESULTS,
    version,
    scores) {
    this.timeSegments = timeSegments;
    this.maxHistoryQueryResults = maxHistoryQueryResults;
    this.version = version;
    if (scores) {
      this.parameterSets = parameterSets;
      this.scores = scores;
    } else {
      this.parameterSets = this.prepareParameterSets(parameterSets);
      this.scores = this.calculateAllUserDomainAffinityScores();
    }
  }

  /**
   * Adds dynamic parameters to the given parameter sets that need to be
   * computed based on time segments.
   *
   * @param ps The parameter sets
   * @return Updated parameter sets with additional fields (i.e. timeSegmentWeights)
   */
  prepareParameterSets(ps) {
    return Object
      .keys(ps)
      // Add timeSegmentWeight fields to param sets e.g. timeSegmentWeights: {"hour": 1, "day": 0.8915, ...}
      .map(k => ({[k]: merge(ps[k], {timeSegmentWeights: this.calculateTimeSegmentWeights(ps[k].recencyFactor)})}))
      .reduce((acc, cur) => merge(acc, cur));
  }

  /**
   * Calculates a time segment weight based on the provided recencyFactor.
   *
   * @param recencyFactor The recency factor indicating how to weigh recency
   * @return An object containing time segment weights: {"hour": 0.987, "day": 1}
   */
  calculateTimeSegmentWeights(recencyFactor) {
    return this.timeSegments
      .reduce((acc, cur) => merge(acc, ({[cur.id]: this.calculateScore(cur.weightPosition, 1, recencyFactor)})), {});
  }

  /**
   * Calculates user domain affinity scores based on browsing history and the
   * available times segments and parameter sets.
   */
  calculateAllUserDomainAffinityScores() {
    return this.timeSegments
      // Calculate parameter set specific domain scores for each time segment
      // => [{"a.com": {"ps1": 12, "ps2": 34}, "b.com": {"ps1": 56, "ps2": 78}}, ...]
      .map(ts => this.calculateUserDomainAffinityScores(ts))
      // Keep format, but reduce to single object, with combined scores across all time segments
      // => "{a.com":{"ps1":2,"ps2":2}, "b.com":{"ps1":3,"ps2":3}}""
      .reduce((acc, cur) => this._combineScores(acc, cur));
  }

  /**
   * Calculates the user domain affinity scores for the given time segment.
   *
   * @param ts The time segment
   * @return The parameter specific scores for all domains with visits in
   * this time segment: {"a.com": {"ps1": 12, "ps2": 34}, "b.com" ...}
   */
  calculateUserDomainAffinityScores(ts) {
    // Returns domains and visit counts for this time segment: {"a.com": 1, "b.com": 2}
    let visits = this.queryVisits(ts);

    return Object
      .keys(visits)
      .reduce((acc, d) => merge(acc, {[d]: this.calculateScoresForParameterSets(ts, visits[d])}), {});
  }

  /**
   * Calculates the scores for all parameter sets for the given time segment
   * and domain visit count.
   *
   * @param ts The time segment
   * @param vc The domain visit count in the given time segment
   * @return The parameter specific scores for the visit count in
   * this time segment: {"ps1": 12, "ps2": 34}
   */
  calculateScoresForParameterSets(ts, vc) {
    return Object
      .keys(this.parameterSets)
      .reduce((acc, ps) => merge(acc, {[ps]: this.calculateScoreForParameterSet(ts, vc, this.parameterSets[ps])}), {});
  }

  /**
   * Calculates the final affinity score in the given time segment for the given parameter set
   *
   * @param timeSegment The time segment
   * @param visitCount The domain visit count in the given time segment
   * @param parameterSet The parameter set to use for scoring
   * @return The final score
   */
  calculateScoreForParameterSet(timeSegment, visitCount, parameterSet) {
    return this.calculateScore(
      visitCount * parameterSet.timeSegmentWeights[timeSegment.id],
      parameterSet.perfectFrequencyVisits,
      parameterSet.frequencyFactor);
  }

  /**
   * Keeps the same format, but reduces the two objects to a single object, with
   * combined scores across all time segments  => {a.com":{"ps1":2,"ps2":2},
   * "b.com":{"ps1":3,"ps2":3}}
   */
  _combineScores(a, b) {
    // Merge both score objects so we get a combined object holding all domains.
    // This is so we can combine them without missing domains that are in a and not in b and vice versa.
    const c = merge({}, a, b);
    return Object.keys(c).reduce((acc, d) => merge(acc, this._combine(a, b, c, d)), {});
  }

  _combine(a, b, c, d) {
    return Object
      .keys(c[d])
      // Summing up the parameter set specific scores of each domain
      .map(ps => ({[d]: {[ps]: Math.min(1, ((a[d] && a[d][ps]) || 0) + ((b[d] && b[d][ps]) || 0))}}))
      // Reducing from an array of objects with a single parameter set to a single object
      // [{"a.com":{"ps1":11}}, {"a.com: {"ps2":12}}] => {"a.com":{"ps1":11,"ps2":12}}
      .reduce((acc, cur) => ({[d]: merge(acc[d], cur[d])}));
  }

  /**
   * Calculates a value on the curve described by the provided parameters. The curve we're using is
   * (a^(b*x) - 1) / (a^b - 1): https://www.desmos.com/calculator/maqhpttupp
   *
   * @param {number} score A value between 0 and maxScore, representing x.
   * @param {number} maxScore Highest possible score.
   * @param {number} factor The slope describing the curve to get to maxScore. A low slope value
   * [0, 0.5] results in a log-shaped curve, a high slope [0.5, 1] results in a exp-shaped curve,
   * a slope of exactly 0.5 is linear.
   * @param {number} ease Adjusts how much bend is in the curve i.e. how dramatic the maximum
   * effect of the slope can be. This represents b in the formula above.
   * @return {number} the final score
   */
  calculateScore(score, maxScore, factor, ease = 2) {
    let a = 0;
    let x = Math.max(0, score / maxScore);

    if (x >= 1) {
      return 1;
    }

    if (factor === 0.5) {
      return x;
    }

    if (factor < 0.5) {
      // We want a log-shaped curve so we scale "a" between 0 and .99
      a = (factor / 0.5) * 0.49;
    } else if (factor > 0.5) {
      // We want an exp-shaped curve so we scale "a" between 1.01 and 10
      a = 1 + (factor - 0.5) / 0.5 * 9;
    }

    return (Math.pow(a, ease * x) - 1) / (Math.pow(a, ease) - 1);
  }

  /**
   * Queries the visit counts in the given time segment.
   *
   * @param ts the time segment
   * @return the visit count object: {"a.com": 1, "b.com": 2}
   */
  queryVisits(ts) {
    const visitCounts = {};
    const query = PlacesUtils.history.getNewQuery();
    const wwwRegEx = /^www\./;

    query.beginTimeReference = query.TIME_RELATIVE_NOW;
    query.beginTime = (ts.startTime && ts.startTime !== 0) ? -(ts.startTime * 1000 * 1000) : -(Date.now() * 1000);

    query.endTimeReference = query.TIME_RELATIVE_NOW;
    query.endTime = (ts.endTime && ts.endTime !== 0) ? -(ts.endTime * 1000 * 1000) : 0;

    const options = PlacesUtils.history.getNewQueryOptions();
    options.sortingMode = options.SORT_BY_VISITCOUNT_DESCENDING;
    options.maxResults = this.maxHistoryQueryResults;

    const {root} = PlacesUtils.history.executeQuery(query, options);
    root.containerOpen = true;
    for (let i = 0; i < root.childCount; i++) {
      let node = root.getChild(i);
      let host = Services.io.newURI(node.uri).host.replace(wwwRegEx, "");
      if (!visitCounts[host]) {
        visitCounts[host] = 0;
      }
      visitCounts[host] += node.accessCount;
    }
    root.containerOpen = false;
    return visitCounts;
  }

  /**
   * Calculates an item's relevance score.
   *
   * @param item the item (story), must contain domain affinities, otherwise a
   * score of 1 is returned.
   * @return the calculated item's score or 1 if item has no domain_affinities
   * or references an unknown parameter set.
   */
  calculateItemRelevanceScore(item) {
    const params = this.parameterSets[item.parameter_set];
    if (!item.domain_affinities || !params) {
      return item.item_score;
    }

    const scores = Object
      .keys(item.domain_affinities)
      .reduce((acc, d) => {
        let userDomainAffinityScore = this.scores[d] ? this.scores[d][item.parameter_set] : false;
        if (userDomainAffinityScore) {
          acc.combinedDomainScore += userDomainAffinityScore * item.domain_affinities[d];
          acc.matchingDomainsCount++;
        }
        return acc;
      }, {combinedDomainScore: 0, matchingDomainsCount: 0});

    // Boost the score as configured in the provided parameter set
    const boostedCombinedDomainScore = scores.combinedDomainScore *
      Math.pow(params.multiDomainBoost + 1, scores.matchingDomainsCount);

    // Calculate what the score would be if the item score is ignored
    const normalizedCombinedDomainScore = this.calculateScore(boostedCombinedDomainScore,
      params.perfectCombinedDomainScore,
      params.combinedDomainFactor);

    // Calculate the final relevance score using the itemScoreFactor. The itemScoreFactor
    // allows weighting the item score in relation to the normalizedCombinedDomainScore:
    // An itemScoreFactor of 1 results in the item score and ignores the combined domain score
    // An itemScoreFactor of 0.5 results in the the average of item score and combined domain score
    // An itemScoreFactor of 0 results in the combined domain score and ignores the item score
    return params.itemScoreFactor * (item.item_score - normalizedCombinedDomainScore) + normalizedCombinedDomainScore;
  }

  /**
   * Returns an object holding the settings and affinity scores of this provider instance.
   */
  getAffinities() {
    return {
      timeSegments: this.timeSegments,
      parameterSets: this.parameterSets,
      maxHistoryQueryResults: this.maxHistoryQueryResults,
      version: this.version,
      scores: this.scores,
    };
  }
};

const EXPORTED_SYMBOLS = ["UserDomainAffinityProvider"];
