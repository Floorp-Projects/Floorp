/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for snapshots with fragments - addition, retreival, removal.
 */

const TEST_URL1 = "https://example.com/";
const TEST_URL2 = "https://example.com/12345";
const TEST_URL3 = "https://example.com/14235";
const TEST_URL4 = "https://example.com/15234";
const TEST_URL5 = "https://example.com/54321";
const TEST_URL6 = "https://example.com/51432";

const TEST_FRAGMENT_URL1 = "https://example.com/#fragment";
const TEST_FRAGMENT_URL2 = "https://example.com/12345#row=1";
const TEST_FRAGMENT_URL3 = "https://example.com/14235#column=2";
const TEST_FRAGMENT_URL4 = "https://example.com/15234";
const TEST_FRAGMENT_URL5 = "https://example.com/54321";
const TEST_FRAGMENT_URL6 = "https://example.com/51432#cell=4,1-6,2";

add_setup(async () => {
  let now = Date.now();
  await addInteractions([
    { url: TEST_URL1, created_at: now, updated_at: now },
    { url: TEST_URL2, created_at: now, updated_at: now },
    { url: TEST_URL3, created_at: now, updated_at: now },
    { url: TEST_URL4, created_at: now, updated_at: now },
    { url: TEST_URL5, created_at: now, updated_at: now },
    { url: TEST_URL6, created_at: now, updated_at: now },
    { url: TEST_FRAGMENT_URL1, created_at: now, updated_at: now },
    { url: TEST_FRAGMENT_URL2, created_at: now, updated_at: now },
    { url: TEST_FRAGMENT_URL3, created_at: now, updated_at: now },
    { url: TEST_FRAGMENT_URL4, created_at: now, updated_at: now },
    { url: TEST_FRAGMENT_URL5, created_at: now, updated_at: now },
    { url: TEST_FRAGMENT_URL6, created_at: now, updated_at: now },
  ]);
});

add_task(async function test_stripFragments() {
  Assert.equal(
    Snapshots.stripFragments(),
    undefined,
    "stripFragments should handle undefined as an argument"
  );
  Assert.equal(
    Snapshots.stripFragments(TEST_FRAGMENT_URL1),
    TEST_URL1,
    "stripFragments should remove fragments"
  );
  Assert.equal(
    Snapshots.stripFragments(TEST_FRAGMENT_URL2),
    TEST_URL2,
    "stripFragments should remove fragments"
  );
  Assert.equal(
    Snapshots.stripFragments(TEST_FRAGMENT_URL3),
    TEST_URL3,
    "stripFragments should remove fragments"
  );
  Assert.equal(
    Snapshots.stripFragments(TEST_FRAGMENT_URL4),
    TEST_URL4,
    "stripFragments should remove fragments"
  );
  Assert.equal(
    Snapshots.stripFragments(TEST_FRAGMENT_URL5),
    TEST_URL5,
    "stripFragments should remove fragments"
  );
  Assert.equal(
    Snapshots.stripFragments(TEST_FRAGMENT_URL6),
    TEST_URL6,
    "stripFragments should remove fragments"
  );
});

add_task(async function test_add_fragment_snapshots() {
  await Snapshots.reset();

  await assertSnapshots([]);

  // After adding a snapshot with the fragment url, we should retrieve the stripped form
  await Snapshots.add({ url: TEST_FRAGMENT_URL1 });
  await assertSnapshots([{ url: TEST_URL1 }]);

  await Snapshots.reset();
  await Snapshots.add({ url: TEST_FRAGMENT_URL2 });
  await assertSnapshots([{ url: TEST_URL2 }]);

  await Snapshots.reset();
  await Snapshots.add({ url: TEST_FRAGMENT_URL3 });
  await assertSnapshots([{ url: TEST_URL3 }]);

  await Snapshots.reset();
  await Snapshots.add({ url: TEST_FRAGMENT_URL4 });
  await assertSnapshots([{ url: TEST_URL4 }]);

  await Snapshots.reset();
  await Snapshots.add({ url: TEST_FRAGMENT_URL5 });
  await assertSnapshots([{ url: TEST_URL5 }]);

  await Snapshots.reset();
  await Snapshots.add({ url: TEST_FRAGMENT_URL6 });
  await assertSnapshots([{ url: TEST_URL6 }]);
});

add_task(async function test_get_fragment_snapshots() {
  await Snapshots.reset();
  await Snapshots.add({ url: TEST_FRAGMENT_URL1 });
  let snapshot = await Snapshots.get(TEST_FRAGMENT_URL1);
  Assert.equal(
    snapshot.url,
    TEST_URL1,
    "Snapshots.get() should return the snapshot with the fragment stripped"
  );
});

add_task(async function test_delete_fragment_snapshots() {
  await Snapshots.reset();
  await Snapshots.add({ url: TEST_FRAGMENT_URL1 });
  await assertSnapshots([{ url: TEST_URL1 }]);
  await Snapshots.delete(TEST_FRAGMENT_URL1);
  await assertSnapshots([]);
});
