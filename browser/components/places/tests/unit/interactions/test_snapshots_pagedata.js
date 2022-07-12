/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that adding a snapshot also adds related page data.
 */
ChromeUtils.defineESModuleGetters(this, {
  PageDataSchema: "resource:///modules/pagedata/PageDataSchema.sys.mjs",
  PageDataService: "resource:///modules/pagedata/PageDataService.sys.mjs",
});

const TEST_URL1 = "https://example.com/";
const TEST_URL2 = "https://example.com/12345";
const TEST_URL3 = "https://example.com/14235";

add_task(async function pagedata() {
  // Simulate the interactions service locking the urls of interest.
  PageDataService.lockEntry(Interactions, TEST_URL1);
  PageDataService.lockEntry(Interactions, TEST_URL2);
  PageDataService.lockEntry(Interactions, TEST_URL3);

  let actor = {};
  // These urls are manually snapshotted so simulate a browser keeping them alive too.
  PageDataService.lockEntry(actor, TEST_URL2);
  PageDataService.lockEntry(actor, TEST_URL3);

  // Register some page data.
  PageDataService.pageDataDiscovered({
    url: TEST_URL1,
    date: Date.now(),
    siteName: "Mozilla",
    description: "We build the Firefox web browser",
    data: {
      [PageDataSchema.DATA_TYPE.PRODUCT]: {
        price: {
          value: 276,
          currency: "USD",
        },
      },
      // Unsupported page data
      1: {
        bacon: "good",
        kale: "bad",
      },
    },
  });

  PageDataService.pageDataDiscovered({
    url: TEST_URL2,
    date: Date.now(),
    data: {
      [PageDataSchema.DATA_TYPE.PRODUCT]: {
        price: {
          value: 384,
        },
      },
    },
  });

  let now = Date.now();

  // Make snapshots for any page with a typing time greater than 30 seconds
  // in any one visit.
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

  await addInteractions([
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
      created_at: now - 4000,
    },
  ]);

  await assertSnapshots([
    {
      url: TEST_URL1,
      userPersisted: Snapshots.USER_PERSISTED.NO,
      documentType: Interactions.DOCUMENT_TYPE.GENERIC,
    },
  ]);

  // The lock on the page data should have been released now.
  Assert.equal(
    PageDataService.getCached(TEST_URL1),
    null,
    "The entry should no longer be cached"
  );

  let snap = await Snapshots.get(TEST_URL1);
  Assert.equal(snap.siteName, "Mozilla", "Should have the site name.");
  Assert.equal(snap.description, "We build the Firefox web browser");
  Assert.equal(snap.pageData.size, 2, "Should have some page data.");
  Assert.deepEqual(
    snap.pageData.get(PageDataSchema.DATA_TYPE.PRODUCT),
    { price: { value: 276, currency: "USD" } },
    "Should have the right price."
  );

  await Snapshots.add({ url: TEST_URL2 });
  snap = await Snapshots.get(TEST_URL2);
  Assert.equal(snap.description, null);
  Assert.equal(snap.pageData.size, 1, "Should have some page data.");
  Assert.deepEqual(
    snap.pageData.get(PageDataSchema.DATA_TYPE.PRODUCT),
    { price: { value: 384 } },
    "Should have the right price."
  );

  await Snapshots.add({ url: TEST_URL3 });
  snap = await Snapshots.get(TEST_URL3);
  Assert.equal(snap.pageData.size, 0, "Should be no page data.");

  await assertSnapshots(
    [
      {
        url: TEST_URL1,
        userPersisted: Snapshots.USER_PERSISTED.NO,
        documentType: Interactions.DOCUMENT_TYPE.GENERIC,
      },
      {
        url: TEST_URL2,
        userPersisted: Snapshots.USER_PERSISTED.NO,
        documentType: Interactions.DOCUMENT_TYPE.GENERIC,
      },
    ],
    { type: PageDataSchema.DATA_TYPE.PRODUCT }
  );

  info("Ensure that removing a snapshot removes pagedata for it");
  await Snapshots.delete(TEST_URL1);
  await Snapshots.delete(TEST_URL2);
  let db = await PlacesUtils.promiseDBConnection();
  Assert.equal(
    (
      await db.execute(
        "SELECT count(*) FROM moz_places_metadata_snapshots_extra"
      )
    )[0].getResultByIndex(0),
    0,
    "Ensure there's no leftover page data in the database"
  );

  info("Ensure adding back the snapshot adds pagedata for it");
  await Snapshots.add({
    url: TEST_URL2,
    userPersisted: Snapshots.USER_PERSISTED.MANUAL,
  });
  snap = await Snapshots.get(TEST_URL2);
  Assert.equal(snap.description, null);
  Assert.equal(snap.pageData.size, 1, "Should have some page data.");
  Assert.deepEqual(
    snap.pageData.get(PageDataSchema.DATA_TYPE.PRODUCT),
    { price: { value: 384 } },
    "Should have the right price."
  );

  PageDataService.unlockEntry(actor, TEST_URL2);
  PageDataService.unlockEntry(actor, TEST_URL3);

  Assert.equal(
    PageDataService.getCached(TEST_URL2),
    null,
    "The entry should no longer be cached"
  );

  await reset();
});

add_task(async function pagedata_validation() {
  let actor = {};
  PageDataService.lockEntry(actor, TEST_URL1);

  // Register some page data.
  PageDataService.pageDataDiscovered({
    url: TEST_URL1,
    date: Date.now(),
    siteName:
      "This is a very long site name that will be truncated when saved to the database",
    description: "long description".repeat(20),
    data: {},
  });

  await addInteractions([
    {
      url: TEST_URL1,
      totalViewTime: 40000,
      created_at: Date.now() - 1000,
    },
  ]);

  await Snapshots.add({ url: TEST_URL1 });
  PageDataService.unlockEntry(actor, TEST_URL1);

  let snap = await Snapshots.get(TEST_URL1);
  Assert.equal(
    snap.siteName,
    "This is a very long site name that will be truncat"
  );
  Assert.equal(snap.description, "long description".repeat(16));
  Assert.equal(snap.pageData.size, 0, "Should have no page data.");

  await reset();
});
