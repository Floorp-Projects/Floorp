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

add_task(async function test_interactions_recent() {
  let now = Date.now();
  await addInteractions([
    { url: TEST_URL1, created_at: now - 2000 },
    { url: TEST_URL2, created_at: now - 1000 },
    { url: TEST_URL3, created_at: now - 3000 },
  ]);

  let selector = new SnapshotSelector({ count: 2 });

  let snapshotPromise = selector.once("snapshots-updated");
  selector.rebuild();
  let snapshots = await snapshotPromise;

  await assertSnapshotList(snapshots, []);

  snapshotPromise = selector.once("snapshots-updated");
  await Snapshots.add({ url: TEST_URL1 });
  snapshots = await snapshotPromise;

  await assertSnapshotList(snapshots, [{ url: TEST_URL1, source: "recent" }]);

  // Changing the url should generate new snapshots and should exclude the
  // current url.
  snapshotPromise = selector.once("snapshots-updated");
  selector.updateDetailsAndRebuild({ url: TEST_URL1 });
  snapshots = await snapshotPromise;

  await assertSnapshotList(snapshots, []);

  snapshotPromise = selector.once("snapshots-updated");
  selector.updateDetailsAndRebuild({ url: TEST_URL2 });
  snapshots = await snapshotPromise;

  await assertSnapshotList(snapshots, [{ url: TEST_URL1, source: "recent" }]);

  snapshotPromise = selector.once("snapshots-updated");
  await Snapshots.add({ url: TEST_URL2 });
  snapshots = await snapshotPromise;

  await assertSnapshotList(snapshots, [{ url: TEST_URL1, source: "recent" }]);

  snapshotPromise = selector.once("snapshots-updated");
  await Snapshots.add({ url: TEST_URL3 });
  snapshots = await snapshotPromise;

  await assertSnapshotList(snapshots, [
    { url: TEST_URL1, source: "recent" },
    { url: TEST_URL3, source: "recent" },
  ]);

  snapshotPromise = selector.once("snapshots-updated");
  selector.updateDetailsAndRebuild({ url: TEST_URL3 });
  snapshots = await snapshotPromise;

  await assertSnapshotList(snapshots, [
    { url: TEST_URL2, source: "recent" },
    { url: TEST_URL1, source: "recent" },
  ]);

  snapshotPromise = selector.once("snapshots-updated");
  selector.updateDetailsAndRebuild({ url: TEST_URL4 });
  snapshots = await snapshotPromise;

  // The snapshot count is limited to 2.
  await assertSnapshotList(snapshots, [
    { url: TEST_URL2, source: "recent" },
    { url: TEST_URL1, source: "recent" },
  ]);

  await reset();
});
