/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for snapshot creation and removal.
 */

const TEST_URL1 = "https://example.com/";
const TEST_URL2 = "https://example.com/12345";
const TOPIC_ADDED = "places-snapshot-added";
const TOPIC_DELETED = "places-snapshot-deleted";

add_task(async function test_add_simple_snapshot() {
  await addInteractions([
    { url: TEST_URL1, documentType: Interactions.DOCUMENT_TYPE.MEDIA },
    { url: TEST_URL2 },
  ]);

  let promise = TestUtils.topicObserved(
    TOPIC_ADDED,
    (subject, data) => !subject && data == TEST_URL1
  );
  await Snapshots.add({ url: TEST_URL1 });
  await promise;

  promise = TestUtils.topicObserved(
    TOPIC_ADDED,
    (subject, data) => !subject && data == TEST_URL2
  );
  await Snapshots.add({ url: TEST_URL2, userPersisted: true });
  await promise;

  await assertSnapshots([
    { url: TEST_URL1, documentType: Interactions.DOCUMENT_TYPE.MEDIA },
    { url: TEST_URL2, userPersisted: true },
  ]);

  let snapshot = await Snapshots.get(TEST_URL2);
  assertSnapshot(snapshot, { url: TEST_URL2, userPersisted: true });
});

add_task(async function test_add_snapshot_without_interaction() {
  await Assert.rejects(
    Snapshots.add({ url: "https://invalid.com/" }),
    /Could not find existing interactions/,
    "Should reject if an interaction is missing"
  );
});

add_task(async function test_get_snapshot_not_found() {
  let snapshot = await Snapshots.get("https://invalid.com/");
  Assert.ok(
    !snapshot,
    "Should not have found a snapshot for a URL not in the database"
  );
});

add_task(async function test_remove_snapshot() {
  let removedAt = new Date();

  let promise = TestUtils.topicObserved(
    TOPIC_DELETED,
    (subject, data) => !subject && data == TEST_URL1
  );
  await Snapshots.delete(TEST_URL1);
  await promise;

  let snapshot = await Snapshots.get(TEST_URL1);
  Assert.ok(!snapshot, "Tombstone snapshots should not be returned by default");

  snapshot = await Snapshots.get(TEST_URL1, true);
  assertSnapshot(snapshot, {
    url: TEST_URL1,
    documentType: Interactions.DOCUMENT_TYPE.MEDIA,
    removedAt,
  });
});
