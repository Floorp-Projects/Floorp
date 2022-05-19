/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that the selectCommonReferrer param only selects common referrer snapshots
 */

const TEST_SEARCH_URL = "https://example.com/product_search";
const TEST_PRODUCT_A_URL = "https://example.com/product_a";
const TEST_PRODUCT_B_URL = "https://example.com/product_b";
const TEST_PRODUCT_C_URL = "https://example.com/product_c";

let selector;
let currentSessionUrls = new Set();

function getCurrentSessionUrls() {
  return currentSessionUrls;
}

add_task(async function test_setup() {
  await addInteractions([
    {
      url: TEST_SEARCH_URL,
    },
    {
      url: TEST_PRODUCT_A_URL,
      referrer: TEST_SEARCH_URL,
    },
    {
      url: TEST_PRODUCT_B_URL,
      referrer: TEST_SEARCH_URL,
    },
    {
      url: TEST_PRODUCT_C_URL,
    },
  ]);

  selector = new SnapshotSelector({
    count: 5,
    filterAdult: false,
    sourceWeights: {
      CommonReferrer: 3,
      Overlapping: 0,
    },
    getCurrentSessionUrls,
  });
});

add_task(async function test_common_referrer() {
  // Allow all snapshots regardless of their score.
  Services.prefs.setIntPref("browser.places.snapshots.threshold", -10);

  let snapshotPromise = selector.once("snapshots-updated");
  selector.rebuild();
  let snapshots = await snapshotPromise;

  await assertSnapshotList(snapshots, []);

  snapshotPromise = selector.once("snapshots-updated");
  await Snapshots.add({ url: TEST_SEARCH_URL });
  await Snapshots.add({ url: TEST_PRODUCT_A_URL });
  await Snapshots.add({ url: TEST_PRODUCT_B_URL });
  await Snapshots.add({ url: TEST_PRODUCT_C_URL });

  selector.updateDetailsAndRebuild({ url: TEST_SEARCH_URL });
  snapshots = await snapshotPromise;
  await assertSnapshotList(snapshots, []);

  snapshotPromise = selector.once("snapshots-updated");
  selector.updateDetailsAndRebuild({ url: TEST_PRODUCT_A_URL });
  snapshots = await snapshotPromise;

  await assertSnapshotList(snapshots, [{ url: TEST_PRODUCT_B_URL }]);

  snapshotPromise = selector.once("snapshots-updated");
  selector.updateDetailsAndRebuild({ url: TEST_PRODUCT_B_URL });
  snapshots = await snapshotPromise;

  await assertSnapshotList(snapshots, [{ url: TEST_PRODUCT_A_URL }]);
});

add_task(async function test_test_common_referrer_with_scoring() {
  // Allow any snapshots with a low, but non-zero score
  Services.prefs.setIntPref("browser.places.snapshots.threshold", 0.5);

  let snapshotPromise = selector.once("snapshots-updated");
  selector.updateDetailsAndRebuild({ url: TEST_PRODUCT_C_URL });
  let snapshots = await snapshotPromise;

  // Should still be empty -- no common referrer
  await assertSnapshotList(snapshots, []);

  snapshotPromise = selector.once("snapshots-updated");
  selector.updateDetailsAndRebuild({ url: TEST_PRODUCT_A_URL });
  snapshots = await snapshotPromise;

  await assertSnapshotList(snapshots, [{ url: TEST_PRODUCT_B_URL }]);
});
