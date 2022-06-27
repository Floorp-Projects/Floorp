/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that snapshots are correctly scored. Note this file does not test
 * InNavigation and InTimeWindow.
 */

const TEST_URL1 = "https://example.com/";
const TEST_URL2 = "https://invalid.com/";

add_task(async function setup() {
  let now = Date.now();
  SnapshotScorer.overrideCurrentTimeForTests(now);

  await addInteractions([{ url: TEST_URL1, created_at: now }]);
  await Snapshots.add({
    url: TEST_URL1,
    userPersisted: Snapshots.USER_PERSISTED.MANUAL,
  });

  await addInteractions([{ url: TEST_URL2, created_at: now }]);
  await Snapshots.add({
    url: TEST_URL2,
    userPersisted: Snapshots.USER_PERSISTED.MANUAL,
  });
});

add_task(async function test_combining_throw_away_first() {
  let snapshot1 = await Snapshots.get(TEST_URL1);
  let snapshot2 = await Snapshots.get(TEST_URL2);

  let combined = SnapshotScorer.combineAndScore(
    { getCurrentSessionUrls: () => new Set([TEST_URL1, TEST_URL2]) },
    {
      source: "foo",
      recommendations: [{ snapshot: snapshot1, score: 0.5 }],
      weight: 3.0,
    },
    {
      source: "bar",
      recommendations: [
        { snapshot: snapshot2, score: 0.5 },
        { snapshot: snapshot1, score: 1 },
      ],
      weight: 3.0,
    }
  );

  assertRecommendations(combined, [
    {
      url: TEST_URL1,
      score: 7.5,
      source: "bar",
    },
    {
      url: TEST_URL2,
      score: 4.5,
      source: "bar",
    },
  ]);
});

add_task(async function test_combining_throw_away_second_and_sort() {
  // We swap the snapshots around a bit here to additionally test the sort.
  let snapshot1 = await Snapshots.get(TEST_URL1);
  let snapshot2 = await Snapshots.get(TEST_URL2);

  let combined = SnapshotScorer.combineAndScore(
    { getCurrentSessionUrls: () => new Set([TEST_URL1, TEST_URL2]) },
    {
      recommendations: [{ snapshot: snapshot2, score: 1 }],
      weight: 3.0,
      source: "foo",
    },
    {
      recommendations: [
        { snapshot: snapshot1, score: 0.5 },
        { snapshot: snapshot2, score: 0.5 },
      ],
      weight: 3.0,
      source: "bar",
    }
  );

  assertRecommendations(combined, [
    {
      url: TEST_URL2,
      score: 7.5,
      source: "foo",
    },
    {
      url: TEST_URL1,
      score: 4.5,
      source: "bar",
    },
  ]);
});
