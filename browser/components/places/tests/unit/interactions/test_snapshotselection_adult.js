/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that we adult sites are correctly filtered from snapshot selections
 * when requested.
 */

const TEST_URL1 = "https://example.com/";
const TEST_URL2 = "https://invalid.com/";
const TEST_URL3 = "https://foo.com/";
const TEST_URL4 = "https://bar.com/";
const TEST_URL5 = "https://something.com/";

XPCOMUtils.defineLazyModuleGetters(this, {
  FilterAdult: "resource://activity-stream/lib/FilterAdult.jsm",
});

async function addSnapshotAndFilter(url) {
  await PlacesTestUtils.addVisits(url);
  await Snapshots.add({ url, userPersisted: Snapshots.USER_PERSISTED.MANUAL });

  FilterAdult.addDomainToList(url);
}

add_task(async function setup() {
  let now = Date.now();

  await addInteractions([{ url: TEST_URL1, created_at: now - 2000 }]);
  await Snapshots.add({ url: TEST_URL1 });

  await addSnapshotAndFilter(TEST_URL2);
});

add_task(async function test_interactions_adult_basic() {
  let anySelector = new SnapshotSelector({ count: 2, filterAdult: false });
  let adultFilterSelector = new SnapshotSelector({
    count: 2,
    filterAdult: true,
  });

  let snapshotPromise = anySelector.once("snapshots-updated");
  anySelector.rebuild();
  let snapshots = await snapshotPromise;

  await assertSnapshotList(snapshots, [
    { url: TEST_URL2, userPersisted: Snapshots.USER_PERSISTED.MANUAL },
    { url: TEST_URL1 },
  ]);

  snapshotPromise = adultFilterSelector.once("snapshots-updated");
  adultFilterSelector.rebuild();
  snapshots = await snapshotPromise;

  await assertSnapshotList(snapshots, [{ url: TEST_URL1 }]);

  anySelector.destroy();
  adultFilterSelector.destroy();
});

add_task(async function test_interactions_adult_filter_multiple() {
  // Add a couple more sites to the filter.
  await addSnapshotAndFilter(TEST_URL3);
  await addSnapshotAndFilter(TEST_URL4);

  // Add a second url to not be filtered.
  await addInteractions([{ url: TEST_URL5, created_at: Date.now() - 2000 }]);
  await Snapshots.add({ url: TEST_URL5 });

  let adultFilterSelector = new SnapshotSelector({
    count: 2,
    filterAdult: true,
  });

  let snapshotPromise = adultFilterSelector.once("snapshots-updated");
  adultFilterSelector.rebuild();
  let snapshots = await snapshotPromise;

  await assertSnapshotList(snapshots, [{ url: TEST_URL5 }, { url: TEST_URL1 }]);

  adultFilterSelector.destroy();
});
