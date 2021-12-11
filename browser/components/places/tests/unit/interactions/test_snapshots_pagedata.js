/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that adding a snapshot also adds related page data.
 */
XPCOMUtils.defineLazyModuleGetters(this, {
  PageDataCollector: "resource:///modules/pagedata/PageDataCollector.jsm",
  PageDataService: "resource:///modules/pagedata/PageDataService.jsm",
});

const TEST_URL1 = "https://example.com/";
const TEST_URL2 = "https://example.com/12345";
const TEST_URL3 = "https://example.com/14235";

add_task(async function pagedata() {
  // Register some page data.
  PageDataService.pageDataDiscovered(TEST_URL1, [
    {
      type: PageDataCollector.DATA_TYPE.PRODUCT,
      data: {
        price: 276,
      },
    },
    {
      // Unsupported page data.
      type: 1234,
      data: {
        bacon: "good",
      },
    },
  ]);

  PageDataService.pageDataDiscovered(TEST_URL2, [
    {
      type: PageDataCollector.DATA_TYPE.PRODUCT,
      data: {
        price: 384,
      },
    },
  ]);

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
      userPersisted: false,
      documentType: Interactions.DOCUMENT_TYPE.GENERIC,
    },
  ]);

  let snap = await Snapshots.get(TEST_URL1);
  Assert.equal(snap.pageData.size, 1, "Should have some page data.");
  Assert.equal(
    snap.pageData.get(PageDataCollector.DATA_TYPE.PRODUCT).price,
    276,
    "Should have the right price."
  );

  await Snapshots.add({ url: TEST_URL2 });
  snap = await Snapshots.get(TEST_URL2);
  Assert.equal(snap.pageData.size, 1, "Should have some page data.");
  Assert.equal(
    snap.pageData.get(PageDataCollector.DATA_TYPE.PRODUCT).price,
    384,
    "Should have the right price."
  );

  await Snapshots.add({ url: TEST_URL3 });
  snap = await Snapshots.get(TEST_URL3);
  Assert.equal(snap.pageData.size, 0, "Should be no page data.");

  await assertSnapshots(
    [
      {
        url: TEST_URL1,
        userPersisted: false,
        documentType: Interactions.DOCUMENT_TYPE.GENERIC,
      },
      {
        url: TEST_URL2,
        userPersisted: false,
        documentType: Interactions.DOCUMENT_TYPE.GENERIC,
      },
    ],
    { type: PageDataCollector.DATA_TYPE.PRODUCT }
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
  await Snapshots.add({ url: TEST_URL1, userPersisted: true });
  snap = await Snapshots.get(TEST_URL1);
  Assert.equal(snap.pageData.size, 1, "Should have some page data.");
  Assert.equal(
    snap.pageData.get(PageDataCollector.DATA_TYPE.PRODUCT).price,
    276,
    "Should have the right price."
  );

  await reset();
});
