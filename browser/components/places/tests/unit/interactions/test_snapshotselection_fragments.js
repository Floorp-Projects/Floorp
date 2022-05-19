/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we correctly handle urls with fragments in snapshot selection
 */

const TEST_URL1 = "https://example.com/";
const TEST_URL2 = "https://example.com/12345";

const TEST_FRAGMENT_URL1 = "https://example.com/#fragment";
const TEST_FRAGMENT_URL2 = "https://example.com/12345#row=1";

add_task(async function test_snapshot_selection_fragments() {
  await Snapshots.reset();

  let selector = new SnapshotSelector({ count: 5 });
  let snapshotPromise = selector.once("snapshots-updated");
  selector.rebuild();
  let snapshots = await snapshotPromise;

  await assertSnapshotList(snapshots, []);

  await addInteractions([
    {
      url: TEST_URL1,
    },
  ]);
  await Snapshots.add({ url: TEST_FRAGMENT_URL1 });

  // Only the stripped url should be retrieved as a snapshot
  snapshotPromise = selector.once("snapshots-updated");
  selector.updateDetailsAndRebuild({ url: TEST_URL2 });
  snapshots = await snapshotPromise;
  await assertSnapshotList(snapshots, [{ url: TEST_URL1 }]);

  // The full url with fragments as the context should not return any snapshot
  snapshotPromise = selector.once("snapshots-updated");
  selector.updateDetailsAndRebuild({ url: TEST_FRAGMENT_URL1 });
  snapshots = await snapshotPromise;
  await assertSnapshotList(snapshots, []);

  snapshotPromise = selector.once("snapshots-updated");
  selector.updateDetailsAndRebuild({ url: TEST_FRAGMENT_URL2 });
  snapshots = await snapshotPromise;
  await assertSnapshotList(snapshots, [{ url: TEST_URL1 }]);

  // The url without fragments should also not return any snapshot
  snapshotPromise = selector.once("snapshots-updated");
  selector.updateDetailsAndRebuild({ url: TEST_URL1 });
  snapshots = await snapshotPromise;
  await assertSnapshotList(snapshots, []);

  selector.destroy();
});
