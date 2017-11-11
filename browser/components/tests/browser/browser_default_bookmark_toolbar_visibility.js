/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test _maybeToggleBookmarkToolbarVisibility() code running for new profiles.
 * Ensure that the bookmarks toolbar is hidden in a default configuration.
 * If new default bookmarks are added to the toolbar then the threshold of > 3
 * in NUM_TOOLBAR_BOOKMARKS_TO_UNHIDE may need to be adjusted there.
 */
add_task(async function test_default_bookmark_toolbar_visibility() {
  const BROWSER_DOCURL = "chrome://browser/content/browser.xul";
  let xulStore = Cc["@mozilla.org/xul/xulstore;1"].getService(Ci.nsIXULStore);

  is(xulStore.getValue(BROWSER_DOCURL, "PersonalToolbar", "collapsed"), "",
     "Check that @collapsed isn't persisted");
  ok(document.getElementById("PersonalToolbar").collapsed,
     "The bookmarks toolbar should be collapsed by default");
});
