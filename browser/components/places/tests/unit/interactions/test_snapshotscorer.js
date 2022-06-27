/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that snapshots are correctly scored.
 */

const SCORE_TESTS = [
  {
    testName: "Basic test",
    lastVisit: 0,
    visitCount: 1,
    currentSession: false,
    sourceScore: 0,
    userPersisted: Snapshots.USER_PERSISTED.NO,
    userRemoved: false,
    score: 1,
  },
  {
    testName: "Last visit test 1",
    lastVisit: 4,
    visitCount: 1,
    currentSession: false,
    sourceScore: 0,
    userPersisted: Snapshots.USER_PERSISTED.MANUAL,
    userRemoved: false,
    score: 1.1294362440155186,
  },
  {
    testName: "Last visit test 2",
    lastVisit: 6,
    visitCount: 1,
    currentSession: false,
    sourceScore: 0,
    userPersisted: Snapshots.USER_PERSISTED.MANUAL,
    userRemoved: false,
    score: 0.8487456913539,
  },
  {
    testName: "Last visit test 3",
    lastVisit: 7,
    visitCount: 1,
    currentSession: false,
    userPersisted: Snapshots.USER_PERSISTED.MANUAL,
    userRemoved: false,
    score: 0.7357588823428847,
  },
  {
    testName: "Large visit count test",
    lastVisit: 0,
    visitCount: 100,
    currentSession: false,
    sourceScore: 0,
    userPersisted: Snapshots.USER_PERSISTED.NO,
    userRemoved: false,
    score: 1.99,
  },
  {
    testName: "Zero visit count test",
    lastVisit: 0,
    visitCount: 0,
    currentSession: false,
    sourceScore: 0,
    userPersisted: Snapshots.USER_PERSISTED.MANUAL,
    userRemoved: false,
    score: 1,
  },
  {
    testName: "In current session test",
    lastVisit: 0,
    visitCount: 1,
    currentSession: true,
    sourceScore: 0,
    userPersisted: Snapshots.USER_PERSISTED.NO,
    userRemoved: false,
    score: 2,
  },
  {
    testName: "Overlapping visit score test 1",
    lastVisit: 0,
    visitCount: 1,
    currentSession: false,
    sourceScore: 1.0,
    userPersisted: Snapshots.USER_PERSISTED.NO,
    userRemoved: false,
    score: 4,
  },
  {
    testName: "Overlapping visit score test 2",
    lastVisit: 0,
    visitCount: 1,
    currentSession: false,
    sourceScore: 0.5,
    userPersisted: Snapshots.USER_PERSISTED.NO,
    userRemoved: false,
    score: 2.5,
  },
  {
    testName: "User persisted test",
    lastVisit: 0,
    visitCount: 1,
    currentSession: false,
    sourceScore: 0,
    userPersisted: Snapshots.USER_PERSISTED.MANUAL,
    userRemoved: false,
    score: 2,
  },
  {
    testName: "User removed test",
    lastVisit: 0,
    visitCount: 1,
    currentSession: false,
    sourceScore: 0,
    userPersisted: Snapshots.USER_PERSISTED.NO,
    userRemoved: true,
    score: -9, // 1 for the visit, -10 for removed
  },
];

// Tests for ensuring the threshold works. Note that these need to be in reverse
// score order.
const THRESHOLD_TESTS = [
  {
    lastVisit: 0,
    visitCount: 100,
    currentSession: true,
    sourceScore: 1.0,
    userPersisted: Snapshots.USER_PERSISTED.MANUAL,
    userRemoved: false,
    score: 6.99,
  },
  {
    lastVisit: 0,
    visitCount: 100,
    currentSession: false,
    sourceScore: 0.35,
    userPersisted: Snapshots.USER_PERSISTED.MANUAL,
    userRemoved: false,
    score: 4.04,
  },
  {
    lastVisit: 0,
    visitCount: 1,
    currentSession: false,
    sourceScore: 0.0,
    userPersisted: Snapshots.USER_PERSISTED.NO,
    userRemoved: false,
    score: 1,
  },
];

