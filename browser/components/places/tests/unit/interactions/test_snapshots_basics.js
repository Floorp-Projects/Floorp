/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for snapshot creation and removal.
 */

const TEST_URL1 = "https://example.com/";
const TEST_URL2 = "https://example.com/12345";
const TEST_URL3 = "https://example.com/14235";
const TEST_URL4 = "https://example.com/15234";
const TEST_URL5 = "https://example.com/54321";
const TEST_URL6 = "https://example.com/51432";

add_task(async function setup() {
  let now = Date.now();
  await addInteractions([
    // The updated_at values are force to ensure unique times so that we can
    // retrieve the snapshots in the expected order.
    {
      url: TEST_URL1,
      documentType: Interactions.DOCUMENT_TYPE.MEDIA,
      created_at: now - 30000,
      updated_at: now - 30000,
    },
    { url: TEST_URL2, created_at: now - 20000, updated_at: now - 20000 },
    { url: TEST_URL3, created_at: now - 10000, updated_at: now - 10000 },
    { url: TEST_URL4, created_at: now, updated_at: now },
    { url: TEST_URL5, created_at: now, updated_at: now },
    { url: TEST_URL6, created_at: now, updated_at: now },
  ]);
});

add_task(async function test_add_simple_snapshot() {
  await assertUrlNotification(TOPIC_ADDED, [TEST_URL1], () =>
    Snapshots.add({ url: TEST_URL1 })
  );

  await assertUrlNotification(TOPIC_ADDED, [TEST_URL2], () =>
    Snapshots.add({
      url: TEST_URL2,
      userPersisted: Snapshots.USER_PERSISTED.MANUAL,
    })
  );

  await assertUrlNotification(TOPIC_ADDED, [TEST_URL3], () =>
    Snapshots.add({
      url: TEST_URL3,
      userPersisted: Snapshots.USER_PERSISTED.PINNED,
    })
  );

  await assertSnapshots([
    { url: TEST_URL3, userPersisted: Snapshots.USER_PERSISTED.PINNED },
    { url: TEST_URL2, userPersisted: Snapshots.USER_PERSISTED.MANUAL },
    { url: TEST_URL1, documentType: Interactions.DOCUMENT_TYPE.MEDIA },
  ]);

  let snapshot = await Snapshots.get(TEST_URL2);
  assertSnapshot(snapshot, {
    url: TEST_URL2,
    userPersisted: Snapshots.USER_PERSISTED.MANUAL,
  });
});

add_task(async function test_add_duplicate_snapshot() {
  await Snapshots.add({ url: TEST_URL4 });

  let initialSnapshot = await Snapshots.get(TEST_URL4);

  await assertTopicNotObserved(TOPIC_ADDED, () =>
    Snapshots.add({ url: TEST_URL4 })
  );

  let newSnapshot = await Snapshots.get(TEST_URL4);
  Assert.deepEqual(
    initialSnapshot,
    newSnapshot,
    "Snapshot should have remained the same"
  );

  // Check that the other snapshots have not had userPersisted changed.
  await assertSnapshots([
    { url: TEST_URL4 },
    { url: TEST_URL3, userPersisted: Snapshots.USER_PERSISTED.PINNED },
    { url: TEST_URL2, userPersisted: Snapshots.USER_PERSISTED.MANUAL },
    { url: TEST_URL1, documentType: Interactions.DOCUMENT_TYPE.MEDIA },
  ]);

  info("Re-add existing snapshot to check for userPersisted value");
  await Snapshots.add({
    url: TEST_URL4,
    userPersisted: Snapshots.USER_PERSISTED.MANUAL,
  });

  newSnapshot = await Snapshots.get(TEST_URL4);
  Assert.deepEqual(
    { ...initialSnapshot, userPersisted: Snapshots.USER_PERSISTED.MANUAL },
    newSnapshot,
    "Snapshot should have remained the same apart from the userPersisted value"
  );

  info("Change existing snapshot userPersisted from MANUAL to PINNED");
  await Snapshots.add({
    url: TEST_URL4,
    userPersisted: Snapshots.USER_PERSISTED.PINNED,
  });

  newSnapshot = await Snapshots.get(TEST_URL4);
  Assert.deepEqual(
    { ...initialSnapshot, userPersisted: Snapshots.USER_PERSISTED.PINNED },
    newSnapshot,
    "Snapshot should have remained the same apart from the userPersisted value"
  );

  // Check that the other snapshots have not had userPersisted changed.
  await assertSnapshots([
    { url: TEST_URL4, userPersisted: Snapshots.USER_PERSISTED.PINNED },
    { url: TEST_URL3, userPersisted: Snapshots.USER_PERSISTED.PINNED },
    { url: TEST_URL2, userPersisted: Snapshots.USER_PERSISTED.MANUAL },
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
    removedReason: Snapshots.REMOVED_REASON.DISMISS,
  });

  info("Attempt to re-add non-user persisted snapshot");
  await Snapshots.add({ url: TEST_URL1 });

  snapshot = await Snapshots.get(TEST_URL1, true);
  assertSnapshot(snapshot, {
    url: TEST_URL1,
    documentType: Interactions.DOCUMENT_TYPE.MEDIA,
    removedAt,
    removedReason: Snapshots.REMOVED_REASON.DISMISS,
  });

  info("Re-add user persisted snapshot");
  await Snapshots.add({
    url: TEST_URL1,
    userPersisted: Snapshots.USER_PERSISTED.MANUAL,
  });

  snapshot = await Snapshots.get(TEST_URL1);
  assertSnapshot(snapshot, {
    url: TEST_URL1,
    documentType: Interactions.DOCUMENT_TYPE.MEDIA,
    userPersisted: Snapshots.USER_PERSISTED.MANUAL,
  });
});

