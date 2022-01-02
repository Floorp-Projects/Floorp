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
  await Snapshots.add({ url: TEST_URL1, userPersisted: true });

  await addInteractions([{ url: TEST_URL2, created_at: now }]);
  await Snapshots.add({ url: TEST_URL2, userPersisted: true });
});

add_task(async function test_combining_throw_away_first() {
  let snapshot1a = await Snapshots.get(TEST_URL1);
  let snapshot1b = await Snapshots.get(TEST_URL1);
  let snapshot2 = await Snapshots.get(TEST_URL2);

  // Set up so that 1a will be thrown away.
  snapshot1a.overlappingVisitScore = 0.5;
  snapshot2.overlappingVisitScore = 0.5;
  snapshot1b.overlappingVisitScore = 1.0;

  let combined = SnapshotScorer.combineAndScore(
    new Set([TEST_URL1, TEST_URL2]),
    [snapshot1a],
    [snapshot1b, snapshot2]
  );

  assertSnapshotScores(combined, [
    {
      url: TEST_URL1,
      score: 6,
    },
    {
      url: TEST_URL2,
      score: 4.5,
    },
  ]);
});

add_task(async function test_combining_throw_away_second_and_sort() {
  // We swap the snapshots around a bit here to additionally test the sort.
  let snapshot1 = await Snapshots.get(TEST_URL1);
  let snapshot2a = await Snapshots.get(TEST_URL2);
  let snapshot2b = await Snapshots.get(TEST_URL2);

  snapshot1.overlappingVisitScore = 0.5;
  snapshot2a.overlappingVisitScore = 1.0;
  snapshot2b.overlappingVisitScore = 0.5;

  let combined = SnapshotScorer.combineAndScore(
    new Set([TEST_URL1, TEST_URL2]),
    [snapshot2a],
    [snapshot1, snapshot2b]
  );

  assertSnapshotScores(combined, [
    {
      url: TEST_URL2,
      score: 6,
    },
    {
      url: TEST_URL1,
      score: 4.5,
    },
  ]);
});
