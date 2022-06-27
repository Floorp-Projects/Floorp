/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that the selectOverlappingVisits param only selects overlapping snapshots
 */

const TEST_URL1 = "https://example.com/";
const TEST_URL2 = "https://example.com/2";
const TEST_URL3 = "https://example.com/3";
const TEST_URL4 = "https://example.com/4";

let selector;
let currentSessionUrls = new Set();

function getCurrentSessionUrls() {
  return currentSessionUrls;
}

add_task(async function test_setup() {
  const ONE_MINUTE = 1000 * 60;
  const ONE_HOUR = ONE_MINUTE * 60;

  let now = Date.now();
  await addInteractions([
    {
      url: TEST_URL1,
      created_at: now - ONE_MINUTE,
      updated_at: now + ONE_MINUTE,
    },
    {
      url: TEST_URL2,
      created_at: now - ONE_MINUTE * 2,
      updated_at: now + ONE_MINUTE * 2,
    },
    { url: TEST_URL3, created_at: now + ONE_HOUR, updated_at: now + ONE_HOUR },
    { url: TEST_URL4, created_at: now - ONE_HOUR, updated_at: now - ONE_HOUR },
  ]);

  selector = new SnapshotSelector({
    count: 5,
    filterAdult: false,
    sourceWeights: {
      CommonReferrer: 0,
      Overlapping: 3,
    },
    getCurrentSessionUrls,
  });
});

add_task(async function test_enable_overlapping() {
  // Allow all snapshots regardless of their score.
  Services.prefs.setIntPref("browser.places.snapshots.threshold", -10);

  let snapshotPromise = selector.once("snapshots-updated");
  selector.rebuild();
  let snapshots = await snapshotPromise;

  await assertSnapshotList(snapshots, []);

  snapshotPromise = selector.once("snapshots-updated");
  await Snapshots.add({ url: TEST_URL1 });
  await Snapshots.add({ url: TEST_URL2 });
  await Snapshots.add({ url: TEST_URL3 });
  await Snapshots.add({ url: TEST_URL4 });

  selector.updateDetailsAndRebuild({ url: TEST_URL1 });
  snapshots = await snapshotPromise;

  // Only snapshots with overlapping interactions should be selected
  await assertSnapshotList(snapshots, [
    { url: TEST_URL2, source: "Overlapping" },
  ]);
});

add_task(async function test_overlapping_with_scoring() {
  // Reset the threshold, the snapshot should be lower than the required score.
  Services.prefs.clearUserPref("browser.places.snapshots.threshold");

  let snapshotPromise = selector.once("snapshots-updated");
  selector.rebuild();
  let snapshots = await snapshotPromise;

  await assertSnapshotList(snapshots, []);

  // Boost the score of the expected snapshot by adding it to the current url
  // set.
  currentSessionUrls.add(TEST_URL2);

  snapshotPromise = selector.once("snapshots-updated");
  selector.rebuild();
  snapshots = await snapshotPromise;

  await assertSnapshotList(snapshots, [
    { url: TEST_URL2, source: "Overlapping" },
  ]);
});
