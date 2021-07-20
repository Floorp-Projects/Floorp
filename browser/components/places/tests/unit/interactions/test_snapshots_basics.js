/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for snapshot creation and removal.
 */

const TEST_URL1 = "https://example.com/";
const TEST_URL2 = "https://example.com/12345";
const TEST_URL3 = "https://example.com/14235";

add_task(async function setup() {
  let now = Date.now();
  await addInteractions([
    // The updated_at values are force to ensure unique times so that we can
    // retrieve the snapshots in the expected order.
    {
      url: TEST_URL1,
      documentType: Interactions.DOCUMENT_TYPE.MEDIA,
      created_at: now - 20000,
      updated_at: now - 20000,
    },
    { url: TEST_URL2, created_at: now - 10000, updated_at: now - 10000 },
    { url: TEST_URL3, created_at: now, updated_at: now },
  ]);
});

add_task(async function test_add_simple_snapshot() {
  await assertUrlNotification(TOPIC_ADDED, [TEST_URL1], () =>
    Snapshots.add({ url: TEST_URL1 })
  );

  await assertUrlNotification(TOPIC_ADDED, [TEST_URL2], () =>
    Snapshots.add({ url: TEST_URL2, userPersisted: true })
  );

  await assertSnapshots([
    { url: TEST_URL2, userPersisted: true },
    { url: TEST_URL1, documentType: Interactions.DOCUMENT_TYPE.MEDIA },
  ]);

  let snapshot = await Snapshots.get(TEST_URL2);
  assertSnapshot(snapshot, { url: TEST_URL2, userPersisted: true });
});

add_task(async function test_add_snapshot_without_interaction() {
  await Assert.rejects(
    Snapshots.add({ url: "https://invalid.com/" }),
    /NOT NULL constraint failed/,
    "Should reject if an interaction is missing"
  );
});

add_task(async function test_add_duplicate_snapshot() {
  await Snapshots.add({ url: TEST_URL3 });

  let initialSnapshot = await Snapshots.get(TEST_URL3);

  await assertTopicNotObserved(TOPIC_ADDED, () =>
    Snapshots.add({ url: TEST_URL3 })
  );

  let newSnapshot = await Snapshots.get(TEST_URL3);
  Assert.deepEqual(
    initialSnapshot,
    newSnapshot,
    "Snapshot should have remained the same"
  );

  // Check that the other snapshots have not had userPersisted changed.
  await assertSnapshots([
    { url: TEST_URL3 },
    { url: TEST_URL2, userPersisted: true },
    { url: TEST_URL1, documentType: Interactions.DOCUMENT_TYPE.MEDIA },
  ]);

  info("Re-add existing snapshot to check for userPersisted flag");
  await Snapshots.add({ url: TEST_URL3, userPersisted: true });

  newSnapshot = await Snapshots.get(TEST_URL3);
  Assert.deepEqual(
    { ...initialSnapshot, userPersisted: true },
    newSnapshot,
    "Snapshot should have remained the same apart from the userPersisted value"
  );

  // Check that the other snapshots have not had userPersisted changed.
  await assertSnapshots([
    { url: TEST_URL3, userPersisted: true },
    { url: TEST_URL2, userPersisted: true },
    { url: TEST_URL1, documentType: Interactions.DOCUMENT_TYPE.MEDIA },
  ]);
});

add_task(async function test_get_snapshot_not_found() {
  let snapshot = await Snapshots.get("https://invalid.com/");
  Assert.ok(
    !snapshot,
    "Should not have found a snapshot for a URL not in the database"
  );
});

add_task(async function test_delete_snapshot() {
  let removedAt = new Date();

  await assertUrlNotification(TOPIC_DELETED, [TEST_URL1], () =>
    Snapshots.delete(TEST_URL1)
  );

  let snapshot = await Snapshots.get(TEST_URL1);
  Assert.ok(!snapshot, "Tombstone snapshots should not be returned by default");

  snapshot = await Snapshots.get(TEST_URL1, true);
  assertSnapshot(snapshot, {
    url: TEST_URL1,
    documentType: Interactions.DOCUMENT_TYPE.MEDIA,
    removedAt,
  });

  info("Attempt to re-add non-user persisted snapshot");
  await Snapshots.add({ url: TEST_URL1 });

  snapshot = await Snapshots.get(TEST_URL1, true);
  assertSnapshot(snapshot, {
    url: TEST_URL1,
    documentType: Interactions.DOCUMENT_TYPE.MEDIA,
    removedAt,
  });

  info("Re-add user persisted snapshot");
  await Snapshots.add({ url: TEST_URL1, userPersisted: true });

  snapshot = await Snapshots.get(TEST_URL1);
  assertSnapshot(snapshot, {
    url: TEST_URL1,
    documentType: Interactions.DOCUMENT_TYPE.MEDIA,
    userPersisted: true,
  });
});
