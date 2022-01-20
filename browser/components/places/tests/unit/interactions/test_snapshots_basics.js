/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for snapshot creation and removal.
 */

const TEST_URL1 = "https://example.com/";
const TEST_URL2 = "https://example.com/12345";
const TEST_URL3 = "https://example.com/14235";
const TEST_URL4 = "https://example.com/15234";
const VERSION_PREF = "browser.places.snapshots.version";

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
  Sqlite: "resource://gre/modules/Sqlite.jsm",
});

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

add_task(async function deleteKeyframesDb() {
  Services.prefs.setIntPref(VERSION_PREF, 0);

  let profileDir = await PathUtils.getProfileDir();
  let pathToKeyframes = PathUtils.join(profileDir, "keyframes.sqlite");

  try {
    let db = await Sqlite.openConnection({
      path: pathToKeyframes,
    });
    await db.close();

    Assert.ok(
      await IOUtils.exists(pathToKeyframes),
      "Sanity check: keyframes.sqlite exists."
    );
    await Snapshots.query({ url: TEST_URL1 });
    Assert.ok(
      !(await IOUtils.exists(pathToKeyframes)),
      "Keyframes.sqlite was deleted."
    );
  } catch (ex) {
    console.warn(`Error occured in deleteKeyframesDb: ${ex}`);
  }

  Assert.equal(
    Services.prefs.getIntPref(VERSION_PREF, 0),
    Snapshots.currentVersion,
    "Calling Snapshots.query successfully updated to the most recent schema version."
  );
});
