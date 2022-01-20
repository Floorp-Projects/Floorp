/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that users can add snapshots for a URL before the corresponding
 * interaction is created.
 */

const TEST_URL1 = "https://example.com/";

add_task(async function setup() {
  await reset();
  registerCleanupFunction(async () => {
    await reset();
  });
});

add_task(async function test_add_snapshot_no_interaction_userPersisted() {
  let now = Date.now();
  Assert.ok(
    !(await PlacesTestUtils.isPageInDB(TEST_URL1)),
    "Sanity check: URL is not yet in Places."
  );

  // Sanity check: we don't yet have any snapshots or interactions.
  await assertSnapshots([]);
  let interactions = await getInteractions();
  Assert.ok(!interactions.length, "There should be no interactions.");

  // userPersisted must be true to be able to add a snapshot before an
  // interaction.
  await assertUrlNotification(TOPIC_ADDED, [TEST_URL1], () =>
    Snapshots.add({
      url: TEST_URL1,
      userPersisted: Snapshots.USER_PERSISTED.MANUAL,
    })
  );

  Assert.ok(
    await PlacesTestUtils.isPageInDB(TEST_URL1),
    "Adding a snapshot added the URL to Places."
  );

  interactions = await getInteractions();
  Assert.ok(!interactions.length, "There should still be no interactions.");

  await assertSnapshots([
    {
      url: TEST_URL1,
      title: null,
      documentType: Interactions.DOCUMENT_TYPE.GENERIC,
      firstInteractionAt: new Date(0),
      userPersisted: Snapshots.USER_PERSISTED.MANUAL,
    },
  ]);

  await addInteractions([
    {
      url: TEST_URL1,
      documentType: Interactions.DOCUMENT_TYPE.MEDIA,
      created_at: now - 10000,
      updated_at: now - 10000,
    },
  ]);
  interactions = await getInteractions();
  Assert.equal(interactions.length, 1, "There should be one interaction.");

  // We expect the snapshot has been updated with some of the properties from
  // the interaction.
  await assertSnapshots([
    {
      url: TEST_URL1,
      // title is exempted because addInteractions called PlacesUtils.addVisits,
      // which adds the default title of `test visit for $URL`.
      documentType: Interactions.DOCUMENT_TYPE.MEDIA,
      firstInteractionAt: new Date(now - 10000),
      lastInteractionAt: new Date(now - 10000),
      userPersisted: Snapshots.USER_PERSISTED.MANUAL,
    },
  ]);
  await PlacesUtils.history.clear();
});

add_task(async function test_add_snapshot_no_interaction_not_userPersisted() {
  await Assert.rejects(
    Snapshots.add({ url: "https://invalid.com/" }),
    /NOT NULL constraint failed/,
    "Should reject if an interaction is missing and userPersisted is false."
  );
});
