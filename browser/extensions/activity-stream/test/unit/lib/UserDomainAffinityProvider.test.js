import {GlobalOverrider} from "test/unit/utils";
import injector from "inject!lib/UserDomainAffinityProvider.jsm";

const TIME_SEGMENTS = [
  {"id": "hour", "startTime": 3600, "endTime": 0, "weightPosition": 1},
  {"id": "day", "startTime": 86400, "endTime": 3600, "weightPosition": 0.75},
  {"id": "week", "startTime": 604800, "endTime": 86400, "weightPosition": 0.5},
  {"id": "weekPlus", "startTime": null, "endTime": 604800, "weightPosition": 0.25}
];

const PARAMETER_SETS = {
  "paramSet1": {
    "recencyFactor": 0.5,
    "frequencyFactor": 0.5,
    "combinedDomainFactor": 0.5,
    "perfectFrequencyVisits": 10,
    "perfectCombinedDomainScore": 2,
    "multiDomainBoost": 0.1,
    "itemScoreFactor": 0
  },
  "paramSet2": {
    "recencyFactor": 1,
    "frequencyFactor": 0.7,
    "combinedDomainFactor": 0.8,
    "perfectFrequencyVisits": 10,
    "perfectCombinedDomainScore": 2,
    "multiDomainBoost": 0.1,
    "itemScoreFactor": 0
  }
};