add_task(async function setup() {
  let now = Date.now();

  SnapshotScorer.overrideCurrentTimeForTests(now);

  for (let [i, data] of [...SCORE_TESTS, ...THRESHOLD_TESTS].entries()) {
    let createdAt = now - data.lastVisit * 24 * 60 * 60 * 1000;
    let url = `https://example.com/${i}`;
    if (data.visitCount != 0) {
      await addInteractions([{ url, created_at: createdAt }]);
      if (data.visitCount > 2) {
        let urls = new Array(data.visitCount - 1);
        urls.fill(url);
        await PlacesTestUtils.addVisits(urls);
      }
    }
    await Snapshots.add({ url, userPersisted: data.userPersisted });

    if (data.visitCount == 0) {
      // For the last updated time for the snapshot to be "now", so that we can
      // have a fixed value for the score in the test.
      await PlacesUtils.withConnectionWrapper(
        "test_snapshotscorer",
        async db => {
          await db.executeCached(
            `UPDATE moz_places_metadata_snapshots
             SET last_interaction_at = :lastInteractionAt
             WHERE place_id = (SELECT id FROM moz_places WHERE url_hash = hash(:url) AND url = :url)`,
            { lastInteractionAt: now, url }
          );
        }
      );
    }

    if (data.userRemoved) {
      await Snapshots.delete(url);
    }
  }
});

function handleSnapshotSetup(testData, snapshot, sessionUrls) {
  if (testData.currentSession) {
    sessionUrls.add(snapshot.url);
  }
}

add_task(async function test_scores() {
  // Set threshold to -10 as that's below the lowest we can get and we want
  // to test the full score range.
  Services.prefs.setIntPref("browser.places.snapshots.threshold", -10);

  for (let [i, data] of SCORE_TESTS.entries()) {
    info(`${data.testName}`);

    let sessionUrls = new Set([`https://mochitest:8888/`]);

    let url = `https://example.com/${i}`;
    let snapshot = await Snapshots.get(url, true);

    handleSnapshotSetup(data, snapshot, sessionUrls);

    let snapshots = await SnapshotScorer.combineAndScore(
      { getCurrentSessionUrls: () => sessionUrls },
      {
        source: "foo",
        recommendations: [{ snapshot, score: data.sourceScore ?? 0 }],
        weight: 3.0,
      }
    );

    assertRecommendations(snapshots, [
      {
        url,
        source: "foo",
        score: data.score,
      },
    ]);
  }
});

add_task(async function test_score_threshold() {
  const THRESHOLD = 4;
  Services.prefs.setIntPref("browser.places.snapshots.threshold", THRESHOLD);

  let sessionUrls = new Set([`https://mochitest:8888/`]);
  let sourceRecommendations = [];

  for (let i = 0; i < THRESHOLD_TESTS.length; i++) {
    let url = `https://example.com/${i + SCORE_TESTS.length}`;
    let snapshot = await Snapshots.get(url, true);

    handleSnapshotSetup(THRESHOLD_TESTS[i], snapshot, sessionUrls);
    sourceRecommendations.push({
      snapshot,
      score: THRESHOLD_TESTS[i].sourceScore,
    });
  }

  let snapshots = await SnapshotScorer.combineAndScore(
    { getCurrentSessionUrls: () => sessionUrls },
    { source: "bar", recommendations: sourceRecommendations, weight: 3.0 }
  );

  assertRecommendations(
    snapshots,
    // map before filter so that we get the url values correct.
    THRESHOLD_TESTS.map((t, i) => {
      return {
        url: `https://example.com/${i + SCORE_TESTS.length}`,
        source: "bar",
        score: t.score,
      };
    }).filter(t => t.score > THRESHOLD)
  );
});
