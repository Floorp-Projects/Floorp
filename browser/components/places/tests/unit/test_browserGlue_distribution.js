/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that nsBrowserGlue correctly imports bookmarks from distribution.ini.
 */

const PREF_BMPROCESSED = "distribution.516444.bookmarksProcessed";
const PREF_DISTRIBUTION_ID = "distribution.id";

const TOPICDATA_DISTRIBUTION_CUSTOMIZATION = "force-distribution-customization";
const TOPIC_CUSTOMIZATION_COMPLETE = "distribution-customization-complete";
const TOPIC_BROWSERGLUE_TEST = "browser-glue-test";

function run_test() {
  // Set special pref to load distribution.ini from the profile folder.
  Services.prefs.setBoolPref("distribution.testing.loadFromProfile", true);

  // Copy distribution.ini file to the profile dir.
  let distroDir = gProfD.clone();
  distroDir.leafName = "distribution";
  let iniFile = distroDir.clone();
  iniFile.append("distribution.ini");
  if (iniFile.exists()) {
    iniFile.remove(false);
    print("distribution.ini already exists, did some test forget to cleanup?");
  }

  let testDistributionFile = gTestDir.clone();
  testDistributionFile.append("distribution.ini");
  testDistributionFile.copyTo(distroDir, "distribution.ini");
  Assert.ok(testDistributionFile.exists());

  run_next_test();
}

registerCleanupFunction(function() {
  // Remove the distribution file, even if the test failed, otherwise all
  // next tests will import it.
  let iniFile = gProfD.clone();
  iniFile.leafName = "distribution";
  iniFile.append("distribution.ini");
  if (iniFile.exists()) {
    iniFile.remove(false);
  }
  Assert.ok(!iniFile.exists());
});

add_task(async function() {
  let { DistributionCustomizer } = ChromeUtils.import(
    "resource:///modules/distribution.js"
  );
  let distribution = new DistributionCustomizer();

  let glue = Cc["@mozilla.org/browser/browserglue;1"].getService(
    Ci.nsIObserver
  );
  // Initialize Places through the History Service and check that a new
  // database has been created.
  Assert.equal(
    PlacesUtils.history.databaseStatus,
    PlacesUtils.history.DATABASE_STATUS_CREATE
  );
  // Force distribution.
  glue.observe(
    null,
    TOPIC_BROWSERGLUE_TEST,
    TOPICDATA_DISTRIBUTION_CUSTOMIZATION
  );

  // Test will continue on customization complete notification.
  await promiseTopicObserved(TOPIC_CUSTOMIZATION_COMPLETE);

  // Check the custom bookmarks exist on menu.
  let menuItem = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    index: 0,
  });
  Assert.equal(menuItem.title, "Menu Link Before");
  Assert.ok(
    menuItem.guid.startsWith(distribution.BOOKMARK_GUID_PREFIX),
    "Guid of this bookmark has expected prefix"
  );

  menuItem = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    index: 1 + DEFAULT_BOOKMARKS_ON_MENU,
  });
  Assert.equal(menuItem.title, "Menu Link After");

  // Check no favicon exists for this bookmark
  await Assert.rejects(
    waitForResolvedPromise(
      () => {
        return PlacesUtils.promiseFaviconData(menuItem.url.href);
      },
      "Favicon not found",
      10
    ),
    /Favicon\snot\sfound/,
    "Favicon not found"
  );

  // Check the custom bookmarks exist on toolbar.
  let toolbarItem = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0,
  });
  Assert.equal(toolbarItem.title, "Toolbar Link Before");

  // Check the custom favicon exist for this bookmark
  let faviconItem = await waitForResolvedPromise(
    () => {
      return PlacesUtils.promiseFaviconData(toolbarItem.url.href);
    },
    "Favicon not found",
    10
  );
  Assert.equal(faviconItem.uri.spec, "https://example.org/favicon.png");
  Assert.greater(faviconItem.dataLen, 0);
  Assert.equal(faviconItem.mimeType, "image/png");

  let base64Icon =
    "data:image/png;base64," +
    base64EncodeString(String.fromCharCode.apply(String, faviconItem.data));
  Assert.equal(base64Icon, SMALLPNG_DATA_URI.spec);

  toolbarItem = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 1 + DEFAULT_BOOKMARKS_ON_TOOLBAR,
  });
  Assert.equal(toolbarItem.title, "Toolbar Folder After");
  Assert.ok(
    toolbarItem.guid.startsWith(distribution.FOLDER_GUID_PREFIX),
    "Guid of this folder has expected prefix"
  );

  // Check the bmprocessed pref has been created.
  Assert.ok(Services.prefs.getBoolPref(PREF_BMPROCESSED));

  // Check distribution prefs have been created.
  Assert.equal(Services.prefs.getCharPref(PREF_DISTRIBUTION_ID), "516444");
});
