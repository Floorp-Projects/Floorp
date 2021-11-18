/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that the selectOverlappingVisits param only selects overlapping snapshots
 */

const TEST_URL1 = "https://example.com/";
const TEST_URL2 = "https://example.com/2";
const TEST_URL3 = "https://example.com/3";
const TEST_URL4 = "https://example.com/4";

add_task(async function test_enable_overlapping() {
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

  let selector = new SnapshotSelector(
    5 /* count */,
    false /* filter adult */,
    true /* selectOverlappingVisits */
  );

  let snapshotPromise = selector.once("snapshots-updated");
  selector.rebuild();
  let snapshots = await snapshotPromise;

  await assertSnapshotList(snapshots, []);

  snapshotPromise = selector.once("snapshots-updated");
  await Snapshots.add({ url: TEST_URL1 });
  await Snapshots.add({ url: TEST_URL2 });
  await Snapshots.add({ url: TEST_URL3 });
  await Snapshots.add({ url: TEST_URL4 });

  selector.setUrl(TEST_URL1);
  snapshots = await snapshotPromise;

  // Only snapshots with overlaping interactions should be selected
  await assertSnapshotList(snapshots, [{ url: TEST_URL2 }]);
});
