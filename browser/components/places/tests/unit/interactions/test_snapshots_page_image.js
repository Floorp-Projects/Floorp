/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that adding a snapshot also adds related page data.
 */
ChromeUtils.defineESModuleGetters(this, {
  PageDataService: "resource:///modules/pagedata/PageDataService.sys.mjs",
});

const TEST_URL1 = "https://example.com/";
const TEST_URL2 = "https://example.com/12345";

const TEST_IMAGE_URL = "https://example.com/dummy.png";

add_task(async function pageImage() {
  // Simulate the interactions service locking page data into the cache.
  PageDataService.lockEntry(Interactions, TEST_URL1);
  PageDataService.lockEntry(Interactions, TEST_URL2);

  // Register some page data.
  PageDataService.pageDataDiscovered({
    url: TEST_URL1,
    image: TEST_IMAGE_URL,
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

  // Page thumbnails are broken in xpcshell tests.
  Services.prefs.setBoolPref("browser.pagethumbnails.capturing_disabled", true);

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
  ]);

  await assertSnapshots([
    {
      url: TEST_URL1,
      userPersisted: Snapshots.USER_PERSISTED.NO,
      documentType: Interactions.DOCUMENT_TYPE.GENERIC,
    },
  ]);

  let snap = await Snapshots.get(TEST_URL1);
  let imageUrl = await Snapshots.getSnapshotImageURL(snap);
  Assert.equal(snap.pageData.size, 0, "Should have no page data.");
  Assert.equal(imageUrl, TEST_IMAGE_URL, "Should have a page image.");

  // In the browser a snapshot would usually have a thumbnail generated, but the
  // thumbnail service does not work in xpcshell tests.
  await Snapshots.add({ url: TEST_URL2 });
  snap = await Snapshots.get(TEST_URL2);
  imageUrl = await Snapshots.getSnapshotImageURL(snap);
  Assert.equal(imageUrl, null, "Should not have a page image.");

  await reset();
});
