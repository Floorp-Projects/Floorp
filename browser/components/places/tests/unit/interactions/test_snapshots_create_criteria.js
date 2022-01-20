/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for automatic snapshot creation.
 */

const TEST_URL1 = "https://example.com/";
const TEST_URL2 = "https://example.com/12345";
const TEST_URL3 = "https://example.com/14235";

add_task(async function maxViewTime() {
  let now = Date.now();

  // Make snapshots for any page with a typing time greater than 30 seconds in any one visit.
  Services.prefs.setCharPref(
    "browser.places.interactions.snapshotCriteria",
    JSON.stringify([
      {
        property: "total_view_time",
        aggregator: "max",
        cutoff: 30000,
      },
    ])
  );

  await assertUrlNotification(TOPIC_ADDED, [TEST_URL1, TEST_URL3], () =>
    addInteractions([
      {
        url: TEST_URL1,
        totalViewTime: 40000,
        created_at: now - 1000,
      },
      {
        url: TEST_URL2,
        totalViewTime: 20000,
        created_at: now - 2000,
      },
      {
        url: TEST_URL2,
        totalViewTime: 20000,
        created_at: now - 3000,
      },
      {
        url: TEST_URL3,
        totalViewTime: 20000,
        documentType: Interactions.DOCUMENT_TYPE.MEDIA,
        created_at: now - 3000,
      },
      {
        url: TEST_URL3,
        totalViewTime: 20000,
        documentType: Interactions.DOCUMENT_TYPE.GENERIC,
        created_at: now - 4000,
      },
      {
        url: TEST_URL3,
        totalViewTime: 30000,
        documentType: Interactions.DOCUMENT_TYPE.MEDIA,
        created_at: now - 5000,
      },
    ])
  );

  await assertSnapshots([
    {
      url: TEST_URL1,
      userPersisted: Snapshots.USER_PERSISTED.NO,
      documentType: Interactions.DOCUMENT_TYPE.GENERIC,
    },
    {
      url: TEST_URL3,
      userPersisted: Snapshots.USER_PERSISTED.NO,
      documentType: Interactions.DOCUMENT_TYPE.MEDIA,
    },
  ]);

  await reset();
});

add_task(async function cumulative() {
  // Snapshots should be created as new interactions occur
  Services.prefs.setCharPref(
    "browser.places.interactions.snapshotCriteria",
    JSON.stringify([
      {
        property: "total_view_time",
        aggregator: "sum",
        cutoff: 30000,
      },
    ])
  );

  await assertTopicNotObserved(TOPIC_ADDED, () =>
    addInteractions([
      {
        url: TEST_URL1,
        totalViewTime: 20000,
      },
    ])
  );

  Assert.strictEqual(
    await Snapshots.get(TEST_URL1, true),
    null,
    "Should not have created this snapshot yet."
  );

  await assertUrlNotification(TOPIC_ADDED, [TEST_URL1], () =>
    addInteractions([
      {
        url: TEST_URL1,
        totalViewTime: 20000,
      },
    ])
  );

  await assertSnapshots([
    {
      url: TEST_URL1,
      userPersisted: Snapshots.USER_PERSISTED.NO,
    },
  ]);

  // Shouldn't see the notification when a url is already a snapshot.
  await assertTopicNotObserved(TOPIC_ADDED, () =>
    addInteractions([
      {
        url: TEST_URL1,
        totalViewTime: 20000,
      },
    ])
  );

  await reset();
});

add_task(async function tombstoned() {
  let removedAt = new Date();

  // Tombstoned snapshots should not be re-created.
  Services.prefs.setCharPref(
    "browser.places.interactions.snapshotCriteria",
    JSON.stringify([
      {
        property: "total_view_time",
        aggregator: "sum",
        cutoff: 30000,
      },
    ])
  );

  await addInteractions([
    {
      url: TEST_URL1,
      totalViewTime: 1000,
    },
  ]);

  Assert.strictEqual(
    await Snapshots.get(TEST_URL1, true),
    null,
    "Should not have created this snapshot yet."
  );

  await Snapshots.add({
    url: TEST_URL1,
    userPersisted: Snapshots.USER_PERSISTED.MANUAL,
  });

  assertSnapshot(await Snapshots.get(TEST_URL1, true), {
    url: TEST_URL1,
    userPersisted: Snapshots.USER_PERSISTED.MANUAL,
  });

  await assertTopicNotObserved(TOPIC_ADDED, () =>
    addInteractions([
      {
        url: TEST_URL1,
        totalViewTime: 40000,
      },
    ])
  );

  // Shouldn't have overwritten the userPersisted value.
  assertSnapshot(await Snapshots.get(TEST_URL1, true), {
    url: TEST_URL1,
    userPersisted: Snapshots.USER_PERSISTED.MANUAL,
  });

  await assertUrlNotification(TOPIC_DELETED, [TEST_URL1], () =>
    Snapshots.delete(TEST_URL1)
  );

  assertSnapshot(await Snapshots.get(TEST_URL1, true), {
    url: TEST_URL1,
    userPersisted: Snapshots.USER_PERSISTED.MANUAL,
    removedAt,
  });

  await assertTopicNotObserved(TOPIC_ADDED, () =>
    addInteractions([
      {
        url: TEST_URL1,
        totalViewTime: 40000,
      },
    ])
  );

  // Shouldn't have overwritten the tombstone flag.
  assertSnapshot(await Snapshots.get(TEST_URL1, true), {
    url: TEST_URL1,
    userPersisted: Snapshots.USER_PERSISTED.MANUAL,
    removedAt,
  });

  await reset();
});

add_task(async function multipleCriteria() {
  let now = Date.now();

  // Different criteria can apply to create snapshots.
  Services.prefs.setCharPref(
    "browser.places.interactions.snapshotCriteria",
    JSON.stringify([
      {
        property: "total_view_time",
        aggregator: "min",
        cutoff: 10000,
      },
      {
        property: "key_presses",
        aggregator: "avg",
        cutoff: 20,
      },
    ])
  );

  await addInteractions([
    {
      url: TEST_URL1,
      totalViewTime: 10000,
      created_at: now - 1000,
    },
    {
      url: TEST_URL1,
      totalViewTime: 20000,
      created_at: now - 1000,
    },
    {
      url: TEST_URL2,
      keypresses: 10,
      created_at: now - 2000,
    },
    {
      url: TEST_URL2,
      keypresses: 50,
      created_at: now - 2000,
    },
    {
      url: TEST_URL2,
      keypresses: 20,
      created_at: now - 2000,
    },
    {
      url: TEST_URL3,
      totalViewTime: 30000,
      keypresses: 10,
      created_at: now - 3000,
    },
    {
      url: TEST_URL3,
      totalViewTime: 5000,
      keypresses: 10,
      created_at: now - 3000,
    },
  ]);

  await assertSnapshots([{ url: TEST_URL1 }, { url: TEST_URL2 }]);

  await reset();
});
