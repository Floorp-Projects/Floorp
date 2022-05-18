/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that updateDetailsAndRebuild coalesces calls in quick succession if
 * rebuildImmediately is not passed in.
 */

const TEST_URL1 = "https://example.com/";
const TEST_URL2 = "https://example.com/12345";
const TEST_URL3 = "https://example.com/14235";

add_task(async function test_coalescing() {
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
  selector.updateDetailsAndRebuild({ url: TEST_URL1 });
  snapshots = await snapshotPromise;

  await assertSnapshotList(snapshots, [{ url: TEST_URL2 }, { url: TEST_URL3 }]);

  snapshotPromise = selector.once("snapshots-updated");
  // However, setting the Url in quick succession should coalesce the calls,
  // and cause only the second call to be considered.
  selector.updateDetailsAndRebuild({ url: TEST_URL2 });
  selector.updateDetailsAndRebuild({ url: TEST_URL3 });
  snapshots = await snapshotPromise;
  await assertSnapshotList(snapshots, [{ url: TEST_URL2 }, { url: TEST_URL1 }]);

  // Lastly, rebuildImmediately should NOT coalesce, and two events should
  // come in: one with the result of rebuildImmediately, and the other one
  // without it.
  snapshotPromise = selector.once("snapshots-updated");
  selector.updateDetailsAndRebuild({
    url: TEST_URL2,
    rebuildImmediately: true,
  });
  selector.updateDetailsAndRebuild({ url: TEST_URL1 });
  snapshots = await snapshotPromise;
  snapshotPromise = selector.once("snapshots-updated");
  await assertSnapshotList(snapshots, [{ url: TEST_URL1 }, { url: TEST_URL3 }]);
  snapshots = await snapshotPromise;
  await assertSnapshotList(snapshots, [{ url: TEST_URL2 }, { url: TEST_URL3 }]);

  await reset();
});
