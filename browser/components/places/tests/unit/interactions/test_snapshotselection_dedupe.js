/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that snapshots are correctly de-duped.
 */

let now = Date.now();

const TEST_DATA = [
  // These urls are deduplicated down to the one without the query.
  {
    url: "https://example.com/1?foo=135246",
    title: "Example1",
    created_at: now - 2000,
  },
  {
    url: "https://example.com/1",
    title: "Example1",
    created_at: now - 1000,
  },
  {
    url: "https://example.com/1?bar=142536",
    title: "Example1",
    created_at: now - 3000,
  },
  // This should not be deduped.
  {
    url: "https://example.com/foo",
    title: "Other",
    created_at: now - 40000,
  },
  // These should be deduped to the most recent visit or highest score.
  {
    url: "https://example.com/2?foo=531245",
    title: "Example2",
    created_at: now - 10000,
    // pinned will give this a higher score even though it is not the most
    // recent visit.
    pinned: true,
  },
  {
    url: "https://example.com/2?foo=341256",
    title: "Example2",
    created_at: now - 1000,
  },
  // This is a snapshot for relevancy which won't be returned in that set as
  // it is the snapshot being visited.
  {
    url: "https://example.com/relevant",
    title: "Relevant",
    created_at: now - 50000,
  },
  // Long titles which match but the urls do not, so we should get a difference
  // indication.
  {
    url: "https://example.com/title1",
    title: "Long title with a difference to the other one.",
    created_at: now - 5000,
  },
  {
    url: "https://example.com/title2",
    title: "Long title with a difference to the previous one.",
    created_at: now - 6000,
  },
];

add_setup(async () => {
  // Allow all snapshots regardless of their score.
  Services.prefs.setIntPref("browser.places.snapshots.threshold", -10);
  // Ensure the pref is disabled, so the test runs properly whichever way it
  // is set.
  Services.prefs.setBoolPref(
    "browser.pinebuild.snapshots.relevancy.enabled",
    false
  );

  await addInteractions(TEST_DATA);
  for (let { url, pinned } of TEST_DATA) {
    await Snapshots.add({
      url,
      userPersisted: pinned
        ? Snapshots.USER_PERSISTED.PINNED
        : Snapshots.USER_PERSISTED.NO,
    });
  }
});

add_task(async function test_dedupe() {
  let selector = new SnapshotSelector({
    count: 6,
    filterAdult: false,
    getCurrentSessionUrls: () => new Set(),
  });

  let snapshotPromise = selector.once("snapshots-updated");
  selector.rebuild();
  let snapshots = await snapshotPromise;

  await assertSnapshotList(snapshots, [
    // Deduped to no-params.
    { url: "https://example.com/1", title: "Example1" },
    // Deduped to most recent.
    { url: "https://example.com/2?foo=341256", title: "Example2" },
    // Difference indication on titles.
    {
      url: "https://example.com/title1",
      title: "Long title with a difference to the other one.",
      titleDifferentIndex: 40,
    },
    {
      url: "https://example.com/title2",
      title: "Long title with a difference to the previous one.",
      titleDifferentIndex: 40,
    },
    // No duplicates of this URL, so we expect it to be included.
    { url: "https://example.com/foo", title: "Other" },
    // This is really for the next test, but shows up here as it is in the list.
    { url: "https://example.com/relevant", title: "Relevant" },
  ]);
});

add_task(async function test_relevancy_dedupe() {
  let selector = new SnapshotSelector({
    count: 10,
    filterAdult: false,
    sourceWeights: {
      CommonReferrer: 0,
      Overlapping: 3,
    },
    getCurrentSessionUrls: () => new Set(),
  });
  selector.updateDetailsAndRebuild({ url: "https://example.com/relevant" });

  let snapshotPromise = selector.once("snapshots-updated");
  selector.rebuild();
  let snapshots = await snapshotPromise;

  await assertSnapshotList(snapshots, [
    {
      url: "https://example.com/2?foo=531245",
      title: "Example2",
      userPersisted: Snapshots.USER_PERSISTED.PINNED,
    },
    { url: "https://example.com/1", title: "Example1" },
    // Difference indication on titles.
    {
      url: "https://example.com/title1",
      title: "Long title with a difference to the other one.",
      titleDifferentIndex: 40,
    },
    {
      url: "https://example.com/title2",
      title: "Long title with a difference to the previous one.",
      titleDifferentIndex: 40,
    },
    { url: "https://example.com/foo", title: "Other" },
  ]);
});
