/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that we select the most recent set of snapshots excluding the current
 * url.
 */

const TEST_URL1 = "https://example.com/";
const TEST_URL2 = "https://example.com/12345";
const TEST_URL3 = "https://example.com/14235";
const TEST_URL4 = "https://example.com/14345";

XPCOMUtils.defineLazyModuleGetters(this, {
  SnapshotSelector: "resource:///modules/SnapshotSelector.jsm",
  PageDataService: "resource:///modules/pagedata/PageDataService.jsm",
  PageDataCollector: "resource:///modules/pagedata/PageDataCollector.jsm",
});

add_task(async () => {
  let now = Date.now();
  await addInteractions([
    { url: TEST_URL1, created_at: now - 2000 },
    { url: TEST_URL2, created_at: now - 1000 },
    { url: TEST_URL3, created_at: now - 3000 },
  ]);

  PageDataService.pageDataDiscovered(TEST_URL1, [
    {
      type: PageDataCollector.DATA_TYPE.PRODUCT,
      data: {
        price: 276,
      },
    },
  ]);

  await Snapshots.add({ url: TEST_URL1 });
  await Snapshots.add({ url: TEST_URL2 });
  await Snapshots.add({ url: TEST_URL3 });

  let selector = new SnapshotSelector(5);

  let snapshotPromise = selector.once("snapshots-updated");
  selector.setUrl(TEST_URL4);
  let snapshots = await snapshotPromise;

  // Finds any snapshot.
  await assertSnapshotList(snapshots, [
    { url: TEST_URL2 },
    { url: TEST_URL1 },
    { url: TEST_URL3 },
  ]);

  snapshotPromise = selector.once("snapshots-updated");
  selector.setType(PageDataCollector.DATA_TYPE.PRODUCT);
  snapshots = await snapshotPromise;

  // Only finds the product snapshot.
  await assertSnapshotList(snapshots, [{ url: TEST_URL1 }]);

  snapshotPromise = selector.once("snapshots-updated");
  selector.setType(undefined);
  snapshots = await snapshotPromise;

  // Back to any.
  await assertSnapshotList(snapshots, [
    { url: TEST_URL2 },
    { url: TEST_URL1 },
    { url: TEST_URL3 },
  ]);

  await reset();
});
