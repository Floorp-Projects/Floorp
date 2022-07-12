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

ChromeUtils.defineESModuleGetters(this, {
  PageDataSchema: "resource:///modules/pagedata/PageDataSchema.sys.mjs",
  PageDataService: "resource:///modules/pagedata/PageDataService.sys.mjs",
});

add_task(async () => {
  let now = Date.now();
  await addInteractions([
    { url: TEST_URL1, created_at: now - 2000 },
    { url: TEST_URL2, created_at: now - 1000 },
    { url: TEST_URL3, created_at: now - 3000 },
  ]);

  // Simulate a browser keeping this page data cached.
  let actor = {};
  PageDataService.lockEntry(actor, TEST_URL1);

  PageDataService.pageDataDiscovered({
    url: TEST_URL1,
    data: {
      [PageDataSchema.DATA_TYPE.PRODUCT]: {
        price: {
          value: 276,
        },
      },
    },
  });

  await Snapshots.add({ url: TEST_URL1 });
  await Snapshots.add({ url: TEST_URL2 });
  await Snapshots.add({ url: TEST_URL3 });

  PageDataService.unlockEntry(actor, TEST_URL1);

  let selector = new SnapshotSelector({ count: 5 });

  let snapshotPromise = selector.once("snapshots-updated");
  selector.updateDetailsAndRebuild({ url: TEST_URL4 });
  let snapshots = await snapshotPromise;

  // Finds any snapshot.
  await assertSnapshotList(snapshots, [
    { url: TEST_URL2 },
    { url: TEST_URL1 },
    { url: TEST_URL3 },
  ]);

  snapshotPromise = selector.once("snapshots-updated");
  selector.updateDetailsAndRebuild({ type: PageDataSchema.DATA_TYPE.PRODUCT });
  snapshots = await snapshotPromise;

  // Only finds the product snapshot.
  await assertSnapshotList(snapshots, [{ url: TEST_URL1 }]);

  snapshotPromise = selector.once("snapshots-updated");
  selector.updateDetailsAndRebuild({ type: null });
  snapshots = await snapshotPromise;

  // Back to any.
  await assertSnapshotList(snapshots, [
    { url: TEST_URL2 },
    { url: TEST_URL1 },
    { url: TEST_URL3 },
  ]);

  await reset();
});