describe("User Domain Affinity Provider", () => {
  let UserDomainAffinityProvider;
  let instance;
  let globals;

  beforeEach(() => {
    globals = new GlobalOverrider();
    globals.set("Services", {io: {newURI: u => ({host: "www.somedomain.org"})}});
    globals.set("PlacesUtils", {
      history: {
        getNewQuery: () => ({"TIME_RELATIVE_NOW": 1}),
        getNewQueryOptions: () => ({}),
        executeQuery: () => ({root: {childCount: 1, getChild: index => ({uri: "www.somedomain.org", accessCount: 1})}})
      }
    });
    global.Components.classes["@mozilla.org/browser/nav-history-service;1"] = {
      getService() {
        return global.PlacesUtils.history;
      }
    };

    ({UserDomainAffinityProvider} = injector());
    instance = new UserDomainAffinityProvider(TIME_SEGMENTS, PARAMETER_SETS);
  });
  afterEach(() => {
    globals.restore();
  });
  describe("#init", () => {
    function calculateScore(visitCounts, timeSeg, domain, ps) {
      const vc = visitCounts[timeSeg][domain];
      const score = instance.calculateScore(vc * Number(ps.timeSegmentWeights[timeSeg]), ps.perfectFrequencyVisits, ps.frequencyFactor);
      return Math.min(1, score);
    }
    it("should create a UserDomainAffinityProvider", () => {
      assert.instanceOf(instance, UserDomainAffinityProvider);
    });
    it("should calculate time segment weights for parameter sets", () => {
      const expectedParamSets = Object.assign({}, PARAMETER_SETS);

      // Verify that parameter set specific recencyFactor was applied
      expectedParamSets.paramSet1.timeSegmentWeights = {"hour": 1, "day": 0.75, "week": 0.5, "weekPlus": 0.25};
      expectedParamSets.paramSet2.timeSegmentWeights = {"hour": 1, "day": 1, "week": 1, "weekPlus": 1};
      assert.deepEqual(expectedParamSets, instance.parameterSets);
    });
    it("should calculate user domain affinity scores", () => {
      const ps1 = instance.parameterSets.paramSet1;
      const ps2 = instance.parameterSets.paramSet2;

      const visitCounts = {
        "hour": {"a.com": 1, "b.com": 2},
        "day": {"a.com": 4},
        "week": {"c.com": 1},
        "weekPlus": {"a.com": 1, "d.com": 3}
      };
      instance.queryVisits = ts => visitCounts[ts.id];

      const expScoreAHourPs1 = calculateScore(visitCounts, "hour", "a.com", ps1);
      const expScoreBHourPs1 = calculateScore(visitCounts, "hour", "b.com", ps1);
      const expScoreAHourPs2 = calculateScore(visitCounts, "hour", "a.com", ps2);
      const expScoreBHourPs2 = calculateScore(visitCounts, "hour", "b.com", ps2);
      const expScoreADayPs1 = calculateScore(visitCounts, "day", "a.com", ps1);
      const expScoreADayPs2 = calculateScore(visitCounts, "day", "a.com", ps2);
      const expScoreCWeekPs1 = calculateScore(visitCounts, "week", "c.com", ps1);
      const expScoreCWeekPs2 = calculateScore(visitCounts, "week", "c.com", ps2);
      const expScoreAWeekPlusPs1 = calculateScore(visitCounts, "weekPlus", "a.com", ps1);
      const expScoreAWeekPlusPs2 = calculateScore(visitCounts, "weekPlus", "a.com", ps2);
      const expScoreDWeekPlusPs1 = calculateScore(visitCounts, "weekPlus", "d.com", ps1);
      const expScoreDWeekPlusPs2 = calculateScore(visitCounts, "weekPlus", "d.com", ps2);
      const expectedScores = {
        "a.com": {
          "paramSet1": Math.min(1, expScoreAHourPs1 + expScoreADayPs1 + expScoreAWeekPlusPs1),
          "paramSet2": Math.min(1, expScoreAHourPs2 + expScoreADayPs2 + expScoreAWeekPlusPs2)
        },
        "b.com": {"paramSet1": expScoreBHourPs1, "paramSet2": expScoreBHourPs2},
        "c.com": {"paramSet1": expScoreCWeekPs1, "paramSet2": expScoreCWeekPs2},
        "d.com": {"paramSet1": expScoreDWeekPlusPs1, "paramSet2": expScoreDWeekPlusPs2}
      };

      const scores = instance.calculateAllUserDomainAffinityScores();
      assert.deepEqual(expectedScores, scores);
    });
    it("should return domain affinities", () => {
      const scores = {
        "a.com": {
          "paramSet1": 1,
          "paramSet2": 0.9
        }
      };
      instance = new UserDomainAffinityProvider(TIME_SEGMENTS, PARAMETER_SETS, 100, "v1", scores);

      const expectedAffinities = {
        "timeSegments": TIME_SEGMENTS,
        "parameterSets": PARAMETER_SETS,
        "maxHistoryQueryResults": 100,
        "scores": scores,
        "version": "v1"
      };
      assert.deepEqual(instance.getAffinities(), expectedAffinities);
    });
  });
  describe("#score", () => {
    it("should calculate item relevance score", () => {
      const ps = instance.parameterSets.paramSet2;

      const visitCounts = {
        "hour": {"a.com": 1, "b.com": 2},
        "day": {"a.com": 4},
        "week": {"c.com": 1},
        "weekPlus": {"a.com": 1, "d.com": 3}
      };
      instance.queryVisits = ts => visitCounts[ts.id];
      instance.scores = instance.calculateAllUserDomainAffinityScores();

      const testItem = {
        "domain_affinities": {"a.com": 1},
        "item_score": 1,
        "parameter_set": "paramSet2"
      };
      const combinedDomainScore = instance.scores["a.com"].paramSet2 * Math.pow(ps.multiDomainBoost + 1, 1);
      const expectedItemScore = instance.calculateScore(combinedDomainScore,
        ps.perfectCombinedDomainScore,
        ps.combinedDomainFactor);

      const itemScore = instance.calculateItemRelevanceScore(testItem);
      assert.equal(expectedItemScore, itemScore);
    });
    it("should calculate relevance score equal to item_score if item has no domain affinities", () => {
      const testItem = {item_score: 0.985};
      const itemScore = instance.calculateItemRelevanceScore(testItem);
      assert.equal(testItem.item_score, itemScore);
    });
    it("should calculate scores with factor", () => {
      assert.equal(1, instance.calculateScore(2, 1, 0.5));
      assert.equal(0.5, instance.calculateScore(0.5, 1, 0.5));
      assert.isBelow(instance.calculateScore(0.5, 1, 0.49), 1);
      assert.isBelow(instance.calculateScore(0.5, 1, 0.51), 1);
    });
  });
});
