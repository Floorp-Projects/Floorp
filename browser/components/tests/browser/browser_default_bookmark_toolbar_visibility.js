/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test PlacesUIUtils.maybeToggleBookmarkToolbarVisibility() code running for new profiles.
 * Ensure that the bookmarks toolbar is hidden in a default configuration.
 * If new default bookmarks are added to the toolbar then the threshold of > 3
 * in NUM_TOOLBAR_BOOKMARKS_TO_UNHIDE may need to be adjusted there.
 */
add_task(async function test_default_bookmark_toolbar_visibility() {
  // The Bookmarks Toolbar visibility state should be set only after
  // Places has notified that it's done initializing.
  const browserGlue = Cc["@mozilla.org/browser/browserglue;1"].getService(
    Ci.nsIObserver
  );

  let placesInitCompleteObserved = TestUtils.topicObserved(
    "places-browser-init-complete"
  );

  // If places-browser-init-complete has already notified, this will cause it
  // to notify again. Otherwise, we wait until the notify is done.
  browserGlue.observe(
    null,
    "browser-glue-test",
    "places-browser-init-complete"
  );

  await placesInitCompleteObserved;

  const BROWSER_DOCURL = AppConstants.BROWSER_CHROME_URL;
  let xulStore = Services.xulStore;

  is(
    xulStore.getValue(BROWSER_DOCURL, "PersonalToolbar", "collapsed"),
    "",
    "Check that @collapsed isn't persisted"
  );
  ok(
    document.getElementById("PersonalToolbar").collapsed,
    "The bookmarks toolbar should be collapsed by default"
  );
});
