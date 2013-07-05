/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that nsBrowserGlue correctly imports bookmarks from distribution.ini.
 */

const PREF_SMART_BOOKMARKS_VERSION = "browser.places.smartBookmarksVersion";
const PREF_BMPROCESSED = "distribution.516444.bookmarksProcessed";
const PREF_DISTRIBUTION_ID = "distribution.id";

const TOPICDATA_DISTRIBUTION_CUSTOMIZATION = "force-distribution-customization";
const TOPIC_CUSTOMIZATION_COMPLETE = "distribution-customization-complete";
const TOPIC_BROWSERGLUE_TEST = "browser-glue-test";

function run_test()
{
  do_test_pending();

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
  do_check_true(testDistributionFile.exists());

  // Disable Smart Bookmarks creation.
  Services.prefs.setIntPref(PREF_SMART_BOOKMARKS_VERSION, -1);

  // Initialize Places through the History Service and check that a new
  // database has been created.
  do_check_eq(PlacesUtils.history.databaseStatus,
              PlacesUtils.history.DATABASE_STATUS_CREATE);

  // Force distribution.
  Cc["@mozilla.org/browser/browserglue;1"].
  getService(Ci.nsIObserver).observe(null,
                                     TOPIC_BROWSERGLUE_TEST,
                                     TOPICDATA_DISTRIBUTION_CUSTOMIZATION);

  // Test will continue on customization complete notification.
  Services.obs.addObserver(function(aSubject, aTopic, aData) {
    Services.obs.removeObserver(arguments.callee,
                                TOPIC_CUSTOMIZATION_COMPLETE,
                                false);
    do_execute_soon(onCustomizationComplete);
  }, TOPIC_CUSTOMIZATION_COMPLETE, false);
}

function onCustomizationComplete()
{
  // Check the custom bookmarks exist on menu.
  let menuItemId =
    PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.bookmarksMenuFolderId, 0);
  do_check_neq(menuItemId, -1);
  do_check_eq(PlacesUtils.bookmarks.getItemTitle(menuItemId),
              "Menu Link Before");
  menuItemId =
    PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.bookmarksMenuFolderId,
                                         1 + DEFAULT_BOOKMARKS_ON_MENU);
  do_check_neq(menuItemId, -1);
  do_check_eq(PlacesUtils.bookmarks.getItemTitle(menuItemId),
              "Menu Link After");

  // Check the custom bookmarks exist on toolbar.
  let toolbarItemId =
    PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.toolbarFolderId, 0);
  do_check_neq(toolbarItemId, -1);
  do_check_eq(PlacesUtils.bookmarks.getItemTitle(toolbarItemId),
              "Toolbar Link Before");
  toolbarItemId =
    PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.toolbarFolderId,
                                         1 + DEFAULT_BOOKMARKS_ON_TOOLBAR);
  do_check_neq(toolbarItemId, -1);
  do_check_eq(PlacesUtils.bookmarks.getItemTitle(toolbarItemId),
              "Toolbar Link After");

  // Check the bmprocessed pref has been created.
  do_check_true(Services.prefs.getBoolPref(PREF_BMPROCESSED));

  // Check distribution prefs have been created.
  do_check_eq(Services.prefs.getCharPref(PREF_DISTRIBUTION_ID), "516444");

  do_test_finished();
}

do_register_cleanup(function() {
  // Remove the distribution file, even if the test failed, otherwise all
  // next tests will import it.
  let iniFile = gProfD.clone();
  iniFile.leafName = "distribution";
  iniFile.append("distribution.ini");
  if (iniFile.exists())
    iniFile.remove(false);
  do_check_false(iniFile.exists());
});
