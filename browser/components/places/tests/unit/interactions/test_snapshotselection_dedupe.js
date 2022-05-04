/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that snapshots are correctly de-duped.
 */

let now = Date.now();

const URLS = [
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
  },
  {
    url: "https://example.com/2?foo=341256",
    title: "Example2",
    created_at: now - 1000,
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

  await addInteractions(URLS);
  for (let { url } of URLS) {
    await Snapshots.add({ url });
  }
});

add_task(async function test_dedupe() {
  let selector = new SnapshotSelector({
    count: 5,
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
    // No duplicates of this URL, so we expect it to be included.
    { url: "https://example.com/foo", title: "Other" },
  ]);
});
