/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that setUrl coalesces calls in quick succession, while setUrlAndRebuildNow
 * does not.
 */

const TEST_URL1 = "https://example.com/";
const TEST_URL2 = "https://example.com/12345";
const TEST_URL3 = "https://example.com/14235";

add_task(async function test_setUrlCoalescing() {
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
  await Snapshots.add({ url: TEST_URL2 });
  await Snapshots.add({ url: TEST_URL3 });
  // Changing the url should generate new snapshots and should exclude the
  // current url.
  selector.setUrl(TEST_URL1);
  snapshots = await snapshotPromise;

  await assertSnapshotList(snapshots, [{ url: TEST_URL2 }, { url: TEST_URL3 }]);

  snapshotPromise = selector.once("snapshots-updated");
  // However, setting the Url in quick succession should coalesce the calls,
  // and cause only the second call to be considered.
  selector.setUrl(TEST_URL2);
  selector.setUrl(TEST_URL3);
  snapshots = await snapshotPromise;
  await assertSnapshotList(snapshots, [{ url: TEST_URL2 }, { url: TEST_URL1 }]);

  // Lastly, calling setUrlAndRebuildNow should NOT coalesce, and two events should
  // come in: one with the result of setUrlAndRebuildNow, and the other with the
  // result of setUrl
  snapshotPromise = selector.once("snapshots-updated");
  selector.setUrlAndRebuildNow(TEST_URL2);
  selector.setUrl(TEST_URL1);
  snapshots = await snapshotPromise;
  snapshotPromise = selector.once("snapshots-updated");
  await assertSnapshotList(snapshots, [{ url: TEST_URL1 }, { url: TEST_URL3 }]);
  snapshots = await snapshotPromise;
  await assertSnapshotList(snapshots, [{ url: TEST_URL2 }, { url: TEST_URL3 }]);

  await reset();
});