add_task(async function test_add_titled_snapshot() {
  let title = "test";
  await Snapshots.add({ url: TEST_URL5, title });
  let snapshot = await Snapshots.get(TEST_URL5);
  Assert.equal(snapshot.title, title, "Title should have been set");

  info("Update the title");
  title = "test-changed";
  await Snapshots.add({ url: TEST_URL5, title });
  snapshot = await Snapshots.get(TEST_URL5);
  Assert.equal(snapshot.title, title, "Title should have been updated");

  info("Also check query");
  let snapshots = (await Snapshots.query()).filter(s => s.url == TEST_URL5);
  Assert.equal(snapshots[0].title, title, "Title should have been updated");
});

add_task(async function test_update_untitled_snapshot() {
  info("Add snapshot to a page having history title");
  const HISTORY_TITLE = "history-title";
  let title = HISTORY_TITLE;
  await PlacesTestUtils.addVisits({ uri: TEST_URL6, title });
  await Snapshots.add({
    url: TEST_URL6,
    userPersisted: Snapshots.USER_PERSISTED.PINNED,
  });
  let snapshot = await Snapshots.get(TEST_URL6);
  Assert.equal(snapshot.title, title, "History title should have been used");

  info("Update the title");
  title = "test-changed";
  await Snapshots.add({ url: TEST_URL6, title });
  snapshot = await Snapshots.get(TEST_URL6);
  Assert.equal(snapshot.title, title, "Title should have been updated");
  Assert.equal(
    snapshot.userPersisted,
    Snapshots.USER_PERSISTED.PINNED,
    "UserPersisted should not have been changed"
  );

  info("Not passing a title should not clear it");
  await Snapshots.add({ url: TEST_URL6 });
  snapshot = await Snapshots.get(TEST_URL6);
  Assert.equal(snapshot.title, title, "Title should not have been updated");

  info("Passing an empty title should clear it");
  title = "";
  await Snapshots.add({ url: TEST_URL6, title });
  snapshot = await Snapshots.get(TEST_URL6);
  Assert.equal(
    snapshot.title,
    HISTORY_TITLE,
    "Title should revert to the history one"
  );
});

add_task(async function test_removed_reason() {
  let reasons = Object.keys(Snapshots.REMOVED_REASON);
  for (let reason of reasons) {
    let expectedReason = Snapshots.REMOVED_REASON[reason];
    info(`Testing removed reason ${reason} (${expectedReason})`);
    Assert.equal(typeof expectedReason, "number");
    await Snapshots.reset();
    await Snapshots.add({ url: TEST_URL1 });
    await assertUrlNotification(TOPIC_DELETED, [TEST_URL1], () =>
      Snapshots.delete(TEST_URL1, expectedReason)
    );
    let db = await PlacesUtils.promiseDBConnection();
    let rows = await db.execute(
      "SELECT removed_at, removed_reason FROM moz_places_metadata_snapshots"
    );
    if (reason == "EXPIRED") {
      Assert.equal(rows.length, 0);
    } else {
      Assert.equal(rows.length, 1);
      Assert.greater(rows[0].getResultByName("removed_at"), 0);
      Assert.equal(rows[0].getResultByName("removed_reason"), expectedReason);
      let snapshot = await Snapshots.get(TEST_URL1, true);
      Assert.equal(snapshot.removedReason, expectedReason);
    }
  }
});
